/********************************************************************
* Description: command.c
*   emcmotCommandhandler() takes commands passed from user space and
*   performs various functions based on the value in emcmotCommand->command.
*   For the full list, see the EMCMOT_COMMAND enum in emcmot.h
*   
*   Most of the configs would be better off being passed via an ioctl
*   implimentation leaving pure realtime data to be handled by
*   emcmotCommmandHandler() - This would provide a small performance
*   increase on slower systems.
*   
* Author:
* Created at:
* Computer:
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/

#include <linux/types.h>
#include <float.h>
#include <math.h>
#include "rtapi.h"
#include "extintf.h"
#include "motion.h"
#include "emcmotglb.h"
#include "mot_priv.h"

/* value for world home position */
EmcPose worldHome = { {0.0, 0.0, 0.0}, 0.0, 0.0, 0.0};

int logSkip = 0;		/* how many to skip, for per-cycle logging */
int loggingAxis = 0;		/* record of which axis to log */
int logStartTime;		/* set when logging is started, and
				   subtracted off each log time for better
				   resolution */
/* kinematics flags */
KINEMATICS_FORWARD_FLAGS fflags = 0;
KINEMATICS_INVERSE_FLAGS iflags = 0;

/* checkLimits() returns 1 if none of the soft or hard limits are
   set, 0 if any are set. Called on a linear and circular move. */
static int checkLimits(void)
{
    int axis;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (!GET_AXIS_ACTIVE_FLAG(axis)) {
	    /* if axis is not active, don't even look at its limits */
	    continue;
	}

	if (GET_AXIS_PSL_FLAG(axis) || GET_AXIS_NSL_FLAG(axis)
	    || GET_AXIS_PHL_FLAG(axis) || GET_AXIS_NHL_FLAG(axis)) {
	    return 0;
	}
    }

    return 1;
}

/* check the value of the axis and velocity against current position,
   returning 1 (okay) if the request is to jog off the limit, 0 (bad)
   if the request is to jog further past a limit. Software limits are
   ignored if the axis hasn't been homed */
static int checkJog(int axis, double vel)
{
    if (emcmotStatus->overrideLimits) {
	return 1;		/* okay to jog when limits overridden */
    }

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	reportError("Can't jog out of range axis %d.", axis);
	return 0;		/* can't jog out-of-range axis */
    }

    if (vel > 0.0 && GET_AXIS_PSL_FLAG(axis)) {
	reportError("Can't jog axis %d further past max soft limit.", axis);
	return 0;
    }

    if (vel > 0.0 && GET_AXIS_PHL_FLAG(axis)) {
	reportError("Can't jog axis %d further past max hard limit.", axis);
	return 0;
    }

    if (vel < 0.0 && GET_AXIS_NSL_FLAG(axis)) {
	reportError("Can't jog axis %d further past min soft limit.", axis);
	return 0;
    }

    if (vel < 0.0 && GET_AXIS_NHL_FLAG(axis)) {
	reportError("Can't jog axis %d further past min hard limit.", axis);
	return 0;
    }

    /* okay to jog */
    return 1;
}

/* inRange() returns non-zero if the position lies within the axis
   limits, or 0 if not */
static int inRange(EmcPose pos)
{
    double joint[EMCMOT_MAX_AXIS];
    int axis;

    /* fill in all joints with 0 */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	joint[axis] = 0.0;
    }

    /* now fill in with real values, for joints that are used */
    kinematicsInverse(&pos, joint, &iflags, &fflags);

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (!GET_AXIS_ACTIVE_FLAG(axis)) {
	    /* if axis is not active, don't even look at its limits */
	    continue;
	}

	if (joint[axis] > emcmotConfig->maxLimit[axis] ||
	    joint[axis] < emcmotConfig->minLimit[axis]) {
	    return 0;		/* can't move further past limit */
	}
    }

    /* okay to move */
    return 1;
}

/* clearHomes() will clear the homed flags for axes that have moved
   since homing, outside coordinated control, for machines with no
   forward kinematics. This is used in conjunction with the rehomeAll
   flag, which is set for any coordinated move that in general will
   result in all joints moving. The flag is consulted whenever a joint
   is jogged in joint mode, so that either its flag can be cleared if
   no other joints have moved, or all have to be cleared. */
static void clearHomes(int axis)
{
    int t;

    if (kinType == KINEMATICS_INVERSE_ONLY) {
	if (rehomeAll) {
	    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
		SET_AXIS_HOMED_FLAG(t, 0);
	    }
	} else {
	    SET_AXIS_HOMED_FLAG(axis, 0);
	}
    }
    if (0 != emcmotDebug) {
	emcmotDebug->allHomed = 0;
    }
}

/*
  emcmotCommandHandler() is called each main cycle to read the
  shared memory buffer
  */
int emcmotCommandHandler(void)
{
    int axis;
    int valid;
    /* check for split read */
    if (emcmotCommand->head != emcmotCommand->tail) {
	emcmotDebug->split++;
	return 0;		/* not really an error */
    }

    if (emcmotCommand->commandNum != emcmotStatus->commandNumEcho) {
	/* increment head count-- we'll be modifying emcmotStatus */
	emcmotStatus->head++;
	emcmotDebug->head++;

	/* got a new command-- echo command and number... */
	emcmotStatus->commandEcho = emcmotCommand->command;
	emcmotStatus->commandNumEcho = emcmotCommand->commandNum;

	/* clear status value by default */
	emcmotStatus->commandStatus = EMCMOT_COMMAND_OK;

	/* log it, if appropriate */
	if (emcmotStatus->logStarted &&
	    emcmotStatus->logType == EMCMOT_LOG_TYPE_CMD) {
	    ls.item.cmd.time = etime();	/* don't subtract off logStartTime,
					   since we want an absolute time
					   value */
	    ls.item.cmd.command = emcmotCommand->command;
	    ls.item.cmd.commandNum = emcmotCommand->commandNum;
	    emcmotLogAdd(emcmotLog, ls);
	    emcmotStatus->logPoints = emcmotLog->howmany;
	}

	/* ...and process command */
	switch (emcmotCommand->command) {
	case EMCMOT_FREE:
	    /* change the mode to free axis motion */
	    /* can be done at any time */
	    /* reset the emcmotDebug->coordinating flag to defer transition
	       to controller cycle */
	    emcmotDebug->coordinating = 0;
	    emcmotDebug->teleoperating = 0;
	    break;

	case EMCMOT_COORD:
	    /* change the mode to coordinated axis motion */
	    /* can be done at any time */
	    /* set the emcmotDebug->coordinating flag to defer transition to
	       controller cycle */
	    emcmotDebug->coordinating = 1;
	    emcmotDebug->teleoperating = 0;
	    if (kinType != KINEMATICS_IDENTITY) {
		if (!emcmotDebug->allHomed) {
		    reportError
			("all axes must be homed before going into coordinated mode");
		    emcmotDebug->coordinating = 0;
		    break;
		}
	    }
	    break;

	case EMCMOT_TELEOP:
	    /* change the mode to teleop motion */
	    /* can be done at any time */
	    /* set the emcmotDebug->teleoperating flag to defer transition to
	       controller cycle */
	    emcmotDebug->teleoperating = 1;
	    if (kinType != KINEMATICS_IDENTITY) {
		if (!emcmotDebug->allHomed) {
		    reportError
			("all axes must be homed before going into teleop mode");
		    emcmotDebug->teleoperating = 0;
		    break;
		}

	    }
	    break;

	case EMCMOT_SET_NUM_AXES:
	    /* set the global NUM_AXES, which must be between 1 and
	       EMCMOT_MAX_AXIS, inclusive */
	    axis = emcmotCommand->axis;
	    /* note that this comparison differs from the check on the range
	       of 'axis' in most other places, since those checks are for a
	       value to be used as an index and here it's a value to be used
	       as a counting number. The indenting is different here so as
	       not to match macro editing on that other bunch. */
	    if (axis <= 0 || axis > EMCMOT_MAX_AXIS) {
		break;
	    }
	    NUM_AXES = axis;
	    emcmotConfig->numAxes = axis;
	    break;

	case EMCMOT_SET_WORLD_HOME:
	    worldHome = emcmotCommand->pos;
	    break;

	case EMCMOT_SET_JOINT_HOME:
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    /* FIXME-- use 'home' instead */
	    emcmotDebug->jointHome[axis] = emcmotCommand->offset;
	    break;

	case EMCMOT_SET_HOME_OFFSET:
	    emcmot_config_change();
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    emcmotConfig->homeOffset[axis] = emcmotCommand->offset;
	    break;

	case EMCMOT_OVERRIDE_LIMITS:
	    if (emcmotCommand->axis < 0) {
		/* don't override limits */
		emcmotStatus->overrideLimits = 0;
	    } else {
		emcmotStatus->overrideLimits = 1;
	    }
	    emcmotDebug->overriding = 0;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		SET_AXIS_ERROR_FLAG(axis, 0);
	    }
	    break;

	case EMCMOT_SET_TRAJ_CYCLE_TIME:
	    /* set the cycle time for trajectory calculations */
	    /* really should be done only at startup before controller is
	       run, but at least it requires no active motions and the
	       interpolators need to be cleared */
	    setTrajCycleTime(emcmotCommand->cycleTime);
	    break;

	case EMCMOT_SET_SERVO_CYCLE_TIME:
	    /* set the cycle time for servo calculations, which is the period 
	       for emcmotController execution */
	    /* really should be done only at startup before controller is
	       run, but at least it requires no active motions and the
	       interpolators need to be cleared */
	    /* FIXME-- add re-timing a task to RTAPI */
	    rtapi_print("emcmot: servo cycle time set to %d nsecs\n",
		(int) (emcmotCommand->cycleTime * 1.0e9));
	    break;

	case EMCMOT_SET_POSITION_LIMITS:
	    emcmot_config_change();
	    /* set the position limits for the axis */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    emcmotConfig->minLimit[axis] = emcmotCommand->minLimit;
	    emcmotConfig->maxLimit[axis] = emcmotCommand->maxLimit;
	    break;

	case EMCMOT_SET_OUTPUT_LIMITS:
	    emcmot_config_change();
	    /* set the output limits for the axis */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    emcmotConfig->minOutput[axis] = emcmotCommand->minLimit;
	    emcmotConfig->maxOutput[axis] = emcmotCommand->maxLimit;
	    break;

	case EMCMOT_SET_OUTPUT_SCALE:
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->scale == 0) {
		break;
	    }
	    emcmotStatus->outputScale[axis] = emcmotCommand->scale;
	    emcmotStatus->outputOffset[axis] = emcmotCommand->offset;
	    emcmotDebug->inverseOutputScale[axis] =
		1.0 / emcmotStatus->outputScale[axis];
	    break;

	case EMCMOT_SET_INPUT_SCALE:
	    /* 
	       change the scale factor for the position input, e.g., encoder
	       counts per unit. Note that this is not a good idea once things 
	       have gotten underway, since the axis will jump servo to the
	       "new" position, the gains will no longer be appropriate, etc. */
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->scale == 0.0) {
		break;
	    }
	    emcmotDebug->oldInputValid[axis] = 0;

	    /* now make them active */
	    emcmotStatus->inputScale[axis] = emcmotCommand->scale;
	    emcmotStatus->inputOffset[axis] = emcmotCommand->offset;
	    emcmotDebug->inverseInputScale[axis] =
		1.0 / emcmotStatus->inputScale[axis];
	    break;

	    /* 
	       Max and min ferror work like this: limiting ferror is
	       determined by slope of ferror line, = maxFerror/limitVel ->
	       limiting ferror = maxFerror/limitVel * vel. If ferror <
	       minFerror then OK else if ferror < limiting ferror then OK
	       else ERROR */

	case EMCMOT_SET_MAX_FERROR:
	    emcmot_config_change();
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->maxFerror < 0.0) {
		break;
	    }
	    emcmotConfig->maxFerror[axis] = emcmotCommand->maxFerror;
	    break;

	case EMCMOT_SET_MIN_FERROR:
	    emcmot_config_change();
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->minFerror < 0.0) {
		break;
	    }
	    emcmotConfig->minFerror[axis] = emcmotCommand->minFerror;
	    break;

	case EMCMOT_JOG_CONT:
	    /* do a continuous jog, implemented as an incremental jog to the
	       software limit, or the full range of travel if software limits 
	       don't yet apply because we're not homed */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    /* requires no motion, in free mode, enable on */
	    if (GET_MOTION_COORD_FLAG()) {
		reportError("Can't jog axis in coordinated mode.");
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (!GET_MOTION_INPOS_FLAG()) {
		reportError("Can't jog axis when not in position.");
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError("Can't jog axis when not enabled.");
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(axis, emcmotCommand->vel)) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (emcmotCommand->vel > 0.0) {
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->maxLimit[axis];
		} else {
		    emcmotDebug->freePose.tran.x =
			emcmotDebug->jointPos[axis] + AXRANGE(axis);
		}
	    } else {
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->minLimit[axis];
		} else {
		    emcmotDebug->freePose.tran.x =
			emcmotDebug->jointPos[axis] - AXRANGE(axis);
		}
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis], fabs(emcmotCommand->vel));
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    SET_AXIS_ERROR_FLAG(axis, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(axis);
	    break;

	case EMCMOT_JOG_INCR:
	    /* do an incremental jog */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    /* requires no motion, in free mode, enable on */
	    if (GET_MOTION_COORD_FLAG() ||
		!GET_MOTION_INPOS_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(axis, emcmotCommand->vel)) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (emcmotCommand->vel > 0.0) {
		emcmotDebug->freePose.tran.x = emcmotDebug->jointPos[axis] + emcmotCommand->offset;	/* FIXME-- 
													   use 
													   'goal' 
													   instead 
													 */
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    if (emcmotDebug->freePose.tran.x >
			emcmotConfig->maxLimit[axis]) {
			emcmotDebug->freePose.tran.x =
			    emcmotConfig->maxLimit[axis];
		    }
		}
	    } else {
		emcmotDebug->freePose.tran.x = emcmotDebug->jointPos[axis] - emcmotCommand->offset;	/* FIXME-- 
													   use 
													   'goal' 
													   instead 
													 */
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    if (emcmotDebug->freePose.tran.x <
			emcmotConfig->minLimit[axis]) {
			emcmotDebug->freePose.tran.x =
			    emcmotConfig->minLimit[axis];
		    }
		}
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis], fabs(emcmotCommand->vel));
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    SET_AXIS_ERROR_FLAG(axis, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(axis);

	    break;

	case EMCMOT_JOG_ABS:
	    /* do an absolute jog */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    /* requires no motion, in free mode, enable on */
	    if (GET_MOTION_COORD_FLAG() ||
		!GET_MOTION_INPOS_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(axis, emcmotCommand->vel)) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }
            	/* FIXME-- 
									   use 
									   'goal' 
									   instead 
									 */
	    emcmotDebug->freePose.tran.x = emcmotCommand->offset;
	    if (GET_AXIS_HOMED_FLAG(axis)) {
		if (emcmotDebug->freePose.tran.x >
		    emcmotConfig->maxLimit[axis]) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->maxLimit[axis];
		} else if (emcmotDebug->freePose.tran.x <
		    emcmotConfig->minLimit[axis]) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->minLimit[axis];
		}
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis], fabs(emcmotCommand->vel));
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    SET_AXIS_ERROR_FLAG(axis, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(axis);

	    break;

	case EMCMOT_SET_TERM_COND:
	    /* sets termination condition for motion emcmotDebug->queue */
	    tpSetTermCond(&emcmotDebug->queue, emcmotCommand->termCond);
	    break;

	case EMCMOT_SET_LINE:
	    /* emcmotDebug->queue up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for linear move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos)) {
		reportError("linear move %d out of range", emcmotCommand->id);
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!checkLimits()) {
		reportError("can't do linear move with limits exceeded");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

	    /* append it to the emcmotDebug->queue */
	    tpSetId(&emcmotDebug->queue, emcmotCommand->id);
	    if (-1 == tpAddLine(&emcmotDebug->queue, emcmotCommand->pos)) {
		reportError("can't add linear move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_CIRCLE:
	    /* emcmotDebug->queue up a circular move */
	    /* requires coordinated mode, enable on, not on limits */
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for circular move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos)) {
		reportError("circular move %d out of range",
		    emcmotCommand->id);
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!checkLimits()) {
		reportError("can't do circular move with limits exceeded");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

	    /* append it to the emcmotDebug->queue */
	    tpSetId(&emcmotDebug->queue, emcmotCommand->id);
	    if (-1 ==
		tpAddCircle(&emcmotDebug->queue, emcmotCommand->pos,
		    emcmotCommand->center, emcmotCommand->normal,
		    emcmotCommand->turn)) {
		reportError("can't add circular move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_VEL:
	    /* set the velocity for subsequent moves */
	    /* can do it at any time */
	    emcmotStatus->vel = emcmotCommand->vel;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpSetVmax(&emcmotDebug->freeAxis[axis], emcmotStatus->vel);
	    }
	    tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel);
	    break;

	case EMCMOT_SET_VEL_LIMIT:
	    emcmot_config_change();
	    /* set the absolute max velocity for all subsequent moves */
	    /* can do it at any time */
	    emcmotConfig->limitVel = emcmotCommand->vel;
	    tpSetVlimit(&emcmotDebug->queue, emcmotConfig->limitVel);
	    break;

	case EMCMOT_SET_AXIS_VEL_LIMIT:
	    emcmot_config_change();
	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    tpSetVlimit(&emcmotDebug->freeAxis[axis], emcmotCommand->vel);
	    emcmotConfig->axisLimitVel[axis] = emcmotCommand->vel;
	    emcmotDebug->bigVel[axis] = 10 * emcmotCommand->vel;
	    break;

	case EMCMOT_SET_HOMING_VEL:
	    emcmot_config_change();
	    /* set the homing velocity */
	    /* can do it at any time */
	    /* sign of vel should set polarity, and mag-sign are recorded */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    if (emcmotCommand->vel < 0.0) {
		emcmotConfig->homingVel[axis] = -emcmotCommand->vel;
		SET_AXIS_HOMING_POLARITY(axis, 0);
	    } else {
		emcmotConfig->homingVel[axis] = emcmotCommand->vel;
		SET_AXIS_HOMING_POLARITY(axis, 1);
	    }
	    break;

	case EMCMOT_SET_ACC:
	    /* set the max acceleration */
	    /* can do it at any time */
	    emcmotStatus->acc = emcmotCommand->acc;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpSetAmax(&emcmotDebug->freeAxis[axis], emcmotStatus->acc);
	    }
	    tpSetAmax(&emcmotDebug->queue, emcmotStatus->acc);
	    break;

	case EMCMOT_PAUSE:
	    /* pause the motion */
	    /* can happen at any time */
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpPause(&emcmotDebug->freeAxis[axis]);
	    }
	    tpPause(&emcmotDebug->queue);
	    emcmotStatus->paused = 1;
	    break;

	case EMCMOT_RESUME:
	    /* resume paused motion */
	    /* can happen at any time */
	    emcmotDebug->stepping = 0;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpResume(&emcmotDebug->freeAxis[axis]);
	    }
	    tpResume(&emcmotDebug->queue);
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_STEP:
	    /* resume paused motion until id changes */
	    /* can happen at any time */
	    emcmotDebug->idForStep = emcmotStatus->id;
	    emcmotDebug->stepping = 1;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpResume(&emcmotDebug->freeAxis[axis]);
	    }
	    tpResume(&emcmotDebug->queue);
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_SCALE:
	    /* override speed */
	    /* can happen at any time */
	    if (emcmotCommand->scale < 0.0) {
		emcmotCommand->scale = 0.0;	/* clamp it */
	    }
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpSetVscale(&emcmotDebug->freeAxis[axis],
		    emcmotCommand->scale);
		emcmotStatus->axVscale[axis] = emcmotCommand->scale;
	    }
	    tpSetVscale(&emcmotDebug->queue, emcmotCommand->scale);
	    emcmotStatus->qVscale = emcmotCommand->scale;
	    break;

	case EMCMOT_ABORT:
	    /* abort motion */
	    /* can happen at any time */
	    /* check for coord or free space motion active */
	    if (GET_MOTION_TELEOP_FLAG()) {
		emcmotDebug->teleop_data.desiredVel.tran.x = 0.0;
		emcmotDebug->teleop_data.desiredVel.tran.y = 0.0;
		emcmotDebug->teleop_data.desiredVel.tran.z = 0.0;
		emcmotDebug->teleop_data.desiredVel.a = 0.0;
		emcmotDebug->teleop_data.desiredVel.b = 0.0;
		emcmotDebug->teleop_data.desiredVel.c = 0.0;
	    } else if (GET_MOTION_COORD_FLAG()) {
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(0);
	    } else {
		/* check axis range */
		axis = emcmotCommand->axis;
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		    break;
		}
		tpAbort(&emcmotDebug->freeAxis[axis]);
		SET_AXIS_HOMING_FLAG(axis, 0);
		SET_AXIS_ERROR_FLAG(axis, 0);
	    }
	    break;

	case EMCMOT_DISABLE:
	    /* go into disable */
	    /* can happen at any time */
	    /* reset the emcmotDebug->enabling flag to defer disable until
	       controller cycle (it *will* be honored) */
	    emcmotDebug->enabling = 0;
	    if (kinType == KINEMATICS_INVERSE_ONLY) {
		emcmotDebug->teleoperating = 0;
		emcmotDebug->coordinating = 0;
	    }
	    break;

	case EMCMOT_ENABLE:
	    /* come out of disable */
	    /* can happen at any time */
	    /* set the emcmotDebug->enabling flag to defer enable until
	       controller cycle */
	    emcmotDebug->enabling = 1;
	    if (kinType == KINEMATICS_INVERSE_ONLY) {
		emcmotDebug->teleoperating = 0;
		emcmotDebug->coordinating = 0;
	    }
	    break;

	case EMCMOT_SET_PID:
	    emcmot_config_change();
	    /* configure the PID gains */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    pidSetGains(&emcmotConfig->pid[axis], emcmotCommand->pid);
	    break;

	case EMCMOT_ACTIVATE_AXIS:
	    /* make axis active, so that amps will be enabled when system is
	       enabled or disabled */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    SET_AXIS_ACTIVE_FLAG(axis, 1);
	    break;

	case EMCMOT_DEACTIVATE_AXIS:
	    /* make axis inactive, so that amps won't be affected when system 
	       is enabled or disabled */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    SET_AXIS_ACTIVE_FLAG(axis, 0);
	    break;

	case EMCMOT_ENABLE_AMPLIFIER:
	    /* enable the amplifier directly, but don't enable calculations */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    extAmpEnable(axis, GET_AXIS_ENABLE_POLARITY(axis));
	    break;

	case EMCMOT_DISABLE_AMPLIFIER:
	    /* disable the axis calculations and amplifier, but don't disable 
	       calculations */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
	    break;

	case EMCMOT_OPEN_LOG:
	    /* open a data log */
	    axis = emcmotCommand->axis;
	    valid = 0;
	    if (emcmotCommand->logSize > 0 &&
		emcmotCommand->logSize <= EMCMOT_LOG_MAX) {
		/* handle log-specific data */
		switch (emcmotCommand->logType) {
		case EMCMOT_LOG_TYPE_AXIS_POS:
		case EMCMOT_LOG_TYPE_AXIS_VEL:
		case EMCMOT_LOG_TYPE_POS_VOLTAGE:
		    if (axis >= 0 && axis < EMCMOT_MAX_AXIS) {
			valid = 1;
		    }
		    break;

		default:
		    valid = 1;
		    break;
		}
	    }

	    if (valid) {
		/* success */
		loggingAxis = axis;
		emcmotLogInit(emcmotLog,
		    emcmotCommand->logType, emcmotCommand->logSize);
		emcmotStatus->logOpen = 1;
		emcmotStatus->logStarted = 0;
		emcmotStatus->logSize = emcmotCommand->logSize;
		emcmotStatus->logSkip = emcmotCommand->logSkip;
		emcmotStatus->logType = emcmotCommand->logType;
		emcmotStatus->logTriggerType = emcmotCommand->logTriggerType;
		emcmotStatus->logTriggerVariable =
		    emcmotCommand->logTriggerVariable;
		emcmotStatus->logTriggerThreshold =
		    emcmotCommand->logTriggerThreshold;
		if (axis >= 0 && axis < EMCMOT_MAX_AXIS
		    && emcmotStatus->logTriggerType == EMCLOG_DELTA_TRIGGER) {
		    switch (emcmotStatus->logTriggerVariable) {
		    case EMCLOG_TRIGGER_ON_FERROR:
			emcmotStatus->logStartVal =
			    emcmotDebug->ferrorCurrent[loggingAxis];
			break;

		    case EMCLOG_TRIGGER_ON_VOLT:
			emcmotStatus->logStartVal =
			    emcmotDebug->rawOutput[loggingAxis];
			break;
		    case EMCLOG_TRIGGER_ON_POS:
			emcmotStatus->logStartVal =
			    emcmotDebug->jointPos[loggingAxis];
			break;
		    case EMCLOG_TRIGGER_ON_VEL:
			emcmotStatus->logStartVal =
			    emcmotDebug->jointPos[loggingAxis] -
			    emcmotDebug->oldJointPos[loggingAxis];
			break;

		    default:
			break;
		    }
		}
	    }
	    break;

	case EMCMOT_START_LOG:
	    /* start logging */
	    /* first ignore triggered log types */
	    if (emcmotStatus->logType == EMCMOT_LOG_TYPE_POS_VOLTAGE) {
		break;
	    }
	    /* set the global baseTime, to be subtracted off log times,
	       otherwise time values are too large for the small increments
	       to appear */
	    if (emcmotStatus->logOpen &&
		emcmotStatus->logTriggerType == EMCLOG_MANUAL_TRIGGER) {
		logStartTime = etime();
		emcmotStatus->logStarted = 1;
		logSkip = 0;
	    }
	    break;

	case EMCMOT_STOP_LOG:
	    /* stop logging */
	    emcmotStatus->logStarted = 0;
	    break;

	case EMCMOT_CLOSE_LOG:
	    emcmotStatus->logOpen = 0;
	    emcmotStatus->logStarted = 0;
	    emcmotStatus->logSize = 0;
	    emcmotStatus->logSkip = 0;
	    emcmotStatus->logType = 0;
	    break;

	case EMCMOT_DAC_OUT:
	    /* write output to dacs directly */
	    /* will only persist if amplifiers are disabled */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    /* trigger log, if active */
	    if (emcmotStatus->logType == EMCMOT_LOG_TYPE_POS_VOLTAGE &&
		loggingAxis == axis && emcmotStatus->logOpen != 0) {
		emcmotStatus->logStarted = 1;
		logSkip = 0;
	    }
	    emcmotStatus->output[axis] = emcmotCommand->dacOut;
	    break;

	case EMCMOT_HOME:
	    /* home the specified axis */
	    /* need to be in free mode, enable on */
	    /* homing is basically a slow incremental jog to full range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    if (GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		break;
	    }

	    if (GET_AXIS_HOMING_POLARITY(axis)) {
		emcmotDebug->freePose.tran.x = +2.0 * AXRANGE(axis);
	    } else {
		emcmotDebug->freePose.tran.x = -2.0 * AXRANGE(axis);
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis],
		emcmotConfig->homingVel[axis]);
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    emcmotDebug->homingPhase[axis] = 1;
	    SET_AXIS_HOMING_FLAG(axis, 1);
	    SET_AXIS_HOMED_FLAG(axis, 0);
	    break;

	case EMCMOT_ENABLE_WATCHDOG:
	    emcmotDebug->wdEnabling = 1;
	    emcmotDebug->wdWait = emcmotCommand->wdWait;
	    if (emcmotDebug->wdWait < 0) {
		emcmotDebug->wdWait = 0;
	    }
	    break;

	case EMCMOT_DISABLE_WATCHDOG:
	    emcmotDebug->wdEnabling = 0;
	    break;

	case EMCMOT_SET_POLARITY:
	    emcmot_config_change();
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    if (emcmotCommand->level) {
		/* normal */
		emcmotConfig->axisPolarity[axis] |= emcmotCommand->axisFlag;
	    } else {
		/* inverted */
		emcmotConfig->axisPolarity[axis] &= ~emcmotCommand->axisFlag;
	    }
	    break;

	case EMCMOT_SET_PROBE_INDEX:
	    emcmot_config_change();
	    emcmotConfig->probeIndex = emcmotCommand->probeIndex;
	    break;

	case EMCMOT_SET_PROBE_POLARITY:
	    emcmot_config_change();
	    emcmotConfig->probePolarity = emcmotCommand->level;
	    break;

	case EMCMOT_CLEAR_PROBE_FLAGS:
	    emcmotStatus->probeTripped = 0;
	    emcmotStatus->probing = 1;
	    break;

	case EMCMOT_PROBE:
	    /* most of this is taken from EMCMOT_SET_LINE */
	    /* emcmotDebug->queue up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for probe move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos)) {
		reportError("probe move %d out of range", emcmotCommand->id);
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!checkLimits()) {
		reportError("can't do probe move with limits exceeded");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

	    /* append it to the emcmotDebug->queue */
	    tpSetId(&emcmotDebug->queue, emcmotCommand->id);
	    if (-1 == tpAddLine(&emcmotDebug->queue, emcmotCommand->pos)) {
		reportError("can't add probe move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		emcmotStatus->probeTripped = 0;
		emcmotStatus->probing = 1;
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_TELEOP_VECTOR:
	    if (!GET_MOTION_TELEOP_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in teleop mode for teleop move");
	    } else {
		double velmag;
		emcmotDebug->teleop_data.desiredVel = emcmotCommand->pos;
		pmCartMag(emcmotDebug->teleop_data.desiredVel.tran, &velmag);
		if (emcmotDebug->teleop_data.desiredVel.a > velmag) {
		    velmag = emcmotDebug->teleop_data.desiredVel.a;
		}
		if (emcmotDebug->teleop_data.desiredVel.b > velmag) {
		    velmag = emcmotDebug->teleop_data.desiredVel.b;
		}
		if (emcmotDebug->teleop_data.desiredVel.c > velmag) {
		    velmag = emcmotDebug->teleop_data.desiredVel.c;
		}
		if (velmag > emcmotConfig->limitVel) {
		    pmCartScalMult(emcmotDebug->teleop_data.desiredVel.tran,
			emcmotConfig->limitVel / velmag,
			&emcmotDebug->teleop_data.desiredVel.tran);
		    emcmotDebug->teleop_data.desiredVel.a *=
			emcmotConfig->limitVel / velmag;
		    emcmotDebug->teleop_data.desiredVel.b *=
			emcmotConfig->limitVel / velmag;
		    emcmotDebug->teleop_data.desiredVel.c *=
			emcmotConfig->limitVel / velmag;
		}
		/* flag that all joints need to be homed, if any joint is
		   jogged individually later */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_DEBUG:
	    emcmotConfig->debug = emcmotCommand->debug;
	    emcmot_config_change();
	    break;

	case EMCMOT_SET_AOUT:
	    tpSetAout(&emcmotDebug->queue, emcmotCommand->index,
		emcmotCommand->minLimit, emcmotCommand->maxLimit);
	    break;

	case EMCMOT_SET_DOUT:
	    tpSetDout(&emcmotDebug->queue, emcmotCommand->index,
		emcmotCommand->start, emcmotCommand->end);
	    break;

	case EMCMOT_SET_INDEX_BIT:
	    if (emcmotCommand->level) {
		/* Set bit */
		extDioWrite(emcmotCommand->index, 1);
	    } else {
		/* Clear bit */
		extDioWrite(emcmotCommand->index, 0);
	    }
	    break;

	case EMCMOT_READ_INDEX_BIT:
	    extDioRead(emcmotCommand->index, &(emcmotStatus->level));
	    break;

	case EMCMOT_SET_STEP_PARAMS:
	    emcmot_config_change();
	    /* configure the step pulse times */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    emcmotConfig->setup_time[axis] = emcmotCommand->setup_time;
	    emcmotConfig->hold_time[axis] = emcmotCommand->hold_time;
	    if (emcmotConfig->setup_time[axis] < 1.0) {
		emcmotConfig->setup_time[axis] = 1.0;
	    }

	    if (emcmotConfig->hold_time[axis] < 1.0) {
		emcmotConfig->hold_time[axis] = 1.0;
	    }
	    break;

	default:
	    reportError("unrecognized command %d", emcmotCommand->command);
	    emcmotStatus->commandStatus = EMCMOT_COMMAND_UNKNOWN_COMMAND;
	    break;

	}			/* end of: command switch */

	/* synch tail count */
	emcmotStatus->tail = emcmotStatus->head;
	emcmotConfig->tail = emcmotConfig->head;
	emcmotDebug->tail = emcmotDebug->head;

    }
    /* end of: if-new-command */
    return 0;
}
