/*
 * siggen.c — cmod HAL component: signal generator.
 *
 * Generates square, triangle, sine, cosine, sawtooth, and clock signals.
 *
 * Usage:
 *   load siggen
 *
 * Original author: John Kasunich
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>
#include <math.h>

typedef struct {
    gomc_hal_float_t *square;
    gomc_hal_float_t *sawtooth;
    gomc_hal_float_t *triangle;
    gomc_hal_float_t *sine;
    gomc_hal_float_t *cosine;
    gomc_hal_bit_t   *clock;
    gomc_hal_float_t *frequency;
    gomc_hal_float_t *amplitude;
    gomc_hal_float_t *offset;
    gomc_hal_bit_t   *reset;
} siggen_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    siggen_hal_t *hal;
    double index;
} inst_t;

static void calc_siggen(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    siggen_hal_t *h = inst->hal;
    double tmp1, tmp2;

    tmp1 = period * 0.000000001;
    tmp2 = *(h->frequency) * tmp1;

    if (tmp2 > 0.5) {
        *(h->frequency) = 0.5 / tmp1;
        tmp2 = 0.5;
    }

    if (*(h->reset))
        inst->index = 0.5;
    else
        inst->index += tmp2;

    if (inst->index >= 1.0)
        inst->index -= 1.0;

    /* square wave + clock */
    if (inst->index > 0.5) {
        tmp1 = 1.0;
        *(h->clock) = 1;
    } else {
        tmp1 = -1.0;
        *(h->clock) = 0;
    }
    *(h->square) = (tmp1 * *(h->amplitude)) + *(h->offset);

    /* sawtooth */
    tmp2 = (inst->index * 2.0) - 1.0;
    *(h->sawtooth) = (tmp2 * *(h->amplitude)) + *(h->offset);

    /* triangle */
    tmp2 *= 2.0;
    tmp2 = (tmp2 * tmp1) - 1.0;
    *(h->triangle) = (tmp2 * *(h->amplitude)) + *(h->offset);

    /* sine and cosine */
    tmp1 = inst->index * (2.0 * 3.1415927);
    *(h->sine) = (sin(tmp1) * *(h->amplitude)) + *(h->offset);
    *(h->cosine) = (cos(tmp1) * *(h->amplitude)) + *(h->offset);
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
    siggen_hal_t *h;
    int r;

    (void)argc; (void)argv;

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(siggen_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(siggen_hal_t));
    h = inst->hal;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->square, inst->comp_id,
                                "%s.square", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->sawtooth, inst->comp_id,
                                "%s.sawtooth", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->triangle, inst->comp_id,
                                "%s.triangle", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->sine, inst->comp_id,
                                "%s.sine", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->cosine, inst->comp_id,
                                "%s.cosine", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->clock, inst->comp_id,
                              "%s.clock", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->frequency, inst->comp_id,
                                "%s.frequency", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->amplitude, inst->comp_id,
                                "%s.amplitude", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->offset, inst->comp_id,
                                "%s.offset", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->reset, inst->comp_id,
                              "%s.reset", name);
    if (r != 0) goto err;

    /* defaults */
    *(h->frequency) = 1.0;
    *(h->amplitude) = 1.0;
    *(h->offset) = 0.0;
    inst->index = 0.0;

    r = env->hal->export_funct(env->hal->ctx, name, calc_siggen, inst, 1, 0,
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
