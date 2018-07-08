//    Copyright 2014 Jeff Epler
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
#ifndef RTAPI_STDINT_H
#define RTAPI_STDINT_H

#ifdef __KERNEL__
#include <asm/types.h>
typedef s8 rtapi_s8;
typedef s16 rtapi_s16;
typedef s32 rtapi_s32;
typedef s64 rtapi_s64;
typedef long rtapi_intptr_t;
typedef u8 rtapi_u8;
typedef u16 rtapi_u16;
typedef u32 rtapi_u32;
typedef u64 rtapi_u64;
typedef unsigned long rtapi_uintptr_t;

#define RTAPI_INT8_MAX (127)
#define RTAPI_INT8_MIN (-128)
#define RTAPI_UINT8_MAX (255)

#define RTAPI_INT16_MAX (32767)
#define RTAPI_INT16_MIN (-32768)
#define RTAPI_UINT16_MAX (65535)

#define RTAPI_INT32_MAX (2147483647)
#define RTAPI_INT32_MIN (-2147483647-1)
#define RTAPI_UINT32_MAX (4294967295ul)

#define RTAPI_INT64_MAX (9223372036854775807)
#define RTAPI_INT64_MIN (-9223372036854775807-1)
#define RTAPI_UINT64_MAX (18446744073709551615ull)
#else
#include <inttypes.h>

typedef int8_t rtapi_s8;
typedef int16_t rtapi_s16;
typedef int32_t rtapi_s32;
typedef int64_t rtapi_s64;
typedef intptr_t rtapi_intptr_t;
typedef uint8_t rtapi_u8;
typedef uint16_t rtapi_u16;
typedef uint32_t rtapi_u32;
typedef uint64_t rtapi_u64;
typedef uintptr_t rtapi_uintptr_t;

#define RTAPI_INT8_MAX INT8_MAX
#define RTAPI_INT8_MIN INT8_MIN
#define RTAPI_UINT8_MAX UINT8_MAX

#define RTAPI_INT16_MAX INT16_MAX
#define RTAPI_INT16_MIN INT16_MIN
#define RTAPI_UINT16_MAX UINT16_MAX

#define RTAPI_INT32_MAX INT32_MAX
#define RTAPI_INT32_MIN INT32_MIN
#define RTAPI_UINT32_MAX UINT32_MAX

#define RTAPI_INT64_MAX INT64_MAX
#define RTAPI_INT64_MIN INT64_MIN
#define RTAPI_UINT64_MAX UINT64_MAX
#endif

#endif
