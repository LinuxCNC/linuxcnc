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
#include <float.h>              // DBL_MAX
#include <string.h>		// memcpy() strncpy()
#include <unistd.h>             // unlink()

#include "usrmotintf.h"		// usrmotInit(), usrmotReadEmcmotStatus(),
				// etc.
#include "motion.h"		// emcmot_command_t,STATUS, etc.
#include "motion_debug.h"
#include "emc.hh"
#include "emccfg.h"		// EMC_INIFILE
#include "emcglb.h"		// EMC_INIFILE
#include "emc_nml.hh"
#include "rcs_print.hh"
#include "timer.hh"
#include "inifile.hh"
#include "iniaxis.hh"
#include "inijoint.hh"
#include "initraj.hh"

/* define this to catch isnan errors, for rtlinux FPU register 
   problem testing */
#define ISNAN_TRAP

#ifdef ISNAN_TRAP
#define CATCH_NAN(cond) do {                           \
    if (cond) {                                        \
        printf("isnan error in %s()\n", __FUNCTION__); \
        return -1;                                     \
    }                                                  \
} while(0)
#else
#define CATCH_NAN(cond) do {} while(0)
#endif


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

static struct TrajConfig_t TrajConfig;
static struct JointConfig_t JointConfig[EMCMOT_MAX_JOINTS];
static struct AxisConfig_t AxisConfig[EMCMOT_MAX_AXIS];

static emcmot_command_t emcmotCommand;

__attribute__ ((unused))
static int emcmotIoInited = 0;	// non-zero means io called init
static int emcmotion_initialized = 0;	// non-zero means both
						// emcMotionInit called.

// local status data, not provided by emcmot
static unsigned long localMotionHeartbeat = 0;
static int localMotionCommandType = 0;
static int localMotionEchoSerialNumber = 0;

//FIXME-AJ: see if needed
//static double localEmcAxisUnits[EMCMOT_MAX_AXIS];

// axes and joints are numbered 0..NUM-1

/*
  In emcmot, we need to set the cycle time for traj, and the interpolation
  rate, in any order, but both need to be done. 
 */

/*! functions involving joints */

int emcJointSetType(int joint, unsigned char jointType)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    JointConfig[joint].Type = jointType;

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %d)\n", __FUNCTION__, joint, jointType);
    }
    return 0;
}

int emcJointSetUnits(int joint, double units)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    JointConfig[joint].Units = units;

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f)\n", __FUNCTION__, joint, units);
    }
    return 0;
}

int emcJointSetBacklash(int joint, double backlash)
{
    CATCH_NAN(isnan(backlash));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_JOINT_BACKLASH;
    emcmotCommand.joint = joint;
    emcmotCommand.backlash = backlash;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, backlash, retval);
    }
    return retval;
}

int emcJointSetMinPositionLimit(int joint, double limit)
{
    CATCH_NAN(isnan(limit));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    JointConfig[joint].MinLimit = limit;

    emcmotCommand.command = EMCMOT_SET_JOINT_POSITION_LIMITS;
    emcmotCommand.joint = joint;
    emcmotCommand.maxLimit = JointConfig[joint].MinLimit;
    emcmotCommand.minLimit = JointConfig[joint].MaxLimit;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, limit, retval);
    }
    return retval;
}

int emcJointSetMaxPositionLimit(int joint, double limit)
{
    CATCH_NAN(isnan(limit));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    JointConfig[joint].MaxLimit = limit;

    emcmotCommand.command = EMCMOT_SET_JOINT_POSITION_LIMITS;
    emcmotCommand.joint = joint;
    emcmotCommand.minLimit = JointConfig[joint].MinLimit;
    emcmotCommand.maxLimit = JointConfig[joint].MaxLimit;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, limit, retval);
    }
    return retval;
}

int emcJointSetMotorOffset(int joint, double offset) 
{
    CATCH_NAN(isnan(offset));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }
    emcmotCommand.command = EMCMOT_SET_JOINT_MOTOR_OFFSET;
    emcmotCommand.joint = joint;
    emcmotCommand.motor_offset = offset;
    
    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, offset, retval);
    }
    return retval;
}

int emcJointSetFerror(int joint, double ferror)
{
    CATCH_NAN(isnan(ferror));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_JOINT_MAX_FERROR;
    emcmotCommand.joint = joint;
    emcmotCommand.maxFerror = ferror;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, ferror, retval);
    }
    return retval;
}

int emcJointSetMinFerror(int joint, double ferror)
{
    CATCH_NAN(isnan(ferror));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }
    emcmotCommand.command = EMCMOT_SET_JOINT_MIN_FERROR;
    emcmotCommand.joint = joint;
    emcmotCommand.minFerror = ferror;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, ferror, retval);
    }
    return retval;
}

int emcJointSetHomingParams(int joint, double home, double offset, double home_final_vel,
			   double search_vel, double latch_vel,
			   int use_index, int ignore_limits, int is_shared,
			   int sequence,int volatile_home, int locking_indexer)
{
    CATCH_NAN(isnan(home) || isnan(offset) || isnan(home_final_vel) || isnan(search_vel) || isnan(latch_vel));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_SET_JOINT_HOMING_PARAMS;
    emcmotCommand.joint = joint;
    emcmotCommand.home = home;
    emcmotCommand.offset = offset;
    emcmotCommand.home_final_vel = home_final_vel;
    emcmotCommand.search_vel = search_vel;
    emcmotCommand.latch_vel = latch_vel;
    emcmotCommand.flags = 0;
    emcmotCommand.home_sequence = sequence;
    emcmotCommand.volatile_home = volatile_home;
    if (use_index) {
	emcmotCommand.flags |= HOME_USE_INDEX;
    }
    if (ignore_limits) {
	emcmotCommand.flags |= HOME_IGNORE_LIMITS;
    }
    if (is_shared) {
	emcmotCommand.flags |= HOME_IS_SHARED;
    }
    if (locking_indexer) {
        emcmotCommand.flags |= HOME_UNLOCK_FIRST;
    }

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f, %.4f, %.4f, %.4f, %.4f, %d, %d, %d, %d, %d) returned %d\n",
          __FUNCTION__, joint, home, offset, home_final_vel, search_vel, latch_vel,
          use_index, ignore_limits, is_shared, sequence, volatile_home, retval);
    }
    return retval;
}

int emcJointSetMaxVelocity(int joint, double vel)
{
    CATCH_NAN(isnan(vel));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    if (vel < 0.0) {
	vel = 0.0;
    }

    JointConfig[joint].MaxVel = vel;

    emcmotCommand.command = EMCMOT_SET_JOINT_VEL_LIMIT;
    emcmotCommand.joint = joint;
    emcmotCommand.vel = vel;
    
    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, vel, retval);
    }
    return retval;
}

int emcJointSetMaxAcceleration(int joint, double acc)
{
    CATCH_NAN(isnan(acc));

    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }
    if (acc < 0.0) {
	acc = 0.0;
    }
    JointConfig[joint].MaxAccel = acc;
    //FIXME-AJ: need functions for setting the AXIS_MAX_ACCEL (either from the ini, or from kins..)
    emcmotCommand.command = EMCMOT_SET_JOINT_ACC_LIMIT;
    emcmotCommand.joint = joint;
    emcmotCommand.acc = acc;
    
    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, joint, acc, retval);
    }
    return retval;
}

/*! functions involving carthesian Axes (X,Y,Z,A,B,C,U,V,W) */
    
int emcAxisSetMinPositionLimit(int axis, double limit)
{
    CATCH_NAN(isnan(limit));

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS || !(TrajConfig.AxisMask & (1 << axis))) {
	return 0;
    }

    AxisConfig[axis].MinLimit = limit;

    emcmotCommand.command = EMCMOT_SET_AXIS_POSITION_LIMITS;
    emcmotCommand.axis = axis;
    emcmotCommand.minLimit = AxisConfig[axis].MinLimit;
    emcmotCommand.maxLimit = AxisConfig[axis].MaxLimit;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, axis, limit, retval);
    }
    return retval;
}

int emcAxisSetMaxPositionLimit(int axis, double limit)
{
    CATCH_NAN(isnan(limit));

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS || !(TrajConfig.AxisMask & (1 << axis))) {
	return 0;
    }

    AxisConfig[axis].MaxLimit = limit;

    emcmotCommand.command = EMCMOT_SET_AXIS_POSITION_LIMITS;
    emcmotCommand.axis = axis;
    emcmotCommand.minLimit = AxisConfig[axis].MinLimit;
    emcmotCommand.maxLimit = AxisConfig[axis].MaxLimit;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, axis, limit, retval);
    }
    return retval;
}

int emcAxisSetMaxVelocity(int axis, double vel)
{
    CATCH_NAN(isnan(vel));

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS || !(TrajConfig.AxisMask & (1 << axis))) {
	return 0;
    }

    if (vel < 0.0) {
	vel = 0.0;
    }

    AxisConfig[axis].MaxVel = vel;

    emcmotCommand.command = EMCMOT_SET_AXIS_VEL_LIMIT;
    emcmotCommand.axis = axis;
    emcmotCommand.vel = vel;
    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, axis, vel, retval);
    }
    return retval;
}

int emcAxisSetMaxAcceleration(int axis, double acc)
{
    CATCH_NAN(isnan(acc));

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS || !(TrajConfig.AxisMask & (1 << axis))) {
	return 0;
    }

    if (acc < 0.0) {
	acc = 0.0;
    }
    
    AxisConfig[axis].MaxAccel = acc;    

    emcmotCommand.command = EMCMOT_SET_AXIS_ACC_LIMIT;
    emcmotCommand.axis = axis;
    emcmotCommand.acc = acc;
    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f) returned %d\n", __FUNCTION__, axis, acc, retval);
    }
    return retval;
}

int emcAxisSetHome(int axis, double home)
{

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }
    
    AxisConfig[axis].Home = home;

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %.4f)\n", __FUNCTION__, axis, home);
    }
    return 0;
}

double emcAxisGetMaxVelocity(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
        return 0;
    }

    return AxisConfig[axis].MaxVel;
}

double emcAxisGetMaxAcceleration(int axis)
{
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
        return 0;
    }

    return AxisConfig[axis].MaxAccel;
}

int emcAxisUpdate(EMC_AXIS_STAT stat[], int numAxes)
{
    int axis_num;
    
    // check for valid range
    if (numAxes <= 0 || numAxes > EMCMOT_MAX_AXIS) {
	return -1;
    }

    for (axis_num = 0; axis_num < numAxes; axis_num++) {
    }
    return 0;
}

/* This function checks to see if any joint or the traj has
   been inited already.  At startup, if none have been inited,
   usrmotIniLoad and usrmotInit must be called first.  At
   shutdown, after all have been halted, the usrmotExit must
   be called.
*/

static int JointOrTrajInited(void)
{
    int joint;

    for (joint = 0; joint < EMCMOT_MAX_JOINTS; joint++) {
	if (JointConfig[joint].Inited) {
	    return 1;
	}
    }
    if (TrajConfig.Inited) {
	return 1;
    }
    return 0;
}

int emcJointInit(int joint)
{
    int retval = 0;
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }
    // init emcmot interface
    if (!JointOrTrajInited()) {
	usrmotIniLoad(EMC_INIFILE);
	if (0 != usrmotInit("emc2_task")) {
	    return -1;
	}
    }
    JointConfig[joint].Inited = 1;
    if (0 != iniJoint(joint, EMC_INIFILE)) {
	retval = -1;
    }
    return retval;
}

int emcAxisInit(int axis)
{
    int retval = 0;

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	return 0;
    }
    // init emcmot interface
    if (!JointOrTrajInited()) {
	usrmotIniLoad(EMC_INIFILE);
	if (0 != usrmotInit("emc2_task")) {
	    return -1;
	}
    }
    AxisConfig[axis].Inited = 1;
    if (0 != iniAxis(axis, EMC_INIFILE)) {
	retval = -1;
    }
    return retval;
}

int emcJointHalt(int joint)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }
    /*! \todo FIXME-- refs global emcStatus; should make EMC_JOINT_STAT an arg here */
    if (NULL != emcStatus && emcmotion_initialized
	&& JointConfig[joint].Inited) {
    }
    JointConfig[joint].Inited = 0;

    if (!JointOrTrajInited()) {
	usrmotExit();		// ours is final exit
    }

    return 0;
}

int emcJointAbort(int joint)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }
    emcmotCommand.command = EMCMOT_JOINT_ABORT;
    emcmotCommand.joint = joint;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJointActivate(int joint)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_JOINT_ACTIVATE;
    emcmotCommand.joint = joint;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d) returned %d\n", __FUNCTION__, joint, retval);
    }
    return retval;
}

int emcJointDeactivate(int joint)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_JOINT_DEACTIVATE;
    emcmotCommand.joint = joint;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJointOverrideLimits(int joint)
{
    // can have joint < 0, for resuming normal limit checking
    if (joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_OVERRIDE_LIMITS;
    emcmotCommand.joint = joint;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJointEnable(int joint)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_JOINT_ENABLE_AMPLIFIER;
    emcmotCommand.joint = joint;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJointDisable(int joint)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_JOINT_DISABLE_AMPLIFIER;
    emcmotCommand.joint = joint;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJointHome(int joint)
{
    if (joint < -1 || joint >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    emcmotCommand.command = EMCMOT_JOINT_HOME;
    emcmotCommand.joint = joint;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJointUnhome(int joint)
{
	if (joint < -2 || joint >= EMCMOT_MAX_JOINTS) {
		return 0;
	}

	emcmotCommand.command = EMCMOT_JOINT_UNHOME;
	emcmotCommand.joint = joint;

	return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJogCont(int nr, double vel)
{
    if (nr < 0 || nr >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    if (vel > JointConfig[nr].MaxVel) {
	vel = JointConfig[nr].MaxVel;
    } else if (vel < -JointConfig[nr].MaxVel) {
	vel = -JointConfig[nr].MaxVel;
    }

    emcmotCommand.command = EMCMOT_JOG_CONT;
    emcmotCommand.joint = nr;
    emcmotCommand.vel = vel;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJogIncr(int nr, double incr, double vel)
{
    if (nr < 0 || nr >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    if (vel > JointConfig[nr].MaxVel) {
	vel = JointConfig[nr].MaxVel;
    } else if (vel < -JointConfig[nr].MaxVel) {
	vel = -JointConfig[nr].MaxVel;
    }

    emcmotCommand.command = EMCMOT_JOG_INCR;
    emcmotCommand.joint = nr;
    emcmotCommand.vel = vel;
    emcmotCommand.offset = incr;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJogAbs(int nr, double pos, double vel)
{
    if (nr < 0 || nr >= EMCMOT_MAX_JOINTS) {
	return 0;
    }

    if (vel > JointConfig[nr].MaxVel) {
	vel = JointConfig[nr].MaxVel;
    } else if (vel < -JointConfig[nr].MaxVel) {
	vel = -JointConfig[nr].MaxVel;
    }

    emcmotCommand.command = EMCMOT_JOG_ABS;
    emcmotCommand.joint = nr;
    emcmotCommand.vel = vel;
    emcmotCommand.offset = pos;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcJogStop(int nr)
{
    if (nr < 0 || nr >= EMCMOT_MAX_JOINTS) {
	return 0;
    }
    emcmotCommand.command = EMCMOT_JOINT_ABORT;
    emcmotCommand.joint = nr;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}


int emcJointLoadComp(int joint, const char *file, int type)
{
    return usrmotLoadComp(joint, file, type);
}

static emcmot_config_t emcmotConfig;
int get_emcmot_debug_info = 0;

/*
  these globals are set in emcMotionUpdate(), then referenced in
  emcJointUpdate(), emcTrajUpdate() to save calls to usrmotReadEmcmotStatus
 */
static emcmot_debug_t emcmotDebug;
static char errorString[EMCMOT_ERROR_LEN];
static int new_config = 0;

/*! \todo FIXME - debugging - uncomment the following line to log changes in
   JOINT_FLAG */
// #define WATCH_FLAGS 1

int emcJointUpdate(EMC_JOINT_STAT stat[], int numJoints)
{
/*! \todo FIXME - this function accesses data that has been
   moved.  Once I know what it is used for I'll fix it */

    int joint_num;
    emcmot_joint_status_t *joint;
#ifdef WATCH_FLAGS
    static int old_joint_flag[8];
#endif

    // check for valid range
    if (numJoints <= 0 || numJoints > EMCMOT_MAX_JOINTS) {
	return -1;
    }

    for (joint_num = 0; joint_num < numJoints; joint_num++) {
	/* point to joint data */

	joint = &(emcmotStatus.joint_status[joint_num]);

	stat[joint_num].jointType = JointConfig[joint_num].Type;
	stat[joint_num].units = JointConfig[joint_num].Units;
	if (new_config) {
	    stat[joint_num].backlash = joint->backlash;
	    stat[joint_num].minPositionLimit = joint->min_pos_limit;
	    stat[joint_num].maxPositionLimit = joint->max_pos_limit;
	    stat[joint_num].minFerror = joint->min_ferror;
	    stat[joint_num].maxFerror = joint->max_ferror;
/*! \todo FIXME - should all homing config params be included here? */
//	    stat[joint_num].homeOffset = joint->home_offset;
	}
	stat[joint_num].output = joint->pos_cmd;
	stat[joint_num].input = joint->pos_fb;
	stat[joint_num].velocity = joint->vel_cmd;
	stat[joint_num].ferrorCurrent = joint->ferror;
	stat[joint_num].ferrorHighMark = joint->ferror_high_mark;

	stat[joint_num].homing = (joint->flag & EMCMOT_JOINT_HOMING_BIT ? 1 : 0);
	stat[joint_num].homed = (joint->flag & EMCMOT_JOINT_HOMED_BIT ? 1 : 0);
	stat[joint_num].fault = (joint->flag & EMCMOT_JOINT_FAULT_BIT ? 1 : 0);
	stat[joint_num].enabled = (joint->flag & EMCMOT_JOINT_ENABLE_BIT ? 1 : 0);
	stat[joint_num].inpos = (joint->flag & EMCMOT_JOINT_INPOS_BIT ? 1 : 0);

/* FIXME - soft limits are now applied to the command, and should never
   happen */
	stat[joint_num].minSoftLimit = 0;
	stat[joint_num].maxSoftLimit = 0;
	stat[joint_num].minHardLimit =
	    (joint->flag & EMCMOT_JOINT_MIN_HARD_LIMIT_BIT ? 1 : 0);
	stat[joint_num].maxHardLimit =
	    (joint->flag & EMCMOT_JOINT_MAX_HARD_LIMIT_BIT ? 1 : 0);
	stat[joint_num].overrideLimits = !!(emcmotStatus.overrideLimitMask);	// one
	// for
	// all

/*! \todo Another #if 0 */
#if 0				/*! \todo FIXME - per-joint Vscale temporarily? removed */
	stat[joint_num].scale = emcmotStatus.axVscale[joint_num];
#endif
#ifdef WATCH_FLAGS
	if (old_joint_flag[joint_num] != joint->flag) {
	    printf("joint %d flag: %04X -> %04X\n", joint_num,
		   old_joint_flag[joint_num], joint->flag);
	    old_joint_flag[joint_num] = joint->flag;
	}
#endif
	if (joint->flag & EMCMOT_JOINT_ERROR_BIT) {
	    if (stat[joint_num].status != RCS_ERROR) {
		rcs_print_error("Error on joint %d, command number %d\n",
				joint_num, emcmotStatus.commandNumEcho);
		stat[joint_num].status = RCS_ERROR;
	    }
	} else if (joint->flag & EMCMOT_JOINT_INPOS_BIT) {
	    stat[joint_num].status = RCS_DONE;
	} else {
	    stat[joint_num].status = RCS_EXEC;
	}
    }
    return 0;
}

// EMC_TRAJ functions

int emcTrajSetJoints(int joints)
{
    if (joints <= 0 || joints > EMCMOT_MAX_JOINTS) {
	rcs_print("emcTrajSetJoints failing: joints=%d\n",
		joints);
	return -1;
    }

    TrajConfig.Joints = joints;
    emcmotCommand.command = EMCMOT_SET_NUM_JOINTS;
    emcmotCommand.joint = joints;
    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d) returned %d\n", __FUNCTION__, joints, retval);
    }
    return retval;
}

int emcTrajSetAxes(int axes, int axismask)
{
    if (axes <= 0 || axes > EMCMOT_MAX_AXIS) { //FIXME-AJ: figure out jointmask/axismask || axismask >= (1<<joints)) {
	rcs_print("emcTrajSetAxes failing: axes=%d axismask=%x\n",
		axes, axismask);
	return -1;
    }

    TrajConfig.Axes = axes;
    TrajConfig.AxisMask = axismask;
    
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%d, %d)\n", __FUNCTION__, axes, axismask);
    }
    return 0;
}

int emcTrajSetUnits(double linearUnits, double angularUnits)
{
    if (linearUnits <= 0.0 || angularUnits <= 0.0) {
	return -1;
    }

    TrajConfig.LinearUnits = linearUnits;
    TrajConfig.AngularUnits = angularUnits;

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%.4f, %.4f)\n", __FUNCTION__, linearUnits, angularUnits);
    }
    return 0;
}

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

int emcTrajSetVelocity(double vel, double ini_maxvel)
{
    if (vel < 0.0) {
	vel = 0.0;
    } else if (vel > TrajConfig.MaxVel) {
	vel = TrajConfig.MaxVel;
    }

    if (ini_maxvel < 0.0) {
	    ini_maxvel = 0.0;
    } else if (vel > TrajConfig.MaxVel) {
	    ini_maxvel = TrajConfig.MaxVel;
    }

    emcmotCommand.command = EMCMOT_SET_VEL;
    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%.4f, %.4f) returned %d\n", __FUNCTION__, vel, ini_maxvel, retval);
    }
    return retval;
}

int emcTrajSetAcceleration(double acc)
{
    if (acc < 0.0) {
	acc = 0.0;
    } else if (acc > TrajConfig.MaxAccel) {
	acc = TrajConfig.MaxAccel;
    }

    emcmotCommand.command = EMCMOT_SET_ACC;
    emcmotCommand.acc = acc;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%.4f) returned %d\n", __FUNCTION__, acc, retval);
    }
    return retval;
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

    TrajConfig.MaxVel = vel;

    emcmotCommand.command = EMCMOT_SET_VEL_LIMIT;
    emcmotCommand.vel = vel;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%.4f) returned %d\n", __FUNCTION__, vel, retval);
    }
    return retval;
}

int emcTrajSetMaxAcceleration(double acc)
{
    if (acc < 0.0) {
	acc = 0.0;
    }

    TrajConfig.MaxAccel = acc;

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%.4f)\n", __FUNCTION__, acc);
    }
    return 0;
}

int emcTrajSetHome(EmcPose home)
{
    CATCH_NAN(isnan(home.tran.x) || isnan(home.tran.y) || isnan(home.tran.z) ||
        isnan(home.a) || isnan(home.b) || isnan(home.c) ||
        isnan(home.u) || isnan(home.v) || isnan(home.w));

    emcmotCommand.command = EMCMOT_SET_WORLD_HOME;
    emcmotCommand.pos = home;

    int retval = usrmotWriteEmcmotCommand(&emcmotCommand);

    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
        rcs_print("%s(%.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f) returned %d\n", 
          __FUNCTION__, home.tran.x, home.tran.y, home.tran.z, home.a, home.b, home.c, 
          home.u, home.v, home.w, retval);
    }
    return retval;
}

int emcTrajSetScale(double scale)
{
    if (scale < 0.0) {
	scale = 0.0;
    }

    emcmotCommand.command = EMCMOT_FEED_SCALE;
    emcmotCommand.scale = scale;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetSpindleScale(double scale)
{
    if (scale < 0.0) {
	scale = 0.0;
    }

    emcmotCommand.command = EMCMOT_SPINDLE_SCALE;
    emcmotCommand.scale = scale;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetFOEnable(unsigned char mode)
{
    emcmotCommand.command = EMCMOT_FS_ENABLE;
    emcmotCommand.mode = mode;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetFHEnable(unsigned char mode)
{
    emcmotCommand.command = EMCMOT_FH_ENABLE;
    emcmotCommand.mode = mode;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetSOEnable(unsigned char mode)
{
    emcmotCommand.command = EMCMOT_SS_ENABLE;
    emcmotCommand.mode = mode;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetAFEnable(unsigned char enable)
{
    emcmotCommand.command = EMCMOT_AF_ENABLE;

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
	if (id != TrajConfig.MotionId) {
	    rcs_print("Outgoing motion id is %d.\n", id);
	}
    }

    TrajConfig.MotionId = id;

    return 0;
}

int emcTrajInit()
{
    int retval = 0;

    TrajConfig.Inited = 0;
    TrajConfig.Joints = 0;
    TrajConfig.MaxAccel = DBL_MAX;
    TrajConfig.Axes = 0;
    TrajConfig.AxisMask = 0;
    TrajConfig.LinearUnits = 1.0;
    TrajConfig.AngularUnits = 1.0;
    TrajConfig.MotionId = 0;
    TrajConfig.MaxVel = DEFAULT_TRAJ_MAX_VELOCITY;

    // init emcmot interface
    if (!JointOrTrajInited()) {
	usrmotIniLoad(EMC_INIFILE);
	if (0 != usrmotInit("emc2_task")) {
	    return -1;
	}
    }
    TrajConfig.Inited = 1;
    // initialize parameters from ini file
    if (0 != iniTraj(EMC_INIFILE)) {
	retval = -1;
    }
    return retval;
}

int emcTrajHalt()
{
    TrajConfig.Inited = 0;

    if (!JointOrTrajInited()) {
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

double emcTrajGetLinearUnits()
{
    return TrajConfig.LinearUnits;
}

double emcTrajGetAngularUnits()
{
    return TrajConfig.AngularUnits;
}

int emcTrajSetOffset(EmcPose tool_offset)
{
    emcmotCommand.command = EMCMOT_SET_OFFSET;
    emcmotCommand.tool_offset = tool_offset;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetSpindleSync(double fpr, bool wait_for_index) 
{
    emcmotCommand.command = EMCMOT_SET_SPINDLESYNC;
    emcmotCommand.spindlesync = fpr;
    emcmotCommand.flags = wait_for_index;
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

int emcTrajLinearMove(EmcPose end, int type, double vel, double ini_maxvel, double acc,
                      int indexrotary)
{
    CATCH_NAN(isnan(end.tran.x) || isnan(end.tran.y) || isnan(end.tran.z) ||
        isnan(end.a) || isnan(end.b) || isnan(end.c) ||
        isnan(end.u) || isnan(end.v) || isnan(end.w));

    emcmotCommand.command = EMCMOT_SET_LINE;

    emcmotCommand.pos = end;

    emcmotCommand.id = TrajConfig.MotionId;
    emcmotCommand.motion_type = type;
    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;
    emcmotCommand.acc = acc;
    emcmotCommand.turn = indexrotary;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajCircularMove(EmcPose end, PM_CARTESIAN center,
			PM_CARTESIAN normal, int turn, int type, double vel, double ini_maxvel, double acc)
{
    CATCH_NAN(isnan(end.tran.x) || isnan(end.tran.y) || isnan(end.tran.z) ||
        isnan(end.a) || isnan(end.b) || isnan(end.c) ||
        isnan(end.u) || isnan(end.v) || isnan(end.w) ||
        isnan(center.x) || isnan(center.y) || isnan(center.z) ||
        isnan(normal.x) || isnan(normal.y) || isnan(normal.z));

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
    emcmotCommand.id = TrajConfig.MotionId;

    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;
    emcmotCommand.acc = acc;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajClearProbeTrippedFlag()
{
    emcmotCommand.command = EMCMOT_CLEAR_PROBE_FLAGS;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajProbe(EmcPose pos, int type, double vel, double ini_maxvel, double acc, unsigned char probe_type)
{
    CATCH_NAN(isnan(pos.tran.x) || isnan(pos.tran.y) || isnan(pos.tran.z) ||
        isnan(pos.a) || isnan(pos.b) || isnan(pos.c) ||
        isnan(pos.u) || isnan(pos.v) || isnan(pos.w));

    emcmotCommand.command = EMCMOT_PROBE;
    emcmotCommand.pos = pos;
    emcmotCommand.id = TrajConfig.MotionId;
    emcmotCommand.motion_type = type;
    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;
    emcmotCommand.acc = acc;
    emcmotCommand.probe_type = probe_type;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajRigidTap(EmcPose pos, double vel, double ini_maxvel, double acc)
{
    CATCH_NAN(isnan(pos.tran.x) || isnan(pos.tran.y) || isnan(pos.tran.z));

    emcmotCommand.command = EMCMOT_RIGID_TAP;
    emcmotCommand.pos.tran = pos.tran;
    emcmotCommand.id = TrajConfig.MotionId;
    emcmotCommand.vel = vel;
    emcmotCommand.ini_maxvel = ini_maxvel;
    emcmotCommand.acc = acc;

    return usrmotWriteEmcmotCommand(&emcmotCommand);
}


static int last_id = 0;
static int last_id_printed = 0;
static int last_status = 0;
static double last_id_time;

int emcTrajUpdate(EMC_TRAJ_STAT * stat)
{
    int joint, enables;

    stat->joints = TrajConfig.Joints;
    stat->axes = TrajConfig.Axes;
    stat->axis_mask = TrajConfig.AxisMask;
    stat->linearUnits = TrajConfig.LinearUnits;
    stat->angularUnits = TrajConfig.AngularUnits;

    stat->mode =
	emcmotStatus.
	motionFlag & EMCMOT_MOTION_TELEOP_BIT ? EMC_TRAJ_MODE_TELEOP
	: (emcmotStatus.
	   motionFlag & EMCMOT_MOTION_COORD_BIT ? EMC_TRAJ_MODE_COORD :
	   EMC_TRAJ_MODE_FREE);

    /* enabled if motion enabled and all joints enabled */
    stat->enabled = 0;		/* start at disabled */
    if (emcmotStatus.motionFlag & EMCMOT_MOTION_ENABLE_BIT) {
	for (joint = 0; joint < TrajConfig.Joints; joint++) {
/*! \todo Another #if 0 */
#if 0				/*! \todo FIXME - the axis flag has been moved to the joint struct */
	    if (!emcmotStatus.axisFlag[axis] & EMCMOT_JOINT_ENABLE_BIT) {
		break;
	    }
#endif
	    /* got here, then all are enabled */
	    stat->enabled = 1;
	}
    }

    stat->inpos = emcmotStatus.motionFlag & EMCMOT_MOTION_INPOS_BIT;
    stat->queue = emcmotStatus.depth;
    stat->activeQueue = emcmotStatus.activeDepth;
    stat->queueFull = emcmotStatus.queueFull;
    stat->id = emcmotStatus.id;
    stat->motion_type = emcmotStatus.motionType;
    stat->distance_to_go = emcmotStatus.distance_to_go;
    stat->dtg = emcmotStatus.dtg;
    stat->current_vel = emcmotStatus.current_vel;
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
    stat->scale = emcmotStatus.feed_scale;
    stat->spindle_scale = emcmotStatus.spindle_scale;

    stat->position = emcmotStatus.carte_pos_cmd;

    stat->actualPosition = emcmotStatus.carte_pos_fb;

    stat->velocity = emcmotStatus.vel;
    stat->acceleration = emcmotStatus.acc;
    stat->maxAcceleration = TrajConfig.MaxAccel;

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

    stat->probedPosition = emcmotStatus.probedPos;

    stat->probeval = emcmotStatus.probeVal;
    stat->probing = emcmotStatus.probing;
    stat->probe_tripped = emcmotStatus.probeTripped;
    
    if (emcmotStatus.motionFlag & EMCMOT_MOTION_COORD_BIT)
        enables = emcmotStatus.enables_queued;
    else
        enables = emcmotStatus.enables_new;
    
    stat->feed_override_enabled = enables & FS_ENABLED;
    stat->spindle_override_enabled = enables & SS_ENABLED;
    stat->adaptive_feed_enabled = enables & AF_ENABLED;
    stat->feed_hold_enabled = enables & FH_ENABLED;

    if (new_config) {
	stat->cycleTime = emcmotConfig.trajCycleTime;
	stat->kinematics_type = emcmotConfig.kinType;
	stat->maxVelocity = emcmotConfig.limitVel;
    }

    return 0;
}

int emcPositionLoad() {
    double positions[EMCMOT_MAX_JOINTS];
    IniFile ini;
    ini.Open(EMC_INIFILE);
    const char *posfile = ini.Find("POSITION_FILE", "TRAJ");
    ini.Close();
    if(!posfile || !posfile[0]) return 0;
    FILE *f = fopen(posfile, "r");
    if(!f) return 0;
    for(int i=0; i<EMCMOT_MAX_JOINTS; i++) {
	int r = fscanf(f, "%lf", &positions[i]);
	if(r != 1) { fclose(f); return -1; }
    }
    fclose(f);
    int result = 0;
    for(int i=0; i<EMCMOT_MAX_JOINTS; i++) {
	if(emcJointSetMotorOffset(i, -positions[i]) != 0) result = -1;;
    }
    return result;
}


int emcPositionSave() {
    IniFile ini;
    const char *posfile;

    ini.Open(EMC_INIFILE);
    try {
        posfile = ini.Find("POSITION_FILE", "TRAJ");
    } catch (IniFile::Exception e) {
        ini.Close();
        return -1;
    }
    ini.Close();

    if(!posfile || !posfile[0]) return 0;
    // like the var file, make sure the posfile is recreated according to umask
    unlink(posfile);
    FILE *f = fopen(posfile, "w");
    if(!f) return -1;
    for(int i=0; i<EMCMOT_MAX_JOINTS; i++) {
	int r = fprintf(f, "%.17f\n", emcmotStatus.joint_status[i].pos_fb);
	if(r < 0) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

// EMC_MOTION functions
int emcMotionInit()
{
    int r1, r2, r3, r4;
    int joint, axis;
    EmcPose home;
    
    r1 = emcTrajInit(); // we want to check Traj first, the sane defaults for units are there
    // it also determines the number of existing joints, and axes

    r2 = 0;
    for (joint = 0; joint < TrajConfig.Joints; joint++) {
	if (0 != emcJointInit(joint)) {
	    r2 = -1;		// at least one is busted
	}
    }

    r3 = 0;
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
        if (TrajConfig.AxisMask & (1<<axis)) {
	    if (0 != emcAxisInit(axis)) {
	        r3 = -1;		// at least one is busted
	    }
	}
    }

    home.tran.x = AxisConfig[0].Home;
    home.tran.y = AxisConfig[1].Home;
    home.tran.z = AxisConfig[2].Home;
    home.a = AxisConfig[3].Home;
    home.b = AxisConfig[4].Home;
    home.c = AxisConfig[5].Home;
    home.u = AxisConfig[6].Home;
    home.v = AxisConfig[7].Home;
    home.w = AxisConfig[8].Home;

    emcTrajSetHome(home);

    r4 = emcPositionLoad();

    if (r1 == 0 && r2 == 0 && r3 == 0 && r4 == 0) {
	emcmotion_initialized = 1;
    }

    return (r1 == 0 && r2 == 0 && r3 == 0) ? 0 : -1;
}

int emcMotionHalt()
{
    int r1, r2, r3, r4;
    int t;

    r1 = -1;
    for (t = 0; t < EMCMOT_MAX_JOINTS; t++) {
	if (0 == emcJointHalt(t)) {
	    r1 = 0;		// at least one is okay
	}
    }

    r2 = emcTrajDisable();
    r3 = emcTrajHalt();
    r4 = emcPositionSave();
    emcmotion_initialized = 0;

    return (r1 == 0 && r2 == 0 && r3 == 0 && r4 == 0) ? 0 : -1;
}

int emcMotionAbort()
{
    int r1;
    int r2;
    int r3 = 0;
    int t;

    r1 = -1;
    for (t = 0; t < EMCMOT_MAX_JOINTS; t++) {
	if (0 == emcJointAbort(t)) {
	    r1 = 0;		// at least one is okay
	}
    }

    r2 = emcTrajAbort();

    return (r1 == 0 && r2 == 0 && r3 == 0) ? 0 : -1;
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

int emcSpindleAbort(void)
{
    return emcSpindleOff();
}

int emcSpindleSpeed(double speed, double css_factor, double offset)
{

    if (emcmotStatus.spindle.speed == 0)
	return 0; //spindle stopped, not updating speed

    emcmotCommand.command = EMCMOT_SPINDLE_ON;

    if ((emcmotStatus.spindle.speed < 0) && (speed > 0)) {
	emcmotCommand.vel = speed * -1;
	emcmotCommand.ini_maxvel = css_factor * -1; 
	emcmotCommand.acc = offset; 
	return usrmotWriteEmcmotCommand(&emcmotCommand);
    } else if ((emcmotStatus.spindle.speed > 0) && (speed > 0)) {
	emcmotCommand.vel = speed;
	emcmotCommand.ini_maxvel = css_factor; 
	emcmotCommand.acc = offset; 
	return usrmotWriteEmcmotCommand(&emcmotCommand);
    }
    
    return -1; //can't have negative speeds
}

int emcSpindleOn(double speed, double css_factor, double offset)
{

    emcmotCommand.command = EMCMOT_SPINDLE_ON;
    emcmotCommand.vel = speed;
    emcmotCommand.ini_maxvel = css_factor;
    emcmotCommand.acc = offset;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcSpindleOff()
{
    emcmotCommand.command = EMCMOT_SPINDLE_OFF;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcSpindleBrakeRelease()
{
    emcmotCommand.command = EMCMOT_SPINDLE_BRAKE_RELEASE;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcSpindleBrakeEngage()
{
    emcmotCommand.command = EMCMOT_SPINDLE_BRAKE_ENGAGE;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcSpindleIncrease()
{
    emcmotCommand.command = EMCMOT_SPINDLE_INCREASE;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcSpindleDecrease()
{
    emcmotCommand.command = EMCMOT_SPINDLE_DECREASE;
    return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcSpindleConstant()
{
    return 0; // nothing to do
}



int emcMotionUpdate(EMC_MOTION_STAT * stat)
{
    int r1, r2, r3;
    int joint;
    int error;
    int exec;
    int dio, aio;

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

    r1 = emcJointUpdate(&stat->joint[0], stat->traj.joints);
    r2 = emcAxisUpdate(&stat->axis[0], stat->traj.axes);
    r3 = emcTrajUpdate(&stat->traj);
    stat->heartbeat = localMotionHeartbeat;
    stat->command_type = localMotionCommandType;
    stat->echo_serial_number = localMotionEchoSerialNumber;
    stat->debug = emcmotConfig.debug;
    
    stat->spindle.enabled = emcmotStatus.spindle.speed != 0;
    stat->spindle.speed = emcmotStatus.spindle.speed;
    stat->spindle.brake = emcmotStatus.spindle.brake;
    stat->spindle.direction = emcmotStatus.spindle.direction;

    for (dio = 0; dio < EMCMOT_MAX_DIO; dio++) {
	stat->synch_di[dio] = emcmotStatus.synch_di[dio];
	stat->synch_do[dio] = emcmotStatus.synch_do[dio];
    }

    for (aio = 0; aio < EMCMOT_MAX_AIO; aio++) {
	stat->analog_input[aio] = emcmotStatus.analog_input[aio];
	stat->analog_output[aio] = emcmotStatus.analog_output[aio];
    }

    // set the status flag
    error = 0;
    exec = 0;

    // FIXME-AJ: joints not axes
    for (joint = 0; joint < stat->traj.joints; joint++) {
	if (stat->joint[joint].status == RCS_ERROR) {
	    error = 1;
	    break;
	}
	if (stat->joint[joint].status == RCS_EXEC) {
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
    return (r1 == 0 && r2 == 0 && r3 == 0) ? 0 : -1;
}
