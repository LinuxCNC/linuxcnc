/********************************************************************
* Description: exthalmot.c
*   Dispatcher of external motion functions for HAL drivers.
*   This should be the only point of contact between emc and HAL.
*
* Author:
* Created at:
* Computer:
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
/*
  exthalmot.c

  Modification history:

  01-Feb-2004  MGS created from extstgmot.c..
  */

#include "emcmotcfg.h"          /* EMCMOT_MAX_AXIS */
#include "extintf.h"            /* these decls */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"                /* decls for HAL implementation */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused))  ident[] = "";

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data for a single axis */
typedef struct {
    hal_float_t *volts; 	/* pin: voltage output */
    hal_float_t *position;	/* pin: position input */
    hal_bit_t *max;		/* max limit switch input */
    hal_bit_t *min;		/* min limit switch input */
    hal_bit_t *home;		/* home switch input */
    hal_float_t *probe;		/* probe input */
    hal_bit_t *enable;		/* amp enable output */
    hal_bit_t *fault;		/* amp fault input */
    /* for now we control the index model through the mode
       and model pins on axis 0, later this may be done on
       a per axis basis */
    hal_u32_t *mode;		/* index model output */
    hal_u32_t *model;		/* index model input */
    hal_bit_t *reset;		/* index latch reset output */
    hal_bit_t *latch;		/* index latch input */
    hal_bit_t *index;		/* index input */
} axis_t;

/* pointer to array of axis_t structs in shmem, 1 per axis */
static axis_t *axis_array;

/* component ID */
static int comp_id;

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/
int export_axis(int num, axis_t * addr)
{
    int retval;
    char buf[HAL_NAME_LEN + 2];

    /* export pins for the axis I/O */
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.volts", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->volts), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.position", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->position), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.max", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->max), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.min", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->min), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.home", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->home), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.probe", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->probe), comp_id);
    if (retval != 0) {
	return retval;
    }

    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.enable", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(addr->enable), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.fault", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->fault), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.mode", num);
    retval = hal_pin_u32_new(buf, HAL_WR, &(addr->mode), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.model", num);
    retval = hal_pin_u32_new(buf, HAL_RD, &(addr->model), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.reset", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(addr->reset), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.latch", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->latch), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.index", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->index), comp_id);
    if (retval != 0) {
	return retval;
    }
    return 0;
}

/***********************************************************************
*                REALTIME EXTERNAL INTERFACE FUNCTIONS                 *
************************************************************************/
int extMotInit(void)
{
    int n, retval;

    /* connect to the HAL */
    comp_id = hal_init("halmot");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HALMOT: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for axis data */
    axis_array = hal_malloc(EMCMOT_MAX_AXIS * sizeof(axis_t));
    if (axis_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HALMOT: ERROR: AXIS: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the variables for each axis */
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
	/* export all vars */
	retval = export_axis(n, &(axis_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HALMOT: ERROR: axis %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init axis outputs */
	*(axis_array[n].volts) = 0.0;
	*(axis_array[n].enable) = 0;
        /* We'll init the index model to zero (no model) for now */
        *(axis_array[n].mode) = 0;
	*(axis_array[n].reset) = 0;
    }
    /* Done! */
    rtapi_print_msg(RTAPI_MSG_INFO,
	"HALMOT: installed %d axes\n", EMCMOT_MAX_AXIS);
    return 0;
}

int extMotQuit(void)
{
  hal_exit(comp_id);
  return 0;
}

/* DAC Functions */
int extDacNum(void)
{
/*
This function isn't currently used, but if it were,
we would need to return the number of DACs in the system.
A possible way of doing this is:
  return GetAxes();
*/
  return 0;
}

int extDacWrite(int dac, double volts)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[dac]);
  if (dac >= EMCMOT_MAX_AXIS || dac < 0) {
    return -1;
  } else {
    *(axis_addr->volts) = volts;
    return 0;
  }
}

int extDacWriteAll(int max, double * volts)
{
  int n;
  axis_t *axis_addr;
  if (max > EMCMOT_MAX_AXIS || max < 1) {
    return -1;
  } else {
    for (n = 0; n < max; n++) {
      axis_addr = &(axis_array[n]);
     *(axis_addr->volts) = volts[n];
    }
    return 0;
  }
}

/* Encoder Functions */
unsigned int extEncoderIndexModel(void)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[0]);
  return *(axis_addr->model);
}

int extEncoderSetIndexModel(unsigned int model)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[0]);
  /* test that the requested model is supported */
  if ((model & *(axis_addr->model)) == 0) {
    return -1;
  } else {
    (unsigned int *)axis_addr->mode = model;
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

int extEncoderRead(int encoder, double * counts)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[encoder]);
  if (encoder >= EMCMOT_MAX_AXIS || encoder < 0) {
    return -1;
  } else {
    *counts = *(axis_addr->position);
    return 0;
  }
}

int extEncoderReadAll(int max, double * counts)
{
  int n;
  axis_t *axis_addr;
  if (max > EMCMOT_MAX_AXIS || max < 1) {
    return -1;
  } else {
    for (n = 0; n < max; n++) {
      axis_addr = &(axis_array[n]);
      counts[n] = *(axis_addr->position);
    }
    return 0;
  }
}

int extEncoderResetIndex(int encoder)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[encoder]);
  if (encoder >= EMCMOT_MAX_AXIS || encoder < 0) {
    return -1;
  } else {
    /* each time this function is called, we set this bit */
    (int *)axis_addr->reset = 1;
    return 0;
  }
}

int extEncoderReadLatch(int encoder, int * flag)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[encoder]);
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

int extEncoderReadLevel(int encoder, int * flag)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[encoder]);
  if (encoder >= EMCMOT_MAX_AXIS || encoder < 0) {
    return -1;
  } else {
    *flag = *(axis_addr->index);
    return 0;
  }
}

/* Switch Functions */
int extMaxLimitSwitchRead(int axis, int * flag)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[axis]);
  if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_addr->max);
    return 0;
  }
}

int extMinLimitSwitchRead(int axis, int * flag)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[axis]);
  if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_addr->min);
    return 0;
  }
}

int extHomeSwitchRead(int axis, int * flag)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[axis]);
  if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_addr->home);
    return 0;
  }
}

/* Probe Functions */
int extProbeCheck(int * flag)
{
  int n, f;
  axis_t *axis_addr;
  f = 0;
  for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
    axis_addr = &(axis_array[n]);
    if (*(axis_addr->probe) != 0.0) {
      f = 1;
    }
  }
  *flag = f;
  return 0;
}

int extProbeRead(int axis, double * counts)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[axis]);
  if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
    return -1;
  } else {
    *counts = *(axis_addr->probe);
    return 0;
  }
}

int extProbeReadAll(int max, double * counts)
{
  int n;
  axis_t *axis_addr;
  if (max > EMCMOT_MAX_AXIS || max < 1) {
    return -1;
  } else {
    for (n = 0; n < max; n++) {
      axis_addr = &(axis_array[n]);
      counts[n] = *(axis_addr->probe);
    }
    return 0;
  }
}

/* Amplifier Functions */
int extAmpEnable(int axis, int enable)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[axis]);
  if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
    return -1;
  } else {
    (int *)axis_addr->enable = enable;
    return 0;
  }
}

int extAmpFault(int axis, int * fault)
{
  axis_t *axis_addr;
  axis_addr = &(axis_array[axis]);
  if (axis >= EMCMOT_MAX_AXIS || axis < 0) {
    return -1;
  } else {
    *fault = *(axis_addr->fault);
    return 0;
  }
}
