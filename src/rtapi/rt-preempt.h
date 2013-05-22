/********************************************************************
* Description:  rt-preempt.h
*               This file defines the differences specific to the
*               the RT_PREEMPT thread system
*
*		It should be included in rtapi_common.h
********************************************************************/

/***********************************************************************
*                           TASK FUNCTIONS                             *
************************************************************************/

#include "config.h"
#include <sched.h>		// sched_get_priority_*()
#include <pthread.h>		/* pthread_* */




/* rtapi_task.c */
#define PRIO_LOWEST sched_get_priority_min(SCHED_FIFO)
#define PRIO_HIGHEST sched_get_priority_max(SCHED_FIFO)

#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_TASK_DELETE_HOOK
#define HAVE_RTAPI_TASK_STOP_HOOK
#define HAVE_RTAPI_WAIT_HOOK


/* misc */
#define HAVE_RTAPI_TASK_FREE
#define HAVE_DROP_RESTORE_PRIVS

/* rtapi_main.c */
#define HAVE_RTAPI_MODULE_INIT_HOOK   // arm SGXCPU handler
