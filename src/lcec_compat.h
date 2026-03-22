/**
 * @file lcec_compat.h
 * @brief Userspace-only compatibility shims replacing the PAL layer.
 *
 * Since the build now targets uspace only (linuxcnc unify-apis), this header
 * provides thin wrappers around standard POSIX/C-library APIs so the rest of
 * the driver can use the same platform-neutral interface it always has.
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

#ifndef _LCEC_COMPAT_H_
#define _LCEC_COMPAT_H_

#include <rtapi.h>
#include <rtapi_stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/**
 * @brief Allocate zeroed memory.
 *
 * Calls `malloc` and immediately zero-fills the returned block with
 * `memset`.
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
 * @param ptr Pointer to memory to free (may be NULL).
 */
#define lcec_free(ptr) free(ptr)

/**
 * @brief Retrieve the current wall-clock time.
 *
 * Wraps `gettimeofday` with a NULL timezone argument.
 *
 * @param x Pointer to a `struct timeval` to populate.
 */
#define lcec_gettimeofday(x) gettimeofday(x, NULL)

/**
 * @brief Compute a signed 64-bit modulo.
 *
 * @param val  The dividend (64-bit signed).
 * @param div  The divisor (unsigned long).
 * @return     The signed remainder of @p val divided by @p div.
 */
static inline long long lcec_mod_64(long long val, unsigned long div) {
  return val % div;
}

/**
 * @brief Portable wrapper for `rtapi_shmem_getptr`.
 *
 * The unify-apis LinuxCNC has a stable shmem API that always accepts a third
 * `size` output parameter.
 */
#define lcec_rtapi_shmem_getptr(id, ptr) rtapi_shmem_getptr(id, ptr)

#endif
