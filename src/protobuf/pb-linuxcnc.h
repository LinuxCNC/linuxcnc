// Nanopb global library options

// #define PB_FIELD_16BIT
// #define PB_BUFFER_ONLY   // custom streams not needed - memory buffers ok

#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)
#define VERSION EXPAND_AND_QUOTE(NANOPB_VERSION)

// take care when compiling as a kernel module:
#ifndef __KERNEL__
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#else
#include <linux/module.h>
#endif

#define _NO_PB_SYSTEM_HEADER
#include <protobuf/nanopb/pb.h>
