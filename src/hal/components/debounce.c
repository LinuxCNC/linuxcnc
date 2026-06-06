/*
 * debounce.c — cmod HAL component: input debounce filter.
 *
 * Single debounce group with N filters. All filters share the same
 * delay (sample count threshold).
 *
 * Usage:
 *   load debounce count=4
 *
 * Original author: John Kasunich
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define MAX_FILTERS 50

typedef struct {
    gomc_hal_bit_t *in;
    gomc_hal_bit_t *out;
    int32_t state;
} filter_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    int count;
    int32_t delay;
    filter_t *filters;
} inst_t;

static void debounce_funct(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    filter_t *f;
    int n;

    (void)period;

    if (inst->delay < 0)
        inst->delay = 1;

    for (n = 0; n < inst->count; n++) {
        f = &inst->filters[n];
        if (*(f->in)) {
            if (f->state < inst->delay)
                f->state++;
            else
                *(f->out) = 1;
        } else {
            if (f->state > 0)
                f->state--;
            else
                *(f->out) = 0;
        }
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
    int r, i;
    int count = 0;

    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "count=", 6) == 0)
            count = atoi(argv[i] + 6);
    }

    if (count < 1 || count > MAX_FILTERS) {
        gomc_log_errorf(env->log, name,
                        "count=%d out of range (1..%d)", count, MAX_FILTERS);
        return -EINVAL;
    }

    size_t sz = sizeof(inst_t) + sizeof(filter_t) * count;
    inst = env->rtapi->calloc(env->rtapi->ctx, sz);
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->count = count;
    inst->delay = 5;
    inst->filters = (filter_t *)((char *)inst + sizeof(inst_t));

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    /* export delay param */
    r = gomc_hal_param_s32_newf(env->hal, GOMC_HAL_RW, &inst->delay,
                                inst->comp_id, "%s.delay", name);
    if (r != 0) goto err;

    /* export per-filter pins */
    for (i = 0; i < count; i++) {
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &inst->filters[i].in,
                                  inst->comp_id, "%s.%d.in", name, i);
        if (r != 0) goto err;
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &inst->filters[i].out,
                                  inst->comp_id, "%s.%d.out", name, i);
        if (r != 0) goto err;
        inst->filters[i].state = 0;
    }

    r = env->hal->export_funct(env->hal->ctx, name, debounce_funct, inst, 0, 0,
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
