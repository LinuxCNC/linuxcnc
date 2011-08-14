#ifndef RTAPI_BYTEORDER_H
#define RTAPI_BYTEORDER_H
#ifdef __KERNEL__
#include <asm/byteorder.h>
#ifdef __BIG_ENDIAN
#define RTAPI_BIG_ENDIAN 1
#define RTAPI_LITTLE_ENDIAN 0
#define RTAPI_FLOAT_BIG_ENDIAN 1
#else
#define RTAPI_LITTLE_ENDIAN 1
#define RTAPI_BIG_ENDIAN 0
#define RTAPI_FLOAT_BIG_ENDIAN 0
#endif
#else
#include <endian.h>
#define RTAPI_BIG_ENDIAN (__BYTE_ORDER == __BIG_ENDIAN)
#define RTAPI_LITTLE_ENDIAN (__BYTE_ORDER == __LITTLE_ENDIAN)
#define RTAPI_FLOAT_BIG_ENDIAN (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
#endif
#endif
