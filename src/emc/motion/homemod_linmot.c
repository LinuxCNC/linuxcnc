/********************************************************************
* homemod_linmot.c — Custom homing module for LinMot linear drives
*
* This module implements CiA 402 drive-internal homing for LinMot axes.
* During normal operation the drive runs in Cyclic Synchronous Position
* mode (opMode=8).  When LinuxCNC requests homing, the module:
*   1. Switches the drive to Homing Mode (opMode=6)
*   2. Sets the Home command bit (control word bit 4)
*   3. Waits for HomingAttained from the drive status word
*   4. Syncs LinuxCNC internal position to the drive's actual position
*   5. Switches back to CSP mode (opMode=8)
*
* Following error is suppressed during drive homing by returning true
* from get_at_index_search_wait() which causes motmod to force
* pos_fb = pos_cmd.
*
* For joints that don't use drive homing, this module delegates to
* the standard homing state machine (HOME_ABSOLUTE_ENCODER path).
*
* License: GPL Version 2
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

/* We do NOT include homing.h — it declares old global-style functions
 * that conflict with our static implementations.  We define the needed
 * constants locally. */

/* HOME_* flags from homing.h */
#define HOME_IGNORE_LIMITS            1
#define HOME_USE_INDEX                2
#define HOME_IS_SHARED                4
#define HOME_UNLOCK_FIRST             8
#define HOME_ABSOLUTE_ENCODER        16
#define HOME_NO_REHOME               32
#define HOME_NO_FINAL_MOVE           64
#define HOME_INDEX_NO_ENCODER_RESET 128

/* Internal homing states (subset needed for standard homing path) */
enum {
    HOME_IDLE = 0,
    HOME_START,
    HOME_SET_SWITCH_POSITION,
    HOME_FINAL_MOVE_START,
    HOME_FINAL_MOVE_WAIT,
    HOME_FINISHED,
    HOME_ABORT,
    HOME_INDEX_SEARCH_WAIT,  /* used for ferror suppression trick */
};

#define ABS(x) (((x) < 0) ? -(x) : (x))
#define MAX_JOINTS EMCMOT_MAX_JOINTS

/* CiA 402 operation modes */
#define CIA402_OP_CSP     8   /* Cyclic Synchronous Position */
#define CIA402_OP_HOMING  6   /* Homing mode */

/* Drive homing states (per-joint) */
typedef enum {
    DRV_HOME_IDLE = 0,        /* Not homing, drive in CSP mode */
    DRV_HOME_SWITCH_MODE,     /* Switching to homing mode */
    DRV_HOME_WAIT_MODE,       /* Waiting for opModeDisp == 6 */
    DRV_HOME_START,           /* Set Home bit */
    DRV_HOME_WAIT_ATTAINED,   /* Waiting for HomingAttained */
    DRV_HOME_SYNC_POS,        /* Sync LinuxCNC position to drive */
    DRV_HOME_SWITCH_BACK,     /* Switch back to CSP */
    DRV_HOME_WAIT_CSP,        /* Wait for opModeDisp == 8 */
    DRV_HOME_FINAL_MOVE,      /* Final move to HOME position */
    DRV_HOME_FINAL_WAIT,      /* Waiting for final move to complete */
    DRV_HOME_DONE,            /* Homing complete */
    DRV_HOME_ERROR,           /* Homing failed */
} drv_home_state_t;

/* Per-joint drive homing HAL pins */
typedef struct {
    gomc_hal_u32_t  *opmode_cmd;       /* OUT: opMode command to drive */
    gomc_hal_u32_t  *opmode_fb;        /* IN:  opModeDisp from drive */
    gomc_hal_bit_t  *home_cmd;         /* OUT: control.Home bit */
    gomc_hal_bit_t  *homing_attained;  /* IN:  status.HomingAttained */
    gomc_hal_bit_t  *homing_error;     /* IN:  status.HomingError */
    gomc_hal_bit_t  *drive_homing;     /* IN:  1 = this joint uses drive homing */
} drv_home_pins_t;

/* Standard homing HAL pins (same as base homemod) */
typedef struct {
    gomc_hal_bit_t *home_sw;
    gomc_hal_bit_t *homing;
    gomc_hal_bit_t *homed;
    gomc_hal_s32_t *home_state;
    gomc_hal_bit_t *index_enable;
} std_home_pins_t;

/* Per-joint local data */
typedef struct {
    /* Standard homing fields (for non-drive-homing joints) */
    int          home_state;         /* HOME_IDLE..HOME_ABORT */
    bool         homing;
    bool         homed;
    bool         home_sw;
    bool         index_enable;
    bool         joint_in_sequence;
    int          pause_timer;
    double       home_offset;
    double       home;
    double       home_final_vel;
    double       home_search_vel;
    double       home_latch_vel;
    int          home_flags;
    int          home_sequence;
    bool         volatile_home;
    bool         home_is_synchronized;

    /* Drive homing state */
    drv_home_state_t  drv_state;
    bool              use_drive_homing;  /* cached from HAL pin at home start */
    int               mode_wait_count;   /* timeout counter */
} joint_data_t;

/* Instance state */
typedef struct {
    const mot_callbacks_t *mot;
    const gomc_api_t *api;
    const gomc_hal_t *hal;
    const gomc_log_t *log;
    char name[GOMC_HAL_NAME_LEN + 1];
    char pin_prefix[GOMC_HAL_NAME_LEN + 1];
    char mot_instance[GOMC_HAL_NAME_LEN + 1];
    int comp_id;
    double servo_freq;
    int all_joints;
    int extra_joints;

    /* Homing sequence management */
    int  sequence_state;
    int  current_sequence;
    bool homing_active;

    joint_data_t H[MAX_JOINTS];

    /* HAL pin storage */
    std_home_pins_t  std_pins[MAX_JOINTS];
    drv_home_pins_t  drv_pins[MAX_JOINTS];

    home_callbacks_t callbacks;
    cmod_t cmod;
} linmot_inst_t;

/* Sequence states (simplified from base homemod) */
enum {
    SEQ_IDLE = 0,
    SEQ_START,
    SEQ_DO_ONE_JOINT,
    SEQ_DO_ONE_SEQUENCE,
    SEQ_START_JOINTS,
    SEQ_WAIT_JOINTS,
};

/* Mode switch timeout (servo cycles, ~5 seconds at 1kHz) */
#define MODE_SWITCH_TIMEOUT 5000

/***********************************************************************
*                     FORWARD DECLARATIONS                            *
***********************************************************************/

static bool get_allhomed(linmot_inst_t *inst);
static bool get_homing_is_active(linmot_inst_t *inst);

/***********************************************************************
*                     HELPER FUNCTIONS                                 *
***********************************************************************/

static void update_home_is_synchronized(linmot_inst_t *inst)
{
    int jno, jj;
    for (jno = 0; jno < inst->all_joints; jno++) {
        inst->H[jno].home_is_synchronized = 0;
        if (inst->H[jno].home_sequence < 0) {
            for (jj = 0; jj < inst->all_joints; jj++) {
                if (ABS(inst->H[jj].home_sequence) == ABS(inst->H[jno].home_sequence)) {
                    inst->H[jj].home_is_synchronized = 1;
                }
            }
        }
    }
}

/***********************************************************************
*               DRIVE HOMING STATE MACHINE                            *
***********************************************************************/

/* Returns: 1 = still homing, 0 = done/idle */
static int drive_home_joint(linmot_inst_t *inst, int jno)
{
    drv_home_pins_t *dp = &inst->drv_pins[jno];
    joint_data_t *jd = &inst->H[jno];

    switch (jd->drv_state) {
    case DRV_HOME_IDLE:
        return 0;

    case DRV_HOME_SWITCH_MODE:
        /* Command drive to homing mode */
        *(dp->opmode_cmd) = CIA402_OP_HOMING;
        *(dp->home_cmd) = 0;
        jd->mode_wait_count = 0;
        jd->drv_state = DRV_HOME_WAIT_MODE;
        return 1;

    case DRV_HOME_WAIT_MODE:
        /* Wait for drive to acknowledge homing mode */
        if (*(dp->opmode_fb) == CIA402_OP_HOMING) {
            jd->mode_wait_count = 0;
            jd->drv_state = DRV_HOME_START;
            return 1;
        }
        jd->mode_wait_count++;
        if (jd->mode_wait_count > MODE_SWITCH_TIMEOUT) {
            gomc_log_errorf(inst->log, inst->name,
                "j%d: timeout waiting for homing mode", jno);
            jd->drv_state = DRV_HOME_ERROR;
        }
        return 1;

    case DRV_HOME_START:
        /* Wait until HomingAttained is clear before asserting home_cmd.
         * This handles the case where the drive still has HomingAttained
         * set from a previous homing cycle. We need a clean 0→1 edge. */
        if (*(dp->homing_attained)) {
            /* Still set from previous homing, keep home_cmd low and wait */
            *(dp->home_cmd) = 0;
            jd->mode_wait_count++;
            if (jd->mode_wait_count > MODE_SWITCH_TIMEOUT) {
                gomc_log_errorf(inst->log, inst->name,
                    "j%d: timeout waiting for HomingAttained to clear", jno);
                jd->drv_state = DRV_HOME_ERROR;
            }
            return 1;
        }
        /* HomingAttained is low, now assert home command */
        *(dp->home_cmd) = 1;
        jd->mode_wait_count = 0;
        jd->drv_state = DRV_HOME_WAIT_ATTAINED;
        return 1;

    case DRV_HOME_WAIT_ATTAINED:
        /* Wait for HomingAttained or HomingError */
        if (*(dp->homing_error)) {
            gomc_log_errorf(inst->log, inst->name,
                "j%d: drive reported homing error", jno);
            *(dp->home_cmd) = 0;
            jd->drv_state = DRV_HOME_ERROR;
            return 1;
        }
        if (*(dp->homing_attained)) {
            *(dp->home_cmd) = 0;
            /* Switch back to CSP first, sync position after mode is stable */
            *(dp->opmode_cmd) = CIA402_OP_CSP;
            jd->mode_wait_count = 0;
            jd->drv_state = DRV_HOME_WAIT_CSP;
            return 1;
        }
        /* No timeout here - drive homing may take a long time */
        return 1;

    case DRV_HOME_SYNC_POS: {
        /* Drive is back in CSP mode and position feedback is stable.
         * Sync LinuxCNC position to actual drive position.
         *
         * IMPORTANT: pos_fb is being faked during ferror suppression
         * (motmod forces pos_fb = pos_cmd). We must use motor_pos_fb
         * which always reflects the real drive feedback.
         *
         * After homing, the drive reports its position relative to its
         * internal reference (typically 0 at home). We want LinuxCNC to
         * consider this position as HOME_OFFSET.
         *
         * pos_fb = motor_pos_fb - backlash_filt - motor_offset
         * We want pos_fb = home_offset, so:
         * motor_offset = motor_pos_fb - backlash_filt - home_offset
         *
         * Also set pos_cmd = pos_fb = home_offset so that motor_pos_cmd
         * (= pos_cmd + backlash + motor_offset) equals motor_pos_fb,
         * keeping the drive at its current position. */
        double motor_fb = inst->mot->joint_get_motor_pos_fb(inst->mot->ctx, jno);
        double backlash = inst->mot->joint_get_backlash_filt(inst->mot->ctx, jno);
        double new_motor_offset = motor_fb - backlash - jd->home_offset;

        inst->mot->joint_set_motor_offset(inst->mot->ctx, jno, new_motor_offset);
        inst->mot->joint_set_pos_fb(inst->mot->ctx, jno, jd->home_offset);
        inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno, jd->home_offset);
        inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, jd->home_offset);

        jd->drv_state = DRV_HOME_FINAL_MOVE;
        return 1;
    }

    case DRV_HOME_SWITCH_BACK:
        /* Switch back to CSP mode (unused, kept for enum completeness) */
        *(dp->opmode_cmd) = CIA402_OP_CSP;
        jd->mode_wait_count = 0;
        jd->drv_state = DRV_HOME_WAIT_CSP;
        return 1;

    case DRV_HOME_WAIT_CSP:
        /* Wait for drive to acknowledge CSP mode, then sync immediately.
         * CRITICAL: sync must happen in the SAME cycle as CSP detection,
         * otherwise the drive will receive the old stale posCmd for one
         * cycle and jump to it. */
        if (*(dp->opmode_fb) == CIA402_OP_CSP) {
            /* Inline position sync — drive feedback is now stable */
            double motor_fb = inst->mot->joint_get_motor_pos_fb(inst->mot->ctx, jno);
            double backlash = inst->mot->joint_get_backlash_filt(inst->mot->ctx, jno);
            double new_motor_offset = motor_fb - backlash - jd->home_offset;

            inst->mot->joint_set_motor_offset(inst->mot->ctx, jno, new_motor_offset);
            inst->mot->joint_set_pos_fb(inst->mot->ctx, jno, jd->home_offset);
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno, jd->home_offset);
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, jd->home_offset);

            jd->drv_state = DRV_HOME_FINAL_MOVE;
            return 1;
        }
        jd->mode_wait_count++;
        if (jd->mode_wait_count > MODE_SWITCH_TIMEOUT) {
            gomc_log_errorf(inst->log, inst->name,
                "j%d: timeout waiting for CSP mode after homing", jno);
            jd->drv_state = DRV_HOME_ERROR;
        }
        return 1;

    case DRV_HOME_FINAL_MOVE:
        /* Final move from HOME_OFFSET to HOME position.
         * If HOME == HOME_OFFSET (or HOME_NO_FINAL_MOVE), skip. */
        if ((jd->home_flags & HOME_NO_FINAL_MOVE) ||
            fabs(jd->home - jd->home_offset) < 1e-9) {
            jd->drv_state = DRV_HOME_DONE;
            return 1;
        }
        /* Plan move to HOME position */
        inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno, jd->home);
        if (jd->home_final_vel > 0) {
            double vl = inst->mot->joint_get_vel_limit(inst->mot->ctx, jno);
            double vel = fabs(jd->home_final_vel);
            inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno, vel < vl ? vel : vl);
        } else {
            inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno,
                inst->mot->joint_get_vel_limit(inst->mot->ctx, jno));
        }
        inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 1);
        jd->drv_state = DRV_HOME_FINAL_WAIT;
        return 1;

    case DRV_HOME_FINAL_WAIT:
        /* Wait for final move to complete */
        if (!inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
            jd->drv_state = DRV_HOME_DONE;
            return 1;
        }
        return 1;

    case DRV_HOME_DONE:
        /* Mark homing complete */
        jd->homing = 0;
        jd->homed = 1;
        jd->home_state = HOME_IDLE;
        jd->drv_state = DRV_HOME_IDLE;
        jd->joint_in_sequence = 0;
        inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, jd->home);
        return 0;

    case DRV_HOME_ERROR:
        /* Abort homing */
        *(dp->home_cmd) = 0;
        *(dp->opmode_cmd) = CIA402_OP_CSP;
        jd->homing = 0;
        jd->homed = 0;
        jd->home_state = HOME_IDLE;
        jd->drv_state = DRV_HOME_IDLE;
        jd->joint_in_sequence = 0;
        return 0;
    }
    return 0;
}

/***********************************************************************
*            STANDARD HOMING (for non-drive joints)                   *
***********************************************************************/

/* Minimal absolute-encoder homing for non-drive joints.
 * search_vel=0, latch_vel=0 → home at current position. */
static int std_home_joint(linmot_inst_t *inst, int jno)
{
    joint_data_t *jd = &inst->H[jno];
    double offset;

    switch (jd->home_state) {
    case HOME_IDLE:
        return 0;

    case HOME_START:
        if (jd->home_flags & HOME_NO_REHOME) {
            if (jd->homed) {
                jd->home_state = HOME_IDLE;
                return 0;
            }
        }
        jd->homing = 1;
        jd->homed = 0;
        inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
        jd->pause_timer = 0;

        if (jd->home_flags & HOME_ABSOLUTE_ENCODER) {
            /* Absolute encoder: set position to home_offset */
            jd->home_state = HOME_SET_SWITCH_POSITION;
            break;
        }
        if (jd->home_search_vel == 0.0 && jd->home_latch_vel == 0.0) {
            /* Home at current position */
            jd->home_state = HOME_SET_SWITCH_POSITION;
            break;
        }
        /* Anything else is unsupported in this module */
        gomc_log_errorf(inst->log, inst->name,
            "j%d: only absolute/immediate homing supported for non-drive joints", jno);
        jd->home_state = HOME_ABORT;
        break;

    case HOME_SET_SWITCH_POSITION:
        if (jd->home_flags & HOME_ABSOLUTE_ENCODER) {
            offset = jd->home_offset;
        } else {
            offset = jd->home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, jno);
        }
        inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno,
            inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) + offset);
        inst->mot->joint_set_pos_fb(inst->mot->ctx, jno,
            inst->mot->joint_get_pos_fb(inst->mot->ctx, jno) + offset);
        inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno,
            inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, jno) + offset);
        inst->mot->joint_set_motor_offset(inst->mot->ctx, jno,
            inst->mot->joint_get_motor_offset(inst->mot->ctx, jno) - offset);

        if (jd->home_flags & HOME_NO_FINAL_MOVE) {
            jd->home_state = HOME_FINISHED;
            jd->homed = 1;
            break;
        }
        jd->home_state = HOME_FINAL_MOVE_START;
        break;

    case HOME_FINAL_MOVE_START:
        inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno, jd->home);
        if (jd->home_final_vel > 0) {
            double vl = inst->mot->joint_get_vel_limit(inst->mot->ctx, jno);
            double vel = fabs(jd->home_final_vel);
            inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno, vel < vl ? vel : vl);
        } else {
            inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno,
                inst->mot->joint_get_vel_limit(inst->mot->ctx, jno));
        }
        inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 1);
        jd->home_state = HOME_FINAL_MOVE_WAIT;
        break;

    case HOME_FINAL_MOVE_WAIT:
        if (!inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
            jd->home_state = HOME_FINISHED;
        }
        break;

    case HOME_FINISHED:
        jd->homing = 0;
        jd->homed = 1;
        jd->home_state = HOME_IDLE;
        if (!(jd->home_flags & HOME_ABSOLUTE_ENCODER)) {
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, jd->home);
        }
        jd->joint_in_sequence = 0;
        return 0;

    case HOME_ABORT:
        jd->homing = 0;
        jd->homed = 0;
        jd->joint_in_sequence = 0;
        inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
        jd->home_state = HOME_IDLE;
        jd->index_enable = 0;
        return 0;

    default:
        jd->home_state = HOME_ABORT;
        break;
    }

    return (jd->home_state != HOME_IDLE) ? 1 : 0;
}

/***********************************************************************
*                SEQUENCE MANAGEMENT (simplified)                     *
***********************************************************************/

static void do_homing_sequence(linmot_inst_t *inst)
{
    int i;

    switch (inst->sequence_state) {
    case SEQ_IDLE:
        inst->current_sequence = 0;
        break;

    case SEQ_DO_ONE_JOINT:
        for (i = 0; i < inst->all_joints; i++) {
            if (inst->H[i].home_state == HOME_START) {
                inst->H[i].joint_in_sequence = 1;
                inst->current_sequence = ABS(inst->H[i].home_sequence);
            } else {
                inst->H[i].joint_in_sequence = 0;
            }
        }
        inst->sequence_state = SEQ_START_JOINTS;
        break;

    case SEQ_DO_ONE_SEQUENCE:
        for (i = 0; i < inst->all_joints; i++) {
            if (inst->H[i].home_state == HOME_START) {
                inst->current_sequence = ABS(inst->H[i].home_sequence);
                inst->H[i].joint_in_sequence = 1;
            } else {
                inst->H[i].joint_in_sequence = 0;
            }
        }
        inst->sequence_state = SEQ_START_JOINTS;
        break;

    case SEQ_START:
        /* Home-all: start with sequence 0 (or 1) */
        for (i = 0; i < inst->all_joints; i++) {
            inst->H[i].joint_in_sequence = 1;
            if (inst->H[i].home_sequence > 100) {
                inst->H[i].joint_in_sequence = 0;
            }
        }
        /* Find the first (lowest) sequence to run */
        {
            int min_seq = 9999;
            for (i = 0; i < inst->all_joints; i++) {
                if (inst->H[i].joint_in_sequence &&
                    ABS(inst->H[i].home_sequence) < min_seq) {
                    min_seq = ABS(inst->H[i].home_sequence);
                }
            }
            inst->current_sequence = min_seq;
        }
        /* Mark only joints in current sequence */
        for (i = 0; i < inst->all_joints; i++) {
            if (ABS(inst->H[i].home_sequence) != inst->current_sequence) {
                inst->H[i].joint_in_sequence = 0;
            }
        }
        /* Set HOME_START for joints in this sequence */
        for (i = 0; i < inst->all_joints; i++) {
            if (inst->H[i].joint_in_sequence) {
                inst->H[i].home_state = HOME_START;
            }
        }
        inst->sequence_state = SEQ_START_JOINTS;
        break;

    case SEQ_START_JOINTS:
        inst->sequence_state = SEQ_WAIT_JOINTS;
        break;

    case SEQ_WAIT_JOINTS:
        /* Check if all joints in current sequence are done */
        {
            bool all_done = true;
            for (i = 0; i < inst->all_joints; i++) {
                if (inst->H[i].joint_in_sequence && inst->H[i].homing) {
                    all_done = false;
                    break;
                }
            }
            if (all_done) {
                /* Find next sequence */
                int next_seq = 9999;
                for (i = 0; i < inst->all_joints; i++) {
                    if (!inst->H[i].homed &&
                        ABS(inst->H[i].home_sequence) > inst->current_sequence &&
                        ABS(inst->H[i].home_sequence) < next_seq &&
                        inst->H[i].home_sequence <= 100) {
                        next_seq = ABS(inst->H[i].home_sequence);
                    }
                }
                if (next_seq == 9999) {
                    /* All sequences done */
                    inst->sequence_state = SEQ_IDLE;
                } else {
                    /* Start next sequence */
                    inst->current_sequence = next_seq;
                    for (i = 0; i < inst->all_joints; i++) {
                        if (ABS(inst->H[i].home_sequence) == next_seq) {
                            inst->H[i].joint_in_sequence = 1;
                            inst->H[i].home_state = HOME_START;
                        } else {
                            inst->H[i].joint_in_sequence = 0;
                        }
                    }
                    inst->sequence_state = SEQ_START_JOINTS;
                }
            }
        }
        break;
    }
}

/***********************************************************************
*                    MAIN HOMING ENTRY POINTS                         *
***********************************************************************/

static int make_pins(linmot_inst_t *inst, int id, int njoints)
{
    int jno, retval = 0;
    const char *P = inst->pin_prefix;

    for (jno = 0; jno < njoints; jno++) {
        /* Standard homing pins (same interface as default homemod) */
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN,
            &inst->std_pins[jno].home_sw, id,
            "%sjoint.%d.home-sw-in", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT,
            &inst->std_pins[jno].homing, id,
            "%sjoint.%d.homing", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT,
            &inst->std_pins[jno].homed, id,
            "%sjoint.%d.homed", P, jno);
        retval += gomc_hal_pin_s32_newf(inst->hal, GOMC_HAL_OUT,
            &inst->std_pins[jno].home_state, id,
            "%sjoint.%d.home-state", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IO,
            &inst->std_pins[jno].index_enable, id,
            "%sjoint.%d.index-enable", P, jno);

        /* Drive homing pins */
        retval += gomc_hal_pin_u32_newf(inst->hal, GOMC_HAL_OUT,
            &inst->drv_pins[jno].opmode_cmd, id,
            "%sjoint.%d.drv-opmode-cmd", P, jno);
        retval += gomc_hal_pin_u32_newf(inst->hal, GOMC_HAL_IN,
            &inst->drv_pins[jno].opmode_fb, id,
            "%sjoint.%d.drv-opmode-fb", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT,
            &inst->drv_pins[jno].home_cmd, id,
            "%sjoint.%d.drv-home-cmd", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN,
            &inst->drv_pins[jno].homing_attained, id,
            "%sjoint.%d.drv-homing-attained", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN,
            &inst->drv_pins[jno].homing_error, id,
            "%sjoint.%d.drv-homing-error", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN,
            &inst->drv_pins[jno].drive_homing, id,
            "%sjoint.%d.drv-use-drive-homing", P, jno);
    }
    return retval;
}

static int base_homing_init(linmot_inst_t *inst, int id,
                            double servo_period, int n_joints, int n_extrajoints)
{
    int jno;
    inst->servo_freq = 1.0 / servo_period;
    inst->all_joints = n_joints;
    inst->extra_joints = n_extrajoints;

    for (jno = 0; jno < MAX_JOINTS; jno++) {
        inst->H[jno].home_state = HOME_IDLE;
        inst->H[jno].drv_state = DRV_HOME_IDLE;
        inst->H[jno].homing = 0;
        inst->H[jno].homed = 0;
        inst->H[jno].home_sequence = 999; /* unspecified */
    }

    /* Set all opmode_cmd to CSP as default */
    for (jno = 0; jno < n_joints; jno++) {
        if (inst->drv_pins[jno].opmode_cmd) {
            *(inst->drv_pins[jno].opmode_cmd) = CIA402_OP_CSP;
        }
    }

    return make_pins(inst, id, n_joints);
}

static void read_in_pins(linmot_inst_t *inst, int njoints)
{
    int jno;
    for (jno = 0; jno < njoints; jno++) {
        inst->H[jno].home_sw      = *(inst->std_pins[jno].home_sw);
        inst->H[jno].index_enable = *(inst->std_pins[jno].index_enable);
    }
}

static void write_out_pins(linmot_inst_t *inst, int njoints)
{
    int jno;
    for (jno = 0; jno < njoints; jno++) {
        *(inst->std_pins[jno].homing)     = inst->H[jno].homing;
        *(inst->std_pins[jno].homed)      = inst->H[jno].homed;
        *(inst->std_pins[jno].home_state) = inst->H[jno].home_state;
        *(inst->std_pins[jno].index_enable) = inst->H[jno].index_enable;
    }
}

static bool do_homing(linmot_inst_t *inst)
{
    int jno;
    int homing_flag = 0;
    bool beginning_allhomed = get_allhomed(inst);

    do_homing_sequence(inst);

    for (jno = 0; jno < inst->all_joints; jno++) {
        if (!inst->H[jno].joint_in_sequence) continue;
        if (!inst->mot->joint_get_active_flag(inst->mot->ctx, jno)) continue;

        /* Determine homing method when homing starts */
        if (inst->H[jno].home_state == HOME_START && !inst->H[jno].homing) {
            inst->H[jno].use_drive_homing = *(inst->drv_pins[jno].drive_homing);
            if (inst->H[jno].use_drive_homing) {
                /* Start drive homing */
                inst->H[jno].homing = 1;
                inst->H[jno].homed = 0;
                inst->H[jno].home_state = HOME_INDEX_SEARCH_WAIT; /* for ferror suppression */
                inst->H[jno].drv_state = DRV_HOME_SWITCH_MODE;
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
            }
        }

        /* Run appropriate state machine */
        if (inst->H[jno].use_drive_homing && inst->H[jno].drv_state != DRV_HOME_IDLE) {
            homing_flag += drive_home_joint(inst, jno);
        } else {
            homing_flag += std_home_joint(inst, jno);
        }
    }

    if (homing_flag > 0) {
        inst->homing_active = 1;
    } else {
        if (inst->sequence_state == SEQ_IDLE) {
            inst->homing_active = 0;
        }
    }

    if (!beginning_allhomed && get_allhomed(inst)) {
        inst->homing_active = 0;
        return 1;
    }
    return 0;
}

/***********************************************************************
*                    QUERY FUNCTIONS                                   *
***********************************************************************/

static bool get_allhomed(linmot_inst_t *inst)
{
    int jno;
    for (jno = 0; jno < inst->all_joints; jno++) {
        if (!inst->mot->joint_get_active_flag(inst->mot->ctx, jno)) continue;
        if (!inst->H[jno].homed) return false;
    }
    return true;
}

static bool get_homing_is_active(linmot_inst_t *inst) {
    return inst->homing_active;
}

static int get_home_sequence(linmot_inst_t *inst, int jno) {
    return inst->H[jno].home_sequence;
}

static bool get_homing(linmot_inst_t *inst, int jno) {
    return inst->H[jno].homing;
}

static bool get_homed(linmot_inst_t *inst, int jno) {
    return inst->H[jno].homed;
}

static bool get_index_enable(linmot_inst_t *inst, int jno) {
    return inst->H[jno].index_enable;
}

static bool get_home_needs_unlock_first(linmot_inst_t *inst, int jno) {
    return (inst->H[jno].home_flags & HOME_UNLOCK_FIRST) ? 1 : 0;
}

static bool get_home_is_idle(linmot_inst_t *inst, int jno) {
    return inst->H[jno].home_state == HOME_IDLE;
}

static bool get_home_is_synchronized(linmot_inst_t *inst, int jno) {
    return inst->H[jno].home_is_synchronized;
}

static bool get_homing_at_index_search_wait(linmot_inst_t *inst, int jno) {
    /* This is the key: return true while drive is homing to suppress ferror.
     * motmod uses this + index_enable==0 to force pos_fb = pos_cmd.
     * Stop suppressing once we're in final move (drive is back in CSP,
     * position tracking is active). */
    if (inst->H[jno].use_drive_homing &&
        inst->H[jno].drv_state != DRV_HOME_IDLE &&
        inst->H[jno].drv_state != DRV_HOME_FINAL_MOVE &&
        inst->H[jno].drv_state != DRV_HOME_FINAL_WAIT &&
        inst->H[jno].drv_state != DRV_HOME_DONE) {
        return true;
    }
    return inst->H[jno].home_state == HOME_INDEX_SEARCH_WAIT;
}

/***********************************************************************
*                    CONTROL FUNCTIONS                                 *
***********************************************************************/

static void do_home_joint(linmot_inst_t *inst, int jno)
{
    if (jno == -1) {
        /* Home all */
        inst->H[0].homed = 0;
        inst->sequence_state = SEQ_START;
    } else if (inst->H[jno].home_sequence < 0) {
        /* Negative sequence: home all joints in that sequence */
        int jj;
        inst->sequence_state = SEQ_DO_ONE_SEQUENCE;
        for (jj = 0; jj < inst->all_joints; jj++) {
            if (ABS(inst->H[jj].home_sequence) == ABS(inst->H[jno].home_sequence)) {
                inst->H[jj].home_state = HOME_START;
            }
        }
    } else {
        inst->sequence_state = SEQ_DO_ONE_JOINT;
        inst->H[jno].home_state = HOME_START;
    }
}

static void do_cancel_homing(linmot_inst_t *inst, int jno)
{
    if (inst->H[jno].homing) {
        if (inst->H[jno].use_drive_homing) {
            /* Cancel drive homing - switch back to CSP */
            inst->H[jno].drv_state = DRV_HOME_ERROR;
        } else {
            inst->H[jno].home_state = HOME_ABORT;
        }
    } else if (inst->H[jno].joint_in_sequence) {
        inst->H[jno].home_state = HOME_ABORT;
    }
}

static void set_unhomed(linmot_inst_t *inst, int jno, motion_state_t motstate __attribute__((unused)))
{
    if (jno == -1 || jno == -2) {
        int j;
        for (j = 0; j < inst->all_joints; j++) {
            if (inst->mot->joint_get_active_flag(inst->mot->ctx, j)) {
                if (get_homing(inst, j)) continue;
                if (jno == -1 || (jno == -2 && inst->H[j].volatile_home)) {
                    inst->H[j].homed = 0;
                }
            }
        }
    } else {
        if (!get_homing(inst, jno)) {
            inst->H[jno].homed = 0;
        }
    }
}

/***********************************************************************
*                GMI CALLBACK WRAPPERS                                 *
***********************************************************************/

static int32_t gmi_home_init(void *ctx, int32_t comp_id, double servo_period,
    int32_t n_joints, int32_t n_extrajoints)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    inst->comp_id = comp_id;
    return base_homing_init(inst, comp_id, servo_period, n_joints, n_extrajoints);
}

static int32_t gmi_home_set_joint_params(void *ctx, int32_t jno, double offset, double home,
    double home_final_vel, double home_search_vel,
    double home_latch_vel, int32_t home_flags,
    int32_t home_sequence, int32_t volatile_home)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    inst->H[jno].home_offset     = offset;
    inst->H[jno].home            = home;
    inst->H[jno].home_final_vel  = home_final_vel;
    inst->H[jno].home_search_vel = home_search_vel;
    inst->H[jno].home_latch_vel  = home_latch_vel;
    inst->H[jno].home_flags      = home_flags;
    inst->H[jno].home_sequence   = home_sequence;
    inst->H[jno].volatile_home   = (bool)volatile_home;
    update_home_is_synchronized(inst);
    return 0;
}

static int32_t gmi_home_update_joint_params(void *ctx, int32_t jno, double home_offset,
    double home_home, int32_t home_sequence)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    inst->H[jno].home_offset   = home_offset;
    inst->H[jno].home          = home_home;
    inst->H[jno].home_sequence = home_sequence;
    update_home_is_synchronized(inst);
    return 0;
}

static int32_t gmi_home_read_in_pins(void *ctx, int32_t njoints)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    read_in_pins(inst, njoints);
    return 0;
}

static int32_t gmi_home_do_homing(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    return (int32_t)do_homing(inst);
}

static int32_t gmi_home_write_out_pins(void *ctx, int32_t njoints)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    write_out_pins(inst, njoints);
    return 0;
}

static int32_t gmi_home_do_home_joint(void *ctx, int32_t jno)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    do_home_joint(inst, jno);
    return 0;
}

static int32_t gmi_home_do_cancel(void *ctx, int32_t jno)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    do_cancel_homing(inst, jno);
    return 0;
}

static int32_t gmi_home_set_unhomed(void *ctx, int32_t jno, home_motion_state_t motstate)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    set_unhomed(inst, jno, (motion_state_t)motstate);
    return 0;
}

static int32_t gmi_home_get_allhomed(void *ctx) {
    return (int32_t)get_allhomed((linmot_inst_t *)ctx);
}
static int32_t gmi_home_get_is_active(void *ctx) {
    return (int32_t)get_homing_is_active((linmot_inst_t *)ctx);
}
static int32_t gmi_home_get_sequence(void *ctx, int32_t jno) {
    return get_home_sequence((linmot_inst_t *)ctx, jno);
}
static int32_t gmi_home_get_homing(void *ctx, int32_t jno) {
    return (int32_t)get_homing((linmot_inst_t *)ctx, jno);
}
static int32_t gmi_home_get_homed(void *ctx, int32_t jno) {
    return (int32_t)get_homed((linmot_inst_t *)ctx, jno);
}
static int32_t gmi_home_get_index_enable(void *ctx, int32_t jno) {
    return (int32_t)get_index_enable((linmot_inst_t *)ctx, jno);
}
static int32_t gmi_home_get_needs_unlock_first(void *ctx, int32_t jno) {
    return (int32_t)get_home_needs_unlock_first((linmot_inst_t *)ctx, jno);
}
static int32_t gmi_home_get_is_idle(void *ctx, int32_t jno) {
    return (int32_t)get_home_is_idle((linmot_inst_t *)ctx, jno);
}
static int32_t gmi_home_get_is_synchronized(void *ctx, int32_t jno) {
    return (int32_t)get_home_is_synchronized((linmot_inst_t *)ctx, jno);
}
static int32_t gmi_home_get_at_index_search_wait(void *ctx, int32_t jno) {
    return (int32_t)get_homing_at_index_search_wait((linmot_inst_t *)ctx, jno);
}

/***********************************************************************
*                    CMOD LIFECYCLE                                    *
***********************************************************************/

static void linmot_cmod_destroy(cmod_t *self)
{
    linmot_inst_t *inst = (linmot_inst_t *)((char *)self - offsetof(linmot_inst_t, cmod));
    free(inst);
}

static int linmot_cmod_init(cmod_t *self)
{
    linmot_inst_t *inst = (linmot_inst_t *)((char *)self - offsetof(linmot_inst_t, cmod));
    const mot_callbacks_t *mot = mot_api_get(inst->api, inst->mot_instance);
    if (!mot) return -1;
    inst->mot = mot;
    return 0;
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    linmot_inst_t *inst = calloc(1, sizeof(linmot_inst_t));
    if (!inst) return -1;

    inst->api = env->api;
    inst->hal = env->hal;
    inst->log = env->log;
    snprintf(inst->name, sizeof(inst->name), "%s", name);

    /* Pin prefix: empty for "homemod" name, "name." otherwise */
    if (strcmp(name, "homemod") == 0) {
        inst->pin_prefix[0] = '\0';
    } else {
        snprintf(inst->pin_prefix, sizeof(inst->pin_prefix), "%s.", name);
    }

    /* Parse mot_instance parameter (default: "motmod") */
    snprintf(inst->mot_instance, sizeof(inst->mot_instance), "motmod");
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "mot_instance=", 13) == 0)
            snprintf(inst->mot_instance, sizeof(inst->mot_instance), "%s", argv[i] + 13);
    }

    /* Register callbacks */
    inst->callbacks = (home_callbacks_t)GMI_HOME_CALLBACKS;
    inst->callbacks.ctx = inst;

    int rc = home_api_register(env->api, name, &inst->callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register home API: %d", rc);
        free(inst);
        return rc;
    }

    inst->cmod.Init    = linmot_cmod_init;
    inst->cmod.Start   = NULL;
    inst->cmod.Destroy = linmot_cmod_destroy;
    *out = &inst->cmod;
    return 0;
}
