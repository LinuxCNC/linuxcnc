// gomc_rtapi.h — RTAPI utility functions for gomc C modules.
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

    // Task PLL functions for RT thread synchronisation.
    int64_t (*pll_get_reference)(void *ctx);
    int     (*pll_set_correction)(void *ctx, long value);
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
