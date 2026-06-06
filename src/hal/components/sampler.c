//
// sampler.c — HAL sampler component (cmod version)
//
// Captures HAL pin data at the servo rate into a ring buffer.
// Connected WebSocket clients receive the data via the stream_server API.
//
// Usage:
//   loadrt sampler cfg=uffb depth=1000
//
// The cfg string defines pin types:
//   f = float, b = bit, u = u32, s = s32
//
// Copyright (C) 2006 John Kasunich (original sampler.c)
// Copyright (C) 2026 cmod port
// License: GPL Version 2
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "gomc_env.h"
#include "hal_sampler_stream_api.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define MAX_PINS        20
#define MAX_CONNS       8
#define DEFAULT_DEPTH   1000

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

// One sample: array of raw pin values packed as doubles (simplifies encoding).
// We transmit each sample as num_pins * 8 bytes (little-endian doubles).
typedef union {
    double f;
    int32_t s;
    uint32_t u;
    uint32_t b;  // 0 or 1
} sample_val_t;

typedef union {
    volatile double *hfloat;
    volatile int32_t *hs32;
    volatile uint32_t *hu32;
    volatile unsigned *hbit;
} pin_ptr_t;

// Per-connection state
typedef struct {
    int active;
    uint32_t read_pos;   // where this conn reads from in the ring
} conn_state_t;

// Per-instance state
typedef struct {
    // HAL pins
    volatile int32_t *curr_depth;
    volatile unsigned *full;
    volatile unsigned *enable;
    volatile int32_t *overruns;
    volatile int32_t *sample_num;

    // Pin configuration
    int num_pins;
    char pin_types[MAX_PINS];  // 'f', 'b', 'u', 's'
    pin_ptr_t pins[MAX_PINS];

    // Ring buffer (allocated at init)
    sample_val_t *ring;     // ring[depth * num_pins]
    int depth;
    uint32_t write_pos;     // monotonically increasing write position

    // Connection tracking
    conn_state_t conns[MAX_CONNS];
} sampler_inst_t;

typedef struct {
    cmod_env_t env;
    cmod_t mod;
    int comp_id;
    char name[64];
    sampler_inst_t inst;
    hal_sampler_stream_callbacks_t stream_cb;
} sampler_priv_t;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static void sample_funct(void *arg, long period);
static int32_t on_new_conn(void *ctx, uint32_t conn_id);
static void on_closed_conn(void *ctx, uint32_t conn_id);
static int32_t on_poll_transmit(void *ctx, uint32_t conn_id,
                                void *buf, int32_t maxlen);
static void sampler_destroy(cmod_t *self);

// ---------------------------------------------------------------------------
// Stream server callbacks
// ---------------------------------------------------------------------------

static int32_t on_new_conn(void *ctx, uint32_t conn_id) {
    sampler_priv_t *priv = (sampler_priv_t *)ctx;
    sampler_inst_t *inst = &priv->inst;

    if (conn_id == 0 || conn_id > MAX_CONNS) return -1;
    int idx = conn_id - 1;

    if (inst->conns[idx].active) return -1;  // slot in use

    inst->conns[idx].active = 1;
    inst->conns[idx].read_pos = inst->write_pos;  // start from current
    return 0;
}

static void on_closed_conn(void *ctx, uint32_t conn_id) {
    sampler_priv_t *priv = (sampler_priv_t *)ctx;
    sampler_inst_t *inst = &priv->inst;

    if (conn_id == 0 || conn_id > MAX_CONNS) return;
    inst->conns[conn_id - 1].active = 0;
}

static int32_t on_poll_transmit(void *ctx, uint32_t conn_id,
                                void *buf, int32_t maxlen) {
    sampler_priv_t *priv = (sampler_priv_t *)ctx;
    sampler_inst_t *inst = &priv->inst;

    if (conn_id == 0 || conn_id > MAX_CONNS) return -1;
    int idx = conn_id - 1;
    conn_state_t *conn = &inst->conns[idx];

    if (!conn->active) return -1;

    // Calculate how many samples are available
    uint32_t wp = inst->write_pos;
    uint32_t rp = conn->read_pos;
    uint32_t available = wp - rp;

    if (available == 0) return 0;  // nothing to send

    // Clamp to ring buffer size (if reader fell behind, skip to oldest)
    if (available > (uint32_t)inst->depth) {
        rp = wp - inst->depth;
        conn->read_pos = rp;
        available = inst->depth;
    }

    // Calculate how many samples fit in the output buffer
    int sample_bytes = inst->num_pins * (int)sizeof(sample_val_t);
    int max_samples = maxlen / sample_bytes;
    if (max_samples <= 0) return 0;

    int to_send = (int)available;
    if (to_send > max_samples) to_send = max_samples;

    // Copy samples to output buffer
    char *out = (char *)buf;
    for (int i = 0; i < to_send; i++) {
        uint32_t ring_idx = (rp + i) % inst->depth;
        memcpy(out, &inst->ring[ring_idx * inst->num_pins],
               sample_bytes);
        out += sample_bytes;
    }

    conn->read_pos = rp + to_send;
    return to_send * sample_bytes;
}

// ---------------------------------------------------------------------------
// RT function — samples HAL pins into ring buffer
// ---------------------------------------------------------------------------

static void sample_funct(void *arg, long period) {
    sampler_inst_t *inst = (sampler_inst_t *)arg;

    if (!*(inst->enable)) {
        return;
    }

    // Write current pin values into ring buffer
    uint32_t ring_idx = inst->write_pos % inst->depth;
    sample_val_t *dst = &inst->ring[ring_idx * inst->num_pins];

    for (int n = 0; n < inst->num_pins; n++) {
        switch (inst->pin_types[n]) {
        case 'f':
            dst[n].f = *(inst->pins[n].hfloat);
            break;
        case 'b':
            dst[n].b = *(inst->pins[n].hbit) ? 1 : 0;
            break;
        case 'u':
            dst[n].u = *(inst->pins[n].hu32);
            break;
        case 's':
            dst[n].s = *(inst->pins[n].hs32);
            break;
        }
    }

    inst->write_pos++;
    (*(inst->sample_num))++;

    // Update depth/full status (approximate for connected clients)
    // We report based on the slowest reader
    uint32_t max_lag = 0;
    for (int i = 0; i < MAX_CONNS; i++) {
        if (inst->conns[i].active) {
            uint32_t lag = inst->write_pos - inst->conns[i].read_pos;
            if (lag > max_lag) max_lag = lag;
        }
    }
    *(inst->curr_depth) = (int32_t)max_lag;
    if (max_lag >= (uint32_t)inst->depth) {
        *(inst->full) = 1;
        (*(inst->overruns))++;
    } else {
        *(inst->full) = 0;
    }
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

static void sampler_destroy(cmod_t *self) {
    sampler_priv_t *priv = (sampler_priv_t *)self->priv;
    if (priv->comp_id > 0) {
        priv->env.hal->exit(priv->env.hal->ctx, priv->comp_id);
    }
    free(priv->inst.ring);
    free(priv);
}

// ---------------------------------------------------------------------------
// New — constructor
// ---------------------------------------------------------------------------

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    int retval;
    const char *cfg_str = NULL;
    int depth = DEFAULT_DEPTH;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "cfg=", 4) == 0) {
            cfg_str = argv[i] + 4;
        } else if (strncmp(argv[i], "depth=", 6) == 0) {
            depth = atoi(argv[i] + 6);
            if (depth <= 0) depth = DEFAULT_DEPTH;
        }
    }

    if (!cfg_str || !cfg_str[0]) {
        gomc_log_errorf(env->log, name,
                        "sampler requires a 'cfg=' parameter (e.g. cfg=uffb)");
        return -EINVAL;
    }

    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -EINVAL;
    }

    if (!env->api) {
        gomc_log_errorf(env->log, name, "API registry not available");
        return -EINVAL;
    }

    // Count and validate pins
    int num_pins = strlen(cfg_str);
    if (num_pins > MAX_PINS) {
        gomc_log_errorf(env->log, name,
                        "too many pins (%d), max is %d", num_pins, MAX_PINS);
        return -EINVAL;
    }
    for (int i = 0; i < num_pins; i++) {
        char c = cfg_str[i];
        if (c != 'f' && c != 'b' && c != 'u' && c != 's') {
            gomc_log_errorf(env->log, name,
                            "invalid pin type '%c' in cfg string", c);
            return -EINVAL;
        }
    }

    // Allocate private data
    sampler_priv_t *priv = calloc(1, sizeof(sampler_priv_t));
    if (!priv) return -ENOMEM;

    priv->env = *env;
    snprintf(priv->name, sizeof(priv->name), "%s", name);

    sampler_inst_t *inst = &priv->inst;
    inst->num_pins = num_pins;
    inst->depth = depth;
    memcpy(inst->pin_types, cfg_str, num_pins);

    // Allocate ring buffer
    inst->ring = calloc(depth * num_pins, sizeof(sample_val_t));
    if (!inst->ring) {
        free(priv);
        return -ENOMEM;
    }

    // Initialize HAL component
    priv->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (priv->comp_id < 0) {
        gomc_log_errorf(env->log, name, "hal_init failed");
        free(inst->ring);
        free(priv);
        return -1;
    }

    // Create standard pins
    char pin_name[128];

    snprintf(pin_name, sizeof(pin_name), "%s.full", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_BIT, GOMC_HAL_OUT,
                 (void **)&inst->full, priv->comp_id);
    if (retval != 0) goto fail;

    snprintf(pin_name, sizeof(pin_name), "%s.enable", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_BIT, GOMC_HAL_IN,
                 (void **)&inst->enable, priv->comp_id);
    if (retval != 0) goto fail;

    snprintf(pin_name, sizeof(pin_name), "%s.curr-depth", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_S32, GOMC_HAL_OUT,
                 (void **)&inst->curr_depth, priv->comp_id);
    if (retval != 0) goto fail;

    snprintf(pin_name, sizeof(pin_name), "%s.overruns", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_S32, GOMC_HAL_IO,
                 (void **)&inst->overruns, priv->comp_id);
    if (retval != 0) goto fail;

    snprintf(pin_name, sizeof(pin_name), "%s.sample-num", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_S32, GOMC_HAL_IO,
                 (void **)&inst->sample_num, priv->comp_id);
    if (retval != 0) goto fail;

    // Initialize standard pins
    *(inst->full) = 0;
    *(inst->enable) = 1;
    *(inst->curr_depth) = 0;
    *(inst->overruns) = 0;
    *(inst->sample_num) = 0;

    // Create data pins based on cfg string
    int usefp = 0;
    for (int n = 0; n < num_pins; n++) {
        int hal_type;
        snprintf(pin_name, sizeof(pin_name), "%s.pin.%d", name, n);

        switch (cfg_str[n]) {
        case 'f':
            hal_type = GOMC_HAL_FLOAT;
            usefp = 1;
            break;
        case 'b':
            hal_type = GOMC_HAL_BIT;
            break;
        case 'u':
            hal_type = GOMC_HAL_U32;
            break;
        case 's':
            hal_type = GOMC_HAL_S32;
            break;
        default:
            goto fail;  // unreachable after validation
        }

        retval = env->hal->pin_new(env->hal->ctx, pin_name,
                     hal_type, GOMC_HAL_IN,
                     (void **)&inst->pins[n], priv->comp_id);
        if (retval != 0) goto fail;
    }

    // Export the RT sample function
    retval = env->hal->export_funct(env->hal->ctx, name,
                                    sample_funct, inst, usefp, 0, priv->comp_id);
    if (retval < 0) {
        gomc_log_errorf(env->log, name, "function export failed");
        goto fail;
    }

    // Register stream server callbacks
    priv->stream_cb.ctx = priv;
    priv->stream_cb.cfg = inst->pin_types;
    priv->stream_cb.new_conn = on_new_conn;
    priv->stream_cb.closed_conn = on_closed_conn;
    priv->stream_cb.poll_transmit = on_poll_transmit;

    retval = hal_sampler_stream_register(
        (gomc_api_t *)env->api, name, &priv->stream_cb);
    if (retval != 0) {
        gomc_log_errorf(env->log, name, "stream_register failed: %d", retval);
        goto fail;
    }

    env->hal->ready(env->hal->ctx, priv->comp_id);

    // Set up cmod_t
    priv->mod.priv = priv;
    priv->mod.Init = NULL;
    priv->mod.Start = NULL;
    priv->mod.Stop = NULL;
    priv->mod.Destroy = sampler_destroy;

    *out = &priv->mod;
    return 0;

fail:
    env->hal->exit(env->hal->ctx, priv->comp_id);
    free(inst->ring);
    free(priv);
    return retval;
}
