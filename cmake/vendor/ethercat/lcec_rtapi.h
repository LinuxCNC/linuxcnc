//
//    Copyright (C) 2015 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#ifndef _LCEC_RTAPI_H_
#define _LCEC_RTAPI_H_

#include <rtapi.h>
#include <rtapi_stdint.h>

// hack to identify LinuxCNC >= 2.8
#ifdef RTAPI_UINT64_MAX
#include <rtapi_mutex.h>
#endif

#ifdef __KERNEL__
  #include "lcec_rtapi_kmod.h"
#else
  #include "lcec_rtapi_user.h"
#endif

#if defined RTAPI_SERIAL && RTAPI_SERIAL >= 2
 #define lcec_rtapi_shmem_getptr(id, ptr) rtapi_shmem_getptr(id, ptr, NULL)
#else
 #define lcec_rtapi_shmem_getptr(id, ptr) rtapi_shmem_getptr(id, ptr)
#endif

#endif

