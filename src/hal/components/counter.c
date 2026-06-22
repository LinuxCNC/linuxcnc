/*
 * counter.c — cmod HAL component: software pulse counter.
 *
 * Single-channel software counter for unidirectional pulse streams.
 * Counts rising edges on phase-A, optional index reset on phase-Z.
 *
 * Usage:
 *   load counter
 *
 * Original author: Chris Radek <chris@timeguy.com>
 * Copyright (c) 2006 Chris Radek
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>

typedef struct {
    gomc_hal_bit_t   *phaseA;
    gomc_hal_bit_t   *phaseZ;
    gomc_hal_bit_t   *index_ena;
    gomc_hal_bit_t   *reset;
    gomc_hal_s32_t   *raw_count;
    gomc_hal_s32_t   *count;
    gomc_hal_float_t *pos;
    gomc_hal_float_t *vel;
    gomc_hal_float_t *pos_scale;
} counter_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    counter_hal_t *hal;
    /* internal state */
    unsigned char oldA;
    unsigned char oldZ;
    unsigned char reset_on_index;
    double old_scale;
    double scale;
    int32_t last_count;
    int32_t last_index_count;
} inst_t;

static void update_counter(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    counter_hal_t *h = inst->hal;

    (void)period;

    /* count on rising edge */
    if (!inst->oldA && *(h->phaseA))
        (*(h->raw_count))++;
    inst->oldA = *(h->phaseA);

    /* reset on rising edge of Z */
    if (inst->reset_on_index && !inst->oldZ && *(h->phaseZ)) {
        inst->last_index_count = *(h->raw_count);
        *(h->index_ena) = 0;
    }
    inst->oldZ = *(h->phaseZ);
}

static void capture_position(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    counter_hal_t *h = inst->hal;
    int32_t raw_count, counts;

    if (*(h->reset)) {
        *(h->raw_count) = 0;
        inst->last_index_count = 0;
        inst->last_count = 0;
    }

    raw_count = *(h->raw_count);
    *(h->count) = raw_count - inst->last_index_count;
    counts = raw_count - inst->last_count;
    inst->last_count = raw_count;

    /* check for scale change */
    if (*(h->pos_scale) != inst->old_scale) {
        inst->old_scale = *(h->pos_scale);
        if (*(h->pos_scale) < 1e-20 && *(h->pos_scale) > -1e-20)
            *(h->pos_scale) = 1.0;
        inst->scale = 1.0 / *(h->pos_scale);
    }

    *(h->pos) = *(h->count) * inst->scale;
    *(h->vel) = counts * inst->scale * 1e9 / period;

    inst->reset_on_index = *(h->index_ena);
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
    counter_hal_t *h;
    int r;
    char buf[GOMC_HAL_NAME_LEN + 1];

    (void)argc; (void)argv;

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->old_scale = 1.0;
    inst->scale = 1.0;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(counter_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(counter_hal_t));
    h = inst->hal;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->phaseA, inst->comp_id,
                              "%s.phase-A", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->phaseZ, inst->comp_id,
                              "%s.phase-Z", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &h->index_ena, inst->comp_id,
                              "%s.index-enable", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->reset, inst->comp_id,
                              "%s.reset", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &h->raw_count, inst->comp_id,
                              "%s.rawcounts", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &h->count, inst->comp_id,
                              "%s.counts", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->pos, inst->comp_id,
                                "%s.position", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->vel, inst->comp_id,
                                "%s.velocity", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->pos_scale, inst->comp_id,
                                "%s.position-scale", name);
    if (r != 0) goto err;

    *(h->pos_scale) = 1.0;

    /* export functions */
    snprintf(buf, sizeof(buf), "%s.update-counters", name);
    r = env->hal->export_funct(env->hal->ctx, buf, update_counter, inst, 0, 0,
                               inst->comp_id);
    if (r != 0) goto err;

    snprintf(buf, sizeof(buf), "%s.capture-position", name);
    r = env->hal->export_funct(env->hal->ctx, buf, capture_position, inst, 1, 0,
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
