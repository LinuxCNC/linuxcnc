/********************************************************************
* Description: joint_ctrl_main.c
*   Standalone HAL realtime component for single-joint control.
*
*   Provides: homing, keyboard jogging, jogwheel jogging, trapezoidal
*   trajectory planning, following-error monitoring, limit-switch
*   handling, and backlash compensation — all as a self-contained
*   HAL component independent of the full LinuxCNC motion controller.
*
*   Usage:
*     loadrt joint_ctrl count=N
*     addf joint_ctrl.N.update servo-thread
*
*   Sources extracted/adapted from:
*     emc/motion/control.c   — process_inputs, check_for_faults,
*                               compute_screw_comp, output_to_hal
*     emc/motion/command.c   — JOG_CONT, JOG_INCR, handle_jjogwheels
*     emc/motion/homing.c    — base_1joint_state_machine  (-> joint_homing.c)
*     emc/motion/simple_tp.c — simple_tp_update           (-> joint_simple_tp.c)
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_math.h"
#include "hal.h"

#include "joint_hal.h"
#include "joint_simple_tp.h"
#include "joint_homing.h"

MODULE_AUTHOR("LinuxCNC");
MODULE_DESCRIPTION("Standalone single-joint control HAL component");
MODULE_LICENSE("GPL");

static int count = 1;
RTAPI_MP_INT(count, "Number of joint_ctrl instances");

#define MAX_JOINTS 16

static int comp_id;

/***********************************************************************
*                       PER-INSTANCE STATE                             *
************************************************************************/

typedef struct {
    joint_hal_t        hal;        /* HAL pins and parameters */
    joint_home_data_t  home;       /* homing state machine */
    joint_home_joint_t jstate;     /* joint state used by homing + free_tp */

    /* Internal state not stored in jstate */
    double  vel_cmd;           /* current velocity command */
    double  motor_pos_cmd;     /* motor command after backlash + motor_offset */
    double  backlash_corr;     /* computed backlash correction */
    double  backlash_vel;      /* backlash velocity (for ramped correction) */
    double  ferror;            /* current following error */
    double  ferror_limit;      /* current following error limit */
    double  max_jog_limit;     /* dynamic jog limit (positive direction) */
    double  min_jog_limit;     /* dynamic jog limit (negative direction) */

    int     kb_jjog_active;    /* non-zero during a keyboard jog */
    int     wheel_jjog_active; /* non-zero during a jogwheel jog */
    int     old_jjog_counts;   /* previous jogwheel count for delta */

    int     enabled;           /* drives are on and no fault */
    int     was_enabled;       /* enabled state from previous cycle */
    int     in_error;          /* joint has an active error */
    int     ferr_flag;         /* following error exceeded */

    /* Edge-detection state */
    int     prev_do_home;
    int     prev_jog_enable;

    double  servo_period;      /* servo cycle time in seconds */
} joint_ctrl_t;

static joint_ctrl_t *ctrl_array;

/***********************************************************************
*                         HELPER FUNCTIONS                             *
************************************************************************/

/* Compute dynamic jog limits.  When homed, use soft limits; when not
   homed, extend by the full joint range so the operator can still move. */
static void refresh_jog_limits(joint_ctrl_t *ctrl)
{
    double range;

    if (ctrl->home.homed) {
        ctrl->max_jog_limit = ctrl->hal.max_limit;
        ctrl->min_jog_limit = ctrl->hal.min_limit;
    } else {
        range = ctrl->hal.max_limit - ctrl->hal.min_limit;
        ctrl->max_jog_limit = ctrl->jstate.pos_fb + range;
        ctrl->min_jog_limit = ctrl->jstate.pos_fb - range;
    }
}

/***********************************************************************
*                        REALTIME UPDATE FUNCTION                      *
************************************************************************/

static void joint_ctrl_update(void *arg, long period)
{
    joint_ctrl_t *ctrl = (joint_ctrl_t *)arg;
    double fperiod = period * 1e-9;

    /* backlash ramp temporaries */
    double bv_max, ba_max, bv, bs_to_go, bds_stop, bds_vel, bds_acc, bdv_acc;

    int    new_jjog_counts, delta;
    int    do_home_cur, jog_enable_cur;
    int    enable, override_limits, amp_fault;
    int    homing_active;
    double abs_ferror;

    /* ----------------------------------------------------------------
     * Step 1: Read input pins
     * ---------------------------------------------------------------- */

    ctrl->jstate.motor_pos_fb = *(ctrl->hal.motor_pos_fb);
    ctrl->jstate.on_pos_limit  = *(ctrl->hal.pos_lim_sw) ? 1 : 0;
    ctrl->jstate.on_neg_limit  = *(ctrl->hal.neg_lim_sw) ? 1 : 0;

    ctrl->home.home_sw      = *(ctrl->hal.home_sw)      ? 1 : 0;
    ctrl->home.index_enable = *(ctrl->hal.index_enable) ? 1 : 0;

    enable          = *(ctrl->hal.enable)          ? 1 : 0;
    override_limits = *(ctrl->hal.override_limits)  ? 1 : 0;
    amp_fault       = *(ctrl->hal.amp_fault)        ? 1 : 0;

    /* ----------------------------------------------------------------
     * Step 2: Detect do-home rising edge → start homing
     * ---------------------------------------------------------------- */

    do_home_cur = *(ctrl->hal.do_home) ? 1 : 0;
    if (do_home_cur && !ctrl->prev_do_home && enable) {
        joint_home_start(&ctrl->home);
    }
    ctrl->prev_do_home = do_home_cur;

    if (*(ctrl->hal.do_cancel_home)) {
        joint_home_cancel(&ctrl->home);
    }

    homing_active = ctrl->home.homing ||
                    (ctrl->home.home_state != HOME_IDLE);

    /* Push current HAL param values into homing data each cycle so
       changes take effect without a module reload. */
    ctrl->home.home_offset     = ctrl->hal.home_offset;
    ctrl->home.home            = ctrl->hal.home_pos;
    ctrl->home.home_search_vel = ctrl->hal.home_search_vel;
    ctrl->home.home_latch_vel  = ctrl->hal.home_latch_vel;
    ctrl->home.home_final_vel  = ctrl->hal.home_final_vel;
    ctrl->home.home_flags      = ctrl->hal.home_flags;
    ctrl->home.volatile_home   = ctrl->hal.volatile_home;

    /* Push current limits / velocity into jstate so homing sees them. */
    ctrl->jstate.max_pos_limit = ctrl->hal.max_limit;
    ctrl->jstate.min_pos_limit = ctrl->hal.min_limit;
    ctrl->jstate.vel_limit     = ctrl->hal.max_vel;

    /* ----------------------------------------------------------------
     * Step 3: Compute position feedback
     * ---------------------------------------------------------------- */

    /* Special case during index search: encoder counter may be about to
       step-change.  Hold pos_fb == pos_cmd to avoid false following error. */
    if ((ctrl->home.home_state == HOME_INDEX_SEARCH_WAIT) &&
        (ctrl->home.index_enable == 0)) {
        ctrl->jstate.pos_fb = ctrl->jstate.pos_cmd;
    } else {
        /* Normal case: subtract backlash filter and motor offset from motor fb */
        ctrl->jstate.pos_fb = ctrl->jstate.motor_pos_fb -
                              (ctrl->jstate.backlash_filt +
                               ctrl->jstate.motor_offset);
    }

    /* ----------------------------------------------------------------
     * Step 4: Compute following error
     * ---------------------------------------------------------------- */

    ctrl->ferror = ctrl->jstate.pos_cmd - ctrl->jstate.pos_fb;
    abs_ferror   = fabs(ctrl->ferror);

    if (ctrl->hal.max_vel > 0.0) {
        ctrl->ferror_limit = ctrl->hal.max_ferror *
                             fabs(ctrl->vel_cmd) / ctrl->hal.max_vel;
    } else {
        ctrl->ferror_limit = 0.0;
    }
    if (ctrl->ferror_limit < ctrl->hal.min_ferror) {
        ctrl->ferror_limit = ctrl->hal.min_ferror;
    }

    ctrl->ferr_flag = (abs_ferror > ctrl->ferror_limit) ? 1 : 0;

    /* ----------------------------------------------------------------
     * Step 5: Check for faults
     * ---------------------------------------------------------------- */

    ctrl->in_error = 0;

    /* Hard limits (ignored while homing or when override active) */
    if (!override_limits && !homing_active) {
        if (ctrl->jstate.on_pos_limit || ctrl->jstate.on_neg_limit) {
            ctrl->in_error = 1;
        }
    }

    if (amp_fault) {
        ctrl->in_error = 1;
    }

    if (ctrl->ferr_flag) {
        ctrl->in_error = 1;
    }

    /* ----------------------------------------------------------------
     * Step 6: Handle enable / disable
     * ---------------------------------------------------------------- */

    if (!enable || ctrl->in_error) {
        if (ctrl->was_enabled) {
            /* Transition to disabled: stop all motion */
            ctrl->jstate.free_tp.enable = 0;
            ctrl->jstate.free_tp.curr_vel = 0.0;
            ctrl->kb_jjog_active    = 0;
            ctrl->wheel_jjog_active = 0;
            if (ctrl->home.volatile_home) {
                joint_set_unhomed(&ctrl->home);
            }
        }
        ctrl->enabled = 0;
    } else {
        ctrl->enabled = 1;
    }
    ctrl->was_enabled = ctrl->enabled;

    /* ----------------------------------------------------------------
     * Step 7: Run homing state machine
     * ---------------------------------------------------------------- */

    if (homing_active) {
        joint_home_run(&ctrl->home, &ctrl->jstate);
        /* homing may have updated pos_cmd, pos_fb, motor_offset */
    }

    /* ----------------------------------------------------------------
     * Step 8: Handle keyboard jogging
     * ---------------------------------------------------------------- */

    if (ctrl->enabled && !homing_active) {
        jog_enable_cur = *(ctrl->hal.jog_enable) ? 1 : 0;

        if (jog_enable_cur && !ctrl->prev_jog_enable) {
            /* Rising edge of jog-enable → start jog */
            double vel  = *(ctrl->hal.jog_vel);
            double incr = *(ctrl->hal.jog_incr);

            if (vel != 0.0 && !ctrl->wheel_jjog_active) {
                refresh_jog_limits(ctrl);

                if (incr == 0.0) {
                    /* Continuous jog: drive to the relevant limit */
                    if (vel > 0.0 && !ctrl->jstate.on_pos_limit) {
                        ctrl->jstate.free_tp.pos_cmd = ctrl->max_jog_limit;
                    } else if (vel < 0.0 && !ctrl->jstate.on_neg_limit) {
                        ctrl->jstate.free_tp.pos_cmd = ctrl->min_jog_limit;
                    }
                } else {
                    /* Incremental jog: move by jog-incr in jog-vel direction */
                    double target = ctrl->jstate.free_tp.pos_cmd +
                                    (vel > 0.0 ? 1.0 : -1.0) * fabs(incr);
                    if (target > ctrl->max_jog_limit) {
                        target = ctrl->max_jog_limit;
                    }
                    if (target < ctrl->min_jog_limit) {
                        target = ctrl->min_jog_limit;
                    }
                    ctrl->jstate.free_tp.pos_cmd = target;
                }

                ctrl->jstate.free_tp.max_vel = fabs(vel);
                ctrl->jstate.free_tp.max_acc = ctrl->hal.max_acc;
                ctrl->jstate.free_tp.enable  = 1;
                ctrl->kb_jjog_active    = 1;
                ctrl->wheel_jjog_active = 0;
            }
        }

        /* Falling edge of jog-enable → abort jog */
        if (!jog_enable_cur && ctrl->prev_jog_enable && ctrl->kb_jjog_active) {
            ctrl->jstate.free_tp.enable = 0;
            ctrl->kb_jjog_active = 0;
        }

        /* Clear flag when motion completes (incremental jog) */
        if (ctrl->kb_jjog_active &&
            !ctrl->jstate.free_tp.active &&
            !ctrl->jstate.free_tp.enable) {
            ctrl->kb_jjog_active = 0;
        }

        ctrl->prev_jog_enable = jog_enable_cur;
    }

    /* ----------------------------------------------------------------
     * Step 9: Handle wheel / MPG jogging
     * ---------------------------------------------------------------- */

    if (ctrl->enabled && !homing_active) {
        double jaccel_limit;

        if ((*(ctrl->hal.jjog_accel_fraction) > 1.0) ||
            (*(ctrl->hal.jjog_accel_fraction) < 0.0)) {
            jaccel_limit = ctrl->hal.max_acc;
        } else {
            jaccel_limit = *(ctrl->hal.jjog_accel_fraction) * ctrl->hal.max_acc;
        }

        new_jjog_counts = *(ctrl->hal.jjog_counts);
        delta = new_jjog_counts - ctrl->old_jjog_counts;
        ctrl->old_jjog_counts = new_jjog_counts;

        if (delta != 0 &&
            *(ctrl->hal.jjog_enable) &&
            !ctrl->kb_jjog_active) {

            double distance = (double)delta * *(ctrl->hal.jjog_scale);
            int    do_jog   = 1;
            double pos;

            /* Refuse to jog further onto a hard limit */
            if (distance > 0.0 && ctrl->jstate.on_pos_limit) { do_jog = 0; }
            if (distance < 0.0 && ctrl->jstate.on_neg_limit) { do_jog = 0; }

            if (do_jog) {
                pos = ctrl->jstate.free_tp.pos_cmd + distance;
                refresh_jog_limits(ctrl);
                if (pos > ctrl->max_jog_limit) { do_jog = 0; }
                if (pos < ctrl->min_jog_limit) { do_jog = 0; }
            }

            if (do_jog) {
                /* Velocity mode: clamp so motion stops when wheel stops */
                if (*(ctrl->hal.jjog_vel_mode) && jaccel_limit > 0.0) {
                    double v2        = ctrl->hal.max_vel;
                    double stop_dist = v2 * v2 / (2.0 * jaccel_limit);
                    double cur_pos   = ctrl->jstate.pos_cmd;
                    if (pos > cur_pos + stop_dist) { pos = cur_pos + stop_dist; }
                    if (pos < cur_pos - stop_dist) { pos = cur_pos - stop_dist; }
                }

                ctrl->jstate.free_tp.pos_cmd = pos;
                ctrl->jstate.free_tp.max_vel = ctrl->hal.max_vel;
                ctrl->jstate.free_tp.max_acc = jaccel_limit;
                ctrl->jstate.free_tp.enable  = 1;
                ctrl->wheel_jjog_active = 1;
                ctrl->kb_jjog_active    = 0;
            }
        }

        /* Clear wheel flag when motion completes */
        if (ctrl->wheel_jjog_active && !ctrl->jstate.free_tp.active) {
            ctrl->wheel_jjog_active = 0;
        }
    }

    /* ----------------------------------------------------------------
     * Step 10: Run trajectory planner
     * ---------------------------------------------------------------- */

    /* Do NOT unconditionally override max_vel/max_acc here: the jog
       and homing code sets them appropriately when initiating motion.
       Clamp to configured maximums as a safety guard only. */
    if (ctrl->jstate.free_tp.max_vel > ctrl->hal.max_vel) {
        ctrl->jstate.free_tp.max_vel = ctrl->hal.max_vel;
    }
    if (ctrl->jstate.free_tp.max_acc > ctrl->hal.max_acc) {
        ctrl->jstate.free_tp.max_acc = ctrl->hal.max_acc;
    }

    simple_tp_update(&ctrl->jstate.free_tp, fperiod);

    ctrl->jstate.pos_cmd = ctrl->jstate.free_tp.curr_pos;
    ctrl->vel_cmd        = ctrl->jstate.free_tp.curr_vel;

    /* ----------------------------------------------------------------
     * Step 11: Compute backlash compensation (ramped, from control.c)
     * ---------------------------------------------------------------- */

    /* Determine which direction backlash correction should be applied */
    if (ctrl->vel_cmd > 0.0) {
        ctrl->backlash_corr =  0.5 * ctrl->hal.backlash;
    } else if (ctrl->vel_cmd < 0.0) {
        ctrl->backlash_corr = -0.5 * ctrl->hal.backlash;
    }
    /* else: not moving, keep previous correction direction */

    /* Ramp backlash_filt toward backlash_corr at capped accel/vel */
    bv_max = 0.5 * ctrl->hal.max_vel;
    ba_max = 0.5 * ctrl->hal.max_acc;
    bv     = ctrl->backlash_vel;

    if (ctrl->backlash_corr >= ctrl->jstate.backlash_filt) {
        bs_to_go = ctrl->backlash_corr - ctrl->jstate.backlash_filt;
        if (bs_to_go > 0.0) {
            bds_vel  = bv * fperiod;
            bdv_acc  = ba_max * fperiod;
            bds_stop = 0.5 * (bv + bdv_acc) * (bv + bdv_acc) / ba_max;
            if (bs_to_go <= bds_stop + bds_vel) {
                if (bv > bdv_acc) {
                    bds_acc = 0.5 * bdv_acc * fperiod;
                    ctrl->backlash_vel          -= bdv_acc;
                    ctrl->jstate.backlash_filt  += bds_vel - bds_acc;
                } else {
                    ctrl->backlash_vel          = 0.0;
                    ctrl->jstate.backlash_filt  = ctrl->backlash_corr;
                }
            } else {
                if (bv + bdv_acc > bv_max) { bdv_acc = bv_max - bv; }
                bds_acc  = 0.5 * bdv_acc * fperiod;
                bds_stop = 0.5 * (bv + bdv_acc) * (bv + bdv_acc) / ba_max;
                if (bs_to_go > bds_stop + bds_vel + bds_acc) {
                    ctrl->backlash_vel         += bdv_acc;
                    ctrl->jstate.backlash_filt += bds_vel + bds_acc;
                } else {
                    ctrl->jstate.backlash_filt += bds_vel;
                }
            }
        } else if (bs_to_go < 0.0) {
            ctrl->backlash_vel         = 0.0;
            ctrl->jstate.backlash_filt = ctrl->backlash_corr;
        }
    } else {
        bs_to_go = ctrl->jstate.backlash_filt - ctrl->backlash_corr;
        if (bs_to_go > 0.0) {
            /* bv is negative in this branch; use fabs to keep distances positive */
            bds_vel  = fabs(bv) * fperiod;
            bdv_acc  = ba_max * fperiod;
            bds_stop = 0.5 * (bv - bdv_acc) * (bv - bdv_acc) / ba_max;
            if (bs_to_go <= bds_stop + bds_vel) {
                if (-bv > bdv_acc) {
                    bds_acc = 0.5 * bdv_acc * fperiod;
                    ctrl->backlash_vel         += bdv_acc;
                    ctrl->jstate.backlash_filt -= bds_vel - bds_acc;
                } else {
                    ctrl->backlash_vel         = 0.0;
                    ctrl->jstate.backlash_filt = ctrl->backlash_corr;
                }
            } else {
                if (-bv + bdv_acc > bv_max) { bdv_acc = bv_max + bv; }
                bds_acc  = 0.5 * bdv_acc * fperiod;
                bds_stop = 0.5 * (bv - bdv_acc) * (bv - bdv_acc) / ba_max;
                if (bs_to_go > bds_stop + bds_vel + bds_acc) {
                    ctrl->backlash_vel         -= bdv_acc;
                    ctrl->jstate.backlash_filt -= bds_vel + bds_acc;
                } else {
                    ctrl->jstate.backlash_filt -= bds_vel;
                }
            }
        } else if (bs_to_go < 0.0) {
            ctrl->backlash_vel         = 0.0;
            ctrl->jstate.backlash_filt = ctrl->backlash_corr;
        }
    }

    /* Final motor command: joint position + backlash + motor offset */
    ctrl->motor_pos_cmd = ctrl->jstate.pos_cmd +
                          ctrl->jstate.backlash_filt +
                          ctrl->jstate.motor_offset;

    /* ----------------------------------------------------------------
     * Step 12: Write output pins
     * ---------------------------------------------------------------- */

    *(ctrl->hal.motor_pos_cmd)  = ctrl->motor_pos_cmd;
    *(ctrl->hal.joint_pos_cmd)  = ctrl->jstate.pos_cmd;
    *(ctrl->hal.joint_pos_fb)   = ctrl->jstate.pos_fb;
    *(ctrl->hal.vel_cmd)        = ctrl->vel_cmd;
    *(ctrl->hal.amp_enable)     = ctrl->enabled;

    /* Write back index_enable (I/O pin: encoder clears it on index) */
    *(ctrl->hal.index_enable)   = ctrl->home.index_enable ? 1 : 0;

    *(ctrl->hal.homing)         = ctrl->home.homing     ? 1 : 0;
    *(ctrl->hal.homed)          = ctrl->home.homed      ? 1 : 0;
    *(ctrl->hal.home_state)     = (hal_s32_t)ctrl->home.home_state;

    *(ctrl->hal.jog_active)     = (ctrl->kb_jjog_active ||
                                   ctrl->wheel_jjog_active) ? 1 : 0;

    *(ctrl->hal.in_position)    = !ctrl->jstate.free_tp.active ? 1 : 0;
    *(ctrl->hal.error)          = ctrl->in_error;
    *(ctrl->hal.f_error)        = ctrl->ferror;
    *(ctrl->hal.f_error_lim)    = ctrl->ferror_limit;
    *(ctrl->hal.f_errored)      = ctrl->ferr_flag;
    *(ctrl->hal.faulted)        = amp_fault;
    *(ctrl->hal.pos_hard_limit) = ctrl->jstate.on_pos_limit;
    *(ctrl->hal.neg_hard_limit) = ctrl->jstate.on_neg_limit;

    /* Clear rotary-unlock output when drives are disabled */
    if (!ctrl->enabled) {
        *(ctrl->hal.unlock) = 0;
    }
}

/***********************************************************************
*                     PIN AND PARAMETER EXPORT                         *
************************************************************************/

static int export_joint_ctrl(int n, joint_ctrl_t *ctrl)
{
    int   retval = 0;
    char  prefix[HAL_NAME_LEN + 1];
    char  buf[HAL_NAME_LEN + 1];

    rtapi_snprintf(prefix, sizeof(prefix), "joint_ctrl.%d", n);

    /* Helper macros to keep pin/param declarations compact */
#define PINF_OUT(m, pn) \
    retval += hal_pin_float_newf(HAL_OUT, &(ctrl->hal.m), comp_id, \
                                 "%s." pn, prefix)
#define PINF_IN(m, pn) \
    retval += hal_pin_float_newf(HAL_IN,  &(ctrl->hal.m), comp_id, \
                                 "%s." pn, prefix)
#define PINB_OUT(m, pn) \
    retval += hal_pin_bit_newf(HAL_OUT, &(ctrl->hal.m), comp_id, \
                               "%s." pn, prefix)
#define PINB_IN(m, pn) \
    retval += hal_pin_bit_newf(HAL_IN,  &(ctrl->hal.m), comp_id, \
                               "%s." pn, prefix)
#define PINB_IO(m, pn) \
    retval += hal_pin_bit_newf(HAL_IO,  &(ctrl->hal.m), comp_id, \
                               "%s." pn, prefix)
#define PINS32_OUT(m, pn) \
    retval += hal_pin_s32_newf(HAL_OUT, &(ctrl->hal.m), comp_id, \
                               "%s." pn, prefix)
#define PINS32_IN(m, pn) \
    retval += hal_pin_s32_newf(HAL_IN,  &(ctrl->hal.m), comp_id, \
                               "%s." pn, prefix)

    /* Motor interface */
    PINF_OUT(motor_pos_cmd, "motor-pos-cmd");
    PINF_IN (motor_pos_fb,  "motor-pos-fb");
    PINB_OUT(amp_enable,    "amp-enable");
    PINB_IN (amp_fault,     "amp-fault");

    /* Limit switches */
    PINB_IN(pos_lim_sw, "pos-lim-sw-in");
    PINB_IN(neg_lim_sw, "neg-lim-sw-in");

    /* Homing */
    PINB_IN  (home_sw,        "home-sw-in");
    PINB_IO  (index_enable,   "index-enable");
    PINB_IN  (do_home,        "do-home");
    PINB_IN  (do_cancel_home, "do-cancel-home");
    PINB_OUT (homing,         "homing");
    PINB_OUT (homed,          "homed");
    PINS32_OUT(home_state,    "home-state");

    /* Keyboard jogging */
    PINB_IN (jog_enable, "jog-enable");
    PINF_IN (jog_vel,    "jog-vel");
    PINF_IN (jog_incr,   "jog-incr");
    PINB_OUT(jog_active, "jog-active");

    /* Wheel / MPG jogging */
    PINS32_IN(jjog_counts,           "jjog-counts");
    PINB_IN  (jjog_enable,           "jjog-enable");
    PINF_IN  (jjog_scale,            "jjog-scale");
    PINF_IN  (jjog_accel_fraction,   "jjog-accel-fraction");
    PINB_IN  (jjog_vel_mode,         "jjog-vel-mode");

    /* Control and status */
    PINB_IN  (enable,          "enable");
    PINB_OUT (in_position,     "in-position");
    PINB_OUT (error,           "error");
    PINF_OUT (f_error,         "f-error");
    PINF_OUT (f_error_lim,     "f-error-lim");
    PINB_OUT (f_errored,       "f-errored");
    PINB_OUT (faulted,         "faulted");
    PINB_OUT (pos_hard_limit,  "pos-hard-limit");
    PINB_OUT (neg_hard_limit,  "neg-hard-limit");
    PINF_OUT (joint_pos_cmd,   "joint-pos-cmd");
    PINF_OUT (joint_pos_fb,    "joint-pos-fb");
    PINF_OUT (vel_cmd,         "vel-cmd");
    PINB_IN  (override_limits, "override-limits");
    PINB_OUT (unlock,          "unlock");
    PINB_IN  (is_unlocked,     "is-unlocked");

#undef PINF_OUT
#undef PINF_IN
#undef PINB_OUT
#undef PINB_IN
#undef PINB_IO
#undef PINS32_OUT
#undef PINS32_IN

    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "JOINT_CTRL: pin export failed for instance %d\n", n);
        return retval;
    }

    /* Parameters */
#define PARAMS32(m, pn) \
    retval += hal_param_s32_newf(HAL_RW, &(ctrl->hal.m), comp_id, \
                                 "%s." pn, prefix)
#define PARAMF(m, pn) \
    retval += hal_param_float_newf(HAL_RW, &(ctrl->hal.m), comp_id, \
                                   "%s." pn, prefix)
#define PARAMB(m, pn) \
    retval += hal_param_bit_newf(HAL_RW, &(ctrl->hal.m), comp_id, \
                                 "%s." pn, prefix)

    PARAMS32(type,            "type");
    PARAMF  (max_vel,         "max-vel");
    PARAMF  (max_acc,         "max-acc");
    PARAMF  (min_limit,       "min-limit");
    PARAMF  (max_limit,       "max-limit");
    PARAMF  (max_ferror,      "max-ferror");
    PARAMF  (min_ferror,      "min-ferror");
    PARAMF  (backlash,        "backlash");
    PARAMF  (home_offset,     "home-offset");
    PARAMF  (home_pos,        "home-pos");
    PARAMF  (home_search_vel, "home-search-vel");
    PARAMF  (home_latch_vel,  "home-latch-vel");
    PARAMF  (home_final_vel,  "home-final-vel");
    PARAMS32(home_flags,      "home-flags");
    PARAMB  (volatile_home,   "volatile-home");

#undef PARAMS32
#undef PARAMF
#undef PARAMB

    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "JOINT_CTRL: param export failed for instance %d\n", n);
        return retval;
    }

    /* ---- Initialize HAL parameters to safe defaults ---- */
    ctrl->hal.type            = 0;     /* linear */
    ctrl->hal.max_vel         = 1.0;
    ctrl->hal.max_acc         = 10.0;
    ctrl->hal.min_limit       = -1e20;
    ctrl->hal.max_limit       =  1e20;
    ctrl->hal.max_ferror      = 1.0;
    ctrl->hal.min_ferror      = 0.01;
    ctrl->hal.backlash        = 0.0;
    ctrl->hal.home_offset     = 0.0;
    ctrl->hal.home_pos        = 0.0;
    ctrl->hal.home_search_vel = 0.0;
    ctrl->hal.home_latch_vel  = 0.0;
    ctrl->hal.home_final_vel  = 0.0;
    ctrl->hal.home_flags      = 0;
    ctrl->hal.volatile_home   = 0;

    /* ---- Initialize internal state ---- */
    ctrl->vel_cmd           = 0.0;
    ctrl->motor_pos_cmd     = 0.0;
    ctrl->backlash_corr     = 0.0;
    ctrl->backlash_vel      = 0.0;
    ctrl->ferror            = 0.0;
    ctrl->ferror_limit      = 0.0;
    ctrl->max_jog_limit     =  1e20;
    ctrl->min_jog_limit     = -1e20;
    ctrl->kb_jjog_active    = 0;
    ctrl->wheel_jjog_active = 0;
    ctrl->old_jjog_counts   = 0;
    ctrl->enabled           = 0;
    ctrl->was_enabled       = 0;
    ctrl->in_error          = 0;
    ctrl->ferr_flag         = 0;
    ctrl->prev_do_home      = 0;
    ctrl->prev_jog_enable   = 0;

    /* ---- Initialize joint state (jstate) ---- */
    ctrl->jstate.pos_cmd      = 0.0;
    ctrl->jstate.pos_fb       = 0.0;
    ctrl->jstate.motor_pos_fb = 0.0;
    ctrl->jstate.motor_offset = 0.0;
    ctrl->jstate.backlash_filt = 0.0;
    ctrl->jstate.max_pos_limit =  1e20;
    ctrl->jstate.min_pos_limit = -1e20;
    ctrl->jstate.vel_limit     = 1.0;
    ctrl->jstate.on_pos_limit  = 0;
    ctrl->jstate.on_neg_limit  = 0;
    ctrl->jstate.free_tp.pos_cmd  = 0.0;
    ctrl->jstate.free_tp.curr_pos = 0.0;
    ctrl->jstate.free_tp.curr_vel = 0.0;
    ctrl->jstate.free_tp.max_vel  = 1.0;
    ctrl->jstate.free_tp.max_acc  = 10.0;
    ctrl->jstate.free_tp.enable   = 0;
    ctrl->jstate.free_tp.active   = 0;

    /* ---- Initialize homing ---- */
    joint_homing_init(&ctrl->home, ctrl->servo_period);

    /* ---- Export the per-instance realtime function ---- */
    rtapi_snprintf(buf, sizeof(buf), "%s.update", prefix);
    retval = hal_export_funct(buf, joint_ctrl_update, ctrl, 1, 0, comp_id);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "JOINT_CTRL: update funct export failed for instance %d\n", n);
        return retval;
    }

    return 0;
}

/***********************************************************************
*                         MODULE INIT / EXIT                           *
************************************************************************/

int rtapi_app_main(void)
{
    int n, retval;

    if (count <= 0 || count > MAX_JOINTS) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "JOINT_CTRL: invalid count=%d (must be 1..%d)\n",
            count, MAX_JOINTS);
        return -EINVAL;
    }

    comp_id = hal_init("joint_ctrl");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "JOINT_CTRL: hal_init() failed\n");
        return -1;
    }

    ctrl_array = hal_malloc(count * sizeof(joint_ctrl_t));
    if (!ctrl_array) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "JOINT_CTRL: hal_malloc() failed\n");
        hal_exit(comp_id);
        return -1;
    }

    for (n = 0; n < count; n++) {
        /* Use a nominal 1 ms servo period for homing timing.
           The actual period is passed in period (nanoseconds) at
           runtime; homing only needs an approximate frequency. */
        ctrl_array[n].servo_period = 0.001;

        retval = export_joint_ctrl(n, &ctrl_array[n]);
        if (retval != 0) {
            hal_exit(comp_id);
            return -1;
        }
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
        "JOINT_CTRL: installed %d instance(s)\n", count);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
