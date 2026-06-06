// GMI JSON Utilities (wraps cJSON)
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GMI_JSON_H
#define GMI_JSON_H

#include "gmi_types.h"
#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── JSON Parsing Helpers ───

// Parse JSON string. Caller must free with cJSON_Delete().
// Returns NULL on parse error.
cJSON *gmi_json_parse(const char *json_str);

// Parse JSON from buffer.
cJSON *gmi_json_parse_buf(const gmi_buf_t *buf);

// ─── Type-safe Getters ───
// These return default values on missing/wrong-type fields.

bool gmi_json_get_bool(const cJSON *obj, const char *key, bool def);
int32_t gmi_json_get_i32(const cJSON *obj, const char *key, int32_t def);
uint32_t gmi_json_get_u32(const cJSON *obj, const char *key, uint32_t def);
int64_t gmi_json_get_i64(const cJSON *obj, const char *key, int64_t def);
uint64_t gmi_json_get_u64(const cJSON *obj, const char *key, uint64_t def);
double gmi_json_get_f64(const cJSON *obj, const char *key, double def);

// Returns NULL if missing or not a string. Returned pointer is valid while obj exists.
const char *gmi_json_get_str(const cJSON *obj, const char *key);

// Get array field. Returns NULL if missing or not an array.
cJSON *gmi_json_get_array(const cJSON *obj, const char *key);

// Get object field. Returns NULL if missing or not an object.
cJSON *gmi_json_get_object(const cJSON *obj, const char *key);

// ─── JSON Building Helpers ───

// Create a new JSON object.
cJSON *gmi_json_object(void);

// Create a new JSON array.
cJSON *gmi_json_array(void);

// Add fields to object (returns false on allocation failure).
bool gmi_json_add_bool(cJSON *obj, const char *key, bool val);
bool gmi_json_add_i32(cJSON *obj, const char *key, int32_t val);
bool gmi_json_add_u32(cJSON *obj, const char *key, uint32_t val);
bool gmi_json_add_i64(cJSON *obj, const char *key, int64_t val);
bool gmi_json_add_u64(cJSON *obj, const char *key, uint64_t val);
bool gmi_json_add_f64(cJSON *obj, const char *key, double val);
bool gmi_json_add_str(cJSON *obj, const char *key, const char *val);
bool gmi_json_add_null(cJSON *obj, const char *key);

// Serialize JSON to buffer. Returns GMI_OK or error.
int gmi_json_to_buf(const cJSON *json, gmi_buf_t *buf);

// Serialize JSON to newly allocated string. Caller must free().
char *gmi_json_to_str(const cJSON *json);

#ifdef __cplusplus
}
#endif

#endif // GMI_JSON_H
