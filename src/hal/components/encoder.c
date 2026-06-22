/*
 * encoder.c — cmod HAL component: software quadrature encoder counter.
 *
 * Single-channel quadrature encoder with index, latch, velocity estimation,
 * and missing-tooth support.
 *
 * Usage:
 *   load encoder
 *
 * Original author: John Kasunich
 * Copyright (c) 2003 John Kasunich
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>

/* Quadrature state machine bitmasks */
#define SM_PHASE_A_MASK 0x01
#define SM_PHASE_B_MASK 0x02
#define SM_LOOKUP_MASK  0x0F
#define SM_CNT_UP_MASK  0x40
#define SM_CNT_DN_MASK  0x80

static const unsigned char lut_x4[16] = {
    0x00, 0x44, 0x88, 0x0C, 0x80, 0x04, 0x08, 0x4C,
    0x40, 0x04, 0x08, 0x8C, 0x00, 0x84, 0x48, 0x0C
};
static const unsigned char lut_x1[16] = {
    0x00, 0x44, 0x08, 0x0C, 0x80, 0x04, 0x08, 0x0C,
    0x00, 0x04, 0x08, 0x0C, 0x00, 0x04, 0x08, 0x0C
};
static const unsigned char lut_ctr[16] = {
    0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Atomically passed data between fast and slow functions */
typedef struct {
    char count_detected;
    char index_detected;
    char latch_detected;
    int32_t raw_count;
    uint32_t timestamp;
    int32_t index_count;
    int32_t latch_count;
} atomic_t;

typedef struct {
    gomc_hal_bit_t   *phaseA;
    gomc_hal_bit_t   *phaseB;
    gomc_hal_bit_t   *phaseZ;
    gomc_hal_bit_t   *index_ena;
    gomc_hal_bit_t   *reset;
    gomc_hal_bit_t   *latch_in;
    gomc_hal_bit_t   *latch_rising;
    gomc_hal_bit_t   *latch_falling;
    gomc_hal_bit_t   *x4_mode;
    gomc_hal_bit_t   *counter_mode;
    gomc_hal_s32_t   *missing_teeth;
    gomc_hal_s32_t   *raw_counts;
    gomc_hal_s32_t   *count;
    gomc_hal_s32_t   *count_latch;
    gomc_hal_float_t *min_speed;
    gomc_hal_float_t *pos;
    gomc_hal_float_t *pos_interp;
    gomc_hal_float_t *pos_latch;
    gomc_hal_float_t *vel;
    gomc_hal_float_t *vel_rpm;
    gomc_hal_float_t *pos_scale;
} enc_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    enc_hal_t *hal;
    /* internal state */
    unsigned char state;
    unsigned char oldZ;
    unsigned char Zmask;
    int32_t old_latch;
    int32_t dt;
    int32_t limit_dt;
    int gaps;
    atomic_t buf[2];
    volatile atomic_t *bp;
    int32_t raw_count;
    uint32_t timestamp;
    int32_t index_count;
    int32_t latch_count;
    double old_scale;
    double scale;
    int counts_since_timeout;
    uint32_t timebase;
} inst_t;

static void update_counter(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    enc_hal_t *h = inst->hal;
    atomic_t *ab;
    unsigned char state;
    int latch, old_latch, rising, falling;

    ab = (atomic_t *)inst->bp;
    inst->dt += period;

    state = inst->state;
    if (*(h->phaseA)) state |= SM_PHASE_A_MASK;
    if (*(h->phaseB)) state |= SM_PHASE_B_MASK;

    if (*(h->counter_mode))
        state = lut_ctr[state & (SM_LOOKUP_MASK & ~SM_PHASE_B_MASK)];
    else if (*(h->x4_mode))
        state = lut_x4[state & SM_LOOKUP_MASK];
    else
        state = lut_x1[state & SM_LOOKUP_MASK];

    if (state & SM_CNT_UP_MASK) {
        if (*(h->missing_teeth) && inst->dt > inst->limit_dt)
            inst->gaps++;
        (*h->raw_counts)++;
        ab->raw_count = *(h->raw_counts);
        ab->timestamp = inst->timebase;
        ab->count_detected = 1;
        inst->dt = 0;
    } else if (state & SM_CNT_DN_MASK) {
        (*h->raw_counts)--;
        ab->raw_count = *(h->raw_counts);
        ab->timestamp = inst->timebase;
        ab->count_detected = 1;
    }
    inst->state = state;

    /* index detection */
    state = inst->oldZ << 1;
    if (*(h->phaseZ) || inst->gaps) state |= 1;
    inst->oldZ = state & 3;
    if ((state & inst->Zmask) == 1) {
        ab->index_count = *(h->raw_counts);
        ab->index_detected = 1;
        inst->Zmask = 0;
    }

    /* latch detection */
    latch = *(h->latch_in);
    old_latch = inst->old_latch;
    rising = latch && !old_latch;
    falling = !latch && old_latch;
    if ((rising && *(h->latch_rising)) || (falling && *(h->latch_falling))) {
        ab->latch_detected = 1;
        ab->latch_count = *(h->raw_counts);
    }
    inst->old_latch = latch;

    inst->timebase += period;
}

static void capture_position(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    enc_hal_t *h = inst->hal;
    atomic_t *ab;
    int32_t delta_counts;
    uint32_t delta_time;
    double vel, interp;

    ab = (atomic_t *)inst->bp;
    /* swap buffers */
    inst->bp = (ab == &inst->buf[0]) ? &inst->buf[1] : &inst->buf[0];

    if (ab->index_detected) {
        ab->index_detected = 0;
        inst->index_count = ab->index_count;
        *(h->index_ena) = 0;
    }
    if (ab->latch_detected) {
        ab->latch_detected = 0;
        inst->latch_count = ab->latch_count;
    }

    inst->Zmask = *(h->index_ena) ? 3 : 0;

    /* scale */
    if (*(h->pos_scale) != inst->old_scale) {
        inst->old_scale = *(h->pos_scale);
        if (*(h->pos_scale) < 1e-20 && *(h->pos_scale) > -1e-20)
            *(h->pos_scale) = 1.0;
        inst->scale = 1.0 / *(h->pos_scale);
    }
    if (*(h->min_speed) == 0) *(h->min_speed) = 1;

    /* reset */
    if (*(h->reset)) {
        inst->raw_count = *(h->raw_counts);
        inst->index_count = inst->raw_count;
    }

    if (ab->count_detected) {
        ab->count_detected = 0;
        delta_time = ab->timestamp - inst->timestamp;
        delta_counts = ab->raw_count - inst->raw_count;

        if (delta_counts != 0) {
            inst->limit_dt *= 0.9;
            inst->limit_dt += 0.1 * ((*(h->missing_teeth) + 0.5) * (delta_time / delta_counts));
        }

        *h->raw_counts += *(h->missing_teeth) * inst->gaps;
        inst->raw_count = ab->raw_count + *(h->missing_teeth) * inst->gaps;
        delta_counts += *(h->missing_teeth) * inst->gaps;
        inst->gaps = 0;
        inst->timestamp = ab->timestamp;

        if (inst->counts_since_timeout < 2)
            inst->counts_since_timeout++;
        else {
            vel = (delta_counts * inst->scale) / (delta_time * 1e-9);
            *(h->vel) = vel;
        }
    } else {
        if (inst->counts_since_timeout) {
            delta_time = inst->timebase - inst->timestamp;
            if (*(h->missing_teeth) && delta_time > 1.5 * (uint32_t)inst->limit_dt) {
                /* don't update in tooth gap */
            } else if (delta_time < 1e9 / (*(h->min_speed) * inst->scale)) {
                vel = inst->scale / (delta_time * 1e-9);
                if (vel < 0.0) vel = -vel;
                if (vel < *(h->vel)) *(h->vel) = vel;
                if (-vel > *(h->vel)) *(h->vel) = -vel;
            } else {
                inst->counts_since_timeout = 0;
                *(h->vel) = 0;
            }
        } else {
            *(h->vel) = 0;
        }
    }

    *(h->vel_rpm) = *(h->vel) * 60.0;
    *(h->count) = inst->raw_count - inst->index_count;
    *(h->count_latch) = inst->latch_count - inst->index_count;
    *(h->pos) = *(h->count) * inst->scale;
    *(h->pos_latch) = *(h->count_latch) * inst->scale;

    delta_time = inst->timebase - inst->timestamp;
    interp = *(h->vel) * (delta_time * 1e-9);
    *(h->pos_interp) = *(h->pos) + interp;
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
    enc_hal_t *h;
    int r;
    char buf[GOMC_HAL_NAME_LEN + 1];

    (void)argc; (void)argv;

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->bp = &inst->buf[0];
    inst->old_scale = 1.0;
    inst->scale = 1.0;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(enc_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(enc_hal_t));
    h = inst->hal;

    /* input pins */
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->phaseA, inst->comp_id, "%s.phase-A", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->phaseB, inst->comp_id, "%s.phase-B", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->phaseZ, inst->comp_id, "%s.phase-Z", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &h->index_ena, inst->comp_id, "%s.index-enable", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->reset, inst->comp_id, "%s.reset", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->latch_in, inst->comp_id, "%s.latch-input", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->latch_rising, inst->comp_id, "%s.latch-rising", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->latch_falling, inst->comp_id, "%s.latch-falling", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &h->x4_mode, inst->comp_id, "%s.x4-mode", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &h->counter_mode, inst->comp_id, "%s.counter-mode", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IN, &h->missing_teeth, inst->comp_id, "%s.missing-teeth", name);
    if (r != 0) goto err;

    /* output pins */
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &h->raw_counts, inst->comp_id, "%s.rawcounts", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &h->count, inst->comp_id, "%s.counts", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &h->count_latch, inst->comp_id, "%s.counts-latched", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->min_speed, inst->comp_id, "%s.min-speed-estimate", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->pos, inst->comp_id, "%s.position", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->pos_interp, inst->comp_id, "%s.position-interpolated", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->pos_latch, inst->comp_id, "%s.position-latched", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->vel, inst->comp_id, "%s.velocity", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->vel_rpm, inst->comp_id, "%s.velocity-rpm", name);
    if (r != 0) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->pos_scale, inst->comp_id, "%s.position-scale", name);
    if (r != 0) goto err;

    /* defaults */
    *(h->x4_mode) = 1;
    *(h->latch_rising) = 1;
    *(h->latch_falling) = 1;
    *(h->min_speed) = 1.0;
    *(h->pos_scale) = 1.0;

    /* export functions */
    snprintf(buf, sizeof(buf), "%s.update-counters", name);
    r = env->hal->export_funct(env->hal->ctx, buf, update_counter, inst, 0, 0, inst->comp_id);
    if (r != 0) goto err;

    snprintf(buf, sizeof(buf), "%s.capture-position", name);
    r = env->hal->export_funct(env->hal->ctx, buf, capture_position, inst, 1, 0, inst->comp_id);
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
