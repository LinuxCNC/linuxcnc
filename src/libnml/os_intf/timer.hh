/********************************************************************
* Description: timer.hh
*   Clock and sleep functions for libnml, implemented in _timer.c.
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

#endif
