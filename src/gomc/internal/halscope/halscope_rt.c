/* halscope_rt.c — RT sample capture for the HAL oscilloscope.
 *
 * This file implements the realtime sample function and buffer management.
 * It is compiled into gomc-server via cgo.  The halscope_t struct is
 * allocated by Go, the RT function is exported to HAL via hal_export_funct().
 *
 * Copyright (C) 2003 John Kasunich (original scope_rt.c)
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License.
 */

#include "halscope_rt.h"

#include <stdlib.h>
#include <string.h>
#include <endian.h>

/* ------------------------------------------------------------------ */
/*                    SAMPLE CAPTURE                                   */
/* ------------------------------------------------------------------ */

static void capture_sample(halscope_t *s)
{
    double *ring = (double *)(s->bufs[s->write_buf].data +
                              sizeof(halscope_sample_header_t));
    double *out = &ring[s->ring_pos];

    for (int n = 0; n < s->sample_len; n++) {
        if (s->channels[n].data_len == 0 || s->channels[n].data_addr == NULL) {
            *out++ = 0.0;
            continue;
        }
        switch (s->channels[n].data_type) {
        case HAL_BIT:
            *out++ = (double)*((unsigned char *)s->channels[n].data_addr);
            break;
        case HAL_S32:
            *out++ = (double)*((rtapi_s32 *)s->channels[n].data_addr);
            break;
        case HAL_U32:
            *out++ = (double)*((rtapi_u32 *)s->channels[n].data_addr);
            break;
        case HAL_FLOAT:
            *out++ = (double)*((real_t *)s->channels[n].data_addr);
            break;
        default:
            *out++ = 0.0;
            break;
        }
    }
    s->ring_pos += s->sample_len;
    if (s->ring_pos >= s->ring_cap)
        s->ring_pos = 0;
}

/* ------------------------------------------------------------------ */
/*                    TRIGGER DETECTION                                */
/* ------------------------------------------------------------------ */

static int check_trigger(halscope_t *s)
{
    int prev;

    if (s->trig.force) {
        s->trig.force = 0;
        return 1;
    }
    if (s->trig.auto_trig) {
        if (++s->auto_timer >= s->rec_len)
            return 1;
    } else {
        s->auto_timer = 0;
    }
    if (s->trig.channel < 0)
        return 0;

    int ch = s->trig.channel;
    if (ch >= HALSCOPE_MAX_CHANNELS || s->channels[ch].data_addr == NULL)
        return 0;

    halscope_data_t *value = (halscope_data_t *)s->channels[ch].data_addr;
    halscope_data_t *level = &s->trig.level;
    prev = s->compare_result;

    switch (s->channels[ch].data_type) {
    case HAL_BIT:
        s->compare_result = value->d_u8;
        break;
    case HAL_FLOAT:
        s->compare_result = (value->d_real > level->d_real);
        break;
    case HAL_S32:
        s->compare_result = (value->d_s32 > level->d_s32);
        break;
    case HAL_U32:
        s->compare_result = (value->d_u32 > level->d_u32);
        break;
    default:
        s->compare_result = 0;
        break;
    }

    if (s->trig.edge && s->compare_result && !prev)
        return 1;
    if (!s->trig.edge && !s->compare_result && prev)
        return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*                    BUFFER MANAGEMENT                                */
/* ------------------------------------------------------------------ */

/* Pick a free buffer for writing (not done_buf, no active readers). */
static int pick_write_buf(halscope_t *s)
{
    int db = atomic_load_explicit(&s->done_buf, memory_order_acquire);
    for (int i = 0; i < HALSCOPE_NUM_BUFFERS; i++) {
        if (i == db)
            continue;
        if (atomic_load_explicit(&s->bufs[i].readers,
                                 memory_order_acquire) == 0)
            return i;
    }
    return -1;
}

/* Convert doubles to little-endian in-place (no-op on LE platforms) */
static void apply_le_encoding(halscope_t *s __attribute__((unused)))
{
#if __BYTE_ORDER != __LITTLE_ENDIAN
    double *data = (double *)(s->bufs[s->write_buf].data +
                              sizeof(halscope_sample_header_t));
    int count = s->samples * s->sample_len;
    for (int i = 0; i < count; i++) {
        uint64_t le;
        memcpy(&le, &data[i], sizeof(le));
        le = htole64(le);
        memcpy(&data[i], &le, sizeof(le));
    }
#endif
}

/* Write the sample_header_t into the current write buffer */
static void write_header(halscope_t *s)
{
    halscope_sample_header_t *hdr =
        (halscope_sample_header_t *)s->bufs[s->write_buf].data;
    hdr->sample_count = htole32((uint32_t)s->samples);
    hdr->sample_len   = htole32((uint32_t)s->sample_len);
    hdr->start_offset = htole32((uint32_t)s->pre_trig);
    hdr->reserved     = 0;
}

/* ------------------------------------------------------------------ */
/*                    RT SAMPLE FUNCTION                               */
/* ------------------------------------------------------------------ */

void halscope_sample(void *arg, long period)
{
    halscope_t *s = (halscope_t *)arg;
    (void)period;

    halscope_state_t cur_state = atomic_load_explicit(&s->state,
                                                       memory_order_acquire);
    if (cur_state == HALSCOPE_ST_RESET) {
        s->ring_pos = 0;
        s->ring_start = 0;
        s->samples = 0;
        s->trig.force = 0;
        atomic_store_explicit(&s->state, HALSCOPE_ST_IDLE,
                              memory_order_release);
        return;
    }

    s->mult_cntr++;
    if (s->mult_cntr < s->mult)
        return;
    s->mult_cntr = 0;

    switch (cur_state) {
    case HALSCOPE_ST_IDLE:
        break;

    case HALSCOPE_ST_INIT: {
        int wb = pick_write_buf(s);
        if (wb < 0)
            break;
        s->write_buf = wb;
        s->ring_pos = 0;
        s->ring_start = 0;
        s->sample_len = s->max_channels;
        s->ring_cap = s->rec_len * s->sample_len;
        s->samples = 0;
        s->trig.force = 0;
        s->auto_timer = 0;
        s->compare_result = 0;
        atomic_store_explicit(&s->state, HALSCOPE_ST_PRE_TRIG,
                              memory_order_release);
        break;
    }

    case HALSCOPE_ST_PRE_TRIG:
        capture_sample(s);
        s->samples++;
        if (s->samples >= s->pre_trig) {
            atomic_store_explicit(&s->state, HALSCOPE_ST_TRIG_WAIT,
                                  memory_order_release);
            check_trigger(s);
        }
        break;

    case HALSCOPE_ST_TRIG_WAIT:
        capture_sample(s);
        s->samples++;
        if (check_trigger(s)) {
            atomic_store_explicit(&s->state, HALSCOPE_ST_POST_TRIG,
                                  memory_order_release);
        } else {
            s->samples--;
            s->ring_start += s->sample_len;
            if (s->ring_start >= s->ring_cap)
                s->ring_start = 0;
        }
        break;

    case HALSCOPE_ST_POST_TRIG:
        capture_sample(s);
        s->samples++;
        if (s->samples >= s->rec_len) {
            apply_le_encoding(s);
            write_header(s);
            s->done_len = sizeof(halscope_sample_header_t) +
                          s->samples * s->sample_len * sizeof(double);
            s->done_ring_start = s->ring_start;
            atomic_store_explicit(&s->done_buf, s->write_buf,
                                  memory_order_release);
            atomic_fetch_add_explicit(&s->done_gen, 1,
                                      memory_order_release);
            atomic_store_explicit(&s->state, HALSCOPE_ST_DONE,
                                  memory_order_release);
        }
        break;

    case HALSCOPE_ST_DONE:
        if (atomic_load_explicit(&s->continuous, memory_order_relaxed))
            atomic_store_explicit(&s->state, HALSCOPE_ST_INIT,
                                  memory_order_release);
        break;

    default:
        atomic_store_explicit(&s->state, HALSCOPE_ST_IDLE,
                              memory_order_release);
        break;
    }
}

/* ------------------------------------------------------------------ */
/*                    ALLOC / FREE                                     */
/* ------------------------------------------------------------------ */

halscope_t *halscope_alloc(int num_samples)
{
    halscope_t *s = calloc(1, sizeof(halscope_t));
    if (!s)
        return NULL;

    s->num_samples = num_samples;
    s->mult = 1;
    s->max_channels = 1;
    s->rec_len = num_samples;  /* derived: num_samples / max_channels */
    s->sample_len = 1;
    s->pre_trig = num_samples / 2;
    s->trig.channel = -1;
    atomic_init(&s->state, HALSCOPE_ST_IDLE);
    atomic_init(&s->continuous, 0);
    atomic_init(&s->done_buf, -1);
    atomic_init(&s->done_gen, 0);

    int buf_cap = sizeof(halscope_sample_header_t) +
                  num_samples * HALSCOPE_MAX_CHANNELS * sizeof(double);
    for (int i = 0; i < HALSCOPE_NUM_BUFFERS; i++) {
        s->bufs[i].data = calloc(1, buf_cap);
        if (!s->bufs[i].data) {
            for (int j = 0; j < i; j++)
                free(s->bufs[j].data);
            free(s);
            return NULL;
        }
        s->bufs[i].capacity = buf_cap;
        atomic_init(&s->bufs[i].readers, 0);
    }

    return s;
}

void halscope_free(halscope_t *s)
{
    if (!s)
        return;
    for (int i = 0; i < HALSCOPE_NUM_BUFFERS; i++)
        free(s->bufs[i].data);
    free(s);
}
