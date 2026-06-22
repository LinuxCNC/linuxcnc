/*
 * weighted_sum.c — cmod HAL component: weighted summer.
 *
 * Single weighted summer with N bit inputs. Each bit has a weight;
 * the output is the sum of weights for which the input bit is true.
 *
 * Usage:
 *   load weighted_sum num_bits=8
 *
 * Original author: Stephen Wille Padnos (swpadnos AT sourceforge DOT net)
 *       Based on a work by John Kasunich
 * Copyright (c) 2006 Stephen Wille Padnos
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define MAX_BITS 16

typedef struct {
    gomc_hal_bit_t *bit;
    gomc_hal_s32_t *weight;
} wsum_bit_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    int num_bits;
    gomc_hal_s32_t *sum;
    gomc_hal_s32_t *offset;
    gomc_hal_bit_t *hold;
    wsum_bit_t bits[MAX_BITS];
} inst_t;

static void process_wsum(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int32_t running_total;
    int i;

    (void)period;

    if (*(inst->hold)) return;

    running_total = *(inst->offset);
    for (i = 0; i < inst->num_bits; i++) {
        if (*(inst->bits[i].bit))
            running_total += *(inst->bits[i].weight);
    }
    *(inst->sum) = running_total;
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
    int r, i, w;
    int num_bits = 0;

    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "num_bits=", 9) == 0)
            num_bits = atoi(argv[i] + 9);
    }

    if (num_bits < 1 || num_bits > MAX_BITS) {
        gomc_log_errorf(env->log, name,
                        "num_bits=%d out of range (1..%d)", num_bits, MAX_BITS);
        return -EINVAL;
    }

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->num_bits = num_bits;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    /* output sum pin */
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &inst->sum, inst->comp_id,
                              "%s.sum", name);
    if (r != 0) goto err;

    /* offset pin */
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IO, &inst->offset, inst->comp_id,
                              "%s.offset", name);
    if (r != 0) goto err;

    /* hold pin */
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &inst->hold, inst->comp_id,
                              "%s.hold", name);
    if (r != 0) goto err;

    /* per-bit pins */
    w = 1;
    for (i = 0; i < num_bits; i++) {
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &inst->bits[i].bit,
                                  inst->comp_id, "%s.bit.%d.in", name, i);
        if (r != 0) goto err;
        r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IO, &inst->bits[i].weight,
                                  inst->comp_id, "%s.bit.%d.weight", name, i);
        if (r != 0) goto err;
        *(inst->bits[i].weight) = w;
        w <<= 1;
    }

    *(inst->offset) = 0;
    *(inst->sum) = 0;

    r = env->hal->export_funct(env->hal->ctx, name, process_wsum, inst, 1, 0,
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
