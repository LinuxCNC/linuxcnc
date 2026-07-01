/*
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */
/* test_halscope_rt.c — Unit tests for the halscope RT state machine.
 *
 * Tests state transitions, trigger detection, sample capture, buffer
 * management, and ring-buffer linearization — all without HAL or RTAPI.
 *
 * Build:  cd src/gomc/internal/halscope/testrt && gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -O2 -I testmock -I ../../../../../unit_tests -I .. -o test_halscope_rt test_halscope_rt.c -lm
 * Run:    ./test_halscope_rt
 */

#define _DEFAULT_SOURCE  /* for htole32/htole64 in <endian.h> */
#include <assert.h>
#include "greatest.h"

/* hal.h and rtapi.h in this directory are mocks that redirect to hal_mock.h.
 * The -I order ensures they're found before the real headers. */

/* Pull in the implementation — gives us access to static functions.
 * Only include the .c (which includes .h internally). */
#include "../halscope_rt.c"

/* ================================================================== */
/*  Helpers                                                            */
/* ================================================================== */

/* Allocate a scope with sensible defaults and return it.
 * Caller must free with halscope_free(). */
static halscope_t *make_scope(int num_samples)
{
    halscope_t *s = halscope_alloc(num_samples);
    assert(s != NULL && "halscope_alloc failed");
    return s;
}

/* Configure one FLOAT channel at index `ch` pointed at `src`. */
static void add_float_channel(halscope_t *s, int ch, double *src)
{
    s->channels[ch].enabled   = 1;
    s->channels[ch].data_type = HAL_FLOAT;
    s->channels[ch].data_len  = sizeof(double);
    s->channels[ch].data_addr = src;
}

/* Configure one BIT channel at index `ch` pointed at `src`. */
static void add_bit_channel(halscope_t *s, int ch, bool *src)
{
    s->channels[ch].enabled   = 1;
    s->channels[ch].data_type = HAL_BIT;
    s->channels[ch].data_len  = 1;
    s->channels[ch].data_addr = src;
}

/* Configure one S32 channel at index `ch` pointed at `src`. */
static void add_s32_channel(halscope_t *s, int ch, int32_t *src)
{
    s->channels[ch].enabled   = 1;
    s->channels[ch].data_type = HAL_S32;
    s->channels[ch].data_len  = sizeof(int32_t);
    s->channels[ch].data_addr = src;
}

/* Set max_channels and derive sample_len (matches Go-side logic).
 * max_ch must be 1, 2, 4, 8, or 16.
 * rec_len defaults to num_samples/max_ch but can be overridden after. */
static void set_max_channels(halscope_t *s, int max_ch)
{
    s->max_channels = max_ch;
    s->sample_len = max_ch;
    s->rec_len = s->num_samples / max_ch;
    s->pre_trig = s->rec_len / 2;
}

/* Run one RT cycle */
static void tick(halscope_t *s)
{
    halscope_sample(s, 1000000);
}

/* Run N RT cycles */
static void tick_n(halscope_t *s, int n)
{
    for (int i = 0; i < n; i++)
        halscope_sample(s, 1000000);
}

/* Read the output header from the done buffer */
static halscope_sample_header_t read_done_header(halscope_t *s)
{
    int db = atomic_load(&s->done_buf);
    halscope_sample_header_t hdr;
    memcpy(&hdr, s->bufs[db].data, sizeof(hdr));
    return hdr;
}

/* Get a pointer to the linearized sample data in the done buffer. */
static double *done_data_ptr(halscope_t *s)
{
    int db = atomic_load(&s->done_buf);
    return (double *)(s->bufs[db].data + sizeof(halscope_sample_header_t));
}

/* Linearize done buffer (same logic as Go watchSamples). Returns malloc'd
 * array of doubles, caller must free.  Sets *out_count to number of doubles. */
static double *linearize_done(halscope_t *s, int *out_count)
{
    int db = atomic_load(&s->done_buf);
    int total_len = s->done_len;
    int hdr_size = (int)sizeof(halscope_sample_header_t);
    int data_bytes = total_len - hdr_size;
    int ring_start_bytes = s->done_ring_start * (int)sizeof(double);

    double *result = malloc(data_bytes);
    uint8_t *src = s->bufs[db].data + hdr_size;

    if (ring_start_bytes == 0 || ring_start_bytes >= data_bytes) {
        memcpy(result, src, data_bytes);
    } else {
        int part1 = data_bytes - ring_start_bytes;
        memcpy(result, src + ring_start_bytes, part1);
        memcpy((uint8_t *)result + part1, src, ring_start_bytes);
    }
    *out_count = data_bytes / (int)sizeof(double);
    return result;
}


/* ================================================================== */
/*  SUITE: State transitions                                           */
/* ================================================================== */

TEST idle_after_alloc(void)
{
    halscope_t *s = make_scope(100);
    ASSERT_EQ(HALSCOPE_ST_IDLE, s->state);
    ASSERT_EQ(-1, atomic_load(&s->done_buf));
    ASSERT_EQ(100, s->num_samples);
    ASSERT_EQ(100, s->rec_len);
    ASSERT_EQ(50, s->pre_trig);
    halscope_free(s);
    PASS();
}

TEST reset_goes_to_idle(void)
{
    halscope_t *s = make_scope(100);
    s->state = HALSCOPE_ST_RESET;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_IDLE, s->state);
    ASSERT_EQ(0, s->samples);
    ASSERT_EQ(0, s->ring_pos);
    halscope_free(s);
    PASS();
}

TEST arm_to_pretrig_single_channel(void)
{
    halscope_t *s = make_scope(100);
    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* INIT → PRE_TRIG */
    ASSERT_EQ(HALSCOPE_ST_PRE_TRIG, s->state);
    ASSERT_EQ(0, s->samples);
    ASSERT_EQ(100, s->ring_cap);  /* rec_len * sample_len = 100 * 1 */

    halscope_free(s);
    PASS();
}

TEST arm_to_pretrig_multi_channel(void)
{
    halscope_t *s = make_scope(100);
    double v1 = 0.0, v2 = 0.0;
    add_float_channel(s, 0, &v1);
    add_float_channel(s, 1, &v2);
    set_max_channels(s, 2);
    ASSERT_EQ(2, s->sample_len);

    s->state = HALSCOPE_ST_INIT;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_PRE_TRIG, s->state);
    ASSERT_EQ(100, s->ring_cap);  /* rec_len(50) * sample_len(2) */

    halscope_free(s);
    PASS();
}

TEST full_cycle_auto_trigger(void)
{
    /* Small buffer: rec_len=10, pre_trig=5, auto_trig, no signal trigger */
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 1;
    s->trig.channel = -1;

    double val = 42.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 5;

    /* Arm: INIT tick transitions to PRE_TRIG but does not capture */
    s->state = HALSCOPE_ST_INIT;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_PRE_TRIG, s->state);

    /* Pre-trigger: need 5 samples. Each tick captures one. */
    tick_n(s, 4);
    ASSERT_EQ(HALSCOPE_ST_PRE_TRIG, s->state);
    ASSERT_EQ(4, s->samples);
    tick(s);  /* 5th sample → TRIG_WAIT + check_trigger (auto_timer→1) */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* TRIG_WAIT: auto_timer is 1 (from transition check_trigger).
     * Each tick: capture, samples++, check_trigger(auto_timer++), no trigger → samples--.
     * Need auto_timer to reach rec_len=10, so 9 more ticks. */
    tick_n(s, 8);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);
    tick(s);  /* auto_timer reaches 10 → POST_TRIG, samples becomes pre_trig+1=6 */
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);
    ASSERT_EQ(6, s->samples);

    /* POST_TRIG: captures until samples >= rec_len(10). Need 10-6=4 more. */
    tick_n(s, 3);
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    /* Verify done buffer */
    ASSERT(atomic_load(&s->done_buf) >= 0);
    ASSERT_EQ(1, (int)atomic_load(&s->done_gen));

    halscope_sample_header_t hdr = read_done_header(s);
    ASSERT_EQ(10, (int)hdr.sample_count);
    ASSERT_EQ(1,  (int)hdr.sample_len);
    ASSERT_EQ(5,  (int)hdr.start_offset);  /* pre_trig */

    halscope_free(s);
    PASS();
}

TEST idle_stays_idle(void)
{
    halscope_t *s = make_scope(100);
    tick_n(s, 10);
    ASSERT_EQ(HALSCOPE_ST_IDLE, s->state);
    halscope_free(s);
    PASS();
}

TEST done_stays_done(void)
{
    halscope_t *s = make_scope(100);
    s->state = HALSCOPE_ST_DONE;
    s->continuous = 0;
    tick_n(s, 10);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);
    halscope_free(s);
    PASS();
}

TEST continuous_rearms_after_done(void)
{
    /* Full cycle with continuous=1: DONE should transition back to INIT */
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;
    s->continuous = 1;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 5;
    s->pre_trig = 2;

    /* First capture */
    s->state = HALSCOPE_ST_INIT;
    tick(s);         /* → PRE_TRIG */
    tick_n(s, 2);    /* 2 pre-trig → TRIG_WAIT */
    s->trig.force = 1;
    tick(s);         /* → POST_TRIG (samples=3) */
    tick_n(s, 2);    /* 2 more → samples=5 → DONE */
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);
    ASSERT_EQ(1, (int)atomic_load(&s->done_gen));

    /* Next tick: DONE + continuous → INIT */
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_INIT, s->state);

    /* Next tick: INIT → PRE_TRIG (second capture starts) */
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_PRE_TRIG, s->state);

    /* Complete second capture */
    tick_n(s, 2);
    s->trig.force = 1;
    tick(s);
    tick_n(s, 2);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);
    ASSERT_EQ(2, (int)atomic_load(&s->done_gen));

    halscope_free(s);
    PASS();
}

TEST continuous_off_stays_done(void)
{
    /* Verify continuous=0 doesn't re-arm */
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;
    s->continuous = 0;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 5;
    s->pre_trig = 2;

    s->state = HALSCOPE_ST_INIT;
    tick(s);
    tick_n(s, 2);
    s->trig.force = 1;
    tick(s);
    tick_n(s, 2);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    /* Should stay in DONE */
    tick_n(s, 10);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);
    ASSERT_EQ(1, (int)atomic_load(&s->done_gen));

    halscope_free(s);
    PASS();
}

TEST init_clears_force(void)
{
    halscope_t *s = make_scope(100);
    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);

    s->trig.force = 1;
    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* INIT → PRE_TRIG, should clear force */
    ASSERT_EQ(0, s->trig.force);
    ASSERT_EQ(HALSCOPE_ST_PRE_TRIG, s->state);

    halscope_free(s);
    PASS();
}

TEST mult_skips_cycles(void)
{
    halscope_t *s = make_scope(100);
    s->mult = 3;
    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);

    s->rec_len = 10;
    s->pre_trig = 5;
    s->trig.auto_trig = 1;
    s->trig.channel = -1;
    s->state = HALSCOPE_ST_INIT;

    /* Tick 1: mult_cntr becomes 1, < 3, skip. Still INIT. */
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_INIT, s->state);
    tick(s);  /* mult_cntr=2, skip */
    ASSERT_EQ(HALSCOPE_ST_INIT, s->state);
    tick(s);  /* mult_cntr=3 >= 3, process INIT→PRE_TRIG */
    ASSERT_EQ(HALSCOPE_ST_PRE_TRIG, s->state);

    halscope_free(s);
    PASS();
}

SUITE(state_transitions)
{
    RUN_TEST(idle_after_alloc);
    RUN_TEST(reset_goes_to_idle);
    RUN_TEST(arm_to_pretrig_single_channel);
    RUN_TEST(arm_to_pretrig_multi_channel);
    RUN_TEST(full_cycle_auto_trigger);
    RUN_TEST(idle_stays_idle);
    RUN_TEST(done_stays_done);
    RUN_TEST(continuous_rearms_after_done);
    RUN_TEST(continuous_off_stays_done);
    RUN_TEST(init_clears_force);
    RUN_TEST(mult_skips_cycles);
}


/* ================================================================== */
/*  SUITE: Trigger detection                                           */
/* ================================================================== */

TEST force_trigger_in_trig_wait(void)
{
    halscope_t *s = make_scope(100);
    s->trig.channel = -1;  /* no signal trigger */
    s->trig.auto_trig = 0; /* no auto either */

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 3;

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */
    tick_n(s, 3);  /* 3 pre-trig samples → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Without force, stays in TRIG_WAIT */
    tick_n(s, 5);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Set force — should trigger immediately on next tick */
    s->trig.force = 1;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);
    ASSERT_EQ(0, s->trig.force);  /* force consumed */

    halscope_free(s);
    PASS();
}

TEST force_trigger_set_before_arm_is_cleared(void)
{
    /* Bug we caught: force set before arm is cleared by INIT */
    halscope_t *s = make_scope(100);
    s->trig.channel = -1;
    s->trig.auto_trig = 0;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 3;

    s->trig.force = 1;  /* Set BEFORE arm */
    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* INIT clears force */
    ASSERT_EQ(0, s->trig.force);

    tick_n(s, 3);  /* → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Stuck: no force, no auto, no signal trigger */
    tick_n(s, 100);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    halscope_free(s);
    PASS();
}

TEST rising_edge_float_trigger(void)
{
    halscope_t *s = make_scope(100);

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 3;

    /* Trigger on channel 0, rising edge, level 5.0 */
    s->trig.channel = 0;
    s->trig.edge = HALSCOPE_RISING;
    s->trig.auto_trig = 0;

    /* Set level to 5.0 via the ireal_t union */
    double lvl = 5.0;
    memcpy(&s->trig.level, &lvl, sizeof(double));

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */
    tick_n(s, 3);  /* → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Value below threshold — no trigger */
    val = 3.0;
    tick_n(s, 5);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Value above threshold — rising edge detected */
    val = 7.0;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    halscope_free(s);
    PASS();
}

TEST falling_edge_float_trigger(void)
{
    halscope_t *s = make_scope(100);

    double val = 10.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 3;

    s->trig.channel = 0;
    s->trig.edge = HALSCOPE_FALLING;
    s->trig.auto_trig = 0;
    double lvl = 5.0;
    memcpy(&s->trig.level, &lvl, sizeof(double));

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */

    /* Pre-trig with val > level, so compare_result = 1 */
    tick_n(s, 3);  /* → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Still above — no falling edge */
    tick_n(s, 3);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Drop below — falling edge */
    val = 2.0;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    halscope_free(s);
    PASS();
}

TEST bit_trigger_rising(void)
{
    halscope_t *s = make_scope(100);

    bool val = false;
    add_bit_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 3;

    s->trig.channel = 0;
    s->trig.edge = HALSCOPE_RISING;
    s->trig.auto_trig = 0;

    s->state = HALSCOPE_ST_INIT;
    tick(s);
    tick_n(s, 3);  /* → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Still false — no trigger */
    tick_n(s, 3);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Rising edge */
    val = true;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    halscope_free(s);
    PASS();
}

TEST s32_trigger_rising(void)
{
    halscope_t *s = make_scope(100);

    int32_t val = 0;
    add_s32_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 3;

    s->trig.channel = 0;
    s->trig.edge = HALSCOPE_RISING;
    s->trig.auto_trig = 0;
    s->trig.level.d_s32 = 100;

    s->state = HALSCOPE_ST_INIT;
    tick(s);
    tick_n(s, 3);  /* → TRIG_WAIT */

    val = 50;
    tick_n(s, 3);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    val = 200;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    halscope_free(s);
    PASS();
}

TEST auto_trigger_timeout(void)
{
    halscope_t *s = make_scope(100);
    s->trig.channel = -1;
    s->trig.auto_trig = 1;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 20;
    s->pre_trig = 5;

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */
    tick_n(s, 5);  /* → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Auto timer counts to rec_len.  One check_trigger ran in PRE_TRIG
     * transition, so auto_timer is already 1. Need rec_len - 1 more. */
    tick_n(s, 18);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);
    tick(s);  /* auto_timer reaches 20 */
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    halscope_free(s);
    PASS();
}

TEST trigger_on_different_channel_than_data(void)
{
    /* Trigger on channel 1 (S32), data on channel 0 (FLOAT) */
    halscope_t *s = make_scope(100);

    double data_val = 1.0;
    int32_t trig_val = 0;
    add_float_channel(s, 0, &data_val);
    add_s32_channel(s, 1, &trig_val);
    set_max_channels(s, 2);
    s->rec_len = 10;
    s->pre_trig = 3;

    s->trig.channel = 1;
    s->trig.edge = HALSCOPE_RISING;
    s->trig.auto_trig = 0;
    s->trig.level.d_s32 = 50;

    s->state = HALSCOPE_ST_INIT;
    tick(s);
    tick_n(s, 3);
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    trig_val = 100;
    tick(s);
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    halscope_free(s);
    PASS();
}

SUITE(trigger_detection)
{
    RUN_TEST(force_trigger_in_trig_wait);
    RUN_TEST(force_trigger_set_before_arm_is_cleared);
    RUN_TEST(rising_edge_float_trigger);
    RUN_TEST(falling_edge_float_trigger);
    RUN_TEST(bit_trigger_rising);
    RUN_TEST(s32_trigger_rising);
    RUN_TEST(auto_trigger_timeout);
    RUN_TEST(trigger_on_different_channel_than_data);
}


/* ================================================================== */
/*  SUITE: Sample capture and buffer correctness                       */
/* ================================================================== */

TEST captured_data_matches_source(void)
{
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 8;
    s->pre_trig = 3;

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* INIT → PRE_TRIG */

    /* Pre-trig: 3 samples */
    val = 10.0; tick(s);  /* samples=1 */
    val = 20.0; tick(s);  /* samples=2 */
    val = 30.0; tick(s);  /* samples=3 → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Force trigger */
    s->trig.force = 1;
    val = 40.0; tick(s);  /* trigger fires, samples=4 → POST_TRIG */
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    /* Post-trig: need 8-4=4 more */
    val = 50.0; tick(s);
    val = 60.0; tick(s);
    val = 70.0; tick(s);
    val = 80.0; tick(s);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    int count;
    double *data = linearize_done(s, &count);
    ASSERT_EQ(8, count);

    /* Expected: [10, 20, 30, 40, 50, 60, 70, 80] */
    ASSERT_EQ_FMT(10.0, data[0], "%f");
    ASSERT_EQ_FMT(20.0, data[1], "%f");
    ASSERT_EQ_FMT(30.0, data[2], "%f");
    ASSERT_EQ_FMT(40.0, data[3], "%f");
    ASSERT_EQ_FMT(50.0, data[4], "%f");
    ASSERT_EQ_FMT(60.0, data[5], "%f");
    ASSERT_EQ_FMT(70.0, data[6], "%f");
    ASSERT_EQ_FMT(80.0, data[7], "%f");

    free(data);
    halscope_free(s);
    PASS();
}

TEST multi_channel_interleaved(void)
{
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;

    double v1 = 0.0, v2 = 0.0;
    add_float_channel(s, 0, &v1);
    add_float_channel(s, 1, &v2);
    set_max_channels(s, 2);
    s->rec_len = 5;
    s->pre_trig = 2;
    ASSERT_EQ(2, s->sample_len);

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */

    /* Pre-trig: 2 samples */
    v1 = 10.0; v2 = 100.0; tick(s);  /* samples=1 */
    v1 = 20.0; v2 = 200.0; tick(s);  /* samples=2 → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Force trigger */
    s->trig.force = 1;
    v1 = 30.0; v2 = 300.0; tick(s);  /* trigger, samples=3 → POST_TRIG */
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    v1 = 40.0; v2 = 400.0; tick(s);
    v1 = 50.0; v2 = 500.0; tick(s);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    int count;
    double *data = linearize_done(s, &count);
    ASSERT_EQ(10, count);  /* 5 samples × 2 channels */

    /* Interleaved: [ch0_0, ch1_0, ch0_1, ch1_1, ...] */
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ_FMT((double)((i + 1) * 10),  data[i * 2 + 0], "%f");
        ASSERT_EQ_FMT((double)((i + 1) * 100), data[i * 2 + 1], "%f");
    }

    free(data);
    halscope_free(s);
    PASS();
}

TEST ring_buffer_wraps_correctly(void)
{
    /* With pre-trigger, the ring wraps before trigger fires.
     * Verify that linearized data is correct. */
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 10;
    s->pre_trig = 5;

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */

    /* Fill pre-trigger with values 100..104 */
    for (int i = 0; i < 5; i++) {
        val = 100.0 + i;
        tick(s);
    }
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Slide the window in TRIG_WAIT for several cycles with values 105..114.
     * The ring overwrites old pre-trigger samples. */
    for (int i = 0; i < 10; i++) {
        val = 105.0 + i;
        tick(s);
    }
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    /* Force trigger. At this point, pre-trig window holds values 110..114. */
    val = 200.0;
    s->trig.force = 1;
    tick(s);  /* This sample (200.0) is the trigger sample */
    ASSERT_EQ(HALSCOPE_ST_POST_TRIG, s->state);

    /* Capture 4 more post-trigger samples */
    for (int i = 0; i < 4; i++) {
        val = 201.0 + i;
        tick(s);
    }
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    /* Verify linearized data */
    int count;
    double *data = linearize_done(s, &count);
    ASSERT_EQ(10, count);

    /* Pre-trigger: the last 5 values before trigger */
    ASSERT_EQ_FMT(110.0, data[0], "%f");
    ASSERT_EQ_FMT(111.0, data[1], "%f");
    ASSERT_EQ_FMT(112.0, data[2], "%f");
    ASSERT_EQ_FMT(113.0, data[3], "%f");
    ASSERT_EQ_FMT(114.0, data[4], "%f");

    /* Trigger sample + post-trigger */
    ASSERT_EQ_FMT(200.0, data[5], "%f");
    ASSERT_EQ_FMT(201.0, data[6], "%f");
    ASSERT_EQ_FMT(202.0, data[7], "%f");
    ASSERT_EQ_FMT(203.0, data[8], "%f");
    ASSERT_EQ_FMT(204.0, data[9], "%f");

    free(data);
    halscope_free(s);
    PASS();
}

TEST header_start_offset_equals_pretrig(void)
{
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 20;
    s->pre_trig = 7;

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */
    tick_n(s, 7);  /* → TRIG_WAIT */

    s->trig.force = 1;
    tick(s);  /* → POST_TRIG */

    /* Finish capture */
    tick_n(s, 20);  /* more than enough */
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    halscope_sample_header_t hdr = read_done_header(s);
    ASSERT_EQ(7, (int)hdr.start_offset);

    halscope_free(s);
    PASS();
}

TEST generation_increments(void)
{
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;

    double val = 0.0;
    add_float_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 5;
    s->pre_trig = 2;

    ASSERT_EQ(0, (int)atomic_load(&s->done_gen));

    /* First capture */
    s->state = HALSCOPE_ST_INIT;
    tick(s);         /* → PRE_TRIG */
    tick_n(s, 2);    /* 2 pre-trig → TRIG_WAIT */
    s->trig.force = 1;
    tick(s);         /* → POST_TRIG (samples=3) */
    tick_n(s, 2);    /* 2 more → samples=5 → DONE */
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);
    ASSERT_EQ(1, (int)atomic_load(&s->done_gen));

    /* Second capture */
    s->state = HALSCOPE_ST_INIT;
    tick(s);
    tick_n(s, 2);
    s->trig.force = 1;
    tick(s);
    tick_n(s, 2);
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);
    ASSERT_EQ(2, (int)atomic_load(&s->done_gen));

    halscope_free(s);
    PASS();
}

TEST bit_channel_capture(void)
{
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;

    bool val = false;
    add_bit_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 4;
    s->pre_trig = 1;

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */

    val = false; tick(s);  /* pre-trig sample, samples=1 → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    s->trig.force = 1;
    val = true;  tick(s);  /* trigger, samples=2 → POST_TRIG */
    val = true;  tick(s);  /* samples=3 */
    val = false; tick(s);  /* samples=4 → DONE */
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    int count;
    double *data = linearize_done(s, &count);
    ASSERT_EQ(4, count);
    ASSERT_EQ_FMT(0.0, data[0], "%f");
    ASSERT_EQ_FMT(1.0, data[1], "%f");
    ASSERT_EQ_FMT(1.0, data[2], "%f");
    ASSERT_EQ_FMT(0.0, data[3], "%f");

    free(data);
    halscope_free(s);
    PASS();
}

TEST s32_channel_capture(void)
{
    halscope_t *s = make_scope(100);
    s->trig.auto_trig = 0;
    s->trig.channel = -1;

    int32_t val = 0;
    add_s32_channel(s, 0, &val);
    set_max_channels(s, 1);
    s->rec_len = 3;
    s->pre_trig = 1;

    s->state = HALSCOPE_ST_INIT;
    tick(s);  /* → PRE_TRIG */

    val = -42;   tick(s);  /* pre-trig, samples=1 → TRIG_WAIT */
    ASSERT_EQ(HALSCOPE_ST_TRIG_WAIT, s->state);

    s->trig.force = 1;
    val = 0;     tick(s);  /* trigger, samples=2 → POST_TRIG */
    val = 12345; tick(s);  /* samples=3 → DONE */
    ASSERT_EQ(HALSCOPE_ST_DONE, s->state);

    int count;
    double *data = linearize_done(s, &count);
    ASSERT_EQ(3, count);
    ASSERT_EQ_FMT(-42.0,   data[0], "%f");
    ASSERT_EQ_FMT(0.0,     data[1], "%f");
    ASSERT_EQ_FMT(12345.0, data[2], "%f");

    free(data);
    halscope_free(s);
    PASS();
}

SUITE(capture_and_buffer)
{
    RUN_TEST(captured_data_matches_source);
    RUN_TEST(multi_channel_interleaved);
    RUN_TEST(ring_buffer_wraps_correctly);
    RUN_TEST(header_start_offset_equals_pretrig);
    RUN_TEST(generation_increments);
    RUN_TEST(bit_channel_capture);
    RUN_TEST(s32_channel_capture);
}


/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(state_transitions);
    RUN_SUITE(trigger_detection);
    RUN_SUITE(capture_and_buffer);
    GREATEST_MAIN_END();
}
