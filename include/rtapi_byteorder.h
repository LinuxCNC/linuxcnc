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
#ifndef RTAPI_BYTEORDER_H
#define RTAPI_BYTEORDER_H
#ifdef __KERNEL__
#include <asm/byteorder.h>
#ifdef __BIG_ENDIAN
#define RTAPI_BIG_ENDIAN 1
#define RTAPI_LITTLE_ENDIAN 0
#define RTAPI_FLOAT_BIG_ENDIAN 1
#else
#define RTAPI_LITTLE_ENDIAN 1
#define RTAPI_BIG_ENDIAN 0
#define RTAPI_FLOAT_BIG_ENDIAN 0
#endif
#else
#include <endian.h>
#define RTAPI_BIG_ENDIAN (__BYTE_ORDER == __BIG_ENDIAN)
#define RTAPI_LITTLE_ENDIAN (__BYTE_ORDER == __LITTLE_ENDIAN)
#define RTAPI_FLOAT_BIG_ENDIAN (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
#endif
#endif
