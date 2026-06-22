/*
 * gmi_error.h — GMI Error Codes and Handling
 *
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
