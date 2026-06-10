/********************************************************************
* Description: motion_struct.h
*   A data structure used in only a few places
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved
********************************************************************/

#ifndef MOTION_STRUCT_H
#define MOTION_STRUCT_H

#include <rtapi_mutex.h>
#include <stdatomic.h>

/* Lock-free SPSC triple buffer for status.
   Writer (RT servo thread) and a single logical consumer exchange slots
   through an atomic middle index.  Multiple physical readers are
   serialized by reader_mtx so the buffer sees exactly one consumer. */
#define MOTSTAT_SLOTS 3

typedef struct emcmot_status_buf_t {
    struct emcmot_status_t slots[MOTSTAT_SLOTS];
    atomic_int middle;          /* exchange slot between writer and reader */
    int write_idx;              /* writer-private slot index */
    int read_idx;               /* reader-private slot index (under reader_mtx) */
    rtapi_mutex_t reader_mtx;   /* serialize consumer access */
} emcmot_status_buf_t;

/* big comm structure, for upper memory */
    typedef struct emcmot_struct_t {
        rtapi_mutex_t command_mutex;  // Used to protect access to `command`.
        struct emcmot_command_t command;   /* struct used to pass commands/data from Task to Motion */

	struct emcmot_status_t status;	/* Legacy single status (used as writer workspace) */
	emcmot_status_buf_t status_buf;	/* Triple-buffered status for readers */
	struct emcmot_config_t config;	/* Struct used to store RT config */
	struct emcmot_internal_t internal;	/* Struct used to store RT status and debug
				   data - 2nd largest block */
    } emcmot_struct_t;


#endif // MOTION_STRUCT_H
