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
********************************************************************/

#include "posemath.h"
#include "rtapi.h"
#include "hal.h"
#include "emcmotglb.h"
#include "motion.h"
#include "mot_priv.h"
#include "rtapi_math.h"

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/*! \todo FIXME - this is a leftover global, it will eventually go away */
int rehomeAll;

/* these variables have the servo cycle time and 1/cycle time */
static double servo_period;
static double servo_freq;


/* debugging function - prints a cartesean pose (multplies the floating
   point numbers by 1 million since kernel printf doesn't handle floats
*/

void print_pose ( EmcPose *pos )
{
    rtapi_print(" (%d, %d, %d):(%d, %d, %d) ",
	(int)(pos->tran.x*1000000.0),
	(int)(pos->tran.y*1000000.0),
	(int)(pos->tran.z*1000000.0),
	(int)(pos->a*1000000.0),
	(int)(pos->b*1000000.0),
	(int)(pos->c*1000000.0) );
}


/*! \todo FIXME - debugging - uncomment the following line to log changes in
   AXIS_FLAG and MOTION_FLAG */
// #define WATCH_FLAGS 1


/* debugging function - it watches a particular variable and
   prints a message when the value changes.  Right now there are
   calls to this scattered throughout this and other files.
   To disable them, comment out the following define:
*/
// #define ENABLE_CHECK_STUFF

#ifdef ENABLE_CHECK_STUFF
void check_stuff(char *location)
{
   static char *target, old = 0xFF;
/*! \todo Another #if 0 */
#if 0
/* kludge to look at emcmotDebug->enabling and emcmotStatus->motionFlag
   at the same time - we simply use a high bit of the flags to
   hold "enabling" */
   short tmp;
   if ( emcmotDebug->enabling )
     tmp = 0x1000;
   else
     tmp = 0x0;
   tmp |= emcmotStatus->motionFlag;
   target = &tmp;
/* end of kluge */
#endif

    target = (emcmot_hal_data->enable);
    if ( old != *target ) {
	rtapi_print ( "%d: watch value %02X (%s)\n", emcmotStatus->heartbeat, *target, location );
	old = *target;
    }
}
#else /* make it disappear */
void check_stuff(char *location)
{
/* do nothing (I wonder if gcc is smart
   enough to optimize the calls away?) */
}
#endif /* ENABLE_CHECK_STUFF */


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
   the case of position feedback, that means removing backlash or
   screw error comp and calculating the following error.  For 
   switches, it means debouncing them and setting flags in the
   emcmotStatus structure.
*/
static void process_inputs(void);

/* 'do forward kins()' takes the position feedback in joint coords
   and applies the forward kinematics to it to generate feedback
   in Cartesean coordinates.  It has code to handle machines that
   don't have forward kins, and other special cases, such as when
   the joints have not been homed.
*/
static void do_forward_kins(void);

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

/* 'handle_jogwheels()' reads jogwheels, decides if they should be
   enabled, and if so, changes the free mode planner's target position
   when the jogwheel(s) turn.
*/
static void handle_jogwheels(void);

/* 'do_homing_sequence()' looks at emcmotStatus->homingSequenceState 
   to decide what, if anything, needs to be done related to multi-joint
   homing.
*/

static void do_homing_sequence(void);

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

static void get_pos_cmds(long period);

/* 'compute_screw_comp()' is responsible for calculating backlash and
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
static void compute_screw_comp(void);

/* 'output_to_hal()' writes the handles the final stages of the
   control function.  It applies screw comp and writes the
   final motor position to the HAL (which routes it to the PID
   loop).  It also drives other HAL outputs, and it writes a
   number of internal variables to HAL parameters so they can
   be observed with halscope and halmeter.
*/
static void output_to_hal(void);

/* 'update_status()' copies assorted status information to shared
   memory (the emcmotStatus structure) so that it is available to
   higher level code.
*/
static void update_status(void);


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
#ifdef RTAPI
    // - overrun detection -
    // maintain some records of how long it's been between calls.  The
    // first time we see a delay that's much longer than the records show
    // is normal, report an error.  This might detect bogus realtime 
    // performance caused by ACPI, onboard video, etc.  It can be reliably
    // triggered by maximizing glxgears on my nvidia system, which also
    // causes the rtai latency test to show overruns.

    // check below if you set this under 5
#define CYCLE_HISTORY 5

    static long int cycles[CYCLE_HISTORY];
    static long long int last = 0;

    static int index = 0, priming = 1;

    long long int now = rtapi_get_clocks();
    long int this_run = (long int)(now - last);
    emcmot_hal_data->last_period = this_run;
#ifdef HAVE_CPU_KHZ
    emcmot_hal_data->last_period_ns = this_run * 1e6 / cpu_khz;
#endif

#ifndef RTAPI_SIM
    if(!priming) {
        // we have CYCLE_HISTORY samples, so check for this call being 
        // anomolously late
        int i;

        for(i=0; i<CYCLE_HISTORY; i++) {
            if (this_run > 1.2 * cycles[i]) {
                emcmot_hal_data->overruns++;
		// print message on first overrun only
	        if(emcmot_hal_data->overruns == 1) {
	            reportError("Unexpected realtime delay; check dmesg for details.");
	            rtapi_print_msg(RTAPI_MSG_ERR, 
	                "\nIn recent history there were\n"); 
	            rtapi_print_msg(RTAPI_MSG_ERR, "%ld, %ld, %ld, %ld, and %ld\n",
	                cycles[0], cycles[1], cycles[2], cycles[3], cycles[4]);
	            rtapi_print_msg(RTAPI_MSG_ERR, 
	                "elapsed clocks between calls to the motion controller.\n");
	            rtapi_print_msg(RTAPI_MSG_ERR, 
	                "This time, there were %ld which is so anomalously\n",
	                this_run);
	            rtapi_print_msg(RTAPI_MSG_ERR, 
	                "large that it probably signifies a problem with your\n");
	            rtapi_print_msg(RTAPI_MSG_ERR, 
	                "realtime configuration.  For the rest of this run of\n");
	            rtapi_print_msg(RTAPI_MSG_ERR,
	                "EMC, this message will be suppressed.\n\n");
		}
		break;
	    }
        }
    }
#endif
    if(last) {
        cycles[index++] = this_run;
    }
    if(index == CYCLE_HISTORY) {
        // wrap around to the start of the array
        index = 0;
        // we now have CYCLE_HISTORY good samples, so start checking times
        priming = 0;
    }
    // we need this for next time
    last = now;

    // end of overrun detection
#endif

    /* calculate servo period as a double - period is in integer nsec */
    servo_period = period * 0.000000001;
    /* calculate servo frequency for calcs like vel = Dpos / period */
    /* it's faster to do vel = Dpos * freq */
    servo_freq = 1.0 / servo_period;
    /* increment head count to indicate work in progress */
    emcmotStatus->head++;
    /* here begins the core of the controller */

check_stuff ( "before process_inputs()" );
    process_inputs();
check_stuff ( "after process_inputs()" );
    do_forward_kins();
check_stuff ( "after do_forward_kins()" );
    check_soft_limits();
check_stuff ( "after check_soft_limits()" );
    check_for_faults();
check_stuff ( "after check_for_faults()" );
    set_operating_mode();
check_stuff ( "after set_operating_mode()" );
    handle_jogwheels();
check_stuff ( "after handle_jogwheels()" );
    do_homing_sequence();
check_stuff ( "after do_homing_sequence()" );
    do_homing();
check_stuff ( "after do_homing()" );
    get_pos_cmds(period);
check_stuff ( "after get_pos_cmds()" );
    compute_screw_comp();
check_stuff ( "after compute_screw_comp()" );
    output_to_hal();
check_stuff ( "after output_to_hal()" );
    update_status();
check_stuff ( "after update_status()" );
    /* here ends the core of the controller */
    emcmotStatus->heartbeat++;
    /* set tail to head, to indicate work complete */
    emcmotStatus->tail = emcmotStatus->head;
    /* clear init flag */
    first_pass = 0;

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
    int joint_num;
    double abs_ferror, tmp, scale;
    axis_hal_t *axis_data;
    emcmot_joint_t *joint;
    unsigned char enables;

    /* read probe input */
    if (*(emcmot_hal_data->probe_input)) {
	emcmotStatus->probeVal = 1;
    } else {
	emcmotStatus->probeVal = 0;
    }
    /* check if the probe has been tripped */
    if (emcmotStatus->probing && emcmotStatus->probeTripped ) {
	tpClear(&emcmotDebug->queue);
	emcmotStatus->probing=0;
    /* check if the probe hasn't tripped, but the move finished */
    } else if (emcmotStatus->probing && GET_MOTION_INPOS_FLAG() && 
	    ( tpQueueDepth(&emcmotDebug->queue) == 0 ) ) {
	emcmotStatus->probing=0;
    /* check if the probe trips */
    } else if (emcmotStatus->probing) {
	/* if the probe tripped (val == 1 ) */
	if (emcmotStatus->probeVal) {
	    emcmotStatus->probeTripped = 1; //we have tripped the probe
	    emcmotStatus->probedPos = emcmotStatus->carte_pos_fb; //remember the current position
	    if (GET_MOTION_COORD_FLAG()) {
		tpAbort(&emcmotDebug->queue); //if we are in coordinated move, abort
		SET_MOTION_ERROR_FLAG(0);     //and clear any error flag which might have been set by the abort
	    } else { //free mode, abort any joint
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint struct */
		    joint = &joints[joint_num];
		    /* tell joint planner to stop */
		    joint->free_tp_enable = 0;
		    /* stop homing if in progress */
		    if ( joint->home_state != HOME_IDLE ) {
			joint->home_state = HOME_ABORT;
		    }
		}
	    }
	}
    }
    /* read spindle angle (for threading, etc) */
    emcmotStatus->spindleRevs = *emcmot_hal_data->spindle_revs;
    /* compute net feed and spindle scale factors */
    if ( emcmotStatus->motion_state == EMCMOT_MOTION_COORD ) {
	/* use the enables that were queued with the current move */
	enables = emcmotStatus->enables_queued;
    } else {
	/* use the enables that are in effect right now */
	enables = emcmotStatus->enables_new;
    }
    /* feed scaling first:  feed_scale, adaptive_feed, and feed_hold */
    scale = 1.0;
    if ( enables & FS_ENABLED ) {
	scale *= emcmotStatus->feed_scale;
    }
    if ( enables & AF_ENABLED ) {
	/* read and clamp (0.0 to 1.0) adaptive feed HAL pin */
	tmp = *emcmot_hal_data->adaptive_feed;
	if ( tmp > 1.0 ) {
	    tmp = 1.0;
	} else if ( tmp < 0.0 ) {
	    tmp = 0.0;
	}
	scale *= tmp;
    }
    if ( enables & FH_ENABLED ) {
	/* read feed hold HAL pin */
	if ( *emcmot_hal_data->feed_hold ) {
	    scale = 0;
	}
    }
    /* save the resulting combined scale factor */
    emcmotStatus->net_feed_scale = scale;

    /* now do spindle scaling: only one item to consider */
    scale = 1.0;
    if ( enables & SS_ENABLED ) {
	scale *= emcmotStatus->spindle_scale;
    }
    /* save the resulting combined scale factor */
    emcmotStatus->net_spindle_scale = scale;

    /* read and process per-joint inputs */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to axis HAL data */
	axis_data = &(emcmot_hal_data->axis[joint_num]);
	/* point to joint data */
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, skip it */
	    continue;
	}
	/* copy data from HAL to joint structure */
	joint->index_enable = *(axis_data->index_enable);
	joint->motor_pos_fb = *(axis_data->motor_pos_fb);
	/* calculate pos_fb */
	if (( joint->home_state == HOME_INDEX_SEARCH_WAIT ) &&
	    ( joint->index_enable == 0 )) {
	    /* special case - we're homing the joint, and it just
	       hit the index.  The encoder count might have made a
	       step change.  The homing code will correct for it
	       later, so we ignore motor_pos_fb and set pos_fb
	       to match the commanded value instead. */
	    joint->pos_fb = joint->pos_cmd;
	} else {
	    /* normal case: subtract backlash comp and motor offset */
	    joint->pos_fb = joint->motor_pos_fb -
		(joint->backlash_filt + joint->motor_offset);
	}
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

	/* read limit switches */
	if (*(axis_data->pos_lim_sw)) {
	    SET_JOINT_PHL_FLAG(joint, 1);
	} else {
	    SET_JOINT_PHL_FLAG(joint, 0);
	}
	if (*(axis_data->neg_lim_sw)) {
	    SET_JOINT_NHL_FLAG(joint, 1);
	} else {
	    SET_JOINT_NHL_FLAG(joint, 0);
	}
	joint->on_pos_limit = GET_JOINT_PHL_FLAG(joint);
	joint->on_neg_limit = GET_JOINT_NHL_FLAG(joint);
	/* read amp fault input */
	if (*(axis_data->amp_fault)) {
	    SET_JOINT_FAULT_FLAG(joint, 1);
	} else {
	    SET_JOINT_FAULT_FLAG(joint, 0);
	}

	/* read home switch input */
	if (*(axis_data->home_sw)) {
	    SET_JOINT_HOME_SWITCH_FLAG(joint, 1);
	} else {
	    SET_JOINT_HOME_SWITCH_FLAG(joint, 0);
	}
	/* end of read and process axis inputs loop */
    }
}

static void do_forward_kins(void)
{
/* there are four possibilities for kinType:

   IDENTITY: Both forward and inverse kins are available, and they
   can used without an initial guess, even if one or more joints
   are not homed.  In this case, we apply the forward kins to the
   joint->pos_fb to produce carte_pos_fb, and if all axes are homed
   we set carte_pos_fb_ok to 1 to indicate that the feedback data
   is good.

   BOTH: Both forward and inverse kins are available, but the forward
   kins need an initial guess, and/or the kins require all joints to
   be homed before they work properly.  Here we must tread carefully.
   IF all the joints have been homed, we apply the forward kins to
   the joint->pos_fb to produce carte_pos_fb, and set carte_pos_fb_ok
   to indicate that the feedback is good.  We use the previous value
   of carte_pos_fb as the initial guess.  If all joints have not been
   homed, we don't call the kinematics, instead we set carte_pos_fb to
   the cartesean coordinates of home, as stored in the global worldHome,
   and we set carte_fb_ok to 0 to indicate that the feedback is invalid.
\todo  FIXME - maybe setting to home isn't the right thing to do.  We need
   it to be set to home eventually, (right before the first attemt to
   run the kins), but that doesn't mean we should say we're at home
   when we're not.

   INVERSE_ONLY: Only inverse kinematics are available, forward
   kinematics cannot be used.  So we have to fake it, the question is
   simply "what way of faking it is best".  In free mode, or if all
   axes have not been homed, the feedback position is unknown.  If
   we are in teleop or coord mode, or if we are in free mode and all
   axes are homed, and haven't been moved since they were homed, then
   we set carte_pos_fb to carte_pos_cmd, and set carte_pos_fb_ok to 1.
   If we are in free mode, and any axis is not homed, or any axis has
   moved since it was homed, we leave cart_pos_fb alone, and set
   carte_pos_fb_ok to 0.

   FORWARD_ONLY: Only forward kinematics are available, inverse kins
   cannot be used.  This exists for completeness only, since EMC won't
   work without inverse kinematics.

*/

/*! \todo FIXME FIXME FIXME - need to put a rate divider in here, run it
   at the traj rate */

    double joint_pos[EMCMOT_MAX_AXIS];
    int joint_num, all_homed, all_at_home, result;
    emcmot_joint_t *joint;

    all_homed = 1;
    all_at_home = 1;
    /* copy joint position feedback to local array */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint struct */
	joint = &joints[joint_num];
	/* copy feedback */
	joint_pos[joint_num] = joint->pos_fb;
	/* check for homed, only if the current joint is active */
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, don't even look at its limits */
	    continue;
	}

	if (!GET_JOINT_HOMED_FLAG(joint)) {
	    all_homed = 0;
	    all_at_home = 0;
	} else if (!GET_JOINT_AT_HOME_FLAG(joint)) {
	    all_at_home = 0;
	}
    }
    switch (kinType) {

    case KINEMATICS_IDENTITY:
	kinematicsForward(joint_pos, &emcmotStatus->carte_pos_fb, &fflags,
	    &iflags);
	if (checkAllHomed()) {
	    emcmotStatus->carte_pos_fb_ok = 1;
	} else {
	    emcmotStatus->carte_pos_fb_ok = 0;
	}
	break;

    case KINEMATICS_BOTH:
	if (checkAllHomed()) {
	    /* is previous value suitable for use as initial guess? */
	    if (!emcmotStatus->carte_pos_fb_ok) {
		/* no, use home position as initial guess */
		emcmotStatus->carte_pos_fb = emcmotStatus->world_home;
	    }
	    /* calculate Cartesean position feedback from joint pos fb */
	    result =
		kinematicsForward(joint_pos, &emcmotStatus->carte_pos_fb,
		&fflags, &iflags);
	    /* check to make sure kinematics converged */
	    if (result < 0) {
		/* error during kinematics calculations */
		emcmotStatus->carte_pos_fb_ok = 0;
	    } else {
		/* it worked! */
		emcmotStatus->carte_pos_fb_ok = 1;
	    }
	} else {
	    emcmotStatus->carte_pos_fb_ok = 0;
	}
	break;

    case KINEMATICS_INVERSE_ONLY:

	if ((GET_MOTION_COORD_FLAG()) || (GET_MOTION_TELEOP_FLAG())) {
	    /* use Cartesean position command as feedback value */
	    emcmotStatus->carte_pos_fb = emcmotStatus->carte_pos_cmd;
	    emcmotStatus->carte_pos_fb_ok = 1;
	} else {
	    emcmotStatus->carte_pos_fb_ok = 0;
	}
	break;

    default:
	emcmotStatus->carte_pos_fb_ok = 0;
	break;
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
	joint = &joints[joint_num];
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
	joint = &joints[joint_num];
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
		joint = &joints[joint_num];
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
    
    /* check for various global fault conditions */
    /* only check enable input if running */
    if ( GET_MOTION_ENABLE_FLAG() != 0 ) {
	if ( *(emcmot_hal_data->enable) == 0 ) {
	    reportError("motion stopped by enable input");
	    emcmotDebug->enabling = 0;
	}
    }
    /* check for various joint fault conditions */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	joint = &joints[joint_num];
	/* only check active, enabled axes */
	if ( GET_JOINT_ACTIVE_FLAG(joint) && GET_JOINT_ENABLE_FLAG(joint) ) {
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
	    axis_data = &(emcmot_hal_data->axis[joint_num]);
	    joint = &joints[joint_num];
	    /* disable free mode planner */
	    joint->free_tp_enable = 0;
	    /* drain coord mode interpolators */
	    cubicDrain(&(joint->cubic));
	    if (GET_JOINT_ACTIVE_FLAG(joint)) {
		SET_JOINT_ENABLE_FLAG(joint, 0);
		SET_JOINT_HOMING_FLAG(joint, 0);
		joint->home_state = HOME_IDLE;
	    }
	    /* don't clear the joint error flag, since that may signify why
	       we just went into disabled state */
	}
	/* reset the trajectory interpolation counter, so that the kinematics
	   functions called during the disabled state run at the nominal
	   trajectory rate rather than the servo rate. It's loaded with
	   emcmotConfig->interpolationRate when it goes to zero. */
/*! \todo FIXME - interpolation is still under construction */
/*! \todo Another #if 0 */
#if 0
	interpolationCounter = 0;
#endif
	SET_MOTION_ENABLE_FLAG(0);
	/* don't clear the motion error flag, since that may signify why we
	   just went into disabled state */
    }

    /* check for emcmotDebug->enabling */
    if (emcmotDebug->enabling && !GET_MOTION_ENABLE_FLAG()) {
	tpSetPos(&emcmotDebug->queue, emcmotStatus->carte_pos_cmd);
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint data */
	    axis_data = &(emcmot_hal_data->axis[joint_num]);
	    joint = &joints[joint_num];

	    joint->free_pos_cmd = joint->pos_cmd;
	    if (GET_JOINT_ACTIVE_FLAG(joint)) {
		SET_JOINT_ENABLE_FLAG(joint, 1);
		SET_JOINT_HOMING_FLAG(joint, 0);
		joint->home_state = HOME_IDLE;
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
	    tpSetPos(&emcmotDebug->queue, emcmotStatus->carte_pos_cmd);
	    /* drain the cubics so they'll synch up */
	    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		/* point to joint data */
		joint = &joints[joint_num];
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
			joint = &joints[joint_num];
			/* update free planner positions */
			joint->free_pos_cmd = joint->pos_cmd;
		    }
		}
	    }
	}

	/* check for entering coordinated mode */
	if (emcmotDebug->coordinating && !GET_MOTION_COORD_FLAG()) {
	    if (GET_MOTION_INPOS_FLAG()) {
		/* preset traj planner to current position */
		tpSetPos(&emcmotDebug->queue, emcmotStatus->carte_pos_cmd);
		/* drain the cubics so they'll synch up */
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint data */
		    joint = &joints[joint_num];
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
		    joint = &joints[joint_num];
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
    /*! \todo FIXME - this code is temporary - eventually this function will be
       cleaned up and simplified, and 'motion_state' will become the master
       for this info, instead of having to gather it from several flags */
    if (!GET_MOTION_ENABLE_FLAG()) {
	emcmotStatus->motion_state = EMCMOT_MOTION_DISABLED;
    } else if (GET_MOTION_TELEOP_FLAG()) {
	emcmotStatus->motion_state = EMCMOT_MOTION_TELEOP;
    } else if (GET_MOTION_COORD_FLAG()) {
	emcmotStatus->motion_state = EMCMOT_MOTION_COORD;
    } else {
	emcmotStatus->motion_state = EMCMOT_MOTION_FREE;
    }
}

static void handle_jogwheels(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    axis_hal_t *axis_data;
    int new_jog_counts, delta;
    double distance, pos;

    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	axis_data = &(emcmot_hal_data->axis[joint_num]);
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, skip it */
	    continue;
	}
	/* get counts from jogwheel */
	new_jog_counts = *(axis_data->jog_counts);
	delta = new_jog_counts - joint->old_jog_counts;
	/* save value for next time */
	joint->old_jog_counts = new_jog_counts;
	/* did the wheel move? */
	if ( delta == 0 ) {
	    /* no, nothing to do */
	    continue;
	}
	/* must be in free mode and enabled */
	if (GET_MOTION_COORD_FLAG()) {
	    continue;
	}
	if (!GET_MOTION_ENABLE_FLAG()) {
	    continue;
	}
	if ( first_pass ) {
	    continue;
	}
	/* the jogwheel input for this axis must be enabled */
	if ( *(axis_data->jog_enable) == 0 ) {
	    continue;
	}
	/* calculate distance to jog */
	distance = delta * *(axis_data->jog_scale);
	/* check for joint already on soft limit */
	if (distance > 0.0 && GET_JOINT_PSL_FLAG(joint)) {
	    continue;
	}
	if (distance < 0.0 && GET_JOINT_NSL_FLAG(joint)) {
	    continue;
	}
	/* check for joint already on hard limit */
	if (distance > 0.0 && GET_JOINT_PHL_FLAG(joint)) {
	    continue;
	}
	if (distance < 0.0 && GET_JOINT_NHL_FLAG(joint)) {
	    continue;
	}
	/* calc target position for jog */
	pos = joint->free_pos_cmd + distance;
	/* don't jog past limits */
	refresh_jog_limits(joint);
	if (pos > joint->max_jog_limit) {
	    continue;
	}
	if (pos < joint->min_jog_limit) {
	    continue;
	}
	/* set target position */
	joint->free_pos_cmd = pos;
	/* set velocity of jog */
	joint->free_vel_lim = joint->vel_limit;
	/* and let it go */
	joint->free_tp_enable = 1;
	SET_JOINT_ERROR_FLAG(joint, 0);
	/* clear axis homed flag(s) if we don't have forward kins.
	   Otherwise, a transition into coordinated mode will incorrectly
	   assume the homed position. Do all if they've all been moved
	   since homing, otherwise just do this one */
	clearHomes(joint_num);
    }
}

/* Beginning of homing related code */

/* Length of delay between homing motions - this is intended to
   ensure that all motion has ceased and switch bouncing has
   ended.  We might want to make this user adjustable, but for
   now it's a constant.  It is in seconds */
#define HOME_DELAY 0.100

/* variable used internally by do_homing, but global so that
   'home_do_moving_checks()' can access it */
static int immediate_state;

/* a couple of helper functions with code that would otherwise be
   repeated in several different states of the homing state machine */

/* 'home_start_move()' starts a move at the specified velocity.  The
   length of the move is equal to twice the overall range of the axis,
   but the intent is that something (like a home switch or index pulse)
   will stop it before that point. */
void home_start_move(emcmot_joint_t * joint, double vel)
{
    double axis_range;

    /* set up a long move */
    axis_range = joint->max_pos_limit - joint->min_pos_limit;
    if (vel > 0.0) {
	joint->free_pos_cmd = joint->pos_cmd + 2.0 * axis_range;
    } else {
	joint->free_pos_cmd = joint->pos_cmd - 2.0 * axis_range;
    }
    joint->free_vel_lim = fabs(vel);
    /* start the move */
    joint->free_tp_enable = 1;
}

/* 'home_do_moving_checks()' is called from states where the machine
   is supposed to be moving.  It checks to see if the machine has
   hit a limit, or if the move has stopped.  (Normally such moves
   will be terminated by the home switch or an index pulse or some
   other event, if the move goes to completion, something is wrong.) */
static void home_do_moving_checks(emcmot_joint_t * joint)
{
    /* check for limit switches */
    if (joint->on_pos_limit || joint->on_neg_limit) {
	/* on limit, check to see if we should trip */
	if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
	    /* not ignoring limits, time to quit */
	    reportError("hit limit in home state %d", joint->home_state);
	    joint->home_state = HOME_ABORT;
	    immediate_state = 1;
	    return;
	}
    }
    /* check for reached end of move */
    if (!joint->free_tp_active) {
	/* reached end of move without hitting switch */
	joint->free_tp_enable = 0;
	reportError("end of move in home state %d", joint->home_state);
	joint->home_state = HOME_ABORT;
	immediate_state = 1;
	return;
    }
}

static void do_homing_sequence(void) {
    static int home_sequence = -1;
    int i;
    int seen = 0;
    if(home_sequence == -1) {
        emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE;
	home_sequence = 0;
    }
    switch(emcmotStatus->homingSequenceState) {
    case HOME_SEQUENCE_IDLE:
	/* nothing to do */
	break;

    case HOME_SEQUENCE_START:
	for(i=0; i < EMCMOT_MAX_AXIS; i++) {
	    emcmot_joint_t *joint = &joints[i];
	    if(joint->home_state != HOME_IDLE) {
		emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE; return;
	    }
	    home_sequence = 0;
	}
    
    case HOME_SEQUENCE_START_JOINTS:
	for(i=0; i < EMCMOT_MAX_AXIS; i++) {
	    emcmot_joint_t *joint = &joints[i];
	    int j = joint->home_sequence;
	    if(j == home_sequence) {
	        joint->free_tp_enable = 0;
		joint->home_state = HOME_START;
		seen++;
	    }
	}
	if(seen) 
	    emcmotStatus->homingSequenceState = HOME_SEQUENCE_WAIT_JOINTS;
	else
	    emcmotStatus->homingSequenceState = HOME_IDLE;
	break;

    case HOME_SEQUENCE_WAIT_JOINTS:
	for(i=0; i < EMCMOT_MAX_AXIS; i++) {
	    emcmot_joint_t *joint = &joints[i];
	    int j = joint->home_sequence;
	    if(j != home_sequence) continue;
	    if(joint->home_state != HOME_IDLE) {
		seen = 1;
		continue;
	    }
	    if(!GET_JOINT_AT_HOME_FLAG(joint)) {
		emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE; return;
	    }
	}
	if(!seen) {
	    home_sequence ++;
	    emcmotStatus->homingSequenceState = HOME_SEQUENCE_START_JOINTS;
	}
	break;
    default:
	/* should never get here */
	reportError("unknown state '%d' during homing sequence",
	    emcmotStatus->homingSequenceState);
	emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE;
	break;
    }
}

static void do_homing(void)
{
/* this is still very much under construction */
    int joint_num;
    emcmot_joint_t *joint;
    double offset, tmp;
    int home_sw_new, home_sw_rise, home_sw_fall;

    if (emcmotStatus->motion_state != EMCMOT_MOTION_FREE) {
	/* can't home unless in free mode */
	return;
    }
    /* loop thru axis, treat each one individually */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint struct */
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, skip it */
	    continue;
	}
	/* detect rising and falling edges on home switch */
	home_sw_rise = 0;
	home_sw_fall = 0;
	home_sw_new = GET_JOINT_HOME_SWITCH_FLAG(joint);
	if (home_sw_new) {
	    if (!joint->home_sw_old) {
		home_sw_rise = 1;
	    }
	} else {
	    if (joint->home_sw_old) {
		home_sw_fall = 1;
	    }
	}
	joint->home_sw_old = home_sw_new;

	/* when an axis is homing, 'check_for_faults()' ignores its limit
	   switches, so that this code can do the right thing with them. Once
	   the homing process is finished, the 'check_for_faults()' resumes
	   checking */

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
	    case HOME_IDLE:
		/* nothing to do */
		break;

	    case HOME_START:
		/* This state is responsible for getting the homing process
		   started.  It doesn't actually do anything, it simply
		   determines what state is next */
		if (joint->home_flags & HOME_IS_SHARED && home_sw_new) {
		    reportError(
			"Cannot home while shared home switch is closed");
		    joint->home_state = HOME_IDLE;
		    break;
		}
		/* set flags that communicate with the rest of EMC */
		SET_JOINT_HOMING_FLAG(joint, 1);
		SET_JOINT_HOMED_FLAG(joint, 0);
		SET_JOINT_AT_HOME_FLAG(joint, 0);
		/* stop any existing motion */
		joint->free_tp_enable = 0;
		/* reset delay counter */
		joint->home_pause_timer = 0;
		/* figure out exactly what homing sequence is needed */
		if (joint->home_search_vel == 0.0) {
		    /* vel == 0 means no switch, home at current position */
		    joint->home_state = HOME_SET_SWITCH_POSITION;
		    immediate_state = 1;
		    break;
		} else {
		    /* need to find home switch */
		    joint->home_state = HOME_INITIAL_SEARCH_START;
		    immediate_state = 1;
		    break;
		}
		break;

	    case HOME_INITIAL_BACKOFF_START:
		/* This state is called if the homing sequence starts at a
		   location where the home switch is already tripped. It
		   starts a move away from the switch. */
		/* is the joint still moving? */
		if (joint->free_tp_active) {
		    /* yes, reset delay, wait until joint stops */
		    joint->home_pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (joint->home_pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    joint->home_pause_timer++;
		    break;
		}
		joint->home_pause_timer = 0;
		/* set up a move at '-search_vel' to back off of switch */
		home_start_move(joint, -joint->home_search_vel);
		/* next state */
		joint->home_state = HOME_INITIAL_BACKOFF_WAIT;
		break;

	    case HOME_INITIAL_BACKOFF_WAIT:
		/* This state is called while the machine is moving off of
		   the home switch.  It terminates when the switch is cleared
		   successfully.  If the move ends or hits a limit before it
		   clears the switch, the home is aborted. */
		/* are we off home switch yet? */
		if (home_sw_fall) {
		    /* yes, stop motion */
		    joint->free_tp_enable = 0;
		    /* begin initial search */
		    joint->home_state = HOME_INITIAL_SEARCH_START;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint);
		break;

	    case HOME_INITIAL_SEARCH_START:
		/* This state is responsible for starting a move toward the
		   home switch.  This move is at 'search_vel', which can be
		   fairly fast, because once the switch is found another
		   slower move will be used to set the exact home position. */
		/* is the joint already moving? */
		if (joint->free_tp_active) {
		    /* yes, reset delay, wait until joint stops */
		    joint->home_pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (joint->home_pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    joint->home_pause_timer++;
		    break;
		}
		joint->home_pause_timer = 0;
		/* make sure we aren't already on home switch */
		if (GET_JOINT_HOME_SWITCH_FLAG(joint)) {
		    /* already on switch, need to back off it first */
		    joint->home_state = HOME_INITIAL_BACKOFF_START;
		    immediate_state = 1;
		    break;
		}
		/* set up a move at 'search_vel' to find switch */
		home_start_move(joint, joint->home_search_vel);
		/* next state */
		joint->home_state = HOME_INITIAL_SEARCH_WAIT;
		break;

	    case HOME_INITIAL_SEARCH_WAIT:
		/* This state is called while the machine is looking for the
		   home switch.  It terminates when the switch is found.  If
		   the move ends or hits a limit before it finds the switch,
		   the home is aborted. */
		/* have we hit home switch yet? */
		if (home_sw_rise) {
		    /* yes, stop motion */
		    joint->free_tp_enable = 0;
		    /* go to next step */
		    joint->home_state = HOME_SET_COARSE_POSITION;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint);
		break;

	    case HOME_SET_COARSE_POSITION:
		/* This state is called after the first time the switch is
		   found.  At this point, we are approximately home. Although
		   we will do another slower pass to get the exact home
		   location, we reset the joint coordinates now so that screw
		   error comp will be appropriate for this portion of the
		   screw (previously we didn't know where we were at all). */
		/* set the current position to 'home_offset' */
		offset = joint->home_offset - joint->pos_fb;
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_pos_cmd += offset;
		joint->motor_offset -= offset;
		/* The next state depends on the signs of 'search_vel' and
		   'latch_vel'.  If they are the same, that means we must
		   back up, then do the final homing moving the same
		   direction as the initial search, on a rising edge of the
		   switch.  If they are opposite, it means that the final
		   homing will take place on a falling edge as the machine
		   moves off of the switch. */
		tmp = joint->home_search_vel * joint->home_latch_vel;
		if (tmp > 0.0) {
		    /* search and latch vel are same direction */
		    joint->home_state = HOME_FINAL_BACKOFF_START;
		} else if (tmp < 0.0) {
		    /* search and latch vel are opposite directions */
		    joint->home_state = HOME_FALL_SEARCH_START;
		} else {
		    /* latch vel is zero - error */
		    reportError("home latch velocity is zero");
		    joint->home_state = HOME_ABORT;
		}
		immediate_state = 1;
		break;

	    case HOME_FINAL_BACKOFF_START:
		/* This state is called once the approximate location of the
		   switch has been found.  It is responsible for starting a
		   move that will back off of the switch in preparation for a
		   final slow move that captures the exact switch location. */
		/* is the joint already moving? */
		if (joint->free_tp_active) {
		    /* yes, reset delay, wait until joint stops */
		    joint->home_pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (joint->home_pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    joint->home_pause_timer++;
		    break;
		}
		joint->home_pause_timer = 0;
		/* set up a move at '-search_vel' to back off of switch */
		home_start_move(joint, -joint->home_search_vel);
		/* next state */
		joint->home_state = HOME_FINAL_BACKOFF_WAIT;
		break;

	    case HOME_FINAL_BACKOFF_WAIT:
		/* This state is called while the machine is moving off of
		   the home switch after finding it's approximate location.
		   It terminates when the switch is cleared successfully.  If
		   the move ends or hits a limit before it clears the switch,
		   the home is aborted. */
		/* are we off home switch yet? */
		if (home_sw_fall) {
		    /* yes, stop motion */
		    joint->free_tp_enable = 0;
		    /* begin final search */
		    joint->home_state = HOME_RISE_SEARCH_START;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint);
		break;

	    case HOME_RISE_SEARCH_START:
		/* This state is called to start the final search for the
		   point where the home switch trips.  It moves at
		   'latch_vel' and looks for a rising edge on the switch */
		/* is the joint already moving? */
		if (joint->free_tp_active) {
		    /* yes, reset delay, wait until joint stops */
		    joint->home_pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (joint->home_pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    joint->home_pause_timer++;
		    break;
		}
		joint->home_pause_timer = 0;
		/* set up a move at 'latch_vel' to locate the switch */
		home_start_move(joint, joint->home_latch_vel);
		/* next state */
		joint->home_state = HOME_RISE_SEARCH_WAIT;
		break;

	    case HOME_RISE_SEARCH_WAIT:
		/* This state is called while the machine is moving towards
		   the home switch on it's final, low speed pass.  It
		   terminates when the switch is detected. If the move ends
		   or hits a limit before it hits the switch, the home is
		   aborted. */
		/* have we hit the home switch yet? */
		if (home_sw_rise) {
		    /* yes, where do we go next? */
		    if (joint->home_flags & HOME_USE_INDEX) {
			/* look for index pulse */
			joint->home_state = HOME_INDEX_SEARCH_START;
			immediate_state = 1;
			break;
		    } else {
			/* no index pulse, stop motion */
			joint->free_tp_enable = 0;
			/* go to next step */
			joint->home_state = HOME_SET_SWITCH_POSITION;
			immediate_state = 1;
			break;
		    }
		}
		home_do_moving_checks(joint);
		break;

	    case HOME_FALL_SEARCH_START:
		/* This state is called to start the final search for the
		   point where the home switch releases.  It moves at
		   'latch_vel' and looks for a falling edge on the switch */
		/* is the joint already moving? */
		if (joint->free_tp_active) {
		    /* yes, reset delay, wait until joint stops */
		    joint->home_pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (joint->home_pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    joint->home_pause_timer++;
		    break;
		}
		joint->home_pause_timer = 0;
		/* set up a move at 'latch_vel' to locate the switch */
		home_start_move(joint, joint->home_latch_vel);
		/* next state */
		joint->home_state = HOME_FALL_SEARCH_WAIT;
		break;

	    case HOME_FALL_SEARCH_WAIT:
		/* This state is called while the machine is moving away from
		   the home switch on it's final, low speed pass.  It
		   terminates when the switch is cleared. If the move ends or
		   hits a limit before it clears the switch, the home is
		   aborted. */
		/* have we cleared the home switch yet? */
		if (home_sw_fall) {
		    /* yes, where do we go next? */
		    if (joint->home_flags & HOME_USE_INDEX) {
			/* look for index pulse */
			joint->home_state = HOME_INDEX_SEARCH_START;
			immediate_state = 1;
			break;
		    } else {
			/* no index pulse, stop motion */
			joint->free_tp_enable = 0;
			/* go to next step */
			joint->home_state = HOME_SET_SWITCH_POSITION;
			immediate_state = 1;
			break;
		    }
		}
		home_do_moving_checks(joint);
		break;

	    case HOME_SET_SWITCH_POSITION:
		/* This state is called when the machine has determined the
		   switch position as accurately as possible.  It sets the
		   current joint position to 'home_offset', which is the
		   location of the home switch in joint coordinates. */
		/* set the current position to 'home_offset' */
		offset = joint->home_offset - joint->pos_fb;
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_pos_cmd += offset;
		joint->motor_offset -= offset;
		/* next state */
		joint->home_state = HOME_FINAL_MOVE_START;
		immediate_state = 1;
		break;

	    case HOME_INDEX_SEARCH_START:
		/* This state is called after the machine has made a low
		   speed pass to determine the limit switch location. It
		   sets index-enable, which tells the encoder driver to
		   reset its counter to zero and clear the enable when the
		   next index pulse arrives. */
		/* set the index enable */
		joint->index_enable = 1;
		/* and move right into the waiting state */
		joint->home_state = HOME_INDEX_SEARCH_WAIT;
		immediate_state = 1;
		home_do_moving_checks(joint);
		break;

	    case HOME_INDEX_SEARCH_WAIT:
		/* This state is called after the machine has found the
		   home switch and "armed" the encoder counter to reset on
		   the next index pulse. It continues at low speed until
		   an index pulse is detected, at which point it sets the
		   final home position.  If the move ends or hits a limit
		   before an index pulse occurs, the home is aborted. */
		/* has an index pulse arrived yet? encoder driver clears
		   enable when it does */
		if ( joint->index_enable == 0 ) {
		    /* yes, stop motion */
		    joint->free_tp_enable = 0;
		    /* go to next step */
		    joint->home_state = HOME_SET_INDEX_POSITION;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint);
		break;

	    case HOME_SET_INDEX_POSITION:
		/* This state is called when the encoder has been reset at
		   the index pulse position.  It sets the current joint 
		   position to 'home_offset', which is the location of the
		   index pulse in joint coordinates. */
		/* set the current position to 'home_offset' */
		joint->motor_offset = -joint->home_offset;
		joint->pos_fb = joint->motor_pos_fb -
		    (joint->backlash_filt + joint->motor_offset);
		joint->pos_cmd = joint->pos_fb;
		joint->free_pos_cmd = joint->pos_fb;
		/* next state */
		joint->home_state = HOME_FINAL_MOVE_START;
		immediate_state = 1;
		break;

	    case HOME_FINAL_MOVE_START:
		/* This state is called once the joint coordinate system is
		   set properly.  It moves to the actual 'home' position,
		   which is not neccessarily the position of the home switch
		   or index pulse. */
		/* is the joint already moving? */
		if (joint->free_tp_active) {
		    /* yes, reset delay, wait until joint stops */
		    joint->home_pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (joint->home_pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    joint->home_pause_timer++;
		    break;
		}
		joint->home_pause_timer = 0;
		/* plan a move to home position */
		joint->free_pos_cmd = joint->home;
		/* do the move at max speed */
		/*! \todo FIXME - should this be search_vel? or another user
		   specified speed? or is a rapid OK? */
		joint->free_vel_lim = joint->vel_limit;
		/* start the move */
		joint->free_tp_enable = 1;
		joint->home_state = HOME_FINAL_MOVE_WAIT;
		break;

	    case HOME_FINAL_MOVE_WAIT:
		/* This state is called while the machine makes its final
		   move to the home position.  It terminates when the machine 
		   arrives at the final location. If the move hits a limit
		   before it arrives, the home is aborted. */
		/* have we arrived (and stopped) at home? */
		if (!joint->free_tp_active) {
		    /* yes, stop motion */
		    joint->free_tp_enable = 0;
		    /* we're finally done */
		    joint->home_state = HOME_FINISHED;
		    immediate_state = 1;
		    break;
		}
		if (joint->on_pos_limit || joint->on_neg_limit) {
		    /* on limit, check to see if we should trip */
		    if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			reportError("hit limit in home state %d",
			    joint->home_state);
			joint->home_state = HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		break;

	    case HOME_FINISHED:
		SET_JOINT_HOMING_FLAG(joint, 0);
		SET_JOINT_HOMED_FLAG(joint, 1);
		SET_JOINT_AT_HOME_FLAG(joint, 1);
		joint->home_state = HOME_IDLE;
		immediate_state = 1;
		break;

	    case HOME_ABORT:
		SET_JOINT_HOMING_FLAG(joint, 0);
		SET_JOINT_HOMED_FLAG(joint, 0);
		SET_JOINT_AT_HOME_FLAG(joint, 0);
		joint->free_tp_enable = 0;
		joint->home_state = HOME_IDLE;
		joint->index_enable = 0;
		immediate_state = 1;
		break;

	    default:
		/* should never get here */
		reportError("unknown state '%d' during homing",
		    joint->home_state);
		joint->home_state = EMCMOT_ABORT;
		immediate_state = 1;
		break;
	    }			/* end of switch(joint->home_state) */
	} while (immediate_state);
    }
}

static void get_pos_cmds(long period)
{
    int joint_num, all_homed, all_at_home, result;
    emcmot_joint_t *joint;
    double positions[EMCMOT_MAX_AXIS];
/*! \todo Another #if 0 */
#if 0
    static int interpolationCounter = 0;
#endif
    double old_pos_cmd;
    double max_dv, tiny_dp, pos_err, vel_req, vel_lim;

    /* used in teleop mode to compute the max accell requested */
    double accell_mag;

    /* RUN MOTION CALCULATIONS: */

    /* run traj planner code depending on the state */
    switch ( emcmotStatus->motion_state) {
    case EMCMOT_MOTION_FREE:
	/* in free mode, each joint is planned independently */
	/* Each joint has a very simple "trajectory planner".  If the planner
	   is enabled (free_tp_enable), then it moves toward free_pos_cmd at
	   free_vel_lim, obeying the joint's accel and velocity limits, and
	   stopping when it gets there.  If it is not enabled, it stops as
	   quickly as possible, again obeying the accel limit.  When
	   disabled, free_pos_cmd is set to the current position. */
	/* initial value for flag, if needed it will be cleared below */
	SET_MOTION_INPOS_FLAG(1);
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &joints[joint_num];
	    //AJ: only need to worry about free mode if joint is active
	    if (GET_JOINT_ACTIVE_FLAG(joint)) {
		joint->free_tp_active = 0;
		/* compute max change in velocity per servo period */
		max_dv = joint->acc_limit * servo_period;
		/* compute a tiny position range, to be treated as zero */
		tiny_dp = max_dv * servo_period * 0.001;
		/* calculate desired velocity */
		if (joint->free_tp_enable) {
		    /* planner enabled, request a velocity that tends to drive
		       pos_err to zero, but allows for stopping without position
		       overshoot */
		    pos_err = joint->free_pos_cmd - joint->pos_cmd;
		    /* positive and negative errors require some sign flipping to
		       avoid sqrt(negative) */
		    if (pos_err > tiny_dp) {
			vel_req =
			    -max_dv + sqrt(2.0 * joint->acc_limit * pos_err +
			    max_dv * max_dv);
			/* mark joint active */
			joint->free_tp_active = 1;
		    } else if (pos_err < -tiny_dp) {
			vel_req =
			    max_dv - sqrt(-2.0 * joint->acc_limit * pos_err +
			    max_dv * max_dv);
			/* mark joint active */
			joint->free_tp_active = 1;
		    } else {
			/* within 'tiny_dp' of desired pos, no need to move */
			vel_req = 0.0;
		    }
		} else {
		    /* planner disabled, request zero velocity */
		    vel_req = 0.0;
		    /* and set command to present position to avoid movement when
		       next enabled */
		    joint->free_pos_cmd = joint->pos_cmd;
		}
		/* if we move at all, clear AT_HOME flag */
		if (joint->free_tp_active) {
		    SET_JOINT_AT_HOME_FLAG(joint, 0);
		}
                if ( joint->home_state == HOME_IDLE ) {
                    /* velocity limit = planner limit * global scale factor */
                    /* the global factor is used for feedrate override */
                    vel_lim = joint->free_vel_lim * emcmotStatus->net_feed_scale;
                } else {
                    /* except if homing, when we ignore FO */
                    vel_lim = joint->free_vel_lim;
                }
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
		/* ramp velocity toward request at axis accel limit */
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
		/* copy to coarse_pos */
		joint->coarse_pos = joint->pos_cmd;
		/* update joint status flag and overall status flag */
		if ( joint->free_tp_active ) {
		    /* active TP means we're moving, so not in position */
		    SET_JOINT_INPOS_FLAG(joint, 0);
		    SET_MOTION_INPOS_FLAG(0);
		    /* check to see if this move is with limits disabled */
		    if ( emcmotStatus->overrideLimits ) {
			emcmotDebug->overriding = 1;
		    }
		} else {
		    SET_JOINT_INPOS_FLAG(joint, 1);
		}
	    }//if (GET_JOINT_ACTIVE_FLAG(join))
	}//for loop for joints
	/* if overriding is true and we're in position, the jog
	   is complete, and the limits should be re-enabled */
	if ( (emcmotDebug->overriding ) && ( GET_MOTION_INPOS_FLAG() ) ) {
	    emcmotStatus->overrideLimits = 0;
	    emcmotDebug->overriding = 0;
	}
	/*! \todo FIXME - this should run at the traj rate */
	all_homed = 1;
	all_at_home = 1;
	/* copy joint position feedback to local array */
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &joints[joint_num];
	    /* copy coarse command */
	    positions[joint_num] = joint->coarse_pos;
	    /* check for homed */
	    if (!GET_JOINT_HOMED_FLAG(joint)) {
		all_homed = 0;
		all_at_home = 0;
	    } else if (!GET_JOINT_AT_HOME_FLAG(joint)) {
		all_at_home = 0;
	    }
	}
	switch (kinType) {

	case KINEMATICS_IDENTITY:
	    kinematicsForward(positions, &emcmotStatus->carte_pos_cmd, &fflags, &iflags);
	    if (checkAllHomed()) {
		emcmotStatus->carte_pos_cmd_ok = 1;
	    } else {
		emcmotStatus->carte_pos_cmd_ok = 0;
	    }
	    break;

	case KINEMATICS_BOTH:
	    if (checkAllHomed()) {
		/* is previous value suitable for use as initial guess? */
		if (!emcmotStatus->carte_pos_cmd_ok) {
		    /* no, use home position as initial guess */
		    emcmotStatus->carte_pos_cmd = emcmotStatus->world_home;
		}
		/* calculate Cartesean position command from joint coarse pos cmd */
		result =
		    kinematicsForward(positions, &emcmotStatus->carte_pos_cmd, &fflags, &iflags);
		/* check to make sure kinematics converged */
		if (result < 0) {
		    /* error during kinematics calculations */
		    emcmotStatus->carte_pos_cmd_ok = 0;
		} else {
		    /* it worked! */
		    emcmotStatus->carte_pos_cmd_ok = 1;
		}
	    } else {
		emcmotStatus->carte_pos_cmd_ok = 0;
	    }
	    break;

	case KINEMATICS_INVERSE_ONLY:
	    emcmotStatus->carte_pos_cmd_ok = 0;
	    break;

	default:
	    emcmotStatus->carte_pos_cmd_ok = 0;
	    break;
	}
        /* end of FREE mode */
	break;
    case EMCMOT_MOTION_COORD:
	/* check joint 0 to see if the interpolators are empty */
	while (cubicNeedNextPoint(&(joints[0].cubic))) {
	    /* they're empty, pull next point(s) off Cartesian planner */
	    /* run coordinated trajectory planning cycle */
	    tpRunCycle(&emcmotDebug->queue, period);
	    /* gt new commanded traj pos */
	    emcmotStatus->carte_pos_cmd = tpGetPos(&emcmotDebug->queue);
	    /* OUTPUT KINEMATICS - convert to joints in local array */
	    kinematicsInverse(&emcmotStatus->carte_pos_cmd, positions,
		&iflags, &fflags);
	    /* copy to joint structures and spline them up */
	    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		/* point to joint struct */
		joint = &joints[joint_num];
		joint->coarse_pos = positions[joint_num];
		/* spline joints up-- note that we may be adding points
		   that fail soft limits, but we'll abort at the end of
		   this cycle so it doesn't really matter */
		cubicAddPoint(&(joint->cubic), joint->coarse_pos);
	    }
	    /* END OF OUTPUT KINS */
	}
	/* there is data in the interpolators */
	/* run interpolation */
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &joints[joint_num];
	    /* save old command */
	    old_pos_cmd = joint->pos_cmd;
	    /* interpolate to get new one */
	    joint->pos_cmd = cubicInterpolate(&(joint->cubic), 0, 0, 0, 0);
	    joint->vel_cmd = (joint->pos_cmd - old_pos_cmd) * servo_freq;
	}
	/* report motion status */
	SET_MOTION_INPOS_FLAG(0);
	if (tpIsDone(&emcmotDebug->queue)) {
	    SET_MOTION_INPOS_FLAG(1);
	}
	break;
    case EMCMOT_MOTION_TELEOP:

	/* first the desired Accell's are computed based on
	    desired Velocity, current velocity and period */
	emcmotDebug->teleop_data.desiredAccell.tran.x =
	    (emcmotDebug->teleop_data.desiredVel.tran.x -
	    emcmotDebug->teleop_data.currentVel.tran.x) /
	    servo_period;
	emcmotDebug->teleop_data.desiredAccell.tran.y =
	    (emcmotDebug->teleop_data.desiredVel.tran.y -
	    emcmotDebug->teleop_data.currentVel.tran.y) /
	    servo_period;
	emcmotDebug->teleop_data.desiredAccell.tran.z =
	    (emcmotDebug->teleop_data.desiredVel.tran.z -
	    emcmotDebug->teleop_data.currentVel.tran.z) /
	    servo_period;

	/* a Carthesian Accell is computed */
	pmCartMag(emcmotDebug->teleop_data.desiredAccell.tran,
	    &accell_mag);

	/* then the accells for the rotary axes */
	emcmotDebug->teleop_data.desiredAccell.a =
	    (emcmotDebug->teleop_data.desiredVel.a -
	    emcmotDebug->teleop_data.currentVel.a) /
	    servo_period;
	emcmotDebug->teleop_data.desiredAccell.b =
	    (emcmotDebug->teleop_data.desiredVel.b -
	    emcmotDebug->teleop_data.currentVel.b) /
	    servo_period;
	emcmotDebug->teleop_data.desiredAccell.c =
	    (emcmotDebug->teleop_data.desiredVel.c -
	    emcmotDebug->teleop_data.currentVel.c) /
	    servo_period;
	if (emcmotDebug->teleop_data.desiredAccell.a > accell_mag) {
	    accell_mag = emcmotDebug->teleop_data.desiredAccell.a;
	}
	if (emcmotDebug->teleop_data.desiredAccell.b > accell_mag) {
	    accell_mag = emcmotDebug->teleop_data.desiredAccell.b;
	}
	if (emcmotDebug->teleop_data.desiredAccell.c > accell_mag) {
	    accell_mag = emcmotDebug->teleop_data.desiredAccell.c;
	}
	
	/* accell_mag should now hold the max accell */
	
	if (accell_mag > emcmotStatus->acc) {
	    /* if accell_mag is too great, all need resizing */
	    pmCartScalMult(emcmotDebug->teleop_data.desiredAccell.tran, 
		emcmotStatus->acc / accell_mag,
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
		servo_period;
	    emcmotDebug->teleop_data.currentVel.tran.y +=
		emcmotDebug->teleop_data.currentAccell.tran.y *
		servo_period;
	    emcmotDebug->teleop_data.currentVel.tran.z +=
		emcmotDebug->teleop_data.currentAccell.tran.z *
		servo_period;
	    emcmotDebug->teleop_data.currentVel.a +=
		emcmotDebug->teleop_data.currentAccell.a *
		servo_period;
	    emcmotDebug->teleop_data.currentVel.b +=
		emcmotDebug->teleop_data.currentAccell.b *
		servo_period;
	    emcmotDebug->teleop_data.currentVel.c +=
		emcmotDebug->teleop_data.currentAccell.c *
		servo_period;
	} else {
	    /* if accell_mag is not greater, the computed accell's stay as is */
	    emcmotDebug->teleop_data.currentAccell =
		emcmotDebug->teleop_data.desiredAccell;
	    emcmotDebug->teleop_data.currentVel =
		emcmotDebug->teleop_data.desiredVel;
	}


	/* based on curent position, current vel and period, 
	   the next position is computed */
	emcmotStatus->carte_pos_cmd.tran.x +=
	    emcmotDebug->teleop_data.currentVel.tran.x *
	    servo_period;
	emcmotStatus->carte_pos_cmd.tran.y +=
	    emcmotDebug->teleop_data.currentVel.tran.y *
	    servo_period;
	emcmotStatus->carte_pos_cmd.tran.z +=
	    emcmotDebug->teleop_data.currentVel.tran.z *
	    servo_period;
	emcmotStatus->carte_pos_cmd.a +=
	    emcmotDebug->teleop_data.currentVel.a *
	    servo_period;
	emcmotStatus->carte_pos_cmd.b +=
	    emcmotDebug->teleop_data.currentVel.b *
	    servo_period;
	emcmotStatus->carte_pos_cmd.c +=
	    emcmotDebug->teleop_data.currentVel.c *
	    servo_period;

	/* the next position then gets run through the inverse kins,
	    to compute the next positions of the joints */

	/* OUTPUT KINEMATICS - convert to joints in local array */
	kinematicsInverse(&emcmotStatus->carte_pos_cmd, positions,
	    &iflags, &fflags);
	/* copy to joint structures and spline them up */
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &joints[joint_num];
	    joint->coarse_pos = positions[joint_num];
	    /* spline joints up-- note that we may be adding points
		   that fail soft limits, but we'll abort at the end of
		   this cycle so it doesn't really matter */
	    cubicAddPoint(&(joint->cubic), joint->coarse_pos);
	}
	/* END OF OUTPUT KINS */

	/* there is data in the interpolators */
	/* run interpolation */
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &joints[joint_num];
	    /* save old command */
	    old_pos_cmd = joint->pos_cmd;
	    /* interpolate to get new one */
	    joint->pos_cmd = cubicInterpolate(&(joint->cubic), 0, 0, 0, 0);
	    joint->vel_cmd = (joint->pos_cmd - old_pos_cmd) * servo_freq;
	}

	/* end of teleop mode */
	break;

    case EMCMOT_MOTION_DISABLED:
	/* set position commands to match feedbacks, this avoids
	   disturbances and/or following errors when enabling */
	emcmotStatus->carte_pos_cmd = emcmotStatus->carte_pos_fb;
	for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	    /* point to joint struct */
	    joint = &joints[joint_num];
	    /* save old command */
	    joint->pos_cmd = joint->pos_fb;
	    /* set joint velocity to zero */
	    joint->vel_cmd = 0.0;
	}
	
	break;
    default:
	break;
    }

/* NOTES:  These notes are just my understanding of how things work.

There are seven sets of position information.

1) emcmotStatus->carte_pos_cmd
2) emcmotStatus->joints[n].coarse_pos
3) emcmotStatus->joints[n].pos_cmd
4) emcmotStatus->joints[n].motor_pos_cmd
5) emcmotStatus->joints[n].motor_pos_fb
6) emcmotStatus->joints[n].pos_fb
7) emcmotStatus->carte_pos_fb

Their exact contents and meaning are as follows:

1) This is the desired position, in Cartesean coordinates.  It is
   updated at the traj rate, not the servo rate.
   In coord mode, it is determined by the traj planner
   In teleop mode, it is determined by the traj planner?
   In free mode, it is not used, since free mode motion takes
     place in joint space, not cartesean space.  It may be
     displayed by the GUI however, so it is updated by
     applying forward kins to (2), unless forward kins are
     not available, in which case it is copied from (7).

2) This is the desired position, in joint coordinates, but
   before interpolation.  It is updated at the traj rate, not
   the servo rate..
   In coord mode, it is generated by applying inverse kins to (1)
   In teleop mode, it is generated by applying inverse kins to (1)
   In free mode, it is not used, since the free mode planner generates
     a new (3) position every servo period without interpolation.
     However, it is used indirectly by GUIs, so it is copied from (3).

3) This is the desired position, in joint coords, after interpolation.
   A new set of these coords is generated every servo period.
   In coord mode, it is generated from (2) by the interpolator.
   In teleop mode, it is generated from (2) by the interpolator.
   In free mode, it is generated by the simple free mode traj planner.

4) This is the desired position, in motor coords.  Motor coords are
   generated by adding backlash compensation, lead screw error
   compensation, and offset (for homing) to (3).
   It is generated the same way regardless of the mode, and is the
   output to the PID loop or other position loop.

5) This is the actual position, in motor coords.  It is the input from
   encoders or other feedback device (or from virtual encoders on open
   loop machines).  It is "generated" by reading the feedback device.

6) This is the actual position, in joint coordinates.  It is generated
   by subtracting offset, lead screw error compensation, and backlash
   compensation from (5).  It is generated the same way regardless of
   the operating mode.

7) This is the actual position, in Cartesean coordinates.  It is updated
   at the traj rate, not the servo rate.
   OLD VERSION:
   In the old version, there are four sets of code to generate actualPos.
   One for each operating mode, and one for when motion is disabled.
   The code for coord and teleop modes is identical.  The code for free
   mode is somewhat different, in particular to deal with the case where
   one or more axes are not homed.  The disabled code is quite similar,
   but not identical, to the coord mode code.  In general, the code
   calculates actualPos by applying the forward kins to (6).  However,
   where forward kins are not available, actualPos is either copied
   from (1) (assumes no following error), or simply left alone.
   These special cases are handled differently for each operating mode.
   NEW VERSION:
   I would like to both simplify and relocate this.  As far as I can
   tell, actualPos should _always_ be the best estimate of the actual
   machine position in Cartesean coordinates.  So it should always be
   calculated the same way.
   In addition to always using the same code to calculate actualPos,
   I want to move that code.  It is really a feedback calculation, and
   as such it belongs with the rest of the feedback calculations early
   in control.c, not as part of the output generation code as it is now.
   Ideally, actualPos would always be calculated by applying forward
   kinematics to (6).  However, forward kinematics may not be available,
   or they may be unusable because one or more axes aren't homed.  In
   that case, the options are: A) fake it by copying (1), or B) admit
   that we don't really know the Cartesean coordinates, and simply
   don't update actualPos.  Whatever approach is used, I can see no
   reason not to do it the same way regardless of the operating mode.
   I would propose the following:  If there are forward kins, use them,
   unless they don't work because of unhomed axes or other problems,
   in which case do (B).  If no forward kins, do (A), since otherwise
   actualPos would _never_ get updated.

*/

/* there was a large snip of old code here, 
   removed since the new implementation works.
   if the old code is needed it's in CVS revision <= 1.46 */

}

static void compute_screw_comp(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    emcmot_comp_t *comp;
    double dpos;
    double a_max, v_max, v, s_to_go, ds_stop, ds_vel, ds_acc, dv_acc;


    /* compute the correction */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
        /* point to joint struct */
        joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, skip it */
	    continue;
	}
	/* point to compensation data */
	comp = &(joint->comp);
	if ( comp->entries > 0 ) {
	    /* there is data in the comp table, use it */
	    /* first make sure we're in the right spot in the table */
	    while ( joint->pos_cmd < comp->entry->nominal ) {
		comp->entry--;
	    }
	    while ( joint->pos_cmd >= (comp->entry+1)->nominal ) {
		comp->entry++;
	    }
	    /* now interpolate */
	    dpos = joint->pos_cmd - comp->entry->nominal;
	    if (joint->vel_cmd > 0.0) {
	        /* moving "up". apply forward screw comp */
		joint->backlash_corr = comp->entry->fwd_trim + 
					comp->entry->fwd_slope * dpos;
	    } else if (joint->vel_cmd < 0.0) {
	        /* moving "down". apply reverse screw comp */
		joint->backlash_corr = comp->entry->rev_trim +
					comp->entry->rev_slope * dpos;
	    } else {
		/* not moving, use whatever was there before */
	    }
	} else {
	    /* no compensation data, just use +/- 1/2 of backlash */
	    /** FIXME: this can actually be removed - if the user space code
		sends a single compensation entry with any nominal value,
		and with fwd_trim = +0.5 times the backlash value, and 
		rev_trim = -0.5 times backlash, the above screw comp code
		will give exactly the same result as this code. */
	    /* determine which way the compensation should be applied */
	    if (joint->vel_cmd > 0.0) {
	        /* moving "up". apply positive backlash comp */
		joint->backlash_corr = 0.5 * joint->backlash;
	    } else if (joint->vel_cmd < 0.0) {
	        /* moving "down". apply negative backlash comp */
		joint->backlash_corr = -0.5 * joint->backlash;
	    } else {
		/* not moving, use whatever was there before */
	    }
	}
	/* at this point, the correction has been computed, but
	   the value may make abrupt jumps on direction reversal */
    /*
     * 07/09/2005 - S-curve implementation by Bas Laarhoven
     *
     * Implementation:
     *   Generate a ramped velocity profile for backlash or screw error comp.
     *   The velocity is ramped up to the maximum speed setting (if possible),
     *   using the maximum acceleration setting.
     *   At the end, the speed is ramped dowm using the same acceleration.
     *   The algorithm keeps looking ahead. Depending on the distance to go,
     *   the speed is increased, kept constant or decreased.
     *   
     * Limitations:
     *   Since the compensation adds up to the normal movement, total
     *   accelleration and total velocity may exceed maximum settings!
     *   Currently this is limited to 150% by implementation.
     *   To fix this, the calculations in get_pos_cmd should include
     *   information from the backlash corection. This makes things
     *   rather complicated and it might be better to implement the
     *   backlash compensation at another place to prevent this kind
     *   of interaction.
     *   More testing under different circumstances will show if this
     *   needs a more complicate solution.
     *   For now this implementation seems to generate smoother
     *   movements and less following errors than the original code.
     */

	/* Limit maximum accelleration and velocity 'overshoot'
	 * to 150% of the maximum settings.
	 * The TP and backlash shouldn't use more than 100%
	 * (together) but this requires some interaction that
	 * isn't implemented yet.
	 */ 
        v_max = 0.5 * joint->vel_limit * emcmotStatus->net_feed_scale;
        a_max = 0.5 * joint->acc_limit;
        v = joint->backlash_vel;
        if (joint->backlash_corr >= 0.0) {
            s_to_go = joint->backlash_corr - joint->backlash_filt; /* abs val */
            if (s_to_go > 0) {
                // off target, need to move
                ds_vel  = v * servo_period;           /* abs val */
                dv_acc  = a_max * servo_period;       /* abs val */
                ds_stop = 0.5 * (v + dv_acc) *
		                (v + dv_acc) / a_max; /* abs val */
                if (s_to_go <= ds_stop + ds_vel) {
                    // ramp down
                    if (v > dv_acc) {
                        // decellerate one period
                        ds_acc = 0.5 * dv_acc * servo_period; /* abs val */
                        joint->backlash_vel  -= dv_acc;
                        joint->backlash_filt += ds_vel - ds_acc;
                    } else {
                        // last step to target
                        joint->backlash_vel  = 0.0;
                        joint->backlash_filt = joint->backlash_corr;
                    }
                } else {
                    if (v + dv_acc > v_max) {
                        dv_acc = v_max - v;                /* abs val */
                    }
                    ds_acc  = 0.5 * dv_acc * servo_period; /* abs val */
                    ds_stop = 0.5 * (v + dv_acc) *
                                    (v + dv_acc) / a_max;  /* abs val */
                    if (s_to_go > ds_stop + ds_vel + ds_acc) {
                        // ramp up
                       joint->backlash_vel  += dv_acc;
                       joint->backlash_filt += ds_vel + ds_acc;
                    } else {
                       // constant velocity
                       joint->backlash_filt += ds_vel;
                    }
                }
            } else if (s_to_go < 0) {
                // safely handle overshoot (should not occur)
               joint->backlash_vel = 0.0;
               joint->backlash_filt = joint->backlash_corr;
            }
        } else {  /* joint->backlash_corr < 0.0 */
            s_to_go = joint->backlash_filt - joint->backlash_corr; /* abs val */
            if (s_to_go > 0) {
                // off target, need to move
                ds_vel  = -v * servo_period;          /* abs val */
                dv_acc  = a_max * servo_period;       /* abs val */
                ds_stop = 0.5 * (v - dv_acc) *
			        (v - dv_acc) / a_max; /* abs val */
                if (s_to_go <= ds_stop + ds_vel) {
                    // ramp down
                    if (-v > dv_acc) {
                        // decellerate one period
                        ds_acc = 0.5 * dv_acc * servo_period; /* abs val */
                        joint->backlash_vel  += dv_acc;   /* decrease */
                        joint->backlash_filt -= ds_vel - ds_acc;
                    } else {
                        // last step to target
                        joint->backlash_vel = 0.0;
                        joint->backlash_filt = joint->backlash_corr;
                    }
                } else {
                    if (-v + dv_acc > v_max) {
                        dv_acc = v_max + v;               /* abs val */
                    }
                    ds_acc = 0.5 * dv_acc * servo_period; /* abs val */
                    ds_stop = 0.5 * (v - dv_acc) *
                                    (v - dv_acc) / a_max; /* abs val */
                    if (s_to_go > ds_stop + ds_vel + ds_acc) {
                        // ramp up
                        joint->backlash_vel  -= dv_acc;   /* increase */
                        joint->backlash_filt -= ds_vel + ds_acc;
                    } else {
                        // constant velocity
                        joint->backlash_filt -= ds_vel;
                    }
                }
            } else if (s_to_go < 0) {
                // safely handle overshoot (should not occur)
                joint->backlash_vel = 0.0;
                joint->backlash_filt = joint->backlash_corr;
            }
        }
        /* backlash (and motor offset) will be applied to output later */
        /* end of axis loop */
    }
}

/*! \todo FIXME - once the HAL refactor is done so that metadata isn't stored
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
    emcmot_hal_data->motion_enabled = GET_MOTION_ENABLE_FLAG();
    emcmot_hal_data->in_position = GET_MOTION_INPOS_FLAG();
    *(emcmot_hal_data->inpos_output) = emcmot_hal_data->in_position;
    emcmot_hal_data->coord_mode = GET_MOTION_COORD_FLAG();
    emcmot_hal_data->teleop_mode = GET_MOTION_TELEOP_FLAG();
    emcmot_hal_data->coord_error = GET_MOTION_ERROR_FLAG();
    *(emcmot_hal_data->spindle_speed_out) = emcmotStatus->spindle.speed * emcmotStatus->spindle_scale;
    *(emcmot_hal_data->spindle_on) = ((emcmotStatus->spindle.speed * emcmotStatus->net_spindle_scale) != 0) ? 1 : 0;
    *(emcmot_hal_data->spindle_forward) = (emcmotStatus->spindle.speed > 0) ? 1 : 0;
    *(emcmot_hal_data->spindle_reverse) = (emcmotStatus->spindle.speed < 0) ? 1 : 0;
    *(emcmot_hal_data->spindle_brake) = (emcmotStatus->spindle.brake != 0) ? 1 : 0;
    
    /* These params can be used to examine any internal variable. */
    /* Change the following lines to assign the variable you want to observe
       to one of the debug parameters.  You can also comment out these lines
       and copy elsewhere if you want to observe an automatic variable that
       isn't in scope here. */
    emcmot_hal_data->debug_bit_0 = joints[1].free_tp_active;
    emcmot_hal_data->debug_bit_1 = emcmotStatus->enables_new & AF_ENABLED;
    emcmot_hal_data->debug_float_0 = emcmotStatus->net_feed_scale;
    emcmot_hal_data->debug_float_1 = 0.0;
    *emcmot_hal_data->spindle_sync = emcmotStatus->spindleSync;

    /* output axis info to HAL for scoping, etc */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint struct */
	joint = &joints[joint_num];
	/* apply backlash and motor offset to output */
	joint->motor_pos_cmd =
	    joint->pos_cmd + joint->backlash_filt + joint->motor_offset;
	/* point to HAL data */
	axis_data = &(emcmot_hal_data->axis[joint_num]);
	/* write to HAL pins */
	*(axis_data->motor_pos_cmd) = joint->motor_pos_cmd;
	*(axis_data->amp_enable) = GET_JOINT_ENABLE_FLAG(joint);
	*(axis_data->index_enable) = joint->index_enable;
	/* output to parameters (for scoping, etc.) */
	axis_data->coarse_pos_cmd = joint->coarse_pos;
	axis_data->joint_pos_cmd = joint->pos_cmd;
	axis_data->joint_vel_cmd = joint->vel_cmd;
	axis_data->backlash_corr = joint->backlash_corr;
	axis_data->backlash_filt = joint->backlash_filt;
	axis_data->backlash_vel = joint->backlash_vel;
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
	axis_data->homing = GET_JOINT_HOMING_FLAG(joint);
	axis_data->homed = GET_JOINT_HOMED_FLAG(joint);
	axis_data->f_errored = GET_JOINT_FERROR_FLAG(joint);
	axis_data->faulted = GET_JOINT_FAULT_FLAG(joint);
	axis_data->home_state = joint->home_state;
    }
}

static void update_status(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    emcmot_joint_status_t *joint_status;
#ifdef WATCH_FLAGS
    static int old_joint_flags[8];
    static int old_motion_flag;
#endif

    /* copy status info from private joint structure to status
       struct in shared memory */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	joint = &joints[joint_num];
	/* point to joint status */
	joint_status = &(emcmotStatus->joint_status[joint_num]);
	/* copy stuff */
#ifdef WATCH_FLAGS
	/*! \todo FIXME - this is for debugging */
	if ( old_joint_flags[joint_num] != joint->flag ) {
	    rtapi_print ( "Joint %d flag %04X -> %04X\n", joint_num, old_joint_flags[joint_num], joint->flag );
	    old_joint_flags[joint_num] = joint->flag;
	}
#endif
	joint_status->flag = joint->flag;
	joint_status->pos_cmd = joint->pos_cmd;
	joint_status->pos_fb = joint->pos_fb;
	joint_status->ferror = joint->ferror;
	joint_status->ferror_high_mark = joint->ferror_high_mark;
	joint_status->backlash = joint->backlash;
	joint_status->max_pos_limit = joint->max_pos_limit;
	joint_status->min_pos_limit = joint->min_pos_limit;
	joint_status->min_ferror = joint->min_ferror;
	joint_status->max_ferror = joint->max_ferror;
	joint_status->home_offset = joint->home_offset;
    }

    /*! \todo FIXME - the rest of this function is stuff that was apparently
       dropped in the initial move from emcmot.c to control.c.  I
       don't know how much is still needed, and how much is baggage.
    */

    /* motion emcmotDebug->queue status */
    emcmotStatus->depth = tpQueueDepth(&emcmotDebug->queue);
    emcmotStatus->activeDepth = tpActiveDepth(&emcmotDebug->queue);
    emcmotStatus->id = tpGetExecId(&emcmotDebug->queue);
    emcmotStatus->motionType = tpGetMotionType(&emcmotDebug->queue);
    emcmotStatus->queueFull = tcqFull(&emcmotDebug->queue.queue);

    /* check to see if we should pause in order to implement
       single emcmotDebug->stepping */
    if (emcmotDebug->stepping && emcmotDebug->idForStep != emcmotStatus->id) {
      tpPause(&emcmotDebug->queue);
      emcmotDebug->stepping = 0;
      emcmotStatus->paused = 1;
    }
#ifdef WATCH_FLAGS
    /*! \todo FIXME - this is for debugging */
    if ( old_motion_flag != emcmotStatus->motionFlag ) {
	rtapi_print ( "Motion flag %04X -> %04X\n", old_motion_flag, emcmotStatus->motionFlag );
	old_motion_flag = emcmotStatus->motionFlag;
    }
#endif
}

