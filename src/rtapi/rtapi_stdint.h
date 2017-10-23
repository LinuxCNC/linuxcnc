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
typedef u8 rtapi_u8;
typedef u16 rtapi_u16;
typedef u32 rtapi_u32;
typedef u64 rtapi_u64;
#else
#include <inttypes.h>
typedef int8_t rtapi_s8;
typedef int16_t rtapi_s16;
typedef int32_t rtapi_s32;
typedef int64_t rtapi_s64;
typedef uint8_t rtapi_u8;
typedef uint16_t rtapi_u16;
typedef uint32_t rtapi_u32;
typedef uint64_t rtapi_u64;
#endif

#endif
