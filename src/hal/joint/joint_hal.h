/********************************************************************
* Description: joint_hal.h
*   HAL pin and parameter structure for the joint_ctrl component.
*   Adapted from the joint_hal_t struct in emc/motion/mot_priv.h.
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#ifndef JOINT_HAL_H
#define JOINT_HAL_H

#include "hal.h"

/* HAL pins and parameters for one joint_ctrl instance.
   Pin pointers follow the standard LinuxCNC HAL convention:
     hal_bit_t *  / hal_float_t * / hal_s32_t *
   Parameter values are stored directly (not as pointers). */
typedef struct {
    /* --- Motor interface --- */
    hal_float_t *motor_pos_cmd;       /* OUT: commanded motor position (after backlash) */
    hal_float_t *motor_pos_fb;        /* IN:  actual motor position feedback */
    hal_bit_t   *amp_enable;          /* OUT: amplifier / drive enable */
    hal_bit_t   *amp_fault;           /* IN:  amplifier fault signal */

    /* --- Limit switches --- */
    hal_bit_t   *pos_lim_sw;          /* IN:  positive hard limit switch */
    hal_bit_t   *neg_lim_sw;          /* IN:  negative hard limit switch */

    /* --- Homing --- */
    hal_bit_t   *home_sw;             /* IN:  home switch input */
    hal_bit_t   *index_enable;        /* I/O: index-pulse enable (encoder resets on index) */
    hal_bit_t   *do_home;             /* IN:  rising edge triggers homing */
    hal_bit_t   *do_cancel_home;      /* IN:  cancel homing in progress */
    hal_bit_t   *homing;              /* OUT: currently homing */
    hal_bit_t   *homed;               /* OUT: has been homed */
    hal_s32_t   *home_state;          /* OUT: homing state machine state */

    /* --- Keyboard jogging --- */
    hal_bit_t   *jog_enable;          /* IN:  enable jogging (edge-triggered) */
    hal_float_t *jog_vel;             /* IN:  jog velocity (signed = direction) */
    hal_float_t *jog_incr;            /* IN:  jog increment (0 = continuous) */
    hal_bit_t   *jog_active;          /* OUT: jog motion in progress */

    /* --- Wheel / MPG jogging --- */
    hal_s32_t   *jjog_counts;         /* IN:  jogwheel encoder counts */
    hal_bit_t   *jjog_enable;         /* IN:  jogwheel enable */
    hal_float_t *jjog_scale;          /* IN:  distance per count */
    hal_float_t *jjog_accel_fraction; /* IN:  fraction of max-acc for wheel jog */
    hal_bit_t   *jjog_vel_mode;       /* IN:  velocity mode (vs position mode) */

    /* --- Control and status --- */
    hal_bit_t   *enable;              /* IN:  master enable (drives are enabled) */
    hal_bit_t   *in_position;         /* OUT: joint is at commanded position */
    hal_bit_t   *error;               /* OUT: joint has an error condition */
    hal_float_t *f_error;             /* OUT: current following error */
    hal_float_t *f_error_lim;         /* OUT: current following error limit */
    hal_bit_t   *f_errored;           /* OUT: following error limit exceeded */
    hal_bit_t   *faulted;             /* OUT: amplifier faulted */
    hal_bit_t   *pos_hard_limit;      /* OUT: at positive hard limit */
    hal_bit_t   *neg_hard_limit;      /* OUT: at negative hard limit */
    hal_float_t *joint_pos_cmd;       /* OUT: commanded joint position (no backlash) */
    hal_float_t *joint_pos_fb;        /* OUT: joint position feedback */
    hal_float_t *vel_cmd;             /* OUT: current velocity command */
    hal_bit_t   *override_limits;     /* IN:  override limit switches */
    hal_bit_t   *unlock;              /* OUT: request rotary joint unlock */
    hal_bit_t   *is_unlocked;         /* IN:  joint is currently unlocked */

    /* --- HAL parameters --- */
    hal_s32_t   type;                 /* RW: 0=linear, 1=rotary */
    hal_float_t max_vel;              /* RW: maximum velocity */
    hal_float_t max_acc;              /* RW: maximum acceleration */
    hal_float_t min_limit;            /* RW: soft minimum position limit */
    hal_float_t max_limit;            /* RW: soft maximum position limit */
    hal_float_t max_ferror;           /* RW: max following error (at max speed) */
    hal_float_t min_ferror;           /* RW: min following error (at zero speed) */
    hal_float_t backlash;             /* RW: backlash amount */
    hal_float_t home_offset;          /* RW: home switch/index position offset */
    hal_float_t home_pos;             /* RW: position to move to after homing */
    hal_float_t home_search_vel;      /* RW: coarse search velocity */
    hal_float_t home_latch_vel;       /* RW: fine latch velocity */
    hal_float_t home_final_vel;       /* RW: final move velocity (0 = use max_vel) */
    hal_s32_t   home_flags;           /* RW: HOME_* flag bits */
    hal_bit_t   volatile_home;        /* RW: clear homed on drive-enable */
} joint_hal_t;

#endif /* JOINT_HAL_H */
