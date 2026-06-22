// gomc_rtapi.h — RTAPI utility functions for gomc C modules.
//
// RTAPI_NAME_LEN mirrors rtapi.h by John Kasunich and Paul Corner.
// License of rtapi.h: LGPL Version 2.1.
// Copyright (c) 2004 John Kasunich, Paul Corner.
//
// New API (callback table, inline helpers):
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: LGPL Version 2.1
//
// Provides RT-safe memory allocation (mlock + page pre-fault) and time
// functions through callbacks in gomc_rtapi_t.  Initially these delegate
// to the existing liblinuxcnchal.so / uspace_rtapi_lib.c implementation.
//
// Pure formatting functions (snprintf, vsnprintf) are NOT callbacks — they
// are simply libc on uspace targets.  This header provides gomc_snprintf()
// and gomc_vsnprintf() as thin aliases for consistency.
//
// Usage:
//   void *p = env->rtapi->calloc(env->rtapi->ctx, sizeof(my_struct));
//   int64_t now = env->rtapi->get_time(env->rtapi->ctx);
//   env->rtapi->free(env->rtapi->ctx, p);

#ifndef GOMC_RTAPI_H
#define GOMC_RTAPI_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// PLL functions are always available in gomc_rtapi_t.
#define GOMC_RTAPI_TASK_PLL_SUPPORT

#define GOMC_RTAPI_NAME_LEN 31

// ---------------------------------------------------------------------------
// gomc_rtapi_t — RTAPI callback table.
// ---------------------------------------------------------------------------

typedef struct {
    void *ctx;

    // RT-safe memory allocation.
    // calloc: allocates size bytes, zeroed, mlock'd and page-faulted.
    // realloc: resizes an existing allocation (unlocks old, locks new).
    // free: munlock + free.
    void *(*calloc) (void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t size);
    void  (*free)   (void *ctx, void *ptr);

    // Monotonic time in nanoseconds.
    int64_t (*get_time)(void *ctx);

    // Busy-wait delay (nanoseconds).
    void    (*delay)(void *ctx, long nsec);
    long    (*delay_max)(void *ctx);

    // Task PLL functions for RT thread synchronisation.
    int64_t (*pll_get_reference)(void *ctx);
    int     (*pll_set_correction)(void *ctx, long value);

    // Returns >= 0 if called from a RT task, < 0 otherwise.
    int     (*task_self)(void *ctx);
} gomc_rtapi_t;

// ---------------------------------------------------------------------------
// Formatting — thin aliases for libc.  No callback needed on uspace.
// ---------------------------------------------------------------------------

#define gomc_snprintf  snprintf
#define gomc_vsnprintf vsnprintf

#ifdef __cplusplus
}
#endif

#endif // GOMC_RTAPI_H
