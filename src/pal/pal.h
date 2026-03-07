/**
 * @file pal.h
 * @brief Platform Abstraction Layer (PAL) top-level header.
 *
 * Selects the correct platform-specific PAL implementation at compile time:
 * - When building as a Linux kernel module (`__KERNEL__` defined), includes
 *   `pal_kmod.h` which wraps Linux kernel APIs.
 * - When building for userspace (e.g. with `EC_USPACE_MASTER`), includes
 *   `pal_user.h` which wraps POSIX APIs.
 *
 * All drivers should include this header rather than either platform-specific
 * header directly, ensuring portability between kernel-module and userspace
 * EtherCAT master modes.
 *
 * @copyright Copyright (C) 2015-2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef _PAL_H_
#define _PAL_H_

#include <rtapi.h>
#include <rtapi_stdint.h>

// hack to identify LinuxCNC >= 2.8
#ifdef RTAPI_UINT64_MAX
#include <rtapi_mutex.h>
#endif

#ifdef __KERNEL__
  #include "pal_kmod.h"
#else
  #include "pal_user.h"
#endif

/**
 * @brief Portable wrapper for `rtapi_shmem_getptr`.
 *
 * LinuxCNC >= 2.8 (identified by `RTAPI_SERIAL >= 2`) changed the signature of
 * `rtapi_shmem_getptr` to accept a third `size` output parameter.  This macro
 * hides the difference so call sites compile against both versions.
 *
 * @param id  Shared-memory segment ID returned by `rtapi_shmem_new`.
 * @param ptr Pointer-to-pointer that will receive the mapped address.
 */
#if defined RTAPI_SERIAL && RTAPI_SERIAL >= 2
 #define lcec_rtapi_shmem_getptr(id, ptr) rtapi_shmem_getptr(id, ptr, NULL)
#else
 #define lcec_rtapi_shmem_getptr(id, ptr) rtapi_shmem_getptr(id, ptr)
#endif

#endif
