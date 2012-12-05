/********************************************************************
* Description:  xenomai-kernel.h
*               This file defines the differences specific to the
*               the Xenomai kernel thread system
********************************************************************/

#define REV_CODE 2

#include <nucleus/types.h>	/* XNOBJECT_NAME_LEN, RTIME */
#include <native/task.h>	/* RT_TASK, rt_task_*() */
#include <native/heap.h>	/* RT_HEAP */
/* this needs to be fixed in rtapi_common.h
#undefine RTAPI_NAME_LEN
#define RTAPI_NAME_LEN XNOBJECT_NAME_LEN
*/

#define MASTER_HEAP "rtapi-heap"

/* add some fields to rtapi_data_t:
   rt_wait_error:		release point missed
   rt_last_overrun:		last number of overruns reported by Xenomai
   rt_total_overruns:		total number of overruns reported by Xenomai */
#define THREAD_RTAPI_DATA	\
    int rt_wait_error;		\
    int rt_last_overrun;	\
    int rt_total_overruns
/* ...and a hook to initialize it */
#define HAVE_INIT_RTAPI_DATA_HOOK


/* Priority functions settings */

// Xenomai rt_task priorities are 0: lowest .. 99: highest
#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99


/* rtapi_module.c */
/* rt_linux_use_fpu informs the scheduler that floating point
   arithmetic operations will be used also by foreground Linux
   processes, i.e. the Linux kernel itself (unlikely) and any of its
   processes. */
// FIXME unsure how this relates to Xenomai
// #define RT_LINUX_USE_FPU

#define HAVE_RTAPI_MODULE_INIT_HOOK


/* rtapi_task.c */
#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_START_HOOK
#define NO_RTAPI_TASK_WRAPPER


/* rtapi_io hooks */


/* rtapi_time.c */
#ifdef RTAPI
#define HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK
#define HAVE_RTAPI_GET_TIME_HOOK
#define HAVE_RTAPI_GET_CLOCKS_HOOK
#endif
