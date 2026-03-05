/* Stub: minimal rtapi_mutex for test builds */
#ifndef _RTAPI_MUTEX_H_
#define _RTAPI_MUTEX_H_

typedef unsigned long rtapi_mutex_t;

#define rtapi_mutex_get(mutex)      do {} while (0)
#define rtapi_mutex_try(mutex)      0
#define rtapi_mutex_give(mutex)     do {} while (0)

#endif
