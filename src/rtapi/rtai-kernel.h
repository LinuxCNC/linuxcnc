/********************************************************************
* Description:  rtai.h
*               This file defines the differences specific to the
*               the RTAI thread system
*
*     Copyright 2006-2010 Various Authors
* 
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; either version 2 of the License, or
*     (at your option) any later version.
* 
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
* 
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
********************************************************************/

#include <rtai_sched.h>		/* RT_TASK */

#define FLAVOR_FLAGS RTAI_KERNEL_FLAVOR_FLAGS // see rtapi_compat.h

/* rtapi_module.c */
#define RT_LINUX_USE_FPU


/* rtapi_time.c */
#define HAVE_RTAPI_MODULE_TIMER_STOP
#define HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK
#ifdef RTAPI  // no rt_get_cpu_time_ns() in RTAI userland
#define HAVE_RTAPI_GET_TIME_HOOK
#endif

/* rtapi_task.c */

/* RTAI uses 0 as the highest priority; higher numbers are lower
   priority */
#define INVERSE_PRIO
#define PRIO_LOWEST 0xFFF
#define PRIO_HIGHEST 0

#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_SELF_HOOK

