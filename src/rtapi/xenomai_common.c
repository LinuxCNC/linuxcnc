// code common to Xenomai kernel and userland thread styles

#include "rtapi.h"		/* these decls */
#include "rtapi_common.h"		/* these decls */

#if defined(RTAPI_XENOMAI_USER) || defined(RTAPI_XENOMAI_KERNEL)
#include <native/task.h>
#include <native/timer.h>
#include <rtdk.h>
#include "xenomai_common.h"		/* these decls */

// Xenomai rt_task priorities are 0: lowest .. 99: highest

int rtapi_prio_highest(void)
{
    /* The base priority of the new task. This value must range from  */
    /* [0..99] (inclusive) where 0 is the lowest effective priority. */
    return 99;
}

int rtapi_prio_lowest(void)
{
  return 0;
}

int rtapi_prio_next_higher(int prio)
{
  /* return a valid priority for out of range arg */
  if (prio >= rtapi_prio_highest())
    return rtapi_prio_highest();
  if (prio <= rtapi_prio_lowest())
    return rtapi_prio_lowest();

  /* return next higher priority for in-range arg */
  return prio + 1;
}

int rtapi_prio_next_lower(int prio)
{
  /* return a valid priority for out of range arg */
  if (prio < rtapi_prio_lowest())
    return rtapi_prio_lowest();
  if (prio > rtapi_prio_highest())
    return rtapi_prio_highest();
  /* return next lower priority for in-range arg */
  return prio - 1;
}


int rtapi_task_self(void)
{
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
#if defined(RTAPI_XENOMAI_USER) 
	if (ostask_self[n] == ptr) {
#endif
#if defined(RTAPI_XENOMAI_KERNEL)
        if (ostask_array[n] == ptr) {
#endif
	    /* found a match */
	    return n;
	}
	n++;
    }
    return -EINVAL;
}

long long int rtapi_get_time(void)
{
    /* The value returned will represent a count of jiffies if the native  */
    /* skin is bound to a periodic time base (see CONFIG_XENO_OPT_NATIVE_PERIOD),  */
    /* or nanoseconds otherwise.  */
    return  rt_timer_read();
}

/* This returns a result in clocks instead of nS, and needs to be used
   with care around CPUs that change the clock speed to save power and
   other disgusting, non-realtime oriented behavior.  But at least it
   doesn't take a week every time you call it.
*/
long long int rtapi_get_clocks(void)
{
    // Gilles says: do this - it's portable
    return rt_timer_tsc();
}

void rtapi_wait(void)
{
    unsigned long overruns;
    static int error_printed = 0;
    int task_id;
    task_data *task;

    int result =  rt_task_wait_period(&overruns);
    switch (result) {
    case 0: // ok - no overruns;
	break;

    case -ETIMEDOUT: // release point was missed
	rtapi_data->rt_wait_error++;
	rtapi_data->rt_last_overrun = overruns;
	rtapi_data->rt_total_overruns += overruns;

	if (!error_printed) {
	    task_id = rtapi_task_self();
	    task = &(task_array[task_id]);

	    rtapi_print_msg(RTAPI_MSG_ERR,
	    	      "RTAPI: ERROR: Unexpected realtime delay on task %d - '%s' (%lu overruns)\n" 
		      "This Message will only display once per session.\n"
		      "Run the Latency Test and resolve before continuing.\n", 
			    task_id, task->name, overruns);
	}
	error_printed++;
	/* if(error_printed > 10) // FIXME */
	/*     rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: (further messages will be suppressed)\n"); */
	break;

    case -EWOULDBLOCK:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: rt_task_wait_period() without previous rt_task_set_periodic()\n");
	error_printed++;
	break;

    case -EINTR:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: rt_task_unblock() called before release point\n");
	error_printed++;
	break;

    case -EPERM:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: cannot rt_task_wait_period() from this context\n");
	error_printed++;
	break;
    default:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: unknown error code %d\n", result);
	error_printed++;
	break;
    }
}
#endif
