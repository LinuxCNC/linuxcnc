/*
 * stepgen.c — cmod HAL component: software step pulse generator.
 *
 * Single-channel step pulse generator supporting step/dir, up/down,
 * and state-pattern stepping types 0-14.
 *
 * Usage:
 *   load stepgen step_type=0 ctrl_type=p
 *   load stepgen step_type=2 ctrl_type=v
 *
 * Original author: John Kasunich
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#define MAX_CYCLE 18
#define PICKOFF   28

/* lookup tables for stepping types 2 and higher - phase A is the LSB */
static const unsigned char master_lut[][MAX_CYCLE] = {
    {1, 3, 2, 0, 0, 0, 0, 0, 0, 0},       /* type 2: Quadrature */
    {1, 2, 4, 0, 0, 0, 0, 0, 0, 0},       /* type 3: Three Wire */
    {1, 3, 2, 6, 4, 5, 0, 0, 0, 0},       /* type 4: Three Wire Half Step */
    {1, 2, 4, 8, 0, 0, 0, 0, 0, 0},       /* 5: Unipolar Full Step 1 */
    {3, 6, 12, 9, 0, 0, 0, 0, 0, 0},      /* 6: Unipolar Full Step 2 */
    {1, 7, 14, 8, 0, 0, 0, 0, 0, 0},      /* 7: Bipolar Full Step 1 */
    {5, 6, 10, 9, 0, 0, 0, 0, 0, 0},      /* 8: Bipolar Full Step 2 */
    {1, 3, 2, 6, 4, 12, 8, 9, 0, 0},      /* 9: Unipolar Half Step */
    {1, 5, 7, 6, 14, 10, 8, 9, 0, 0},     /* 10: Bipolar Half Step */
    {1, 2, 4, 8, 16, 0, 0, 0, 0, 0},      /* 11: Five Wire Unipolar */
    {3, 6, 12, 24, 17, 0, 0, 0, 0, 0},    /* 12: Five Wire Wave */
    {1, 3, 2, 6, 4, 12, 8, 24, 16, 17},   /* 13: Five Wire Uni Half */
    {3, 7, 6, 14, 12, 28, 24, 25, 17, 19},/* 14: Five Wire Wave Half */
};

static const unsigned char cycle_len_lut[] =
    { 4, 3, 6, 4, 4, 4, 4, 8, 8, 5, 5, 10, 10 };

static const unsigned char num_phases_lut[] =
    { 2, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5 };

#define MAX_STEP_TYPE 14
#define STEP_PIN  0
#define DIR_PIN   1
#define UP_PIN    0
#define DOWN_PIN  1

typedef struct {
    gomc_hal_bit_t   *enable;
    gomc_hal_bit_t   *phase[5];
    gomc_hal_s32_t   *count;
    gomc_hal_float_t *pos_cmd;
    gomc_hal_float_t *vel_cmd;
    gomc_hal_float_t *pos_fb;
} sg_pins_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    sg_pins_t *pins;
    /* stepping configuration */
    int step_type;
    int pos_mode;
    int num_phases;
    int cycle_max;
    const unsigned char *lut;
    /* DDS core (accessed by make_pulses) */
    unsigned int timer1;
    unsigned int timer2;
    unsigned int timer3;
    int hold_dds;
    long addval;
    volatile long long accum;
    int32_t rawcount;
    int curr_dir;
    int state;
    long target_addval;
    long deltalim;
    /* timing parameters (HAL params) */
    uint32_t step_len;
    uint32_t step_space;
    uint32_t dir_hold_dly;
    uint32_t dir_setup;
    uint32_t old_step_len;
    uint32_t old_step_space;
    uint32_t old_dir_hold_dly;
    uint32_t old_dir_setup;
    /* scaling */
    double pos_scale;
    double old_scale;
    double scale_recip;
    double maxvel;
    double maxaccel;
    double freq;
    double old_pos_cmd;
    int printed_error;
    /* timing state */
    long periodns;
    long old_periodns;
    double periodfp;
    double freqscale;
    double accelscale;
    long old_dtns;
    double dt;
    double recip_dt;
} inst_t;

static unsigned long ulceil(unsigned long value, unsigned long increment) {
    if (value == 0) return 0;
    return increment * (((value - 1) / increment) + 1);
}

static void make_pulses(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    long old_addval, target_addval, new_addval, step_now;
    int p;
    unsigned char outbits;

    inst->periodns = period;

    /* decrement timers */
    if (inst->timer1 > 0) {
        inst->timer1 = (inst->timer1 > (unsigned int)period) ? inst->timer1 - period : 0;
    }
    if (inst->timer2 > 0) {
        inst->timer2 = (inst->timer2 > (unsigned int)period) ? inst->timer2 - period : 0;
    }
    if (inst->timer3 > 0) {
        if (inst->timer3 > (unsigned int)period)
            inst->timer3 -= period;
        else {
            inst->timer3 = 0;
            inst->hold_dds = 0;
        }
    }

    if (!inst->hold_dds && *(inst->pins->enable)) {
        old_addval = inst->addval;
        target_addval = inst->target_addval;
        if (inst->deltalim != 0) {
            if (target_addval > (old_addval + inst->deltalim))
                new_addval = old_addval + inst->deltalim;
            else if (target_addval < (old_addval - inst->deltalim))
                new_addval = old_addval - inst->deltalim;
            else
                new_addval = target_addval;
        } else {
            new_addval = target_addval;
        }
        inst->addval = new_addval;
        if (((new_addval >= 0) && (old_addval < 0)) ||
            ((new_addval < 0) && (old_addval >= 0))) {
            if (inst->timer3 != 0) inst->hold_dds = 1;
        }
    }

    /* update DDS */
    if (!inst->hold_dds && *(inst->pins->enable)) {
        step_now = inst->accum;
        inst->accum += inst->addval;
        step_now ^= inst->accum;
        step_now &= (1L << PICKOFF);
        inst->rawcount = inst->accum >> PICKOFF;
    } else {
        step_now = 0;
    }

    if (inst->timer2 == 0) {
        if (inst->addval > 0) inst->curr_dir = 1;
        else if (inst->addval < 0) inst->curr_dir = -1;
    }

    if (step_now) {
        inst->timer1 = inst->step_len;
        inst->timer2 = inst->timer1 + inst->dir_hold_dly;
        inst->timer3 = inst->timer2 + inst->dir_setup;
        if (inst->step_type >= 2) {
            inst->state += inst->curr_dir;
            if (inst->state < 0) inst->state = inst->cycle_max;
            else if (inst->state > inst->cycle_max) inst->state = 0;
        }
    }

    /* generate output */
    if (inst->step_type == 0) {
        *(inst->pins->phase[STEP_PIN]) = (inst->timer1 != 0);
        *(inst->pins->phase[DIR_PIN]) = (inst->curr_dir < 0);
    } else if (inst->step_type == 1) {
        if (inst->timer1 != 0) {
            *(inst->pins->phase[UP_PIN]) = (inst->curr_dir >= 0);
            *(inst->pins->phase[DOWN_PIN]) = (inst->curr_dir < 0);
        } else {
            *(inst->pins->phase[UP_PIN]) = 0;
            *(inst->pins->phase[DOWN_PIN]) = 0;
        }
    } else {
        outbits = inst->lut[inst->state];
        for (p = 0; p < inst->num_phases; p++) {
            *(inst->pins->phase[p]) = outbits & 1;
            outbits >>= 1;
        }
    }
}

static void capture_position(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    long long int accum_a, accum_b;
    (void)period;

    do {
        accum_a = inst->accum;
        accum_b = inst->accum;
    } while (accum_a != accum_b);

    *(inst->pins->count) = accum_a >> PICKOFF;

    if (inst->pos_scale != inst->old_scale) {
        inst->old_scale = inst->pos_scale;
        if (inst->pos_scale < 1e-20 && inst->pos_scale > -1e-20)
            inst->pos_scale = 1.0;
        inst->scale_recip = (1.0 / (1L << PICKOFF)) / inst->pos_scale;
    }

    *(inst->pins->pos_fb) = (double)(accum_a - (1 << (PICKOFF - 1))) * inst->scale_recip;
}

static void update_freq(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int newperiod = 0;
    long min_step_period;
    long long int accum_a, accum_b;
    double pos_cmd, vel_cmd, curr_pos, curr_vel, avg_v, max_freq, max_ac;
    double match_ac, match_time, est_out, est_cmd, est_err, dp, dv, new_vel;
    double desired_freq;

    if (inst->periodns != inst->old_periodns) {
        inst->old_periodns = inst->periodns;
        inst->periodfp = inst->periodns * 0.000000001;
        inst->freqscale = (1L << PICKOFF) * inst->periodfp;
        inst->accelscale = inst->freqscale * inst->periodfp;
        newperiod = 1;
    }
    if (period != inst->old_dtns) {
        inst->old_dtns = period;
        inst->dt = period * 0.000000001;
        inst->recip_dt = 1.0 / inst->dt;
    }

    /* check for scale change */
    if (inst->pos_scale != inst->old_scale) {
        inst->old_scale = inst->pos_scale;
        if (inst->pos_scale < 1e-20 && inst->pos_scale > -1e-20)
            inst->pos_scale = 1.0;
        inst->scale_recip = (1.0 / (1L << PICKOFF)) / inst->pos_scale;
    }

    if (newperiod) {
        inst->old_step_len = ~0;
        inst->old_step_space = ~0;
        inst->old_dir_hold_dly = ~0;
        inst->old_dir_setup = ~0;
    }

    /* process timing parameters */
    if (inst->step_len != inst->old_step_len) {
        if (inst->step_len == 0) inst->step_len = 1;
        inst->old_step_len = ulceil(inst->step_len, inst->periodns);
        inst->step_len = inst->old_step_len;
    }
    if (inst->step_space != inst->old_step_space) {
        inst->old_step_space = ulceil(inst->step_space, inst->periodns);
        inst->step_space = inst->old_step_space;
    }
    if (inst->dir_setup != inst->old_dir_setup) {
        inst->old_dir_setup = ulceil(inst->dir_setup, inst->periodns);
        inst->dir_setup = inst->old_dir_setup;
    }
    if (inst->dir_hold_dly != inst->old_dir_hold_dly) {
        if ((inst->dir_hold_dly + inst->dir_setup) == 0 && inst->step_type < 2)
            inst->dir_hold_dly = 1;
        inst->old_dir_hold_dly = ulceil(inst->dir_hold_dly, inst->periodns);
        inst->dir_hold_dly = inst->old_dir_hold_dly;
    }

    /* disabled? */
    if (!*(inst->pins->enable)) {
        if (inst->pos_mode)
            inst->old_pos_cmd = *(inst->pins->pos_cmd) * inst->pos_scale;
        inst->freq = 0;
        inst->addval = 0;
        inst->target_addval = 0;
        return;
    }

    /* calculate frequency limit */
    min_step_period = inst->step_len + inst->step_space;
    max_freq = 1.0 / (min_step_period * 0.000000001);

    if (inst->maxvel <= 0.0) {
        inst->maxvel = 0.0;
    } else {
        desired_freq = inst->maxvel * fabs(inst->pos_scale);
        if (desired_freq > max_freq) {
            if (!inst->printed_error) {
                inst->printed_error = 1;
            }
            inst->maxvel = max_freq / fabs(inst->pos_scale);
        } else {
            max_freq = inst->maxvel * fabs(inst->pos_scale);
        }
    }

    max_ac = max_freq * inst->recip_dt;
    if (inst->maxaccel <= 0.0) {
        inst->maxaccel = 0.0;
    } else {
        if ((inst->maxaccel * fabs(inst->pos_scale)) > max_ac)
            inst->maxaccel = max_ac / fabs(inst->pos_scale);
        else
            max_ac = inst->maxaccel * fabs(inst->pos_scale);
    }

    if (inst->pos_mode) {
        pos_cmd = *(inst->pins->pos_cmd) * inst->pos_scale;
        vel_cmd = (pos_cmd - inst->old_pos_cmd) * inst->recip_dt;
        inst->old_pos_cmd = pos_cmd;

        do {
            accum_a = inst->accum;
            accum_b = inst->accum;
        } while (accum_a != accum_b);

        curr_pos = (accum_a - (1 << (PICKOFF - 1))) * (1.0 / (1L << PICKOFF));
        curr_vel = inst->freq;

        if (vel_cmd > curr_vel) match_ac = max_ac;
        else match_ac = -max_ac;

        match_time = (vel_cmd - curr_vel) / match_ac;
        avg_v = (vel_cmd + curr_vel) * 0.5;
        est_out = curr_pos + avg_v * match_time;
        est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * inst->dt);
        est_err = est_out - est_cmd;

        if (match_time < inst->dt) {
            if (fabs(est_err) < 0.0001) {
                new_vel = vel_cmd;
            } else {
                new_vel = vel_cmd - 0.5 * est_err * inst->recip_dt;
                if (new_vel > (curr_vel + max_ac * inst->dt))
                    new_vel = curr_vel + max_ac * inst->dt;
                else if (new_vel < (curr_vel - max_ac * inst->dt))
                    new_vel = curr_vel - max_ac * inst->dt;
            }
        } else {
            dv = -2.0 * match_ac * inst->dt;
            dp = dv * match_time;
            if (fabs(est_err + dp * 2.0) < fabs(est_err))
                match_ac = -match_ac;
            new_vel = curr_vel + match_ac * inst->dt;
        }

        if (new_vel > max_freq) new_vel = max_freq;
        else if (new_vel < -max_freq) new_vel = -max_freq;
    } else {
        vel_cmd = *(inst->pins->vel_cmd) * inst->pos_scale;
        if (vel_cmd > max_freq) vel_cmd = max_freq;
        else if (vel_cmd < -max_freq) vel_cmd = -max_freq;

        dv = max_ac * inst->dt;
        if (vel_cmd > (inst->freq + dv)) new_vel = inst->freq + dv;
        else if (vel_cmd < (inst->freq - dv)) new_vel = inst->freq - dv;
        else new_vel = vel_cmd;
    }

    inst->freq = new_vel;
    inst->target_addval = inst->freq * inst->freqscale;
    inst->deltalim = max_ac * inst->accelscale;
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
    sg_pins_t *p;
    int r, i, n;
    int step_type = 0;
    int pos_mode = 1;
    char buf[GOMC_HAL_NAME_LEN + 1];

    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "step_type=", 10) == 0)
            step_type = atoi(argv[i] + 10);
        else if (strncmp(argv[i], "ctrl_type=", 10) == 0) {
            char c = argv[i][10];
            if (c == 'v' || c == 'V') pos_mode = 0;
        }
    }

    if (step_type < 0 || step_type > MAX_STEP_TYPE) return -EINVAL;

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->step_type = step_type;
    inst->pos_mode = pos_mode;

    /* init timing defaults */
    inst->old_periodns = inst->periodns = 50000;
    inst->old_dtns = 1000000;
    inst->periodfp = inst->periodns * 0.000000001;
    inst->freqscale = (1L << PICKOFF) * inst->periodfp;
    inst->accelscale = inst->freqscale * inst->periodfp;
    inst->dt = inst->old_dtns * 0.000000001;
    inst->recip_dt = 1.0 / inst->dt;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->pins = env->hal->malloc(env->hal->ctx, sizeof(sg_pins_t));
    if (!inst->pins) goto err;
    memset(inst->pins, 0, sizeof(sg_pins_t));
    p = inst->pins;

    /* enable */
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &p->enable, inst->comp_id, "%s.enable", name);
    if (r) goto err;

    /* position feedback */
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &p->count, inst->comp_id, "%s.counts", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &p->pos_fb, inst->comp_id, "%s.position-fb", name);
    if (r) goto err;

    /* command pin depends on mode */
    if (pos_mode) {
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &p->pos_cmd, inst->comp_id, "%s.position-cmd", name);
        if (r) goto err;
    } else {
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &p->vel_cmd, inst->comp_id, "%s.velocity-cmd", name);
        if (r) goto err;
    }

    /* output pins depend on step type */
    if (step_type == 0) {
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &p->phase[STEP_PIN], inst->comp_id, "%s.step", name);
        if (r) goto err;
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &p->phase[DIR_PIN], inst->comp_id, "%s.dir", name);
        if (r) goto err;
    } else if (step_type == 1) {
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &p->phase[UP_PIN], inst->comp_id, "%s.up", name);
        if (r) goto err;
        r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &p->phase[DOWN_PIN], inst->comp_id, "%s.down", name);
        if (r) goto err;
    } else {
        inst->num_phases = num_phases_lut[step_type - 2];
        inst->cycle_max = cycle_len_lut[step_type - 2] - 1;
        inst->lut = &master_lut[step_type - 2][0];
        for (n = 0; n < inst->num_phases; n++) {
            r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &p->phase[n],
                inst->comp_id, "%s.phase-%c", name, n + 'A');
            if (r) goto err;
        }
    }

    /* timing params as HAL params */
    r = gomc_hal_param_u32_newf(env->hal, GOMC_HAL_RW, &inst->step_len, inst->comp_id, "%s.steplen", name);
    if (r) goto err;
    if (step_type < 2) {
        r = gomc_hal_param_u32_newf(env->hal, GOMC_HAL_RW, &inst->step_space, inst->comp_id, "%s.stepspace", name);
        if (r) goto err;
    }
    if (step_type == 0) {
        r = gomc_hal_param_u32_newf(env->hal, GOMC_HAL_RW, &inst->dir_setup, inst->comp_id, "%s.dirsetup", name);
        if (r) goto err;
        r = gomc_hal_param_u32_newf(env->hal, GOMC_HAL_RW, &inst->dir_hold_dly, inst->comp_id, "%s.dirhold", name);
        if (r) goto err;
    } else {
        r = gomc_hal_param_u32_newf(env->hal, GOMC_HAL_RW, &inst->dir_hold_dly, inst->comp_id, "%s.dirdelay", name);
        if (r) goto err;
    }

    /* scaling and motion params */
    r = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW, &inst->pos_scale, inst->comp_id, "%s.position-scale", name);
    if (r) goto err;
    r = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW, &inst->maxvel, inst->comp_id, "%s.maxvel", name);
    if (r) goto err;
    r = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW, &inst->maxaccel, inst->comp_id, "%s.maxaccel", name);
    if (r) goto err;
    r = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RO, &inst->freq, inst->comp_id, "%s.frequency", name);
    if (r) goto err;
    r = gomc_hal_param_s32_newf(env->hal, GOMC_HAL_RO, &inst->rawcount, inst->comp_id, "%s.rawcounts", name);
    if (r) goto err;

    /* defaults */
    inst->pos_scale = 1.0;
    inst->step_len = 1;
    if (step_type < 2) inst->step_space = 1;
    if (step_type == 0) { inst->dir_hold_dly = 1; inst->dir_setup = 1; }
    else inst->dir_hold_dly = 1;
    inst->old_step_len = ~0;
    inst->old_step_space = ~0;
    inst->old_dir_hold_dly = ~0;
    inst->old_dir_setup = ~0;
    inst->accum = 1 << (PICKOFF - 1);

    /* export functions */
    snprintf(buf, sizeof(buf), "%s.make-pulses", name);
    r = env->hal->export_funct(env->hal->ctx, buf, make_pulses, inst, 0, 0, inst->comp_id);
    if (r) goto err;

    snprintf(buf, sizeof(buf), "%s.update-freq", name);
    r = env->hal->export_funct(env->hal->ctx, buf, update_freq, inst, 1, 0, inst->comp_id);
    if (r) goto err;

    snprintf(buf, sizeof(buf), "%s.capture-position", name);
    r = env->hal->export_funct(env->hal->ctx, buf, capture_position, inst, 1, 0, inst->comp_id);
    if (r) goto err;

    r = env->hal->ready(env->hal->ctx, inst->comp_id);
    if (r) goto err;

    *out = &inst->base;
    return 0;

err:
    if (inst->comp_id > 0)
        env->hal->exit(env->hal->ctx, inst->comp_id);
    env->rtapi->free(env->rtapi->ctx, inst);
    return -1;
}
