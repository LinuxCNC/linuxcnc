/*
 * rtapi_core.h — Public API for librtapi_core.so
 *
 * RT-safe memory allocation and timing primitives for cmod modules.
 * No dependency on hal.h or rtapi.h.
 */
#ifndef RTAPI_CORE_H
#define RTAPI_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory locking (pre-fault + mlock) */
int  rtapi_lock_mem(void *p, size_t size, int prefault_rw);
void rtapi_unlock_mem(void *p, size_t size);

/* RT-safe memory allocation */
void *rtapi_malloc(size_t size);
void *rtapi_calloc(size_t size);
void *rtapi_realloc(void *ptr, size_t size);
void  rtapi_free(void *p);

/* Timing */
long long rtapi_get_time(void);
long int  rtapi_delay_max(void);
void      rtapi_delay(long ns);

/* Compatibility */
long int simple_strtol(const char *nptr, char **endptr, int base);

#ifdef __cplusplus
}
#endif

#endif /* RTAPI_CORE_H */
