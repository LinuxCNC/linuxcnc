/*
 * cruckig_internal.h - Internal header for cruckig
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 *
 * Provides RTAPI-portable types, memory allocation, math, string
 * functions, and compiler hint macros for cruckig internals.
 *
 * All cruckig headers should include this as their first include.
 * C files should NOT include this directly -- they get it through
 * their corresponding header.
 */
#ifndef CRUCKIG_CRUCKIG_INTERNAL_H
#define CRUCKIG_CRUCKIG_INTERNAL_H

/* RTAPI provides bool, size_t, math, string, and memory allocation
 * portably across userspace and kernel builds. */
#include <rtapi.h>
#include <rtapi_bool.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <rtapi_slab.h>
#include <float.h>

/* INFINITY: not provided by rtapi_math.h in kernel space */
#ifndef INFINITY
#define INFINITY __builtin_inf()
#endif

/* Memory allocation: always use rtapi_slab wrappers */
#define cruckig_malloc(sz)       rtapi_kmalloc(sz, RTAPI_GFP_KERNEL)
#define cruckig_calloc(n, sz)    rtapi_kzalloc((n) * (sz), RTAPI_GFP_KERNEL)
#define cruckig_realloc(p, sz)   rtapi_krealloc(p, sz, RTAPI_GFP_KERNEL)
#define cruckig_free(p)          rtapi_kfree(p)

/* Branch prediction hints */
#if defined(__GNUC__) || defined(__clang__)
#  define CRUCKIG_LIKELY(x)      __builtin_expect(!!(x), 1)
#  define CRUCKIG_UNLIKELY(x)    __builtin_expect(!!(x), 0)
#  define CRUCKIG_FORCE_INLINE   static inline __attribute__((always_inline))
#  define CRUCKIG_HOT            __attribute__((hot))
#  define CRUCKIG_RESTRICT       __restrict__
#  define CRUCKIG_PREFETCH(addr) __builtin_prefetch(addr, 0, 1)
#else
#  define CRUCKIG_LIKELY(x)      (x)
#  define CRUCKIG_UNLIKELY(x)    (x)
#  define CRUCKIG_FORCE_INLINE   static inline
#  define CRUCKIG_HOT
#  define CRUCKIG_RESTRICT       restrict
#  define CRUCKIG_PREFETCH(addr) ((void)0)
#endif

#endif /* CRUCKIG_CRUCKIG_INTERNAL_H */
