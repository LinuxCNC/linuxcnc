/* FIXME - this has been obsoleted by HAL - remove from CVS after
   emc2 is working */

/********************************************************************
* Description: exthalmot.c
*   Dispatcher of external motion functions for HAL drivers.
*   This should be the only point of contact between emc and HAL.
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
/*
  exthalmot.c

  Modification history:

  01-Feb-2004  MGS created from extstgmot.c..
  */

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* decls for HAL implementation */
#include "emcmotcfg.h"		/* EMCMOT_MAX_AXIS */
#include "extintf.h"		/* these decls */
//#include "emcmotglb.h"
#include "motion.h"
#include "../motion/mot_priv.h"	/* motion module decls */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

/***********************************************************************
*                REALTIME EXTERNAL INTERFACE FUNCTIONS                 *
************************************************************************/
int extDacWrite(int dac, double volts)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[dac]);
    if (dac >= EMCMOT_MAX_AXIS || dac < 0) {
	return -1;
    } else {
	*(axis_addr->volts) = volts;
	return 0;
    }
}

int extDacWriteAll(int max, double *volts)
{
    int n;
    axis_hal_t *axis_addr;
    if (max > EMCMOT_MAX_AXIS || max < 1) {
	return -1;
    } else {
	for (n = 0; n < max; n++) {
	    axis_addr = &(axis_hal_array[n]);
	    *(axis_addr->volts) = volts[n];
	}
	return 0;
    }
}

/* Encoder Functions */
unsigned int extEncoderIndexModel(void)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[0]);
    return *(axis_addr->model);
}

int extEncoderSetIndexModel(unsigned int model)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[0]);
    /* test that the requested model is supported */
    if ((model & *(axis_addr->model)) == 0) {
	return -1;
    } else {
	(unsigned int *) axis_addr->mode = model;
	return 0;
    }
}

int extEncoderNum(void)
{
/*
This function isn't currently used, but if it were,
we would need to return the number of encoders in the system.
A possible way of doing this is:
  return GetAxes();
*/
    return 0;
}

int extEncoderRead(int encoder, double *counts)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[encoder]);
    if (encoder >= EMCMOT_MAX_AXIS || encoder < 0) {
	return -1;
    } else {
	*counts = *(axis_addr->position);
	return 0;
    }
}

int extEncoderReadAll(int max, double *counts)
{
    int n;
    if (max > EMCMOT_MAX_AXIS || max < 1) {
	return -1;
    } else {
	for (n = 0; n < max; n++) {
	    counts[n] = (double) *axis_hal_array[n].position;
	}
	return 0;
    }
}

int extEncoderResetIndex(int encoder)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[encoder]);
    if (encoder >= EMCMOT_MAX_AXIS || encoder < 0) {
	return -1;
    } else {
	/* each time this function is called, we set this bit */
	(int *) axis_addr->reset = 1;
	return 0;
    }
}

int extEncoderReadLatch(int encoder, int *flag)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[encoder]);
    if (encoder >= EMCMOT_MAX_AXIS || encoder < 0) {
	return -1;
    } else {
	*flag = *(axis_addr->latch);
	/* if the latch bit is set, we clear the reset bit */
	if (*flag == 1) {
	    axis_addr->reset = 0;
	}
	return 0;
    }
}

int extEncoderReadLevel(int encoder, int *flag)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[encoder]);
    if (encoder >= EMCMOT_MAX_AXIS || encoder < 0) {
	return -1;
    } else {
	*flag = *(axis_addr->index);
	return 0;
    }
}

/* Switch Functions */
int extMaxLimitSwitchRead(int axis, int *flag)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[axis]);
    if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
	return -1;
    } else {
	*flag = *(axis_addr->max);
	return 0;
    }
}

int extMinLimitSwitchRead(int axis, int *flag)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[axis]);
    if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
	return -1;
    } else {
	*flag = *(axis_addr->min);
	return 0;
    }
}

int extHomeSwitchRead(int axis, int *flag)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[axis]);
    if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
	return -1;
    } else {
	*flag = *(axis_addr->home);
	return 0;
    }
}

/* Probe Functions */
int extProbeCheck(int *flag)
{
    int n, f;
    axis_hal_t *axis_addr;
    f = 0;
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
	axis_addr = &(axis_hal_array[n]);
	if (*(axis_addr->probe) != 0.0) {
	    f = 1;
	}
    }
    *flag = f;
    return 0;
}

int extProbeRead(int axis, double *counts)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[axis]);
    if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
	return -1;
    } else {
	*counts = *(axis_addr->probe);
	return 0;
    }
}

int extProbeReadAll(int max, double *counts)
{
    int n;
    axis_hal_t *axis_addr;
    if (max > EMCMOT_MAX_AXIS || max < 1) {
	return -1;
    } else {
	for (n = 0; n < max; n++) {
	    axis_addr = &(axis_hal_array[n]);
	    counts[n] = *(axis_addr->probe);
	}
	return 0;
    }
}

/* Amplifier Functions */
int extAmpEnable(int axis, int enable)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[axis]);
    if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
	return -1;
    } else {
	(int *) axis_addr->enable = enable;
	return 0;
    }
}

int extAmpFault(int axis, int *fault)
{
    axis_hal_t *axis_addr;
    axis_addr = &(axis_hal_array[axis]);
    if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
	return -1;
    } else {
	*fault = *(axis_addr->fault);
	return 0;
    }
}
