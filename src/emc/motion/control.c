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
#include "emcmotglb.h"
#include "motion.h"
#include "mot_priv.h"

/* these determine the debounce delay for the hardware inputs */
/* a setting of zero reacts immediately */
#define LIMIT_SWITCH_DEBOUNCE 10
#define AMP_FAULT_DEBOUNCE 10
#define HOME_SW_DEBOUNCE 0
#define PROBE_DEBOUNCE 0

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/* FIXME - this is a leftover global, it will eventually go away */
int rehomeAll;

/* these variables have the servo cycle time and 1/cycle time */
static double servo_period;
static double servo_freq;

/***********************************************************************
*                      LOCAL FUNCTION PROTOTYPES                       *
************************************************************************/

/* the following functions are called (in this order) by the main
   controller function.  They are an attempt to break the huge
   function (originally 1600 lines) into something a little easier
   to understand.
*/

/* 'process_inputs()' is responsible for reading hardware input
   signals (from the HAL) and doing basic processing on them.  In
   the case of position feedback, that means removing backlash comp
   and calculating the following error.  For switches, it means
   debouncing them and setting flags in the emcmotStatus structure.
*/
static void process_inputs(void);

/* 'check_soft_limits()' checks the position of active axes against
   their soft limits, and sets flags accordingly.  It does not check
   limits on axes that have not been homed, since their position is
   unknown.  It only sets flags, it does not take any action.
*/
static void check_soft_limits(void);

/* 'check_for_faults()' is responsible for detecting fault conditions
   such as limit switches, amp faults, following error, etc.  It only
   checks active axes.  It is also responsible for generating an error
   message.  (Later, once I understand the cmd/status/error interface
   better, it will probably generate error codes that can be passed
   up the architecture toward the GUI - printing error messages
   directly seems a little messy)
*/
static void check_for_faults(void);

/* 'set_operating_mode()' handles transitions between the operating
   modes, which are free, coordinated, and teleop.  This stuff needs
   to be better documented.  It is basically a state machine, with
   a current state, a desired state, and rules determining when the
   state can change.  It should be rewritten as such, but for now
   it consists of code copied exactly from emc1.
*/
static void set_operating_mode(void);

/* 'get_pos_cmds()' generates the position setpoints.  This includes
   calling the trajectory planner and interpolating it's outputs.
   Currently it is code copied directly from emc1.
*/

static void get_pos_cmds(void);

/* 'compute_backlash()' is responsible for calculating backlash and
   lead screw error compensation.  (Leadscrew error compensation is
   a more sophisticated version that includes backlash comp.)  It uses
   the velocity in emcmotStatus->joint_vel_cmd to determine which way
   each axis is moving, and the position in emcmotStatus->joint_pos_cmd
   to determine where the axis is at.  That information is used to
   create the compensation value that is added to the joint_pos_cmd
   to create motor_pos_cmd, and is subtracted from motor_pos_fb to
   get joint_pos_fb.  (This function does not add or subtract the
   compensation value, it only computes it.)  The basic compensation
   value is in backlash_corr, however has makes step changes when
   the direction reverses.  backlash_filt is a ramped version, and
   that is the one that is later added/subtracted from the position.
*/
static void compute_backlash(void);

/* 'output_to_hal()' writes the handles the final stages of the
   control function.  It applies backlash comp and writes the
   final motor position to the HAL (which routes it to the PID
   loop).  It also drives other HAL outputs, and it writes a
   number of internal variables to HAL parameters so they can
   be observed with halscope and halmeter.
*/
static void output_to_hal(void);

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
    int axis;

    /* calculate servo period as a double - period is in integer nsec */
    servo_period = period * 0.000000001;
    /* calculate servo frequency for calcs like vel = Dpos / period */
    /* it's faster to do vel = Dpos * freq */
    servo_freq = 1.0 / servo_period;
    /* increment head count to indicate work in progress */
    emcmotStatus->head++;
    /* here begins the core of the controller */
    process_inputs();
    check_soft_limits();
    check_for_faults();
    set_operating_mode();

/* FIXME - about the only major thing missing now is homing */

    get_pos_cmds();

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* calculate joint velocity commands */
	emcmotStatus->joint_vel_cmd[axis] =
	    (emcmotStatus->joint_pos_cmd[axis] -
	    emcmotInternal->old_joint_pos_cmd[axis]) * servo_freq;
	/* update old position */
	emcmotInternal->old_joint_pos_cmd[axis] =
	    emcmotStatus->joint_pos_cmd[axis];
    }

    /* calculate backlash (or leadscrew comp) */
    compute_backlash();
    output_to_hal();
    /* here ends the core of the controller */
    emcmotStatus->heartbeat++;
    /* set tail to head, to indicate work complete */
    emcmotStatus->tail = emcmotStatus->head;

/* end of controller function */
}

/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

/* The protoypes and documentation for these functions are located
   at the top of the file in the section called "local function
   prototypes"
*/

static void process_inputs(void)
{
    int axis;
    double tmpd;
    axis_hal_t *axis_data;

    /* read and debounce probe input */
    if (*(machine_hal_data->probe_input)) {
	if (emcmotInternal->probe_debounce_cntr < PROBE_DEBOUNCE) {
	    emcmotInternal->probe_debounce_cntr++;
	} else {
	    emcmotStatus->probeVal = 1;
	}
    } else {
	if (emcmotInternal->probe_debounce_cntr > 0) {
	    emcmotInternal->probe_debounce_cntr--;
	} else {
	    emcmotStatus->probeVal = 0;
	}
    }

    /* read and process per-axis inputs */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* point to axis data */
	axis_data = &(machine_hal_data->axis[axis]);

	/* get position feedback, subtract backlash comp */
	axis_data->joint_pos_fb =
	    *(axis_data->motor_pos_fb) - axis_data->backlash_filt;
	/* copy to status structure */
	emcmotStatus->joint_pos_fb[axis] = axis_data->joint_pos_fb;

	/* calculate following error */
	emcmotStatus->ferrorCurrent[axis] =
	    emcmotStatus->joint_pos_cmd[axis] -
	    emcmotStatus->joint_pos_fb[axis];

	/* absolute value of ferror */
	if (emcmotStatus->ferrorCurrent[axis] < 0.0) {
	    tmpd = -emcmotStatus->ferrorCurrent[axis];
	} else {
	    tmpd = emcmotStatus->ferrorCurrent[axis];
	}
	emcmotInternal->ferrorAbs[axis] = tmpd;

	/* update maximum ferror if needed */
	if (tmpd > emcmotStatus->ferrorHighMark[axis]) {
	    emcmotStatus->ferrorHighMark[axis] = tmpd;
	}

	/* calculate following error limit */
	/* FIXME - should this use per axis max velocity instead of limitVel? 
	 */
	tmpd = emcmotConfig->maxFerror[axis] *
	    (emcmotStatus->joint_vel_cmd[axis] / emcmotConfig->limitVel);
	if (tmpd < emcmotConfig->minFerror[axis]) {
	    tmpd = emcmotConfig->minFerror[axis];
	}
	emcmotStatus->ferrorLimit[axis] = tmpd;

	/* update following error flag */
	if (emcmotInternal->ferrorAbs[axis] > emcmotStatus->ferrorLimit[axis]) {
	    SET_AXIS_FERROR_FLAG(axis, 1);
	} else {
	    SET_AXIS_FERROR_FLAG(axis, 0);
	}
	/* read and debounce limit switches */
	if (*(axis_data->pos_lim_sw)) {
	    if (emcmotInternal->pos_limit_debounce_cntr[axis] <
		LIMIT_SWITCH_DEBOUNCE) {
		emcmotInternal->pos_limit_debounce_cntr[axis]++;
	    } else {
		SET_AXIS_PHL_FLAG(axis, 1);
	    }
	} else {
	    if (emcmotInternal->pos_limit_debounce_cntr[axis] > 0) {
		emcmotInternal->pos_limit_debounce_cntr[axis]--;
	    } else {
		SET_AXIS_PHL_FLAG(axis, 0);
	    }
	}
	if (*(axis_data->neg_lim_sw)) {
	    if (emcmotInternal->neg_limit_debounce_cntr[axis] <
		LIMIT_SWITCH_DEBOUNCE) {
		emcmotInternal->neg_limit_debounce_cntr[axis]++;
	    } else {
		SET_AXIS_NHL_FLAG(axis, 1);
	    }
	} else {
	    if (emcmotInternal->neg_limit_debounce_cntr[axis] > 0) {
		emcmotInternal->neg_limit_debounce_cntr[axis]--;
	    } else {
		SET_AXIS_NHL_FLAG(axis, 0);
	    }
	}

	/* read and debounce amp fault input */
	if (*(axis_data->amp_fault)) {
	    if (emcmotInternal->amp_fault_debounce_cntr[axis] <
		AMP_FAULT_DEBOUNCE) {
		emcmotInternal->amp_fault_debounce_cntr[axis]++;
	    } else {
		SET_AXIS_FAULT_FLAG(axis, 1);
	    }
	} else {
	    if (emcmotInternal->amp_fault_debounce_cntr[axis] > 0) {
		emcmotInternal->amp_fault_debounce_cntr[axis]--;
	    } else {
		SET_AXIS_FAULT_FLAG(axis, 0);
	    }
	}

	/* read and debounce home switch input */
	if (*(axis_data->home_sw)) {
	    if (emcmotInternal->home_sw_debounce_cntr[axis] <
		HOME_SW_DEBOUNCE) {
		emcmotInternal->home_sw_debounce_cntr[axis]++;
	    } else {
		SET_AXIS_HOME_SWITCH_FLAG(axis, 1);
	    }
	} else {
	    if (emcmotInternal->home_sw_debounce_cntr[axis] > 0) {
		emcmotInternal->home_sw_debounce_cntr[axis]--;
	    } else {
		SET_AXIS_HOME_SWITCH_FLAG(axis, 0);
	    }
	}
	/* end of read and process axis inputs loop */
    }
}

static void check_soft_limits(void)
{
    int axis;
    int tmp;

    /* check for limits on all active axes */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* clear soft limits */
	SET_AXIS_PSL_FLAG(axis, 0);
	SET_AXIS_NSL_FLAG(axis, 0);
	/* skip inactive or unhomed axes */
	if (GET_AXIS_ACTIVE_FLAG(axis) && GET_AXIS_HOMED_FLAG(axis)) {
	    /* check for soft limits */
	    if (emcmotStatus->joint_pos_fb[axis] >
		emcmotConfig->maxLimit[axis]) {
		SET_AXIS_PSL_FLAG(axis, 1);
	    }
	    if (emcmotStatus->joint_pos_fb[axis] <
		emcmotConfig->minLimit[axis]) {
		SET_AXIS_NSL_FLAG(axis, 1);
	    }
	}
    }

    /* this part takes action when the limits are hit - it may eventually be
       moved somewhere else */

    /* check flags, see if any axis is in limit */
    tmp = 0;
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (GET_AXIS_PSL_FLAG(axis) || GET_AXIS_NSL_FLAG(axis)) {
	    /* yes, on limit */
	    tmp = 1;
	}
    }
    /* check for transitions */
    if (tmp) {
	/* on limit now, were we before? */
	if (!emcmotStatus->onSoftLimit) {
	    /* no, update status */
	    emcmotStatus->onSoftLimit = 1;
	    /* abort everything, regardless of coord or free mode */
	    tpAbort(&emcmotDebug->queue);
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpAbort(&emcmotDebug->freeAxis[axis]);
	    }
	}
    } else {
	/* off limit now, were we before? */
	if (emcmotStatus->onSoftLimit) {
	    /* no, update status */
	    emcmotStatus->onSoftLimit = 0;
	}
    }
}

static void check_for_faults(void)
{
    int axis;

    /* check for various fault conditions */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* skip inactive axes */
	if (GET_AXIS_ACTIVE_FLAG(axis)) {
	    /* check for hard limits */
	    if (GET_AXIS_PHL_FLAG(axis) || GET_AXIS_NHL_FLAG(axis)) {
		/* axis is on limit switch, should we trip? */
		if (emcmotStatus->overrideLimits
		    || GET_AXIS_HOMING_FLAG(axis)) {
		    /* no, ignore limits */
		} else {
		    /* trip on limits */
		    if (!GET_AXIS_ERROR_FLAG(axis)) {
			/* report the error just this once */
			reportError("axis %d on limit switch error", axis);
		    }
		    SET_AXIS_ERROR_FLAG(axis, 1);
		    emcmotDebug->enabling = 0;
		}
	    }
	    /* check for amp fault */
	    if (GET_AXIS_FAULT_FLAG(axis)) {
		/* axis is faulted, trip */
		if (!GET_AXIS_ERROR_FLAG(axis)) {
		    /* report the error just this once */
		    reportError("axis %d amplifier fault", axis);
		}
		SET_AXIS_ERROR_FLAG(axis, 1);
		emcmotDebug->enabling = 0;
	    }
	    /* check for excessive following error */
	    if (GET_AXIS_FERROR_FLAG(axis)) {
		if (!GET_AXIS_ERROR_FLAG(axis)) {
		    /* report the error just this once */
		    reportError("axis %d following error", axis);
		}
		SET_AXIS_ERROR_FLAG(axis, 1);
		emcmotDebug->enabling = 0;
	    }
	    /* end of if AXIS_ACTIVE_FLAG(axis) */
	}
	/* end of check for axis faults loop */
    }
}

static void set_operating_mode(void)
{
    int axis;
    axis_hal_t *axis_data;

    /* check for disabling */
    if (!emcmotDebug->enabling && GET_MOTION_ENABLE_FLAG()) {
	/* clear out the motion emcmotDebug->queue and interpolators */
	tpClear(&emcmotDebug->queue);
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    axis_data = &(machine_hal_data->axis[axis]);
	    tpClear(&emcmotDebug->freeAxis[axis]);
	    cubicDrain(&emcmotDebug->cubic[axis]);
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		*(axis_data->amp_enable) = 0;
		SET_AXIS_ENABLE_FLAG(axis, 0);
		SET_AXIS_HOMING_FLAG(axis, 0);
	    }
	    /* don't clear the axis error flag, since that may signify why we
	       just went into disabled state */
	}
	/* reset the trajectory interpolation counter, so that the kinematics
	   functions called during the disabled state run at the nominal
	   trajectory rate rather than the servo rate. It's loaded with
	   emcmotConfig->interpolationRate when it goes to zero. */
/* FIXME - interpolation is still under construction */
#if 0
	interpolationCounter = 0;
#endif
	SET_MOTION_ENABLE_FLAG(0);
	/* don't clear the motion error flag, since that may signify why we
	   just went into disabled state */
    }

    /* check for emcmotDebug->enabling */
    if (emcmotDebug->enabling && !GET_MOTION_ENABLE_FLAG()) {
	tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    axis_data = &(machine_hal_data->axis[axis]);
	    emcmotDebug->freePose.tran.x = emcmotStatus->joint_pos_cmd[axis];
	    tpSetPos(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		*(axis_data->amp_enable) = 1;
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
			    emcmotStatus->joint_pos_cmd[axis];
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
			emcmotStatus->joint_pos_cmd[axis];
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
}

static void get_pos_cmds(void)
{
    int axis;
    static int interpolationCounter = 0;

    /* RUN MOTION CALCULATIONS: */

    /* run axis interpolations and outputs, but only if we're enabled. This
       section is "suppressed" if we're not enabled, although the read/write
       of encoders/dacs is still done. */
#if 0				/* dunno what whichCycle is all about yet */
    whichCycle = 0;
#endif
    if (GET_MOTION_ENABLE_FLAG()) {
	/* set whichCycle to be at least a servo cycle, for calc time logging */
#if 0				/* dunno what whichCycle is all about yet */
	whichCycle = 1;
#endif

	/* check to see if the interpolators are empty */
	while (cubicNeedNextPoint(&emcmotDebug->cubic[0])) {
	    /* they're empty, so pull next point(s) off Cartesian or joint
	       planner, depending upon coord or free mode. */

	    /* check to see whether we're in teleop, coordinated or free
	       mode, to decide which motion planner to call */

	    if (GET_MOTION_TELEOP_FLAG()) {
		/* teleop mode */
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
		    kinematicsForward(emcmotStatus->joint_pos_fb,
			&emcmotStatus->actualPos, &fflags, &iflags);
		} else {
		    /* fake it by setting actual pos to commanded pos */
		    emcmotStatus->actualPos = emcmotStatus->pos;
		}
		/* end of teleop mode */
	    } else if (GET_MOTION_COORD_FLAG()) {
		/* coordinated mode */
		/* we're in coordinated mode-- pull a pose off the Cartesian
		   trajectory planner, run it through the inverse kinematics, 
		   and spline up the joint points for interpolation in servo
		   cycles. */

		/* set whichCycle to be a Cartesian trajectory cycle, for
		   calc time logging */
#if 0				/* dunno what whichCycle is all about yet */
		whichCycle = 2;
#endif

		/* run coordinated trajectory planning cycle */
		tpRunCycle(&emcmotDebug->queue);

		/* set new commanded traj pos */
		emcmotStatus->pos = tpGetPos(&emcmotDebug->queue);

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
		    kinematicsForward(emcmotStatus->joint_pos_fb,
			&emcmotStatus->actualPos, &fflags, &iflags);
		} else {
		    /* fake it by setting actual pos to commanded pos */
		    emcmotStatus->actualPos = emcmotStatus->pos;
		}

		/* now emcmotStatus->actualPos, emcmotStatus->pos, and
		   emcmotDebug->coarseJointPos[] are set */

		/* end of coord mode */
	    } else {
		/* free mode */
		/* we're in free mode-- run joint planning cycles */
		for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		    /* set whichCycle to be a joint trajectory cycle, for
		       calc time logging */
		    /* note that this may include one or more joint
		       trajectory cycles, so calc time may be inherently
		       variable */
#if 0				/* dunno what whichCycle is all about yet */
		    whichCycle = 2;
#endif

		    /* run joint trajectory planning cycle */
		    tpRunCycle(&emcmotDebug->freeAxis[axis]);

		    /* set new coarse joint position. FIXME-- this uses only
		       the tran.x field of the TP_STRUCT, which is overkill.
		       We need a TP_STRUCT with a single scalar element. */
		    emcmotDebug->coarseJointPos[axis] =
			tpGetPos(&emcmotDebug->freeAxis[axis]).tran.x;

		    /* spline joint up-- note that we may be adding a point
		       that fails soft limit, but we'll abort at the end of
		       this cycle so it doesn't really matter */
		    cubicAddPoint(&emcmotDebug->cubic[axis],
			emcmotDebug->coarseJointPos[axis]);
		}		/* end of: axis for loop for joint planning
				   cycle */

		if (kinType == KINEMATICS_IDENTITY) {
		    /* set actualPos from actual inputs */
		    kinematicsForward(emcmotStatus->joint_pos_fb,
			&emcmotStatus->actualPos, &fflags, &iflags);
		    /* set pos from nominal joints, since we're in joint mode 
		     */
		    kinematicsForward(emcmotDebug->coarseJointPos,
			&emcmotStatus->pos, &fflags, &iflags);
		} else if (kinType != KINEMATICS_INVERSE_ONLY) {
		    /* here is where we call the forward kinematics
		       repeatedly, when we're in free mode, so that the world 
		       coordinates are kept up to date when joints are
		       moving. This is only done if we have the kinematics.
		       emcmotStatus->pos needs to be set with an estimate for 
		       the kinematics to converge, which is true when we
		       enter free mode from coordinated mode or after the
		       machine is homed. */
		    EmcPose temp = emcmotStatus->pos;
		    if (0 == kinematicsForward(emcmotStatus->joint_pos_fb,
			    &temp, &fflags, &iflags)) {
			emcmotStatus->pos = temp;
			emcmotStatus->actualPos = temp;
		    } else {
			/* leave them alone */
		    }
		} else {
		    /* no foward kins, and we're in joint mode, so we have no 
		       estimate of world coords, and we have to leave them
		       alone */
		}
		/* now emcmotStatus->actualPos, emcmotStatus->pos, and
		   emcmotDebug->coarseJointPos[] are set */
	    }			/* end of free mode */
	}			/* end of: while (cubicNeedNextPoint(0)) */

	/* at this point, there is data in the interpolators */

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
	   updated via the forward kinematics. FIXME-- make sure this happens, 
	   and note where in this comment. emcmotDebug->coarseJointPos[]
	   contains the results of the joint trajectory calculations at the
	   coarse (trajectory) rate, and the interpolators are not empty. */

	/* run interpolation */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    /* interpolate */
	    emcmotDebug->oldJointPos[axis] =
		emcmotStatus->joint_pos_cmd[axis];
	    emcmotStatus->joint_pos_cmd[axis] =
		cubicInterpolate(&emcmotDebug->cubic[axis], 0, 0, 0, 0);
	    emcmotDebug->jointVel[axis] =
		(emcmotStatus->joint_pos_cmd[axis] -
		emcmotDebug->oldJointPos[axis]) * servo_freq;
	}
	/* end if enabled */
    } else {
	/* 
	   we're not enabled, so no motion planning or interpolation has been
	   done. emcmotStatus->joint_pos_cmd[] is set to
	   emcmotStatus->input[], and likewise with
	   emcmotDebug->coarseJointPos[], which is normally updated at the
	   traj rate but it's convenient to do them here at the same time at
	   the servo rate. emcmotStatus->pos, ->actualPos need to be run
	   through forward kinematics.  Note that we are running at the servo
	   rate, so we need to slow down by the interpolation factor to avoid
	   soaking the CPU. If we were enabled, ->pos was set by calcs (coord
	   mode) or forward kins (free mode), and ->actualPos was set by
	   forward kins on ->input[], all at the trajectory rate. */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    emcmotDebug->coarseJointPos[axis] =
		emcmotStatus->joint_pos_fb[axis];
	    emcmotDebug->oldJointPos[axis] =
		emcmotStatus->joint_pos_cmd[axis];
	    emcmotStatus->joint_pos_cmd[axis] =
		emcmotDebug->coarseJointPos[axis];
	    emcmotDebug->jointVel[axis] =
		(emcmotStatus->joint_pos_cmd[axis] -
		emcmotDebug->oldJointPos[axis]) * servo_freq;
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
		if (0 == kinematicsForward(emcmotStatus->joint_pos_fb, &temp,
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
	/* end of not enabled */
    }
}

static void compute_backlash(void)
{
    int axis;
    axis_hal_t *axis_data;
    double max_delta_pos, dist_to_go;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {

	axis_data = &(machine_hal_data->axis[axis]);
	/* determine which way the compensation should be applied */
	if (emcmotStatus->joint_vel_cmd[axis] > 0.0) {
	    /* moving "up". apply positive backlash comp */
	    /* FIXME - the more sophisticated axisComp should be applied
	       here, if available */
	    axis_data->backlash_corr = 0.5 * emcmotConfig->backlash[axis];
	} else if (emcmotStatus->joint_vel_cmd[axis] < 0.0) {
	    /* moving "down". apply negative backlash comp */
	    /* FIXME - the more sophisticated axisComp should be applied
	       here, if available */
	    axis_data->backlash_corr = -0.5 * emcmotConfig->backlash[axis];
	} else {
	    /* not moving, use whatever was there before */
	}

	/* filter backlash_corr to avoid position steps */
	/* FIXME - this is a linear ramp - an S-ramp would be better because
	   it would limit acceleration */
	max_delta_pos = emcmotConfig->axisLimitVel[axis] * servo_period;
	dist_to_go = axis_data->backlash_corr - axis_data->backlash_filt;
	if (dist_to_go > max_delta_pos) {
	    /* need to go up, can't get there in one jump, take a step */
	    axis_data->backlash_filt += max_delta_pos;
	} else if (dist_to_go < -max_delta_pos) {
	    /* need to go down, can't get there in one jump, take a step */
	    axis_data->backlash_filt -= max_delta_pos;
	} else {
	    /* within one step of final value, go there now */
	    axis_data->backlash_filt = axis_data->backlash_corr;
	}
	/* end of axis loop */
    }
}

static void output_to_hal(void)
{
    int axis;
    axis_hal_t *axis_data;

    /* FIXME - copying these things to the HAL is a pain... I'd rather have
       only one copy (on the HAL side, so they can be scoped).  However, it
       seems that the user space code also looks at them. (maybe - everything 
       including the kitchen sink is in the status struct, but there's no way 
       to tell if any user space code actually uses it) So it looks like I'm
       stuck with duplicates of all these things (and with the annoying
       macros to read and write them, because the user side uses bits packed
       into a word, instead of independent variables. */
    /* output machine info to HAL for scoping, etc */
    machine_hal_data->motion_enable = GET_MOTION_ENABLE_FLAG();
    machine_hal_data->in_position = GET_MOTION_INPOS_FLAG();
    machine_hal_data->coord_mode = GET_MOTION_COORD_FLAG();
    machine_hal_data->teleop_mode = GET_MOTION_TELEOP_FLAG();
    machine_hal_data->coord_error = GET_MOTION_ERROR_FLAG();
    /* output axis info to HAL for scoping, etc */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* point to axis data */
	axis_data = &(machine_hal_data->axis[axis]);
	/* add backlash comp to position command, send to PID */
	axis_data->coarse_pos_cmd = emcmotDebug->coarseJointPos[axis];
	axis_data->joint_pos_cmd = emcmotStatus->joint_pos_cmd[axis];
	axis_data->joint_vel_cmd = emcmotStatus->joint_vel_cmd[axis];
	*(axis_data->motor_pos_cmd) =
	    axis_data->joint_pos_cmd + axis_data->backlash_filt;
	/* output to amp_enable pin */
	*(axis_data->amp_enable) = GET_AXIS_ENABLE_FLAG(axis);
	/* output to parameters (for scoping, etc.) */
	axis_data->active = GET_AXIS_ACTIVE_FLAG(axis);
	axis_data->in_position = GET_AXIS_INPOS_FLAG(axis);
	axis_data->error = GET_AXIS_ERROR_FLAG(axis);
	axis_data->psl = GET_AXIS_PSL_FLAG(axis);
	axis_data->nsl = GET_AXIS_NSL_FLAG(axis);
	axis_data->phl = GET_AXIS_PHL_FLAG(axis);
	axis_data->nhl = GET_AXIS_NHL_FLAG(axis);
	axis_data->home_sw_flag = GET_AXIS_HOME_SWITCH_FLAG(axis);
	axis_data->homing = GET_AXIS_HOMING_FLAG(axis);
	axis_data->homed = GET_AXIS_HOMED_FLAG(axis);
	axis_data->f_errored = GET_AXIS_FERROR_FLAG(axis);
	axis_data->faulted = GET_AXIS_FAULT_FLAG(axis);
	axis_data->f_error = emcmotStatus->ferrorCurrent[axis];
	axis_data->f_error_lim = emcmotStatus->ferrorLimit[axis];

    }
}
