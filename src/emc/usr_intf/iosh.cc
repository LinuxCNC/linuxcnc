/*
  iosh.cc

  Extended-Tcl-based general IO programming shell with NML connectivity

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

/*
  FIXME

  add tool_in_spindle status
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/io.h>

#include "tcl.h"
#include "tk.h"

#include "rcs.hh"		// etime()
#include "posemath.h"		// PM_POSE
#include "emc.hh"		// EMC NML
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.h"		// INIFILE

#include "motion.h"		// emc struct and commands
#include "usrmotintf.h"		// usrmot interface

#include <unistd.h>		/* iopl() */

/*
  Using iosh:

  iosh {<script>} {-- -ini <ini file>}

  With filename, it opens NML buffers to the EMC IO, runs the script, closes
  the buffers, and quits.

  Without filename, it runs interactively.

  With -- -ini <inifile>, uses inifile instead of emc.ini. Note that
  the two dashes prevents Tcl from looking at the remaining args, which
  would otherwise trigger a Tcl error that it doesn't understand what
  -ini means.

  EMC IO commands:

  emc_io_connect
  emc_io_disconnect
  Open or close the NML buffers to the command in, status out, and error out.
  Returns 0 if OK, or -1 if not.

  emc_io_read_command
  Peek the NML command buffer. Returns 0 if OK, -1 if not.

  emc_io_get_command
  Puts the command string, e.g., "emc_aux_estop_off", or "none". Returns 0.

  emc_io_get_command_type
  Puts the command NML number. Returns 0.

  emc_io_get_serial_number
  Puts the command serial number. Returns 0.

  emc_io_write_status
  Write the EMC_IO_STAT structure out to NML. Returns 0 if OK, -1 if error.

  emc_io_write_error
  Write the error string to the error NML buffer. Returns 0 if OK, -1 if error.

  IO status, sets associated field in the NML status structure

  emc_io_status_heartbeat <number>
  emc_io_status_echo_serial_number <number>
  emc_io_status_status done | exec | error
  emc_io_status_estop on | off
  emc_io_status_mist on | off
  emc_io_status_flood on | off
  emc_io_status_lube on | off
  emc_io_status_lube_level ok | low
  emc_io_status_spindle_speed <speed>
  emc_io_status_spindle_enabled on | off
  emc_io_status_spindle_direction <pos> <neg> 0
  emc_io_status_spindle_increasing <pos> <neg> 0
  emc_io_status_spindle_brake on | off
  emc_io_status_tool_prepped <number>
  emc_io_status_tool_in_spindle <number>

  IO commands:

  inb <address>
  Reads and returns the byte at <address>. If address begins with 0x,
  it's interpreted as a hex number, otherwise it's decimal.

  outb <address> <value>
  Writes the byte <value> to <address>. If address or value begins with 0x,
  it's interpreted as a hex number, otherwise it's decimal. Returns nothing.

  inw <address>
  Reads and returns the short at <address>. If address begins with 0x,
  it's interpreted as a hex number, otherwise it's decimal.

  outw <address> <value>
  Writes the short <value> to <address>. If address or value begins with 0x,
  it's interpreted as a hex number, otherwise it's decimal. Returns nothing.

  inl <address>
  Reads and returns the long at <address>. If address begins with 0x,
  it's interpreted as a hex number, otherwise it's decimal.

  outl <address> <value>
  Writes the long <value> to <address>. If address or value begins with 0x,
  it's interpreted as a hex number, otherwise it's decimal. Returns nothing.
*/

// the NML channels to the EMC task
static RCS_CMD_CHANNEL *emcioCommandBuffer = 0;
// NML command channel data pointer
static RCS_CMD_MSG *emcioCommand = 0;
static RCS_STAT_CHANNEL *emcioStatusBuffer = 0;
static EMC_IO_STAT emcioStatus;

// the NML channel for errors
static NML *emcErrorBuffer = 0;

// Shared memory to communicate with emcmot
static EMCMOT_COMMAND emcmotCommand;
extern EMCMOT_STRUCT *emcmotshmem;
static long shmem = 0;		// Shared memory flag

// "defined but not used"...
//static int motionId = 0;

static int emcIoNmlGet()
{
    int retval = 0;

    // try to connect to EMC IO cmd
    if (emcioCommandBuffer == 0) {
	emcioCommandBuffer =
	    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "tool", EMC_NMLFILE);
	if (!emcioCommandBuffer->valid()) {
	    rcs_print_error("emcToolCmd buffer not available\n");
	    delete emcioCommandBuffer;
	    emcioCommandBuffer = 0;
	    retval = -1;
	} else {
	    // get our command data structure
	    emcioCommand = emcioCommandBuffer->get_address();
	}
    }
    // try to connect to EMC IO status
    if (emcioStatusBuffer == 0) {
	emcioStatusBuffer =
	    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "tool", EMC_NMLFILE);
	if (!emcioStatusBuffer->valid()) {
	    rcs_print_error("toolSts buffer not available\n");
	    delete emcioStatusBuffer;
	    emcioStatusBuffer = 0;
	    retval = -1;
	} else {
	    // initialize and write status
	    emcioStatus.heartbeat = 0;
	    emcioStatus.command_type = 0;
	    emcioStatus.echo_serial_number = 0;
	    emcioStatus.status = RCS_DONE;
	    emcioStatusBuffer->write(&emcioStatus);
	}
    }

    return retval;
}

static int emcErrorNmlGet()
{
    int retval = 0;

    if (emcErrorBuffer == 0) {
	emcErrorBuffer =
	    new NML(nmlErrorFormat, "emcError", "tool", EMC_NMLFILE);
	if (!emcErrorBuffer->valid()) {
	    rcs_print_error("emcError buffer not available\n");
	    delete emcErrorBuffer;
	    emcErrorBuffer = 0;
	    retval = -1;
	}
    }

    return retval;
}

// EMC IO commands

static int emc_ini(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    INIFILE inifile;
    const char *inistring;
    const char *varstr, *secstr;

    if (objc != 3) {
	Tcl_SetResult(interp, "emc_ini: need 'var' and 'section'",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }
    // open it
    if (-1 == inifile.open(EMC_INIFILE)) {
	return -1;
    }

    varstr = Tcl_GetStringFromObj(objv[1], 0);
    secstr = Tcl_GetStringFromObj(objv[2], 0);

    if (NULL == (inistring = inifile.find(varstr, secstr))) {
	return TCL_OK;
    }

    Tcl_SetResult(interp, (char *) inistring, TCL_VOLATILE);

    // close it
    inifile.close();

    return TCL_OK;
}

static int emc_io_connect(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    double end;
    int good;

#define RETRY_TIME 10.0		// seconds to wait for subsystems to come up
#define RETRY_INTERVAL 1.0	// seconds between wait tries for a subsystem

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_connect: need no args\n", TCL_VOLATILE);
	return TCL_ERROR;
    }
    if (!(EMC_DEBUG & EMC_DEBUG_NML)) {
	set_rcs_print_destination(RCS_PRINT_TO_NULL);	// inhibit diag
	// messages
    }
    end = RETRY_TIME;
    good = 0;
    do {
	if (0 == emcIoNmlGet()) {
	    good = 1;
	    break;
	}
	esleep(RETRY_INTERVAL);
	end -= RETRY_INTERVAL;
    } while (end > 0.0);

    if (!(EMC_DEBUG & EMC_DEBUG_NML)) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// restore diag
	// messages
    }

    if (!good) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	return TCL_OK;
    }

    if (!(EMC_DEBUG & EMC_DEBUG_NML)) {
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

    if (!(EMC_DEBUG & EMC_DEBUG_NML)) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// restore diag
	// messages
    }
    if (!good) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	return TCL_OK;
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    return TCL_OK;
}

static int emc_io_disconnect(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_disconnect: need no args\n",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcErrorBuffer != 0) {
	delete emcErrorBuffer;
	emcErrorBuffer = 0;
    }

    if (emcioStatusBuffer != 0) {
	delete emcioStatusBuffer;
	emcioStatusBuffer = 0;
    }

    if (emcioCommandBuffer != 0) {
	delete emcioCommandBuffer;
	emcioCommandBuffer = 0;
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    return TCL_OK;
}

/*
  emc_io_read_command peeks the NML command buffer. The side effect is
  that emcioCommand now latches the NML contents. Other functions, like
  emc_io_get_command,serial_number, get the contents from emcioCommand.

  Returns 0 if OK, -1 if error reading NML.
*/
static int emc_io_read_command(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_read_command: need no args\n",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }
    // read NML buffer
    if (0 == emcioCommandBuffer || 0 == emcioCommand) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	return TCL_OK;
    }
    // latch new command into emcioCommand
    if (-1 == emcioCommandBuffer->peek()) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	return TCL_OK;
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    return TCL_OK;
}

/*
  emc_io_get_command returns the string and any args associated with
  the command, or "none" if nothing is there.

  The convention for returning strings is that they're the lower-case
  version of their NMLTYPE declarations in emc.hh, without the trailing
  _TYPE, e.g.,

  EMC_IO_INIT_TYPE -> emc_io_init

  Note that these aren't always what you type in emcsh, for example,
  EMC_AUX_ESTOP_OFF_TYPE -> emc_aux_estop off, not "emc_estop off". To
  support the interchangeability of these, emcsh will be set up so that
  the short versions ("emc_estop off") will supplement the conventional
  names, which will always be present.
*/
static int emc_io_get_command(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    NMLTYPE type;
    char string[256];

    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_read_command: need no args\n",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }
    // check for valid ptr
    if (0 == emcioCommand) {
	Tcl_SetResult(interp, "none", TCL_VOLATILE);
	return TCL_OK;
    }

    type = emcioCommand->type;
    switch (type) {
    case 0:
	Tcl_SetResult(interp, "none", TCL_VOLATILE);
	break;

    case EMC_IO_INIT_TYPE:
	Tcl_SetResult(interp, "emc_io_init", TCL_VOLATILE);
	break;

    case EMC_TOOL_INIT_TYPE:
	Tcl_SetResult(interp, "emc_tool_init", TCL_VOLATILE);
	break;

    case EMC_TOOL_HALT_TYPE:
	Tcl_SetResult(interp, "emc_tool_halt", TCL_VOLATILE);
	break;

    case EMC_TOOL_ABORT_TYPE:
	Tcl_SetResult(interp, "emc_tool_abort", TCL_VOLATILE);
	break;

    case EMC_TOOL_PREPARE_TYPE:
	sprintf(string, "emc_tool_prepare %d",
	    ((EMC_TOOL_PREPARE *) emcioCommand)->tool);
	Tcl_SetResult(interp, string, TCL_VOLATILE);
	break;

    case EMC_TOOL_LOAD_TYPE:
	Tcl_SetResult(interp, "emc_tool_load", TCL_VOLATILE);
	break;

    case EMC_TOOL_UNLOAD_TYPE:
	Tcl_SetResult(interp, "emc_tool_unload", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_INIT_TYPE:
	Tcl_SetResult(interp, "emc_spindle_init", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_HALT_TYPE:
	Tcl_SetResult(interp, "emc_spindle_halt", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_ABORT_TYPE:
	Tcl_SetResult(interp, "emc_spindle_abort", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_ON_TYPE:
	sprintf(string, "emc_spindle_on %f",
	    ((EMC_SPINDLE_ON *) emcioCommand)->speed);
	Tcl_SetResult(interp, string, TCL_VOLATILE);
	break;

    case EMC_SPINDLE_OFF_TYPE:
	Tcl_SetResult(interp, "emc_spindle_off", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_FORWARD_TYPE:
	Tcl_SetResult(interp, "emc_spindle_forward", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_REVERSE_TYPE:
	Tcl_SetResult(interp, "emc_spindle_reverse", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_STOP_TYPE:
	Tcl_SetResult(interp, "emc_spindle_stop", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_INCREASE_TYPE:
	Tcl_SetResult(interp, "emc_spindle_increase", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_DECREASE_TYPE:
	Tcl_SetResult(interp, "emc_spindle_decrease", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_CONSTANT_TYPE:
	Tcl_SetResult(interp, "emc_spindle_constant", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_BRAKE_RELEASE_TYPE:
	Tcl_SetResult(interp, "emc_spindle_brake_release", TCL_VOLATILE);
	break;

    case EMC_SPINDLE_BRAKE_ENGAGE_TYPE:
	Tcl_SetResult(interp, "emc_spindle_brake_engage", TCL_VOLATILE);
	break;

    case EMC_COOLANT_INIT_TYPE:
	Tcl_SetResult(interp, "emc_coolant_init", TCL_VOLATILE);
	break;

    case EMC_COOLANT_HALT_TYPE:
	Tcl_SetResult(interp, "emc_coolant_halt", TCL_VOLATILE);
	break;

    case EMC_COOLANT_ABORT_TYPE:
	Tcl_SetResult(interp, "emc_coolant_abort", TCL_VOLATILE);
	break;

    case EMC_COOLANT_MIST_ON_TYPE:
	Tcl_SetResult(interp, "emc_coolant_mist_on", TCL_VOLATILE);
	break;

    case EMC_COOLANT_MIST_OFF_TYPE:
	Tcl_SetResult(interp, "emc_coolant_mist_off", TCL_VOLATILE);
	break;

    case EMC_COOLANT_FLOOD_ON_TYPE:
	Tcl_SetResult(interp, "emc_coolant_flood_on", TCL_VOLATILE);
	break;

    case EMC_COOLANT_FLOOD_OFF_TYPE:
	Tcl_SetResult(interp, "emc_coolant_flood_off", TCL_VOLATILE);
	break;

    case EMC_AUX_INIT_TYPE:
	Tcl_SetResult(interp, "emc_aux_init", TCL_VOLATILE);
	break;

    case EMC_AUX_HALT_TYPE:
	Tcl_SetResult(interp, "emc_aux_halt", TCL_VOLATILE);
	break;

    case EMC_AUX_ABORT_TYPE:
	Tcl_SetResult(interp, "emc_aux_abort", TCL_VOLATILE);
	break;

    case EMC_AUX_ESTOP_ON_TYPE:
	Tcl_SetResult(interp, "emc_aux_estop_on", TCL_VOLATILE);
	break;

    case EMC_AUX_ESTOP_OFF_TYPE:
	Tcl_SetResult(interp, "emc_aux_estop_off", TCL_VOLATILE);
	break;

    case EMC_LUBE_INIT_TYPE:
	Tcl_SetResult(interp, "emc_lube_init", TCL_VOLATILE);
	break;

    case EMC_LUBE_HALT_TYPE:
	Tcl_SetResult(interp, "emc_lube_halt", TCL_VOLATILE);
	break;

    case EMC_LUBE_ABORT_TYPE:
	Tcl_SetResult(interp, "emc_lube_abort", TCL_VOLATILE);
	break;

    case EMC_LUBE_ON_TYPE:
	Tcl_SetResult(interp, "emc_lube_on", TCL_VOLATILE);
	break;

    case EMC_LUBE_OFF_TYPE:
	Tcl_SetResult(interp, "emc_lube_off", TCL_VOLATILE);
	break;

    default:
	Tcl_SetResult(interp, (char *) emcSymbolLookup(type), TCL_VOLATILE);
	break;
    }

    return TCL_OK;
}

/*
  Returns the NML command type number
*/
static int emc_io_get_command_type(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_get_command_type: need no args\n",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(emcioCommand->type));

    return TCL_OK;
}

/*
  Returns the NML command serial number
*/
static int emc_io_get_serial_number(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_get_serial_number: need no args\n",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(emcioCommand->serial_number));

    return TCL_OK;
}

static int emc_io_write_status(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    if (objc != 1) {
	Tcl_SetResult(interp, "emc_io_write_status: need no args",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 == emcioStatusBuffer) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	return TCL_OK;
    }

    if (0 == emcioStatusBuffer->write(&emcioStatus)) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    } else {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
    }
    return TCL_OK;
}

static int emc_io_write_error(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    EMC_OPERATOR_ERROR error_msg;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_write_error: need error string",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 == emcErrorBuffer) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	return TCL_OK;
    }

    strncpy(error_msg.error, Tcl_GetStringFromObj(objv[1], 0),
	EMC_OPERATOR_ERROR_LEN);
    error_msg.error[EMC_OPERATOR_ERROR_LEN - 1] = 0;
    if (0 == emcErrorBuffer->write(&error_msg)) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    } else {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
    }
    return TCL_OK;
}

// IO status

/* set the status heartbeat */
static int emc_io_status_heartbeat(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int heartbeat;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_heartbeat: need heartbeat",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &heartbeat)) {
	emcioStatus.heartbeat = heartbeat;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_heartbeat: bad heartbeat",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_command_type(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int command_type;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_command_type: need command type",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &command_type)) {
	emcioStatus.command_type = command_type;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_command_type: need command type",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_echo_serial_number(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int echo_serial_number;

    if (objc != 2) {
	Tcl_SetResult(interp,
	    "emc_io_status_echo_serial_number: need echo serial number",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &echo_serial_number)) {
	emcioStatus.echo_serial_number = echo_serial_number;
	return TCL_OK;
    }

    Tcl_SetResult(interp,
	"emc_io_status_echo_serial_number: need echo serial number",
	TCL_VOLATILE);
    return TCL_ERROR;
}

/*
  set the NML status to done, exec, error, or read it with no args.
*/
static int emc_io_status_status(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status: need done | exec | error",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "done")) {
	emcioStatus.status = RCS_DONE;
	return TCL_OK;
    } else if (!strcmp(objstr, "exec")) {
	emcioStatus.status = RCS_EXEC;
	return TCL_OK;
    } else if (!strcmp(objstr, "error")) {
	emcioStatus.status = RCS_ERROR;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status: need done | exec | error",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_estop(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_estop: need on | off",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "on")) {
	emcioStatus.aux.estop = 1;
	return TCL_OK;
    } else if (!strcmp(objstr, "off")) {
	emcioStatus.aux.estop = 0;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_estop: need on | off", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_mist(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_mist: need on | off",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "on")) {
	emcioStatus.coolant.mist = 1;
	return TCL_OK;
    } else if (!strcmp(objstr, "off")) {
	emcioStatus.coolant.mist = 0;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_mist: need on | off", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_flood(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_flood: need on | off",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "on")) {
	emcioStatus.coolant.flood = 1;
	return TCL_OK;
    } else if (!strcmp(objstr, "off")) {
	emcioStatus.coolant.flood = 0;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_flood: need on | off", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_lube(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_lube: need on | off",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "on")) {
	emcioStatus.lube.on = 1;
	return TCL_OK;
    } else if (!strcmp(objstr, "off")) {
	emcioStatus.lube.on = 0;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_lube: need on | off", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_lube_level(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_lube_level: need ok | low",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "ok")) {
	emcioStatus.lube.level = 1;
	return TCL_OK;
    } else if (!strcmp(objstr, "low")) {
	emcioStatus.lube.level = 0;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_lube_level: need ok | low",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_spindle_speed(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    double speed;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_spindle_speed: need speed",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetDoubleFromObj(0, objv[1], &speed)) {
	emcioStatus.spindle.speed = speed;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_spindle_speed: need speed",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_spindle_enabled(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_spindle_enabled: need on | off",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "on")) {
	emcioStatus.spindle.enabled = 1;
	return TCL_OK;
    } else if (!strcmp(objstr, "off")) {
	emcioStatus.spindle.enabled = 0;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_spindle_enabled: need on | off",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_spindle_direction(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int direction;

    if (objc != 2) {
	Tcl_SetResult(interp,
	    "emc_io_status_spindle_direction: need direction", TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &direction)) {
	emcioStatus.spindle.direction = direction;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_spindle_direction: need direction",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_spindle_increasing(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int increasing;

    if (objc != 2) {
	Tcl_SetResult(interp,
	    "emc_io_status_spindle_increasing: need increasing",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &increasing)) {
	emcioStatus.spindle.increasing = increasing;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_spindle_increasing: need increasing",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_spindle_brake(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    const char *objstr;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_spindle_brake: need on | off",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    objstr = Tcl_GetStringFromObj(objv[1], 0);
    if (!strcmp(objstr, "on")) {
	emcioStatus.spindle.brake = 1;
	return TCL_OK;
    } else if (!strcmp(objstr, "off")) {
	emcioStatus.spindle.brake = 0;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_spindle_brake: need on | off",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_tool_prepped(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int toolPrepped;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_tool_prepped: need tool",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &toolPrepped)) {
	emcioStatus.tool.toolPrepped = toolPrepped;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_tool_prepped: need tool",
	TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_io_status_tool_in_spindle(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int toolInSpindle;

    if (objc != 2) {
	Tcl_SetResult(interp, "emc_io_status_tool_in_spindle: need tool",
	    TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &toolInSpindle)) {
	emcioStatus.tool.toolInSpindle = toolInSpindle;
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_io_status_tool_in_spindle: need tool",
	TCL_VOLATILE);
    return TCL_ERROR;
}

// IO commands

static int unpriv = 0;		// non-zero means can't read IO

// note the leading f_ so we don't conflict with real inb
static int f_inb(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    long address;

    if (objc == 2) {
	if (TCL_OK == Tcl_GetLongFromObj(0, objv[1], &address)) {
	    if (unpriv) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(0xFF));
		return TCL_OK;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj((int) inb(address)));
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "syntax: inb <address>", TCL_VOLATILE);
    return TCL_ERROR;
}

// note the leading f_ so we don't conflict with real outb
static int f_outb(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    long address;
    int value;

    if (objc == 3) {
	if (TCL_OK == Tcl_GetLongFromObj(0, objv[1], &address) &&
	    TCL_OK == Tcl_GetIntFromObj(0, objv[2], &value)) {
	    if (unpriv) {
		return TCL_OK;
	    }
	    outb((char) value, address);
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "syntax: outb <address> <value>", TCL_VOLATILE);
    return TCL_ERROR;
}

// note the leading f_ so we don't conflict with real inw
static int f_inw(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    long address;

    if (objc == 2) {
	if (TCL_OK == Tcl_GetLongFromObj(0, objv[1], &address)) {
	    if (unpriv) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(0xFFFF));
		return TCL_OK;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj((int) inw(address)));
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "syntax: inw <address>", TCL_VOLATILE);
    return TCL_ERROR;
}

// note the leading f_ so we don't conflict with real outw
static int f_outw(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    long address;
    int value;

    if (objc == 3) {
	if (TCL_OK == Tcl_GetLongFromObj(0, objv[1], &address) &&
	    TCL_OK == Tcl_GetIntFromObj(0, objv[2], &value)) {
	    if (unpriv) {
		return TCL_OK;
	    }
	    outw((short) value, address);
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "syntax: outw <address> <value>", TCL_VOLATILE);
    return TCL_ERROR;
}

// note the leading f_ so we don't conflict with real inl
static int f_inl(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    long address;

    if (objc == 2) {
	if (TCL_OK == Tcl_GetLongFromObj(0, objv[1], &address)) {
	    if (unpriv) {
		Tcl_SetObjResult(interp, Tcl_NewLongObj(0xFFFFFFFF));
		return TCL_OK;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewLongObj(inl(address)));
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "syntax: inl <address>", TCL_VOLATILE);
    return TCL_ERROR;
}

// note the leading f_ so we don't conflict with real outl
static int f_outl(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    long address;
    long value;

    if (objc == 3) {
	if (TCL_OK == Tcl_GetLongFromObj(0, objv[1], &address) &&
	    TCL_OK == Tcl_GetLongFromObj(0, objv[2], &value)) {
	    if (unpriv) {
		return TCL_OK;
	    }
	    outl(value, address);
	    return TCL_OK;
	}
    }

    Tcl_SetResult(interp, "syntax: outl <address> <value>", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_mot_shmem(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{

    if (objc == 1) {
	if (unpriv) {
	    Tcl_SetObjResult(interp, Tcl_NewLongObj(0xFFFFFFFF));
	    return TCL_OK;
	}
	Tcl_SetObjResult(interp, Tcl_NewLongObj(shmem));
	return TCL_OK;
    }

    Tcl_SetResult(interp, "emc_mot_shmem: need no args", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_mot_rawinput(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int axis;

    if (objc == 2) {
	if (TCL_OK == Tcl_GetIntFromObj(0, objv[1], &axis)) {
	    if (shmem > 0) {
		Tcl_SetObjResult(interp,
		    Tcl_NewDoubleObj(emcmotshmem->debug.rawInput[axis]));
		return TCL_OK;
	    } else {
		Tcl_SetObjResult(interp, Tcl_NewDoubleObj(0));
		return TCL_OK;
	    }
	}
    }
    Tcl_SetResult(interp, "syntax: emc_mot_rawinput <axis>", TCL_VOLATILE);
    return TCL_ERROR;
}

static int emc_mot_move(ClientData clientdata,
    Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int axis;
    double move;
    double vel;

    if (objc == 4) {
	if (TCL_OK != Tcl_GetIntFromObj(0, objv[1], &axis)) {
	    return TCL_ERROR;
	}
	if (TCL_OK != Tcl_GetDoubleFromObj(0, objv[2], &move)) {
	    return TCL_ERROR;
	}
	if (TCL_OK != Tcl_GetDoubleFromObj(0, objv[3], &vel)) {
	    return TCL_ERROR;
	}
// Set the velocity
	emcmotCommand.vel = vel;
	emcmotCommand.command = EMCMOT_SET_VEL;
	usrmotWriteEmcmotCommand(&emcmotCommand);
// Get the current position
	emcmotCommand.pos.tran.x = emcmotshmem->status.actualPos.tran.x;
	emcmotCommand.pos.tran.y = emcmotshmem->status.actualPos.tran.y;
	emcmotCommand.pos.tran.z = emcmotshmem->status.actualPos.tran.z;
// Program the move
	switch (axis) {
	case 0:
	    emcmotCommand.pos.tran.x = move;
	    break;
	case 1:
	    emcmotCommand.pos.tran.y = move;
	    break;
	case 2:
	    emcmotCommand.pos.tran.z = move;
	    break;
	}

	emcmotCommand.id = emcmotshmem->status.id++;
	emcmotCommand.command = EMCMOT_SET_LINE;
	usrmotWriteEmcmotCommand(&emcmotCommand);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "syntax: emc_mot_move <axis> <position> <velocity>",
	TCL_VOLATILE);
    return TCL_ERROR;
}

int Tcl_AppInit(Tcl_Interp * interp)
{
    /* 
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.
     */

    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /* 
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    Tcl_CreateObjCommand(interp, "emc_ini", emc_ini, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_connect", emc_io_connect,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_disconnect", emc_io_disconnect,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_read_command", emc_io_read_command,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_get_command", emc_io_get_command,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_get_command_type",
	emc_io_get_command_type, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_get_serial_number",
	emc_io_get_serial_number, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_write_status", emc_io_write_status,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_write_error", emc_io_write_error,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_heartbeat",
	emc_io_status_heartbeat, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_command_type",
	emc_io_status_command_type, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_echo_serial_number",
	emc_io_status_echo_serial_number, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_status", emc_io_status_status,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_estop", emc_io_status_estop,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_mist", emc_io_status_mist,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_flood", emc_io_status_flood,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_lube", emc_io_status_lube,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_lube_level",
	emc_io_status_lube_level, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_spindle_speed",
	emc_io_status_spindle_speed, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_spindle_enabled",
	emc_io_status_spindle_enabled, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_spindle_direction",
	emc_io_status_spindle_direction, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_spindle_increasing",
	emc_io_status_spindle_increasing, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_spindle_brake",
	emc_io_status_spindle_brake, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_tool_prepped",
	emc_io_status_tool_prepped, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_io_status_tool_in_spindle",
	emc_io_status_tool_in_spindle, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "inb", f_inb, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "outb", f_outb, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "inw", f_inw, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "outw", f_outw, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "inl", f_inl, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "outl", f_outl, (ClientData) NULL,
	(Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_mot_shmem", emc_mot_shmem,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_mot_rawinput", emc_mot_rawinput,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, "emc_mot_move", emc_mot_move,
	(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    /* 
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */

    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclmainrc", TCL_GLOBAL_ONLY);

    // set app-specific global variables
    Tcl_SetVar(interp, "EMC_INIFILE", EMC_INIFILE, TCL_GLOBAL_ONLY);

    return TCL_OK;
}

static void thisQuit(ClientData clientData)
{
    // clean up NML channels

    if (emcErrorBuffer != 0) {
	delete emcErrorBuffer;
	emcErrorBuffer = 0;
    }

    if (emcioStatusBuffer != 0) {
	delete emcioStatusBuffer;
	emcioStatusBuffer = 0;
    }

    if (emcioCommandBuffer != 0) {
	delete emcioCommandBuffer;
	emcioCommandBuffer = 0;
	emcioCommand = 0;
    }
    // turn off port access
    iopl(0);
    // Clean up shared memory
//  usrmotExit();
    Tcl_Exit(0);
    exit(0);
}

/*
  iniLoad() loads basic parameters from the ini file that are needed
  to define functions here:

  [EMC] DEBUG
  [EMC] VERSION
  [EMC] MACHINE
  [EMC] NML_FILE

  The rest of the ini file parameters for the IO subsystems, e.g.,

  [EMCIO] CYCLE_TIME
  [EMCIO] or [EMC] IO_BASE_ADDRESS
  [EMCIO] TOOL_TABLE
  [EMCIO] SPINDLE_OFF_WAIT
  [EMCIO] ESTOP_SENSE_INDEX

  etc. are read in by the Tcl/Tk script via emc_ini and used in the script.
*/

static int iniLoad(const char *filename)
{
    INIFILE inifile;
    const char *inistring;
    char version[INIFILE_MAX_LINELEN];

    // open it
    if (-1 == inifile.open(filename)) {
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

    if (EMC_DEBUG & EMC_DEBUG_VERSIONS) {
	if (NULL != (inistring = inifile.find("VERSION", "EMC"))) {
	    // print version
	    sscanf(inistring, "$Revision: %s", version);
	    rcs_print("Version:  %s\n", version);
	} else {
	    // not found, not fatal
	    rcs_print("Version:  (not found)\n");
	}

	if (NULL != (inistring = inifile.find("MACHINE", "EMC"))) {
	    // print machine
	    rcs_print("Machine:  %s\n", inistring);
	} else {
	    // not found, not fatal
	    rcs_print("Machine:  (not found)\n");
	}
    }

    if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
	// copy to global
	strcpy(EMC_NMLFILE, inistring);
    } else {
	// not found, use default
    }

    // close it
    inifile.close();

    return 0;
}

static void sigQuit(int sig)
{
    thisQuit((ClientData) 0);
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

    // Enable shared memory comms.
    usrmotIniLoad(EMC_INIFILE);
    if (-1 != (shmem = usrmotInit())) {
	shmem = (long) emcmotshmem;
    }
    // turn on port access
    unpriv = 0;
    if (0 != iopl(3)) {
	fprintf(stderr, "not privileged to access IO-- disabling IO\n");
	unpriv = 1;
    }
    // attach our quit function to exit
    Tcl_CreateExitHandler(thisQuit, (ClientData) 0);

    // attach our quit function to SIGINT
    signal(SIGINT, sigQuit);

    // run Tk main loop
    Tk_Main(argc, argv, Tcl_AppInit);
/* Tk_Main bombs straight out, so anything after this does NOT get executed.
   So any cleanup operations need to be done in thisQuit(). */

    exit(0);
}
