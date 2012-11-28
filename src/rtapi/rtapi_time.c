/********************************************************************
* Description:  rtapi_time.c
*               This file, 'rtapi_time.c', implements the timing-
*               related functions for realtime modules.  See rtapi.h
*               for more info.
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// these functions

#ifdef RTAPI
#include <time.h>		// clock_getres(), clock_gettime()
#else  /* ULAPI */
#include <sys/time.h>		// gettimeofday()
#endif

#ifdef MODULE
#include <linux/module.h>	// EXPORT_SYMBOL
#endif


// find a useable time stamp counter
#ifndef HAVE_RTAPI_GET_CLOCKS_HOOK  // only if thread system uses the default
#ifdef MSR_H_USABLE
#include <asm/msr.h>
#elif defined(__i386__) || defined(__x86_64__)
#define rdtscll(val) \
         __asm__ __volatile__("rdtsc" : "=A" (val))
#else
#warning No implementation of rtapi_get_clocks available
#define rdtscll(val) (val)=0
#endif
#endif /* HAVE_RTAPI_GET_CLOCKS_HOOK */


#ifdef RTAPI  /* hide most functions from ULAPI */

int period = 0;

long int rtapi_clock_set_period(long int nsecs) {
#ifndef RTAPI_TIME_NO_CLOCK_MONOTONIC
    struct timespec res = { 0, 0 };
#endif

    if (nsecs == 0)
	return period;
    if (period != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "attempt to set period twice\n");
	return -EINVAL;
    }

#ifdef RTAPI_TIME_NO_CLOCK_MONOTONIC
    period = nsecs;
#else
    clock_getres(CLOCK_MONOTONIC, &res);
    period = (nsecs / res.tv_nsec) * res.tv_nsec;
    if (period < 1)
	period = res.tv_nsec;

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_clock_set_period (res=%ld) -> %d\n", res.tv_nsec,
		    period);
#endif

    return period;
}

#endif /* RTAPI */

/* The following functions are common to both RTAPI and ULAPI */

#ifdef HAVE_RTAPI_GET_TIME_HOOK
long long int rtapi_get_time_hook(void);

long long int rtapi_get_time(void) {
    return rtapi_get_time_hook();
}
#elif defined(RTAPI)
long long int rtapi_get_time(void) {

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

#else /* ULAPI */
long long rtapi_get_time(void)
{
	struct timeval tv;
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ulapi get_time\n");
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}
#endif

#ifdef HAVE_RTAPI_GET_CLOCKS_HOOK
long long int rtapi_get_clocks_hook(void);
#endif

long long int rtapi_get_clocks(void) {
#ifndef HAVE_RTAPI_GET_CLOCKS_HOOK
    long long int retval;

    rdtscll(retval);
    return retval;
#else
    return rtapi_get_clocks_hook();
#endif
}

