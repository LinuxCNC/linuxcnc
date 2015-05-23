/********************************************************************
* Description: shcom.cc
*   Common functions for NML calls
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Further derived from work by jmkasunich, Alex Joni
*
* Author: Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2006 All rights reserved.
*
* Last change:
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include "rtapi_math.h"
#include <sys/types.h>

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "nml_oi.hh"            // nmlErrorFormat, NML_ERROR, etc
#include "rcs_print.hh"
#include "timer.hh"             // esleep
#include "shcom.hh"             // Common NML communications functions

LINEAR_UNIT_CONVERSION linearUnitConversion;
ANGULAR_UNIT_CONVERSION angularUnitConversion;

int emcCommandSerialNumber;
int saveEmcCommandSerialNumber;

// the NML channels to the EMC task
RCS_CMD_CHANNEL *emcCommandBuffer;
RCS_STAT_CHANNEL *emcStatusBuffer;
EMC_STAT *emcStatus;

// the NML channel for errors
NML *emcErrorBuffer;
char error_string[NML_ERROR_LEN];
char operator_text_string[NML_TEXT_LEN];
char operator_display_string[NML_DISPLAY_LEN];
char defaultPath[80] = DEFAULT_PATH;
// default value for timeout, 0 means wait forever
double emcTimeout;
int programStartLine;

EMC_UPDATE_TYPE emcUpdateType;
EMC_WAIT_TYPE emcWaitType;

void strupr(char *s)
{  
  int i;
  
  for (i = 0; i < (int)strlen(s); i++)
    if (s[i] > 96 && s[i] <= 'z')
      s[i] -= 32;
}

int emcTaskNmlGet()
{
    int retval = 0;

    // try to connect to EMC cmd
    if (emcCommandBuffer == 0) {
	emcCommandBuffer =
	    new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc",
				emc_nmlfile);
	if (!emcCommandBuffer->valid()) {
	    delete emcCommandBuffer;
	    emcCommandBuffer = 0;
	    retval = -1;
	}
    }
    // try to connect to EMC status
    if (emcStatusBuffer == 0) {
	emcStatusBuffer =
	    new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc",
				 emc_nmlfile);
	if (!emcStatusBuffer->valid()
	    || EMC_STAT_TYPE != emcStatusBuffer->peek()) {
	    delete emcStatusBuffer;
	    emcStatusBuffer = 0;
	    emcStatus = 0;
	    retval = -1;
	} else {
	    emcStatus = (EMC_STAT *) emcStatusBuffer->get_address();
	}
    }

    return retval;
}

int emcErrorNmlGet()
{
    int retval = 0;

    if (emcErrorBuffer == 0) {
	emcErrorBuffer =
	    new NML(nmlErrorFormat, "emcError", "xemc", emc_nmlfile);
	if (!emcErrorBuffer->valid()) {
	    delete emcErrorBuffer;
	    emcErrorBuffer = 0;
	    retval = -1;
	}
    }

    return retval;
}

int tryNml(double retry_time, double retry_interval)
{
    double end;
    int good;

    if ((emc_debug & EMC_DEBUG_NML) == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_NULL);	// inhibit diag
	// messages
    }
    end = retry_time;
    good = 0;
    do {
	if (0 == emcTaskNmlGet()) {
	    good = 1;
	    break;
	}
	esleep(retry_interval);
	end -= retry_interval;
    } while (end > 0.0);
    if ((emc_debug & EMC_DEBUG_NML) == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// inhibit diag
	// messages
    }
    if (!good) {
	return -1;
    }

    if ((emc_debug & EMC_DEBUG_NML) == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_NULL);	// inhibit diag
	// messages
    }
    end = retry_time;
    good = 0;
    do {
	if (0 == emcErrorNmlGet()) {
	    good = 1;
	    break;
	}
	esleep(retry_interval);
	end -= retry_interval;
    } while (end > 0.0);
    if ((emc_debug & EMC_DEBUG_NML) == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// inhibit diag
	// messages
    }
    if (!good) {
	return -1;
    }

    return 0;
}

int updateStatus()
{
    NMLTYPE type;

    if (0 == emcStatus || 0 == emcStatusBuffer
	|| !emcStatusBuffer->valid()) {
	return -1;
    }

    switch (type = emcStatusBuffer->peek()) {
    case -1:
	// error on CMS channel
	return -1;
	break;

    case 0:			// no new data
    case EMC_STAT_TYPE:	// new data
	// new data
	break;

    default:
	return -1;
	break;
    }

    return 0;
}

/*
  updateError() updates "errors," which are true errors and also
  operator display and text messages.
*/
int updateError()
{
    NMLTYPE type;

    if (0 == emcErrorBuffer || !emcErrorBuffer->valid()) {
	return -1;
    }

    switch (type = emcErrorBuffer->read()) {
    case -1:
	// error reading channel
	return -1;
	break;

    case 0:
	// nothing new
	break;

    case EMC_OPERATOR_ERROR_TYPE:
	strncpy(error_string,
		((EMC_OPERATOR_ERROR *) (emcErrorBuffer->get_address()))->
		error, LINELEN - 1);
	error_string[NML_ERROR_LEN - 1] = 0;
	break;

    case EMC_OPERATOR_TEXT_TYPE:
	strncpy(operator_text_string,
		((EMC_OPERATOR_TEXT *) (emcErrorBuffer->get_address()))->
		text, LINELEN - 1);
	operator_text_string[NML_TEXT_LEN - 1] = 0;
	break;

    case EMC_OPERATOR_DISPLAY_TYPE:
	strncpy(operator_display_string,
		((EMC_OPERATOR_DISPLAY *) (emcErrorBuffer->
					   get_address()))->display,
		LINELEN - 1);
	operator_display_string[NML_DISPLAY_LEN - 1] = 0;
	break;

    case NML_ERROR_TYPE:
	strncpy(error_string,
		((NML_ERROR *) (emcErrorBuffer->get_address()))->error,
		NML_ERROR_LEN - 1);
	error_string[NML_ERROR_LEN - 1] = 0;
	break;

    case NML_TEXT_TYPE:
	strncpy(operator_text_string,
		((NML_TEXT *) (emcErrorBuffer->get_address()))->text,
		NML_TEXT_LEN - 1);
	operator_text_string[NML_TEXT_LEN - 1] = 0;
	break;

    case NML_DISPLAY_TYPE:
	strncpy(operator_display_string,
		((NML_DISPLAY *) (emcErrorBuffer->get_address()))->display,
		NML_DISPLAY_LEN - 1);
	operator_display_string[NML_DISPLAY_LEN - 1] = 0;
	break;

    default:
	// if not recognized, set the error string
	sprintf(error_string, "unrecognized error %ld", type);
	return -1;
	break;
    }

    return 0;
}

#define EMC_COMMAND_DELAY   0.1	// how long to sleep between checks

/*
  emcCommandWaitReceived() waits until the EMC reports that it got
  the command with the indicated serial_number.
  emcCommandWaitDone() waits until the EMC reports that it got the
  command with the indicated serial_number, and it's done, or error.
*/

int emcCommandWaitReceived(int serial_number)
{
    double end = 0.0;

    while (emcTimeout <= 0.0 || end < emcTimeout) {
	updateStatus();

	if (emcStatus->echo_serial_number == serial_number) {
	    return 0;
	}

	esleep(EMC_COMMAND_DELAY);
	end += EMC_COMMAND_DELAY;
    }

    return -1;
}

int emcCommandWaitDone(int serial_number)
{
    double end = 0.0;

    // first get it there
    if (0 != emcCommandWaitReceived(serial_number)) {
	return -1;
    }
    // now wait until it, or subsequent command (e.g., abort) is done
    while (emcTimeout <= 0.0 || end < emcTimeout) {
	updateStatus();

	if (emcStatus->status == RCS_DONE) {
	    return 0;
	}

	if (emcStatus->status == RCS_ERROR) {
	    return -1;
	}

	esleep(EMC_COMMAND_DELAY);
	end += EMC_COMMAND_DELAY;
    }

    return -1;
}


/*
  Unit conversion

  Length and angle units in the EMC status buffer are in user units, as
  defined in the INI file in [TRAJ] LINEAR,ANGULAR_UNITS. These may differ
  from the program units, and when they are the display is confusing.

  It may be desirable to synchronize the display units with the program
  units automatically, and also to break this sync and allow independent
  display of position values.

  The global variable "linearUnitConversion" is set by the Tcl commands
  emc_linear_unit_conversion to correspond to either "inch",
  "mm", "cm", "auto", or "custom". This forces numbers to be returned in the
  units specified, in program units when "auto" is set, or not converted
  at all if "custom" is specified.

  Ditto for "angularUnitConversion", set by emc_angular_unit_conversion
  to "deg", "rad", "grad", "auto", or "custom".

  With no args, emc_linear/angular_unit_conversion return the setting.

  The functions convertLinearUnits and convertAngularUnits take a length
  or angle value, typically from the emcStatus structure, and convert it
  as indicated by linearUnitConversion and angularUnitConversion, resp.
*/


/*
  to convert linear units, values are converted to mm, then to desired
  units
*/
double convertLinearUnits(double u)
{
    double in_mm;

    /* convert u to mm */
    in_mm = u / emcStatus->motion.traj.linearUnits;

    /* convert u to display units */
    switch (linearUnitConversion) {
    case LINEAR_UNITS_MM:
	return in_mm;
	break;
    case LINEAR_UNITS_INCH:
	return in_mm * INCH_PER_MM;
	break;
    case LINEAR_UNITS_CM:
	return in_mm * CM_PER_MM;
	break;
    case LINEAR_UNITS_AUTO:
	switch (emcStatus->task.programUnits) {
	case CANON_UNITS_MM:
	    return in_mm;
	    break;
	case CANON_UNITS_INCHES:
	    return in_mm * INCH_PER_MM;
	    break;
	case CANON_UNITS_CM:
	    return in_mm * CM_PER_MM;
	    break;
	}
	break;

    case LINEAR_UNITS_CUSTOM:
	return u;
	break;
    }

    // If it ever gets here we have an error.

    return u;
}

double convertAngularUnits(double u)
{
    // Angular units are always degrees
    return u;
}

// polarities for axis jogging, from ini file
static int jogPol[EMC_AXIS_MAX];

int sendDebug(int level)
{
    EMC_SET_DEBUG debug_msg;

    debug_msg.debug = level;
    debug_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(debug_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendEstop()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ESTOP;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendEstopReset()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ESTOP_RESET;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendMachineOn()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ON;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendMachineOff()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_OFF;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendManual()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE_MANUAL;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendAuto()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE_AUTO;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendMdi()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE_MDI;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendOverrideLimits(int axis)
{
    EMC_AXIS_OVERRIDE_LIMITS lim_msg;

    lim_msg.axis = axis;	// neg means off, else on for all
    lim_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(lim_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}
static int axisJogging = -1;

int sendJogStop(int axis)
{
    EMC_AXIS_ABORT emc_axis_abort_msg;
    
    // in case of TELEOP mode we really need to send an TELEOP_VECTOR message
    // not a simple AXIS_ABORT, as more than one axis would be moving
    // (hint TELEOP mode is for nontrivial kinematics)
    EMC_TRAJ_SET_TELEOP_VECTOR emc_set_teleop_vector;

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    if (emcStatus->motion.traj.mode != EMC_TRAJ_MODE_TELEOP) {
	emc_axis_abort_msg.serial_number = ++emcCommandSerialNumber;
	emc_axis_abort_msg.axis = axis;
	emcCommandBuffer->write(emc_axis_abort_msg);

	if (emcWaitType == EMC_WAIT_RECEIVED) {
	    return emcCommandWaitReceived(emcCommandSerialNumber);
	} else if (emcWaitType == EMC_WAIT_DONE) {
	    return emcCommandWaitDone(emcCommandSerialNumber);
	}

	axisJogging = -1;
    }
    else {
	emc_set_teleop_vector.serial_number = ++emcCommandSerialNumber;
        ZERO_EMC_POSE(emc_set_teleop_vector.vector);
	emcCommandBuffer->write(emc_set_teleop_vector);

	if (emcWaitType == EMC_WAIT_RECEIVED) {
	    return emcCommandWaitReceived(emcCommandSerialNumber);
	} else if (emcWaitType == EMC_WAIT_DONE) {
	    return emcCommandWaitDone(emcCommandSerialNumber);
	}
	// \todo FIXME - should remember a list of jogging axes, and remove the last one
	axisJogging = -1;
	
    }
    return 0;
}

int sendJogCont(int axis, double speed)
{
    EMC_AXIS_JOG emc_axis_jog_msg;
    EMC_TRAJ_SET_TELEOP_VECTOR emc_set_teleop_vector;

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    if (emcStatus->motion.traj.mode != EMC_TRAJ_MODE_TELEOP) {
	if (0 == jogPol[axis]) {
	    speed = -speed;
	}

	emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
	emc_axis_jog_msg.axis = axis;
	emc_axis_jog_msg.vel = speed / 60.0;
	emcCommandBuffer->write(emc_axis_jog_msg);
    } else {
	emc_set_teleop_vector.serial_number = ++emcCommandSerialNumber;
        ZERO_EMC_POSE(emc_set_teleop_vector.vector);

	switch (axis) {
	case 0:
	    emc_set_teleop_vector.vector.tran.x = speed / 60.0;
	    break;
	case 1:
	    emc_set_teleop_vector.vector.tran.y = speed / 60.0;
	    break;
	case 2:
	    emc_set_teleop_vector.vector.tran.z = speed / 60.0;
	    break;
	case 3:
	    emc_set_teleop_vector.vector.a = speed / 60.0;
	    break;
	case 4:
	    emc_set_teleop_vector.vector.b = speed / 60.0;
	    break;
	case 5:
	    emc_set_teleop_vector.vector.c = speed / 60.0;
	    break;
	}
	emcCommandBuffer->write(emc_set_teleop_vector);
    }

    axisJogging = axis;
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendJogIncr(int axis, double speed, double incr)
{
    EMC_AXIS_INCR_JOG emc_axis_incr_jog_msg;

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    if (0 == jogPol[axis]) {
	speed = -speed;
    }

    emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_incr_jog_msg.axis = axis;
    emc_axis_incr_jog_msg.vel = speed / 60.0;
    emc_axis_incr_jog_msg.incr = incr;
    emcCommandBuffer->write(emc_axis_incr_jog_msg);

    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }
    axisJogging = -1;

    return 0;
}

int sendMistOn()
{
    EMC_COOLANT_MIST_ON emc_coolant_mist_on_msg;

    emc_coolant_mist_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_mist_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendMistOff()
{
    EMC_COOLANT_MIST_OFF emc_coolant_mist_off_msg;

    emc_coolant_mist_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_mist_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendFloodOn()
{
    EMC_COOLANT_FLOOD_ON emc_coolant_flood_on_msg;

    emc_coolant_flood_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_flood_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendFloodOff()
{
    EMC_COOLANT_FLOOD_OFF emc_coolant_flood_off_msg;

    emc_coolant_flood_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_flood_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendLubeOn()
{
    EMC_LUBE_ON emc_lube_on_msg;

    emc_lube_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_lube_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendLubeOff()
{
    EMC_LUBE_OFF emc_lube_off_msg;

    emc_lube_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_lube_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSpindleForward()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed = rtapi_fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = +500;
    }
    emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSpindleReverse()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed =
	    -1 * rtapi_fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = -500;
    }
    emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSpindleOff()
{
    EMC_SPINDLE_OFF emc_spindle_off_msg;

    emc_spindle_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSpindleIncrease()
{
    EMC_SPINDLE_INCREASE emc_spindle_increase_msg;

    emc_spindle_increase_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_increase_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSpindleDecrease()
{
    EMC_SPINDLE_DECREASE emc_spindle_decrease_msg;

    emc_spindle_decrease_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_decrease_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSpindleConstant()
{
    EMC_SPINDLE_CONSTANT emc_spindle_constant_msg;

    emc_spindle_constant_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_constant_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendBrakeEngage()
{
    EMC_SPINDLE_BRAKE_ENGAGE emc_spindle_brake_engage_msg;

    emc_spindle_brake_engage_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_brake_engage_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendBrakeRelease()
{
    EMC_SPINDLE_BRAKE_RELEASE emc_spindle_brake_release_msg;

    emc_spindle_brake_release_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_brake_release_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendAbort()
{
    EMC_TASK_ABORT task_abort_msg;

    task_abort_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(task_abort_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendHome(int axis)
{
    EMC_AXIS_HOME emc_axis_home_msg;

    emc_axis_home_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_home_msg.axis = axis;
    emcCommandBuffer->write(emc_axis_home_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendUnHome(int axis)
{
    EMC_AXIS_UNHOME emc_axis_home_msg;

    emc_axis_home_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_home_msg.axis = axis;
    emcCommandBuffer->write(emc_axis_home_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendFeedOverride(double override)
{
    EMC_TRAJ_SET_SCALE emc_traj_set_scale_msg;

    if (override < 0.0) {
	override = 0.0;
    }

    emc_traj_set_scale_msg.serial_number = ++emcCommandSerialNumber;
    emc_traj_set_scale_msg.scale = override;
    emcCommandBuffer->write(emc_traj_set_scale_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSpindleOverride(double override)
{
    EMC_TRAJ_SET_SPINDLE_SCALE emc_traj_set_spindle_scale_msg;

    if (override < 0.0) {
	override = 0.0;
    }

    emc_traj_set_spindle_scale_msg.serial_number = ++emcCommandSerialNumber;
    emc_traj_set_spindle_scale_msg.scale = override;
    emcCommandBuffer->write(emc_traj_set_spindle_scale_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendTaskPlanInit()
{
    EMC_TASK_PLAN_INIT task_plan_init_msg;

    task_plan_init_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(task_plan_init_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

// saved value of last program opened
static char lastProgramFile[LINELEN] = "";

int sendProgramOpen(char *program)
{
    EMC_TASK_PLAN_OPEN emc_task_plan_open_msg;

    // save this to run again
    strcpy(lastProgramFile, program);

    emc_task_plan_open_msg.serial_number = ++emcCommandSerialNumber;
    strcpy(emc_task_plan_open_msg.file, program);
    emcCommandBuffer->write(emc_task_plan_open_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendProgramRun(int line)
{
    EMC_TASK_PLAN_RUN emc_task_plan_run_msg;

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }
    // first reopen program if it's not open
    if (0 == emcStatus->task.file[0]) {
	// send a request to open last one
	sendProgramOpen(lastProgramFile);
    }
    // save the start line, to compare against active line later
    programStartLine = line;

    emc_task_plan_run_msg.serial_number = ++emcCommandSerialNumber;
    emc_task_plan_run_msg.line = line;
    emcCommandBuffer->write(emc_task_plan_run_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendProgramPause()
{
    EMC_TASK_PLAN_PAUSE emc_task_plan_pause_msg;

    emc_task_plan_pause_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_pause_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendProgramResume()
{
    EMC_TASK_PLAN_RESUME emc_task_plan_resume_msg;

    emc_task_plan_resume_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_resume_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSetOptionalStop(bool state)
{
    EMC_TASK_PLAN_SET_OPTIONAL_STOP emc_task_plan_set_optional_stop_msg;

    emc_task_plan_set_optional_stop_msg.state = state;
    emc_task_plan_set_optional_stop_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_set_optional_stop_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}


int sendProgramStep()
{
    EMC_TASK_PLAN_STEP emc_task_plan_step_msg;

    // clear out start line, if we had a verify before it would be -1
    programStartLine = 0;

    emc_task_plan_step_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_step_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendMdiCmd(const char *mdi)
{
    EMC_TASK_PLAN_EXECUTE emc_task_plan_execute_msg;

    strcpy(emc_task_plan_execute_msg.command, mdi);
    emc_task_plan_execute_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_execute_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendLoadToolTable(const char *file)
{
    EMC_TOOL_LOAD_TOOL_TABLE emc_tool_load_tool_table_msg;

    strcpy(emc_tool_load_tool_table_msg.file, file);
    emc_tool_load_tool_table_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_tool_load_tool_table_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendToolSetOffset(int toolno, double zoffset, double diameter)
{
    EMC_TOOL_SET_OFFSET emc_tool_set_offset_msg;

    emc_tool_set_offset_msg.toolno = toolno;
    emc_tool_set_offset_msg.offset.tran.z = zoffset;
    emc_tool_set_offset_msg.diameter = diameter;
    emc_tool_set_offset_msg.orientation = 0; // mill style tool table

    emc_tool_set_offset_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_tool_set_offset_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendToolSetOffset(int toolno, double zoffset, double xoffset, 
                      double diameter, double frontangle, double backangle,
                      int orientation)
{
    EMC_TOOL_SET_OFFSET emc_tool_set_offset_msg;

    emc_tool_set_offset_msg.toolno = toolno;
    emc_tool_set_offset_msg.offset.tran.z = zoffset;
    emc_tool_set_offset_msg.offset.tran.x = xoffset;
    emc_tool_set_offset_msg.diameter = diameter;      
    emc_tool_set_offset_msg.frontangle = frontangle;  
    emc_tool_set_offset_msg.backangle = backangle;    
    emc_tool_set_offset_msg.orientation = orientation;

    emc_tool_set_offset_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_tool_set_offset_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendAxisSetBacklash(int axis, double backlash)
{
    EMC_AXIS_SET_BACKLASH emc_axis_set_backlash_msg;

    emc_axis_set_backlash_msg.axis = axis;
    emc_axis_set_backlash_msg.backlash = backlash;
    emc_axis_set_backlash_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_set_backlash_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendAxisEnable(int axis, int val)
{
    EMC_AXIS_ENABLE emc_axis_enable_msg;
    EMC_AXIS_DISABLE emc_axis_disable_msg;

    if (val) {
	emc_axis_enable_msg.axis = axis;
	emc_axis_enable_msg.serial_number = ++emcCommandSerialNumber;
	emcCommandBuffer->write(emc_axis_enable_msg);
    } else {
	emc_axis_disable_msg.axis = axis;
	emc_axis_disable_msg.serial_number = ++emcCommandSerialNumber;
	emcCommandBuffer->write(emc_axis_disable_msg);
    }
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendAxisLoadComp(int axis, const char *file, int type)
{
    EMC_AXIS_LOAD_COMP emc_axis_load_comp_msg;

    strcpy(emc_axis_load_comp_msg.file, file);
    emc_axis_load_comp_msg.type = type;
    emc_axis_load_comp_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_load_comp_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendSetTeleopEnable(int enable)
{
    EMC_TRAJ_SET_TELEOP_ENABLE emc_set_teleop_enable_msg;

    emc_set_teleop_enable_msg.enable = enable;
    emc_set_teleop_enable_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_set_teleop_enable_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendClearProbeTrippedFlag()
{
    EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG emc_clear_probe_tripped_flag_msg;

    emc_clear_probe_tripped_flag_msg.serial_number =
	++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_clear_probe_tripped_flag_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int sendProbe(double x, double y, double z)
{
    EMC_TRAJ_PROBE emc_probe_msg;

    emc_probe_msg.pos.tran.x = x;
    emc_probe_msg.pos.tran.y = y;
    emc_probe_msg.pos.tran.z = z;

    emc_probe_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_probe_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

int iniLoad(const char *filename)
{
    IniFile inifile;
    const char *inistring;
    char displayString[LINELEN] = "";
    int t;
    int i;

    // open it
    if (inifile.Open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = inifile.Find("DEBUG", "EMC"))) {
	// copy to global
	if (1 != sscanf(inistring, "%i", &emc_debug)) {
	    emc_debug = 0;
	}
    } else {
	// not found, use default
	emc_debug = 0;
    }

    if (NULL != (inistring = inifile.Find("NML_FILE", "EMC"))) {
	// copy to global
	strcpy(emc_nmlfile, inistring);
    } else {
	// not found, use default
    }

    for (t = 0; t < EMC_AXIS_MAX; t++) {
	jogPol[t] = 1;		// set to default
	sprintf(displayString, "AXIS_%d", t);
	if (NULL != (inistring =
		     inifile.Find("JOGGING_POLARITY", displayString)) &&
	    1 == sscanf(inistring, "%d", &i) && i == 0) {
	    // it read as 0, so override default
	    jogPol[t] = 0;
	}
    }

    if (NULL != (inistring = inifile.Find("LINEAR_UNITS", "DISPLAY"))) {
	if (!strcmp(inistring, "AUTO")) {
	    linearUnitConversion = LINEAR_UNITS_AUTO;
	} else if (!strcmp(inistring, "INCH")) {
	    linearUnitConversion = LINEAR_UNITS_INCH;
	} else if (!strcmp(inistring, "MM")) {
	    linearUnitConversion = LINEAR_UNITS_MM;
	} else if (!strcmp(inistring, "CM")) {
	    linearUnitConversion = LINEAR_UNITS_CM;
	}
    } else {
	// not found, leave default alone
    }

    if (NULL != (inistring = inifile.Find("ANGULAR_UNITS", "DISPLAY"))) {
	if (!strcmp(inistring, "AUTO")) {
	    angularUnitConversion = ANGULAR_UNITS_AUTO;
	} else if (!strcmp(inistring, "DEG")) {
	    angularUnitConversion = ANGULAR_UNITS_DEG;
	} else if (!strcmp(inistring, "RAD")) {
	    angularUnitConversion = ANGULAR_UNITS_RAD;
	} else if (!strcmp(inistring, "GRAD")) {
	    angularUnitConversion = ANGULAR_UNITS_GRAD;
	}
    } else {
	// not found, leave default alone
    }

    // close it
    inifile.Close();

    return 0;
}

int checkStatus ()
{
    if (emcStatus) return 1;    
    return 0;
}



