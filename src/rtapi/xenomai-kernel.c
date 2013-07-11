
#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#include <nucleus/types.h>		/* XNOBJECT_NAME_LEN, RTIME */
#include <native/heap.h>		// RT_HEAP, H_SHARED, rt_heap_*
#include <native/task.h>		// RT_TASK, rt_task_*()

#ifdef RTAPI	/* In kernel land, this is equiv. to MODULE */
#include <linux/slab.h>			// kfree
#include <native/types.h>		// TM_INFINITE
#include <native/timer.h>		// rt_timer_*()
#include "procfs_macros.h"		// PROC_PRINT()

#else /* ULAPI */
#include <errno.h>		/* errno */
#include <unistd.h>             // getpid()

#endif

#define MAX_ERRORS 3

#ifdef RTAPI
static rthal_trap_handler_t old_trap_handler;
static int _rtapi_trap_handler(unsigned event, unsigned domid, void *data);
static struct rt_stats_struct {
    int rt_wait_error;		/* release point missed */
    int rt_last_overrun;	/* last number of overruns reported by
				   Xenomai */
    int rt_total_overruns;	/* total number of overruns reported
				   by Xenomai */
} rt_stats;

#endif /* RTAPI */



/***********************************************************************
*                          rtapi_common.h                              *
************************************************************************/
/* fill out Xenomai-specific fields in rtapi_data */
void init_rtapi_data_hook(rtapi_data_t * data) {
#ifdef RTAPI
    rt_stats.rt_wait_error = 0;
    rt_stats.rt_last_overrun = 0;
    rt_stats.rt_total_overruns = 0;
#endif
}


/***********************************************************************
*                          rtapi_module.c                              *
************************************************************************/

#ifdef RTAPI

void _rtapi_module_init_hook(void) {
    old_trap_handler = \
	rthal_trap_catch((rthal_trap_handler_t) _rtapi_trap_handler);
}

void _rtapi_module_cleanup_hook(void) {
    /* release master shared memory block */
    rthal_trap_catch(old_trap_handler);
}
#endif /* RTAPI */


/***********************************************************************
*                           rtapi_time.c                               *
************************************************************************/

#ifdef RTAPI
/*  RTAPI time functions */
long long int _rtapi_get_time_hook(void) {
    /* The value returned will represent a count of jiffies if the
       native skin is bound to a periodic time base (see
       CONFIG_XENO_OPT_NATIVE_PERIOD), or nanoseconds otherwise.  */
    return  rt_timer_read();
}

/* This returns a result in clocks instead of nS, and needs to be used
   with care around CPUs that change the clock speed to save power and
   other disgusting, non-realtime oriented behavior.  But at least it
   doesn't take a week every time you call it.
*/
long long int _rtapi_get_clocks_hook(void) {
    // Gilles says: do this - it's portable
    return rt_timer_tsc();
}

void _rtapi_clock_set_period_hook(long int nsecs, RTIME *counts, 
				 RTIME *got_counts) {
    rtapi_data->timer_period = *got_counts = (RTIME) nsecs; 
}


void _rtapi_delay_hook(long int nsec) 
{
    long long int release = rt_timer_tsc() + nsec;
    while (rt_timer_tsc() < release);
}
#endif /* RTAPI */


/***********************************************************************
*                            rtapi_task.c                              *
************************************************************************/

#ifdef RTAPI

extern rtapi_exception_handler_t rt_exception_handler;

// not better than the builtin Xenomai handler, but at least
// hook into to rtapi_print
int _rtapi_trap_handler(unsigned event, unsigned domid, void *data) {
    char buf[LINELEN];
    struct pt_regs *regs = data;
    xnthread_t *thread = xnpod_current_thread(); ;

    rtapi_snprintf(buf, sizeof(buf),
		    "RTAPI: trap event=%d thread=%s ip:%lx sp:%lx "
		    "userpid=%d errcode=%d\n",
		    event, thread->name,
		    regs->ip, regs->sp,
		    xnthread_user_pid(thread), thread->errcode);

    if (rt_exception_handler)
	rt_exception_handler(XK_TRAP, event, buf);

    // forward to default Xenomai trap handler
    return ((rthal_trap_handler_t) old_trap_handler)(event, domid, data);
}

int _rtapi_task_self_hook(void) {
    RT_TASK *ptr;
    int n;

    /* ask OS for pointer to its data for the current task */
    ptr = rt_task_self();

    if (ptr == NULL) {
	/* called from outside a task? */
	return -EINVAL;
    }
    /* find matching entry in task array */
    n = 1;
    while (n <= RTAPI_MAX_TASKS) {
        if (ostask_array[n] == ptr) {
	    /* found a match */
	    return n;
	}
	n++;
    }
    return -EINVAL;
}


void _rtapi_wait_hook(void) {
    unsigned long overruns;
    int task_id;
    task_data *task;
    char buf[LINELEN];

    int result =  rt_task_wait_period(&overruns);
    switch (result) {
    case 0: // ok - no overruns;
	break;

    case -ETIMEDOUT: // release point was missed
	rt_stats.rt_wait_error++;
	rt_stats.rt_last_overrun = overruns;
	rt_stats.rt_total_overruns += overruns;

	task_id = _rtapi_task_self();
	task = &(task_array[task_id]);

	rtapi_snprintf(buf, sizeof(buf),
			"Unexpected realtime delay on RT thread %d - "
			"'%s' (%lu overruns)",
			task_id, task->name, overruns);
	if (rt_exception_handler)
	    rt_exception_handler(XK_ETIMEDOUT, overruns, buf);
	break;

    case -EWOULDBLOCK:
	if (rt_exception_handler)
	    rt_exception_handler(XK_EWOULDBLOCK, 0,
				 "rt_task_wait_period() without "
				 "previous rt_task_set_periodic()");
	break;

    case -EINTR:
	if (rt_exception_handler)
	    rt_exception_handler(XK_EINTR, 0,
				 "rt_task_unblock() called before "
				 "release point");
	break;

    case -EPERM:
	if (rt_exception_handler)
	    rt_exception_handler(XK_EPERM, 0,"cannot rt_task_wait_period() from "
				 "this context");
	break;
    default:
	rtapi_snprintf(buf, sizeof(buf),
			"unknown error code %d", result);
	if (rt_exception_handler)
	    rt_exception_handler(XK_OTHER, result,buf);
    }
}


int _rtapi_task_new_hook(task_data *task, int task_id) {
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rt_task_create %d \"%s\" cpu=%d fpu=%d prio=%d\n", 
		    task_id, task->name, task->cpu, task->uses_fp,
		    task->prio );

    return rt_task_create(ostask_array[task_id], task->name, task->stacksize,
			  task->prio,
			  (task->uses_fp ? T_FPU : 0) | T_CPU(task->cpu));
}


int _rtapi_task_start_hook(task_data *task, int task_id,
			  unsigned long int period_nsec) {
    int retval;

    if ((retval = rt_task_set_periodic(ostask_array[task_id], TM_NOW,
				       period_nsec)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: rt_task_set_periodic() task_id %d "
			"periodns=%ld returns %d\n", 
			task_id, period_nsec, retval);
	return -EINVAL;
    }
    if ((retval = rt_task_start(ostask_array[task_id], task->taskcode,
				(void*)task->arg )) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: rt_task_start() task_id %d returns %d\n", 
			task_id, retval);
	return -EINVAL;
    }

    return 0;
}


#endif /* ULAPI */


/***********************************************************************
*                          rtapi_proc.h                                *
************************************************************************/
#ifdef RTAPI
void rtapi_proc_read_status_hook(char *page, char **start, off_t off,
				 int count, int *eof, void *data) {
    PROC_PRINT_VARS;
    PROC_PRINT("  Wait errors = %i\n", rt_stats.rt_wait_error);
    PROC_PRINT(" Last overrun = %i\n", rt_stats.rt_last_overrun);
    PROC_PRINT("Total overruns = %i\n", rt_stats.rt_total_overruns);
    PROC_PRINT_DONE;
}
#endif


/***********************************************************************
*                          rtapi_common.c                              *
************************************************************************/
// almost same code as in xenomai.c - could be folded into a single file
int _rtapi_backtrace_hook(int msglevel)
{
#ifdef RTAPI
    int task_id = _rtapi_task_self_hook();

    if (task_id != -EINVAL) {
	// an RT thread
	rtapi_print_msg(msglevel, "RT thread %d: \n", task_id);
	RT_TASK_INFO info;
	int retval = rt_task_inquire(ostask_array[task_id], &info);
	if (retval) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "rt_task_inquire() failed: %d\n", retval);
	} else {
	    rtapi_print_msg(msglevel,
			    "name=%s modeswitches=%d context switches=%d page faults=%d\n",
			    info.name, info.modeswitches, info.ctxswitches, info.pagefaults);
	    rtapi_print_msg(msglevel,"wait errors=%d last overrun=%d total overruns=%d\n",
			    rt_stats.rt_wait_error,
			    rt_stats.rt_last_overrun,
			    rt_stats.rt_total_overruns);
	}
    }
#endif
#ifdef ULAPI
    // ULAPI; use pid
    rtapi_print_msg(msglevel, "pid %d: \n", getpid());
#endif
    return 0;
}
