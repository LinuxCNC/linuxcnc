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


/* big comm structure, for upper memory */
    typedef struct emcmot_struct_t {
        rtapi_mutex_t command_mutex;  // Used to protect access to `command`.
        struct emcmot_command_t command;   /* struct used to pass commands/data from Task to Motion */

	struct emcmot_status_t status;	/* Struct used to store RT status */
	struct emcmot_config_t config;	/* Struct used to store RT config */
	struct emcmot_error_t error;	/* ring buffer for error messages */
	struct emcmot_internal_t internal;	/* Struct used to store RT status and debug
				   data - 2nd largest block */
    } emcmot_struct_t;


#endif // MOTION_STRUCT_H
