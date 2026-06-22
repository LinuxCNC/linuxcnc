/*
 * gmi_http.h — GMI HTTP Client Utilities (wraps libcurl)
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

#ifndef GMI_HTTP_H
#define GMI_HTTP_H

#include "gmi_types.h"
#include "gmi_error.h"
#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── HTTP Methods ───

typedef enum {
    GMI_HTTP_GET,
    GMI_HTTP_POST,
    GMI_HTTP_PUT,
    GMI_HTTP_PATCH,
    GMI_HTTP_DELETE
} gmi_http_method_t;

// ─── HTTP Client ───
//
// Each gmi_http_t owns a persistent CURL handle for connection pooling
// (TCP keep-alive, TLS session reuse). A single gmi_http_t must only be
// used from one thread at a time. For concurrent access, create separate
// gmi_http_t instances.

typedef struct gmi_http gmi_http_t;

// Create a new HTTP client. base_url is copied.
// Example: gmi_http_new("http://localhost:8080/api/v1")
gmi_http_t *gmi_http_new(const char *base_url);

// Free HTTP client and release resources.
void gmi_http_free(gmi_http_t *http);

// Set request timeout in seconds.
void gmi_http_set_timeout(gmi_http_t *http, long timeout_sec);

// ─── Request Building ───

typedef struct gmi_request gmi_request_t;

// Create a new request. Path is appended to base_url.
// Example: gmi_request_new(http, GMI_HTTP_GET, "/pins")
gmi_request_t *gmi_request_new(gmi_http_t *http, gmi_http_method_t method, const char *path);

// Free request (also frees response data if any).
void gmi_request_free(gmi_request_t *req);

// Add path parameter substitution. Path should contain {name}.
// Example: gmi_request_path_param(req, "name", "axis.0.pos-cmd")
// Transforms "/pin/{name}" -> "/pin/axis.0.pos-cmd"
int gmi_request_path_param(gmi_request_t *req, const char *name, const char *value);

// Add query parameter (URL-encoded automatically).
int gmi_request_query_param(gmi_request_t *req, const char *name, const char *value);

// Set JSON request body from cJSON object.
int gmi_request_set_json(gmi_request_t *req, const cJSON *json);

// Set raw request body.
int gmi_request_set_body(gmi_request_t *req, const char *content_type, const void *data, size_t len);

// ─── Request Execution ───

// Execute the request. Returns GMI_OK on success (HTTP 2xx).
// On HTTP error, returns the HTTP status code (>=100).
// On transport error, returns negative GMI_ERR_* code.
int gmi_request_execute(gmi_request_t *req);

// Get HTTP status code after execution.
int gmi_request_status(const gmi_request_t *req);

// Get response body after execution. Valid until request is freed.
const gmi_buf_t *gmi_request_response(const gmi_request_t *req);

// Parse response body as JSON. Caller must cJSON_Delete().
cJSON *gmi_request_response_json(const gmi_request_t *req);

// ─── Convenience Functions ───

// Simple GET request returning JSON. Caller must cJSON_Delete().
// Returns NULL on error, sets *err if provided.
cJSON *gmi_http_get_json(gmi_http_t *http, const char *path, int *err);

// Simple POST request with JSON body, returning JSON response.
cJSON *gmi_http_post_json(gmi_http_t *http, const char *path, const cJSON *body, int *err);

// Simple PUT request with JSON body, returning JSON response.
cJSON *gmi_http_put_json(gmi_http_t *http, const char *path, const cJSON *body, int *err);

// Simple DELETE request returning JSON response.
cJSON *gmi_http_delete_json(gmi_http_t *http, const char *path, int *err);

#ifdef __cplusplus
}
#endif

#endif // GMI_HTTP_H
