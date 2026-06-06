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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gomc_env.h"
#include "home_api.h"
#include "mot_api.h"
#include "motion.h"
#include "homing.h"


#define ABS(x) (((x) < 0) ? -(x) : (x))




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

// internal SEQUENCE states
typedef enum {
  HOME_SEQUENCE_IDLE = 0,        // valid start state
  HOME_SEQUENCE_START,           // valid start state
  HOME_SEQUENCE_DO_ONE_JOINT,    // valid start state
  HOME_SEQUENCE_DO_ONE_SEQUENCE, // valid start state
  HOME_SEQUENCE_START_JOINTS,    // internal usage
  HOME_SEQUENCE_WAIT_JOINTS,     // internal usage
} home_sequence_state_t;

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

// local per-joint data (includes hal pin data)
typedef struct {
  home_state_t home_state;           // OUT pin
  bool         homing;               // OUT pin
  bool         homed;                // OUT pin
  bool         home_sw;              // IN  pin
  bool         index_enable;         // IO  pin
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

// data for per-joint homing-specific hal pins:
typedef struct {
    gomc_hal_bit_t *home_sw;      // home switch input
    gomc_hal_bit_t *homing;       // joint is homing
    gomc_hal_bit_t *homed;        // joint was homed
    gomc_hal_bit_t *index_enable; // motmod sets: request reset on index
                             //        encoder clears: index arrived
    gomc_hal_s32_t *home_state;   // homing state machine state
} one_joint_home_data_t;

typedef struct {
    one_joint_home_data_t jhd[EMCMOT_MAX_JOINTS];
} all_joints_home_data_t;

/***********************************************************************
*              PER-INSTANCE STATE (multi-instance support)             *
************************************************************************/

typedef struct {
    const mot_callbacks_t *mot;
    const gomc_api_t *api;
    const gomc_hal_t *hal;
    const gomc_log_t *log;
    char name[GOMC_HAL_NAME_LEN + 1];
    char pin_prefix[GOMC_HAL_NAME_LEN + 1]; /* "" when default, "name." when aliased */
    char mot_instance[GOMC_HAL_NAME_LEN + 1];
    int comp_id;
    double servo_freq;
    int all_joints;
    int extra_joints;
    home_sequence_state_t sequence_state;
    int current_sequence;
    bool homing_active;
    home_local_data H[EMCMOT_MAX_JOINTS];
    all_joints_home_data_t *joint_home_data;
    bool sync_now;
    home_callbacks_t callbacks;
    cmod_t cmod;
} homemod_inst_t;

/* Forward declarations */
static bool base_get_allhomed(homemod_inst_t *inst);
static bool base_get_homing_is_active(homemod_inst_t *inst);
static bool base_get_homing(homemod_inst_t *inst, int jno);
static bool base_get_homed(homemod_inst_t *inst, int jno);


/***********************************************************************
*                      LOCAL FUNCTIONS                                 *
************************************************************************/

/* a couple of helper functions with code that would otherwise be
   repeated in several different states of the homing state machine */

/* 'home_start_move()' starts a move at the specified velocity.  The
   length of the move is equal to twice the overall range of the joint,
   but the intent is that something (like a home switch or index pulse)
   will stop it before that point. */
static void home_start_move(homemod_inst_t *inst, int jno, double vel)
{
    double joint_range;

    /* set up a long move */
    joint_range = inst->mot->joint_get_max_pos_limit(inst->mot->ctx, jno) - inst->mot->joint_get_min_pos_limit(inst->mot->ctx, jno);
    if (vel > 0.0) {
        inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno, inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) + 2.0 * joint_range);
    } else {
        inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, jno, inst->mot->joint_get_pos_cmd(inst->mot->ctx, jno) - 2.0 * joint_range);
    }
    if (fabs(vel) < inst->mot->joint_get_vel_limit(inst->mot->ctx, jno)) {
        inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno, fabs(vel));
    } else {
        /* clamp on max vel for this joint */
        inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, jno, inst->mot->joint_get_vel_limit(inst->mot->ctx, jno));
    }
    /* start the move */
    inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 1);
} // home_start_move()

/* 'home_do_moving_checks()' is called from states where the machine
   is supposed to be moving.  It checks to see if the machine has
   hit a limit, or if the move has stopped.  (Normally such moves
   will be terminated by the home switch or an index pulse or some
   other event, if the move goes to completion, something is wrong.) */
static bool home_do_moving_checks(homemod_inst_t *inst, int jno)
{
    /* check for limit switches */
    if ( inst->mot->joint_get_on_pos_limit(inst->mot->ctx, jno) ||  inst->mot->joint_get_on_neg_limit(inst->mot->ctx, jno)) {
        /* on limit, check to see if we should trip */
        if (!(inst->H[jno].home_flags & HOME_IGNORE_LIMITS)) {
            /* not ignoring limits, time to quit */
            gomc_log_errorf(inst->log, inst->name, "j%d hit limit in home state %d", jno, inst->H[jno].home_state);
            inst->H[jno].home_state = HOME_ABORT;
            return 1; // abort reqd
        }
    }
    /* check for reached end of move */
    if (! inst->mot->joint_get_free_tp_active(inst->mot->ctx, jno)) {
        /* reached end of move without hitting switch */
         inst->mot->joint_set_free_tp_enable(inst->mot->ctx, jno, 0);
        gomc_log_errorf(inst->log, inst->name, "j%d end of move in home state %d", jno, inst->H[jno].home_state);
        inst->H[jno].home_state = HOME_ABORT;
        return 1; // abort reqd
    }
    return 0;
} // home_do_moving_checks()

#define ABORT_CHECK(joint_num) do { \
    if (home_do_moving_checks(inst, joint_num)) { \
        inst->H[joint_num].home_state = HOME_ABORT; \
        immediate_state = 1; \
    } \
} while(0);

static void update_home_is_synchronized(homemod_inst_t *inst) {
    // invoke anytime inst->H[*].home_sequence is altered
    int jno,jj;

    for (jno = 0; jno < inst->all_joints; jno++) {
        inst->H[jno].home_is_synchronized = 0;
        if (inst->H[jno].home_sequence < 0) {
            // neg: sync all joints with same ABS(inst->H[jno].home_sequence):
            for (jj = 0; jj < inst->all_joints; jj++) {
                if (ABS(inst->H[jj].home_sequence) == ABS(inst->H[jno].home_sequence)) {
                    inst->H[jj].home_is_synchronized = 1;
                }
            }
        }
    }
}

static int base_make_joint_home_pins(homemod_inst_t *inst, int id, int njoints)
{
//NOTE: motmod supplies the component id
    int jno,retval;
    one_joint_home_data_t *addr;
    const char *P = inst->pin_prefix;

    inst->joint_home_data = inst->hal->malloc(inst->hal->ctx, sizeof(all_joints_home_data_t));
    if (inst->joint_home_data == 0) {
        gomc_log_errorf(inst->log, inst->name, "all_joints_home_data_t malloc failed");
        return -1;
    }

    retval = 0;
    for (jno = 0; jno < njoints; jno++) {
        addr = &(inst->joint_home_data->jhd[jno]);

        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IN, &(addr->home_sw), id,
                                  "%sjoint.%d.home-sw-in", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT, &(addr->homing), id,
                                  "%sjoint.%d.homing", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_OUT, &(addr->homed), id,
                                  "%sjoint.%d.homed", P, jno);
        retval += gomc_hal_pin_s32_newf(inst->hal, GOMC_HAL_OUT, &(addr->home_state), id,
                                  "%sjoint.%d.home-state", P, jno);
        retval += gomc_hal_pin_bit_newf(inst->hal, GOMC_HAL_IO, &(addr->index_enable), id,
                                  "%sjoint.%d.index-enable", P, jno);
    }
    return retval;
} // base_make_joint_home_pins()

static void do_home_all(homemod_inst_t *inst)
{
    if (!base_get_homing_is_active(inst) ) {
        inst->sequence_state = HOME_SEQUENCE_START;
    }
} // do_home_all(inst)

static void do_home_one_joint(homemod_inst_t *inst, int jno)
{
    //NOTE: if inst->H[jno].home_sequence neg, home all joints in sequence
    int jj;
    if (inst->H[jno].home_sequence < 0) {  //neg: home all joints in sequence
        inst->sequence_state = HOME_SEQUENCE_DO_ONE_SEQUENCE;
        for (jj = 0; jj < inst->all_joints; jj++) {
            if (ABS(inst->H[jj].home_sequence) == ABS(inst->H[jno].home_sequence)) {
                // set home_state for all joints with same sequence
                inst->H[jj].home_state = HOME_START;
            }
        }
    } else {
        inst->sequence_state = HOME_SEQUENCE_DO_ONE_JOINT;
    }
    inst->H[jno].home_state = HOME_START;
} // do_home_one_joint()

static void set_all_unhomed(homemod_inst_t *inst, int unhome_method, motion_state_t motstate)
{
    /*
    ** unhome_method == -1: unhome all joints
    ** unhome_method == -2: unhome joints marked as VOLATILE_HOME
    */
    int jno;
    /* we want all or none, so these checks need to all be done first.
     * but, let's only report the first error.  There might be several,
     * for instance if a homing sequence is running. */
    for (jno = 0; jno < inst->all_joints; jno++) {
        if(inst->mot->joint_get_active_flag(inst->mot->ctx, jno)) {
            if (base_get_homing(inst, jno)) {
                gomc_log_errorf(inst->log, inst->name,
                     "Cannot unhome while homing, joint %d", jno);
                return;
            }
            if (!inst->mot->joint_get_inpos_flag(inst->mot->ctx, jno)) {
                gomc_log_errorf(inst->log, inst->name,
                     "Cannot unhome while moving, joint %d", jno);
                return;
            }
        }
        if (   (jno >= (inst->all_joints - inst->extra_joints))  // jno is extrajoint
            && (motstate != EMCMOT_MOTION_DISABLED)) {
            gomc_log_errorf(inst->log, inst->name,
                 "Cannot unhome extrajoint <%d> with motion enabled", jno);
            return;
        }
    }
    /* we made it through the checks, so unhome them all per unhome_method */
    for (jno = 0; jno < inst->all_joints; jno++) {
        if(inst->mot->joint_get_active_flag(inst->mot->ctx, jno)) {
            if (     (unhome_method == -1)
                || ( (unhome_method == -2) && (inst->H[jno].volatile_home) )
               ) {
                inst->H[jno].homed = 0;
            }
        }
    }
} // set_all_unhomed()

// SEQUENCE management
static void do_homing_sequence(homemod_inst_t *inst)
{
    int i,ii;
    int seen;
    int sequence_is_set = 0;

    switch( inst->sequence_state ) {
    case HOME_SEQUENCE_IDLE:
        inst->current_sequence = 0;
        /* nothing to do */
        break;

    case HOME_SEQUENCE_DO_ONE_JOINT:
        // Expect one joint with home_state==HOME_START
        for (i=0; i < inst->all_joints; i++) {
            if (inst->H[i].home_state == HOME_START) {
               inst->H[i].joint_in_sequence = 1;
               inst->current_sequence = ABS(inst->H[i].home_sequence);
            } else {
               inst->H[i].joint_in_sequence = 0;
            }
        }
        sequence_is_set = 1;
        /* fallthrough */
    case HOME_SEQUENCE_DO_ONE_SEQUENCE:
        // Expect multiple joints with home_state==HOME_START
        // specified by a negative sequence
        // Determine inst->current_sequence and set inst->H[i].joint_in_sequence
        // based on home_state[i] == HOME_START
        if (!sequence_is_set) {
            for (i=0; i < inst->all_joints; i++) {
                if (inst->H[i].home_state == HOME_START) {
                    if (   sequence_is_set
                        && (ABS(inst->H[i].home_sequence) != inst->current_sequence)) {
                        gomc_log_errorf(inst->log, inst->name,
                           "Unexpected joint=%d jseq=%d current_seq=%d"
                           ,i,inst->H[i].home_sequence,inst->current_sequence);
                    }
                    inst->current_sequence = ABS(inst->H[i].home_sequence);
                    sequence_is_set = 1;
                }
                inst->H[i].joint_in_sequence = 1; //disprove
                if  (   (inst->H[i].home_state  != HOME_START)
                     || (inst->current_sequence != ABS(inst->H[i].home_sequence))
                    ) {
                    inst->H[i].joint_in_sequence = 0;
                }
            }
        }
        inst->sequence_state = HOME_SEQUENCE_START;
        /* fallthrough */
    case HOME_SEQUENCE_START:
        // Request to home all joints or a single sequence
        // A negative inst->H[i].home_sequence means sync final move
        if (!sequence_is_set) {
            // sequence_is_set not otherwise established: home-all
            for (i=0; i < EMCMOT_MAX_JOINTS; i++) {
                inst->H[i].joint_in_sequence = 1;
                // unspecified joints have an unrealizable inst->H[i].home_sequence:
                if (inst->H[i].home_sequence >100) {
                   // docs: 'If HOME_SEQUENCE is not specified then this joint
                   //        will not be homed by the HOME ALL sequence'
                   inst->H[i].joint_in_sequence = 0;  // per docs
                }
            }
            sequence_is_set  = 1;
            inst->current_sequence = 0;
        }
        /* Initializations */
        for(i=0; i < inst->all_joints; i++) {
            if (!inst->H[i].joint_in_sequence) continue;
            if (   (inst->H[i].home_flags & HOME_NO_REHOME)
                &&  inst->H[i].homed
               ) {
                continue;
            } else {
                inst->H[i].homed = 0;
            }
            if (inst->H[i].home_sequence < 0) {
                // If a inst->H[i].home_sequence is neg, find all joints that
                // have the same ABS sequence value and make them the same.
                // Note: overrides inconsistent mixed positive/negative settings
                //       for inst->H[ii].home_sequence
                for(ii=0; ii < inst->all_joints; ii++) {
                    if (inst->H[ii].home_sequence == ABS(inst->H[i].home_sequence)) {
                        inst->H[ii].home_sequence =      inst->H[i].home_sequence;
                    }
                }
            }
        }
        for(i=0; i < inst->all_joints; i++) {
            if (!inst->H[i].joint_in_sequence) continue;
            if  ( inst->H[i].home_state != HOME_IDLE && inst->H[i].home_state != HOME_START) {
                /* a home is already in progress, abort the home-all */
                inst->sequence_state = HOME_SEQUENCE_IDLE;
                return;
            }
        }
        /* tell the world we're on the job */
        inst->homing_active = 1;
        /* fallthrough */
    case HOME_SEQUENCE_START_JOINTS:
        seen = 0;
        /* start all joints whose sequence number matches inst->H[i].home_sequence */
        for(i=0; i < inst->all_joints; i++) {
            if(ABS(inst->H[i].home_sequence) == inst->current_sequence) {
                if (!inst->H[i].joint_in_sequence) continue;
                /* start this joint */
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, i, 0);
                inst->H[i].home_state = HOME_START;
                seen++;
            }
        }
        if (seen || inst->current_sequence == 0) {
            inst->sequence_state = HOME_SEQUENCE_WAIT_JOINTS;
        } else {
            /* no joints have this sequence number, we're done */
            inst->sequence_state = HOME_SEQUENCE_IDLE;
            /* tell the world */
            inst->homing_active = 0;
        }
        break;

    case HOME_SEQUENCE_WAIT_JOINTS:
        seen = 0;
        for(i=0; i < inst->all_joints; i++) {
            if (!inst->H[i].joint_in_sequence) continue;
            // negative inst->H[i].home_sequence means sync final move
            if(ABS(inst->H[i].home_sequence) != inst->current_sequence) {
                /* this joint is not at the current sequence number, ignore it */
                continue;
            }
            if(inst->H[i].home_state != HOME_IDLE) {
                /* still busy homing, keep waiting */
                seen = 1;
                continue;
            }
        }
        if(!seen) {
            /* all joints at this step have finished, move on to next step */
            inst->current_sequence++;
            inst->sequence_state = HOME_SEQUENCE_START_JOINTS;
        }
        break;

    default:
        /* should never get here */
        gomc_log_errorf(inst->log, inst->name, "unknown state '%d' during homing sequence",
                        inst->sequence_state);
        inst->sequence_state = HOME_SEQUENCE_IDLE;
        inst->homing_active = 0;
        break;
    }
} // do_homing_sequence(inst)

static int base_homing_init(homemod_inst_t *inst, int id,
                            double servo_period,
                            int njoints,
                            int nextrajoints)
{
    int i;
    inst->all_joints   = njoints;
    inst->extra_joints = nextrajoints;

    if (servo_period < 1e-9) {
        gomc_log_errorf(inst->log, inst->name, "bad servo_period:%g",
                        servo_period);
        return -1;
    }
    if (base_make_joint_home_pins(inst, id, inst->all_joints)) {
        gomc_log_errorf(inst->log, inst->name, "base_make_joint_home_pins fail");
        return -1;
    }

    inst->servo_freq = 1/servo_period;
    inst->homing_active = 0;
    for (i=0; i < inst->all_joints; i++) {
        inst->H[i].home_state      =  HOME_IDLE;
        inst->H[i].home_search_vel =  0;
        inst->H[i].home_latch_vel  =  0;
        inst->H[i].home_final_vel  =  0;
        inst->H[i].home_offset     =  0;
        inst->H[i].home            =  0;
        inst->H[i].home_flags      =  0;
        inst->H[i].home_sequence   = 1000; //startup: unrealizable, positive seq no.
        inst->H[i].volatile_home   =  0;
    }
    return 0;
}

static void base_read_homing_in_pins(homemod_inst_t *inst, int njoints)
{
    int jno;
    one_joint_home_data_t *addr;
    for (jno = 0; jno < njoints; jno++) {
        addr = &(inst->joint_home_data->jhd[jno]);
        inst->H[jno].home_sw      = *(addr->home_sw);      // IN
        inst->H[jno].index_enable = *(addr->index_enable); // IO
    }
}

static void base_write_homing_out_pins(homemod_inst_t *inst, int njoints)
{
    int jno;
    one_joint_home_data_t *addr;
    for (jno = 0; jno < njoints; jno++) {
        addr = &(inst->joint_home_data->jhd[jno]);
        *(addr->homing)       = inst->H[jno].homing;       // OUT
        *(addr->homed)        = inst->H[jno].homed;        // OUT
        *(addr->home_state)   = inst->H[jno].home_state;   // OUT
        *(addr->index_enable) = inst->H[jno].index_enable; // IO
    }
}

static void base_do_home_joint(homemod_inst_t *inst, int jno) {
    if (jno == -1) {
        inst->H[0].homed = 0; // ensure at least one unhomed
        do_home_all(inst);
    } else {
        do_home_one_joint(inst, jno); // apply rules if home_sequence negative
    }
}

static void base_do_cancel_homing(homemod_inst_t *inst, int jno) {
    if (inst->H[jno].homing) {
        inst->H[jno].home_state = HOME_ABORT;
    }else if(inst->H[jno].joint_in_sequence){
        inst->H[jno].home_state = HOME_ABORT;
    }
}

static void base_set_unhomed(homemod_inst_t *inst, int jno, motion_state_t motstate) {
    // Note: negative jno ==> unhome multiple joints
    if (jno < 0) { set_all_unhomed(inst, jno, motstate); return; }

    if (jno > inst->all_joints) {
        /* invalid joint number specified */
        gomc_log_errorf(inst->log, inst->name,
             "Cannot unhome invalid joint %d (max %d)", jno, (inst->all_joints-1));
        return;
    }
    /* request was for one joint number */
    if (   (jno >= (inst->all_joints - inst->extra_joints) )  // jno is extrajoint
        && (motstate != EMCMOT_MOTION_DISABLED)) {
        gomc_log_errorf(inst->log, inst->name,
             "Cannot unhome extrajoint <%d> with motion enabled", jno);
        return;
    }
    if(inst->mot->joint_get_active_flag(inst->mot->ctx, jno) ) {
        if (base_get_homing(inst, jno) ) {
            gomc_log_errorf(inst->log, inst->name,
                 "Cannot unhome while homing, joint %d", jno);
            return;
        }
        if (!inst->mot->joint_get_inpos_flag(inst->mot->ctx, jno) ) {
            gomc_log_errorf(inst->log, inst->name,
                 "Cannot unhome while moving, joint %d", jno);
            return;
        }
        inst->H[jno].homed = 0;
    } else {
        gomc_log_errorf(inst->log, inst->name,
             "Cannot unhome inactive joint %d", jno);
    }
} // base_set_unhomed()

static void base_set_joint_homing_params(homemod_inst_t *inst, int jno,
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
    inst->H[jno].home_offset     = offset;
    inst->H[jno].home            = home;
    inst->H[jno].home_final_vel  = home_final_vel;
    inst->H[jno].home_search_vel = home_search_vel;
    inst->H[jno].home_latch_vel  = home_latch_vel;
    inst->H[jno].home_flags      = home_flags;
    inst->H[jno].home_sequence   = home_sequence;
    inst->H[jno].volatile_home   = volatile_home;
    update_home_is_synchronized(inst);
}

static void base_update_joint_homing_params(homemod_inst_t *inst, int jno,
                                             double offset,
                                             double home,
                                             int    home_sequence
                                            )
{
    inst->H[jno].home_offset   = offset;
    inst->H[jno].home          = home;
    inst->H[jno].home_sequence = home_sequence;
    update_home_is_synchronized(inst);
}

static bool base_get_allhomed(homemod_inst_t *inst) {
    int joint_num;

    for (joint_num = 0; joint_num < inst->all_joints; joint_num++) {
        if (!inst->mot->joint_get_active_flag(inst->mot->ctx, joint_num)) {
            /* if joint is not active, don't even look at its limits */
            continue;
        }
        if (!base_get_homed(inst, joint_num) ) {
            /* if any of the joints is not homed return false */
            return 0;
        }
    }
    /* return true if all active joints are homed*/
    return 1;
} // base_get_allhomed(inst)

static bool base_get_homing_is_active(homemod_inst_t *inst) {
    return inst->homing_active;
}

static int base_get_home_sequence(homemod_inst_t *inst, int jno) {
    return inst->H[jno].home_sequence;
}

static bool base_get_homing(homemod_inst_t *inst, int jno) {
    return inst->H[jno].homing;
}

static bool base_get_homed(homemod_inst_t *inst, int jno) {
    return inst->H[jno].homed;
}

static bool base_get_index_enable(homemod_inst_t *inst, int jno) {
     return inst->H[jno].index_enable;
}

static bool base_get_home_needs_unlock_first(homemod_inst_t *inst, int jno) {
    return (inst->H[jno].home_flags & HOME_UNLOCK_FIRST) ? 1 : 0;
}

static bool base_get_home_is_idle(homemod_inst_t *inst, int jno) {
    return inst->H[jno].home_state == HOME_IDLE ? 1 : 0;
}

static bool base_get_home_is_synchronized(homemod_inst_t *inst, int jno) {
    return inst->H[jno].home_is_synchronized;
}

static bool base_get_homing_at_index_search_wait(homemod_inst_t *inst, int jno) {
    return inst->H[jno].home_state == HOME_INDEX_SEARCH_WAIT ? 1 : 0;
}

static void sync_reset(homemod_inst_t *inst) { inst->sync_now=0; return; }

static bool sync_ready(homemod_inst_t *inst, int joint_num)
{
    // defer a move until all joints in sequence are at current state
    if  (   ( ABS(inst->H[joint_num].home_sequence) == inst->current_sequence)
         && !inst->sync_now) {
        int jno;
        for (jno = 0; jno < inst->all_joints; jno++) {
            if (!inst->H[jno].joint_in_sequence)                     {continue;}
            if (ABS(inst->H[jno].home_sequence) != inst->current_sequence) {continue;}
            if (inst->H[jno].home_flags &  HOME_ABSOLUTE_ENCODER)    {continue;}
            if (inst->H[jno].home_state != inst->H[joint_num].home_state) {
                inst->sync_now = 0; return 0; // not ready
            }
        }
        inst->sync_now = 1;
    }
    return 1; // ready
} // sync_ready()

static int base_1joint_state_machine(homemod_inst_t *inst, int joint_num)
{
    double offset, tmp;
    int home_sw_active, homing_flag;
    bool immediate_state = 0;

    homing_flag = 0;
    home_sw_active = inst->H[joint_num].home_sw;
    if (inst->H[joint_num].home_state != HOME_IDLE) {
        homing_flag = 1; /* at least one joint is homing */
    }

    /* when a joint is homing, 'check_for_faults()' ignores its limit
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
        switch (inst->H[joint_num].home_state) {
        case HOME_IDLE:
            /* nothing to do */
            break;

        case HOME_START:
            /* This state is responsible for getting the homing process
               started.  It doesn't actually do anything, it simply
               determines what state is next */
            if (inst->H[joint_num].home_flags & HOME_IS_SHARED && home_sw_active) {
                gomc_log_errorf(inst->log, inst->name, "Cannot home while shared home switch is closed j=%d",
                                joint_num);
                inst->H[joint_num].home_state = HOME_IDLE;
                break;
            }
            /* set flags that communicate with the rest of EMC */
            if (   (inst->H[joint_num].home_flags & HOME_NO_REHOME)
                &&  inst->H[joint_num].homed
               ) {
               inst->H[joint_num].home_state = HOME_IDLE;
               break; //no rehome allowed if absolute_enoder
            } else {
                inst->H[joint_num].homing = 1;
                inst->H[joint_num].homed = 0;
            }
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);    /* stop any existing motion */
            sync_reset(inst);                 /* stop any interrupted/canceled sync */
            inst->H[joint_num].pause_timer = 0; /* reset delay counter */
            /* figure out exactly what homing sequence is needed */
            if (inst->H[joint_num].home_flags & HOME_ABSOLUTE_ENCODER) {
                inst->H[joint_num].home_flags &= ~HOME_IS_SHARED; // shared not applicable
                inst->H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
                immediate_state = 1;
                // Note: inst->H[joint_num].homed
                // is not set in case there is a final move requested
                break;
            }
            if (inst->H[joint_num].home_flags & HOME_UNLOCK_FIRST) {
                inst->H[joint_num].home_state = HOME_UNLOCK;
            } else {
                inst->H[joint_num].home_state = HOME_UNLOCK_WAIT;
                immediate_state = 1;
            }
            break;

        case HOME_UNLOCK:
            // unlock now
            inst->mot->set_rotary_unlock(inst->mot->ctx, joint_num, 1);
            inst->H[joint_num].home_state = HOME_UNLOCK_WAIT;
            break;

        case HOME_UNLOCK_WAIT:
            // if not yet unlocked, continue waiting
            if ((inst->H[joint_num].home_flags & HOME_UNLOCK_FIRST) &&
                !inst->mot->get_rotary_unlock(inst->mot->ctx, joint_num)) break;

            // either we got here without an unlock needed, or the
            // unlock is now complete.
            if (inst->H[joint_num].home_search_vel == 0.0) {
                if (inst->H[joint_num].home_latch_vel == 0.0) {
                    /* both vels == 0 means home at current position */
                    inst->H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                } else if (inst->H[joint_num].home_flags & HOME_USE_INDEX) {
                    /* home using index pulse only */
                    inst->H[joint_num].home_state = HOME_INDEX_ONLY_START;
                    immediate_state = 1;
                } else {
                    gomc_log_errorf(inst->log, inst->name,
                         "invalid homing config: non-zero LATCH_VEL needs either SEARCH_VEL or USE_INDEX");
                    inst->H[joint_num].home_state = HOME_IDLE;
                }
            } else {
                if (inst->H[joint_num].home_latch_vel != 0.0) {
                    /* need to find home switch */
                    inst->H[joint_num].home_state = HOME_INITIAL_SEARCH_START;
                    immediate_state = 1;
                } else {
                    gomc_log_errorf(inst->log, inst->name,
                         "invalid homing config: non-zero SEARCH_VEL needs LATCH_VEL");
                    inst->H[joint_num].home_state = HOME_IDLE;
                }
            }
            break;

        case HOME_INITIAL_BACKOFF_START:
            /* This state is called if the homing sequence starts at a
               location where the home switch is already tripped. It
               starts a move away from the switch. */
            /* is the joint still moving? */
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, reset delay, wait until joint stops */
                inst->H[joint_num].pause_timer = 0;
                break;
            }
            /* has delay timed out? */
            if (inst->H[joint_num].pause_timer < (HOME_DELAY * inst->servo_freq)) {
                /* no, update timer and wait some more */
                inst->H[joint_num].pause_timer++;
                break;
            }
            inst->H[joint_num].pause_timer = 0;
            /* set up a move at '-search_vel' to back off of switch */
            home_start_move(inst, joint_num, - inst->H[joint_num].home_search_vel);
            /* next state */
            inst->H[joint_num].home_state = HOME_INITIAL_BACKOFF_WAIT;
            break;

        case HOME_INITIAL_BACKOFF_WAIT:
            /* This state is called while the machine is moving off of
               the home switch.  It terminates when the switch is cleared
               successfully.  If the move ends or hits a limit before it
               clears the switch, the home is aborted. */
            /* are we off home switch yet? */
            if (! home_sw_active) {
                /* yes, stop motion */
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
                /* begin initial search */
                inst->H[joint_num].home_state = HOME_INITIAL_SEARCH_START;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK(joint_num);
            break;

        case HOME_INITIAL_SEARCH_START:
            /* This state is responsible for starting a move toward the
               home switch.  This move is at 'search_vel', which can be
               fairly fast, because once the switch is found another
               slower move will be used to set the exact home position. */
            /* is the joint already moving? */
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, reset delay, wait until joint stops */
                inst->H[joint_num].pause_timer = 0;
                break;
            }
            /* has delay timed out? */
            if (inst->H[joint_num].pause_timer < (HOME_DELAY * inst->servo_freq)) {
                /* no, update timer and wait some more */
                inst->H[joint_num].pause_timer++;
                break;
            }
            inst->H[joint_num].pause_timer = 0;
            /* make sure we aren't already on home switch */
            if (home_sw_active) {
                /* already on switch, need to back off it first */
                inst->H[joint_num].home_state = HOME_INITIAL_BACKOFF_START;
                immediate_state = 1;
                break;
            }
            /* set up a move at 'search_vel' to find switch */
            home_start_move(inst, joint_num, inst->H[joint_num].home_search_vel);
            /* next state */
            inst->H[joint_num].home_state = HOME_INITIAL_SEARCH_WAIT;
            break;

        case HOME_INITIAL_SEARCH_WAIT:
            /* This state is called while the machine is looking for the
               home switch.  It terminates when the switch is found.  If
               the move ends or hits a limit before it finds the switch,
               the home is aborted. */
            /* have we hit home switch yet? */
            if (home_sw_active) {
                /* yes, stop motion */
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
                /* go to next step */
                inst->H[joint_num].home_state = HOME_SET_COARSE_POSITION;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK(joint_num);
            break;

        case HOME_SET_COARSE_POSITION:
            /* This state is called after the first time the switch is
               found.  At this point, we are approximately home. Although
               we will do another slower pass to get the exact home
               location, we reset the joint coordinates now so that screw
               error comp will be appropriate for this portion of the
               screw (previously we didn't know where we were at all). */
            /* set the current position to 'home_offset' */
            offset = inst->H[joint_num].home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num);
            /* this moves the internal position but does not affect the
               motor position */
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_cmd(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_pos_fb(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, joint_num, inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_motor_offset(inst->mot->ctx, joint_num, inst->mot->joint_get_motor_offset(inst->mot->ctx, joint_num) - (offset));
            /* The next state depends on the signs of 'search_vel' and
               'latch_vel'.  If they are the same, that means we must
               back up, then do the final homing moving the same
               direction as the initial search, on a rising edge of the
               switch.  If they are opposite, it means that the final
               homing will take place on a falling edge as the machine
               moves off of the switch. */
            tmp = inst->H[joint_num].home_search_vel * inst->H[joint_num].home_latch_vel;
            if (tmp > 0.0) {
                /* search and latch vel are same direction */
                inst->H[joint_num].home_state = HOME_FINAL_BACKOFF_START;
            } else {
                /* search and latch vel are opposite directions */
                inst->H[joint_num].home_state = HOME_FALL_SEARCH_START;
            }
            immediate_state = 1;
            break;

        case HOME_FINAL_BACKOFF_START:
            /* This state is called once the approximate location of the
               switch has been found.  It is responsible for starting a
               move that will back off of the switch in preparation for a
               final slow move that captures the exact switch location. */
            /* is the joint already moving? */
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, reset delay, wait until joint stops */
                inst->H[joint_num].pause_timer = 0;
                break;
            }
            /* has delay timed out? */
            if (inst->H[joint_num].pause_timer < (HOME_DELAY * inst->servo_freq)) {
                /* no, update timer and wait some more */
                inst->H[joint_num].pause_timer++;
                break;
            }
            inst->H[joint_num].pause_timer = 0;
            /* we should still be on the switch */
            if (! home_sw_active) {
                gomc_log_errorf(inst->log, inst->name,
                     "Home switch inactive before start of backoff move j=%d",
                     joint_num);
                inst->H[joint_num].home_state = HOME_IDLE;
                break;
            }
            /* set up a move at '-search_vel' to back off of switch */
            home_start_move(inst, joint_num, - inst->H[joint_num].home_search_vel);
            /* next state */
            inst->H[joint_num].home_state = HOME_FINAL_BACKOFF_WAIT;
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
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
                /* begin final search */
                inst->H[joint_num].home_state = HOME_RISE_SEARCH_START;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK(joint_num);
            break;

        case HOME_RISE_SEARCH_START:
            /* This state is called to start the final search for the
               point where the home switch trips.  It moves at
               'latch_vel' and looks for a rising edge on the switch */
            /* is the joint already moving? */
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, reset delay, wait until joint stops */
                inst->H[joint_num].pause_timer = 0;
                break;
            }
            /* has delay timed out? */
            if (inst->H[joint_num].pause_timer < (HOME_DELAY * inst->servo_freq)) {
                /* no, update timer and wait some more */
                inst->H[joint_num].pause_timer++;
                break;
            }
            inst->H[joint_num].pause_timer = 0;
            /* we should still be off of the switch */
            if (home_sw_active) {
                gomc_log_errorf(inst->log, inst->name, "Home switch active before start of latch move j=%d",
                                joint_num);
                inst->H[joint_num].home_state = HOME_IDLE;
                break;
            }
            /* set up a move at 'latch_vel' to locate the switch */
            home_start_move(inst, joint_num, inst->H[joint_num].home_latch_vel);
            /* next state */
            inst->H[joint_num].home_state = HOME_RISE_SEARCH_WAIT;
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
                if (inst->H[joint_num].home_flags & HOME_USE_INDEX) {
                    /* look for index pulse */
                    inst->H[joint_num].home_state = HOME_INDEX_SEARCH_START;
                    immediate_state = 1;
                    break;
                } else {
                    /* no index pulse, stop motion */
                    inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
                    /* go to next step */
                    inst->H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                    break;
                }
            }
            ABORT_CHECK(joint_num);
            break;

        case HOME_FALL_SEARCH_START:
            /* This state is called to start the final search for the
               point where the home switch releases.  It moves at
               'latch_vel' and looks for a falling edge on the switch */
            /* is the joint already moving? */
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, reset delay, wait until joint stops */
                inst->H[joint_num].pause_timer = 0;
                break;
            }
            /* has delay timed out? */
            if (inst->H[joint_num].pause_timer < (HOME_DELAY * inst->servo_freq)) {
                /* no, update timer and wait some more */
                inst->H[joint_num].pause_timer++;
                break;
            }
            inst->H[joint_num].pause_timer = 0;
            /* we should still be on the switch */
            if (!home_sw_active) {
                gomc_log_errorf(inst->log, inst->name,
                     "Home switch inactive before start of latch move j=%d",
                     joint_num);
                inst->H[joint_num].home_state = HOME_IDLE;
                break;
            }
            /* set up a move at 'latch_vel' to locate the switch */
            home_start_move(inst, joint_num, inst->H[joint_num].home_latch_vel);
            /* next state */
            inst->H[joint_num].home_state = HOME_FALL_SEARCH_WAIT;
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
                if (inst->H[joint_num].home_flags & HOME_USE_INDEX) {
                    /* look for index pulse */
                    inst->H[joint_num].home_state = HOME_INDEX_SEARCH_START;
                    immediate_state = 1;
                    break;
                } else {
                    /* no index pulse, stop motion */
                    inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
                    /* go to next step */
                    inst->H[joint_num].home_state = HOME_SET_SWITCH_POSITION;
                    immediate_state = 1;
                    break;
                }
            }
            ABORT_CHECK(joint_num);
            break;

        case HOME_SET_SWITCH_POSITION:
            /* This state is called when the machine has determined the
               switch position as accurately as possible.  It sets the
               current joint position to 'home_offset', which is the
               location of the home switch in joint coordinates. */
            /* set the current position to 'home_offset' */
            if (inst->H[joint_num].home_flags & HOME_ABSOLUTE_ENCODER) {
                offset = inst->H[joint_num].home_offset;
            } else {
                offset = inst->H[joint_num].home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num);
            }
            /* this moves the internal position but does not affect the
               motor position */
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_cmd(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_pos_fb(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, joint_num, inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_motor_offset(inst->mot->ctx, joint_num, inst->mot->joint_get_motor_offset(inst->mot->ctx, joint_num) - (offset));
            if (inst->H[joint_num].home_flags & HOME_ABSOLUTE_ENCODER) {
                if (inst->H[joint_num].home_flags & HOME_NO_FINAL_MOVE) {
                    inst->H[joint_num].home_state = HOME_FINISHED;
                    immediate_state = 1;
                    inst->H[joint_num].homed = 1; // finished absolute encoder
                    break;
                }
            }
            /* next state */
            inst->H[joint_num].home_state = HOME_FINAL_MOVE_START;
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
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, reset delay, wait until joint stops */
                inst->H[joint_num].pause_timer = 0;
                break;
            }
            /* has delay timed out? */
            if (inst->H[joint_num].pause_timer < (HOME_DELAY * inst->servo_freq)) {
                /* no, update timer and wait some more */
                inst->H[joint_num].pause_timer++;
                break;
            }
            inst->H[joint_num].pause_timer = 0;
            /* Although we don't know the exact home position yet, we
               we reset the joint coordinates now so that screw error
               comp will be appropriate for this portion of the screw
               (previously we didn't know where we were at all). */
            /* set the current position to 'home_offset' */
            offset = inst->H[joint_num].home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num);
            /* this moves the internal position but does not affect the
               motor position */
            inst->mot->joint_set_pos_cmd(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_cmd(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_pos_fb(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, joint_num, inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, joint_num) + (offset));
            inst->mot->joint_set_motor_offset(inst->mot->ctx, joint_num, inst->mot->joint_get_motor_offset(inst->mot->ctx, joint_num) - (offset));
            /* set the index enable */
            inst->H[joint_num].index_enable = 1;
            /* set up a move at 'latch_vel' to find the index pulse */
            home_start_move(inst, joint_num, inst->H[joint_num].home_latch_vel);
            /* next state */
            inst->H[joint_num].home_state = HOME_INDEX_SEARCH_WAIT;
            break;

        case HOME_INDEX_SEARCH_START:
            /* This state is called after the machine has made a low
               speed pass to determine the limit switch location. It
               sets index-enable, which tells the encoder driver to
               reset its counter to zero and clear the enable when the
               next index pulse arrives. */
            /* set the index enable */
            inst->H[joint_num].index_enable = 1;
            /* and move right into the waiting state */
            inst->H[joint_num].home_state = HOME_INDEX_SEARCH_WAIT;
            immediate_state = 1;
            ABORT_CHECK(joint_num);
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
            if ( inst->H[joint_num].index_enable == 0 ) {
                /* yes, stop motion */
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
                /* go to next step */
                inst->H[joint_num].home_state = HOME_SET_INDEX_POSITION;
                immediate_state = 1;
                break;
            }
            ABORT_CHECK(joint_num);
            break;

        case HOME_SET_INDEX_POSITION:
            /* This state is called when the encoder has been reset at
               the index pulse position.  It sets the current joint
               position to 'home_offset', which is the location of the
               index pulse in joint coordinates. */
            /* set the current position to 'home_offset' */
            inst->mot->joint_set_motor_offset(inst->mot->ctx, joint_num, - inst->H[joint_num].home_offset);
            { double pf = inst->mot->joint_get_motor_pos_fb(inst->mot->ctx, joint_num)
                        - (inst->mot->joint_get_backlash_filt(inst->mot->ctx, joint_num)
                         + inst->mot->joint_get_motor_offset(inst->mot->ctx, joint_num));
              inst->mot->joint_set_pos_fb(inst->mot->ctx, joint_num, pf);
              inst->mot->joint_set_pos_cmd(inst->mot->ctx, joint_num, pf);
              inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, joint_num, pf);
            }

            if (inst->H[joint_num].home_flags & HOME_INDEX_NO_ENCODER_RESET) {
               /* Special case: encoder does not reset on index pulse.
                  This moves the internal position but does not affect
                  the motor position */
               offset = inst->H[joint_num].home_offset - inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num);
               inst->mot->joint_set_pos_cmd(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_cmd(inst->mot->ctx, joint_num) + (offset));
               inst->mot->joint_set_pos_fb(inst->mot->ctx, joint_num, inst->mot->joint_get_pos_fb(inst->mot->ctx, joint_num) + (offset));
               inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, joint_num, inst->mot->joint_get_free_tp_curr_pos(inst->mot->ctx, joint_num) + (offset));
               inst->mot->joint_set_motor_offset(inst->mot->ctx, joint_num, inst->mot->joint_get_motor_offset(inst->mot->ctx, joint_num) - (offset));
            }

            /* next state */
            inst->H[joint_num].home_state = HOME_FINAL_MOVE_START;
            immediate_state = 1;
            break;

        case HOME_FINAL_MOVE_START:
            /* This state is called once the joint coordinate system is
               set properly.  It moves to the actual 'home' position,
               which is not necessarily the position of the home switch
               or index pulse. */
            /* is the joint already moving? */
            if (inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, reset delay, wait until joint stops */
                inst->H[joint_num].pause_timer = 0;
                break;
            }
            /* has delay timed out? */
            if (inst->H[joint_num].pause_timer < (HOME_DELAY * inst->servo_freq)) {
                /* no, update timer and wait some more */
                inst->H[joint_num].pause_timer++;
            }
            inst->H[joint_num].pause_timer = 0;

            // neg home sequence: sync final move
            if  (    (inst->H[joint_num].home_sequence  < 0)
                  && !sync_ready(inst, joint_num) ) {
                break; // not all joints at *this* state, wait for them
            }

            /* plan a final move to home position */
            inst->mot->joint_set_free_tp_pos_cmd(inst->mot->ctx, joint_num, inst->H[joint_num].home);
            /* if home_vel is set (>0) then we use that, otherwise we rapid there */
            if (inst->H[joint_num].home_final_vel > 0) {
                inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, joint_num, fabs(inst->H[joint_num].home_final_vel));
                /* clamp on max vel for this joint */
                { double vl = inst->mot->joint_get_vel_limit(inst->mot->ctx, joint_num);
                    if (inst->mot->joint_get_free_tp_max_vel(inst->mot->ctx, joint_num) > vl)
                        inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, joint_num, vl);
                }
            } else {
                inst->mot->joint_set_free_tp_max_vel(inst->mot->ctx, joint_num, inst->mot->joint_get_vel_limit(inst->mot->ctx, joint_num));
            }
            /* start the move */
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 1);
            inst->H[joint_num].home_state = HOME_FINAL_MOVE_WAIT;
            break;

        case HOME_FINAL_MOVE_WAIT:
            /* This state is called while the machine makes its final
               move to the home position.  It terminates when the machine
               arrives at the final location. If the move hits a limit
               before it arrives, the home is aborted. */

            // neg home sequence: sync final move
            if  (  (inst->H[joint_num].home_sequence  < 0)
                && !sync_ready(inst, joint_num) ) {
                break; // not all joints at *this* state, wait for them
            }

            /* have we arrived (and stopped) at home? */
            if (!inst->mot->joint_get_free_tp_active(inst->mot->ctx, joint_num)) {
                /* yes, stop motion */
                inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
                /* we're finally done */
                inst->H[joint_num].home_state = HOME_LOCK;
                immediate_state = 1;
                break;
            }
            if (inst->mot->joint_get_on_pos_limit(inst->mot->ctx, joint_num) || inst->mot->joint_get_on_neg_limit(inst->mot->ctx, joint_num)) {
                /* on limit, check to see if we should trip */
                if (!(inst->H[joint_num].home_flags & HOME_IGNORE_LIMITS)) {
                    /* not ignoring limits, time to quit */
                    gomc_log_errorf(inst->log, inst->name, "hit limit in home state j=%d", joint_num);
                    inst->H[joint_num].home_state = HOME_ABORT;
                    immediate_state = 1;
                    break;
                }
            }
            break;

        case HOME_LOCK:
            if (inst->H[joint_num].home_flags & HOME_UNLOCK_FIRST) {
                inst->mot->set_rotary_unlock(inst->mot->ctx, joint_num, 0);
            } else {
                immediate_state = 1;
            }
            inst->H[joint_num].home_state = HOME_LOCK_WAIT;
            break;

        case HOME_LOCK_WAIT:
            // if not yet locked, continue waiting
            if ((inst->H[joint_num].home_flags & HOME_UNLOCK_FIRST) &&
                inst->mot->get_rotary_unlock(inst->mot->ctx, joint_num)) break;

            // either we got here without a lock needed, or the
            // lock is now complete.
            inst->H[joint_num].home_state = HOME_FINISHED;
            immediate_state = 1;
            break;

        case HOME_FINISHED:
            inst->H[joint_num].homing = 0;
            inst->H[joint_num].homed = 1; // finished
            inst->H[joint_num].home_state = HOME_IDLE;
            if ( ! (inst->H[joint_num].home_flags & HOME_ABSOLUTE_ENCODER)) {
                inst->mot->joint_set_free_tp_curr_pos(inst->mot->ctx, joint_num, inst->H[joint_num].home);
            }
            immediate_state = 1;
            inst->H[joint_num].joint_in_sequence = 0;
            break;

        case HOME_ABORT:
            inst->H[joint_num].homing = 0;
            inst->H[joint_num].homed = 0;
            inst->H[joint_num].joint_in_sequence = 0;
            inst->mot->joint_set_free_tp_enable(inst->mot->ctx, joint_num, 0);
            inst->H[joint_num].home_state = HOME_IDLE;
            inst->H[joint_num].index_enable = 0;
            immediate_state = 1;
            break;

        default:
            /* should never get here */
            gomc_log_errorf(inst->log, inst->name, "unknown state '%d' during homing j=%d",
                            inst->H[joint_num].home_state,joint_num);
            inst->H[joint_num].home_state = HOME_ABORT;
            immediate_state = 1;
            break;
        }        /* end of switch(inst->H[joint_num].home_state) */
    } while (immediate_state);

    return homing_flag;
} // base_1joint_state_machine()

static bool base_do_homing(homemod_inst_t *inst)
{
    int  joint_num;
    int  homing_flag = 0;
    bool beginning_allhomed = base_get_allhomed(inst);

    do_homing_sequence(inst);
    /* loop thru joints, treat each one individually */
    for (joint_num = 0; joint_num < inst->all_joints; joint_num++) {
        if (!inst->H[joint_num].joint_in_sequence)            { continue; }
        if (!inst->mot->joint_get_active_flag(inst->mot->ctx, joint_num)) { continue; }
        // DEFAULT joint homing state machine:
        homing_flag += base_1joint_state_machine(inst, joint_num);
    }
    if ( homing_flag > 0 ) { /* one or more joint is homing */
        inst->homing_active = 1;
    } else { /* is a homing sequence in progress? */
        if (inst->sequence_state == HOME_SEQUENCE_IDLE) {
            /* no, single joint only, we're done */
            inst->homing_active = 0;
        }
    }
    // return 1 if homing completed this period
    if (!beginning_allhomed && base_get_allhomed(inst)) {inst->homing_active=0; return 1;}
    return 0;
} // base_do_homing(inst)

/***********************************************************************
*                      PUBLIC FUNCTIONS                                *
************************************************************************/

//========================================================
// ═══════════════════════════════════════════════════════════════════════════
// GMI cmod interface — direct implementation, no wrapper layer
// ═══════════════════════════════════════════════════════════════════════════

static int32_t gmi_home_init(void *ctx, int32_t comp_id, double servo_period,
    int32_t n_joints, int32_t n_extrajoints)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    inst->comp_id = comp_id;
    return base_homing_init(inst, comp_id, servo_period, n_joints, n_extrajoints);
}

static int32_t gmi_home_set_joint_params(void *ctx, int32_t jno, double offset, double home,
    double home_final_vel, double home_search_vel,
    double home_latch_vel, int32_t home_flags,
    int32_t home_sequence, int32_t volatile_home)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    base_set_joint_homing_params(inst, jno, offset, home,
        home_final_vel, home_search_vel, home_latch_vel,
        home_flags, home_sequence, (bool)volatile_home);
    return 0;
}

static int32_t gmi_home_update_joint_params(void *ctx, int32_t jno, double home_offset,
    double home_home, int32_t home_sequence)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    base_update_joint_homing_params(inst, jno, home_offset, home_home, home_sequence);
    return 0;
}

static int32_t gmi_home_read_in_pins(void *ctx, int32_t njoints)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    base_read_homing_in_pins(inst, njoints);
    return 0;
}

static int32_t gmi_home_do_homing(void *ctx) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_do_homing(inst); }

static int32_t gmi_home_write_out_pins(void *ctx, int32_t njoints)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    base_write_homing_out_pins(inst, njoints);
    return 0;
}

static int32_t gmi_home_do_home_joint(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; base_do_home_joint(inst, jno); return 0; }
static int32_t gmi_home_do_cancel(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; base_do_cancel_homing(inst, jno); return 0; }

static int32_t gmi_home_set_unhomed(void *ctx, int32_t jno, home_motion_state_t motstate)
{
    homemod_inst_t *inst = (homemod_inst_t *)ctx;
    base_set_unhomed(inst, jno, (motion_state_t)motstate);
    return 0;
}

static int32_t gmi_home_get_allhomed(void *ctx) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_allhomed(inst); }
static int32_t gmi_home_get_is_active(void *ctx) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_homing_is_active(inst); }
static int32_t gmi_home_get_sequence(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return base_get_home_sequence(inst, jno); }
static int32_t gmi_home_get_homing(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_homing(inst, jno); }
static int32_t gmi_home_get_homed(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_homed(inst, jno); }
static int32_t gmi_home_get_index_enable(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_index_enable(inst, jno); }
static int32_t gmi_home_get_needs_unlock_first(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_home_needs_unlock_first(inst, jno); }
static int32_t gmi_home_get_is_idle(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_home_is_idle(inst, jno); }
static int32_t gmi_home_get_is_synchronized(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_home_is_synchronized(inst, jno); }
static int32_t gmi_home_get_at_index_search_wait(void *ctx, int32_t jno) { homemod_inst_t *inst = (homemod_inst_t *)ctx; return (int32_t)base_get_homing_at_index_search_wait(inst, jno); }

// ─── cmod lifecycle ─────────────────────────────────────────────────────

static void home_cmod_destroy(cmod_t *self) {
    homemod_inst_t *inst = (homemod_inst_t *)((char *)self - offsetof(homemod_inst_t, cmod));
    free(inst);
}

static int home_cmod_init(cmod_t *self)
{
    homemod_inst_t *inst = (homemod_inst_t *)((char *)self - offsetof(homemod_inst_t, cmod));
    const mot_callbacks_t *mot = mot_api_get(inst->api, inst->mot_instance);
    if (!mot) return -1;
    inst->mot = mot;
    return 0;
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    /* Allocate per-instance state */
    homemod_inst_t *inst = calloc(1, sizeof(homemod_inst_t));
    if (!inst) return -1;

    inst->api = env->api;
    inst->hal = env->hal;
    inst->log = env->log;
    snprintf(inst->name, sizeof(inst->name), "%s", name);

    /* Set pin_prefix: empty for default module name (bare pins), "name." for aliases */
    if (strcmp(name, "homemod") == 0) {
        inst->pin_prefix[0] = '\0';
    } else {
        snprintf(inst->pin_prefix, sizeof(inst->pin_prefix), "%s.", name);
    }

    /* Parse mot_instance parameter (default: "motmod") */
    snprintf(inst->mot_instance, sizeof(inst->mot_instance), "motmod");
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "mot_instance=", 13) == 0)
            snprintf(inst->mot_instance, sizeof(inst->mot_instance), "%s", argv[i] + 13);
    }

    /* Set up per-instance callbacks with ctx pointing to this instance */
    inst->callbacks = (home_callbacks_t)GMI_HOME_CALLBACKS;
    inst->callbacks.ctx = inst;

    int rc = home_api_register(env->api, name, &inst->callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register home API: %d", rc);
        free(inst);
        return rc;
    }

    inst->cmod.Init    = home_cmod_init;
    inst->cmod.Start   = NULL;
    inst->cmod.Destroy = home_cmod_destroy;
    *out = &inst->cmod;
    return 0;
}
