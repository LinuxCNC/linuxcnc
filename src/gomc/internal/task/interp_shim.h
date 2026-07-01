/*
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */
// interp_shim.h — C declarations for the interpreter shim functions.
// This header is included by interp.go via cgo.

#ifndef INTERP_SHIM_H
#define INTERP_SHIM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- INI accessor callback struct ---
// Provides INI values to the interpreter without it needing to parse files.
// The caller (Go milltask) owns the ctx and implements the callbacks.
// Namespace resolution is handled by the caller — the interpreter just asks
// for section/key and gets the (possibly namespace-overridden) value back.
typedef struct {
    void *ctx;
    // Get the first value for section/key.  Returns NULL if not found.
    // The returned string is valid until the next call to get/get_nth on the
    // same accessor (caller may use a single reusable buffer).
    const char* (*get)(void *ctx, const char *section, const char *key);
    // Get the n-th value (1-based) for section/key (for repeated keys like REMAP).
    // Returns NULL when there is no n-th occurrence.
    const char* (*get_nth)(void *ctx, const char *section, const char *key, int n);
} interp_ini_accessor_t;

// Interpreter lifecycle
void *interp_new(void);
void *interp_from_lib(const char *shlib);
void interp_delete(void *handle);

// Configuration and initialization
int interp_ini_load(void *handle, const char *inifile);
int interp_ini_load_accessor(void *handle, const interp_ini_accessor_t *accessor);
int interp_init(void *handle);

// Set the INI accessor for runtime INI parameter lookups (#<_ini[SEC]KEY>).
// Must be called before init() if runtime INI access is desired.
void interp_set_ini_accessor(void *handle, const interp_ini_accessor_t *accessor);

// File operations
int interp_open(void *handle, const char *filename);
int interp_close(void *handle);
int interp_reset(void *handle);
int interp_exit(void *handle);

// Read/Execute
int interp_read(void *handle);
int interp_read_string(void *handle, const char *line);
int interp_execute(void *handle);
int interp_execute_string(void *handle, const char *line);
int interp_execute_string_lineno(void *handle, const char *line, int line_number);
int interp_synch(void *handle);

// State queries
int interp_line(void *handle);
int interp_sequence_number(void *handle);
int interp_call_level(void *handle);
int interp_on_abort(void *handle, int reason, const char *message);

// String queries
const char *interp_error_text(void *handle, int errcode, char *buf, size_t buflen);
const char *interp_line_text(void *handle, char *buf, size_t buflen);
const char *interp_file_name(void *handle, char *buf, size_t buflen);
const char *interp_command(void *handle, char *buf, size_t buflen);

// Configuration
void interp_set_loglevel(void *handle, int level);
void interp_set_loop_on_main_m99(void *handle, int state);
void interp_set_task_mode(void *handle, int mode);

// Canon callback wiring
struct canon_callbacks;
typedef struct canon_callbacks canon_callbacks_t;
void interp_set_canon_callbacks(void *handle, const canon_callbacks_t *cb);

// M-code handler registration (M100-M199)
// Register a user-defined function slot in the interpreter.
// The slot will call the provided function pointer when M(100+idx) is encountered.
void interp_set_user_defined_function(void *handle, int idx,
    void (*fn)(int num, double arg1, double arg2));

// Active G/M codes and settings
// These copy the interpreter's internal arrays into caller-provided buffers.
void interp_active_g_codes(void *handle, int *gcodes, int max_len);
void interp_active_m_codes(void *handle, int *mcodes, int max_len);
void interp_active_settings(void *handle, double *settings, int max_len);

// Parameter I/O backend.
// Set before init() to override the default file-based persistence.
struct interp_param_io_t;
void interp_set_param_io(void *handle, const struct interp_param_io_t *io);

#ifdef __cplusplus
}
#endif

#endif // INTERP_SHIM_H
