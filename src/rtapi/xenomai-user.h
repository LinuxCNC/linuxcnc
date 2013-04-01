/********************************************************************
* Description:  xenomai-user.h
*               This file defines the differences specific to the
*               the Xenomai user land thread system
********************************************************************/

#include <native/task.h>	/* RT_TASK, rt_task_*() */
#include <nucleus/types.h>	/* XNOBJECT_NAME_LEN */


/* rtapi_common.h */

/* this needs to be fixed
#undefine RTAPI_NAME_LEN
#define RTAPI_NAME_LEN XNOBJECT_NAME_LEN
*/

/* add some fields to rtapi_data_t:
   rt_wait_error:		release point missed
   rt_last_overrun:		last number of overruns reported by Xenomai
   rt_total_overruns:		total number of overruns reported by Xenomai */
#define THREAD_RTAPI_DATA	\
    int rt_wait_error;		\
    int rt_last_overrun;	\
    int rt_total_overruns
/* ...and a hook to initialize it: */
#define HAVE_INIT_RTAPI_DATA_HOOK

#define THREAD_TASK_DATA RT_TASK *self


/* rtapi_task.c */
// Xenomai rt_task priorities are 0: lowest .. 99: highest
#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99

#define HAVE_RTAPI_TASK_DELETE_HOOK
#define HAVE_RTAPI_TASK_STOP_HOOK
#define HAVE_RTAPI_TASK_PAUSE_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_RESUME_HOOK
#define HAVE_RTAPI_TASK_SELF_HOOK


/* rtapi_time.c */
#define RTAPI_TIME_NO_CLOCK_MONOTONIC  // Xenomai has its own time features
#define HAVE_RTAPI_GET_TIME_HOOK
#define HAVE_RTAPI_GET_CLOCKS_HOOK
