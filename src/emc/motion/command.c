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
*   emcmotCommmandHandler() - This would provide a small performance
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
*
* Last change:
* $Revision$
* $Author$
* $Date$
*
********************************************************************/

#include <linux/types.h>
#include <float.h>
#include "posemath.h"
#include "rtapi.h"
#include "hal.h"
#include "motion.h"
#include "emcmotglb.h"
#include "mot_priv.h"


/* debugging functions */
extern void print_pose ( EmcPose *pos );
extern void check_stuff(char *msg);


/* value for world home position */
EmcPose worldHome = { {0.0, 0.0, 0.0}
, 0.0, 0.0, 0.0
};

int logSkip = 0;		/* how many to skip, for per-cycle logging */
int loggingAxis = 0;		/* record of which axis to log */
int logStartTime;		/* set when logging is started, and
				   subtracted off each log time for better
				   resolution */
/* kinematics flags */
KINEMATICS_FORWARD_FLAGS fflags = 0;
KINEMATICS_INVERSE_FLAGS iflags = 0;

/* loops through the active joints and checks if any are not homed */
static int checkAllHomed(void) {
    int joint_num;
    emcmot_joint_t *joint;

    /* bail out if the allHomed flag is already set */
    if (0 != emcmotDebug) {
	if (emcmotDebug->allHomed) return 1;
    }

    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
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
    /* set the global flag that all axes are homed */
    if (0 != emcmotDebug) {
	emcmotDebug->allHomed = 1;
    }
    /* return true if all are actives are homed*/
    return 1;
}


/* checkLimits() returns 1 if none of the soft or hard limits are
   set, 0 if any are set. Called on a linear and circular move. */
static int checkLimits(void)
{
    int joint_num;
    emcmot_joint_t *joint;

    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	joint = &joints[joint_num];
	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, don't even look at its limits */
	    continue;
	}

	if (GET_JOINT_PSL_FLAG(joint) || GET_JOINT_NSL_FLAG(joint)
	    || GET_JOINT_PHL_FLAG(joint) || GET_JOINT_NHL_FLAG(joint)) {
	    return 0;
	}
    }

    return 1;
}

/* check the value of the axis and velocity against current position,
   returning 1 (okay) if the request is to jog off the limit, 0 (bad)
   if the request is to jog further past a limit. Software limits are
   ignored if the joint hasn't been homed */
static int checkJog(int joint_num, double vel)
{
    emcmot_joint_t *joint;

    /* point to joint data */
    joint = &joints[joint_num];

    if (emcmotStatus->overrideLimits) {
	return 1;		/* okay to jog when limits overridden */
    }

    if (joint_num < 0 || joint_num >= EMCMOT_MAX_AXIS) {
	reportError("Can't jog out of range axis %d.", joint_num);
	return 0;
    }

    if (vel > 0.0 && GET_JOINT_PSL_FLAG(joint)) {
	reportError("Can't jog axis %d further past max soft limit.",
	    joint_num);
	return 0;
    }

    if (vel > 0.0 && GET_JOINT_PHL_FLAG(joint)) {
	reportError("Can't jog axis %d further past max hard limit.",
	    joint_num);
	return 0;
    }

    if (vel < 0.0 && GET_JOINT_NSL_FLAG(joint)) {
	reportError("Can't jog axis %d further past min soft limit.",
	    joint_num);
	return 0;
    }

    if (vel < 0.0 && GET_JOINT_NHL_FLAG(joint)) {
	reportError("Can't jog axis %d further past min hard limit.",
	    joint_num);
	return 0;
    }

    /* okay to jog */
    return 1;
}

/* inRange() returns non-zero if the position lies within the joint
   limits, or 0 if not */
static int inRange(EmcPose pos)
{
    double joint_pos[EMCMOT_MAX_AXIS];
    int joint_num;
    emcmot_joint_t *joint;

    /* fill in all joints with 0 */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	joint_pos[joint_num] = 0.0;
    }

    /* now fill in with real values, for joints that are used */
    kinematicsInverse(&pos, joint_pos, &iflags, &fflags);

    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to joint data */
	joint = &joints[joint_num];

	if (!GET_JOINT_ACTIVE_FLAG(joint)) {
	    /* if joint is not active, don't even look at its limits */
	    continue;
	}
	if ((joint_pos[joint_num] > joint->max_pos_limit) ||
	    (joint_pos[joint_num] < joint->min_pos_limit)) {
	    return 0;		/* can't move further past limit */
	}
    }

    /* okay to move */
    return 1;
}

/* clearHomes() will clear the homed flags for joints that have moved
   since homing, outside coordinated control, for machines with no
   forward kinematics. This is used in conjunction with the rehomeAll
   flag, which is set for any coordinated move that in general will
   result in all joints moving. The flag is consulted whenever a joint
   is jogged in joint mode, so that either its flag can be cleared if
   no other joints have moved, or all have to be cleared. */
static void clearHomes(int joint_num)
{
    int n;
    emcmot_joint_t *joint;

    if (kinType == KINEMATICS_INVERSE_ONLY) {
	if (rehomeAll) {
	    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
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
    if (0 != emcmotDebug) {
	emcmotDebug->allHomed = 0;
    }
}

/*
  emcmotCommandHandler() is called each main cycle to read the
  shared memory buffer
  */
void emcmotCommandHandler(void *arg, long period)
{
    int joint_num;
    int valid;
    double tmp;
    emcmot_joint_t *joint;

check_stuff ( "before command_handler()" );

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

	/* log it, if appropriate */
	if (emcmotStatus->logStarted &&
	    emcmotStatus->logType == EMCMOT_LOG_TYPE_CMD) {
	    ls.item.cmd.time = etime();	/* don't subtract off logStartTime,
					   since we want an absolute time
					   value */
	    ls.item.cmd.command = emcmotCommand->command;
	    ls.item.cmd.commandNum = emcmotCommand->commandNum;
	    emcmotLogAdd(emcmotLog, ls);
	    emcmotStatus->logPoints = emcmotLog->howmany;
	}

	/* ...and process command */

	/* Many commands uses "command->axis" to indicate which joint they
	   wish to operate on.  This code eliminates the need to copy
	   command->axis to "joint_num", limit check it, and then set "joint"
	   to point to the joint data.  All the individual commands need to do
	   is verify that "joint" is non-zero. */
	joint_num = emcmotCommand->axis;
	if (joint_num >= 0 && joint_num < EMCMOT_MAX_AXIS) {
	    /* valid joint, point to it's data */
	    joint = &joints[joint_num];
	} else {
	    /* bad joint number */
	    joint = 0;
	}

/* printing of commands for troubleshooting */
	rtapi_print_msg(RTAPI_MSG_DBG, "%d: CMD %d, code %3d ", emcmotStatus->heartbeat,
	    emcmotCommand->commandNum, emcmotCommand->command);

	switch (emcmotCommand->command) {
	case EMCMOT_ABORT:
	    /* abort motion */
	    /* can happen at any time */
	    /* this command attempts to stop all machine motion. it looks at
	       the current mode and acts accordingly, if in teleop mode, it
	       sets the desired velocities to zero, if in coordinated mode,
	       it calls the traj planner abort function (don't know what that
	       does yet), and if in free mode, it disables the free mode traj
	       planners which stops axis motion */
	    rtapi_print_msg(RTAPI_MSG_DBG, "ABORT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", emcmotCommand->axis);
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
		for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		    /* point to joint struct */
		    joint = &joints[joint_num];
		    /* tell joint planner to stop */
		    joint->free_tp_enable = 0;
		    /* stop homing if in progress */
		    if ( joint->home_state != HOME_IDLE ) {
			joint->home_state = HOME_ABORT;
		    }
		    /* update status flags */
		    SET_JOINT_ERROR_FLAG(joint, 0);
		}
	    }
	    break;

	case EMCMOT_AXIS_ABORT:
	    /* abort one axis */
	    /* can happen at any time */
	    /* this command stops a single axis.  It is only usefull
	       in free mode, so in coord or teleop mode it does
	       nothing. */
	    rtapi_print_msg(RTAPI_MSG_DBG, "AXIS_ABORT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", emcmotCommand->axis);
	    if (GET_MOTION_TELEOP_FLAG()) {
		/* do nothing in teleop mode */
	    } else if (GET_MOTION_COORD_FLAG()) {
		/* do nothing in coord mode */
	    } else {
		/* validate joint */
		if (joint == 0) {
		    break;
		}
		/* tell joint planner to stop */
		joint->free_tp_enable = 0;
		/* stop homing if in progress */
		if ( joint->home_state != HOME_IDLE ) {
		    joint->home_state = HOME_ABORT;
		}
		/* update status flags */
		SET_JOINT_ERROR_FLAG(joint, 0);
	    }
	    break;

	case EMCMOT_FREE:
	    /* change the mode to free axis motion */
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
	    if (kinType != KINEMATICS_IDENTITY) {
		if (!checkAllHomed()) {
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
	    /* this code doesn't actually make the transition, it merely
	       tests a condition and then sets a flag requesting the
	       transition */
	    /* set the emcmotDebug->teleoperating flag to defer transition to
	       controller cycle */
	    rtapi_print_msg(RTAPI_MSG_DBG, "TELEOP");
	    emcmotDebug->teleoperating = 1;
	    if (kinType != KINEMATICS_IDENTITY) {
		
		if (!checkAllHomed()) {
		    reportError
			("all axes must be homed before going into teleop mode");
		    emcmotDebug->teleoperating = 0;
		    break;
		}

	    }
	    break;

	case EMCMOT_SET_NUM_AXES:
	    /* set the global NUM_AXES, which must be between 1 and
	       EMCMOT_MAX_AXIS, inclusive */
	    /* this sets a global - I hate globals - hopefully this can be
	       moved into the config structure, or dispensed with completely */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_NUM_AXES");
	    joint_num = emcmotCommand->axis;
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    /* note that this comparison differs from the check on the range
	       of 'joint_num' in most other places, since those checks are
	       for a value to be used as an index and here it's a value to be 
	       used as a counting number. The indenting is different here so
	       as not to match macro editing on that other bunch. */
	    if (joint_num <= 0 || joint_num > EMCMOT_MAX_AXIS) {
		break;
	    }
	    num_axes = joint_num;
	    emcmotConfig->numAxes = joint_num;
	    break;

	case EMCMOT_SET_WORLD_HOME:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_WORLD_HOME");
	    emcmotStatus->world_home = emcmotCommand->pos;
	    break;

	case EMCMOT_SET_HOMING_PARAMS:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_HOMING_PARAMS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0) {
		break;
	    }
	    joint->home_offset = emcmotCommand->offset;
	    joint->home = emcmotCommand->home;
	    joint->home_search_vel = emcmotCommand->search_vel;
	    joint->home_latch_vel = emcmotCommand->latch_vel;
	    joint->home_flags = emcmotCommand->flags;
	    break;

	case EMCMOT_OVERRIDE_LIMITS:
	    /* this command can be issued with axix < 0 to re-enable
	       limits, but they are automatically re-enabled at the
	       end of the next jog */
	    rtapi_print_msg(RTAPI_MSG_DBG, "OVERRIDE_LIMITS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", emcmotCommand->axis);
	    if (emcmotCommand->axis < 0) {
		/* don't override limits */
		rtapi_print_msg(RTAPI_MSG_DBG, "override off");
		emcmotStatus->overrideLimits = 0;
	    } else {
		rtapi_print_msg(RTAPI_MSG_DBG, "override on");
		emcmotStatus->overrideLimits = 1;
	    }
	    emcmotDebug->overriding = 0;
	    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
		/* point at joint data */
		joint = &joints[joint_num];
		/* clear joint errors */
		SET_JOINT_ERROR_FLAG(joint, 0);
	    }
	    break;

	case EMCMOT_SET_POSITION_LIMITS:
	    /* sets soft limits for an axis */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_POSITION_LIMITS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    /* set the position limits for the axis */
	    /* can be done at any time */
	    if (joint == 0) {
		break;
	    }
	    joint->min_pos_limit = emcmotCommand->minLimit;
	    joint->max_pos_limit = emcmotCommand->maxLimit;
	    break;

	case EMCMOT_SET_BACKLASH:
	    /* sets backlash for an axis */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_BACKLASH");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    /* set the backlash for the axis */
	    /* can be done at any time */
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
	case EMCMOT_SET_MAX_FERROR:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_MAX_FERROR");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0 || emcmotCommand->maxFerror < 0.0) {
		break;
	    }
	    joint->max_ferror = emcmotCommand->maxFerror;
	    break;

	case EMCMOT_SET_MIN_FERROR:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_MIN_FERROR");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    if (joint == 0 || emcmotCommand->minFerror < 0.0) {
		break;
	    }
	    joint->min_ferror = emcmotCommand->minFerror;
	    break;

	case EMCMOT_JOG_CONT:
	    /* do a continuous jog, implemented as an incremental jog to the
	       software limit, or the full range of travel if software limits
	       don't yet apply because we're not homed */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOG_CONT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    /* check axis range */
	    if (joint == 0) {
		break;
	    }

	    /* must be in free mode and enabled */
	    if (GET_MOTION_COORD_FLAG()) {
		reportError("Can't jog axis in coordinated mode.");
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError("Can't jog axis when not enabled.");
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(joint_num, emcmotCommand->vel)) {
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    /* set destination of jog */
	    if (GET_JOINT_HOMED_FLAG(joint)) {
		/* axis is homed, we can use soft limits */
		if (emcmotCommand->vel > 0.0) {
		    joint->free_pos_cmd = joint->max_pos_limit;
		} else {
		    joint->free_pos_cmd = joint->min_pos_limit;
		}
	    } else {
		/* axis not homed, use current position + range */
		tmp = joint->max_pos_limit - joint->min_pos_limit;
		if (emcmotCommand->vel > 0.0) {
		    joint->free_pos_cmd = joint->pos_cmd + tmp;
		} else {
		    joint->free_pos_cmd = joint->pos_cmd - tmp;
		}
	    }
	    /* set velocity of jog */
	    joint->free_vel_lim = fabs(emcmotCommand->vel);
	    /* and let it go */
	    joint->free_tp_enable = 1;
	    /*! \todo FIXME - should we really be clearing errors here? */
	    SET_JOINT_ERROR_FLAG(joint, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(joint_num);
	    break;

	case EMCMOT_JOG_INCR:
	    /* do an incremental jog */

	    /* check axis range */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOG_INCR");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }

	    /* must be in free mode and enabled */
	    if (GET_MOTION_COORD_FLAG()) {
		reportError("Can't jog axis in coordinated mode.");
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError("Can't jog axis when not enabled.");
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(joint_num, emcmotCommand->vel)) {
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    /* set target position for jog */
	    if (emcmotCommand->vel > 0.0) {
		joint->free_pos_cmd += emcmotCommand->offset;
	    } else {
		joint->free_pos_cmd -= emcmotCommand->offset;
	    }
	    /* don't jog past soft limits, if homed */
	    if (GET_JOINT_HOMED_FLAG(joint)) {
		if (joint->free_pos_cmd > joint->max_pos_limit) {
		    joint->free_pos_cmd = joint->max_pos_limit;
		}
		if (joint->free_pos_cmd < joint->min_pos_limit) {
		    joint->free_pos_cmd = joint->min_pos_limit;
		}
	    }
	    /* set velocity of jog */
	    joint->free_vel_lim = fabs(emcmotCommand->vel);
	    /* and let it go */
	    joint->free_tp_enable = 1;
	    SET_JOINT_ERROR_FLAG(joint, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(joint_num);
	    break;

	case EMCMOT_JOG_ABS:
	    /* do an absolute jog */

	    /* check axis range */
	    rtapi_print_msg(RTAPI_MSG_DBG, "JOG_ABS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    /* must be in free mode and enabled */
	    if (GET_MOTION_COORD_FLAG()) {
		reportError("Can't jog axis in coordinated mode.");
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError("Can't jog axis when not enabled.");
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(joint_num, emcmotCommand->vel)) {
		SET_JOINT_ERROR_FLAG(joint, 1);
		break;
	    }
	    /*! \todo FIXME-- use 'goal' instead */
	    joint->free_pos_cmd = emcmotCommand->offset;
	    /* don't jog past soft limits, if homed */
	    if (GET_JOINT_HOMED_FLAG(joint)) {
		if (joint->free_pos_cmd > joint->max_pos_limit) {
		    joint->free_pos_cmd = joint->max_pos_limit;
		}
		if (joint->free_pos_cmd < joint->min_pos_limit) {
		    joint->free_pos_cmd = joint->min_pos_limit;
		}
	    }
	    /* set velocity of jog */
	    joint->free_vel_lim = fabs(emcmotCommand->vel);
	    /* and let it go */
	    joint->free_tp_enable = 1;
	    SET_JOINT_ERROR_FLAG(joint, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(joint_num);
	    break;

	case EMCMOT_SET_TERM_COND:
	    /* sets termination condition for motion emcmotDebug->queue */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_TERM_COND");
	    tpSetTermCond(&emcmotDebug->queue, emcmotCommand->termCond);
	    break;

	case EMCMOT_SET_LINE:
	    /* emcmotDebug->queue up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_LINE");
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for linear move");
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
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_CIRCLE:
	    /* emcmotDebug->queue up a circular move */
	    /* requires coordinated mode, enable on, not on limits */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_CIRCLE");
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for circular move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos)) {
		reportError("circular move %d out of range",
		    emcmotCommand->id);
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
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_VEL:
	    /* set the velocity for subsequent moves */
	    /* can do it at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_VEL");
	    emcmotStatus->vel = emcmotCommand->vel;
	    tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel);
	    break;

	case EMCMOT_SET_VEL_LIMIT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_VEL_LIMIT");
	    emcmot_config_change();
	    /* set the absolute max velocity for all subsequent moves */
	    /* can do it at any time */
	    emcmotConfig->limitVel = emcmotCommand->vel;
	    tpSetVlimit(&emcmotDebug->queue, emcmotConfig->limitVel);
	    break;

	case EMCMOT_SET_JOINT_VEL_LIMIT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_VEL_LIMIT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    /* check axis range */
	    if (joint == 0) {
		break;
	    }
	    joint->vel_limit = emcmotCommand->vel;
	    joint->big_vel = 10 * emcmotCommand->vel;
	    break;

	case EMCMOT_SET_JOINT_ACC_LIMIT:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_JOINT_ACC_LIMIT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    emcmot_config_change();
	    /* check axis range */
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
	    tpSetAmax(&emcmotDebug->queue, emcmotStatus->acc);
	    break;

	case EMCMOT_PAUSE:
	    /* pause the motion */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "PAUSE");
	    tpPause(&emcmotDebug->queue);
	    emcmotStatus->paused = 1;
	    break;

	case EMCMOT_RESUME:
	    /* resume paused motion */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "RESUME");
	    emcmotDebug->stepping = 0;
	    tpResume(&emcmotDebug->queue);
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_STEP:
	    /* resume paused motion until id changes */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "STEP");
	    emcmotDebug->idForStep = emcmotStatus->id;
	    emcmotDebug->stepping = 1;
	    tpResume(&emcmotDebug->queue);
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_SCALE:
	    /* override speed */
	    /* can happen at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "SCALE");
	    if (emcmotCommand->scale < 0.0) {
		emcmotCommand->scale = 0.0;	/* clamp it */
	    }
	    emcmotStatus->qVscale = emcmotCommand->scale;
	    tpSetVscale(&emcmotDebug->queue, emcmotCommand->scale);
	    break;

	case EMCMOT_DISABLE:
	    /* go into disable */
	    /* can happen at any time */
	    /* reset the emcmotDebug->enabling flag to defer disable until
	       controller cycle (it *will* be honored) */
	    rtapi_print_msg(RTAPI_MSG_DBG, "DISABLE");
	    emcmotDebug->enabling = 0;
	    if (kinType == KINEMATICS_INVERSE_ONLY) {
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
	    emcmotDebug->enabling = 1;
	    if (kinType == KINEMATICS_INVERSE_ONLY) {
		emcmotDebug->teleoperating = 0;
		emcmotDebug->coordinating = 0;
	    }
	    break;

	case EMCMOT_ACTIVATE_JOINT:
	    /* make axis active, so that amps will be enabled when system is
	       enabled or disabled */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "ACTIVATE_JOINT");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    SET_JOINT_ACTIVE_FLAG(joint, 1);
	    break;

	case EMCMOT_DEACTIVATE_JOINT:
	    /* make axis inactive, so that amps won't be affected when system
	       is enabled or disabled */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "DEACTIVATE_AXIS");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    SET_JOINT_ACTIVE_FLAG(joint, 0);
	    break;
/*! \todo FIXME - need to replace the ext function */
	case EMCMOT_ENABLE_AMPLIFIER:
	    /* enable the amplifier directly, but don't enable calculations */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "ENABLE_AMP");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
/*! \todo Another #if 0 */
#if 0
	    extAmpEnable(axis, 1);
#endif
	    break;

	case EMCMOT_DISABLE_AMPLIFIER:
	    /* disable the axis calculations and amplifier, but don't disable
	       calculations */
	    /* can be done at any time */
	    rtapi_print_msg(RTAPI_MSG_DBG, "DISABLE_AMP");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
/*! \todo Another #if 0 */
#if 0
	    extAmpEnable(axis, 0);
#endif
	    break;
	case EMCMOT_OPEN_LOG:
	    /* open a data log */
	    rtapi_print_msg(RTAPI_MSG_DBG, "OPEN_LOG");
	    valid = 0;
	    if (emcmotCommand->logSize > 0 &&
		emcmotCommand->logSize <= EMCMOT_LOG_MAX) {
		/* handle log-specific data */
		switch (emcmotCommand->logType) {
		case EMCMOT_LOG_TYPE_AXIS_POS:
		case EMCMOT_LOG_TYPE_AXIS_VEL:
		case EMCMOT_LOG_TYPE_POS_VOLTAGE:
		    if (joint != 0) {
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
		loggingAxis = joint_num;
		emcmotLogInit(emcmotLog,
		    emcmotCommand->logType, emcmotCommand->logSize);
		emcmotStatus->logOpen = 1;
		emcmotStatus->logStarted = 0;
		emcmotStatus->logSize = emcmotCommand->logSize;
		emcmotStatus->logSkip = emcmotCommand->logSkip;
		emcmotStatus->logType = emcmotCommand->logType;
		emcmotStatus->logTriggerType = emcmotCommand->logTriggerType;
		emcmotStatus->logTriggerVariable =
		    emcmotCommand->logTriggerVariable;
		emcmotStatus->logTriggerThreshold =
		    emcmotCommand->logTriggerThreshold;
		if (joint != 0
		    && emcmotStatus->logTriggerType == EMCLOG_DELTA_TRIGGER) {
		    switch (emcmotStatus->logTriggerVariable) {
		    case EMCLOG_TRIGGER_ON_FERROR:
			emcmotStatus->logStartVal = joint->ferror;
			break;
/*! \todo Another #if 0 */
#if 0
		    case EMCLOG_TRIGGER_ON_VOLT:
			emcmotStatus->logStartVal =
			    emcmotDebug->rawOutput[loggingAxis];
			break;
#endif
		    case EMCLOG_TRIGGER_ON_POS:
			emcmotStatus->logStartVal = joint->pos_cmd;
			break;
		    case EMCLOG_TRIGGER_ON_VEL:
			emcmotStatus->logStartVal = joint->vel_cmd;
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
	    rtapi_print_msg(RTAPI_MSG_DBG, "START_LOG");
	    if (emcmotStatus->logType == EMCMOT_LOG_TYPE_POS_VOLTAGE) {
		break;
	    }
	    /* set the global baseTime, to be subtracted off log times,
	       otherwise time values are too large for the small increments
	       to appear */
	    if (emcmotStatus->logOpen &&
		emcmotStatus->logTriggerType == EMCLOG_MANUAL_TRIGGER) {
		logStartTime = etime();
		emcmotStatus->logStarted = 1;
		logSkip = 0;
	    }
	    break;

	case EMCMOT_STOP_LOG:
	    /* stop logging */
	    rtapi_print_msg(RTAPI_MSG_DBG, "STOP_LOG");
	    emcmotStatus->logStarted = 0;
	    break;

	case EMCMOT_CLOSE_LOG:
	    rtapi_print_msg(RTAPI_MSG_DBG, "CLOSE_LOG");
	    emcmotStatus->logOpen = 0;
	    emcmotStatus->logStarted = 0;
	    emcmotStatus->logSize = 0;
	    emcmotStatus->logSkip = 0;
	    emcmotStatus->logType = 0;
	    break;

	case EMCMOT_HOME:
	    /* home the specified axis */
	    /* need to be in free mode, enable on */
	    /* this just sets the initial state, then the state machine in
	       control.c does the rest */
	    rtapi_print_msg(RTAPI_MSG_DBG, "HOME");
	    rtapi_print_msg(RTAPI_MSG_DBG, " %d", joint_num);
	    if (joint == 0) {
		break;
	    }
	    if (GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		break;
	    }
	    /* abort any movememt (jog, etc) that is in progress */
	    joint->free_tp_enable = 0;
	    /* prime the homing state machine */
	    joint->home_state = HOME_START;
	    break;

	case EMCMOT_ENABLE_WATCHDOG:
	    rtapi_print_msg(RTAPI_MSG_DBG, "ENABLE_WATCHDOG");
/*! \todo Another #if 0 */
#if 0
	    emcmotDebug->wdEnabling = 1;
	    emcmotDebug->wdWait = emcmotCommand->wdWait;
	    if (emcmotDebug->wdWait < 0) {
		emcmotDebug->wdWait = 0;
	    }
#endif
	    break;

	case EMCMOT_DISABLE_WATCHDOG:
	    rtapi_print_msg(RTAPI_MSG_DBG, "DISABLE_WATCHDOG");
/*! \todo Another #if 0 */
#if 0
	    emcmotDebug->wdEnabling = 0;
#endif
	    break;

	case EMCMOT_CLEAR_PROBE_FLAGS:
	    rtapi_print_msg(RTAPI_MSG_DBG, "CLEAR_PROBE_FLAGS");
	    emcmotStatus->probeTripped = 0;
	    emcmotStatus->probing = 1;
	    break;

	case EMCMOT_PROBE:
	    /* most of this is taken from EMCMOT_SET_LINE */
	    /* emcmotDebug->queue up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    rtapi_print_msg(RTAPI_MSG_DBG, "PROBE");
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for probe move");
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
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_TELEOP_VECTOR:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_TELEOP_VECTOR");
	    if (!GET_MOTION_TELEOP_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in teleop mode for teleop move");
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
		/* flag that all joints need to be homed, if any joint is
		   jogged individually later */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_DEBUG:
	    rtapi_print_msg(RTAPI_MSG_DBG, "SET_DEBUG");
	    emcmotConfig->debug = emcmotCommand->debug;
	    emcmot_config_change();
	    break;
/*! \todo Another #if 0 */
#if 0
/*! \todo FIXME - needed for synchronous I/O */
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
#endif
/*! \todo Another #if 0 */
#if 0
/*! \todo FIXME - needed for M62/M63 */
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
#endif
	default:
	    rtapi_print_msg(RTAPI_MSG_DBG, "UNKNOWN");
	    reportError("unrecognized command %d", emcmotCommand->command);
	    emcmotStatus->commandStatus = EMCMOT_COMMAND_UNKNOWN_COMMAND;
	    break;

	}			/* end of: command switch */
	if (emcmotStatus->commandStatus != EMCMOT_COMMAND_OK) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "ERRROR: %d",
		emcmotStatus->commandStatus);
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "\n");
	/* synch tail count */
	emcmotStatus->tail = emcmotStatus->head;
	emcmotConfig->tail = emcmotConfig->head;
	emcmotDebug->tail = emcmotDebug->head;

    }
    /* end of: if-new-command */
check_stuff ( "after command_handler()" );

    return;
}
