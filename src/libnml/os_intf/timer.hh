/********************************************************************
* Description: timer.hh
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
#ifndef TIMER_HH
#define TIMER_HH

extern "C" {
#include <stdio.h>		/* NULL */
} extern "C" {
    /* number of seconds from standard epoch, to clock tick resolution */
    extern double etime(void);
    /* Return the number of seconds from some event. The time will be rounded 
       up to the resolution of the system clock or the most precise time
       measuring function available for the given platform. For the value
       returned to mean anything you need to be able to compare it with a
       value stored from a previous call to etime(). */

    /* sleeps # of seconds, to clock tick resolution */
    extern void esleep(double secs);
    /* Go to sleep for _secs seconds. The time will be rounded up to the
       resolution of the system clock or the most precise sleep or delay
       function available for the given platform. */
}

/*
  general-purpose timer, which can be used for waiting until a
  synchronous time tick, slept on for any period at all, or to
  obtain a time in system clock ticks from creation of the timer.
  */

/*
Programmers can create Timer objects to synchronize to the system clock or
to some other event(s).

To synchronize a cyclic process to the system clock:

Initialize the Timer object with the cycle period. 
Call Timer::wait() at the end of each cycle. 

The cycle period will be rounded up to the resolution of the system clock or
the most precise time measuring or sleeping function available for the given
platform. Timer::wait() will wait the remainder of the cycle period since
the last call. The units for the cycle time are seconds.
*/

class Timer {
  public:
    /* timeout is wait interval, rounded up to clock tick resolution */
    Timer(double timeout);
     ~Timer();
    int wait();			/* wait on synch; returns # of cycles missed */
    /* Wait until the end of interval or until a user function returns.

       Returns: 0 for success, the number of cycles missed if it missed some
       cycles, or -1 if some other error occurred. */

    double load();		/* returns % loading on timer, 0.0 means all
				   waits, 1.0 means no time in wait */
    /* Returns the percentage of loading by the cyclic process. If the
       process spends all of its time waiting for the synchronizing event
       then it returns 0.0. If it spends all of its time doing something else 
       before calling wait then it returns 1.0. The load percentage is the
       average load over all of the previous cycles. */
    void sync();		/* restart the wait interval. */
    /* Restart the wait interval now. */
    double timeout;		/* copy of timeout */

  private:
    void init(double _timeout);

    void zero_timer();
    void set_timeout(double _timeout);

    double last_time;		/* last wakeup, in ticks from epoch */
    double start_time;		/* epoch time at creation of timer */
    double idle;		/* accumulated idle time */
    int counts;			/* accumulated waits */
    int counts_since_real_sleep;
    int counts_per_real_sleep;
    double time_since_real_sleep;
    double clk_tck_val;
};

#endif
