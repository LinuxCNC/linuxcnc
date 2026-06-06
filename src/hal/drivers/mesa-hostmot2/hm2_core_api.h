// hm2_core_api.h — Internal API for transport→core board registration.
//
// Provided by: hostmot2.so (core module)
// Consumed by: hm2_eth.so, hm2_pci.so, hm2_spi.so, etc. (transport drivers)
//
// This is a hand-written API header (not GMI-generated) because the
// callbacks pass opaque hm2_lowlevel_io_t pointers that carry function
// pointers — something GMI doesn't handle natively.
//
// Transport drivers call register_board() in their Init() to hand over
// their hm2_lowlevel_io_t struct (already populated with read/write callbacks
// and board metadata).  hostmot2 core then does the full FPGA initialization,
// HAL pin creation, and function export as before.

#ifndef HM2_CORE_API_H
#define HM2_CORE_API_H

#include "gomc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration — full definition in hostmot2-lowlevel.h.
// Both hostmot2.c and transport drivers include hostmot2-lowlevel.h
// before this header, so the typedef is already available.
// This is only needed if someone includes this header standalone.
#ifndef HOSTMOT2_LOWLEVEL_H
struct hm2_lowlevel_io_struct;
typedef struct hm2_lowlevel_io_struct hm2_lowlevel_io_t;
#endif

// register_board: transport hands its llio + config to the hostmot2 core.
// Equivalent to the old hm2_register(llio, config) call.
// Returns 0 on success, negative on error.
typedef int (*hm2_core_register_board_fn)(void *ctx,
                                          hm2_lowlevel_io_t *llio,
                                          char *config);

// unregister_board: transport requests shutdown of a previously registered board.
// Equivalent to the old hm2_unregister(llio) call.
typedef void (*hm2_core_unregister_board_fn)(void *ctx,
                                             hm2_lowlevel_io_t *llio);

typedef struct {
    void *ctx;
    hm2_core_register_board_fn register_board;
    hm2_core_unregister_board_fn unregister_board;
} hm2_core_callbacks_t;

// --- Registration & Lookup (same pattern as generated GMI headers) ---

static inline int hm2_core_api_register(
    const gomc_api_t *api,
    const char *instance_name,
    const hm2_core_callbacks_t *callbacks)
{
    return api->register_api(api->ctx, "hm2_core", 1,
                             instance_name, callbacks);
}

static inline const hm2_core_callbacks_t *hm2_core_api_get(
    const gomc_api_t *api,
    const char *instance_name)
{
    return (const hm2_core_callbacks_t *)api->get_api(
        api->ctx, "hm2_core", 1, instance_name);
}

#ifdef __cplusplus
}
#endif

#endif // HM2_CORE_API_H
