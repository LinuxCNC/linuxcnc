/********************************************************************
*
* Description: emccommand.c
*
*	Just the emcmotCommandHandler routine split out from emcmot.c
*	The calls to handle configuration should (will ?) be handled
*	by ioctl calls - This will allow the RT task creation to be
*	deferred till run time rather than at load time. Should avoid
*	system locks when X starts/stops...
*
*	Principles of communication:
*
*	Data is copied in or out from the various types of comm mechanisms:
*	mbuff mapped memory for Linux/RT-Linux, or OS shared memory for Unixes.
*
*	emcmotStruct is ptr to this memory.
*	emcmotCommand points to emcmotStruct->command,
*	emcmotStatus points to emcmotStruct->status,
*	emcmotError points to emcmotStruct->error, and
*	emcmotLog points to emcmotStruct->log.
*	emcmotComp[] points to emcmotStruct->comp[].
*
*    
* Author:
* License:
* System: Linux
*    
* Last change: 
* $Revision$
* $Author$
* $Date$
*
* Copyright (c) 2004  All rights reserved.
*
********************************************************************/


/* FIXME-- testing output rounding to input resolution */
#define NO_ROUNDING

/* FIXME-- testing axis comping */
#define COMPING

#include <linux/types.h>
#include <float.h>
#include <math.h>


#include "rtai.h"
#include "rtai_sched.h"
#include "rtai_shm.h"

#if 0
/* undef'ing __attribute_used__ and redefining it later is a damned ugly way of
    avoiding a cmpile time warning - There must be a better way when using
    system headers */
#ifdef __attribute_used__
#undef __attribute_used__
#endif
#ifdef __attribute_pure__
#undef __attribute_pure__
#endif


#if __GNUC_PREREQ (3,1)
# define __attribute_used__ __attribute__ ((__used__))
#endif
#if __GNUC_PREREQ (2,96)
# define __attribute_pure__ __attribute__ ((__pure__))
#endif
#endif

#include "posemath.h"		/* PmPose, pmCartMag() */
#include "emcpos.h"
#include "emcmotcfg.h"
#include "emcmotglb.h"
#include "emcmot.h"
#include "pid.h"
#include "cubic.h"
#include "tc.h"
#include "tp.h"
#include "extintf.h"
#include "mmxavg.h"
#include "emcmotlog.h"

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#include "diagnostics.h"

static char __attribute__ ((unused)) ident[] = "$Id$";

/* value for world home position */
EmcPose worldHome = { {0.0, 0.0, 0.0},
0.0, 0.0, 0.0
};


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
    return 0;			/* not really an error */
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
					   since we want an absolute time value */
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
      /* reset the emcmotDebug->coordinating flag to defer transition to
         controller cycle */
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
	  reportError("all axes must be homed before going into teleop mode");
	  emcmotDebug->teleoperating = 0;
	  break;
	}

      }
      break;

    case EMCMOT_SET_NUM_AXES:
      /* set the global NUM_AXES, which must be between 1 and
         EMCMOT_MAX_AXIS, inclusive */
      axis = emcmotCommand->axis;
      /* note that this comparison differs from the check on the
         range of 'axis' in most other places, since those checks
         are for a value to be used as an index and here it's a value
         to be used as a counting number. The indenting is different
         here so as not to match macro editing on that other bunch. */
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
      emcmotDebug->jointHome[axis] = emcmotCommand->offset;	/* FIXME-- use 'home' instead */
      break;

    case EMCMOT_SET_HOME_OFFSET:
      MARK_EMCMOT_CONFIG_CHANGE();
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
      /* really should be done only at startup before
         controller is run, but at least it requires
         no active motions and the interpolators need
         to be cleared */
      setTrajCycleTime(emcmotCommand->cycleTime);
      break;

    case EMCMOT_SET_SERVO_CYCLE_TIME:
      /* set the cycle time for servo calculations, which is the
         period for emcmotController execution */
      /* really should be done only at startup before
         controller is run, but at least it requires
         no active motions and the interpolators need
         to be cleared */

      iperiod = nano2count((int) (1e9 * emcmotCommand->cycleTime));

      if (rtaiTickPeriod > 0) {
	rtaiTickPeriod_long = (long) rtaiTickPeriod;
	if (iperiod < rtaiTickPeriod_long) {
	  iperiod = rtaiTickPeriod_long;
	}
	iperiod /= rtaiTickPeriod_long;
	iperiod *= rtaiTickPeriod_long;
      }
      iperiod_rtime = (RTIME) iperiod;
      now = rt_get_time();
      rt_task_make_periodic(&emcmotTask, now + iperiod_rtime, iperiod_rtime);
      setServoCycleTime(count2nano(iperiod) * 1e-9);
      break;

    case EMCMOT_SET_POSITION_LIMITS:
      MARK_EMCMOT_CONFIG_CHANGE();
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
      MARK_EMCMOT_CONFIG_CHANGE();
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
      if (axis < 0 || axis >= EMCMOT_MAX_AXIS || emcmotCommand->scale == 0) {
	break;
      }
      emcmotStatus->outputScale[axis] = emcmotCommand->scale;
      emcmotStatus->outputOffset[axis] = emcmotCommand->offset;
      emcmotDebug->inverseOutputScale[axis] =
	1.0 / emcmotStatus->outputScale[axis];
      break;

    case EMCMOT_SET_INPUT_SCALE:
      /*
         change the scale factor for the position input, e.g.,
         encoder counts per unit. Note that this is not a good idea
         once things have gotten underway, since the axis will
         jump servo to the "new" position, the gains will no longer
         be appropriate, etc.
       */
      axis = emcmotCommand->axis;
      if (axis < 0 || axis >= EMCMOT_MAX_AXIS || emcmotCommand->scale == 0.0) {
	break;
      }
#if 0
      /* adjust last saved input value to match this one, so we
         don't get a spurious following error */
      emcmotDebug->oldInput[axis] = emcmotDebug->oldInput[axis] * emcmotStatus->inputScale[axis] + emcmotStatus->inputOffset[axis];	/* temp calc */
      emcmotDebug->oldInput[axis] =
	(emcmotDebug->oldInput[axis] -
	 emcmotCommand->offset) / emcmotCommand->scale;
#endif
      emcmotDebug->oldInputValid[axis] = 0;

      /* now make them active */
      emcmotStatus->inputScale[axis] = emcmotCommand->scale;
      emcmotStatus->inputOffset[axis] = emcmotCommand->offset;
      emcmotDebug->inverseInputScale[axis] =
	1.0 / emcmotStatus->inputScale[axis];
      break;

      /*
         Max and min ferror work like this:
         limiting ferror is determined by slope of ferror line,
         = maxFerror/limitVel
         -> limiting ferror = maxFerror/limitVel * vel.
         If ferror < minFerror then OK else
         if ferror < limiting ferror then OK else ERROR
       */

    case EMCMOT_SET_MAX_FERROR:
      MARK_EMCMOT_CONFIG_CHANGE();
      axis = emcmotCommand->axis;
      if (axis < 0 ||
	  axis >= EMCMOT_MAX_AXIS || emcmotCommand->maxFerror < 0.0) {
	break;
      }
      emcmotConfig->maxFerror[axis] = emcmotCommand->maxFerror;
      break;

    case EMCMOT_SET_MIN_FERROR:
      MARK_EMCMOT_CONFIG_CHANGE();
      axis = emcmotCommand->axis;
      if (axis < 0 ||
	  axis >= EMCMOT_MAX_AXIS || emcmotCommand->minFerror < 0.0) {
	break;
      }
      emcmotConfig->minFerror[axis] = emcmotCommand->minFerror;
      break;

    case EMCMOT_JOG_CONT:
      /* do a continuous jog, implemented as an incremental
         jog to the software limit, or the full range of travel
         if software limits don't yet apply because we're not homed */

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
	  emcmotDebug->freePose.tran.x = emcmotConfig->maxLimit[axis];
	} else {
	  emcmotDebug->freePose.tran.x =
	    emcmotDebug->jointPos[axis] + AXRANGE(axis);
	}
      } else {
	if (GET_AXIS_HOMED_FLAG(axis)) {
	  emcmotDebug->freePose.tran.x = emcmotConfig->minLimit[axis];
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
	emcmotDebug->freePose.tran.x = emcmotDebug->freeAxis[axis].goalPos.tran.x + emcmotCommand->offset;
	if (GET_AXIS_HOMED_FLAG(axis)) {
	  if (emcmotDebug->freePose.tran.x > emcmotConfig->maxLimit[axis]) {
	    emcmotDebug->freePose.tran.x = emcmotConfig->maxLimit[axis];
	  }
	}
      } else {
	emcmotDebug->freePose.tran.x = emcmotDebug->freeAxis[axis].goalPos.tran.x - emcmotCommand->offset;
	if (GET_AXIS_HOMED_FLAG(axis)) {
	  if (emcmotDebug->freePose.tran.x < emcmotConfig->minLimit[axis]) {
	    emcmotDebug->freePose.tran.x = emcmotConfig->minLimit[axis];
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

      emcmotDebug->freePose.tran.x = emcmotCommand->offset;	/* FIXME-- use 'goal' instead */
      if (GET_AXIS_HOMED_FLAG(axis)) {
	if (emcmotDebug->freePose.tran.x > emcmotConfig->maxLimit[axis]) {
	  emcmotDebug->freePose.tran.x = emcmotConfig->maxLimit[axis];
	} else if (emcmotDebug->freePose.tran.x <
		   emcmotConfig->minLimit[axis]) {
	  emcmotDebug->freePose.tran.x = emcmotConfig->minLimit[axis];
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
	reportError("need to be enabled, in coord mode for linear move");
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
	/* set flag that indicates all axes need rehoming, if any axis
	   is moved in joint mode, for machines with no forward kins */
	rehomeAll = 1;
      }
      break;

    case EMCMOT_SET_CIRCLE:
      /* emcmotDebug->queue up a circular move */
      /* requires coordinated mode, enable on, not on limits */
      if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
	reportError("need to be enabled, in coord mode for circular move");
	emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
	SET_MOTION_ERROR_FLAG(1);
	break;
      } else if (!inRange(emcmotCommand->pos)) {
	reportError("circular move %d out of range", emcmotCommand->id);
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
	/* set flag that indicates all axes need rehoming, if any axis
	   is moved in joint mode, for machines with no forward kins */
	rehomeAll = 1;
      }
      break;

    case EMCMOT_SET_VEL:
      /* set the velocity for subsequent moves */
      /* can do it at any time */
      emcmotStatus->vel = emcmotCommand->vel > emcmotConfig->limitVel ? emcmotConfig->limitVel : emcmotCommand->vel;
      tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel);
      for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	tpSetVmax(&emcmotDebug->freeAxis[axis], emcmotCommand->vel > emcmotConfig->axisLimitVel[axis] ? emcmotConfig->axisLimitVel[axis] : emcmotCommand->vel);
      }
      break;

    case EMCMOT_SET_VEL_LIMIT:
      MARK_EMCMOT_CONFIG_CHANGE();
      /* set the absolute max velocity for all subsequent moves */
      /* can do it at any time */
      emcmotConfig->limitVel = emcmotCommand->vel;
      tpSetVlimit(&emcmotDebug->queue, emcmotConfig->limitVel);
      break;

    case EMCMOT_SET_AXIS_VEL_LIMIT:
      MARK_EMCMOT_CONFIG_CHANGE();
      /* check axis range */
      axis = emcmotCommand->axis;
      if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	break;
      }
      tpSetVlimit(&emcmotDebug->freeAxis[axis], emcmotCommand->vel);
      emcmotConfig->axisLimitVel[axis] = emcmotCommand->vel;
      emcmotDebug->bigVel[axis] = 10 * emcmotCommand->vel;
      break;

    case EMCMOT_SET_AXIS_ACC_LIMIT:
      MARK_EMCMOT_CONFIG_CHANGE();
      /* check axis range */
      axis = emcmotCommand->axis;
      if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	break;
      }
      tpSetAmax(&emcmotDebug->freeAxis[axis], emcmotCommand->acc);
      emcmotConfig->axisAcc[axis] = emcmotCommand->acc;
      break;

    case EMCMOT_SET_HOMING_VEL:
      MARK_EMCMOT_CONFIG_CHANGE();
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
	tpSetVscale(&emcmotDebug->freeAxis[axis], emcmotCommand->scale);
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
      /* set the emcmotDebug->enabling flag to defer enable
         until controller cycle */
      emcmotDebug->enabling = 1;
      if (kinType == KINEMATICS_INVERSE_ONLY) {
	emcmotDebug->teleoperating = 0;
	emcmotDebug->coordinating = 0;
      }
      break;

    case EMCMOT_SET_PID:
      MARK_EMCMOT_CONFIG_CHANGE();
      /* configure the PID gains */
      axis = emcmotCommand->axis;
      if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	break;
      }
      pidSetGains(&emcmotConfig->pid[axis], emcmotCommand->pid);
      break;

    case EMCMOT_ACTIVATE_AXIS:
      /* make axis active, so that amps will be enabled when
         system is enabled or disabled */
      /* can be done at any time */
      axis = emcmotCommand->axis;
      if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	break;
      }
      SET_AXIS_ACTIVE_FLAG(axis, 1);
      break;

    case EMCMOT_DEACTIVATE_AXIS:
      /* make axis inactive, so that amps won't be affected when
         system is enabled or disabled */
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
      /* disable the axis calculations and amplifier, but don't
         disable calculations */
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
	emcmotStatus->logTriggerVariable = emcmotCommand->logTriggerVariable;
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
	    emcmotStatus->logStartVal = emcmotDebug->rawOutput[loggingAxis];
	    break;
	  case EMCLOG_TRIGGER_ON_POS:
	    emcmotStatus->logStartVal = emcmotDebug->jointPos[loggingAxis];
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
      /* set the global baseTime, to be subtracted off log times, otherwise
         time values are too large for the small increments to appear */
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
/* kasunich made this mess */

#undef NO_SWITCHES

#ifdef NO_SWITCHES
      emcmotDebug->homingPhase[axis] = 5;
#else
      if (GET_AXIS_HOMING_POLARITY(axis)) {
	emcmotDebug->freePose.tran.x = +2.0 * AXRANGE(axis);
      } else {
	emcmotDebug->freePose.tran.x = -2.0 * AXRANGE(axis);
      }

      tpSetVmax(&emcmotDebug->freeAxis[axis], emcmotConfig->homingVel[axis]);
      tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
      emcmotDebug->homingPhase[axis] = 1;
#endif
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
      MARK_EMCMOT_CONFIG_CHANGE();
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

#ifdef ENABLE_PROBING
    case EMCMOT_SET_PROBE_INDEX:
      MARK_EMCMOT_CONFIG_CHANGE();
      emcmotConfig->probeIndex = emcmotCommand->probeIndex;
      break;

    case EMCMOT_SET_PROBE_POLARITY:
      MARK_EMCMOT_CONFIG_CHANGE();
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
	reportError("need to be enabled, in coord mode for probe move");
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
	/* set flag that indicates all axes need rehoming, if any axis
	   is moved in joint mode, for machines with no forward kins */
	rehomeAll = 1;
      }
      break;
#endif /* ENABLE_PROBING */

    case EMCMOT_SET_TELEOP_VECTOR:
      if (!GET_MOTION_TELEOP_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
	reportError("need to be enabled, in teleop mode for teleop move");
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
	/* flag that all joints need to be homed, if any joint
	   is jogged individually later */
	rehomeAll = 1;
      }
      break;

    case EMCMOT_SET_DEBUG:
      emcmotConfig->debug = emcmotCommand->debug;
      MARK_EMCMOT_CONFIG_CHANGE();
      break;

    case EMCMOT_SET_AOUT:
      if (emcmotCommand->now) {
	extAioWrite(emcmotCommand->index, emcmotCommand->minLimit);
      } else {
	tpSetAout(&emcmotDebug->queue, emcmotCommand->index,
		  emcmotCommand->minLimit, emcmotCommand->maxLimit);
      }
      break;

    case EMCMOT_SET_DOUT:
      if (emcmotCommand->now) {
	extDioWrite(emcmotCommand->index, emcmotCommand->start);
      } else {
	tpSetDout(&emcmotDebug->queue, emcmotCommand->index,
		  emcmotCommand->start, emcmotCommand->end);
      }
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
      MARK_EMCMOT_CONFIG_CHANGE();
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

    }				/* end of: command switch */

    /* synch tail count */
    emcmotStatus->tail = emcmotStatus->head;
    emcmotConfig->tail = emcmotConfig->head;
    emcmotDebug->tail = emcmotDebug->head;

  }
  /* end of: if-new-command */
  return 0;
}
