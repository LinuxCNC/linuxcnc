// GMI HTTP Implementation
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gmi_http.h"
#include "gmi_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

// ─── HTTP Client Structure ───

struct gmi_http {
    char *base_url;
    long timeout;
    CURL *curl;   // persistent handle — reused across requests for connection pooling
};

// ─── Request Structure ───

struct gmi_request {
    gmi_http_t *http;
    gmi_http_method_t method;
    char url[GMI_URL_MAX];
    struct curl_slist *headers;
    gmi_buf_t body;
    gmi_buf_t response;
    int status;
};

// ─── Curl Write Callback ───

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    gmi_buf_t *buf = (gmi_buf_t *)userp;
    if (gmi_buf_append(buf, contents, total) != GMI_OK) {
        return 0;
    }
    return total;
}

// ─── HTTP Client ───

gmi_http_t *gmi_http_new(const char *base_url) {
    if (!base_url) {
        return NULL;
    }

    gmi_http_t *http = calloc(1, sizeof(*http));
    if (!http) {
        return NULL;
    }

    http->base_url = strdup(base_url);
    if (!http->base_url) {
        free(http);
        return NULL;
    }

    http->curl = curl_easy_init();
    if (!http->curl) {
        free(http->base_url);
        free(http);
        return NULL;
    }

    http->timeout = GMI_HTTP_TIMEOUT;
    return http;
}

void gmi_http_free(gmi_http_t *http) {
    if (!http) {
        return;
    }
    if (http->curl) {
        curl_easy_cleanup(http->curl);
    }
    free(http->base_url);
    free(http);
}

void gmi_http_set_timeout(gmi_http_t *http, long timeout_sec) {
    if (http) {
        http->timeout = timeout_sec;
    }
}

// ─── Request Building ───

gmi_request_t *gmi_request_new(gmi_http_t *http, gmi_http_method_t method, const char *path) {
    if (!http || !path) {
        return NULL;
    }

    gmi_request_t *req = calloc(1, sizeof(*req));
    if (!req) {
        return NULL;
    }

    req->http = http;
    req->method = method;
    req->status = 0;

    // Build URL
    int n = snprintf(req->url, sizeof(req->url), "%s%s", http->base_url, path);
    if (n < 0 || (size_t)n >= sizeof(req->url)) {
        free(req);
        return NULL;
    }

    // Initialize buffers
    if (gmi_buf_init(&req->body, 256) != GMI_OK ||
        gmi_buf_init(&req->response, GMI_RESPONSE_INITIAL_SIZE) != GMI_OK) {
        gmi_buf_free(&req->body);
        gmi_buf_free(&req->response);
        free(req);
        return NULL;
    }

    return req;
}

void gmi_request_free(gmi_request_t *req) {
    if (!req) {
        return;
    }
    if (req->headers) {
        curl_slist_free_all(req->headers);
    }
    gmi_buf_free(&req->body);
    gmi_buf_free(&req->response);
    free(req);
}

int gmi_request_path_param(gmi_request_t *req, const char *name, const char *value) {
    if (!req || !name || !value) {
        return GMI_ERR_INVALID;
    }

    // Build placeholder string "{name}"
    char placeholder[128];
    int n = snprintf(placeholder, sizeof(placeholder), "{%s}", name);
    if (n < 0 || (size_t)n >= sizeof(placeholder)) {
        return GMI_ERR_OVERFLOW;
    }

    // Find placeholder in URL
    char *pos = strstr(req->url, placeholder);
    if (!pos) {
        return GMI_ERR_NOT_FOUND;
    }

    // URL-encode the value using the client's persistent handle
    char *encoded = curl_easy_escape(req->http->curl, value, 0);
    if (!encoded) {
        return GMI_ERR_ALLOC;
    }

    // Build new URL
    size_t prefix_len = (size_t)(pos - req->url);
    size_t suffix_start = prefix_len + strlen(placeholder);
    size_t encoded_len = strlen(encoded);
    size_t suffix_len = strlen(req->url + suffix_start);

    if (prefix_len + encoded_len + suffix_len >= sizeof(req->url)) {
        curl_free(encoded);
        return GMI_ERR_OVERFLOW;
    }

    // Shift suffix and insert encoded value
    memmove(pos + encoded_len, pos + strlen(placeholder), suffix_len + 1);
    memcpy(pos, encoded, encoded_len);

    curl_free(encoded);
    return GMI_OK;
}

int gmi_request_query_param(gmi_request_t *req, const char *name, const char *value) {
    if (!req || !name || !value) {
        return GMI_ERR_INVALID;
    }

    char *encoded_name = curl_easy_escape(req->http->curl, name, 0);
    char *encoded_value = curl_easy_escape(req->http->curl, value, 0);
    if (!encoded_name || !encoded_value) {
        curl_free(encoded_name);
        curl_free(encoded_value);
        return GMI_ERR_ALLOC;
    }

    // Determine separator (? or &)
    char sep = strchr(req->url, '?') ? '&' : '?';

    size_t current_len = strlen(req->url);
    size_t needed = strlen(encoded_name) + strlen(encoded_value) + 2; // sep + = + names
    if (current_len + needed >= sizeof(req->url)) {
        curl_free(encoded_name);
        curl_free(encoded_value);
        return GMI_ERR_OVERFLOW;
    }

    snprintf(req->url + current_len, sizeof(req->url) - current_len,
             "%c%s=%s", sep, encoded_name, encoded_value);

    curl_free(encoded_name);
    curl_free(encoded_value);
    return GMI_OK;
}

int gmi_request_set_json(gmi_request_t *req, const cJSON *json) {
    if (!req || !json) {
        return GMI_ERR_INVALID;
    }

    gmi_buf_reset(&req->body);
    int err = gmi_json_to_buf(json, &req->body);
    if (err != GMI_OK) {
        return err;
    }

    req->headers = curl_slist_append(req->headers, "Content-Type: application/json");
    if (!req->headers) {
        return GMI_ERR_ALLOC;
    }

    return GMI_OK;
}

int gmi_request_set_body(gmi_request_t *req, const char *content_type, const void *data, size_t len) {
    if (!req || !content_type || !data) {
        return GMI_ERR_INVALID;
    }

    gmi_buf_reset(&req->body);
    int err = gmi_buf_append(&req->body, data, len);
    if (err != GMI_OK) {
        return err;
    }

    char header[256];
    snprintf(header, sizeof(header), "Content-Type: %s", content_type);
    req->headers = curl_slist_append(req->headers, header);
    if (!req->headers) {
        return GMI_ERR_ALLOC;
    }

    return GMI_OK;
}

// ─── Request Execution ───

int gmi_request_execute(gmi_request_t *req) {
    if (!req) {
        return GMI_ERR_INVALID;
    }

    CURL *curl = req->http->curl;

    // Reset handle state from previous request, keep connection pool
    curl_easy_reset(curl);

    // URL
    curl_easy_setopt(curl, CURLOPT_URL, req->url);

    // Timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, req->http->timeout);

    // Response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req->response);

    // Method and body
    switch (req->method) {
    case GMI_HTTP_GET:
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        break;
    case GMI_HTTP_POST:
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (req->body.size > 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body.data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req->body.size);
        }
        break;
    case GMI_HTTP_PUT:
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (req->body.size > 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body.data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req->body.size);
        }
        break;
    case GMI_HTTP_PATCH:
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (req->body.size > 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body.data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req->body.size);
        }
        break;
    case GMI_HTTP_DELETE:
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    }

    // Headers
    if (req->headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req->headers);
    }

    // Execute
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        return GMI_ERR_CURL;
    }

    // Get status code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    req->status = (int)http_code;

    // Return HTTP status for non-2xx
    if (http_code < 200 || http_code >= 300) {
        return (int)http_code;
    }

    return GMI_OK;
}

int gmi_request_status(const gmi_request_t *req) {
    return req ? req->status : 0;
}

const gmi_buf_t *gmi_request_response(const gmi_request_t *req) {
    return req ? &req->response : NULL;
}

cJSON *gmi_request_response_json(const gmi_request_t *req) {
    if (!req || req->response.size == 0) {
        return NULL;
    }
    return gmi_json_parse_buf(&req->response);
}

// ─── Convenience Functions ───

cJSON *gmi_http_get_json(gmi_http_t *http, const char *path, int *err) {
    gmi_request_t *req = gmi_request_new(http, GMI_HTTP_GET, path);
    if (!req) {
        if (err) *err = GMI_ERR_ALLOC;
        return NULL;
    }

    int rc = gmi_request_execute(req);
    if (rc != GMI_OK) {
        if (err) *err = rc;
        gmi_request_free(req);
        return NULL;
    }

    cJSON *json = gmi_request_response_json(req);
    if (!json) {
        if (err) *err = GMI_ERR_JSON;
    } else {
        if (err) *err = GMI_OK;
    }

    gmi_request_free(req);
    return json;
}

cJSON *gmi_http_post_json(gmi_http_t *http, const char *path, const cJSON *body, int *err) {
    gmi_request_t *req = gmi_request_new(http, GMI_HTTP_POST, path);
    if (!req) {
        if (err) *err = GMI_ERR_ALLOC;
        return NULL;
    }

    if (body) {
        int rc = gmi_request_set_json(req, body);
        if (rc != GMI_OK) {
            if (err) *err = rc;
            gmi_request_free(req);
            return NULL;
        }
    }

    int rc = gmi_request_execute(req);
    if (rc != GMI_OK) {
        if (err) *err = rc;
        gmi_request_free(req);
        return NULL;
    }

    cJSON *json = gmi_request_response_json(req);
    if (!json) {
        if (err) *err = GMI_ERR_JSON;
    } else {
        if (err) *err = GMI_OK;
    }

    gmi_request_free(req);
    return json;
}

cJSON *gmi_http_put_json(gmi_http_t *http, const char *path, const cJSON *body, int *err) {
    gmi_request_t *req = gmi_request_new(http, GMI_HTTP_PUT, path);
    if (!req) {
        if (err) *err = GMI_ERR_ALLOC;
        return NULL;
    }

    if (body) {
        int rc = gmi_request_set_json(req, body);
        if (rc != GMI_OK) {
            if (err) *err = rc;
            gmi_request_free(req);
            return NULL;
        }
    }

    int rc = gmi_request_execute(req);
    if (rc != GMI_OK) {
        if (err) *err = rc;
        gmi_request_free(req);
        return NULL;
    }

    cJSON *json = gmi_request_response_json(req);
    if (!json) {
        if (err) *err = GMI_ERR_JSON;
    } else {
        if (err) *err = GMI_OK;
    }

    gmi_request_free(req);
    return json;
}

cJSON *gmi_http_delete_json(gmi_http_t *http, const char *path, int *err) {
    gmi_request_t *req = gmi_request_new(http, GMI_HTTP_DELETE, path);
    if (!req) {
        if (err) *err = GMI_ERR_ALLOC;
        return NULL;
    }

    int rc = gmi_request_execute(req);
    if (rc != GMI_OK) {
        if (err) *err = rc;
        gmi_request_free(req);
        return NULL;
    }

    cJSON *json = gmi_request_response_json(req);
    if (!json && req->response.size > 0) {
        if (err) *err = GMI_ERR_JSON;
    } else {
        if (err) *err = GMI_OK;
    }

    gmi_request_free(req);
    return json;
}
