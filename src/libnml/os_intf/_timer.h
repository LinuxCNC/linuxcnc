/********************************************************************
* Description: _timer.h
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
#ifndef _TIMER_H
#define _TIMER_H

/* Useful time routines */

#ifdef __cplusplus
extern "C" {
#endif
/* clock tick resolution, in seconds */
    extern double clk_tck(void);
/* number of seconds from standard epoch, to clock tick resolution */
    extern double etime(void);
/* sleeps # of seconds, to clock tick resolution */
    extern void esleep(double secs);
    void start_timer_server(int priority, int sem_id);
    void kill_timer_server(void);
    extern void print_etime(void);

#ifdef __cplusplus
}
#endif
extern int etime_disabled;
extern double etime_disable_time;
extern int esleep_use_yield;

#endif
