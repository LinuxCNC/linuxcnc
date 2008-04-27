/********************************************************************
* Description: _timer.c
*   timer.cc -- interval timer code.  A TIMER object lets you wait
*   on the expiration of a cyclic period, to the resolution of the
*   system clock.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#include "rcs_print.hh"		/* rcs_print_error */

#include <stdio.h>		/* NULL */
#include <errno.h>		/* errno */
#include <string.h>		/* strerror() */

#include <errno.h>		/* EINTR */
#include <unistd.h>		/* select(), sysconf(), _SC_CLK_TCK */
#include <sys/time.h>		/* struct timeval, gettimeofday(), struct
				   itimerval, setitimer(), ITIMER_REAL */

#include <linux/version.h>

#if defined(LINUX_VERSION_CODE) && defined(KERNEL_VERSION)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,2,0)
#include <sched.h>
#endif
#endif

#include "_timer.h"

/* number of seconds in a system clock tick */
double clk_tck()
{
    return 1.0 / (double) sysconf(_SC_CLK_TCK);
}

/*
 These values can be used with a debugger to prevent
 or deliberately cause timeouts.
 */

int etime_disabled = 0;
double etime_disable_time = 0.0;

/* number of seconds from some epoch, to clock tick resolution */
double etime()
{
    struct timeval tp;
    double retval;

    if (0 != gettimeofday(&tp, NULL)) {
	rcs_print_error("etime: can't get time\n");
	return 0.0;
    }

    retval = ((double) tp.tv_sec) + ((double) tp.tv_usec) / 1000000.0;
    return retval;
}

int esleep_use_yield = 0;

/* sleeps # of seconds */
void esleep(double seconds_to_sleep)
{
    struct timeval tval;
    static double clk_tck_val = 0;
    double total = seconds_to_sleep;	/* total sleep asked for */
    double started = etime();	/* time when called */
    double left = total;
    if (seconds_to_sleep <= 0.0)
	return;
    if (clk_tck_val <= 0) {
	clk_tck_val = clk_tck();
    }
    do {
	if (left < clk_tck_val && esleep_use_yield) {
	    sched_yield();
	} else {
	    tval.tv_sec = (long) left;	/* double->long truncates, ANSI */
	    tval.tv_usec = (long) ((left - (double) tval.tv_sec) * 1000000.0);
	    if (tval.tv_sec == 0 && tval.tv_usec == 0) {
		tval.tv_usec = 1;
	    }
	    if (select(0, NULL, NULL, NULL, &tval) < 0) {
		if (errno != EINTR) {
		    break;
		}
	    }
	}
	left = total - etime() + started;
    }
    while (left > 0 && (left > clk_tck_val && esleep_use_yield));
    return;
}

void print_etime()
{
    printf("etime = %f\n", etime());
}
