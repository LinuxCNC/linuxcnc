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

#include <rtapi_stdint.h>

#ifdef __KERNEL__
#include <linux/math64.h>
#define rtapi_div_u64_rem div_u64_rem
#define rtapi_div_u64 div_u64
#define rtapi_div_s64_rem div_s64_rem
#define rtapi_div_s64 div_s64
#else
static inline rtapi_u64 rtapi_div_u64_rem(rtapi_u64 dividend, rtapi_u32 divisor, rtapi_u32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline rtapi_u64 rtapi_div_u64(rtapi_u64 dividend, rtapi_u32 divisor) {
	return dividend / divisor;
}
static inline rtapi_s64 rtapi_div_s64_rem(rtapi_s64 dividend, rtapi_s32 divisor, rtapi_s32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline rtapi_s64 rtapi_div_s64(rtapi_s64 dividend, rtapi_s32 divisor) {
	return dividend / divisor;
}
#endif

#endif
