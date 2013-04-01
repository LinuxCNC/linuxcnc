/********************************************************************
* Description: emcmotglb.c
*   Compile-time configuration parameters
*
*   Set the values in emcmotcfg.h; these vars will be set to those values
*   and emcmot.c can reference the variables with their defaults. This file
*   exists to avoid having to recompile emcmot.c every time a default is
*   changed.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

#include "emcmotglb.h"		/* these decls */
#include "emcmotcfg.h"		/* initial values */

char EMCMOT_INIFILE[EMCMOT_INIFILE_LEN] = DEFAULT_EMCMOT_INIFILE;

unsigned int SHMEM_KEY = DEFAULT_MOTION_SHMEM_KEY;

double EMCMOT_COMM_TIMEOUT = DEFAULT_EMCMOT_COMM_TIMEOUT;
double EMCMOT_COMM_WAIT = DEFAULT_EMCMOT_COMM_WAIT;

double VELOCITY = DEFAULT_VELOCITY;
double ACCELERATION = DEFAULT_ACCELERATION;

double MAX_LIMIT = DEFAULT_MAX_LIMIT;
double MIN_LIMIT = DEFAULT_MIN_LIMIT;

int TC_QUEUE_SIZE = DEFAULT_TC_QUEUE_SIZE;

double MAX_FERROR = DEFAULT_MAX_FERROR;
