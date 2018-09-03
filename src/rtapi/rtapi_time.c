/********************************************************************
* Description:  rtapi_time.c
*               This file, 'rtapi_time.c', implements the timing-
*               related functions for realtime modules.  See rtapi.h
*               for more info.
*
*     Copyright 2006-2013 Various Authors
* 
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; either version 2 of the License, or
*     (at your option) any later version.
* 
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
* 
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// these functions

#ifndef MODULE  // kernel threads systems have own timer functions
#    include <time.h>		// clock_getres(), clock_gettime()
#endif


#ifndef HAVE_RTAPI_GET_CLOCKS_HOOK
// find a useable time stamp counter
#ifndef MODULE			/* kernel has rdtscll in
				   arch/x86/include/msr.h; does this
				   apply to other arches?  */
#ifdef MSR_H_USABLE
#include <asm/msr.h>
#elif defined(__i386__) || defined(__x86_64__)
#define rdtscll(val) \
         __asm__ __volatile__("rdtsc" : "=A" (val))
#else
#error No implementation of rtapi_get_clocks available
#define rdtscll(val) (val)=0
#endif
#endif
#endif /* HAVE_RTAPI_GET_CLOCKS_HOOK */

long int max_delay = DEFAULT_MAX_DELAY;

#ifdef RTAPI  /* hide most functions from ULAPI */

#ifdef BUILD_SYS_USER_DSO
int period = 0;
#endif

// Actual number of counts of the periodic timer
unsigned long timer_counts;


#ifdef HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK
void _rtapi_clock_set_period_hook(long int nsecs, RTIME *counts, 
				 RTIME *got_counts);
#endif

#ifdef BUILD_SYS_USER_DSO
long int _rtapi_clock_set_period(long int nsecs) {
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
#endif  /* ! RTAPI_TIME_NO_CLOCK_MONOTONIC */

    return period;
}
#else  /* BUILD_SYS_KBUILD  */
long int _rtapi_clock_set_period(long int nsecs) {
    RTIME counts, got_counts;

    if (nsecs == 0) {
	/* it's a query, not a command */
	return rtapi_data->timer_period;
    }
    if (rtapi_data->timer_running) {
	/* already started, can't restart */
	return -EINVAL;
    }
    /* limit period to 2 micro-seconds min, 1 second max */
    if ((nsecs < 2000) || (nsecs > 1000000000L)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERR: clock_set_period: %ld nsecs,  out of range\n",
	    nsecs);
	return -EINVAL;
    }

    /* kernel thread systems should init counts and
       rtapi_data->timer_period using their own timer functions */
#ifdef HAVE_RTAPI_CLOCK_SET_PERIOD_HOOK
    _rtapi_clock_set_period_hook(nsecs, &counts, &got_counts);
    timer_counts = got_counts;
#endif

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "RTAPI: clock_set_period requested: %ld  actual: %ld  "
		    "counts requested: %llu  actual: %d\n",
		    nsecs, rtapi_data->timer_period,
		    (unsigned long long)counts, (int)got_counts);

    rtapi_data->timer_running = 1;
    max_delay = rtapi_data->timer_period / 4;
    return rtapi_data->timer_period;
}
#endif  /* BUILD_SYS_KBUILD  */

// rtapi_delay_hook MUST be implemented by all threads systems
void _rtapi_delay_hook(long int nsec);

void _rtapi_delay(long int nsec)
{
    if (nsec > max_delay) {
	nsec = max_delay;
    }
    _rtapi_delay_hook(nsec);
}


long int _rtapi_delay_max(void)
{
    return max_delay;
}

#endif /* RTAPI */

/* The following functions are common to both RTAPI and ULAPI */

#ifdef HAVE_RTAPI_GET_TIME_HOOK
long long int _rtapi_get_time_hook(void);

long long int _rtapi_get_time(void) {
    return _rtapi_get_time_hook();
}
#else
long long int _rtapi_get_time(void) {

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
#endif /* HAVE_RTAPI_GET_TIME_HOOK */

#ifdef HAVE_RTAPI_GET_CLOCKS_HOOK
long long int _rtapi_get_clocks_hook(void);
#endif

long long int _rtapi_get_clocks(void) {
#ifndef HAVE_RTAPI_GET_CLOCKS_HOOK
    long long int retval;

    /* This returns a result in clocks instead of nS, and needs to be
       used with care around CPUs that change the clock speed to save
       power and other disgusting, non-realtime oriented behavior.
       But at least it doesn't take a week every time you call it.  */

    rdtscll(retval);
    return retval;
#else
    return _rtapi_get_clocks_hook();
#endif  /* HAVE_RTAPI_GET_CLOCKS_HOOK */
}


