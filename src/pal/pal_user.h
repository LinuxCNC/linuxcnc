/**
 * @file pal_user.h
 * @brief Userspace PAL implementation.
 *
 * Provides thin wrappers around standard POSIX / C-library APIs so that the
 * rest of the LinuxCNC EtherCAT driver can use the same platform-neutral
 * interface as the kernel-module build.  This header is included automatically
 * by `pal.h` when `__KERNEL__` is *not* defined (i.e. when building for the
 * userspace EtherCAT master).
 *
 * @note Do not include this header directly; use `pal.h` instead.
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

#ifndef _LCEC_PAL_USER_H_
#define _LCEC_PAL_USER_H_

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sched.h>

/**
 * @brief Allocate zeroed memory (userspace variant).
 *
 * Calls `malloc` and immediately zero-fills the returned block with
 * `memset`, mirroring the behaviour of `kzalloc` in the kernel build.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to zeroed memory on success, or NULL if `malloc` fails.
 */
static inline void *lcec_zalloc(size_t size) {
  void *p = malloc(size);
  if (p) memset(p, 0, size);
  return p;
}

/**
 * @brief Free memory previously allocated with `lcec_zalloc`.
 *
 * Wraps the standard `free`.
 *
 * @param ptr Pointer to memory to free (may be NULL).
 */
#define lcec_free(ptr) free(ptr)

/**
 * @brief Retrieve the current wall-clock time (userspace variant).
 *
 * Wraps `gettimeofday` with a NULL timezone argument, populating a
 * `struct timeval` with seconds and microseconds since the Unix epoch.
 *
 * @param x Pointer to a `struct timeval` to populate.
 */
#define lcec_gettimeofday(x) gettimeofday(x, NULL)

/**
 * @brief Compute a signed 64-bit modulo (userspace variant).
 *
 * In userspace the standard `%` operator handles 64-bit values correctly
 * on all supported architectures, so no kernel helper is required.
 *
 * @param val  The dividend (64-bit signed).
 * @param div  The divisor (unsigned long).
 * @return     The signed remainder of @p val divided by @p div.
 */
static inline long long lcec_mod_64(long long val, unsigned long div) {
  return val % div;
}

#endif
