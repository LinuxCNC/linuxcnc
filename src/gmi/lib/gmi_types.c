// GMI Types Implementation
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gmi_types.h"
#include "gmi_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// ─── Buffer Implementation ───

int gmi_buf_init(gmi_buf_t *buf, size_t initial_capacity) {
    if (!buf) {
        return GMI_ERR_INVALID;
    }
    if (initial_capacity == 0) {
        initial_capacity = GMI_RESPONSE_INITIAL_SIZE;
    }
    buf->data = malloc(initial_capacity);
    if (!buf->data) {
        return GMI_ERR_ALLOC;
    }
    buf->data[0] = '\0';
    buf->size = 0;
    buf->capacity = initial_capacity;
    return GMI_OK;
}

static int gmi_buf_grow(gmi_buf_t *buf, size_t needed) {
    if (buf->size + needed + 1 <= buf->capacity) {
        return GMI_OK;
    }

    size_t new_cap = buf->capacity * 2;
    if (new_cap < buf->size + needed + 1) {
        new_cap = buf->size + needed + 1;
    }

    char *new_data = realloc(buf->data, new_cap);
    if (!new_data) {
        return GMI_ERR_ALLOC;
    }

    buf->data = new_data;
    buf->capacity = new_cap;
    return GMI_OK;
}

int gmi_buf_append(gmi_buf_t *buf, const void *data, size_t len) {
    if (!buf || !data) {
        return GMI_ERR_INVALID;
    }

    int err = gmi_buf_grow(buf, len);
    if (err != GMI_OK) {
        return err;
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
    return GMI_OK;
}

int gmi_buf_append_str(gmi_buf_t *buf, const char *str) {
    if (!str) {
        return GMI_ERR_INVALID;
    }
    return gmi_buf_append(buf, str, strlen(str));
}

int gmi_buf_printf(gmi_buf_t *buf, const char *fmt, ...) {
    if (!buf || !fmt) {
        return GMI_ERR_INVALID;
    }

    va_list args;
    va_start(args, fmt);

    // First, try to format into remaining buffer space
    size_t remaining = buf->capacity - buf->size;
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(buf->data + buf->size, remaining, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return GMI_ERR_INVALID;
    }

    if ((size_t)needed >= remaining) {
        // Need to grow buffer
        int err = gmi_buf_grow(buf, (size_t)needed);
        if (err != GMI_OK) {
            va_end(args);
            return err;
        }
        vsnprintf(buf->data + buf->size, buf->capacity - buf->size, fmt, args);
    }

    buf->size += (size_t)needed;
    va_end(args);
    return GMI_OK;
}

void gmi_buf_reset(gmi_buf_t *buf) {
    if (buf && buf->data) {
        buf->size = 0;
        buf->data[0] = '\0';
    }
}

void gmi_buf_free(gmi_buf_t *buf) {
    if (buf) {
        free(buf->data);
        buf->data = NULL;
        buf->size = 0;
        buf->capacity = 0;
    }
}
