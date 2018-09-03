/********************************************************************
 * Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
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

// support for thread style autodetection based on digging in the kernel
// and userland libraries

#include "rtapi_bitops.h"

// utsname.release matches telltale strings (soft)
#define UTSNAME_REL_RTAI     RTAPI_BIT(0)
#define UTSNAME_REL_RT       RTAPI_BIT(1)
#define UTSNAME_REL_XENOMAI  RTAPI_BIT(2)

// utsname.version matches "PREEMPT RT" (hard)
#define UTSNAME_VER_RT_PREEMPT RTAPI_BIT(3)

// utsname.version matches "#rtai" (soft)
#define UTSNAME_VER_RTAI     RTAPI_BIT(4)

#define XENO_RTHEAP_FOUND     RTAPI_BIT(5) // /dev/rtheap seen (hard)
#define XENO_PROCENTRY_FOUND  RTAPI_BIT(6) // /proc/xenomai seen (hard)

#define HAS_HIRES_TIMERS      RTAPI_BIT(7) // sanitary - assume vanilla if false.
#define SYS_KERNEL_REALTIME_FOUND RTAPI_BIT(8) // RT_PREEMPT (hard)

#define DEV_RTAI_SHM_FOUND RTAPI_BIT(9) // RTAI (hard)

// verify the Xenomai userland libraries are present and make sense (sanitary for xenomai)
#define XENO_LIBXENOMAI       RTAPI_BIT(10) // can dlopen("libxenomai.so")
#define XENO_LIBNATIVE        RTAPI_BIT(11) // can dlopen("libnative.so")
#define XENO_LIBSYMBOL        RTAPI_BIT(12) // can resolve symbol in "libnative.so"

// hard evidence for an ipipe patch in place (RTAI and Xenomai):
#define HAS_PROC_IPIPE        RTAPI_BIT(13) // /proc/ipipe exists and is a directory

// Exists on RTAI and Xenomai
#define PROC_IPIPE "/proc/ipipe"

// really in nucleus/heap.h but we rather get away with minimum include files
#ifndef XNHEAP_DEV_NAME
#define XNHEAP_DEV_NAME  "/dev/rtheap"
#endif

// test for "/proc/xenomai" existance and a directory
#define PROC_XENOMAI  "/proc/xenomai"

// dlopen() these shared libraries
#define LIBXENOMAI "libxenomai.so"
#ifdef XENOMAI_V2
#define LIBNATIVE "libnative.so"
#else
#define LIBNATIVE "libalchemy.so"
#endif
// and dig for LIBNATIVE_SYM
#define LIBNATIVE_SYM "rt_task_create"

// if this exists, and contents is '1', it's RT_PREEMPT
#define PREEMPT_RT_SYSFS "/sys/kernel/realtime"

// dev/rtai_shm visible only after 'realtime start'
#define DEV_RTAI_SHM "/dev/rtai_shm"

// These exist on Xenomai but not on RTAI - regular files:
#define PROC_IPIPE_XENOMAI "/proc/ipipe/Xenomai" 
#define XENO_GID_SYSFS "/sys/module/xeno_nucleus/parameters/xenomai_gid"

// hard evidence for Xenomai
#define HAS_PROC_IPIPE_XENOMAI    RTAPI_BIT(14) // /proc/ipipe/Xenomai exists
#define HAS_XENO_GID_SYSFS        RTAPI_BIT(15)

int rtapi_kdetect(unsigned long *feat);
