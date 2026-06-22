/*
 * spindle.c — cmod HAL component for spindle control with gear support.
 *
 * Converted from spindle.comp to native cmod API to support dynamic
 * per-gear pin creation via multiple load commands.
 *
 * Usage:
 *   load spindle numgears=3
 *
 * Creates pins for the specified number of gears.
 *
 * Author: Les Newell <les at sheetcam dot com> (original)
 * Copyright (c) 2009 Les Newell
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
 * Converted to cmod API: 2026
 * License: GPL Version 2 or later
 */

#include "gomc_env.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAX_GEARS 16

/* Per-gear pin structure */
typedef struct {
    gomc_hal_float_t *scale;
    gomc_hal_float_t *min;
    gomc_hal_float_t *max;
    gomc_hal_float_t *accel;
    gomc_hal_float_t *decel;
    gomc_hal_float_t *speed_tolerance;
    gomc_hal_float_t *zero_tolerance;
    gomc_hal_float_t *offset;
    gomc_hal_bit_t   *select;
} gear_t;

/* HAL shared memory portion */
typedef struct {
    /* Input pins */
    gomc_hal_u32_t   *select_gear;
    gomc_hal_float_t *commanded_speed;
    gomc_hal_float_t *actual_speed;
    gomc_hal_bit_t   *simulate_encoder;
    gomc_hal_bit_t   *enable;
    gomc_hal_float_t *spindle_lpf;

    /* Output pins */
    gomc_hal_float_t *spindle_rpm;
    gomc_hal_float_t *spindle_rpm_abs;
    gomc_hal_float_t *output;
    gomc_hal_u32_t   *current_gear;
    gomc_hal_bit_t   *at_speed;
    gomc_hal_bit_t   *forward;
    gomc_hal_bit_t   *reverse;
    gomc_hal_bit_t   *brake;
    gomc_hal_bit_t   *zero_speed;
    gomc_hal_bit_t   *limited;

    /* Per-gear pins */
    gear_t gears[MAX_GEARS];
} inst_hal_t;

/* Instance structure */
typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    inst_hal_t *hal;
    int ngears;
} inst_t;

/* Convenience macros for accessing pins — only for use in funct__() */
#define HAL_select_gear      (*(__comp_inst->hal->select_gear))
#define HAL_commanded_speed  (*(__comp_inst->hal->commanded_speed))
#define HAL_actual_speed     (*(__comp_inst->hal->actual_speed))
#define HAL_simulate_encoder (*(__comp_inst->hal->simulate_encoder))
#define HAL_enable           (*(__comp_inst->hal->enable))
#define HAL_spindle_lpf      (*(__comp_inst->hal->spindle_lpf))
#define HAL_spindle_rpm      (*(__comp_inst->hal->spindle_rpm))
#define HAL_spindle_rpm_abs  (*(__comp_inst->hal->spindle_rpm_abs))
#define HAL_output           (*(__comp_inst->hal->output))
#define HAL_current_gear     (*(__comp_inst->hal->current_gear))
#define HAL_at_speed         (*(__comp_inst->hal->at_speed))
#define HAL_forward          (*(__comp_inst->hal->forward))
#define HAL_reverse          (*(__comp_inst->hal->reverse))
#define HAL_brake            (*(__comp_inst->hal->brake))
#define HAL_zero_speed       (*(__comp_inst->hal->zero_speed))
#define HAL_limited          (*(__comp_inst->hal->limited))

#define fperiod (period * 1e-9)

/* Forward declarations */
static void funct__(void *arg, long period);
static void spindle_Stop(cmod_t *self);
static void spindle_Destroy(cmod_t *self);

static void funct__(void *arg, long period) {
    inst_t *__comp_inst = (inst_t *)arg;
    int ct;
    double cmdspeed;
    double curspeed;
    double diff;
    double tmp;
    bool limit = false;
    bool reversing;

    gear_t *thisgear = &__comp_inst->hal->gears[0];
    if (HAL_select_gear != 0) {
        HAL_current_gear = HAL_select_gear;
        if (HAL_current_gear > 15) HAL_current_gear = 15;
        thisgear = &__comp_inst->hal->gears[HAL_current_gear];
    } else {
        HAL_current_gear = 0;
        for (ct = __comp_inst->ngears - 1; ct >= 0; ct--) {
            thisgear = &__comp_inst->hal->gears[ct];
            if (*(thisgear->select)) {
                HAL_current_gear = ct;
                break;
            }
        }
    }

    if (!HAL_simulate_encoder) {
        HAL_spindle_rpm = HAL_actual_speed * 60; /* RPS to RPM */
    }
    curspeed = HAL_spindle_rpm;

    if (HAL_enable) {
        cmdspeed = HAL_commanded_speed;
    } else {
        cmdspeed = 0;
    }

    if (curspeed >= 0) {
        tmp = curspeed;
    } else {
        tmp = -curspeed;
    }
    HAL_zero_speed = (tmp <= *(thisgear->zero_tolerance));

    if (HAL_spindle_lpf > 0 && HAL_at_speed) {
        HAL_spindle_rpm_abs += (tmp - HAL_spindle_rpm_abs) * HAL_spindle_lpf * fperiod;
    } else {
        HAL_spindle_rpm_abs = tmp;
    }

    reversing = (cmdspeed > 0 && curspeed < 0) || (cmdspeed < 0 && curspeed > 0);

    diff = cmdspeed - curspeed;
    if (diff < 0) diff = -diff;
    tmp = *(thisgear->speed_tolerance);
    HAL_at_speed = (tmp > 0 && tmp >= diff) && !reversing;

    tmp = *(thisgear->min);
    if (tmp > 0) {
        if (cmdspeed > 0) {
            if (cmdspeed < tmp) {
                cmdspeed = tmp;
                limit = true;
            }
        } else if (cmdspeed < 0) {
            if (cmdspeed > -tmp) {
                cmdspeed = -tmp;
                limit = true;
            }
        }
    }

    tmp = *(thisgear->max);
    if (tmp > 0) {
        if (cmdspeed > 0 && cmdspeed > tmp) {
            cmdspeed = tmp;
            limit = true;
        }
        if (cmdspeed < 0 && cmdspeed < -tmp) {
            cmdspeed = -tmp;
            limit = true;
        }
    }

    diff = cmdspeed - curspeed;

    /* make sure we don't cross zero speed */
    if (curspeed > 0 && diff < -curspeed) diff = -curspeed;
    if (curspeed < 0 && diff > -curspeed) diff = -curspeed;

    tmp = *(thisgear->accel);
    if (tmp > 0 && !reversing) {
        if (HAL_simulate_encoder) tmp *= fperiod;
        if (cmdspeed > 0 && diff > tmp) {
            diff = tmp;
        }
        if (cmdspeed < 0 && diff < -tmp) {
            diff = -tmp;
        }
    }

    tmp = *(thisgear->decel);
    if (tmp > 0) {
        if (HAL_simulate_encoder) tmp *= fperiod;
        if (reversing) {
            if (cmdspeed >= 0 && diff > tmp) diff = tmp;
            if (cmdspeed <= 0 && diff < -tmp) diff = -tmp;
        } else {
            if (cmdspeed >= 0 && diff < -tmp) diff = -tmp;
            if (cmdspeed <= 0 && diff > tmp) diff = tmp;
        }
    }

    if (HAL_at_speed) { /* stop hunting when at speed */
        curspeed = cmdspeed;
    } else {
        curspeed += diff;
    }

    if (HAL_simulate_encoder) {
        HAL_spindle_rpm = curspeed;
    }

    if (cmdspeed != 0) {
        HAL_forward = (curspeed > 0);
        HAL_reverse = (curspeed < 0);
    } else { /* don't try to move once stopped */
        HAL_forward = (curspeed > 0) && HAL_forward;
        HAL_reverse = (curspeed < 0) && HAL_forward;
    }

    if (curspeed < 0) curspeed = -curspeed;
    HAL_output = (curspeed + (*(thisgear->offset))) * (*(thisgear->scale));
    HAL_limited = limit;
}

static int add_gear(const cmod_env_t *env, int comp_id, const char *name,
                    int index, gear_t *g) {
    int r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->scale, comp_id,
                                "%s.scale.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->min, comp_id,
                                "%s.min.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->max, comp_id,
                                "%s.max.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->accel, comp_id,
                                "%s.accel.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->decel, comp_id,
                                "%s.decel.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->speed_tolerance, comp_id,
                                "%s.speed-tolerance.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->zero_tolerance, comp_id,
                                "%s.zero-tolerance.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &g->offset, comp_id,
                                "%s.offset.%d", name, index);
    if (r != 0) return r;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &g->select, comp_id,
                              "%s.select.%d", name, index);
    if (r != 0) return r;

    /* Set defaults */
    *(g->scale) = 1.0;
    *(g->min) = 0.0;
    *(g->max) = 0.0;
    *(g->accel) = 0.0;
    *(g->decel) = 0.0;
    *(g->speed_tolerance) = 20.0;
    *(g->zero_tolerance) = 20.0;
    *(g->offset) = 0.0;
    *(g->select) = FALSE;

    return 0;
}

static void spindle_Stop(cmod_t *self) {
    (void)self;
}

static void spindle_Destroy(cmod_t *self) {
    inst_t *inst = (inst_t *)self;
    if (inst->comp_id > 0) {
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    }
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    inst_t *inst;
    int r, ct;
    int numgears = 1;

    /* Parse modparams */
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "numgears=", 9) == 0) {
            numgears = atoi(argv[i] + 9);
        }
    }

    if (numgears < 1 || numgears > MAX_GEARS) {
        gomc_log_errorf(env->log, name,
                        "numgears=%d is out of range (1..%d)", numgears, MAX_GEARS);
        return -EINVAL;
    }

    /* Allocate instance */
    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) {
        gomc_log_errorf(env->log, name, "failed to allocate instance");
        return -ENOMEM;
    }

    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->ngears = numgears;

    /* Initialize HAL component */
    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) {
        gomc_log_errorf(env->log, name, "hal_init failed");
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }

    /* Allocate HAL shared memory */
    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(inst_hal_t));
    if (!inst->hal) {
        gomc_log_errorf(env->log, name, "failed to allocate HAL memory");
        goto err;
    }
    memset(inst->hal, 0, sizeof(inst_hal_t));

    /* Create input pins */
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IN,
            &inst->hal->select_gear, inst->comp_id,
            "%s.select-gear", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &inst->hal->commanded_speed, inst->comp_id,
            "%s.commanded-speed", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &inst->hal->actual_speed, inst->comp_id,
            "%s.actual-speed", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN,
            &inst->hal->simulate_encoder, inst->comp_id,
            "%s.simulate-encoder", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN,
            &inst->hal->enable, inst->comp_id,
            "%s.enable", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &inst->hal->spindle_lpf, inst->comp_id,
            "%s.spindle-lpf", name);
    if (r != 0) goto err;

    /* Create output pins */
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->spindle_rpm, inst->comp_id,
            "%s.spindle-rpm", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->spindle_rpm_abs, inst->comp_id,
            "%s.spindle-rpm-abs", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->output, inst->comp_id,
            "%s.output", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->current_gear, inst->comp_id,
            "%s.current-gear", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->at_speed, inst->comp_id,
            "%s.at-speed", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->forward, inst->comp_id,
            "%s.forward", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->reverse, inst->comp_id,
            "%s.reverse", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->brake, inst->comp_id,
            "%s.brake", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->zero_speed, inst->comp_id,
            "%s.zero-speed", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT,
            &inst->hal->limited, inst->comp_id,
            "%s.limited", name);
    if (r != 0) goto err;

    /* Create per-gear pins */
    for (ct = 0; ct < numgears; ct++) {
        r = add_gear(env, inst->comp_id, name, ct, &inst->hal->gears[ct]);
        if (r != 0) goto err;
    }

    /* If only one gear, select it by default */
    if (numgears == 1) {
        *(inst->hal->gears[0].select) = TRUE;
    }

    /* Export function */
    r = env->hal->export_funct(env->hal->ctx, name, funct__, inst,
                               1, 0, inst->comp_id);
    if (r != 0) goto err;

    /* Mark component ready */
    r = env->hal->ready(env->hal->ctx, inst->comp_id);
    if (r != 0) goto err;

    /* Set up cmod interface */
    inst->base.Stop = spindle_Stop;
    inst->base.Destroy = spindle_Destroy;

    *out = &inst->base;
    return 0;

err:
    env->hal->exit(env->hal->ctx, inst->comp_id);
    env->rtapi->free(env->rtapi->ctx, inst);
    return -1;
}
