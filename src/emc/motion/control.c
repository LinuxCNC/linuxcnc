/********************************************************************
* Description: control.c
*   emcmotController() is the main loop running at the servo cycle
*   rate. All state logic and trajectory calcs are called from here.
*
*   Derived from a work by Fred Proctor & Will Shackleford
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

/* 'do_homing()' looks at the home_state field of each joint struct
    to decide what, if anything, needs to be done related to homing
    the joint.  Homing is implemented as a state machine, the exact
    sequence of states depends on the machine configuration.  It
    can be as simple as immediately setting the current position to
    zero, or a it can be a multi-step process (find switch, set
    approximate zero, back off switch, find index, set final zero,
    rapid to home position), or anywhere in between.
*/
static void do_homing(void);

/* 'get_pos_cmds()' generates the position setpoints.  This includes
   calling the trajectory planner and interpolating it's outputs.
   The teleop and coord mode code is copied directly from emc1 for
   now, the free mode code has been re-written.
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
    do_homing();
    get_pos_cmds();
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
    int joint_num, tmp;
    double abs_ferror;
    axis_hal_t *axis_data;
    emcmot_joint_t *joint;

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

    /* read and process per-joint inputs */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to axis HAL data */
	axis_data = &(machine_hal_data->axis[joint_num]);
	/* point to joint data */
	joint = &(emcmotStruct->joints[joint_num]);
	/* copy data from HAL to joint structure */
	joint->motor_pos_fb = *(axis_data->motor_pos_fb);

	/* subtract backlash comp and motor offset */
	joint->pos_fb =
	    joint->motor_pos_fb - (joint->backlash_filt +
	    joint->motor_offset);
	/* calculate following error */
	joint->ferror = joint->pos_cmd - joint->pos_fb;
	abs_ferror = fabs(joint->ferror);
	/* update maximum ferror if needed */
	if (abs_ferror > joint->ferror_high_mark) {
	    joint->ferror_high_mark = abs_ferror;
	}

	/* calculate following error limit */
	if (joint->vel_limit > 0.0) {
	    joint->ferror_limit =
		joint->max_ferror * fabs(joint->vel_cmd) / joint->vel_limit;
	} else {
	    joint->ferror_limit = 0;
	}
	if (joint->ferror_limit < joint->min_ferror) {
	    joint->ferror_limit = joint->min_ferror;
	}
	/* update following error flag */
	if (abs_ferror > joint->ferror_limit) {
	    SET_JOINT_FERROR_FLAG(joint, 1);
	} else {
	    SET_JOINT_FERROR_FLAG(joint, 0);
	}

	/* read and debounce limit switches */
	if (*(axis_data->pos_lim_sw)) {
	    if (joint->pos_limit_debounce < LIMIT_SWITCH_DEBOUNCE) {
		joint->pos_limit_debounce++;
	    } else {
		SET_JOINT_PHL_FLAG(joint, 1);
	    }
	} else {
	    if (joint->pos_limit_debounce > 0) {
		joint->pos_limit_debounce--;
	    } else {
		SET_JOINT_PHL_FLAG(joint, 0);
	    }
	}
	if (*(axis_data->neg_lim_sw)) {
	    if (joint->neg_limit_debounce < LIMIT_SWITCH_DEBOUNCE) {
		joint->neg_limit_debounce++;
	    } else {
		SET_JOINT_NHL_FLAG(joint, 1);
	    }
	} else {
	    if (joint->neg_limit_debounce > 0) {
		joint->neg_limit_debounce--;
	    } else {
		SET_JOINT_NHL_FLAG(joint, 0);
	    }
	}
	/* Some machines can coast past the limit switches - this makes EMC
	   think the machine is no longer on the limit. To avoid this
	   problem, we latch the position when the limit first trips, and we
	   won't release the limit unless the current position is inside the
	   latched position. We use motor position because joint position
	   makes step changes during homing */
	/* latching of limit switches is optional - it should not be used if
	   pos_limit and neg_limit are tied together */
	if (joint->switch_flags & SWITCHES_LATCH_LIMITS) {
	    if (GET_JOINT_PHL_FLAG(joint)) {
		if (!joint->pos_limit_latch) {
		    /* on switch and not latched */
		    joint->pos_limit_latch = 1;
		    joint->pos_limit_pos = joint->motor_pos_fb;
		}
	    } else {
		if (joint->pos_limit_latch &&
		    (joint->motor_pos_fb < joint->pos_limit_pos)) {
		    /* off switch and inside switch position */
		    joint->pos_limit_latch = 0;
		}
	    }
	    if (GET_JOINT_NHL_FLAG(joint)) {
		if (!joint->neg_limit_latch) {
		    /* on switch and not latched */
		    joint->neg_limit_latch = 1;
		    joint->neg_limit_pos = joint->motor_pos_fb;
		}
	    } else {
		if (joint->neg_limit_latch &&
		    (joint->motor_pos_fb > joint->neg_limit_pos)) {
		    /* off switch and inside switch position */
		    joint->neg_limit_latch = 0;
		}
	    }
	} else {
	    /* don't latch, just copy flags */
	    joint->pos_limit_latch = GET_JOINT_PHL_FLAG(joint);
	    joint->neg_limit_latch = GET_JOINT_NHL_FLAG(joint);
	}
	/* read and debounce amp fault input */
	if (*(axis_data->amp_fault)) {
	    if (joint->amp_fault_debounce < AMP_FAULT_DEBOUNCE) {
		joint->amp_fault_debounce++;
	    } else {
		SET_JOINT_FAULT_FLAG(joint, 1);
	    }
	} else {
	    if (joint->amp_fault_debounce > 0) {
		joint->amp_fault_debounce--;
	    } else {
		SET_JOINT_FAULT_FLAG(joint, 0);
	    }
	}

	/* read and debounce home switch input */
	if (*(axis_data->home_sw)) {
	    if (joint->home_sw_debounce < HOME_SW_DEBOUNCE) {
		joint->home_sw_debounce++;
	    } else {
		SET_JOINT_HOME_SWITCH_FLAG(joint, 1);
	    }
	} else {
	    if (joint->home_sw_debounce > 0) {
		joint->home_sw_debounce--;
	    } else {
		SET_JOINT_HOME_SWITCH_FLAG(joint, 0);
	    }
	}

	/* read index pulse from HAL */
	tmp = *(axis_data->index_pulse);
	/* detect rising edge of index pulse */
	if (tmp && !joint->index_pulse) {
	    joint->index_pulse_edge = 1;
	} else {
	    joint->index_pulse_edge = 0;
	}
	joint->index_pulse = tmp;
	/* end of read and process axis inputs loop */
    }
}

static void check_soft_limits(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    int tmp;

    /* check for limits on all active axes */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	joint = &(emcmotStruct->joints[joint_num]);
	/* clear soft limits */
	SET_JOINT_PSL_FLAG(joint, 0);
	SET_JOINT_NSL_FLAG(joint, 0);
	/* skip inactive or unhomed axes */
	if (GET_JOINT_ACTIVE_FLAG(joint) && GET_JOINT_HOMED_FLAG(joint)) {
	    /* check for soft limits */
	    if (joint->pos_fb > joint->max_pos_limit) {
		SET_JOINT_PSL_FLAG(joint, 1);
	    }
	    if (joint->pos_fb < joint->min_pos_limit) {
		SET_JOINT_NSL_FLAG(joint, 1);
	    }
	}
    }

    /* this part takes action when the limits are hit - it may eventually be
       moved somewhere else - if not, it can be pulled into the loop above */

    /* check flags, see if any joint is in limit */
    tmp = 0;
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	joint = &(emcmotStruct->joints[joint_num]);
	if (GET_JOINT_PSL_FLAG(joint) || GET_JOINT_NSL_FLAG(joint)) {
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
	    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		/* point to joint data */
		joint = &(emcmotStruct->joints[joint_num]);
		/* shut off free mode planner */
		joint->free_tp_enable = 0;
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
    int joint_num;
    emcmot_joint_t *joint;

    /* check for various fault conditions */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	joint = &(emcmotStruct->joints[joint_num]);
	/* skip inactive axes */
	if (GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* check for hard limits */
	    if (GET_JOINT_PHL_FLAG(joint) || GET_JOINT_NHL_FLAG(joint)) {
		/* joint is on limit switch, should we trip? */
		if (emcmotStatus->overrideLimits
		    || GET_JOINT_HOMING_FLAG(joint)) {
		    /* no, ignore limits */
		} else {
		    /* trip on limits */
		    if (!GET_JOINT_ERROR_FLAG(joint)) {
			/* report the error just this once */
			reportError("joint %d on limit switch error",
			    joint_num);
		    }
		    SET_JOINT_ERROR_FLAG(joint, 1);
		    emcmotDebug->enabling = 0;
		}
	    }
	    /* check for amp fault */
	    if (GET_JOINT_FAULT_FLAG(joint)) {
		/* axis is faulted, trip */
		if (!GET_JOINT_ERROR_FLAG(joint)) {
		    /* report the error just this once */
		    reportError("joint %d amplifier fault", joint_num);
		}
		SET_JOINT_ERROR_FLAG(joint, 1);
		emcmotDebug->enabling = 0;
	    }
	    /* check for excessive following error */
	    if (GET_JOINT_FERROR_FLAG(joint)) {
		if (!GET_JOINT_ERROR_FLAG(joint)) {
		    /* report the error just this once */
		    reportError("joint %d following error", joint_num);
		}
		SET_JOINT_ERROR_FLAG(joint, 1);
		emcmotDebug->enabling = 0;
	    }
	    /* end of if AXIS_ACTIVE_FLAG(joint) */
	}
	/* end of check for joint faults loop */
    }
}

static void set_operating_mode(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    axis_hal_t *axis_data;

    /* check for disabling */
    if (!emcmotDebug->enabling && GET_MOTION_ENABLE_FLAG()) {
	/* clear out the motion emcmotDebug->queue and interpolators */
	tpClear(&emcmotDebug->queue);
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint data */
	    axis_data = &(machine_hal_data->axis[joint_num]);
	    joint = &(emcmotStruct->joints[joint_num]);
	    /* disable free mode planner */
	    joint->free_tp_enable = 0;
	    /* drain coord mode interpolators */
	    cubicDrain(&(joint->cubic));
	    if (GET_JOINT_ACTIVE_FLAG(joint)) {
		*(axis_data->amp_enable) = 0;
		SET_JOINT_ENABLE_FLAG(joint, 0);
		SET_JOINT_HOMING_FLAG(joint, 0);
		joint->home_state = EMCMOT_NOT_HOMING;
	    }
	    /* don't clear the joint error flag, since that may signify why
	       we just went into disabled state */
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
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint data */
	    axis_data = &(machine_hal_data->axis[joint_num]);
	    joint = &(emcmotStruct->joints[joint_num]);

	    joint->free_pos_cmd = joint->pos_cmd;
	    if (GET_JOINT_ACTIVE_FLAG(joint)) {
		*(axis_data->amp_enable) = 1;
		SET_JOINT_ENABLE_FLAG(joint, 1);
		SET_JOINT_HOMING_FLAG(joint, 0);
		joint->home_state = EMCMOT_NOT_HOMING;
	    }
	    /* clear any outstanding axis errors when going into enabled
	       state */
	    SET_JOINT_ERROR_FLAG(joint, 0);
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
	    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		/* point to joint data */
		joint = &(emcmotStruct->joints[joint_num]);
		cubicDrain(&(joint->cubic));
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
		    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS;
			joint_num++) {
			/* point to joint data */
			joint = &(emcmotStruct->joints[joint_num]);
			/* update free planner positions */
			joint->free_pos_cmd = joint->pos_cmd;
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
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint data */
		    joint = &(emcmotStruct->joints[joint_num]);
		    cubicDrain(&(joint->cubic));
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
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint data */
		    joint = &(emcmotStruct->joints[joint_num]);
		    /* set joint planner pos cmd to current location */
		    joint->free_pos_cmd = joint->pos_cmd;
		    /* but it can stay disabled until a move is required */
		    joint->free_tp_enable = 0;
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

static void do_homing(void)
{
/* this is still very much under construction */
    int joint_num, immediate_state;
    emcmot_joint_t *joint;
    double offset, tmp;

    motion_state_t motion_state;

    /* FIXME - this code is temporary - eventually 'motion_state' will be the
       master for this info, instead of having to gather it from several flags
       - I need to re-write 'set_operating_mode()' because it's ugly */
    if (!GET_MOTION_ENABLE_FLAG()) {
	motion_state = EMCMOT_MOTION_DISABLED;
    } else if (GET_MOTION_TELEOP_FLAG()) {
	motion_state = EMCMOT_MOTION_TELEOP;
    } else if (GET_MOTION_COORD_FLAG()) {
	motion_state = EMCMOT_MOTION_COORD;
    } else {
	motion_state = EMCMOT_MOTION_FREE;
    }

    if (motion_state != EMCMOT_MOTION_FREE) {
	/* can't home unless in free mode */
	return;
    }
    /* loop thru axis, treat each one individually */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint struct */
	joint = &(emcmotStruct->joints[joint_num]);
	/* Some machines have home switches that only trip for a short
	   distance.  Overshoot after the search move could cause the joint
	   to move past the switch, which would mess up the remaining steps
	   of the homing process. To avoid this problem, we latch the
	   position when the switch first trips, and we don't allow the
	   switch to reset if it is beyond that position.  We use motor
	   position because joint position makes step changes during homing */
	if (GET_JOINT_HOME_SWITCH_FLAG(joint)) {
	    if (!joint->home_sw_latch) {
		/* on switch and not latched, latch it */
		joint->home_sw_latch = 1;
		joint->home_sw_pos = joint->motor_pos_fb;
	    }
	} else {
	    if (joint->home_sw_latch) {
		/* off switch but latch is set, can we clear it? */
		if (joint->home_search_vel > 0.0) {
		    if (joint->motor_pos_fb < joint->home_sw_pos) {
			joint->home_sw_latch = 0;
		    }
		} else {
		    if (joint->motor_pos_fb > joint->home_sw_pos) {
			joint->home_sw_latch = 0;
		    }
		}
	    }
	}
	/* when an axis is homing, 'check_for_faults()' ignores its limit
	   switches, so that this code can do the right thing with them.
	   Once the homing process is finished, the 'check_for_faults()'
	   resumes checking */

	/* homing state machine */

	/* Some portions of the homing sequence can run thru two or more
	   states during a single servo period.  This is done using
	   'immediate_state'.  If a state transition sets it true (non-zero),
	   this 'do-while' will loop executing switch(home_state) immediately
	   to run the new state code.  Otherwise, the loop will fall thru, and 
	   switch(home_state) runs only once per servo period. Do _not_ set
	   'immediate_state' true unless you also change 'home_state', unless
	   you want an infinite loop! */
	do {
	    immediate_state = 0;
	    switch (joint->home_state) {
	    case EMCMOT_NOT_HOMING:
		/* nothing to do */
		break;

	    case EMCMOT_HOME_START:
		if (joint->home_search_vel == 0.0) {
		    /* vel == 0 means no switch, home at current position */
		    joint->home_state = EMCMOT_HOME_SET_FINAL_POS;
		    immediate_state = 1;
		} else {
		    /* need to find home switch */
		    if (joint->home_sw_latch) {
			/* already on switch, need to back clear */
			joint->home_state = EMCMOT_HOME_START_PRE_SEARCH;
			immediate_state = 1;
		    } else {
			/* start searching for home switch */
			joint->home_state = EMCMOT_HOME_START_SEARCH;
			immediate_state = 1;
		    }
		}
		break;

	    case EMCMOT_HOME_START_PRE_SEARCH:
		/* move to current location minus joint range */
		offset = joint->max_pos_limit - joint->min_pos_limit;
		if (joint->home_search_vel > 0.0) {
		    joint->free_pos_cmd = joint->pos_cmd - offset;
		} else {
		    joint->free_pos_cmd = joint->pos_cmd + offset;
		}
		joint->free_vel_lim = fabs(joint->home_search_vel);
		joint->free_tp_enable = 1;
		joint->home_state = EMCMOT_HOME_WAIT_PRE_SEARCH;
		break;

	    case EMCMOT_HOME_WAIT_PRE_SEARCH:
		/* wait for off home sw, on limit sw, or end of move */
		if (!joint->home_sw_latch) {
		    /* now off home switch, begin real search */
		    joint->home_state = EMCMOT_HOME_START_SEARCH;
		    immediate_state = 1;
		    break;
		}
		if (joint->pos_limit_latch || joint->neg_limit_latch) {
		    /* on limit, check to see if we should trip */
		    if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			joint->home_state = EMCMOT_HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		if (!joint->free_tp_active) {
		    /* reached end of move without hitting switch */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_ABORT;
		    immediate_state = 1;
		    break;
		}
		break;

	    case EMCMOT_HOME_START_SEARCH:
		/* move to current location plus joint range */
		offset = joint->max_pos_limit - joint->min_pos_limit;
		if (joint->home_search_vel > 0.0) {
		    joint->free_pos_cmd = joint->pos_cmd + offset;
		} else {
		    joint->free_pos_cmd = joint->pos_cmd - offset;
		}
		joint->free_vel_lim = fabs(joint->home_search_vel);
		joint->free_tp_enable = 1;
		joint->home_state = EMCMOT_HOME_WAIT_SEARCH;
		break;

	    case EMCMOT_HOME_WAIT_SEARCH:
		/* wait for home sw, limit sw, or end of move */
		if (joint->home_sw_latch) {
		    /* found switch */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_SET_COARSE_POS;
		    immediate_state = 1;
		    break;
		}
		if (joint->pos_limit_latch || joint->neg_limit_latch) {
		    /* on limit, check to see if we should trip */
		    if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			joint->home_state = EMCMOT_HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		if (!joint->free_tp_active) {
		    /* reached end of move without hitting switch */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_ABORT;
		    immediate_state = 1;
		    break;
		}
		break;

	    case EMCMOT_HOME_SET_COARSE_POS:
		/* set the current position to 'home_offset' */
		offset = joint->home_offset - joint->pos_fb;
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_pos_cmd += offset;
		joint->motor_offset -= offset;
		/* next state */
		tmp = joint->home_search_vel * joint->home_latch_vel;
		if (tmp > 0.0) {
		    /* search and latch vel are same direction */
		    joint->home_state = EMCMOT_HOME_START_BACKOFF;
		} else if (tmp < 0.0) {
		    /* search and latch vel are opposite directions */
		    joint->home_state = EMCMOT_HOME_START_LATCH;
		} else {
		    /* latch vel is zero - error */
		    joint->home_state = EMCMOT_HOME_ABORT;
		}
		immediate_state = 1;
		break;

	    case EMCMOT_HOME_START_BACKOFF:
		/* move to current location minus joint range */
		offset = joint->max_pos_limit - joint->min_pos_limit;
		if (joint->home_latch_vel > 0.0) {
		    joint->free_pos_cmd = joint->pos_cmd - offset;
		} else {
		    joint->free_pos_cmd = joint->pos_cmd + offset;
		}
		joint->free_vel_lim = fabs(joint->home_search_vel);
		joint->free_tp_enable = 1;
		joint->home_state = EMCMOT_HOME_WAIT_BACKOFF;
		break;

	    case EMCMOT_HOME_WAIT_BACKOFF:
		/* wait for off home sw, limit sw, or end of move */
		if (!joint->home_sw_latch) {
		    /* now off switch, ready for latching */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_START_LATCH;
		    immediate_state = 1;
		    break;
		}
		if (joint->pos_limit_latch || joint->neg_limit_latch) {
		    /* on limit, check to see if we should trip */
		    if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			joint->home_state = EMCMOT_HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		if (!joint->free_tp_active) {
		    /* reached end of move without hitting switch */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_ABORT;
		    immediate_state = 1;
		    break;
		}
		break;

	    case EMCMOT_HOME_START_LATCH:
		/* move to current location plus joint range */
		offset = joint->max_pos_limit - joint->min_pos_limit;
		if (joint->home_latch_vel > 0.0) {
		    joint->free_pos_cmd = joint->pos_cmd - offset;
		} else {
		    joint->free_pos_cmd = joint->pos_cmd + offset;
		}
		joint->free_vel_lim = fabs(joint->home_latch_vel);
		joint->free_tp_enable = 1;
		joint->home_state = EMCMOT_HOME_WAIT_LATCH_SWITCH;
		break;

	    case EMCMOT_HOME_WAIT_LATCH_SWITCH:
		/* wait for home sw, limit sw, or end of move */
		if (joint->home_sw_latch) {
		    /* on switch, where do we go next? */
		    if (joint->home_flags & HOME_USE_INDEX) {
			/* look for index pulse */
			joint->free_tp_enable = 0;
			joint->home_state = EMCMOT_HOME_WAIT_LATCH_INDEX;
			immediate_state = 1;
		    } else {
			/* no index pulse, almost done */
			joint->free_tp_enable = 0;
			joint->home_state = EMCMOT_HOME_SET_FINAL_POS;
			immediate_state = 1;
		    }
		    break;
		}
		if (joint->pos_limit_latch || joint->neg_limit_latch) {
		    /* on limit, check to see if we should trip */
		    if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			joint->home_state = EMCMOT_HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		if (!joint->free_tp_active) {
		    /* reached end of move without hitting switch */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_ABORT;
		    immediate_state = 1;
		    break;
		}
		break;

	    case EMCMOT_HOME_WAIT_LATCH_INDEX:
		/* wait for rising edge of index */
		if (joint->index_pulse_edge) {
		    /* index detected */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_SET_FINAL_POS;
		    immediate_state = 1;
		    break;
		}
		if (joint->pos_limit_latch || joint->neg_limit_latch) {
		    /* on limit, check to see if we should trip */
		    if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			joint->home_state = EMCMOT_HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		if (!joint->free_tp_active) {
		    /* reached end of move without hitting switch */
		    joint->free_tp_enable = 0;
		    joint->home_state = EMCMOT_HOME_ABORT;
		    immediate_state = 1;
		    break;
		}
		break;

	    case EMCMOT_HOME_SET_FINAL_POS:
		/* set the current position to 'home_offset' */
		offset = joint->home_offset - joint->pos_fb;
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_pos_cmd += offset;
		joint->motor_offset -= offset;
		/* next state */
		joint->home_state = EMCMOT_HOME_START_FINAL;
		immediate_state = 1;
		break;

	    case EMCMOT_HOME_START_FINAL:
		/* plan a move to home position */
		joint->free_pos_cmd = joint->home;
		/* do the move at max speed */
		/* FIXME - should this be search_vel? or another user
		   specified speed? or is a rapid OK? */
		joint->free_vel_lim = joint->vel_limit;
		joint->free_tp_enable = 1;
		joint->home_state = EMCMOT_HOME_WAIT_FINAL;
		break;

	    case EMCMOT_HOME_WAIT_FINAL:
		/* have we arrived (and stopped) at home? */
		if (!joint->free_tp_active) {
		    /* yes, we're finally done */
		    joint->home_state = EMCMOT_HOME_FINISHED;
		    immediate_state = 1;
		}
		break;

	    case EMCMOT_HOME_FINISHED:
		/* FIXME - this should set various flags, etc not done yet */
		joint->home_state = EMCMOT_NOT_HOMING;
		immediate_state = 1;
		break;

	    case EMCMOT_HOME_ABORT:
		/* FIXME - this should set various flags, etc not done yet */
		joint->free_tp_enable = 0;
		joint->home_state = EMCMOT_NOT_HOMING;
		immediate_state = 1;
		break;

	    default:
		/* should never get here */
		/* FIXME - should set an error flag or something here */
		joint->home_state = EMCMOT_ABORT;
		immediate_state = 1;
		break;
	    }			/* end of switch(joint->home_state) */
	} while (immediate_state);
    }
}

static void get_pos_cmds(void)
{
    int joint_num;
    emcmot_joint_t *joint;
#if 0
    double positions[EMCMOT_MAX_AXIS];
    static int interpolationCounter = 0;
    double old_pos_cmd;
#endif
    motion_state_t motion_state;
    double max_dv, pos_err, vel_req, vel_lim;

    /* RUN MOTION CALCULATIONS: */

    /* FIXME - this code is temporary - eventually 'motion_state' will be the
       master for this info, instead of having to gather it from several flags 
       - I need to re-write 'set_operating_mode()' because it's ugly */
    if (!GET_MOTION_ENABLE_FLAG()) {
	motion_state = EMCMOT_MOTION_DISABLED;
    } else if (GET_MOTION_TELEOP_FLAG()) {
	motion_state = EMCMOT_MOTION_TELEOP;
    } else if (GET_MOTION_COORD_FLAG()) {
	motion_state = EMCMOT_MOTION_COORD;
    } else {
	motion_state = EMCMOT_MOTION_FREE;
    }

    /* run traj planner code depending on the state */
    switch (motion_state) {
    case EMCMOT_MOTION_FREE:
	/* in free mode, each joint is planned independently */
	/* Each joint has a very simple "trajectory planner".  If the planner 
	   is enabled (free_tp_enable), then it moves toward free_pos_cmd at
	   free_vel_lim, obeying the joint's accel and velocity limits, and
	   stopping when it gets there.  If it is not enabled, it stops as
	   quickly as possible, again obeying the accel limit.  When
	   disabled, free_pos_cmd is set to the current position. */
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &(emcmotStruct->joints[joint_num]);
	    joint->free_tp_active = 0;
	    /* compute max change in velocity per servo period */
	    max_dv = joint->acc_limit * servo_period;
	    /* calculate desired velocity */
	    if (joint->free_tp_enable) {
		/* planner enabled, request a velocity that tends to drive
		   pos_err to zero, but allows for stopping without position
		   overshoot */
		pos_err = joint->free_pos_cmd - joint->pos_cmd;
		/* positive and negative errors require some sign flipping to
		   avoid sqrt(negative) */
		if (pos_err > 0.0) {
		    vel_req =
			-max_dv + sqrt(2.0 * joint->acc_limit * pos_err +
			max_dv * max_dv);
		    /* mark joint active */
		    joint->free_tp_active = 1;
		} else if (pos_err < 0.0) {
		    vel_req =
			max_dv - sqrt(-2.0 * joint->acc_limit * pos_err +
			max_dv * max_dv);
		    /* mark joint active */
		    joint->free_tp_active = 1;
		} else {
		    vel_req = 0.0;
		}
	    } else {
		/* planner disabled, request zero velocity */
		vel_req = 0.0;
		/* and set command to present position to avoid movement when 
		   next enabled */
		joint->free_pos_cmd = joint->pos_cmd;
	    }
	    /* velocity limit = planner limit * global scale factor */
	    /* the global factor is used for feedrate override */
	    vel_lim = joint->free_vel_lim * emcmotStatus->qVscale;
	    /* must not be greater than the joint physical limit */
	    if (vel_lim > joint->vel_limit) {
		vel_lim = joint->vel_limit;
	    }
	    /* limit velocity request */
	    if (vel_req > vel_lim) {
		vel_req = vel_lim;
	    } else if (vel_req < -vel_lim) {
		vel_req = -vel_lim;
	    }
	    /* ramp velocity toward request */
	    if (vel_req > joint->vel_cmd + max_dv) {
		joint->vel_cmd += max_dv;
	    } else if (vel_req < joint->vel_cmd - max_dv) {
		joint->vel_cmd -= max_dv;
	    } else {
		joint->vel_cmd = vel_req;
	    }
	    /* check for still moving */
	    if (joint->vel_cmd != 0.0) {
		/* yes, mark joint active */
		joint->free_tp_active = 1;
	    }
	    /* integrate velocity to get new position */
	    joint->pos_cmd += joint->vel_cmd * servo_period;
	}
	break;
    case EMCMOT_MOTION_COORD:
	break;
    case EMCMOT_MOTION_TELEOP:
	break;
    case EMCMOT_MOTION_DISABLED:
    default:
	break;
    }

#if 0
/* FIXME - disables old code */

    /* run axis interpolations and outputs, but only if we're enabled. This
       section is "suppressed" if we're not enabled, although the read/write
       of encoders/dacs is still done. */
#if 0				/* dunno what whichCycle is all about yet */
    whichCycle = 0;
#endif
    if (GET_MOTION_ENABLE_FLAG()) {
	/* set whichCycle to be at least a servo cycle, for calc time logging 
	 */
#if 0				/* dunno what whichCycle is all about yet */
	whichCycle = 1;
#endif

	/* check joint 0 to see if the interpolators are empty */
	while (cubicNeedNextPoint(&(emcmotStruct->joints[0].cubic))) {
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

		/* convert to joint positions in local array */
		kinematicsInverse(&emcmotStatus->pos, positions, &iflags,
		    &fflags);
		/* copy to joint structures and spline them up */
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint struct */
		    joint = &(emcmotStruct->joints[joint_num]);
		    joint->coarse_pos = positions[joint_num];
		    /* spline joints up-- note that we may be adding points
		       that fail soft limits, but we'll abort at the end of
		       this cycle so it doesn't really matter */
		    cubicAddPoint(&(joint->cubic), joint->coarse_pos);
		}

		if (kinType == KINEMATICS_IDENTITY) {
		    /* copy position feedback to local array */
		    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS;
			joint_num++) {
			/* point to joint struct */
			joint = &(emcmotStruct->joints[joint_num]);
			/* copy feedback */
			positions[joint_num] = joint->pos_fb;
		    }
		    /* call forward kinematics on input points for actual
		       pos, at trajectory rate to save bandwidth */
		    kinematicsForward(positions, &emcmotStatus->actualPos,
			&fflags, &iflags);
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

		/* convert to joint positions in local array */
		kinematicsInverse(&emcmotStatus->pos, positions, &iflags,
		    &fflags);
		/* copy to joint structures and spline them up */
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint struct */
		    joint = &(emcmotStruct->joints[joint_num]);
		    joint->coarse_pos = positions[joint_num];
		    /* spline joints up-- note that we may be adding points
		       that fail soft limits, but we'll abort at the end of
		       this cycle so it doesn't really matter */
		    cubicAddPoint(&(joint->cubic), joint->coarse_pos);
		}

		if (kinType == KINEMATICS_IDENTITY) {
		    /* copy position feedback to local array */
		    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS;
			joint_num++) {
			/* point to joint struct */
			joint = &(emcmotStruct->joints[joint_num]);
			/* copy feedback */
			positions[joint_num] = joint->pos_fb;
		    }
		    /* call forward kinematics on input points for actual
		       pos, at trajectory rate to save bandwidth */
		    kinematicsForward(positions, &emcmotStatus->actualPos,
			&fflags, &iflags);
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
		/* position generating code that was here has been replaced */

		if (kinType == KINEMATICS_IDENTITY) {
		    /* copy position feedback to local array */
		    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS;
			joint_num++) {
			/* point to joint struct */
			joint = &(emcmotStruct->joints[joint_num]);
			/* copy feedback */
			positions[joint_num] = joint->pos_fb;
		    }
		    /* set actualPos from actual inputs */
		    kinematicsForward(positions, &emcmotStatus->actualPos,
			&fflags, &iflags);
		    /* copy coarse position commands to local array */
		    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS;
			joint_num++) {
			/* point to joint struct */
			joint = &(emcmotStruct->joints[joint_num]);
			/* copy feedback */
			positions[joint_num] = joint->coarse_pos;
		    }
		    /* set pos from nominal joints, we're in joint mode */
		    kinematicsForward(positions, &emcmotStatus->pos, &fflags,
			&iflags);
		} else if (kinType != KINEMATICS_INVERSE_ONLY) {
		    /* here is where we call the forward kinematics
		       repeatedly, when we're in free mode, so that the world
		       coordinates are kept up to date when joints are moving.
		       This is only done if we have the kinematics.
		       emcmotStatus->pos needs to be set with an estimate for
		       the kinematics to converge, which is true when we enter
		       free mode from coordinated mode or after the machine is
		       homed. */
		    EmcPose temp = emcmotStatus->pos;
		    /* copy position feedback to local array */
		    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS;
			joint_num++) {
			/* point to joint struct */
			joint = &(emcmotStruct->joints[joint_num]);
			/* copy feedback */
			positions[joint_num] = joint->pos_fb;
		    }
		    /* calculate a pose from the actual inputs */
		    if (0 == kinematicsForward(positions, &temp, &fflags,
			    &iflags)) {
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
		   joints[]->coarse_pos are set */
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
	   and note where in this comment. joints[]->coarse_pos[] contains the
	   results of the joint trajectory calculations at the coarse
	   (trajectory) rate, and the interpolators are not empty. */

	/* run interpolation */
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &(emcmotStruct->joints[joint_num]);
	    /* save old command */
	    old_pos_cmd = joint->pos_cmd;
	    /* interpolate to get new one */
	    joint->pos_cmd = cubicInterpolate(&(joint->cubic), 0, 0, 0, 0);
	    joint->vel_cmd = (joint->pos_cmd - old_pos_cmd) * servo_freq;
	}
	/* end if enabled */
    } else {
	/* 
	   we're not enabled, so no motion planning or interpolation has been
	   done. joint->pos_cmd is set to joint->pos_fb, and likewise with
	   joint->coarse_pos, which is normally updated at the traj rate but
	   it's convenient to do them here at the same time at the servo rate.
	   emcmotStatus->pos, ->actualPos need to be run through forward
	   kinematics.  Note that we are running at the servo rate, so we need
	   to slow down by the interpolation factor to avoid soaking the CPU.
	   If we were enabled, ->pos was set by calcs (coord mode) or forward
	   kins (free mode), and ->actualPos was set by forward kins on
	   ->pos_fb, all at the trajectory rate. */
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &(emcmotStruct->joints[joint_num]);

	    joint->coarse_pos = joint->pos_fb;
	    old_pos_cmd = joint->pos_cmd;
	    joint->pos_cmd = joint->coarse_pos;
	    joint->vel_cmd = (joint->pos_cmd - old_pos_cmd) * servo_freq;
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
		/* copy position feedback to local array */
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint struct */
		    joint = &(emcmotStruct->joints[joint_num]);
		    /* copy feedback */
		    positions[joint_num] = joint->pos_fb;
		}
		if (0 == kinematicsForward(positions, &temp, &fflags,
			&iflags)) {
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
#endif
}

static void compute_backlash(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    double max_delta_pos, dist_to_go;

    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint struct */
	joint = &(emcmotStruct->joints[joint_num]);
	/* determine which way the compensation should be applied */
	if (joint->vel_cmd > 0.0) {
	    /* moving "up". apply positive backlash comp */
	    /* FIXME - the more sophisticated axisComp should be applied
	       here, if available */
	    joint->backlash_corr = 0.5 * joint->backlash;
	} else if (joint->vel_cmd < 0.0) {
	    /* moving "down". apply negative backlash comp */
	    /* FIXME - the more sophisticated axisComp should be applied
	       here, if available */
	    joint->backlash_corr = -0.5 * joint->backlash;
	} else {
	    /* not moving, use whatever was there before */
	}

	/* filter backlash_corr to avoid position steps */
	/* FIXME - this is a linear ramp - an S-ramp would be better because
	   it would limit acceleration */
	max_delta_pos = joint->vel_limit * servo_period;
	dist_to_go = joint->backlash_corr - joint->backlash_filt;
	if (dist_to_go > max_delta_pos) {
	    /* need to go up, can't get there in one jump, take a step */
	    joint->backlash_filt += max_delta_pos;
	} else if (dist_to_go < -max_delta_pos) {
	    /* need to go down, can't get there in one jump, take a step */
	    joint->backlash_filt -= max_delta_pos;
	} else {
	    /* within one step of final value, go there now */
	    joint->backlash_filt = joint->backlash_corr;
	}
	/* backlash (and motor offset) will be applied to output later */
	/* end of axis loop */
    }
}

/* FIXME - once the HAL refactor is done so that metadata isn't stored
   in shared memory, I want to seriously consider moving some of the
   structures into the HAL memory block.  This will eliminate most of
   this useless copying, and make nearly everything accessible to
   halscope and halmeter for debugging.
*/

static void output_to_hal(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    axis_hal_t *axis_data;

    /* output machine info to HAL for scoping, etc */
    machine_hal_data->motion_enable = GET_MOTION_ENABLE_FLAG();
    machine_hal_data->in_position = GET_MOTION_INPOS_FLAG();
    machine_hal_data->coord_mode = GET_MOTION_COORD_FLAG();
    machine_hal_data->teleop_mode = GET_MOTION_TELEOP_FLAG();
    machine_hal_data->coord_error = GET_MOTION_ERROR_FLAG();
    /* output axis info to HAL for scoping, etc */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint struct */
	joint = &(emcmotStruct->joints[joint_num]);
	/* apply backlash and motor offset to output */
	joint->motor_pos_cmd =
	    joint->pos_cmd + joint->backlash_filt + joint->motor_offset;
	/* point to HAL data */
	axis_data = &(machine_hal_data->axis[joint_num]);
	/* write to HAL pins */
	*(axis_data->motor_pos_cmd) = joint->motor_pos_cmd;
	*(axis_data->amp_enable) = GET_JOINT_ENABLE_FLAG(joint);
	/* output to parameters (for scoping, etc.) */
	axis_data->coarse_pos_cmd = joint->coarse_pos;
	axis_data->joint_pos_cmd = joint->pos_cmd;
	axis_data->joint_vel_cmd = joint->vel_cmd;
	axis_data->backlash_corr = joint->backlash_corr;
	axis_data->backlash_filt = joint->backlash_filt;
	axis_data->joint_pos_fb = joint->pos_fb;
	axis_data->f_error = joint->ferror;
	axis_data->f_error_lim = joint->ferror_limit;

	axis_data->free_pos_cmd = joint->free_pos_cmd;
	axis_data->free_vel_lim = joint->free_vel_lim;
	axis_data->free_tp_enable = joint->free_tp_enable;

	axis_data->active = GET_JOINT_ACTIVE_FLAG(joint);
	axis_data->in_position = GET_JOINT_INPOS_FLAG(joint);
	axis_data->error = GET_JOINT_ERROR_FLAG(joint);
	axis_data->psl = GET_JOINT_PSL_FLAG(joint);
	axis_data->nsl = GET_JOINT_NSL_FLAG(joint);
	axis_data->phl = GET_JOINT_PHL_FLAG(joint);
	axis_data->nhl = GET_JOINT_NHL_FLAG(joint);
	axis_data->home_sw_flag = GET_JOINT_HOME_SWITCH_FLAG(joint);
	axis_data->homing = GET_JOINT_HOMING_FLAG(joint);
	axis_data->homed = GET_JOINT_HOMED_FLAG(joint);
	axis_data->f_errored = GET_JOINT_FERROR_FLAG(joint);
	axis_data->faulted = GET_JOINT_FAULT_FLAG(joint);
    }
}
