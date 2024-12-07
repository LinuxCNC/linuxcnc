/*
 * This is a component for RaspberryPi support for linuxcnc.
 * Copyright (c) 2024 B.Stultiens <lcnc@vagrearg.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <https://www.gnu.org/licenses/>.
 */
#ifndef HAL_HM2_HWREGACCESS_H
#define HAL_HM2_HWREGACCESS_H

/* Register access modifiers */
#if !defined(__I) && !defined(__O) && !defined(__IO)
#ifdef __cplusplus
#define __I  volatile		/* read only permission */
#else
#define __I  volatile const	/* read only permission */
#endif
#define __O	 volatile		/* write only permission */
#define __IO volatile		/* read/write permission */
#else
#error "Possible define collision for __I, __O and __IO"
#endif

/* Forced inline expansion */
#define HWREGACCESS_ALWAYS_INLINE	__attribute__((always_inline))

/*
 * Synchronisation primitives
 * - rmb(): Read memory barrier
 * - wmb(): Write memory barrier
 */
HWREGACCESS_ALWAYS_INLINE static inline void rmb(void) { __sync_synchronize(); }
HWREGACCESS_ALWAYS_INLINE static inline void wmb(void) { __sync_synchronize(); }

/*
 * Synchronized read and write to peripheral memory.
 * Ensures coherency between cores, cache and peripherals
 */
HWREGACCESS_ALWAYS_INLINE static inline uint32_t reg_rd(const volatile void *addr)
{
	uint32_t val;
	val = *(volatile uint32_t *)addr;
	rmb();
	return val;
}

HWREGACCESS_ALWAYS_INLINE static inline void reg_wr(const volatile void *addr, uint32_t val)
{
	wmb();
	*(volatile uint32_t *)addr = val;
}

/*
 * These are the _unsynchronised_ versions. These may limit the latency of
 * reads and writes. For example, PCIe transactions may possibly be pipe-lined
 * with multiple subsequent raw read/write calls.
 */
HWREGACCESS_ALWAYS_INLINE static inline uint32_t reg_rd_raw(const volatile void *addr)
{
	uint32_t val;
	val = *(volatile uint32_t *)addr;
	return val;
}

HWREGACCESS_ALWAYS_INLINE static inline void reg_wr_raw(const volatile void *addr, uint32_t val)
{
	*(volatile uint32_t *)addr = val;
}

#endif
// vim: ts=4
