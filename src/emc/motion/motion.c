/********************************************************************
* Description: motion.c
*   Main module initialisation and cleanup routines.
*
* Author:
* License: GPL Version 2
* Created on:
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
*
********************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif
#include <stdarg.h>
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* decls for HAL implementation */
#include "emcmotglb.h"
#include "motion.h"
#include "mot_priv.h"
#include "extintf.h"

/***********************************************************************
*                    KERNEL MODULE PARAMETERS                          *
************************************************************************/

#ifdef MODULE
/* module information */
/* register symbols to be modified by insmod
   see "Linux Device Drivers", Alessandro Rubini, p. 385
   (p.42-44 in 2nd edition) */
MODULE_AUTHOR("Matt Shaver/John Kasunich");
MODULE_DESCRIPTION("Motion Controller for EMC");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */

/* FIXME - find a better way to do this */
int DEBUG_MOTION = 0;
MODULE_PARM(DEBUG_MOTION, "i");

/* RTAPI shmem key - for comms with higher level user space stuff */
static int key = 100;		/* the shared memory key, default value */
MODULE_PARM(key, "i");
MODULE_PARM_DESC(key, "shared memory key");

#if 0
/* FIXME - currently HAL has a fixed stacksize of 16384...
   the upcoming HAL rewrite may make it a paramater of the
   create_thread call, in which case this will be restored */
static int EMCMOT_TASK_STACKSIZE = 8192;	/* default stacksize */
MODULE_PARM(EMCMOT_TASK_STACKSIZE, "i");
MODULE_PARM_DESC(EMCMOT_TASK_STACKSIZE, "motion stack size");
#endif

static long base_period_nsec = 0;	/* fastest thread period */
MODULE_PARM(base_period_nsec, "l");
MODULE_PARM_DESC(base_period_nsec, "fastest thread period (nsecs)");
static long servo_period_nsec = 0;	/* servo thread period */
MODULE_PARM(servo_period_nsec, "l");
MODULE_PARM_DESC(servo_period_nsec, "servo thread period (nsecs)");
static long traj_period_nsec = 0;	/* trajectory planner period */
MODULE_PARM(traj_period_nsec, "l");
MODULE_PARM_DESC(traj_period_nsec, "trajectory planner period (nsecs)");

#endif /* MODULE */

/***********************************************************************
*                  GLOBAL VARIABLE DEFINITIONS                         *
************************************************************************/

/* pointer to array of axis_hal_t structs in HAL shmem, 1 per axis */
axis_hal_t *axis_hal_array;

int mot_comp_id;		/* component ID for motion module */

int kinType = 0;

/*
  Principles of communication:

  Data is copied in or out from the various types of comm mechanisms:
  mbuff mapped memory for Linux/RT-Linux, or OS shared memory for Unixes.

  emcmotStruct is ptr to this memory.

  emcmotCommand points to emcmotStruct->command,
  emcmotStatus points to emcmotStruct->status,
  emcmotError points to emcmotStruct->error, and
  emcmotLog points to emcmotStruct->log.
  emcmotComp[] points to emcmotStruct->comp[].
 */
emcmot_struct_t *emcmotStruct;
/* ptrs to either buffered copies or direct memory for
   command and status */
emcmot_command_t *emcmotCommand;
emcmot_status_t *emcmotStatus;
emcmot_config_t *emcmotConfig;
emcmot_debug_t *emcmotDebug;
emcmot_error_t *emcmotError;	/* unused for RT_FIFO */
emcmot_log_t *emcmotLog;		/* unused for RT_FIFO */
emcmot_comp_t *emcmotComp[EMCMOT_MAX_AXIS];	/* unused for RT_FIFO */
emcmot_log_struct_t ls;

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/* RTAPI shmem ID - for comms with higher level user space stuff */
static int emc_shmem_id;	/* the shared memory ID */

/***********************************************************************
*                   LOCAL FUNCTION PROTOTYPES                          *
************************************************************************/

/* init_hal_io() exports HAL pins and parameters making data from
   the realtime control module visible and usable by the world
*/
static int init_hal_io(void);

/* functions called by init_hal_io() */
static int export_axis(int num, axis_hal_t * addr);


/* init_comm_buffers() allocates and initializes the command,
   status, and error buffers used to communicate witht the user
   space parts of emc.
*/
static int init_comm_buffers(void);

/* functions called by init_comm_buffers() */


/* init_threads() creates realtime threads, exports functions to
   do the realtime control, and adds the functions to the threads.
*/
static int init_threads(void);

/* functions called by init_threads() */
static int setTrajCycleTime(double secs);
static int setServoCycleTime(double secs);


/***********************************************************************
*                     PUBLIC FUNCTION CODE                             *
************************************************************************/

void emcmot_config_change(void)
{
    if (emcmotConfig->head == emcmotConfig->tail) {
	emcmotConfig->config_num++;
	emcmotStatus->config_num = emcmotConfig->config_num;
	emcmotConfig->head++;
    }
}

void reportError(const char *fmt, ...)
{
    va_list args;
    char error[EMCMOT_ERROR_LEN];

    va_start(args, fmt);
    /* Don't use the rtapi_snprintf... */
    vsprintf(error, fmt, args);
    va_end(args);

    emcmotErrorPut(emcmotError, error);
}

int init_module(void)
{
    int retval;

    /* FIXME - debug only */
    rtapi_set_msg_level(RTAPI_MSG_ALL);

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_module() starting...\n");

    /* connect to the HAL and RTAPI */
    mot_comp_id = hal_init("motmod");
    if (mot_comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: hal_init() failed\n");
	return -1;
    }

    /* initialize/export HAL pins and parameters */
    retval = init_hal_io();
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: init_hal_io() failed\n");
	hal_exit(mot_comp_id);
	return -1;
    }

    /* allocate/initialize user space comm buffers (cmd/status/err) */
    retval = init_comm_buffers();
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: init_comm_buffers() failed\n");
	hal_exit(mot_comp_id);
	return -1;
    }

    /* set up for realtime execution of code */
    retval = init_threads();
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: init_threads() failed\n");
	hal_exit(mot_comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_module() complete\n");

    return 0;
}

void cleanup_module(void)
{
//    int axis;
    int retval;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: cleanup_module() started.\n");

    retval = hal_stop_threads();
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: hal_stop_threads() failed, returned %d\n", retval);
    }
#if 0
    /* WPS these were moved from above to avoid a possible mutex problem. */
    /* There is no point in clearing the trajectory queue since the planner
       should be dead by now anyway. */
    if (emcmotStruct != 0 && emcmotDebug != 0 && emcmotConfig != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "motion: disabling amps\n");
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
	}
    }
#endif

    /* free shared memory */
    retval = rtapi_shmem_delete(emc_shmem_id, mot_comp_id);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: rtapi_shmem_delete() failed, returned %d\n", retval);
    }
    /* disconnect from HAL and RTAPI */
    retval = hal_exit(mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: hal_exit() failed, returned %d\n", retval);
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: cleanup_module() finished.\n");
}

/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

/* init_hal_io() exports HAL pins and parameters making data from
   the realtime control module visible and usable by the world
*/
static int init_hal_io(void)
{
    int n, retval;

    rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: init_hal_io() starting...\n");

    /* allocate shared memory for axis data */
    axis_hal_array = hal_malloc(EMCMOT_MAX_AXIS * sizeof(axis_hal_t));
    if (axis_hal_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: axis_hal_array malloc failed\n");
	return -1;
    }
    /* export all the variables for each axis */
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
	/* export all vars */
	retval = export_axis(n, &(axis_hal_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"MOTION: axis %d pin/param export failed\n", n);
	    return -1;
	}
	/* init axis outputs */
	*(axis_hal_array[n].volts) = 0.0;
	*(axis_hal_array[n].enable) = 0;
	/* We'll init the index model to EXT_ENCODER_INDEX_MODEL_RAW for now,
	   because it is always supported. */
	*(axis_hal_array[n].mode) = EXT_ENCODER_INDEX_MODEL_RAW;
	*(axis_hal_array[n].reset) = 0;
    }
    /* Done! */
    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_hal_io() complete, %d axes.\n", EMCMOT_MAX_AXIS);
    return 0;
}

static int export_axis(int num, axis_hal_t * addr)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];

    /* This function exports a lot of stuff, which results
       in a lot of logging if msg_level is at INFO or ALL.
       So we save the current value of msg_level and restore
       it later.  If you actually need to log this function's
       actions, change the second line below
    */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export axis pins and parameters */
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.joint-pos-cmd", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->joint_pos_cmd), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.joint-vel-cmd", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->joint_vel_cmd), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.backlash-corr", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->backlash_corr), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.backlash-filt", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->backlash_filt), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.motor-pos-cmd", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->motor_pos_cmd), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.motor-pos-fb", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->motor_pos_fb), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.joint-pos-fb", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->joint_pos_fb), mot_comp_id);
    if (retval != 0) {
	return retval;
    }

/* OLD PINS */

    /* export pins for the axis I/O */
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.volts", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->volts), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.position", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->position), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.max", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->max), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.min", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->min), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.home", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->home), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.probe", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->probe), mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.enable", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(addr->enable), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.fault", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->fault), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.mode", num);
    retval = hal_pin_u32_new(buf, HAL_WR, &(addr->mode), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.model", num);
    retval = hal_pin_u32_new(buf, HAL_RD, &(addr->model), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.reset", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(addr->reset), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.latch", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->latch), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.index", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->index), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    
    /* restore saved message level */
    rtapi_set_msg_level(msg);

    return 0;
}


/* init_comm_buffers() allocates and initializes the command,
   status, and error buffers used to communicate with the user
   space parts of emc.
*/
static int init_comm_buffers(void)
{
    int axis;
    PID_STRUCT pid;
    int retval;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_comm_buffers() starting...\n" );

    emcmotStruct = 0;
    emcmotDebug = 0;
    emcmotStatus = 0;
    emcmotCommand = 0;
    emcmotConfig = 0;

    /* record the kinematics type of the machine */
    kinType = kinematicsType();

    /* allocate and initialize the shared memory structure */
    emc_shmem_id = rtapi_shmem_new(key, mot_comp_id, sizeof(emcmot_struct_t));
    if (emc_shmem_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: rtapi_shmem_new failed, returned %d\n", emc_shmem_id);
	return -1;
    }
    retval = rtapi_shmem_getptr(emc_shmem_id, (void **) &emcmotStruct);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: rtapi_shmem_getptr failed, returned %d\n", retval);
	return -1;
    }

    /* zero shared memory before doing anything else. */
    memset(emcmotStruct, 0, sizeof(emcmot_struct_t));

    /* we'll reference emcmotStruct directly */
    emcmotCommand = (emcmot_command_t *) & emcmotStruct->command;
    emcmotStatus = (emcmot_status_t *) & emcmotStruct->status;
    emcmotConfig = (emcmot_config_t *) & emcmotStruct->config;
    emcmotDebug = (emcmot_debug_t *) & emcmotStruct->debug;
    emcmotError = (emcmot_error_t *) & emcmotStruct->error;
    emcmotLog = (emcmot_log_t *) & emcmotStruct->log;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis] = (emcmot_comp_t *) & emcmotStruct->comp[axis];
	emcmotDebug->bcomp[axis] = 0;	/* backlash comp value */
	emcmotDebug->bcompdir[axis] = 0;	/* 0=none, 1=pos, -1=neg */
	emcmotDebug->bcompincr[axis] = 0;	/* incremental backlash comp */
	emcmotDebug->bac_done[axis] = 0;
	emcmotDebug->bac_d[axis] = 0;
	emcmotDebug->bac_di[axis] = 0;
	emcmotDebug->bac_D[axis] = 0;
	emcmotDebug->bac_halfD[axis] = 0;
	emcmotDebug->bac_incrincr[axis] = 0;
	emcmotDebug->bac_incr[axis] = 0;
    }

    /* init locals */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotDebug->maxLimitSwitchCount[axis] = 0;
	emcmotDebug->minLimitSwitchCount[axis] = 0;
	emcmotDebug->ampFaultCount[axis] = 0;
    }

    /* init compensation struct */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis]->total = 0;
	emcmotComp[axis]->alter = 0.0;
	/* leave out the avgint, nominal, forward, and reverse, since these
	   can't be zero and the total flag prevents their use anyway */
    }

    /* init error struct */
    emcmotErrorInit(emcmotError);

    /* init command struct */
    emcmotCommand->head = 0;
    emcmotCommand->command = 0;
    emcmotCommand->commandNum = 0;
    emcmotCommand->tail = 0;

    /* init status struct */

    emcmotStatus->head = 0;
    emcmotDebug->head = 0;
    emcmotConfig->head = 0;

    emcmotStatus->motionFlag = 0;
    SET_MOTION_ERROR_FLAG(0);
    SET_MOTION_COORD_FLAG(0);
    SET_MOTION_TELEOP_FLAG(0);
    emcmotDebug->split = 0;
    emcmotStatus->commandEcho = 0;
    emcmotStatus->commandNumEcho = 0;
    emcmotStatus->commandStatus = 0;
    emcmotStatus->heartbeat = 0;
    emcmotStatus->computeTime = 0.0;
    emcmotConfig->numAxes = EMCMOT_MAX_AXIS;

    emcmotConfig->trajCycleTime = TRAJ_CYCLE_TIME;
    emcmotConfig->servoCycleTime = SERVO_CYCLE_TIME;
    emcmotStatus->pos.tran.x = 0.0;
    emcmotStatus->pos.tran.y = 0.0;
    emcmotStatus->pos.tran.z = 0.0;
    emcmotStatus->actualPos.tran.x = 0.0;
    emcmotStatus->actualPos.tran.y = 0.0;
    emcmotStatus->actualPos.tran.z = 0.0;
    emcmotStatus->vel = VELOCITY;
    emcmotConfig->limitVel = VELOCITY;
    emcmotStatus->acc = ACCELERATION;
    emcmotStatus->qVscale = 1.0;
    emcmotStatus->id = 0;
    emcmotStatus->depth = 0;
    emcmotStatus->activeDepth = 0;
    emcmotStatus->paused = 0;
    emcmotStatus->overrideLimits = 0;
    SET_MOTION_INPOS_FLAG(1);
    emcmotStatus->logOpen = 0;
    emcmotStatus->logStarted = 0;
    emcmotStatus->logSize = 0;
    emcmotStatus->logSkip = 0;
    emcmotStatus->logPoints = 0;
    SET_MOTION_ENABLE_FLAG(0);
    emcmotConfig->kinematics_type = kinType;

    emcmotDebug->oldPos = emcmotStatus->pos;
    emcmotDebug->oldVel.tran.x = 0.0;
    emcmotDebug->oldVel.tran.y = 0.0;
    emcmotDebug->oldVel.tran.z = 0.0;

    emcmot_config_change();

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotConfig->homingVel[axis] = VELOCITY;
	emcmotConfig->homeOffset[axis] = 0.0;
	emcmotStatus->axisFlag[axis] = 0;
	emcmotConfig->maxLimit[axis] = MAX_LIMIT;
	emcmotConfig->minLimit[axis] = MIN_LIMIT;
	emcmotConfig->maxOutput[axis] = MAX_OUTPUT;
	emcmotConfig->minOutput[axis] = MIN_OUTPUT;
	emcmotConfig->minFerror[axis] = 0.0;	/* gives a true linear ferror
						 */
	emcmotConfig->maxFerror[axis] = MAX_FERROR;
	emcmotDebug->ferrorCurrent[axis] = 0.0;
	emcmotDebug->ferrorHighMark[axis] = 0.0;
	emcmotStatus->outputScale[axis] = OUTPUT_SCALE;
	emcmotStatus->outputOffset[axis] = OUTPUT_OFFSET;
	emcmotStatus->inputScale[axis] = INPUT_SCALE;
	emcmotDebug->inverseInputScale[axis] = 1.0 / INPUT_SCALE;
	emcmotStatus->inputOffset[axis] = INPUT_OFFSET;
	emcmotDebug->inverseOutputScale[axis] = 1.0 / OUTPUT_SCALE;
	emcmotStatus->axVscale[axis] = 1.0;
	emcmotConfig->axisLimitVel[axis] = 1.0;
	emcmotDebug->bigVel[axis] = 1.0;
	SET_AXIS_ENABLE_FLAG(axis, 0);
	SET_AXIS_ACTIVE_FLAG(axis, 0);	/* default is not to use it; need an
					   explicit activate */
	SET_AXIS_NSL_FLAG(axis, 0);
	SET_AXIS_PSL_FLAG(axis, 0);
	SET_AXIS_NHL_FLAG(axis, 0);
	SET_AXIS_PHL_FLAG(axis, 0);
	SET_AXIS_INPOS_FLAG(axis, 1);
	SET_AXIS_HOMING_FLAG(axis, 0);
	SET_AXIS_HOMED_FLAG(axis, 0);
	SET_AXIS_FERROR_FLAG(axis, 0);
	SET_AXIS_FAULT_FLAG(axis, 0);
	SET_AXIS_ERROR_FLAG(axis, 0);
	emcmotConfig->axisPolarity[axis] = (EMCMOT_AXIS_FLAG) 0xFFFFFFFF;
	/* will read encoders directly, so don't set them here */
    }

    /* FIXME-- add emcmotError */

    /* init min-max-avg stats */
    mmxavgInit(&emcmotDebug->tMmxavg, emcmotDebug->tMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->sMmxavg, emcmotDebug->sMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->nMmxavg, emcmotDebug->nMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->yMmxavg, emcmotDebug->yMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fMmxavg, emcmotDebug->fMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fyMmxavg, emcmotDebug->fyMmxavgSpace,
	MMXAVG_SIZE);
    emcmotDebug->tMin = 0.0;
    emcmotDebug->tMax = 0.0;
    emcmotDebug->tAvg = 0.0;
    emcmotDebug->sMin = 0.0;
    emcmotDebug->sMax = 0.0;
    emcmotDebug->sAvg = 0.0;
    emcmotDebug->nMin = 0.0;
    emcmotDebug->nMax = 0.0;
    emcmotDebug->nAvg = 0.0;
    emcmotDebug->yMin = 0.0;
    emcmotDebug->yMax = 0.0;
    emcmotDebug->yAvg = 0.0;
    emcmotDebug->fyMin = 0.0;
    emcmotDebug->fyMax = 0.0;
    emcmotDebug->fyAvg = 0.0;
    emcmotDebug->fMin = 0.0;
    emcmotDebug->fMax = 0.0;
    emcmotDebug->fAvg = 0.0;

    emcmotDebug->cur_time = emcmotDebug->last_time = 0.0;
    emcmotDebug->start_time = etime();
    emcmotDebug->running_time = 0.0;

    /* init motion emcmotDebug->queue */
    if (-1 == tpCreate(&emcmotDebug->queue, DEFAULT_TC_QUEUE_SIZE,
	    emcmotDebug->queueTcSpace)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to create motion emcmotDebug->queue\n");
	return -1;
    }
//    tpInit(&emcmotDebug->queue); // tpInit called from tpCreate
    tpSetCycleTime(&emcmotDebug->queue, emcmotConfig->trajCycleTime);
    tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
    tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel);
    tpSetAmax(&emcmotDebug->queue, emcmotStatus->acc);

    /* init the axis components */
    pid.p = P_GAIN;
    pid.i = I_GAIN;
    pid.d = D_GAIN;
    pid.ff0 = FF0_GAIN;
    pid.ff1 = FF1_GAIN;
    pid.ff2 = FF2_GAIN;
    pid.backlash = BACKLASH;
    pid.bias = BIAS;
    pid.maxError = MAX_ERROR;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (-1 == tpCreate(&emcmotDebug->freeAxis[axis], FREE_AXIS_QUEUE_SIZE,
		emcmotDebug->freeAxisTcSpace[axis])) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"MOTION: failed to create axis emcmotDebug->queue %d\n",
		axis);
	    return -1;
	}
	tpInit(&emcmotDebug->freeAxis[axis]);
	tpSetCycleTime(&emcmotDebug->freeAxis[axis],
	    emcmotConfig->trajCycleTime);
	/* emcmotDebug->freePose is inited to 0's in decl */
	tpSetPos(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	tpSetVmax(&emcmotDebug->freeAxis[axis], emcmotStatus->vel);
	tpSetAmax(&emcmotDebug->freeAxis[axis], emcmotStatus->acc);
	pidInit(&emcmotConfig->pid[axis]);
	pidSetGains(&emcmotConfig->pid[axis], pid);
	cubicInit(&emcmotDebug->cubic[axis]);
    }
    extEncoderSetIndexModel(EXT_ENCODER_INDEX_MODEL_MANUAL);
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotDebug->rawInput[axis] = 0.0;
	emcmotDebug->rawOutput[axis] = 0.0;
	emcmotDebug->coarseJointPos[axis] = 0.0;
	emcmotDebug->jointPos[axis] = 0.0;
	emcmotDebug->jointVel[axis] = 0.0;
	emcmotStatus->axisPos[axis] = 0.0;
	emcmotDebug->oldJointPos[axis] = 0.0;
	emcmotDebug->outJointPos[axis] = 0.0;
	emcmotDebug->homingPhase[axis] = 0;
	emcmotDebug->latchFlag[axis] = 0;
	emcmotDebug->saveLatch[axis] = 0.0;
	emcmotStatus->input[axis] = 0.0;
	emcmotDebug->oldInput[axis] = 0.0;
	emcmotDebug->oldInputValid[axis] = 0;
	emcmotStatus->output[axis] = 0.0;
	emcmotDebug->jointHome[axis] = 0.0;

	extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
    }
    emcmotStatus->tail = 0;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_comm_buffers() complete\n" );
    return 0;
}

/* init_threads() creates realtime threads, exports functions to
   do the realtime control, and adds the functions to the threads.
*/
static int init_threads(void)
{
    double base_period_sec, servo_period_sec, traj_period_sec;
    int servo_base_ratio, traj_servo_ratio;
    int retval;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_threads() starting...\n" );

    /* if base_period not specified, assume same as servo_period */
    if (base_period_nsec == 0) {
	base_period_nsec = servo_period_nsec;
    }
    /* servo period must be greater or equal to base period */
    if (servo_period_nsec < base_period_nsec) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: bad servo period %d nsec\n", servo_period_nsec);
	return -1;
    }
    /* traj period must be at least 2x servo period */
    if (traj_period_nsec < 2 * servo_period_nsec) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: bad traj period %d nsec\n", traj_period_nsec);
	return -1;
    }
    /* convert desired periods to floating point */
    base_period_sec = base_period_nsec * 0.000000001;
    servo_period_sec = servo_period_nsec * 0.000000001;
    traj_period_sec = traj_period_nsec * 0.000000001;
    /* calculate period ratios, round to nearest integer */
    servo_base_ratio = (servo_period_sec / base_period_sec) + 0.5;
    traj_servo_ratio = (traj_period_sec / servo_period_sec) + 0.5;
    /* revise desired periods to be integer multiples of each other */
    servo_period_nsec = base_period_nsec * servo_base_ratio;
    traj_period_nsec = servo_period_nsec * traj_servo_ratio;
    /* create HAL threads for each period */
    /* only create base thread if it is faster than servo thread */
    if (servo_base_ratio > 1) {
	retval = hal_create_thread("base-thread", base_period_nsec, 0);
	if (retval != HAL_SUCCESS) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"MOTION: failed to create %d nsec base thread\n",
		base_period_nsec);
	    return -1;
	}
    }
    retval = hal_create_thread("servo-thread", servo_period_nsec, 1);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to create %d nsec servo thread\n",
	    servo_period_nsec);
	return -1;
    }
    retval = hal_create_thread("traj-thread", traj_period_nsec, 1);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to create %d nsec traj thread\n",
	    traj_period_nsec);
	return -1;
    }

    /* export realtime functions that do the real work */
    retval = hal_export_funct("motion-controller", emcmotController,
	0 /* arg */, 1 /* uses_fp */, 0 /* reentrant */, mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to export controller function\n");
	return -1;
    }
    retval = hal_export_funct("motion-command-handler", emcmotCommandHandler,
	0 /* arg */, 1 /* uses_fp */, 0 /* reentrant */, mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to export command handler function\n");
	return -1;
    }
#if 0
    /* FIXME - currently the traj planner is called from the controller */
    /* eventually it will be a separate function */
    retval = hal_export_funct("motion-traj-planner", emcmotTrajPlanner, 0	/* arg
	 */ , 1 /* uses_fp */ ,
	0 /* reentrant */ , mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to export traj planner function\n");
	return -1;
    }
#endif

    /* add command handler function to servo thread */
    retval =
	hal_add_funct_to_thread("motion-command-handler", "servo-thread", 1);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to add motion-command-handler to servo-thread\n");
	return -1;
    }
    /* add motion controller function to servo thread */
    retval = hal_add_funct_to_thread("motion-controller", "servo-thread", 2);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to add motion-controller to servo-thread\n");
	return -1;
    }
#if 0
    /* FIXME - currently traj and handler are all inside the controller */
    /* add trajectory planner function to traj thread */
    retval = hal_add_funct_to_thread("motion-traj-planner", "traj-thread", 1);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to add motion-traj-planner to traj-thread\n");
	return -1;
    }
#endif

    /* init the time and rate using functions to affect traj, the pids,
       and the cubics properly, since they're coupled */

    retval = setTrajCycleTime(TRAJ_CYCLE_TIME);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: setTrajCycleTime() failed\n");
	return -1;
    }

    retval = setServoCycleTime(SERVO_CYCLE_TIME);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: setTrajCycleTime() failed\n");
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_threads() complete\n" );
    return 0;
}


/* call this when setting the trajectory cycle time */
static int setTrajCycleTime(double secs)
{
    static int t;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: setting Traj cycle time to %d nsecs\n", secs*1e9 );

    /* make sure it's not zero */
    if (secs <= 0.0) {

	return -1;
    }

    emcmot_config_change();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (secs / emcmotConfig->servoCycleTime + 0.5);

    /* set traj planner */
    tpSetCycleTime(&emcmotDebug->queue, secs);

    /* set the free planners, cubic interpolation rate and segment time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	tpSetCycleTime(&emcmotDebug->freeAxis[t], secs);
	cubicSetInterpolationRate(&emcmotDebug->cubic[t],
	    emcmotConfig->interpolationRate);
    }

    /* copy into status out */
    emcmotConfig->trajCycleTime = secs;

    return 0;
}

/* call this when setting the servo cycle time */
static int setServoCycleTime(double secs)
{
    static int t;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: setting Servo cycle time to %d nsecs\n", secs*1e9 );

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return -1;
    }

    emcmot_config_change();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (emcmotConfig->trajCycleTime / secs + 0.5);

    /* set the cubic interpolation rate and PID cycle time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	cubicSetInterpolationRate(&emcmotDebug->cubic[t],
	    emcmotConfig->interpolationRate);
	cubicSetSegmentTime(&emcmotDebug->cubic[t], secs);
	pidSetCycleTime(&emcmotConfig->pid[t], secs);
    }

    /* copy into status out */
    emcmotConfig->servoCycleTime = secs;

    return 0;
}



