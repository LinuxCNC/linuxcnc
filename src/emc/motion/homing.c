/********************************************************************
* Description: homing.c
*   code to handle homing - originally in control.c, but moved out
*   to improve modularity and keep control.c from bloating
*
* Author: jmkasunich
* License: GPL Version 2
* Created on:
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include "rtapi.h"
#include "hal.h"
#include "motion.h"
#include "mot_priv.h"
#include "rtapi_math.h"

// Mark strings for translation, but defer translation to userspace
#define _(s) (s)

/***********************************************************************
*                         LOCAL CONSTANTS                              *
************************************************************************/

/* Length of delay between homing motions - this is intended to
   ensure that all motion has ceased and switch bouncing has
   ended.  We might want to make this user adjustable, but for
   now it's a constant.  It is in seconds */
#define HOME_DELAY 0.100

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/* variable used internally by do_homing, but global so that
   'home_do_moving_checks()' can access it */
static int immediate_state;

/***********************************************************************
*                      LOCAL FUNCTIONS                                 *
************************************************************************/

/* a couple of helper functions with code that would otherwise be
   repeated in several different states of the homing state machine */

/* 'home_start_move()' starts a move at the specified velocity.  The
   length of the move is equal to twice the overall range of the joint,
   but the intent is that something (like a home switch or index pulse)
   will stop it before that point. */
static void home_start_move(emcmot_joint_t * joint, double vel)
{
    double joint_range;

    /* set up a long move */
    joint_range = joint->max_pos_limit - joint->min_pos_limit;
    if (vel > 0.0) {
	joint->free_pos_cmd = joint->pos_cmd + 2.0 * joint_range;
    } else {
	joint->free_pos_cmd = joint->pos_cmd - 2.0 * joint_range;
    }
    joint->free_vel_lim = rtapi_fabs(vel);
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
	    reportError(_("hit limit in home state %d"), joint->home_state);
	    joint->home_state = HOME_ABORT;
	    immediate_state = 1;
	    return;
	}
    }
    /* check for reached end of move */
    if (!joint->free_tp_active) {
	/* reached end of move without hitting switch */
	joint->free_tp_enable = 0;
	reportError(_("end of move in home state %d"), joint->home_state);
	joint->home_state = HOME_ABORT;
	immediate_state = 1;
	return;
    }
}

/***********************************************************************
*                      PUBLIC FUNCTIONS                                *
************************************************************************/

void do_homing_sequence(void)
{
    static int home_sequence = -1;
    int i;
    int seen = 0;
    emcmot_joint_t *joint;

    /* first pass init */
    if(home_sequence == -1) {
        emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE;
	home_sequence = 0;
    }

    switch(emcmotStatus->homingSequenceState) {
    case HOME_SEQUENCE_IDLE:
	/* nothing to do */
	break;

    case HOME_SEQUENCE_START:
	/* a request to home all joints */
	for(i=0; i < num_joints; i++) {
	    joint = &joints[i];
	    if(joint->home_state != HOME_IDLE) {
		/* a home is already in progress, abort the home-all */
		emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE;
		return;
	    }
	}
	/* ok to start the sequence, start at zero */
	home_sequence = 0;
	/* tell the world we're on the job */
	emcmotStatus->homing_active = 1;
	/* and drop into next state */

    case HOME_SEQUENCE_START_JOINTS:
	/* start all joints whose sequence number matches home_sequence */
	for(i=0; i < num_joints; i++) {
	    joint = &joints[i];
	    if(joint->home_sequence == home_sequence) {
		/* start this joint */
	        joint->free_tp_enable = 0;
		joint->home_state = HOME_START;
		seen++;
	    }
	}
	if(seen) {
	    /* at least one joint is homing, wait for it */
	    emcmotStatus->homingSequenceState = HOME_SEQUENCE_WAIT_JOINTS;
	} else {
	    /* no joints have this sequence number, we're done */
	    emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE;
	    /* tell the world */
	    emcmotStatus->homing_active = 0;
	}
	break;

    case HOME_SEQUENCE_WAIT_JOINTS:
	for(i=0; i < num_joints; i++) {
	    joint = &joints[i];
	    if(joint->home_sequence != home_sequence) {
		/* this joint is not at the current sequence number, ignore it */
		continue;
	    }
	    if(joint->home_state != HOME_IDLE) {
		/* still busy homing, keep waiting */
		seen = 1;
		continue;
	    }
	    if(!GET_JOINT_AT_HOME_FLAG(joint)) {
		/* joint should have been homed at this step, it is no longer
		   homing, but its not at home - must have failed.  bail out */
		emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE;
		emcmotStatus->homing_active = 0;
		return;
	    }
	}
	if(!seen) {
	    /* all joints at this step have finished homing, move on to next step */
	    home_sequence ++;
	    emcmotStatus->homingSequenceState = HOME_SEQUENCE_START_JOINTS;
	}
	break;
    default:
	/* should never get here */
	reportError(_("unknown state '%d' during homing sequence"),
	    emcmotStatus->homingSequenceState);
	emcmotStatus->homingSequenceState = HOME_SEQUENCE_IDLE;
	emcmotStatus->homing_active = 0;
	break;
    }
}

void do_homing(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    double offset, tmp;
    int home_sw_active, homing_flag;

    homing_flag = 0;
    if (emcmotStatus->motion_state != EMCMOT_MOTION_FREE) {
	/* can't home unless in free mode */
	return;
    }
    /* loop thru joints, treat each one individually */
    for (joint_num = 0; joint_num < num_joints; joint_num++) {
	/* point to joint struct */
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, skip it */
	    continue;
	}
	home_sw_active = GET_JOINT_HOME_SWITCH_FLAG(joint);
	if (joint->home_state != HOME_IDLE) {
	    homing_flag = 1; /* at least one joint is homing */
	}
	
	/* when an joint is homing, 'check_for_faults()' ignores its limit
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
		if (joint->home_flags & HOME_IS_SHARED && home_sw_active) {
		    reportError(
			_("Cannot home while shared home switch is closed"));
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
		if (joint->home_flags & HOME_UNLOCK_FIRST) {
		    joint->home_state = HOME_UNLOCK;
		} else {
		    joint->home_state = HOME_UNLOCK_WAIT;
		    immediate_state = 1;
		}		     
		break;

	    case HOME_UNLOCK:
		// unlock now
		emcmotSetRotaryUnlock(joint_num, 1);
		joint->home_state = HOME_UNLOCK_WAIT;
		break;

	    case HOME_UNLOCK_WAIT:
		// if not yet unlocked, continue waiting
		if ((joint->home_flags & HOME_UNLOCK_FIRST) && 
		    !emcmotGetRotaryIsUnlocked(joint_num)) break;

		// either we got here without an unlock needed, or the
		// unlock is now complete.
		if (joint->home_search_vel == 0.0) {
		    if (joint->home_latch_vel == 0.0) {
			/* both vels == 0 means home at current position */
			joint->home_state = HOME_SET_SWITCH_POSITION;
			immediate_state = 1;
		    } else if (joint->home_flags & HOME_USE_INDEX) {
			/* home using index pulse only */
			joint->home_state = HOME_INDEX_ONLY_START;
			immediate_state = 1;
		    } else {
			reportError(_("invalid homing config: non-zero LATCH_VEL needs either SEARCH_VEL or USE_INDEX"));
			joint->home_state = HOME_IDLE;
		    }
		} else {
		    if (joint->home_latch_vel != 0.0) {
			/* need to find home switch */
			joint->home_state = HOME_INITIAL_SEARCH_START;
			immediate_state = 1;
		    } else {
			reportError(_("invalid homing config: non-zero SEARCH_VEL needs LATCH_VEL"));
			joint->home_state = HOME_IDLE;
		    }
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
		if (! home_sw_active) {
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
		if (home_sw_active) {
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
		if (home_sw_active) {
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
		offset = *(joint->home_offset) - joint->pos_fb;
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
		} else {
		    /* search and latch vel are opposite directions */
		    joint->home_state = HOME_FALL_SEARCH_START;
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
		/* we should still be on the switch */
		if (! home_sw_active) {
		    reportError(
			_("Home switch inactive before start of backoff move"));
		    joint->home_state = HOME_IDLE;
		    break;
		}
		/* set up a move at '-search_vel' to back off of switch */
		home_start_move(joint, -joint->home_search_vel);
		/* next state */
		joint->home_state = HOME_FINAL_BACKOFF_WAIT;
		break;

	    case HOME_FINAL_BACKOFF_WAIT:
		/* This state is called while the machine is moving off of
		   the home switch after finding its approximate location.
		   It terminates when the switch is cleared successfully.  If
		   the move ends or hits a limit before it clears the switch,
		   the home is aborted. */
		/* are we off home switch yet? */
		if (! home_sw_active) {
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
		/* we should still be off of the switch */
		if (home_sw_active) {
		    reportError(
			_("Home switch active before start of latch move"));
		    joint->home_state = HOME_IDLE;
		    break;
		}
		/* set up a move at 'latch_vel' to locate the switch */
		home_start_move(joint, joint->home_latch_vel);
		/* next state */
		joint->home_state = HOME_RISE_SEARCH_WAIT;
		break;

	    case HOME_RISE_SEARCH_WAIT:
		/* This state is called while the machine is moving towards
		   the home switch on its final, low speed pass.  It
		   terminates when the switch is detected. If the move ends
		   or hits a limit before it hits the switch, the home is
		   aborted. */
		/* have we hit the home switch yet? */
		if (home_sw_active) {
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
		/* we should still be on the switch */
		if (!home_sw_active) {
		    reportError(
			_("Home switch inactive before start of latch move"));
		    joint->home_state = HOME_IDLE;
		    break;
		}
		/* set up a move at 'latch_vel' to locate the switch */
		home_start_move(joint, joint->home_latch_vel);
		/* next state */
		joint->home_state = HOME_FALL_SEARCH_WAIT;
		break;

	    case HOME_FALL_SEARCH_WAIT:
		/* This state is called while the machine is moving away from
		   the home switch on its final, low speed pass.  It
		   terminates when the switch is cleared. If the move ends or
		   hits a limit before it clears the switch, the home is
		   aborted. */
		/* have we cleared the home switch yet? */
		if (!home_sw_active) {
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
		offset = *(joint->home_offset) - joint->pos_fb;
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

	    case HOME_INDEX_ONLY_START:
		/* This state is used if the machine has been pre-positioned
		   near the home position, and simply needs to find the
		   next index pulse.  It starts a move at latch_vel, and
		   sets index-enable, which tells the encoder driver to
		   reset its counter to zero and clear the enable when the
		   next index pulse arrives. */
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
		/* Although we don't know the exact home position yet, we
		   we reset the joint coordinates now so that screw error
		   comp will be appropriate for this portion of the screw
		   (previously we didn't know where we were at all). */
		/* set the current position to 'home_offset' */
		offset = *(joint->home_offset) - joint->pos_fb;
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_pos_cmd += offset;
		joint->motor_offset -= offset;
		/* set the index enable */
		joint->index_enable = 1;
		/* set up a move at 'latch_vel' to find the index pulse */
		home_start_move(joint, joint->home_latch_vel);
		/* next state */
		joint->home_state = HOME_INDEX_SEARCH_WAIT;
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
		joint->motor_offset = - *(joint->home_offset);
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
		joint->free_pos_cmd = *(joint->home);
		/* do the move at max speed */
		/* if home_vel is set (>0) then we use that, otherwise we rapid there */
		if (joint->home_final_vel > 0) {
		    joint->free_vel_lim = rtapi_fabs(joint->home_final_vel);
		    /* clamp on max vel for this joint */
		    if (joint->free_vel_lim > joint->vel_limit)
			joint->free_vel_lim = joint->vel_limit;
		} else { 
		    joint->free_vel_lim = joint->vel_limit;
		}
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
		    joint->home_state = HOME_LOCK;
		    immediate_state = 1;
		    break;
		}
		if (joint->on_pos_limit || joint->on_neg_limit) {
		    /* on limit, check to see if we should trip */
		    if (!(joint->home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			reportError(_("hit limit in home state %d"),
			    joint->home_state);
			joint->home_state = HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		break;

	    case HOME_LOCK:
		if (joint->home_flags & HOME_UNLOCK_FIRST) {
		    emcmotSetRotaryUnlock(joint_num, 0);
		} else {
		    immediate_state = 1;
		}
		joint->home_state = HOME_LOCK_WAIT;
		break;

	    case HOME_LOCK_WAIT:
		// if not yet locked, continue waiting
		if ((joint->home_flags & HOME_UNLOCK_FIRST) && 
		    emcmotGetRotaryIsUnlocked(joint_num)) break;

		// either we got here without a lock needed, or the
		// lock is now complete.
		joint->home_state = HOME_FINISHED;
		immediate_state = 1;
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
		reportError(_("unknown state '%d' during homing"),
		    joint->home_state);
		joint->home_state = HOME_ABORT;
		immediate_state = 1;
		break;
	    }	/* end of switch(joint->home_state) */
	} while (immediate_state);
    }	/* end of loop through all joints */

    if ( homing_flag ) {
	/* at least one joint is homing, set global flag */
	emcmotStatus->homing_active = 1;
    } else {
	/* is a homing sequence in progress? */
	if (emcmotStatus->homingSequenceState == HOME_SEQUENCE_IDLE) {
	    /* no, single joint only, we're done */
	    emcmotStatus->homing_active = 0;
	}
    }
}

