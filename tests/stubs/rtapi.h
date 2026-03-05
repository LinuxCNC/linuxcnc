/* Stub rtapi.h for test builds — replaces the real LinuxCNC RTAPI header */
#ifndef _RTAPI_H_
#define _RTAPI_H_

#include <stdarg.h>
#include <stdint.h>
#include <errno.h>

/*
 * Define RTAPI_UINT64_MAX so that pal/pal.h knows to include rtapi_mutex.h
 * and activates the LinuxCNC >= 2.8 path.
 */
#define RTAPI_UINT64_MAX UINT64_MAX

/* Message severity levels */
#define RTAPI_MSG_NONE  0
#define RTAPI_MSG_ERR   1
#define RTAPI_MSG_WARN  2
#define RTAPI_MSG_INFO  3
#define RTAPI_MSG_DBG   4
#define RTAPI_MSG_ALL   5

/* Stub: prints to stderr */
extern void rtapi_print_msg(int level, const char *fmt, ...);

/* Stub: standard snprintf wrapper */
#include <stdio.h>
#define rtapi_snprintf  snprintf

/* Include stdbool for bool/true/false */
#include <stdbool.h>

#endif
