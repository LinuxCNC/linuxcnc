/********************************************************************
* Description: timer.hh
*   A TIMER object lets you wait on the expiration of a cyclic
*   period, to the resolution of the system clock.
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
class RCS_SEMAPHORE;

/* prototype for user-defined timing function */
typedef int (*RCS_TIMERFUNC) (void *_arg);

/* Getting rid of this stuff which no one uses and makes porting more
difficult */
#ifdef RCS_TIMER_USE_ITIMER

/* prototype for signal hander function */
typedef void (*RCS_SIGFUNC) (...);

#endif /* RCS_TIMER_USE_ITIMER */

/*
  general-purpose timer, which can be used for waiting until a
  synchronous time tick, slept on for any period at all, or to
  obtain a time in system clock ticks from creation of the timer.
  */

/*
Programmers can create RCS_TIMER objects to synchronize to the system clock or to some other event(s).

To synchronize a cyclic process to the system clock:

Initialize the RCS_TIMER object with the cycle period. 
Call RCS_TIMER::wait() at the end of each cycle. 

The cycle period will be rounded up to the resolution of the system clock or
the most precise time measuring or sleeping function available for the given
platform. RCS_TIMER::wait() will wait the remainder of the cycle period since
the last call. The units for the cycle time are seconds.
*/
/*

To synchronize to some other event(s):

 Create a function that takes a (void *) as an argument and returns an int. 
 Initialize the RCS_TIMER object with a cycle period used only for diagnostics, 
 the address of the function and a parameter for the function.
 Use RCS_TIMER::wait().

The user's function should return 0 when the event to synchronize to occurs or -1 
if an error occurs. The argument passed to the users function will be whatever was
passed as the third parameter to the constructor of the RCS_TIMER or NULL if no
third argument is given. This argument could be used by the synchronizing function
to know which timer is calling it if the synchronization function is called by more
than one timer. Nothing will force the function to return within the cycle period but
there are ways to check if the function took longer than the cycle period after it returns.
*/
class RCS_TIMER {
  public:
/* Getting rid of this stuff which noone uses and makes porting more
difficult */
    RCS_TIMER(double timeout, RCS_TIMERFUNC function =
	(RCS_TIMERFUNC) NULL, void *arg = NULL);

      RCS_TIMER(const char *process_name, const char *timer_config_file);
      RCS_TIMER(double _timeout, const char *process_name, const char *timer_config_file);
    /* Initialize a new RCS_TIMER object. _interval is the cycle period.
       _function is an optional function for synchronizing to an event other
       than the system clock. _arg is a parameter that will be passed to
       function. */

    /* timeout is wait interval, rounded up to clock tick resolution;
       function is external time base, if provided */
     ~RCS_TIMER();
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
    void init(double _timeout, int _id);

    void zero_timer();
    void set_timeout(double _timeout);
/*! \todo Another #if 0 */
#if 0
    void read_config_file(char *process, char *config_file);
#endif
    RCS_TIMERFUNC function;	/* copy of function */
    void *arg;			/* arg for function */
    double last_time;		/* last wakeup, in ticks from epoch */
    double start_time;		/* epoch time at creation of timer */
    double idle;		/* accumulated idle time */
    int counts;			/* accumulated waits */
    int counts_since_real_sleep;
    int counts_per_real_sleep;
    double time_since_real_sleep;
#ifdef USE_SEMS_FOR_TIMER
    RCS_SEMAPHORE **sems;
#endif
    int num_sems;
    int id;
    double clk_tck_val;
};

#endif
