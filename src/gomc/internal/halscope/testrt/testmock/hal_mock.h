/* hal_mock.h — Minimal HAL/RTAPI type stubs for halscope unit tests.
 *
 * Provides just enough definitions to compile halscope_rt.c without
 * pulling in the full HAL shared-memory infrastructure.
 */
#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#include <stdint.h>
#include <stdbool.h>

/* rtapi types */
typedef int32_t  rtapi_s32;
typedef uint32_t rtapi_u32;
typedef uint64_t rtapi_u64;

/* hal.h types */
#define HAL_NAME_LEN 127

typedef enum {
    HAL_BIT   = 1,
    HAL_FLOAT = 2,
    HAL_S32   = 3,
    HAL_U32   = 4,
} hal_type_t;

typedef double   real_t   __attribute__((aligned(8)));
typedef uint64_t ireal_t  __attribute__((aligned(8)));

typedef volatile bool     hal_bit_t;
typedef volatile rtapi_s32 hal_s32_t;
typedef volatile rtapi_u32 hal_u32_t;
typedef volatile real_t    hal_float_t;

#endif /* HAL_MOCK_H */
