/*
 * sim_encoder.c — cmod HAL component: simulated quadrature encoder.
 *
 * Generates A, B, Z quadrature signals at a commanded speed.
 *
 * Usage:
 *   load sim_encoder
 *
 * Original author: John Kasunich
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>

typedef struct {
    gomc_hal_bit_t   *phaseA;
    gomc_hal_bit_t   *phaseB;
    gomc_hal_bit_t   *phaseZ;
    gomc_hal_u32_t   *ppr;
    gomc_hal_float_t *scale;
    gomc_hal_float_t *speed;
    gomc_hal_s32_t   *rawcounts;
} se_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    se_hal_t *hal;
    /* internal state */
    signed long addval;
    unsigned long accum;
    signed char state;
    long cycle;
    double old_scale;
    double scale_mult;
    long periodns;
    long old_periodns;
    double periodfp;
    double freqscale;
    double maxf;
} inst_t;

static void make_pulses(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    se_hal_t *h = inst->hal;
    int overunder, dir;

    inst->periodns = period;

    overunder = inst->accum >> 31;
    inst->accum += inst->addval;
    overunder ^= inst->accum >> 31;

    if (overunder) {
        dir = inst->addval >> 31;
        if (dir) {
            (*h->rawcounts)--;
            if (--(inst->state) < 0) {
                inst->state = 3;
                if (--(inst->cycle) < 0)
                    inst->cycle += *(h->ppr);
            }
        } else {
            (*h->rawcounts)++;
            if (++(inst->state) > 3) {
                inst->state = 0;
                if (++(inst->cycle) >= (long)*(h->ppr))
                    inst->cycle -= *(h->ppr);
            }
        }
    }

    switch (inst->state) {
    case 0: *(h->phaseA) = 1; *(h->phaseB) = 0; break;
    case 1: *(h->phaseA) = 1; *(h->phaseB) = 1; break;
    case 2: *(h->phaseA) = 0; *(h->phaseB) = 1; break;
    case 3: *(h->phaseA) = 0; *(h->phaseB) = 0; break;
    default: inst->state = 0; break;
    }

    *(h->phaseZ) = (inst->state == 0 && inst->cycle == 0) ? 1 : 0;
}

static void update_speed(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    se_hal_t *h = inst->hal;
    double rev_sec, freq;

    (void)period;

    if (inst->periodns != inst->old_periodns) {
        inst->periodfp = inst->periodns * 0.000000001;
        inst->maxf = 1.0 / inst->periodfp;
        inst->freqscale = ((1L << 30) * 2.0) / inst->maxf;
        inst->old_periodns = inst->periodns;
    }

    if (*(h->scale) != inst->old_scale) {
        inst->old_scale = *(h->scale);
        if (*(h->scale) < 1e-20 && *(h->scale) > -1e-20)
            *(h->scale) = 1.0;
        inst->scale_mult = 1.0 / *(h->scale);
    }

    rev_sec = *(h->speed) * inst->scale_mult;
    freq = rev_sec * (*(h->ppr)) * 4.0;

    if (freq > inst->maxf)
        freq = inst->maxf;
    else if (freq < -inst->maxf)
        freq = -inst->maxf;

    inst->addval = freq * inst->freqscale;
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
    se_hal_t *h;
    int r;
    char buf[GOMC_HAL_NAME_LEN + 1];

    (void)argc; (void)argv;

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);

    /* precompute timing constants with default period */
    inst->periodns = 50000;
    inst->periodfp = inst->periodns * 0.000000001;
    inst->maxf = 1.0 / inst->periodfp;
    inst->freqscale = ((1L << 30) * 2.0) / inst->maxf;
    inst->old_periodns = inst->periodns;
    inst->old_scale = 0.0;
    inst->scale_mult = 1.0;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(se_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(se_hal_t));
    h = inst->hal;

    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &h->ppr, inst->comp_id,
                              "%s.ppr", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->scale, inst->comp_id,
                                "%s.scale", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->speed, inst->comp_id,
                                "%s.speed", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->phaseA, inst->comp_id,
                              "%s.phase-A", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->phaseB, inst->comp_id,
                              "%s.phase-B", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->phaseZ, inst->comp_id,
                              "%s.phase-Z", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IN, &h->rawcounts, inst->comp_id,
                              "%s.rawcounts", name);
    if (r != 0) goto err;

    *(h->ppr) = 100;
    *(h->scale) = 1.0;

    snprintf(buf, sizeof(buf), "%s.make-pulses", name);
    r = env->hal->export_funct(env->hal->ctx, buf, make_pulses, inst, 0, 0,
                               inst->comp_id);
    if (r != 0) goto err;

    snprintf(buf, sizeof(buf), "%s.update-speed", name);
    r = env->hal->export_funct(env->hal->ctx, buf, update_speed, inst, 1, 0,
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
