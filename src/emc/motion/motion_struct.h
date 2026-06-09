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

/* Triple-buffered status: writer (servo thread) cycles through slots and
   atomically publishes the latest completed slot index.  Reader always gets
   a consistent, never-torn snapshot (at most 1 cycle old). */
#define MOTSTAT_SLOTS 3

typedef struct emcmot_status_buf_t {
    struct emcmot_status_t slots[MOTSTAT_SLOTS];
    atomic_int readable;  /* index of last fully-written slot (0..2) */
    int write_idx;        /* current writer slot (not shared — writer only) */
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
