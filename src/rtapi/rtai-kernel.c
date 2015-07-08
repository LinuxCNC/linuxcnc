#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#include <rtai.h>
#include <rtai_sched.h>

#ifdef MODULE
#include <linux/delay.h>  // udelay()
#endif 

#ifdef RTAPI
extern rtapi_exception_handler_t rt_exception_handler;
#endif

/***********************************************************************
*                           rtapi_time.c                               *
************************************************************************/

#ifdef RTAPI
void _rtapi_module_timer_stop(void) {
    stop_rt_timer();
#if RTAI_VERSION <= 400
    rt_free_timer();
#else
    rt_free_timers();
#endif
}


void _rtapi_clock_set_period_hook(long int nsecs, RTIME *counts, 
				 RTIME *got_counts) {
    rt_set_periodic_mode();
    *counts = nano2count((RTIME) nsecs);
    if(count2nano(*counts) > nsecs) (*counts)--;
    *got_counts = start_rt_timer(*counts);
    rtapi_data->timer_period = count2nano(*got_counts);
}


void _rtapi_delay_hook(long int nsec)
{
     udelay(nsec / 1000);
}

long long int _rtapi_get_time_hook(void) {
    //struct timeval tv;

    //AJ: commenting the following code out, as it seems on some systems it
    // really breaks
    
    /* call the kernel's internal implementation of gettimeofday() */
    /* unfortunately timeval has only usec, struct timespec would be
       better, it has nsec resolution.  Doing this right probably
       involves a number of ifdefs based on kernel version and such */
    /*do_gettimeofday(&tv);*/
    /* convert to nanoseconds */
    /*return (tv.tv_sec * 1000000000LL) + (tv.tv_usec * 1000L);*/
    
    //reverted to old code for now
    /* this is a monstrosity that seems to take several MICROSECONDS!!!
       on some boxes.  Why the RTAI folks even bothered I have no idea!
       If you have any need for speed at all use _rtapi_get_clocks()!!
    */
    return rt_get_cpu_time_ns();
}
#endif  /* RTAPI */


/***********************************************************************
*                           rtapi_task.c                               *
************************************************************************/

#ifdef RTAPI

# if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)
#  define IP(x) ((x)->ip)
# elif defined(__i386__)
#  define IP(x) ((x)->eip)
# else
#  define IP(x) ((x)->rip)
# endif

static int _rtapi_trap_handler(int vec, int signo, struct pt_regs *regs,
			      void *task) {
    int task_id = _rtapi_task_self();

    rtapi_exception_detail_t detail = {0};

    detail.task_id = task_id;
    detail.flavor.rtai.vector = vec;
    detail.flavor.rtai.signo = signo;
    detail.flavor.rtai.ip = (exc_register_t) IP(regs);

    if (rt_exception_handler)
	rt_exception_handler(RTAI_TRAP, &detail,
			     (task_id > -1) ?
			     &global_data->thread_status[task_id] : NULL);

    _rtapi_task_pause(task_id);
    return 0;
}


static void _rtapi_task_wrapper(long task_id)  {
    task_data *task;

    /* point to the task data */
    task = &task_array[task_id];
    /* call the task function with the task argument */
    (task->taskcode) (task->arg);
    /* if the task ever returns, we record that fact */
    task->state = ENDED;
    /* and return to end the thread */
    return;
}

int _rtapi_task_new_hook(task_data *task, int task_id) {
    int retval, v;
    retval = rt_task_init_cpuid(ostask_array[task_id], _rtapi_task_wrapper,
				task_id, task->stacksize, task->prio,
				task->uses_fp, 0 /* signal */, task->cpu);
    if (retval) return retval;

    /* request to handle traps in the new task */
#ifdef HAL_NR_FAULTS
    for(v=0; v<HAL_NR_FAULTS; v++)
        rt_set_task_trap_handler(ostask_array[task_id], v, _rtapi_trap_handler);
#else
    for(v=0; v<IPIPE_NR_FAULTS; v++)
        rt_set_task_trap_handler(ostask_array[task_id], v, _rtapi_trap_handler);
#endif

    return 0;
}


int _rtapi_task_start_hook(task_data *task, int task_id,
			  unsigned long int period_nsec) {
    int retval;
    unsigned long int quo, period_counts;

    period_counts = nano2count((RTIME)period_nsec);  
    quo = (period_counts + timer_counts / 2) / timer_counts;
    period_counts = quo * timer_counts;
    period_nsec = count2nano(period_counts);

    /* start the task */
    retval = rt_task_make_periodic(ostask_array[task_id],
	rt_get_time() + period_counts, period_counts);
    if (retval != 0) {
	return -EINVAL;
    }

    return 0;
}


void _rtapi_wait_hook(void) {

    int result = rt_task_wait_period();
    if (result != 0) {
	int task_id = _rtapi_task_self();
	if ((task_id < 0) || (task_id > RTAPI_MAX_TASKS)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "_rtapi_wait_hook: BUG - task_id out of range: %d\n",
			    task_id);
	    return;
	}
	rtapi_exception_detail_t detail = {0};
	rtapi_threadstatus_t *ts = &global_data->thread_status[task_id];
	rtapi_exception_t type;

	detail.task_id = task_id;
	detail.error_code = result;

	switch (result) {

	case RTE_TMROVRN:
	    // an immediate return was taken because the time
	    // deadline has already expired
	    ts->flavor.rtai.wait_errors++;
	    type = RTAI_RTE_TMROVRN;
	    break;

	case RTE_UNBLKD:
	    // the task was unblocked while sleeping
	    // an API usage error
	    ts->api_errors++;
	    type = RTAI_RTE_UNBLKD;
	    break;

	default:
	    // whzat?
	    ts->other_errors++;
	    type = RTAI_RTE_UNCLASSIFIED;
	}

	if (rt_exception_handler)
	    rt_exception_handler(type, &detail, ts);

    }
}

int _rtapi_task_self_hook(void) {
    RT_TASK *ptr;
    int n;

    /* ask OS for pointer to its data for the current task */
    ptr = rt_whoami();
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
#endif /* RTAPI */

