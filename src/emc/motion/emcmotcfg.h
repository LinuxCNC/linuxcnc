/********************************************************************
* Description: emcmotcfg.h
*   Default values for compile-time parameters.
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
#ifndef EMCMOTCFG_H
#define EMCMOTCFG_H

#include "rtapi_shmkeys.h"  // unified shm key management

/* default name of EMCMOT ini file */
#define DEFAULT_EMCMOT_INIFILE "emc.ini"	/* same as for EMC-- we're in 
						   touch */

/* number of joints supported
   Note: this is not a global variable but a compile-time parameter
   since it sets array sizes, etc. */
#define EMCMOT_MAX_JOINTS 9
/* number of axes defined by the interp */ //FIXME: shouldn't be here..
#define EMCMOT_MAX_AXIS 9

#define EMCMOT_MAX_DIO 128
#define EMCMOT_MAX_AIO 64

#if (EMCMOT_MAX_AIO > 64)
#error A 64 bit bitmask is used in the planner.  Don't increase these until that's fixed.
#endif

#define EMCMOT_ERROR_NUM 32	/* how many errors we can queue */
#define EMCMOT_ERROR_LEN 1024	/* how long error string can be */

/* default comm timeout, in seconds */
#define DEFAULT_EMCMOT_COMM_TIMEOUT 1.0
/* seconds to delay between comm retries */
#define DEFAULT_EMCMOT_COMM_WAIT 0.010

/* initial velocity, accel used for coordinated moves */
#define DEFAULT_VELOCITY 1.0
#define DEFAULT_ACCELERATION 10.0

/* maximum and minimum limit defaults for all axes */
#define DEFAULT_MAX_LIMIT 1000
#define DEFAULT_MIN_LIMIT -1000

/* size of motion queue
 * a TC_STRUCT is about 512 bytes so this queue is
 * about a megabyte.  */
#define DEFAULT_TC_QUEUE_SIZE 2000
#define DEFAULT_ALT_TC_QUEUE_SIZE 100   // size of secondary motion queue

/* max following error */
#define DEFAULT_MAX_FERROR 100

#endif
