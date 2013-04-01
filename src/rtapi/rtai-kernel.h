/********************************************************************
* Description:  rtai.h
*               This file defines the differences specific to the
*               the RTAI thread system
********************************************************************/

#include <rtai.h>
#include <rtai_sched.h>
#if RTAI > 2
#include <rtai_sem.h>
#endif
#include <rtai_shm.h>
#include <rtai_fifos.h>

#ifdef MODULE
#include <linux/delay.h>  // udelay()
#endif 

/* rtapi_module.c */
#define RT_LINUX_USE_FPU


/* rtapi_time.c */
#define HAVE_RTAPI_MODULE_TIMER_STOP
#define HAVE_RTAPI_GET_TIME_HOOK
#define HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK


/* rtapi_msg.c */
// RTAI uses rt_printk() instead of printk()
#define RTAPI_PRINTK rt_printk


/* rtapi_task.c */

/* RTAI uses 0 as the highest priority; higher numbers are lower
   priority */
#define INVERSE_PRIO
#define PRIO_LOWEST 0xFFF
#define PRIO_HIGHEST 0

#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_SELF_HOOK
