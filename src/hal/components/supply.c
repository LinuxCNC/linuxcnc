/*
 * supply.c — cmod HAL component supplying preset pin values.
 *
 * Provides HAL pins preset to useful values like TRUE and 1.0.
 * Each load creates one supply instance.
 *
 * Usage:
 *   load supply
 *
 * Original author: Matt Shaver
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>

typedef struct {
    gomc_hal_bit_t   *q;
    gomc_hal_bit_t   *_q;
    gomc_hal_float_t *variable;
    gomc_hal_float_t *_variable;
    gomc_hal_bit_t   *d;
    gomc_hal_float_t *value;
} inst_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    inst_hal_t *hal;
} inst_t;

static void update_supply(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    inst_hal_t *h = inst->hal;

    *(h->q) = *(h->d);
    *(h->_q) = !(*(h->d));
    *(h->variable) = *(h->value);
    *(h->_variable) = *(h->value) * -1.0;
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

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &inst->hal->q, inst->comp_id,
                              "%s.q", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &inst->hal->_q, inst->comp_id,
                              "%s._q", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &inst->hal->variable, inst->comp_id,
                                "%s.variable", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &inst->hal->_variable, inst->comp_id,
                                "%s._variable", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &inst->hal->d, inst->comp_id,
                              "%s.d", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &inst->hal->value, inst->comp_id,
                                "%s.value", name);
    if (r != 0) goto err;

    /* init pin values */
    *(inst->hal->q) = 0;
    *(inst->hal->_q) = 1;
    *(inst->hal->variable) = 0.0;
    *(inst->hal->_variable) = 0.0;
    *(inst->hal->d) = 0;
    *(inst->hal->value) = 0.0;

    r = env->hal->export_funct(env->hal->ctx, name, update_supply, inst, 1, 0,
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
