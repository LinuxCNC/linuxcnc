/********************************************************************
* Description:  xenomai-kernel.h
*               This file defines the differences specific to the
*               the Xenomai kernel thread system
*
* Copyright (C) 2012 - 2013 John Morris <john AT zultron DOT com>
*                           Michael Haberler <license AT mah DOT priv DOT at>
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

#define FLAVOR_FLAGS XENOMAI_KERNEL_FLAVOR_FLAGS // see rtapi_compat.h

#include "xenomai-common.h"
#include XENOMAI_INCLUDE(task.h)	/* RT_TASK, rt_task_*() */

/* rtapi_common.c */

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

