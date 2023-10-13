//    Copyright 2015 Jeff Epler
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
#ifndef RTAPI_ATOMIC_H
#define RTAPI_ATOMIC_H

#if defined(__GNUC__) && ((__GNUC__ << 8) | __GNUC_MINOR__) >= 0x409
#define RTAPI_USE_STDATOMIC
#elif defined(__STDC_VERSION__) && __STDC_VERSION > 201112L
#define RTAPI_USE_STDATOMIC
#endif

#ifdef RTAPI_USE_STDATOMIC
#include <stdatomic.h>
#else

enum memory_order {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
};

#define atomic_store(obj, desired) atomic_store_explicit((obj), (desired), memory_order_seq_cst)
#define atomic_load(obj) atomic_load_explicit((obj), memory_order_seq_cst)

// note that in this implementation, only one level of synchronization is supported, equivalent to memory_order_seq_cst
#define atomic_store_explicit(obj, desired, order) \
    ({ (void)order; __sync_synchronize(); *(obj) = (desired); (void)0; })

#define atomic_load_explicit(obj, order) \
    ({ (void)order; __typeof__(*(obj)) v = *(obj); __sync_synchronize(); v; })

#endif

#endif
