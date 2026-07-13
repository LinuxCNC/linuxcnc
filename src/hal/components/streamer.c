//
// streamer.c — HAL streamer component (cmod version)
//
// Receives data from WebSocket clients and outputs it onto HAL pins
// at the servo rate.
//
// Usage:
//   loadrt streamer cfg=uffb depth=1000
//
// The cfg string defines pin types:
//   f = float, b = bit, u = u32, s = s32
//
// Copyright (C) 2006 John Kasunich (original streamer.c)
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
// License: GPL Version 2
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "gomc_env.h"
#include "hal_streamer_stream_api.h"
#include "hal_stream_common.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define MAX_PINS        20
#define MAX_CONNS       8
#define DEFAULT_DEPTH   1000

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

// Per-pin value encoding and HAL-pin marshalling are shared across the stream
// family (sampler/streamer/filestream) via hal_stream_common.h.
typedef hal_stream_val_t sample_val_t;
typedef hal_stream_pin_t pin_ptr_t;

// Per-instance state
typedef struct {
    // HAL pins
    volatile int32_t *curr_depth;
    volatile unsigned *empty;
    volatile unsigned *enable;
    volatile int32_t *underruns;
    volatile unsigned *clock;
    volatile int32_t *clock_mode;

    // Pin configuration
    int num_pins;
    char pin_types[MAX_PINS];  // 'f', 'b', 'u', 's'
    pin_ptr_t pins[MAX_PINS];

    // Ring buffer (allocated at init)
    sample_val_t *ring;     // ring[depth * num_pins]
    int depth;
    volatile uint32_t write_pos;  // written by data_received (userspace thread)
    uint32_t read_pos;            // read by RT function only

    // Clock edge state
    int clock_edge;
} streamer_inst_t;

typedef struct {
    cmod_env_t env;
    cmod_t mod;
    int comp_id;
    char name[64];
    streamer_inst_t inst;
    hal_streamer_stream_callbacks_t stream_cb;
} streamer_priv_t;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static void update_funct(void *arg, long period);
static int32_t on_new_conn(void *ctx, uint32_t conn_id);
static void on_closed_conn(void *ctx, uint32_t conn_id);
static int32_t on_data_received(void *ctx, uint32_t conn_id,
                                void *buf, int32_t len);
static void streamer_destroy(cmod_t *self);

// ---------------------------------------------------------------------------
// Stream server callbacks
// ---------------------------------------------------------------------------

static int32_t on_new_conn(void *ctx, uint32_t conn_id) {
    (void)ctx;
    (void)conn_id;
    // Accept any connection — multiple writers just append to the same ring
    return 0;
}

static void on_closed_conn(void *ctx, uint32_t conn_id) {
    (void)ctx;
    (void)conn_id;
}

static int32_t on_data_received(void *ctx, uint32_t conn_id,
                                void *buf, int32_t len) {
    streamer_priv_t *priv = (streamer_priv_t *)ctx;
    streamer_inst_t *inst = &priv->inst;
    (void)conn_id;

    int sample_bytes = inst->num_pins * (int)sizeof(sample_val_t);
    if (len <= 0 || (len % sample_bytes) != 0) return -1;

    int num_samples = len / sample_bytes;
    char *src = (char *)buf;

    for (int i = 0; i < num_samples; i++) {
        // Check if ring is full
        uint32_t wp = inst->write_pos;
        uint32_t rp = inst->read_pos;
        uint32_t used = wp - rp;

        if (used >= (uint32_t)inst->depth) {
            // Ring full, drop remaining samples
            return i * sample_bytes;
        }

        uint32_t ring_idx = wp % inst->depth;
        memcpy(&inst->ring[ring_idx * inst->num_pins], src, sample_bytes);
        src += sample_bytes;

        // Memory barrier: ensure data is written before advancing write_pos
        __sync_synchronize();
        inst->write_pos = wp + 1;
    }

    return len;
}

// ---------------------------------------------------------------------------
// RT function — reads from ring buffer, writes to HAL pins
// ---------------------------------------------------------------------------

static void update_funct(void *arg, long period) {
    streamer_inst_t *inst = (streamer_inst_t *)arg;
    (void)period;

    // Update depth/empty status.  Acquire barrier pairs with the release barrier
    // in on_data_received: after observing write_pos we must see the sample data
    // written before it.
    uint32_t wp = inst->write_pos;
    __sync_synchronize();
    uint32_t rp = inst->read_pos;
    uint32_t available = wp - rp;
    *(inst->curr_depth) = (int32_t)available;

    if (available == 0) {
        *(inst->empty) = 1;
    } else {
        *(inst->empty) = 0;
    }

    if (!*(inst->enable)) return;

    // Clock mode logic
    int doclk = 1;
    int clock_edge = inst->clock_edge =
        ((inst->clock_edge << 1) | (*(inst->clock) & 1)) & 3;

    switch (*(inst->clock_mode)) {
    case 0: // freerun
        break;
    case 1: // falling edge
        if (clock_edge != 2) doclk = 0;
        break;
    case 2: // rising edge
        if (clock_edge != 1) doclk = 0;
        break;
    case 3: // any edge
        if (clock_edge == 0 || clock_edge == 3) doclk = 0;
        break;
    default:
        break;
    }

    if (!doclk) return;

    // Read next sample from ring
    if (available == 0) {
        (*(inst->underruns))++;
        return;
    }

    uint32_t ring_idx = rp % inst->depth;
    sample_val_t *src = &inst->ring[ring_idx * inst->num_pins];

    for (int n = 0; n < inst->num_pins; n++)
        hal_stream_apply(inst->pin_types[n], inst->pins[n], &src[n]);

    inst->read_pos = rp + 1;
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

static void streamer_destroy(cmod_t *self) {
    streamer_priv_t *priv = (streamer_priv_t *)self->priv;
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
                        "streamer requires a 'cfg=' parameter (e.g. cfg=uffb)");
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
    streamer_priv_t *priv = calloc(1, sizeof(streamer_priv_t));
    if (!priv) return -ENOMEM;

    priv->env = *env;
    snprintf(priv->name, sizeof(priv->name), "%s", name);

    streamer_inst_t *inst = &priv->inst;
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

    snprintf(pin_name, sizeof(pin_name), "%s.empty", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_BIT, GOMC_HAL_OUT,
                 (void **)&inst->empty, priv->comp_id);
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

    snprintf(pin_name, sizeof(pin_name), "%s.underruns", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_S32, GOMC_HAL_IO,
                 (void **)&inst->underruns, priv->comp_id);
    if (retval != 0) goto fail;

    snprintf(pin_name, sizeof(pin_name), "%s.clock", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_BIT, GOMC_HAL_IN,
                 (void **)&inst->clock, priv->comp_id);
    if (retval != 0) goto fail;

    snprintf(pin_name, sizeof(pin_name), "%s.clock-mode", name);
    retval = env->hal->pin_new(env->hal->ctx, pin_name,
                 GOMC_HAL_S32, GOMC_HAL_IN,
                 (void **)&inst->clock_mode, priv->comp_id);
    if (retval != 0) goto fail;

    // Initialize standard pins
    *(inst->empty) = 1;
    *(inst->enable) = 1;
    *(inst->curr_depth) = 0;
    *(inst->underruns) = 0;
    *(inst->clock) = 0;
    *(inst->clock_mode) = 0;

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
                     hal_type, GOMC_HAL_OUT,
                     (void **)&inst->pins[n], priv->comp_id);
        if (retval != 0) goto fail;
    }

    // Export the RT update function
    retval = env->hal->export_funct(env->hal->ctx, name,
                                    update_funct, inst, usefp, 0, priv->comp_id);
    if (retval < 0) {
        gomc_log_errorf(env->log, name, "function export failed");
        goto fail;
    }

    // Register stream server callbacks
    priv->stream_cb.ctx = priv;
    priv->stream_cb.cfg = inst->pin_types;
    priv->stream_cb.new_conn = on_new_conn;
    priv->stream_cb.closed_conn = on_closed_conn;
    priv->stream_cb.data_received = on_data_received;

    retval = hal_streamer_stream_register(
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
    priv->mod.Destroy = streamer_destroy;

    *out = &priv->mod;
    return 0;

fail:
    env->hal->exit(env->hal->ctx, priv->comp_id);
    free(inst->ring);
    free(priv);
    return retval;
}
