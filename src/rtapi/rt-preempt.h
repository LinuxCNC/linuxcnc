/********************************************************************
* Description:  rt-preempt.h
*               This file defines the differences specific to the
*               the RT_PREEMPT thread system
*
*		It should be included in rtapi_common.h
*
*
* Copyright (C) 2012, 2013 Michael Büsch <m AT bues DOT CH>, 
*                          John Morris <john AT zultron DOT com>
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
*                           TASK FUNCTIONS                             *
************************************************************************/

#include "config.h"
#include <sched.h>		// sched_get_priority_*()
#include <pthread.h>		/* pthread_* */

#if THREAD_FLAVOR_ID == RTAPI_POSIX_ID
#define FLAVOR_FLAGS POSIX_FLAVOR_FLAGS  // see rtapi_compat.h
#endif

#if THREAD_FLAVOR_ID == RTAPI_RT_PREEMPT_ID
#define FLAVOR_FLAGS  RTPREEMPT_FLAVOR_FLAGS
#endif

/* rtapi_task.c */
#define PRIO_LOWEST sched_get_priority_min(SCHED_FIFO)
#define PRIO_HIGHEST sched_get_priority_max(SCHED_FIFO)

#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_TASK_DELETE_HOOK
#define HAVE_RTAPI_TASK_STOP_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_SELF_HOOK
#define HAVE_RTAPI_TASK_UPDATE_STATS_HOOK

#if !defined(__i386__) && !defined(__x86_64__)
#define HAVE_RTAPI_GET_CLOCKS_HOOK // needed for e.g. ARM, see rtapi_time.c
#endif


/* misc */
#define HAVE_RTAPI_TASK_FREE
#define HAVE_DROP_RESTORE_PRIVS
