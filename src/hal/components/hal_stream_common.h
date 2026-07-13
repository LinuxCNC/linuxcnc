//
// hal_stream_common.h — shared types/helpers for the HAL stream family
// (sampler, streamer, filestream).
//
// The three comps share the same per-pin value encoding and the same
// HAL-pin marshalling (capture a HAL pin into a sample word / apply a sample
// word onto a HAL pin).  Keeping them here means the wire format and the
// pin-type handling live in exactly one place.
//
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
//

#ifndef HAL_STREAM_COMMON_H
#define HAL_STREAM_COMMON_H

#include <stdint.h>
#include <string.h>

#define HAL_STREAM_MAX_PINS 20

// One pin's value in a sample.  Every value occupies 8 bytes so a sample is a
// fixed num_pins*8 blob (matches the WebSocket wire format).
typedef union {
    double   f;
    int32_t  s;
    uint32_t u;
    uint32_t b;  // 0 or 1
} hal_stream_val_t;

// A typed pointer to the backing HAL pin.
typedef union {
    volatile double   *hfloat;
    volatile int32_t  *hs32;
    volatile uint32_t *hu32;
    volatile unsigned *hbit;
} hal_stream_pin_t;

// Validate a cfg string (e.g. "uffb") and copy its types into types_out.
// Returns the pin count, or -1 on an invalid type / too many pins.
static inline int hal_stream_parse_cfg(const char *cfg, char *types_out, int max)
{
    if (!cfg || !cfg[0]) return -1;
    int n = (int)strlen(cfg);
    if (n > max) return -1;
    for (int i = 0; i < n; i++) {
        char c = cfg[i];
        if (c != 'f' && c != 'b' && c != 'u' && c != 's') return -1;
        types_out[i] = c;
    }
    return n;
}

// Capture the current value of a HAL pin into a sample word.
static inline void hal_stream_capture(hal_stream_val_t *dst, char type,
                                      hal_stream_pin_t pin)
{
    switch (type) {
    case 'f': dst->f = *pin.hfloat;          break;
    case 'b': dst->b = *pin.hbit ? 1 : 0;    break;
    case 'u': dst->u = *pin.hu32;            break;
    case 's': dst->s = *pin.hs32;            break;
    }
}

// Apply a sample word onto a HAL pin.
static inline void hal_stream_apply(char type, hal_stream_pin_t pin,
                                    const hal_stream_val_t *src)
{
    switch (type) {
    case 'f': *pin.hfloat = src->f;          break;
    case 'b': *pin.hbit   = src->b ? 1 : 0;  break;
    case 'u': *pin.hu32   = src->u;          break;
    case 's': *pin.hs32   = src->s;          break;
    }
}

#endif // HAL_STREAM_COMMON_H
