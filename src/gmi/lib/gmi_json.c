// GMI JSON Implementation
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gmi_json.h"
#include "gmi_error.h"
#include <stdlib.h>
#include <string.h>

// ─── Parsing ───

cJSON *gmi_json_parse(const char *json_str) {
    if (!json_str) {
        return NULL;
    }
    return cJSON_Parse(json_str);
}

cJSON *gmi_json_parse_buf(const gmi_buf_t *buf) {
    if (!buf || !buf->data) {
        return NULL;
    }
    return cJSON_ParseWithLength(buf->data, buf->size);
}

// ─── Type-safe Getters ───

bool gmi_json_get_bool(const cJSON *obj, const char *key, bool def) {
    if (!obj || !key) return def;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return def;
    if (cJSON_IsTrue(item)) return true;
    if (cJSON_IsFalse(item)) return false;
    return def;
}

int32_t gmi_json_get_i32(const cJSON *obj, const char *key, int32_t def) {
    if (!obj || !key) return def;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return (int32_t)cJSON_GetNumberValue(item);
}

uint32_t gmi_json_get_u32(const cJSON *obj, const char *key, uint32_t def) {
    if (!obj || !key) return def;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    double v = cJSON_GetNumberValue(item);
    if (v < 0) return def;
    return (uint32_t)v;
}

int64_t gmi_json_get_i64(const cJSON *obj, const char *key, int64_t def) {
    if (!obj || !key) return def;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return (int64_t)cJSON_GetNumberValue(item);
}

uint64_t gmi_json_get_u64(const cJSON *obj, const char *key, uint64_t def) {
    if (!obj || !key) return def;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    double v = cJSON_GetNumberValue(item);
    if (v < 0) return def;
    return (uint64_t)v;
}

double gmi_json_get_f64(const cJSON *obj, const char *key, double def) {
    if (!obj || !key) return def;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return cJSON_GetNumberValue(item);
}

const char *gmi_json_get_str(const cJSON *obj, const char *key) {
    if (!obj || !key) return NULL;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsString(item)) return NULL;
    return cJSON_GetStringValue(item);
}

cJSON *gmi_json_get_array(const cJSON *obj, const char *key) {
    if (!obj || !key) return NULL;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsArray(item)) return NULL;
    return item;
}

cJSON *gmi_json_get_object(const cJSON *obj, const char *key) {
    if (!obj || !key) return NULL;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsObject(item)) return NULL;
    return item;
}

// ─── Building ───

cJSON *gmi_json_object(void) {
    return cJSON_CreateObject();
}

cJSON *gmi_json_array(void) {
    return cJSON_CreateArray();
}

bool gmi_json_add_bool(cJSON *obj, const char *key, bool val) {
    if (!obj || !key) return false;
    return cJSON_AddBoolToObject(obj, key, val) != NULL;
}

bool gmi_json_add_i32(cJSON *obj, const char *key, int32_t val) {
    if (!obj || !key) return false;
    return cJSON_AddNumberToObject(obj, key, (double)val) != NULL;
}

bool gmi_json_add_u32(cJSON *obj, const char *key, uint32_t val) {
    if (!obj || !key) return false;
    return cJSON_AddNumberToObject(obj, key, (double)val) != NULL;
}

bool gmi_json_add_i64(cJSON *obj, const char *key, int64_t val) {
    if (!obj || !key) return false;
    return cJSON_AddNumberToObject(obj, key, (double)val) != NULL;
}

bool gmi_json_add_u64(cJSON *obj, const char *key, uint64_t val) {
    if (!obj || !key) return false;
    return cJSON_AddNumberToObject(obj, key, (double)val) != NULL;
}

bool gmi_json_add_f64(cJSON *obj, const char *key, double val) {
    if (!obj || !key) return false;
    return cJSON_AddNumberToObject(obj, key, val) != NULL;
}

bool gmi_json_add_str(cJSON *obj, const char *key, const char *val) {
    if (!obj || !key) return false;
    if (!val) {
        return cJSON_AddNullToObject(obj, key) != NULL;
    }
    return cJSON_AddStringToObject(obj, key, val) != NULL;
}

bool gmi_json_add_null(cJSON *obj, const char *key) {
    if (!obj || !key) return false;
    return cJSON_AddNullToObject(obj, key) != NULL;
}

// ─── Serialization ───

int gmi_json_to_buf(const cJSON *json, gmi_buf_t *buf) {
    if (!json || !buf) {
        return GMI_ERR_INVALID;
    }

    char *str = cJSON_PrintUnformatted(json);
    if (!str) {
        return GMI_ERR_JSON;
    }

    int err = gmi_buf_append_str(buf, str);
    free(str);
    return err;
}

char *gmi_json_to_str(const cJSON *json) {
    if (!json) {
        return NULL;
    }
    return cJSON_PrintUnformatted(json);
}
