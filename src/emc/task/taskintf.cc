/********************************************************************
* Description: taskintf.cc
*   Interface functions for motion.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/

#include <math.h>		// isnan()
#include <float.h>		// DBL_MAX
#include <string.h>		// memcpy() strncpy()

#include "usrmotintf.h"		// usrmotInit(), usrmotReadEmcmotStatus(),
				// etc.
#include "motion.h"		// emcmot_command_t,STATUS, etc.
#include "emcglb.h"		// EMC_INIFILE

#include "iniaxis.hh"
#include "initraj.hh"

/* define this to catch isnan errors, for rtlinux FPU register 
   problem testing */
#define ISNAN_TRAP

// MOTION INTERFACE

/*! \todo FIXME - this decl was originally much later in the file, moved
here temporarily for debugging */
static emcmot_status_t emcmotStatus;

/*
  Implementation notes:

  Initing:  the emcmot interface needs to be inited once, but nml_traj_init()
  and nml_servo_init() can be called in any order. Similarly, the emcmot
  interface needs to be exited once, but nml_traj_exit() and nml_servo_exit()
  can be called in any order. They can also be called multiple times. Flags
  are used to signify if initing has been done, or if the final exit has
  been called.
  */

static emcmot_command_t emcmotCommand;

static int emcmotTrajInited = 0;	// non-zero means traj called init
static int emcmotAxisInited[EMCMOT_MAX_AXIS] = { 0 };	// non-zero means axis called init

__attribute__ ((unused))
static int emcmotIoInited = 0;	// non-zero means io called init
static int emcmotion_initialized = 0;	// non-zero means both
						// emcMotionInit called.

// saved value of velocity last sent out, so we don't send redundant requests
// used by emcTrajSetVelocity(), emcMotionAbort()
static double lastVel = -1.0;
static double last_ini_maxvel = -1.0;

// EMC_AXIS functions

// local status data, not provided by emcmot
static unsigned long localMotionHeartbeat = 0;
static int localMotionCommandType = 0;
static int localMotionEchoSerialNumber = 0;
static unsigned char localEmcAxisAxisType[EMCMOT_MAX_AXIS];
static double localEmcAxisUnits[EMCMOT_MAX_AXIS];
static double localEmcMaxAcceleration = DBL_MAX;

// axes are numbered 0..NUM-1

/*
  In emcmot, we need to set the cycle time for traj, and the interpolation
  rate, in any order, but both need to be done. 
 */

int emcAxisSetAxis(int axis, unsigned char axisType)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    localEmcAxisAxisType[axis] = axisType;

    return 0;
}

int emcAxisSetUnits(int axis, double units)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    localEmcAxisUnits[axis] = units;

    return 0;
}

/*! \todo Another #if 0 */
#if 0
int emcAxisSetGains(int axis, double p, double i, double d,
		    double ff0, double ff1, double ff2,
		    double bias, double maxError, double deadband)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_PID;
    emcmotCommand.axis = axis;

    emcmotCommand.pid.p = p;
    emcmotCommand.pid.i = i;
    emcmotCommand.pid.d = d;
    emcmotCommand.pid.ff0 = ff0;
    emcmotCommand.pid.ff1 = ff1;
    emcmotCommand.pid.ff2 = ff2;
    emcmotCommand.pid.bias = bias;
    emcmotCommand.pid.maxError = maxError;
    emcmotCommand.pid.deadband = deadband;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.pid.p) ||
	isnan(emcmotCommand.pid.i) ||
	isnan(emcmotCommand.pid.d) ||
	isnan(emcmotCommand.pid.ff0) ||
	isnan(emcmotCommand.pid.ff1) ||
	isnan(emcmotCommand.pid.ff2) ||
	isnan(emcmotCommand.pid.bias) ||
	isnan(emcmotCommand.pid.maxError) ||
	isnan(emcmotCommand.pid.deadband)) {
	printf("isnan error in emcAxisSetGains\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}
#endif

int emcAxisSetBacklash(int axis, double backlash)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_BACKLASH;
    emcmotCommand.axis = axis;

    emcmotCommand.backlash = backlash;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.backlash)) {
	printf("isnan error in emcAxisSetBacklash\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*! \todo Another #if 0 */
#if 0
int emcAxisSetCycleTime(int axis, double cycleTime)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    if (cycleTime <= 0.0) {
	return -1;
    }

    emcmotCommand.command = EMCMOT_SET_SERVO_CYCLE_TIME;
    emcmotCommand.cycleTime = cycleTime;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetInputScale(int axis, double scale, double offset)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_INPUT_SCALE;
    emcmotCommand.axis = axis;
    emcmotCommand.scale = scale;
    emcmotCommand.offset = offset;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.scale) || isnan(emcmotCommand.offset)) {
	printf("isnan eror in emcAxisSetInputScale\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetOutputScale(int axis, double scale, double offset)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_OUTPUT_SCALE;
    emcmotCommand.axis = axis;
    emcmotCommand.scale = scale;
    emcmotCommand.offset = offset;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.scale) || isnan(emcmotCommand.offset)) {
	printf("isnan eror in emcAxisSetOutputScale\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

#endif				/* #if 0 */

// saved values of limits, since emcmot expects them to be set in
// pairs and we set them individually.
static double saveMinLimit[EMCMOT_MAX_AXIS];
static double saveMaxLimit[EMCMOT_MAX_AXIS];

int emcAxisSetMinPositionLimit(int axis, double limit)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_POSITION_LIMITS;
    emcmotCommand.axis = axis;
    emcmotCommand.maxLimit = saveMaxLimit[axis];
    emcmotCommand.minLimit = limit;
    saveMinLimit[axis] = limit;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.maxLimit) || isnan(emcmotCommand.minLimit)) {
	printf("isnan error in emcAxisSetMinPosition\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMaxPositionLimit(int axis, double limit)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_POSITION_LIMITS;
    emcmotCommand.axis = axis;
    emcmotCommand.minLimit = saveMinLimit[axis];
    emcmotCommand.maxLimit = limit;
    saveMaxLimit[axis] = limit;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.maxLimit) || isnan(emcmotCommand.minLimit)) {
	printf("isnan error in emcAxisSetMaxPosition\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*! \todo Another #if 0 */
#if 0

// saved values of limits, since emcmot expects them to be set in
// pairs and we set them individually.
static double saveMinOutput[EMCMOT_MAX_AXIS];
static double saveMaxOutput[EMCMOT_MAX_AXIS];

int emcAxisSetMinOutputLimit(int axis, double limit)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_OUTPUT_LIMITS;
    emcmotCommand.axis = axis;
    emcmotCommand.maxLimit = saveMaxOutput[axis];
    emcmotCommand.minLimit = limit;
    saveMinOutput[axis] = limit;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.maxLimit) || isnan(emcmotCommand.minLimit)) {
	printf("isnan error in emcAxisSetMinOutputLimit\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMaxOutputLimit(int axis, double limit)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_OUTPUT_LIMITS;
    emcmotCommand.axis = axis;
    emcmotCommand.minLimit = saveMinOutput[axis];
    emcmotCommand.maxLimit = limit;
    saveMaxOutput[axis] = limit;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.maxLimit) || isnan(emcmotCommand.minLimit)) {
	printf("isnan error in emcAxisSetMaxOutputLimit\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}
#endif

int emcAxisSetFerror(int axis, double ferror)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_MAX_FERROR;
    emcmotCommand.axis = axis;
    emcmotCommand.maxFerror = ferror;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.maxFerror)) {
	printf("isnan error in emcAxisSetFerror\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMinFerror(int axis, double ferror)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }
    emcmotCommand.command = EMCMOT_SET_MIN_FERROR;
    emcmotCommand.axis = axis;
    emcmotCommand.minFerror = ferror;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.minFerror)) {
	printf("isnan error in emcAxisSetMinFerror\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetHomingParams(int axis, double home, double offset,
			   double search_vel, double latch_vel,
			   int use_index, int ignore_limits)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_HOMING_PARAMS;
    emcmotCommand.axis = axis;
    emcmotCommand.home = home;
    emcmotCommand.offset = offset;
    emcmotCommand.search_vel = search_vel;
    emcmotCommand.latch_vel = latch_vel;
    emcmotCommand.flags = 0;
    if (use_index) {
	emcmotCommand.flags |= HOME_USE_INDEX;
    }
    if (ignore_limits) {
	emcmotCommand.flags |= HOME_IGNORE_LIMITS;
    }
#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.home) ||
	isnan(emcmotCommand.offset) ||
	isnan(emcmotCommand.search_vel) ||
	isnan(emcmotCommand.latch_vel)) {
	printf("isnan error in emcAxisSetHoming\n");
	return -1;
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*! \todo Another #if 0 */
#if 0
/*! \todo FIXME - to be deleted eventually, these are configured thru HAL */
int emcAxisSetStepParams(int axis, double setup_time, double hold_time)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_STEP_PARAMS;
    emcmotCommand.axis = axis;
    emcmotCommand.setup_time = setup_time;
    emcmotCommand.hold_time = hold_time;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}
#endif

int emcAxisSetMaxVelocity(int axis, double vel)
{
    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return 0;
    }

    if (vel < 0.0) {
	vel = 0.0;
    }

    AXIS_MAX_VELOCITY[axis] = vel;

    emcmotCommand.command = EMCMOT_SET_JOINT_VEL_LIMIT;
    emcmotCommand.axis = axis;
    emcmotCommand.vel = vel;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMaxAcceleration(int axis, double acc)
{

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return 0;
    }
    if (acc < 0.0) {
	acc = 0.0;
    }
    AXIS_MAX_ACCELERATION[axis] = acc;
    emcmotCommand.command = EMCMOT_SET_JOINT_ACC_LIMIT;
    emcmotCommand.axis = axis;
    emcmotCommand.acc = acc;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/* This function checks to see if any axis or the traj has
   been inited already.  At startup, if none have been inited,
   usrmotIniLoad and usrmotInit must be called first.  At
   shutdown, after all have been halted, the usrmotExit must
   be called.
*/

static int AxisOrTrajInited(void)
{
    int axis;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (emcmotAxisInited[axis]) {
	    return 1;
	}
    }
    if (emcmotTrajInited) {
	return 1;
    }
    return 0;
}

int emcAxisInit(int axis)
{
    int retval = 0;

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }
    // init emcmot interface
    if (!AxisOrTrajInited()) {
	usrmotIniLoad(EMC_INIFILE);
	if (0 != usrmotInit("emc2_task")) {
	    return -1;
	}
    }
    emcmotAxisInited[axis] = 1;
    if (0 != iniAxis(axis, EMC_INIFILE)) {
	retval = -1;
    }
    return retval;
}

int emcAxisHalt(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }
    /*! \todo FIXME-- refs global emcStatus; should make EMC_AXIS_STAT an arg here */
    if (NULL != emcStatus && emcmotion_initialized
	&& emcmotAxisInited[axis]) {
	dumpAxis(axis, EMC_INIFILE, &emcStatus->motion.axis[axis]);
    }
    emcmotAxisInited[axis] = 0;

    if (!AxisOrTrajInited()) {
	usrmotExit();		// ours is final exit
    }

    return 0;
}

int emcAxisAbort(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }
    emcmotCommand.command = EMCMOT_AXIS_ABORT;
    emcmotCommand.axis = axis;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisActivate(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_ACTIVATE_JOINT;
    emcmotCommand.axis = axis;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisDeactivate(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_DEACTIVATE_JOINT;
    emcmotCommand.axis = axis;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisOverrideLimits(int axis)
{
    // can have axis < 0, for resuming normal limit checking
    if (axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_OVERRIDE_LIMITS;
    emcmotCommand.axis = axis;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*! \todo Another #if 0 */
#if 0
int emcAxisSetOutput(int axis, double output)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_DAC_OUT;
    emcmotCommand.axis = axis;
    emcmotCommand.dacOut = output;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}
#endif

int emcAxisEnable(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_ENABLE_AMPLIFIER;
    emcmotCommand.axis = axis;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisDisable(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_DISABLE_AMPLIFIER;
    emcmotCommand.axis = axis;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisHome(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_HOME;
    emcmotCommand.axis = axis;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisJog(int axis, double vel)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    if (vel > AXIS_MAX_VELOCITY[axis]) {
	vel = AXIS_MAX_VELOCITY[axis];
    } else if (vel < -AXIS_MAX_VELOCITY[axis]) {
	vel = -AXIS_MAX_VELOCITY[axis];
    }

    emcmotCommand.command = EMCMOT_JOG_CONT;
    emcmotCommand.axis = axis;
    emcmotCommand.vel = vel;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisIncrJog(int axis, double incr, double vel)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    if (vel > AXIS_MAX_VELOCITY[axis]) {
	vel = AXIS_MAX_VELOCITY[axis];
    } else if (vel < -AXIS_MAX_VELOCITY[axis]) {
	vel = -AXIS_MAX_VELOCITY[axis];
    }

    emcmotCommand.command = EMCMOT_JOG_INCR;
    emcmotCommand.axis = axis;
    emcmotCommand.vel = vel;
    emcmotCommand.offset = incr;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisAbsJog(int axis, double pos, double vel)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }

    if (vel > AXIS_MAX_VELOCITY[axis]) {
	vel = AXIS_MAX_VELOCITY[axis];
    } else if (vel < -AXIS_MAX_VELOCITY[axis]) {
	vel = -AXIS_MAX_VELOCITY[axis];
    }

    emcmotCommand.command = EMCMOT_JOG_ABS;
    emcmotCommand.axis = axis;
    emcmotCommand.vel = vel;
    emcmotCommand.offset = pos;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisLoadComp(int axis, const char *file)
{
    return usrmotLoadComp(axis, file);
}

int emcAxisAlter(int axis, double alter)
{
    return usrmotAlter(axis, alter);
}

static emcmot_config_t emcmotConfig;
int get_emcmot_debug_info = 0;

/*
  these globals are set in emcMotionUpdate(), then referenced in
  emcAxisUpdate(), emcTrajUpdate() to save calls to usrmotReadEmcmotStatus
 */
/*! \todo FIXME - next line commented out and moved to top of file for debugging */
//static emcmot_status_t emcmotStatus;*/
static emcmot_debug_t emcmotDebug;
static char errorString[EMCMOT_ERROR_LEN];
static int new_config = 0;

/*! \todo FIXME - debugging - uncomment the following line to log changes in
   AXIS_FLAG */
// #define WATCH_FLAGS 1

int emcAxisUpdate(EMC_AXIS_STAT stat[], int numAxes)
{
/*! \todo FIXME - this function accesses data that has been
   moved.  Once I know what it is used for I'll fix it */

    int axis;
    emcmot_joint_status_t *joint;
#ifdef WATCH_FLAGS
    static int old_joint_flag[8];
#endif

    // check for valid range
    if (numAxes <= 0 || numAxes > EMCMOT_MAX_AXIS) {
	return -1;
    }

    for (axis = 0; axis < numAxes; axis++) {
	/* point to joint data */

	joint = &(emcmotStatus.joint_status[axis]);

	stat[axis].axisType = localEmcAxisAxisType[axis];
	stat[axis].units = localEmcAxisUnits[axis];
/*! \todo Another #if 0 */
#if 0
	stat[axis].inputScale = emcmotStatus.inputScale[axis];
	stat[axis].inputOffset = emcmotStatus.inputOffset[axis];
	stat[axis].outputScale = emcmotStatus.outputScale[axis];
	stat[axis].outputOffset = emcmotStatus.outputOffset[axis];
#endif
	if (new_config) {
	    stat[axis].backlash = joint->backlash;
	    stat[axis].minPositionLimit = joint->min_pos_limit;
	    stat[axis].maxPositionLimit = joint->max_pos_limit;
	    stat[axis].minFerror = joint->min_ferror;
	    stat[axis].maxFerror = joint->max_ferror;
/*! \todo FIXME - should all homing config params be included here? */
	    stat[axis].homeOffset = joint->home_offset;
	}
	stat[axis].setpoint = joint->pos_cmd;
	/*! \todo FIXME - output is the DAC output, now part of HAL */
	stat[axis].output = 0.0;
	stat[axis].input = joint->pos_fb;

	if (get_emcmot_debug_info) {
	    stat[axis].ferrorCurrent = joint->ferror;
	    stat[axis].ferrorHighMark = joint->ferror_high_mark;
	}

	stat[axis].homing = (joint->flag & EMCMOT_AXIS_HOMING_BIT ? 1 : 0);
	stat[axis].homed = (joint->flag & EMCMOT_AXIS_HOMED_BIT ? 1 : 0);
	stat[axis].fault = (joint->flag & EMCMOT_AXIS_FAULT_BIT ? 1 : 0);
	stat[axis].enabled =
	    (joint->flag & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0);

	stat[axis].minSoftLimit =
	    (joint->flag & EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT ? 1 : 0);
	stat[axis].maxSoftLimit =
	    (joint->flag & EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT ? 1 : 0);
	stat[axis].minHardLimit =
	    (joint->flag & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT ? 1 : 0);
	stat[axis].maxHardLimit =
	    (joint->flag & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT ? 1 : 0);
	stat[axis].overrideLimits = (emcmotStatus.overrideLimits);	// one
	// for
	// all

/*! \todo Another #if 0 */
#if 0				/*! \todo FIXME - per-axis Vscale temporarily? removed */
	stat[axis].scale = emcmotStatus.axVscale[axis];
#endif
	usrmotQueryAlter(axis, &stat[axis].alter);
#ifdef WATCH_FLAGS
	if (old_joint_flag[axis] != joint->flag) {
	    printf("joint %d flag: %04X -> %04X\n", axis,
		   old_joint_flag[axis], joint->flag);
	    old_joint_flag[axis] = joint->flag;
	}
#endif
	if (joint->flag & EMCMOT_AXIS_ERROR_BIT) {
	    if (stat[axis].status != RCS_ERROR) {
		rcs_print_error("Error on axis %d, command number %d\n",
				axis, emcmotStatus.commandNumEcho);
		stat[axis].status = RCS_ERROR;
	    }
	} else if (joint->flag & EMCMOT_AXIS_INPOS_BIT) {
	    stat[axis].status = RCS_DONE;
	} else {
	    stat[axis].status = RCS_EXEC;
	}
    }
    return 0;
}

// EMC_TRAJ functions

// local status data, not provided by emcmot
static int localEmcTrajAxes = 0;
static double localEmcTrajLinearUnits = 1.0;
static double localEmcTrajAngularUnits = 1.0;
static int localEmcTrajMotionId = 0;

int emcTrajSetAxes(int axes)
{
    if (axes <= 0 || axes > EMCMOT_MAX_AXIS) {
	return -1;
    }

    localEmcTrajAxes = axes;
    emcmotCommand.command = EMCMOT_SET_NUM_AXES;
    emcmotCommand.axis = axes;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetUnits(double linearUnits, double angularUnits)
{
    if (linearUnits <= 0.0 || angularUnits <= 0.0) {
	return -1;
    }

    localEmcTrajLinearUnits = linearUnits;
    localEmcTrajAngularUnits = angularUnits;

    return 0;
}

/*! \todo Another #if 0 */
#if 0
int emcTrajSetCycleTime(double cycleTime)
{
    if (cycleTime <= 0.0) {
	return -1;
    }

    emcmotCommand.command = EMCMOT_SET_TRAJ_CYCLE_TIME;
    emcmotCommand.cycleTime = cycleTime;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}
#endif

int emcTrajSetMode(int mode)
{
    switch (mode) {
    case EMC_TRAJ_MODE_FREE:
	emcmotCommand.command = EMCMOT_FREE;
	return usrmotWriteEmcmotCommand(&emcmotCommand);

    case EMC_TRAJ_MODE_COORD:
	emcmotCommand.command = EMCMOT_COORD;
	return usrmotWriteEmcmotCommand(&emcmotCommand);

    case EMC_TRAJ_MODE_TELEOP:
	emcmotCommand.command = EMCMOT_TELEOP;
	return usrmotWriteEmcmotCommand(&emcmotCommand);

    default:
	return -1;
    }
}

int emcTrajSetTeleopVector(EmcPose vel)
{
    emcmotCommand.command = EMCMOT_SET_TELEOP_VECTOR;
    emcmotCommand.pos = vel;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetVelocity(double vel, double ini_maxvel)
{
    int retval;

    if (vel < 0.0) {
	vel = 0.0;
    } else if (vel > TRAJ_MAX_VELOCITY) {
	vel = TRAJ_MAX_VELOCITY;
    }

    if (ini_maxvel < 0.0) {
	    ini_maxvel = 0.0;
    } else if (vel > TRAJ_MAX_VELOCITY) {
	    ini_maxvel = TRAJ_MAX_VELOCITY;
    }

/*! \todo Another #if 0 */
#if 0
    /*! \todo FIXME-- this fixes rapid rate reset problem */
    if (vel == lastVel) {
	// suppress it
	return 0;
    }
#endif

    emcmotCommand.command = EMCMOT_SET_VEL;
    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;

    retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (0 == retval) {
	lastVel = vel;
	last_ini_maxvel = ini_maxvel;
    }

    return retval;
}

int emcTrajSetAcceleration(double acc)
{
    if (acc < 0.0) {
	acc = 0.0;
    } else if (acc > localEmcMaxAcceleration) {
	acc = localEmcMaxAcceleration;
    }

    emcmotCommand.command = EMCMOT_SET_ACC;
    emcmotCommand.acc = acc;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*
  emcmot has no limits on max velocity, acceleration so we'll save them
  here and apply them in the functions above
  */
int emcTrajSetMaxVelocity(double vel)
{
    if (vel < 0.0) {
	vel = 0.0;
    }

    TRAJ_MAX_VELOCITY = vel;

    emcmotCommand.command = EMCMOT_SET_VEL_LIMIT;
    emcmotCommand.vel = vel;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetMaxAcceleration(double acc)
{
    if (acc < 0.0) {
	acc = 0.0;
    }

    localEmcMaxAcceleration = acc;

    return 0;
}

int emcTrajSetHome(EmcPose home)
{
    emcmotCommand.command = EMCMOT_SET_WORLD_HOME;

    emcmotCommand.pos = home;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.pos.tran.x) ||
	isnan(emcmotCommand.pos.tran.y)
	|| isnan(emcmotCommand.pos.tran.z)) {
	printf("isnan error in emcTrajSetHome\n");
	return 0;		// ignore it for now, just don't send it
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetScale(double scale)
{
    if (scale < 0.0) {
	scale = 0.0;
    }

    emcmotCommand.command = EMCMOT_SCALE;
    emcmotCommand.scale = scale;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetAdaptiveFeed(unsigned char enable)
{
    emcmotCommand.command = EMCMOT_ADAPTIVE_FEED;

    if ( enable ) {
	emcmotCommand.flags = 1;
    } else {
	emcmotCommand.flags = 0;
    }
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetMotionId(int id)
{

    if (EMC_DEBUG_MOTION_TIME & EMC_DEBUG) {
	if (id != localEmcTrajMotionId) {
	    rcs_print("Outgoing motion id is %d.\n", id);
	}
    }

    localEmcTrajMotionId = id;

    return 0;
}

int emcTrajInit()
{
    int retval = 0;

    // init emcmot interface
    if (!AxisOrTrajInited()) {
	usrmotIniLoad(EMC_INIFILE);
	if (0 != usrmotInit("emc2_task")) {
	    return -1;
	}
    }
    emcmotTrajInited = 1;
    // initialize parameters from ini file
    if (0 != iniTraj(EMC_INIFILE)) {
	retval = -1;
    }
    return retval;
}

int emcTrajHalt()
{
    emcmotTrajInited = 0;

    if (!AxisOrTrajInited()) {
	usrmotExit();		// ours is final exit
    }

    return 0;
}

int emcTrajEnable()
{
    emcmotCommand.command = EMCMOT_ENABLE;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajDisable()
{
    emcmotCommand.command = EMCMOT_DISABLE;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajAbort()
{
    emcmotCommand.command = EMCMOT_ABORT;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajPause()
{
    emcmotCommand.command = EMCMOT_PAUSE;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajStep()
{
    emcmotCommand.command = EMCMOT_STEP;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajResume()
{
    emcmotCommand.command = EMCMOT_RESUME;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajDelay(double delay)
{
    /* nothing need be done here - it's done in task controller */

    return 0;
}

int emcTrajSetSpindleSync(double sync) 
{
    emcmotCommand.command = EMCMOT_SET_SPINDLESYNC;
    emcmotCommand.spindlesync = sync;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetTermCond(int cond, double tolerance)
{
    emcmotCommand.command = EMCMOT_SET_TERM_COND;
    emcmotCommand.termCond =
	(cond ==
	 EMC_TRAJ_TERM_COND_STOP ? EMCMOT_TERM_COND_STOP :
	 EMCMOT_TERM_COND_BLEND);
    emcmotCommand.tolerance = tolerance;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajLinearMove(EmcPose end, int type, double vel, double ini_maxvel, double acc)
{

    emcmotCommand.command = EMCMOT_SET_LINE;

    emcmotCommand.pos = end;

    emcmotCommand.id = localEmcTrajMotionId;
    emcmotCommand.motion_type = type;
    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;
    emcmotCommand.acc = acc;
#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.pos.tran.x) ||
	isnan(emcmotCommand.pos.tran.y)
	|| isnan(emcmotCommand.pos.tran.z)) {
	printf("isnan error in emcTrajLinearMove\n");
	return 0;		// ignore it for now, just don't send it
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajCircularMove(EmcPose end, PM_CARTESIAN center,
			PM_CARTESIAN normal, int turn, int type, double vel, double ini_maxvel, double acc)
{
    emcmotCommand.command = EMCMOT_SET_CIRCLE;

    emcmotCommand.pos = end;
    emcmotCommand.motion_type = type;

    emcmotCommand.center.x = center.x;
    emcmotCommand.center.y = center.y;
    emcmotCommand.center.z = center.z;

    emcmotCommand.normal.x = normal.x;
    emcmotCommand.normal.y = normal.y;
    emcmotCommand.normal.z = normal.z;

    emcmotCommand.turn = turn;
    emcmotCommand.id = localEmcTrajMotionId;

    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;
    emcmotCommand.acc = acc;

#ifdef ISNAN_TRAP
    if (isnan(emcmotCommand.pos.tran.x) ||
	isnan(emcmotCommand.pos.tran.y) ||
	isnan(emcmotCommand.pos.tran.z) ||
	isnan(emcmotCommand.pos.a) ||
	isnan(emcmotCommand.pos.b) ||
	isnan(emcmotCommand.pos.c) ||
	isnan(emcmotCommand.center.x) ||
	isnan(emcmotCommand.center.y) ||
	isnan(emcmotCommand.center.z) ||
	isnan(emcmotCommand.normal.x) ||
	isnan(emcmotCommand.normal.y) || isnan(emcmotCommand.normal.z)) {
	printf("isnan error in emcTrajCircularMove\n");
	return 0;		// ignore it for now, just don't send it
    }
#endif

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*! \todo Another #if 0 */
#if 0
int emcTrajSetProbeIndex(int index)
{
    emcmotCommand.command = EMCMOT_SET_PROBE_INDEX;
    emcmotCommand.probeIndex = index;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetProbePolarity(int polarity)
{
    emcmotCommand.command = EMCMOT_SET_PROBE_POLARITY;
    emcmotCommand.level = polarity;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}
#endif				/* #if 0 */

int emcTrajClearProbeTrippedFlag()
{
    emcmotCommand.command = EMCMOT_CLEAR_PROBE_FLAGS;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajProbe(EmcPose pos)
{
    emcmotCommand.command = EMCMOT_PROBE;
    emcmotCommand.pos.tran.x = pos.tran.x;
    emcmotCommand.pos.tran.y = pos.tran.y;
    emcmotCommand.pos.tran.z = pos.tran.z;
    emcmotCommand.id = localEmcTrajMotionId;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

static int last_id = 0;
static int last_id_printed = 0;
static int last_status = 0;
static double last_id_time;

int emcTrajUpdate(EMC_TRAJ_STAT * stat)
{
    int axis;

    stat->axes = localEmcTrajAxes;
    stat->linearUnits = localEmcTrajLinearUnits;
    stat->angularUnits = localEmcTrajAngularUnits;

    stat->mode =
	emcmotStatus.
	motionFlag & EMCMOT_MOTION_TELEOP_BIT ? EMC_TRAJ_MODE_TELEOP
	: (emcmotStatus.
	   motionFlag & EMCMOT_MOTION_COORD_BIT ? EMC_TRAJ_MODE_COORD :
	   EMC_TRAJ_MODE_FREE);

    /* enabled if motion enabled and all axes enabled */
    stat->enabled = 0;		/* start at disabled */
    if (emcmotStatus.motionFlag & EMCMOT_MOTION_ENABLE_BIT) {
	for (axis = 0; axis < localEmcTrajAxes; axis++) {
/*! \todo Another #if 0 */
#if 0				/*! \todo FIXME - the axis flag has been moved to the joint struct */
	    if (!emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_ENABLE_BIT) {
		break;
	    }
#endif
	    /* got here, then all are enabled */
	    stat->enabled = 1;
	}
    }

    stat->inpos =
	emcmotStatus.motionFlag & EMCMOT_MOTION_INPOS_BIT ? 1 : 0;
    stat->queue = emcmotStatus.depth;
    stat->activeQueue = emcmotStatus.activeDepth;
    stat->queueFull = emcmotStatus.queueFull;
    stat->id = emcmotStatus.id;
    stat->motion_type = emcmotStatus.motionType;
    if (EMC_DEBUG_MOTION_TIME & EMC_DEBUG) {
	if (stat->id != last_id) {
	    if (last_id != last_id_printed) {
		rcs_print("Motion id %d took %f seconds.\n", last_id,
			  etime() - last_id_time);
		last_id_printed = last_id;
	    }
	    last_id = stat->id;
	    last_id_time = etime();
	}
    }

    stat->paused = emcmotStatus.paused;
    stat->scale = emcmotStatus.qVscale;

    stat->position = emcmotStatus.carte_pos_cmd;

    stat->actualPosition = emcmotStatus.carte_pos_fb;

    stat->velocity = emcmotStatus.vel;
    stat->acceleration = emcmotStatus.acc;
    stat->maxAcceleration = localEmcMaxAcceleration;

    if (emcmotStatus.motionFlag & EMCMOT_MOTION_ERROR_BIT) {
	stat->status = RCS_ERROR;
    } else if (stat->inpos && (stat->queue == 0)) {
	stat->status = RCS_DONE;
    } else {
	stat->status = RCS_EXEC;
    }

    if (EMC_DEBUG_MOTION_TIME & EMC_DEBUG) {
	if (stat->status == RCS_DONE && last_status != RCS_DONE
	    && stat->id != last_id_printed) {
	    rcs_print("Motion id %d took %f seconds.\n", last_id,
		      etime() - last_id_time);
	    last_id_printed = last_id = stat->id;
	    last_id_time = etime();
	}
    }

    stat->probedPosition.tran.x = emcmotStatus.probedPos.tran.x;
    stat->probedPosition.tran.y = emcmotStatus.probedPos.tran.y;
    stat->probedPosition.tran.z = emcmotStatus.probedPos.tran.z;
    stat->probeval = emcmotStatus.probeVal;
    stat->probe_tripped = emcmotStatus.probeTripped;

    if (new_config) {
	stat->cycleTime = emcmotConfig.trajCycleTime;
/*! \todo Another #if 0 */
#if 0
	stat->probe_index = emcmotConfig.probeIndex;
	stat->probe_polarity = emcmotConfig.probePolarity;
#endif
	stat->kinematics_type = emcmotConfig.kinematics_type;
	stat->maxVelocity = emcmotConfig.limitVel;
    }

    return 0;
}

// EMC_MOTION functions
int emcMotionInit()
{
    int r1;
    int r2;
    int axis;

    r1 = -1;
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (0 == emcAxisInit(axis)) {
	    r1 = 0;		// at least one is okay
	}
    }
    r2 = emcTrajInit();

    if (r1 == 0 && r2 == 0) {
	emcmotion_initialized = 1;
    }

    return (r1 == 0 && r2 == 0) ? 0 : -1;
}

int emcMotionHalt()
{
    int r1, r2, r3;
    int t;

    r1 = -1;
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	if (0 == emcAxisHalt(t)) {
	    r1 = 0;		// at least one is okay
	}
    }

    r2 = emcTrajDisable();
    r3 = emcTrajHalt();

    emcmotion_initialized = 0;

    return (r1 == 0 && r2 == 0 && r3 == 0) ? 0 : -1;
}

int emcMotionAbort()
{
    int r1;
    int r2;
    int t;

    r1 = -1;
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	if (0 == emcAxisAbort(t)) {
	    r1 = 0;		// at least one is okay
	}
    }

    r2 = emcTrajAbort();

    // reset optimization flag which suppresses duplicate speed requests
    lastVel = -1.0;

    return (r1 == 0 && r2 == 0) ? 0 : -1;
}

int emcMotionSetDebug(int debug)
{
    emcmotCommand.command = EMCMOT_SET_DEBUG;
    emcmotCommand.debug = debug;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*! \function emcMotionSetAout()
    
    This function sends a EMCMOT_SET_AOUT message to the motion controller.
    That one plans a AOUT command when motion starts or right now.

    @parameter	index	which output gets modified
    @parameter	now	wheather change is imediate or synched with motion
    @parameter	start	value set at start of motion
    @parameter	end	value set at end of motion
*/
int emcMotionSetAout(unsigned char index, double start, double end, unsigned char now)
{
    emcmotCommand.command = EMCMOT_SET_AOUT;
    emcmotCommand.now = now;
    emcmotCommand.out = index;
  /*! \todo FIXME-- if this works, set up some dedicated cmd fields instead of
     borrowing these */
    emcmotCommand.minLimit = start;
    emcmotCommand.maxLimit = end;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*! \function emcMotionSetDout()
    
    This function sends a EMCMOT_SET_DOUT message to the motion controller.
    That one plans a DOUT command when motion starts or right now.

    @parameter	index	which output gets modified
    @parameter	now	wheather change is imediate or synched with motion
    @parameter	start	value set at start of motion
    @parameter	end	value set at end of motion
*/
int emcMotionSetDout(unsigned char index, unsigned char start,
		     unsigned char end, unsigned char now)
{
    emcmotCommand.command = EMCMOT_SET_DOUT;
    emcmotCommand.now = now;
    emcmotCommand.out = index;
    emcmotCommand.start = start;
    emcmotCommand.end = end;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcMotionUpdate(EMC_MOTION_STAT * stat)
{
    int r1;
    int r2;
    int axis;
    int error;
    int exec;

    // read the emcmot status
    if (0 != usrmotReadEmcmotStatus(&emcmotStatus)) {
	return -1;
    }

    new_config = 0;
    if (emcmotStatus.config_num != emcmotConfig.config_num) {
	if (0 != usrmotReadEmcmotConfig(&emcmotConfig)) {
	    return -1;
	}
	new_config = 1;
    }

    if (get_emcmot_debug_info) {
	if (0 != usrmotReadEmcmotDebug(&emcmotDebug)) {
	    return -1;
	}
    }
    // read the emcmot error
    if (0 != usrmotReadEmcmotError(errorString)) {
	// no error, so ignore
    } else {
	// an error to report
	emcOperatorError(0, errorString);
    }

    // save the heartbeat and command number locally,
    // for use with emcMotionUpdate
    localMotionHeartbeat = emcmotStatus.heartbeat;
    localMotionCommandType = emcmotStatus.commandEcho;	/*! \todo FIXME-- not NML one! */
    localMotionEchoSerialNumber = emcmotStatus.commandNumEcho;

    r1 = emcAxisUpdate(&stat->axis[0], EMCMOT_MAX_AXIS);
    r2 = emcTrajUpdate(&stat->traj);
    stat->heartbeat = localMotionHeartbeat;
    stat->command_type = localMotionCommandType;
    stat->echo_serial_number = localMotionEchoSerialNumber;
    stat->debug = emcmotConfig.debug;

    // set the status flag
    error = 0;
    exec = 0;

    for (axis = 0; axis < stat->traj.axes; axis++) {
	if (stat->axis[axis].status == RCS_ERROR) {
	    error = 1;
	    break;
	}
	if (stat->axis[axis].status == RCS_EXEC) {
	    exec = 1;
	    break;
	}
    }
    if (stat->traj.status == RCS_ERROR) {
	error = 1;
    } else if (stat->traj.status == RCS_EXEC) {
	exec = 1;
    }

    if (error) {
	stat->status = RCS_ERROR;
    } else if (exec) {
	stat->status = RCS_EXEC;
    } else {
	stat->status = RCS_DONE;
    }
    return (r1 == 0 && r2 == 0) ? 0 : -1;
}
