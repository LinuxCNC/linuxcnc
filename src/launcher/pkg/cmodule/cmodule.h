// cmodule.h — C plugin ABI for LinuxCNC launcher modules.
//
// C/C++ shared libraries (.so) built for the "load" HAL command must export
// a symbol named "New" with the signature cmod_new_fn.
//
// Lifecycle order:
//   New(env, name, argc, argv) → Start() → Stop() → Destroy()
//
// The launcher guarantees:
//   - All modules are stopped (Stop) before any module's Destroy is called.
//   - Strings returned by env callbacks (get_ini, ini_source_file) remain
//     valid until Destroy is called on the module that requested them (arena).
//   - The env pointer remains valid from New() through Destroy().
//
// For C++ plugins: declare the New symbol as extern "C".

#ifndef CMODULE_H
#define CMODULE_H

#ifdef __cplusplus
extern "C" {
#endif

// cmod_env_t is the launcher-provided environment passed to the constructor.
// All access to INI configuration and logging goes through these callbacks.
// The ctx pointer is opaque — pass it as the first argument to each callback.
typedef struct {
    void *ctx;

    // dl_handle is the dlopen() handle of the .so file that contains this
    // plugin.  Plugins that create RT HAL components should pass this to
    // hal_init_ex() so the launcher can lock the .so's memory pages.
    void *dl_handle;

    // get_ini returns the value for the given section/key from the parsed INI
    // file.  Returns NULL if the key does not exist.  The returned string is
    // valid until Destroy() is called on this module (arena lifetime).
    const char* (*get_ini)(void *ctx, const char *section, const char *key);

    // ini_source_file returns the absolute path of the INI file.  Valid until
    // Destroy().  Useful for NML initialization and subprocess launching.
    const char* (*ini_source_file)(void *ctx);

    // Structured logging callbacks.  The component argument should be the
    // module's instance name (passed to New).
    void (*log_info)(void *ctx, const char *component, const char *msg);
    void (*log_warn)(void *ctx, const char *component, const char *msg);
    void (*log_error)(void *ctx, const char *component, const char *msg);
    void (*log_debug)(void *ctx, const char *component, const char *msg);
} cmod_env_t;

// cmod_t is the module instance returned by the constructor.
// The plugin fills in the function pointers; the launcher calls them in order.
// The priv pointer is for the plugin's private data.
//
// Destroy must release all resources including the cmod_t itself (e.g.
// "delete m" for C++ modules).
typedef struct cmod {
    int  (*Start)(struct cmod *self);
    void (*Stop)(struct cmod *self);
    void (*Destroy)(struct cmod *self);
    void *priv;
} cmod_t;

// cmod_new_fn is the constructor signature.  Plugins must export a symbol
// named "New" with this type.
//
// Parameters:
//   env  — launcher-provided environment (INI access, logging)
//   name — instance name (use as HAL component name)
//   argc — number of arguments
//   argv — key=value argument strings from the "load" command
//   out  — on success, set to the allocated cmod_t
//
// Returns 0 on success, non-zero on error.
typedef int (*cmod_new_fn)(
    const cmod_env_t *env,
    const char *name,
    int argc, const char **argv,
    cmod_t **out
);

#ifdef __cplusplus
}
#endif

#endif // CMODULE_H
