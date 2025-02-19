#ifndef RTAPI_MUTEX_H
#define RTAPI_MUTEX_H

//    Copyright 2004-2015 Jeff Epler, John Kasunich, Paul Corner, and other
//    authors
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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

/**
 * @file
 * @brief Lightweight mutex functions.
 *
 * These functions provide a very simple way to do mutual exclusion around
 * shared resources. They do _not_ replace semaphores, and can result in
 * significant slowdowns if contention is severe. However, unlike semaphores
 * they can be used from both user and kernel space. The rtapi_mutex_try() and
 * rtapi_mutex_give() are non-blocking, and can be used anywhere. The
 * rtapi_mutex_get() blocks if the mutex is already taken and should not be used
 * in realtime code.
 */


#if defined(__KERNEL__)
#include <linux/sched.h>	/* for blocking when needed */
#else
#include <sched.h>		/* for blocking when needed */
#endif
#include "rtapi_bitops.h"	/* atomic bit ops for lightweight mutex */

typedef unsigned long rtapi_mutex_t;


/**
 * @brief Releases the mutex.
 *
 * The release is unconditional, even if the caller doesn't have the mutex, it
 * will be released.
 * @param mutex Pointer to the @c mutex.
 */
    static __inline__ void rtapi_mutex_give(unsigned long *mutex) {
	test_and_clear_bit(0, mutex);
    }
/**
 * @brief Non-blocking attempt to get the mutex.
 *
 * The programmer is responsible for "doing the right thing" when it returns
 * non-zero. "Doing the right thing" almost certainly means doing something
 * that will yield the CPU, so that whatever other process has the mutex gets a
 * chance to release it.
 * @param mutex Pointer to the @c mutex.
 * @return If the mutex is available, it returns 0, and the mutex is no longer
 *         available. Otherwise, it returns a nonzero value indicating that
 *         someone else has the mutex.
 */
    static __inline__ int rtapi_mutex_try(unsigned long *mutex) {
	return test_and_set_bit(0, mutex);
    }

/**
 * @brief Blocking attempt to gGet the mutex.
 *
 * This function will block if the mutex is not available. Because of this,
 * calling it from a realtime task is a "very bad" thing to do.
 * @param mutex Pointer to the @c mutex.
 * @note Can not be used in realtime code.
 */
    static __inline__ void rtapi_mutex_get(unsigned long *mutex) {
	while (test_and_set_bit(0, mutex)) {
#if defined(__KERNEL__)
	    schedule();
#else
	    sched_yield();
#endif
	}
    }


#endif
