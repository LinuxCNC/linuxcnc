/*
 * watchdog.c — cmod HAL component: multiple-input watchdog.
 *
 * Monitors N input bits for transitions. If any input stops toggling
 * within its timeout, the output goes low.
 *
 * Usage:
 *   load watchdog num_inputs=4
 *
 * Original author: Stephen Wille Padnos (swpadnos AT sourceforge DOT net)
 * Copyright (c) 2010 Stephen Wille Padnos
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define MAX_INPUTS 32

typedef struct {
    gomc_hal_bit_t *input;
    int32_t c_secs, c_nsecs;
    int32_t t_secs, t_nsecs;
    int32_t old_bit;
} wd_input_t;

typedef struct {
    gomc_hal_bit_t   *output;
    gomc_hal_bit_t   *enable;
    gomc_hal_float_t *timeout;  /* per-input timeout array (HAL params) */
} wd_pins_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    int num_inputs;
    wd_pins_t *pins;
    wd_input_t *inputs;
    gomc_hal_float_t **timeouts; /* array of pointers to per-input timeout pins */
    int32_t old_enable;
} inst_t;

static void process(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int i, fault = 0;

    if (!*(inst->pins->enable) || !*(inst->pins->output))
        return;

    for (i = 0; i < inst->num_inputs; i++) {
        wd_input_t *inp = &inst->inputs[i];
        int32_t cur = *(inp->input);

        if (cur != inp->old_bit) {
            inp->c_secs = inp->t_secs;
            inp->c_nsecs = inp->t_nsecs;
        } else {
            inp->c_nsecs -= period;
            if (inp->c_nsecs < 0) {
                inp->c_nsecs += 1000000000;
                if (inp->c_secs > 0) {
                    inp->c_secs--;
                } else {
                    fault = 1;
                    inp->c_secs = inp->c_nsecs = 0;
                }
            }
        }
        inp->old_bit = cur;
    }
    if (fault)
        *(inst->pins->output) = 0;
}

static void set_timeouts(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int i;
    double temp;

    (void)period;

    for (i = 0; i < inst->num_inputs; i++) {
        wd_input_t *inp = &inst->inputs[i];
        temp = *(inst->timeouts[i]);
        if (temp < 0) temp = 0;
        inp->t_secs = (int32_t)temp;
        temp -= inp->t_secs;
        inp->t_nsecs = (int32_t)(1e9 * temp);
    }

    if (!*(inst->pins->output)) {
        if (*(inst->pins->enable) && !inst->old_enable) {
            for (i = 0; i < inst->num_inputs; i++) {
                inst->inputs[i].c_secs = inst->inputs[i].t_secs;
                inst->inputs[i].c_nsecs = inst->inputs[i].t_nsecs;
            }
            *(inst->pins->output) = 1;
        }
    }
    inst->old_enable = *(inst->pins->enable);
}

static void inst_destroy(cmod_t *self) {
    inst_t *inst = (inst_t *)self;
    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    inst_t *inst;
    int r, i;
    int num_inputs = 0;
    char buf[GOMC_HAL_NAME_LEN + 1];

    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "num_inputs=", 11) == 0)
            num_inputs = atoi(argv[i] + 11);
    }

    if (num_inputs < 1 || num_inputs > MAX_INPUTS) {
        gomc_log_errorf(env->log, name,
                        "num_inputs=%d out of range (1..%d)", num_inputs, MAX_INPUTS);
        return -EINVAL;
    }

    /* allocate instance (includes space for inputs and timeout pointers) */
    size_t sz = sizeof(inst_t) + sizeof(wd_input_t) * num_inputs +
                sizeof(gomc_hal_float_t *) * num_inputs;
    inst = env->rtapi->calloc(env->rtapi->ctx, sz);
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->num_inputs = num_inputs;
    inst->inputs = (wd_input_t *)((char *)inst + sizeof(inst_t));
    inst->timeouts = (gomc_hal_float_t **)((char *)inst->inputs +
                     sizeof(wd_input_t) * num_inputs);

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->pins = env->hal->malloc(env->hal->ctx,
                                  sizeof(wd_pins_t) + sizeof(gomc_hal_bit_t *) * num_inputs +
                                  sizeof(gomc_hal_float_t *) * num_inputs);
    if (!inst->pins) goto err;
    memset(inst->pins, 0, sizeof(wd_pins_t));

    /* per-input pins */
    for (i = 0; i < num_inputs; i++) {
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN,
                                  &inst->inputs[i].input, inst->comp_id,
                                  "%s.input-%d", name, i);
        if (r != 0) goto err;

        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO,
                                    &inst->timeouts[i], inst->comp_id,
                                    "%s.timeout-%d", name, i);
        if (r != 0) goto err;
        *(inst->timeouts[i]) = 0;
    }

    /* global pins */
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &inst->pins->output,
                              inst->comp_id, "%s.ok-out", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &inst->pins->enable,
                              inst->comp_id, "%s.enable-in", name);
    if (r != 0) goto err;

    /* export functions */
    snprintf(buf, sizeof(buf), "%s.process", name);
    r = env->hal->export_funct(env->hal->ctx, buf, process, inst, 0, 0,
                               inst->comp_id);
    if (r != 0) goto err;

    snprintf(buf, sizeof(buf), "%s.set-timeouts", name);
    r = env->hal->export_funct(env->hal->ctx, buf, set_timeouts, inst, 1, 0,
                               inst->comp_id);
    if (r != 0) goto err;

    r = env->hal->ready(env->hal->ctx, inst->comp_id);
    if (r != 0) goto err;

    *out = &inst->base;
    return 0;

err:
    if (inst->comp_id > 0)
        env->hal->exit(env->hal->ctx, inst->comp_id);
    env->rtapi->free(env->rtapi->ctx, inst);
    return -1;
}
