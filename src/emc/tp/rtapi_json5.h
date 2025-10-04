/**
 * @file rtapi_json5.h
 *
 * @author Robert W. Ellenberg
 * @copyright Copyright (c) 2019 All rights reserved. This project is released under the GPL v2 license.
 */

#ifndef RTAPI_JSON5_H
#define RTAPI_JSON5_H

#include "rtapi.h"  /* printing functions */

// Macro wrappers to stringify the variable name
// NOTE: callers can directly call the underlying print function if a custom field name is needed
#define print_json5_double(varname_) print_json5_double_(#varname_, (double)varname_)
#define print_json5_string(varname_) print_json5_string_(#varname_, varname_)
#define print_json5_long(varname_) print_json5_long_(#varname_, (long)varname_)
#define print_json5_int(varname_) print_json5_int_(#varname_, (int)varname_)
#define print_json5_unsigned(varname_) print_json5_unsigned_(#varname_, (unsigned)varname_)

#define print_json5_double_field(base_, varname_) print_json5_double_(#varname_, (base_)->varname_)
#define print_json5_string_field(base_, varname_) print_json5_string_(#varname_, (base_)->varname_ ?:"NULL")
#define print_json5_int_field(base_, varname_) print_json5_int_(#varname_, (base_)->varname_)
#define print_json5_unsigned_field(base_, varname_) print_json5_unsigned_(#varname_, (base_)->varname_)
#define print_json5_long_field(base_, varname_) print_json5_long_(#varname_, (base_)->varname_)
#define print_json5_long_long_field(base_, varname_) print_json5_long_long_(#varname_, (base_)->varname_)

// Hacky way to do realtime-logging of
// KLUDGE shove these in the header to avoid linking issues

static inline void print_json5_start_()
{
    rtapi_print("{");
}

static inline void print_json5_end_()
{
    rtapi_print("}\n");
}

static inline void print_json5_fieldname_(const char *fname)
{
    rtapi_print("%s: ", fname ?: "NULL");
}

static inline void print_json5_array_start_(const char *arr_name)
{
    if (arr_name) {
        rtapi_print("%s: [", arr_name);
    } else {

        rtapi_print("[");
    }
}

static inline void print_json5_array_end_()
{
    rtapi_print("]\n");
}

static inline void print_json5_object_start_(const char *fname)
{
    if (fname) {
        rtapi_print("%s: {", fname);
    } else {
        rtapi_print("{");
    }
}

static inline void print_json5_object_end_()
{
    rtapi_print("}, ");
}

static inline void print_json5_double_(const char *varname, double value)
{
    rtapi_print("%s: %0.17g, ", varname ?: "NO_NAME", value);
}

static inline void print_json5_bool_(const char *varname, double value)
{
    rtapi_print("%s: %s, ", varname ?: "NO_NAME", value ? "true" : "false");
}

static inline void print_json5_int_(const char *varname, int value)
{
    rtapi_print("%s: %d, ", varname ?: "NO_NAME", value);
}

static inline void print_json5_long_(const char *varname, long value)
{
    rtapi_print("%s: %ld, ", varname ?: "NO_NAME", value);
}

static inline void print_json5_long_long_(const char *varname, long long value)
{
    rtapi_print("%s: %lld, ", varname ?: "NO_NAME", value);
}

static inline void print_json5_unsigned_(const char *varname, unsigned value)
{
    rtapi_print("%s: %u, ", varname ?: "NO_NAME", value);
}

static inline void print_json5_string_(const char *field_name, const char *value)
{
    // TODO handle nulls gracefully with proper JSON null
    rtapi_print("%s: \"%s\", ", field_name ?: "NULL", value ?: "NULL");
}

#endif // RTAPI_JSON5_H
