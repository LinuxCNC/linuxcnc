/********************************************************************
* Copyright (C) 2012 - 2013 Michael Haberler <license AT mah DOT priv DOT at>
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


/***********************************************************************
*                           RT exception handling                      *
************************************************************************/

#include "config.h"
#include "rtapi.h"
#include "rtapi_exception.h"

#ifdef RTAPI

#define MAX_RT_ERRORS 10

static int rtapi_default_rt_exception_handler(rtapi_exception_t type,
					      rtapi_exception_detail_t *detail,
					      rtapi_threadstatus_t *ts);

// exported symbol within RTAPI
rtapi_exception_handler_t rt_exception_handler
 = rtapi_default_rt_exception_handler;

static void log_thread_stats(rtapi_exception_t type,
			     rtapi_threadstatus_t *ts);

// The RTAPI default exception handler -
// factored out as separate file to ease rolling your own in a component
//
// This function is not visible in ULAPI.

// this default handler just writes to the log but does not cause any action
// (eg wiggling an estop pin)
//
// for a more intelligent way to handle RT faults, see
// hal/components/rtmon.comp which uses rtapi_set_exception(handler)
// to override this default handler during module lifetime

static int rtapi_default_rt_exception_handler(rtapi_exception_t type,
					      rtapi_exception_detail_t *detail,
					      rtapi_threadstatus_t *ts)
{
    static int error_printed = 0;
    int level = (error_printed == 0) ? RTAPI_MSG_ERR : RTAPI_MSG_WARN;

    if (error_printed < MAX_RT_ERRORS) {
	error_printed++;

	// apply output policy in one place only.
	switch (type) {

	    // RTAI errors
	case RTAI_RTE_UNCLASSIFIED:
	    rtapi_print_msg(level,
			    "%d: RT thread %d: "
			    "unknown return code from rt_task_wait_period():  %d ",
			    type, detail->task_id, detail->error_code);
	    break;

	case RTAI_RTE_UNBLKD:
	    rtapi_print_msg(level,
			    "%d: RT thread %d: rt_task_wait_period() "
			    "unblocked while sleeping:  %d ",
			    type, detail->task_id, detail->error_code);
	    break;

	    // Timing violations
	case RTAI_RTE_TMROVRN:
	case XK_ETIMEDOUT:
	case XU_ETIMEDOUT:
	case RTP_DEADLINE_MISSED:
	     rtapi_print_msg(level,
			    "%d: Unexpected realtime delay on RT thread %d ",
			     type, detail->task_id);
	    log_thread_stats(type, ts);
	    break;

	    //  Kernel traps
	case RTAI_TRAP:
	    rtapi_print_msg(level,
			    "%d: trap event: thread %d "
			    "vec=%d signo=%d ip:%p",
			    type, detail->task_id,
			    detail->flavor.rtai.vector,
			    detail->flavor.rtai.signo,
			    detail->flavor.rtai.ip);
	    break;

	case XK_TRAP:
	    rtapi_print_msg(level,
			    "%d: trap event: thread %d - error %d "
			    "event=%d domain=%u ip:%p sp:%p",
			    type, detail->task_id, detail->error_code,
			    detail->flavor.xeno.event,
			    detail->flavor.xeno.domid,
			    detail->flavor.xeno.ip,
			    detail->flavor.xeno.sp);
	    break;

	    // Xenomai Kernel errors
	case XK_EINTR:
	    rtapi_print_msg(level,
			    "%d: API usage bug: rt_task_unblock() "
			    "called before release point: "
			    "thread %d - errno %d",
			    type,
			    detail->task_id,
			    detail->error_code);
	    break;

	case XK_EPERM:
	    rtapi_print_msg(level,
			    "%d: API usage bug: cannot call rt_task_wait_period()"
			    " from this context: "
			    "thread %d - errno %d",
			    type,
			    detail->task_id,
			    detail->error_code);

	    break;

	    // Xenomai User errors
	case XU_SIGXCPU:	// Xenomai Domain switch
	    rtapi_print_msg(level,
			    "%d: Xenomai Domain switch for thread %d",
			    type, detail->task_id);
	    log_thread_stats(type, ts);
	    break;

	case XU_EWOULDBLOCK:
	case XK_EWOULDBLOCK:
	    rtapi_print_msg(level,
			    "API usage bug: rt_task_set_periodic() not called: "
			    "thread %d - errno %d",
			    detail->task_id,
			    detail->error_code);
	    break;

	case XU_EINTR:
	    rtapi_print_msg(level,
			    "API usage bug: rt_task_unblock() called before"
			    " release point: thread %d -errno %d",
			    detail->task_id,
			    detail->error_code);
	    break;

	case XU_EPERM:
	    rtapi_print_msg(level,
			    "API usage bug: cannot call service from current"
			    " context: thread %d - errno %d",
			    detail->task_id,
			    detail->error_code);
	    break;

	case XU_UNDOCUMENTED:
	case XK_UNDOCUMENTED:
	    rtapi_print_msg(level,
			    "%d: unspecified Xenomai error: thread %d - errno %d",
			    type,
			    detail->task_id,
			    detail->error_code);
	    break;

	default:
	    rtapi_print_msg(level,
			    "%d: unspecified exceptiond detail=%p ts=%p",
			    type, detail, ts);
	}
	if (error_printed ==  MAX_RT_ERRORS)
	    rtapi_print_msg(RTAPI_MSG_WARN,
			    "RTAPI: (further messages will be suppressed)\n");
    }
    return 0;
}

// not every exception might actually update the thread status,
// so output this only if known to be current
static void log_thread_stats(rtapi_exception_t type,
			     rtapi_threadstatus_t *ts)
{
    int flavor = global_data->rtapi_thread_flavor;

    // generic (flavor-independent) counters
    rtapi_print_msg(RTAPI_MSG_WARN,
		    "updates=%d api_err=%d other_err=%d",
		    ts->num_updates, ts->api_errors, ts->other_errors);

    // flavor-specific counters

    switch (flavor) {
    case RTAPI_XENOMAI_ID:
    case RTAPI_XENOMAI_KERNEL_ID:
	if (ts->num_updates) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
			    "wait_errors=%d overruns=%d modesw=%d ctxsw=%d"
			    " pagefaults=%d exectime=%lldnS status=0x%x",
			    ts->flavor.xeno.wait_errors,
			    ts->flavor.xeno.total_overruns,
			    ts->flavor.xeno.modeswitches,
			    ts->flavor.xeno.ctxswitches,
			    ts->flavor.xeno.pagefaults,
			    ts->flavor.xeno.exectime,
			    ts->flavor.xeno.status);
	}
	break;


    default: ;
    }
}


#endif
