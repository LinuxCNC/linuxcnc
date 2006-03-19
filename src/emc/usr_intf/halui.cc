/********************************************************************
* Description: halui.cc
*   HAL User-Interface component.
*   This file exports various UI related hal pins, and communicates 
*   with EMC through NML messages
*
*   Derived from a work by Fred Proctor & Will Shackleford (emcsh.cc)
*   some of the functions (sendFooBar() are adapted from there)
*
* Author: Alex Joni
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2006 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE

/*
  Using halui:

  halui {-ini <ini file>}

  With -ini <inifile>, uses inifile instead of emc.ini. 

  Once executed, it connects to NML buffers, exports various pins
  and communicates changes to EMC. It also sets certain pins based on
  status values.

  
  Naming:
  
  All pins will be named after the following scheme:
  
  halui.<id>.section.<sec_id>.pin_name
  
  <id>       refers to the current HAL ui component, more than one could 
             be running inside the same HAL
  
  section    refers to the type of pins used,
             currently one of:
	     - general
	     - io
	     - axis
	     - jog
	     - program
	     - probe
	     

  <sec_id>   if more than one section instatiation happens (currently it is
             mostly 0)
	     
  pin_name   described in the next paragraph
	     

  Exported pins:

  bidirectional (you can set them, and also read the value)
  
   halui.0.general.0.machine               bit  //pin for On/Off
   halui.0.general.0.estop                 bit  //pin for Estop (emc internal) On/Off
   
   halui.0.general.0.emc_mode_manual       bit
   halui.0.general.0.emc_mode_auto         bit
   halui.0.general.0.emc_mode_mdi          bit

   halui.0.io.0.mist                       bit
   halui.0.io.0.flood                      bit
   halui.0.io.0.lube                       bit

   halui.0.io.0.spindle-start              bit
   halui.0.io.0.spindle-stop               bit
   halui.0.io.0.spindle-forward            bit
   halui.0.io.0.spindle-reverse            bit
   halui.0.io.0.spindle-increase           bit
   halui.0.io.0.spindle-decrease           bit

   halui.0.io.0.brake                      bit

   halui.0.joint.0.home                    bit //works both ways
   ..
   halui.0.joint.7.home                    bit //works both ways

   halui.0.joint.x.limit-min-soft          bit
   halui.0.joint.x.limit-max-soft          bit
   halui.0.joint.x.limit-min-hard          bit
   halui.0.joint.x.limit-max-hard          bit
   
   halui.0.joint.x.fault                   bit   
   halui.0.joint.x.homed                   bit

   halui.0.jog.0.speed                     float //set jog speed

   halui.0.jog.0.jog-minus                 bit
   halui.0.jog.0.jog-plus                  bit
   ..
   halui.0.jog.7.jog-minus                 bit
   halui.0.jog.7.jog-plus                  bit

   halui.0.general.feed_override           float


 one way (NML -> HAL)

   halui.0.io.1.lube-level                 bit  //0-low, 1-ok
   halui.0.io.1.tool-number                u16  //current selected tool
   halui.0.io.1.tool-length-offset         float //current applied tool-length-offset

   halui.0.program.0.idle                  bit
   halui.0.program.0.running               bit
   halui.0.program.0.paused                bit


 one way (HAL -> NML)

   halui.0.program.0.run                   bit
   halui.0.program.0.pause                 bit
   halui.0.program.0.resume                bit
   halui.0.program.0.step                  bit

   halui.0.probe.0.start                   bit
   halui.0.probe.0.clear                   bit
   halui.0.probe.0.tripped                 bit
   halui.0.probe.0.value                   float

*/

// the NML channels to the EMC task
static RCS_CMD_CHANNEL *emcCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcStatusBuffer = 0;
EMC_STAT *emcStatus = 0;

// the NML channel for errors
static NML *emcErrorBuffer = 0;

// the current command numbers, set up updateStatus(), used in main()
static int emcCommandSerialNumber = 0;
static int saveEmcCommandSerialNumber = 0;

// default value for timeout, 0 means wait forever
static double emcTimeout = 0.0;

static enum {
    EMC_WAIT_NONE = 1,
    EMC_WAIT_RECEIVED,
    EMC_WAIT_DONE
} emcWaitType = EMC_WAIT_DONE;

static enum {
    EMC_UPDATE_NONE = 1,
    EMC_UPDATE_AUTO
} emcUpdateType = EMC_UPDATE_AUTO;

static int emcTaskNmlGet()
{
    int retval = 0;

    // try to connect to EMC cmd
    if (emcCommandBuffer == 0) {
	emcCommandBuffer =
	    new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc",
				EMC_NMLFILE);
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
				 EMC_NMLFILE);
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

static int emcErrorNmlGet()
{
    int retval = 0;

    if (emcErrorBuffer == 0) {
	emcErrorBuffer =
	    new NML(nmlErrorFormat, "emcError", "xemc", EMC_NMLFILE);
	if (!emcErrorBuffer->valid()) {
	    delete emcErrorBuffer;
	    emcErrorBuffer = 0;
	    retval = -1;
	}
    }

    return retval;
}

static int tryNml()
{
    double end;
    int good;
#define RETRY_TIME 10.0		// seconds to wait for subsystems to come up
#define RETRY_INTERVAL 1.0	// seconds between wait tries for a subsystem

    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_NULL);	// inhibit diag
	// messages
    }
    end = RETRY_TIME;
    good = 0;
    do {
	if (0 == emcTaskNmlGet()) {
	    good = 1;
	    break;
	}
	esleep(RETRY_INTERVAL);
	end -= RETRY_INTERVAL;
    } while (end > 0.0);
    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// inhibit diag
	// messages
    }
    if (!good) {
	return -1;
    }

    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_NULL);	// inhibit diag
	// messages
    }
    end = RETRY_TIME;
    good = 0;
    do {
	if (0 == emcErrorNmlGet()) {
	    good = 1;
	    break;
	}
	esleep(RETRY_INTERVAL);
	end -= RETRY_INTERVAL;
    } while (end > 0.0);
    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// inhibit diag
	// messages
    }
    if (!good) {
	return -1;
    }

    return 0;

#undef RETRY_TIME
#undef RETRY_INTERVAL
}

static int updateStatus()
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


#define EMC_COMMAND_DELAY   0.1	// how long to sleep between checks

/*
  emcCommandWaitReceived() waits until the EMC reports that it got
  the command with the indicated serial_number.
  emcCommandWaitDone() waits until the EMC reports that it got the
  command with the indicated serial_number, and it's done, or error.
*/

static int emcCommandWaitReceived(int serial_number)
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

static int emcCommandWaitDone(int serial_number)
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

static void thisQuit()
{
    EMC_NULL emc_null_msg;

    if (0 != emcStatusBuffer) {
	// wait until current message has been received
	emcCommandWaitReceived(emcCommandSerialNumber);
    }

    if (0 != emcCommandBuffer) {
	// send null message to reset serial number to original
	emc_null_msg.serial_number = saveEmcCommandSerialNumber;
	emcCommandBuffer->write(emc_null_msg);
    }
    // clean up NML buffers

    if (emcErrorBuffer != 0) {
	delete emcErrorBuffer;
	emcErrorBuffer = 0;
    }

    if (emcStatusBuffer != 0) {
	delete emcStatusBuffer;
	emcStatusBuffer = 0;
	emcStatus = 0;
    }

    if (emcCommandBuffer != 0) {
	delete emcCommandBuffer;
	emcCommandBuffer = 0;
    }

    exit(0);
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

static enum {
    LINEAR_UNITS_CUSTOM = 1,
    LINEAR_UNITS_AUTO,
    LINEAR_UNITS_MM,
    LINEAR_UNITS_INCH,
    LINEAR_UNITS_CM
} linearUnitConversion = LINEAR_UNITS_AUTO;

static enum {
    ANGULAR_UNITS_CUSTOM = 1,
    ANGULAR_UNITS_AUTO,
    ANGULAR_UNITS_DEG,
    ANGULAR_UNITS_RAD,
    ANGULAR_UNITS_GRAD
} angularUnitConversion = ANGULAR_UNITS_AUTO;

#define CLOSE(a,b,eps) ((a)-(b) < +(eps) && (a)-(b) > -(eps))
#define LINEAR_CLOSENESS 0.0001
#define ANGULAR_CLOSENESS 0.0001
#define INCH_PER_MM (1.0/25.4)
#define CM_PER_MM 0.1
#define GRAD_PER_DEG (100.0/90.0)
#define RAD_PER_DEG TO_RAD	// from posemath.h

/*
  to convert linear units, values are converted to mm, then to desired
  units
*/
static double convertLinearUnits(double u)
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

// polarities for axis jogging, from ini file
static int jogPol[EMC_AXIS_MAX];

static int sendDebug(int level)
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

static int sendEstop()
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

static int sendEstopReset()
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

static int sendMachineOn()
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

static int sendMachineOff()
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

static int sendManual()
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

static int sendAuto()
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

static int sendMdi()
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

static int sendOverrideLimits(int axis)
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

static int sendJogStop(int axis)
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
	emc_set_teleop_vector.vector.tran.x = 0;
	emc_set_teleop_vector.vector.tran.y = 0;
	emc_set_teleop_vector.vector.tran.z = 0;
	emc_set_teleop_vector.vector.a = 0;
	emc_set_teleop_vector.vector.b = 0;
	emc_set_teleop_vector.vector.c = 0;
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

static int sendJogCont(int axis, double speed)
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
	emc_set_teleop_vector.vector.tran.x = 0.0;
	emc_set_teleop_vector.vector.tran.y = 0.0;
	emc_set_teleop_vector.vector.tran.z = 0.0;

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

static int sendJogIncr(int axis, double speed, double incr)
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

static int sendMistOn()
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

static int sendMistOff()
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

static int sendFloodOn()
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

static int sendFloodOff()
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

static int sendLubeOn()
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

static int sendLubeOff()
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

static int sendSpindleForward()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed = fabs(emcStatus->task.activeSettings[2]);
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

static int sendSpindleReverse()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed =
	    -1 * fabs(emcStatus->task.activeSettings[2]);
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

static int sendSpindleOff()
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

static int sendSpindleIncrease()
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

static int sendSpindleDecrease()
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

static int sendSpindleConstant()
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

static int sendBrakeEngage()
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

static int sendBrakeRelease()
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

static int sendAbort()
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

static int sendHome(int axis)
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

static int sendFeedOverride(double override)
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

static int sendTaskPlanInit()
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

// programStartLine is the saved valued of the line that
// sendProgramRun(int line) sent
static int programStartLine = 0;

static int sendProgramRun(int line)
{
    EMC_TASK_PLAN_RUN emc_task_plan_run_msg;

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (0 == emcStatus->task.file[0]) {
	return -1; // no program open
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

static int sendProgramPause()
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

static int sendProgramResume()
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

static int sendProgramStep()
{
    EMC_TASK_PLAN_STEP emc_task_plan_step_msg;

    emc_task_plan_step_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_step_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendMdiCmd(char *mdi)
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

static int sendToolSetOffset(int tool, double length, double diameter)
{
    EMC_TOOL_SET_OFFSET emc_tool_set_offset_msg;

    emc_tool_set_offset_msg.tool = tool;
    emc_tool_set_offset_msg.length = length;
    emc_tool_set_offset_msg.diameter = diameter;
    emc_tool_set_offset_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_tool_set_offset_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAxisEnable(int axis, int val)
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

static int sendAxisLoadComp(int axis, const char *file)
{
    EMC_AXIS_LOAD_COMP emc_axis_load_comp_msg;

    strcpy(emc_axis_load_comp_msg.file, file);
    emc_axis_load_comp_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_load_comp_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAxisAlter(int axis, double alter)
{
    EMC_AXIS_ALTER emc_axis_alter_msg;

    emc_axis_alter_msg.alter = alter;
    emc_axis_alter_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_alter_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSetTeleopEnable(int enable)
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

static int sendClearProbeTrippedFlag()
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

static int sendProbe(double x, double y, double z)
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

static int iniLoad(const char *filename)
{
    Inifile inifile;
    const char *inistring;
    char displayString[LINELEN] = "";
    int t;
    int i;

    // open it
    if (inifile.open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = inifile.find("DEBUG", "EMC"))) {
	// copy to global
	if (1 != sscanf(inistring, "%i", &EMC_DEBUG)) {
	    EMC_DEBUG = 0;
	}
    } else {
	// not found, use default
	EMC_DEBUG = 0;
    }

    if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
	// copy to global
	strcpy(EMC_NMLFILE, inistring);
    } else {
	// not found, use default
    }

    for (t = 0; t < EMC_AXIS_MAX; t++) {
	jogPol[t] = 1;		// set to default
	sprintf(displayString, "AXIS_%d", t);
	if (NULL != (inistring =
		     inifile.find("JOGGING_POLARITY", displayString)) &&
	    1 == sscanf(inistring, "%d", &i) && i == 0) {
	    // it read as 0, so override default
	    jogPol[t] = 0;
	}
    }

    if (NULL != (inistring = inifile.find("LINEAR_UNITS", "DISPLAY"))) {
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

    if (NULL != (inistring = inifile.find("ANGULAR_UNITS", "DISPLAY"))) {
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
    inifile.close();

    return 0;
}

int main(int argc, char *argv[])
{

    // process command line args
    if (0 != emcGetArgs(argc, argv)) {
	rcs_print_error("error in argument list\n");
	exit(1);
    }
    // get configuration information
    iniLoad(EMC_INIFILE);

    // init NML
    if (0 != tryNml()) {
	rcs_print_error("can't connect to emc\n");
	thisQuit();
	exit(1);
    }
    // get current serial number, and save it for restoring when we quit
    // so as not to interfere with real operator interface
    updateStatus();
    emcCommandSerialNumber = emcStatus->echo_serial_number;
    saveEmcCommandSerialNumber = emcStatus->echo_serial_number;


    thisQuit();
    return 0;
}
