/********************************************************************
* Description: emcrsh.cc  (source for linuxcncrsh)
*   Extended telnet based LinuxCNC interface
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Further derived from work by jmkasunich
*
* Author: Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2006-2008 All rights reserved.
*
* Last change:
********************************************************************/

#define _REENTRANT

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

#include <getopt.h>

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "rcs_print.hh"
#include "timer.hh"             // etime()
#include "shcom.hh"             // NML Messaging functions

/*
  Using linuxcncrsh:

  linuxcncrsh {-- --port <port number> --name <server name> --connectpw <password>
             --enablepw <password> --sessions <max sessions> --path <path>
             -ini<inifile>}

  With -- --port Waits for socket connections (Telnet) on specified socket, without port
            uses default port 5007.
  With -- --name <server name> Sets the server name to specified name for Hello.
  With -- --connectpw <password> Sets the connection password to 'password'. Default EMC
  With -- --enablepw <password> Sets the enable password to 'password'. Default EMCTOO
  With -- --sessions <max sessions> Sets the maximum number of simultaneous connextions
            to max sessions. Default is no limit (-1).
  With -- --path Sets the base path to program (G-Code) files, default is "../../nc_files/".
            Make sure to include the final slash (/).
  With -- -ini <inifile>, uses inifile instead of emc.ini. 

  There are six commands supported, Where the commands set and get contain LinuxCNC
  specific sub-commands based on the commands supported by linuxcncrsh, but where the 
  usual prefix ( "emc_") is omitted. Commands and most parameters are not case sensitive.
  The exceptions are passwords, file paths and text strings.
  
  The supported commands are as follows:
  
  ==> HELLO <==
  
  Hello <password> <client> <version>
  If a valid password was entered the server will respond with
  
  HELLO ACK <Server Name> <Server Version>
  
  Where server name and server version are looked up from the implementation.
  if an invalid password or any other syntax error occurs then the server 
  responds with:
  
  HELLO NAK
  
  ==> Get <==
  
  The get command includes one of the LinuxCNC sub-commands, described below and
  zero or more additional parameters. 
  
  ==> Set <==
  
  The set command inclides one of the LinuxCNC sub-commands, described below and
  one or more additional parameters.
  
  ==> Quit <==
  
  The quit command disconnects the associated socket connection.
  
  ==> Shutdown <==
  
  The shutdown command tells LinuxCNC to shutdown before quitting the connection. This
  command may only be issued if the Hello has been successfully negotiated and the
  connection has control of the CNC (see enable sub-command below). This command
  has no parameters.
  
  ==> Help <==
  
  The help command will return help information in text format over the telnet
  connection. If no parameters are specified, it will itemize the available commands.
  If a command is specified, it will provide usage information for the specified
  command. Help will respond regardless of whether a "Hello" has been
  successsfully negotiated.
  
  
  LinuxCNC sub-commands:
  
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
  Returns the path and file name of the current configuration inifile.

  plat
  Returns the platform for which this was compiled, e.g., linux_2_0_36

  ini <var> <section>
  Returns the string value of <var> in section <section>, in EMC_INIFILE

  debug {<new value>}
  With get, returns the integer value of EMC_DEBUG, in LinuxCNC. Note that
  it may not be true that the local EMC_DEBUG variable here (in linuxcncrsh and
  the GUIs that use it) is the same as the EMC_DEBUG value in the LinuxCNC. This
  can happen if LinuxCNC is started from one .ini file, and the GUI is started
  with another that has a different value for DEBUG.
  With set, sends a command to the LinuxCNC to set the new debug level,
  and sets the EMC_DEBUG global here to the same value. This will make
  the two values the same, since they really ought to be the same.

  set_wait none | received | done
  Set the wait for commands to return to be right away (none), after the
  command was sent and received (received), or after the command was
  done (done).

  wait received | done
  Force a wait for the previous command to be received, or done. This lets
  you wait in the event that "emc_set_wait none" is in effect.

  set_timeout <timeout>
  Set the timeout for commands to return to <timeout>, in seconds. Timeout
  is a real number. If it's <= 0.0, it means wait forever. Default is 0.0,
  wait forever.

  update (none) | none | auto
  With no arg, forces an update of the LinuxCNC status. With "none", doesn't
  cause an automatic update of status with other emc_ words. With "auto",
  makes emc_ words automatically update status before they return values.

  error
  Returns the current LinuxCNC error string, or "ok" if no error.

  operator_display
  Returns the current LinuxCNC operator display string, or "ok" if none.

  operator_text
  Returns the current LinuxCNC operator text string, or "ok" if none.

  time
  Returns the time, in seconds, from the start of the epoch. This starting
  time depends on the platform.

  estop (none) | on | off
  With no arg, returns the estop setting as "on" or "off". Otherwise,
  sends an estop on or off command.

  machine (none) | on | off
  With no arg, returns the machine setting as "on" or "off". Otherwise,
  sends a machine on or off command.

  mode (none) | manual | auto | mdi
  With no arg, returns the mode setting as "manual", "auto", or "mdi".
  Otherwise, sends a mode manual, auto, or mdi command.

  mist (none) | on | off
  With no arg, returns the mist setting as "on" or "off". Otherwise,
  sends a mist on or off command.

  flood (none) | on | off
  With no arg, returns the flood setting as "on" or "off". Otherwise,
  sends a flood on or off command.

  lube (none) | on | off
  With no arg, returns the lubricant pump setting as "on" or "off".
  Otherwise, sends a lube on or off command.

  lube_level
  Returns the lubricant level sensor reading as "ok" or "low".

  spindle (none) | forward | reverse | increase | decrease | constant | off
  With no arg, returns the value of the spindle state as "forward",
  "reverse", "increase", "decrease", or "off". With arg, sends the spindle
  command. Note that "increase" and "decrease" will cause a speed change in
  the corresponding direction until a "constant" command is sent.

  brake (none) | on | off
  With no arg, returns the brake setting. Otherwise sets the brake.

  tool
  Returns the id of the currently loaded tool

  tool_offset
  Returns the currently applied tool length offset

  load_tool_table <file>
  Loads the tool table specified by <file>

  home 0 | 1 | 2 | ... 
  Homes the indicated joint.

  jog_stop joint_number|axis_letter
  Stop the axis or joint jog

  jog joint_number|axis_letter speed
  Jog the indicated joint or axis at <speed>; sign of speed is direction

  jog_incr joint_number|axis_letter <speed> <incr>
  Jog the indicated joint or axis by increment <incr> at the <speed>; sign of
  speed is direction

  feed_override {<percent>}
  With no args, returns the current feed override, as a percent. With
  argument, set the feed override to be the percent value

  spindle_override {<percent>}
  With no args, returns the current spindle override, as a percent. With
  argument, set the spindle override to be the percent value

  abs_cmd_pos 0 | 1 | ...
  Returns double obj containing the XYZ-SXYZ commanded pos in abs coords,
  at given index, 0 = X, etc.

  abs_act_pos
  Returns double objs containing the XYZ-SXYZ actual pos in abs coords

  rel_cmd_pos 0 | 1 | ...
  Returns double obj containing the XYZ-SXYZ commanded pos in rel coords,
  at given index, 0 = X, etc., including tool length offset

  rel_act_pos
  Returns double objs containing the XYZ-SXYZ actual pos in rel coords,
  including tool length offset

  joint_pos
  Returns double objs containing the actual pos in absolute coords of individual
  joint/slider positions, excludes tool length offset

  pos_offset X | Y | Z | R | P | W
  Returns the position offset associated with the world coordinate provided

  joint_limit 0 | 1 | ...
  Returns "ok", "minsoft", "minhard", "maxsoft", "maxhard"

  joint_fault 0 | 1 | ...
  Returns "ok" or "fault"

  joint_homed 0 | 1 | ...
  Returns "homed", "not"

  mdi <string>
  Sends the <string> as an MDI command

  task_plan_init
  Initializes the program interpreter

  open <filename>
  Opens the named file

  run {<start line>}
  Without start line, runs the opened program from the beginning. With
  start line, runs from that line. A start line of -1 runs in verify mode.

  pause
  Pause program execution

  resume
  Resume program execution

  step
  Step the program one line

  program
  Returns the name of the currently opened program, or "none"

  program_line
  Returns the currently executing line of the program

  program_status
  Returns "idle", "running", or "paused"

  program_codes
  Returns the string for the currently active program codes

  joint_type <joint>
  Returns "linear", "angular", or "custom" for the type of the specified joint

  joint_units <joint>
  Returns "inch", "mm", "cm", or "deg", "rad", "grad", or "custom",
  for the corresponding native units of the specified axis. The type
  of the axis (linear or angular) is used to resolve which type of units
  are returned. The units are obtained heuristically, based on the
  EMC_AXIS_STAT::units numerical value of user units per mm or deg.
  For linear joints, something close to 0.03937 is deemed "inch",
  1.000 is "mm", 0.1 is "cm", otherwise it's "custom".
  For angular joints, something close to 1.000 is deemed "deg",
  PI/180 is "rad", 100/90 is "grad", otherwise it's "custom".
 
  program_units
  program_linear_units
  Returns "inch", "mm", "cm", or "none", for the corresponding linear 
  units that are active in the program interpreter.

  program_angular_units
  Returns "deg", "rad", "grad", or "none" for the corresponding angular
  units that are active in the program interpreter.

  user_linear_units
  Returns "inch", "mm", "cm", or "custom", for the
  corresponding native user linear units of the LinuxCNC trajectory
  level. This is obtained heuristically, based on the
  EMC_TRAJ_STAT::linearUnits numerical value of user units per mm.
  Something close to 0.03937 is deemed "inch", 1.000 is "mm", 0.1 is
  "cm", otherwise it's "custom".

  user_angular_units
  Returns "deg", "rad", "grad", or "custom" for the corresponding native
  user angular units of the LinuxCNC trajectory level. Like with linear units,
  this is obtained heuristically.

  display_linear_units
  display_angular_units
  Returns "inch", "mm", "cm", or "deg", "rad", "grad", or "custom",
  for the linear or angular units that are active in the display. 
  This is effectively the value of linearUnitConversion or
  angularUnitConversion, resp.

  linear_unit_conversion {inch | mm | cm | auto}
  With no args, returns the unit conversion active. With arg, sets the
  units to be displayed. If it's "auto", the units to be displayed match
  the program units.
 
  angular_unit_conversion {deg | rad | grad | auto}
  With no args, returns the unit conversion active. With arg, sets the
  units to be displayed. If it's "auto", the units to be displayed match
  the program units.

  probe_clear
  Clear the probe tripped flag.

  probe_tripped
  Has the probe been tripped since the last clear.

  probe_value
  Value of current probe signal. (read-only)

  probe
  Move toward a certain location. If the probe is tripped on the way stop
  motion, record the position and raise the probe tripped flag.

  teleop_enable
  Should motion run in teleop mode? (No args
  gets it, one arg sets it.)

  kinematics_type
  returns the type of kinematics functions used identity=1, serial=2,
  parallel=3, custom=4
  
  override_limits on | off
  If parameter is on, disables end of travel hardware limits to allow
  jogging off of a limit. If parameters is off, then hardware limits
  are enabled.

  optional_stop  none | 0 | 1
  returns state of optional setop, sets it or deactivates it (used to stop/continue on M1)
  
  <------------------------------------------------>
  
  To Do:
  
  1> Load / save connect and enable passwords to file.
  2> Implement commands to set / get passwords
  3> Get enable to tell peer connections which connection has control.
  4> Get shutdown to notify LinuxCNC to actually shutdown.
*/

// EMC_STAT *emcStatus;

typedef enum {
  cmdHello, cmdSet, cmdGet, cmdQuit, cmdShutdown, cmdHelp, cmdUnknown} commandTokenType;
  
typedef enum {
  scEcho, scVerbose, scEnable, scConfig, scCommMode, scCommProt, scIniFile,
  scPlat, scIni, scDebug, scSetWait, scWait, scSetTimeout, scUpdate, scError,
  scOperatorDisplay, scOperatorText, scTime, scEStop, scMachine, scMode,
  scMist, scFlood, scLube, scLubeLevel, scSpindle, scBrake, scTool, scToolOffset,
  scLoadToolTable, scHome, scJogStop, scJog, scJogIncr, scFeedOverride,
  scAbsCmdPos, scAbsActPos, scRelCmdPos, scRelActPos, scJointPos, scPosOffset,
  scJointLimit, scJointFault, scJointHomed, scMDI, scTskPlanInit, scOpen, scRun,
  scPause, scResume, scStep, scAbort, scProgram, scProgramLine, scProgramStatus,
  scProgramCodes, scJointType, scJointUnits, scProgramUnits, scProgramLinearUnits,
  scProgramAngularUnits, scUserLinearUnits, scUserAngularUnits, scDisplayLinearUnits,
  scDisplayAngularUnits, scLinearUnitConversion,  scAngularUnitConversion, scProbeClear, 
  scProbeTripped, scProbeValue, scProbe, scTeleopEnable, scKinematicsType, scOverrideLimits, 
  scSpindleOverride, scOptionalStop, scUnknown
  } setCommandType;
  
typedef enum {
  rtNoError, rtHandledNoError, rtStandardError, rtCustomError, rtCustomHandledError
  } cmdResponseType;
  
typedef struct {  
  int cliSock;
  char hostName[80];
  char version[8];
  bool linked;
  bool echo;
  bool verbose;
  bool enabled;
  int commMode;
  int commProt;
  char inBuf[256];
  char outBuf[4096];
  char progName[PATH_MAX];} connectionRecType;

int port = 5007;
int server_sockfd;
socklen_t server_len, client_len;
struct sockaddr_in server_address;
struct sockaddr_in client_address;
bool useSockets = true;
int tokenIdx;
const char *delims = " \n\r\0";
int enabledConn = -1;
char pwd[16] = "EMC\0";
char enablePWD[16] = "EMCTOO\0";
char serverName[24] = "EMCNETSVR\0";
int sessions = 0;
int maxSessions = -1;

const char *setCommands[] = {
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
  "LINEAR_UNIT_CONVERSION", "ANGULAR_UNIT_CONVERSION", "PROBE_CLEAR", "PROBE_TRIPPED", 
  "PROBE_VALUE", "PROBE", "TELEOP_ENABLE", "KINEMATICS_TYPE", "OVERRIDE_LIMITS", 
  "SPINDLE_OVERRIDE", "OPTIONAL_STOP", ""};

const char *commands[] = {"HELLO", "SET", "GET", "QUIT", "SHUTDOWN", "HELP", ""};

struct option longopts[] = {
  {"help", 0, NULL, 'h'},
  {"port", 1, NULL, 'p'},
  {"name", 1, NULL, 'n'},
  {"sessions", 1, NULL, 's'},
  {"connectpw", 1, NULL, 'w'},
  {"enablepw", 1, NULL, 'e'},
  {"path", 1, NULL, 'd'},
  {0,0,0,0}};

/* static char *skipWhite(char *s)
{
    while (isspace(*s)) {
	s++;
    }
    return s;
} */

static void thisQuit()
{
    EMC_NULL emc_null_msg;

    if (emcStatusBuffer != 0) {
	// wait until current message has been received
	emcCommandWaitReceived();
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

static int initSockets()
{
  int optval = 1;

  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);
  server_len = sizeof(server_address);
  bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
  listen(server_sockfd, 5);

  // ignore SIGCHLD
  {
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);
  }

  return 0;
}

static void sigQuit(int sig)
{
    thisQuit();
}

static int sockWrite(connectionRecType *context)
{
   strcat(context->outBuf, "\r\n");
   return write(context->cliSock, context->outBuf, strlen(context->outBuf));
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

static int commandHello(connectionRecType *context)
{
  char *pch;

  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  if (strcmp(pch, pwd) != 0) return -1;

  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  strncpy(context->hostName, pch, sizeof(context->hostName));
  if (context->hostName[sizeof(context->hostName)-1] != '\0') {
    return -1;
  }

  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  strncpy(context->version, pch, sizeof(context->version));
  if (context->version[sizeof(context->version)-1] != '\0') {
    return -1;
  }

  context->linked = true;
  printf("Connected to %s\n", context->hostName);
  return 0;
}

static int checkOnOff(char *s)
{
  static const char *onStr = "ON";
  static const char *offStr = "OFF";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, onStr) == 0) return 0;
  if (strcmp(s, offStr) == 0) return 1;
  return -1;
}

static int checkBinaryASCII(char *s)
{
  static const char *binaryStr = "BINARY";
  static const char *ASCIIStr = "ASCII";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, ASCIIStr) == 0) return 0;
  if (strcmp(s, binaryStr) == 0) return 1;
  return -1;
}

static int checkReceivedDoneNone(char *s)
{
  static const char *receivedStr = "RECEIVED";
  static const char *doneStr = "DONE";
  static const char *noneStr = "NONE";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, receivedStr) == 0) return 0;
  if (strcmp(s, doneStr) == 0) return 1;
  if (strcmp(s, noneStr) == 0) return 2;
  return -1;
}

static int checkNoneAuto(char *s)
{
  static const char *noneStr = "NONE";
  static const char *autoStr = "AUTO";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, noneStr) == 0) return 0;
  if (strcmp(s, autoStr) == 0) return 1;
  return -1;
}

static int checkManualAutoMDI(char *s)
{
  static const char *manualStr = "MANUAL";
  static const char *autoStr = "AUTO";
  static const char *mDIStr = "MDI";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, manualStr) == 0) return 0;
  if (strcmp(s, autoStr) == 0) return 1;
  if (strcmp(s, mDIStr) == 0) return 2;
  return -1;
}

static int checkSpindleStr(char *s)
{
  static const char *forwardStr = "FORWARD";
  static const char *reverseStr = "REVERSE";
  static const char *increaseStr = "INCREASE";
  static const char *decreaseStr = "DECREASE";
  static const char *constantStr = "CONSTANT";
  static const char *offStr = "OFF";
  
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
  static const char *inchStr = "INCH";
  static const char *mmStr = "MM";
  static const char *cmStr = "CM";
  static const char *autoStr = "AUTO";
  static const char *customStr = "CUSTOM";
  
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
  static const char *degStr = "DEG";
  static const char *radStr = "RAD";
  static const char *gradStr = "GRAD";
  static const char *autoStr = "AUTO";
  static const char *customStr = "CUSTOM";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, degStr) == 0) return 0;
  if (strcmp(s, radStr) == 0) return 1;
  if (strcmp(s, gradStr) == 0) return 2;
  if (strcmp(s, autoStr) == 0) return 3;
  if (strcmp(s, customStr) == 0) return 4;
  return -1;
}

static cmdResponseType setEcho(char *s, connectionRecType *context)
{
   
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: context->echo = true; break;
     case 1: context->echo = false;
     }
   return rtNoError;
}

static cmdResponseType setVerbose(char *s, connectionRecType *context)
{
   
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: context->verbose = true; break;
     case 1: context->verbose = false;
     }
   return rtNoError;
}

static cmdResponseType setEnable(char *s, connectionRecType *context)
{
  
   if (s && (strcmp(s, enablePWD) == 0) ) {
     enabledConn = context->cliSock;
     context->enabled = true;
     return rtNoError;
     }
   else 
     if (checkOnOff(s) == 1) {
       context->enabled = false;
       enabledConn = -1;
       return rtNoError;
       }
     else return rtStandardError;
}

static cmdResponseType setConfig(char *s, connectionRecType *context)
{
  return rtNoError;
}

static cmdResponseType setCommMode(char *s, connectionRecType *context)
{
  int ret;
  
  ret = checkBinaryASCII(s);
  if (ret == -1) return rtStandardError;
  context->commMode = ret;
  return rtNoError;
}

static cmdResponseType setCommProt(char *s, connectionRecType *context)
{
  char *pVersion;
  
  pVersion = strtok(NULL, delims);
  if (pVersion == NULL) return rtStandardError;
  strcpy(context->version, pVersion);
  return rtNoError;
}

static cmdResponseType setDebug(char *s, connectionRecType *context)
{
  char *pLevel;
  int level;
  
  pLevel = strtok(NULL, delims);
  if (pLevel == NULL) return rtStandardError;
  if (sscanf(pLevel, "%i", &level) == -1) return rtStandardError;
  else sendDebug(level);
  return rtNoError;
}

static cmdResponseType setSetWait(char *s, connectionRecType *context)
{
   switch (checkReceivedDoneNone(s)) {
     case -1: return rtStandardError;
     case 0: {
       emcWaitType = EMC_WAIT_RECEIVED;
       break;
     }
     case 1: {
       emcWaitType = EMC_WAIT_DONE;
       break;
     }
     case 2: {
       fprintf(stderr, "linuxcncrsh: 'set set_wait' asked for 'none', but that setting has been removed as it may cause commands to be lost\n");
       return rtStandardError;
       break;
     }
   }
   return rtNoError;
}

static cmdResponseType setMachine(char *s, connectionRecType *context)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendMachineOn(); break;
     case 1: sendMachineOff();
     }
   return rtNoError;
}

static cmdResponseType setEStop(char *s, connectionRecType *context)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendEstop(); break;
     case 1: sendEstopReset();
     }
   return rtNoError;
}

static cmdResponseType setWait(char *s, connectionRecType *context)
{
  switch (checkReceivedDoneNone(s)) {
    case -1: return rtStandardError;
    case 0: 
      if (emcCommandWaitReceived() != 0) return rtStandardError;
      break;
    case 1: 
      if (emcCommandWaitDone() != 0) return rtStandardError;
      break;
    case 2: ;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType setTimeout(char *s, connectionRecType *context)
{
  float Timeout;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%f", &Timeout) < 1) return rtStandardError;
  emcTimeout = Timeout;
  return rtNoError;
}

static cmdResponseType setUpdate(char *s, connectionRecType *context)
{
  switch (checkNoneAuto(s)) {
    case 0: emcUpdateType = EMC_UPDATE_NONE; break;
    case 1: emcUpdateType = EMC_UPDATE_AUTO; break;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType setMode(char *s, connectionRecType *context)
{
  switch (checkManualAutoMDI(s)) {
    case 0: sendManual(); break;
    case 1: sendAuto(); break;
    case 2: sendMdi(); break;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType setMist(char *s, connectionRecType *context)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendMistOn(); break;
     case 1: sendMistOff();
     }
   return rtNoError;
}

static cmdResponseType setFlood(char *s, connectionRecType *context)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendFloodOn(); break;
     case 1: sendFloodOff();
     }
   return rtNoError;
}

static cmdResponseType setLube(char *s, connectionRecType *context)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendLubeOn(); break;
     case 1: sendLubeOff();
     }
   return rtNoError;
}

static cmdResponseType setSpindle(char *s, connectionRecType *context)
{
	int spindle = 0;
	s = strtok(NULL, delims);
	if (sscanf(s, "%d", &spindle) > 0){// there is a spindle number
		s = strtok(NULL, delims);
	} else {
		spindle = -1;
	}
	switch (checkSpindleStr(s)) {
     	 case -1: return rtStandardError;
     	 case 0: sendSpindleForward(spindle); break;
     	 case 1: sendSpindleReverse(spindle); break;
     	 case 2: sendSpindleIncrease(spindle); break;
     	 case 3: sendSpindleDecrease(spindle); break;
     	 case 4: sendSpindleConstant(spindle); break;
     	 case 5: sendSpindleOff(spindle);
	}
	return rtNoError;
}

static cmdResponseType setBrake(char *s, connectionRecType *context)
{
	int spindle = 0;
	s = strtok(NULL, delims);
	if (sscanf(s, "%d", &spindle) > 0){// there is a spindle number
		if (spindle < 0 || spindle > EMCMOT_MAX_SPINDLES) return rtStandardError;
		s = strtok(NULL, delims);
	} else {
		spindle = -1;
	}
	switch (checkOnOff(s)) {
     	 case -1: return rtStandardError;
     	 case 0: sendBrakeEngage(spindle); break;
     	 case 1: sendBrakeRelease(spindle);
     }
   return rtNoError;
}

static cmdResponseType setLoadToolTable(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  if (sendLoadToolTable(s) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setToolOffset(char *s, connectionRecType *context)
{
  int tool;
  float length, diameter;
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%d", &tool) <= 0) return rtStandardError;
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &length) <= 0) return rtStandardError;
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &diameter) <= 0) return rtStandardError;
  
  if (sendToolSetOffset(tool, length, diameter) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setOverrideLimits(char *s, connectionRecType *context)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: if (sendOverrideLimits(0) != 0) return rtStandardError; break;
     case 1: if (sendOverrideLimits(-1) != 0) return rtStandardError;
     }
   return rtNoError;
}

static cmdResponseType setMDI(char *s, connectionRecType *context)
{
  char *pch;
  
  pch = strtok(NULL, "\n\r\0");
  if (sendMdiCmd(pch) !=0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setHome(char *s, connectionRecType *context)
{
  int joint;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%d", &joint) <= 0) return rtStandardError;
  // joint == -1 means "Home All", any other negative is wrong
  if ((joint < -1) || (joint > EMCMOT_MAX_JOINTS)) return rtStandardError;
  sendHome(joint);
  return rtNoError;
}

static int axisnumber(char letter) {
   switch (letter) {
     case 'x': case 'X': return 0;
     case 'y': case 'Y': return 1;
     case 'z': case 'Z': return 2;
     case 'a': case 'A': return 3;
     case 'b': case 'B': return 4;
     case 'c': case 'C': return 5;
     case 'u': case 'U': return 6;
     case 'v': case 'V': return 7;
     case 'w': case 'W': return 8;
   }
   return -1;
}

static cmdResponseType setJogStop(char *pch, connectionRecType *context)
{
  int ja,jnum,jjogmode;
  char aletter;
  //parms:  jnum|aletter
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;

  if (sscanf(pch, "%d", &jnum) > 0) {
    ja = jnum;
    jjogmode = JOGJOINT;
  } else if (sscanf(pch, "%c", &aletter) > 0) {
    ja = axisnumber(aletter);
    jjogmode = JOGTELEOP;
  } else {
    return rtStandardError;
  }

  if (   (jjogmode == JOGJOINT)
      && ((ja < 0) || (ja > EMCMOT_MAX_JOINTS))
     ) return rtStandardError;

  if (sendJogStop(ja,jjogmode) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setJog(char *s, connectionRecType *context)
{
  int ja,jnum,jjogmode;
  char aletter;
  float speed;
  char *pch;
  //parms:  jnum|aletter speed

  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;

  if (sscanf(pch, "%d", &jnum) > 0) {
    ja = jnum;
    jjogmode = JOGJOINT;
  } else if (sscanf(pch, "%c", &aletter) > 0) {
    ja = axisnumber(aletter);
    jjogmode = JOGTELEOP;
  } else {
    return rtStandardError;
  }

  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &speed) <= 0) return rtStandardError; 

  if (sendJogCont(ja, jjogmode, speed) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setFeedOverride(char *s, connectionRecType *context)
{
  int percent;
  
  if (s == NULL) return rtStandardError;
  if (sscanf(s, "%d", &percent) <= 0) return rtStandardError;
  sendFeedOverride(((double) percent) / 100.0);
  return rtNoError;
}

static cmdResponseType setJogIncr(char *s, connectionRecType *context)
{
  int jnum,ja,jjogmode;
  char aletter;
  float speed, incr;
  char *pch;
  //parms:  jnum|aletter speed distance
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;

  if (sscanf(pch, "%d", &jnum) > 0) {
    ja = jnum;
    jjogmode = JOGJOINT;
  } else if (sscanf(pch, "%c", &aletter) > 0) {
    ja = axisnumber(aletter);
    jjogmode = JOGTELEOP;
  } else {
    return rtStandardError;
  }

  if (   (jjogmode == JOGJOINT)
      && ((ja < 0) || (ja > EMCMOT_MAX_JOINTS))
     ) return rtStandardError;

  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;

  if (sscanf(pch, "%f", &speed) <= 0) return rtStandardError; 
  pch = strtok(NULL, delims);

  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &incr) <= 0) return rtStandardError; 

  if (sendJogIncr(ja, jjogmode, speed, incr) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setTaskPlanInit(char *s, connectionRecType *context)
{
  if (sendTaskPlanInit() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setOpen(char *s, connectionRecType *context)
{
  char *pch;

  pch = strtok(NULL, "\n\r\0");
  if (pch == NULL) return rtStandardError;

  strncpy(context->progName, pch, sizeof(context->progName));
  if (context->progName[sizeof(context->progName) - 1] != '\0') {
    fprintf(stderr, "linuxcncrsh: 'set open' filename too long for context (got %lu bytes, max %lu)", (unsigned long)strlen(pch), (unsigned long)sizeof(context->progName));
    return rtStandardError;
  }

  if (sendProgramOpen(context->progName) != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setRun(char *s, connectionRecType *context)
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

static cmdResponseType setPause(char *s, connectionRecType *context)
{
  if (sendProgramPause() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setResume(char *s, connectionRecType *context)
{
  if (sendProgramResume() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setStep(char *s, connectionRecType *context)
{
  if (sendProgramStep() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setAbort(char *s, connectionRecType *context)
{
  if (sendAbort() != 0) return rtStandardError;
  return rtNoError;
}

static cmdResponseType setLinearUnitConversion(char *s, connectionRecType *context)
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

static cmdResponseType setAngularUnitConversion(char *s, connectionRecType *context)
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

static cmdResponseType setTeleopEnable(char *s, connectionRecType *context)
{
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: sendSetTeleopEnable(1); break;
     case 1: sendSetTeleopEnable(0);
     }
   return rtNoError;
}

static cmdResponseType setProbe(char *s, connectionRecType *context)
{
  float x, y, z;
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return rtStandardError;
fprintf(stderr,"0_probe pch=%s\n",pch);
  if (sscanf(pch, "%f", &x) <= 0) return rtStandardError;

  pch = strtok(NULL, delims);
fprintf(stderr,"1_probe pch=%s\n",pch);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &y) <= 0) return rtStandardError;

  pch = strtok(NULL, delims);
fprintf(stderr,"2_probe pch=%s\n",pch);
  if (pch == NULL) return rtStandardError;
  if (sscanf(pch, "%f", &z) <= 0) return rtStandardError;
  
  sendProbe(x, y, z);
  return rtNoError;
}

static cmdResponseType setProbeClear(char *s, connectionRecType *context)
{
  sendClearProbeTrippedFlag();
  return rtNoError;
}

static cmdResponseType setSpindleOverride(char *s, connectionRecType *context)
{
	int percent;
	int spindle = 0;
	s = strtok(NULL, delims);
	if (sscanf(s, "%d", &spindle) > 0){// there is at least one number
		s = strtok(NULL, delims);
		if (sscanf(s, "%d", &percent) < 0){ // no second number
			percent = spindle;
			spindle = -1;
		}
	} else {
		return rtStandardError;
	}
	sendSpindleOverride(spindle, ((double) percent) / 100.0);
	return rtNoError;
}

static cmdResponseType setOptionalStop(char *s, connectionRecType *context)
{
  int value;

  sscanf(s, "%d", &value);
  if (sendSetOptionalStop(value) != 0) return rtStandardError;
  return rtNoError;
}

int commandSet(connectionRecType *context)
{
  static const char *setNakStr = "SET NAK\n\r";
  static const char *setCmdNakStr = "SET %s NAK\n\r";
  static const char *ackStr = "SET %s ACK\n\r";
  setCommandType cmd;
  char *pch;
  cmdResponseType ret = rtNoError;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) {
    return write(context->cliSock, setNakStr, strlen(setNakStr));
    }
  strupr(pch);
  cmd = lookupSetCommand(pch);
  if ((cmd >= scIniFile) && (context->cliSock != enabledConn)) {
    sprintf(context->outBuf, setCmdNakStr, pch);
    return write(context->cliSock, context->outBuf, strlen(context->outBuf));
    }
  if ((cmd > scMachine) && (emcStatus->task.state != EMC_TASK_STATE_ON)) {
//  Extra check in the event of an undetected change in Machine state resulting in
//  sending a set command when the machine state is off. This condition is detected
//  and appropriate error messages are generated, however erratic behavior has been
//  seen when doing certain set commands when the Machine state is other than 'On'.
    sprintf(context->outBuf, setCmdNakStr, pch);
    return write(context->cliSock, context->outBuf, strlen(context->outBuf));
    }
  switch (cmd) {
    case scEcho: ret = setEcho(strtok(NULL, delims), context); break;
    case scVerbose: ret = setVerbose(strtok(NULL, delims), context); break;
    case scEnable: ret = setEnable(strtok(NULL, delims), context); break;
    case scConfig: ret = setConfig(strtok(NULL, delims), context); break;
    case scCommMode: ret = setCommMode(strtok(NULL, delims), context); break;
    case scCommProt: ret = setCommProt(strtok(NULL, delims), context); break;
    case scIniFile: break;
    case scPlat: break;
    case scIni: break;
    case scDebug: ret = setDebug(strtok(NULL, delims), context); break;
    case scSetWait: ret = setSetWait(strtok(NULL, delims), context); break;
    case scWait: ret = setWait(strtok(NULL, delims), context); break;
    case scSetTimeout: ret = setTimeout(strtok(NULL, delims), context); break;
    case scUpdate: ret = setUpdate(strtok(NULL, delims), context); break;
    case scError: ret = rtStandardError; break;
    case scOperatorDisplay: ret = rtStandardError; break;
    case scOperatorText: ret = rtStandardError; break;
    case scTime: ret = rtStandardError; break;
    case scEStop: ret = setEStop(strtok(NULL, delims), context); break;
    case scMachine: ret = setMachine(strtok(NULL, delims), context); break;
    case scMode: ret = setMode(strtok(NULL, delims), context); break;
    case scMist: ret = setMist(strtok(NULL, delims), context); break;
    case scFlood: ret = setFlood(strtok(NULL, delims), context); break;
    case scLube: ret = setLube(strtok(NULL, delims), context); break;
    case scLubeLevel: ret = rtStandardError; break;
    case scSpindle: ret = setSpindle(pch, context); break;
    case scBrake: ret = setBrake(pch, context); break;
    case scTool: ret = rtStandardError; break;
    case scToolOffset: ret = setToolOffset(pch, context); break;
    case scLoadToolTable: ret = setLoadToolTable(strtok(NULL, delims), context); break;
    case scHome: ret = setHome(strtok(NULL, delims), context); break;
    case scJogStop: ret = setJogStop(pch, context); break;
    case scJog: ret = setJog(pch, context); break;
    case scJogIncr: ret = setJogIncr(pch, context); break;
    case scFeedOverride: ret = setFeedOverride(strtok(NULL, delims), context); break;
    case scAbsCmdPos: ret = rtStandardError; break;
    case scAbsActPos: ret = rtStandardError; break;
    case scRelCmdPos: ret = rtStandardError; break;
    case scRelActPos: ret = rtStandardError; break;
    case scJointPos: ret = rtStandardError; break;
    case scPosOffset: ret = rtStandardError; break;
    case scJointLimit: ret = rtStandardError; break;
    case scJointFault: ret = rtStandardError; break;
    case scJointHomed: ret = rtStandardError; break;
    case scMDI: ret = setMDI(pch, context); break;
    case scTskPlanInit: ret = setTaskPlanInit(pch, context); break;
    case scOpen: ret = setOpen(pch, context); break;
    case scRun: ret = setRun(strtok(NULL, delims), context); break;
    case scPause: ret = setPause(pch, context); break;
    case scResume: ret = setResume(pch, context); break;
    case scAbort: ret = setAbort(pch, context); break;
    case scStep: ret = setStep(pch, context); break;    
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
    case scLinearUnitConversion: ret = setLinearUnitConversion(strtok(NULL, delims), context); break;
    case scAngularUnitConversion: ret = setAngularUnitConversion(strtok(NULL, delims), context); break;
    case scProbeClear: ret = setProbeClear(pch, context); break;
    case scProbeTripped: ret = rtStandardError; break;
    case scProbeValue: ret = rtStandardError; break;
    case scProbe: ret = setProbe(pch, context); break;
    case scTeleopEnable: ret = setTeleopEnable(strtok(NULL, delims), context); break;
    case scKinematicsType: ret = rtStandardError; break;
    case scOverrideLimits: ret = setOverrideLimits(strtok(NULL, delims), context); break;
    case scSpindleOverride: ret = setSpindleOverride(pch, context); break;
    case scOptionalStop: ret = setOptionalStop(strtok(NULL, delims), context); break;
    case scUnknown: ret = rtStandardError;
    }
  switch (ret) {
    case rtNoError:  
      if (context->verbose) {
        sprintf(context->outBuf, ackStr, pch);
        return write(context->cliSock, context->outBuf, strlen(context->outBuf));
        }
      break;
    case rtHandledNoError: // Custom ok response already handled, take no action
      break; 
    case rtStandardError:
      sprintf(context->outBuf, setCmdNakStr, pch);
      return write(context->cliSock, context->outBuf, strlen(context->outBuf));
      break;
    case rtCustomError: // Custom error response entered in buffer
      return write(context->cliSock, context->outBuf, strlen(context->outBuf));
      break;
    case rtCustomHandledError: ;// Custom error respose handled, take no action
    }
  return 0;
}

static cmdResponseType getEcho(char *s, connectionRecType *context)
{
  static const char *pEchoStr = "ECHO %s";
  
  if (context->echo) sprintf(context->outBuf, pEchoStr, "ON");
  else sprintf(context->outBuf, pEchoStr, "OFF");
  return rtNoError;
}

static cmdResponseType getVerbose(char *s, connectionRecType *context)
{
  const char *pVerboseStr = "VERBOSE %s";
  
  if (context->verbose) sprintf(context->outBuf, pVerboseStr, "ON");
  else sprintf(context->outBuf, pVerboseStr, "OFF");
  return rtNoError;
}

static cmdResponseType getEnable(char *s, connectionRecType *context)
{
  const char *pEnableStr = "ENABLE %s";
  
  if (context->cliSock == enabledConn) 
//  if (context->enabled == true)
    sprintf(context->outBuf, pEnableStr, "ON");
  else sprintf(context->outBuf, pEnableStr, "OFF");
  return rtNoError;
}

static cmdResponseType getConfig(char *s, connectionRecType *context)
{
  const char *pConfigStr = "CONFIG";

  strcpy(context->outBuf, pConfigStr);
  return rtNoError;
}

static cmdResponseType getCommMode(char *s, connectionRecType *context)
{
  const char *pCommModeStr = "COMM_MODE %s";
  
  switch (context->commMode) {
    case 0: sprintf(context->outBuf, pCommModeStr, "ASCII"); break;
    case 1: sprintf(context->outBuf, pCommModeStr, "BINARY"); break;
    }
  return rtNoError;
}

static cmdResponseType getCommProt(char *s, connectionRecType *context)
{
  const char *pCommProtStr = "COMM_PROT %s";
  
  sprintf(context->outBuf, pCommProtStr, context->version);
  return rtNoError;
}

static cmdResponseType getDebug(char *s, connectionRecType *context)
{
  const char *pUpdateStr = "DEBUG %d";
  
  sprintf(context->outBuf, pUpdateStr, emcStatus->debug);
  return rtNoError;
}

static cmdResponseType getSetWait(char *s, connectionRecType *context)
{
  const char *pSetWaitStr = "SET_WAIT %s";
  
  switch (emcWaitType) {
    case EMC_WAIT_RECEIVED: sprintf(context->outBuf, pSetWaitStr, "RECEIVED"); break;
    case EMC_WAIT_DONE: sprintf(context->outBuf, pSetWaitStr, "DONE"); break;
    default: return rtStandardError;
    }
  return rtNoError;
}

static cmdResponseType getPlat(char *s, connectionRecType *context)
{
  const char *pPlatStr = "PLAT %s";
  
  sprintf(context->outBuf, pPlatStr, "Linux");
  return rtNoError;  
}

static cmdResponseType getEStop(char *s, connectionRecType *context)
{
  const char *pEStopStr = "ESTOP %s";
  
  if (emcStatus->task.state == EMC_TASK_STATE_ESTOP)
    sprintf(context->outBuf, pEStopStr, "ON");
  else sprintf(context->outBuf, pEStopStr, "OFF");
  return rtNoError;
}

static cmdResponseType getTimeout(char *s, connectionRecType *context)
{
  const char *pTimeoutStr = "SET_TIMEOUT %f";
  
  sprintf(context->outBuf, pTimeoutStr, emcTimeout);
  return rtNoError;
}

static cmdResponseType getTime(char *s, connectionRecType *context)
{
  const char *pTimeStr = "TIME %f";
  
  sprintf(context->outBuf, pTimeStr, etime());
  return rtNoError;
}

static cmdResponseType getError(char *s, connectionRecType *context)
{
  const char *pErrorStr = "ERROR %s";
  
  if (updateError() != 0)
    sprintf(context->outBuf, pErrorStr, "emc_error: bad status from LinuxCNC");
  else
    if (error_string[0] == 0)
      sprintf(context->outBuf, pErrorStr, "OK");
    else {
      sprintf(context->outBuf, pErrorStr, error_string);
      error_string[0] = 0;
      }
  return rtNoError;
}

static cmdResponseType getOperatorDisplay(char *s, connectionRecType *context)
{
  const char *pOperatorDisplayStr = "OPERATOR_DISPLAY %s";
  
  if (updateError() != 0)
    sprintf(context->outBuf, pOperatorDisplayStr, "emc_operator_display: bad status from LinuxCNC");
  else
    if (operator_display_string[0] == 0)
      sprintf(context->outBuf, pOperatorDisplayStr, "OK");
    else {
      sprintf(context->outBuf, pOperatorDisplayStr, operator_display_string);
      operator_display_string[0] = 0;
      }
  return rtNoError; 
}

static cmdResponseType getOperatorText(char *s, connectionRecType *context)
{
  const char *pOperatorTextStr = "OPERATOR_TEXT %s";
  
  if (updateError() != 0)
    sprintf(context->outBuf, pOperatorTextStr, "emc_operator_text: bad status from LinuxCNC");
  else
    if (operator_text_string[0] == 0)
      sprintf(context->outBuf, pOperatorTextStr, "OK");
    else {
      sprintf(context->outBuf, pOperatorTextStr, operator_text_string);
      operator_text_string[0] = 0;
      }
  return rtNoError; 
}

static cmdResponseType getMachine(char *s, connectionRecType *context)
{
  const char *pMachineStr = "MACHINE %s";
  
  if (emcStatus->task.state == EMC_TASK_STATE_ON)
    sprintf(context->outBuf, pMachineStr, "ON");
  else sprintf(context->outBuf, pMachineStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getMode(char *s, connectionRecType *context)
{
  const char *pModeStr = "MODE %s";
  
  switch (emcStatus->task.mode) {
    case EMC_TASK_MODE_MANUAL: sprintf(context->outBuf, pModeStr, "MANUAL"); break;
    case EMC_TASK_MODE_AUTO: sprintf(context->outBuf, pModeStr, "AUTO"); break;
    case EMC_TASK_MODE_MDI: sprintf(context->outBuf, pModeStr, "MDI"); break;
    default: sprintf(context->outBuf, pModeStr, "?");
    }
  return rtNoError; 
}

static cmdResponseType getMist(char *s, connectionRecType *context)
{
  const char *pMistStr = "MIST %s";
  
  if (emcStatus->io.coolant.mist == 1)
    sprintf(context->outBuf, pMistStr, "ON");
  else sprintf(context->outBuf, pMistStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getFlood(char *s, connectionRecType *context)
{
  const char *pFloodStr = "FLOOD %s";
  
  if (emcStatus->io.coolant.flood == 1)
    sprintf(context->outBuf, pFloodStr, "ON");
  else sprintf(context->outBuf, pFloodStr, "OFF");
  return rtNoError; 
}

static cmdResponseType getLube(char *s, connectionRecType *context)
{
  const char *pLubeStr = "LUBE %s";
  
  if (emcStatus->io.lube.on == 0)
    sprintf(context->outBuf, pLubeStr, "OFF");
  else sprintf(context->outBuf, pLubeStr, "ON");
  return rtNoError; 
}

static cmdResponseType getLubeLevel(char *s, connectionRecType *context)
{
  const char *pLubeLevelStr = "LUBE_LEVEL %s";
  
  if (emcStatus->io.lube.level == 0)
    sprintf(context->outBuf, pLubeLevelStr, "LOW");
  else sprintf(context->outBuf, pLubeLevelStr, "OK");
  return rtNoError; 
}

static cmdResponseType getSpindle(char *s, connectionRecType *context)
{
  const char *pSpindleStr = "SPINDLE %d %s";
  int spindle = -1;
  int n;
  s = strtok(NULL, delims);
  if (sscanf(s, "%d", &spindle) < 0) spindle = -1; // no spindle number given return all
  for (n = 0; n < emcStatus->motion.traj.spindles; n++){
	  if (n == spindle || spindle == -1){
		  if (emcStatus->motion.spindle[n].increasing > 0)
			sprintf(context->outBuf, pSpindleStr, n, "INCREASE");
		  else
			if (emcStatus->motion.spindle[n].increasing < 0)
			  sprintf(context->outBuf, pSpindleStr, n, "DECREASE");
			else
			  if (emcStatus->motion.spindle[n].direction > 0)
				sprintf(context->outBuf, pSpindleStr, n, "FORWARD");
			  else
				if (emcStatus->motion.spindle[n].direction < 0)
				  sprintf(context->outBuf, pSpindleStr, n, "REVERSE");
			else sprintf(context->outBuf, pSpindleStr, n, "OFF");
	  }
  }
  return rtNoError; 
}

static cmdResponseType getBrake(char *s, connectionRecType *context)
{
  const char *pBrakeStr = "BRAKE %s";
  int spindle;
  int n;
  s = strtok(NULL, delims);
  if (sscanf(s, "%d", &spindle) < 0) spindle = -1; // no spindle number return all
  for (n = 0; n < emcStatus->motion.traj.spindles; n++){
	  if (n == spindle || spindle == -1){
		  if (emcStatus->motion.spindle[spindle].brake == 1)
			sprintf(context->outBuf, pBrakeStr, "ON");
		  else sprintf(context->outBuf, pBrakeStr, "OFF");
	  }
  }
  return rtNoError; 
}

static cmdResponseType getTool(char *s, connectionRecType *context)
{
  const char *pToolStr = "TOOL %d";
  
  sprintf(context->outBuf, pToolStr, emcStatus->io.tool.toolInSpindle);
  return rtNoError; 
}

static cmdResponseType getToolOffset(char *s, connectionRecType *context)
{
  const char *pToolOffsetStr = "TOOL_OFFSET %d";
  
  sprintf(context->outBuf, pToolOffsetStr, emcStatus->task.toolOffset.tran.z);
  return rtNoError; 
}

static cmdResponseType getAbsCmdPos(char *s, connectionRecType *context)
{
  const char *pAbsCmdPosStr = "ABS_CMD_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(context->outBuf, pAbsCmdPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.x);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.y);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.z);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.a);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.b);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.c);
    strcat(context->outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getAbsActPos(char *s, connectionRecType *context)
{
  const char *pAbsActPosStr = "ABS_ACT_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(context->outBuf, pAbsActPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.x);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.y);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.z);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.a);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.b);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.c);
    strcat(context->outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getRelCmdPos(char *s, connectionRecType *context)
{
  const char *pRelCmdPosStr = "REL_CMD_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(context->outBuf, pRelCmdPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.g92_offset.tran.x);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.g92_offset.tran.y);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.a - emcStatus->task.g5x_offset.a - emcStatus->task.g92_offset.a);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.b - emcStatus->task.g5x_offset.b - emcStatus->task.g92_offset.b);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.position.c - emcStatus->task.g5x_offset.c - emcStatus->task.g92_offset.c);
    strcat(context->outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getRelActPos(char *s, connectionRecType *context)
{
  const char *pRelActPosStr = "REL_ACT_POS";
  char buf[16];
  int axis;
  
  if (s == NULL) axis = -1; // Return all axes
  else axis = atoi(s);
  strcpy(context->outBuf, pRelActPosStr);
  if (axis != -1) {
    sprintf(buf, " %d", axis);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 0)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.g92_offset.tran.x);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 1)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.g92_offset.tran.y);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 2)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 3)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.a - emcStatus->task.g5x_offset.a - emcStatus->task.g92_offset.a);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 4)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.b - emcStatus->task.g5x_offset.b - emcStatus->task.g92_offset.b);
    strcat(context->outBuf, buf);
    }
  if ((axis == -1) || (axis == 5)) {
    sprintf(buf, " %f", emcStatus->motion.traj.actualPosition.c - emcStatus->task.g5x_offset.c - emcStatus->task.g92_offset.c);
    strcat(context->outBuf, buf);
    }
  return rtNoError;
}

static cmdResponseType getJointPos(char *s, connectionRecType *context)
{
  const char *pJointPos = "JOINT_POS";
  int joint, i;
  char buf[16];
  
  if (s == NULL) joint = -1; // Return all axes
  else joint = atoi(s);
  if (joint == -1) {
    strcpy(context->outBuf, pJointPos);
    for (i=0; i<6; i++) {
      sprintf(buf, " %f", emcStatus->motion.joint[i].input);
      strcat(context->outBuf, buf);
      }
    }
  else
    sprintf(context->outBuf, "%s %d %f", pJointPos, joint, emcStatus->motion.joint[joint].input);
  
  return rtNoError;
}

static cmdResponseType getPosOffset(char *s, connectionRecType *context)
{
  const char *pPosOffset = "POS_OFFSET";
  char buf[16];
  
  if (s == NULL) {
    strcpy(context->outBuf, pPosOffset);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x));
    strcat(context->outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y));
    strcat(context->outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z));
    strcat(context->outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.a + emcStatus->task.g92_offset.a));
    strcat(context->outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.b + emcStatus->task.g92_offset.b));
    strcat(context->outBuf, buf);
    sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.c + emcStatus->task.g92_offset.c));
    strcat(context->outBuf, buf);
    }
  else
    {
      switch (s[0]) {
        case 'X': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x)); break;
        case 'Y': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y)); break;
        case 'Z': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z)); break;
        case 'A': 
        case 'R': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.a + emcStatus->task.g92_offset.a)); break;
        case 'B': 
        case 'P': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.b + emcStatus->task.g92_offset.b)); break;
        case 'C': 
        case 'W': sprintf(buf, " %f", convertLinearUnits(emcStatus->task.g5x_offset.c + emcStatus->task.g92_offset.c));
      }
      sprintf(context->outBuf, "%s %c %s", pPosOffset, s[0], buf);
    }
  return rtNoError;
}

static cmdResponseType getJointLimit(char *s, connectionRecType *context)
{
  const char *pJointLimit = "JOINT_LIMIT";
  char buf[16];
  int joint, i;
  
  if (s == NULL) {
    strcpy(context->outBuf, pJointLimit);
    for (i=0; i<6; i++) {
      if (emcStatus->motion.joint[i].minHardLimit)
        strcpy(buf, " MINHARD");
      else
        if (emcStatus->motion.joint[i].minSoftLimit)
	  strcpy(buf, " MINSOFT");
	else
	  if (emcStatus->motion.joint[i].maxSoftLimit)
	    strcpy(buf, " MAXSOFT");
	  else
	    if (emcStatus->motion.joint[i].maxHardLimit)
	      strcpy(buf, " MAXHARD");
	    else strcpy(buf, " OK");
      strcat(context->outBuf, buf);
      }
    }
  else
    {
      joint = atoi(s);
      if (emcStatus->motion.joint[joint].minHardLimit)
        strcpy(buf, "MINHARD");
      else
        if (emcStatus->motion.joint[joint].minSoftLimit)
	  strcpy(buf, "MINSOFT");
	else
	  if (emcStatus->motion.joint[joint].maxSoftLimit)
	    strcpy(buf, "MAXSOFT");
	  else
	    if (emcStatus->motion.joint[joint].maxHardLimit)
	      strcpy(buf, "MAXHARD");
	    else strcpy(buf, "OK");
      sprintf(context->outBuf, "%s %d %s", pJointLimit, joint, buf);
    }
  return rtNoError;
}

static cmdResponseType getJointFault(char *s, connectionRecType *context)
{
  const char *pJointFault = "JOINT_FAULT";
  char buf[16];
  int joint, i;
  
  if (s == NULL) {
    strcpy(context->outBuf, pJointFault);
    for (i=0; i<6; i++) {
      if (emcStatus->motion.joint[i].fault)
        strcat(context->outBuf, " FAULT");
      else strcat(context->outBuf, " OK");
      }
    }
  else {
      joint = atoi(s);
      if (emcStatus->motion.joint[joint].fault)
        strcpy(buf, "FAULT");
      else strcpy(buf, "OK");
      sprintf(context->outBuf, "%s %d %s", pJointFault, joint, buf);
    }
  return rtNoError;
}

static cmdResponseType getOverrideLimits(char *s, connectionRecType *context)
{
  const char *pOverrideLimits = "OVERRIDE_LIMITS %d";
  
  sprintf(context->outBuf, pOverrideLimits, emcStatus->motion.joint[0].overrideLimits);
  return rtNoError;
}

static cmdResponseType getJointHomed(char *s, connectionRecType *context)
{
  const char *pJointHomed = "JOINT_HOMED";
  char buf[16];
  int joint, i;
  
  if (s == NULL) {
    strcpy(context->outBuf, pJointHomed);
    for (i=0; i<6; i++) {
      if (emcStatus->motion.joint[i].homed)
        strcat(context->outBuf, " YES");
      else strcat(context->outBuf, " NO");
      }
    }
  else {
      joint = atoi(s);
      if (emcStatus->motion.joint[joint].homed)
        strcpy(buf, "YES");
      else strcpy(buf, "NO");
      sprintf(context->outBuf, "%s %d %s", pJointHomed, joint, buf);
    }
  return rtNoError;
}

static cmdResponseType getProgram(char *s, connectionRecType *context)
{
  const char *pProgram = "PROGRAM %s";
  
//  sprintf(outBuf, pProgram, progName);
//  printf("Program name = %s", emcStatus->task.file[0]);
  if (emcStatus->task.file[0] != 0)
    sprintf(context->outBuf, pProgram, emcStatus->task.file);
  return rtNoError;
}

static cmdResponseType getProgramLine(char *s, connectionRecType *context)
{
  const char *pProgramLine = "PROGRAM_LINE %d";
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
  sprintf(context->outBuf, pProgramLine, lineNo);
  return rtNoError;
}

static cmdResponseType getProgramStatus(char *s, connectionRecType *context)
{
  const char *pProgramStatus = "PROGRAM_STATUS %s";
  
  switch (emcStatus->task.interpState) {
      case EMC_TASK_INTERP_READING:
      case EMC_TASK_INTERP_WAITING: sprintf(context->outBuf, pProgramStatus, "RUNNING"); break;
      case EMC_TASK_INTERP_PAUSED: sprintf(context->outBuf, pProgramStatus, "PAUSED"); break;
      default: sprintf(context->outBuf, pProgramStatus, "IDLE"); break;
    }
  return rtNoError;
}

static cmdResponseType getProgramCodes(char *s, connectionRecType *context)
{
  const char *pProgramCodes = "PROGRAM_CODES ";
  char buf[256];
  int code, i;
  
  buf[0] = 0;
  strcpy(context->outBuf, pProgramCodes);
  for (i=1; i<ACTIVE_G_CODES; i++) {
      code = emcStatus->task.activeGCodes[i];
      if (code == -1) continue;
      if (code % 10) sprintf(buf, "G%.1f ", (double) code / 10.0);
      else sprintf(buf, "G%d ", code / 10);
      strcat(context->outBuf, buf);
    }
  sprintf(buf, "F%.0f ", emcStatus->task.activeSettings[1]);
  strcat(context->outBuf, buf);
  sprintf(buf, "S%.0f", fabs(emcStatus->task.activeSettings[2]));
  strcat(context->outBuf, buf);
  return rtNoError;
}

static cmdResponseType getJointType(char *s, connectionRecType *context)
{
  const char *pJointType = "JOINT_TYPE";
  char buf[16];
  int joint, i;
  
  if (s == NULL) {
    strcpy(context->outBuf, pJointType);
    for (i=0; i<6; i++) {
      switch (emcStatus->motion.joint[i].jointType) {
        case EMC_LINEAR: strcat(context->outBuf, " LINEAR"); break;
	case EMC_ANGULAR: strcat(context->outBuf, " ANGULAR"); break;
	default: strcat(context->outBuf, "CUSTOM");
	}
      }
    }
  else {
      joint = atoi(s);
      switch (emcStatus->motion.joint[joint].jointType) {
        case EMC_LINEAR: strcpy(buf, " LINEAR"); break;
	case EMC_ANGULAR: strcpy(buf, " ANGULAR"); break;
	default: strcpy(buf, "CUSTOM");
	}
      sprintf(context->outBuf, "%s %d %s", pJointType, joint, buf);
    }
  return rtNoError;
}

static cmdResponseType getJointUnits(char *s, connectionRecType *context)
{
  const char *pJointUnits = "JOINT_UNITS";
  char buf[16];
  int joint, i;
  
  if (s == NULL) {
    strcpy(context->outBuf, pJointUnits);
    for (i=0; i<6; i++) {
      switch (emcStatus->motion.joint[i].jointType) {
        case EMC_LINEAR: 
	  if (CLOSE(emcStatus->motion.joint[i].units, 1.0, LINEAR_CLOSENESS))
	    strcat(context->outBuf, " MM");
	  else 
	    if (CLOSE(emcStatus->motion.joint[i].units, INCH_PER_MM,
	      LINEAR_CLOSENESS)) strcat(context->outBuf, " INCH");
	    else
	      if (CLOSE(emcStatus->motion.joint[i].units, CM_PER_MM,
	        LINEAR_CLOSENESS)) strcat(context->outBuf, " CM");
	      else strcat(context->outBuf, " CUSTOM");
	  break;
	case EMC_ANGULAR:
	  if (CLOSE(emcStatus->motion.joint[i].units, 1.0, ANGULAR_CLOSENESS))
	    strcat(context->outBuf, " DEG");
	  else
  	    if (CLOSE(emcStatus->motion.joint[i].units, RAD_PER_DEG, ANGULAR_CLOSENESS))
	      strcat(context->outBuf, " RAD");
	    else
	      if (CLOSE(emcStatus->motion.joint[i].units, GRAD_PER_DEG, ANGULAR_CLOSENESS))
	        strcat(context->outBuf, " GRAD");
	      else strcat(context->outBuf, " CUSTOM");
	  break;
	default: strcat(context->outBuf, " CUSTOM");
	}
      }
    }
  else {
      joint = atoi(s);
      switch (emcStatus->motion.joint[joint].jointType) {
        case EMC_LINEAR: 
	  if (CLOSE(emcStatus->motion.joint[joint].units, 1.0, LINEAR_CLOSENESS))
	    strcpy(buf, "MM");
	  else 
	    if (CLOSE(emcStatus->motion.joint[joint].units, INCH_PER_MM,
	      LINEAR_CLOSENESS)) strcpy(buf, "INCH");
	    else
	      if (CLOSE(emcStatus->motion.joint[joint].units, CM_PER_MM,
	        LINEAR_CLOSENESS)) strcpy(buf, "CM");
	      else strcpy(buf, "CUSTOM");
	  break;
	case EMC_ANGULAR:
	  if (CLOSE(emcStatus->motion.joint[joint].units, 1.0, ANGULAR_CLOSENESS))
	    strcpy(buf, "DEG");
	  else
  	    if (CLOSE(emcStatus->motion.joint[joint].units, RAD_PER_DEG, ANGULAR_CLOSENESS))
	      strcpy(buf, "RAD");
	    else
	      if (CLOSE(emcStatus->motion.joint[joint].units, GRAD_PER_DEG, ANGULAR_CLOSENESS))
	        strcpy(buf, "GRAD");
	      else strcpy(buf, "CUSTOM");
	  break;
	default: strcpy(buf, "CUSTOM");
      sprintf(context->outBuf, "%s %d %s", pJointUnits, joint, buf);
      }
    }
  return rtNoError;
}

static cmdResponseType getProgramLinearUnits(char *s, connectionRecType *context)
{
  const char *programUnits = "PROGRAM_UNITS %s";
  
  switch (emcStatus->task.programUnits) {
    case CANON_UNITS_INCHES: sprintf(context->outBuf, programUnits, "INCH"); break;
    case CANON_UNITS_MM: sprintf(context->outBuf, programUnits, "MM"); break;
    case CANON_UNITS_CM: sprintf(context->outBuf, programUnits, "CM"); break;
    default: sprintf(context->outBuf, programUnits, "CUSTOM"); break;
    }
  return rtNoError;
}

static cmdResponseType getProgramAngularUnits(char *s, connectionRecType *context)
{
  const char *programAngularUnits = "PROGRAM_ANGULAR_UNITS %s";
  
  sprintf(context->outBuf, programAngularUnits, "DEG");
  return rtNoError;
}

static cmdResponseType getUserLinearUnits(char *s, connectionRecType *context)
{
  const char *userLinearUnits = "USER_LINEAR_UNITS %s";
  
  if (CLOSE(emcStatus->motion.traj.linearUnits, 1.0, LINEAR_CLOSENESS))
    sprintf(context->outBuf, userLinearUnits, "MM");
  else
    if (CLOSE(emcStatus->motion.traj.linearUnits, INCH_PER_MM, LINEAR_CLOSENESS))
      sprintf(context->outBuf, userLinearUnits, "INCH");
    else
      if (CLOSE(emcStatus->motion.traj.linearUnits, CM_PER_MM, LINEAR_CLOSENESS))
        sprintf(context->outBuf, userLinearUnits, "CM");
      else
        sprintf(context->outBuf, userLinearUnits, "CUSTOM");
  return rtNoError;
}

static cmdResponseType getUserAngularUnits(char *s, connectionRecType *context)
{
  const char *pUserAngularUnits = "USER_ANGULAR_UNITS %s";
  
  if (CLOSE(emcStatus->motion.traj.angularUnits, 1.0, ANGULAR_CLOSENESS))
    sprintf(context->outBuf, pUserAngularUnits, "DEG");
  else
    if (CLOSE(emcStatus->motion.traj.angularUnits, RAD_PER_DEG, ANGULAR_CLOSENESS))
      sprintf(context->outBuf, pUserAngularUnits, "RAD");
    else
      if (CLOSE(emcStatus->motion.traj.angularUnits, GRAD_PER_DEG, ANGULAR_CLOSENESS))
        sprintf(context->outBuf, pUserAngularUnits, "GRAD");
      else
        sprintf(context->outBuf, pUserAngularUnits, "CUSTOM");
  return rtNoError;
}

static cmdResponseType getDisplayLinearUnits(char *s, connectionRecType *context)
{
  const char *pDisplayLinearUnits = "DISPLAY_LINEAR_UNITS %s";
  
  switch (linearUnitConversion) {
      case LINEAR_UNITS_INCH: sprintf(context->outBuf, pDisplayLinearUnits, "INCH"); break;
      case LINEAR_UNITS_MM: sprintf(context->outBuf, pDisplayLinearUnits, "MM"); break;
      case LINEAR_UNITS_CM: sprintf(context->outBuf, pDisplayLinearUnits, "CM"); break;
      case LINEAR_UNITS_AUTO: 
        switch (emcStatus->task.programUnits) {
	    case CANON_UNITS_MM: sprintf(context->outBuf, pDisplayLinearUnits, "MM"); break;
	    case CANON_UNITS_INCHES: sprintf(context->outBuf, pDisplayLinearUnits, "INCH"); break;
	    case CANON_UNITS_CM: sprintf(context->outBuf, pDisplayLinearUnits, ""); break;
	    default: sprintf(context->outBuf, pDisplayLinearUnits, "CUSTOM");
	  }
        break;
      default: sprintf(context->outBuf, pDisplayLinearUnits, "CUSTOM");
    }
  return rtNoError;
}

static cmdResponseType getDisplayAngularUnits(char *s, connectionRecType *context)
{
  const char *pDisplayAngularUnits = "DISPLAY_ANGULAR_UNITS %s";
  
  switch (angularUnitConversion) {
      case ANGULAR_UNITS_DEG: sprintf(context->outBuf, pDisplayAngularUnits, "DEG"); break;
      case ANGULAR_UNITS_RAD: sprintf(context->outBuf, pDisplayAngularUnits, "RAD"); break;
      case ANGULAR_UNITS_GRAD: sprintf(context->outBuf, pDisplayAngularUnits, "GRAD"); break;
      case ANGULAR_UNITS_AUTO: sprintf(context->outBuf, pDisplayAngularUnits, "DEG"); break; 
      default: sprintf(context->outBuf, pDisplayAngularUnits, "CUSTOM");
    }
  return rtNoError;
}

static cmdResponseType getLinearUnitConversion(char *s, connectionRecType *context)
{
  const char *pLinearUnitConversion = "LINEAR_UNIT_CONVERSION %s";
  
  switch (linearUnitConversion) {
      case LINEAR_UNITS_INCH: sprintf(context->outBuf, pLinearUnitConversion, "INCH"); break;
      case LINEAR_UNITS_MM: sprintf(context->outBuf, pLinearUnitConversion, "MM"); break;
      case LINEAR_UNITS_CM: sprintf(context->outBuf, pLinearUnitConversion, "CM"); break;
      case LINEAR_UNITS_AUTO: sprintf(context->outBuf, pLinearUnitConversion, "AUTO"); break;
      default: sprintf(context->outBuf, pLinearUnitConversion, "CUSTOM"); 
    }
  return rtNoError;
}

static cmdResponseType getAngularUnitConversion(char *s, connectionRecType *context)
{
  const char *pAngularUnitConversion = "ANGULAR_UNIT_CONVERSION %s";
  
  switch (angularUnitConversion) {
      case ANGULAR_UNITS_DEG: sprintf(context->outBuf, pAngularUnitConversion, "DEG"); break;
      case ANGULAR_UNITS_RAD: sprintf(context->outBuf, pAngularUnitConversion, "RAD"); break;
      case ANGULAR_UNITS_GRAD: sprintf(context->outBuf, pAngularUnitConversion, "GRAD"); break;
      case ANGULAR_UNITS_AUTO: sprintf(context->outBuf, pAngularUnitConversion, "AUTO"); break;
      default: sprintf(context->outBuf, pAngularUnitConversion, "CUSTOM");
    }
  return rtNoError;
}

static cmdResponseType getProbeValue(char *s, connectionRecType *context)
{
  const char *pProbeValue = "PROBE_VALUE %d";
  
  sprintf(context->outBuf, pProbeValue, emcStatus->motion.traj.probeval);  
  return rtNoError;
}

static cmdResponseType getProbeTripped(char *s, connectionRecType *context)
{
  const char *pProbeTripped = "PROBE_TRIPPED %d";
  
  sprintf(context->outBuf, pProbeTripped, emcStatus->motion.traj.probe_tripped);  
  return rtNoError;
}

static cmdResponseType getTeleopEnable(char *s, connectionRecType *context)
{
  const char *pTeleopEnable = "TELEOP_ENABLE %s";
  
  if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE_TELEOP)
    sprintf(context->outBuf, pTeleopEnable, "YES");  
   else sprintf(context->outBuf, pTeleopEnable, "NO");
  return rtNoError;
}

static cmdResponseType getKinematicsType(char *s, connectionRecType *context)
{
  const char *pKinematicsType = "KINEMATICS_TYPE %d";
  
  sprintf(context->outBuf, pKinematicsType, emcStatus->motion.traj.kinematics_type);  
  return rtNoError;
}

static cmdResponseType getFeedOverride(char *s, connectionRecType *context)
{
  const char *pFeedOverride = "FEED_OVERRIDE %d";
  int percent;
  
  percent = (int)floor(emcStatus->motion.traj.scale * 100.0 + 0.5);
  sprintf(context->outBuf, pFeedOverride, percent);
  return rtNoError;
}

static cmdResponseType getIniFile(char *s, connectionRecType *context)
{
  const char *pIniFile = "INIFILE %s";
  
  sprintf(context->outBuf, pIniFile, emc_inifile);
  return rtNoError;
}

static cmdResponseType getSpindleOverride(char *s, connectionRecType *context)
{
  const char *pSpindleOverride = "SPINDLE_OVERRIDE %d %d";
  int percent;
  int spindle;
  int n;
  s = strtok(NULL, delims);
  if (sscanf(s, "%d", &spindle) < 0) spindle = -1; // no spindle number return all
  for (n = 0; n < emcStatus->motion.traj.spindles; n++){
	  if (n == spindle || spindle == -1){
		  percent = (int)floor(emcStatus->motion.spindle[n].spindle_scale * 100.0 + 0.5);
		  sprintf(context->outBuf, pSpindleOverride, n, percent);
	  }
  }
  return rtNoError;
}

static cmdResponseType getOptionalStop(char *s, connectionRecType *context)
{
  const char *pOptionalStop = "OPTIONAL_STOP %d";

  sprintf(context->outBuf, pOptionalStop, emcStatus->task.optional_stop_state);
  return rtNoError;
}

int commandGet(connectionRecType *context)
{
  const static char *setNakStr = "GET NAK\r\n";
  const static char *setCmdNakStr = "GET %s NAK\r\n";
  setCommandType cmd;
  char *pch;
  cmdResponseType ret = rtNoError;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) {
    return write(context->cliSock, setNakStr, strlen(setNakStr));
    }
  if (emcUpdateType == EMC_UPDATE_AUTO) updateStatus();
  strupr(pch);
  cmd = lookupSetCommand(pch);
  if (cmd > scIni)
    if (emcUpdateType == EMC_UPDATE_AUTO) updateStatus();
  switch (cmd) {
    case scEcho: ret = getEcho(pch, context); break;
    case scVerbose: ret = getVerbose(pch, context); break;
    case scEnable: ret = getEnable(pch, context); break;
    case scConfig: ret = getConfig(pch, context); break;
    case scCommMode: ret = getCommMode(pch, context); break;
    case scCommProt: ret = getCommProt(pch, context); break;
    case scIniFile: getIniFile(pch, context); break;
    case scPlat: ret = getPlat(pch, context); break;
    case scIni: break;
    case scDebug: ret = getDebug(pch, context); break;
    case scSetWait: ret = getSetWait(pch, context); break;
    case scWait: break;
    case scSetTimeout: ret = getTimeout(pch, context); break;
    case scUpdate: break;
    case scError: ret = getError(pch, context); break;
    case scOperatorDisplay: ret = getOperatorDisplay(pch, context); break;
    case scOperatorText: ret = getOperatorText(pch, context); break;
    case scTime: ret = getTime(pch, context); break;
    case scEStop: ret = getEStop(pch, context); break;
    case scMachine: ret = getMachine(pch, context); break;
    case scMode: ret = getMode(pch, context); break;
    case scMist: ret = getMist(pch, context); break;
    case scFlood: ret = getFlood(pch, context); break;
    case scLube: ret = getLube(pch, context); break;
    case scLubeLevel: ret = getLubeLevel(pch, context); break;
    case scSpindle: ret = getSpindle(pch, context); break;
    case scBrake: ret = getBrake(pch, context); break;
    case scTool: ret = getTool(pch, context); break;
    case scToolOffset: ret = getToolOffset(pch, context); break;
    case scLoadToolTable: ret = rtStandardError; break;
    case scHome: ret = rtStandardError; break;
    case scJogStop: ret = rtStandardError; break;
    case scJog: ret = rtStandardError; break;
    case scJogIncr: ret = rtStandardError; break;
    case scFeedOverride: ret = getFeedOverride(pch, context); break;
    case scAbsCmdPos: ret = getAbsCmdPos(strtok(NULL, delims), context); break;
    case scAbsActPos: ret = getAbsActPos(strtok(NULL, delims), context); break;
    case scRelCmdPos: ret = getRelCmdPos(strtok(NULL, delims), context); break;
    case scRelActPos: ret = getRelActPos(strtok(NULL, delims), context); break;
    case scJointPos: ret = getJointPos(strtok(NULL, delims), context); break;
    case scPosOffset: ret = getPosOffset(strtok(NULL, delims), context); break;
    case scJointLimit: ret = getJointLimit(strtok(NULL, delims), context); break;
    case scJointFault: ret = getJointFault(strtok(NULL, delims), context); break;
    case scJointHomed: ret = getJointHomed(strtok(NULL, delims), context); break;
    case scMDI: ret = rtStandardError; break;
    case scTskPlanInit: ret = rtStandardError; break;
    case scOpen: ret = rtStandardError; break;
    case scRun: ret = rtStandardError; break;
    case scPause: ret = rtStandardError; break;
    case scResume: ret = rtStandardError; break;
    case scStep: ret = rtStandardError; break;
    case scAbort: ret = rtStandardError; break;
    case scProgram: ret = getProgram(pch, context); break;
    case scProgramLine: ret = getProgramLine(pch, context); break;
    case scProgramStatus: ret = getProgramStatus(pch, context); break;
    case scProgramCodes: ret = getProgramCodes(pch, context); break;
    case scJointType: ret = getJointType(strtok(NULL, delims), context); break;
    case scJointUnits: ret = getJointUnits(strtok(NULL, delims), context); break;
    case scProgramUnits: 
    case scProgramLinearUnits: ret = getProgramLinearUnits(pch, context); break;
    case scProgramAngularUnits: ret = getProgramAngularUnits(pch, context); break;
    case scUserLinearUnits: ret = getUserLinearUnits(pch, context); break;
    case scUserAngularUnits: ret = getUserAngularUnits(pch, context); break;
    case scDisplayLinearUnits: ret = getDisplayLinearUnits(pch, context); break;
    case scDisplayAngularUnits: ret = getDisplayAngularUnits(pch, context); break;
    case scLinearUnitConversion: ret = getLinearUnitConversion(pch, context); break;
    case scAngularUnitConversion: ret = getAngularUnitConversion(pch, context); break;
    case scProbeClear: break;
    case scProbeTripped: ret = getProbeTripped(pch, context); break;
    case scProbeValue: ret = getProbeValue(pch, context); break;
    case scProbe: break;
    case scTeleopEnable: ret = getTeleopEnable(pch, context); break;
    case scKinematicsType: ret = getKinematicsType(pch, context); break;
    case scOverrideLimits: ret = getOverrideLimits(pch, context); break;
    case scSpindleOverride: ret = getSpindleOverride(pch, context); break;
    case scOptionalStop: ret = getOptionalStop(pch, context); break;
    case scUnknown: ret = rtStandardError;
    }
  switch (ret) {
    case rtNoError: // Standard ok response, just write value in buffer
      sockWrite(context);
      break;
    case rtHandledNoError: // Custom ok response already handled, take no action
      break; 
    case rtStandardError: // Standard error response
      sprintf(context->outBuf, setCmdNakStr, pch); 
      sockWrite(context);
      break;
    case rtCustomError: // Custom error response entered in buffer
      sockWrite(context);
      break;
    case rtCustomHandledError: ;// Custom error respose handled, take no action
    }
  return 0;
}

int commandQuit(connectionRecType *context)
{
  printf("Closing connection with %s\n", context->hostName);
  return -1;
}

int commandShutdown(connectionRecType *context)
{
  if (context->cliSock == enabledConn) {
    printf("Shutting down\n");
    thisQuit();
    return -1;
    }
  else
    return 0;
}

static int helpGeneral(connectionRecType *context)
{
  sprintf(context->outBuf, "Available commands:\n\r");
  strcat(context->outBuf, "  Hello <password> <client name> <protocol version>\n\r");
  strcat(context->outBuf, "  Get <LinuxCNC command>\n\r");
  strcat(context->outBuf, "  Set <LinuxCNC command>\n\r");
  strcat(context->outBuf, "  Shutdown\n\r");
  strcat(context->outBuf, "  Help <command>\n\r");
  sockWrite(context);
  return 0;
}

static int helpHello(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r");
  strcat(context->outBuf, "  Hello <Password> <Client Name> <Protocol Version>\n\rWhere:\n\r");
  strcat(context->outBuf, "  Password is the connection password to allow communications with the CNC server.\n\r");
  strcat(context->outBuf, "  Client Name is the name of client trying to connect, typically the network name of the client.\n\r");
  strcat(context->outBuf, "  Protocol Version is the version of the protocol with which the client wishes to use.\n\r\n\r");
  strcat(context->outBuf, "  With valid password, server responds with:\n\r");
  strcat(context->outBuf, "  Hello Ack <Server Name> <Protocol Version>\n\rWhere:\n\r");
  strcat(context->outBuf, "  Ack is acknowledging the connection has been made.\n\r");
  strcat(context->outBuf, "  Server Name is the name of the LinuxCNC Server to which the client has connected.\n\r");
  strcat(context->outBuf, "  Protocol Version is the client requested version or latest version support by server if");
  strcat(context->outBuf, "  the client requests a version later than that supported by the server.\n\r\n\r");
  strcat(context->outBuf, "  With invalid password, the server responds with:\n\r");
  strcat(context->outBuf, "  Hello Nak\n\r");
  sockWrite(context);
  return 0;
}

static int helpGet(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\rGet <LinuxCNC command>\n\r");
  strcat(context->outBuf, "  Get commands require that a hello has been successfully negotiated.\n\r");
  strcat(context->outBuf, "  LinuxCNC command may be one of:\n\r");
  strcat(context->outBuf, "    Abs_act_pos\n\r");
  strcat(context->outBuf, "    Abs_cmd_pos\n\r");
  strcat(context->outBuf, "    Angular_unit_conversion\n\r");
  strcat(context->outBuf, "    Brake\n\r");
  strcat(context->outBuf, "    Comm_mode\n\r");
  strcat(context->outBuf, "    Comm_prot\n\r");
  strcat(context->outBuf, "    Debug\n\r");
  strcat(context->outBuf, "    Display_angular_units\n\r"); 
  strcat(context->outBuf, "    Display_linear_units\n\r");
  strcat(context->outBuf, "    Echo\n\r");
  strcat(context->outBuf, "    Enable\n\r");
  strcat(context->outBuf, "    Error\n\r");
  strcat(context->outBuf, "    EStop\n\r");
  strcat(context->outBuf, "    Feed_override\n\r");
  strcat(context->outBuf, "    Flood\n\r");
  strcat(context->outBuf, "    Inifile\n\r");
  strcat(context->outBuf, "    Joint_fault\n\r");
  strcat(context->outBuf, "    Joint_homed\n\r");
  strcat(context->outBuf, "    Joint_limit\n\r");
  strcat(context->outBuf, "    Joint_pos\n\r");
  strcat(context->outBuf, "    Joint_type\n\r");
  strcat(context->outBuf, "    Joint_units\n\r");
  strcat(context->outBuf, "    Kinematics_type\n\r");
  strcat(context->outBuf, "    Linear_unit_conversion\n\r");
  strcat(context->outBuf, "    Lube\n\r");
  strcat(context->outBuf, "    Lube_level\n\r");
  strcat(context->outBuf, "    Machine\n\r");
  strcat(context->outBuf, "    Mist\n\r");
  strcat(context->outBuf, "    Mode\n\r");
  strcat(context->outBuf, "    Operator_display\n\r");
  strcat(context->outBuf, "    Operator_text\n\r");
  strcat(context->outBuf, "    Optional_stop\n\r");
  strcat(context->outBuf, "    Override_limits\n\r");
  strcat(context->outBuf, "    Plat\n\r");
  strcat(context->outBuf, "    Pos_offset\n\r");
  strcat(context->outBuf, "    Probe_tripped\n\r");
  strcat(context->outBuf, "    Probe_value\n\r");
  strcat(context->outBuf, "    Program\n\r");
  strcat(context->outBuf, "    Program_angular_units\n\r"); 
  strcat(context->outBuf, "    Program_codes\n\r");
  strcat(context->outBuf, "    Program_line\n\r");
  strcat(context->outBuf, "    Program_linear_units\n\r");
  strcat(context->outBuf, "    Program_status\n\r");
  strcat(context->outBuf, "    Program_units\n\r");
  strcat(context->outBuf, "    Rel_act_pos\n\r");
  strcat(context->outBuf, "    Rel_cmd_pos\n\r");
  strcat(context->outBuf, "    Set_wait\n\r");
  strcat(context->outBuf, "    Spindle\n\r");
  strcat(context->outBuf, "    Spindle_override\n\r");
  strcat(context->outBuf, "    Teleop_enable\n\r");
  strcat(context->outBuf, "    Time\n\r");
  strcat(context->outBuf, "    Timeout\n\r");
  strcat(context->outBuf, "    Tool\n\r");
  strcat(context->outBuf, "    Tool_offset\n\r");
  strcat(context->outBuf, "    User_angular_units\n\r");
  strcat(context->outBuf, "    User_linear_units\n\r");
  strcat(context->outBuf, "    Verbose\n\r");
//  strcat(context->outBuf, "CONFIG\n\r");
  sockWrite(context);
  return 0;
}

static int helpSet(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r  Set <LinuxCNC command>\n\r");
  strcat(context->outBuf, "  Set commands require that a hello has been successfully negotiated,\n\r");
  strcat(context->outBuf, "  in most instances requires that control be enabled by the connection.\n\r");
  strcat(context->outBuf, "  The set commands not requiring control enabled are:\n\r");
  strcat(context->outBuf, "    Comm_mode <mode>\n\r");
  strcat(context->outBuf, "    Comm_prot <protocol>\n\r");
  strcat(context->outBuf, "    Echo <On | Off>\n\r");
  strcat(context->outBuf, "    Enable <Pwd | Off>\n\r");
  strcat(context->outBuf, "    Verbose <On | Off>\n\r\n\r");
  strcat(context->outBuf, "  The set commands requiring control enabled are:\n\r");
  strcat(context->outBuf, "    Abort\n\r");
  strcat(context->outBuf, "    Angular_unit_conversion <Deg | Rad | Grad | Auto | Custom>\n\r");
  strcat(context->outBuf, "    Brake <On | Off>\n\r");
  strcat(context->outBuf, "    Debug <Debug level>\n\r");
  strcat(context->outBuf, "    EStop <On | Off>\n\r");
  strcat(context->outBuf, "    Feed_override <Percent>\n\r");
  strcat(context->outBuf, "    Flood <On | Off>\n\r");
  strcat(context->outBuf, "    Home <Axis No>\n\r");
  strcat(context->outBuf, "    Jog <Axis No, Speed>\n\r");
  strcat(context->outBuf, "    Jog_incr <Axis No, Speed, Distance>\n\r");
  strcat(context->outBuf, "    Jog_stop\n\r");
  strcat(context->outBuf, "    Linear_unit_conversion <Inch | CM | MM | Auto | Custom>\n\r");
  strcat(context->outBuf, "    Load_tool_table <Table name>\n\r");
  strcat(context->outBuf, "    Lube <On | Off>\n\r");
  strcat(context->outBuf, "    Machine <On | Off>\n\r");
  strcat(context->outBuf, "    MDI <MDI String>\n\r");
  strcat(context->outBuf, "    Mist <On | Off>\n\r");
  strcat(context->outBuf, "    Mode <Manual | Auto | MDI>\n\r");
  strcat(context->outBuf, "    Open <File path / name>\n\r");
  strcat(context->outBuf, "    Optional_stop <none | 0 | 1>\n\r");
  strcat(context->outBuf, "    Override_limits <On | Off>\n\r");
  strcat(context->outBuf, "    Pause\n\r");
  strcat(context->outBuf, "    Probe\n\r");
  strcat(context->outBuf, "    Probe_clear\n\r");
  strcat(context->outBuf, "    Resume\n\r");
  strcat(context->outBuf, "    Run <Line No>\n\r");
  strcat(context->outBuf, "    SetWait <Time>\n\r");
  strcat(context->outBuf, "    Spindle <Increase | Decrease | Forward | Reverse | Constant | Off>\n\r");
  strcat(context->outBuf, "    Spindle_override <percent>\n\r");
  strcat(context->outBuf, "    Step\n\r");
  strcat(context->outBuf, "    Task_plan_init\n\r");
  strcat(context->outBuf, "    Teleop_enable\n\r");
  strcat(context->outBuf, "    Timeout <Time>\n\r");
  strcat(context->outBuf, "    Tool_offset <Offset>\n\r");
  strcat(context->outBuf, "    Update <On | Off>\n\r");
  strcat(context->outBuf, "    Wait <Time>\n\r");
  
  sockWrite(context);
  return 0;
}

static int helpQuit(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r");
  strcat(context->outBuf, "  The quit command has the server initiate a disconnect from the client,\n\r");
  strcat(context->outBuf, "  the command has no parameters and no requirements to have negotiated\n\r");
  strcat(context->outBuf, "  a hello, or be in control.");
  sockWrite(context);
  return 0;
}

static int helpShutdown(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r");
  strcat(context->outBuf, "  The shutdown command terminates the connection with all clients,\n\r");
  strcat(context->outBuf, "  and initiates a shutdown of LinuxCNC. The command has no parameters, and\n\r");
  strcat(context->outBuf, "  can only be issued by the connection having control.\n\r");
  sockWrite(context);
  return 0;
}

static int helpHelp(connectionRecType *context)
{
  sprintf(context->outBuf, "If you need help on help, it is time to look into another line of work.\n\r");
  sockWrite(context);
  return 0;
}

int commandHelp(connectionRecType *context)
{
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return (helpGeneral(context));
  strupr(pch);
  if (strcmp(pch, "HELLO") == 0) return (helpHello(context));
  if (strcmp(pch, "GET") == 0) return (helpGet(context));
  if (strcmp(pch, "SET") == 0) return (helpSet(context));
  if (strcmp(pch, "QUIT") == 0) return (helpQuit(context));
  if (strcmp(pch, "SHUTDOWN") == 0) return (helpShutdown(context));
  if (strcmp(pch, "HELP") == 0) return (helpHelp(context));
  sprintf(context->outBuf, "%s is not a valid command.", pch);
  sockWrite(context);
  return 0;
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
  
// handle the linuxcncrsh command in context->inBuf
int parseCommand(connectionRecType *context)
{
  int ret = 0;
  char *pch;
  char s[64];
  static const char *helloNakStr = "HELLO NAK\r\n";
  static const char *shutdownNakStr = "SHUTDOWN NAK\r\n";
  static const char *helloAckStr = "HELLO ACK %s 1.1\r\n";
  static const char *setNakStr = "SET NAK\r\n";
    
  pch = strtok(context->inBuf, delims);
  sprintf(s, helloAckStr, serverName);
  if (pch != NULL) {
    strupr(pch);
    switch (lookupToken(pch)) {
      case cmdHello: 
        if (commandHello(context) == -1)
          ret = write(context->cliSock, helloNakStr, strlen(helloNakStr));
        else ret = write(context->cliSock, s, strlen(s));
        break;
      case cmdGet: 
        ret = commandGet(context);
        break;
      case cmdSet:
        if (!context->linked)
	  ret = write(context->cliSock, setNakStr, strlen(setNakStr));
        else ret = commandSet(context);
        break;
      case cmdQuit: 
        ret = commandQuit(context);
        break;
      case cmdShutdown:
        ret = commandShutdown(context);
        if(ret ==0){
          ret = write(context->cliSock, shutdownNakStr, strlen(shutdownNakStr));
        }
	break;
      case cmdHelp:
        ret = commandHelp(context);
	break;
      case cmdUnknown: ret = -2;
      }
    }
  return ret;
}  

void *readClient(void *arg)
{
  char buf[1600];
  int context_index;
  int i;
  int len;
  connectionRecType *context = (connectionRecType *)arg;

  context_index = 0;

  while (1) {
    // We always start this loop with an empty buf, though there may be one
    // partial line in context->inBuf[0..context_index].
    len = read(context->cliSock, buf, sizeof(buf));
    if (len < 0) {
      fprintf(stderr, "linuxcncrsh: error reading from client: %s\n", strerror(errno));
      goto finished;
    }
    if (len == 0) {
      printf("linuxcncrsh: eof from client\n");
      goto finished;
    }

    if (context->echo && context->linked)
      if(write(context->cliSock, buf, len) != (ssize_t)len) {
        fprintf(stderr, "linuxcncrsh: write() failed: %s", strerror(errno));
      }

    for (i = 0; i < len; i ++) {
        if ((buf[i] != '\n') && (buf[i] != '\r')) {
            context->inBuf[context_index] = buf[i];
            context_index ++;
            continue;
        }

        // if we get here, i is the index of a line terminator in buf

        if (context_index > 0) {
            // we have some bytes in the context buffer, parse them now
            context->inBuf[context_index] = '\0';

            // The return value from parseCommand was meant to indicate
            // success or error, but it is unusable.  Some paths return
            // the return value of write(2) and some paths return small
            // positive integers (cmdResponseType) to indicate failure.
            // We're best off just ignoring it.
            (void)parseCommand(context);

            context_index = 0;
        }
    }
  }

finished:
  printf("linuxcncrsh: disconnecting client %s (%s)\n", context->hostName, context->version);
  close(context->cliSock);
  free(context);
  pthread_exit((void *)0);
  sessions--;  // FIXME: not reached
}

int sockMain()
{
    int res;
    
    while (1) {
      int client_sockfd;

      client_len = sizeof(client_address);
      client_sockfd = accept(server_sockfd,
        (struct sockaddr *)&client_address, &client_len);
      if (client_sockfd < 0) exit(0);
      sessions++;
      if ((maxSessions == -1) || (sessions <= maxSessions)) {
        pthread_t *thrd;
        connectionRecType *context;

        thrd = (pthread_t *)calloc(1, sizeof(pthread_t));
        if (thrd == NULL) {
          fprintf(stderr, "linuxcncrsh: out of memory\n");
          exit(1);
        }

        context = (connectionRecType *) malloc(sizeof(connectionRecType));
        if (context == NULL) {
          fprintf(stderr, "linuxcncrsh: out of memory\n");
          exit(1);
        }

        context->cliSock = client_sockfd;
        context->linked = false;
        context->echo = true;
        context->verbose = false;
        strcpy(context->version, "1.0");
        strcpy(context->hostName, "Default");
        context->enabled = false;
        context->commMode = 0;
        context->commProt = 0;
        context->inBuf[0] = 0;

        res = pthread_create(thrd, NULL, readClient, (void *)context);
      } else {
        res = -1;
      }
      if (res != 0) {
        close(client_sockfd);
        sessions--;
        }
     }
    return 0;
}

static void initMain()
{
    emcWaitType = EMC_WAIT_RECEIVED;
    emcCommandSerialNumber = 0;
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

static void usage(char* pname) {
    printf("Usage: \n");
    printf("         %s [Options] [-- LinuxCNC_Options]\n"
           "Options:\n"
           "         --help       this help\n"
           "         --port       <port number>  (default=%d)\n"
           "         --name       <server name>  (default=%s)\n"
           "         --connectpw  <password>     (default=%s)\n"
           "         --enablepw   <password>     (default=%s)\n"
           "         --sessions   <max sessions> (default=%d) (-1 ==> no limit) \n"
           "         --path       <path>         (default=%s)\n"
           "LinuxCNC_Options:\n"
           "          -ini        <inifile>      (default=%s)\n"
          ,pname,port,serverName,pwd,enablePWD,maxSessions,defaultPath,emc_inifile
          );
}

int main(int argc, char *argv[])
{
    int opt;

    initMain();
    // process local command line args
    while((opt = getopt_long(argc, argv, "he:n:p:s:w:d:", longopts, NULL)) != - 1) {
      switch(opt) {
        case 'h': usage(argv[0]); exit(1);
        case 'e': strncpy(enablePWD, optarg, strlen(optarg) + 1); break;
        case 'n': strncpy(serverName, optarg, strlen(optarg) + 1); break;
        case 'p': sscanf(optarg, "%d", &port); break;
        case 's': sscanf(optarg, "%d", &maxSessions); break;
        case 'w': strncpy(pwd, optarg, strlen(optarg) + 1); break;
        case 'd': strncpy(defaultPath, optarg, strlen(optarg) + 1);
        }
      }

    // process LinuxCNC command line args
    // Note: '--' may be used to separate cmd line args
    //       optind is index of next arg to process
    //       make argv[optind] zeroth arg
    argc = argc - optind + 1;
    argv = argv + optind - 1;
    if (emcGetArgs(argc, argv) != 0) {
	rcs_print_error("error in argument list\n");
	exit(1);
    }
    // get configuration information
    iniLoad(emc_inifile);
    initSockets();
    // init NML
    if (tryNml() != 0) {
	rcs_print_error("can't connect to LinuxCNC\n");
	thisQuit();
	exit(1);
    }
    // get current serial number, and save it for restoring when we quit
    // so as not to interfere with real operator interface
    updateStatus();
    emcCommandSerialNumber = emcStatus->echo_serial_number;

    // attach our quit function to SIGINT
    {
        struct sigaction act;
        act.sa_handler = sigQuit;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGINT, &act, NULL);
    }

    // make all threads ignore SIGPIPE
    {
        struct sigaction act;
        act.sa_handler = SIG_IGN;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGPIPE, &act, NULL);
    }

    if (useSockets) sockMain();

    return 0;
}
