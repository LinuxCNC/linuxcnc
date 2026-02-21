/********************************************************************
* Description: joint_homing.h
*   Single-joint homing state machine interface for joint_ctrl.
*   Extracted and refactored from emc/motion/homing.c/.h.
*   Multi-joint sequencing logic stripped; operates on a single
*   joint via pointers.
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#ifndef JOINT_HOMING_H
#define JOINT_HOMING_H

#include <rtapi_bool.h>
#include "joint_simple_tp.h"

/* HOME_* flags (same values as in emc/motion/homing.h) */
#define HOME_IGNORE_LIMITS            1
#define HOME_USE_INDEX                2
#define HOME_IS_SHARED                4
#define HOME_UNLOCK_FIRST             8
#define HOME_ABSOLUTE_ENCODER        16
#define HOME_NO_REHOME               32
#define HOME_NO_FINAL_MOVE           64
#define HOME_INDEX_NO_ENCODER_RESET 128

/* Homing state machine states */
typedef enum {
    HOME_IDLE = 0,
    HOME_START,
    HOME_UNLOCK,
    HOME_UNLOCK_WAIT,
    HOME_INITIAL_BACKOFF_START,
    HOME_INITIAL_BACKOFF_WAIT,
    HOME_INITIAL_SEARCH_START,
    HOME_INITIAL_SEARCH_WAIT,
    HOME_SET_COARSE_POSITION,
    HOME_FINAL_BACKOFF_START,
    HOME_FINAL_BACKOFF_WAIT,
    HOME_RISE_SEARCH_START,
    HOME_RISE_SEARCH_WAIT,
    HOME_FALL_SEARCH_START,
    HOME_FALL_SEARCH_WAIT,
    HOME_SET_SWITCH_POSITION,
    HOME_INDEX_ONLY_START,
    HOME_INDEX_SEARCH_START,
    HOME_INDEX_SEARCH_WAIT,
    HOME_SET_INDEX_POSITION,
    HOME_FINAL_MOVE_START,
    HOME_FINAL_MOVE_WAIT,
    HOME_LOCK,
    HOME_LOCK_WAIT,
    HOME_FINISHED,
    HOME_ABORT
} joint_home_state_t;

/* Joint state fields needed by the homing state machine.
   The caller (joint_ctrl_main.c) owns and updates these. */
typedef struct {
    double       pos_cmd;
    double       pos_fb;
    double       motor_pos_fb;
    double       motor_offset;
    double       backlash_filt;
    double       max_pos_limit;
    double       min_pos_limit;
    double       vel_limit;
    int          on_pos_limit;
    int          on_neg_limit;
    simple_tp_t  free_tp;
} joint_home_joint_t;

/* Per-joint homing data */
typedef struct {
    /* State machine state (exported as HAL pin) */
    joint_home_state_t home_state;

    /* Status (exported as HAL pins) */
    bool homing;
    bool homed;

    /* HAL pin inputs (written by caller before each call) */
    bool home_sw;
    bool index_enable;

    /* Internal timer */
    int  pause_timer;

    /* Configuration (read from HAL params each cycle) */
    double home_offset;
    double home;
    double home_final_vel;
    double home_search_vel;
    double home_latch_vel;
    int    home_flags;
    bool   volatile_home;

    /* Servo timing */
    double servo_freq;

    /* Optional rotary axis support (NULL if not needed) */
    void (*set_rotary_unlock)(int unlock);
    int  (*get_rotary_is_unlocked)(void);
} joint_home_data_t;

/* Initialize homing data for one joint.
   servo_period is the servo cycle time in seconds. */
void joint_homing_init(joint_home_data_t *h, double servo_period);

/* Trigger homing (call when do-home rising edge detected) */
void joint_home_start(joint_home_data_t *h);

/* Cancel homing in progress (call when do-cancel-home is asserted) */
void joint_home_cancel(joint_home_data_t *h);

/* Mark joint as unhomed */
void joint_set_unhomed(joint_home_data_t *h);

/* Run the homing state machine for one servo period.
   Returns non-zero if homing is active this period. */
int joint_home_run(joint_home_data_t *h, joint_home_joint_t *joint);

#endif /* JOINT_HOMING_H */
