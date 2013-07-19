/********************************************************************
* Description:  xenomai-kernel.h
*               This file defines the differences specific to the
*               the Xenomai kernel thread system
********************************************************************/

#include <native/task.h>	/* RT_TASK, rt_task_*() */

/* rtapi_common.c */
// output threadsystem-specific detail when doing a backtrace
#define HAVE_RTAPI_BACKTRACE_HOOK

/* Priority functions settings */

// Xenomai rt_task priorities are 0: lowest .. 99: highest
#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99


/* rtapi_module.c */
#define HAVE_RTAPI_MODULE_INIT_HOOK

/* rtapi_task.c */
#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_SELF_HOOK
#define HAVE_RTAPI_TASK_UPDATE_STATS_HOOK


/* rtapi_io hooks */


/* rtapi_time.c */
#ifdef RTAPI
#define HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK
#define HAVE_RTAPI_GET_TIME_HOOK
#define HAVE_RTAPI_GET_CLOCKS_HOOK
#endif

