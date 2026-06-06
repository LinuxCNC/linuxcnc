// interp_shim.h — C interface to the RS274NGC interpreter for cgo.
// Provides an opaque handle and lifecycle functions for preview interpretation.
#ifndef INTERP_SHIM_H
#define INTERP_SHIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque interpreter handle
typedef struct interp_handle interp_handle_t;

// Create a new interpreter instance.
interp_handle_t *interp_shim_new(void);

// Set the canon callback table. Must be called before init.
// The callbacks pointer must remain valid for the lifetime of the handle.
typedef struct canon_callbacks canon_callbacks_t;
void interp_shim_set_callbacks(interp_handle_t *h,
                               const canon_callbacks_t *cb);

// Initialize the interpreter (loads parameter file, etc.)
int interp_shim_init(interp_handle_t *h);

// Open a G-code file for interpretation.
int interp_shim_open(interp_handle_t *h, const char *filename);

// Read the next line. Returns INTERP_OK (0), INTERP_ENDFILE (2), or error.
int interp_shim_read(interp_handle_t *h);

// Read a specific string (e.g. initcodes).
int interp_shim_read_string(interp_handle_t *h, const char *code);

// Execute the last-read line.
int interp_shim_execute(interp_handle_t *h);

// Get current sequence (line) number.
int interp_shim_sequence_number(interp_handle_t *h);

// Close the file and finalize.
int interp_shim_close(interp_handle_t *h);

// Get a numbered parameter value (for debugging).
double interp_shim_get_parameter(interp_handle_t *h, int index);

// Destroy the interpreter instance.
void interp_shim_destroy(interp_handle_t *h);

// Get the error text for the last error. Returns pointer to internal buffer.
void interp_shim_error_text(interp_handle_t *h, int error_code,
                            char *buf, int buf_size);

// Interpreter return codes
#define INTERP_SHIM_OK       0
#define INTERP_SHIM_EXIT     1
#define INTERP_SHIM_EXECUTE_FINISH 2
#define INTERP_SHIM_ENDFILE  3
#define INTERP_SHIM_ERROR   -1

#ifdef __cplusplus
}
#endif

#endif // INTERP_SHIM_H
