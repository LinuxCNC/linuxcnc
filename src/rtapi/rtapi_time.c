/********************************************************************
* Description:  rtapi_time.c
*               This file, 'rtapi_time.c', implements the timing-
*               related functions for realtime modules.  See rtapi.h
*               for more info.
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#ifdef RTAPI
#include <time.h>		// clock_getres(), clock_gettime()
#else  /* ULAPI */
#include <sys/time.h>		// gettimeofday()
#endif

#ifdef MODULE
#include <linux/module.h>	// EXPORT_SYMBOL
#endif


// find a useable time stamp counter
#ifdef MSR_H_USABLE
#include <asm/msr.h>
#elif defined(__i386__) || defined(__x86_64__)
#define rdtscll(val) \
         __asm__ __volatile__("rdtsc" : "=A" (val))
#else
#warning No implementation of rtapi_get_clocks available
#define rdtscll(val) (val)=0
#endif

#ifdef RTAPI  /* hide most functions from ULAPI */

int period = 0;

long int rtapi_clock_set_period(long int nsecs) {
    struct timespec res = { 0, 0 };

    if (nsecs == 0)
	return period;
    if (period != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "attempt to set period twice\n");
	return -EINVAL;
    }

    clock_getres(CLOCK_MONOTONIC, &res);
    period = (nsecs / res.tv_nsec) * res.tv_nsec;
    if (period < 1)
	period = res.tv_nsec;

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_clock_set_period (res=%ld) -> %d\n", res.tv_nsec,
		    period);

    return period;
}

#endif /* RTAPI */

/* The following functions are common to both RTAPI and ULAPI */

#ifdef RTAPI
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

long long int rtapi_get_clocks(void) {
    long long int retval;

    rdtscll(retval);
    return retval;
}

