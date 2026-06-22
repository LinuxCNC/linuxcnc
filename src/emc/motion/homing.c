/********************************************************************
* Description: homing.c
*   Per-joint homing module. Each instance manages ONE joint.
*   Originally based on the multi-joint homing.c but refactored
*   so each instance owns a single joint's homing state machine.
*
* Author: jmkasunich (original), refactored for per-joint by AI
* License: GPL Version 2
*
* Copyright (c) 2004 All rights reserved.
* Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — per-joint homing cmod port
********************************************************************/

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gomc_env.h"
#include "home_api.h"
#include "mot_api.h"
#include "motion.h"
#include "homing.h"


#define ABS(x) (((x) < 0) ? -(x) : (x))

/***********************************************************************
*                         LOCAL CONSTANTS                              *
************************************************************************/

#define HOME_DELAY 0.100

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/* internal states for homing */
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
} home_state_t;

// per-joint homing data
typedef struct {
  home_state_t home_state;
  bool         homing;
  bool         homed;
  bool         home_sw;
  bool         index_enable;
  int          pause_timer;
  double       home_offset;
  double       home;
  double       home_final_vel;
  double       home_search_vel;
  double       home_latch_vel;
  int          home_flags;
  int          home_sequence;
  bool         volatile_home;
  bool         sync_final_move_released; /* set by do_final_move() to release sync pause */
} home_local_data;

// HAL pin data for one joint
typedef struct {
    gomc_hal_bit_t *home_sw;
    gomc_hal_bit_t *homing;
    gomc_hal_bit_t *homed;
    gomc_hal_bit_t *index_enable;
    gomc_hal_s32_t *home_state;
} joint_home_pins_t;

/***********************************************************************
*              PER-INSTANCE STATE                                      *
************************************************************************/

typedef struct {
    const mot_callbacks_t *mot;
    const gomc_api_t *api;
    const gomc_hal_t *hal;
    const gomc_log_t *log;
    char name[GOMC_HAL_NAME_LEN + 1];
    char mot_instance[GOMC_HAL_NAME_LEN + 1];
    char pin_prefix[GOMC_HAL_NAME_LEN + 1]; /* empty for default, "prefix." for non-default */
    int comp_id;
    int jno;            /* joint number — parsed from instance name */
    double servo_freq;
    home_local_data H;  /* single joint state */
    joint_home_pins_t pins;
    home_callbacks_t callbacks;
    cmod_t cmod;
} homemod_inst_t;


/***********************************************************************
*                      LOCAL FUNCTIONS                                 *
************************************************************************/

static void home_start_move(homemod_inst_t *inst, double vel)
{
    int jno = inst->jno;
    double joint_range;

    joint_range = inst->mot->joint_get_max_pos_limit(inst->mot->ctx, jno)
                - inst->mot->joint_get_min_pos_limit(inst->mot->ctx, jno);
    if (vel > 0.0) {
        inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno,
            inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) + 2.0 * joint_range);
    } else {
        inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno,
            inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) - 2.0 * joint_range);
    }
    if (fabs(vel) < inst->mot->joint_get_vel_limit(inst->mot->ctx, jno)) {
        inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno, fabs(vel));
    } else {
        inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno,
            inst->mot->joint_get_vel_limit(inst->mot->ctx, jno));
    }
    inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 1);
}

static bool home_do_moving_checks(homemod_inst_t *inst)
{
    int jno = inst->jno;
    if (inst->mot->joint_get_on_pos_limit(inst->mot->ctx, jno) ||
        inst->mot->joint_get_on_neg_limit(inst->mot->ctx, jno)) {
        if (!(inst->H.home_flags & HOME_IGNORE_LIMITS)) {
            gomc_log_errorf(inst->log, inst->name,
                "j%d hit limit in home state %d", jno, inst->H.home_state);
            inst->H.home_state = HOME_ABORT;
            return 1;
        }
    }
    if (!inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
        inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
        gomc_log_errorf(inst->log, inst->name,
            "j%d end of move in home state %d", jno, inst->H.home_state);
        inst->H.home_state = HOME_ABORT;
        return 1;
    }
    return 0;
}

#define ABORT_CHECK() do { \
    if (home_do_moving_checks(inst)) { \
        inst->H.home_state = HOME_ABORT; \
        immediate_state = 1; \
    } \
} while(0);

static int do_homing_state_machine(homemod_inst_t *inst)
{
    int jno = inst->jno;
    double offset, tmp;
    int home_sw_active, homing_flag;
    bool immediate_state = 0;

    homing_flag = 0;
    home_sw_active = inst->H.home_sw;
    if (inst->H.home_state != HOME_IDLE) {
        homing_flag = 1;
    }

    do {
        immediate_state = 0;
        switch (inst->H.home_state) {
        case HOME_IDLE:
            break;

        case HOME_START:
            if (inst->H.home_flags & HOME_IS_SHARED && home_sw_active) {
                gomc_log_errorf(inst->log, inst->name,
                    "Cannot home while shared home switch is closed j=%d", jno);
                inst->H.home_state = HOME_IDLE;
                break;
            }
            if ((inst->H.home_flags & HOME_NO_REHOME) && inst->H.homed) {
                inst->H.home_state = HOME_IDLE;
                break;
            } else {
                inst->H.homing = 1;
                inst->H.homed = 0;
            }
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
            inst->H.pause_timer = 0;
            if (inst->H.home_flags & HOME_ABSOLUTE_ENCODER) {
                inst->H.home_flags &= ~HOME_IS_SHARED;
                inst->H.home_state = HOME_SET_SWITCH_POSITION;
                immediate_state = 1;
                break;
            }
            if (inst->H.home_flags & HOME_UNLOCK_FIRST) {
                inst->H.home_state = HOME_UNLOCK;
            } else {
                inst->H.home_state = HOME_UNLOCK_WAIT;
                immediate_state = 1;
            }
            break;

        case HOME_UNLOCK:
            inst->mot->set_rotary_unlock(inst->mot->ctx, jno, 1);
            inst->H.home_state = HOME_UNLOCK_WAIT;
            break;

        case HOME_UNLOCK_WAIT:
            if ((inst->H.home_flags & HOME_UNLOCK_FIRST) &&
                !inst->mot->get_rotary_unlock(inst->mot->ctx, jno)) break;

            if (inst->H.home_search_vel == 0.0) {
                if (inst->H.home_latch_vel == 0.0) {
                    inst->H.home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                } else if (inst->H.home_flags & HOME_USE_INDEX) {
                    inst->H.home_state = HOME_INDEX_ONLY_START;
                    immediate_state = 1;
                } else {
                    gomc_log_errorf(inst->log, inst->name,
                        "invalid homing config: non-zero LATCH_VEL needs either SEARCH_VEL or USE_INDEX");
                    inst->H.home_state = HOME_IDLE;
                }
            } else {
                if (inst->H.home_latch_vel != 0.0) {
                    inst->H.home_state = HOME_INITIAL_SEARCH_START;
                    immediate_state = 1;
                } else {
                    gomc_log_errorf(inst->log, inst->name,
                        "invalid homing config: non-zero SEARCH_VEL needs LATCH_VEL");
                    inst->H.home_state = HOME_IDLE;
                }
            }
            break;

        case HOME_INITIAL_BACKOFF_START:
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->H.pause_timer = 0;
                break;
            }
            if (inst->H.pause_timer < (HOME_DELAY * inst->servo_freq)) {
                inst->H.pause_timer++;
                break;
            }
            inst->H.pause_timer = 0;
            home_start_move(inst, -inst->H.home_search_vel);
            inst->H.home_state = HOME_INITIAL_BACKOFF_WAIT;
            break;

        case HOME_INITIAL_BACKOFF_WAIT:
            if (!home_sw_active) {
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
                inst->H.home_state = HOME_INITIAL_SEARCH_START;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_INITIAL_SEARCH_START:
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->H.pause_timer = 0;
                break;
            }
            if (inst->H.pause_timer < (HOME_DELAY * inst->servo_freq)) {
                inst->H.pause_timer++;
                break;
            }
            inst->H.pause_timer = 0;
            if (home_sw_active) {
                inst->H.home_state = HOME_INITIAL_BACKOFF_START;
                immediate_state = 1;
                break;
            }
            home_start_move(inst, inst->H.home_search_vel);
            inst->H.home_state = HOME_INITIAL_SEARCH_WAIT;
            break;

        case HOME_INITIAL_SEARCH_WAIT:
            if (home_sw_active) {
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
                inst->H.home_state = HOME_SET_COARSE_POSITION;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_SET_COARSE_POSITION:
            offset = inst->H.home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, jno);
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno,
                inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_pos_fb(inst->mot->ctx, jno,
                inst->mot->joint_get_pos_fb(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno,
                inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_motor_offset(inst->mot->ctx, jno,
                inst->mot->joint_get_motor_offset(inst->mot->ctx, jno) - offset);
            tmp = inst->H.home_search_vel * inst->H.home_latch_vel;
            if (tmp > 0.0) {
                inst->H.home_state = HOME_FINAL_BACKOFF_START;
            } else {
                inst->H.home_state = HOME_FALL_SEARCH_START;
            }
            immediate_state = 1;
            break;

        case HOME_FINAL_BACKOFF_START:
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->H.pause_timer = 0;
                break;
            }
            if (inst->H.pause_timer < (HOME_DELAY * inst->servo_freq)) {
                inst->H.pause_timer++;
                break;
            }
            inst->H.pause_timer = 0;
            if (!home_sw_active) {
                gomc_log_errorf(inst->log, inst->name,
                    "Home switch inactive before start of backoff move j=%d", jno);
                inst->H.home_state = HOME_IDLE;
                break;
            }
            home_start_move(inst, -inst->H.home_search_vel);
            inst->H.home_state = HOME_FINAL_BACKOFF_WAIT;
            break;

        case HOME_FINAL_BACKOFF_WAIT:
            if (!home_sw_active) {
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
                inst->H.home_state = HOME_RISE_SEARCH_START;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_RISE_SEARCH_START:
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->H.pause_timer = 0;
                break;
            }
            if (inst->H.pause_timer < (HOME_DELAY * inst->servo_freq)) {
                inst->H.pause_timer++;
                break;
            }
            inst->H.pause_timer = 0;
            if (home_sw_active) {
                gomc_log_errorf(inst->log, inst->name,
                    "Home switch active before start of latch move j=%d", jno);
                inst->H.home_state = HOME_IDLE;
                break;
            }
            home_start_move(inst, inst->H.home_latch_vel);
            inst->H.home_state = HOME_RISE_SEARCH_WAIT;
            break;

        case HOME_RISE_SEARCH_WAIT:
            if (home_sw_active) {
                if (inst->H.home_flags & HOME_USE_INDEX) {
                    inst->H.home_state = HOME_INDEX_SEARCH_START;
                    immediate_state = 1;
                    break;
                } else {
                    inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
                    inst->H.home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                    break;
                }
            }
            ABORT_CHECK();
            break;

        case HOME_FALL_SEARCH_START:
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->H.pause_timer = 0;
                break;
            }
            if (inst->H.pause_timer < (HOME_DELAY * inst->servo_freq)) {
                inst->H.pause_timer++;
                break;
            }
            inst->H.pause_timer = 0;
            if (!home_sw_active) {
                gomc_log_errorf(inst->log, inst->name,
                    "Home switch inactive before start of latch move j=%d", jno);
                inst->H.home_state = HOME_IDLE;
                break;
            }
            home_start_move(inst, inst->H.home_latch_vel);
            inst->H.home_state = HOME_FALL_SEARCH_WAIT;
            break;

        case HOME_FALL_SEARCH_WAIT:
            if (!home_sw_active) {
                if (inst->H.home_flags & HOME_USE_INDEX) {
                    inst->H.home_state = HOME_INDEX_SEARCH_START;
                    immediate_state = 1;
                    break;
                } else {
                    inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
                    inst->H.home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                    break;
                }
            }
            ABORT_CHECK();
            break;

        case HOME_SET_SWITCH_POSITION:
            if (inst->H.home_flags & HOME_ABSOLUTE_ENCODER) {
                offset = inst->H.home_offset;
            } else {
                offset = inst->H.home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, jno);
            }
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno,
                inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_pos_fb(inst->mot->ctx, jno,
                inst->mot->joint_get_pos_fb(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno,
                inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_motor_offset(inst->mot->ctx, jno,
                inst->mot->joint_get_motor_offset(inst->mot->ctx, jno) - offset);
            if (inst->H.home_flags & HOME_ABSOLUTE_ENCODER) {
                if (inst->H.home_flags & HOME_NO_FINAL_MOVE) {
                    inst->H.home_state = HOME_FINISHED;
                    immediate_state = 1;
                    inst->H.homed = 1;
                    break;
                }
            }
            inst->H.home_state = HOME_FINAL_MOVE_START;
            immediate_state = 1;
            break;

        case HOME_INDEX_ONLY_START:
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->H.pause_timer = 0;
                break;
            }
            if (inst->H.pause_timer < (HOME_DELAY * inst->servo_freq)) {
                inst->H.pause_timer++;
                break;
            }
            inst->H.pause_timer = 0;
            offset = inst->H.home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, jno);
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno,
                inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_pos_fb(inst->mot->ctx, jno,
                inst->mot->joint_get_pos_fb(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno,
                inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, jno) + offset);
            inst->mot->joint_set_motor_offset(inst->mot->ctx, jno,
                inst->mot->joint_get_motor_offset(inst->mot->ctx, jno) - offset);
            inst->H.index_enable = 1;
            home_start_move(inst, inst->H.home_latch_vel);
            inst->H.home_state = HOME_INDEX_SEARCH_WAIT;
            break;

        case HOME_INDEX_SEARCH_START:
            inst->H.index_enable = 1;
            inst->H.home_state = HOME_INDEX_SEARCH_WAIT;
            immediate_state = 1;
            ABORT_CHECK();
            break;

        case HOME_INDEX_SEARCH_WAIT:
            if (inst->H.index_enable == 0) {
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
                inst->H.home_state = HOME_SET_INDEX_POSITION;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK();
            break;

        case HOME_SET_INDEX_POSITION:
            inst->mot->joint_set_motor_offset(inst->mot->ctx, jno, -inst->H.home_offset);
            { double pf = inst->mot->joint_get_motor_pos_fb(inst->mot->ctx, jno)
                        - (inst->mot->joint_get_backlash_filt(inst->mot->ctx, jno)
                         + inst->mot->joint_get_motor_offset(inst->mot->ctx, jno));
              inst->mot->joint_set_pos_fb(inst->mot->ctx, jno, pf);
              inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno, pf);
              inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, pf);
            }
            if (inst->H.home_flags & HOME_INDEX_NO_ENCODER_RESET) {
               offset = inst->H.home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, jno);
               inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno,
                   inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) + offset);
               inst->mot->joint_set_pos_fb(inst->mot->ctx, jno,
                   inst->mot->joint_get_pos_fb(inst->mot->ctx, jno) + offset);
               inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno,
                   inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, jno) + offset);
               inst->mot->joint_set_motor_offset(inst->mot->ctx, jno,
                   inst->mot->joint_get_motor_offset(inst->mot->ctx, jno) - offset);
            }
            inst->H.home_state = HOME_FINAL_MOVE_START;
            immediate_state = 1;
            break;

        case HOME_FINAL_MOVE_START:
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->H.pause_timer = 0;
                break;
            }
            if (inst->H.pause_timer < (HOME_DELAY * inst->servo_freq)) {
                inst->H.pause_timer++;
                break;
            }
            inst->H.pause_timer = 0;

            /* For synchronized homing (negative home_sequence), pause here
             * until motmod calls do_final_move() to release all joints
             * simultaneously. */
            if (inst->H.home_sequence < 0 && !inst->H.sync_final_move_released) {
                break;
            }
            inst->H.sync_final_move_released = 0;

            /* plan a final move to home position */
            inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno, inst->H.home);
            if (inst->H.home_final_vel > 0) {
                inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno,
                    fabs(inst->H.home_final_vel));
                { double vl = inst->mot->joint_get_vel_limit(inst->mot->ctx, jno);
                    if (inst->mot->joint_get_free_tp_max_vel(inst->mot->ctx, jno) > vl)
                        inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno, vl);
                }
            } else {
                inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno,
                    inst->mot->joint_get_vel_limit(inst->mot->ctx, jno));
            }
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 1);
            inst->H.home_state = HOME_FINAL_MOVE_WAIT;
            break;

        case HOME_FINAL_MOVE_WAIT:
            if (!inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
                inst->H.home_state = HOME_LOCK;
                immediate_state = 1;
                break;
            }
            if (inst->mot->joint_get_on_pos_limit(inst->mot->ctx, jno) ||
                inst->mot->joint_get_on_neg_limit(inst->mot->ctx, jno)) {
                if (!(inst->H.home_flags & HOME_IGNORE_LIMITS)) {
                    gomc_log_errorf(inst->log, inst->name,
                        "hit limit in home state j=%d", jno);
                    inst->H.home_state = HOME_ABORT;
                    immediate_state = 1;
                    break;
                }
            }
            break;

        case HOME_LOCK:
            if (inst->H.home_flags & HOME_UNLOCK_FIRST) {
                inst->mot->set_rotary_unlock(inst->mot->ctx, jno, 0);
            } else {
                immediate_state = 1;
            }
            inst->H.home_state = HOME_LOCK_WAIT;
            break;

        case HOME_LOCK_WAIT:
            if ((inst->H.home_flags & HOME_UNLOCK_FIRST) &&
                inst->mot->get_rotary_unlock(inst->mot->ctx, jno)) break;
            inst->H.home_state = HOME_FINISHED;
            immediate_state = 1;
            break;

        case HOME_FINISHED:
            inst->H.homing = 0;
            inst->H.homed = 1;
            inst->H.home_state = HOME_IDLE;
            if (!(inst->H.home_flags & HOME_ABSOLUTE_ENCODER)) {
                inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, inst->H.home);
            }
            immediate_state = 1;
            break;

        case HOME_ABORT:
            inst->H.homing = 0;
            inst->H.homed = 0;
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
            inst->H.home_state = HOME_IDLE;
            inst->H.index_enable = 0;
            inst->H.sync_final_move_released = 0;
            immediate_state = 1;
            break;

        default:
            gomc_log_errorf(inst->log, inst->name,
                "unknown state '%d' during homing j=%d",
                inst->H.home_state, jno);
            inst->H.home_state = HOME_ABORT;
            immediate_state = 1;
            break;
        }
    } while (immediate_state);

    return homing_flag;
}


/***********************************************************************
*              GMI API FUNCTIONS (new per-joint signatures)            *
************************************************************************/

static int32_t gmi_home_init(void *ctx, int32_t comp_id, double servo_period)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    int retval = 0;

    inst->comp_id = comp_id;

    if (servo_period < 1e-9) {
        gomc_log_errorf(inst->log, inst->name, "bad servo_period:%g", servo_period);
        return -1;
    }
    inst->servo_freq = 1.0 / servo_period;

    /* Create HAL pins for this joint.
       Default prefix ("homemod.N"): bare "joint.N.*"
       Non-default ("home1.N"): "home1.joint.N.*" */
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN, &inst->pins.home_sw, comp_id,
                              "%sjoint.%d.home-sw-in", inst->pin_prefix, inst->jno);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.homing, comp_id,
                              "%sjoint.%d.homing", inst->pin_prefix, inst->jno);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.homed, comp_id,
                              "%sjoint.%d.homed", inst->pin_prefix, inst->jno);
    retval += gomc_hal_pin_s32_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.home_state, comp_id,
                              "%sjoint.%d.home-state", inst->pin_prefix, inst->jno);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IO, &inst->pins.index_enable, comp_id,
                              "%sjoint.%d.index-enable", inst->pin_prefix, inst->jno);
    if (retval != 0) {
        gomc_log_errorf(inst->log, inst->name, "failed to create HAL pins");
        return -1;
    }

    /* Initialize state */
    inst->H.home_state               = HOME_IDLE;
    inst->H.home_search_vel          = 0;
    inst->H.home_latch_vel           = 0;
    inst->H.home_final_vel           = 0;
    inst->H.home_offset              = 0;
    inst->H.home                     = 0;
    inst->H.home_flags               = 0;
    inst->H.home_sequence            = 1000;
    inst->H.volatile_home            = 0;
    inst->H.homing                   = 0;
    inst->H.homed                    = 0;
    inst->H.sync_final_move_released = 0;

    return 0;
}

static int32_t gmi_home_set_params(void *ctx, double offset, double home,
    double home_final_vel, double home_search_vel,
    double home_latch_vel, int32_t home_flags,
    int32_t home_sequence, int32_t volatile_home)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    inst->H.home_offset     = offset;
    inst->H.home            = home;
    inst->H.home_final_vel  = home_final_vel;
    inst->H.home_search_vel = home_search_vel;
    inst->H.home_latch_vel  = home_latch_vel;
    inst->H.home_flags      = home_flags;
    inst->H.home_sequence   = home_sequence;
    inst->H.volatile_home   = (bool)volatile_home;
    return 0;
}

static int32_t gmi_home_update_params(void *ctx, double home_offset,
    double home_home, int32_t home_sequence)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    inst->H.home_offset   = home_offset;
    inst->H.home          = home_home;
    inst->H.home_sequence = home_sequence;
    return 0;
}

static int32_t gmi_home_read_in_pins(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    inst->H.home_sw      = *(inst->pins.home_sw);
    inst->H.index_enable = *(inst->pins.index_enable);
    return 0;
}

static int32_t gmi_home_do_homing(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    if (!inst->mot->joint_get_active_flag(inst->mot->ctx, inst->jno))
        return 0;
    return (int32_t)do_homing_state_machine(inst);
}

static int32_t gmi_home_write_out_pins(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    *(inst->pins.homing)       = inst->H.homing;
    *(inst->pins.homed)        = inst->H.homed;
    *(inst->pins.home_state)   = inst->H.home_state;
    *(inst->pins.index_enable) = inst->H.index_enable;
    return 0;
}

static int32_t gmi_home_do_home(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    inst->H.home_state = HOME_START;
    inst->H.sync_final_move_released = 0;
    return 0;
}

static int32_t gmi_home_do_cancel(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    if (inst->H.homing || inst->H.home_state != HOME_IDLE) {
        inst->H.home_state = HOME_ABORT;
    }
    return 0;
}

static int32_t gmi_home_set_unhomed(void *ctx, home_motion_state_t motstate)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    (void)motstate;

    if (!inst->mot->joint_get_active_flag(inst->mot->ctx, inst->jno)) {
        gomc_log_errorf(inst->log, inst->name,
            "Cannot unhome inactive joint %d", inst->jno);
        return -1;
    }
    if (inst->H.homing) {
        gomc_log_errorf(inst->log, inst->name,
            "Cannot unhome while homing, joint %d", inst->jno);
        return -1;
    }
    if (!inst->mot->joint_get_inpos_flag(inst->mot->ctx, inst->jno)) {
        gomc_log_errorf(inst->log, inst->name,
            "Cannot unhome while moving, joint %d", inst->jno);
        return -1;
    }
    inst->H.homed = 0;
    return 0;
}

static int32_t gmi_home_get_homing(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    return (int32_t)inst->H.homing;
}

static int32_t gmi_home_get_homed(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    return (int32_t)inst->H.homed;
}

static int32_t gmi_home_get_index_enable(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    return (int32_t)inst->H.index_enable;
}

static int32_t gmi_home_get_needs_unlock_first(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    return (inst->H.home_flags & HOME_UNLOCK_FIRST) ? 1 : 0;
}

static int32_t gmi_home_get_is_idle(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    return inst->H.home_state == HOME_IDLE ? 1 : 0;
}

static int32_t gmi_home_get_at_index_search_wait(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    return inst->H.home_state == HOME_INDEX_SEARCH_WAIT ? 1 : 0;
}

static int32_t gmi_home_get_at_final_move_wait(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    /* Returns 1 when the state machine is paused at HOME_FINAL_MOVE_START
     * waiting for the sync signal from motmod. */
    return (inst->H.home_state == HOME_FINAL_MOVE_START &&
            inst->H.home_sequence < 0 &&
            !inst->H.sync_final_move_released) ? 1 : 0;
}

static int32_t gmi_home_do_final_move(void *ctx)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    /* Release the sync pause so the final move can start. */
    inst->H.sync_final_move_released = 1;
    return 0;
}


static void home_cmod_destroy(cmod_t *self) {
    homemod_inst_t *inst = (homemod_inst_t *)((char *)self - offsetof(homemod_inst_t, cmod));
    free(inst);
}

static int home_cmod_init(cmod_t *self)
{
    homemod_inst_t *inst = (homemod_inst_t *)((char *)self - offsetof(homemod_inst_t, cmod));
    const mot_callbacks_t *mot = mot_api_get(inst->api, inst->mot_instance);
    if (!mot) return -1;
    inst->mot = mot;
    return 0;
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    homemod_inst_t *inst = calloc(1, sizeof(homemod_inst_t));
    if (!inst) return -1;

    inst->api = env->api;
    inst->hal = env->hal;
    inst->log = env->log;
    snprintf(inst->name, sizeof(inst->name), "%s", name);

    /* Parse joint number from instance name (e.g. "homemod.3" -> 3) */
    const char *dot = strrchr(name, '.');
    if (dot && dot[1] >= '0' && dot[1] <= '9') {
        inst->jno = atoi(dot + 1);
    } else {
        gomc_log_errorf(env->log, name,
            "Cannot parse joint number from instance name '%s' (expected 'prefix.N')", name);
        free(inst);
        return -1;
    }

    /* Pin prefix: empty for default "homemod" prefix, "prefix." for non-default.
       e.g. "homemod.2" -> bare "joint.2.*", "home1.2" -> "home1.joint.2.*" */
    {
        size_t plen = (size_t)(dot - name);
        if (plen == 7 && strncmp(name, "homemod", 7) == 0) {
            inst->pin_prefix[0] = '\0';
        } else {
            snprintf(inst->pin_prefix, sizeof(inst->pin_prefix), "%.*s.", (int)plen, name);
        }
    }

    /* Parse mot_instance parameter (default: "motmod") */
    snprintf(inst->mot_instance, sizeof(inst->mot_instance), "motmod");
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "mot_instance=", 13) == 0)
            snprintf(inst->mot_instance, sizeof(inst->mot_instance), "%s", argv[i] + 13);
    }

    /* Set up per-instance callbacks */
    inst->callbacks = (home_callbacks_t)GMI_HOME_CALLBACKS;
    inst->callbacks.ctx = inst;

    int rc = home_api_register(env->api, name, &inst->callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name, "failed to register home API: %d", rc);
        free(inst);
        return rc;
    }

    inst->cmod.Init    = home_cmod_init;
    inst->cmod.Start   = NULL;
    inst->cmod.Destroy = home_cmod_destroy;
    *out = &inst->cmod;
    return 0;
}
