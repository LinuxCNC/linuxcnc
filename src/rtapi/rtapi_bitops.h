//    Copyright © 2015 Jeff Epler <jepler@unpythonic.net>
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
#ifndef RTAPI_BITOPS_H
#define RTAPI_BITOPS_H
#if defined(__KERNEL__)
#include <asm/bitops.h>
#else
#include <limits.h>
#define RTAPI_LONG_BIT (CHAR_BIT * sizeof(unsigned long))
static __inline__ void set_bit(int nr, volatile void *addr) {
    size_t loff = nr / RTAPI_LONG_BIT;
    size_t boff = nr % RTAPI_LONG_BIT;
    unsigned long *laddr = (unsigned long*)addr;
    __sync_fetch_and_or(laddr + loff, 1lu << boff);
}

static __inline__ int test_bit(int nr, const volatile void *addr) {
    size_t loff = nr / RTAPI_LONG_BIT;
    size_t boff = nr % RTAPI_LONG_BIT;
    unsigned long *laddr = (unsigned long*)addr;
    return (laddr[loff] & (1lu << boff)) != 0;
}

static __inline__ void clear_bit(int nr, volatile void *addr) {
    size_t loff = nr / RTAPI_LONG_BIT;
    size_t boff = nr % RTAPI_LONG_BIT;
    unsigned long *laddr = (unsigned long*)addr;
    __sync_fetch_and_and(laddr + loff, ~(1lu << boff));
}

static __inline__ long test_and_set_bit(int nr, volatile void *addr) {
    size_t loff = nr / RTAPI_LONG_BIT;
    size_t boff = nr % RTAPI_LONG_BIT;
    unsigned long *laddr = (unsigned long*)addr;
    unsigned long oldval = __sync_fetch_and_or(laddr + loff, 1lu << boff);
    return (oldval & (1lu << boff)) != 0;
}

static __inline__ long test_and_clear_bit(int nr, volatile void *addr) {
    size_t loff = nr / RTAPI_LONG_BIT;
    size_t boff = nr % RTAPI_LONG_BIT;
    unsigned long *laddr = (unsigned long*)addr;
    unsigned long oldval = __sync_fetch_and_and(laddr + loff, ~(1lu << boff));
    return (oldval & (1lu << boff)) != 0;
}
#endif
#endif
