/********************************************************************
* Description: simple_tp.h
*   A simple, single axis trajectory planner
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved
********************************************************************/

/*  simple_tp.c and simple_tp.h define a simple, single axis trajectory
    planner.  It is based on the "free mode trajectory planner" that was
    originally written as part of EMC2's control.c, but the code has
    been pulled out of control.c and given a somewhat object oriented
    API to allow it to be used for both teleop and free mode.
*/

#ifndef SIMPLE_TP_H
#define SIMPLE_TP_H

// stopping criterion:
#define TINY_DP(max_acc,period) (max_acc*period*period*0.001)

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct simple_tp_t {
	double pos_cmd;		/* position command */
	double max_vel;		/* velocity limit */
	double max_acc;		/* acceleration limit */
	int enable;		/* if zero, motion stops ASAP */
	double curr_pos;	/* current position */
	double curr_vel;	/* current velocity */
	int active;		/* non-zero if motion in progress */
    } simple_tp_t;

/* I could write a bunch of functions to read and write the first four
   structure members, and to read the last three, but that seems silly.
*/

/* The update() function does all the work.  If 'enable' is true, it
   computes a new value of 'curr_pos', which moves toward 'pos_cmd'
   while obeying the 'max_vel' and 'max_accel' limits.  It also sets
   'active' if movement is in progress, and clears it when motion
   stops at the commanded position.  The command or either of the
   limits can be changed at any time.  If 'enable' is false, it
   ramps the velocity to zero, then clears 'active' and sets
   'pos_cmd' to match 'curr_pos', to avoid motion the next time it
   is enabled.  'period' is the period between calls, in seconds.
*/

extern void simple_tp_update(simple_tp_t *tp, double period);


#ifdef __cplusplus
}
#endif
#endif	/* SIMPLE_TP_H */
