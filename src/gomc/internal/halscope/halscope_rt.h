/* halscope_rt.h — Shared data structures between RT C code and Go.
 *
 * The halscope_t struct is allocated by Go (via C.calloc) and passed
 * to the RT sample function.  Go reads non-RT fields directly; RT
 * fields are accessed through atomics.
 *
 * Copyright (C) 2003 John Kasunich (original scope_rt.c)
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License.
 */

#ifndef HALSCOPE_RT_H
#define HALSCOPE_RT_H

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

#ifndef HALSCOPE_API_CGO
#define HALSCOPE_API_CGO
#endif
#include "halscope_api.h"

#include "hal.h"
#include "rtapi.h"

/* --- Constants --- */

#define HALSCOPE_DEFAULT_NUM_SAMPLES 16000
#define HALSCOPE_NUM_BUFFERS   3   /* triple buffer */

/* Binary sample data header — precedes the double array in each output buffer.
 * All fields little-endian uint32.  Must be a multiple of 8 bytes for
 * double alignment. */
typedef struct {
    uint32_t sample_count;   /* number of samples (rows) */
    uint32_t sample_len;     /* number of channels per sample (columns) */
    uint32_t start_offset;   /* always 0 (data is linearized by reader) */
    uint32_t reserved;
} halscope_sample_header_t;

/* State enum matching the IDL ScopeState */
typedef enum {
    HALSCOPE_ST_IDLE = 0,
    HALSCOPE_ST_INIT,
    HALSCOPE_ST_PRE_TRIG,
    HALSCOPE_ST_TRIG_WAIT,
    HALSCOPE_ST_POST_TRIG,
    HALSCOPE_ST_DONE,
    HALSCOPE_ST_RESET
} halscope_state_t;

/* Single sample value — union of all HAL types */
typedef union {
    unsigned char  d_u8;
    rtapi_u32      d_u32;
    rtapi_s32      d_s32;
    real_t         d_real;
    ireal_t        d_ireal;
} halscope_data_t;

/* Per-channel info */
typedef struct {
    int         enabled;
    char        pin_name[HAL_NAME_LEN + 1];
    hal_type_t  data_type;
    int         data_len;       /* 0, 1, 4, or 8 */
    void       *data_addr;      /* resolved HAL data pointer */
} halscope_channel_t;

/* Trigger config */
typedef struct {
    int              channel;     /* 0-based, -1 = none */
    halscope_data_t  level;
    halscope_trig_edge_t edge;
    int              force;
    int              auto_trig;
} halscope_trigger_t;

/* Output buffer — pre-allocated, stores doubles + 16-byte header.
 * RT writes here during capture; readers borrow the done buffer. */
typedef struct {
    uint8_t     *data;          /* header (16 bytes) + doubles */
    int          capacity;      /* allocated size in bytes */
    atomic_int   readers;       /* refcount for safe borrowing */
} halscope_output_buf_t;

/* Main halscope instance — shared between RT C code and Go.
 *
 * Layout contract:
 *   - RT function (halscope_sample) reads/writes all fields marked [RT].
 *   - Go code reads/writes fields marked [Go].
 *   - Fields marked [shared] are accessed from both sides using atomics
 *     or under the guarantee that RT is not running (e.g. during configure).
 */
typedef struct {
    /* Capture config [Go-write, RT-read] */
    char             thread_name[HAL_NAME_LEN + 1];
    int              num_samples;    /* buffer capacity (total doubles) */
    int              rec_len;        /* samples per record (derived: num_samples / max_channels) */
    int              pre_trig;       /* pre-trigger samples */
    int              mult;           /* sample period multiplier */
    int              max_channels;   /* columns per sample row (1/2/4/8/16) */

    /* Channel config [Go-write when idle, RT-read] */
    halscope_channel_t channels[HALSCOPE_MAX_CHANNELS];
    int              sample_len;     /* == max_channels (set in INIT from max_channels) */

    /* Trigger [Go-write, RT-read] */
    halscope_trigger_t trig;

    /* Continuous mode [Go-write, RT-read] — when set, RT automatically
     * re-arms (DONE → INIT) after each completed capture. */
    atomic_int       continuous;

    /* State machine [shared — RT writes state transitions,
     * Go writes ST_INIT (arm) and ST_RESET] */
    _Atomic halscope_state_t state;
    int              samples;        /* valid sample count [RT] */
    int              ring_pos;       /* current write position in doubles [RT] */
    int              ring_start;     /* first valid sample in doubles [RT] */
    int              ring_cap;       /* ring capacity in doubles [RT] */

    /* RT-only fields */
    int              mult_cntr;
    int              auto_timer;
    int              compare_result; /* for trigger edge detection */

    /* Triple buffer [shared via atomics] */
    halscope_output_buf_t bufs[HALSCOPE_NUM_BUFFERS];
    int              write_buf;     /* index RT is writing to [RT] */
    atomic_int       done_buf;      /* index of last completed capture (-1 = none) */
    atomic_uint      done_gen;      /* incremented on each capture completion */
    int              done_len;      /* byte length of last completed buffer [RT-write, Go-read after done_buf] */
    int              done_ring_start; /* ring_start at completion [RT-write, Go-read after done_buf] */
} halscope_t;

/* --- RT function prototype --- */

/* Called by HAL thread scheduler.  arg is halscope_t*. */
void halscope_sample(void *arg, long period);

/* --- Init/cleanup called from Go --- */

/* Allocate and initialize a halscope_t with the given buffer capacity.
 * Returns NULL on allocation failure. */
halscope_t *halscope_alloc(int num_samples);

/* Free all resources in a halscope_t allocated by halscope_alloc(). */
void halscope_free(halscope_t *s);

#endif /* HALSCOPE_RT_H */

/* --- Inline helpers for Go cgo access to C11 atomics --- */
/* cgo cannot call _Generic-based atomic macros directly. */

static inline int halscope_atomic_load_int(int *p, int order) {
    return atomic_load_explicit((_Atomic int *)p, (memory_order)order);
}
static inline unsigned int halscope_atomic_load_uint(unsigned int *p, int order) {
    return atomic_load_explicit((_Atomic unsigned int *)p, (memory_order)order);
}
static inline int halscope_atomic_fetch_add_int(int *p, int val, int order) {
    return atomic_fetch_add_explicit((_Atomic int *)p, val, (memory_order)order);
}
static inline int halscope_atomic_fetch_sub_int(int *p, int val, int order) {
    return atomic_fetch_sub_explicit((_Atomic int *)p, val, (memory_order)order);
}
static inline void halscope_atomic_store_int(int *p, int val, int order) {
    atomic_store_explicit((_Atomic int *)p, val, (memory_order)order);
}
static inline void halscope_atomic_store_state(halscope_state_t *p, halscope_state_t val, int order) {
    atomic_store_explicit((_Atomic halscope_state_t *)p, val, (memory_order)order);
}
static inline halscope_state_t halscope_atomic_load_state(halscope_state_t *p, int order) {
    return atomic_load_explicit((_Atomic halscope_state_t *)p, (memory_order)order);
}

/* Expose sizeof for Go */
static inline int halscope_get_header_size(void) { return (int)sizeof(halscope_sample_header_t); }
