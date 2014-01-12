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

#ifndef _RTAPI_SHMKEYS_H
#define _RTAPI_SHMKEYS_H

// the single place for shared memory keys

// convention: allocate a new key such that its
// most significant byte is zero, which is used for 
// instance management. 
// the rtapi_shmem_new code actually ignores the MSB
// and replaces it be the rtapi_instance variable, so we
// dont have to do the big shm key rename right away; just
// make sure the LSB 24bit differ, which is currently the case.
// this convention is also used for the global shm segment.

// actual shm keys are constructed as follows:

#define OS_KEY(key, instance) (( key & 0x00ffffff) | ((instance << 24) & 0xff000000))
#define INSTANCE_OF(key)  (((key & 0xff000000) >> 24) & 0x000000ff)

// formerly emcmotcfg.h
#define DEFAULT_MOTION_SHMEM_KEY 0x00000064

// the global segment shm key
#define GLOBAL_KEY  0x00154711     // key for GLOBAL 

// from scope_shm.h
#define SCOPE_SHM_KEY  0x000CF406

// from streamer.h
#define STREAMER_SHMEM_KEY 	0x00535430
#define SAMPLER_SHMEM_KEY	0x00534130

// from hal/classicladder/arrays.c
#define CL_SHMEM_KEY 0x004C522b // "CLR+"

// from hal/hal_priv.h
#define HAL_KEY   0x00414C32	/* key used to open HAL shared memory */

// from rtapi/rtapi_common.h
#define RTAPI_KEY   0x00280A48	/* key used to open RTAPI shared memory */

// RTAPI rings
#define RTAPI_RING_SHM_KEY 0x00415000  

// filename format in /dev/shm for POSIX shm_open() names:
// 'linuxcnc-<rtapi_instance>-<key>
// this makes it easier to delete all leftover segments in scripts/realtime:Unload
#define SHM_FMT "/linuxcnc-%d-%8.8x"

#endif // _RTAPI_SHMKEYS_H
