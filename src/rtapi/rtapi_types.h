// Author: Jeff Epler
// License: LGPL Version 2
//    
// Copyright (c) 2007 All rights reserved.
// 
// rtapi_types.h provides the following types:
// s32 (signed type at least 32 bits wide)
// u32 (unsigned type at least 32 bits wide)

#ifndef RTAPI_TYPES_H
#define RTAPI_TYPES_H

#if defined(SIM) || defined(ULAPI)
#include <stdint.h>
typedef int32_t s32;
typedef uint32_t u32;
#ifndef RTAPI_NO_DEPRECATED_TYPES
typedef s32 __s32 __attribute((deprecated));
typedef u32 __u32 __attribute((deprecated));
#endif
#else  // from #ifdef SIM
#include <asm/types.h>
#endif // from #ifdef SIM

#endif // from #ifdef RTAPI_TYPES_H
