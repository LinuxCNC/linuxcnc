/********************************************************************
* homemod_cia402.c — Per-joint homing module for CiA 402 drives
*
* Each instance manages ONE joint with drive-internal homing.
* Works with any CiA 402 compliant drive (Beckhoff, Festo, LinMot, etc.).
* During normal operation the drive runs in CSP mode (opMode=8).
* When homing is requested:
*   1. Switch drive to Homing Mode (opMode=6)
*   2. Assert Home command (control word bit 4)
*   3. Wait for HomingAttained from drive status word
*   4. Sync LinuxCNC position to drive's actual position
*   5. Switch back to CSP mode (opMode=8)
*   6. Optional final move to HOME position
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

/* HOME_* flags */
#define HOME_NO_REHOME               32
#define HOME_NO_FINAL_MOVE           64

/* CiA 402 operation modes */
#define CIA402_OP_CSP     8
#define CIA402_OP_HOMING  6

/* Mode switch timeout (servo cycles, ~5s at 1kHz) */
#define MODE_SWITCH_TIMEOUT 5000

/* Drive homing states */
typedef enum {
    DRV_HOME_IDLE = 0,
    DRV_HOME_SWITCH_MODE,
    DRV_HOME_WAIT_MODE,
    DRV_HOME_WAIT_ATTAINED,
    DRV_HOME_WAIT_CSP,
    DRV_HOME_FINAL_MOVE,
    DRV_HOME_FINAL_WAIT,
    DRV_HOME_DONE,
    DRV_HOME_ERROR,
} drv_home_state_t;

/* HAL pins */
typedef struct {
    gomc_hal_u32_t  *opmode_cmd;
    gomc_hal_u32_t  *opmode_fb;
    gomc_hal_bit_t  *home_cmd;
    gomc_hal_bit_t  *homing_attained;
    gomc_hal_bit_t  *homing_error;
    gomc_hal_bit_t  *homing_pin;     /* OUT: 1 while homing */
    gomc_hal_bit_t  *homed_pin;      /* OUT: 1 when homed */
    gomc_hal_s32_t  *home_state_pin; /* OUT: state for debug */
} linmot_pins_t;

/* Per-instance state */
typedef struct {
    const mot_callbacks_t *mot;
    const gomc_api_t *api;
    const gomc_hal_t *hal;
    const gomc_log_t *log;
    char name[GOMC_HAL_NAME_LEN + 1];
    char mot_instance[GOMC_HAL_NAME_LEN + 1];
    char pin_prefix[GOMC_HAL_NAME_LEN + 1];
    int comp_id;
    int jno;

    /* Homing parameters */
    double home_offset;
    double home;
    double home_final_vel;
    int    home_flags;
    int    home_sequence;
    bool   volatile_home;

    /* Homing state */
    drv_home_state_t drv_state;
    bool   homing;
    bool   homed;
    int    mode_wait_count;
    bool   sync_final_move_released; /* set by do_final_move() to release sync pause */

    /* HAL pins */
    linmot_pins_t pins;

    home_callbacks_t callbacks;
    cmod_t cmod;
} linmot_inst_t;


/***********************************************************************
*               DRIVE HOMING STATE MACHINE                            *
***********************************************************************/

static int drive_home_tick(linmot_inst_t *inst)
{
    int jno = inst->jno;
    linmot_pins_t *p = &inst->pins;

    switch (inst->drv_state) {
    case DRV_HOME_IDLE:
        return 0;

    case DRV_HOME_SWITCH_MODE:
        *(p->opmode_cmd) = CIA402_OP_HOMING;
        *(p->home_cmd) = 0;
        inst->mode_wait_count = 0;
        inst->drv_state = DRV_HOME_WAIT_MODE;
        return 1;

    case DRV_HOME_WAIT_MODE:
        if (*(p->opmode_fb) == CIA402_OP_HOMING) {
            *(p->home_cmd) = 1;
            inst->mode_wait_count = 0;
            inst->drv_state = DRV_HOME_WAIT_ATTAINED;
            return 1;
        }
        inst->mode_wait_count++;
        if (inst->mode_wait_count > MODE_SWITCH_TIMEOUT) {
            gomc_log_errorf(inst->log, inst->name,
                "j%d: timeout waiting for homing mode", jno);
            inst->drv_state = DRV_HOME_ERROR;
        }
        return 1;

    case DRV_HOME_WAIT_ATTAINED:
        if (*(p->homing_error)) {
            gomc_log_errorf(inst->log, inst->name,
                "j%d: drive reported homing error", jno);
            *(p->home_cmd) = 0;
            inst->drv_state = DRV_HOME_ERROR;
            return 1;
        }
        if (*(p->homing_attained)) {
            *(p->home_cmd) = 0;
            /* Sync position while still in homing mode */
            double motor_fb = inst->mot->joint_get_motor_pos_fb(inst->mot->ctx, jno);
            double backlash = inst->mot->joint_get_backlash_filt(inst->mot->ctx, jno);
            double new_motor_offset = motor_fb - backlash - inst->home_offset;

            inst->mot->joint_set_motor_offset(inst->mot->ctx, jno, new_motor_offset);
            inst->mot->joint_set_pos_fb(inst->mot->ctx, jno, inst->home_offset);
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, jno, inst->home_offset);
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, inst->home_offset);

            /* Switch to CSP */
            *(p->opmode_cmd) = CIA402_OP_CSP;
            inst->mode_wait_count = 0;
            inst->drv_state = DRV_HOME_WAIT_CSP;
            return 1;
        }
        return 1;

    case DRV_HOME_WAIT_CSP:
        if (*(p->opmode_fb) == CIA402_OP_CSP) {
            inst->drv_state = DRV_HOME_FINAL_MOVE;
            return 1;
        }
        inst->mode_wait_count++;
        if (inst->mode_wait_count > MODE_SWITCH_TIMEOUT) {
            gomc_log_errorf(inst->log, inst->name,
                "j%d: timeout waiting for CSP mode after homing", jno);
            inst->drv_state = DRV_HOME_ERROR;
        }
        return 1;

    case DRV_HOME_FINAL_MOVE:
        if ((inst->home_flags & HOME_NO_FINAL_MOVE) ||
            fabs(inst->home - inst->home_offset) < 1e-9) {
            inst->drv_state = DRV_HOME_DONE;
            return 1;
        }
        /* For synchronized homing (negative home_sequence), pause here
         * until motmod calls do_final_move() to release all joints
         * simultaneously. */
        if (inst->home_sequence < 0 && !inst->sync_final_move_released) {
            return 1; /* stay paused */
        }
        inst->sync_final_move_released = 0;
        inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno, inst->home);
        if (inst->home_final_vel > 0) {
            double vl = inst->mot->joint_get_vel_limit(inst->mot->ctx, jno);
            double vel = fabs(inst->home_final_vel);
            inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno, vel < vl ? vel : vl);
        } else {
            inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno,
                inst->mot->joint_get_vel_limit(inst->mot->ctx, jno));
        }
        inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 1);
        inst->drv_state = DRV_HOME_FINAL_WAIT;
        return 1;

    case DRV_HOME_FINAL_WAIT:
        if (!inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
            inst->drv_state = DRV_HOME_DONE;
            return 1;
        }
        return 1;

    case DRV_HOME_DONE:
        inst->homing = 0;
        inst->homed = 1;
        inst->drv_state = DRV_HOME_IDLE;
        /* If no final move was performed (HOME_NO_FINAL_MOVE or near-zero
         * offset delta), set position to home_offset (where the drive
         * actually is). Otherwise set to home (where the final move ended). */
        if (inst->home_flags & HOME_NO_FINAL_MOVE) {
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, inst->home_offset);
        } else {
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, jno, inst->home);
        }
        return 0;

    case DRV_HOME_ERROR:
        *(p->home_cmd) = 0;
        *(p->opmode_cmd) = CIA402_OP_CSP;
        inst->homing = 0;
        inst->homed = 0;
        inst->drv_state = DRV_HOME_IDLE;
        return 0;
    }
    return 0;
}


/***********************************************************************
*              GMI API FUNCTIONS                                       *
***********************************************************************/

static int32_t gmi_home_init(void *ctx, int32_t comp_id, double servo_period)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    int retval = 0;
    (void)servo_period;

    inst->comp_id = comp_id;

    retval += gomc_hal_pin_u32_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.opmode_cmd, comp_id,
                              "%s.opmode-cmd", inst->name);
    retval += gomc_hal_pin_u32_newf(inst->hal, GOMC_HAL_IN, &inst->pins.opmode_fb, comp_id,
                              "%s.opmode-fb", inst->name);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.home_cmd, comp_id,
                              "%s.home-cmd", inst->name);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN, &inst->pins.homing_attained, comp_id,
                              "%s.homing-attained", inst->name);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN, &inst->pins.homing_error, comp_id,
                              "%s.homing-error", inst->name);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.homing_pin, comp_id,
                              "%sjoint.%d.homing", inst->pin_prefix, inst->jno);
    retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.homed_pin, comp_id,
                              "%sjoint.%d.homed", inst->pin_prefix, inst->jno);
    retval += gomc_hal_pin_s32_newf(inst->hal, GOMC_HAL_OUT, &inst->pins.home_state_pin, comp_id,
                              "%sjoint.%d.home-state", inst->pin_prefix, inst->jno);
    if (retval != 0) {
        gomc_log_errorf(inst->log, inst->name, "failed to create HAL pins");
        return -1;
    }

    /* Set initial opmode to CSP */
    *(inst->pins.opmode_cmd) = CIA402_OP_CSP;

    return 0;
}

static int32_t gmi_home_set_params(void *ctx, double offset, double home,
    double home_final_vel, double home_search_vel,
    double home_latch_vel, int32_t home_flags,
    int32_t home_sequence, int32_t volatile_home)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    (void)home_search_vel; /* unused — drive does search internally */
    (void)home_latch_vel;
    inst->home_offset    = offset;
    inst->home           = home;
    inst->home_final_vel = home_final_vel;
    inst->home_flags     = home_flags;
    inst->home_sequence  = home_sequence;
    inst->volatile_home  = (bool)volatile_home;
    return 0;
}

static int32_t gmi_home_update_params(void *ctx, double home_offset,
    double home_home, int32_t home_sequence)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    inst->home_offset   = home_offset;
    inst->home          = home_home;
    inst->home_sequence = home_sequence;
    return 0;
}

static int32_t gmi_home_read_in_pins(void *ctx)
{
    (void)ctx; /* pins are read directly in state machine */
    return 0;
}

static int32_t gmi_home_do_homing(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    return (int32_t)drive_home_tick(inst);
}

static int32_t gmi_home_write_out_pins(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    *(inst->pins.homing_pin) = inst->homing;
    *(inst->pins.homed_pin) = inst->homed;
    *(inst->pins.home_state_pin) = (int32_t)inst->drv_state;
    return 0;
}

static int32_t gmi_home_do_home(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    if ((inst->home_flags & HOME_NO_REHOME) && inst->homed)
        return 0;
    inst->homing = 1;
    inst->homed = 0;
    inst->sync_final_move_released = 0;
    inst->drv_state = DRV_HOME_SWITCH_MODE;
    return 0;
}

static int32_t gmi_home_do_cancel(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    if (inst->homing || inst->drv_state != DRV_HOME_IDLE) {
        inst->drv_state = DRV_HOME_ERROR;
    }
    return 0;
}

static int32_t gmi_home_set_unhomed(void *ctx, home_motion_state_t motstate)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    (void)motstate;
    if (inst->homing) return -1;
    inst->homed = 0;
    return 0;
}

static int32_t gmi_home_get_homing(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    return (int32_t)inst->homing;
}

static int32_t gmi_home_get_homed(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    return (int32_t)inst->homed;
}

static int32_t gmi_home_get_index_enable(void *ctx)
{
    (void)ctx;
    return 0; /* LinMot doesn't use index pulse */
}

static int32_t gmi_home_get_needs_unlock_first(void *ctx)
{
    (void)ctx;
    return 0; /* Drive handles its own unlocking */
}

static int32_t gmi_home_get_is_idle(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    return inst->drv_state == DRV_HOME_IDLE ? 1 : 0;
}

static int32_t gmi_home_get_at_index_search_wait(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    /* Suppress ferror during drive homing phases (not during final move) */
    return (inst->drv_state >= DRV_HOME_SWITCH_MODE &&
            inst->drv_state <= DRV_HOME_WAIT_CSP) ? 1 : 0;
}

static int32_t gmi_home_get_at_final_move_wait(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    /* Returns 1 when the state machine is paused at DRV_HOME_FINAL_MOVE
     * waiting for the sync signal from motmod. */
    return (inst->drv_state == DRV_HOME_FINAL_MOVE &&
            inst->home_sequence < 0 &&
            !inst->sync_final_move_released) ? 1 : 0;
}

static int32_t gmi_home_do_final_move(void *ctx)
{
    linmot_inst_t *inst = (linmot_inst_t *)ctx;
    /* Release the sync pause so the final move can start. */
    inst->sync_final_move_released = 1;
    return 0;
}

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

    /* Parse joint number from instance name (e.g. "homemod.7" -> 7) */
    const char *dot = strrchr(name, '.');
    if (dot && dot[1] >= '0' && dot[1] <= '9') {
        inst->jno = atoi(dot + 1);
    } else {
        gomc_log_errorf(env->log, name,
            "Cannot parse joint number from instance name '%s'", name);
        free(inst);
        return -1;
    }

    /* Pin prefix: empty for default "homemod" prefix, "prefix." for non-default */
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

    /* Register per-instance callbacks */
    inst->callbacks = (home_callbacks_t)GMI_HOME_CALLBACKS;
    inst->callbacks.ctx = inst;

    int rc = home_api_register(env->api, name, &inst->callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name, "failed to register home API: %d", rc);
        free(inst);
        return rc;
    }

    inst->cmod.Init    = linmot_cmod_init;
    inst->cmod.Start   = NULL;
    inst->cmod.Destroy = linmot_cmod_destroy;
    *out = &inst->cmod;
    return 0;
}
