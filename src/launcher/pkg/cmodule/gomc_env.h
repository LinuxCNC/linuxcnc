// gomc_env.h — Combined environment header for gomc C modules.
//
// This is the new top-level header that replaces cmodule.h for modules
// migrating to the gomc API.  It composes the four sub-API headers
// (log, ini, hal, rtapi) into a single cmod_env_t structure.
//
// Module lifecycle remains the same:
//   New(env, name, argc, argv) → Start() → Stop() → Destroy()
//
// The launcher guarantees:
//   - Strings returned by ini->get() and ini->source_file() remain valid
//     until Destroy() is called on the module that requested them (arena).
//   - The env pointer and all sub-API pointers remain valid from New()
//     through Destroy().
//   - hal and rtapi pointers may be NULL for modules that don't need them.
//     Always check before use.
//
// For C++ plugins: declare the New symbol as extern "C".

#ifndef GOMC_ENV_H
#define GOMC_ENV_H

#include "gomc_log.h"
#include "gomc_ini.h"
#include "gomc_hal.h"
#include "gomc_rtapi.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// cmod_env_t — the launcher-provided environment passed to New().
// ---------------------------------------------------------------------------

typedef struct {
    // dlopen() handle of the .so file that contains this plugin.
    // Plugins that create RT HAL components should pass this to
    // hal->init() so the launcher can lock the .so's memory pages.
    void *dl_handle;

    // Sub-API pointers — always non-NULL for log and ini.
    // hal and rtapi may be NULL for pure userspace / non-HAL modules.
    const gomc_log_t   *log;
    const gomc_ini_t   *ini;
    const gomc_hal_t   *hal;    // NULL if module has no HAL component
    const gomc_rtapi_t *rtapi;  // NULL if module needs no RT services
} cmod_env_t;

// ---------------------------------------------------------------------------
// cmod_t — module instance returned by the constructor.
// ---------------------------------------------------------------------------

typedef struct cmod {
    int  (*Start)(struct cmod *self);
    void (*Stop)(struct cmod *self);
    void (*Destroy)(struct cmod *self);
    void *priv;
} cmod_t;

// ---------------------------------------------------------------------------
// cmod_new_fn — constructor signature.  Plugins export "New" with this type.
// ---------------------------------------------------------------------------

typedef int (*cmod_new_fn)(
    const cmod_env_t *env,
    const char *name,
    int argc, const char **argv,
    cmod_t **out
);

#ifdef __cplusplus
}
#endif

#endif // GOMC_ENV_H
