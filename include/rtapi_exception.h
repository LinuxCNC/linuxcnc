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

#ifndef _RTAPI_EXCEPTION_H
#define _RTAPI_EXCEPTION_H

typedef void * exc_register_t;  // questionable

// this enum lists all possible cause codes
// passed in rtapi_exception_detai_t.type to the exception handler

typedef enum {
    RTAPI_EXCEPTION_NONE=0,

    // RTAI: failures of rt_task_wait_period(), traps
    RTAI_RTE_TMROVRN, // an immediate return was taken because the 
                      // next period has already expired.
    RTAI_RTE_UNBLKD,  // the task was unblocked while sleeping
    RTAI_RTE_UNCLASSIFIED, // none of the previous two
    RTAI_TRAP,        // received via rtapi_trap_handler

    // Xenomai kernel
    XK_TRAP,
    XK_TRAP_BUG,      // same, but failed to identify RT thread
    XK_ETIMEDOUT,     // release point was missed
    // the next one is likely caused by a programming error:
    XK_EWOULDBLOCK,   // rt_task_wait_period() without previous rt_task_set_periodic()
    XK_EINTR,         // rt_task_unblock() called before release point
    XK_EPERM,         // cannot rt_task_wait_period() from this context
    XK_UNDOCUMENTED,  // unknown error code

    // Xenomai user
    XU_SIGXCPU,       // RT task switched to secondary domain
    XU_SIGXCPU_BUG,   // same, but failed to identify RT thread
    XU_ETIMEDOUT,     // release point was missed
    XU_EWOULDBLOCK,   // rt_task_wait_period() without previous rt_task_set_periodic()
    XU_EINTR,         // rt_task_unblock() called before release point
    XU_EPERM,         // cannot rt_task_wait_period() from this context
    XU_UNDOCUMENTED,  // unknown error code

    // RT-PREEMPT
    RTP_DEADLINE_MISSED, // clock_gettime(CLOCK_MONOTONIC) returned 'too late'

    RTAPI_EXCEPTION_LAST,

} rtapi_exception_t;

// ---- per-flavor RT status  descriptors -------
//
// collect thread status variables from various flavors
// the intent is to collect everything 'interesting'
// about a thread and make it accessible via an  RTAPI_MAX_TASKS sized array
// in global_data_t, so any HAL entity  may peruse it
// this tries to abstract various fields of rather system-specific nature
// in a system-independent, fixed size structure
// the interpretation of members necessarily has to happen in a flavor-specific way
// - only a subset of the fields is valid per flavor

typedef struct { // xenomai-kernel and xenomai user
    // traps really are signals posted to an RT thread in-kernel
    int trap_errors;

    // as reported by rt_task_inquire()
    // filled in by rtapi_thread_updatestats(task_id) RTAPI call (TBD)
    int modeswitches;
    int ctxswitches;
    int pagefaults;
    long long exectime;    // Execution time in primary mode in nanoseconds.
    unsigned status;       // T_BLOCKED etc.

    // errors returned by rt_task_wait_period():
    // set by -ETIMEDOUT:
    int wait_errors; 	// total times the release point was missed
    int total_overruns;	// running count of the above
    // the -EWOULDBLOCK and -EINTR returns are API violations
    // and increment api_errors

    // all others increment other_errors
} xenomai_stats_t;

typedef struct {
    int wait_errors; // RT deadline missed
} rtai_stats_t;

typedef struct {

    int wait_errors; // RT deadline missed

    // filled in by rtapi_thread_update_stats() RTAPI method
    long utime_sec;      // user CPU time used
    long utime_usec;

    long stime_sec;      // system CPU time used
    long stime_usec;

    long ru_minflt;        // page reclaims (soft page faults)
    long ru_majflt;        // page faults (hard page faults)
    long ru_nsignals;      // signals received
    long ru_nivcsw;        // involuntary context switches

    long startup_ru_minflt; // page fault counts at end of
    long startup_ru_majflt; // initalisation
    long startup_ru_nivcsw; //

} rtprempt_stats_t;

// ---- the common thread status descriptor -------

typedef struct  {
    // number of updates (calls to rtapi_task_update_stats()
    int num_updates;

    // eventually it would be nice to have a timestamp of the last
    // update; this should be done as soon as we have an actual
    // invocation time stamp in thread functions

    // generic
    int api_errors;      // hint at programming error
    int other_errors;    // unclassified error returns - peruse log for details

    // flavor-specific
    union {
        xenomai_stats_t xeno;
	rtai_stats_t rtai;
	rtprempt_stats_t rtpreempt;
    } flavor;
} rtapi_threadstatus_t;


// ---- per-flavor exception descriptors -------

typedef struct {
    // details for a Xenomai trap event
    unsigned event; // Xenomai trap event number
    unsigned domid; // Domain id
    exc_register_t ip;  // instruction pointer
    exc_register_t sp;  // stack pointer
    int      pid;   // user process pid
    int    errcode; // xnthread_t.errcode

    // passed by ref from rt_task_wait_period()
    unsigned long overruns;

    // XU_SIGXCPU: siginfo_t reference
    void *siginfo;

} xenomai_exception_t;

typedef struct {
     // details for a RTAI trap event
    unsigned vector; //
    int signo;
    exc_register_t ip;  // instruction pointer

} rtai_exception_t;

typedef struct {
    // RTP_SIGNAL: unhandled signal: siginfo_t reference
    // currently unused - signals handled in rtapi_app
    void *siginfo;
} rtpreempt_exception_t;


// ---- the common thread exception descriptor -------

typedef struct {
    int task_id;            // which RTAPI thread caused this
    int error_code;         // as reported by the failing API or system call

    // flavor-specific
    union {
	// covers xenomai-user and xenomai-kernel:
	xenomai_exception_t xeno;

	rtai_exception_t rtai;

	// covers RT-PREEMPT and Posix:
	rtpreempt_exception_t rtpreempt;
    } flavor;
} rtapi_exception_detail_t;

// Exception handler signature

// NB: the exception handler very likely executes in a severely
// restricted environment (e.g. kernel trap, signal handler) - take
// care to do only the absolutely minimal processing in the handler
// and avoid operations which use large local variables

// 'type' is guaranteed to be set
// both of detail and threadstatus might be passed as NULL
typedef int (*rtapi_exception_handler_t) (rtapi_exception_t type,
					  // which determines the interpretation of
					  // the rtapi_exception_detail_t
					  rtapi_exception_detail_t *,
					  rtapi_threadstatus_t *);

#endif // _RTAPI_EXCEPTION_H
