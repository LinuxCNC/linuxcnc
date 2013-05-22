/********************************************************************
* Description:  xenomai.c
*               This file, 'xenomai.c', implements the unique
*               functions for the Xenomai userland thread system.
********************************************************************/

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#include <sys/mman.h>		/* munlockall() */
#include <nucleus/types.h>	/* XNOBJECT_NAME_LEN */
#include <native/task.h>	/* RT_TASK, rt_task_*() */
#include <native/timer.h>	/* rt_timer_*() */
#include <signal.h>		/* sigaction/SIGXCPU handling */


#ifdef RTAPI
#include <native/mutex.h>
#include <rtdk.h>

#include <stdlib.h>		// abort()

#define MAX_ERRORS 3

/*  RTAPI task functions  */
RT_TASK ostask_array[RTAPI_MAX_TASKS + 1];

// this is needed due to the weirdness of the rt_task_self return value -
// it does _not_ match the address of the RT_TASK structure it was 
// created with
RT_TASK *ostask_self[RTAPI_MAX_TASKS + 1];

static struct rt_stats_struct {
    int rt_wait_error;		/* release point missed */
    int rt_last_overrun;	/* last number of overruns reported by
				   Xenomai */
    int rt_total_overruns;	/* total number of overruns reported
				   by Xenomai */
} rt_stats;
#endif  /* RTAPI */

/* init_rtapi_data */
#ifdef RTAPI
void init_rtapi_data_hook(rtapi_data_t * data) {
    rt_stats.rt_wait_error = 0;
    rt_stats.rt_last_overrun = 0;
    rt_stats.rt_total_overruns = 0;
}
#endif


/* rtapi_init() and rtapi_exit() */

/***********************************************************************
*                    INIT AND EXIT FUNCTIONS                           *
************************************************************************/

int _rtapi_init(const char *modname) {

    return _rtapi_next_module_id();
}

int _rtapi_exit(int module_id) {
  munlockall();
  return 0;
}

/***********************************************************************
*                           RT scheduling exception handling           *
************************************************************************/


#ifdef RTAPI
extern rtapi_exception_handler_t rt_exception_handler;

// trampoline to current handler
static void signal_handler(int sig, siginfo_t *si, void *uctx)
{
    if (rt_exception_handler)
	rt_exception_handler(XU_SIGXCPU, 0,
			     "Xenomai switched RT task to secondary domain");
}
#endif

/***********************************************************************
*                           rtapi_main.c                               *
************************************************************************/
#ifdef RTAPI
void _rtapi_module_init_hook(void)
{
    struct sigaction sig_act;

    sigemptyset( &sig_act.sa_mask );

    sig_act.sa_sigaction = signal_handler;
    sig_act.sa_flags   = SA_SIGINFO;
    sig_act.sa_handler = SIG_IGN;

    sigaction(SIGXCPU, &sig_act, (struct sigaction *) NULL);

    // start rdtk real-time printing library output thread
    // very likely this is actually needed; probably superseded by rtapi_print
    // since we're not using the xenomai rtdk printing library
    // rt_print_auto_init(1);
}
#else
void _rtapi_module_init_hook(void) {}
#endif
/***********************************************************************
*                           rtapi_task.c                               *
************************************************************************/

#ifdef RTAPI
int _rtapi_task_delete_hook(task_data *task, int task_id) {
    int retval = 0;

    if ((retval = rt_task_delete( &ostask_array[task_id] )) < 0)
	rtapi_print_msg(RTAPI_MSG_ERR,"ERROR: rt_task_delete() = %d %s\n", 
			retval, strerror(retval));

    return retval;
}

void _rtapi_task_wrapper(void * task_id_hack) {
    int ret;
    int task_id = (int)(long) task_id_hack; // ugly, but I ain't gonna fix it
    task_data *task = &task_array[task_id];

    /* use the argument to point to the task data */
    if (task->period < period) task->period = period;
    task->ratio = task->period / period;
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_task_wrapper: task %p '%s' period=%d "
		    "prio=%d ratio=%d\n",
		    task, task->name, task->ratio * period,
		    task->prio, task->ratio);

    ostask_self[task_id]  = rt_task_self();
    
    if ((ret = rt_task_set_periodic(NULL, TM_NOW, task->ratio * period)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ERROR: rt_task_set_periodic(%d,%s) failed %d\n", 
			task_id, task->name, ret);
	abort();
    }

    /* call the task function with the task argument */
    (task->taskcode) (task->arg);
    
    /* if the task ever returns, we record that fact */
    task->state = ENDED;
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "ERROR: reached end of wrapper for task %d '%s'\n", 
		    task_id, task->name);
}


int _rtapi_task_start_hook(task_data *task, int task_id) {
    int which_cpu = 0;
    int retval;

#if !defined(BROKEN_XENOMAU_CPU_AFFINITY)
    // seems to work for me
    // not sure T_CPU(n) is possible - see:
    // http://www.xenomai.org/pipermail/xenomai-help/2010-09/msg00081.html

    if (task->cpu > -1)  // explicitly set by threads, motmod
	which_cpu = T_CPU(task->cpu);
#endif

    // http://www.xenomai.org/documentation/trunk/html/api/group__task.html#ga03387550693c21d0223f739570ccd992
    // Passing T_FPU|T_CPU(1) in the mode parameter thus creates a
    // task with FPU support enabled and which will be affine to CPU #1
    // the task will start out dormant; execution begins with rt_task_start()

    // since this is a usermode RT task, it will be FP anyway
    if ((retval = rt_task_create (&ostask_array[task_id], task->name, 
				  task->stacksize, task->prio, 
				  (task->uses_fp ? T_FPU : 0) | which_cpu )
	 ) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rt_task_create failed, rc = %d\n", retval );
	return -ENOMEM;
    }

    if ((retval = rt_task_start( &ostask_array[task_id],
				 _rtapi_task_wrapper, (void *)(long)task_id))) {
	rtapi_print_msg(RTAPI_MSG_INFO,
			"rt_task_start failed, rc = %d\n", retval );
	return -ENOMEM;
    }
    return 0;
}

int _rtapi_task_stop_hook(task_data *task, int task_id) {
    int retval;

    if ((retval = rt_task_delete( &ostask_array[task_id] )) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"rt_task_delete() = %d\n", retval);
	return retval;
    }

    return 0;
}

int _rtapi_task_pause_hook(task_data *task, int task_id) {
    return rt_task_suspend( &ostask_array[task_id] );
}

int _rtapi_task_resume_hook(task_data *task, int task_id) {
    return rt_task_resume( &ostask_array[task_id] );
}

void _rtapi_wait_hook() {
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
			"Unexpected realtime delay on task %d - "
			"'%s' (%lu overruns)",
			task_id, task->name, overruns);
	if (rt_exception_handler)
	    rt_exception_handler(XU_ETIMEDOUT, overruns, buf);
	break;

    case -EWOULDBLOCK:
	if (rt_exception_handler)
	    rt_exception_handler(XU_EWOULDBLOCK, 0,
				 "rt_task_wait_period() without "
				 "previous rt_task_set_periodic()");
	break;

    case -EINTR:
	if (rt_exception_handler)
	    rt_exception_handler(XU_EINTR, 0,
				 "rt_task_unblock() called before "
				 "release point");
	break;

    case -EPERM:
	if (rt_exception_handler)
	    rt_exception_handler(XU_EPERM, 0,"cannot rt_task_wait_period() from "
				 "this context");
	break;

    default:
	rtapi_snprintf(buf, sizeof(buf),
			"unknown error code %d", result);
	if (rt_exception_handler)
	    rt_exception_handler(XU_OTHER, result,buf);
    }
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
	if (ostask_self[n] == ptr) {
	    /* found a match */
	    return n;
	}
	n++;
    }
    return -EINVAL;
}

#endif  /* RTAPI */


/***********************************************************************
*                           rtapi_time.c                               *
************************************************************************/

#ifdef RTAPI
void _rtapi_delay_hook(long int nsec)
{
    long long int release = rt_timer_tsc() + nsec;
    while (rt_timer_tsc() < release);
}
#endif

long long int _rtapi_get_time_hook(void) {
    /* The value returned will represent a count of jiffies if the
       native skin is bound to a periodic time base (see
       CONFIG_XENO_OPT_NATIVE_PERIOD), or nanoseconds otherwise.  */
    return rt_timer_read();
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
