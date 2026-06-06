// Standalone compatibility header for classicladder UI
// Provides stubs for HAL/RTAPI functions when building without liblinuxcnchal
// Used for evaluation/testing of the ladder editor UI in isolation

#ifndef STANDALONE_COMPAT_H
#define STANDALONE_COMPAT_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// RTAPI message levels (from rtapi.h)
#define RTAPI_MSG_NONE 0
#define RTAPI_MSG_ERR 1
#define RTAPI_MSG_WARN 2
#define RTAPI_MSG_INFO 3
#define RTAPI_MSG_DBG 4
#define RTAPI_MSG_ALL 5

// rtapi_print stubs
static inline void rtapi_print(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static inline void rtapi_print_msg(int level, const char *fmt, ...) {
    (void)level;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static inline int rtapi_get_msg_level(void) { return RTAPI_MSG_INFO; }
static inline void rtapi_set_msg_level(int level) { (void)level; }

// HAL stubs
typedef int hal_bit_t;
typedef int hal_s32_t;
typedef double hal_float_t;

#define HAL_NAME_LEN 47
#define HAL_IN 0
#define HAL_OUT 1
#define HAL_RO 0

static int _standalone_comp_id = 1;

static inline int hal_init(const char *name) { (void)name; return _standalone_comp_id; }
static inline void hal_exit(int comp_id) { (void)comp_id; }
static inline void hal_ready(int comp_id) { (void)comp_id; }

// rtapi_string.h replacements
#ifdef __cplusplus
#include <type_traits>
#define rtapi_static_assert(a,b) static_assert(a,b)
#define rtapi_is_array(x) (std::is_array<decltype(x)>::value)
#else
#define rtapi_static_assert(a,b) _Static_assert(a,b)
#define rtapi_is_array(x) (!__builtin_types_compatible_p(__typeof__((x)), __typeof__(&(x)[0])))
#endif

static inline size_t rtapi_strlcpy(char *dst, const char *src, size_t size) {
    return snprintf(dst, size, "%s", src);
}
#define rtapi_strxcpy(dst, src) ({ \
    rtapi_static_assert(rtapi_is_array(dst), "dst must be non-const array"); \
    rtapi_strlcpy(dst, src, sizeof(dst)); \
})

static inline size_t rtapi_strlcat(char *dst, const char *src, size_t size) {
    size_t l = strlen(dst);
    return snprintf(dst+l, size-l, "%s", src);
}
#define rtapi_strxcat(dst, src) ({ \
    rtapi_static_assert(rtapi_is_array(dst), "dst must be non-const array"); \
    rtapi_strlcat(dst, src, sizeof(dst)); \
})

// hal_priv.h stubs for emc_mods.c
typedef struct { char name[HAL_NAME_LEN+1]; } hal_sig_t;
typedef struct { hal_sig_t *signal; } hal_pin_t;

static inline hal_pin_t *halpr_find_pin_by_name(const char *name) {
    (void)name;
    return NULL;
}

#endif // STANDALONE_COMPAT_H
