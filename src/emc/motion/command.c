/********************************************************************
* Description: command.c
*   emcmotCommandhandler() takes commands passed from user space and
*   performs various functions based on the value in emcmotCommand->command.
*   For the full list, see the EMCMOT_COMMAND enum in motion.h
*
* pc says:
*
*   Most of the configs would be better off being passed via an ioctl
*   implimentation leaving pure realtime data to be handled by
*   emcmotCommandHandler() - This would provide a small performance
*   increase on slower systems.
*
* jmk says:
*
*   Using commands to set config parameters is "undesireable", because
*   of the large amount of code needed for each parameter.  Today you
*   need to do the following to add a single new parameter called foo:
*
*   1)  Add a member 'foo' to the config or joint structure in motion.h
*   2)  Add a command 'EMCMOT_SET_FOO" to the cmd_code_t enum in motion.h
*   3)  Add a field to the command_t struct for the value used by
*       the set command (if there isn't already one that can be used.)
*   4)  Add a case to the giant switch statement in command.c to
*       handle the 'EMCMOT_SET_FOO' command.
*   5)  Write a function emcSetFoo() in taskintf.cc to issue the command.
*   6)  Add a prototype for emcSetFoo() to emc.hh
*   7)  Add code to iniaxis.cc (or one of the other inixxx.cc files) to
*       get the value from the ini file and call emcSetFoo().  (Note
*       that each parameter has about 16 lines of code, but the code
*       is identical except for variable/parameter names.)
*   8)  Add more code to iniaxis.cc to write the new value back out
*       to the ini file.
*   After all that, you have the abililty to get a number from the
*   ini file to a structure in shared memory where the motion controller
*   can actually use it.  However, if you want to manipulate that number
*   using NML, you have to do more:
*   9)  Add a #define EMC_SET_FOO_TYPE to emc.hh
*   10) Add a class definition for EMC_SET_FOO to emc.hh
*   11) Add a case to a giant switch statement in emctaskmain.cc to
*       call emcSetFoo() when the NML command is received.  (Actually
*       there are about 6 switch statements that need at least a
*       case label added.
*   12) Add cases to two giant switch statements in emc.cc, associated
*       with looking up and formating the command.
*
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include <float.h>
#include "posemath.h"
#include "rtapi.h"
#include "hal.h"
#include "motion.h"
#include "motion_debug.h"
#include "motion_struct.h"
#include "mot_priv.h"
#include "rtapi_math.h"
#include "motion_types.h"

#include "tp_debug.h"

#define ABS(x) (((x) < 0) ? -(x) : (x))

// Mark strings for translation, but defer translation to userspace
#define _(s) (s)

static int rehomeAll;

/* loops through the active joints and checks if any are not homed */
int checkAllHomed(void)
{
    int joint_num;
    emcmot_joint_t *joint;

    for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
	/* point to joint data */
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, don't even look at its limits */
	    continue;
	}
	if (!GET_JOINT_HOMED_FLAG(joint)) {
	    /* if any of the joints is not homed return false */
	    return 0;
	}
    }
    /* return true if all active joints are homed*/
    return 1;
}

/* limits_ok() returns 1 if none of the hard limits are set,
   0 if any are set. Called on a linear and circular move. */
STATIC int limits_ok(void)
{
    int joint_num;
    emcmot_joint_t *joint;

    for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
	/* point to joint data */
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, don't even look at its limits */
	    continue;
	}

	if (GET_JOINT_PHL_FLAG(joint) || GET_JOINT_NHL_FLAG(joint)) {
	    return 0;
	}
    }

    return 1;
}

/* check the value of the joint and velocity against current position,
   returning 1 (okay) if the request is to jog off the limit, 0 (bad)
   if the request is to jog further past a limit. */
STATIC int joint_jog_ok(int joint_num, double vel)
{
    emcmot_joint_t *joint;
    int neg_limit_override, pos_limit_override;

    /* point to joint data */
    joint = &joints[joint_num];
    /* are any limits for this joint overridden? */
    neg_limit_override = emcmotStatus->overrideLimitMask & ( 1 << (joint_num*2));
    pos_limit_override = emcmotStatus->overrideLimitMask & ( 2 << (joint_num*2));
    if ( neg_limit_override && pos_limit_override ) {
	/* both limits have been overridden at the same time.  This
	   happens only when they both share an input, but means it
	   is impossible to know which direction is safe to move.  So
	   we skip the following tests... */
	return 1;
    }
    if (joint_num < 0 || joint_num >= emcmotConfig->numJoints) {
	reportError(_("Can't jog invalid joint number %d."), joint_num);
	return 0;
    }
    if (vel > 0.0 && GET_JOINT_PHL_FLAG(joint)) {
	reportError(_("Can't jog joint %d further past max hard limit."),
	    joint_num);
	return 0;
    }
    if (vel < 0.0 && GET_JOINT_NHL_FLAG(joint)) {
	reportError(_("Can't jog joint %d further past min hard limit."),
	    joint_num);
	return 0;
    }
    refresh_jog_limits(joint);
    if ( vel > 0.0 && (joint->pos_cmd > joint->max_jog_limit) ) {
	reportError(_("Can't jog joint %d further past max soft limit."),
	    joint_num);
	return 0;
    }
    if ( vel < 0.0 && (joint->pos_cmd < joint->min_jog_limit) ) {
	reportError(_("Can't jog joint %d further past min soft limit."),
	    joint_num);
	return 0;
    }
    /* okay to jog */
    return 1;
}

/* Jogs limits change, based on whether the machine is homed or
   or not.  If not homed, the limits are relative to the current
   position by +/- the full range of travel.  Once homed, they
   are absolute.
*/
void refresh_jog_limits(emcmot_joint_t *joint)
{
    double range;

    if (GET_JOINT_HOMED_FLAG(joint)) {
	/* if homed, set jog limits using soft limits */
	joint->max_jog_limit = joint->max_pos_limit;
	joint->min_jog_limit = joint->min_pos_limit;
    } else {
	/* not homed, set limits based on current position */
	range = joint->max_pos_limit - joint->min_pos_limit;
	joint->max_jog_limit = joint->pos_fb + range;
	joint->min_jog_limit = joint->pos_fb - range;
    }
}

static int check_axis_constraint(double target, int id, char *move_type,
                                 int axis_no, char axis_name) {
    int in_range = 1;
    double nl = axes[axis_no].min_pos_limit;
    double pl = axes[axis_no].max_pos_limit;

    double eps = 1e-308;

    if (    (fabs(target) < eps)
         && (fabs(axes[axis_no].min_pos_limit) < eps)
         && (fabs(axes[axis_no].max_pos_limit) < eps) ) { return 1;}

    if(target < nl) {
        in_range = 0;
        reportError(_("%s move on line %d would exceed %c's %s limit"),
                    move_type, id, axis_name, _("negative"));
    }

    if(target > pl) {
        in_range = 0;
        reportError(_("%s move on line %d would exceed %c's %s limit"),
                    move_type, id, axis_name, _("positive"));
    }

    return in_range;
}

/* inRange() returns non-zero if the position lies within the joint
   limits, or 0 if not.  It also reports an error for each joint limit
   violation.  It's possible to get more than one violation per move. */
STATIC int inRange(EmcPose pos, int id, char *move_type)
{
    double joint_pos[EMCMOT_MAX_JOINTS];
    int joint_num;
    emcmot_joint_t *joint;
    int in_range = 1;

    if(check_axis_constraint(pos.tran.x, id, move_type, 0, 'X') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.tran.y, id, move_type, 1, 'Y') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.tran.z, id, move_type, 2, 'Z') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.a, id, move_type, 3, 'A') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.b, id, move_type, 4, 'B') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.c, id, move_type, 5, 'C') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.u, id, move_type, 6, 'U') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.v, id, move_type, 7, 'V') == 0) 
        in_range = 0;
    if(check_axis_constraint(pos.w, id, move_type, 8, 'W') == 0) 
        in_range = 0;

    /* Now, check that the endpoint puts the joints within their limits too */

    /* fill in all joints with 0 */
    for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
	joint_pos[joint_num] = 0.0;
    }

    /* now fill in with real values, for joints that are used */
    if (kinematicsInverse(&pos, joint_pos, &iflags, &fflags) < 0)
    {
	reportError(_("%s move on line %d fails kinematicsInverse"),
		    move_type, id);
	return 0;
    }

    for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
	/* point to joint data */
	joint = &joints[joint_num];

	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, don't even look at its limits */
	    continue;
	}
	if(!isfinite(joint_pos[joint_num]))
	{
	    reportError(_("%s move on line %d gave non-finite joint location on joint %d"),
		    move_type, id, joint_num);
	    in_range = 0;
	    continue;
	}
	if (joint_pos[joint_num] > joint->max_pos_limit) {
            in_range = 0;
	    reportError(_("%s move on line %d would exceed joint %d's positive limit"),
			move_type, id, joint_num);
        }

        if (joint_pos[joint_num] < joint->min_pos_limit) {
	    in_range = 0;
	    reportError(_("%s move on line %d would exceed joint %d's negative limit"),
			move_type, id, joint_num);
	}
    }
    return in_range;
}

/* clearHomes() will clear the homed flags for joints that have moved
   since homing, outside coordinated control, for machines with no
   forward kinematics. This is used in conjunction with the rehomeAll
   flag, which is set for any coordinated move that in general will
   result in all joints moving. The flag is consulted whenever a joint
   is jogged in joint mode, so that either its flag can be cleared if
   no other joints have moved, or all have to be cleared. */
void clearHomes(int joint_num)
{
    int n;
    emcmot_joint_t *joint;

    if (emcmotConfig->kinType == KINEMATICS_INVERSE_ONLY) {
	if (rehomeAll) {
	    for (n = 0; n < emcmotConfig->numJoints; n++) {
		/* point at joint data */
		joint = &(joints[n]);
		/* clear flag */
		SET_JOINT_HOMED_FLAG(joint, 0);
	    }
	} else {
	    /* point at joint data */
	    joint = &joints[joint_num];
	    /* clear flag */
	    SET_JOINT_HOMED_FLAG(joint, 0);
	}
    }
}

void emcmotSetRotaryUnlock(int jnum, int unlock) {
    if (NULL == emcmot_hal_data->joint[jnum].unlock) {
        reportError(
        "emcmotSetRotaryUnlock(): No unlock pin configured for joint %d\n"
        "   Use motmod parameter: unlock_joints_mask=%X",
        jnum,1<<jnum);
        return;
    }
    *(emcmot_hal_data->joint[jnum].unlock) = unlock;
}

int emcmotGetRotaryIsUnlocked(int jnum) {
    static int gave_message = 0;
    if (NULL == emcmot_hal_data->joint[jnum].unlock) {
        if (!gave_message) {
            reportError(
            "emcmotGetRotaryUnlocked(): No unlock pin configured for joint %d\n"
            "   Use motmod parameter: unlock_joints_mask=%X'",
            jnum,1<<jnum);
        }
        gave_message = 1;
        return 0;
    }
    return *(emcmot_hal_data->joint[jnum].is_unlocked);
}

/*! \function emcmotDioWrite()

  sets or clears a HAL DIO pin, 
  pins get exported at runtime
  
  index is valid from 0 to emcmotConfig->num_dio <= EMCMOT_MAX_DIO, defined in emcmotcfg.h
  
*/
void emcmotDioWrite(int index, char value)
{
    if ((index >= emcmotConfig->numDIO) || (index < 0)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "ERROR: index out of range, %d not in [0..%d] (increase num_dio/EMCMOT_MAX_DIO=%d)\n", index, emcmotConfig->numDIO, EMCMOT_MAX_DIO);
    } else {
	if (value != 0) {
	    *(emcmot_hal_data->synch_do[index])=1;
	} else {
	    *(emcmot_hal_data->synch_do[index])=0;
	}
    }
}

/*! \function emcmotAioWrite()

  sets or clears a HAL AIO pin, 
  pins get exported at runtime
  
  index is valid from 0 to emcmotConfig->num_aio <= EMCMOT_MAX_AIO, defined in emcmotcfg.h
  
*/
void emcmotAioWrite(int index, double value)
{
    if ((index >= emcmotConfig->numAIO) || (index < 0)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "ERROR: index out of range, %d not in [0..%d] (increase num_aio/EMCMOT_MAX_AIO=%d)\n", index, emcmotConfig->numAIO, EMCMOT_MAX_AIO);
    } else {
        *(emcmot_hal_data->analog_output[index]) = value;
    }
}

STATIC int is_feed_type(int motion_type)
{
    switch(motion_type) {
    case EMC_MOTION_TYPE_ARC:
    case EMC_MOTION_TYPE_FEED:
    case EMC_MOTION_TYPE_PROBING:
        return 1;
    default:
        rtapi_print_msg(RTAPI_MSG_ERR, "Internal error: unhandled motion type %d\n", motion_type);
    case EMC_MOTION_TYPE_TOOLCHANGE:
    case EMC_MOTION_TYPE_TRAVERSE:
    case EMC_MOTION_TYPE_INDEXROTARY:
        return 0;
    }
}

/*
  emcmotCommandHandler() is called each main cycle to read the
  shared memory buffer
  */
void emcmotCommandHandler(void *arg, long period)
{
    int joint_num, axis_num, spindle_num;
    int n;
    emcmot_joint_t *joint;
    emcmot_axis_t *axis;
    double tmp1;
    emcmot_comp_entry_t *comp_entry;
    char issue_atspeed = 0;
    int abort = 0;
    char* emsg;

    /* check for split read */
    if (emcmotCommand->head != emcmotCommand->tail) {
	emcmotDebug->split++;
	return;			/* not really an error */
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
	
	/* ...and process command */

        joint = 0;
        axis  = 0;
        joint_num = emcmotCommand->joint;
        axis_num  = emcmotCommand->axis;

//-----------------------------------------------------------------------------
// joints_axes test for unexpected conditions
// example: non-cooperating guis
// example: attempt to jog locking indexer axis letter
        if (   emcmotCommand->command == EMCMOT_JOG_CONT
            || emcmotCommand->command == EMCMOT_JOG_INCR
            || emcmotCommand->command == EMCMOT_JOG_ABS
           ) {
           if (GET_MOTION_TELEOP_FLAG() && axis_num < 0) {
               emsg = "command.com teleop: unexpected negative axis_num";
               if (joint_num >= 0) {
                   emsg = "Mode is TELEOP, cannot jog joint";
               }
               abort = 1;
           }
           if (!GET_MOTION_TELEOP_FLAG() && joint_num < 0) {
               emsg = "command.com !teleop: unexpected negative joint_num";
               if (axis_num >= 0) {
                   emsg = "Mode is NOT TELEOP, cannot jog axis coordinate";
               }
               abort = 1;
           }
           if (GET_MOTION_TELEOP_FLAG()) {
                axis = &axes[axis_num];
                if ( (axis_num >= 0) && (axis->locking_joint >= 0) ) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                    "Cannot jog a locking indexer AXIS_%c,joint_num=%d\n",
                    "XYZABCUVW"[axis_num],axis->locking_joint);
                    return;
                }
           }
        }
        if (abort) {
          switch (emcmotCommand->command) {
          case EMCMOT_JOG_CONT:
               rtapi_print_msg(RTAPI_MSG_ERR,"JOG_CONT %s\n",emsg);
               break;
          case EMCMOT_JOG_INCR:
               rtapi_print_msg(RTAPI_MSG_ERR,"JOG_INCR %s\n",emsg);
               break;
          case EMCMOT_JOG_ABS:
               rtapi_print_msg(RTAPI_MSG_ERR,"JOG_ABS %s\n",emsg);
               break;
          default: break;
          }
          return;
        }

        if (joint_num >= 0 && joint_num < emcmotConfig->numJoints) {
            joint = &joints[joint_num];
            if (   (   emcmotCommand->command == EMCMOT_JOG_CONT
                    || emcmotCommand->command == EMCMOT_JOG_INCR
                    || emcmotCommand->command == EMCMOT_JOG_ABS
                   )
                && !(GET_MOTION_TELEOP_FLAG())
                && (joint->home_sequence < 0)
                && !emcmotStatus->homing_active
               ) {
                  if (emcmotConfig->kinType == KINEMATICS_IDENTITY) {
                      rtapi_print_msg(RTAPI_MSG_ERR,
                      "Homing is REQUIRED to jog requested coordinate\n"
                      "because joint (%d) in home_sequence is negative (%d)\n"
                      ,joint_num,joint->home_sequence);
                  } else {
                      rtapi_print_msg(RTAPI_MSG_ERR,
                      "Cannot jog joint %d because home_sequence is negative (%d)\n"
                      ,joint_num,joint->home_sequence);
                  }
                  return;
            }
        }
        if (axis_num >= 0 && axis_num < EMCMOT_MAX_AXIS) {
            axis = &axes[axis_num];
        }
	switch (emcmotCommand->command) {
	case EMCMOT_ABORT:
	    /* abort motion */
	    /* can happen at any time */
	    /* this command attempts to stop all machine motion. it looks at
	       the current mode and acts accordingly, if in teleop mode, it
	       sets the desired velocities to zero, if in coordinated mode,
	       it calls the traj planner abort function (don't know what that
	       does yet), and if in free mode, it disables the free mode traj
	       planners which stops joint motion */
	    rtapi_print_msg(RTAPI_MSG_DBG, "ABORT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    /* check for coord or free space motion active */
	    if (GET_MOTION_TELEOP_FLAG()) {
		for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
		    /* point to joint struct */
		    axis = &axes[axis_num];
		    /* tell teleop planner to stop */
		    axis->teleop_tp.enable = 0;
                }
	    } else if (GET_MOTION_COORD_FLAG()) {
		tpAbort(&emcmotDebug->coord_tp);
	    } else {
		for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
		    /* point to joint struct */
		    joint = &joints[joint_num];
		    /* tell joint planner to stop */
		    joint->free_tp.enable = 0;
		    /* stop homing if in progress */
		    if ( joint->home_state != HOME_IDLE ) {
			joint->home_state = HOME_ABORT;
		    }
		}
	    }
            SET_MOTION_ERROR_FLAG(0);
	    /* clear joint errors (regardless of mode) */
	    for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
		/* point to joint struct */
		joint = &joints[joint_num];
		/* update status flags */
		SET_JOINT_ERROR_FLAG(joint, 0);
		SET_JOINT_FAULT_FLAG(joint, 0);
	    }
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_JOINT_ABORT:
	    /* abort one joint */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOINT_ABORT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (GET_MOTION_TELEOP_FLAG()) {
		/* tell teleop planner to stop */
		if (axis != 0) axis->teleop_tp.enable = 0;
		/* do nothing in teleop mode */
	    } else if (GET_MOTION_COORD_FLAG()) {
		/* do nothing in coord mode */
	    } else {
		/* tell joint planner to stop */
		if (joint != 0) joint->free_tp.enable = 0;
		/* validate joint */
		if (joint == 0) { break; }
		/* stop homing if in progress */
		if ( joint->home_state != HOME_IDLE ) {
		    joint->home_state = HOME_ABORT;
		}
		/* update status flags */
		SET_JOINT_ERROR_FLAG(joint, 0);
	    }
	    break;

	case EMCMOT_FREE:
            for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
              axis = &axes[axis_num];
              if (axis != 0) { axis->teleop_tp.enable = 0; }
            }
	    /* change the mode to free mode motion (joint mode) */
	    /* can be done at any time */
	    /* this code doesn't actually make the transition, it merely
	       requests the transition by clearing a couple of flags */
	    /* reset the emcmotDebug->coordinating flag to defer transition
	       to controller cycle */
	    rtapi_print_msg(RTAPI_MSG_DBG, "FREE");
	    emcmotDebug->coordinating = 0;
	    emcmotDebug->teleoperating = 0;
	    break;

	case EMCMOT_COORD:
	    /* change the mode to coordinated axis motion */
	    /* can be done at any time */
	    /* this code doesn't actually make the transition, it merely
	       tests a condition and then sets a flag requesting the
	       transition */
	    /* set the emcmotDebug->coordinating flag to defer transition to
	       controller cycle */

	    rtapi_print_msg(RTAPI_MSG_DBG, "COORD");
	    emcmotDebug->coordinating = 1;
	    emcmotDebug->teleoperating = 0;
	    if (emcmotConfig->kinType != KINEMATICS_IDENTITY) {
		if (!checkAllHomed()) {
		    reportError
			(_("all joints must be homed before going into coordinated mode"));
		    emcmotDebug->coordinating = 0;
		    break;
		}
	    }
	    break;

	case EMCMOT_TELEOP:
	    rtapi_print_msg(RTAPI_MSG_DBG, "TELEOP");
            switch_to_teleop_mode();
	    break;

	case EMCMOT_SET_NUM_JOINTS:
	    /* set the global NUM_JOINTS, which must be between 1 and
	       EMCMOT_MAX_JOINTS, inclusive */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_NUM_JOINTS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", emcmotCommand->joint);
	    if (( emcmotCommand->joint <= 0 ) ||
		( emcmotCommand->joint > EMCMOT_MAX_JOINTS )) {
		break;
	    }
	    emcmotConfig->numJoints = emcmotCommand->joint;
	    break;

	case EMCMOT_SET_NUM_SPINDLES:
	    /* set the global NUM_SPINDLES, which must be between 1 and
	       EMCMOT_MAX_SPINDLES, inclusive */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_NUM_SPINDLES");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", emcmotCommand->spindle);
	    if (( emcmotCommand->spindle <= 0 ) ||
		( emcmotCommand->spindle > EMCMOT_MAX_SPINDLES )) {
		break;
	    }
	    emcmotConfig->numSpindles = emcmotCommand->spindle;
	    break;

	case EMCMOT_SET_WORLD_HOME:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_WORLD_HOME");
	    emcmotStatus->world_home = emcmotCommand->pos;
	    break;

	case EMCMOT_SET_JOINT_HOMING_PARAMS:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_HOMING_PARAMS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0) {
		break;
	    }
	    joint->home_offset = emcmotCommand->offset;
	    joint->home = emcmotCommand->home;
	    joint->home_final_vel = emcmotCommand->home_final_vel;
	    joint->home_search_vel = emcmotCommand->search_vel;
	    joint->home_latch_vel = emcmotCommand->latch_vel;
	    joint->home_flags = emcmotCommand->flags;
	    joint->home_sequence = emcmotCommand->home_sequence;
	    joint->volatile_home = emcmotCommand->volatile_home;
	    break;

	case EMCMOT_UPDATE_JOINT_HOMING_PARAMS:
	    rtapi_print_msg(RTAPI_MSG_DBG, "UPDATE_JOINT_HOMING_PARAMS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0) {
		break;
	    }
	    joint->home_offset = emcmotCommand->offset;
	    joint->home = emcmotCommand->home;
	    joint->home_sequence = emcmotCommand->home_sequence;
	    break;

	case EMCMOT_OVERRIDE_LIMITS:
	    /* this command can be issued with joint < 0 to re-enable
	       limits, but they are automatically re-enabled at the
	       end of the next jog */
	    rtapi_print_msg(RTAPI_MSG_DBG, "OVERRIDE_LIMITS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint_num < 0) {
		/* don't override limits */
		rtapi_print_msg(RTAPI_MSG_DBG, "override off");
		emcmotStatus->overrideLimitMask = 0;
	    } else {
		rtapi_print_msg(RTAPI_MSG_DBG, "override on");
		emcmotStatus->overrideLimitMask = 0;
		for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
		    /* point at joint data */
		    joint = &joints[joint_num];
		    /* only override limits that are currently tripped */
		    if ( GET_JOINT_NHL_FLAG(joint) ) {
			emcmotStatus->overrideLimitMask |= (1 << (joint_num*2));
		    }
		    if ( GET_JOINT_PHL_FLAG(joint) ) {
			emcmotStatus->overrideLimitMask |= (2 << (joint_num*2));
		    }
		}
	    }
	    emcmotDebug->overriding = 0;
	    for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
		/* point at joint data */
		joint = &joints[joint_num];
		/* clear joint errors */
		SET_JOINT_ERROR_FLAG(joint, 0);
	    }
	    break;

	case EMCMOT_SET_JOINT_MOTOR_OFFSET:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_MOTOR_OFFSET");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if(joint == 0) {
		break;
	    }
	    joint->motor_offset = emcmotCommand->motor_offset;
	    break;

	case EMCMOT_SET_JOINT_POSITION_LIMITS:
	    /* set the position limits for the joint */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_POSITION_LIMITS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0) {
		break;
	    }
	    joint->min_pos_limit = emcmotCommand->minLimit;
	    joint->max_pos_limit = emcmotCommand->maxLimit;
	    break;

	case EMCMOT_SET_JOINT_BACKLASH:
	    /* set the backlash for the joint */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_BACKLASH");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0) {
		break;
	    }
	    joint->backlash = emcmotCommand->backlash;
	    break;

	    /*
	       Max and min ferror work like this: limiting ferror is
	       determined by slope of ferror line, = maxFerror/limitVel ->
	       limiting ferror = maxFerror/limitVel * vel. If ferror <
	       minFerror then OK else if ferror < limiting ferror then OK
	       else ERROR */
	case EMCMOT_SET_JOINT_MAX_FERROR:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_MAX_FERROR");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0 || emcmotCommand->maxFerror < 0.0) {
		break;
	    }
	    joint->max_ferror = emcmotCommand->maxFerror;
	    break;

	case EMCMOT_SET_JOINT_MIN_FERROR:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_MIN_FERROR");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0 || emcmotCommand->minFerror < 0.0) {
		break;
	    }
	    joint->min_ferror = emcmotCommand->minFerror;
	    break;

	case EMCMOT_JOG_CONT:
	    /* do a continuous jog, implemented as an incremental jog to the
	       limit.  When the user lets go of the button an abort will
	       stop the jog. */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOG_CONT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError(_("Can't jog joint when not enabled."));
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (emcmotStatus->homing_active) {
		reportError(_("Can't jog any joints while homing."));
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (emcmotStatus->net_feed_scale < 0.0001) {
		/* don't jog if feedhold is on or if feed override is zero */
		break;
	    }
            if (!GET_MOTION_TELEOP_FLAG()) {
	        if (joint->wheel_jjog_active) {
		    /* can't do two kinds of jog at once */
		    break;
	        }
                if (joint->home_flags & HOME_UNLOCK_FIRST) {
                    reportError("Can't jog locking joint_num=%d",joint_num);
                    SET_JOINT_ERROR_FLAG(joint, 1);
                    break;
                }
	        /* don't jog further onto limits */
	        if (!joint_jog_ok(joint_num, emcmotCommand->vel)) {
		    SET_JOINT_ERROR_FLAG(joint, 1);
		    break;
	        }
	        /* set destination of jog */
	        refresh_jog_limits(joint);
	        if (emcmotCommand->vel > 0.0) {
		    joint->free_tp.pos_cmd = joint->max_jog_limit;
	        } else {
		    joint->free_tp.pos_cmd = joint->min_jog_limit;
	        }
	        /* set velocity of jog */
	        joint->free_tp.max_vel = fabs(emcmotCommand->vel);
	        /* use max joint accel */
	        joint->free_tp.max_acc = joint->acc_limit;
	        /* lock out other jog sources */
	        joint->kb_jjog_active = 1;
	        /* and let it go */
	        joint->free_tp.enable = 1;
                for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
                    axis = &axes[axis_num];
                    if (axis != 0) { axis->teleop_tp.enable = 0; }
                }
	        /*! \todo FIXME - should we really be clearing errors here? */
	        SET_JOINT_ERROR_FLAG(joint, 0);
	        /* clear joints homed flag(s) if we don't have forward kins.
	           Otherwise, a transition into coordinated mode will incorrectly
	           assume the homed position. Do all if they've all been moved
	           since homing, otherwise just do this one */
	        clearHomes(joint_num);
            } else {
                // TELEOP  JOG_CONT
                if (GET_MOTION_ERROR_FLAG()) { break; }
                axis_hal_t *axis_data = &(emcmot_hal_data->axis[axis_num]);
                if (   axis->ext_offset_tp.enable
                    && (fabs(*(axis_data->external_offset)) > EOFFSET_EPSILON)) {
                    /* here: set pos_cmd to a big number so that with combined
                    *        teleop jog plus external offsets the soft limits
                    *        can always be reached
                    *  a fixed epsilon is used here for convenience
                    *  it is not the same as the epsilon used as a stopping 
                    *  criterion in control.c
                    */
                    if (emcmotCommand->vel > 0.0) {
                        axis->teleop_tp.pos_cmd =  1e12; // 1T halscope limit
                    } else {
                        axis->teleop_tp.pos_cmd = -1e12; // 1T halscope limit
                    }
                } else {
                    if (emcmotCommand->vel > 0.0) {
                        axis->teleop_tp.pos_cmd = axis->max_pos_limit;
                    } else {
                        axis->teleop_tp.pos_cmd = axis->min_pos_limit;
                    }
                }

	        axis->teleop_tp.max_vel = fabs(emcmotCommand->vel);
	        axis->teleop_tp.max_acc = axis->acc_limit;
	        axis->kb_ajog_active = 1;
                for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
                    joint = &joints[joint_num];
                    if (joint != 0) { joint->free_tp.enable = 0; }
                }
	        axis->teleop_tp.enable = 1;
            }
	    break;

	case EMCMOT_JOG_INCR:
	    /* do an incremental jog */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOG_INCR");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError(_("Can't jog joint when not enabled."));
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (emcmotStatus->homing_active) {
		reportError(_("Can't jog any joint while homing."));
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (emcmotStatus->net_feed_scale < 0.0001 ) {
		/* don't jog if feedhold is on or if feed override is zero */
		break;
	    }
            if (!GET_MOTION_TELEOP_FLAG()) {
	        if (joint->wheel_jjog_active) {
		    /* can't do two kinds of jog at once */
		    break;
	        }
                if (joint->home_flags & HOME_UNLOCK_FIRST) {
                    reportError("Can't jog locking joint_num=%d",joint_num);
                    SET_JOINT_ERROR_FLAG(joint, 1);
                    break;
                }
	        /* don't jog further onto limits */
	        if (!joint_jog_ok(joint_num, emcmotCommand->vel)) {
		    SET_JOINT_ERROR_FLAG(joint, 1);
		    break;
	        }
	        /* set target position for jog */
	        if (emcmotCommand->vel > 0.0) {
		    tmp1 = joint->free_tp.pos_cmd + emcmotCommand->offset;
	        } else {
		    tmp1 = joint->free_tp.pos_cmd - emcmotCommand->offset;
	        }
	        /* don't jog past limits */
	        refresh_jog_limits(joint);
	        if (tmp1 > joint->max_jog_limit) {
		    break;
	        }
	        if (tmp1 < joint->min_jog_limit) {
		    break;
	        }
	        /* set target position */
	        joint->free_tp.pos_cmd = tmp1;
	        /* set velocity of jog */
	        joint->free_tp.max_vel = fabs(emcmotCommand->vel);
	        /* use max joint accel */
	        joint->free_tp.max_acc = joint->acc_limit;
	        /* lock out other jog sources */
	        joint->kb_jjog_active = 1;
	        /* and let it go */
	        joint->free_tp.enable = 1;
                for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
                    axis = &axes[axis_num];
                    if (axis != 0) { axis->teleop_tp.enable = 0; }
                }
	        SET_JOINT_ERROR_FLAG(joint, 0);
	        /* clear joint homed flag(s) if we don't have forward kins.
	           Otherwise, a transition into coordinated mode will incorrectly
	           assume the homed position. Do all if they've all been moved
	           since homing, otherwise just do this one */
	        clearHomes(joint_num);
            } else {
                // TELEOP JOG_INCR
                if (GET_MOTION_ERROR_FLAG()) { break; }
	        if (emcmotCommand->vel > 0.0) {
		    tmp1 = axis->teleop_tp.pos_cmd + emcmotCommand->offset;
	        } else {
		    tmp1 = axis->teleop_tp.pos_cmd - emcmotCommand->offset;
	        }
                axis_hal_t *axis_data = &(emcmot_hal_data->axis[axis_num]);
                // a fixed epsilon is used here for convenience
                // it is not the same as the epsilon used as a stopping 
                // criterion in control.c
                if (   axis->ext_offset_tp.enable
                    && (fabs(*(axis_data->external_offset)) > EOFFSET_EPSILON)) {
                    // external_offsets: soft limit enforcement is in control.c
                } else {
                    if (tmp1 > axis->max_pos_limit) { break; }
                    if (tmp1 < axis->min_pos_limit) { break; }
                }

	        axis->teleop_tp.pos_cmd = tmp1;
	        axis->teleop_tp.max_vel = fabs(emcmotCommand->vel);
	        axis->teleop_tp.max_acc = axis->acc_limit;
	        axis->kb_ajog_active = 1;
	        axis->teleop_tp.enable = 1;
                for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
                    joint = &joints[joint_num];
                    if (joint != 0) { joint->free_tp.enable = 0; }
                }
            }
	    break;

	case EMCMOT_JOG_ABS:
	    /* do an absolute jog */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOG_ABS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError(_("Can't jog joint when not enabled."));
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (emcmotStatus->homing_active) {
		reportError(_("Can't jog any joints while homing."));
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
            if (!GET_MOTION_TELEOP_FLAG()) {
                // FREE JOG_ABS
                if (joint->wheel_jjog_active) {
                    /* can't do two kinds of jog at once */
                    break;
                }
                if (emcmotStatus->net_feed_scale < 0.0001 ) {
                    /* don't jog if feedhold is on or if feed override is zero */
                    break;
                }
                /* don't jog further onto limits */
                if (!joint_jog_ok(joint_num, emcmotCommand->vel)) {
                    SET_JOINT_ERROR_FLAG(joint, 1);
                    break;
                }
                /*! \todo FIXME-- use 'goal' instead */
                joint->free_tp.pos_cmd = emcmotCommand->offset;
                /* don't jog past limits */
                refresh_jog_limits(joint);
                if (joint->free_tp.pos_cmd > joint->max_jog_limit) {
                    joint->free_tp.pos_cmd = joint->max_jog_limit;
                }
                if (joint->free_tp.pos_cmd < joint->min_jog_limit) {
                    joint->free_tp.pos_cmd = joint->min_jog_limit;
                }
                /* set velocity of jog */
                joint->free_tp.max_vel = fabs(emcmotCommand->vel);
                /* use max joint accel */
                joint->free_tp.max_acc = joint->acc_limit;
                /* lock out other jog sources */
                joint->kb_jjog_active = 1;
                /* and let it go */
                joint->free_tp.enable = 1;
                SET_JOINT_ERROR_FLAG(joint, 0);
                /* clear joint homed flag(s) if we don't have forward kins.
                   Otherwise, a transition into coordinated mode will incorrectly
                   assume the homed position. Do all if they've all been moved
                   since homing, otherwise just do this one */
                clearHomes(joint_num);
            } else {
                axis->kb_ajog_active = 1;
                // TELEOP JOG_ABS
                if (axis->wheel_ajog_active) { break; }
	        if (emcmotCommand->vel > 0.0) {
		    tmp1 = axis->teleop_tp.pos_cmd + emcmotCommand->offset;
	        } else {
		    tmp1 = axis->teleop_tp.pos_cmd - emcmotCommand->offset;
	        }
	        if (tmp1 > axis->max_pos_limit) { break; }
	        if (tmp1 < axis->min_pos_limit) { break; }
                axis->teleop_tp.pos_cmd = tmp1;
                axis->teleop_tp.max_vel = fabs(emcmotCommand->vel);
                axis->teleop_tp.max_acc = axis->acc_limit;
                axis->kb_ajog_active = 1;
                axis->teleop_tp.enable = 1;
                for (joint_num = 0; joint_num < emcmotConfig->numJoints; joint_num++) {
                   joint = &joints[joint_num];
                   if (joint != 0) { joint->free_tp.enable = 0; }
                }
                return;
            }
            break;

	case EMCMOT_SET_TERM_COND:
	    /* sets termination condition for motion emcmotDebug->coord_tp */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_TERM_COND");
	    tpSetTermCond(&emcmotDebug->coord_tp, emcmotCommand->termCond, emcmotCommand->tolerance);
	    break;

	case EMCMOT_SET_SPINDLESYNC:
		tpSetSpindleSync(&emcmotDebug->coord_tp, emcmotCommand->spindle, emcmotCommand->spindlesync, emcmotCommand->flags);
		break;

	case EMCMOT_SET_LINE:
	    /* emcmotDebug->coord_tp up a linear move */
	    /* requires motion enabled, coordinated mode, not on limits */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_LINE");
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError(_("need to be enabled, in coord mode for linear move"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos, emcmotCommand->id, "Linear")) {
		reportError(_("invalid params in linear command"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!limits_ok()) {
		reportError(_("can't do linear move with limits exceeded"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

		if(emcmotStatus->atspeed_next_feed && is_feed_type(emcmotCommand->motion_type) ) {
			issue_atspeed = 1;
			emcmotStatus->atspeed_next_feed = 0;
		}
		if(!is_feed_type(emcmotCommand->motion_type) &&
				emcmotStatus->spindle_status[emcmotCommand->spindle].css_factor) {
			emcmotStatus->atspeed_next_feed = 1;
		}

	    /* append it to the emcmotDebug->coord_tp */
	    tpSetId(&emcmotDebug->coord_tp, emcmotCommand->id);
        int res_addline = tpAddLine(&emcmotDebug->coord_tp, emcmotCommand->pos, emcmotCommand->motion_type, 
                                emcmotCommand->vel, emcmotCommand->ini_maxvel, 
                                emcmotCommand->acc, emcmotStatus->enables_new, issue_atspeed,
                                emcmotCommand->turn);
        //KLUDGE ignore zero length line
        if (res_addline < 0) {
            reportError(_("can't add linear move at line %d, error code %d"),
                    emcmotCommand->id, res_addline);
            emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
            tpAbort(&emcmotDebug->coord_tp);
            SET_MOTION_ERROR_FLAG(1);
            break;
        } else if (res_addline != 0) {
            //TODO make this hand-shake more explicit
            //KLUDGE Non fatal error, need to restore state so that the next
            //line properly handles at_speed
            if (issue_atspeed) {
                emcmotStatus->atspeed_next_feed = 1;
            }
        } else {
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all joints need rehoming, if any
		   joint is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_CIRCLE:
	    /* emcmotDebug->coord_tp up a circular move */
	    /* requires coordinated mode, enable on, not on limits */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_CIRCLE");
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError(_("need to be enabled, in coord mode for circular move"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos, emcmotCommand->id, "Circular")) {
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!limits_ok()) {
		reportError(_("can't do circular move with limits exceeded"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }
            if(emcmotStatus->atspeed_next_feed) {
                issue_atspeed = 1;
                emcmotStatus->atspeed_next_feed = 0;
            }
	    /* append it to the emcmotDebug->coord_tp */
	    tpSetId(&emcmotDebug->coord_tp, emcmotCommand->id);
	    int res_addcircle = tpAddCircle(&emcmotDebug->coord_tp, emcmotCommand->pos,
                            emcmotCommand->center, emcmotCommand->normal,
                            emcmotCommand->turn, emcmotCommand->motion_type,
                            emcmotCommand->vel, emcmotCommand->ini_maxvel,
                            emcmotCommand->acc, emcmotStatus->enables_new, issue_atspeed);
        if (res_addcircle < 0) {
            reportError(_("can't add circular move at line %d, error code %d"),
                    emcmotCommand->id, res_addcircle);
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
        } else if (res_addcircle != 0) {
            //FIXME! This is a band-aid for a single issue, but there may be
            //other consequences of non-fatal errors from AddXXX functions. We
            //either need to fix the root cause (subtle position error after
            //homing), or have a full restore here.
            if (issue_atspeed) {
                emcmotStatus->atspeed_next_feed = 1;
            }
        } else {
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all joints need rehoming, if any
		   joint is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_VEL:
	    /* set the velocity for subsequent moves */
	    /* can do it at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_VEL");
	    emcmotStatus->vel = emcmotCommand->vel;
	    tpSetVmax(&emcmotDebug->coord_tp, emcmotStatus->vel, emcmotCommand->ini_maxvel);
	    break;

	case EMCMOT_SET_VEL_LIMIT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_VEL_LIMIT");
	    emcmot_config_change();
	    /* set the absolute max velocity for all subsequent moves */
	    /* can do it at any time */
	    emcmotConfig->limitVel = emcmotCommand->vel;
	    tpSetVlimit(&emcmotDebug->coord_tp, emcmotConfig->limitVel);
	    break;

	case EMCMOT_SET_JOINT_VEL_LIMIT:
	    /* set joint max velocity */
	    /* can do it at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_VEL_LIMIT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    /* check joint range */
	    if (joint == 0) {
		break;
	    }
	    joint->vel_limit = emcmotCommand->vel;
	    break;

	case EMCMOT_SET_JOINT_ACC_LIMIT:
	    /* set joint max acceleration */
	    /* can do it at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_ACC_LIMIT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    /* check joint range */
	    if (joint == 0) {
		break;
	    }
	    joint->acc_limit = emcmotCommand->acc;
	    break;

	case EMCMOT_SET_ACC:
	    /* set the max acceleration */
	    /* can do it at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_ACCEL");
	    emcmotStatus->acc = emcmotCommand->acc;
	    tpSetAmax(&emcmotDebug->coord_tp, emcmotStatus->acc);
	    break;

	case EMCMOT_PAUSE:
	    /* pause the motion */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "PAUSE");
	    tpPause(&emcmotDebug->coord_tp);
	    emcmotStatus->paused = 1;
	    break;

	case EMCMOT_RESUME:
	    /* resume paused motion */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "RESUME");
	    emcmotDebug->stepping = 0;
	    tpResume(&emcmotDebug->coord_tp);
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_STEP:
	    /* resume paused motion until id changes */
	    /* can happen at any time */
            rtapi_print_msg(RTAPI_MSG_DBG, "STEP");
            if(emcmotStatus->paused) {
                emcmotDebug->idForStep = emcmotStatus->id;
                emcmotDebug->stepping = 1;
                tpResume(&emcmotDebug->coord_tp);
                emcmotStatus->paused = 1;
            } else {
		reportError(_("MOTION: can't STEP while already executing"));
	    }
	    break;

	case EMCMOT_FEED_SCALE:
	    /* override speed */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "FEED SCALE");
	    if (emcmotCommand->scale < 0.0) {
		emcmotCommand->scale = 0.0;	/* clamp it */
	    }
	    emcmotStatus->feed_scale = emcmotCommand->scale;
	    break;

	case EMCMOT_RAPID_SCALE:
	    /* override rapids */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "RAPID SCALE");
	    if (emcmotCommand->scale < 0.0) {
		emcmotCommand->scale = 0.0;	/* clamp it */
	    }
	    emcmotStatus->rapid_scale = emcmotCommand->scale;
	    break;

	case EMCMOT_FS_ENABLE:
	    /* enable/disable overriding speed */
	    /* can happen at any time */
	    if ( emcmotCommand->mode != 0 ) {
		rtapi_print_msg(RTAPI_MSG_DBG, "FEED SCALE: ON");
		emcmotStatus->enables_new |= FS_ENABLED;
            } else {
		rtapi_print_msg(RTAPI_MSG_DBG, "FEED SCALE: OFF");
		emcmotStatus->enables_new &= ~FS_ENABLED;
	    }
	    break;

	case EMCMOT_FH_ENABLE:
	    /* enable/disable feed hold */
	    /* can happen at any time */
	    if ( emcmotCommand->mode != 0 ) {
		rtapi_print_msg(RTAPI_MSG_DBG, "FEED HOLD: ENABLED");
		emcmotStatus->enables_new |= FH_ENABLED;
            } else {
		rtapi_print_msg(RTAPI_MSG_DBG, "FEED HOLD: DISABLED");
		emcmotStatus->enables_new &= ~FH_ENABLED;
	    }
	    break;

	case EMCMOT_SPINDLE_SCALE:
	    /* override spindle speed */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE SCALE");
	    if (emcmotCommand->scale < 0.0) {
		emcmotCommand->scale = 0.0;	/* clamp it */
	    }
	    emcmotStatus->spindle_status[emcmotCommand->spindle].scale = emcmotCommand->scale;
	    break;

	case EMCMOT_SS_ENABLE:
	    /* enable/disable overriding spindle speed */
	    /* can happen at any time */
	    if ( emcmotCommand->mode != 0 ) {
		rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE SCALE: ON");
		emcmotStatus->enables_new |= SS_ENABLED;
            } else {
		rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE SCALE: OFF");
		emcmotStatus->enables_new &= ~SS_ENABLED;
	    }
	    break;

	case EMCMOT_AF_ENABLE:
	    /* enable/disable adaptive feedrate override from HAL pin */
	    /* can happen at any time */
	    if ( emcmotCommand->flags != 0 ) {
		rtapi_print_msg(RTAPI_MSG_DBG, "ADAPTIVE FEED: ON");
		emcmotStatus->enables_new |= AF_ENABLED;
            } else {
		rtapi_print_msg(RTAPI_MSG_DBG, "ADAPTIVE FEED: OFF");
		emcmotStatus->enables_new &= ~AF_ENABLED;
	    }
	    break;

	case EMCMOT_DISABLE:
	    /* go into disable */
	    /* can happen at any time */
	    /* reset the emcmotDebug->enabling flag to defer disable until
	       controller cycle (it *will* be honored) */
	    rtapi_print_msg(RTAPI_MSG_DBG, "DISABLE");
	    emcmotDebug->enabling = 0;
	    if (emcmotConfig->kinType == KINEMATICS_INVERSE_ONLY) {
		emcmotDebug->teleoperating = 0;
		emcmotDebug->coordinating = 0;
	    }
	    break;

	case EMCMOT_ENABLE:
	    /* come out of disable */
	    /* can happen at any time */
	    /* set the emcmotDebug->enabling flag to defer enable until
	       controller cycle */
	    rtapi_print_msg(RTAPI_MSG_DBG, "ENABLE");
	    if ( *(emcmot_hal_data->enable) == 0 ) {
		reportError(_("can't enable motion, enable input is false"));
	    } else {
		emcmotDebug->enabling = 1;
		if (emcmotConfig->kinType == KINEMATICS_INVERSE_ONLY) {
		    emcmotDebug->teleoperating = 0;
		    emcmotDebug->coordinating = 0;
		}
	    }
	    break;

	case EMCMOT_JOINT_ACTIVATE:
	    /* make joint active, so that amps will be enabled when system is
	       enabled or disabled */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOINT_ACTIVATE");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    SET_JOINT_ACTIVE_FLAG(joint, 1);
	    break;

	case EMCMOT_JOINT_DEACTIVATE:
	    /* make joint inactive, so that amps won't be affected when system
	       is enabled or disabled */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOINT_DEACTIVATE");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    SET_JOINT_ACTIVE_FLAG(joint, 0);
	    break;
	case EMCMOT_JOINT_ENABLE_AMPLIFIER:
	    /* enable the amplifier directly, but don't enable calculations */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOINT_ENABLE_AMP");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    break;

	case EMCMOT_JOINT_DISABLE_AMPLIFIER:
	    /* disable the joint calculations and amplifier, but don't disable
	       calculations */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOINT_DISABLE_AMP");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    break;

	case EMCMOT_JOINT_HOME:
	    /* home the specified joint */
	    /* need to be in free mode, enable on */
	    /* this just sets the initial state, then the state machine in
	       homing.c does the rest */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOINT_HOME");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);

	    if (emcmotStatus->motion_state != EMCMOT_MOTION_FREE) {
		/* can't home unless in free mode */
		reportError(_("must be in joint mode to home"));
		return;
	    }
	    if (*(emcmot_hal_data->homing_inhibit)) {
	        reportError(_("Homing denied by motion.homing-inhibit joint=%d\n"),
	                   joint_num);
                return;
	    }

	    if (!GET_MOTION_ENABLE_FLAG()) {
		break;
	    }


	    if(joint_num == -1) { // -1 means home all
                if(emcmotStatus->homingSequenceState == HOME_SEQUENCE_IDLE) {
                    emcmotStatus->homingSequenceState = HOME_SEQUENCE_START;
                } else {
                    reportError(_("homing sequence already in progress"));
                }
	        break;  // do home-all sequence
	    }

	    if (joint == NULL) { break; }
            joint->free_tp.enable = 0; /* abort movement (jog, etc) in progress */

            // ********************************************************
            // support for other homing modes (one sequence, one joint)
            if (joint->home_sequence < 0) {
               int jj;
               emcmot_joint_t *syncjoint;
               emcmotStatus->homingSequenceState = HOME_SEQUENCE_DO_ONE_SEQUENCE;
               for (jj = 0; jj < emcmotConfig->numJoints; jj++) {
                  syncjoint = &joints[jj];
                  if (ABS(syncjoint->home_sequence) == ABS(joint->home_sequence)) {
                      // set home_state for all joints at same neg sequence
                      syncjoint->home_state = HOME_START;
                  }
               }
               break;
            } else {
               emcmotStatus->homingSequenceState = HOME_SEQUENCE_DO_ONE_JOINT;
               joint->home_state = HOME_START; // one joint only
            }
	    break;

	case EMCMOT_JOINT_UNHOME:
            /* unhome the specified joint, or all joints if -1 */
            rtapi_print_msg(RTAPI_MSG_DBG, "JOINT_UNHOME");
            rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
            
            if (   (emcmotStatus->motion_state != EMCMOT_MOTION_FREE)
                && (emcmotStatus->motion_state != EMCMOT_MOTION_DISABLED)) {
                reportError(_("must be in joint mode or disabled to unhome"));
                return;
            }

            if (joint_num < 0) {
                /* we want all or none, so these checks need to all be done first.
                 * but, let's only report the first error.  There might be several,
                 * for instance if a homing sequence is running. */
                for (n = 0; n < emcmotConfig->numJoints; n++) {
                    joint = &joints[n];
                    if(GET_JOINT_ACTIVE_FLAG(joint)) {
                        if (GET_JOINT_HOMING_FLAG(joint)) {
                            reportError(_("Cannot unhome while homing, joint %d"), n);
                            return;
                        }
                        if (!GET_JOINT_INPOS_FLAG(joint)) {
                            reportError(_("Cannot unhome while moving, joint %d"), n);
                            return;
                        }
                    }
                }
                /* we made it through the checks, so unhome them all */
                for (n = 0; n < emcmotConfig->numJoints; n++) {
                    joint = &joints[n];
                    if(GET_JOINT_ACTIVE_FLAG(joint)) {
                        /* if -2, only unhome the volatile_home joints */
                        if(joint_num != -2 || joint->volatile_home) {
                            SET_JOINT_HOMED_FLAG(joint, 0);
                        }
                    }
                }
            } else if (joint_num < emcmotConfig->numJoints) {
                /* request was for only one joint */
                if(GET_JOINT_ACTIVE_FLAG(joint)) {
                    if (GET_JOINT_HOMING_FLAG(joint)) {
                        reportError(_("Cannot unhome while homing, joint %d"), joint_num);
                        return;
                    }
                    if (!GET_JOINT_INPOS_FLAG(joint)) {
                        reportError(_("Cannot unhome while moving, joint %d"), joint_num);
                        return;
                    }
                    SET_JOINT_HOMED_FLAG(joint, 0);
                } else {
                    reportError(_("Cannot unhome inactive joint %d"), joint_num);
                }
            } else {
                /* invalid joint number specified */
                reportError(_("Cannot unhome invalid joint %d (max %d)"), joint_num, (emcmotConfig->numJoints-1));
                return;
            }

            break;

	case EMCMOT_CLEAR_PROBE_FLAGS:
	    rtapi_print_msg(RTAPI_MSG_DBG, "CLEAR_PROBE_FLAGS");
	    emcmotStatus->probing = 0;
            emcmotStatus->probeTripped = 0;
	    break;

	case EMCMOT_PROBE:
	    /* most of this is taken from EMCMOT_SET_LINE */
	    /* emcmotDebug->coord_tp up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    rtapi_print_msg(RTAPI_MSG_DBG, "PROBE");
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError(_("need to be enabled, in coord mode for probe move"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos, emcmotCommand->id, "Probe")) {
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!limits_ok()) {
		reportError(_("can't do probe move with limits exceeded"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!(emcmotCommand->probe_type & 1)) {
                // if suppress errors = off...

                int probeval = !!*(emcmot_hal_data->probe_input);
                int probe_whenclears = !!(emcmotCommand->probe_type & 2);

                if (probeval != probe_whenclears) {
                    // the probe is already in the state we're seeking.
                    if(probe_whenclears) 
                        reportError(_("Probe is already clear when starting G38.4 or G38.5 move"));
                    else
                        reportError(_("Probe is already tripped when starting G38.2 or G38.3 move"));

                    emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
                    tpAbort(&emcmotDebug->coord_tp);
                    SET_MOTION_ERROR_FLAG(1);
                    break;
                }
            }

	    /* append it to the emcmotDebug->coord_tp */
	    tpSetId(&emcmotDebug->coord_tp, emcmotCommand->id);
	    if (-1 == tpAddLine(&emcmotDebug->coord_tp, emcmotCommand->pos, emcmotCommand->motion_type, emcmotCommand->vel, emcmotCommand->ini_maxvel, emcmotCommand->acc, emcmotStatus->enables_new, 0, -1)) {
		reportError(_("can't add probe move"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		emcmotStatus->probing = 1;
                emcmotStatus->probe_type = emcmotCommand->probe_type;
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all joints need rehoming, if any
		   joint is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_RIGID_TAP:
	    /* most of this is taken from EMCMOT_SET_LINE */
	    /* emcmotDebug->coord_tp up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    rtapi_print_msg(RTAPI_MSG_DBG, "RIGID_TAP");
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError(_("need to be enabled, in coord mode for rigid tap move"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos, emcmotCommand->id, "Rigid tap")) {
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!limits_ok()) {
		reportError(_("can't do rigid tap move with limits exceeded"));
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

	    /* append it to the emcmotDebug->coord_tp */
	    tpSetId(&emcmotDebug->coord_tp, emcmotCommand->id);
	    int res_addtap = tpAddRigidTap(&emcmotDebug->coord_tp, emcmotCommand->pos, emcmotCommand->vel, emcmotCommand->ini_maxvel, emcmotCommand->acc, emcmotStatus->enables_new, emcmotCommand->scale);
        if (res_addtap < 0) {
            emcmotStatus->atspeed_next_feed = 0; /* rigid tap always waits for spindle to be at-speed */
            reportError(_("can't add rigid tap move at line %d, error code %d"),
                    emcmotCommand->id, res_addtap);
		tpAbort(&emcmotDebug->coord_tp);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		SET_MOTION_ERROR_FLAG(0);
	    }
	    break;

	case EMCMOT_SET_DEBUG:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_DEBUG");
	    emcmotConfig->debug = emcmotCommand->debug;
	    emcmot_config_change();
	    break;

	/* needed for synchronous I/O */
	case EMCMOT_SET_AOUT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_AOUT");
	    if (emcmotCommand->now) { //we set it right away
		emcmotAioWrite(emcmotCommand->out, emcmotCommand->minLimit);
	    } else { // we put it on the TP queue, warning: only room for one in there, any new ones will overwrite
		tpSetAout(&emcmotDebug->coord_tp, emcmotCommand->out,
		    emcmotCommand->minLimit, emcmotCommand->maxLimit);
	    }
	    break;

	case EMCMOT_SET_DOUT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_DOUT");
	    if (emcmotCommand->now) { //we set it right away
		emcmotDioWrite(emcmotCommand->out, emcmotCommand->start);
	    } else { // we put it on the TP queue, warning: only room for one in there, any new ones will overwrite
		tpSetDout(&emcmotDebug->coord_tp, emcmotCommand->out,
		    emcmotCommand->start, emcmotCommand->end);
	    }
	    break;

	case EMCMOT_SPINDLE_ON:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_ON: spindle %d/%d speed %d\n",
                        emcmotCommand->spindle, emcmotConfig->numSpindles, (int) emcmotCommand->vel);
	    spindle_num = emcmotCommand->spindle;
        if (spindle_num >= emcmotConfig->numSpindles){
            reportError(_("Attempt to start non-existent spindle"));
            emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
            break;
        }
	    if (*(emcmot_hal_data->spindle[spindle_num].spindle_orient))
		rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_ORIENT cancelled by SPINDLE_ON\n");
	    if (*(emcmot_hal_data->spindle[spindle_num].spindle_locked))
		rtapi_print_msg(RTAPI_MSG_DBG, "spindle-locked cleared by SPINDLE_ON\n");
	    *(emcmot_hal_data->spindle[spindle_num].spindle_locked) = 0;
	    *(emcmot_hal_data->spindle[spindle_num].spindle_orient) = 0;
	    emcmotStatus->spindle_status[spindle_num].orient_state = EMCMOT_ORIENT_NONE;

	    /* if (emcmotStatus->spindle.orient) { */
	    /* 	reportError(_("cant turn on spindle during orient in progress")); */
	    /* 	emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND; */
	    /* 	tpAbort(&emcmotDebug->tp); */
	    /* 	SET_MOTION_ERROR_FLAG(1); */
	    /* } else {...} */
	    emcmotStatus->spindle_status[spindle_num].speed = emcmotCommand->vel;
	    emcmotStatus->spindle_status[spindle_num].css_factor = emcmotCommand->ini_maxvel;
	    emcmotStatus->spindle_status[spindle_num].xoffset = emcmotCommand->acc;
	    if (emcmotCommand->vel >= 0) {
		emcmotStatus->spindle_status[spindle_num].direction = 1;
	    } else {
		emcmotStatus->spindle_status[spindle_num].direction = -1;
	    }
	    emcmotStatus->spindle_status[spindle_num].brake = 0; //disengage brake
            emcmotStatus->atspeed_next_feed = emcmotCommand->wait_for_spindle_at_speed;

           // check wether it's passed correctly
           if (!emcmotStatus->atspeed_next_feed)
               rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_ON without wait-for-atspeed");
	    break;

	case EMCMOT_SPINDLE_OFF:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_OFF");
	    spindle_num = emcmotCommand->spindle;
        if (spindle_num >= emcmotConfig->numSpindles){
            reportError(_("Attempt to stop non-existent spindle"));
            emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
            break;
        }
	    emcmotStatus->spindle_status[spindle_num].speed = 0;
	    emcmotStatus->spindle_status[spindle_num].direction = 0;
	    emcmotStatus->spindle_status[spindle_num].brake = 1; // engage brake
	    if (*(emcmot_hal_data->spindle[spindle_num].spindle_orient))
		rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_ORIENT cancelled by SPINDLE_OFF");
	    if (*(emcmot_hal_data->spindle[spindle_num].spindle_locked))
		rtapi_print_msg(RTAPI_MSG_DBG, "spindle-locked cleared by SPINDLE_OFF");
	    *(emcmot_hal_data->spindle[spindle_num].spindle_locked) = 0;
	    *(emcmot_hal_data->spindle[spindle_num].spindle_orient) = 0;
	    emcmotStatus->spindle_status[spindle_num].orient_state = EMCMOT_ORIENT_NONE;
	    break;

	case EMCMOT_SPINDLE_ORIENT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_ORIENT");
	    spindle_num = emcmotCommand->spindle;
        if (spindle_num >= emcmotConfig->numSpindles){
            reportError(_("Attempt to orient non-existent spindle"));
            emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
            break;
        }
	    if (spindle_num > emcmotConfig->numSpindles){
            rtapi_print_msg(RTAPI_MSG_ERR, "spindle number too high in M19");
            break;
	    }
	    if (*(emcmot_hal_data->spindle[spindle_num].spindle_orient)) {
		rtapi_print_msg(RTAPI_MSG_DBG, "orient already in progress");

		// mah:FIXME unsure wether this is ok or an error
		/* reportError(_("orient already in progress")); */
		/* emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND; */
		/* tpAbort(&emcmotDebug->tp); */
		/* SET_MOTION_ERROR_FLAG(1); */
	    }
	    emcmotStatus->spindle_status[spindle_num].orient_state = EMCMOT_ORIENT_IN_PROGRESS;
	    emcmotStatus->spindle_status[spindle_num].speed = 0;
	    emcmotStatus->spindle_status[spindle_num].direction = 0;
	    // so far like spindle stop, except opening brake
	    emcmotStatus->spindle_status[spindle_num].brake = 0; // open brake

	    *(emcmot_hal_data->spindle[spindle_num].spindle_orient_angle) = emcmotCommand->orientation;
	    *(emcmot_hal_data->spindle[spindle_num].spindle_orient_mode) = emcmotCommand->mode;
	    *(emcmot_hal_data->spindle[spindle_num].spindle_locked) = 0;
	    *(emcmot_hal_data->spindle[spindle_num].spindle_orient) = 1;

	    // mirror in spindle status
	    emcmotStatus->spindle_status[spindle_num].orient_fault = 0; // this pin read during spindle-orient == 1
	    emcmotStatus->spindle_status[spindle_num].locked = 0;
	    break;

	case EMCMOT_SPINDLE_INCREASE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_INCREASE");
	    spindle_num = emcmotCommand->spindle;
        if (spindle_num >= emcmotConfig->numSpindles){
            reportError(_("Attempt to increase non-existent spindle"));
            emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
            break;
        }
	    if (emcmotStatus->spindle_status[spindle_num].speed > 0) {
		emcmotStatus->spindle_status[spindle_num].speed += 100; //FIXME - make the step a HAL parameter
	    } else if (emcmotStatus->spindle_status[spindle_num].speed < 0) {
		emcmotStatus->spindle_status[spindle_num].speed -= 100;
	    }
	    break;

	case EMCMOT_SPINDLE_DECREASE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_DECREASE");
	    spindle_num = emcmotCommand->spindle;
        if (spindle_num >= emcmotConfig->numSpindles){
            reportError(_("Attempt to decreasenon-existent spindle"));
            emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
            break;
        }
	    if (emcmotStatus->spindle_status[spindle_num].speed > 100) {
		emcmotStatus->spindle_status[spindle_num].speed -= 100; //FIXME - make the step a HAL parameter
	    } else if (emcmotStatus->spindle_status[spindle_num].speed < -100) {
		emcmotStatus->spindle_status[spindle_num].speed += 100;
	    }
	    break;

	case EMCMOT_SPINDLE_BRAKE_ENGAGE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_BRAKE_ENGAGE");
	    spindle_num = emcmotCommand->spindle;
        if (spindle_num >= emcmotConfig->numSpindles){
            reportError(_("Attempt to engage brake of non-existent spindle"));
            emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
            break;
        }
	    emcmotStatus->spindle_status[spindle_num].speed = 0;
	    emcmotStatus->spindle_status[spindle_num].direction = 0;
	    emcmotStatus->spindle_status[spindle_num].brake = 1;
	    break;

	case EMCMOT_SPINDLE_BRAKE_RELEASE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SPINDLE_BRAKE_RELEASE");
	    spindle_num = emcmotCommand->spindle;
        if (spindle_num >= emcmotConfig->numSpindles){
            reportError(_("Attempt to release brake of non-existent spindle"));
            emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
            break;
        }
	    emcmotStatus->spindle_status[spindle_num].brake = 0;
	    break;

	case EMCMOT_SET_JOINT_COMP:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_COMP for joint %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    if (joint->comp.entries >= EMCMOT_COMP_SIZE) {
		reportError(_("joint %d: too many compensation entries"), joint_num);
		break;
	    }
	    /* point to last entry */
	    comp_entry = &(joint->comp.array[joint->comp.entries]);
	    if (emcmotCommand->comp_nominal <= comp_entry[0].nominal) {
		reportError(_("joint %d: compensation values must increase"), joint_num);
		break;
	    }
	    /* store data to new entry */
	    comp_entry[1].nominal = emcmotCommand->comp_nominal;
	    comp_entry[1].fwd_trim = emcmotCommand->comp_forward;
	    comp_entry[1].rev_trim = emcmotCommand->comp_reverse;
	    /* calculate slopes from previous entry to the new one */
	    if ( comp_entry[0].nominal != -DBL_MAX ) {
		/* but only if the previous entry is "real" */
		tmp1 = comp_entry[1].nominal - comp_entry[0].nominal;
		comp_entry[0].fwd_slope =
		    (comp_entry[1].fwd_trim - comp_entry[0].fwd_trim) / tmp1;
		comp_entry[0].rev_slope =
		    (comp_entry[1].rev_trim - comp_entry[0].rev_trim) / tmp1;
	    } else {
		/* previous entry is at minus infinity, slopes are zero */
		comp_entry[0].fwd_trim = comp_entry[1].fwd_trim;
		comp_entry[0].rev_trim = comp_entry[1].rev_trim;
	    }
	    joint->comp.entries++;
	    break;

        case EMCMOT_SET_OFFSET:
            emcmotStatus->tool_offset = emcmotCommand->tool_offset;
            break;

	case EMCMOT_SET_AXIS_POSITION_LIMITS:
	    /* set the position limits for axis */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_AXIS_POSITION_LIMITS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", axis_num);
	    emcmot_config_change();
	    if (axis == 0) {
		break;
	    }
	    axis->min_pos_limit = emcmotCommand->minLimit;
	    axis->max_pos_limit = emcmotCommand->maxLimit;
	    break;

        case EMCMOT_SET_AXIS_VEL_LIMIT:
	    /* set the max axis vel */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_AXIS_VEL_LIMITS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", axis_num);
	    emcmot_config_change();
	    if (axis == 0) {
		break;
	    }
	    axis->vel_limit = emcmotCommand->vel;
	    axis->ext_offset_vel_limit = emcmotCommand->ext_offset_vel;
            break;

        case EMCMOT_SET_AXIS_ACC_LIMIT:
 	    /* set the max axis acc */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_AXIS_ACC_LIMITS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", axis_num);
	    emcmot_config_change();
	    if (axis == 0) {
		break;
	    }
	    axis->acc_limit = emcmotCommand->acc;
	    axis->ext_offset_acc_limit = emcmotCommand->ext_offset_acc;
            break;

        case EMCMOT_SET_AXIS_LOCKING_JOINT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_AXIS_ACC_LOCKING_JOINT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", axis_num);
	    emcmot_config_change();
	    if (axis == 0) {
		break;
	    }
	    axis->locking_joint = joint_num;
            break;

	default:
	    rtapi_print_msg(RTAPI_MSG_DBG, "UNKNOWN");
	    reportError(_("unrecognized command %d"), emcmotCommand->command);
	    emcmotStatus->commandStatus = EMCMOT_COMMAND_UNKNOWN_COMMAND;
	    break;
        case EMCMOT_SET_MAX_FEED_OVERRIDE:
            emcmotConfig->maxFeedScale = emcmotCommand->maxFeedScale;
            break;
        case EMCMOT_SETUP_ARC_BLENDS:
            emcmotConfig->arcBlendEnable = emcmotCommand->arcBlendEnable;
            emcmotConfig->arcBlendFallbackEnable = emcmotCommand->arcBlendFallbackEnable;
            emcmotConfig->arcBlendOptDepth = emcmotCommand->arcBlendOptDepth;
            emcmotConfig->arcBlendGapCycles = emcmotCommand->arcBlendGapCycles;
            emcmotConfig->arcBlendRampFreq = emcmotCommand->arcBlendRampFreq;
            emcmotConfig->arcBlendTangentKinkRatio = emcmotCommand->arcBlendTangentKinkRatio;
            break;
        case EMCMOT_SET_PROBE_ERR_INHIBIT:
            emcmotConfig->inhibit_probe_jog_error = emcmotCommand->probe_jog_err_inhibit;
            emcmotConfig->inhibit_probe_home_error = emcmotCommand->probe_home_err_inhibit;
            break;

	}			/* end of: command switch */
	if (emcmotStatus->commandStatus != EMCMOT_COMMAND_OK) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "ERROR: %d",
		emcmotStatus->commandStatus);
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "\n");
	/* synch tail count */
	emcmotStatus->tail = emcmotStatus->head;
	emcmotConfig->tail = emcmotConfig->head;
	emcmotDebug->tail = emcmotDebug->head;

    }
    /* end of: if-new-command */

    return;
}
