/********************************************************************
* Description: control.c
*   emcmotController() is the main loop running at the servo cycle
*   rate. All state logic and trajectory calcs are called from here.
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

#include <linux/types.h>
#include <float.h>
#include <math.h>
#include "rtapi.h"
#include "hal.h"
#include "extintf.h"	/* WORKING ON MAKING THIS GO AWAY */
#include "emcmotglb.h"
#include "motion.h"
#include "mot_priv.h"

#define LIMIT_SWITCH_DEBOUNCE 10
#define AMP_FAULT_DEBOUNCE 100
#define POSITION_INPUT_DEBOUNCE 10

/* FIXME-- testing output rounding to input resolution */
#define NO_ROUNDING

/* FIXME-- testing axis comping */
#define COMPING

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/* debouncing */
/* FIXME-- testing */
static int debouncecount[EMCMOT_MAX_AXIS] = { 0 };

#ifndef SIMULATED_MOTORS
static double positionInputDebounce[EMCMOT_MAX_AXIS] = { 0.0 };
#endif

/* counter that triggers computation of forward kinematics during
   disabled state, making them run at the trajectory rate instead
   of the servo rate. In the disabled state the interpolators are
   not queried so we don't know when a trajectory cycle would occur,
   so we use this counter. It's loaded with emcmotConfig->interpolationRate
   whenever it goes to zero, during the code executed in the disabled
   state. */
static int interpolationCounter = 0;

/* flag to handle need to re-home when joints have moved outside purview
   of Cartesian control, for machines with no forward kinematics.
   0 = need to home only joint that was jogged, 1 = need to home all joints
   if any axis has been jogged. Initially 0, it's set to 1 for any coordinated
   move that will in general move all joints. It's set back to zero when
   all joints have been rehomed. */
int rehomeAll = 0;
static int logIt = 0;


/***********************************************************************
*                      LOCAL FUNCTION PROTOTYPES                       *
************************************************************************/

/* isHoming() returns non-zero if any axes are homing, 0 if none are */
static int isHoming(void);

/* axisComp(int axis, int dir, double nominal) looks up the real axis
   position value, given the nominal value and the direction of motion,
   based on the emcmotComp tables. It returns the comped value. If there's
   an error, the comped value is set to the nominal value. The alter value
   is added regardless of whether the comp table is active (total > 1),
   but if it is active it's added before the comping since the alter is
   a nominal delta.
*/
static double axisComp(int axis, int dir, double nominput);




/***********************************************************************
*                        PUBLIC FUNCTION CODE                          *
************************************************************************/

/*
  emcmotController() runs the trajectory and interpolation calculations
  each control cycle

  This function gets called at regular intervals - therefore it does NOT
  have a loop within it!

  Inactive axes are still calculated, but the PIDs are inhibited and
  the amp enable/disable are inhibited
  */
void emcmotController(void *arg, long period)
{
    int first = 1;		/* true the first time thru, for initing */
    double start, end, delta;	/* time stamping */
    int homeFlag;		/* result of ext read to home switch */
    int axis;			/* axis loop counter */
    int t;			/* loop counter if we're in axis loop */
    int isLimit;		/* result of ext read to limit sw */
    int whichCycle;		/* 0=none, 1=servo, 2=traj */
    int fault;
#ifndef NO_ROUNDING
    double numres;
#endif
    double thisFerror[EMCMOT_MAX_AXIS] = { 0.0 };
    double limitFerror;		/* current scaled ferror */
    double magFerror;
    double oldbcomp;
    int retval;
    /* end of backlash stuff */

#ifdef COMPING
    int dir[EMCMOT_MAX_AXIS] = { 1 };	/* flag for direction, used for axis
					   comp */
#endif /* COMPING */

    /* record start time */
    start = etime();

    /* increment head count */
    emcmotStatus->head++;

    /* READ INPUTS: */

    /* latch all encoder feedback into raw input array, done outside of
       for-loop on joints below, since it's a single call for all joints */
    extEncoderReadAll(EMCMOT_MAX_AXIS, emcmotDebug->rawInput);

    /* process input and read limit switches */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* save old cycle's values */
	if (!first && emcmotDebug->oldInputValid[axis]) {
	    emcmotDebug->oldInput[axis] = emcmotStatus->input[axis];
	}
	/* set input, scaled according to input = (raw - offset) / scale */
	emcmotStatus->input[axis] =
	    (emcmotDebug->rawInput[axis] -
	    emcmotStatus->inputOffset[axis]) *
	    emcmotDebug->inverseInputScale[axis] -
	    emcmotDebug->bcompincr[axis];

#ifdef COMPING
	/* adjust feedback using compensation tables */
	if (GET_AXIS_HOMED_FLAG(axis)) {
	    emcmotStatus->input[axis] =
		axisComp(axis, dir[axis], emcmotStatus->input[axis]);
	}
#endif /* COMPING */
	if (first || !emcmotDebug->oldInputValid[axis]) {
	    emcmotDebug->oldInput[axis] = emcmotStatus->input[axis];
	    emcmotDebug->oldInputValid[axis] = 1;
	    first = 0;
	}
#if 0				/* FIX ME - Move in to the HAL layer and make
				   sure it actually works */
	/* debounce bad feedback */
#ifndef SIMULATED_MOTORS
	if (fabs(emcmotStatus->input[axis] -
		emcmotDebug->oldInput[axis]) / emcmotConfig->servoCycleTime >
	    /* IF we are at the point where the encoder needs debouncing, the
	       max velocity of the axis has been exceeded by a major margin !
	       bigVel = 10 * emcmotConfig->axisLimitVel[axis] .... Well
	       beyond a reasonable speed ! */
	    emcmotDebug->bigVel[axis]) {
	    /* bad input value-- interpolate last value, up to max debounces,
	       then hold it */
	    if (++positionInputDebounce[axis] > POSITION_INPUT_DEBOUNCE) {
		/* we haven't exceeded max number of debounces allowed, so
		   interpolate off the velocity estimate */
		emcmotStatus->input[axis] = emcmotDebug->oldInput[axis] +
		    (emcmotDebug->jointVel[axis] *
		    emcmotConfig->servoCycleTime);
	    } else {
		/* we've exceeded the max number of debounces allowed, so
		   hold position. We should flag an error here, abort the
		   move, disable motion, etc. but for now we'll rely on
		   following error to do this */
		emcmotStatus->input[axis] = emcmotDebug->oldInput[axis];
	    }
	    /* FIXME-- testing */
	    debouncecount[axis]++;
	} else {
	    /* no debounce needed, so reset debounce counter */
	    positionInputDebounce[axis] = 0;
	}
#endif /* SIMULATED MOTORS */
#endif /* if 0 */

	/* read limit switches and amp fault from external interface, and set
	   'emcmotDebug->enabling' to zero if tripped to cause immediate stop
	 */

	/* handle limits */
	if (GET_AXIS_ACTIVE_FLAG(axis)) {
	    extMaxLimitSwitchRead(axis, &isLimit);
	    if (isLimit == GET_AXIS_PHL_POLARITY(axis)) {
		if (++emcmotDebug->maxLimitSwitchCount[axis] >
		    LIMIT_SWITCH_DEBOUNCE) {
		    SET_AXIS_PHL_FLAG(axis, 1);
		    emcmotDebug->maxLimitSwitchCount[axis] =
			LIMIT_SWITCH_DEBOUNCE;
		    if (emcmotStatus->overrideLimits || isHoming()) {
		    } else {
			SET_AXIS_ERROR_FLAG(axis, 1);
			emcmotDebug->enabling = 0;
		    }
		}
	    } else {
		SET_AXIS_PHL_FLAG(axis, 0);
		emcmotDebug->maxLimitSwitchCount[axis] = 0;
	    }
	}
	if (GET_AXIS_ACTIVE_FLAG(axis)) {
	    extMinLimitSwitchRead(axis, &isLimit);
	    if (isLimit == GET_AXIS_NHL_POLARITY(axis)) {
		if (++emcmotDebug->minLimitSwitchCount[axis] >
		    LIMIT_SWITCH_DEBOUNCE) {
		    SET_AXIS_NHL_FLAG(axis, 1);
		    emcmotDebug->minLimitSwitchCount[axis] =
			LIMIT_SWITCH_DEBOUNCE;
		    if (emcmotStatus->overrideLimits || isHoming()) {
		    } else {
			SET_AXIS_ERROR_FLAG(axis, 1);
			emcmotDebug->enabling = 0;
		    }
		}
	    } else {
		SET_AXIS_NHL_FLAG(axis, 0);
		emcmotDebug->minLimitSwitchCount[axis] = 0;
	    }
	}

	if (GET_AXIS_ACTIVE_FLAG(axis) && GET_AXIS_ENABLE_FLAG(axis)) {
	    extAmpFault(axis, &fault);
	    if (fault == GET_AXIS_FAULT_POLARITY(axis)) {
		if (++emcmotDebug->ampFaultCount[axis] > AMP_FAULT_DEBOUNCE) {
		    SET_AXIS_ERROR_FLAG(axis, 1);
		    SET_AXIS_FAULT_FLAG(axis, 1);
		    emcmotDebug->ampFaultCount[axis] = AMP_FAULT_DEBOUNCE;
		    emcmotDebug->enabling = 0;
		}
	    } else {
		SET_AXIS_FAULT_FLAG(axis, 0);
		emcmotDebug->ampFaultCount[axis] = 0;
	    }
	}

	/* read home switch and set flag if tripped. Handling of home
	   sequence is done later. */
	if (GET_AXIS_ACTIVE_FLAG(axis)) {
	    extHomeSwitchRead(axis, &homeFlag);
	    if (homeFlag == GET_AXIS_HOME_SWITCH_POLARITY(axis)) {
		SET_AXIS_HOME_SWITCH_FLAG(axis, 1);
	    } else {
		SET_AXIS_HOME_SWITCH_FLAG(axis, 0);
	    }
	}

    }				/* end of: loop on axes, for reading inputs,
				   setting limit and home switch flags */

    /* check to see if logging should be triggered */
    if (emcmotStatus->logOpen &&
	!emcmotStatus->logStarted &&
	loggingAxis >= 0 && loggingAxis < EMCMOT_MAX_AXIS) {
	double val = 0.0;

	switch (emcmotStatus->logTriggerVariable) {
	case EMCLOG_TRIGGER_ON_FERROR:
	    val = emcmotDebug->ferrorCurrent[loggingAxis];
	    break;

	case EMCLOG_TRIGGER_ON_VOLT:
	    val = emcmotDebug->rawOutput[loggingAxis];
	    break;

	case EMCLOG_TRIGGER_ON_POS:
	    val = emcmotDebug->jointPos[loggingAxis];
	    break;

	case EMCLOG_TRIGGER_ON_VEL:
	    val = emcmotDebug->jointPos[loggingAxis] -
		emcmotDebug->oldJointPos[loggingAxis];
	    break;

	default:
	    break;
	}

	switch (emcmotStatus->logTriggerType) {
	case EMCLOG_MANUAL_TRIGGER:
	    break;

	case EMCLOG_DELTA_TRIGGER:
	    if (emcmotStatus->logStartVal - val <
		-emcmotStatus->logTriggerThreshold
		|| emcmotStatus->logStartVal - val >
		emcmotStatus->logTriggerThreshold) {
		emcmotStatus->logStarted = 1;
	    }
	    break;

	case EMCLOG_OVER_TRIGGER:
	    if (val > emcmotStatus->logTriggerThreshold) {
		emcmotStatus->logStarted = 1;
	    }
	    break;

	case EMCLOG_UNDER_TRIGGER:
	    if (val < emcmotStatus->logTriggerThreshold) {
		emcmotStatus->logStarted = 1;
	    }
	}
    }

    /* now we're outside the axis loop, having just read input, scaled it,
       read the limit and home switches and amp faults. We need to abort all
       motion if we're on limits, handle homing sequence, and handle mode and
       state transitions. */

    /* RUN STATE LOGIC: */

    /* check for disabling */
    if (!emcmotDebug->enabling && GET_MOTION_ENABLE_FLAG()) {
	/* clear out the motion emcmotDebug->queue and interpolators */
	tpClear(&emcmotDebug->queue);
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    tpClear(&emcmotDebug->freeAxis[axis]);
	    cubicDrain(&emcmotDebug->cubic[axis]);
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
		SET_AXIS_ENABLE_FLAG(axis, 0);
		SET_AXIS_HOMING_FLAG(axis, 0);
		emcmotStatus->output[axis] = 0.0;
	    }
	    /* don't clear the axis error flag, since that may signify why we 
	       just went into disabled state */
	}
	/* reset the trajectory interpolation counter, so that the kinematics
	   functions called during the disabled state run at the nominal
	   trajectory rate rather than the servo rate. It's loaded with
	   emcmotConfig->interpolationRate when it goes to zero. */
	interpolationCounter = 0;
	SET_MOTION_ENABLE_FLAG(0);
	/* don't clear the motion error flag, since that may signify why we
	   just went into disabled state */
    }

    /* check for emcmotDebug->enabling */
    if (emcmotDebug->enabling && !GET_MOTION_ENABLE_FLAG()) {
	tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    emcmotDebug->freePose.tran.x = emcmotDebug->jointPos[axis];
	    tpSetPos(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    pidReset(&emcmotConfig->pid[axis]);
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		extAmpEnable(axis, GET_AXIS_ENABLE_POLARITY(axis));
		SET_AXIS_ENABLE_FLAG(axis, 1);
		SET_AXIS_HOMING_FLAG(axis, 0);
	    }
	    /* clear any outstanding axis errors when going into enabled
	       state */
	    SET_AXIS_ERROR_FLAG(axis, 0);
	}
	SET_MOTION_ENABLE_FLAG(1);
	/* clear any outstanding motion errors when going into enabled state */
	SET_MOTION_ERROR_FLAG(0);

	/* init min-max-avg stats */
	mmxavgInit(&emcmotDebug->tMmxavg, emcmotDebug->tMmxavgSpace,
	    MMXAVG_SIZE);
	mmxavgInit(&emcmotDebug->sMmxavg, emcmotDebug->sMmxavgSpace,
	    MMXAVG_SIZE);
	mmxavgInit(&emcmotDebug->yMmxavg, emcmotDebug->yMmxavgSpace,
	    MMXAVG_SIZE);
	mmxavgInit(&emcmotDebug->fMmxavg, emcmotDebug->fMmxavgSpace,
	    MMXAVG_SIZE);
	mmxavgInit(&emcmotDebug->fyMmxavg, emcmotDebug->fyMmxavgSpace,
	    MMXAVG_SIZE);
	emcmotDebug->tMin = 0.0;
	emcmotDebug->tMax = 0.0;
	emcmotDebug->tAvg = 0.0;
	emcmotDebug->sMin = 0.0;
	emcmotDebug->sMax = 0.0;
	emcmotDebug->sAvg = 0.0;
	emcmotDebug->yMin = 0.0;
	emcmotDebug->yMax = 0.0;
	emcmotDebug->yAvg = 0.0;
	emcmotDebug->fyMin = 0.0;
	emcmotDebug->fyMax = 0.0;
	emcmotDebug->fyAvg = 0.0;
	emcmotDebug->fMin = 0.0;
	emcmotDebug->fMax = 0.0;
	emcmotDebug->fAvg = 0.0;
    }

    /* check for entering teleop mode */
    if (emcmotDebug->teleoperating && !GET_MOTION_TELEOP_FLAG()) {
	if (GET_MOTION_INPOS_FLAG()) {

	    /* update coordinated emcmotDebug->queue position */
	    tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
	    /* drain the cubics so they'll synch up */
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		cubicDrain(&emcmotDebug->cubic[axis]);
	    }
	    /* Initialize things to do when starting teleop mode. */
	    emcmotDebug->teleop_data.currentVel.tran.x = 0.0;
	    emcmotDebug->teleop_data.currentVel.tran.y = 0.0;
	    emcmotDebug->teleop_data.currentVel.tran.z = 0.0;
	    emcmotDebug->teleop_data.currentVel.a = 0.0;
	    emcmotDebug->teleop_data.currentVel.b = 0.0;
	    emcmotDebug->teleop_data.currentVel.c = 0.0;
	    emcmotDebug->teleop_data.desiredVel.tran.x = 0.0;
	    emcmotDebug->teleop_data.desiredVel.tran.y = 0.0;
	    emcmotDebug->teleop_data.desiredVel.tran.z = 0.0;
	    emcmotDebug->teleop_data.desiredVel.a = 0.0;
	    emcmotDebug->teleop_data.desiredVel.b = 0.0;
	    emcmotDebug->teleop_data.desiredVel.c = 0.0;
	    emcmotDebug->teleop_data.currentAccell.tran.x = 0.0;
	    emcmotDebug->teleop_data.currentAccell.tran.y = 0.0;
	    emcmotDebug->teleop_data.currentAccell.tran.z = 0.0;
	    emcmotDebug->teleop_data.currentAccell.a = 0.0;
	    emcmotDebug->teleop_data.currentAccell.b = 0.0;
	    emcmotDebug->teleop_data.currentAccell.c = 0.0;
	    emcmotDebug->teleop_data.desiredAccell.tran.x = 0.0;
	    emcmotDebug->teleop_data.desiredAccell.tran.y = 0.0;
	    emcmotDebug->teleop_data.desiredAccell.tran.z = 0.0;
	    emcmotDebug->teleop_data.desiredAccell.a = 0.0;
	    emcmotDebug->teleop_data.desiredAccell.b = 0.0;
	    emcmotDebug->teleop_data.desiredAccell.c = 0.0;
	    SET_MOTION_TELEOP_FLAG(1);
	    SET_MOTION_ERROR_FLAG(0);
	} else {
	    /* not in position-- don't honor mode change */
	    emcmotDebug->teleoperating = 0;
	}
    } else {
	if (GET_MOTION_INPOS_FLAG()) {
	    if (!emcmotDebug->teleoperating && GET_MOTION_TELEOP_FLAG()) {
		SET_MOTION_TELEOP_FLAG(0);
		if (!emcmotDebug->coordinating) {
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			/* update free planner positions */
			emcmotDebug->freePose.tran.x =
			    emcmotDebug->jointPos[axis];
			tpSetPos(&emcmotDebug->freeAxis[axis],
			    emcmotDebug->freePose);
			/* drain the cubics so they'll synch up */
			cubicDrain(&emcmotDebug->cubic[axis]);
		    }
		}
	    }
	}

	/* check for entering coordinated mode */
	if (emcmotDebug->coordinating && !GET_MOTION_COORD_FLAG()) {
	    if (GET_MOTION_INPOS_FLAG()) {
		/* update coordinated emcmotDebug->queue position */
		tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
		/* drain the cubics so they'll synch up */
		for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		    cubicDrain(&emcmotDebug->cubic[axis]);
		}
		/* clear the override limits flags */
		emcmotDebug->overriding = 0;
		emcmotStatus->overrideLimits = 0;
		SET_MOTION_COORD_FLAG(1);
		SET_MOTION_TELEOP_FLAG(0);
		SET_MOTION_ERROR_FLAG(0);
	    } else {
		/* not in position-- don't honor mode change */
		emcmotDebug->coordinating = 0;
	    }
	}

	/* check entering free space mode */
	if (!emcmotDebug->coordinating && GET_MOTION_COORD_FLAG()) {
	    if (GET_MOTION_INPOS_FLAG()) {
		for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		    /* update free planner positions */
		    emcmotDebug->freePose.tran.x =
			emcmotDebug->jointPos[axis];
		    tpSetPos(&emcmotDebug->freeAxis[axis],
			emcmotDebug->freePose);
		    /* drain the cubics so they'll synch up */
		    cubicDrain(&emcmotDebug->cubic[axis]);
		}
		SET_MOTION_COORD_FLAG(0);
		SET_MOTION_TELEOP_FLAG(0);
		SET_MOTION_ERROR_FLAG(0);
	    } else {
		/* not in position-- don't honor mode change */
		emcmotDebug->coordinating = 1;
	    }
	}

    }

    /* check for homing sequences */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (GET_AXIS_HOMING_FLAG(axis)) {
	    if (tpIsDone(&emcmotDebug->freeAxis[axis])) {
		/* check for decel or final move */
		if (emcmotDebug->homingPhase[axis] == 5) {
		    /* reset flag-- we're back at home */
		    emcmotDebug->homingPhase[axis] = 0;

		    /* rework the home kinematics values-- this could be done
		       after each message to set world or joint kinematics,
		       but we'll defer it to here so it happens just prior to
		       when it's needed. Note that the nominal value of these
		       may be changed, if the kinematics need to. */
		    kinematicsHome(&worldHome, emcmotDebug->jointHome,
			&fflags, &iflags);

		    /* clear flag that indicates all joints need rehoming if
		       any joint is moved, for machines with no forward kins */
		    rehomeAll = 0;

		    /* set input offset to value such that resulting input is
		       the emcmotDebug->jointHome[] value, with: input = (raw
		       - offset) / scale -> offset = raw - input * scale ->
		       offset = raw - emcmotDebug->jointHome * scale */

		    emcmotStatus->inputOffset[axis] =
			emcmotDebug->rawInput[axis] -
			emcmotDebug->jointHome[axis] *
			emcmotStatus->inputScale[axis];

		    /* recompute inputs to match so we don't have a momentary
		       jump */
		    emcmotStatus->input[axis] = emcmotDebug->jointHome[axis];
		    emcmotDebug->oldInput[axis] = emcmotStatus->input[axis];

		    /* reset interpolator so that it doesn't jump */
		    cubicOffset(&emcmotDebug->cubic[axis],
			emcmotDebug->jointHome[axis] -
			emcmotDebug->coarseJointPos[axis]);

		    /* set axis position to emcmotDebug->jointHome upon
		       homing */
		    emcmotDebug->jointPos[axis] =
			emcmotDebug->jointHome[axis];
		    emcmotDebug->coarseJointPos[axis] =
			emcmotDebug->jointHome[axis];
		    emcmotDebug->freePose.tran.x =
			emcmotDebug->jointHome[axis];
		    tpSetPos(&emcmotDebug->freeAxis[axis],
			emcmotDebug->freePose);

		    SET_AXIS_HOMING_FLAG(axis, 0);
		    SET_AXIS_HOMED_FLAG(axis, 1);

		    /* set emcmotDebug->allHomed flag if all active axes are
		       homed; this will signify that kinematics functions can
		       be called */
		    emcmotDebug->allHomed = 1;
		    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
			if (GET_AXIS_ACTIVE_FLAG(t) &&
			    !GET_AXIS_HOMED_FLAG(t)) {
			    /* one active axis is not homed */
			    emcmotDebug->allHomed = 0;
			    break;
			}
		    }
		    /* FIXME-- this only works with no forward kins */
		    if (emcmotDebug->allHomed) {
			emcmotStatus->pos = worldHome;
			emcmotStatus->actualPos = worldHome;
		    }
		}

		if (emcmotDebug->homingPhase[axis] == 4) {
		    /* just finished decel, now we'll do final home */
		    /* add move back to latched position + backoff */
		    emcmotDebug->freePose.tran.x =
			(emcmotDebug->saveLatch[axis] -
			emcmotStatus->inputOffset[axis]) *
			emcmotDebug->inverseInputScale[axis] +
			emcmotConfig->homeOffset[axis];
		    /* Note that I put a multiplication factor of 2 in front
		       of the homing velocity below. The reason is that, I
		       think, if you found the index pulse you know your
		       exact position it is save to travel with higher
		       speeds. In addition to that, you actually see that the
		       machine has found its index pulse for the specified
		       axis */
		    tpSetVmax(&emcmotDebug->freeAxis[axis],
			2 * (emcmotConfig->homingVel[axis]));
		    if ((retval = tpAddLine(&emcmotDebug->freeAxis[axis],
				emcmotDebug->freePose)) == 0) {
			/* Advance homing sequence only if motion is added to 
			   the tp */
			emcmotDebug->homingPhase[axis] = 5;
		    }
		}
	    }			/* end of: if axis is done either decel or
				   final home */
	}			/* end of: if axis is homing */
    }				/* end of: axis loop that checks for homing */

    /* RUN MOTION CALCULATIONS: */

    /* run axis interpolations and outputs, but only if we're enabled. This
       section is "suppressed" if we're not enabled, although the read/write
       of encoders/dacs is still done. */
    whichCycle = 0;
    if (GET_MOTION_ENABLE_FLAG()) {
	/* set whichCycle to be at least a servo cycle, for calc time logging
	 */
	whichCycle = 1;

	/* check to see if the interpolators are empty */
	while (cubicNeedNextPoint(&emcmotDebug->cubic[0])) {
	    /* they're empty, so pull next point(s) off Cartesian or joint
	       planner, depending upon coord or free mode. */

	    /* check to see whether we're in teleop, coordinated or free
	       mode, to decide which motion planner to call */
	    if (GET_MOTION_TELEOP_FLAG()) {
		double accell_mag;

		emcmotDebug->teleop_data.desiredAccell.tran.x =
		    (emcmotDebug->teleop_data.desiredVel.tran.x -
		    emcmotDebug->teleop_data.currentVel.tran.x) /
		    emcmotConfig->trajCycleTime;
		emcmotDebug->teleop_data.desiredAccell.tran.y =
		    (emcmotDebug->teleop_data.desiredVel.tran.y -
		    emcmotDebug->teleop_data.currentVel.tran.y) /
		    emcmotConfig->trajCycleTime;
		emcmotDebug->teleop_data.desiredAccell.tran.z =
		    (emcmotDebug->teleop_data.desiredVel.tran.z -
		    emcmotDebug->teleop_data.currentVel.tran.z) /
		    emcmotConfig->trajCycleTime;
		pmCartMag(emcmotDebug->teleop_data.desiredAccell.tran,
		    &accell_mag);
		emcmotDebug->teleop_data.desiredAccell.a =
		    (emcmotDebug->teleop_data.desiredVel.a -
		    emcmotDebug->teleop_data.currentVel.a) /
		    emcmotConfig->trajCycleTime;
		emcmotDebug->teleop_data.desiredAccell.b =
		    (emcmotDebug->teleop_data.desiredVel.b -
		    emcmotDebug->teleop_data.currentVel.b) /
		    emcmotConfig->trajCycleTime;
		emcmotDebug->teleop_data.desiredAccell.c =
		    (emcmotDebug->teleop_data.desiredVel.c -
		    emcmotDebug->teleop_data.currentVel.c) /
		    emcmotConfig->trajCycleTime;

		if (emcmotDebug->teleop_data.desiredAccell.a > accell_mag) {
		    accell_mag = emcmotDebug->teleop_data.desiredAccell.a;
		}
		if (emcmotDebug->teleop_data.desiredAccell.b > accell_mag) {
		    accell_mag = emcmotDebug->teleop_data.desiredAccell.b;
		}
		if (emcmotDebug->teleop_data.desiredAccell.c > accell_mag) {
		    accell_mag = emcmotDebug->teleop_data.desiredAccell.c;
		}
		if (accell_mag > emcmotStatus->acc) {
		    pmCartScalMult(emcmotDebug->teleop_data.desiredAccell.
			tran, emcmotStatus->acc / accell_mag,
			&emcmotDebug->teleop_data.currentAccell.tran);
		    emcmotDebug->teleop_data.currentAccell.a =
			emcmotDebug->teleop_data.desiredAccell.a *
			emcmotStatus->acc / accell_mag;
		    emcmotDebug->teleop_data.currentAccell.b =
			emcmotDebug->teleop_data.desiredAccell.b *
			emcmotStatus->acc / accell_mag;
		    emcmotDebug->teleop_data.currentAccell.c =
			emcmotDebug->teleop_data.desiredAccell.c *
			emcmotStatus->acc / accell_mag;
		    emcmotDebug->teleop_data.currentVel.tran.x +=
			emcmotDebug->teleop_data.currentAccell.tran.x *
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.currentVel.tran.y +=
			emcmotDebug->teleop_data.currentAccell.tran.y *
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.currentVel.tran.z +=
			emcmotDebug->teleop_data.currentAccell.tran.z *
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.currentVel.a +=
			emcmotDebug->teleop_data.currentAccell.a *
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.currentVel.b +=
			emcmotDebug->teleop_data.currentAccell.b *
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.currentVel.c +=
			emcmotDebug->teleop_data.currentAccell.c *
			emcmotConfig->trajCycleTime;
		} else {
		    emcmotDebug->teleop_data.currentAccell =
			emcmotDebug->teleop_data.desiredAccell;
		    emcmotDebug->teleop_data.currentVel =
			emcmotDebug->teleop_data.desiredVel;
		}

		emcmotStatus->pos.tran.x +=
		    emcmotDebug->teleop_data.currentVel.tran.x *
		    emcmotConfig->trajCycleTime;
		emcmotStatus->pos.tran.y +=
		    emcmotDebug->teleop_data.currentVel.tran.y *
		    emcmotConfig->trajCycleTime;
		emcmotStatus->pos.tran.z +=
		    emcmotDebug->teleop_data.currentVel.tran.z *
		    emcmotConfig->trajCycleTime;
		emcmotStatus->pos.a +=
		    emcmotDebug->teleop_data.currentVel.a *
		    emcmotConfig->trajCycleTime;
		emcmotStatus->pos.b +=
		    emcmotDebug->teleop_data.currentVel.b *
		    emcmotConfig->trajCycleTime;
		emcmotStatus->pos.c +=
		    emcmotDebug->teleop_data.currentVel.c *
		    emcmotConfig->trajCycleTime;

		/* convert to joints */
		kinematicsInverse(&emcmotStatus->pos,
		    emcmotDebug->coarseJointPos, &iflags, &fflags);

		/* spline joints up-- note that we may be adding points that
		   fail soft limits, but we'll abort at the end of this cycle 
		   so it doesn't really matter */
		for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		    cubicAddPoint(&emcmotDebug->cubic[axis],
			emcmotDebug->coarseJointPos[axis]);
		}

		if (kinType == KINEMATICS_IDENTITY) {
		    /* call forward kinematics on input points for actual
		       pos, at trajectory rate to save bandwidth */
		    kinematicsForward(emcmotStatus->input,
			&emcmotStatus->actualPos, &fflags, &iflags);
		} else {
		    /* fake it by setting actual pos to commanded pos */
		    emcmotStatus->actualPos = emcmotStatus->pos;
		}
	    } else {
		if (GET_MOTION_COORD_FLAG()) {
		    /* we're in coordinated mode-- pull a pose off the
		       Cartesian trajectory planner, run it through the
		       inverse kinematics, and spline up the joint points for 
		       interpolation in servo cycles. */

		    /* set whichCycle to be a Cartesian trajectory cycle, for 
		       calc time logging */
		    whichCycle = 2;

		    /* run coordinated trajectory planning cycle */
		    tpRunCycle(&emcmotDebug->queue);

		    /* set new commanded traj pos */
		    emcmotStatus->pos = tpGetPos(&emcmotDebug->queue);

		    /* convert to joints */
		    kinematicsInverse(&emcmotStatus->pos,
			emcmotDebug->coarseJointPos, &iflags, &fflags);

		    /* spline joints up-- note that we may be adding points
		       that fail soft limits, but we'll abort at the end of
		       this cycle so it doesn't really matter */
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			cubicAddPoint(&emcmotDebug->cubic[axis],
			    emcmotDebug->coarseJointPos[axis]);
		    }

		    if (kinType == KINEMATICS_IDENTITY) {
			/* call forward kinematics on input points for actual 
			   pos, at trajectory rate to save bandwidth */
			kinematicsForward(emcmotStatus->input,
			    &emcmotStatus->actualPos, &fflags, &iflags);
		    } else {
			/* fake it by setting actual pos to commanded pos */
			emcmotStatus->actualPos = emcmotStatus->pos;
		    }

		    /* now emcmotStatus->actualPos, emcmotStatus->pos, and
		       emcmotDebug->coarseJointPos[] are set */

		} /* end of: coord mode */
		else {
		    /* we're in free mode-- run joint planning cycles */
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			/* set whichCycle to be a joint trajectory cycle, for 
			   calc time logging */
			/* note that this may include one or more joint
			   trajectory cycles, so calc time may be inherently
			   variable */
			whichCycle = 2;

			/* run joint trajectory planning cycle */
			tpRunCycle(&emcmotDebug->freeAxis[axis]);

			/* set new coarse joint position. FIXME-- this uses
			   only the tran.x field of the TP_STRUCT, which is
			   overkill. We need a TP_STRUCT with a single scalar 
			   element. */
			emcmotDebug->coarseJointPos[axis] =
			    tpGetPos(&emcmotDebug->freeAxis[axis]).tran.x;

			/* spline joint up-- note that we may be adding a
			   point that fails soft limit, but we'll abort at
			   the end of this cycle so it doesn't really matter */
			cubicAddPoint(&emcmotDebug->cubic[axis],
			    emcmotDebug->coarseJointPos[axis]);
		    }		/* end of: axis for loop for joint planning
				   cycle */

		    if (kinType == KINEMATICS_IDENTITY) {
			/* set actualPos from actual inputs */
			kinematicsForward(emcmotStatus->input,
			    &emcmotStatus->actualPos, &fflags, &iflags);
			/* set pos from nominal joints, since we're in joint
			   mode */
			kinematicsForward(emcmotDebug->coarseJointPos,
			    &emcmotStatus->pos, &fflags, &iflags);
		    } else if (kinType != KINEMATICS_INVERSE_ONLY) {
			/* here is where we call the forward kinematics
			   repeatedly, when we're in free mode, so that the
			   world coordinates are kept up to date when joints
			   are moving. This is only done if we have the
			   kinematics. emcmotStatus->pos needs to be set with 
			   an estimate for the kinematics to converge, which
			   is true when we enter free mode from coordinated
			   mode or after the machine is homed. */
			EmcPose temp = emcmotStatus->pos;
			if (0 == kinematicsForward(emcmotStatus->input,
				&temp, &fflags, &iflags)) {
			    emcmotStatus->pos = temp;
			    emcmotStatus->actualPos = temp;
			}
			/* else leave them alone */
		    } else {
			/* no foward kins, and we're in joint mode, so we
			   have no estimate of world coords, and we have to
			   leave them alone */
		    }
		    /* now emcmotStatus->actualPos, emcmotStatus->pos, and
		       emcmotDebug->coarseJointPos[] are set */
		}		/* end of: free mode trajectory planning */
	    }			/* end of: not teleop mode */
	}			/* end of: while (cubicNeedNextPoint(0)) */

	/* we're still in motion enabled section. For coordinated mode, the
	   Cartesian trajectory cycle has been computed, if necessary, run
	   through the inverse kinematics, and the joints have been splined
	   up for interpolation. For free mode, the joint trajectory cycles
	   have been computed, if necessary, and the joints have been splined 
	   up for interpolation. We still need to push the actual input
	   through the forward kinematics, for actual pos.

	   Effects:

	   For coord mode, emcmotStatus->pos contains the commanded Cartesian 
	   pose, emcmotDebug->coarseJointPos[] contains the results of the
	   inverse kinematics at the coarse (trajectory) rate, and the
	   interpolators are not empty.

	   For free mode, emcmotStatus->pos is unchanged, and needs to be
	   updated via the forward kinematics. FIXME-- make sure this
	   happens, and note where in this comment.
	   emcmotDebug->coarseJointPos[] contains the results of the joint
	   trajectory calculations at the coarse (trajectory) rate, and the
	   interpolators are not empty. */

	/* check for soft joint limits. If so, abort all motion. The
	   interpolators will pick this up further down and begin planning
	   abort and stop. */
	emcmotDebug->onLimit = 0;
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    SET_AXIS_PSL_FLAG(axis, 0);
	    SET_AXIS_NSL_FLAG(axis, 0);
	    if (GET_AXIS_HOMED_FLAG(axis)) {
		if (emcmotDebug->coarseJointPos[axis] >
		    emcmotConfig->maxLimit[axis]) {
		    SET_AXIS_ERROR_FLAG(axis, 1);
		    SET_AXIS_PSL_FLAG(axis, 1);
		    emcmotDebug->onLimit = 1;
		} else if (emcmotDebug->coarseJointPos[axis] <
		    emcmotConfig->minLimit[axis]) {
		    SET_AXIS_ERROR_FLAG(axis, 1);
		    SET_AXIS_NSL_FLAG(axis, 1);
		}
	    }
	}

	/* reset emcmotDebug->wasOnLimit flag iff all joints are free of soft
	   limits, as seen in the flag bits set last cycle. No need to do
	   this for hard limits, since emcmotDebug->wasOnLimit only prevents
	   flurry of aborts while on a soft limit and hard limits don't
	   abort, they disable. */
	if (emcmotDebug->onLimit) {
	    if (!emcmotDebug->wasOnLimit) {
		/* abort everything, regardless of coord or free mode */
		tpAbort(&emcmotDebug->queue);
		for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		    tpAbort(&emcmotDebug->freeAxis[axis]);
		    emcmotDebug->wasOnLimit = 1;
		}
	    }
	    /* else we were on a limit, so inhibit firing of aborts */
	} else {
	    /* not on a limit, so clear emcmotDebug->wasOnLimit so aborts
	       fire next time we are on a limit */
	    emcmotDebug->wasOnLimit = 0;
	}

	if (whichCycle == 2) {
	    /* we're on a trajectory cycle, either Cartesian or joint
	       planning, so log per-traj-cycle data if logging active */
	    logIt = 0;

	    if (emcmotStatus->logStarted && emcmotStatus->logSkip >= 0) {

		/* calculate current vel, accel, for logging */
		emcmotDebug->newVel.tran.x =
		    (emcmotStatus->pos.tran.x -
		    emcmotDebug->oldPos.tran.x) / emcmotConfig->trajCycleTime;
		emcmotDebug->newVel.tran.y =
		    (emcmotStatus->pos.tran.y -
		    emcmotDebug->oldPos.tran.y) / emcmotConfig->trajCycleTime;
		emcmotDebug->newVel.tran.z =
		    (emcmotStatus->pos.tran.z -
		    emcmotDebug->oldPos.tran.z) / emcmotConfig->trajCycleTime;
		emcmotDebug->oldPos = emcmotStatus->pos;
		emcmotDebug->newAcc.tran.x =
		    (emcmotDebug->newVel.tran.x -
		    emcmotDebug->oldVel.tran.x) / emcmotConfig->trajCycleTime;
		emcmotDebug->newAcc.tran.y =
		    (emcmotDebug->newVel.tran.y -
		    emcmotDebug->oldVel.tran.y) / emcmotConfig->trajCycleTime;
		emcmotDebug->newAcc.tran.z =
		    (emcmotDebug->newVel.tran.z -
		    emcmotDebug->oldVel.tran.z) / emcmotConfig->trajCycleTime;
		emcmotDebug->oldVel = emcmotDebug->newVel;

		/* save the type with the log item */
		ls.type = emcmotStatus->logType;

		/* now log type-specific data */
		switch (emcmotStatus->logType) {

		case EMCMOT_LOG_TYPE_TRAJ_POS:
		    if (logSkip-- <= 0) {
			ls.item.trajPos.time = start - logStartTime;
			ls.item.trajPos.pos = emcmotStatus->pos.tran;
			logSkip = emcmotStatus->logSkip;
			logIt = 1;
		    }
		    break;

		case EMCMOT_LOG_TYPE_TRAJ_VEL:
		    if (logSkip-- <= 0) {
			ls.item.trajVel.time = start - logStartTime;
			ls.item.trajVel.vel = emcmotDebug->newVel.tran;
			pmCartMag(emcmotDebug->newVel.tran,
			    &ls.item.trajVel.mag);
			logSkip = emcmotStatus->logSkip;
			logIt = 1;
		    }
		    break;

		case EMCMOT_LOG_TYPE_TRAJ_ACC:
		    if (logSkip-- <= 0) {
			ls.item.trajAcc.time = start - logStartTime;
			ls.item.trajAcc.acc = emcmotDebug->newAcc.tran;
			pmCartMag(emcmotDebug->newAcc.tran,
			    &ls.item.trajAcc.mag);
			logSkip = emcmotStatus->logSkip;
			logIt = 1;
		    }
		    break;

		default:
		    break;
		}		/* end of: switch on log type */

		/* now log it */
		if (logIt) {
		    emcmotLogAdd(emcmotLog, ls);
		    emcmotStatus->logPoints = emcmotLog->howmany;
		    logIt = 0;
		}
	    }			/* end of: if (emcmotStatus->logStarted &&
				   emcmotStatus->logSkip >= 0) */
	}
	/* end of: if (whichCycle == 2), for trajectory cycle logging */
	/* run interpolation and compensation */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    /* interpolate */
	    emcmotDebug->oldJointPos[axis] = emcmotDebug->jointPos[axis];
	    emcmotDebug->jointPos[axis] =
		cubicInterpolate(&emcmotDebug->cubic[axis], 0, 0, 0, 0);
	    emcmotDebug->jointVel[axis] =
		(emcmotDebug->jointPos[axis] -
		emcmotDebug->oldJointPos[axis]) /
		emcmotConfig->servoCycleTime;



#ifdef COMPING
	    /* set direction flag iff there's a direction change, otherwise
	       leave it alone so that stops won't change dir */
	    if (emcmotDebug->jointVel[axis] > 0.0 && dir[axis] < 0) {
		dir[axis] = 1;
	    } else if (emcmotDebug->jointVel[axis] < 0.0 && dir[axis] > 0) {
		dir[axis] = -1;
	    }
	    /* else leave it alone */
#endif /* COMPING */

	    /* run output calculations */
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		/* BACKLASH COMPENSATION */
		/* FIXME-- make backlash part of the EMC status proper, not
		   the PID structure */
		if (emcmotDebug->jointPos[axis] -
		    emcmotDebug->oldJointPos[axis] > 0.0) {
		    oldbcomp = emcmotDebug->bcomp[axis];
		    emcmotDebug->bcomp[axis] =
			+emcmotConfig->pid[axis].backlash / 2.0;
		    if (emcmotDebug->bcompdir[axis] != +1) {
			emcmotDebug->bac_done[axis] = 0;
			emcmotDebug->bac_di[axis] =
			    emcmotDebug->bcompincr[axis];
			emcmotDebug->bac_d[axis] = 0;
			emcmotDebug->bac_D[axis] =
			    emcmotDebug->bcomp[axis] - oldbcomp;
			emcmotDebug->bac_halfD[axis] =
			    0.5 * emcmotDebug->bac_D[axis];
			emcmotDebug->bac_incrincr[axis] =
			    emcmotStatus->acc *
			    emcmotConfig->servoCycleTime *
			    emcmotConfig->servoCycleTime;
			emcmotDebug->bac_incr[axis] =
			    -0.5 * emcmotDebug->bac_incrincr[axis];
			emcmotDebug->bcompdir[axis] = +1;
		    }
		} else if (emcmotDebug->jointPos[axis] -
		    emcmotDebug->oldJointPos[axis] < 0.0) {
		    oldbcomp = emcmotDebug->bcomp[axis];
		    emcmotDebug->bcomp[axis] =
			-emcmotConfig->pid[axis].backlash / 2.0;
		    if (emcmotDebug->bcompdir[axis] != -1) {
			emcmotDebug->bac_done[axis] = 0;
			emcmotDebug->bac_di[axis] =
			    emcmotDebug->bcompincr[axis];
			emcmotDebug->bac_d[axis] = 0;
			emcmotDebug->bac_D[axis] =
			    emcmotDebug->bcomp[axis] - oldbcomp;
			emcmotDebug->bac_halfD[axis] =
			    0.5 * emcmotDebug->bac_D[axis];
			emcmotDebug->bac_incrincr[axis] =
			    emcmotStatus->acc *
			    emcmotConfig->servoCycleTime *
			    emcmotConfig->servoCycleTime;
			emcmotDebug->bac_incr[axis] =
			    -0.5 * emcmotDebug->bac_incrincr[axis];
			emcmotDebug->bcompdir[axis] = -1;
		    }
		}
		/* else no motion, so leave emcmotDebug->bcomp what it was */

		/* mete out the backlash according to an acc/dec profile */
		if (!emcmotDebug->bac_done[axis]) {
		    if (emcmotDebug->bac_D[axis] > 0.0) {
			emcmotDebug->bcompincr[axis] =
			    emcmotDebug->bac_di[axis] +
			    emcmotDebug->bac_d[axis];
			if (emcmotDebug->bac_d[axis] <
			    emcmotDebug->bac_halfD[axis]) {
			    emcmotDebug->bac_incr[axis] +=
				emcmotDebug->bac_incrincr[axis];
			    emcmotDebug->bac_d[axis] +=
				emcmotDebug->bac_incr[axis];
			} else if (emcmotDebug->bac_d[axis] <
			    emcmotDebug->bac_D[axis]) {
			    emcmotDebug->bac_incr[axis] -=
				emcmotDebug->bac_incrincr[axis];
			    emcmotDebug->bac_d[axis] +=
				emcmotDebug->bac_incr[axis];
			}
			if (emcmotDebug->bac_d[axis] >=
			    emcmotDebug->bac_D[axis]
			    || emcmotDebug->bac_incr[axis] <= 0.0) {
			    emcmotDebug->bac_done[axis] = 1;
			}
		    } else {
			emcmotDebug->bcompincr[axis] =
			    emcmotDebug->bac_di[axis] +
			    emcmotDebug->bac_d[axis];
			if (emcmotDebug->bac_d[axis] >
			    emcmotDebug->bac_halfD[axis]) {
			    emcmotDebug->bac_incr[axis] +=
				emcmotDebug->bac_incrincr[axis];
			    emcmotDebug->bac_d[axis] -=
				emcmotDebug->bac_incr[axis];
			} else if (emcmotDebug->bac_d[axis] >
			    emcmotDebug->bac_D[axis]) {
			    emcmotDebug->bac_incr[axis] -=
				emcmotDebug->bac_incrincr[axis];
			    emcmotDebug->bac_d[axis] -=
				emcmotDebug->bac_incr[axis];
			}
			if (emcmotDebug->bac_d[axis] <=
			    emcmotDebug->bac_D[axis]
			    || emcmotDebug->bac_incr[axis] <= 0.0) {
			    emcmotDebug->bac_done[axis] = 1;
			}
		    }
		}
		/* end of: if not emcmotDebug->bac_done[axis] */
		/* ADJUST OUTPUT: this computes outJointPos[] from
		   emcmotDebug->jointPos[], by adding backlash compensation
		   and rounding result to nearest input unit */
		emcmotDebug->outJointPos[axis] =
		    emcmotDebug->jointPos[axis] +
		    emcmotDebug->bcompincr[axis];

#ifndef NO_ROUNDING
		numres =
		    emcmotDebug->outJointPos[axis] *
		    emcmotStatus->inputScale[axis];
		if (numres < 0.0) {
		    emcmotDebug->outJointPos[axis] =
			emcmotDebug->inverseInputScale[axis] *
			((int) (numres - 0.5));
		} else {
		    emcmotDebug->outJointPos[axis] =
			emcmotDebug->inverseInputScale[axis] *
			((int) (numres + 0.5));
		}
#endif

		/* Here's where binfunc_trajupdate() was called. */

		/* COMPENSATE: compensation means compute output
		   'emcmotStatus->output[]' based on desired position
		   'outJointPos[]' and input 'emcmotStatus->input[]'.

		   Currently the source calls for PID compensation. FIXME--
		   add wrapper for compensator, with ptr to emcmotStatus
		   struct, with semantics that ->output[] needs to be filled. 
		 */

		/* here is PID compensation */
		/* note that we have to compare adjusted output 'outJointPos' 
		   with the input, but the input has already had backlash
		   comp taken out, while the output has just had it added in. 
		   So, we need to add it to the input for this calculation */
		emcmotStatus->output[axis] =
		    pidRunCycle(&emcmotConfig->pid[axis],
		    emcmotStatus->input[axis] +
		    emcmotDebug->bcompincr[axis],
		    emcmotDebug->outJointPos[axis]);

		/* COMPUTE FOLLOWING ERROR: */

		/* compute signed following error and magnitude */
		thisFerror[axis] =
		    emcmotDebug->jointPos[axis] - emcmotStatus->input[axis];
		magFerror = fabs(thisFerror[axis]);
		emcmotDebug->ferrorCurrent[axis] = thisFerror[axis];

		/* record the max ferror for this axis */
		if (emcmotDebug->ferrorHighMark[axis] < magFerror) {
		    emcmotDebug->ferrorHighMark[axis] = magFerror;
		}

		/* compute the scaled ferror for a move of this speed */
		limitFerror = emcmotConfig->maxFerror[axis] /
		    emcmotConfig->limitVel * emcmotDebug->jointVel[axis];
		if (limitFerror < emcmotConfig->minFerror[axis]) {
		    limitFerror = emcmotConfig->minFerror[axis];
		}

		/* abort if this ferror is greater than the scaled ferror */
		if (magFerror > limitFerror) {
		    /* abort! abort! following error exceeded */
		    SET_AXIS_ERROR_FLAG(axis, 1);
		    SET_AXIS_FERROR_FLAG(axis, 1);
		    if (emcmotDebug->enabling) {
			/* report the following error just this once */
			reportError("axis %d following error", axis);
		    }
		    emcmotDebug->enabling = 0;
		} else {
		    SET_AXIS_FERROR_FLAG(axis, 0);
		}
	    } /* end of: if (GET_AXIS_ACTIVE_FLAG(axis)) */
	    else {
		/* axis is not active-- leave the pid output where it is-- if 
		   axis is not active one can still write to the dac */
	    }

	    /* CLAMP OUTPUT: */

	    /* clamp output means take 'emcmotStatus->output[]' and limit to
	       range 'emcmotConfig->minOutput[] .. emcmotConfig->maxOutput[]' 
	     */
	    if (emcmotStatus->output[axis] < emcmotConfig->minOutput[axis]) {
		emcmotStatus->output[axis] = emcmotConfig->minOutput[axis];
	    } else if (emcmotStatus->output[axis] >
		emcmotConfig->maxOutput[axis]) {
		emcmotStatus->output[axis] = emcmotConfig->maxOutput[axis];
	    }

	    /* CHECK FOR LATCH CONDITION: */
	    /* 
	       check for latch condition means if we're waiting for a latched 
	       index pulse, and we see the pulse switch, we read the raw
	       input and abort. The offset is set above in the homing section 
	       by noting that if we're homing, and emcmotDebug->homingPhase[] 
	       is 3, we latched.

	       This presumes an encoder index pulse. FIXME-- remove explicit
	       calls to encoder index pulse, to allow for open-loop control
	       latching via switches only. Open-loop control can be achieved, 
	       at least for STG boards, by defining NO_INDEX_PULSE in
	       extstgmot.c */
	    if (emcmotDebug->homingPhase[axis] == 3) {
		/* read encoder index pulse */
		extEncoderReadLatch(axis, &emcmotDebug->latchFlag[axis]);
		if (emcmotDebug->latchFlag[axis]) {
		    /* code below is excuted once the index pulse is found */
		    /* call for an abort-- when it's finished, code above
		       sets inputOffset[] to emcmotDebug->saveLatch[] */
		    if (tpAbort(&emcmotDebug->freeAxis[axis]) == 0) {
			/* Only advance the homing sequence if the motion is
			   really aborted */
			emcmotDebug->homingPhase[axis] = 4;
			/* get latched position in RAW UNITS */
			emcmotDebug->saveLatch[axis] =
			    emcmotDebug->rawInput[axis];
		    }
		}		/* end of: if (emcmotDebug->latchFlag[axis]) */
	    }
	    /* end of: if (emcmotDebug->homingPhase[axis] == 3 */
	    /* CHECK FOR HOMING PHASE 2, COMMAND THE MOVE TO THE INDEX PULSE */
	    if (emcmotDebug->homingPhase[axis] == 2) {
		if (GET_AXIS_HOMING_POLARITY(axis)) {
		    emcmotDebug->freePose.tran.x = -2.0 * AXRANGE(axis);
		} else {
		    emcmotDebug->freePose.tran.x = +2.0 * AXRANGE(axis);
		}
		if ((retval = tpAddLine(&emcmotDebug->freeAxis[axis],
			    emcmotDebug->freePose)) == 0) {
		    /* Only advance homing sequence if the motion is actually 
		       put in the traj. planner */
		    extEncoderResetIndex(axis);
		    emcmotDebug->homingPhase[axis] = 3;
		}
	    }

	    /* END OF: PHASE 2 */
	    /* CHECK FOR HOME SWITCH CONDITION AND then tpAbort: */
	    /* check if any of the home switch, phl, nhl are tripped */
	    if (emcmotDebug->homingPhase[axis] == 1) {
		if (GET_AXIS_HOME_SWITCH_FLAG(axis) ||
		    GET_AXIS_PHL_FLAG(axis) || GET_AXIS_NHL_FLAG(axis)) {
		    if (tpAbort(&emcmotDebug->freeAxis[axis]) == 0) {
			/* Advance homing sequence if motion is aborted */
			emcmotDebug->homingPhase[axis] = 2;
		    }
		}
	    }			/* end of:
				   if(emcmotDebug->homingPhase[axis]==1){ */
	}			/* end of: for (axis = 0; ...) */
    } /* end of: if (GET_MOTION_ENABLE_FLAG()) */
    else {
	/* 
	   we're not enabled, so no motion planning or interpolation has been 
	   done. emcmotDebug->jointPos[] is set to emcmotStatus->input[], and 
	   likewise with emcmotDebug->coarseJointPos[], which is normally
	   updated at the traj rate but it's convenient to do them here at
	   the same time at the servo rate. emcmotStatus->pos, ->actualPos
	   need to be run through forward kinematics.  Note that we are
	   running at the servo rate, so we need to slow down by the
	   interpolation factor to avoid soaking the CPU. If we were enabled, 
	   ->pos was set by calcs (coord mode) or forward kins (free mode),
	   and ->actualPos was set by forward kins on ->input[], all at the
	   trajectory rate. */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    emcmotDebug->coarseJointPos[axis] = emcmotStatus->input[axis];
	    emcmotDebug->oldJointPos[axis] = emcmotDebug->jointPos[axis];
	    emcmotDebug->jointPos[axis] = emcmotDebug->coarseJointPos[axis];
	    emcmotDebug->jointVel[axis] =
		(emcmotDebug->jointPos[axis] -
		emcmotDebug->oldJointPos[axis]) /
		emcmotConfig->servoCycleTime;
	}
	/* synthesize the trajectory interpolation, via a counter that
	   decrements from the interpolation rate. This causes the statements 
	   to execute at the trajectory rate instead of the servo rate at
	   which this enclosing code is called. */
	if (--interpolationCounter <= 0) {
	    if (kinType != KINEMATICS_INVERSE_ONLY) {
		/* call the forward kinematics, at the effective trajectory
		   rate */
		EmcPose temp = emcmotStatus->pos;
		if (0 == kinematicsForward(emcmotStatus->input, &temp,
			&fflags, &iflags)) {
		    emcmotStatus->pos = temp;
		    emcmotStatus->actualPos = temp;
		}
	    }
	    /* else can't generate Cartesian position, so leave it alone */

	    /* reload the interpolation counter that simulates the
	       interpolation done when enabled */
	    interpolationCounter = emcmotConfig->interpolationRate;
	}
    }

    extProbeCheck(&emcmotStatus->probeval);
    if (emcmotStatus->probing && emcmotStatus->probeTripped) {
	tpClear(&emcmotDebug->queue);
	emcmotStatus->probing = 0;
    } else if (emcmotStatus->probing && GET_MOTION_INPOS_FLAG() &&
	tpQueueDepth(&emcmotDebug->queue) == 0) {
	emcmotStatus->probing = 0;
    } else if (emcmotStatus->probing) {
	if (emcmotStatus->probeval == emcmotConfig->probePolarity) {
	    emcmotStatus->probeTripped = 1;
	    emcmotStatus->probedPos = emcmotStatus->actualPos;
	    if (GET_MOTION_COORD_FLAG()) {
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(0);
	    } else {
		/* check axis range */
		for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		    tpAbort(&emcmotDebug->freeAxis[axis]);
		    SET_AXIS_HOMING_FLAG(axis, 0);
		    SET_AXIS_ERROR_FLAG(axis, 0);
		}
	    }
	}
    }

    /* SCALE OUTPUTS: */

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotDebug->rawOutput[axis] =
	    (emcmotStatus->output[axis] -
	    emcmotStatus->outputOffset[axis]) *
	    emcmotDebug->inverseOutputScale[axis];
    }

    /* WRITE OUTPUTS: */

    /* write DACs-- note that this is done even when not enabled, although in 
       this case the pidOutputs are all zero unless manually overridden. They 
       will not be set by any calculations if we're not enabled. */
    extDacWriteAll(EMCMOT_MAX_AXIS, emcmotDebug->rawOutput);

    /* UPDATE THE REST OF THE DYNAMIC STATUS: */

    /* copy computed axis positions to status. Note that if motion is
       disabled, this is the same as ->input[]. */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotStatus->axisPos[axis] = emcmotDebug->jointPos[axis];
    }

    /* health heartbeat */
    emcmotStatus->heartbeat++;

    /* motion emcmotDebug->queue status */
    emcmotStatus->depth = tpQueueDepth(&emcmotDebug->queue);
    emcmotStatus->activeDepth = tpActiveDepth(&emcmotDebug->queue);
    emcmotStatus->id = tpGetExecId(&emcmotDebug->queue);
    emcmotStatus->queueFull = tcqFull(&emcmotDebug->queue.queue);
    SET_MOTION_INPOS_FLAG(0);
    if (tpIsDone(&emcmotDebug->queue)) {
	SET_MOTION_INPOS_FLAG(1);
    }

    /* axis status */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	SET_AXIS_INPOS_FLAG(axis, 0);
	if (tpIsDone(&emcmotDebug->freeAxis[axis])) {
	    SET_AXIS_INPOS_FLAG(axis, 1);
	} else {
	    /* this axis, at least, is moving, so set emcmotDebug->overriding 
	       flag */
	    if (emcmotStatus->overrideLimits) {
		emcmotDebug->overriding = 1;
	    }
	}
    }

    /* reset overrideLimits flag if we have started a move and now are in
       position */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (!GET_AXIS_INPOS_FLAG(axis)) {
	    break;
	}
    }
    if (axis == EMCMOT_MAX_AXIS) {
	/* ran through all axes, and all are in position */
	if (emcmotDebug->overriding) {
	    emcmotDebug->overriding = 0;
	    emcmotStatus->overrideLimits = 0;
	}
    }

    /* check to see if we should pause in order to implement single
       emcmotDebug->stepping */
    if (emcmotDebug->stepping && emcmotDebug->idForStep != emcmotStatus->id) {
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    tpPause(&emcmotDebug->freeAxis[axis]);
	}
	tpPause(&emcmotDebug->queue);
	emcmotDebug->stepping = 0;
	emcmotStatus->paused = 1;
    }

    /* calculate elapsed time and min/max/avg */
    end = etime();
    delta = end - start;

    emcmotDebug->last_time = emcmotDebug->cur_time;
    emcmotDebug->cur_time = end;

    if (emcmotDebug->last_time != 0.0) {
	if (GET_MOTION_ENABLE_FLAG()) {
	    if (emcmotDebug->cur_time - emcmotDebug->last_time >
		10 * emcmotConfig->servoCycleTime) {
		reportError("controller missed realtime deadline.");
	    }
	}
	if (DEBUG_MOTION) {
	    mmxavgAdd(&emcmotDebug->yMmxavg,
		(emcmotDebug->cur_time - emcmotDebug->last_time));
	    emcmotDebug->yMin = mmxavgMin(&emcmotDebug->yMmxavg);
	    emcmotDebug->yMax = mmxavgMax(&emcmotDebug->yMmxavg);
	    emcmotDebug->yAvg = mmxavgAvg(&emcmotDebug->yMmxavg);
	}
    }

    emcmotStatus->computeTime = delta;
    if (DEBUG_MOTION) {
	if (2 == whichCycle) {
	    /* traj calcs done this cycle-- use tMin,Max,Avg */
	    mmxavgAdd(&emcmotDebug->tMmxavg, delta);
	    emcmotDebug->tMin = mmxavgMin(&emcmotDebug->tMmxavg);
	    emcmotDebug->tMax = mmxavgMax(&emcmotDebug->tMmxavg);
	    emcmotDebug->tAvg = mmxavgAvg(&emcmotDebug->tMmxavg);
	} else if (1 == whichCycle) {
	    /* servo calcs only this cycle-- use sMin,Max,Avg */
	    mmxavgAdd(&emcmotDebug->sMmxavg, delta);
	    emcmotDebug->sMin = mmxavgMin(&emcmotDebug->sMmxavg);
	    emcmotDebug->sMax = mmxavgMax(&emcmotDebug->sMmxavg);
	    emcmotDebug->sAvg = mmxavgAvg(&emcmotDebug->sMmxavg);
	} else {
	    /* calcs disabled this cycle-- use nMin,Max,Avg */
	    mmxavgAdd(&emcmotDebug->nMmxavg, delta);
	    emcmotDebug->nMin = mmxavgMin(&emcmotDebug->nMmxavg);
	    emcmotDebug->nMax = mmxavgMax(&emcmotDebug->nMmxavg);
	    emcmotDebug->nAvg = mmxavgAvg(&emcmotDebug->nMmxavg);
	}
    }

    /* log per-servo-cycle data if logging active */
    logIt = 0;
    if (emcmotStatus->logStarted && emcmotStatus->logSkip >= 0) {

	/* record type here, since all will set this */
	ls.type = emcmotStatus->logType;

	/* now log type-specific data */
	switch (ls.type) {

	case EMCMOT_LOG_TYPE_AXIS_POS:
	    if (logSkip-- <= 0) {
		ls.item.axisPos.time = end - logStartTime;
		ls.item.axisPos.input = emcmotStatus->input[loggingAxis];
		ls.item.axisPos.output = emcmotDebug->jointPos[loggingAxis];
		logSkip = emcmotStatus->logSkip;
		logIt = 1;
	    }
	    break;

	case EMCMOT_LOG_TYPE_ALL_INPOS:
	    if (logSkip-- <= 0) {
		ls.item.allInpos.time = end - logStartTime;
		for (axis = 0;
		    axis < EMCMOT_LOG_NUM_AXES &&
		    axis < EMCMOT_MAX_AXIS; axis++) {
		    ls.item.allInpos.input[axis] = emcmotStatus->input[axis];
		}
		logSkip = emcmotStatus->logSkip;
		logIt = 1;
	    }
	    break;

	case EMCMOT_LOG_TYPE_ALL_OUTPOS:
	    if (logSkip-- <= 0) {
		ls.item.allOutpos.time = end - logStartTime;
		for (axis = 0;
		    axis < EMCMOT_LOG_NUM_AXES &&
		    axis < EMCMOT_MAX_AXIS; axis++) {
		    ls.item.allOutpos.output[axis] =
			emcmotDebug->jointPos[axis];
		}
		logSkip = emcmotStatus->logSkip;
		logIt = 1;
	    }
	    break;

	case EMCMOT_LOG_TYPE_AXIS_VEL:
	    if (logSkip-- <= 0) {
		ls.item.axisVel.time = end - logStartTime;
		ls.item.axisVel.cmdVel =
		    emcmotDebug->jointPos[loggingAxis] -
		    emcmotDebug->oldJointPos[loggingAxis];
		ls.item.axisVel.actVel =
		    emcmotStatus->input[loggingAxis] -
		    emcmotDebug->oldInput[loggingAxis];
		logSkip = emcmotStatus->logSkip;
		logIt = 1;
	    }
	    break;

	case EMCMOT_LOG_TYPE_ALL_FERROR:
	    if (logSkip-- <= 0) {
		ls.item.allFerror.time = end - logStartTime;
		for (axis = 0;
		    axis < EMCMOT_LOG_NUM_AXES &&
		    axis < EMCMOT_MAX_AXIS; axis++) {
		    ls.item.allFerror.ferror[axis] =
			emcmotDebug->ferrorCurrent[axis];
		}
		logSkip = emcmotStatus->logSkip;
		logIt = 1;
	    }
	    break;

	case EMCMOT_LOG_TYPE_POS_VOLTAGE:
	    /* don't do a circular log-- suppress if full */
	    if (emcmotLog->howmany >= emcmotStatus->logSize) {
		emcmotStatus->output[loggingAxis] = 0.0;	/* drop the
								   DAC to
								   zero */
		emcmotStatus->logStarted = 0;	/* stop logging */
		logIt = 0;	/* should still be zero, reset anyway */
		break;
	    }
	    if (logSkip-- <= 0) {
		ls.item.posVoltage.time = end - logStartTime;
		for (axis = 0;
		    axis < EMCMOT_LOG_NUM_AXES &&
		    axis < EMCMOT_MAX_AXIS; axis++) {
		    ls.item.posVoltage.pos = emcmotStatus->input[loggingAxis];
		    ls.item.posVoltage.voltage =
			emcmotDebug->rawOutput[loggingAxis];
		}
		logSkip = emcmotStatus->logSkip;
		logIt = 1;
	    }
	    break;

	default:
	    break;
	}			/* end of: switch on log type */

	/* now log it */
	if (logIt) {
	    emcmotLogAdd(emcmotLog, ls);
	    emcmotStatus->logPoints = emcmotLog->howmany;
	    logIt = 0;
	}
    }				/* end of: if logging */
    emcmotDebug->running_time = etime() - emcmotDebug->start_time;

    /* set tail to head, which has already been incremented */
    emcmotStatus->tail = emcmotStatus->head;
    emcmotDebug->tail = emcmotDebug->head;
    emcmotConfig->tail = emcmotConfig->head;
}				/* end of: emcmotController() function */


/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/





/* isHoming() returns non-zero if any axes are homing, 0 if none are */
static int isHoming(void)
{
    int axis;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (GET_AXIS_HOMING_FLAG(axis)) {
	    return 1;
	}
    }
    /* got here, so none are homing */
    return 0;
}

/*
  axisComp(int axis, int dir, double nominal) looks up the real axis
  position value, given the nominal value and the direction of motion,
  based on the emcmotComp tables. It returns the comped value. If there's
  an error, the comped value is set to the nominal value. The alter value
  is added regardless of whether the comp table is active (total > 1),
  but if it is active it's added before the comping since the alter is
  a nominal delta.
*/
static double axisComp(int axis, int dir, double nominput)
{
    int index;
    double avgint;
    double compin;
    int lower, upper;
    int total;
    double *nominal;
    double *ptr;
    double denom;

    /* first adjust nominput by the alter value, before looking it up */
    nominput += emcmotComp[axis]->alter;

    total = emcmotComp[axis]->total;
    if (total < 2) {
	return nominput;
    }
    avgint = emcmotComp[axis]->avgint;
    index = (int) (nominput / avgint);
    nominal = emcmotComp[axis]->nominal;
    if (dir < 0) {
	ptr = emcmotComp[axis]->reverse;
    } else {
	ptr = emcmotComp[axis]->forward;
    }

    /* set the comp input to the nominput, by default */
    compin = nominput;
    lower = upper = 0;
    while (index >= 0 && index < total) {
	if (nominput > nominal[index]) {
	    if (index == total - 1) {
		/* off the top */
		break;
	    } else if (nominput <= nominal[index + 1]) {
		/* in range */
		lower = index;
		upper = index + 1;
		denom = nominal[upper] - nominal[lower];
		if (denom < DBL_MIN) {
		    compin = nominput;
		    /* error */
		} else {
		    compin =
			ptr[lower] + (nominput -
			nominal[lower]) * (ptr[upper] - ptr[lower]) / denom;
		}
		break;
	    } else {
		/* index too low */
		index++;
		continue;
	    }
	} else if (nominput < nominal[index]) {
	    if (index == 0) {
		/* off the bottom */
		break;
	    } else if (nominput >= nominal[index - 1]) {
		/* in range */
		lower = index - 1;
		upper = index;
		denom = nominal[upper] - nominal[lower];
		if (denom < DBL_MIN) {
		    compin = nominput;
		    /* error */
		} else {
		    compin =
			ptr[lower] + (nominput -
			nominal[lower]) * (ptr[upper] - ptr[lower]) / denom;
		}
		break;
	    } else {
		/* index too high */
		index--;
		continue;
	    }
	} else {
	    /* nominput == nominal[index], so return ptr[index] */
	    compin = ptr[index];
	    lower = index;
	    upper = index;
	    break;
	}
    }

    return compin;
}


