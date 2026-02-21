/********************************************************************
* Description: joint_simple_tp.h
*   A simple, single axis trajectory planner.
*   (Copied from emc/motion/simple_tp.h for use by the standalone
*    joint_ctrl HAL component.)
*
* Author: jmkasunich
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved
********************************************************************/

#ifndef JOINT_SIMPLE_TP_H
#define JOINT_SIMPLE_TP_H

/* stopping criterion */
#define TINY_DP(max_acc, period) (max_acc * period * period * 0.001)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct simple_tp_t {
    double pos_cmd;     /* position command */
    double max_vel;     /* velocity limit */
    double max_acc;     /* acceleration limit */
    int    enable;      /* if zero, motion stops ASAP */
    double curr_pos;    /* current position */
    double curr_vel;    /* current velocity */
    int    active;      /* non-zero if motion in progress */
} simple_tp_t;

extern void simple_tp_update(simple_tp_t *tp, double period);

#ifdef __cplusplus
}
#endif

#endif /* JOINT_SIMPLE_TP_H */
