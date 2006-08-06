/********************************************************************
* Description: emcsh.cc
*   Extended-Tcl-based EMC automatic test interface
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Further derived from work by jmkasunich
*
* Author: Eric H. Johnson
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
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE

/*
  Using emcsh:

  emcsh {<filename>} {-- -ini <ini file>}

  With filename, it opens NML buffers to the EMC, runs the script, closes
  the buffers, and quits.

  With -- -ini <inifile>, uses inifile instead of emc.ini. 

  Without filename, it waits for socket connections (Telnet).

  The files (or manual input) are EMC-specific commands similar to those
  of emcsh. Unlike emcsh these commands do not use the "emc_" prefix.

  Some commands take 0 or more arguments. 0 arguments means they return
  the associated value; the argument would be to set the value.

  Commands are sent to the EMC, and control resumes immediately. You can
  call a timed wait until the command got there, or a timed wait until the
  command completed, or not wait at all.
  
  There are four commands supported. Commands and most parameters are not
  case sensitive. The exceptions are passwords, file paths and text strings.
  The supported commands are as follows:
  
  ==> HELLO <==
  
  Hello <password> <cleint> <version>
  If a valid password was entered the server will respond with
  
  HELLO ACK <Server Name> <Server Version>
  
  Where server name and server version are looked up from the implementation.
  if an invalid password or any other syntax error occurs then the server 
  responds with:
  
  HELLO NAK
  
  ==> Get <==
  
  The get command includes one of the emc sub-commands, described below and
  zero or more additional parameters. 
  
  ==> Set <==
  
  The set command inclides one of the emc sub-commands, described below and
  one or more additional parameters.
  
  ==> Quit <==
  
  The quit command disconnects the associated socket connection.
  
  
  EMC sub-commands:
  
  echo on | off
  With get will return the current echo state, with set, sets the echo
  state. When echo is on, all commands will be echoed upon receipt. This
  state is local to each connection.
  
  verbose on | off
  With get will return the current verbose state, with set, sets the
  verbose state. When in verbose mode is on, all set commands return
  positive acknowledgement in the form SET <COMMAND> ACK. In addition,
  text error messages will be issued when in verbose mode. This state
  is local to each connection.
  
  enable <pwd> | off
  With get will return On or Off to indicate whether the current connection
  is enabled to perform control functions. With set and a valid password,
  the current connection is enabled for control functions. "OFF" may not
  be used as a password and disables control functions for this connection.
  
  config [TBD]
  
  comm_mode ascii | binary
  With get, will return the current communications mode. With set, will
  set the communications mode to the specified mode. The binary protocol 
  is TBD.
  
  comm_prot <version no>
  With get, returns the current protocol version used by the server,
  with set, sets the server to use the specified protocol version,
  provided it is lower than or equal to the highest version number
  supported by the server implementation.

  INIFILE
  Exported values of the EMC global of the same name

  plat
  Returns the platform for which this was compiled, e.g., linux_2_0_36

  ini <var> <section>
  Returns the string value of <var> in section <section>, in EMC_INIFILE

  debug {<new value>}
  With get, returns the integer value of EMC_DEBUG, in the EMC. Note that
  it may not be true that the local EMC_DEBUG variable here (in emcsh and
  the GUIs that use it) is the same as the EMC_DEBUG value in the EMC. This
  can happen if the EMC is started from one .ini file, and the GUI is started
  with another that has a different value for DEBUG.
  With set, sends a command to the EMC to set the new debug level,
  and sets the EMC_DEBUG global here to the same value. This will make
  the two values the same, since they really ought to be the same.

  set_wait none | received | done
  Set the wait for commands to return to be right away (none), after the
  command was sent and received (received), or after the command was
  done (done).

  wait received | done
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

  emc_probe_index
  Which wire is the probe on or which bit of digital IO to use? (No args
  gets it, one arg sets it.)

  emc_probe_polarity
  Value to look for for probe tripped? (0 args gets it, one arg sets it.)

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

// the NML channels to the EMC task
static RCS_CMD_CHANNEL *emcCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcStatusBuffer = 0;
EMC_STAT *emcStatus = 0;

// the NML channel for errors
static NML *emcErrorBuffer = 0;
static char error_string[LINELEN] = "";
static char operator_text_string[LINELEN] = "";
static char operator_display_string[LINELEN] = "";

// the current command numbers, set up updateStatus(), used in main()
static int emcCommandSerialNumber = 0;
static int saveEmcCommandSerialNumber = 0;

// default value for timeout, 0 means wait forever
static double emcTimeout = 0.0;

static enum {
    EMC_WAIT_NONE = 1,
    EMC_WAIT_RECEIVED,
    EMC_WAIT_DONE
} emcWaitType = EMC_WAIT_RECEIVED;

static enum {
    EMC_UPDATE_NONE = 1,
    EMC_UPDATE_AUTO
} emcUpdateType = EMC_UPDATE_AUTO;

typedef enum {
  cmdHello, cmdSet, cmdGet, cmdQuit, cmdUnknown} commandTokenType;
  
typedef enum {
  scEcho, scVerbose, scEnable, scConfig, scCommMode, scCommProt, scIniFile,
  scPlat, scIni, scDebug, scSetWait, scWait, scSetTimeout, scUpdate, scError,
  scOperatorDisplay, scOperatorText, scTime, scEStop, scMachine, scMode,
  scMist, scFlood, scLube, scLubeLevel, scSpindle, scBrake, scTool, scToolOffset,
  scLoadToolTable, scHome, scJogStop, scJog, scJogIncr, scFeedOverride,
  scAbsCmdPos, scAbsActPos, scRelCmdPos, scRelActPos, scJointPos, scPosOffset,
  scJointLimit, scJointFault, scJointHomed, scMDI, scTskPlanInit, scOpen, scRun,
  scPause, scResume, scStep, scAbort, scProgram, scProgramLine, scProgramStatus, scProgramCodes,
  scJointType, scJointUnits, scProgramUnits, scProgramLinearUnits, scProgramAngularUnits, 
  scUserLinearUnits, scUserAngularUnits, scDisplayLinearUnits, scDisplayAngularUnits,
  scLinearUnitConversion,  scAngularUnitConversion, scProbeIndex, scProbePolarity, scProbeClear, 
  scProbeTripped, scProbeValue, scProbe, scTeleopEnable, scKinematicsType, scOverrideLimits, 
  scUnknown
  } setCommandType;
  
typedef enum {
  rtNoError, rtHandledNoError, rtStandardError, rtCustomError, rtCustomHandledError
  } cmdResponseType;
  
  
int cliSock;
char hostName[80];
char version[8];
bool linked;
bool echo;
bool verbose;
bool connId;
int commMode;
int commProt;
char inBuf[256];
char outBuf[1600];
char progName[256];

int server_sockfd, client_sockfd;
socklen_t server_len, client_len;
struct sockaddr_in server_address;
struct sockaddr_in client_address;
bool useSockets = true;
int tokenIdx;
// char buffer[1600];
char *delims = " \n\r\0";
int connCount = -1;
int enabledConn = -1;

char *setCommands[] = {
  "ECHO", "VERBOSE", "ENABLE", "CONFIG", "COMM_MODE", "COMM_PROT", "INIFILE", "PLAT", "INI", "DEBUG",
  "SET_WAIT", "WAIT", "TIMEOUT", "UPDATE", "ERROR", "OPERATOR_DISPLAY", "OPERATOR_TEXT",
  "TIME", "ESTOP", "MACHINE", "MODE", "MIST", "FLOOD", "LUBE", "LUBE_LEVEL",
  "SPINDLE", "BRAKE", "TOOL", "TOOL_OFFSET", "LOAD_TOOL_TABLE", "HOME",
  "JOG_STOP", "JOG", "JOG_INCR", "FEED_OVERRIDE", "ABS_CMD_POS", "ABS_ACT_POS",
  "REL_CMD_POS", "REL_ACT_POS", "JOINT_POS", "POS_OFFSET", "JOINT_LIMIT",
  "JOINT_FAULT", "JOINT_HOMED", "MDI", "TASK_PLAN_INIT", "OPEN", "RUN", "PAUSE",
  "RESUME", "STEP", "ABORT", "PROGRAM", "PROGRAM_LINE", "PROGRAM_STATUS", "PROGRAM_CODES",
  "JOINT_TYPE", "JOINT_UNITS", "PROGRAM_UNITS", "PROGRAM_LINEAR_UNITS", "PROGRAM_ANGULAR_UNITS", 
  "USER_LINEAR_UNITS", "USER_ANGULAR_UNITS", "DISPLAY_LINEAR_UNITS", "DISPLAY_ANGULAR_UNITS", 
  "LINEAR_UNIT_CONVERSION", "ANGULAR_UNIT_CONVERSION", "PROBE_INDEX", "PROBE_POLARITY", "PROBE_CLEAR",
  "PROBE_TRIPPED", "PROBE_VALUE", "PROBE", "TELEOP_ENABLE", "KINEMATICS_TYPE", "OVERRIDE_LIMITS", ""};
char *commands[5] = {"HELLO", "SET", "GET", "QUIT", ""};

/* static char *skipWhite(char *s)
{
    while (isspace(*s)) {
	s++;
    }
    return s;
} */

void strupr(char *s)
{  
  int i;
  
  for (i = 0; i < (int)strlen(s); i++)
    if (s[i] > 96 && s[i] <= 'z')
      s[i] -= 32;
}

static int initSockets()
{
  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(5007);
  server_len = sizeof(server_address);
  bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
  listen(server_sockfd, 5);
  signal(SIGCHLD, SIG_IGN);
  return 0;
}

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

/*
  updateError() updates "errors," which are true errors and also
  operator display and text messages.
*/
static int updateError()
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
	error_string[LINELEN - 1] = 0;
	break;

    case EMC_OPERATOR_TEXT_TYPE:
	strncpy(operator_text_string,
		((EMC_OPERATOR_TEXT *) (emcErrorBuffer->get_address()))->
		text, LINELEN - 1);
	operator_text_string[LINELEN - 1] = 0;
	break;

    case EMC_OPERATOR_DISPLAY_TYPE:
	strncpy(operator_display_string,
		((EMC_OPERATOR_DISPLAY *) (emcErrorBuffer->
					   get_address()))->display,
		LINELEN - 1);
	operator_display_string[LINELEN - 1] = 0;
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

//    Tcl_Exit(0);
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

/*! \todo Another #if 0 */
#if 0
static double convertAngularUnits(double u)
{
    double in_deg;

    /* convert u to deg */
    in_deg = u / emcStatus->motion.traj.angularUnits;

    /* convert u to display units */
    switch (angularUnitConversion) {
    case ANGULAR_UNITS_DEG:
	return in_deg;
	break;
    case ANGULAR_UNITS_RAD:
	return in_deg * RAD_PER_DEG;
	break;
    case ANGULAR_UNITS_GRAD:
	return in_deg * GRAD_PER_DEG;
	break;
    case ANGULAR_UNITS_AUTO:
	return in_deg;		/*! \todo FIXME-- program units always degrees now */
	break;

    case ANGULAR_UNITS_CUSTOM:
	return u;
	break;

    }

    /* should never get here */
    return u;
}
#endif

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

// saved value of last program opened
static char lastProgramFile[LINELEN] = "";

static int sendProgramOpen(char *program)
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

// programStartLine is the saved valued of the line that
// sendProgramRun(int line) sent
static int programStartLine = 0;

static int sendProgramRun(int line)
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

static int sendLoadToolTable(const char *file)
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

/* static int sendAxisSetGains(int axis, double p, double i, double d,
			    double ff0, double ff1, double ff2,
			    double bias, double maxError, double deadband)
{
    EMC_AXIS_SET_GAINS emc_axis_set_gains_msg;

    emc_axis_set_gains_msg.axis = axis;
    emc_axis_set_gains_msg.p = p;
    emc_axis_set_gains_msg.i = i;
    emc_axis_set_gains_msg.d = d;
    emc_axis_set_gains_msg.ff0 = ff0;
    emc_axis_set_gains_msg.ff1 = ff1;
    emc_axis_set_gains_msg.ff2 = ff2;
    emc_axis_set_gains_msg.bias = bias;
    emc_axis_set_gains_msg.maxError = maxError;
    emc_axis_set_gains_msg.deadband = deadband;
    emc_axis_set_gains_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_set_gains_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAxisSetBacklash(int axis, double backlash)
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

static int sendAxisSetOutput(int axis, double output)
{
    EMC_AXIS_SET_OUTPUT emc_axis_set_output_msg;

    emc_axis_set_output_msg.axis = axis;
    emc_axis_set_output_msg.output = output;
    emc_axis_set_output_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_set_output_msg);
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
} */

/* static int sendAxisLoadComp(int axis, const char *file)
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
} */

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

static int sendSetProbeIndex(int index)
{
    EMC_TRAJ_SET_PROBE_INDEX emc_set_probe_index_msg;

    emc_set_probe_index_msg.index = index;
    emc_set_probe_index_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_set_probe_index_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSetProbePolarity(int polarity)
{
    EMC_TRAJ_SET_PROBE_POLARITY emc_set_probe_polarity_msg;

    emc_set_probe_polarity_msg.serial_number = ++emcCommandSerialNumber;
    emc_set_probe_polarity_msg.polarity = polarity;
    emcCommandBuffer->write(emc_set_probe_polarity_msg);
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

/* static int emc_ini(ClientData clientdata,
		   Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    Inifile inifile;
    const char *inistring;
    const char *varstr, *secstr, *defaultstr;
    defaultstr = 0;

    if (objc != 3 && objc != 4) {
	Tcl_SetResult(interp, "emc_ini: need 'var' and 'section'",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    // open it
    if (inifile.open(EMC_INIFILE) == false) {
	return TCL_OK;
    }

    varstr = Tcl_GetStringFromObj(objv[1], 0);
    secstr = Tcl_GetStringFromObj(objv[2], 0);

    if (objc == 4) {
	defaultstr = Tcl_GetStringFromObj(objv[3], 0);
    }

    if (NULL == (inistring = inifile.find(varstr, secstr))) {
	if (defaultstr != 0) {
	    Tcl_SetResult(interp, (char *) defaultstr, TCL_VOLATILE);
	}
	return TCL_OK;
    }

    Tcl_SetResult(interp, (char *) inistring, TCL_VOLATILE);

    // close it
    inifile.close();

    return TCL_OK;
} */

/* static int emc_task_heartbeat(ClientData clientdata,
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
} */

/* static int emc_io_command(ClientData clientdata,
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
} */

/* static int emc_io_command_number(ClientData clientdata,
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

static int emc_axis_gains(ClientData clientdata,
			  Tcl_Interp * interp, int objc,
			  Tcl_Obj * CONST objv[])
{
    Tcl_Obj *valobj;
    int axis;
    const char *varstr;
    double val;
    double p, i, d, ff0, ff1, ff2, bias, maxError, deadband;

    // syntax is emc_axis_gains <axis> <var> {<val>}
    // or emc_axis_gains <axis> all <p> <i> ... <deadband>
    // without <val> only, returns value of <var> for <axis>
    // otherwise sets <var> to <value> for <axis>
    // <axis> is 0,1,...
    // <var> is p i d ff0 ff1 ff2 bias maxerror deadband
    // <val> is floating point number

    if (objc < 3) {
	Tcl_SetResult(interp, "emc_axis_gains: need <axis> <var> {<val>}",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis) ||
	axis < 0 || axis >= EMC_AXIS_MAX) {
	Tcl_SetResult(interp,
		      "emc_axis_gains: need axis as integer, 0..EMC_AXIS_MAX-1",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    varstr = Tcl_GetStringFromObj(objv[2], 0);

    if (objc == 3) {
	if (!strcmp(varstr, "p")) {
	    valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].p);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "i")) {
	    valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].i);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "d")) {
	    valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].d);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "ff0")) {
	    valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].ff0);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "ff1")) {
	    valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].ff1);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "ff2")) {
	    valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].ff2);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "bias")) {
	    valobj = Tcl_NewDoubleObj(emcStatus->motion.axis[axis].bias);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "maxerror")) {
	    valobj =
		Tcl_NewDoubleObj(emcStatus->motion.axis[axis].maxError);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else if (!strcmp(varstr, "deadband")) {
	    valobj =
		Tcl_NewDoubleObj(emcStatus->motion.axis[axis].deadband);
	    Tcl_SetObjResult(interp, valobj);
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, "emc_axis_gains: bad value for <val>",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
    } else {
	// <val> is provided, so set it and use current values for others
	if (0 != Tcl_GetDoubleFromObj(0, objv[3], &val)) {
	    Tcl_SetResult(interp,
			  "emc_axis_gains: need value as floating point number",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}

	p = emcStatus->motion.axis[axis].p;
	i = emcStatus->motion.axis[axis].i;
	d = emcStatus->motion.axis[axis].d;
	ff0 = emcStatus->motion.axis[axis].ff0;
	ff1 = emcStatus->motion.axis[axis].ff1;
	ff2 = emcStatus->motion.axis[axis].ff2;
	bias = emcStatus->motion.axis[axis].bias;
	maxError = emcStatus->motion.axis[axis].maxError;
	deadband = emcStatus->motion.axis[axis].deadband;

	if (!strcmp(varstr, "p")) {
	    p = val;
	} else if (!strcmp(varstr, "i")) {
	    i = val;
	} else if (!strcmp(varstr, "d")) {
	    d = val;
	} else if (!strcmp(varstr, "ff0")) {
	    ff0 = val;
	} else if (!strcmp(varstr, "ff1")) {
	    ff1 = val;
	} else if (!strcmp(varstr, "ff2")) {
	    ff2 = val;
	} else if (!strcmp(varstr, "bias")) {
	    bias = val;
	} else if (!strcmp(varstr, "maxerror")) {
	    maxError = val;
	} else if (!strcmp(varstr, "deadband")) {
	    deadband = val;
	} else if (!strcmp(varstr, "all") && objc == 12) {
	    // it's "emc_axis_gains axis all p i d ff0 ff1 ff2 bias
	    // maxerror deadband"

	    // P
	    if (0 != Tcl_GetDoubleFromObj(0, objv[3], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need P gain as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    p = val;
	    // I
	    if (0 != Tcl_GetDoubleFromObj(0, objv[4], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need I gain as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    i = val;
	    // D
	    if (0 != Tcl_GetDoubleFromObj(0, objv[5], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need D gain as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    d = val;
	    // FF0
	    if (0 != Tcl_GetDoubleFromObj(0, objv[6], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need FF0 gain as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    ff0 = val;
	    // FF1
	    if (0 != Tcl_GetDoubleFromObj(0, objv[7], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need FF1 gain as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    ff1 = val;
	    // FF2
	    if (0 != Tcl_GetDoubleFromObj(0, objv[8], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need FF2 gain as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    ff2 = val;
	    // bias
	    if (0 != Tcl_GetDoubleFromObj(0, objv[9], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need bias as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    bias = val;
	    // maxerror
	    if (0 != Tcl_GetDoubleFromObj(0, objv[10], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need maxerror as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    maxError = val;
	    // deadband
	    if (0 != Tcl_GetDoubleFromObj(0, objv[11], &val)) {
		Tcl_SetResult(interp,
			      "emc_axis_gains: need deadband as floating point number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    deadband = val;
	} else {
	    Tcl_SetResult(interp,
			  "emc_axis_gains: not enough values for all gains",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}

	// now write it out
	sendAxisSetGains(axis, p, i, d, ff0, ff1, ff2, bias,
			 maxError, deadband);
	return TCL_OK;
    }

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

static int emc_axis_set_output(ClientData clientdata,
			       Tcl_Interp * interp, int objc,
			       Tcl_Obj * CONST objv[])
{
    int axis;
    double output;

    // syntax is emc_axis_output <axis> <output>

    if (objc != 3) {
	Tcl_SetResult(interp, "emc_axis_set_output: need <axis> <output>",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetIntFromObj(0, objv[1], &axis) ||
	axis < 0 || axis >= EMC_AXIS_MAX) {
	Tcl_SetResult(interp,
		      "emc_axis_set_output: need axis as integer, 0..EMC_AXIS_MAX-1",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }

    if (0 != Tcl_GetDoubleFromObj(0, objv[2], &output)) {
	Tcl_SetResult(interp,
		      "emc_axis_set_output: need output as real number",
		      TCL_VOLATILE);
	return TCL_ERROR;
    }
    // now write it out
    sendAxisSetOutput(axis, output);
    return TCL_OK;
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
    int axis;
    char file[256];

    // syntax is emc_axis_load_comp <axis> <file>

    if (objc != 3) {
	Tcl_SetResult(interp, "emc_axis_load_comp: need <axis> <file>",
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

    // now write it out
    sendAxisLoadComp(axis, file);
    return TCL_OK;
}

static int emc_axis_alter(ClientData clientdata,
			  Tcl_Interp * interp, int objc,
			  Tcl_Obj * CONST objv[])
{
    int axis;
    double alter;
    Tcl_Obj *alterobj;

    if (objc > 1) {
	// try for first arg as axis value
	if (0 != Tcl_GetIntFromObj(0, objv[1], &axis) ||
	    axis < 0 || axis >= EMC_AXIS_MAX) {
	    Tcl_SetResult(interp,
			  "emc_axis_alter: need axis as integer, 0..EMC_AXIS_MAX-1",
			  TCL_VOLATILE);
	    return TCL_ERROR;
	}
	// axis arg OK, so try for next arg, returning current alter val if
	// it's not there, setting it if it is

	if (objc == 2) {
	    if (emcUpdateType == EMC_UPDATE_AUTO) {
		updateStatus();
	    }
	    alterobj =
		Tcl_NewDoubleObj(emcStatus->motion.axis[axis].alter);
	    Tcl_SetObjResult(interp, alterobj);
	    return TCL_OK;
	}

	if (objc == 3) {
	    if (0 != Tcl_GetDoubleFromObj(0, objv[2], &alter)) {
		Tcl_SetResult(interp,
			      "emc_axis_alter: need alter as real number",
			      TCL_VOLATILE);
		return TCL_ERROR;
	    }
	    if (0 != sendAxisAlter(axis, alter)) {
		Tcl_SetResult(interp,
			      "emc_axis_alter: can't set alter value",
			      TCL_VOLATILE);
		return TCL_OK;	// no TCL_ERROR, which is a syntax error
	    }
	    // sent it OK
	    return TCL_OK;
	}
    }
    // else no args, or more than 2 args, so syntax error
    Tcl_SetResult(interp,
		  "emc_axis_alter: need axis, optional alter value",
		  TCL_VOLATILE);
    return TCL_ERROR;
} */


// ********************************************************************
//      Pendant read routine from /dev/psaux, /dev/ttyS0, or /dev/ttyS1
// *********************************************************************

/* static int emc_pendant(ClientData clientdata,
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
} */

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

static void sigQuit(int sig)
{
    thisQuit();
}

static int sockWrite()
{
   strcat(outBuf, "\r\n");
   write(cliSock, outBuf, strlen(outBuf));
   return 0;
}

static setCommandType lookupSetCommand(char *s)
{
  setCommandType i = scEcho;
  int temp;
  
  while (i < scUnknown) {
    if (strcmp(setCommands[i], s) == 0) return i;
//    (int)i += 1;
      temp = i;
      temp++;
      i = (setCommandType) temp;
    }
  return i;
}

static int commandHello()
{
  char *pch;
  char *password = "EMC";
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  if (strcmp(pch, password) != 0) return -1;
  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  strcpy(hostName, pch);  
  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  linked = true;    
  strcpy(version, pch);
  printf("Connected to %s\n", hostName);
  return 0;
}

static int checkOnOff(char *s)
{
  static char *onStr = "ON";
  static char *offStr = "OFF";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, onStr) == 0) return 0;
  if (strcmp(s, offStr) == 0) return 1;
  return -1;
}

static int checkBinaryASCII(char *s)
{
  static char *binaryStr = "BINARY";
  static char *ASCIIStr = "ASCII";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, ASCIIStr) == 0) return 0;
  if (strcmp(s, binaryStr) == 0) return 1;
  return -1;
}

static int checkReceivedDoneNone(char *s)
{
  static char *receivedStr = "RECEIVED";
  static char *doneStr = "DONE";
  static char *noneStr = "NONE";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, receivedStr) == 0) return 0;
  if (strcmp(s, doneStr) == 0) return 1;
  if (strcmp(s, noneStr) == 0) return 2;
  return -1;
}

static int checkNoneAuto(char *s)
{
  static char *noneStr = "NONE";
  static char *autoStr = "AUTO";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, noneStr) == 0) return 0;
  if (strcmp(s, autoStr) == 0) return 1;
  return -1;
}

static int checkManualAutoMDI(char *s)
{
  static char *manualStr = "MANUAL";
  static char *autoStr = "AUTO";
  static char *mDIStr = "MDI";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, manualStr) == 0) return 0;
  if (strcmp(s, autoStr) == 0) return 1;
  if (strcmp(s, mDIStr) == 0) return 2;
  return -1;
}

static int checkSpindleStr(char *s)
{
  static char *forwardStr = "FORWARD";
  static char *reverseStr = "REVERSE";
  static char *increaseStr = "INCREASE";
  static char *decreaseStr = "DECREASE";
  static char *constantStr = "CONSTANT";
  static char *offStr = "OFF";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, forwardStr) == 0) return 0;
  if (strcmp(s, reverseStr) == 0) return 1;
  if (strcmp(s, increaseStr) == 0) return 2;
  if (strcmp(s, decreaseStr) == 0) return 3;
  if (strcmp(s, constantStr) == 0) return 4;
  if (strcmp(s, offStr) == 0) return 5;
  return -1;
}

static int checkConversionStr(char *s)
{
  static char *inchStr = "INCH";
  static char *mmStr = "MM";
  static char *cmStr = "CM";
  static char *autoStr = "AUTO";
  static char *customStr = "CUSTOM";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, inchStr) == 0) return 0;
  if (strcmp(s, mmStr) == 0) return 1;
  if (strcmp(s, cmStr) == 0) return 2;
  if (strcmp(s, autoStr) == 0) return 3;
  if (strcmp(s, customStr) == 0) return 4;
  return -1;
}

static int checkAngularConversionStr(char *s)
{
  static char *degStr = "DEG";
  static char *radStr = "RAD";
  static char *gradStr = "GRAD";
  static char *autoStr = "AUTO";
  static char *customStr = "CUSTOM";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, degStr) == 0) return 0;
  if (strcmp(s, radStr) == 0) return 1;
  if (strcmp(s, gradStr) == 0) return 2;
  if (strcmp(s, autoStr) == 0) return 3;
  if (strcmp(s, customStr) == 0) return 4;
  return -1;
}

static cmdResponseType setEcho(char *s)
{
   
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: echo = true; break;
     case 1: echo = false;
     }
   return rtNoError;
}

static cmdResponseType setVerbose(char *s)
{
   
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: verbose = true; break;
     case 1: verbose = false;
     }
   return rtNoError;
}

static cmdResponseType setEnable(char *s)
{
   char *enablePWD = "EMCTOO";
  
   switch (checkOnOff(s)) {
     case -1: 
       if (strcmp(s, enablePWD) == 0) {
//         enable = true;
	 enabledConn = connId;
//	 printf("Enabled Context = %d This context = %d\n", enabledConn, connId);
         return rtNoError;
	 }
       else return rtStandardError;
     case 1: 
//       enable = false;
       enabledConn = -1;
     }
   return rtNoError;
}

static cmdResponseType setConfig(char *s)
{
  return rtNoError;
}

static cmdResponseType setCommMode(char *s)
{
  int ret;
  
  ret = checkBinaryASCII(s);
  if (ret == -1) return rtStandardError;
  commMode = ret;
  return rtNoError;
}

static cmdResponseType setCommProt(char *s)
{
  char *pVersion;
  
  pVersion = strtok(NULL, delims);
  if (pVersion == NULL) return rtStandardError;
  strcpy(version, pVersion);
  return rtNoError;
}

static cmdResponseType setDebug(char *s)
{
  char *pLevel;
  int level;
  
  pLevel = strtok(NULL, delims);
  if (pLevel == NULL) return rtStandardError;
  if (sscanf(pLevel, "%i", &level) == -1) return rtStandardError;
  else sendDebug(level);
  return rtNoError;
}

static cmdResponseType setSetWait(char *s)
{
   switch (checkReceivedDoneNone(s)) {
     case -1: return rtStandardError;
     case 0: emcWaitType = EMC_WAIT_RECEIVED; break;
     case 1: emcWaitType = EMC_WAIT_DONE; break;
     case 2: emcWaitType = EMC_WAIT_NONE; break;
     }
   return rtNoError;
}

static cmdResponseType setMachine(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendMachineOn(); break;
     case 1: sendMachineOff();
     }
   return rtNoError;
}

static cmdResponseType setEStop(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendEstop(); break;
     case 1: sendEstopReset();
     }
   return rtNoError;
}

static cmdResponseType setWait(char *s)
{
  switch (checkReceivedDoneNone(s)) {
    case -1: return rtStandardError;
    case 0: 
      if (emcCommandWaitReceived(emcCommandSerialNumber) != 0) return rtStandardError;
      break;
    case 1: 
      if (emcCommandWaitDone(emcCommandSerialNumber) != 0) return rtStandardError;
      break;
    case 2: ;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType setTimeout(char *s)
{
  float Timeout;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%f", &Timeout) < 1) return rtStandardError;
  emcTimeout = Timeout;
  return rtNoError;
}

static cmdResponseType setUpdate(char *s)
{
  switch (checkNoneAuto(s)) {
    case 0: emcUpdateType = EMC_UPDATE_NONE; break;
    case 1: emcUpdateType = EMC_UPDATE_AUTO; break;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType setMode(char *s)
{
  switch (checkManualAutoMDI(s)) {
    case 0: sendManual(); break;
    case 1: sendAuto(); break;
    case 2: sendMdi(); break;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType setMist(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendMistOn(); break;
     case 1: sendMistOff();
     }
   return rtNoError;
}

static cmdResponseType setFlood(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendFloodOn(); break;
     case 1: sendFloodOff();
     }
   return rtNoError;
}

static cmdResponseType setLube(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendLubeOn(); break;
     case 1: sendLubeOff();
     }
   return rtNoError;
}

static cmdResponseType setSpindle(char *s)
{
   switch (checkSpindleStr(s)) {
     case -1: return rtStandardError;
     case 0: sendSpindleForward(); break;
     case 1: sendSpindleReverse(); break;
     case 2: sendSpindleIncrease(); break;
     case 3: sendSpindleDecrease(); break;
     case 4: sendSpindleConstant(); break;
     case 5: sendSpindleOff();
     }
   return rtNoError;
}

static cmdResponseType setBrake(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendBrakeEngage(); break;
     case 1: sendBrakeRelease();
     }
   return rtNoError;
}

static cmdResponseType setLoadToolTable(char *s)
{
  if (s == NULL) return rtStandardError;
  if (sendLoadToolTable(s) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setToolOffset(char *s)
{
  int tool;
  float length, diameter;
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%d", &tool) <=0) return rtStandardError;
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &length) <= 0) return rtStandardError;
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &diameter) <= 0) return rtStandardError;
  
  if (sendToolSetOffset(tool, length, diameter) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setOverrideLimits(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: if (sendOverrideLimits(0) != 0) return rtStandardError; break;
     case 1: if (sendOverrideLimits(-1) != 0) return rtStandardError;
     }
   return rtNoError;
}

static cmdResponseType setMDI(char *s)
{
  char *pch;
  
  pch = strtok(NULL, "\n\r\0");
  if (sendMdiCmd(pch) !=0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setHome(char *s)
{
  int axis;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%d", &axis) <= 0) return rtStandardError;
  if ((axis < 0) || (axis > 5)) return rtStandardError;
  sendHome(axis);
  return rtNoError;
}

static cmdResponseType setJogStop(char *s)
{
  int axis;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%d", &axis) <= 0) return rtStandardError;
  if ((axis < 0) || (axis > 5)) return rtStandardError;
  if (sendJogStop(axis) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setJog(char *s)
{
  int axis;
  float speed;
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%d", &axis) <= 0) return rtStandardError;
  if ((axis < 0) || (axis > 5)) return rtStandardError;
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &speed) <= 0) return rtStandardError; 
  if (sendJogCont(axis, speed) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setFeedOverride(char *s)
{
  int percent;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%d", &percent) <= 0) return rtStandardError;
  sendFeedOverride(((double) percent) / 100.0);
  return rtNoError;
}

static cmdResponseType setJogIncr(char *s)
{
  int axis;
  float speed, incr;
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(s, "%d", &axis) <= 0) return rtStandardError;
  if ((axis < 0) || (axis > 5)) return rtStandardError;
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(s, "%f", &speed) <= 0) return rtStandardError; 
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(s, "%f", &incr) <= 0) return rtStandardError; 
  if (sendJogIncr(axis, speed, incr) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setTaskPlanInit(char *s)
{
  if (sendTaskPlanInit() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setOpen(char *s)
{
  char *pch;
  char fileStr[80];
  
  pch = strtok(NULL, "\n\r\0");
  strcpy(progName, pch);
  if (pch == NULL) return rtStandardError;
  strcpy(fileStr, "../../nc_files/");
  strcat(fileStr, pch);
  if (sendProgramOpen(fileStr) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setRun(char *s)
{
  int lineNo;
  
  if (s == NULL) // run from beginning
    if (sendProgramRun(0) != 0) return rtStandardError;
    else ;
  else
    { // run from line number
      if (sscanf(s, "%d", &lineNo) <= 0) return rtStandardError;
      if (sendProgramRun(lineNo) != 0) return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType setPause(char *s)
{
  if (sendProgramPause() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setResume(char *s)
{
  if (sendProgramResume() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setStep(char *s)
{
  if (sendProgramStep() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setAbort(char *s)
{
  if (sendAbort() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setLinearUnitConversion(char *s)
{
   switch (checkConversionStr(s)) {
     case -1: return rtStandardError;
     case 0: linearUnitConversion = LINEAR_UNITS_INCH; break;
     case 1: linearUnitConversion = LINEAR_UNITS_MM; break;
     case 2: linearUnitConversion = LINEAR_UNITS_CM; break;
     case 3: linearUnitConversion = LINEAR_UNITS_AUTO; break;
     case 4: linearUnitConversion = LINEAR_UNITS_CUSTOM; break;
     }
   return rtNoError;
}

static cmdResponseType setAngularUnitConversion(char *s)
{
   switch (checkAngularConversionStr(s)) {
     case -1: return rtStandardError;
     case 0: angularUnitConversion = ANGULAR_UNITS_DEG; break;
     case 1: angularUnitConversion = ANGULAR_UNITS_RAD; break;
     case 2: angularUnitConversion = ANGULAR_UNITS_GRAD; break;
     case 3: angularUnitConversion = ANGULAR_UNITS_AUTO; break;
     case 4: angularUnitConversion = ANGULAR_UNITS_CUSTOM; break;
     }
   return rtNoError;
}

static cmdResponseType setTeleopEnable(char *s)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendSetTeleopEnable(1); break;
     case 1: sendSetTeleopEnable(0);
     }
   return rtNoError;
}

static cmdResponseType setProbeIndex(char *s)
{
  int probeIndex;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%d", &probeIndex) <= 0) return rtStandardError;
  sendSetProbeIndex(probeIndex);
  return rtNoError;
}

static cmdResponseType setProbePolarity(char *s)
{
  int polarity;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%d", &polarity) <= 0) return rtStandardError;
  sendSetProbePolarity(polarity);
  return rtNoError;
}

static cmdResponseType setProbe(char *s)
{
  float x, y, z;
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &x) <= 0) return rtStandardError;

  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &y) <= 0) return rtStandardError;

  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &z) <= 0) return rtStandardError;
  
  sendProbe(x, y, z);
  return rtNoError;
}

static cmdResponseType setProbeClear(char *s)
{
  sendClearProbeTrippedFlag();
  return rtNoError;
}

int commandSet()
{
  static char *setNakStr = "SET NAK\n\r";
  static char *setCmdNakStr = "SET %s NAK\n\r";
  static char *ackStr = "SET %s ACK\n\r";
  setCommandType cmd;
  char *pch;
  cmdResponseType ret = rtNoError;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) {
    write(cliSock, setNakStr, strlen(setNakStr));
    return 0;
    }
  strupr(pch);
  cmd = lookupSetCommand(pch);
  if ((cmd >= scIniFile) && (connId != enabledConn)) {
    sprintf(outBuf, setCmdNakStr, pch);
    write(cliSock, outBuf, strlen(outBuf));
    return 0;
    }
  switch (cmd) {
    case scEcho: ret = setEcho(strtok(NULL, delims)); break;
    case scVerbose: ret = setVerbose(strtok(NULL, delims)); break;
    case scEnable: ret = setEnable(strtok(NULL, delims)); break;
    case scConfig: ret = setConfig(strtok(NULL, delims)); break;
    case scCommMode: ret = setCommMode(strtok(NULL, delims)); break;
    case scCommProt: ret = setCommProt(strtok(NULL, delims)); break;
    case scIniFile: break;
    case scPlat: break;
    case scIni: break;
    case scDebug: ret = setDebug(strtok(NULL, delims)); break;
    case scSetWait: ret = setSetWait(strtok(NULL, delims)); break;
    case scWait: ret = setWait(strtok(NULL, delims)); break;
    case scSetTimeout: ret = setTimeout(strtok(NULL, delims)); break;
    case scUpdate: ret = setUpdate(strtok(NULL, delims)); break;
    case scError: ret = rtStandardError; break;
    case scOperatorDisplay: ret = rtStandardError; break;
    case scOperatorText: ret = rtStandardError; break;
    case scTime: ret = rtStandardError; break;
    case scEStop: ret = setEStop(strtok(NULL, delims)); break;
    case scMachine: ret = setMachine(strtok(NULL, delims)); break;
    case scMode: ret = setMode(strtok(NULL, delims)); break;
    case scMist: ret = setMist(strtok(NULL, delims)); break;
    case scFlood: ret = setFlood(strtok(NULL, delims)); break;
    case scLube: ret = setLube(strtok(NULL, delims)); break;
    case scLubeLevel: ret = rtStandardError; break;
    case scSpindle: ret = setSpindle(strtok(NULL, delims)); break;
    case scBrake: ret = setBrake(strtok(NULL, delims)); break;
    case scTool: ret = rtStandardError; break;
    case scToolOffset: ret = setToolOffset(pch); break;
    case scLoadToolTable: ret = setLoadToolTable(strtok(NULL, delims)); break;
    case scHome: ret = setHome(strtok(NULL, delims)); break;
    case scJogStop: ret = setJogStop(strtok(NULL, delims)); break;
    case scJog: ret = setJog(pch); break;
    case scJogIncr: ret = setJogIncr(pch); break;
    case scFeedOverride: ret = setFeedOverride(strtok(NULL, delims)); break;
    case scAbsCmdPos: ret = rtStandardError; break;
    case scAbsActPos: ret = rtStandardError; break;
    case scRelCmdPos: ret = rtStandardError; break;
    case scRelActPos: ret = rtStandardError; break;
    case scJointPos: ret = rtStandardError; break;
    case scPosOffset: ret = rtStandardError; break;
    case scJointLimit: ret = rtStandardError; break;
    case scJointFault: ret = rtStandardError; break;
    case scJointHomed: ret = rtStandardError; break;
    case scMDI: ret = setMDI(pch); break;
    case scTskPlanInit: ret = setTaskPlanInit(pch); break;
    case scOpen: ret = setOpen(pch); break;
    case scRun: ret = setRun(strtok(NULL, delims)); break;
    case scPause: ret = setPause(pch); break;
    case scResume: ret = setResume(pch); break;
    case scAbort: ret = setAbort(pch); break;
    case scStep: ret = setStep(pch); break;    
    case scProgram:ret = rtStandardError; break;
    case scProgramLine: ret = rtStandardError; break;
    case scProgramStatus: ret = rtStandardError; break;
    case scProgramCodes: ret = rtStandardError; break;
    case scJointType: ret = rtStandardError; break;
    case scJointUnits: ret = rtStandardError; break;
    case scProgramUnits: 
    case scProgramLinearUnits: ret = rtStandardError; break;
    case scProgramAngularUnits: ret = rtStandardError; break;
    case scUserLinearUnits: ret = rtStandardError; break;
    case scUserAngularUnits: ret = rtStandardError; break;
    case scDisplayLinearUnits: ret = rtStandardError; break;
    case scDisplayAngularUnits: ret = rtStandardError; break;
    case scLinearUnitConversion: ret = setLinearUnitConversion(strtok(NULL, delims)); break;
    case scAngularUnitConversion: ret = setAngularUnitConversion(strtok(NULL, delims)); break;
    case scProbeIndex: ret = setProbeIndex(strtok(NULL, delims)); break;
    case scProbePolarity: ret = setProbePolarity(strtok(NULL, delims)); break;
    case scProbeClear: ret = setProbeClear(pch); break;
    case scProbeTripped: ret = rtStandardError; break;
    case scProbeValue: ret = rtStandardError; break;
    case scProbe: ret = setProbe(pch); break;
    case scTeleopEnable: ret = setTeleopEnable(strtok(NULL, delims)); break;
    case scKinematicsType: ret = rtStandardError; break;
    case scOverrideLimits: ret = setOverrideLimits(strtok(NULL, delims)); break;
    case scUnknown: ret = rtStandardError;
    }
  switch (ret) {
    case rtNoError:  
      if (verbose) {
        sprintf(outBuf, ackStr, pch);
        write(cliSock, outBuf, strlen(outBuf));
        }
      break;
    case rtHandledNoError: // Custom ok response already handled, take no action
      break; 
    case rtStandardError:
      sprintf(outBuf, setCmdNakStr, pch);
      write(cliSock, outBuf, strlen(outBuf));
      break;
    case rtCustomError: // Custom error response entered in buffer
      write(cliSock, outBuf, strlen(outBuf));
      break;
    case rtCustomHandledError: ;// Custom error respose handled, take no action
    }
  return 0;
}

static cmdResponseType getEcho(char *s)
{
  char *pEchoStr = "ECHO %s";
  
  if (echo) sprintf(outBuf, pEchoStr, "ON");
  else sprintf(outBuf, pEchoStr, "OFF");
  return rtNoError;
}

static cmdResponseType getVerbose(char *s)
{
  char *pVerboseStr = "VERBOSE %s";
  
  if (verbose) sprintf(outBuf, pVerboseStr, "ON");
  else sprintf(outBuf, pVerboseStr, "OFF");
  return rtNoError;
}

static cmdResponseType getEnable(char *s)
{
  char *pEnableStr = "ENABLE %s";
  
  if (connId == enabledConn) sprintf(outBuf, pEnableStr, "ON");
  else sprintf(outBuf, pEnableStr, "OFF");
  return rtNoError;
}

static cmdResponseType getConfig(char *s)
{
  char *pConfigStr = "CONFIG";

  strcpy(outBuf, pConfigStr);
  return rtNoError;
}

static cmdResponseType getCommMode(char *s)
{
  char *pCommModeStr = "COMM_MODE %s";
  
  switch (commMode) {
    case 0: sprintf(outBuf, pCommModeStr, "ASCII"); break;
    case 1: sprintf(outBuf, pCommModeStr, "BINARY"); break;
    }
  return rtNoError;
}

static cmdResponseType getCommProt(char *s)
{
  char *pCommProtStr = "COMM_PROT %s";
  
  sprintf(outBuf, pCommProtStr, version);
  return rtNoError;
}

static cmdResponseType getDebug(char *s)
{
  char *pUpdateStr = "DEBUG %d";
  
  sprintf(outBuf, pUpdateStr, emcStatus->debug);
  return rtNoError;
}

static cmdResponseType getSetWait(char *s)
{
  char *pSetWaitStr = "SET_WAIT %s";
  
  switch (emcWaitType) {
    case EMC_WAIT_NONE: sprintf(outBuf, pSetWaitStr, "NONE"); break;
    case EMC_WAIT_RECEIVED: sprintf(outBuf, pSetWaitStr, "RECEIVED"); break;
    case EMC_WAIT_DONE: sprintf(outBuf, pSetWaitStr, "DONE"); break;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType getPlat(char *s)
{
  char *pPlatStr = "PLAT %s";
  
  sprintf(outBuf, pPlatStr, "Linux");
  return rtNoError;  
}

static cmdResponseType getEStop(char *s)
{
  char *pEStopStr = "ESTOP %s";
  
  if (emcStatus->task.state == EMC_TASK_STATE_ESTOP)
    sprintf(outBuf, pEStopStr, "ON");
  else sprintf(outBuf, pEStopStr, "OFF");
  return rtNoError;
}

static cmdResponseType getTimeout(char *s)
{
  char *pTimeoutStr = "SET_TIMEOUT %f";
  
  sprintf(outBuf, pTimeoutStr, emcTimeout);
  return rtNoError;
}

static cmdResponseType getTime(char *s)
{
  char *pTimeStr = "TIME %f";
  
#if defined(LINUX_KERNEL_2_2)
  sprintf(outBuf, pTimeStr, 0.0);
#else  
  sprintf(outBuf, pTimeStr, etime());
#endif
  return rtNoError;
}

static cmdResponseType getError(char *s)
{
  char *pErrorStr = "ERROR %s";
  
  if (updateError() != 0)
    sprintf(outBuf, pErrorStr, "emc_error: bad status from EMC");
  else
    if (error_string[0] == 0)
      sprintf(outBuf, pErrorStr, "OK");
    else {
      sprintf(outBuf, pErrorStr, error_string);
      error_string[0] = 0;
      }
  return rtNoError;
}

static cmdResponseType getOperatorDisplay(char *s)
{
  char *pOperatorDisplayStr = "OPERATOR_DISPLAY %s";
  
  if (updateError() != 0)
    sprintf(outBuf, pOperatorDisplayStr, "emc_operator_display: bad status from EMC");
  else
    if (operator_display_string[0] == 0)
      sprintf(outBuf, pOperatorDisplayStr, "OK");
    else {
      sprintf(outBuf, pOperatorDisplayStr, operator_display_string);
      operator_display_string[0] = 0;
      }
  return rtNoError; 
}

static cmdResponseType getOperatorText(char *s)
{
  char *pOperatorTextStr = "OPERATOR_TEXT %s";
  
  if (updateError() != 0)
    sprintf(outBuf, pOperatorTextStr, "emc_operator_text: bad status from EMC");
  else
    if (operator_text_string[0] == 0)
      sprintf(outBuf, pOperatorTextStr, "OK");
    else {
      sprintf(outBuf, pOperatorTextStr, operator_text_string);
      operator_text_string[0] = 0;
      }
  return rtNoError; 
}

static cmdResponseType getMachine(char *s)
{
  char *pMachineStr = "MACHINE %s";
  
  if (emcStatus->task.state == EMC_TASK_STATE_ON)
    sprintf(outBuf, pMachineStr, "ON");
  else sprintf(outBuf, pMachineStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getMode(char *s)
{
  char *pModeStr = "MODE %s";
  
  switch (emcStatus->task.mode) {
    case EMC_TASK_MODE_MANUAL: sprintf(outBuf, pModeStr, "MANUAL"); break;
    case EMC_TASK_MODE_AUTO: sprintf(outBuf, pModeStr, "AUTO"); break;
    case EMC_TASK_MODE_MDI: sprintf(outBuf, pModeStr, "MDI"); break;
    default: sprintf(outBuf, pModeStr, "?");
    }
  return rtNoError; 
}

static cmdResponseType getMist(char *s)
{
  char *pMistStr = "MIST %s";
  
  if (emcStatus->io.coolant.mist == 1)
    sprintf(outBuf, pMistStr, "ON");
  else sprintf(outBuf, pMistStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getFlood(char *s)
{
  char *pFloodStr = "FLOOD %s";
  
  if (emcStatus->io.coolant.flood == 1)
    sprintf(outBuf, pFloodStr, "ON");
  else sprintf(outBuf, pFloodStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getLube(char *s)
{
  char *pLubeStr = "LUBE %s";
  
  if (emcStatus->io.lube.on == 0)
    sprintf(outBuf, pLubeStr, "OFF");
  else sprintf(outBuf, pLubeStr, "ON");
  return rtNoError; 
}

static cmdResponseType getLubeLevel(char *s)
{
  char *pLubeLevelStr = "LUBE_LEVEL %s";
  
  if (emcStatus->io.lube.level == 0)
    sprintf(outBuf, pLubeLevelStr, "LOW");
  else sprintf(outBuf, pLubeLevelStr, "OK");
  return rtNoError; 
}

static cmdResponseType getSpindle(char *s)
{
  char *pSpindleStr = "SPINDLE %s";
  
  if (emcStatus->motion.spindle.increasing > 0)
    sprintf(outBuf, pSpindleStr, "INCREASE");
  else    
    if (emcStatus->motion.spindle.increasing < 0)
      sprintf(outBuf, pSpindleStr, "DECREASE");
    else 
      if (emcStatus->motion.spindle.direction > 0)
        sprintf(outBuf, pSpindleStr, "FORWARD");
      else
        if (emcStatus->motion.spindle.direction < 0)
          sprintf(outBuf, pSpindleStr, "REVERSE");
	else sprintf(outBuf, pSpindleStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getBrake(char *s)
{
  char *pBrakeStr = "BRAKE %s";
  
  if (emcStatus->motion.spindle.brake == 1)
    sprintf(outBuf, pBrakeStr, "ON");
  else sprintf(outBuf, pBrakeStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getTool(char *s)
{
  char *pToolStr = "TOOL %d";
  
  sprintf(outBuf, pToolStr, emcStatus->io.tool.toolInSpindle);
  return rtNoError; 
}

static cmdResponseType getToolOffset(char *s)
{
  char *pToolOffsetStr = "TOOL_OFFSET %d";
  
  sprintf(outBuf, pToolOffsetStr, emcStatus->task.toolOffset.tran.z);
  return rtNoError; 
}

static cmdResponseType getAbsCmdPos(char *s)
{
  char *pAbsCmdPosStr = "ABS_CMD_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(outBuf, pAbsCmdPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.x);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.y);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.z);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.a);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.b);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.c);
    strcat(outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getAbsActPos(char *s)
{
  char *pAbsActPosStr = "ABS_ACT_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(outBuf, pAbsActPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.x);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.y);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.z);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.a);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.b);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.c);
    strcat(outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getRelCmdPos(char *s)
{
  char *pRelCmdPosStr = "REL_CMD_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(outBuf, pRelCmdPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.x - 
      emcStatus->task.origin.tran.x);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.y - 
      emcStatus->task.origin.tran.y);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.z - 
      emcStatus->task.origin.tran.z);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.a - 
      emcStatus->task.origin.a); // No rotational offsets
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.a - 
      emcStatus->task.origin.b); // No rotational offsets
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.a - 
      emcStatus->task.origin.c); // No rotational offsets
    strcat(outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getRelActPos(char *s)
{
  char *pRelActPosStr = "REL_ACT_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(outBuf, pRelActPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.x -
      emcStatus->task.origin.tran.x);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.y -
      emcStatus->task.origin.tran.y);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.z -
      emcStatus->task.origin.tran.z);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.a -
      emcStatus->task.origin.a);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.b -
      emcStatus->task.origin.b);
    strcat(outBuf, buf);
    }
  if ((axis == -1) or (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.c -
      emcStatus->task.origin.c);
    strcat(outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getJointPos(char *s)
{
  char *pJointPos = "JOINT_POS";
  int axis, i;
  char buf[16];
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  if (axis == -1) {
    strcpy(outBuf, pJointPos);
    for (i=0; i<6; i++) {
      sprintf(buf, " %f", emcStatus->motion.axis[i].input);
      strcat(outBuf, buf);
      }
    }
  else
    sprintf(outBuf, "%s %d %f", pJointPos, axis, emcStatus->motion.axis[axis].input);
  
  return rtNoError;
}

static cmdResponseType getPosOffset(char *s)
{
  char *pPosOffset = "POS_OFFSET";
  char buf[16];
  
  if (s == NULL) {
    strcpy(outBuf, pPosOffset);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.tran.x));
    strcat(outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.tran.y));
    strcat(outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.tran.z));
    strcat(outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.a));
    strcat(outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.b));
    strcat(outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.c));
    strcat(outBuf, buf);
    }
  else
    {
      switch (s[0]) {
        case 'X': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.tran.x)); break;
        case 'Y': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.tran.y)); break;
        case 'Z': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.tran.z)); break;
        case 'A': 
        case 'R': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.a)); break;
        case 'B': 
        case 'P': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.b)); break;
        case 'C': 
        case 'W': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.origin.c));
      }
      sprintf(outBuf, "%s %c %s", pPosOffset, s[0], buf);
    }
  return rtNoError;
}

static cmdResponseType getJointLimit(char *s)
{
  char *pJointLimit = "JOINT_LIMIT";
  char buf[16];
  int axis, i;
  
  if (s == NULL) {
    strcpy(outBuf, pJointLimit);
    for (i=0; i<6; i++) {
      if (emcStatus->motion.axis[i].minHardLimit)
        strcpy(buf, " MINHARD");
      else
        if (emcStatus->motion.axis[i].minSoftLimit)
	  strcpy(buf, " MINSOFT");
	else
	  if (emcStatus->motion.axis[i].maxSoftLimit)
	    strcpy(buf, " MAXSOFT");
	  else
	    if (emcStatus->motion.axis[i].maxHardLimit)
	      strcpy(buf, " MAXHARD");
	    else strcpy(buf, "OK");
      strcat(outBuf, buf);
      }
    }
  else
    {
      axis = atoi(s);
      if (emcStatus->motion.axis[axis].minHardLimit)
        strcpy(buf, "MINHARD");
      else
        if (emcStatus->motion.axis[axis].minSoftLimit)
	  strcpy(buf, "MINSOFT");
	else
	  if (emcStatus->motion.axis[axis].maxSoftLimit)
	    strcpy(buf, "MAXSOFT");
	  else
	    if (emcStatus->motion.axis[axis].maxHardLimit)
	      strcpy(buf, "MAXHARD");
	    else strcpy(buf, "OK");
      sprintf(outBuf, "%s %d %s", pJointLimit, axis, buf);
    }
  return rtNoError;
}

static cmdResponseType getJointFault(char *s)
{
  char *pJointFault = "JOINT_LIMIT";
  char buf[16];
  int axis, i;
  
  if (s == NULL) {
    strcpy(outBuf, pJointFault);
    for (i=0; i<6; i++) {
      if (emcStatus->motion.axis[i].fault)
        strcat(outBuf, " FAULT");
      else strcat(outBuf, " OK");
      }
    }
  else {
      axis = atoi(s);
      if (emcStatus->motion.axis[axis].fault)
        strcpy(buf, "FAULT");
      else strcpy(buf, "OK");
      sprintf(outBuf, "%s %d %s", pJointFault, axis, buf);
    }
  write(cliSock, outBuf, strlen(outBuf));
  return rtNoError;
}

static cmdResponseType getOverrideLimits(char *s)
{
  char *pOverrideLimits = "OVERRIDE_LIMITS %s";
  
  sprintf(outBuf, pOverrideLimits, emcStatus->motion.axis[0].overrideLimits);
  return rtNoError;
}

static cmdResponseType getJointHomed(char *s)
{
  char *pJointHomed = "JOINT_HOMED";
  char buf[16];
  int axis, i;
  
  if (s == NULL) {
    strcpy(outBuf, pJointHomed);
    for (i=0; i<6; i++) {
      if (emcStatus->motion.axis[i].homed)
        strcat(outBuf, " YES");
      else strcat(outBuf, " NO");
      }
    }
  else {
      axis = atoi(s);
      if (emcStatus->motion.axis[axis].homed)
        strcpy(buf, "YES");
      else strcpy(buf, "NO");
      sprintf(outBuf, "%s %d %s", pJointHomed, axis, buf);
    }
  return rtNoError;
}

static cmdResponseType getProgram(char *s)
{
  char *pProgram = "PROGRAM %s";
  
//  sprintf(outBuf, pProgram, progName);
//  printf("Program name = %s", emcStatus->task.file[0]);
  if (emcStatus->task.file[0] != 0)
    sprintf(outBuf, pProgram, emcStatus->task.file);
  return rtNoError;
}

static cmdResponseType getProgramLine(char *s)
{
  char *pProgramLine = "PROGRAM_LINE %d";
  int lineNo;
  
  if ((programStartLine< 0) || (emcStatus->task.readLine < programStartLine))
    lineNo = emcStatus->task.readLine;
  else
    if (emcStatus->task.currentLine > 0)
      if ((emcStatus->task.motionLine > 0) && 
        (emcStatus->task.motionLine < emcStatus->task.currentLine))
	  lineNo = emcStatus->task.motionLine;
      else lineNo = emcStatus->task.currentLine;
    else lineNo = 0;
  sprintf(outBuf, pProgramLine, lineNo);
  return rtNoError;
}

static cmdResponseType getProgramStatus(char *s)
{
  char *pProgramStatus = "PROGRAM_STATUS %s";
  
  switch (emcStatus->task.interpState) {
      case EMC_TASK_INTERP_READING:
      case EMC_TASK_INTERP_WAITING: sprintf(outBuf, pProgramStatus, "RUNNING"); break;
      case EMC_TASK_INTERP_PAUSED: sprintf(outBuf, pProgramStatus, "PAUSED"); break;
      default: sprintf(outBuf, pProgramStatus, "IDLE"); break;
    }
  return rtNoError;
}

static cmdResponseType getProgramCodes(char *s)
{
  char *pProgramCodes = "PROGRAM_CODES ";
  char buf[256];
  int code, i;
  
  buf[0] = 0;
  strcpy(outBuf, pProgramCodes);
  for (i=1; i<ACTIVE_G_CODES; i++) {
      code = emcStatus->task.activeGCodes[i];
      if (code == -1) continue;
      if (code % 10) sprintf(buf, "G%.1f ", (double) code / 10.0);
      else sprintf(buf, "G%d ", code / 10);
      strcat(outBuf, buf);
    }
  sprintf(buf, "F%.0f ", emcStatus->task.activeSettings[1]);
  strcat(outBuf, buf);
  sprintf(buf, "S%.0f", fabs(emcStatus->task.activeSettings[2]));
  strcat(outBuf, buf);
  return rtNoError;
}

static cmdResponseType getJointType(char *s)
{
  char *pJointType = "JOINT_TYPE";
  char buf[16];
  int axis, i;
  
  if (s == NULL) {
    strcpy(outBuf, pJointType);
    for (i=0; i<6; i++) {
      switch (emcStatus->motion.axis[i].axisType) {
        case EMC_AXIS_LINEAR: strcat(outBuf, " LINEAR"); break;
	case EMC_AXIS_ANGULAR: strcat(outBuf, " ANGULAR"); break;
	default: strcat(outBuf, "CUSTOM");
	}
      }
    }
  else {
      axis = atoi(s);
      switch (emcStatus->motion.axis[axis].axisType) {
        case EMC_AXIS_LINEAR: strcpy(buf, " LINEAR"); break;
	case EMC_AXIS_ANGULAR: strcpy(buf, " ANGULAR"); break;
	default: strcpy(buf, "CUSTOM");
	}
      sprintf(outBuf, "%s %d %s", pJointType, axis, buf);
    }
  return rtNoError;
}

static cmdResponseType getJointUnits(char *s)
{
  char *pJointUnits = "JOINT_UNITS";
  char buf[16];
  int axis, i;
  
  if (s == NULL) {
    strcpy(outBuf, pJointUnits);
    for (i=0; i<6; i++) {
      switch (emcStatus->motion.axis[i].axisType) {
        case EMC_AXIS_LINEAR: 
	  if (CLOSE(emcStatus->motion.axis[i].units, 1.0, LINEAR_CLOSENESS))
	    strcat(outBuf, " MM");
	  else 
	    if (CLOSE(emcStatus->motion.axis[i].units, INCH_PER_MM,
	      LINEAR_CLOSENESS)) strcat(outBuf, " INCH");
	    else
	      if (CLOSE(emcStatus->motion.axis[i].units, CM_PER_MM,
	        LINEAR_CLOSENESS)) strcat(outBuf, " CM");
	      else strcat(outBuf, " CUSTOM");
	  break;
	case EMC_AXIS_ANGULAR:
	  if (CLOSE(emcStatus->motion.axis[i].units, 1.0, ANGULAR_CLOSENESS))
	    strcat(outBuf, " DEG");
	  else
  	    if (CLOSE(emcStatus->motion.axis[i].units, RAD_PER_DEG, ANGULAR_CLOSENESS))
	      strcat(outBuf, " RAD");
	    else
	      if (CLOSE(emcStatus->motion.axis[i].units, GRAD_PER_DEG, ANGULAR_CLOSENESS))
	        strcat(outBuf, " GRAD");
	      else strcat(outBuf, " CUSTOM");
	  break;
	default: strcat(outBuf, " CUSTOM");
	}
      }
    }
  else {
      axis = atoi(s);
      switch (emcStatus->motion.axis[axis].axisType) {
        case EMC_AXIS_LINEAR: 
	  if (CLOSE(emcStatus->motion.axis[axis].units, 1.0, LINEAR_CLOSENESS))
	    strcpy(buf, "MM");
	  else 
	    if (CLOSE(emcStatus->motion.axis[axis].units, INCH_PER_MM,
	      LINEAR_CLOSENESS)) strcpy(buf, "INCH");
	    else
	      if (CLOSE(emcStatus->motion.axis[axis].units, CM_PER_MM,
	        LINEAR_CLOSENESS)) strcpy(outBuf, "CM");
	      else strcpy(buf, "CUSTOM");
	  break;
	case EMC_AXIS_ANGULAR:
	  if (CLOSE(emcStatus->motion.axis[axis].units, 1.0, ANGULAR_CLOSENESS))
	    strcpy(buf, "DEG");
	  else
  	    if (CLOSE(emcStatus->motion.axis[axis].units, RAD_PER_DEG, ANGULAR_CLOSENESS))
	      strcpy(buf, "RAD");
	    else
	      if (CLOSE(emcStatus->motion.axis[axis].units, GRAD_PER_DEG, ANGULAR_CLOSENESS))
	        strcpy(buf, "GRAD");
	      else strcpy(buf, "CUSTOM");
	  break;
	default: strcpy(buf, "CUSTOM");
      sprintf(outBuf, "%s %d %s", pJointUnits, axis, buf);
      }
    }
  return rtNoError;
}

static cmdResponseType getProgramLinearUnits(char *s)
{
  char *programUnits = "PROGRAM_UNITS %s";
  
  switch (emcStatus->task.programUnits) {
    case CANON_UNITS_INCHES: sprintf(outBuf, programUnits, "INCH"); break;
    case CANON_UNITS_MM: sprintf(outBuf, programUnits, "MM"); break;
    case CANON_UNITS_CM: sprintf(outBuf, programUnits, "CM"); break;
    default: sprintf(outBuf, programUnits, "CUSTOM"); break;
    }
  return rtNoError;
}

static cmdResponseType getProgramAngularUnits(char *s)
{
  char *programAngularUnits = "PROGRAM_ANGULAR_UNITS %s";
  
  sprintf(outBuf, programAngularUnits, "DEG");
  return rtNoError;
}

static cmdResponseType getUserLinearUnits(char *s)
{
  char *userLinearUnits = "USER_LINEAR_UNITS %s";
  
  if (CLOSE(emcStatus->motion.traj.linearUnits, 1.0, LINEAR_CLOSENESS))
    sprintf(outBuf, userLinearUnits, "MM");
  else
    if (CLOSE(emcStatus->motion.traj.linearUnits, INCH_PER_MM, LINEAR_CLOSENESS))
      sprintf(outBuf, userLinearUnits, "INCH");
    else
      if (CLOSE(emcStatus->motion.traj.linearUnits, CM_PER_MM, LINEAR_CLOSENESS))
        sprintf(outBuf, userLinearUnits, "CM");
      else
        sprintf(outBuf, userLinearUnits, "CUSTOM");
  return rtNoError;
}

static cmdResponseType getUserAngularUnits(char *s)
{
  char *pUserAngularUnits = "USER_ANGULAR_UNITS %s";
  
  if (CLOSE(emcStatus->motion.traj.angularUnits, 1.0, ANGULAR_CLOSENESS))
    sprintf(outBuf, pUserAngularUnits, "DEG");
  else
    if (CLOSE(emcStatus->motion.traj.angularUnits, RAD_PER_DEG, ANGULAR_CLOSENESS))
      sprintf(outBuf, pUserAngularUnits, "RAD");
    else
      if (CLOSE(emcStatus->motion.traj.angularUnits, GRAD_PER_DEG, ANGULAR_CLOSENESS))
        sprintf(outBuf, pUserAngularUnits, "GRAD");
      else
        sprintf(outBuf, pUserAngularUnits, "CUSTOM");
  return rtNoError;
}

static cmdResponseType getDisplayLinearUnits(char *s)
{
  char *pDisplayLinearUnits = "DISPLAY_LINEAR_UNITS %s";
  
  switch (linearUnitConversion) {
      case LINEAR_UNITS_INCH: sprintf(outBuf, pDisplayLinearUnits, "INCH"); break;
      case LINEAR_UNITS_MM: sprintf(outBuf, pDisplayLinearUnits, "MM"); break;
      case LINEAR_UNITS_CM: sprintf(outBuf, pDisplayLinearUnits, "CM"); break;
      case LINEAR_UNITS_AUTO: 
        switch (emcStatus->task.programUnits) {
	    case CANON_UNITS_MM: sprintf(outBuf, pDisplayLinearUnits, "MM"); break;
	    case CANON_UNITS_INCHES: sprintf(outBuf, pDisplayLinearUnits, "INCH"); break;
	    case CANON_UNITS_CM: sprintf(outBuf, pDisplayLinearUnits, ""); break;
	    default: sprintf(outBuf, pDisplayLinearUnits, "CUSTOM");
	  }
        break;
      default: sprintf(outBuf, pDisplayLinearUnits, "CUSTOM");
    }
  return rtNoError;
}

static cmdResponseType getDisplayAngularUnits(char *s)
{
  char *pDisplayAngularUnits = "DISPLAY_ANGULAR_UNITS %s";
  
  switch (angularUnitConversion) {
      case ANGULAR_UNITS_DEG: sprintf(outBuf, pDisplayAngularUnits, "DEG"); break;
      case ANGULAR_UNITS_RAD: sprintf(outBuf, pDisplayAngularUnits, "RAD"); break;
      case ANGULAR_UNITS_GRAD: sprintf(outBuf, pDisplayAngularUnits, "GRAD"); break;
      case ANGULAR_UNITS_AUTO: sprintf(outBuf, pDisplayAngularUnits, "DEG"); break; 
      default: sprintf(outBuf, pDisplayAngularUnits, "CUSTOM");
    }
  return rtNoError;
}

static cmdResponseType getLinearUnitConversion(char *s)
{
  char *pLinearUnitConversion = "LINEAR_UNIT_CONVERSION %s";
  
  switch (linearUnitConversion) {
      case LINEAR_UNITS_INCH: sprintf(outBuf, pLinearUnitConversion, "INCH"); break;
      case LINEAR_UNITS_MM: sprintf(outBuf, pLinearUnitConversion, "MM"); break;
      case LINEAR_UNITS_CM: sprintf(outBuf, pLinearUnitConversion, "CM"); break;
      case LINEAR_UNITS_AUTO: sprintf(outBuf, pLinearUnitConversion, "AUTO"); break;
      default: sprintf(outBuf, pLinearUnitConversion, "CUSTOM"); 
    }
  return rtNoError;
}

static cmdResponseType getAngularUnitConversion(char *s)
{
  char *pAngularUnitConversion = "ANGULAR_UNIT_CONVERSION %s";
  
  switch (angularUnitConversion) {
      case ANGULAR_UNITS_DEG: sprintf(outBuf, pAngularUnitConversion, "DEG"); break;
      case ANGULAR_UNITS_RAD: sprintf(outBuf, pAngularUnitConversion, "RAD"); break;
      case ANGULAR_UNITS_GRAD: sprintf(outBuf, pAngularUnitConversion, "GRAD"); break;
      case ANGULAR_UNITS_AUTO: sprintf(outBuf, pAngularUnitConversion, "AUTO"); break;
      default: sprintf(outBuf, pAngularUnitConversion, "CUSTOM");
    }
  return rtNoError;
}

static cmdResponseType getProbeIndex(char *s)
{
  char *pProbeIndex = "PROBE_INDEX %d";
  
  sprintf(outBuf, pProbeIndex, emcStatus->motion.traj.probe_index);  
  return rtNoError;
}

static cmdResponseType getProbePolarity(char *s)
{
  char *pProbePolarity = "PROBE_POLARITY %d";
  
  sprintf(outBuf, pProbePolarity, emcStatus->motion.traj.probe_polarity);  
  return rtNoError;
}

static cmdResponseType getProbeValue(char *s)
{
  char *pProbeValue = "PROBE_VALUE %d";
  
  sprintf(outBuf, pProbeValue, emcStatus->motion.traj.probeval);  
  return rtNoError;
}

static cmdResponseType getProbeTripped(char *s)
{
  char *pProbeTripped = "PROBE_TRIPPED %d";
  
  sprintf(outBuf, pProbeTripped, emcStatus->motion.traj.probe_tripped);  
  return rtNoError;
}

static cmdResponseType getTeleopEnable(char *s)
{
  char *pTeleopEnable = "TELEOP_ENABLE %s";
  
  if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE_TELEOP)
    sprintf(outBuf, pTeleopEnable, "YES");  
   else sprintf(outBuf, pTeleopEnable, "NO");
  return rtNoError;
}

static cmdResponseType getKinematicsType(char *s)
{
  char *pKinematicsType = "KINEMATICS_TYPE %d";
  
  sprintf(outBuf, pKinematicsType, emcStatus->motion.traj.kinematics_type);  
  return rtNoError;
}

static cmdResponseType getFeedOverride(char *s)
{
  char *pFeedOverride = "FEED_OVERRIDE %d";
  int percent;
  
  percent = (int)floor(emcStatus->motion.traj.scale * 100.0 + 0.5);
  sprintf(outBuf, pFeedOverride, percent);
  return rtNoError;
}

int commandGet()
{
  static char *setNakStr = "GET NAK\r\n";
  static char *setCmdNakStr = "GET %s NAK\r\n";
  setCommandType cmd;
  char *pch;
  cmdResponseType ret = rtNoError;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) {
    write(cliSock, setNakStr, strlen(setNakStr));
    return 0;
    }
  if (emcUpdateType == EMC_UPDATE_AUTO) updateStatus();
  strupr(pch);
  cmd = lookupSetCommand(pch);
  if (cmd > scIni)
    if (emcUpdateType == EMC_UPDATE_AUTO) updateStatus();
  switch (cmd) {
    case scEcho: ret = getEcho(pch); break;
    case scVerbose: ret = getVerbose(pch); break;
    case scEnable: ret = getEnable(pch); break;
    case scConfig: ret = getConfig(pch); break;
    case scCommMode: ret = getCommMode(pch); break;
    case scCommProt: ret = getCommProt(pch); break;
    case scIniFile: break;
    case scPlat: ret = getPlat(pch); break;
    case scIni: break;
    case scDebug: ret = getDebug(pch); break;
    case scSetWait: ret = getSetWait(pch); break;
    case scWait: break;
    case scSetTimeout: ret = getTimeout(pch); break;
    case scUpdate: break;
    case scError: ret = getError(pch); break;
    case scOperatorDisplay: ret = getOperatorDisplay(pch); break;
    case scOperatorText: ret = getOperatorText(pch); break;
    case scTime: ret = getTime(pch); break;
    case scEStop: ret = getEStop(pch); break;
    case scMachine: ret = getMachine(pch); break;
    case scMode: ret = getMode(pch); break;
    case scMist: ret = getMist(pch); break;
    case scFlood: ret = getFlood(pch); break;
    case scLube: ret = getLube(pch); break;
    case scLubeLevel: ret = getLubeLevel(pch); break;
    case scSpindle: ret = getSpindle(pch); break;
    case scBrake: ret = getBrake(pch); break;
    case scTool: ret = getTool(pch); break;
    case scToolOffset: ret = getToolOffset(pch); break;
    case scLoadToolTable: break;
    case scHome: ret = rtStandardError; break;
    case scJogStop: ret = rtStandardError; break;
    case scJog: ret = rtStandardError; break;
    case scJogIncr: ret = rtStandardError; break;
    case scFeedOverride: ret = getFeedOverride(pch); break;
    case scAbsCmdPos: ret = getAbsCmdPos(strtok(NULL, delims)); break;
    case scAbsActPos: ret = getAbsActPos(strtok(NULL, delims)); break;
    case scRelCmdPos: ret = getRelCmdPos(strtok(NULL, delims)); break;
    case scRelActPos: ret = getRelActPos(strtok(NULL, delims)); break;
    case scJointPos: ret = getJointPos(strtok(NULL, delims)); break;
    case scPosOffset: ret = getPosOffset(strtok(NULL, delims)); break;
    case scJointLimit: ret = getJointLimit(strtok(NULL, delims)); break;
    case scJointFault: ret = getJointFault(strtok(NULL, delims)); break;
    case scJointHomed: ret = getJointHomed(strtok(NULL, delims)); break;
    case scMDI: ret = rtStandardError; break;
    case scTskPlanInit: ret = rtStandardError; break;
    case scOpen: ret = rtStandardError; break;
    case scRun: ret = rtStandardError; break;
    case scPause: ret = rtStandardError; break;
    case scResume: ret = rtStandardError; break;
    case scStep: ret = rtStandardError; break;
    case scAbort: ret = rtStandardError; break;
    case scProgram: ret = getProgram(pch); break;
    case scProgramLine: ret = getProgramLine(pch); break;
    case scProgramStatus: ret = getProgramStatus(pch); break;
    case scProgramCodes: ret = getProgramCodes(pch); break;
    case scJointType: ret = getJointType(strtok(NULL, delims)); break;
    case scJointUnits: ret = getJointUnits(strtok(NULL, delims)); break;
    case scProgramUnits: 
    case scProgramLinearUnits: ret = getProgramLinearUnits(pch); break;
    case scProgramAngularUnits: ret = getProgramAngularUnits(pch); break;
    case scUserLinearUnits: ret = getUserLinearUnits(pch); break;
    case scUserAngularUnits: ret = getUserAngularUnits(pch); break;
    case scDisplayLinearUnits: ret = getDisplayLinearUnits(pch); break;
    case scDisplayAngularUnits: ret = getDisplayAngularUnits(pch); break;
    case scLinearUnitConversion: ret = getLinearUnitConversion(pch); break;
    case scAngularUnitConversion: ret = getAngularUnitConversion(pch); break;
    case scProbeIndex: ret = getProbeIndex(pch); break;
    case scProbePolarity: ret = getProbePolarity(pch); break;
    case scProbeClear: break;
    case scProbeTripped: ret = getProbeTripped(pch); break;
    case scProbeValue: ret = getProbeValue(pch); break;
    case scProbe: break;
    case scTeleopEnable: ret = getTeleopEnable(pch); break;
    case scKinematicsType: ret = getKinematicsType(pch); break;
    case scOverrideLimits: ret = getOverrideLimits(pch); break;
    case scUnknown: ret = rtStandardError;
    }
  switch (ret) {
    case rtNoError: // Standard ok response, just write value in buffer
      sockWrite();
      break;
    case rtHandledNoError: // Custom ok response already handled, take no action
      break; 
    case rtStandardError: // Standard error response
      sprintf(outBuf, setCmdNakStr, pch); 
      sockWrite();
      break;
    case rtCustomError: // Custom error response entered in buffer
      sockWrite();
      break;
    case rtCustomHandledError: ;// Custom error respose handled, take no action
    }
  return 0;
}

int commandQuit()
{
  printf("Closing connection with %s\n", hostName);
  return -1;
}

commandTokenType lookupToken(char *s)
{
  commandTokenType i = cmdHello;
  int temp;
  
  while (i < cmdUnknown) {
    if (strcmp(commands[i], s) == 0) return i;
//    (int)i += 1;
    temp = i;
    temp++;
    i = (commandTokenType) temp;
    }
  return i;
}
  
int parseCommand()
{
  int ret = 0;
  char *pch;
  static char *helloNakStr = "HELLO NAK\r\n";
  static char *helloAckStr = "HELLO ACK EMCNETSVR 1.0\r\n";
  static char *setNakStr = "SET NAK\r\n";
    
  pch = strtok(inBuf, delims);
  if (pch != NULL) {
    strupr(pch);
    switch (lookupToken(pch)) {
      case cmdHello: 
        if (commandHello() == -1)
          write(cliSock, helloNakStr, strlen(helloNakStr));
        else write(cliSock, helloAckStr, strlen(helloAckStr));
        break;
      case cmdGet: 
        ret = commandGet();
        break;
      case cmdSet:
        if (!linked)
	  write(cliSock, setNakStr, strlen(setNakStr)); 
        else ret = commandSet();
        break;
      case cmdQuit: 
        ret = commandQuit();
        break;
      case cmdUnknown: ret = -2;
      }
    }
  return ret;
}  

int readClient()
{
  char str[1600];
  char buf[1600];
  unsigned int i, j;
  int len;
//  connectionRecType *context;
  
  
//  res = 1;
//  context = (connectionRecType *) malloc(sizeof(connectionRecType));
  cliSock = client_sockfd;
  linked = false;
  echo = true;
  verbose = false;
  strcpy(version, "1.0");
  strcpy(hostName, "Default");
  connCount++;
  connId = connCount;
//  enable = false;
  commMode = 0;
  commProt = 0;
  inBuf[0] = 0;
  buf[0] = 0;
  
  while (1) {
    len = read(client_sockfd, &str, 1600);
    str[len] = 0;
    strcat(buf, str);
    if (!memchr(str, 0x0d, strlen(str))) continue;
    if (echo && linked)
      write(cliSock, &buf, strlen(buf));
    i = 0;
    j = 0;
    while (i <= strlen(buf)) {
      if ((buf[i] != '\n') && (buf[i] != '\r')) {
        inBuf[j] = buf[i];
	j++;
	}
      else
        if (j > 0)
          {
  	    inBuf[j] = 0;
            if (parseCommand() == -1)
              {
               close(client_sockfd);
               exit(0);
//               free(context);
               return 0;
              }
	    j = 0;
	}
        i++;	
      }
    buf[0] = 0;
    } 
  return 0;
}

int sockMain()
{
    while (1) {
      
      client_len = sizeof(client_address);
      client_sockfd = accept(server_sockfd,
        (struct sockaddr *)&client_address, &client_len);
      if (fork() == 0) {
          readClient();
        }
      else {
        close(client_sockfd); }
     }
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
    initSockets();
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

    // attach our quit function to exit
//    Tcl_CreateExitHandler(thisQuit, (ClientData) 0);

    // attach our quit function to SIGINT
    signal(SIGINT, sigQuit);

    // TclX_Main(argc, argv, Tcl_AppInit);
    if (useSockets) sockMain();
//    else Tk_Main(argc, argv, Tcl_AppInit);

    return 0;
}
