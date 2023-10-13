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

#define MAX_STREAMERS		8
#define MAX_SAMPLERS		8
#define MAX_PINS 		20
#define MAX_SHMEM 		128000
#define STREAMER_SHMEM_KEY 	0x48535430
#define SAMPLER_SHMEM_KEY	0x48534130

/* this struct lives in HAL shared memory */

typedef union {
    hal_bit_t *hbit;
    hal_float_t *hfloat;
    hal_u32_t *hu32;
    hal_s32_t *hs32;
} pin_data_t;

