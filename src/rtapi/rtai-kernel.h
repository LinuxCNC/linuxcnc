/********************************************************************
* Description:  rtai.h
*               This file defines the differences specific to the
*               the RTAI thread system
********************************************************************/

#include <rtai_sched.h>		/* RT_TASK */

#define FLAVOR_FLAGS RTAI_KERNEL_FLAVOR_FLAGS // see rtapi_compat.h

/* rtapi_module.c */
#define RT_LINUX_USE_FPU


/* rtapi_time.c */
#define HAVE_RTAPI_MODULE_TIMER_STOP
#define HAVE_RTAPI_GET_TIME_HOOK
#define HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK

/* rtapi_task.c */

/* RTAI uses 0 as the highest priority; higher numbers are lower
   priority */
#define INVERSE_PRIO
#define PRIO_LOWEST 0xFFF
#define PRIO_HIGHEST 0

#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_SELF_HOOK

