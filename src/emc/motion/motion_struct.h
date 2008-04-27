/********************************************************************
* Description: motion_struct.h
*   A data structure used in only a few places
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved
*
* Last change:
********************************************************************/

#ifndef MOTION_STRUCT_H
#define MOTION_STRUCT_H

/* big comm structure, for upper memory */
    typedef struct emcmot_struct_t {
	struct emcmot_command_t command;	/* struct used to pass commands/data
					   to the RT module from usr space */
	struct emcmot_status_t status;	/* Struct used to store RT status */
	struct emcmot_config_t config;	/* Struct used to store RT config */
	struct emcmot_internal_t internal;	/*! \todo FIXME - doesn't need to be in
					   shared memory */
	struct emcmot_error_t error;	/* ring buffer for error messages */
	struct emcmot_debug_t debug;	/* Struct used to store RT status and debug
				   data - 2nd largest block */
    } emcmot_struct_t;


#endif // MOTION_STRUCT_H
