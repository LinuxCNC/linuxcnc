#ifndef _HAL_TYPES_H
#define _HAL_TYPES_H

#include "rtapi.h"  // rtapi_ct_assert()

// the legacy code chose to define HAL types using 'volatile' for no good reason
// it does not address the atomicity issue and disables some optimizations
// see https://www.kernel.org/doc/Documentation/volatile-considered-harmful.txt
// and http://stackoverflow.com/questions/27777140/volatile-and-its-harmful-implications
// in case this change needs regression-testing, redefine _HALTYPE_ATTRIBUTES like so:
// #define _HALTYPE_ATTRIBUTES volatile
#define _HALTYPE_ATTRIBUTES

/* Use these for x86 machines, and anything else that can write to
   individual bytes in a machine word. */
#define __KERNEL_STRICT_NAMES
#include <linux/types.h>
#ifdef __cplusplus
typedef bool hal_bool;
#else
typedef _Bool hal_bool;
#endif
typedef _HALTYPE_ATTRIBUTES hal_bool hal_bit_t;
typedef _HALTYPE_ATTRIBUTES uint8_t  hal_u8_t;
typedef _HALTYPE_ATTRIBUTES int8_t   hal_s8_t;
typedef _HALTYPE_ATTRIBUTES uint32_t hal_u32_t;
typedef _HALTYPE_ATTRIBUTES int32_t  hal_s32_t;
typedef _HALTYPE_ATTRIBUTES int64_t  hal_s64_t;
typedef _HALTYPE_ATTRIBUTES uint64_t hal_u64_t;
typedef double real_t __attribute__((aligned(8)));
typedef __u64 ireal_t __attribute__((aligned(8))); // integral type as wide as real_t / hal_float_t
typedef _HALTYPE_ATTRIBUTES real_t hal_float_t;

/** HAL "data union" structure
 ** This structure may hold any type of hal data
*/
typedef union {
    hal_bit_t _b;
    hal_u8_t  _u8;
    hal_s8_t  _s8;
    hal_s32_t _s;
    hal_u32_t _u;
    hal_float_t _f;
    hal_s64_t _ls;
    hal_u64_t _lu;

    // access for atomics
    // debuging & regression test use
    struct {
	hal_u32_t _u1;
	hal_u32_t _u2;
    } _uint;
    hal_u8_t _bytes[8];
    struct { // alias as single-precision float
	float _fs;
	hal_u32_t _extra;
    } _single;
} hal_data_u;

//rtapi_ct_assert(sizeof(hal_data_u) == sizeof(uint64_t), "BUG: size mismatch");

#endif // _HAL_TYPES_H
