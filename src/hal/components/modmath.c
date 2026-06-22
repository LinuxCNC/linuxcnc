/*
 * modmath.c — cmod HAL component: modulo direction finder.
 *
 * Computes the shortest direction (up/down) from actual to desired
 * in a modular number space, with optional wrap-around.
 *
 * Usage:
 *   load modmath
 *
 * Original author: Stephen Wille Padnos (swpadnos AT sourceforge DOT net)
 *       Based on a work by John Kasunich
 * Copyright (c) 2006 Stephen Wille Padnos
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>

typedef struct {
    gomc_hal_bit_t *up;
    gomc_hal_bit_t *down;
    gomc_hal_bit_t *on_target;
    gomc_hal_s32_t *actual;
    gomc_hal_s32_t *desired;
    gomc_hal_s32_t *max_num;
    gomc_hal_s32_t *min_num;
    gomc_hal_bit_t *wrap;
} inst_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    inst_hal_t *hal;
} inst_t;

static void mod_dir_funct(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    inst_hal_t *h = inst->hal;
    int32_t range, act, des, to_go;

    (void)period;

    range = *(h->max_num) - *(h->min_num) + 1;
    act = *(h->actual);
    if (act > *(h->max_num) || act < *(h->min_num))
        act = *(h->min_num) + ((act - *(h->min_num)) % range);
    des = *(h->desired);
    if (des > *(h->max_num) || des < *(h->min_num))
        des = *(h->min_num) + ((des - *(h->min_num)) % range);

    to_go = des - act;

    if (*(h->wrap) && to_go > range / 2)
        to_go -= range;
    if (*(h->wrap) && to_go < -range / 2)
        to_go += range;

    if (to_go == 0) {
        *(h->up) = 0;
        *(h->down) = 0;
        *(h->on_target) = 1;
    } else if (to_go > 0) {
        *(h->down) = 0;
        *(h->on_target) = 0;
        *(h->up) = 1;
    } else {
        *(h->up) = 0;
        *(h->on_target) = 0;
        *(h->down) = 1;
    }
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
    inst_hal_t *h;
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

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(inst_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(inst_hal_t));
    h = inst->hal;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->up, inst->comp_id,
                              "%s.up", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->down, inst->comp_id,
                              "%s.down", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->on_target, inst->comp_id,
                              "%s.on-target", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IN, &h->actual, inst->comp_id,
                              "%s.actual", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IN, &h->desired, inst->comp_id,
                              "%s.desired", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IO, &h->min_num, inst->comp_id,
                              "%s.min-num", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IO, &h->max_num, inst->comp_id,
                              "%s.max-num", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &h->wrap, inst->comp_id,
                              "%s.wrap", name);
    if (r != 0) goto err;

    /* defaults */
    *(h->up) = 0;
    *(h->down) = 0;
    *(h->on_target) = 1;
    *(h->min_num) = 0;
    *(h->max_num) = 15;
    *(h->actual) = 0;
    *(h->desired) = 0;
    *(h->wrap) = 1;

    r = env->hal->export_funct(env->hal->ctx, name, mod_dir_funct, inst, 1, 0,
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
