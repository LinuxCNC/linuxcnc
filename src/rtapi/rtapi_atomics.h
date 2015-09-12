/********************************************************************
 * Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ********************************************************************/

// basic atomic load/store/CAS/increment operations
// memory barrier primitives - see https://www.kernel.org/doc/Documentation/memory-barriers.txt


#ifndef _RTAPI_ATOMICS_H
#define _RTAPI_ATOMICS_H

#include "config.h" // HAVE_CK
#include "rtapi_int.h"
#include "hal_types.h"


#ifdef HAVE_CK

// use concurrencykit.org primitives
#include <ck_pr.h>

static inline void rtapi_store_u8(hal_u8_t *target, hal_u8_t value)
{
     ck_pr_store_8(target, value);
}

static inline void rtapi_store_u32(hal_u32_t *target, hal_u32_t value)
{
     ck_pr_store_32(target, value);
}

static inline void rtapi_store_s32(hal_s32_t *target, hal_s32_t value)
{
     ck_pr_store_int(target, value);
}

static inline void rtapi_store_ptr(void **target, void *value)
{
    ck_pr_store_ptr(target, value);
}

static inline hal_u8_t rtapi_load_u8(const hal_u8_t *target)
{
    return ck_pr_load_8(target);
}

static inline hal_u32_t rtapi_load_u32(const hal_u32_t *target)
{
    return ck_pr_load_32(target);
}

static inline hal_s32_t rtapi_load_s32(const hal_s32_t *target)
{
    return ck_pr_load_int(target);
}

// takes the _address_ of a pointer, and returns the pointer atomically
static inline void * rtapi_load_ptr(void **target)
{
    return ck_pr_load_ptr(target);
}

static inline void rtapi_add_s32(hal_s32_t *target, const hal_s32_t delta)
{
    ck_pr_faa_int(target, delta);
}

static inline void rtapi_add_u32(hal_u32_t *target, const hal_u32_t delta)
{
    ck_pr_faa_uint(target, delta);
}

#if defined(CK_F_PR_LOAD_64)
static inline hal_u64_t rtapi_load_u64(const hal_u64_t *target)
{
    return ck_pr_load_64((hal_u64_t *)target);
}

static inline hal_s64_t rtapi_load_s64(const hal_s64_t *target)
{
    return ck_pr_load_64((const hal_u64_t *)target);
}
#endif

#if defined(CK_F_PR_STORE_64)
static inline void rtapi_store_u64(hal_u64_t *target, const hal_u64_t value)
{
    ck_pr_store_64(target, value);
}

static inline void rtapi_store_s64(hal_s64_t *target, const hal_s64_t value)
{
    ck_pr_store_64((hal_u64_t *)target, value);
}
#endif

#if defined(CK_F_PR_INC_64)
static inline void rtapi_inc_u64(hal_u64_t *target)
{
    ck_pr_inc_64(target);
}
#endif

static inline bool rtapi_cas_u8(hal_u8_t *target, hal_u8_t old_value, hal_u8_t new_value)
{
     return ck_pr_cas_8(target, old_value, new_value);
}

static inline bool rtapi_cas_u32(hal_u32_t *target, hal_u32_t old_value, hal_u32_t new_value)
{
     return ck_pr_cas_32(target, old_value, new_value);
}

static inline bool rtapi_cas_s32(hal_s32_t *target, hal_s32_t old_value, hal_s32_t new_value)
{
     return ck_pr_cas_int(target, old_value, new_value);
}

#if defined(CK_F_PR_CAS_64)
static inline bool rtapi_cas_u64(hal_u64_t *target, hal_u64_t old_value, hal_u64_t new_value)
{
     return ck_pr_cas_64(target, old_value, new_value);
}
#endif

#define	rtapi_smp_rmb() ck_pr_fence_load()
#define	rtapi_smp_wmb() ck_pr_fence_store()
#define	rtapi_smp_mb()  ck_pr_fence_memory()

#endif

// use gcc intrinsics until the x86 flavor of ck supports 64bit ops,
// or if ck is not used

#if !defined(HAVE_CK) || !defined(CK_F_PR_LOAD_64)
static inline hal_u64_t rtapi_load_u64(const hal_u64_t *target)
{
    hal_u64_t v;
    __atomic_load(target, &v, RTAPI_MEMORY_MODEL);
    return v;
}

static inline hal_s64_t rtapi_load_s64(const hal_s64_t *target)
{
    hal_s64_t v;
    __atomic_load(target, &v, RTAPI_MEMORY_MODEL);
    return v;
}
#endif

#if !defined(HAVE_CK) || !defined(CK_F_PR_STORE_64)
static inline void rtapi_store_u64(hal_u64_t *target, const hal_u64_t value)
{
    __atomic_store(target, &value, RTAPI_MEMORY_MODEL);
}

static inline void rtapi_store_s64(hal_s64_t *target, const hal_s64_t value)
{
    __atomic_store(target, &value, RTAPI_MEMORY_MODEL);
}
#endif

#if !defined(HAVE_CK) || !defined(CK_F_PR_INC_64)
static inline void rtapi_inc_u64(hal_u64_t *target)
{
    __atomic_add_fetch(target, 1, RTAPI_MEMORY_MODEL);
}
#endif

#if !defined(HAVE_CK) || !defined(CK_F_PR_CAS_64)
static inline int rtapi_cas_u64(hal_u64_t *target, hal_u64_t old_value, hal_u64_t new_value)
{
    return __atomic_compare_exchange_n (target, &old_value, new_value, 1,
					RTAPI_MEMORY_MODEL, RTAPI_MEMORY_MODEL);
}
#endif

#if !defined(HAVE_CK)
// use gcc intrinsics

static inline hal_u8_t rtapi_load_u8(const hal_u8_t *target)
{
    hal_u8_t v;
    __atomic_load(target, &v, RTAPI_MEMORY_MODEL);
    return v;
}

static inline hal_u32_t rtapi_load_u32(const hal_u32_t *target)
{
    hal_u32_t v;
    __atomic_load(target, &v, RTAPI_MEMORY_MODEL);
    return v;
}

static inline hal_s32_t rtapi_load_s32(const hal_s32_t *target)
{
    hal_s32_t v;
    __atomic_load(target, &v, RTAPI_MEMORY_MODEL);
    return v;
}

static inline void * rtapi_load_ptr(const void *target)
{
    void *v;
    __atomic_load((rtapi_uintptr_t *)target, &v, RTAPI_MEMORY_MODEL);
    return v;
}

static inline void rtapi_store_u8(hal_u8_t *target, hal_u8_t value)
{
    __atomic_store(target, &value, RTAPI_MEMORY_MODEL);
}

static inline void rtapi_store_u32(hal_u32_t *target, hal_u32_t value)
{
    __atomic_store(target, &value, RTAPI_MEMORY_MODEL);
}

static inline void rtapi_store_s32(hal_s32_t *target, hal_s32_t value)
{
    __atomic_store(target, &value, RTAPI_MEMORY_MODEL);
}

static inline void rtapi_store_ptr(void *target, void *value)
{
    __atomic_store((rtapi_uintptr_t *)target, &value, RTAPI_MEMORY_MODEL);
}


static inline void rtapi_add_s32(hal_s32_t *target, const hal_s32_t delta)
{
    __atomic_add_fetch (target, delta, RTAPI_MEMORY_MODEL);
}


static inline void rtapi_add_u32(hal_u32_t *target, const hal_u32_t delta)
{
    __atomic_add_fetch (target, delta, RTAPI_MEMORY_MODEL);
}

static inline int rtapi_cas_u8(hal_u8_t *target, hal_u8_t old_value, hal_u8_t new_value)
{
    return __atomic_compare_exchange_n (target, &old_value, new_value, 1,
					RTAPI_MEMORY_MODEL, RTAPI_MEMORY_MODEL);
}

static inline int rtapi_cas_u32(hal_u32_t *target, hal_u32_t old_value, hal_u32_t new_value)
{
    return __atomic_compare_exchange_n (target, &old_value, new_value, 1,
					RTAPI_MEMORY_MODEL, RTAPI_MEMORY_MODEL);
}

static inline int rtapi_cas_s32(hal_s32_t *target, hal_s32_t old_value, hal_s32_t new_value)
{
    return __atomic_compare_exchange_n (target, &old_value, new_value, 1,
					RTAPI_MEMORY_MODEL, RTAPI_MEMORY_MODEL);
}

#define	rtapi_smp_mb()  __sync_synchronize()
#define	rtapi_smp_wmb() __sync_synchronize()
#define	rtapi_smp_rmb() __sync_synchronize()
#endif  // !defined(HAVE_CK)

#endif // _RTAPI_ATOMICS_H
