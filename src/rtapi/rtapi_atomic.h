//    Copyright 2015 Jeff Epler
//    Copyright 2026 B.Stultiens
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
#ifndef __LINUXCNC_RTAPI_ATOMIC_H
#define __LINUXCNC_RTAPI_ATOMIC_H

#if defined(__cplusplus)

// We use C++20 and that has all the atomics we need
#include <atomic>

#else // defined(__cplusplus)

// Standard C, we require C11 or better
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define RTAPI_USE_STDATOMIC
#elif defined(__GNUC__) && ((__GNUC__ << 8) | __GNUC_MINOR__) >= 0x409
#define RTAPI_USE_STDATOMIC
#endif

#if defined(RTAPI_USE_STDATOMIC)
#include <stdatomic.h>

#if defined(__STDC_NO_ATOMICS__)
#error "Your compiler/libc has set __STDC_NO_ATOMICS__ and atomics are required."
#endif

#else // defined(RTAPI_USE_STDATOMIC)

#error "Old compiler has no C11 atomics. Please upgrade your compiler to support C11 or better."

#endif // defined(RTAPI_USE_STDATOMIC)

#endif // defined(__cplusplus)

#endif
