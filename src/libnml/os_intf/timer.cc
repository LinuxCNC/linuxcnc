/********************************************************************
* Description: timer.cc
*   A TIMER object lets you wait on the expiration of a cyclic
*   period, to the resolution of the system clock.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
* $Revision$
* $Author$
* $Date$
********************************************************************/

extern "C" {
#include <stdlib.h>		// atof()
#include <string.h>		// strtok(), strncmp()
#include <stdio.h>		/* NULL */

#include <stdlib.h>		/* exit() */
#include <signal.h>		/* struct sigaction, sigaction(), SIGALRM,
				   sigset_t */
#include <errno.h>		/* perror(), EINTR */
#include <unistd.h>		/* select(), sysconf(), _SC_CLK_TCK */
#include <sys/time.h>		/* struct timeval, gettimeofday(), struct
				   itimerval, setitimer(), ITIMER_REAL */
#include <sys/types.h>
#include <sys/wait.h>		// waitpid()
}

#include "timer.hh"
#include "_timer.h"

/* Timer class */
Timer::Timer(double _timeout)
{
    zero_timer();

    if (_timeout < clk_tck_val) {
	counts_per_real_sleep = (int) (clk_tck_val / _timeout);
	/* bump interval up to minimum system clock tick */
	timeout = clk_tck_val;
    } else {
	timeout = _timeout;
    }

    last_time = etime();	/* initialize start time and last time called
				   to current time since epoch */
    start_time = etime();	/* set creation time to now */
    time_since_real_sleep = start_time;
}

void Timer::set_timeout(double _timeout)
{
    timeout = _timeout;
    if (timeout < clk_tck()) {
	counts_per_real_sleep = (int) (clk_tck() / _timeout) + 1;
    } else {
	counts_per_real_sleep = 0;
    }
}

void Timer::zero_timer()
{
    idle = 0.0;			/* set accumulated idle time to 0.0 */
    counts = 0;			/* set accumulated waits to 0 */
    start_time = etime();	/* set creation time to now */
    time_since_real_sleep = start_time;
    counts_per_real_sleep = 0;
    counts_since_real_sleep = 0;
    clk_tck_val = clk_tck();
    timeout = clk_tck_val;
}

void Timer::init(double _timeout)
{
    zero_timer();
    set_timeout(_timeout);

}

/* Restart the timing interval. */
void Timer::sync()
{
    last_time = etime();	/* initialize start time and last time called
				   to current time since epoch */
}

int Timer::wait()
{
    double interval;		/* interval between this and last wakeup */
    double numcycles;		/* interval, in units of timeout */
    int missed = 0;		/* cycles missed */
    double remaining = 0.0;	/* time remaining until timeout */
    double time_in = 0.0;	/* time wait() was entered */

    time_in = etime();

    /* calculate the interval-- for user timing functions, this is how long
       between this wakeup and the last wakeup.  For internal timers, this is 
       how long we need to sleep to make it to the next interval on time. */
    interval = time_in - last_time;
    numcycles = interval / timeout;

    /* synchronize and set last_time correctly; update idle time */
    counts++;
    missed = (int) (numcycles); 
    remaining = timeout * (1.0 - (numcycles - (int) numcycles));
    idle += interval;
    esleep(remaining);
    last_time = etime();
    return missed;
}

double Timer::load()
{
    if (counts * timeout != 0.0)
	return idle / (counts * timeout);
    return -1.0;
}

Timer::~Timer()
{
printf("Timer destructor\n");
}
