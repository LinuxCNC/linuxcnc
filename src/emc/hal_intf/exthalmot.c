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
#include "rtapi_app.h"		/* RTAPI realtime module decls */
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
    hal_bit_t *enable;		/* amp enable output */
    hal_bit_t *fault;		/* amp fault input */
    hal_bit_t *reset;		/* index latch reset output */
    hal_bit_t *latch;		/* index latch input */
    hal_bit_t *index;		/* index input */
} axis_t;

/* pointer to array of axis_t structs in shmem, 1 per axis */
static axis_t *axis_array;

/* this structure contains the control data for all axes */
typedef struct {
    hal_u32_t *mode;		/* index model output */
    hal_u32_t *model;		/* index model input */
} control_t;

/* pointer to control_t struct in shmem */
static control_t *control_array;

/* component ID */
static int comp_id;

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/
static int export_axis(int num, axis_t * addr)
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
}

static int export_control(control_t * addr)
{
    int retval;
    char buf[HAL_NAME_LEN + 2];

    /* export control pins */
    rtapi_snprintf(buf, HAL_NAME_LEN, "control.mode");
    retval = hal_pin_u32_new(buf, HAL_WR, &(addr->mode), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "control.model");
    retval = hal_pin_u32_new(buf, HAL_RD, &(addr->model), comp_id);
    if (retval != 0) {
	return retval;
    }
)

/***********************************************************************
*                REALTIME EXTERNAL INTERFACE FUNCTIONS                 *
************************************************************************/
static int extMotInit(void)
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
    /* allocate shared memory for control data */
    control_array = hal_malloc(sizeof(control_t));
    if (control_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HALMOT: ERROR: CONTROL: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the variables for each axis */
    for (n = 0; n < (EMCMOT_MAX_AXIS - 1); n++) {
	/* export all vars */
	retval = export_axis(n, &(axis_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HALMOT: ERROR: axis %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init axis */
	*(axis_array[n].volts) = 0.0;
	*(axis_array[n].enable) = 0;
	*(axis_array[n].reset) = 0;
    }
    /* export the control variables */
    retval = export_control(&(control_array[n]));
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
	    "HALMOT: ERROR: control var export failed\n", n);
	hal_exit(comp_id);
	return -1;
    }
    /* init control variables */
    /* We'll init the index model to manual for now */
    *(axis_array[n].mode) = EXT_ENCODER_INDEX_MODEL_MANUAL;
    /* Done! */
    rtapi_print_msg(RTAPI_MSG_INFO,
	"HALMOT: installed %d axes & control vars\n", EMCMOT_MAX_AXIS);
    return 0;
}

static int extMotQuit(void)
{
  hal_exit(comp_id);
  return 0;
}

/* DAC Functions */
static int extDacNum(void)
{
/*
This function isn't currently used, but if it were,
we would need to return the number of DACs in the system.
*/
  return 0;
}

static int extDacWrite(int dac, double volts)
{
  if (dac > (EMCMOT_MAX_AXIS - 1) || dac < 0) {
    return -1;
  } else {
    *(axis_array[dac].volts) = volts;
    return 0;
  }
}

static int extDacWriteAll(int max, double * volts)
{
  int n;
  if (max > EMCMOT_MAX_AXIS || max < 1) {
    return -1;
  } else {
    for (n = 0; n < (max - 1); n++) {
      *(axis_array[n].volts) = *(volts + n);
    }
    return 0;
  }
}

/* Encoder Functions */

static unsigned int extEncoderIndexModel(void)
{
  return *(control_array.model);
}

static int extEncoderSetIndexModel(unsigned int model)
{
  /* test that the requested model is supported */
  if ((model & *(control_array.model)) == 0) {
    return -1;
  } else {
  *(control_array.mode) = model;
  return 0;
  }
}

static int extEncoderNum(void)
{
/*
This function isn't currently used, but if it were,
we would need to return the number of encoders in the system.
*/
  return 0;
}

static int extEncoderRead(int encoder, double * counts)
{
  if (encoder > (EMCMOT_MAX_AXIS - 1) || encoder < 0) {
    return -1;
  } else {
    *counts = *(axis_array[encoder].position);
    return 0;
  }
}

static int extEncoderReadAll(int max, double * counts)
{
  int n;
  if (max > EMCMOT_MAX_AXIS || max < 1) {
    return -1;
  } else {
    for (n = 0; n < (max - 1); n++) {
      *(counts + n) = *(axis_array[n].position);
    }
    return 0;
  }
}

static int extEncoderResetIndex(int encoder)
{
  if (encoder > (EMCMOT_MAX_AXIS - 1) || encoder < 0) {
    return -1;
  } else {
    /* each time this function is called, we set this bit */
    *(axis_array[axis].reset) = 1;
    return 0;
  }
}

static int extEncoderReadLatch(int encoder, int * flag)
{
  if (axis > (EMCMOT_MAX_AXIS - 1) || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_array[axis].latch);
    /* if the latch bit is set, we clear the reset bit */
    if (*flag == 1) {
      *(axis_array[axis].reset) = 0;
    }
    return 0;
  }
}

static int extEncoderReadLevel(int encoder, int * flag)
{
  if (axis > (EMCMOT_MAX_AXIS - 1) || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_array[axis].index);
    return 0;
  }
}

/* Switch Functions */
static int extMaxLimitSwitchRead(int axis, int * flag)
{
  if (axis > (EMCMOT_MAX_AXIS - 1) || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_array[axis].max);
    return 0;
  }
}

static int extMinLimitSwitchRead(int axis, int * flag)
{
  if (axis > (EMCMOT_MAX_AXIS - 1) || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_array[axis].min);
    return 0;
  }
}

static int extHomeSwitchRead(int axis, int * flag)
{
  if (axis > (EMCMOT_MAX_AXIS - 1) || axis < 0) {
    return -1;
  } else {
    *flag = *(axis_array[axis].home);
    return 0;
  }
}

/* Amplifier Functions */
static int extAmpEnable(int axis, int enable)
{
  if (axis > (EMCMOT_MAX_AXIS - 1) || axis < 0) {
    return -1;
  } else {
    *(axis_array[axis].enable) = enable;
    return 0;
  }
}

static int extAmpFault(int axis, int * fault)
{
  if (axis > (EMCMOT_MAX_AXIS - 1) || axis < 0) {
    return -1;
  } else {
    *fault = *(axis_array[axis].fault);
    return 0;
  }
}

/* Digital I/O Functions */
static int extDioInit(const char * stuff)
{
  return 0;
}

static int extDioQuit(void)
{
  return 0;
}

static int extDioMaxInputs(void)
{
  return 0;
}

static int extDioMaxOutputs(void)
{
  return 0;
}

static int extDioRead(int index, int *value)
{
  return 0;
}

static int extDioWrite(int index, int value)
{
  return 0;
}

static int extDioCheck(int index, int *value)
{
  return 0;
}

static int extDioByteRead(int index, unsigned char *byte)
{
  return 0;
}

static int extDioShortRead(int index, unsigned short *sh)
{
  return 0;
}

static int extDioWordRead(int index, unsigned int *word)
{
  return 0;
}

static int extDioByteWrite(int index, unsigned char byte)
{
  return 0;
}

static int extDioShortWrite(int index, unsigned short sh)
{
  return 0;
}

static int extDioWordWrite(int index, unsigned int word)
{
  return 0;
}

static int extDioByteCheck(int index, unsigned char *byte)
{
  return 0;
}

static int extDioShortCheck(int index, unsigned short *sh)
{
  return 0;
}

static int extDioWordCheck(int index, unsigned int *word)
{
  return 0;
}

/* Analog I/O Functions */
static int extAioInit(const char * stuff)
{
  return 0;
}

static int extAioQuit(void)
{
  return 0;
}

static int extAioMaxInputs(void)
{
  return 0;
}

static int extAioMaxOutputs(void)
{
  return 0;
}

static int extAioStart(int index)
{
  return 0;
}

static void extAioWait(void)
{
  return;
}

static int extAioRead(int index, double *volts)
{
  return 0;
}

static int extAioWrite(int index, double volts)
{
  return 0;
}

static int extAioCheck(int index, double *volts)
{
  return 0;
}
