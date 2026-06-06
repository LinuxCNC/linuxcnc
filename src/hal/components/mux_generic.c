/*
 * mux_generic.c — cmod HAL component: generic configurable multiplexer.
 *
 * Selects one of N inputs and routes it to the output with optional
 * type conversion. Supports bit, float, s32, u32 types.
 *
 * Usage:
 *   load mux_generic config="bb4"      (4 bit inputs → bit output)
 *   load mux_generic config="fu8"      (8 float inputs → u32 output)
 *   load mux_generic config="s16"      (16 s32 inputs → s32 output)
 *
 * Config format: [in_type][out_type]<count>
 *   Types: b=bit, f=float, s=s32, u=u32
 *   If out_type is omitted, it defaults to in_type.
 *
 * Original author: Andy Pugh
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define MAX_SIZE 1024
#define EPS 2e-7
#define MAX_S32_VAL 0x7FFFFFFF
#define MAX_U32_VAL 0xFFFFFFFF

/* HAL data union — mirrors hal_data_u from hal.h */
typedef union {
    gomc_hal_bit_t   b;
    gomc_hal_float_t f;
    gomc_hal_s32_t   s;
    gomc_hal_u32_t   u;
} hal_data_u;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    hal_data_u **inputs;
    hal_data_u *output;
    gomc_hal_u32_t *sel_int;
    gomc_hal_bit_t **sel_bit;
    uint32_t selection;
    gomc_hal_u32_t *debounce;
    uint32_t timer;
    gomc_hal_bit_t *suppress;
    int in_type;
    int out_type;
    int size;
    int num_bits;
} inst_t;

static void write_fp(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int i = 0, s = 0;

    if (inst->num_bits > 0) {
        while (i < inst->num_bits) {
            s += (*(inst->sel_bit[i]) != 0) << i;
            i++;
        }
    }
    s += *(inst->sel_int);

    if (*(inst->suppress) && s == 0)
        return;
    if ((uint32_t)s != inst->selection && inst->timer < *(inst->debounce)) {
        inst->timer += period / 1000;
        return;
    }

    inst->selection = s;
    inst->timer = 0;

    if (s >= inst->size)
        s = inst->size - 1;

    switch (inst->in_type * 8 + inst->out_type) {
    case 012: /* BIT => FLOAT */
        inst->output->f = inst->inputs[s]->b ? 1.0 : 0.0;
        break;
    case 021: /* FLOAT => BIT */
        inst->output->b =
            (inst->inputs[s]->f > EPS || inst->inputs[s]->f < -EPS) ? 1 : 0;
        break;
    case 022: /* FLOAT => FLOAT */
        inst->output->f = inst->inputs[s]->f;
        break;
    case 023: /* FLOAT => S32 */
        if (inst->inputs[s]->f > MAX_S32_VAL)
            inst->output->s = MAX_S32_VAL;
        else if (inst->inputs[s]->f < -(double)MAX_S32_VAL)
            inst->output->s = -MAX_S32_VAL;
        else
            inst->output->s = (int32_t)inst->inputs[s]->f;
        break;
    case 024: /* FLOAT => U32 */
        if (inst->inputs[s]->f > (double)MAX_U32_VAL)
            inst->output->u = MAX_U32_VAL;
        else if (inst->inputs[s]->f < 0)
            inst->output->u = 0;
        else
            inst->output->u = (uint32_t)inst->inputs[s]->f;
        break;
    case 032: /* S32 => FLOAT */
        inst->output->f = (double)inst->inputs[s]->s;
        break;
    case 042: /* U32 => FLOAT */
        inst->output->f = (double)(unsigned int)inst->inputs[s]->u;
        break;
    }
}

static void write_nofp(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int i = 0, s = 0;

    if (inst->num_bits > 0) {
        while (i < inst->num_bits) {
            s += (*(inst->sel_bit[i]) != 0) << i;
            i++;
        }
    }
    s += *(inst->sel_int);

    if (*(inst->suppress) && s == 0)
        return;
    if ((uint32_t)s != inst->selection && inst->timer < *(inst->debounce)) {
        inst->timer += period / 1000;
        return;
    }

    inst->selection = s;
    inst->timer = 0;

    if (s >= inst->size)
        s = inst->size - 1;

    switch (inst->in_type * 8 + inst->out_type) {
    case 011: /* BIT => BIT */
        inst->output->b = inst->inputs[s]->b;
        break;
    case 013: /* BIT => S32 */
        inst->output->s = inst->inputs[s]->b;
        break;
    case 014: /* BIT => U32 */
        inst->output->u = inst->inputs[s]->b;
        break;
    case 031: /* S32 => BIT */
        inst->output->b = inst->inputs[s]->s == 0 ? 0 : 1;
        break;
    case 033: /* S32 => S32 */
        inst->output->s = inst->inputs[s]->s;
        break;
    case 034: /* S32 => U32 */
        inst->output->u = (inst->inputs[s]->s > 0) ? (uint32_t)inst->inputs[s]->s : 0;
        break;
    case 041: /* U32 => BIT */
        inst->output->b = inst->inputs[s]->u == 0 ? 0 : 1;
        break;
    case 043: /* U32 => S32 */
        inst->output->s =
            ((unsigned int)inst->inputs[s]->u > (unsigned int)MAX_S32_VAL) ?
                MAX_S32_VAL : (int32_t)inst->inputs[s]->u;
        break;
    case 044: /* U32 => U32 */
        inst->output->u = inst->inputs[s]->u;
        break;
    }
}

static void inst_destroy(cmod_t *self) {
    inst_t *inst = (inst_t *)self;
    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

static int parse_type(char c) {
    switch (c) {
    case 'b': case 'B': return GOMC_HAL_BIT;
    case 'f': case 'F': return GOMC_HAL_FLOAT;
    case 's': case 'S': return GOMC_HAL_S32;
    case 'u': case 'U': return GOMC_HAL_U32;
    default: return -1;
    }
}

static const char *type_name(int type) {
    switch (type) {
    case GOMC_HAL_BIT:   return "bit";
    case GOMC_HAL_FLOAT: return "float";
    case GOMC_HAL_S32:   return "s32";
    case GOMC_HAL_U32:   return "u32";
    default: return "invalid";
    }
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    inst_t *inst;
    int r, i, p;
    const char *config_arg = NULL;
    int in_type = -1, out_type = -1, size = 0;

    /* parse arguments */
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "config=", 7) == 0)
            config_arg = argv[i] + 7;
    }

    if (!config_arg || !*config_arg) {
        gomc_log_errorf(env->log, name,
                        "mux_generic requires config= argument (e.g. config=\"ff8\")");
        return -EINVAL;
    }

    /* parse config string */
    for (i = 0; config_arg[i]; i++) {
        int type = parse_type(config_arg[i]);
        if (type >= 0) {
            if (in_type == -1)
                in_type = type;
            else if (out_type == -1)
                out_type = type;
            else {
                gomc_log_errorf(env->log, name,
                                "too many type specifiers in config string");
                return -EINVAL;
            }
        } else if (config_arg[i] >= '0' && config_arg[i] <= '9') {
            size = size * 10 + (config_arg[i] - '0');
            if (size > MAX_SIZE) size = MAX_SIZE;
        } else {
            gomc_log_errorf(env->log, name,
                            "invalid character '%c' in config string", config_arg[i]);
            return -EINVAL;
        }
    }

    if (in_type == -1) {
        gomc_log_errorf(env->log, name, "no type specifiers in config string");
        return -EINVAL;
    }
    if (out_type == -1)
        out_type = in_type;
    if (size < 2) {
        gomc_log_errorf(env->log, name, "mux size must be >= 2");
        return -EINVAL;
    }

    /* determine num_bits (only if size is power of 2) */
    int num_bits = 0;
    {
        int s = size;
        int nb = 0;
        while (!(s & 1)) { s >>= 1; nb++; }
        if (s == 1) num_bits = nb;
    }

    /* allocate instance */
    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(inst_t));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->in_type = in_type;
    inst->out_type = out_type;
    inst->size = size;
    inst->num_bits = num_bits;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    /* export function */
    if (in_type == GOMC_HAL_FLOAT || out_type == GOMC_HAL_FLOAT) {
        r = env->hal->export_funct(env->hal->ctx, name, write_fp, inst, 1, 0,
                                   inst->comp_id);
    } else {
        r = env->hal->export_funct(env->hal->ctx, name, write_nofp, inst, 0, 0,
                                   inst->comp_id);
    }
    if (r != 0) goto err;

    /* create sel-bit pins if size is power of 2 */
    if (num_bits > 0) {
        inst->sel_bit = env->hal->malloc(env->hal->ctx,
                                         num_bits * sizeof(gomc_hal_bit_t *));
        if (!inst->sel_bit) goto err;
        for (p = 0; p < num_bits; p++) {
            r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &inst->sel_bit[p],
                                      inst->comp_id, "%s.sel-bit-%02d", name, p);
            if (r != 0) goto err;
        }
    }

    /* sel-int pin */
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IN, &inst->sel_int,
                              inst->comp_id, "%s.sel-int", name);
    if (r != 0) goto err;

    /* input pins */
    inst->inputs = env->hal->malloc(env->hal->ctx,
                                    size * sizeof(hal_data_u *));
    if (!inst->inputs) goto err;
    for (p = 0; p < size; p++) {
        r = gomc_hal_pin_newf(env->hal, in_type, GOMC_HAL_IN,
                              (void **)&inst->inputs[p], inst->comp_id,
                              "%s.in-%s-%02d", name, type_name(in_type), p);
        if (r != 0) goto err;
    }

    /* behaviour-modifier pins */
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &inst->suppress,
                              inst->comp_id, "%s.suppress-no-input", name);
    if (r != 0) goto err;

    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IN, &inst->debounce,
                              inst->comp_id, "%s.debounce-us", name);
    if (r != 0) goto err;

    /* output pin */
    r = gomc_hal_pin_newf(env->hal, out_type, GOMC_HAL_OUT,
                          (void **)&inst->output, inst->comp_id,
                          "%s.out-%s", name, type_name(out_type));
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
