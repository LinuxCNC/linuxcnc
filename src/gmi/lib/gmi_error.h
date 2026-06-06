// GMI Error Codes and Handling
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GMI_ERROR_H
#define GMI_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

// ─── Error Codes ───
// All GMI errors are negative. HTTP status codes (>=100) are returned as-is.

#define GMI_OK              0      // Success
#define GMI_ERR_ALLOC      -1      // Memory allocation failed
#define GMI_ERR_CURL       -2      // CURL operation failed
#define GMI_ERR_JSON       -3      // JSON parse/encode error
#define GMI_ERR_OVERFLOW   -4      // Buffer overflow
#define GMI_ERR_INVALID    -5      // Invalid argument
#define GMI_ERR_NOT_FOUND  -6      // Resource not found
#define GMI_ERR_TIMEOUT    -7      // Operation timed out
#define GMI_ERR_IO         -8      // I/O error

// Returns a human-readable error string for the given error code.
// For HTTP status codes (>=100), returns a generic HTTP error message.
const char *gmi_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif // GMI_ERROR_H
