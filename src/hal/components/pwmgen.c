/*
 * pwmgen.c — cmod HAL component: PWM/PDM generator.
 *
 * Single-channel PWM/PDM signal generator.
 *
 * Usage:
 *   load pwmgen output_type=0   (0=PWM only, 1=PWM+DIR, 2=UP/DOWN)
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

#define PWM_DISABLED 0
#define PWM_PURE 1
#define PWM_DITHER 2
#define PWM_PDM 3

#define PWM_PIN  0
#define DIR_PIN  1
#define UP_PIN   0
#define DOWN_PIN 1

typedef struct {
    gomc_hal_bit_t   *out[2];
    gomc_hal_bit_t   *enable;
    gomc_hal_float_t *value;
    gomc_hal_float_t *scale;
    gomc_hal_float_t *offset;
    gomc_hal_float_t *pwm_freq;
    gomc_hal_bit_t   *dither_pwm;
    gomc_hal_float_t *min_dc;
    gomc_hal_float_t *max_dc;
    gomc_hal_float_t *curr_dc;
} pwm_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    pwm_hal_t *hal;
    int output_type;
    unsigned char pwm_mode;
    unsigned char curr_output;
    unsigned char direction;
    long period;
    long high_time;
    long period_timer;
    long high_timer;
    double old_scale;
    double scale_recip;
    double old_pwm_freq;
    int periods;
    double periods_recip;
    long periodns;
} inst_t;

static void make_pulses(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    pwm_hal_t *h = inst->hal;

    inst->periodns = period;

    switch (inst->pwm_mode) {
    case PWM_PURE:
        if (inst->curr_output) {
            inst->high_timer += period;
            if (inst->high_timer >= inst->high_time)
                inst->curr_output = 0;
        }
        inst->period_timer += period;
        if (inst->period_timer >= inst->period) {
            inst->period_timer = 0;
            inst->high_timer = 0;
            if (inst->high_time > 0)
                inst->curr_output = 1;
        }
        break;
    case PWM_DITHER:
        if (inst->curr_output) {
            inst->high_timer -= period;
            if (inst->high_timer <= 0)
                inst->curr_output = 0;
        }
        inst->period_timer += period;
        if (inst->period_timer >= inst->period) {
            inst->period_timer -= inst->period;
            inst->high_timer += inst->high_time;
            if (inst->high_timer > 0)
                inst->curr_output = 1;
        }
        break;
    case PWM_PDM:
        inst->high_timer += inst->high_time;
        if (inst->curr_output)
            inst->high_timer -= period;
        inst->curr_output = (inst->high_timer > 0) ? 1 : 0;
        break;
    default:
        inst->curr_output = 0;
        inst->high_timer = 0;
        inst->period_timer = 0;
        break;
    }

    if (inst->output_type < 2) {
        *(h->out[PWM_PIN]) = inst->curr_output;
    } else {
        *(h->out[UP_PIN]) = inst->curr_output & ~inst->direction;
        *(h->out[DOWN_PIN]) = inst->curr_output & inst->direction;
    }
}

static void update_pwm(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    pwm_hal_t *h = inst->hal;
    unsigned char new_pwm_mode;
    double tmpdc, outdc;
    int high_periods;

    (void)period;

    if (*(h->max_dc) > 1.0) *(h->max_dc) = 1.0;
    if (*(h->min_dc) > *(h->max_dc)) *(h->min_dc) = *(h->max_dc);
    if (*(h->min_dc) < 0.0) *(h->min_dc) = 0.0;
    if (*(h->max_dc) < *(h->min_dc)) *(h->max_dc) = *(h->min_dc);

    if (*(h->scale) != inst->old_scale) {
        inst->old_scale = *(h->scale);
        if (*(h->scale) < 1e-20 && *(h->scale) > -1e-20)
            *(h->scale) = 1.0;
        inst->scale_recip = 1.0 / *(h->scale);
    }

    if (!*(h->enable))
        new_pwm_mode = PWM_DISABLED;
    else if (*(h->pwm_freq) == 0)
        new_pwm_mode = PWM_PDM;
    else if (*(h->dither_pwm))
        new_pwm_mode = PWM_DITHER;
    else
        new_pwm_mode = PWM_PURE;

    if (*(h->pwm_freq) != inst->old_pwm_freq)
        inst->pwm_mode = PWM_DISABLED;

    if (inst->pwm_mode != new_pwm_mode || inst->periodns != inst->period) {
        inst->pwm_mode = PWM_DISABLED;
        if (*(h->pwm_freq) <= 0.0) {
            *(h->pwm_freq) = 0.0;
            inst->period = inst->periodns;
        } else {
            if (*(h->pwm_freq) < 0.5) *(h->pwm_freq) = 0.5;
            else if (*(h->pwm_freq) > (1e9 / 2.0) / inst->periodns)
                *(h->pwm_freq) = (1e9 / 2.0) / inst->periodns;
            if (new_pwm_mode == PWM_PURE) {
                inst->periods = ((1e9 / *(h->pwm_freq)) / inst->periodns) + 0.5;
                inst->periods_recip = 1.0 / inst->periods;
                inst->period = inst->periods * inst->periodns;
                *(h->pwm_freq) = 1.0e9 / inst->period;
            } else {
                inst->period = 1.0e9 / *(h->pwm_freq);
            }
        }
        inst->old_pwm_freq = *(h->pwm_freq);
    }

    tmpdc = *(h->value) * inst->scale_recip + *(h->offset);
    if (inst->output_type == 0 && tmpdc < 0.0)
        tmpdc = 0.0;

    if (tmpdc >= 0.0) {
        if (tmpdc > *(h->max_dc)) tmpdc = *(h->max_dc);
        else if (tmpdc < *(h->min_dc)) tmpdc = *(h->min_dc);
        inst->direction = 0;
        outdc = tmpdc;
    } else {
        if (tmpdc < -*(h->max_dc)) tmpdc = -*(h->max_dc);
        else if (tmpdc > -*(h->min_dc)) tmpdc = -*(h->min_dc);
        inst->direction = 1;
        outdc = -tmpdc;
    }

    if (new_pwm_mode == PWM_PURE) {
        high_periods = (inst->periods * outdc) + 0.5;
        inst->high_time = high_periods * inst->periodns;
        *(h->curr_dc) = (tmpdc >= 0) ?
            high_periods * inst->periods_recip :
            -high_periods * inst->periods_recip;
    } else {
        inst->high_time = (inst->period * outdc) + 0.5;
        *(h->curr_dc) = tmpdc;
    }

    if (inst->output_type == 1)
        *(h->out[DIR_PIN]) = inst->direction;

    inst->pwm_mode = new_pwm_mode;
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
    pwm_hal_t *h;
    int r, i;
    int output_type = 0;
    char buf[GOMC_HAL_NAME_LEN + 1];

    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "output_type=", 12) == 0)
            output_type = atoi(argv[i] + 12);
    }
    if (output_type < 0 || output_type > 2) {
        gomc_log_errorf(env->log, name, "output_type=%d out of range (0..2)", output_type);
        return -EINVAL;
    }

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->output_type = output_type;
    inst->periodns = -1;
    inst->period = 50000;
    inst->old_scale = 0.0;
    inst->scale_recip = 1.0;
    inst->old_pwm_freq = -1;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(pwm_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(pwm_hal_t));
    h = inst->hal;

    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->scale, inst->comp_id,
                                "%s.scale", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->offset, inst->comp_id,
                                "%s.offset", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &h->dither_pwm, inst->comp_id,
                              "%s.dither-pwm", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->pwm_freq, inst->comp_id,
                                "%s.pwm-freq", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->min_dc, inst->comp_id,
                                "%s.min-dc", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->max_dc, inst->comp_id,
                                "%s.max-dc", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->curr_dc, inst->comp_id,
                                "%s.curr-dc", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->enable, inst->comp_id,
                              "%s.enable", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->value, inst->comp_id,
                                "%s.value", name);
    if (r != 0) goto err;

    if (output_type == 2) {
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->out[UP_PIN], inst->comp_id,
                                  "%s.up", name);
        if (r != 0) goto err;
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->out[DOWN_PIN], inst->comp_id,
                                  "%s.down", name);
        if (r != 0) goto err;
    } else {
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->out[PWM_PIN], inst->comp_id,
                                  "%s.pwm", name);
        if (r != 0) goto err;
        if (output_type == 1) {
            r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->out[DIR_PIN], inst->comp_id,
                                      "%s.dir", name);
            if (r != 0) goto err;
        }
    }

    /* defaults */
    *(h->scale) = 1.0;
    *(h->offset) = 0.0;
    *(h->min_dc) = 0.0;
    *(h->max_dc) = 1.0;

    snprintf(buf, sizeof(buf), "%s.make-pulses", name);
    r = env->hal->export_funct(env->hal->ctx, buf, make_pulses, inst, 0, 0,
                               inst->comp_id);
    if (r != 0) goto err;

    snprintf(buf, sizeof(buf), "%s.update", name);
    r = env->hal->export_funct(env->hal->ctx, buf, update_pwm, inst, 1, 0,
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
