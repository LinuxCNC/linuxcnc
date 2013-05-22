#ifndef _RTAPI_EXCEPTION_H
#define _RTAPI_EXCEPTION_H

// this enum lists all possible cause codes when calling the
// rtapi exception handler


typedef int (*rtapi_exception_handler_t) (int, int, const char *);
//------- rtapi_exception_t --------------^^^
//------- flavor-specific error code ----------^^^

typedef enum {
    RTAPI_EXCEPTION_FIRST,

    // RTAI: failures of rt_task_wait_period(), traps
    RTAI_RTE_TMROVRN, // an immediate return was taken because the 
                      // next period has already expired.
    RTAI_RTE_UNBLKD,  // the task was unblocked while sleeping
    RTAI_RTE_UNCLASSIFIED, // none of the previous two
    RTAI_TRAP,        // received via rtapi_trap_handler

    // Xenomai kernel
    XK_TRAP,
    XK_ETIMEDOUT,     // release point was missed
    // the next one is likely caused by a programming error:
    XK_EWOULDBLOCK,   // rt_task_wait_period() without previous rt_task_set_periodic()
    XK_EINTR,         // rt_task_unblock() called before release point
    XK_EPERM,         // cannot rt_task_wait_period() from this context
    XK_OTHER,         // unknown error code

    // Xenomai user
    XU_SIGXCPU,
    XU_ETIMEDOUT,     // release point was missed
    XU_EWOULDBLOCK,   // rt_task_wait_period() without previous rt_task_set_periodic()
    XU_EINTR,         // rt_task_unblock() called before release point
    XU_EPERM,         // cannot rt_task_wait_period() from this context
    XU_OTHER,         // unknown error code

    // RT-PREEMPT
    RTP_SIGXCPU,      // Missed scheduling deadline
    RTP_SIGNAL,       // some other signal
    RTP_DEADLINE_MISSED, // clock_gettime(CLOCK_MONOTONIC) returned 'too late'

    RTAPI_EXCEPTION_LAST,

} rtapi_exception_t;

#endif // _RTAPI_EXCEPTION_H
