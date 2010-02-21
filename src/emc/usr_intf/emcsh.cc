/********************************************************************
* Description: emcsh.cc
*   Extended-Tcl-based EMC automatic test interface
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
*         Reorganized by Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <tcl.h>
#include <tk.h>

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "rcs_print.hh"
#include "timer.hh"

#include "shcom.hh"

/*
  Using emcsh:

  emcsh {<filename>} {-- -ini <ini file>}

  With filename, it opens NML buffers to the EMC, runs the script, closes
  the buffers, and quits.

  With -- -ini <inifile>, uses inifile instead of emc.ini. Note that
  the two dashes prevents Tcl from looking at the remaining args, which
  would otherwise trigger a Tcl error that it doesn't understand what
  -ini means.

  Without filename, it runs interactively.

  The files (or manual input) are Tcl scripts, extended with EMC-specific
  commands. These commands are all prefixed with "emc_", which makes them
  somewhat inconvenient for typing but avoids name conflicts, e.g., open.

  Some commands take 0 or more arguments. 0 arguments means they return
  the associated value; the argument would be to set the value.

  Commands are sent to the EMC, and control resumes immediately. You can
  call a timed wait until the command got there, or a timed wait until the
  command completed, or not wait at all.

  EMC commands:

  EMC_INIFILE
  Exported values of the EMC global of the same name

  emc_plat
  Returns the platform for which this was compiled, e.g., linux_2_0_36

  emc_ini <var> <section>
  Returns the string value of <var> in section <section>, in EMC_INIFILE

  emc_debug {<new value>}
  With no arg, returns the integer value of EMC_DEBUG, in the EMC. Note that
  it may not be true that the local EMC_DEBUG variable here (in emcsh and
  the GUIs that use it) is the same as the EMC_DEBUG value in the EMC. This
  can happen if the EMC is started from one .ini file, and the GUI is started
  with another that has a different value for DEBUG.
  With an arg, sends a command to the EMC to set the new debug level,
  and sets the EMC_DEBUG global here to the same value. This will make
  the two values the same, since they really ought to be the same.

  emc_set_wait none | received | done
  Set the wait for commands to return to be right away (none), after the
  command was sent and received (received), or after the command was
  done (done).

  emc_wait received | done
  Force a wait for the previous command to be received, or done. This lets
  you wait in the event that "emc_set_wait none" is in effect.

  emc_set_timeout <timeout>
  Set the timeout for commands to return to <timeout>, in seconds. Timeout
  is a real number. If it's <= 0.0, it means wait forever. Default is 0.0,
  wait forever.

  emc_update (none) | none | auto
  With no arg, forces an update of the EMC status. With "none", doesn't
  cause an automatic update of status with other emc_ words. With "auto",
  makes emc_ words automatically update status before they return values.

  emc_error
  Returns the current EMC error string, or "ok" if no error.

  emc_operator_display
  Returns the current EMC operator display string, or "ok" if none.

  emc_operator_text
  Returns the current EMC operator text string, or "ok" if none.

  emc_time
  Returns the time, in seconds, from the start of the epoch. This starting
  time depends on the platform.

  emc_estop (none) | on | off
  With no arg, returns the estop setting as "on" or "off". Otherwise,
  sends an estop on or off command.

  emc_machine (none) | on | off
  With no arg, returns the machine setting as "on" or "off". Otherwise,
  sends a machine on or off command.

  emc_mode (none) | manual | auto | mdi
  With no arg, returns the mode setting as "manual", "auto", or "mdi".
  Otherwise, sends a mode manual, auto, or mdi command.

  emc_mist (none) | on | off
  With no arg, returns the mist setting as "on" or "off". Otherwise,
  sends a mist on or off command.

  emc_flood (none) | on | off
  With no arg, returns the flood setting as "on" or "off". Otherwise,
  sends a flood on or off command.

  emc_lube (none) | on | off
  With no arg, returns the lubricant pump setting as "on" or "off".
  Otherwise, sends a lube on or off command.

  emc_lube_level
  Returns the lubricant level sensor reading as "ok" or "low".

  emc_spindle (none) | forward | reverse | increase | decrease | constant | off
  With no arg, returns the value of the spindle state as "forward",
  "reverse", "increase", "decrease", or "off". With arg, sends the spindle
  command. Note that "increase" and "decrease" will cause a speed change in
  the corresponding direction until a "constant" command is sent.

  emc_brake (none) | on | off
  With no arg, returns the brake setting. Otherwise sets the brake.

  emc_tool
  Returns the id of the currently loaded tool

  emc_tool_offset
  Returns the currently applied tool length offset

  emc_load_tool_table <file>
  Loads the tool table specified by <file>

  emc_home 0 | 1 | 2 | ...
  Homes the indicated axis.

  emc_unhome 0 | 1 | 2 | ...
  Unhomes the indicated axis.

  emc_jog_stop 0 | 1 | 2 | ...
  Stop the axis jog

  emc_jog 0 | 1 | 2 | ... <speed>
  Jog the indicated axis at <speed>; sign of speed is direction

  emc_jog_incr 0 | 1 | 2 | ... <speed> <incr>
  Jog the indicated axis by increment <incr> at the <speed>; sign of
  speed is direction

  emc_feed_override {<percent>}
  With no args, returns the current feed override, as a percent. With
  argument, set the feed override to be the percent value

  emc_spindle_override {<percent>}
  With no args, returns the current spindle override, as a percent. With
  argument, set the spindle override to be the percent value

  emc_abs_cmd_pos 0 | 1 | ...
  Returns double obj containing the XYZ-SXYZ commanded pos in abs coords,
  at given index, 0 = X, etc.

  emc_abs_act_pos
  Returns double objs containing the XYZ-SXYZ actual pos in abs coords

  emc_rel_cmd_pos 0 | 1 | ...
  Returns double obj containing the XYZ-SXYZ commanded pos in rel coords,
  at given index, 0 = X, etc., including tool length offset

  emc_rel_act_pos
  Returns double objs containing the XYZ-SXYZ actual pos in rel coords,
  including tool length offset

  emc_joint_pos
  Returns double objs containing the actual pos in absolute coords of individual
  joint/slider positions, excludes tool length offset

  emc_pos_offset X | Y | Z | R | P | W
  Returns the position offset associated with the world coordinate provided

  emc_joint_limit 0 | 1 | ...
  Returns "ok", "minsoft", "minhard", "maxsoft", "maxhard"

  emc_joint_fault 0 | 1 | ...
  Returns "ok" or "fault"

  emc_joint_homed 0 | 1 | ...
  Returns "homed", "not"

  emc_mdi <string>
  Sends the <string> as an MDI command

  emc_task_plan_init
  Initializes the program interpreter

  emc_open <filename>
  Opens the named file

  emc_run {<start line>}
  Without start line, runs the opened program from the beginning. With
  start line, runs from that line. A start line of -1 runs in verify mode.

  emc_pause
  Pause program execution

  emc_resume
  Resume program execution

  emc_step
  Step the program one line

  emc_program
  Returns the name of the currently opened program, or "none"

  emc_program_line
  Returns the currently executing line of the program

  emc_program_status
  Returns "idle", "running", or "paused"

  emc_program_codes
  Returns the string for the currently active program codes

  emc_override_limit none | 0 | 1
  returns state of override, sets it or deactivates it (used to jog off hardware limit switches)
  
  emc_optional_stop  none | 0 | 1
  returns state of optional setop, sets it or deactivates it (used to stop/continue on M1)

  emc_program_codes
  Returns the string for the currently active program codes

  emc_joint_type <joint>
  Returns "linear", "angular", or "custom" for the type of the specified joint

  emc_joint_units <joint>
  Returns "inch", "mm", "cm", or "deg", "rad", "grad", or "custom",
  for the corresponding native units of the specified axis. The type
  of the axis (linear or angular) is used to resolve which type of units
  are returned. The units are obtained heuristically, based on the
  EMC_AXIS_STAT::units numerical value of user units per mm or deg.
  For linear joints, something close to 0.03937 is deemed "inch",
  1.000 is "mm", 0.1 is "cm", otherwise it's "custom".
  For angular joints, something close to 1.000 is deemed "deg",
  PI/180 is "rad", 100/90 is "grad", otherwise it's "custom".
 
  emc_program_units
  emc_program_linear_units
  Returns "inch", "mm", "cm", or "none", for the corresponding linear 
  units that are active in the program interpreter.

  emc_program_angular_units
  Returns "deg", "rad", "grad", or "none" for the corresponding angular
  units that are active in the program interpreter.

  emc_user_linear_units
  Returns "inch", "mm", "cm", or "custom", for the
  corresponding native user linear units of the EMC trajectory
  level. This is obtained heuristically, based on the
  EMC_TRAJ_STAT::linearUnits numerical value of user units per mm.
  Something close to 0.03937 is deemed "inch", 1.000 is "mm", 0.1 is
  "cm", otherwise it's "custom".

  emc_user_angular_units
  Returns "deg", "rad", "grad", or "custom" for the corresponding native
  user angular units of the EMC trajectory level. Like with linear units,
  this is obtained heuristically.

  emc_display_linear_units
  emc_display_angular_units
  Returns "inch", "mm", "cm", or "deg", "rad", "grad", or "custom",
  for the linear or angular units that are active in the display. 
  This is effectively the value of linearUnitConversion or
  angularUnitConversion, resp.

  emc_linear_unit_conversion {inch | mm | cm | auto}
  With no args, returns the unit conversion active. With arg, sets the
  units to be displayed. If it's "auto", the units to be displayed match
  the program units.
 
  emc_angular_unit_conversion {deg | rad | grad | auto}
  With no args, returns the unit conversion active. With arg, sets the
  units to be displayed. If it's "auto", the units to be displayed match
  the program units.

  emc_probe_clear
  Clear the probe tripped flag.

  emc_probe_tripped
  Has the probe been tripped since the last clear.

  emc_probe_value
  Value of current probe signal. (read-only)

  emc_probe
  Move toward a certain location. If the probe is tripped on the way stop
  motion, record the position and raise the probe tripped flag.

  emc_teleop_enable
  Should motion run in teleop mode? (No args
  gets it, one arg sets it.)

  emc_kinematics_type
  returns the type of kinematics functions used identity=1, serial=2,
  parallel=3, custom=4
*/

static void thisQuit(ClientData clientData)
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

    Tcl_Exit(0);
    exit(0);
}


/* EMC command functions */

static int emc_plat(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc == 1) {
	Tcl_SetResult(interp, "Linux", TCL_VOLATILE);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_plat: need no args", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_ini(ClientData clientdata,
		   Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    IniFile inifile;
    const char *inistring;
    const char *varstr, *secstr, *defaultstr;
    defaultstr = 0;

    if (objc != 3 && objc != 4) {
	Tcl_SetResult(interp, "emc_ini: need 'var' and 'section'",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    // open it
    if (inifile.Open(EMC_INIFILE) == false) {
	return TCL_OK;
    }

    varstr = Tcl_GetStringFromObj(objv[1], 0);
    secstr = Tcl_GetStringFromObj(objv[2], 0);

    if (objc == 4) {
	defaultstr = Tcl_GetStringFromObj(objv[3], 0);
    }

    if (NULL == (inistring = inifile.Find(varstr, secstr))) {
	if (defaultstr != 0) {
	    Tcl_SetResult(interp, (char *) defaultstr, TCL_VOLATILE);
	}
	return TCL_OK;
    }

    Tcl_SetResult(interp, (char *) inistring, TCL_VOLATILE);

    // close it
    inifile.Close();

    return TCL_OK;
}

static int emc_debug(ClientData clientdata,
		     Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    Tcl_Obj *debug_obj;
    int debug;

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (objc == 1) {
	// no arg-- return current value
	debug_obj = Tcl_NewIntObj(emcStatus->debug);
	Tcl_SetObjResult(interp, debug_obj);
	return TCL_OK;
    }

    if (objc == 2) {
	if (0 != Tcl_GetIntFromObj(0, objv[1], &debug)) {
	    Tcl_SetResult(interp, "emc_debug: need debug level as integer",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	sendDebug(debug);
	EMC_DEBUG = debug;
	return TCL_OK;
    }
    // wrong number of args
    Tcl_SetResult(interp, "emc_debug: need zero or one arg", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_set_wait(ClientData clientdata,
			Tcl_Interp * interp, int objc,
			Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	switch (emcWaitType) {
	case EMC_WAIT_NONE:
	    Tcl_SetResult(interp, "none", TCL_VOLATILE);
	    break;
	case EMC_WAIT_RECEIVED:
	    Tcl_SetResult(interp, "received", TCL_VOLATILE);
	    break;
	case EMC_WAIT_DONE:
	    Tcl_SetResult(interp, "done", TCL_VOLATILE);
	    break;
	default:
	    Tcl_SetResult(interp, "(invalid)", TCL_VOLATILE);
	    break;
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "none")) {
	    emcWaitType = EMC_WAIT_NONE;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "received")) {
	    emcWaitType = EMC_WAIT_RECEIVED;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "done")) {
	    emcWaitType = EMC_WAIT_DONE;
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp,
		  "emc_set_wait: need 'none', 'received', 'done', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_wait(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "received")) {
	    if (0 != emcCommandWaitReceived(emcCommandSerialNumber)) {
		Tcl_SetResult(interp, "timeout", TCL_VOLATILE);
	    }
	    return TCL_OK;
	}
	if (!strcmp(objstr, "done")) {
	    if (0 != emcCommandWaitDone(emcCommandSerialNumber)) {
		Tcl_SetResult(interp, "timeout", TCL_VOLATILE);
	    }
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_wait: need 'received' or 'done'",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_set_timeout(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    double timeout;
    Tcl_Obj *timeout_obj;

    if (objc == 1) {
	timeout_obj = Tcl_NewDoubleObj(emcTimeout);
	Tcl_SetObjResult(interp, timeout_obj);
	return TCL_OK;
    }

    if (objc == 2) {
	if (TCL_OK == Tcl_GetDoubleFromObj(0, objv[1], &timeout)) {
	    emcTimeout = timeout;
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_set_timeout: need time as real number",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_update(ClientData clientdata,
		      Tcl_Interp * interp, int objc,
		      Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	updateStatus();
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "none")) {
	    emcUpdateType = EMC_UPDATE_NONE;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "auto")) {
	    emcUpdateType = EMC_UPDATE_AUTO;
	    return TCL_OK;
	}
    }

    return TCL_OK;
}

static int emc_time(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc == 1) {
	Tcl_SetObjResult(interp, Tcl_NewDoubleObj(etime()));
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_time: needs no arguments", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_error(ClientData clientdata,
		     Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{

    if (objc == 1) {
	// get any new error, it's saved in global error_string[]
	if (0 != updateError()) {
	    Tcl_SetResult(interp, "emc_error: bad status from EMC",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	// put error on result list
	if (error_string[0] == 0) {
	    Tcl_SetResult(interp, "ok", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, error_string, TCL_VOLATILE);
	    error_string[0] = 0;
	}
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_error: need no args", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_operator_text(ClientData clientdata,
			     Tcl_Interp * interp, int objc,
			     Tcl_Obj * CONST objv[])
{

    if (objc == 1) {
	// get any new string, it's saved in global operator_text_string[]
	if (0 != updateError()) {
	    Tcl_SetResult(interp, "emc_operator_text: bad status from EMC",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	// put error on result list
	if (operator_text_string[0] == 0) {
	    Tcl_SetResult(interp, "ok", TCL_VOLATILE);
	    operator_text_string[0] = 0;
	} else {
	    Tcl_SetResult(interp, operator_text_string, TCL_VOLATILE);
	}
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_operator_text: need no args", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_operator_display(ClientData clientdata,
				Tcl_Interp * interp, int objc,
				Tcl_Obj * CONST objv[])
{

    if (objc == 1) {
	// get any new string, it's saved in global operator_display_string[]
	if (0 != updateError()) {
	    Tcl_SetResult(interp,
			  "emc_operator_display: bad status from EMC",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	// put error on result list
	if (operator_display_string[0] == 0) {
	    Tcl_SetResult(interp, "ok", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, operator_display_string, TCL_VOLATILE);
	    operator_display_string[0] = 0;
	}
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_operator_display: need no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_estop(ClientData clientdata,
		     Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->task.state == EMC_TASK_STATE_ESTOP) {
	    Tcl_SetResult(interp, "on", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "off", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "on")) {
	    sendEstop();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "off")) {
	    sendEstopReset();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_estop: need 'on', 'off', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_machine(ClientData clientdata,
		       Tcl_Interp * interp, int objc,
		       Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->task.state == EMC_TASK_STATE_ON) {
	    Tcl_SetResult(interp, "on", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "off", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "on")) {
	    sendMachineOn();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "off")) {
	    sendMachineOff();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_machine: need 'on', 'off', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_mode(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	switch (emcStatus->task.mode) {
	case EMC_TASK_MODE_MANUAL:
	    Tcl_SetResult(interp, "manual", TCL_VOLATILE);
	    break;
	case EMC_TASK_MODE_AUTO:
	    Tcl_SetResult(interp, "auto", TCL_VOLATILE);
	    break;
	case EMC_TASK_MODE_MDI:
	    Tcl_SetResult(interp, "mdi", TCL_VOLATILE);
	    break;
	default:
	    Tcl_SetResult(interp, "?", TCL_VOLATILE);
	    break;
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "manual")) {
	    sendManual();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "auto")) {
	    sendAuto();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "mdi")) {
	    sendMdi();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp,
		  "emc_mode: need 'manual', 'auto', 'mdi', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_mist(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->io.coolant.mist == 1) {
	    Tcl_SetResult(interp, "on", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "off", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "on")) {
	    sendMistOn();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "off")) {
	    sendMistOff();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_mist: need 'on', 'off', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_flood(ClientData clientdata,
		     Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->io.coolant.flood == 1) {
	    Tcl_SetResult(interp, "on", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "off", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "on")) {
	    sendFloodOn();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "off")) {
	    sendFloodOff();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_flood: need 'on', 'off', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_lube(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->io.lube.on == 0) {
	    Tcl_SetResult(interp, "off", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "on", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "on")) {
	    sendLubeOn();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "off")) {
	    sendLubeOff();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_lube: need 'on', 'off', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_lube_level(ClientData clientdata,
			  Tcl_Interp * interp, int objc,
			  Tcl_Obj * CONST objv[])
{
    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->io.lube.level == 0) {
	    Tcl_SetResult(interp, "low", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "ok", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_lube_level: need no args", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_spindle(ClientData clientdata,
		       Tcl_Interp * interp, int objc,
		       Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->motion.spindle.increasing > 0) {
	    Tcl_SetResult(interp, "increase", TCL_VOLATILE);
	} else if (emcStatus->motion.spindle.increasing < 0) {
	    Tcl_SetResult(interp, "decrease", TCL_VOLATILE);
	} else if (emcStatus->motion.spindle.direction > 0) {
	    Tcl_SetResult(interp, "forward", TCL_VOLATILE);
	} else if (emcStatus->motion.spindle.direction < 0) {
	    Tcl_SetResult(interp, "reverse", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "off", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "forward")) {
	    sendSpindleForward();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "reverse")) {
	    sendSpindleReverse();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "increase")) {
	    sendSpindleIncrease();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "decrease")) {
	    sendSpindleDecrease();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "constant")) {
	    sendSpindleConstant();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "off")) {
	    sendSpindleOff();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_spindle: need 'on', 'off', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_brake(ClientData clientdata,
		     Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	if (emcStatus->motion.spindle.brake == 1) {
	    Tcl_SetResult(interp, "on", TCL_VOLATILE);
	} else {
	    Tcl_SetResult(interp, "off", TCL_VOLATILE);
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "on")) {
	    sendBrakeEngage();
	    return TCL_OK;
	}
	if (!strcmp(objstr, "off")) {
	    sendBrakeRelease();
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "emc_brake: need 'on', 'off', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_tool(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    Tcl_Obj *toolobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_tool: need no args", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    toolobj = Tcl_NewIntObj(emcStatus->io.tool.toolInSpindle);

    Tcl_SetObjResult(interp, toolobj);
    return TCL_OK;
}

static int emc_tool_offset(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    Tcl_Obj *tlobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_tool_offset: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    tlobj =
	Tcl_NewDoubleObj(convertLinearUnits
			 (emcStatus->task.toolOffset.tran.z));

    Tcl_SetObjResult(interp, tlobj);
    return TCL_OK;
}

static int emc_load_tool_table(ClientData clientdata,
			       Tcl_Interp * interp, int objc,
			       Tcl_Obj * CONST objv[])
{
    if (objc != 2) {
	Tcl_SetResult(interp, "emc_load_tool_table: need file",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != sendLoadToolTable(Tcl_GetStringFromObj(objv[1], 0))) {
	Tcl_SetResult(interp, "emc_load_tool_table: can't open file",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_set_tool_offset(ClientData clientdata,
			       Tcl_Interp * interp, int objc,
			       Tcl_Obj * CONST objv[])
{
    int tool;
    double length;
    double diameter;

    if (objc != 4) {
	Tcl_SetResult(interp,
		      "emc_set_tool_offset: need <tool> <length> <diameter>",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &tool)) {
	Tcl_SetResult(interp,
		      "emc_set_tool_offset: need tool as integer, 0..",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    if (0 != Tcl_GetDoubleFromObj(0, objv[2], &length)) {
	Tcl_SetResult(interp,
		      "emc_set_tool_offset: need length as real number",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    if (0 != Tcl_GetDoubleFromObj(0, objv[3], &diameter)) {
	Tcl_SetResult(interp,
		      "emc_set_tool_offset: need diameter as real number",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != sendToolSetOffset(tool, length, diameter)) {
	Tcl_SetResult(interp, "emc_set_tool_offset: can't set it",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_abs_cmd_pos(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int axis;
    Tcl_Obj *posobj;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_abs_cmd_pos: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	if (axis == 0) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    position.tran.x));
	} else if (axis == 1) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    position.tran.y));
	} else if (axis == 2) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    position.tran.z));
	} else {
	    if (axis == 3) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.a));
	    } else if (axis == 4) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.b));
	    } else if (axis == 5) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.c));
	    } else if (axis == 6) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.u));
	    } else if (axis == 7) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.v));
	    } else if (axis == 8) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.w));
	    } else {
		posobj = Tcl_NewDoubleObj(0.0);
	    }
	}
    } else {
	Tcl_SetResult(interp, "emc_abs_cmd_pos: bad integer argument",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, posobj);
    return TCL_OK;
}

static int emc_abs_act_pos(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int axis;
    Tcl_Obj *posobj;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_abs_act_pos: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	if (axis == 0) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    actualPosition.tran.
						    x));
	} else if (axis == 1) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    actualPosition.tran.
						    y));
	} else if (axis == 2) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    actualPosition.tran.
						    z));
	} else {
	    if (axis == 3) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.a));
	    } else if (axis == 4) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.b));
	    } else if (axis == 5) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.c));
	    } else if (axis == 6) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.u));
	    } else if (axis == 7) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.v));
	    } else if (axis == 8) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.w));
	    } else {
		posobj = Tcl_NewDoubleObj(0.0);
	    }
	}
    } else {
	Tcl_SetResult(interp, "emc_abs_act_pos: bad integer argument",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, posobj);
    return TCL_OK;
}

static int emc_rel_cmd_pos(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int axis;
    Tcl_Obj *posobj;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_rel_cmd_pos: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	if (axis == 0) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    position.tran.x -
						    emcStatus->task.origin.
						    tran.x));
	} else if (axis == 1) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    position.tran.y -
						    emcStatus->task.origin.
						    tran.y));
	} else if (axis == 2) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    position.tran.z -
						    emcStatus->task.origin.
						    tran.z -
						    emcStatus->task.
						    toolOffset.tran.z));
	} else {
	    /*! \todo FIXME-- no rotational offsets yet */
	    if (axis == 3) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.a -
							emcStatus->task.
							origin.a));
	    } else if (axis == 4) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.b -
							emcStatus->task.
							origin.b));
	    } else if (axis == 5) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.c -
							emcStatus->task.
							origin.c));
	    } else if (axis == 6) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.u - 
							emcStatus->task.
							origin.u));
	    } else if (axis == 7) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.v - 
							emcStatus->task.
							origin.v));
	    } else if (axis == 8) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.position.w - 
							emcStatus->task.
							origin.w));
	    } else {
		posobj = Tcl_NewDoubleObj(0.0);
	    }
	}
    } else {
	Tcl_SetResult(interp, "emc_rel_cmd_pos: bad integer argument",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, posobj);
    return TCL_OK;
}

static int emc_rel_act_pos(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int axis;
    Tcl_Obj *posobj;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_rel_act_pos: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	if (axis == 0) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    actualPosition.tran.x -
						    emcStatus->task.origin.
						    tran.x));
	} else if (axis == 1) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    actualPosition.tran.y -
						    emcStatus->task.origin.
						    tran.y));
	} else if (axis == 2) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    actualPosition.tran.z -
						    emcStatus->task.origin.
						    tran.z -
						    emcStatus->task.
						    toolOffset.tran.z));
	} else {
	    if (axis == 3) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.a -
							emcStatus->task.
							origin.a));
	    } else if (axis == 4) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.b -
							emcStatus->task.
							origin.b));
	    } else if (axis == 5) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.c -
							emcStatus->task.
							origin.c));
	    } else if (axis == 6) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.u -
							emcStatus->task.
							origin.u));
	    } else if (axis == 7) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.v -
							emcStatus->task.
							origin.v));
	    } else if (axis == 8) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							actualPosition.w -
							emcStatus->task.
							origin.w));
	    } else {
		posobj = Tcl_NewDoubleObj(0.0);
	    }
	}
    } else {
	Tcl_SetResult(interp, "emc_rel_act_pos: bad integer argument",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, posobj);
    return TCL_OK;
}

static int emc_joint_pos(ClientData clientdata,
			 Tcl_Interp * interp, int objc,
			 Tcl_Obj * CONST objv[])
{
    int axis;
    Tcl_Obj *posobj;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_joint_pos: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	posobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].input);
    } else {
	Tcl_SetResult(interp, "emc_joint_pos: bad integer argument",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, posobj);
    return TCL_OK;
}

static int emc_pos_offset(ClientData clientdata,
			  Tcl_Interp * interp, int objc,
			  Tcl_Obj * CONST objv[])
{
    char string[256];
    Tcl_Obj *posobj;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_pos_offset: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    strcpy(string, Tcl_GetStringFromObj(objv[1], 0));

    if (string[0] == 'X') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits
			     (emcStatus->task.origin.tran.x));
    } else if (string[0] == 'Y') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits
			     (emcStatus->task.origin.tran.y));
    } else if (string[0] == 'Z') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits
			     (emcStatus->task.origin.tran.z));
    } else if (string[0] == 'A') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->task.origin.a));
    } else if (string[0] == 'B') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->task.origin.b));
    } else if (string[0] == 'C') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->task.origin.c));
    } else if (string[0] == 'U') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->task.origin.u));
    } else if (string[0] == 'V') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->task.origin.v));
    } else if (string[0] == 'W') {
	posobj =
	    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->task.origin.w));
    } else {
	Tcl_SetResult(interp, "emc_pos_offset: bad integer argument",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, posobj);
    return TCL_OK;
}

static int emc_joint_limit(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int joint;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_joint_limit: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &joint)) {
	if (joint < 0 || joint >= EMC_AXIS_MAX) {
	    Tcl_SetResult(interp,
			  "emc_joint_limit: joint out of bounds",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}

	if (emcStatus->motion.axis[joint].minHardLimit) {
	    Tcl_SetResult(interp, "minhard", TCL_VOLATILE);
	    return TCL_OK;
	} else if (emcStatus->motion.axis[joint].minSoftLimit) {
	    Tcl_SetResult(interp, "minsoft", TCL_VOLATILE);
	    return TCL_OK;
	} else if (emcStatus->motion.axis[joint].maxSoftLimit) {
	    Tcl_SetResult(interp, "maxsoft", TCL_VOLATILE);
	    return TCL_OK;
	} else if (emcStatus->motion.axis[joint].maxHardLimit) {
	    Tcl_SetResult(interp, "maxsoft", TCL_VOLATILE);
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, "ok", TCL_VOLATILE);
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp,
		  "emc_joint_limit: joint out of bounds", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_joint_fault(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int joint;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_joint_fault: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &joint)) {
	if (joint < 0 || joint >= EMC_AXIS_MAX) {
	    Tcl_SetResult(interp,
			  "emc_joint_fault: joint out of bounds",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}

	if (emcStatus->motion.axis[joint].fault) {
	    Tcl_SetResult(interp, "fault", TCL_VOLATILE);
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, "ok", TCL_VOLATILE);
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp,
		  "emc_joint_fault: joint out of bounds", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_override_limit(ClientData clientdata,
			      Tcl_Interp * interp, int objc,
			      Tcl_Obj * CONST objv[])
{
    Tcl_Obj *obj;
    int on;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	// motion overrides all axes at same time, so just reference index 0
	obj = Tcl_NewIntObj(emcStatus->motion.axis[0].overrideLimits);
	Tcl_SetObjResult(interp, obj);
	return TCL_OK;
    }

    if (objc == 2) {
	if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &on)) {
	    if (on) {
		if (0 != sendOverrideLimits(0)) {
		    Tcl_SetResult(interp,
				  "emc_override_limit: can't send command",
				  TCL_VOLATILE);
		    return TCL_OK;
		}
	    } else {
		if (0 != sendOverrideLimits(-1)) {
		    Tcl_SetResult(interp,
				  "emc_override_limit: can't send command",
				  TCL_VOLATILE);
		    return TCL_OK;
		}
	    }
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, "emc_override_limit: need 0 or 1",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
    }

    Tcl_SetResult(interp, "emc_override_limit: need no args, 0 or 1",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_joint_homed(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int joint;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_joint_homed: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &joint)) {
	if (joint < 0 || joint >= EMC_AXIS_MAX) {
	    Tcl_SetResult(interp,
			  "emc_joint_homed: joint out of bounds",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}

	if (emcStatus->motion.axis[joint].homed) {
	    Tcl_SetResult(interp, "homed", TCL_VOLATILE);
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, "not", TCL_VOLATILE);
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp,
		  "emc_joint_homed: joint out of bounds", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_mdi(ClientData clientdata,
		   Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    char string[256];
    int t;

    if (objc < 2) {
	Tcl_SetResult(interp, "emc_mdi: need command", TCL_VOLATILE);
	return TCL_ERROR;
    }
    // bug-- check for string overflow
    strcpy(string, Tcl_GetStringFromObj(objv[1], 0));
    for (t = 2; t < objc; t++) {
	strcat(string, " ");
	strcat(string, Tcl_GetStringFromObj(objv[t], 0));
    }

    if (0 != sendMdiCmd(string)) {
	Tcl_SetResult(interp, "emc_mdi: error executing command",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_home(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int axis;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_home: need axis", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	sendHome(axis);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_home: need axis as integer, 0..",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_unhome(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int axis;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_unhome: need axis", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	sendUnHome(axis);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_unhome: need axis as integer, 0..",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_jog_stop(ClientData clientdata,
			Tcl_Interp * interp, int objc,
			Tcl_Obj * CONST objv[])
{
    int axis;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_jog_stop: need axis", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis)) {
	Tcl_SetResult(interp, "emc_jog_stop: need axis as integer, 0..",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != sendJogStop(axis)) {
	Tcl_SetResult(interp, "emc_jog_stop: can't send jog stop msg",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_jog(ClientData clientdata,
		   Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int axis;
    double speed;

    if (objc != 3) {
	Tcl_SetResult(interp, "emc_jog: need axis and speed",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis)) {
	Tcl_SetResult(interp, "emc_jog: need axis as integer, 0..",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    if (0 != Tcl_GetDoubleFromObj(0, objv[2], &speed)) {
	Tcl_SetResult(interp, "emc_jog: need speed as real number",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != sendJogCont(axis, speed)) {
	Tcl_SetResult(interp, "emc_jog: can't jog", TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_jog_incr(ClientData clientdata,
			Tcl_Interp * interp, int objc,
			Tcl_Obj * CONST objv[])
{
    int axis;
    double speed;
    double incr;

    if (objc != 4) {
	Tcl_SetResult(interp,
		      "emc_jog_incr: need axis, speed, and increment",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis)) {
	Tcl_SetResult(interp, "emc_jog_incr: need axis as integer, 0..",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    if (0 != Tcl_GetDoubleFromObj(0, objv[2], &speed)) {
	Tcl_SetResult(interp, "emc_jog_incr: need speed as real number",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    if (0 != Tcl_GetDoubleFromObj(0, objv[3], &incr)) {
	Tcl_SetResult(interp,
		      "emc_jog_incr: need increment as real number",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != sendJogIncr(axis, speed, incr)) {
	Tcl_SetResult(interp, "emc_jog_incr: can't jog", TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_feed_override(ClientData clientdata,
			     Tcl_Interp * interp, int objc,
			     Tcl_Obj * CONST objv[])
{
    Tcl_Obj *feedobj;
    int percent;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	feedobj =
	    Tcl_NewIntObj((int)
			  (emcStatus->motion.traj.scale * 100.0 + 0.5));
	Tcl_SetObjResult(interp, feedobj);
	return TCL_OK;
    }

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_feed_override: need percent",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &percent)) {
	sendFeedOverride(((double) percent) / 100.0);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_feed_override: need percent", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_spindle_override(ClientData clientdata,
			     Tcl_Interp * interp, int objc,
			     Tcl_Obj * CONST objv[])
{
    Tcl_Obj *feedobj;
    int percent;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	feedobj =
	    Tcl_NewIntObj((int)
			  (emcStatus->motion.traj.spindle_scale * 100.0 + 0.5));
	Tcl_SetObjResult(interp, feedobj);
	return TCL_OK;
    }

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_spindle_override: need percent",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &percent)) {
	sendSpindleOverride(((double) percent) / 100.0);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_spindle_override: need percent", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_task_plan_init(ClientData clientdata,
			      Tcl_Interp * interp, int objc,
			      Tcl_Obj * CONST objv[])
{
    if (0 != sendTaskPlanInit()) {
	Tcl_SetResult(interp, "emc_task_plan_init: can't init interpreter",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_open(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 2) {
	Tcl_SetResult(interp, "emc_open: need file", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != sendProgramOpen(Tcl_GetStringFromObj(objv[1], 0))) {
	Tcl_SetResult(interp, "emc_open: can't open file", TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_run(ClientData clientdata,
		   Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int line;

    if (objc == 1) {
	if (0 != sendProgramRun(0)) {
	    Tcl_SetResult(interp, "emc_run: can't execute program",
			  TCL_VOLATILE);
	    return TCL_OK;
	}
    }

    if (objc == 2) {
	if (0 != Tcl_GetIntFromObj(0, objv[1], &line)) {
	    Tcl_SetResult(interp, "emc_run: need integer start line",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	if (0 != sendProgramRun(line)) {
	    Tcl_SetResult(interp, "emc_run: can't execute program",
			  TCL_VOLATILE);
	    return TCL_OK;
	}
    }

    return TCL_OK;
}

static int emc_pause(ClientData clientdata,
		     Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (0 != sendProgramPause()) {
	Tcl_SetResult(interp, "emc_pause: can't pause program",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_optional_stop(ClientData clientdata,
			      Tcl_Interp * interp, int objc,
			      Tcl_Obj * CONST objv[])
{
    Tcl_Obj *obj;
    int on;

    if (objc == 1) {
	// no arg-- return status
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	// get the current state from the status
	obj = Tcl_NewIntObj(emcStatus->task.optional_stop_state);
	Tcl_SetObjResult(interp, obj);
	return TCL_OK;
    }

    if (objc == 2) {
	if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &on)) {
	    if (0 != sendSetOptionalStop(on)) {
		    Tcl_SetResult(interp,
				  "emc_optional_stop: can't send command",
				  TCL_VOLATILE);
		    return TCL_OK;
	    }
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, "emc_optional_stop: need 0 or 1",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
    }

    Tcl_SetResult(interp, "emc_optional_stop: need no args, 0 or 1",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_resume(ClientData clientdata,
		      Tcl_Interp * interp, int objc,
		      Tcl_Obj * CONST objv[])
{
    if (0 != sendProgramResume()) {
	Tcl_SetResult(interp, "emc_resume: can't resume program",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_step(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (0 != sendProgramStep()) {
	Tcl_SetResult(interp, "emc_step: can't step program",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_abort(ClientData clientdata,
		     Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (0 != sendAbort()) {
	Tcl_SetResult(interp, "emc_abort: can't execute program",
		      TCL_VOLATILE);
	return TCL_OK;
    }

    return TCL_OK;
}

static int emc_program(ClientData clientdata,
		       Tcl_Interp * interp, int objc,
		       Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_program: need no args", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (0 != emcStatus->task.file[0]) {
	Tcl_SetResult(interp, emcStatus->task.file, TCL_VOLATILE);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "none", TCL_VOLATILE);
    return TCL_OK;
}

static int emc_program_status(ClientData clientdata,
			      Tcl_Interp * interp, int objc,
			      Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_program_status: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    switch (emcStatus->task.interpState) {
    case EMC_TASK_INTERP_READING:
    case EMC_TASK_INTERP_WAITING:
	Tcl_SetResult(interp, "running", TCL_VOLATILE);
	return TCL_OK;
	break;

    case EMC_TASK_INTERP_PAUSED:
	Tcl_SetResult(interp, "paused", TCL_VOLATILE);
	return TCL_OK;
	break;

    default:
	Tcl_SetResult(interp, "idle", TCL_VOLATILE);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "idle", TCL_VOLATILE);
    return TCL_OK;
}

static int emc_program_line(ClientData clientdata,
			    Tcl_Interp * interp, int objc,
			    Tcl_Obj * CONST objv[])
{
    Tcl_Obj *lineobj;
    int programActiveLine = 0;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_program_line: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (programStartLine < 0
	|| emcStatus->task.readLine < programStartLine) {
	// controller is skipping lines
	programActiveLine = emcStatus->task.readLine;
    } else {			// controller is not skipping lines
	if (emcStatus->task.currentLine > 0) {
	    if (emcStatus->task.motionLine > 0 &&
		emcStatus->task.motionLine < emcStatus->task.currentLine) {
		// active line is the motion line, which lags
		programActiveLine = emcStatus->task.motionLine;
	    } else {
		// active line is the current line-- no motion lag
		programActiveLine = emcStatus->task.currentLine;
	    }
	} else {
	    // no active line at all
	    programActiveLine = 0;
	}
    }				// end of else controller is not skipping
    // lines

    lineobj = Tcl_NewIntObj(programActiveLine);

    Tcl_SetObjResult(interp, lineobj);
    return TCL_OK;
}

static int emc_program_codes(ClientData clientdata,
			     Tcl_Interp * interp, int objc,
			     Tcl_Obj * CONST objv[])
{
    char codes_string[256];
    char string[256];
    int t;
    int code;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_program_codes: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }
    // fill in the active G codes
    codes_string[0] = 0;
    for (t = 1; t < ACTIVE_G_CODES; t++) {
	code = emcStatus->task.activeGCodes[t];
	if (code == -1) {
	    continue;
	}
	if (code % 10) {
	    sprintf(string, "G%.1f ", (double) code / 10.0);
	} else {
	    sprintf(string, "G%d ", code / 10);
	}
	strcat(codes_string, string);
    }

    // fill in the active M codes, settings too
    for (t = 1; t < ACTIVE_M_CODES; t++) {
	code = emcStatus->task.activeMCodes[t];
	if (code == -1) {
	    continue;
	}
	sprintf(string, "M%d ", code);
	strcat(codes_string, string);
    }

    // fill in F and S codes also
    sprintf(string, "F%.0f ", emcStatus->task.activeSettings[1]);
    strcat(codes_string, string);
    sprintf(string, "S%.0f", fabs(emcStatus->task.activeSettings[2]));
    strcat(codes_string, string);

    Tcl_SetResult(interp, codes_string, TCL_VOLATILE);
    return TCL_OK;
}

static int emc_joint_type(ClientData clientdata,
			  Tcl_Interp * interp, int objc,
			  Tcl_Obj * CONST objv[])
{
    int joint;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_joint_type: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &joint)) {
	if (joint < 0 || joint >= EMC_AXIS_MAX) {
	    Tcl_SetResult(interp,
			  "emc_joint_type: joint out of bounds",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}

	switch (emcStatus->motion.axis[joint].axisType) {
	case EMC_AXIS_LINEAR:
	    Tcl_SetResult(interp, "linear", TCL_VOLATILE);
	    break;
	case EMC_AXIS_ANGULAR:
	    Tcl_SetResult(interp, "angular", TCL_VOLATILE);
	    break;
	default:
	    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	    break;
	}

	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_joint_type: invalid joint number",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_joint_units(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int joint;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_joint_units: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &joint)) {
	if (joint < 0 || joint >= EMC_AXIS_MAX) {
	    Tcl_SetResult(interp,
			  "emc_joint_units: joint out of bounds",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}

	switch (emcStatus->motion.axis[joint].axisType) {
	case EMC_AXIS_LINEAR:
	    /* try mm */
	    if (CLOSE(emcStatus->motion.axis[joint].units, 1.0,
		      LINEAR_CLOSENESS)) {
		Tcl_SetResult(interp, "mm", TCL_VOLATILE);
		return TCL_OK;
	    }
	    /* now try inch */
	    else if (CLOSE
		     (emcStatus->motion.axis[joint].units, INCH_PER_MM,
		      LINEAR_CLOSENESS)) {
		Tcl_SetResult(interp, "inch", TCL_VOLATILE);
		return TCL_OK;
	    }
	    /* now try cm */
	    else if (CLOSE(emcStatus->motion.axis[joint].units, CM_PER_MM,
			   LINEAR_CLOSENESS)) {
		Tcl_SetResult(interp, "cm", TCL_VOLATILE);
		return TCL_OK;
	    }
	    /* else it's custom */
	    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	    return TCL_OK;
	    break;

	case EMC_AXIS_ANGULAR:
	    /* try degrees */
	    if (CLOSE(emcStatus->motion.axis[joint].units, 1.0,
		      ANGULAR_CLOSENESS)) {
		Tcl_SetResult(interp, "deg", TCL_VOLATILE);
		return TCL_OK;
	    }
	    /* now try radians */
	    else if (CLOSE
		     (emcStatus->motion.axis[joint].units, RAD_PER_DEG,
		      ANGULAR_CLOSENESS)) {
		Tcl_SetResult(interp, "rad", TCL_VOLATILE);
		return TCL_OK;
	    }
	    /* now try grads */
	    else if (CLOSE
		     (emcStatus->motion.axis[joint].units, GRAD_PER_DEG,
		      ANGULAR_CLOSENESS)) {
		Tcl_SetResult(interp, "grad", TCL_VOLATILE);
		return TCL_OK;
	    }
	    /* else it's custom */
	    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	    return TCL_OK;
	    break;

	default:
	    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	    return TCL_OK;
	    break;
	}
    }

    Tcl_SetResult(interp, "emc_joint_units: invalid joint number",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_program_linear_units(ClientData clientdata,
				    Tcl_Interp * interp, int objc,
				    Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_program_linear_units: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    switch (emcStatus->task.programUnits) {
    case CANON_UNITS_INCHES:
	Tcl_SetResult(interp, "inch", TCL_VOLATILE);
	return TCL_OK;
	break;

    case CANON_UNITS_MM:
	Tcl_SetResult(interp, "mm", TCL_VOLATILE);
	return TCL_OK;
	break;

    case CANON_UNITS_CM:
	Tcl_SetResult(interp, "cm", TCL_VOLATILE);
	return TCL_OK;
	break;

    default:
	Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	return TCL_OK;
	break;
    }

    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
    return TCL_OK;
}

static int emc_program_angular_units(ClientData clientdata,
				     Tcl_Interp * interp, int objc,
				     Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_program_angular_units: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }
    // currently the EMC doesn't have separate program angular units, so
    // these are simply "deg"
    Tcl_SetResult(interp, "deg", TCL_VOLATILE);
    return TCL_OK;
}

static int emc_user_linear_units(ClientData clientdata,
				 Tcl_Interp * interp, int objc,
				 Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_user_linear_units: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    /* try mm */
    if (CLOSE(emcStatus->motion.traj.linearUnits, 1.0, LINEAR_CLOSENESS)) {
	Tcl_SetResult(interp, "mm", TCL_VOLATILE);
	return TCL_OK;
    }
    /* now try inch */
    else if (CLOSE(emcStatus->motion.traj.linearUnits, INCH_PER_MM,
		   LINEAR_CLOSENESS)) {
	Tcl_SetResult(interp, "inch", TCL_VOLATILE);
	return TCL_OK;
    }
    /* now try cm */
    else if (CLOSE(emcStatus->motion.traj.linearUnits, CM_PER_MM,
		   LINEAR_CLOSENESS)) {
	Tcl_SetResult(interp, "cm", TCL_VOLATILE);
	return TCL_OK;
    }

    /* else it's custom */
    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
    return TCL_OK;
}

static int emc_user_angular_units(ClientData clientdata,
				  Tcl_Interp * interp, int objc,
				  Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_user_angular_units: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    /* try degrees */
    if (CLOSE(emcStatus->motion.traj.angularUnits, 1.0, ANGULAR_CLOSENESS)) {
	Tcl_SetResult(interp, "deg", TCL_VOLATILE);
	return TCL_OK;
    }
    /* now try radians */
    else if (CLOSE(emcStatus->motion.traj.angularUnits, RAD_PER_DEG,
		   ANGULAR_CLOSENESS)) {
	Tcl_SetResult(interp, "rad", TCL_VOLATILE);
	return TCL_OK;
    }
    /* now try grads */
    else if (CLOSE(emcStatus->motion.traj.angularUnits, GRAD_PER_DEG,
		   ANGULAR_CLOSENESS)) {
	Tcl_SetResult(interp, "grad", TCL_VOLATILE);
	return TCL_OK;
    }

    /* else it's an abitrary number, so just return it */
    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
    return TCL_OK;
}

static int emc_display_linear_units(ClientData clientdata,
				    Tcl_Interp * interp, int objc,
				    Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_display_linear_units: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    switch (linearUnitConversion) {
    case LINEAR_UNITS_INCH:
	Tcl_SetResult(interp, "inch", TCL_VOLATILE);
	break;
    case LINEAR_UNITS_MM:
	Tcl_SetResult(interp, "mm", TCL_VOLATILE);
	break;
    case LINEAR_UNITS_CM:
	Tcl_SetResult(interp, "cm", TCL_VOLATILE);
	break;
    case LINEAR_UNITS_AUTO:
	switch (emcStatus->task.programUnits) {
	case CANON_UNITS_MM:
	    Tcl_SetResult(interp, "(mm)", TCL_VOLATILE);
	    break;
	case CANON_UNITS_INCHES:
	    Tcl_SetResult(interp, "(inch)", TCL_VOLATILE);
	    break;
	case CANON_UNITS_CM:
	    Tcl_SetResult(interp, "(cm)", TCL_VOLATILE);
	    break;
	}
	break;
    default:
	Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	break;
    }

    return TCL_OK;
}

static int emc_display_angular_units(ClientData clientdata,
				     Tcl_Interp * interp, int objc,
				     Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_display_angular_units: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    switch (angularUnitConversion) {
    case ANGULAR_UNITS_DEG:
	Tcl_SetResult(interp, "deg", TCL_VOLATILE);
	break;
    case ANGULAR_UNITS_RAD:
	Tcl_SetResult(interp, "rad", TCL_VOLATILE);
	break;
    case ANGULAR_UNITS_GRAD:
	Tcl_SetResult(interp, "grad", TCL_VOLATILE);
	break;
    case ANGULAR_UNITS_AUTO:
	Tcl_SetResult(interp, "(deg)", TCL_VOLATILE);	/*! \todo FIXME-- always deg? */
	break;
    default:
	Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	break;
    }

    return TCL_OK;
}

static int emc_linear_unit_conversion(ClientData clientdata,
				      Tcl_Interp * interp, int objc,
				      Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return unit setting
	switch (linearUnitConversion) {
	case LINEAR_UNITS_INCH:
	    Tcl_SetResult(interp, "inch", TCL_VOLATILE);
	    break;
	case LINEAR_UNITS_MM:
	    Tcl_SetResult(interp, "mm", TCL_VOLATILE);
	    break;
	case LINEAR_UNITS_CM:
	    Tcl_SetResult(interp, "cm", TCL_VOLATILE);
	    break;
	case LINEAR_UNITS_AUTO:
	    Tcl_SetResult(interp, "auto", TCL_VOLATILE);
	    break;
	default:
	    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	    break;
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "inch")) {
	    linearUnitConversion = LINEAR_UNITS_INCH;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "mm")) {
	    linearUnitConversion = LINEAR_UNITS_MM;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "cm")) {
	    linearUnitConversion = LINEAR_UNITS_CM;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "auto")) {
	    linearUnitConversion = LINEAR_UNITS_AUTO;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "custom")) {
	    linearUnitConversion = LINEAR_UNITS_CUSTOM;
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp,
		  "emc_linear_unit_conversion: need 'inch', 'mm', 'cm', 'auto', 'custom', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_angular_unit_conversion(ClientData clientdata,
				       Tcl_Interp * interp, int objc,
				       Tcl_Obj * CONST objv[])
{
    char *objstr;

    if (objc == 1) {
	// no arg-- return unit setting
	switch (angularUnitConversion) {
	case ANGULAR_UNITS_DEG:
	    Tcl_SetResult(interp, "deg", TCL_VOLATILE);
	    break;
	case ANGULAR_UNITS_RAD:
	    Tcl_SetResult(interp, "rad", TCL_VOLATILE);
	    break;
	case ANGULAR_UNITS_GRAD:
	    Tcl_SetResult(interp, "grad", TCL_VOLATILE);
	    break;
	case ANGULAR_UNITS_AUTO:
	    Tcl_SetResult(interp, "auto", TCL_VOLATILE);
	    break;
	default:
	    Tcl_SetResult(interp, "custom", TCL_VOLATILE);
	    break;
	}
	return TCL_OK;
    }

    if (objc == 2) {
	objstr = Tcl_GetStringFromObj(objv[1], 0);
	if (!strcmp(objstr, "deg")) {
	    angularUnitConversion = ANGULAR_UNITS_DEG;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "rad")) {
	    angularUnitConversion = ANGULAR_UNITS_RAD;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "grad")) {
	    angularUnitConversion = ANGULAR_UNITS_GRAD;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "auto")) {
	    angularUnitConversion = ANGULAR_UNITS_AUTO;
	    return TCL_OK;
	}
	if (!strcmp(objstr, "custom")) {
	    angularUnitConversion = ANGULAR_UNITS_CUSTOM;
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp,
		  "emc_angular_unit_conversion: need 'deg', 'rad', 'grad', 'auto', 'custom', or no args",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_task_heartbeat(ClientData clientdata,
			      Tcl_Interp * interp, int objc,
			      Tcl_Obj * CONST objv[])
{
    Tcl_Obj *hbobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_task_heartbeat: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    hbobj = Tcl_NewIntObj(emcStatus->task.heartbeat);

    Tcl_SetObjResult(interp, hbobj);
    return TCL_OK;
}

static int emc_task_command(ClientData clientdata,
			    Tcl_Interp * interp, int objc,
			    Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_task_command: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandobj = Tcl_NewIntObj(emcStatus->task.command_type);

    Tcl_SetObjResult(interp, commandobj);
    return TCL_OK;
}

static int emc_task_command_number(ClientData clientdata,
				   Tcl_Interp * interp, int objc,
				   Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandnumber;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_task_command_number: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandnumber = Tcl_NewIntObj(emcStatus->task.echo_serial_number);

    Tcl_SetObjResult(interp, commandnumber);
    return TCL_OK;
}

static int emc_task_command_status(ClientData clientdata,
				   Tcl_Interp * interp, int objc,
				   Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandstatus;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_task_command_status: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandstatus = Tcl_NewIntObj(emcStatus->task.status);

    Tcl_SetObjResult(interp, commandstatus);
    return TCL_OK;
}

static int emc_io_heartbeat(ClientData clientdata,
			    Tcl_Interp * interp, int objc,
			    Tcl_Obj * CONST objv[])
{
    Tcl_Obj *hbobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_heartbeat: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    hbobj = Tcl_NewIntObj(emcStatus->io.heartbeat);

    Tcl_SetObjResult(interp, hbobj);
    return TCL_OK;
}

static int emc_io_command(ClientData clientdata,
			  Tcl_Interp * interp, int objc,
			  Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_command: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandobj = Tcl_NewIntObj(emcStatus->io.command_type);

    Tcl_SetObjResult(interp, commandobj);
    return TCL_OK;
}

static int emc_io_command_number(ClientData clientdata,
				 Tcl_Interp * interp, int objc,
				 Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandnumber;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_command_number: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandnumber = Tcl_NewIntObj(emcStatus->io.echo_serial_number);

    Tcl_SetObjResult(interp, commandnumber);
    return TCL_OK;
}

static int emc_io_command_status(ClientData clientdata,
				 Tcl_Interp * interp, int objc,
				 Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandstatus;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_command_status: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandstatus = Tcl_NewIntObj(emcStatus->io.status);

    Tcl_SetObjResult(interp, commandstatus);
    return TCL_OK;
}

static int emc_motion_heartbeat(ClientData clientdata,
				Tcl_Interp * interp, int objc,
				Tcl_Obj * CONST objv[])
{
    Tcl_Obj *hbobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_motion_heartbeat: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    hbobj = Tcl_NewIntObj(emcStatus->motion.heartbeat);

    Tcl_SetObjResult(interp, hbobj);
    return TCL_OK;
}

static int emc_motion_command(ClientData clientdata,
			      Tcl_Interp * interp, int objc,
			      Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandobj;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_motion_command: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandobj = Tcl_NewIntObj(emcStatus->motion.command_type);

    Tcl_SetObjResult(interp, commandobj);
    return TCL_OK;
}

static int emc_motion_command_number(ClientData clientdata,
				     Tcl_Interp * interp, int objc,
				     Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandnumber;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_motion_command_number: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandnumber = Tcl_NewIntObj(emcStatus->motion.echo_serial_number);

    Tcl_SetObjResult(interp, commandnumber);
    return TCL_OK;
}

static int emc_motion_command_status(ClientData clientdata,
				     Tcl_Interp * interp, int objc,
				     Tcl_Obj * CONST objv[])
{
    Tcl_Obj *commandstatus;

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_motion_command_status: need no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    commandstatus = Tcl_NewIntObj(emcStatus->motion.status);

    Tcl_SetObjResult(interp, commandstatus);
    return TCL_OK;
}

static int emc_axis_backlash(ClientData clientdata,
			     Tcl_Interp * interp, int objc,
			     Tcl_Obj * CONST objv[])
{
    Tcl_Obj *valobj;
    int axis;
    double backlash;

    // syntax is emc_axis_backlash <axis> {<backlash>}
    // if <backlash> is not specified, returns current value,
    // otherwise, sets backlash to specified value

    // check number of args supplied
    if ((objc < 2) || (objc > 3)) {
	Tcl_SetResult(interp,
		      "emc_axis_backlash: need <axis> {<backlash>}",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    // get axis number
    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis) ||
	axis < 0 || axis >= EMC_AXIS_MAX) {
	Tcl_SetResult(interp,
		      "emc_axis_backlash: need axis as integer, 0..EMC_AXIS_MAX-1",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    // test for get or set
    if (objc == 2) {
	// want to get present value
	valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].backlash);
	Tcl_SetObjResult(interp, valobj);
	return TCL_OK;
    } else {
	// want to set new value
	if (0 != Tcl_GetDoubleFromObj(0, objv[2], &backlash)) {
	    Tcl_SetResult(interp,
			  "emc_axis_backlash: need backlash as real number",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	// write it out
	sendAxisSetBacklash(axis, backlash);
	return TCL_OK;
    }
}

static int emc_axis_enable(ClientData clientdata,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[])
{
    int axis;
    int val;
    Tcl_Obj *enobj;

    // syntax is emc_axis_output <axis> {0 | 1}

    if (objc < 2) {
	Tcl_SetResult(interp, "emc_axis_enable: need <axis>",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis) ||
	axis < 0 || axis >= EMC_AXIS_MAX) {
	Tcl_SetResult(interp,
		      "emc_axis_enable: need axis as integer, 0..EMC_AXIS_MAX-1",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (objc == 2) {
	if (emcUpdateType == EMC_UPDATE_AUTO) {
	    updateStatus();
	}
	enobj = Tcl_NewIntObj(emcStatus->motion.axis[axis].enabled);
	Tcl_SetObjResult(interp, enobj);
	return TCL_OK;
    }
    // else we were given 0 or 1 to enable/disable it
    if (0 != Tcl_GetIntFromObj(0, objv[2], &val)) {
	Tcl_SetResult(interp,
		      "emc_axis_enable: need 0, 1 for disable, enable",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    sendAxisEnable(axis, val);
    return TCL_OK;
}

static int emc_axis_load_comp(ClientData clientdata,
			      Tcl_Interp * interp, int objc,
			      Tcl_Obj * CONST objv[])
{
    int axis, type;
    char file[256];

    // syntax is emc_axis_load_comp <axis> <file>

    if (objc != 4) {
	Tcl_SetResult(interp, "emc_axis_load_comp: need <axis> <file> <type>",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis) ||
	axis < 0 || axis >= EMC_AXIS_MAX) {
	Tcl_SetResult(interp,
		      "emc_axis_load_comp: need axis as integer, 0..EMC_AXIS_MAX-1",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    // copy objv[1] to file arg, to make sure it's not modified
    strcpy(file, Tcl_GetStringFromObj(objv[2], 0));

    if (0 != Tcl_GetIntFromObj(0, objv[3], &type)) {
	Tcl_SetResult(interp, "emc_axis_load_comp: <type> must be an int",
		      TCL_VOLATILE);
    }

    // now write it out
    sendAxisLoadComp(axis, file, type);
    return TCL_OK;
}

int emc_teleop_enable(ClientData clientdata,
		      Tcl_Interp * interp, int objc,
		      Tcl_Obj * CONST objv[])
{
    int enable;

    if (objc != 1) {
	if (0 != Tcl_GetIntFromObj(0, objv[1], &enable)) {
	    Tcl_SetResult(interp,
			  "emc_teleop_enable: <enable> must be an integer",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	sendSetTeleopEnable(enable);
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    Tcl_SetObjResult(interp,
		     Tcl_NewIntObj(emcStatus->motion.traj.mode ==
				   EMC_TRAJ_MODE_TELEOP));
    return TCL_OK;
}

int emc_kinematics_type(ClientData clientdata,
			Tcl_Interp * interp, int objc,
			Tcl_Obj * CONST objv[])
{

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    Tcl_SetObjResult(interp,
		     Tcl_NewIntObj(emcStatus->motion.traj.
				   kinematics_type));
    return TCL_OK;
}

int emc_probe_clear(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_probe_clear: needs no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(sendClearProbeTrippedFlag()));
    return TCL_OK;
}

int emc_probe_value(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_probe_value: needs no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    Tcl_SetObjResult(interp,
		     Tcl_NewIntObj(emcStatus->motion.traj.probeval));
    return TCL_OK;
}

int emc_probe_tripped(ClientData clientdata,
		      Tcl_Interp * interp, int objc,
		      Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_probe_tripped: needs no args",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    Tcl_SetObjResult(interp,
		     Tcl_NewIntObj(emcStatus->motion.traj.probe_tripped));
    return TCL_OK;
}

int emc_probe_move(ClientData clientdata,
		   Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    double x, y, z;

    if (objc != 4) {
	Tcl_SetResult(interp, "emc_probe_move: <x> <y> <z>", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetDoubleFromObj(0, objv[1], &x)) {
	Tcl_SetResult(interp, "emc_probe_move: <x> must be a double",
		      TCL_VOLATILE);
    }
    if (0 != Tcl_GetDoubleFromObj(0, objv[2], &y)) {
	Tcl_SetResult(interp, "emc_probe_move: <y> must be a double",
		      TCL_VOLATILE);
    }
    if (0 != Tcl_GetDoubleFromObj(0, objv[3], &z)) {
	Tcl_SetResult(interp, "emc_probe_move: <z> must be a double",
		      TCL_VOLATILE);
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(sendProbe(x, y, z)));
    return TCL_OK;
}

static int emc_probed_pos(ClientData clientdata,
			  Tcl_Interp * interp, int objc,
			  Tcl_Obj * CONST objv[])
{
    int axis;
    Tcl_Obj *posobj;

    if (objc != 2) {
	Tcl_SetResult(interp,
		      "emc_probed_pos: need exactly 1 non-negative integer",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	if (axis == 0) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    probedPosition.tran.
						    x));
	} else if (axis == 1) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    probedPosition.tran.
						    y));
	} else if (axis == 2) {
	    posobj =
		Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.traj.
						    probedPosition.tran.
						    z));
	} else {
	    if (axis == 3) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							probedPosition.a));
	    } else if (axis == 4) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							probedPosition.b));
	    } else if (axis == 5) {
		posobj =
		    Tcl_NewDoubleObj(convertLinearUnits(emcStatus->motion.
							traj.
							probedPosition.c));
	    } else {
		posobj = Tcl_NewDoubleObj(0.0);
	    }
	}
    } else {
	Tcl_SetResult(interp, "emc_probed_pos: bad integer argument",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, posobj);
    return TCL_OK;
}

// ********************************************************************
//      Pendant read routine from /dev/psaux, /dev/ttyS0, or /dev/ttyS1
// *********************************************************************

static int emc_pendant(ClientData clientdata,
		       Tcl_Interp * interp, int objc,
		       Tcl_Obj * CONST objv[])
{
    FILE *inFile;

    char inBytes[5];
    const char *port;

    inBytes[0] = 0;
    inBytes[1] = 0;
    inBytes[2] = 0;
    inBytes[3] = 0;
    inBytes[4] = 0;

    if (objc == 2) {
	port = Tcl_GetStringFromObj(objv[1], 0);
	if ((!strcmp(port, "/dev/psaux")) | (!strcmp(port,
						     "/dev/ttyS0")) |
	    (!strcmp(port, "/dev/ttyS1"))) {
	    inFile = fopen(port, "r+b");

	    if (inFile) {
		if (strcmp(port, "/dev/psaux")) {	// For Serial mice
		    inBytes[1] = fgetc(inFile);	// read the first Byte
		    if (inBytes[1] != 77) {	// If first byte not "M"
			fputc(77, inFile);	// Request data resent
			fflush(inFile);
			inBytes[1] = fgetc(inFile);	// and hope it is
			// correct
		    }
		}
		inBytes[4] = fgetc(inFile);	// Status byte
		inBytes[2] = fgetc(inFile);	// Horizontal movement
		inBytes[3] = fgetc(inFile);	// Vertical Movement
	    }
	    fclose(inFile);

	    if (!strcmp(port, "/dev/psaux")) {	// For PS/2
		inBytes[0] = (inBytes[4] & 0x01);	// Left button
		inBytes[1] = (inBytes[4] & 0x02) >> 1;	// Right button
	    } else {		// For serial mice
		inBytes[0] = (inBytes[4] & 0x20) >> 5;	// Left button
		inBytes[1] = (inBytes[4] & 0x10) >> 4;	// Right button
		if (inBytes[4] & 0x02) {
		    inBytes[2] = inBytes[2] | 0xc0;
		}
		if (inBytes[4] & 0x08) {
		    inBytes[3] = inBytes[3] | 0xc0;
		}
	    }

	    sprintf(interp->result, "%i %i %d %d %i", inBytes[0],
		    inBytes[1], inBytes[2], inBytes[3], inBytes[4]);
	    return TCL_OK;
	}
    }
    Tcl_SetResult(interp,
		  "Need /dev/psaux, /dev/ttyS0 or /dev/ttyS1 as Arg",
		  TCL_VOLATILE);
    return TCL_ERROR;
}

// *******************************************************************

// provide some of the extended Tcl builtins not available for various plats
// "int", as in "int 3.9" which returns 3
static int localint(ClientData clientdata,
		    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    double val;
    char resstring[80];

    if (objc != 2) {
	// need exactly one arg
	Tcl_SetResult(interp, "wrong # args: should be \"int value\"",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetDoubleFromObj(0, objv[1], &val)) {
	resstring[0] = 0;
	strcat(resstring, "expected number but got \"");
	strncat(resstring, Tcl_GetStringFromObj(objv[1], 0),
		sizeof(resstring) - strlen(resstring) - 2);
	strcat(resstring, "\"");
	Tcl_SetResult(interp, resstring, TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj((int) val));
    return TCL_OK;
}

static const char *one_head(int x0, int y0, int x1, int y1)
{
    static char buf[100];
    snprintf(buf, sizeof(buf), "%d %d %d %d", x0, y0, x1, y1);
    return buf;
}

// "round", as in "round 3.9" which returns 4
static int localround(ClientData clientdata,
		      Tcl_Interp * interp, int objc,
		      Tcl_Obj * CONST objv[])
{
    double val;
    char resstring[80];

    if (objc != 2) {
	// need exactly one arg
	Tcl_SetResult(interp, "wrong # args: should be \"round value\"",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetDoubleFromObj(0, objv[1], &val)) {
	resstring[0] = 0;
	strcat(resstring, "expected number but got \"");
	strncat(resstring, Tcl_GetStringFromObj(objv[1], 0),
		sizeof(resstring) - strlen(resstring) - 2);
	strcat(resstring, "\"");
	Tcl_SetResult(interp, resstring, TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp,
		     Tcl_NewIntObj(val <
				   0.0 ? (int) (val - 0.5) : (int) (val +
								    0.5)));
    return TCL_OK;
}

#include <X11/extensions/Xinerama.h>

static int multihead(ClientData clientdata,
		      Tcl_Interp * interp, int objc,
		      Tcl_Obj * CONST objv[])
{
    if(objc > 1)
	Tcl_SetResult(interp, "wrong # args: should be \"multihead\"",
		      TCL_VOLATILE);

    Tk_Window tkwin = Tk_MainWindow(interp);
    if(!tkwin) return TCL_ERROR;

    Display *d = Tk_Display(tkwin);
    if(!d) return TCL_ERROR;

    Tcl_ResetResult(interp);

    XineramaScreenInfo *inf = NULL;
    int count = 0;

    int i, j;
    if(XineramaQueryExtension(d, &i, &j)) {
        inf = XineramaQueryScreens(d, &count);
    }

    if( !inf ) {
        Tcl_AppendElement(interp, one_head(0, 0,
                   DisplayWidth(d, DefaultScreen(d)),
                   DisplayHeight(d, DefaultScreen(d))));
    } else {
        for(i=0; i<count; i++) {
            Tcl_AppendElement(interp, one_head(inf[i].x_org, inf[i].y_org,
                        inf[i].x_org + inf[i].width,
                        inf[i].y_org + inf[i].height));
        }
        XFree(inf);
    }
    return TCL_OK;
}

static void sigQuit(int sig)
{
    thisQuit((ClientData) 0);
}

static void initMain()
{
    emcWaitType = EMC_WAIT_RECEIVED;
    emcCommandSerialNumber = 0;
    saveEmcCommandSerialNumber = 0;
    emcTimeout = 0.0;
    emcUpdateType = EMC_UPDATE_AUTO;
    linearUnitConversion = LINEAR_UNITS_AUTO;
    angularUnitConversion = ANGULAR_UNITS_AUTO;
    emcCommandBuffer = 0;
    emcStatusBuffer = 0;
    emcStatus = 0;

    emcErrorBuffer = 0;
    error_string[LINELEN-1] = 0;
    operator_text_string[LINELEN-1] = 0;
    operator_display_string[LINELEN-1] = 0;
    programStartLine = 0;
}

int emc_init(ClientData cd, Tcl_Interp *interp, int argc, const char **argv)
{
    initMain();
    // process command line args
    if (0 != emcGetArgs(argc, (char**)argv)) {
        Tcl_SetResult(interp, "error in argument list\n", TCL_STATIC);
        return TCL_ERROR;
    }
    // get configuration information
    iniLoad(EMC_INIFILE);

    // update tcl's idea of the inifile name
    Tcl_SetVar(interp, "EMC_INIFILE", EMC_INIFILE, TCL_GLOBAL_ONLY);

    // init NML
    if (0 != tryNml()) {
        Tcl_SetResult(interp, "can't connect to emc\n", TCL_STATIC);
        thisQuit(NULL);
        return TCL_ERROR;
    }
    // get current serial number, and save it for restoring when we quit
    // so as not to interfere with real operator interface
    updateStatus();
    emcCommandSerialNumber = emcStatus->echo_serial_number;
    saveEmcCommandSerialNumber = emcStatus->echo_serial_number;

    // attach our quit function to exit
    Tcl_CreateExitHandler(thisQuit, (ClientData) 0);

    // attach our quit function to SIGINT
    signal(SIGINT, sigQuit);

    Tcl_SetResult(interp, "", TCL_STATIC);
    return TCL_OK;
}

extern "C" 
int Emc_Init(Tcl_Interp * interp);
int Emc_Init(Tcl_Interp * interp)
{
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) 
    {
        return TCL_ERROR;
    }

    /* 
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    Tcl_CreateCommand(interp, "emc_init", emc_init, (ClientData) NULL,
                         (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_plat", emc_plat, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_ini", emc_ini, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_debug", emc_debug, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_set_wait", emc_set_wait,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_wait", emc_wait, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_set_timeout", emc_set_timeout,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_update", emc_update,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_time", emc_time, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_error", emc_error, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_operator_text", emc_operator_text,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_operator_display",
			 emc_operator_display, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_estop", emc_estop, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_machine", emc_machine,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_mode", emc_mode, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_mist", emc_mist, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_flood", emc_flood, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_lube", emc_lube, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_lube_level", emc_lube_level,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_spindle", emc_spindle,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_brake", emc_brake, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_tool", emc_tool, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_tool_offset", emc_tool_offset,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_load_tool_table",
			 emc_load_tool_table, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_set_tool_offset",
			 emc_set_tool_offset, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_abs_cmd_pos", emc_abs_cmd_pos,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_abs_act_pos", emc_abs_act_pos,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_rel_cmd_pos", emc_rel_cmd_pos,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_rel_act_pos", emc_rel_act_pos,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_joint_pos", emc_joint_pos,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_pos_offset", emc_pos_offset,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_joint_limit", emc_joint_limit,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_joint_fault", emc_joint_fault,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_override_limit", emc_override_limit,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_optional_stop", emc_optional_stop,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_joint_homed", emc_joint_homed,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_mdi", emc_mdi, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_home", emc_home, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_unhome", emc_unhome, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_jog_stop", emc_jog_stop,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_jog", emc_jog, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_jog_incr", emc_jog_incr,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_feed_override", emc_feed_override,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_spindle_override", emc_spindle_override,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_task_plan_init", emc_task_plan_init,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_open", emc_open, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_run", emc_run, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_pause", emc_pause, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_resume", emc_resume,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_step", emc_step, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_abort", emc_abort, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_program", emc_program,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_program_line", emc_program_line,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_program_status", emc_program_status,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_program_codes", emc_program_codes,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_joint_type", emc_joint_type,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_joint_units", emc_joint_units,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_program_linear_units",
			 emc_program_linear_units, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_program_angular_units",
			 emc_program_angular_units, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_user_linear_units",
			 emc_user_linear_units, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_user_angular_units",
			 emc_user_angular_units, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_display_linear_units",
			 emc_display_linear_units, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_display_angular_units",
			 emc_display_angular_units, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_linear_unit_conversion",
			 emc_linear_unit_conversion, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_angular_unit_conversion",
			 emc_angular_unit_conversion, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_task_heartbeat", emc_task_heartbeat,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_task_command", emc_task_command,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_task_command_number",
			 emc_task_command_number, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_task_command_status",
			 emc_task_command_status, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_heartbeat", emc_io_heartbeat,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_command", emc_io_command,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_command_number",
			 emc_io_command_number, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_command_status",
			 emc_io_command_status, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_motion_heartbeat",
			 emc_motion_heartbeat, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_motion_command", emc_motion_command,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_motion_command_number",
			 emc_motion_command_number, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_motion_command_status",
			 emc_motion_command_status, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_axis_backlash", emc_axis_backlash,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_axis_load_comp", emc_axis_load_comp,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_axis_enable", emc_axis_enable,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_teleop_enable", emc_teleop_enable,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_kinematics_type",
			 emc_kinematics_type, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_probe_clear", emc_probe_clear,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "emc_probe_value", emc_probe_value,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "emc_probe_tripped", emc_probe_tripped,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "emc_probe_move", emc_probe_move,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "emc_probed_pos", emc_probed_pos,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_pendant", emc_pendant,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    // provide builtins that may have been left out

    Tcl_CreateObjCommand(interp, "int", localint, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "round", localround, (ClientData) NULL,
			 (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "multihead", multihead, (ClientData) NULL,
                         (Tcl_CmdDeleteProc*) NULL);

    /* 
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */

    Tcl_SetVar(interp, "tcl_rcFileName", "~/.emcshrc", TCL_GLOBAL_ONLY);

    // set app-specific global variables
    Tcl_SetVar(interp, "EMC_INIFILE", EMC_INIFILE, TCL_GLOBAL_ONLY);

    Tcl_ResetResult(interp);

    return TCL_OK;
}

