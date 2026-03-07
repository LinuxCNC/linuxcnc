/* Stub hal.h for test builds — replaces the real LinuxCNC HAL header */
#ifndef _HAL_H_
#define _HAL_H_

#include <stdint.h>
#include <stddef.h>

/* HAL scalar types */
typedef unsigned char   hal_bit_t;
typedef double          hal_float_t;
typedef uint32_t        hal_u32_t;
typedef int32_t         hal_s32_t;
typedef uint64_t        hal_u64_t;
typedef int64_t         hal_s64_t;

/* HAL type enumeration */
typedef enum {
    HAL_TYPE_UNSPECIFIED = -1,
    HAL_BIT   = 1,
    HAL_FLOAT = 2,
    HAL_S32   = 3,
    HAL_U32   = 4,
    HAL_U64   = 5,
    HAL_S64   = 6,
} hal_type_t;

/* HAL pin/param direction enumeration */
typedef enum {
    HAL_DIR_UNSPECIFIED = -1,
    HAL_IN  = 16,
    HAL_OUT = 32,
    HAL_IO  = 48,
} hal_pin_dir_t;

/* Aliases used for HAL parameters */
#define HAL_RO  HAL_OUT
#define HAL_RW  HAL_IO

/* Maximum length of a HAL object name */
#define HAL_NAME_LEN 256

/* Stub function declarations — implementations in test_pdo_counts.c */
extern void *hal_malloc(long int size);
extern int   hal_pin_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
                         void **data_ptr_addr, int comp_id);
extern int   hal_param_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
                           void *data_addr, int comp_id);

#endif
