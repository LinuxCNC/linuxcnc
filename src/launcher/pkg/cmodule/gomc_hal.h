// gomc_hal.h — HAL component/pin/param/function API for gomc C modules.
//
// All HAL operations go through callbacks in gomc_hal_t, which the launcher
// populates at module load time.  Initially these callbacks delegate to the
// existing liblinuxcnchal.so implementation; they will be replaced with
// native Go implementations over time.
//
// Typed pin/param creation and printf-style name formatting are provided as
// inline convenience functions in this header — the callbacks themselves use
// generic (type-as-int) signatures to keep the vtable small.
//
// Usage:
//   int comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
//                                GOMC_HAL_COMP_REALTIME);
//   gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &my_pin, comp_id,
//                         "%s.enable", name);
//   env->hal->ready(env->hal->ctx, comp_id);

#ifndef GOMC_HAL_H
#define GOMC_HAL_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// HAL type / direction / component-type constants.
//
// These mirror the values in hal.h so that modules using the new API don't
// need to include the old header.  The numeric values MUST match hal.h.
// ---------------------------------------------------------------------------

typedef enum {
    GOMC_HAL_TYPE_UNSPECIFIED = -1,
    GOMC_HAL_BIT   = 1,
    GOMC_HAL_FLOAT = 2,
    GOMC_HAL_S32   = 3,
    GOMC_HAL_U32   = 4,
    GOMC_HAL_PORT  = 5,
} gomc_hal_type_t;

typedef enum {
    GOMC_HAL_IN  = 16,
    GOMC_HAL_OUT = 32,
    GOMC_HAL_IO  = (16 | 32),
} gomc_hal_pin_dir_t;

typedef enum {
    GOMC_HAL_RO = 64,
    GOMC_HAL_RW = (64 | 32),
} gomc_hal_param_dir_t;

typedef enum {
    GOMC_HAL_COMP_USER     = 0,
    GOMC_HAL_COMP_REALTIME = 1,
} gomc_hal_comp_type_t;

// HAL data types — these must match hal.h typedefs.
typedef volatile signed long       gomc_hal_s32_t;
typedef volatile unsigned long     gomc_hal_u32_t;
typedef volatile double            gomc_hal_float_t;
typedef volatile unsigned          gomc_hal_bit_t;
typedef volatile unsigned          gomc_hal_port_t;

// Maximum HAL name length (matches HAL_NAME_LEN in hal.h).
#define GOMC_HAL_NAME_LEN 127

// ---------------------------------------------------------------------------
// gomc_hal_t — HAL callback table.
// ---------------------------------------------------------------------------

typedef struct {
    void *ctx;

    // Component lifecycle.
    int   (*init)  (void *ctx, const char *name, void *dl_handle, int type);
    void  (*exit)  (void *ctx, int comp_id);
    int   (*ready) (void *ctx, int comp_id);

    // HAL shared-memory allocation (mlock'd, page-faulted).
    void *(*malloc)(void *ctx, long size);

    // Pin and parameter creation (generic, type passed as int).
    int   (*pin_new)  (void *ctx, const char *name, int type, int dir,
                       void **data_ptr_addr, int comp_id);
    int   (*param_new)(void *ctx, const char *name, int type, int dir,
                       void *data_addr, int comp_id);

    // RT function export.
    int   (*export_funct)(void *ctx, const char *name,
                          void (*funct)(void *, long),
                          void *arg, int uses_fp, int reentrant, int comp_id);
} gomc_hal_t;

// ---------------------------------------------------------------------------
// Convenience: generic pin_new with printf-style name.
// ---------------------------------------------------------------------------

static inline __attribute__((format(printf, 6, 7))) int
gomc_hal_pin_newf(const gomc_hal_t *hal, int type, int dir,
                  void **data_ptr_addr, int comp_id,
                  const char *fmt, ...) __attribute__((unused));
static inline int
gomc_hal_pin_newf(const gomc_hal_t *hal, int type, int dir,
                  void **data_ptr_addr, int comp_id,
                  const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->pin_new(hal->ctx, name, type, dir, data_ptr_addr, comp_id);
}

// ---------------------------------------------------------------------------
// Typed pin creation — type-safe wrappers with format checking.
// ---------------------------------------------------------------------------

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_pin_bit_newf(const gomc_hal_t *hal, gomc_hal_pin_dir_t dir,
                      gomc_hal_bit_t **data_ptr_addr, int comp_id,
                      const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->pin_new(hal->ctx, name, GOMC_HAL_BIT, dir,
                        (void **)data_ptr_addr, comp_id);
}

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_pin_float_newf(const gomc_hal_t *hal, gomc_hal_pin_dir_t dir,
                        gomc_hal_float_t **data_ptr_addr, int comp_id,
                        const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->pin_new(hal->ctx, name, GOMC_HAL_FLOAT, dir,
                        (void **)data_ptr_addr, comp_id);
}

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_pin_u32_newf(const gomc_hal_t *hal, gomc_hal_pin_dir_t dir,
                      gomc_hal_u32_t **data_ptr_addr, int comp_id,
                      const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->pin_new(hal->ctx, name, GOMC_HAL_U32, dir,
                        (void **)data_ptr_addr, comp_id);
}

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_pin_s32_newf(const gomc_hal_t *hal, gomc_hal_pin_dir_t dir,
                      gomc_hal_s32_t **data_ptr_addr, int comp_id,
                      const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->pin_new(hal->ctx, name, GOMC_HAL_S32, dir,
                        (void **)data_ptr_addr, comp_id);
}

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_pin_port_newf(const gomc_hal_t *hal, gomc_hal_pin_dir_t dir,
                       gomc_hal_port_t **data_ptr_addr, int comp_id,
                       const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->pin_new(hal->ctx, name, GOMC_HAL_PORT, dir,
                        (void **)data_ptr_addr, comp_id);
}

// ---------------------------------------------------------------------------
// Convenience: generic param_new with printf-style name.
// ---------------------------------------------------------------------------

static inline __attribute__((format(printf, 6, 7))) int
gomc_hal_param_newf(const gomc_hal_t *hal, int type, int dir,
                    void *data_addr, int comp_id,
                    const char *fmt, ...) __attribute__((unused));
static inline int
gomc_hal_param_newf(const gomc_hal_t *hal, int type, int dir,
                    void *data_addr, int comp_id,
                    const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->param_new(hal->ctx, name, type, dir, data_addr, comp_id);
}

// ---------------------------------------------------------------------------
// Typed param creation — type-safe wrappers with format checking.
// ---------------------------------------------------------------------------

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_param_bit_newf(const gomc_hal_t *hal, gomc_hal_param_dir_t dir,
                        gomc_hal_bit_t *data_addr, int comp_id,
                        const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->param_new(hal->ctx, name, GOMC_HAL_BIT, dir,
                          (void *)data_addr, comp_id);
}

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_param_float_newf(const gomc_hal_t *hal, gomc_hal_param_dir_t dir,
                          gomc_hal_float_t *data_addr, int comp_id,
                          const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->param_new(hal->ctx, name, GOMC_HAL_FLOAT, dir,
                          (void *)data_addr, comp_id);
}

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_param_u32_newf(const gomc_hal_t *hal, gomc_hal_param_dir_t dir,
                        gomc_hal_u32_t *data_addr, int comp_id,
                        const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->param_new(hal->ctx, name, GOMC_HAL_U32, dir,
                          (void *)data_addr, comp_id);
}

static inline __attribute__((format(printf, 5, 6))) int
gomc_hal_param_s32_newf(const gomc_hal_t *hal, gomc_hal_param_dir_t dir,
                        gomc_hal_s32_t *data_addr, int comp_id,
                        const char *fmt, ...) {
    char name[GOMC_HAL_NAME_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    return hal->param_new(hal->ctx, name, GOMC_HAL_S32, dir,
                          (void *)data_addr, comp_id);
}

#ifdef __cplusplus
}
#endif

#endif // GOMC_HAL_H
