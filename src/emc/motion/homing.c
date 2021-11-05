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
#include "rtapi_math.h"
#include "motion.h"
#include "hal.h"
#include "mot_priv.h"
#include "homing.h"

#define ABS(x) (((x) < 0) ? -(x) : (x))

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

#define MAX_HOME_SEQUENCES EMCMOT_MAX_JOINTS

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

// home sequences (some states are required)
static home_sequence_state_t sequence_state;
static int  home_sequence = -1;
static bool homing_active;

/* internal states for homing */
typedef enum {
  HOME_IDLE = 0,
  HOME_START,// 1
  HOME_UNLOCK,// 2
  HOME_UNLOCK_WAIT,// 3
  HOME_INITIAL_BACKOFF_START,// 4
  HOME_INITIAL_BACKOFF_WAIT,// 5
  HOME_INITIAL_SEARCH_START,// 6
  HOME_INITIAL_SEARCH_WAIT,// 7
  HOME_SET_COARSE_POSITION,// 8
  HOME_FINAL_BACKOFF_START,// 9
  HOME_FINAL_BACKOFF_WAIT,// 10
  HOME_RISE_SEARCH_START,// 11
  HOME_RISE_SEARCH_WAIT,// 12
  HOME_FALL_SEARCH_START,// 13
  HOME_FALL_SEARCH_WAIT,// 14
  HOME_SET_SWITCH_POSITION,// 15
  HOME_INDEX_ONLY_START,// 16
  HOME_INDEX_SEARCH_START,// 17
  HOME_INDEX_SEARCH_WAIT,// 18
  HOME_SET_INDEX_POSITION,// 19
  HOME_FINAL_MOVE_START,// 20
  HOME_FINAL_MOVE_WAIT,// 21
  HOME_LOCK,// 22
  HOME_LOCK_WAIT,// 23
  HOME_FINISHED,// 24
  HOME_ABORT// 25
} home_state_t;

static int  immediate_state;

// local per-joint data (includes hal pin data)
typedef struct {
  home_state_t home_state;           // OUT pin
  bool         homing;               // OUT pin
  bool         homed;                // OUT pin
  bool         home_sw;              // IN  pin
  bool         index_enable;         // IO  pin
  bool         at_home;
  bool         sync_final_move;      // joints with neg sequence
  bool         joint_in_sequence;
  int          pause_timer;
  double       home_offset;          // intfc, updateable
  double       home;                 // intfc, updateable
  double       home_final_vel;       // intfc
  double       home_search_vel;      // intfc
  double       home_latch_vel;       // intfc
  int          home_flags;           // intfc
  int          home_sequence;        // intfc, updateable
  bool         volatile_home;        // intfc
  bool         home_is_synchronized;
} home_local_data;

static  home_local_data H[EMCMOT_MAX_JOINTS];

// data for per-joint homing-specific hal pins:
typedef struct {
    hal_bit_t *home_sw;      // home switch input
    hal_bit_t *homing;       // joint is homing
    hal_bit_t *homed;        // joint was homed
    hal_bit_t *index_enable; // motmod sets: request reset on index
                             //        encoder clears: index arrived
    hal_s32_t *home_state;   // homing state machine state
} one_joint_home_data_t;

typedef struct {
    one_joint_home_data_t jhd[EMCMOT_MAX_JOINTS];
} all_joints_home_data_t;

static all_joints_home_data_t *joint_home_data = 0;

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
	joint->free_tp.pos_cmd = joint->pos_cmd + 2.0 * joint_range;
    } else {
	joint->free_tp.pos_cmd = joint->pos_cmd - 2.0 * joint_range;
    }
    if (fabs(vel) < joint->vel_limit) {
	joint->free_tp.max_vel = fabs(vel);
    } else {
        /* clamp on max vel for this joint */
	joint->free_tp.max_vel = joint->vel_limit;
    }
    /* start the move */
    joint->free_tp.enable = 1;
} // home_start_move()

/* 'home_do_moving_checks()' is called from states where the machine
   is supposed to be moving.  It checks to see if the machine has
   hit a limit, or if the move has stopped.  (Normally such moves
   will be terminated by the home switch or an index pulse or some
   other event, if the move goes to completion, something is wrong.) */
static void home_do_moving_checks(emcmot_joint_t * joint,int jno)
{
    /* check for limit switches */
    if (joint->on_pos_limit || joint->on_neg_limit) {
	/* on limit, check to see if we should trip */
	if (!(H[jno].home_flags & HOME_IGNORE_LIMITS)) {
	    /* not ignoring limits, time to quit */
	    reportError(_("hit limit in home state %d"), H[jno].home_state);
            H[jno].home_state = HOME_ABORT;
	    immediate_state = 1;
	    return;
	}
    }
    /* check for reached end of move */
    if (!joint->free_tp.active) {
	/* reached end of move without hitting switch */
	joint->free_tp.enable = 0;
	reportError(_("end of move in home state %d"), H[jno].home_state);
        H[jno].home_state = HOME_ABORT;
	immediate_state = 1;
	return;
    }
} // home_do_moving_checks()

static void update_home_is_synchronized(void) {
    // invoke anytime H[*].home_sequence is altered
    int jno,joint_num;

    // first, clear all H[*].home_is_synchronized
    for (jno = 0; jno < ALL_JOINTS; jno++) {
        H[jno].home_is_synchronized = 0;
    }
    for (jno = 0; jno < ALL_JOINTS; jno++) {
        if (H[jno].home_sequence < 0) {
            H[jno].home_is_synchronized = 1;
            continue;
        }
        for (joint_num = 0; joint_num < ALL_JOINTS; joint_num++) {
            if (joint_num == jno) continue;
            if (   (    H[joint_num].home_sequence < 0)
                && (ABS(H[joint_num].home_sequence) == H[jno].home_sequence) )  {
                H[jno].home_is_synchronized = 1;
                H[joint_num].home_is_synchronized = 1;
            }
        }
    }
}
/***********************************************************************
*                      PUBLIC FUNCTIONS                                *
************************************************************************/

void homing_init(void)
{
    int i;
    homing_active = 0;
    for (i=0; i < EMCMOT_MAX_JOINTS; i++) {
        H[i].home_state      =  HOME_IDLE;
        H[i].home_search_vel =  0;
        H[i].home_latch_vel  =  0;
        H[i].home_final_vel  =  0;
        H[i].home_offset     =  0;
        H[i].home            =  0;
        H[i].home_flags      =  0;
        H[i].home_sequence   = -1;
        H[i].volatile_home   =  0;
    }
}

int export_joint_home_pins(int njoints,int id)
{
    int jno,retval;
    one_joint_home_data_t *addr;

    joint_home_data = hal_malloc(sizeof(all_joints_home_data_t));
    if (joint_home_data == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, _("HOMING: all_joints_home_data_t malloc failed\n"));
        return -1;
    }

    retval = 0;
    for (jno = 0; jno < njoints; jno++) {
        addr = &(joint_home_data->jhd[jno]);

        if ((retval = hal_pin_bit_newf(HAL_IN, &(addr->home_sw), id,
                      "joint.%d.home-sw-in", jno)) != 0) break;
        if ((retval = hal_pin_bit_newf(HAL_OUT, &(addr->homing), id,
                      "joint.%d.homing", jno)) != 0) break;
        if ((retval = hal_pin_bit_newf(HAL_OUT, &(addr->homed), id,
                      "joint.%d.homed", jno)) != 0) break;
        if ((retval = hal_pin_s32_newf(HAL_OUT, &(addr->home_state), id,
                      "joint.%d.home-state", jno)) != 0) break;
        if ((retval = hal_pin_bit_newf(HAL_IO, &(addr->index_enable), id,
                      "joint.%d.index-enable", jno)) != 0) break;
    }
    return retval;
} // export_joint_home_pins()

void read_homing_in_pins(int njoints)
{
    int jno;
    one_joint_home_data_t *addr;
    for (jno = 0; jno < njoints; jno++) {
        addr = &(joint_home_data->jhd[jno]);
        H[jno].home_sw      = *(addr->home_sw);      // IN
        H[jno].index_enable = *(addr->index_enable); // IO
    }
}

void write_homing_out_pins(int njoints)
{
    int jno;
    one_joint_home_data_t *addr;
    for (jno = 0; jno < njoints; jno++) {
        addr = &(joint_home_data->jhd[jno]);
        *(addr->homing)       = H[jno].homing;       // OUT
        *(addr->homed)        = H[jno].homed;        // OUT
        *(addr->home_state)   = H[jno].home_state;   // OUT
        *(addr->index_enable) = H[jno].index_enable; // IO
    }
}

void set_home_sequence_state(home_sequence_state_t s_state) {
   sequence_state = s_state;
}

void set_home_abort(int jno) {
    H[jno].home_state = HOME_ABORT;
}

void set_home_start(int jno) {
    H[jno].home_state = HOME_START;
}

void set_home_idle(int jno) {
    H[jno].home_state = HOME_IDLE;
}

void set_joint_homing(int jno,bool value) {
    H[jno].homing = value;
}

void set_joint_homed(int jno,bool value) {
    H[jno].homed = value;
}

void set_joint_at_home(int jno, bool value) {
    H[jno].at_home = value;
}

void set_joint_homing_params(int    jno,
                             double offset,
                             double home,
                             double home_final_vel,
                             double home_search_vel,
                             double home_latch_vel,
                             int    home_flags,
                             int    home_sequence,
                             bool   volatile_home
                             )
{
    H[jno].home_offset     = offset;
    H[jno].home            = home;
    H[jno].home_final_vel  = home_final_vel;
    H[jno].home_search_vel = home_search_vel;
    H[jno].home_latch_vel  = home_latch_vel;
    H[jno].home_flags      = home_flags;
    H[jno].home_sequence   = home_sequence;
    H[jno].volatile_home   = volatile_home;
    update_home_is_synchronized();
}

void update_joint_homing_params (int    jno,
                                 double offset,
                                 double home,
                                 int    home_sequence
                                )
{
    H[jno].home_offset   = offset;
    H[jno].home          = home;
    H[jno].home_sequence = home_sequence;
    update_home_is_synchronized();
}

bool get_homing_is_active() {
    return homing_active;
}

int get_home_sequence(int jno) {
    return H[jno].home_sequence;
}

bool get_homing(int jno) {
    return H[jno].homing;
}

bool get_homed(int jno) {
    return H[jno].homed;
}

bool get_index_enable(int jno) {
     return H[jno].index_enable;
}

bool get_home_is_volatile(int jno) {
    return H[jno].volatile_home;
}

bool get_home_needs_unlock_first(int jno) {
    return (H[jno].home_flags & HOME_UNLOCK_FIRST) ? 1 : 0;
}

bool get_home_is_idle(int jno) {
    return H[jno].home_state == HOME_IDLE ? 1 : 0;
}

bool get_homing_at_index_search_wait(int jno) {
    return H[jno].home_state == HOME_INDEX_SEARCH_WAIT ? 1 : 0;
}

home_sequence_state_t get_home_sequence_state(void) {
   return sequence_state;
}

// SEQUENCE management
void do_homing_sequence(void)
{
    int i,ii;
    int special_case_sync_all;
    int seen = 0;
    emcmot_joint_t *joint;
    int sequence_is_set = 0;
    /* first pass init */
    if(home_sequence == -1) {
        sequence_state = HOME_SEQUENCE_IDLE;
	home_sequence = 0;
    }

    switch( sequence_state ) {
    case HOME_SEQUENCE_IDLE:
	/* nothing to do */
	break;

    case HOME_SEQUENCE_DO_ONE_JOINT:
        // Expect one joint with home_state==HOME_START
        for (i=0; i < ALL_JOINTS; i++) {
            joint = &joints[i];
            if (H[i].home_state == HOME_START) {
               H[i].joint_in_sequence = 1;
               home_sequence = ABS(H[i].home_sequence);
            } else {
               if (H[i].joint_in_sequence) {
                   // it may already be running, leave alone
               } else {
                   H[i].joint_in_sequence = 0;
               }
            }
        }
        sequence_is_set = 1;
        //drop through----drop through----drop through----drop through

    case HOME_SEQUENCE_DO_ONE_SEQUENCE:
        // Expect multiple joints with home_state==HOME_START
        // specified by a negative sequence
        // Determine home_sequence and set H[i].joint_in_sequence
        // based on home_state[i] == HOME_START
        if (!sequence_is_set) {
            for (i=0; i < ALL_JOINTS; i++) {
                joint = &joints[i];
                if (H[i].home_state == HOME_START) {
                    if (   sequence_is_set
                        && (ABS(H[i].home_sequence) != home_sequence)) {
    	                reportError(
                        "homing.c Unexpected joint=%d jsequence=%d seq=%d\n"
                        ,i,H[i].home_sequence,home_sequence);
                    }
                    home_sequence = ABS(H[i].home_sequence);
                    sequence_is_set = 1;
                }
                H[i].joint_in_sequence = 1; //disprove
                if  (   (H[i].home_state != HOME_START)
                     || (home_sequence     != ABS(H[i].home_sequence))
                    ) {
                    H[i].joint_in_sequence = 0;
                } 
            }
        }
        sequence_state = HOME_SEQUENCE_START;

        //drop through----drop through----drop through----drop through

    case HOME_SEQUENCE_START:
        // Request to home all joints or a single sequence
        // A negative H[i].home_sequence means sync final move
        if (!sequence_is_set) {
            // sequence_is_set not otherwise established: home-all 
            for (i=0; i < EMCMOT_MAX_JOINTS; i++) {
                joint = &joints[i];
                H[i].joint_in_sequence = 1;
                // unspecified joints have an unrealizable home_sequence:
                if (H[i].home_sequence >100) {
                   // docs: 'If HOME_SEQUENCE is not specified then this joint
                   //        will not be homed by the HOME ALL sequence'
                   H[i].joint_in_sequence = 0;  // per docs
                }
            }
            sequence_is_set = 1;
            home_sequence = 0;
        }
        /* Initializations */
        for(i=0; i < MAX_HOME_SEQUENCES; i++) {
            H[i].sync_final_move = 0; //reset to allow a rehome
        }
        for(i=0; i < ALL_JOINTS; i++) {
            if (!H[i].joint_in_sequence) continue;
            joint = &joints[i];
            if (   (H[i].home_flags & HOME_NO_REHOME)
                &&  H[i].homed
               ) {
                continue;
            } else {
                H[i].homed = 0;
            }
            if (H[i].home_sequence < 0) {
                // if a H[i].home_sequence is neg, find all joints that
                // have the same ABS sequence value and make them the same
                for(ii=0; ii < ALL_JOINTS; ii++) {
                    if (H[ii].home_sequence == ABS(H[i].home_sequence)) {
                        H[ii].home_sequence =      H[i].home_sequence;
                    }
                }
            }
        }
        /*  special_case_sync_all: if home_sequence == -1 for all joints
        *                          synchronize all joints final move
        */
        special_case_sync_all = 1; // disprove
        for(i=0; i < ALL_JOINTS; i++) {
            joint = &joints[i];
            if (H[i].home_sequence != -1) {special_case_sync_all = 0;}
        }
        if (special_case_sync_all) {
            home_sequence = 1;
        }
	for(i=0; i < ALL_JOINTS; i++) {
            if (!H[i].joint_in_sequence) continue;
	    joint = &joints[i];
	    if  ( H[i].home_state != HOME_IDLE && H[i].home_state != HOME_START) {
		/* a home is already in progress, abort the home-all */
		sequence_state = HOME_SEQUENCE_IDLE;
		return;
	    }
	}
	/* tell the world we're on the job */
	homing_active = 1;
        //drop through----drop through----drop through----drop through

    case HOME_SEQUENCE_START_JOINTS:
	/* start all joints whose sequence number matches home_sequence */
	for(i=0; i < ALL_JOINTS; i++) {
	    joint = &joints[i];
	    if(ABS(H[i].home_sequence) == home_sequence) {
                if (!H[i].joint_in_sequence) continue;
		/* start this joint */
	        joint->free_tp.enable = 0;
		H[i].home_state = HOME_START;
		seen++;
	    }
	}
	if (seen || home_sequence == 0) {
	    sequence_state = HOME_SEQUENCE_WAIT_JOINTS;
	} else {
	    /* no joints have this sequence number, we're done */
	    sequence_state = HOME_SEQUENCE_IDLE;
	    /* tell the world */
	    homing_active = 0;
	}
	break;

    case HOME_SEQUENCE_WAIT_JOINTS:
	for(i=0; i < ALL_JOINTS; i++) {
            if (!H[i].joint_in_sequence) continue;
	    joint = &joints[i];
            // negative H[i].home_sequence means sync final move
	    if(ABS(H[i].home_sequence) != home_sequence) {
		/* this joint is not at the current sequence number, ignore it */
		continue;
	    }
	    if(H[i].home_state != HOME_IDLE) {
		/* still busy homing, keep waiting */
		seen = 1;
		continue;
	    }
	    if (!H[i].at_home) {
		/* joint should have been homed at this step, it is no longer
		   homing, but its not at home - must have failed.  bail out */
		sequence_state = HOME_SEQUENCE_IDLE;
		homing_active = 0;
		return;
	    }
	}
        if(!seen) {
            /* all joints at this step have finished, move on to next step */
            home_sequence ++;
            sequence_state = HOME_SEQUENCE_START_JOINTS;
        }
	break;

    default:
	/* should never get here */
	reportError(_("unknown state '%d' during homing sequence"),
            sequence_state);
	sequence_state = HOME_SEQUENCE_IDLE;
	homing_active = 0;
	break;
    }
} // do_homing_sequence()

// HOMING management
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
    for (joint_num = 0; joint_num < ALL_JOINTS; joint_num++) {
        if (!H[joint_num].joint_in_sequence) continue;
	/* point to joint struct */
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, skip it */
	    continue;
	}
	home_sw_active = H[joint_num].home_sw;
	if (H[joint_num].home_state != HOME_IDLE) {
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
	    switch (H[joint_num].home_state) {
	    case HOME_IDLE:
		/* nothing to do */
		break;

	    case HOME_START:
		/* This state is responsible for getting the homing process
		   started.  It doesn't actually do anything, it simply
		   determines what state is next */
		if (H[joint_num].home_flags & HOME_IS_SHARED && home_sw_active) {
		    reportError(
			_("Cannot home while shared home switch is closed j=%d"),
                         joint_num);
		    H[joint_num].home_state = HOME_IDLE;
		    break;
		}
		/* set flags that communicate with the rest of EMC */
                if (   (H[joint_num].home_flags & HOME_NO_REHOME)
                    &&  H[joint_num].homed
                   ) {
                   H[joint_num].home_state = HOME_IDLE;
                   break; //no rehome allowed if absolute_enoder
                } else {
                    H[joint_num].homing = 1;
                    H[joint_num].homed = 0;
                }
		H[joint_num].at_home = 0;
		/* stop any existing motion */
		joint->free_tp.enable = 0;
		/* reset delay counter */
		H[joint_num].pause_timer = 0;
		/* figure out exactly what homing sequence is needed */
                if (H[joint_num].home_flags & HOME_ABSOLUTE_ENCODER) {
                    H[joint_num].home_flags &= ~HOME_IS_SHARED; // shared not applicable
                    H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                    // Note: H[joint_num].homed
                    // is not set in case there is a final move requested
		    break;
                }
		if (H[joint_num].home_flags & HOME_UNLOCK_FIRST) {
		    H[joint_num].home_state = HOME_UNLOCK;
		} else {
		    H[joint_num].home_state = HOME_UNLOCK_WAIT;
		    immediate_state = 1;
		}		     
		break;

	    case HOME_UNLOCK:
		// unlock now
		emcmotSetRotaryUnlock(joint_num, 1);
		H[joint_num].home_state = HOME_UNLOCK_WAIT;
		break;

	    case HOME_UNLOCK_WAIT:
		// if not yet unlocked, continue waiting
		if ((H[joint_num].home_flags & HOME_UNLOCK_FIRST) &&
		    !emcmotGetRotaryIsUnlocked(joint_num)) break;

		// either we got here without an unlock needed, or the
		// unlock is now complete.
		if (H[joint_num].home_search_vel == 0.0) {
		    if (H[joint_num].home_latch_vel == 0.0) {
			/* both vels == 0 means home at current position */
			H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
			immediate_state = 1;
		    } else if (H[joint_num].home_flags & HOME_USE_INDEX) {
			/* home using index pulse only */
			H[joint_num].home_state = HOME_INDEX_ONLY_START;
			immediate_state = 1;
		    } else {
			reportError(_("invalid homing config: non-zero LATCH_VEL needs either SEARCH_VEL or USE_INDEX"));
			H[joint_num].home_state = HOME_IDLE;
		    }
		} else {
		    if (H[joint_num].home_latch_vel != 0.0) {
			/* need to find home switch */
			H[joint_num].home_state = HOME_INITIAL_SEARCH_START;
			immediate_state = 1;
		    } else {
			reportError(_("invalid homing config: non-zero SEARCH_VEL needs LATCH_VEL"));
			H[joint_num].home_state = HOME_IDLE;
		    }
		}
		break;

	    case HOME_INITIAL_BACKOFF_START:
		/* This state is called if the homing sequence starts at a
		   location where the home switch is already tripped. It
		   starts a move away from the switch. */
		/* is the joint still moving? */
		if (joint->free_tp.active) {
		    /* yes, reset delay, wait until joint stops */
		    H[joint_num].pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (H[joint_num].pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    H[joint_num].pause_timer++;
		    break;
		}
		H[joint_num].pause_timer = 0;
		/* set up a move at '-search_vel' to back off of switch */
		home_start_move(joint, - H[joint_num].home_search_vel);
		/* next state */
		H[joint_num].home_state = HOME_INITIAL_BACKOFF_WAIT;
		break;

	    case HOME_INITIAL_BACKOFF_WAIT:
		/* This state is called while the machine is moving off of
		   the home switch.  It terminates when the switch is cleared
		   successfully.  If the move ends or hits a limit before it
		   clears the switch, the home is aborted. */
		/* are we off home switch yet? */
		if (! home_sw_active) {
		    /* yes, stop motion */
		    joint->free_tp.enable = 0;
		    /* begin initial search */
		    H[joint_num].home_state = HOME_INITIAL_SEARCH_START;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint,joint_num);
		break;

	    case HOME_INITIAL_SEARCH_START:
		/* This state is responsible for starting a move toward the
		   home switch.  This move is at 'search_vel', which can be
		   fairly fast, because once the switch is found another
		   slower move will be used to set the exact home position. */
		/* is the joint already moving? */
		if (joint->free_tp.active) {
		    /* yes, reset delay, wait until joint stops */
		    H[joint_num].pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (H[joint_num].pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    H[joint_num].pause_timer++;
		    break;
		}
		H[joint_num].pause_timer = 0;
		/* make sure we aren't already on home switch */
		if (home_sw_active) {
		    /* already on switch, need to back off it first */
		    H[joint_num].home_state = HOME_INITIAL_BACKOFF_START;
		    immediate_state = 1;
		    break;
		}
		/* set up a move at 'search_vel' to find switch */
		home_start_move(joint, H[joint_num].home_search_vel);
		/* next state */
		H[joint_num].home_state = HOME_INITIAL_SEARCH_WAIT;
		break;

	    case HOME_INITIAL_SEARCH_WAIT:
		/* This state is called while the machine is looking for the
		   home switch.  It terminates when the switch is found.  If
		   the move ends or hits a limit before it finds the switch,
		   the home is aborted. */
		/* have we hit home switch yet? */
		if (home_sw_active) {
		    /* yes, stop motion */
		    joint->free_tp.enable = 0;
		    /* go to next step */
		    H[joint_num].home_state = HOME_SET_COARSE_POSITION;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint,joint_num);
		break;

	    case HOME_SET_COARSE_POSITION:
		/* This state is called after the first time the switch is
		   found.  At this point, we are approximately home. Although
		   we will do another slower pass to get the exact home
		   location, we reset the joint coordinates now so that screw
		   error comp will be appropriate for this portion of the
		   screw (previously we didn't know where we were at all). */
		/* set the current position to 'home_offset' */
		offset = H[joint_num].home_offset - joint->pos_fb;
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_tp.curr_pos += offset;
		joint->motor_offset -= offset;
		/* The next state depends on the signs of 'search_vel' and
		   'latch_vel'.  If they are the same, that means we must
		   back up, then do the final homing moving the same
		   direction as the initial search, on a rising edge of the
		   switch.  If they are opposite, it means that the final
		   homing will take place on a falling edge as the machine
		   moves off of the switch. */
		tmp = H[joint_num].home_search_vel * H[joint_num].home_latch_vel;
		if (tmp > 0.0) {
		    /* search and latch vel are same direction */
		    H[joint_num].home_state = HOME_FINAL_BACKOFF_START;
		} else {
		    /* search and latch vel are opposite directions */
		    H[joint_num].home_state = HOME_FALL_SEARCH_START;
		}
		immediate_state = 1;
		break;

	    case HOME_FINAL_BACKOFF_START:
		/* This state is called once the approximate location of the
		   switch has been found.  It is responsible for starting a
		   move that will back off of the switch in preparation for a
		   final slow move that captures the exact switch location. */
		/* is the joint already moving? */
		if (joint->free_tp.active) {
		    /* yes, reset delay, wait until joint stops */
		    H[joint_num].pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (H[joint_num].pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    H[joint_num].pause_timer++;
		    break;
		}
		H[joint_num].pause_timer = 0;
		/* we should still be on the switch */
		if (! home_sw_active) {
		    reportError(
			_("Home switch inactive before start of backoff move j=%d"),
                         joint_num);
		    H[joint_num].home_state = HOME_IDLE;
		    break;
		}
		/* set up a move at '-search_vel' to back off of switch */
		home_start_move(joint, - H[joint_num].home_search_vel);
		/* next state */
		H[joint_num].home_state = HOME_FINAL_BACKOFF_WAIT;
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
		    joint->free_tp.enable = 0;
		    /* begin final search */
		    H[joint_num].home_state = HOME_RISE_SEARCH_START;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint,joint_num);
		break;

	    case HOME_RISE_SEARCH_START:
		/* This state is called to start the final search for the
		   point where the home switch trips.  It moves at
		   'latch_vel' and looks for a rising edge on the switch */
		/* is the joint already moving? */
		if (joint->free_tp.active) {
		    /* yes, reset delay, wait until joint stops */
		    H[joint_num].pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (H[joint_num].pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    H[joint_num].pause_timer++;
		    break;
		}
		H[joint_num].pause_timer = 0;
		/* we should still be off of the switch */
		if (home_sw_active) {
		    reportError(
			_("Home switch active before start of latch move j=%d"),
                         joint_num);
		    H[joint_num].home_state = HOME_IDLE;
		    break;
		}
		/* set up a move at 'latch_vel' to locate the switch */
		home_start_move(joint, H[joint_num].home_latch_vel);
		/* next state */
		H[joint_num].home_state = HOME_RISE_SEARCH_WAIT;
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
		    if (H[joint_num].home_flags & HOME_USE_INDEX) {
			/* look for index pulse */
			H[joint_num].home_state = HOME_INDEX_SEARCH_START;
			immediate_state = 1;
			break;
		    } else {
			/* no index pulse, stop motion */
			joint->free_tp.enable = 0;
			/* go to next step */
			H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
			immediate_state = 1;
			break;
		    }
		}
		home_do_moving_checks(joint,joint_num);
		break;

	    case HOME_FALL_SEARCH_START:
		/* This state is called to start the final search for the
		   point where the home switch releases.  It moves at
		   'latch_vel' and looks for a falling edge on the switch */
		/* is the joint already moving? */
		if (joint->free_tp.active) {
		    /* yes, reset delay, wait until joint stops */
		    H[joint_num].pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (H[joint_num].pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    H[joint_num].pause_timer++;
		    break;
		}
		H[joint_num].pause_timer = 0;
		/* we should still be on the switch */
		if (!home_sw_active) {
		    reportError(
			_("Home switch inactive before start of latch move j=%d"),
                         joint_num);
		    H[joint_num].home_state = HOME_IDLE;
		    break;
		}
		/* set up a move at 'latch_vel' to locate the switch */
		home_start_move(joint, H[joint_num].home_latch_vel);
		/* next state */
		H[joint_num].home_state = HOME_FALL_SEARCH_WAIT;
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
		    if (H[joint_num].home_flags & HOME_USE_INDEX) {
			/* look for index pulse */
			H[joint_num].home_state = HOME_INDEX_SEARCH_START;
			immediate_state = 1;
			break;
		    } else {
			/* no index pulse, stop motion */
			joint->free_tp.enable = 0;
			/* go to next step */
			H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
			immediate_state = 1;
			break;
		    }
		}
		home_do_moving_checks(joint,joint_num);
		break;

	    case HOME_SET_SWITCH_POSITION:
		/* This state is called when the machine has determined the
		   switch position as accurately as possible.  It sets the
		   current joint position to 'home_offset', which is the
		   location of the home switch in joint coordinates. */
		/* set the current position to 'home_offset' */
                if (H[joint_num].home_flags & HOME_ABSOLUTE_ENCODER) {
                    offset = H[joint_num].home_offset;
                } else {
                    offset = H[joint_num].home_offset - joint->pos_fb;
                }
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_tp.curr_pos += offset;
		joint->motor_offset -= offset;
                if (H[joint_num].home_flags & HOME_ABSOLUTE_ENCODER) {
                    if (H[joint_num].home_flags & HOME_NO_FINAL_MOVE) {
                        H[joint_num].home_state = HOME_FINISHED;
                        immediate_state = 1;
                        H[joint_num].homed = 1;
                        break;
                    }
                }
		/* next state */
		H[joint_num].home_state = HOME_FINAL_MOVE_START;
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
		if (joint->free_tp.active) {
		    /* yes, reset delay, wait until joint stops */
		    H[joint_num].pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (H[joint_num].pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    H[joint_num].pause_timer++;
		    break;
		}
		H[joint_num].pause_timer = 0;
		/* Although we don't know the exact home position yet, we
		   we reset the joint coordinates now so that screw error
		   comp will be appropriate for this portion of the screw
		   (previously we didn't know where we were at all). */
		/* set the current position to 'home_offset' */
		offset = H[joint_num].home_offset - joint->pos_fb;
		/* this moves the internal position but does not affect the
		   motor position */
		joint->pos_cmd += offset;
		joint->pos_fb += offset;
		joint->free_tp.curr_pos += offset;
		joint->motor_offset -= offset;
		/* set the index enable */
		H[joint_num].index_enable = 1;
		/* set up a move at 'latch_vel' to find the index pulse */
		home_start_move(joint, H[joint_num].home_latch_vel);
		/* next state */
		H[joint_num].home_state = HOME_INDEX_SEARCH_WAIT;
		break;

	    case HOME_INDEX_SEARCH_START:
		/* This state is called after the machine has made a low
		   speed pass to determine the limit switch location. It
		   sets index-enable, which tells the encoder driver to
		   reset its counter to zero and clear the enable when the
		   next index pulse arrives. */
		/* set the index enable */
		H[joint_num].index_enable = 1;
		/* and move right into the waiting state */
		H[joint_num].home_state = HOME_INDEX_SEARCH_WAIT;
		immediate_state = 1;
		home_do_moving_checks(joint,joint_num);
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
		if ( H[joint_num].index_enable == 0 ) {
		    /* yes, stop motion */
		    joint->free_tp.enable = 0;
		    /* go to next step */
		    H[joint_num].home_state = HOME_SET_INDEX_POSITION;
		    immediate_state = 1;
		    break;
		}
		home_do_moving_checks(joint,joint_num);
		break;

	    case HOME_SET_INDEX_POSITION:
		/* This state is called when the encoder has been reset at
		   the index pulse position.  It sets the current joint 
		   position to 'home_offset', which is the location of the
		   index pulse in joint coordinates. */
		/* set the current position to 'home_offset' */
		joint->motor_offset = - H[joint_num].home_offset;
		joint->pos_fb = joint->motor_pos_fb -
		    (joint->backlash_filt + joint->motor_offset);
		joint->pos_cmd = joint->pos_fb;
		joint->free_tp.curr_pos = joint->pos_fb;

		if (H[joint_num].home_flags & HOME_INDEX_NO_ENCODER_RESET) {
		   /* Special case: encoder does not reset on index pulse.
		      This moves the internal position but does not affect
		      the motor position */
		   offset = H[joint_num].home_offset - joint->pos_fb;
		   joint->pos_cmd          += offset;
		   joint->pos_fb           += offset;
		   joint->free_tp.curr_pos += offset;
		   joint->motor_offset     -= offset;
		}

		/* next state */
		H[joint_num].home_state = HOME_FINAL_MOVE_START;
		immediate_state = 1;
		break;

	    case HOME_FINAL_MOVE_START:
		/* This state is called once the joint coordinate system is
		   set properly.  It moves to the actual 'home' position,
		   which is not necessarily the position of the home switch
		   or index pulse. */
		/* is the joint already moving? */
		if (joint->free_tp.active) {
		    /* yes, reset delay, wait until joint stops */
		    H[joint_num].pause_timer = 0;
		    break;
		}
		/* has delay timed out? */
		if (H[joint_num].pause_timer < (HOME_DELAY * servo_freq)) {
		    /* no, update timer and wait some more */
		    H[joint_num].pause_timer++;
                    if (H[joint_num].home_sequence < 0) {
                        if (!H[home_sequence].sync_final_move) break;
                    } else {
                        break;
                    }

		}
                // negative H[joint_num].home_sequence means sync final move
                //          defer final move until all joints in sequence are ready
                if  (        (H[joint_num].home_sequence  < 0)
                     && ( ABS(H[joint_num].home_sequence) == home_sequence)
                    ) {
                    if (!H[home_sequence].sync_final_move) {
                        int jno;
                        emcmot_joint_t *jtmp;
                        H[home_sequence].sync_final_move = 1; //disprove
                        for (jno = 0; jno < ALL_JOINTS; jno++) {
                            jtmp = &joints[jno];
                            if (!H[jno].joint_in_sequence) continue;
                            if (ABS(H[jno].home_sequence) != home_sequence) {continue;}
                            if (H[jno].home_flags & HOME_ABSOLUTE_ENCODER)  {continue;}
                            if (   (H[jno].home_state != HOME_FINAL_MOVE_START)
                                ||
                                   (jtmp->free_tp.active)
                                ) {
                                H[home_sequence].sync_final_move = 0;
                                break;
                            }
                        }
                        if (!H[home_sequence].sync_final_move) break;
                    }
                }
		H[joint_num].pause_timer = 0;
		/* plan a move to home position */
		joint->free_tp.pos_cmd = H[joint_num].home;
		/* if home_vel is set (>0) then we use that, otherwise we rapid there */
		if (H[joint_num].home_final_vel > 0) {
		    joint->free_tp.max_vel = fabs(H[joint_num].home_final_vel);
		    /* clamp on max vel for this joint */
		    if (joint->free_tp.max_vel > joint->vel_limit)
			joint->free_tp.max_vel = joint->vel_limit;
		} else { 
		    joint->free_tp.max_vel = joint->vel_limit;
		}
		/* start the move */
		joint->free_tp.enable = 1;
		H[joint_num].home_state = HOME_FINAL_MOVE_WAIT;
		break;

	    case HOME_FINAL_MOVE_WAIT:
		/* This state is called while the machine makes its final
		   move to the home position.  It terminates when the machine 
		   arrives at the final location. If the move hits a limit
		   before it arrives, the home is aborted. */
		/* have we arrived (and stopped) at home? */
		if (!joint->free_tp.active) {
		    /* yes, stop motion */
		    joint->free_tp.enable = 0;
		    /* we're finally done */
		    H[joint_num].home_state = HOME_LOCK;
		    immediate_state = 1;
		    break;
		}
		if (joint->on_pos_limit || joint->on_neg_limit) {
		    /* on limit, check to see if we should trip */
		    if (!(H[joint_num].home_flags & HOME_IGNORE_LIMITS)) {
			/* not ignoring limits, time to quit */
			reportError(_("hit limit in home state j=%d"),joint_num);
			H[joint_num].home_state = HOME_ABORT;
			immediate_state = 1;
			break;
		    }
		}
		break;

	    case HOME_LOCK:
		if (H[joint_num].home_flags & HOME_UNLOCK_FIRST) {
		    emcmotSetRotaryUnlock(joint_num, 0);
		} else {
		    immediate_state = 1;
		}
		H[joint_num].home_state = HOME_LOCK_WAIT;
		break;

	    case HOME_LOCK_WAIT:
		// if not yet locked, continue waiting
		if ((H[joint_num].home_flags & HOME_UNLOCK_FIRST) &&
		    emcmotGetRotaryIsUnlocked(joint_num)) break;

		// either we got here without a lock needed, or the
		// lock is now complete.
		H[joint_num].home_state = HOME_FINISHED;
		immediate_state = 1;
		break;

	    case HOME_FINISHED:
                H[joint_num].homing = 0;
                H[joint_num].homed = 1;
		H[joint_num].at_home = 1;
		H[joint_num].home_state = HOME_IDLE;
		immediate_state = 1;
                H[joint_num].joint_in_sequence = 0;
                // This joint just finished homing.  See if this is the
                // final one and all joints are now homed, and switch to
                // Teleop mode if so.
                if (checkAllHomed()) { // Note: not in homing api
                    switch_to_teleop_mode();
                    homing_flag = 0;
                }
		break;

	    case HOME_ABORT:
                H[joint_num].homing = 0;
                H[joint_num].homed = 0;
                H[joint_num].at_home = 0;
                H[joint_num].joint_in_sequence = 0;
		joint->free_tp.enable = 0;
		H[joint_num].home_state = HOME_IDLE;
		H[joint_num].index_enable = 0;
		immediate_state = 1;
		break;

	    default:
		/* should never get here */
		reportError(_("unknown state '%d' during homing j=%d"),
		    H[joint_num].home_state,joint_num);
		H[joint_num].home_state = HOME_ABORT;
		immediate_state = 1;
		break;
	    }	/* end of switch(H[joint_num].home_state) */
        } while (immediate_state);
    }	/* end of loop through all joints */

    if ( homing_flag ) {
	/* at least one joint is homing, set global flag */
	homing_active = 1;
    } else {
	/* is a homing sequence in progress? */
	if (sequence_state == HOME_SEQUENCE_IDLE) {
	    /* no, single joint only, we're done */
	    homing_active = 0;
	}
    }
} // do_homing()

bool get_home_is_synchronized(int jno) {
    return H[jno].home_is_synchronized;
}
