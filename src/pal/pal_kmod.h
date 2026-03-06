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

/**
 * @file pal_kmod.h
 * @brief Kernel-module PAL implementation.
 *
 * Provides thin wrappers around Linux kernel APIs so that the rest of the
 * LinuxCNC EtherCAT driver can use a single, platform-neutral interface.
 * This header is included automatically by `pal.h` when `__KERNEL__` is
 * defined (i.e. when the driver is compiled as a real-time kernel module).
 *
 * @note Do not include this header directly; use `pal.h` instead.
 */

#ifndef _LCEC_PAL_KMOD_H_
#define _LCEC_PAL_KMOD_H_

#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/math64.h>

/**
 * @brief Allocate zeroed kernel memory.
 *
 * Wraps `kzalloc` with `GFP_KERNEL` flags so that the allocation is
 * guaranteed to be zero-initialised, matching the behaviour of the
 * userspace `lcec_zalloc`.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory, or NULL on failure.
 */
#define lcec_zalloc(size) kzalloc(size, GFP_KERNEL)

/**
 * @brief Free kernel memory previously allocated with `lcec_zalloc`.
 *
 * Wraps `kfree`.
 *
 * @param ptr Pointer to memory to free (may be NULL).
 */
#define lcec_free(ptr) kfree(ptr)

/**
 * @brief Retrieve the current wall-clock time (kernel variant).
 *
 * Wraps `do_gettimeofday`, which fills a `struct timeval` with the current
 * seconds and microseconds since the Unix epoch.
 *
 * @param x Pointer to a `struct timeval` to populate.
 */
#define lcec_gettimeofday(x) do_gettimeofday(x)

/**
 * @brief Compute a signed 64-bit modulo using kernel-safe arithmetic.
 *
 * The kernel does not support the plain `%` operator on 64-bit values on
 * 32-bit architectures.  This function uses `div_s64_rem` from
 * `<linux/math64.h>` to perform the division safely and return only the
 * remainder.
 *
 * @param val  The dividend (64-bit signed).
 * @param div  The divisor (unsigned long).
 * @return     The signed remainder of @p val divided by @p div.
 */
static inline long long lcec_mod_64(long long val, unsigned long div) {
  s32 rem;
  div_s64_rem(val, div, &rem);
  return rem;
}

#endif

