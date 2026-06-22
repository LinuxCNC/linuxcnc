/*
 * rtapi_core.c — Minimal RT-safe memory + timing for cmod modules
 *
 * Provides: rtapi_malloc, rtapi_calloc, rtapi_realloc, rtapi_free,
 *           rtapi_lock_mem, rtapi_unlock_mem,
 *           rtapi_get_time, rtapi_delay, rtapi_delay_max
 *
 * This is a standalone shared library with no dependency on hal.h or rtapi.h.
 * cmod modules link it explicitly via -lrtapi_core.
 *
 * Copyright (C) 2006 John Kasunich, Paul Corner (original uspace_ulapi.c)
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod extraction
 * License: LGPL Version 2
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <malloc.h>

#include "rtapi_core.h"

/* ------------------------------------------------------------------ */
/* Memory locking                                                      */
/* ------------------------------------------------------------------ */

int rtapi_lock_mem(void *p, size_t size, int prefault_rw) {
    long pagesize = sysconf(_SC_PAGESIZE);
    volatile char *c = (volatile char *)p;
    volatile char dummy;

    /* Pre-fault all pages */
    for (size_t i = 0; i < size; i += (size_t)pagesize) {
        dummy = c[i];
        if (prefault_rw)
            c[i] = dummy;
    }
    if (size > 0 && (size % (size_t)pagesize) != 0) {
        dummy = c[size - 1];
        if (prefault_rw)
            c[size - 1] = dummy;
    }
    (void)dummy;

    /* Lock into physical RAM */
    int ret = mlock(p, size);
    if (ret < 0) {
        fprintf(stderr, "rtapi_lock_mem: mlock(%zu) failed: %s\n",
                size, strerror(errno));
    }
    return ret;
}

void rtapi_unlock_mem(void *p, size_t size) {
    if (!p) return;
    munlock(p, size);
}

/* ------------------------------------------------------------------ */
/* RT-safe memory allocation (pre-fault + mlock)                       */
/* ------------------------------------------------------------------ */

void *rtapi_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) return NULL;
    rtapi_lock_mem(p, malloc_usable_size(p), 1);
    return p;
}

void *rtapi_calloc(size_t size) {
    void *p = rtapi_malloc(size);
    if (!p) return NULL;
    memset(p, 0, size);
    return p;
}

void *rtapi_realloc(void *ptr, size_t size) {
    if (ptr)
        rtapi_unlock_mem(ptr, malloc_usable_size(ptr));
    void *p = realloc(ptr, size);
    if (!p) return NULL;
    rtapi_lock_mem(p, malloc_usable_size(p), 1);
    return p;
}

void rtapi_free(void *p) {
    if (!p) return;
    rtapi_unlock_mem(p, malloc_usable_size(p));
    free(p);
}

/* ------------------------------------------------------------------ */
/* Timing                                                              */
/* ------------------------------------------------------------------ */

long long rtapi_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

long int rtapi_delay_max(void) {
    return 10000;
}

void rtapi_delay(long ns) {
    if (ns > rtapi_delay_max())
        ns = rtapi_delay_max();
    struct timespec ts = {0, ns};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

/* ------------------------------------------------------------------ */
/* Compatibility                                                       */
/* ------------------------------------------------------------------ */

long int simple_strtol(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}
