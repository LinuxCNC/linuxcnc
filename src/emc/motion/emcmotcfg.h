/********************************************************************
* Description: emcmotcfg.h
*   Default values for compile-time parameters, used to initialize
*   global variables in emcmotcfg.c.
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
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef EMCMOTCFG_H
#define EMCMOTCFG_H

/* default name of EMCMOT ini file */
#define DEFAULT_EMCMOT_INIFILE "emc.ini"	/* same as for EMC-- we're in 
						   touch */

/* number of axes supported
   Note: this is not a global variable but a compile-time parameter
   since it sets array sizes, etc. */
#define EMCMOT_MAX_AXIS 8

#define EMCMOT_ERROR_NUM 32	/* how many errors we can queue */
#define EMCMOT_ERROR_LEN 256	/* how long error string can be */

/*
  Shared memory keys for simulated motion process. No base address
  values need to be computed, since operating system does this for us
  */
#define DEFAULT_SHMEM_KEY 100

/* default comm timeout, in seconds */
#define DEFAULT_EMCMOT_COMM_TIMEOUT 1.0
/* seconds to delay between comm retries */
#define DEFAULT_EMCMOT_COMM_WAIT 0.010

/* default cycle time for trajectory calculations; cycle time
   for emcmot.c main loop will be this times the interpolation rate */
#ifdef RTAPI
#define DEFAULT_TRAJ_CYCLE_TIME  0.010
#define DEFAULT_SERVO_CYCLE_TIME 0.001
#else
#define DEFAULT_TRAJ_CYCLE_TIME  0.200
#define DEFAULT_SERVO_CYCLE_TIME 0.020
#endif

/* initial velocity, accel used for coordinated moves */
#define DEFAULT_VELOCITY 1.0
#define DEFAULT_ACCELERATION 10.0

/* maximum and minimum limit defaults for all axes */
#define DEFAULT_MAX_LIMIT 1000
#define DEFAULT_MIN_LIMIT -1000

/* output clamps before scaling */
#define DEFAULT_MAX_OUTPUT 10.0
#define DEFAULT_MIN_OUTPUT -10.0

/* size of motion queue */
#define DEFAULT_TC_QUEUE_SIZE 200

/* size of window for averages */
#define DEFAULT_MMXAVG_SIZE 100

/* max following error */
#define DEFAULT_MAX_FERROR 100

/* PID gains */
#define DEFAULT_P_GAIN 0.0
#define DEFAULT_I_GAIN 0.0
#define DEFAULT_D_GAIN 0.0
#define DEFAULT_FF0_GAIN 0.0
#define DEFAULT_FF1_GAIN 0.0
#define DEFAULT_FF2_GAIN 0.0
#define DEFAULT_BACKLASH 0.0
#define DEFAULT_BIAS 0.0
#define DEFAULT_MAX_ERROR 0.0

/* input, output scales */
#define DEFAULT_INPUT_SCALE 1.0
#define DEFAULT_INPUT_OFFSET 0.0
#define DEFAULT_OUTPUT_SCALE 1.0
#define DEFAULT_OUTPUT_OFFSET 0.0

#endif
