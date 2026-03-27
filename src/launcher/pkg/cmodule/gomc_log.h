// gomc_log.h — Structured logging API for gomc C modules.
//
// All log messages (RT and non-RT) are enqueued into a lock-free ring buffer.
// A Go goroutine drains the buffer and forwards entries to the structured
// logging backend.  This guarantees message ordering across RT and non-RT
// code paths and avoids any Go/CGO crossing on the RT hot path.
//
// Usage:
//   gomc_log_infof(env->log, "mycomp", "started %d slaves", n);
//   gomc_log_errorf(env->log, "mycomp", "init failed: %s", reason);

#ifndef GOMC_LOG_H
#define GOMC_LOG_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gomc_rtapi.h"  // GOMC_RTAPI_NAME_LEN

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Log levels
// ---------------------------------------------------------------------------

typedef enum {
    GOMC_LOG_DEBUG = 0,
    GOMC_LOG_INFO  = 1,
    GOMC_LOG_WARN  = 2,
    GOMC_LOG_ERROR = 3,
} gomc_log_level_t;

// ---------------------------------------------------------------------------
// Ring buffer slot — fixed-size, cache-line aligned.
// ---------------------------------------------------------------------------

#define GOMC_LOG_MSG_LEN       216
#define GOMC_LOG_COMPONENT_LEN (GOMC_RTAPI_NAME_LEN + 1)

typedef struct {
    uint32_t          seq;                              // sequence number (0 = free)
    uint32_t          level;                            // gomc_log_level_t
    int64_t           timestamp_ns;                     // CLOCK_MONOTONIC nanoseconds
    char              component[GOMC_LOG_COMPONENT_LEN];
    char              msg[GOMC_LOG_MSG_LEN];
} gomc_log_slot_t;

// Slot is 264 bytes (4+4+8+32+216).  Adjust GOMC_LOG_MSG_LEN if exact
// power-of-2 alignment is desired.

// ---------------------------------------------------------------------------
// Ring buffer — single shared instance per launcher process.
// Multiple producers (C threads), single consumer (Go drain goroutine).
// ---------------------------------------------------------------------------

#define GOMC_LOG_RING_SIZE_SHIFT 10
#define GOMC_LOG_RING_SIZE       (1u << GOMC_LOG_RING_SIZE_SHIFT)  // 1024 slots
#define GOMC_LOG_RING_MASK       (GOMC_LOG_RING_SIZE - 1)

typedef struct {
    // Producer side — each writer does atomic fetch-add on write_pos to
    // claim a slot, then fills it and publishes via store-release on seq.
    uint32_t write_pos;

    // Consumer side — the Go drain goroutine tracks its own read position.
    // Padding avoids false sharing between producer and consumer cache lines.
    char _pad[60];
    uint32_t read_pos;

    // Slot array.
    gomc_log_slot_t slots[GOMC_LOG_RING_SIZE];
} gomc_log_ring_t;

// ---------------------------------------------------------------------------
// gomc_log_t — the logging handle passed to modules via cmod_env_t.
// ---------------------------------------------------------------------------

typedef struct {
    gomc_log_ring_t *ring;  // pointer to shared ring buffer
} gomc_log_t;

// ---------------------------------------------------------------------------
// Producer API — pure C, no allocations, no syscalls, RT-safe.
// ---------------------------------------------------------------------------

// Get current monotonic time in nanoseconds (used for log timestamps).
static inline int64_t gomc_log_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

// Low-level enqueue: claim a slot, format the message, publish.
// Returns 0 on success, -1 if the ring is full (message dropped).
static inline int
gomc_log_emit(const gomc_log_t *log, gomc_log_level_t level,
              const char *component, const char *fmt, va_list ap) {
    gomc_log_ring_t *ring = log->ring;

    // Claim a slot (lock-free, multi-producer safe).
    uint32_t pos = __atomic_fetch_add(&ring->write_pos, 1, __ATOMIC_RELAXED);
    uint32_t idx = pos & GOMC_LOG_RING_MASK;
    gomc_log_slot_t *slot = &ring->slots[idx];

    // Check if the consumer has drained this slot (seq == 0 means free).
    // If not, the ring is full — drop the message to avoid blocking.
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&slot->seq, &expected, 1, 0,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        // Ring full — message dropped.  This should be rare if the Go drain
        // goroutine keeps up.  A dropped-message counter could be added here.
        return -1;
    }

    // Fill the slot.
    slot->level = (uint32_t)level;
    slot->timestamp_ns = gomc_log_now_ns();
    strncpy(slot->component, component, GOMC_RTAPI_NAME_LEN);
    slot->component[GOMC_RTAPI_NAME_LEN] = '\0';
    vsnprintf(slot->msg, GOMC_LOG_MSG_LEN, fmt, ap);

    // Publish: set seq to pos+1 so the consumer knows this slot is ready.
    // The consumer reads slots in order and waits for seq == expected_seq.
    __atomic_store_n(&slot->seq, pos + 1, __ATOMIC_RELEASE);

    return 0;
}

// ---------------------------------------------------------------------------
// Convenience functions with printf format checking.
// ---------------------------------------------------------------------------

static inline __attribute__((format(printf, 3, 4))) void
gomc_log_debugf(const gomc_log_t *log, const char *component,
                const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gomc_log_emit(log, GOMC_LOG_DEBUG, component, fmt, ap);
    va_end(ap);
}

static inline __attribute__((format(printf, 3, 4))) void
gomc_log_infof(const gomc_log_t *log, const char *component,
               const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gomc_log_emit(log, GOMC_LOG_INFO, component, fmt, ap);
    va_end(ap);
}

static inline __attribute__((format(printf, 3, 4))) void
gomc_log_warnf(const gomc_log_t *log, const char *component,
               const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gomc_log_emit(log, GOMC_LOG_WARN, component, fmt, ap);
    va_end(ap);
}

static inline __attribute__((format(printf, 3, 4))) void
gomc_log_errorf(const gomc_log_t *log, const char *component,
                const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gomc_log_emit(log, GOMC_LOG_ERROR, component, fmt, ap);
    va_end(ap);
}

// ---------------------------------------------------------------------------
// Ring buffer management — used by the Go drain loop.
// ---------------------------------------------------------------------------

// Allocate a zeroed log ring buffer.
static inline gomc_log_ring_t *gomc_ring_create(void) {
    return (gomc_log_ring_t *)calloc(1, sizeof(gomc_log_ring_t));
}

// Free a log ring buffer.
static inline void gomc_ring_destroy(gomc_log_ring_t *r) {
    free(r);
}

// Try to read the next slot from the ring at the given read position.
// Returns 1 if a slot was read (output params filled), 0 otherwise.
static inline int
gomc_ring_try_read(gomc_log_ring_t *ring, uint32_t read_pos,
                   uint32_t *out_level, int64_t *out_ts,
                   char *out_component, char *out_msg) {
    uint32_t idx = read_pos & GOMC_LOG_RING_MASK;
    gomc_log_slot_t *slot = &ring->slots[idx];

    uint32_t seq = __atomic_load_n(&slot->seq, __ATOMIC_ACQUIRE);
    if (seq != read_pos + 1) {
        return 0;  // slot not ready yet
    }

    *out_level = slot->level;
    *out_ts = slot->timestamp_ns;
    memcpy(out_component, slot->component, GOMC_RTAPI_NAME_LEN + 1);
    memcpy(out_msg, slot->msg, GOMC_LOG_MSG_LEN);

    // Release the slot for reuse.
    __atomic_store_n(&slot->seq, 0, __ATOMIC_RELEASE);
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif // GOMC_LOG_H
