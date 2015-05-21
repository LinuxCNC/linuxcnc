/********************************************************************
* Copyright (C) 2012 - 2013 John Morris <john AT zultron DOT com>
*                           Michael Haberler <license AT mah DOT priv DOT at>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
********************************************************************/

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"
#include "xenomai-common.h"

#include XENOMAI_INCLUDE(heap.h)	// RT_HEAP, H_SHARED, rt_heap_*
#include XENOMAI_INCLUDE(task.h)	// RT_TASK, rt_task_*()

#ifdef RTAPI	/* In kernel land, this is equiv. to MODULE */
#include <linux/slab.h>			// kfree
#include XENOMAI_INCLUDE(timer.h)	// rt_timer_*()
#include "procfs_macros.h"		// PROC_PRINT()

#else /* ULAPI */
#include <errno.h>			/* errno */
#include <unistd.h>			// getpid()

#endif

#define MAX_ERRORS 3

#ifdef RTAPI
static rthal_trap_handler_t old_trap_handler;
static int _rtapi_trap_handler(unsigned event, unsigned domid, void *data);

#endif /* RTAPI */

/***********************************************************************
*                           RT thread statistics update                *
************************************************************************/
#ifdef RTAPI
int _rtapi_task_update_stats_hook(void)
{
    int task_id = _rtapi_task_self();

    // paranoia
    if ((task_id < 0) || (task_id > RTAPI_MAX_TASKS)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_task_update_stats_hook: BUG -"
			" task_id out of range: %d\n",
			task_id);
	return -ENOENT;
    }

    RT_TASK_INFO rtinfo;
    int retval = rt_task_inquire(ostask_array[task_id], &rtinfo);
    if (retval) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rt_task_inquire() failed: %d\n",
			retval);
	return -ESRCH;
    }

    rtapi_threadstatus_t *ts = &global_data->thread_status[task_id];

    ts->flavor.xeno.modeswitches = rtinfo.modeswitches;
    ts->flavor.xeno.ctxswitches = rtinfo.ctxswitches;
    ts->flavor.xeno.pagefaults = rtinfo.pagefaults;
    ts->flavor.xeno.exectime = rtinfo.exectime;
    ts->flavor.xeno.modeswitches = rtinfo.modeswitches;
    ts->flavor.xeno.status = rtinfo.status;

    ts->num_updates++;

    return task_id;
}
#endif


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
    return rt_timer_read();
}

void _rtapi_clock_set_period_hook(long int nsecs, RTIME *counts, 
				 RTIME *got_counts) {
    rtapi_data->timer_period = *got_counts = (RTIME) nsecs; 
}


void _rtapi_delay_hook(long int nsec) 
{
    long long int release = rt_timer_read() + nsec;
    while (rt_timer_read() < release);
}
#endif /* RTAPI */


/***********************************************************************
*                            rtapi_task.c                              *
************************************************************************/

#ifdef RTAPI
extern int _rtapi_task_self_hook(void);

extern rtapi_exception_handler_t rt_exception_handler;

// not better than the builtin Xenomai handler, but at least
// hook into to rtapi_exception_handler

int _rtapi_trap_handler(unsigned event, unsigned domid, void *data) {
    struct pt_regs *regs = data;
    xnthread_t *thread = xnpod_current_thread(); ;

    int task_id = _rtapi_task_self_hook();

    rtapi_exception_detail_t detail = {0};

    detail.task_id = task_id;
    detail.error_code = thread->errcode;

    detail.flavor.xeno.event = event;
    detail.flavor.xeno.domid = domid;
    detail.flavor.xeno.ip = (exc_register_t) regs->ip;
    detail.flavor.xeno.sp = (exc_register_t) regs->sp;

    if (rt_exception_handler)
	rt_exception_handler(XK_TRAP, &detail,
			     (task_id > -1) ?
			     &global_data->thread_status[task_id] : NULL);

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
    unsigned long overruns = 0;
    int result =  rt_task_wait_period(&overruns);

    if (result) {
	// something went wrong:

	// update stats counters in thread status
	_rtapi_task_update_stats_hook();

	// inquire, fill in
	// exception descriptor, and call exception handler

	int task_id = _rtapi_task_self();

	// paranoid, but you never know; this index off and
	// things will go haywire really fast
	if ((task_id < 0) || (task_id > RTAPI_MAX_TASKS)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "_rtapi_wait_hook: BUG - task_id out of range: %d\n",
			    task_id);
	    // maybe should call a BUG exception here
	    return;
	}

	// task_data *task = &(task_array[task_id]);
	rtapi_exception_detail_t detail = {0};
	rtapi_threadstatus_t *ts = &global_data->thread_status[task_id];
	rtapi_exception_t type;

	// exception descriptor
	detail.task_id = task_id;
	detail.error_code = result;

	switch (result) {

	case -ETIMEDOUT:
	    // release point was missed
	    detail.flavor.xeno.overruns = overruns;

	    // update thread status in global_data
	    ts->flavor.xeno.wait_errors++;
	    ts->flavor.xeno.total_overruns += overruns;
	    type = XK_ETIMEDOUT;
	    break;

	case -EWOULDBLOCK:
	    // returned if rt_task_set_periodic() has not previously
	    // been called for the calling task. This is clearly
	    // a Xenomai API usage error.
	    ts->api_errors++;
	    type = XK_EWOULDBLOCK;
	    break;

	case -EINTR:
	    // returned if rt_task_unblock() has been called for
	    // the waiting task before the next periodic release
	    // point has been reached. In this case, the overrun
	    // counter is reset too.
	    // a Xenomai API usage error.
	    ts->api_errors++;
	    type = XK_EINTR;
	    break;

	case -EPERM:
	    // returned if this service was called from a
	    // context which cannot sleep (e.g. interrupt,
	    // non-realtime or scheduler locked).
	    // a Xenomai API usage error.
	    ts->api_errors++;
	    type = XK_EPERM;
	    break;

	default:
	    // the above should handle all possible returns
	    // as per manual, so at least leave a scent
	    // (or what Jeff calls a 'canary value')
	    ts->other_errors++;
	    type = XK_UNDOCUMENTED;
	}
	if (rt_exception_handler)
		rt_exception_handler(type, &detail, ts);
    }  // else: ok - no overruns;
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
