/********************************************************************
* Description:  streamer.h
*               Typedefs and such for the "streamer" and "sampler"
*               HAL components.
*
* Author: John Kasunich <jmkasunich at sourceforge dot net>
* License: GPL Version 2
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/
#include "rtapi_shmkeys.h"

#define MAX_STREAMERS		8
#define MAX_SAMPLERS		8
#define MAX_PINS 		20
#define MAX_SHMEM 		128000

#define FIFO_MAGIC_NUM		0x4649464F

/* These structs live in the shared memory that connects the user
   space and RT parts.  They are _not_ in HAL shared memory.
*/

typedef union {
    real_t f;
    char  b;
    hal_s32_t s;
    hal_u32_t u;
} shmem_data_t;

typedef struct {
    unsigned int magic;
    volatile unsigned int in;
    volatile unsigned int out;
    int depth;
    int num_pins;
    unsigned long last_sample;
    hal_type_t type[MAX_PINS];
    shmem_data_t data[];
} fifo_t;

/* this struct lives in HAL shared memory */

typedef union {
    hal_bit_t *hbit;
    hal_float_t *hfloat;
    hal_u32_t *hu32;
    hal_s32_t *hs32;
} pin_data_t;

