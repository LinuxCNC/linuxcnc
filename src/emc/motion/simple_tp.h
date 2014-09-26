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

enum acc_state_type {
  ACC_S0 = 0, // 0
  ACC_S1,     // 1
  ACC_S2,     // 2
  ACC_S3,     // 3
  ACC_S4,     // 4
  ACC_S5,     // 5
  ACC_S6,     // 6
  ACC_S7      // 7
};

typedef struct simple_tp_t {
    int enable;
    int active;
    double pos_cmd;
    double vel_cmd;
    int dir;

    double max_vel;
    double max_acc;
    double jerk;

    double curr_pos;
    double start_pos;
    double curr_vel;         // keep track of current step (vel * cycle_time)
    double curr_acc;       // keep track of current acceleration

    double cycle_time;
    double target;          // segment length
    double distance_to_go;  // distance to go for target target..0

    double tolerance;
    double feed_override;
    int aborting;
    enum acc_state_type accel_state;
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
extern void simple_tp_jog_cont(simple_tp_t *tp, double pos_cmd, double vel_cmd, double max_vel, double max_acc, double max_jerk);
extern void simple_tp_jog_abort(simple_tp_t *tp);
extern void simple_tp_setCycleTime(simple_tp_t *tp, double secs);

#endif	/* SIMPLE_TP_H */
