// GMI Error Implementation
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gmi_error.h"

const char *gmi_strerror(int err) {
    if (err >= 0) {
        return "Success";
    }

    switch (err) {
    case GMI_ERR_ALLOC:
        return "Memory allocation failed";
    case GMI_ERR_CURL:
        return "CURL operation failed";
    case GMI_ERR_JSON:
        return "JSON parse/encode error";
    case GMI_ERR_OVERFLOW:
        return "Buffer overflow";
    case GMI_ERR_INVALID:
        return "Invalid argument";
    case GMI_ERR_NOT_FOUND:
        return "Resource not found";
    case GMI_ERR_TIMEOUT:
        return "Operation timed out";
    case GMI_ERR_IO:
        return "I/O error";
    default:
        if (err >= 100 && err < 600) {
            return "HTTP error";
        }
        return "Unknown error";
    }
}
