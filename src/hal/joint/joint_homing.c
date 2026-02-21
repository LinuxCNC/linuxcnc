/********************************************************************
* Description: joint_homing.c
*   Single-joint homing state machine for the joint_ctrl HAL component.
*   Extracted and refactored from emc/motion/homing.c.
*   Multi-joint sequencing (do_homing_sequence, sync_ready, etc.)
*   has been stripped.  All state is passed via pointers so the
*   component can manage multiple independent joints.
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include "rtapi.h"
#include "rtapi_math.h"
#include "joint_homing.h"

#define ABS(x) (((x) < 0) ? -(x) : (x))

/* Mark strings for translation, but defer translation to userspace */
#define _(s) (s)

/* Length of delay between homing motions (seconds) */
#define HOME_DELAY 0.100

/***********************************************************************
*                         LOCAL HELPERS                                *
************************************************************************/

/* Start a long move toward a limit at the given velocity.
   The move is twice the joint range; a switch or index will stop it. */
static void home_start_move(joint_home_joint_t *joint, double vel)
{
    double joint_range;

    joint_range = joint->max_pos_limit - joint->min_pos_limit;
    if (vel > 0.0) {
        joint->free_tp.pos_cmd = joint->pos_cmd + 2.0 * joint_range;
    } else {
        joint->free_tp.pos_cmd = joint->pos_cmd - 2.0 * joint_range;
    }
    if (fabs(vel) < joint->vel_limit) {
        joint->free_tp.max_vel = fabs(vel);
    } else {
        joint->free_tp.max_vel = joint->vel_limit;
    }
    joint->free_tp.enable = 1;
}

/* Check whether a move that should be in progress has been aborted by
   a limit or by reaching end-of-travel.  Sets HOME_ABORT and returns 1
   on error, returns 0 when everything is still OK. */
static int home_do_moving_checks(joint_home_data_t *h,
                                  joint_home_joint_t *joint)
{
    if (joint->on_pos_limit || joint->on_neg_limit) {
        if (!(h->home_flags & HOME_IGNORE_LIMITS)) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                _("joint_homing: hit limit in home state %d\n"),
                h->home_state);
            h->home_state = HOME_ABORT;
            return 1;
        }
    }
    if (!joint->free_tp.active) {
        joint->free_tp.enable = 0;
        rtapi_print_msg(RTAPI_MSG_ERR,
            _("joint_homing: end of move in home state %d\n"),
            h->home_state);
        h->home_state = HOME_ABORT;
        return 1;
    }
    return 0;
}

/* Set HOME_ABORT and immediate_state=1 when a moving-check fails.
   Must be used inside joint_home_run() where immediate_state is in scope. */
#define ABORT_CHECK() do { \
    if (home_do_moving_checks(h, joint)) { \
        immediate_state = 1; \
    } \
} while (0)

/***********************************************************************
*                         PUBLIC FUNCTIONS                             *
************************************************************************/

void joint_homing_init(joint_home_data_t *h, double servo_period)
{
    h->home_state      = HOME_IDLE;
    h->homing          = 0;
    h->homed           = 0;
    h->home_sw         = 0;
    h->index_enable    = 0;
    h->pause_timer     = 0;
    h->home_offset     = 0.0;
    h->home            = 0.0;
    h->home_final_vel  = 0.0;
    h->home_search_vel = 0.0;
    h->home_latch_vel  = 0.0;
    h->home_flags      = 0;
    h->volatile_home   = 0;
    h->servo_freq      = (servo_period > 1e-9) ? (1.0 / servo_period) : 1000.0;
    h->set_rotary_unlock      = NULL;
    h->get_rotary_is_unlocked = NULL;
}

void joint_home_start(joint_home_data_t *h)
{
    if ((h->home_flags & HOME_NO_REHOME) && h->homed) {
        /* absolute encoder with NO_REHOME: silently ignore */
        return;
    }
    h->home_state = HOME_START;
}

void joint_home_cancel(joint_home_data_t *h)
{
    if (h->homing || h->home_state != HOME_IDLE) {
        h->home_state = HOME_ABORT;
    }
}

void joint_set_unhomed(joint_home_data_t *h)
{
    h->homed = 0;
}

int joint_home_run(joint_home_data_t *h, joint_home_joint_t *joint)
{
    double offset, tmp;
    int home_sw_active, homing_flag;
    int immediate_state;

    homing_flag = 0;
    home_sw_active = h->home_sw ? 1 : 0;
    if (h->home_state != HOME_IDLE) {
        homing_flag = 1;
    }

    /* The state machine can run through multiple states in a single servo
       period using immediate_state.  Do not set immediate_state without also
       changing home_state or an infinite loop will result. */
    do {
        immediate_state = 0;
        switch (h->home_state) {

        case HOME_IDLE:
            /* nothing to do */
            break;

        case HOME_START:
            if ((h->home_flags & HOME_IS_SHARED) && home_sw_active) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    _("joint_homing: cannot home while shared home switch is closed\n"));
                h->home_state = HOME_IDLE;
                break;
            }
            if ((h->home_flags & HOME_NO_REHOME) && h->homed) {
                h->home_state = HOME_IDLE;
                break;
            }
            h->homing = 1;
            h->homed  = 0;
            joint->free_tp.enable = 0;   /* stop any existing motion */
            h->pause_timer = 0;

            if (h->home_flags & HOME_ABSOLUTE_ENCODER) {
                h->home_flags &= ~HOME_IS_SHARED;
                h->home_state = HOME_SET_SWITCH_POSITION;
                immediate_state = 1;
                break;
            }
            if (h->home_flags & HOME_UNLOCK_FIRST) {
                h->home_state = HOME_UNLOCK;
            } else {
                h->home_state = HOME_UNLOCK_WAIT;
                immediate_state = 1;
            }
            break;

        case HOME_UNLOCK:
            if (h->set_rotary_unlock) {
                h->set_rotary_unlock(1);
            }
            h->home_state = HOME_UNLOCK_WAIT;
            break;

        case HOME_UNLOCK_WAIT:
            if ((h->home_flags & HOME_UNLOCK_FIRST) &&
                h->get_rotary_is_unlocked &&
                !h->get_rotary_is_unlocked()) {
                break; /* still waiting for unlock */
            }
            /* unlocked (or no unlock needed) */
            if (h->home_search_vel == 0.0) {
                if (h->home_latch_vel == 0.0) {
                    /* both vels == 0: home at current position */
                    h->home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                } else if (h->home_flags & HOME_USE_INDEX) {
                    h->home_state = HOME_INDEX_ONLY_START;
                    immediate_state = 1;
                } else {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        _("joint_homing: non-zero LATCH_VEL needs SEARCH_VEL or USE_INDEX\n"));
                    h->home_state = HOME_IDLE;
                }
            } else {
                if (h->home_latch_vel != 0.0) {
                    h->home_state = HOME_INITIAL_SEARCH_START;
                    immediate_state = 1;
                } else {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        _("joint_homing: non-zero SEARCH_VEL needs LATCH_VEL\n"));
                    h->home_state = HOME_IDLE;
                }
            }
            break;

        case HOME_INITIAL_BACKOFF_START:
            /* Called when homing starts and the switch is already active.
               Waits for any motion to stop, then starts a backoff move. */
            if (joint->free_tp.active) {
                h->pause_timer = 0;
                break;
            }
            if (h->pause_timer < (int)(HOME_DELAY * h->servo_freq)) {
                h->pause_timer++;
                break;
            }
            h->pause_timer = 0;
            home_start_move(joint, -h->home_search_vel);
            h->home_state = HOME_INITIAL_BACKOFF_WAIT;
            break;

        case HOME_INITIAL_BACKOFF_WAIT:
            if (!home_sw_active) {
                joint->free_tp.enable = 0;
                h->home_state = HOME_INITIAL_SEARCH_START;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_INITIAL_SEARCH_START:
            if (joint->free_tp.active) {
                h->pause_timer = 0;
                break;
            }
            if (h->pause_timer < (int)(HOME_DELAY * h->servo_freq)) {
                h->pause_timer++;
                break;
            }
            h->pause_timer = 0;
            if (home_sw_active) {
                /* already on switch: back off first */
                h->home_state = HOME_INITIAL_BACKOFF_START;
                immediate_state = 1;
                break;
            }
            home_start_move(joint, h->home_search_vel);
            h->home_state = HOME_INITIAL_SEARCH_WAIT;
            break;

        case HOME_INITIAL_SEARCH_WAIT:
            if (home_sw_active) {
                joint->free_tp.enable = 0;
                h->home_state = HOME_SET_COARSE_POSITION;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_SET_COARSE_POSITION:
            /* Approximately at home; reset position so screw-error comp
               is appropriate for this portion of travel. */
            offset = h->home_offset - joint->pos_fb;
            joint->pos_cmd          += offset;
            joint->pos_fb           += offset;
            joint->free_tp.curr_pos += offset;
            joint->motor_offset     -= offset;
            /* choose direction for fine approach */
            tmp = h->home_search_vel * h->home_latch_vel;
            if (tmp > 0.0) {
                h->home_state = HOME_FINAL_BACKOFF_START;
            } else {
                h->home_state = HOME_FALL_SEARCH_START;
            }
            immediate_state = 1;
            break;

        case HOME_FINAL_BACKOFF_START:
            if (joint->free_tp.active) {
                h->pause_timer = 0;
                break;
            }
            if (h->pause_timer < (int)(HOME_DELAY * h->servo_freq)) {
                h->pause_timer++;
                break;
            }
            h->pause_timer = 0;
            if (!home_sw_active) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    _("joint_homing: home switch inactive before backoff\n"));
                h->home_state = HOME_IDLE;
                break;
            }
            home_start_move(joint, -h->home_search_vel);
            h->home_state = HOME_FINAL_BACKOFF_WAIT;
            break;

        case HOME_FINAL_BACKOFF_WAIT:
            if (!home_sw_active) {
                joint->free_tp.enable = 0;
                h->home_state = HOME_RISE_SEARCH_START;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_RISE_SEARCH_START:
            if (joint->free_tp.active) {
                h->pause_timer = 0;
                break;
            }
            if (h->pause_timer < (int)(HOME_DELAY * h->servo_freq)) {
                h->pause_timer++;
                break;
            }
            h->pause_timer = 0;
            if (home_sw_active) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    _("joint_homing: home switch active before latch move\n"));
                h->home_state = HOME_IDLE;
                break;
            }
            home_start_move(joint, h->home_latch_vel);
            h->home_state = HOME_RISE_SEARCH_WAIT;
            break;

        case HOME_RISE_SEARCH_WAIT:
            if (home_sw_active) {
                if (h->home_flags & HOME_USE_INDEX) {
                    h->home_state = HOME_INDEX_SEARCH_START;
                    immediate_state = 1;
                } else {
                    joint->free_tp.enable = 0;
                    h->home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                }
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_FALL_SEARCH_START:
            if (joint->free_tp.active) {
                h->pause_timer = 0;
                break;
            }
            if (h->pause_timer < (int)(HOME_DELAY * h->servo_freq)) {
                h->pause_timer++;
                break;
            }
            h->pause_timer = 0;
            if (!home_sw_active) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    _("joint_homing: home switch inactive before latch move\n"));
                h->home_state = HOME_IDLE;
                break;
            }
            home_start_move(joint, h->home_latch_vel);
            h->home_state = HOME_FALL_SEARCH_WAIT;
            break;

        case HOME_FALL_SEARCH_WAIT:
            if (!home_sw_active) {
                if (h->home_flags & HOME_USE_INDEX) {
                    h->home_state = HOME_INDEX_SEARCH_START;
                    immediate_state = 1;
                } else {
                    joint->free_tp.enable = 0;
                    h->home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                }
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_SET_SWITCH_POSITION:
            if (h->home_flags & HOME_ABSOLUTE_ENCODER) {
                offset = h->home_offset;
            } else {
                offset = h->home_offset - joint->pos_fb;
            }
            joint->pos_cmd          += offset;
            joint->pos_fb           += offset;
            joint->free_tp.curr_pos += offset;
            joint->motor_offset     -= offset;
            if (h->home_flags & HOME_ABSOLUTE_ENCODER) {
                if (h->home_flags & HOME_NO_FINAL_MOVE) {
                    h->homed      = 1;
                    h->home_state = HOME_FINISHED;
                    immediate_state = 1;
                    break;
                }
            }
            h->home_state = HOME_FINAL_MOVE_START;
            immediate_state = 1;
            break;

        case HOME_INDEX_ONLY_START:
            if (joint->free_tp.active) {
                h->pause_timer = 0;
                break;
            }
            if (h->pause_timer < (int)(HOME_DELAY * h->servo_freq)) {
                h->pause_timer++;
                break;
            }
            h->pause_timer = 0;
            offset = h->home_offset - joint->pos_fb;
            joint->pos_cmd          += offset;
            joint->pos_fb           += offset;
            joint->free_tp.curr_pos += offset;
            joint->motor_offset     -= offset;
            h->index_enable = 1;
            home_start_move(joint, h->home_latch_vel);
            h->home_state = HOME_INDEX_SEARCH_WAIT;
            break;

        case HOME_INDEX_SEARCH_START:
            h->index_enable = 1;
            h->home_state   = HOME_INDEX_SEARCH_WAIT;
            immediate_state = 1;
            ABORT_CHECK();
            break;

        case HOME_INDEX_SEARCH_WAIT:
            /* encoder driver clears index_enable when index arrives */
            if (!h->index_enable) {
                joint->free_tp.enable = 0;
                h->home_state = HOME_SET_INDEX_POSITION;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_SET_INDEX_POSITION:
            joint->motor_offset     = -h->home_offset;
            joint->pos_fb           = joint->motor_pos_fb -
                                      (joint->backlash_filt + joint->motor_offset);
            joint->pos_cmd          = joint->pos_fb;
            joint->free_tp.curr_pos = joint->pos_fb;
            if (h->home_flags & HOME_INDEX_NO_ENCODER_RESET) {
                /* encoder did not reset on index; apply offset instead */
                offset = h->home_offset - joint->pos_fb;
                joint->pos_cmd          += offset;
                joint->pos_fb           += offset;
                joint->free_tp.curr_pos += offset;
                joint->motor_offset     -= offset;
            }
            h->home_state = HOME_FINAL_MOVE_START;
            immediate_state = 1;
            break;

        case HOME_FINAL_MOVE_START:
            if (joint->free_tp.active) {
                h->pause_timer = 0;
                break;
            }
            if (h->pause_timer < (int)(HOME_DELAY * h->servo_freq)) {
                h->pause_timer++;
                break;
            }
            h->pause_timer = 0;
            joint->free_tp.pos_cmd = h->home;
            if (h->home_final_vel > 0.0) {
                joint->free_tp.max_vel = fabs(h->home_final_vel);
                if (joint->free_tp.max_vel > joint->vel_limit) {
                    joint->free_tp.max_vel = joint->vel_limit;
                }
            } else {
                joint->free_tp.max_vel = joint->vel_limit;
            }
            joint->free_tp.enable = 1;
            h->home_state = HOME_FINAL_MOVE_WAIT;
            break;

        case HOME_FINAL_MOVE_WAIT:
            if (!joint->free_tp.active) {
                joint->free_tp.enable = 0;
                h->home_state = HOME_LOCK;
                immediate_state = 1;
                break;
            }
            if (joint->on_pos_limit || joint->on_neg_limit) {
                if (!(h->home_flags & HOME_IGNORE_LIMITS)) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        _("joint_homing: hit limit during final homing move\n"));
                    h->home_state = HOME_ABORT;
                    immediate_state = 1;
                    break;
                }
            }
            break;

        case HOME_LOCK:
            if (h->home_flags & HOME_UNLOCK_FIRST) {
                if (h->set_rotary_unlock) {
                    h->set_rotary_unlock(0);
                }
            } else {
                immediate_state = 1;
            }
            h->home_state = HOME_LOCK_WAIT;
            break;

        case HOME_LOCK_WAIT:
            if ((h->home_flags & HOME_UNLOCK_FIRST) &&
                h->get_rotary_is_unlocked &&
                h->get_rotary_is_unlocked()) {
                break; /* still waiting for lock */
            }
            h->home_state = HOME_FINISHED;
            immediate_state = 1;
            break;

        case HOME_FINISHED:
            h->homing     = 0;
            h->homed      = 1;
            h->home_state = HOME_IDLE;
            if (!(h->home_flags & HOME_ABSOLUTE_ENCODER)) {
                joint->free_tp.curr_pos = h->home;
            }
            immediate_state = 1;
            break;

        case HOME_ABORT:
            h->homing       = 0;
            h->homed        = 0;
            h->index_enable = 0;
            joint->free_tp.enable = 0;
            h->home_state   = HOME_IDLE;
            immediate_state = 1;
            break;

        default:
            rtapi_print_msg(RTAPI_MSG_ERR,
                _("joint_homing: unknown state %d\n"), h->home_state);
            h->home_state = HOME_ABORT;
            immediate_state = 1;
            break;

        } /* end switch(home_state) */
    } while (immediate_state);

    return homing_flag;
}
