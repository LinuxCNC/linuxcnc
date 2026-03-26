// gomc_ini.h — INI configuration access API for gomc C modules.
//
// Provides read-only access to the parsed INI file through callbacks
// into the Go launcher.  Returned strings have arena lifetime — they
// remain valid until the module's Destroy() is called.
//
// Usage:
//   const char *val = env->ini->get(env->ini->ctx, "DISPLAY", "PROGRAM_PREFIX");
//   const char *path = env->ini->source_file(env->ini->ctx);

#ifndef GOMC_INI_H
#define GOMC_INI_H

#ifdef __cplusplus
extern "C" {
#endif

// gomc_ini_t — INI configuration access callbacks.
//
// ctx:          opaque pointer passed as first argument to each callback.
// get:          returns the value for section/key, or NULL if not found.
//               The returned string is valid until Destroy().
// source_file:  returns the absolute path of the INI file.
//               The returned string is valid until Destroy().
typedef struct {
    void *ctx;
    const char* (*get)(void *ctx, const char *section, const char *key);
    const char* (*source_file)(void *ctx);
} gomc_ini_t;

#ifdef __cplusplus
}
#endif

#endif // GOMC_INI_H
