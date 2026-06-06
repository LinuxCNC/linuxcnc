// GMI Common Types
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GMI_TYPES_H
#define GMI_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Configuration ───

// Maximum URL length for REST requests
#ifndef GMI_URL_MAX
#define GMI_URL_MAX 2048
#endif

// Default HTTP timeout in seconds
#ifndef GMI_HTTP_TIMEOUT
#define GMI_HTTP_TIMEOUT 30
#endif

// Initial response buffer size
#ifndef GMI_RESPONSE_INITIAL_SIZE
#define GMI_RESPONSE_INITIAL_SIZE 4096
#endif

// ─── Dynamic Buffer ───

// A growable byte buffer used for HTTP responses and JSON building.
typedef struct {
    char *data;      // Buffer contents (null-terminated for string use)
    size_t size;     // Current content size (excluding null terminator)
    size_t capacity; // Allocated capacity
} gmi_buf_t;

// Initialize a buffer. Returns GMI_OK or GMI_ERR_ALLOC.
int gmi_buf_init(gmi_buf_t *buf, size_t initial_capacity);

// Append data to buffer, growing if needed. Returns GMI_OK or GMI_ERR_ALLOC.
int gmi_buf_append(gmi_buf_t *buf, const void *data, size_t len);

// Append a null-terminated string.
int gmi_buf_append_str(gmi_buf_t *buf, const char *str);

// Append formatted string (printf-style).
int gmi_buf_printf(gmi_buf_t *buf, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

// Reset buffer to empty (keeps allocated memory).
void gmi_buf_reset(gmi_buf_t *buf);

// Free buffer memory.
void gmi_buf_free(gmi_buf_t *buf);

// ─── String Slice ───

// A non-owning view into a string (not necessarily null-terminated).
typedef struct {
    const char *ptr;
    size_t len;
} gmi_str_t;

// Create a string slice from a null-terminated string.
static inline gmi_str_t gmi_str(const char *s) {
    gmi_str_t str = {s, s ? __builtin_strlen(s) : 0};
    return str;
}

// Create a string slice from pointer and length.
static inline gmi_str_t gmi_str_n(const char *s, size_t len) {
    gmi_str_t str = {s, len};
    return str;
}

#ifdef __cplusplus
}
#endif

#endif // GMI_TYPES_H
