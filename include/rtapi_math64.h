/*
 * Copyright (C) 2013 Jeff Epler <jepler@unpythonic.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef RTAPI_MATH64_H
#define RTAPI_MATH64_H

#ifdef __KERNEL__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#include <asm/div64.h>
static inline __u64 rtapi_div_u64_rem(__u64 dividend, __u32 divisor, __u32 *remainder)
{
    *remainder = do_div(dividend, divisor);
    return dividend;
}
static inline __u64 rtapi_div_u64(__u64 dividend, __u32 divisor)
{
    __u32 remainder;
    return rtapi_div_u64_rem(dividend, divisor, &remainder);
}
static inline __s64 rtapi_div_s64_rem(__s64 dividend, __s32 divisor, __s32 *remainder)
{
    int sgn_dividend = (dividend < 0) ^ (divisor < 0);
    int sgn_remainder = (dividend < 0);
    if(dividend < 0) dividend = -dividend;
    if(divisor < 0) divisor = -divisor;
    *remainder = sgn_remainder
        ? -do_div(dividend, divisor) : do_div(dividend, divisor);
    return sgn_dividend ? -dividend : dividend;
}
static inline __s64 rtapi_div_s64(__s64 dividend, __s32 divisor)
{
	__s32 remainder;
	return rtapi_div_s64_rem(dividend, divisor, &remainder);
}
#else
#include <linux/math64.h>
#define rtapi_div_u64_rem div_u64_rem
#define rtapi_div_u64 div_u64
#define rtapi_div_s64_rem div_s64_rem
#define rtapi_div_s64 div_s64
#endif
#else
static inline __u64 rtapi_div_u64_rem(__u64 dividend, __u32 divisor, __u32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline __u64 rtapi_div_u64(__u64 dividend, __u32 divisor) {
	return dividend / divisor;
}
static inline __s64 rtapi_div_s64_rem(__s64 dividend, __s32 divisor, __s32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline __s64 rtapi_div_s64(__s64 dividend, __s32 divisor) {
	return dividend / divisor;
}
#endif

#endif
