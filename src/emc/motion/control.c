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

    process_inputs();
    check_soft_limits();
    check_for_faults();

/* FIXME - lots more stuff needs to be added here */

/* FIXME - somewhere in here we need to call the TP and kinematics and
           get new position commands
*/

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* calculate joint velocity commands */
	emcmotStatus->joint_vel_cmd[axis] =
	    (emcmotStatus->joint_pos_cmd[axis] -
	    emcmotInternal->old_joint_pos_cmd[axis]) * servo_freq;
	/* update old position */
	emcmotInternal->old_joint_pos_cmd[axis] =
	    emcmotStatus->joint_pos_cmd[axis];
	/* calculate backlash (or leadscrew comp) */
    }

    compute_backlash();
    output_to_hal();

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

    /* check for limits on all active axes */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	/* clear soft limits */
	SET_AXIS_PSL_FLAG(axis, 0);
	SET_AXIS_NSL_FLAG(axis, 0);
	/* skip inactive or unhomed axes */
	if (GET_AXIS_ACTIVE_FLAG(axis) || GET_AXIS_HOMED_FLAG(axis)) {
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
