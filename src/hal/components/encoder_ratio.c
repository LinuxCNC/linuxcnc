/*
 * encoder_ratio.c — cmod HAL component: encoder ratio (electronic gear).
 *
 * Counts encoder pulses from master and slave axes and produces an error
 * value for PID to synchronize them at a specified gear ratio.
 *
 * Usage:
 *   load encoder_ratio
 *
 * Original author: John Kasunich
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>

/* Quadrature decode state machine bitmasks */
#define SM_PHASE_A_MASK 0x01
#define SM_PHASE_B_MASK 0x02
#define SM_LOOKUP_MASK  0x0F
#define SM_CNT_UP_MASK  0x40
#define SM_CNT_DN_MASK  0x80

static const unsigned char lut[16] = {
    0x00, 0x44, 0x88, 0x0C, 0x80, 0x04, 0x08, 0x4C,
    0x40, 0x04, 0x08, 0x8C, 0x00, 0x84, 0x48, 0x0C
};

typedef struct {
    gomc_hal_bit_t   *master_A;
    gomc_hal_bit_t   *master_B;
    gomc_hal_bit_t   *slave_A;
    gomc_hal_bit_t   *slave_B;
    gomc_hal_bit_t   *enable;
    gomc_hal_float_t *error;
    gomc_hal_u32_t   *master_ppr;
    gomc_hal_u32_t   *slave_ppr;
    gomc_hal_u32_t   *master_teeth;
    gomc_hal_u32_t   *slave_teeth;
} er_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    er_hal_t *hal;
    unsigned char master_state;
    unsigned char slave_state;
    int raw_error;
    int master_increment;
    int slave_increment;
    double output_scale;
} inst_t;

static void sample(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    er_hal_t *h = inst->hal;
    unsigned char state;

    (void)period;

    /* master encoder */
    state = inst->master_state;
    if (*(h->master_A)) state |= SM_PHASE_A_MASK;
    if (*(h->master_B)) state |= SM_PHASE_B_MASK;
    state = lut[state & SM_LOOKUP_MASK];
    if (*(h->enable)) {
        if (state & SM_CNT_UP_MASK)
            inst->raw_error -= inst->master_increment;
        else if (state & SM_CNT_DN_MASK)
            inst->raw_error += inst->master_increment;
    }
    inst->master_state = state;

    /* slave encoder */
    state = inst->slave_state;
    if (*(h->slave_A)) state |= SM_PHASE_A_MASK;
    if (*(h->slave_B)) state |= SM_PHASE_B_MASK;
    state = lut[state & SM_LOOKUP_MASK];
    if (state & SM_CNT_UP_MASK)
        inst->raw_error += inst->slave_increment;
    else if (state & SM_CNT_DN_MASK)
        inst->raw_error -= inst->slave_increment;
    inst->slave_state = state;
}

static void update(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    er_hal_t *h = inst->hal;

    (void)period;

    if (inst->output_scale > 0)
        *(h->error) = inst->raw_error / inst->output_scale;

    inst->master_increment = *(h->master_teeth) * *(h->slave_ppr);
    inst->slave_increment = *(h->slave_teeth) * *(h->master_ppr);
    inst->output_scale = (double)*(h->master_ppr) * *(h->slave_ppr) * *(h->slave_teeth);
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
    er_hal_t *h;
    int r;
    char buf[GOMC_HAL_NAME_LEN + 1];

    (void)argc; (void)argv;

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->output_scale = 1.0;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(er_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(er_hal_t));
    h = inst->hal;

    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->master_A, inst->comp_id,
                              "%s.master-A", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->master_B, inst->comp_id,
                              "%s.master-B", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->slave_A, inst->comp_id,
                              "%s.slave-A", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->slave_B, inst->comp_id,
                              "%s.slave-B", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->enable, inst->comp_id,
                              "%s.enable", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->error, inst->comp_id,
                                "%s.error", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &h->master_ppr, inst->comp_id,
                              "%s.master-ppr", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &h->slave_ppr, inst->comp_id,
                              "%s.slave-ppr", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &h->master_teeth, inst->comp_id,
                              "%s.master-teeth", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &h->slave_teeth, inst->comp_id,
                              "%s.slave-teeth", name);
    if (r != 0) goto err;

    /* export functions */
    snprintf(buf, sizeof(buf), "%s.sample", name);
    r = env->hal->export_funct(env->hal->ctx, buf, sample, inst, 0, 0, inst->comp_id);
    if (r != 0) goto err;

    snprintf(buf, sizeof(buf), "%s.update", name);
    r = env->hal->export_funct(env->hal->ctx, buf, update, inst, 1, 0, inst->comp_id);
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
