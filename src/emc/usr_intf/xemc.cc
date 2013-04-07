/********************************************************************
* Description:   xemc.cc
*   X GUI for the EMC
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Brought forward and adapted to emc2 by Alex Joni
*
* Author: 
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <values.h>             // DBL_MAX, maybe
#include <limits.h>             // DBL_MAX, maybe
#include <stdarg.h>
#include <sys/stat.h>           // struct stat, stat()
#include <unistd.h>
#include <fcntl.h>              // O_CREAT

#include "rcs.hh"               // etime()
#include "emc.hh"               // EMC NML
#include "emc_nml.hh"
#include "emcglb.h"             // EMC_NMLFILE, TRAJ_MAX_VELOCITY, TOOL_TABLE_FILE
#include "emccfg.h"             // DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"           // INIFILE
#include "rcs_print.hh"
#include "nml_oi.hh"
#include "timer.hh"

/*
 * Include files required for all Toolkit programs
 */
#include <X11/Intrinsic.h>      /* Intrinsics Definitions */
#include <X11/StringDefs.h>     /* Standard Name-String definitions */

/*
 * Public include file for widgets we actually use in this file.
 */
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/Dialog.h>

#define UPDATE_MSECS 100

// the NML channels to the EMC task
static RCS_CMD_CHANNEL *emcCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcStatusBuffer = 0;
EMC_STAT *emcStatus = 0;

// the NML channel for errors
static NML *emcErrorBuffer = 0;
static char error_string[NML_ERROR_LEN] = "";

// the current command numbers, set up updateStatus(), used in main()
static int emcCommandSerialNumber = 0;

// forward decls

// forward decls for error popup
static void errorReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params);
static void errorDoneCB(Widget w, XtPointer client_data, XtPointer call_data);
static int createErrorShell();
static int destroyErrorShell();
static void popupError(const char *fmt, ...) __attribute__((format(printf,1,2)));

// forward decl for quit() function
static void quit();

static int emcTaskNmlGet()
{
  int retval = 0;

  // try to connect to EMC cmd
  if (emcCommandBuffer == 0)
    {
      emcCommandBuffer = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", emc_nmlfile);
      if (! emcCommandBuffer->valid())
        {
          delete emcCommandBuffer;
          emcCommandBuffer = 0;
          retval = -1;
        }
    }

  // try to connect to EMC status
  if (emcStatusBuffer == 0)
    {
      emcStatusBuffer = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", emc_nmlfile);
      if (! emcStatusBuffer->valid() ||
          EMC_STAT_TYPE != emcStatusBuffer->peek())
        {
          delete emcStatusBuffer;
          emcStatusBuffer = 0;
          emcStatus = 0;
          retval = -1;
        }
      else
        {
          emcStatus = (EMC_STAT *) emcStatusBuffer->get_address();
        }
    }

  return retval;
}

static int emcErrorNmlGet()
{
  int retval = 0;

  if (emcErrorBuffer == 0)
    {
      emcErrorBuffer = new NML(nmlErrorFormat, "emcError", "xemc", emc_nmlfile);
      if (! emcErrorBuffer->valid())
        {
          delete emcErrorBuffer;
          emcErrorBuffer = 0;
          retval = -1;
        }
    }

  return retval;
}

static void printError(const char *error)
{
  printf("%s\n", error);
}

static int updateStatus()
{
  NMLTYPE type;

  if (0 == emcStatus ||
      0 == emcStatusBuffer ||
      ! emcStatusBuffer->valid())
    {
      return -1;
    }

  switch (type = emcStatusBuffer->peek())
    {
    case -1:
      // error on CMS channel
      return -1;
      break;

    case 0:                     // no new data
    case EMC_STAT_TYPE: // new data
      // new data
      break;

    default:
      return -1;
      break;
    }

  return 0;
}

static int updateError()
{
  NMLTYPE type;

  if (0 == emcErrorBuffer ||
      ! emcErrorBuffer->valid())
    {
      return -1;
    }

  switch (type = emcErrorBuffer->read())
    {
    case -1:
      // error reading channel
      return -1;
      break;

    case 0:
      // nothing new
      error_string[0] = 0;
      break;

    case EMC_OPERATOR_ERROR_TYPE:
      strncpy(error_string,
             ((EMC_OPERATOR_ERROR *) (emcErrorBuffer->get_address()))->error,
              LINELEN - 1);
      error_string[LINELEN - 1] = 0;
      break;

    case EMC_OPERATOR_TEXT_TYPE:
      strncpy(error_string,
             ((EMC_OPERATOR_TEXT *) (emcErrorBuffer->get_address()))->text,
              LINELEN - 1);
      error_string[LINELEN - 1] = 0;
      break;

    case EMC_OPERATOR_DISPLAY_TYPE:
      strncpy(error_string,
             ((EMC_OPERATOR_DISPLAY *) (emcErrorBuffer->get_address()))->display,
              LINELEN - 1);
      error_string[LINELEN - 1] = 0;
      break;

    case NML_ERROR_TYPE:
      strncpy(error_string,
             ((NML_ERROR *) (emcErrorBuffer->get_address()))->error,
              NML_ERROR_LEN - 1);
      error_string[NML_ERROR_LEN - 1] = 0;
      break;

    case NML_TEXT_TYPE:
      strncpy(error_string,
             ((NML_TEXT *) (emcErrorBuffer->get_address()))->text,
              NML_ERROR_LEN - 1);
      error_string[NML_ERROR_LEN - 1] = 0;
      break;

    case NML_DISPLAY_TYPE:
      strncpy(error_string,
             ((NML_DISPLAY *) (emcErrorBuffer->get_address()))->display,
              NML_ERROR_LEN - 1);
      error_string[NML_ERROR_LEN - 1] = 0;
      break;

    default:
      sprintf(error_string, "unrecognized error %ld",type);
      return -1;
      break;
    }

  return 0;
}

#define EMC_COMMAND_TIMEOUT 1.0 // how long to wait until timeout
#define EMC_COMMAND_DELAY   0.1 // how long to sleep between checks

static int emcCommandWaitDone(int serial_number)
{
  double start = etime();

  while (etime() - start < EMC_COMMAND_TIMEOUT) {
    updateStatus();

    if (emcStatus->echo_serial_number == serial_number) {
      if (emcStatus->status == RCS_DONE) {
        return 0;
      }
      else if (emcStatus->status == RCS_ERROR) {
        return -1;
      }
    }

    esleep(EMC_COMMAND_DELAY);
  }

  return -1;
}

/*
  functions for handling the windowing of a file, where you
  give the line you wish to window and it produces an array
  of string, or single string, of the file's contents
  at that point and some lines beyond
  */

#define LF 10
#define CR 13

typedef struct {
  /* the array holding the window of lines in the file */
  char *fileWindow;

  /* the number of lines in the window */
  int maxWindowLines;

  /* the max length of each line */
  int maxLineLen;

  /* which array index holds the first filled slot */
  int windowStart;

  /* which array index holds the next open slot */
  int windowEnd;

  /* number in ring, also used to differentiate start = end as full/empty */
  int windowCount;

  /* the file to window */
  FILE * fileFp;

  /* the line currently at the top of the window */
  int fileFpLine;

  /* flag that the line should be kept */
  int keepNextLine;

} FILE_WINDOW;

static int fwClear(FILE_WINDOW *fw)
{
  int t;

  if (NULL == fw) {
    return -1;
  }

  for (t = 0; t < fw->maxWindowLines; t++) {
    fw->fileWindow[t * fw->maxLineLen] = 0;
  }

  fw->windowStart = 0;
  fw->windowEnd = 0;
  fw->windowCount = 0;

  fw->fileFpLine = 0;

  fw->keepNextLine = 1;

  return 0;
}

static int fwInit(FILE_WINDOW *fw, int _maxWindowLines, int _maxLineLen)
{
  if (NULL == fw) {
    return -1;
  }

  fw->fileWindow = (char *) malloc(_maxLineLen * _maxWindowLines);

  fw->maxWindowLines = _maxWindowLines;
  fw->maxLineLen = _maxLineLen;

  /* clear out the remaining vars */
  if (0 != fwClear(fw)) {
    return -1;
  }

  /* mark the file closed */
  fw->fileFp = NULL;

  return 0;
}

static int fwOpen(FILE_WINDOW *fw, const char *file)
{
  if (NULL == fw) {
    return -1;
  }

  /* close any open file */
  if (NULL != fw->fileFp) {
    fclose(fw->fileFp);
    fw->fileFp = NULL;
  }

  if (NULL == (fw->fileFp = fopen(file, "r"))) {
    return -1;
  }

  fw->keepNextLine = 1;

  return 0;
}

static int fwClose(FILE_WINDOW *fw)
{
  int retval = 0;

  if (NULL == fw) {
    return -1;
  }

  /* first clear out the window */
  if (0 != fwClear(fw)) {
    retval = -1;
  }

  if (NULL != fw->fileFp) {
    fclose(fw->fileFp);
  }
  fw->fileFp = NULL;

  return retval;
}

#if 0
static int fwDelete(FILE_WINDOW *fw)
{
  if (NULL == fw) {
    return -1;
  }

  /* should have been closed by call to fwClose(), but make sure */
  if (NULL != fw->fileFp) {
    fclose(fw->fileFp);
    fw->fileFp = NULL;
  }

  free(fw->fileWindow);

  return 0;
}

static int fwPrintWindow(FILE_WINDOW *fw)
{
  int start;

  if (NULL == fw) {
    return -1;
  }

  start = fw->windowStart;

  if (0 == fw->windowCount) {
    return 0;
  }

  do {
    printf("%s\n", &(fw->fileWindow[start * fw->maxLineLen]));
    if (++start >= fw->maxWindowLines) {
      start = 0;
    }
  } while (start != fw->windowEnd);

  return 0;
}
#endif

static int fwAddLine(FILE_WINDOW *fw, const char *line)
{
  if (NULL == fw) {
    return -1;
  }

  strncpy(&fw->fileWindow[fw->windowEnd * fw->maxLineLen], line, fw->maxLineLen - 1);
  fw->fileWindow[fw->windowEnd * fw->maxLineLen + fw->maxLineLen - 1] = 0;

  if (fw->windowEnd == fw->windowStart &&
      0 != fw->windowCount) {
    /* we're full, so move fw->windowStart up */
    /* and don't increment fw->windowCount */
    if (++fw->windowStart >= fw->maxWindowLines) {
      fw->windowStart = 0;
    }
  }
  else {
    /* we're not full, so no need to move fw->windowStart up */
    /* but do increment fw->windowCount */
    ++fw->windowCount;
  }

  /* now increment fw->windowEnd to point to next slot */
  if (++fw->windowEnd >= fw->maxWindowLines) {
    fw->windowEnd = 0;
  }

  return 0;
}

#if 0
static int fwDeleteLine(FILE_WINDOW *fw)
{
  if (NULL == fw) {
    return -1;
  }

  if (0 == fw->windowCount) {
    return 0;
  }

  fw->fileWindow[fw->windowStart * fw->maxLineLen] = 0;
  if (++fw->windowStart >= fw->maxWindowLines) {
    fw->windowStart = 0;
  }

  --fw->windowCount;

  return 0;
}
#endif

static int fwSyncLine(FILE_WINDOW *fw, int syncLine)
{
  char line[256];               // FIXME-- hardcoded
  int pad;
  int len;
  int sawEol;

  if (NULL == fw) {
    return -1;
  }

  if (NULL == fw->fileFp) {
    return -1;
  }

  /* if syncLine is <= 0, make it 1 */
  if (syncLine <= 0) {
    syncLine = 1;
  }

  /* reset syncLine so that it means the first line is synched */
  syncLine += fw->maxWindowLines - 1;

  /* check if our window is ahead of file, and rewind if so */
  if (syncLine < fw->fileFpLine) {
    /* we're ahead of program, so rewind */
    rewind(fw->fileFp);
    /* and clear out fw->fileWindow */
    fwClear(fw);
  }

  /* now the window is at or behind the file */
  /* so fill it up */
  while (fw->fileFpLine < syncLine) {
    if (NULL == fgets(line, fw->maxLineLen, fw->fileFp)) {
      /* end file */
      /* pad remainder if any */
      pad = syncLine - fw->fileFpLine;
      while (pad-- > 0) {
        fwAddLine(fw, "");
      }
      fw->fileFpLine = syncLine;
      break;
    }

    sawEol = 0;
    len = strlen(line);
    while (--len >= 0) {
      if (CR == line[len] ||
          LF == line[len]) {
        line[len] = 0;
        sawEol = 1;
      }
      else {
        break;
      }
    }

    if (fw->keepNextLine) {
      fwAddLine(fw, line);
      ++fw->fileFpLine;
    }

    fw->keepNextLine = sawEol;
  }

  return 0;
}

static int fwString(FILE_WINDOW *fw, char *string)
{
  int start;

  if (NULL == fw) {
    return -1;
  }

  start = fw->windowStart;
  string[0] = 0;

  if (0 == fw->windowCount) {
    return 0;
  }

  do {
    strncat(string, &(fw->fileWindow[start * fw->maxLineLen]),
            fw->maxLineLen - 2);
    strcat(string, "\n");
    if (++start >= fw->maxWindowLines) {
      start = 0;
    }
  } while (start != fw->windowEnd);

  return 0;
}

// the file window structure for the program window and related stuff

#define PROGRAM_FW_NUM_LINES 10
#define PROGRAM_FW_LEN_LINES 80

static char *programFwString = NULL;
static FILE_WINDOW programFw;

// number of axes supported
#define XEMC_NUM_AXES 3

// string for ini file version num
static char version_string[LINELEN] = "";

// interpreter parameter file name, from ini file
static char PARAMETER_FILE[LINELEN] = "";

// help file name, from ini file
static char HELP_FILE[LINELEN] = "";

// the program path prefix
static char programPrefix[LINELEN] = "";
// the program name currently displayed
static char programFile[LINELEN] = "*";
// the program last loaded by the controller
static char lastProgramFile[LINELEN] = "*";

// integer version of ini file max scale factor
static int maxFeedOverride = 100;

#define MDI_LINELEN 80
static char active_g_codes_string[MDI_LINELEN] = "";
static char active_m_codes_string[MDI_LINELEN] = "";

// how position is to be displayed-- relative or machine
typedef enum {
  COORD_RELATIVE = 1,
  COORD_MACHINE
} COORD_TYPE;

static COORD_TYPE coords = COORD_RELATIVE;

// how position is to be displayed-- actual or commanded
typedef enum {
  POS_DISPLAY_ACT = 1,
  POS_DISPLAY_CMD
} POS_DISPLAY_TYPE;

static POS_DISPLAY_TYPE posDisplay = POS_DISPLAY_ACT;

// marker for the active axis
static int activeAxis = 0;      // default is 0, X
static int oldActiveAxis = -1;  // force an update at startup

/*
  Note: the X key press/release events with multiple keys behave such that
  if key a is pressed, then b, then a released, its key-release event
  won't go through. So, multi-axis jogging has been disallowed in xemc,
  although it's supported in the motion system.
*/
// flag that an axis is jogging, so other jogs won't go out
static int axisJogging = -1;

// current jog speed setting
static int jogSpeed = 1;        // overridden in iniLoad()
static int maxJogSpeed = 1;     // overridden in iniLoad()
static int jogSpeedChange = 0;  // 0 means no change, +/- means inc/dec

// current jog increment setting, non-positive means continuous
static double jogIncrement = 0.0;

// the size of the smallest increment to jog, = 1/INPUT_SCALE
static double stepIncrement = 0.0001;

// polarities for axis jogging, from ini file
static int jogPol[XEMC_NUM_AXES];

static int oldFeedOverride = -1; // forces an update at startup
static int feedOverride;        // 100% integer copy of EMC status
static int feedOverrideChange = 0; // same as jogSpeedChange
#define FEED_OVERRIDE_DELAY_COUNT 1
// timer delays until dec/inc appears
static int feedOverrideDelayCount = FEED_OVERRIDE_DELAY_COUNT;

// command sending functions

static int sendEstop()
{
  EMC_TASK_SET_STATE state_msg;

  state_msg.state = EMC_TASK_STATE_ESTOP;
  state_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(state_msg);

  return 0;
}

static int sendEstopReset()
{
  EMC_TASK_SET_STATE state_msg;

  state_msg.state = EMC_TASK_STATE_ESTOP_RESET;
  state_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(state_msg);

  return 0;
}

static int sendMachineOn()
{
  EMC_TASK_SET_STATE state_msg;

  state_msg.state = EMC_TASK_STATE_ON;
  state_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(state_msg);

  return 0;
}

static int sendMachineOff()
{
  EMC_TASK_SET_STATE state_msg;

  state_msg.state = EMC_TASK_STATE_OFF;
  state_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(state_msg);

  return 0;
}

static int sendManual()
{
  EMC_TASK_SET_MODE mode_msg;

  mode_msg.mode = EMC_TASK_MODE_MANUAL;
  mode_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(mode_msg);

  return 0;
}

static int sendAuto()
{
  EMC_TASK_SET_MODE mode_msg;

  mode_msg.mode = EMC_TASK_MODE_AUTO;
  mode_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(mode_msg);

  return 0;
}

static int sendMdi()
{
  EMC_TASK_SET_MODE mode_msg;

  mode_msg.mode = EMC_TASK_MODE_MDI;
  mode_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(mode_msg);

  return 0;
}

static int sendToolSetOffset(int toolno, double zoffset, double diameter)
{
  EMC_TOOL_SET_OFFSET emc_tool_set_offset_msg;

  emc_tool_set_offset_msg.toolno = toolno;
  emc_tool_set_offset_msg.offset.tran.z = zoffset;
  emc_tool_set_offset_msg.diameter = diameter;
  emc_tool_set_offset_msg.orientation = 0; // mill style tool table

  emc_tool_set_offset_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_tool_set_offset_msg);

  return 0;
}

static int sendMistOn()
{
  EMC_COOLANT_MIST_ON emc_coolant_mist_on_msg;

  emc_coolant_mist_on_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_coolant_mist_on_msg);

  return 0;
}

static int sendMistOff()
{
  EMC_COOLANT_MIST_OFF emc_coolant_mist_off_msg;

  emc_coolant_mist_off_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_coolant_mist_off_msg);

  return 0;
}

static int sendFloodOn()
{
  EMC_COOLANT_FLOOD_ON emc_coolant_flood_on_msg;

  emc_coolant_flood_on_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_coolant_flood_on_msg);

  return 0;
}

static int sendFloodOff()
{
  EMC_COOLANT_FLOOD_OFF emc_coolant_flood_off_msg;

  emc_coolant_flood_off_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_coolant_flood_off_msg);

  return 0;
}

static int sendSpindleForward()
{
  EMC_SPINDLE_ON emc_spindle_on_msg;

  emc_spindle_on_msg.speed = +1;
  emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_on_msg);

  return 0;
}

static int sendSpindleReverse()
{
  EMC_SPINDLE_ON emc_spindle_on_msg;

  emc_spindle_on_msg.speed = -1;
  emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_on_msg);

  return 0;
}

static int sendSpindleOff()
{
  EMC_SPINDLE_OFF emc_spindle_off_msg;

  emc_spindle_off_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_off_msg);

  return 0;
}

static int sendSpindleIncrease()
{
  EMC_SPINDLE_INCREASE emc_spindle_increase_msg;

  emc_spindle_increase_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_increase_msg);

  return 0;
}

static int sendSpindleDecrease()
{
  EMC_SPINDLE_DECREASE emc_spindle_decrease_msg;

  emc_spindle_decrease_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_decrease_msg);

  return 0;
}

static int sendSpindleConstant()
{
  EMC_SPINDLE_CONSTANT emc_spindle_constant_msg;

  emc_spindle_constant_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_constant_msg);

  return 0;
}

static int sendBrakeEngage()
{
  EMC_SPINDLE_BRAKE_ENGAGE emc_spindle_brake_engage_msg;

  emc_spindle_brake_engage_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_brake_engage_msg);

  return 0;
}

static int sendBrakeRelease()
{
  EMC_SPINDLE_BRAKE_RELEASE emc_spindle_brake_release_msg;

  emc_spindle_brake_release_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_spindle_brake_release_msg);

  return 0;
}

static int sendAbort()
{
  EMC_TASK_ABORT task_abort_msg;

  task_abort_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(task_abort_msg);

  return 0;
}

static int sendOverrideLimits()
{
  EMC_AXIS_OVERRIDE_LIMITS lim_msg;

  lim_msg.axis = 0;             // same number for all
  lim_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(lim_msg);

  return 0;
}

static int sendJogStop(int axis)
{
  EMC_AXIS_ABORT emc_axis_abort_msg;

  if (axis < 0 || axis >= XEMC_NUM_AXES) {
    return -1;
  }

  // don't send request to jog if none are jogging
  if (axisJogging == -1) {
    return 0;
  }

  emc_axis_abort_msg.serial_number = ++emcCommandSerialNumber;
  emc_axis_abort_msg.axis = axisJogging;
  emcCommandBuffer->write(emc_axis_abort_msg);

  axisJogging = -1;

  return 0;
}

static int sendJogCont(int axis, double speed)
{
  EMC_AXIS_JOG emc_axis_jog_msg;

  if (axis < 0 || axis >= XEMC_NUM_AXES) {
    return -1;
  }

  if (axisJogging != -1) {
    // ignore request to jog, if an axis is already jogging
    return 0;
  }

  if (0 == jogPol[axis]) {
    speed = -speed;
  }

  emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
  emc_axis_jog_msg.axis = axis;
  emc_axis_jog_msg.vel = speed / 60.0;
  emcCommandBuffer->write(emc_axis_jog_msg);

  axisJogging = axis;

  return 0;
}

static int sendJogIncr(int axis, double speed, double incr)
{
  EMC_AXIS_INCR_JOG emc_axis_incr_jog_msg;

  if (axis < 0 || axis >= XEMC_NUM_AXES) {
    return -1;
  }

  if (axisJogging != -1) {
    // ignore request to jog, if an axis is already jogging
    return 0;
  }

  if (0 == jogPol[axis]) {
    speed = -speed;
  }

  emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
  emc_axis_incr_jog_msg.axis = axis;
  emc_axis_incr_jog_msg.vel = speed / 60.0;
  emc_axis_incr_jog_msg.incr = jogIncrement;
  emcCommandBuffer->write(emc_axis_incr_jog_msg);

  // don't flag incremental jogs as jogging an axis-- we can
  // allow multiple incremental jogs since we don't need a key release

  return 0;
}

static int sendHome(int axis)
{
  EMC_AXIS_HOME emc_axis_home_msg;

  emc_axis_home_msg.serial_number = ++emcCommandSerialNumber;
  emc_axis_home_msg.axis = axis;
  emcCommandBuffer->write(emc_axis_home_msg);

  return 0;
}

static int sendFeedOverride(double override)
{
  EMC_TRAJ_SET_SCALE emc_traj_set_scale_msg;

  if (override < 0.0) {
    override = 0.0;
  }
  else if (override > (double) maxFeedOverride / 100.0) {
    override = (double) maxFeedOverride / 100.0;
  }
  emc_traj_set_scale_msg.serial_number = ++emcCommandSerialNumber;
  emc_traj_set_scale_msg.scale = override;
  emcCommandBuffer->write(emc_traj_set_scale_msg);

  return 0;
}

static int sendTaskPlanInit()
{
  EMC_TASK_PLAN_INIT task_plan_init_msg;

  task_plan_init_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(task_plan_init_msg);

  return 0;
}

static int sendProgramOpen(char *program)
{
  EMC_TASK_PLAN_OPEN emc_task_plan_open_msg;

  // first put in auto mode if it's not
  if (EMC_TASK_MODE_AUTO != emcStatus->task.mode) {
    // send a request to go to auto mode
    sendAuto();
  }

  // wait for any previous one to go out
  if (0 != emcCommandWaitDone(emcCommandSerialNumber)) {
    printError("error executing command\n");
    return -1;
  }

  emc_task_plan_open_msg.serial_number = ++emcCommandSerialNumber;
  strcpy(emc_task_plan_open_msg.file, program);
  emcCommandBuffer->write(emc_task_plan_open_msg);

  // now clear out our stored version of the program, in case
  // the file contents have changed but the name is the same
  programFile[0] = 0;

  return 0;
}

// line in program to run from; set it in GUI when user clicks on a line,
// and pass it in calls to sendProgramRun(). sendProgramRun() won't use
// this directly.
static int programStartLine = 0;
static int programStartLineLast = 0;

static int sendProgramRun(int line)
{
  EMC_TASK_PLAN_RUN emc_task_plan_run_msg;

  // first reopen program if it's not open
  if (0 == emcStatus->task.file[0]) {
    // send a request to open last one
    sendProgramOpen(lastProgramFile);

    // wait for command to go out
    if (0 != emcCommandWaitDone(emcCommandSerialNumber)) {
      printError("error executing command\n");
      return -1;
    }
  }

  emc_task_plan_run_msg.serial_number = ++emcCommandSerialNumber;
  emc_task_plan_run_msg.line = line;
  emcCommandBuffer->write(emc_task_plan_run_msg);

  programStartLineLast = programStartLine;
  programStartLine = 0;

  return 0;
}

static int sendProgramPause()
{
  EMC_TASK_PLAN_PAUSE emc_task_plan_pause_msg;

  emc_task_plan_pause_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_task_plan_pause_msg);

  return 0;
}

static int sendProgramResume()
{
  EMC_TASK_PLAN_RESUME emc_task_plan_resume_msg;

  emc_task_plan_resume_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_task_plan_resume_msg);

  return 0;
}

static int sendProgramStep()
{
  EMC_TASK_PLAN_STEP emc_task_plan_step_msg;

  // first reopen program if it's not open
  if (0 == emcStatus->task.file[0]) {
    // send a request to open last one
    sendProgramOpen(lastProgramFile);

    // wait for command to go out
    if (0 != emcCommandWaitDone(emcCommandSerialNumber)) {
      printError("error executing command\n");
      return -1;
    }
  }

  emc_task_plan_step_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_task_plan_step_msg);

  programStartLineLast = programStartLine;
  programStartLine = 0;

  return 0;
}

static int sendMdiCmd(char *mdi)
{
  EMC_TASK_PLAN_EXECUTE emc_task_plan_execute_msg;

  strcpy(emc_task_plan_execute_msg.command, mdi);
  emc_task_plan_execute_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_task_plan_execute_msg);

  return 0;
}

static int sendLoadToolTable(const char *file)
{
  EMC_TOOL_LOAD_TOOL_TABLE emc_tool_load_tool_table_msg;

  strcpy(emc_tool_load_tool_table_msg.file, file);
  emc_tool_load_tool_table_msg.serial_number = ++emcCommandSerialNumber;
  emcCommandBuffer->write(emc_tool_load_tool_table_msg);

  return 0;
}

// paths to awk and xgraph, using to plot logs
// fill these in for your system, if these are not in user's path
#define AWK_PATH "awk"
#define XGRAPH_PATH "xgraph"
#define SYSTEM_STRING_LEN 1024

// X stuff starts here

// flag signifying that window needs to be redrawn
static int redraw = 1;

// global context and widgets, accessed during periodic timer
static XtAppContext app_context;
static Atom killAtom;
static Widget topLevel;
static Widget topForm;
static Widget barMenuForm;
static Widget abortCommand;
static Widget fileMenu;
static Widget viewMenu;
static Widget settingsMenu;
static Widget helpMenu;
static Widget limCommand;
static Widget commandMenuForm;
static Widget stateMenu;
static Widget modeMenu;
static Widget mistMenu;
static Widget floodMenu;
static Widget spindleMenu;
static Widget spindleIncLabel, spindleDecLabel;
static Widget brakeMenu;
static Widget toolTableShell = NULL, toolTableForm, toolTableLabel, toolTableText, toolTableDone, toolTableCancel;
static Widget varFileShell = NULL, varFileForm, varFileLabel, varFileText, varFileDone, varFileCancel;
static Widget toolNumberForm, toolNumberFormTitle, toolNumberFormName;
static Widget toolOffsetForm, toolOffsetFormTitle, toolOffsetFormName;
static Widget toolSetOffsetShell, toolSetOffsetForm, toolSetOffsetToolLabel, toolSetOffsetTool, toolSetOffsetLengthLabel, toolSetOffsetLength, toolSetOffsetDiameterLabel, toolSetOffsetDiameter, toolSetOffsetDone, toolSetOffsetCancel;
static Widget positionTypeForm, positionTypeFormTitle, positionTypeFormName;
static Widget workOffsetForm, workOffsetFormTitle, workOffsetFormName;
static Widget posLabel[XEMC_NUM_AXES];
static Widget posOffsetShell, posOffsetDialog;
static Widget jogSpeedForm, jogSpeedTitleLabel,
  jogSpeedLabel, jogSpeedIncLabel, jogSpeedDecLabel;
static Widget jogSpeedShell, jogSpeedDialog;
static Widget jogIncrementForm, jogIncrementTitleLabel, jogIncrementMenu;
static Widget jogForm, jogTitleLabel,
  jogMinusLabel, homeCommand, jogPlusLabel;
static Widget feedOverrideForm, feedOverrideTitleLabel,
  feedOverrideLabel, feedOverrideIncLabel, feedOverrideDecLabel;
static Widget feedOverrideShell, feedOverrideDialog;
static Widget mdiForm, mdiFormTitle, mdiFormText;
static Widget mdiCodesLabel;
static Widget programForm, programFormTitle, programFormName, programFormStateTitle, programFormState;
static Widget programOpenCommand, programRunCommand, programPauseCommand, programResumeCommand, programStepCommand, programVerifyCommand;
static Widget programText;
static Widget fileOpenShell, fileOpenDialog, fileOpenDone, fileOpenCancel;
static char EDITOR_FILE[256] = ""; // FIXME-- hardcoded
static Widget fileEditShell, fileEditDialog, fileEditDone, fileEditCancel;
static Widget fileEditorShell = NULL, fileEditorForm, fileEditorLabel, fileEditorText, fileEditorDone, fileEditorCancel, fileEditorMark;
static Widget fileQuitShell, fileQuitDialog, fileQuitDone, fileQuitCancel;
static Widget helpXemcShell = NULL, helpXemcForm, helpXemcLabel, helpXemcText, helpXemcDone;
static Widget helpAboutShell, helpAboutDialog, helpAboutDone;
static Widget errorShell = 0, errorDialog = 0, errorDone = 0;

// Pixel values inited from strings, for hard-coded colors
static Pixel pixelWhite;
static Pixel pixelBlack;
static Pixel pixelRed;
static Pixel pixelYellow;
static Pixel pixelGreen;

// saved background and border colors for position labels,
// cleared and restored when highlighting
static Pixel posLabelBackground[XEMC_NUM_AXES];
static Pixel posLabelBorderColor[XEMC_NUM_AXES];

static int getColor(Widget w, Pixel *pixel, int foreground)
{
  Arg args;

  if (foreground)
    XtSetArg(args, XtNforeground, pixel);
  else
    XtSetArg(args, XtNbackground, pixel);
  XtGetValues(w, &args, 1);

  return 0;
}

static int setColor(Widget w, Pixel pixel, int foreground)
{
  if (foreground)
    XtVaSetValues(w, XtNforeground, pixel, NULL);
  else
    XtVaSetValues(w, XtNbackground, pixel, NULL);

  return 0;
}

static int stringToPixel(Widget w, const char * string, Pixel *pixel)
{
  XrmValue from, to;

  from.addr = (char*)string;
  from.size = strlen(string) + 1;
  to.addr = (char *) pixel;
  to.size = sizeof(Pixel);

  if (False == XtConvertAndStore(w, XtRString, &from, XtRPixel, &to))
    return -1;
  return 0;
}

static int getBorderColor(Widget w, Pixel *pixel)
{
  Arg args;

  XtSetArg(args, XtNborderColor, pixel);
  XtGetValues(w, &args, 1);

  return 0;
}

static int setBorderColor(Widget w, Pixel pixel)
{
  XtVaSetValues(w, XtNborderColor, pixel, NULL);

  return 0;
}

static int setLabel(Widget w, char *label)
{
  Arg args;
  int width;

  XtSetArg(args, XtNwidth, &width);
  XtGetValues(w, &args, 1);
  XtVaSetValues(w, XtNlabel, label, NULL);
  XtVaSetValues(w, XtNwidth, width, NULL);
  return(0);
}

static int setProgramText(char *text)
{
  XtVaSetValues(programText, XtNstring, text, NULL);
  return(0);
}

// index is diff between top and line to highlight, 0..max
static int highlightProgramText(char *text, int index)
{
  int start, end;

  // position 'start' to first char on index line
  for (start = 0; ; start++) {
    if (index <= 0) {
      break;
    }
    if (text[start] == 0 || text[start] == '\n') {
      index--;
    }
  }

  // position 'end' to last char on index line
  for (end = start; ; end++) {
    if (text[end] == 0 || text[end] == '\n') {
      break;
    }
  }

  XawTextSetSelection(programText, start, end);
  return(0);
}

// dialogPopup() is an XtNcallback that expects a
// transientShellWidget class as the client_data.
// Widget w is not used, but is required if this is to be used
// as a callback function. If you call it directly you can put NULL
// in for w and call_data.
static void dialogPopup(Widget w, XtPointer client_data, XtPointer call_data)
{
  Position x, y;
  Dimension width, height;

  // get the coordinates of the middle of topLevel widget
  XtVaGetValues(topForm,
                XtNwidth, &width,
                XtNheight, &height,
                NULL);

  // translate coordinates in application top-level window
  // into coordinates from root window origin
  XtTranslateCoords(topForm,
                    (Position) 100,
                    (Position) 100,
                    &x, &y);

  // move popup shell to this position (it's not visible yet)
  XtVaSetValues((Widget) client_data,
                XtNx, x,
                XtNy, y,
                NULL);

  XtPopup((Widget) client_data, XtGrabNone);
}

// command callbacks

static void limCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  sendOverrideLimits();
}

static void abortCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  sendAbort();
}

static void homeCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  sendHome(activeAxis);
}

// flag to prevent errors from popping up multiple times
static int errorIsPopped = 0;

static void errorReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  XtPopdown(errorShell);
  errorIsPopped = 0;
  destroyErrorShell();
}

static void errorDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  XtPopdown(errorShell);
  errorIsPopped = 0;
  destroyErrorShell();
}

// createErrorShell() creates the error dialog widget afresh,
// so that it gets sized correctly for each different error message.
// Otherwise, the widget is sized for the first message, and then
// that size is used for all subsequent messages.

static int createErrorShell()
{
  if (NULL != errorShell) {
    return 0;
  }

  errorShell =
    XtVaCreatePopupShell("errorShell",
                         transientShellWidgetClass,
                         topLevel,
                         NULL);

  errorDialog =
    XtVaCreateManagedWidget("errorDialog",
                            dialogWidgetClass,
                            errorShell,
                            NULL);

  errorDone =
    XtVaCreateManagedWidget("errorDone",
                            commandWidgetClass,
                            errorDialog,
                            NULL);

  XtAddCallback(errorDone, XtNcallback, errorDoneCB, errorShell);

  return 0;
}

static int destroyErrorShell()
{
  if (0 != errorDone)
    {
      XtDestroyWidget(errorDone);
      errorDone = 0;
    }

  if (0 != errorDialog)
    {
      XtDestroyWidget(errorDialog);
      errorDialog = 0;
    }

  if (0 != errorShell)
    {
      XtDestroyWidget(errorShell);
      errorShell = 0;
    }

  return 0;
}

static void popupError(const char *fmt, ...)
{
  va_list ap;
  char string[256];             // FIXME-- hardcoded value

  va_start(ap, fmt);
  vsprintf(string, fmt, ap);
  va_end(ap);

  if (errorIsPopped) {
    // we're popped up already, so just print the error to stdout
    fprintf(stderr, "error: %s\n", string);
    return;
  }

  destroyErrorShell();
  createErrorShell();

  XtVaSetValues(errorDialog, XtNlabel, string, NULL);
  errorIsPopped = 1;
  dialogPopup(NULL, errorShell, NULL);
  // trap window kill
  XSetWMProtocols(XtDisplay(topLevel), XtWindow(errorShell), &killAtom, 1);
}

// file edit forward decls
static void fileEditorDoneCB(Widget w, XtPointer client_data, XtPointer call_data);
static void fileEditorCancelCB(Widget w, XtPointer client_data, XtPointer call_data);
static void fileEditorMarkCB(Widget w, XtPointer client_data, XtPointer call_data);
static int createFileEditorShell(int writeable);
static int destroyFileEditorShell();
static void doFileEditorDone(int get);

static void fileEditorDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doFileEditorDone(1);
}

static void fileEditorCancelCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doFileEditorDone(0);
}

static void fileEditorMarkCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  FILE *fp;
  XawTextPosition pos;
  int charcount = 0;
  int line = 1;
  char buffer[256];             // FIXME-- hardcoded

  if (NULL == (fp = fopen(EDITOR_FILE, "r"))) {
    return;
  }

  programStartLine = 0;
  pos = XawTextGetInsertionPoint(fileEditorText);

  while (! feof(fp)) {
    if (NULL == fgets(buffer, 256, fp)) {
      // hmmm... never found line
      popupError("Can't mark line-- character position\n%d outside range of file", (int)pos);
      break;
    }

    charcount += strlen(buffer);
    if (charcount > pos) {
      // found it
      sendProgramOpen(EDITOR_FILE);
      popupError("Marking program to start here:\n%s", buffer);
      programStartLine = line;
      break;
    }

    line++;
  }

  fclose(fp);
}

static int createFileEditorShell(int writeable)
{
  char label[256];              // FIXME-- hardcoded

  if (NULL != fileEditorShell) {
    return 0;
  }

  strcpy(label, EDITOR_FILE);
  if (! writeable) {
    strcat(label, " (read only)");
  }

  fileEditorShell =
    XtVaCreatePopupShell("fileEditorShell",
                         topLevelShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  fileEditorForm =
    XtVaCreateManagedWidget("fileEditorForm",
                            formWidgetClass,
                            fileEditorShell,
                            NULL);

  fileEditorLabel =
    XtVaCreateManagedWidget("fileEditorLabel",
                            labelWidgetClass,
                            fileEditorForm,
                            XtNlabel, label,
                            NULL);

  fileEditorText =
    XtVaCreateManagedWidget("fileEditorText",
                            asciiTextWidgetClass,
                            fileEditorForm,
                            XtNfromVert, fileEditorLabel,
                            XtNeditType, (writeable ? XawtextEdit :
                                          XawtextRead),
                            XtNtype, XawAsciiFile,
                            XtNstring, EDITOR_FILE,
                            XtNscrollVertical, XawtextScrollWhenNeeded,
                            NULL);

  fileEditorDone =
    XtVaCreateManagedWidget("fileEditorDone",
                            commandWidgetClass,
                            fileEditorForm,
                            XtNfromVert, fileEditorText,
                            NULL);

  XtAddCallback(fileEditorDone, XtNcallback, fileEditorDoneCB, fileEditorShell);

  if (writeable) {
    XtSetSensitive(fileEditorDone, True);
  }
  else {
    XtSetSensitive(fileEditorDone, False);
  }

  fileEditorCancel =
    XtVaCreateManagedWidget("fileEditorCancel",
                            commandWidgetClass,
                            fileEditorForm,
                            XtNfromVert, fileEditorText,
                            XtNfromHoriz, fileEditorDone,
                            NULL);

  XtAddCallback(fileEditorCancel, XtNcallback, fileEditorCancelCB, fileEditorShell);

  fileEditorMark =
    XtVaCreateManagedWidget("fileEditorMark",
                            commandWidgetClass,
                            fileEditorForm,
                            XtNfromVert, fileEditorText,
                            XtNfromHoriz, fileEditorCancel,
                            NULL);

  XtAddCallback(fileEditorMark, XtNcallback, fileEditorMarkCB, fileEditorShell);

  return 0;
}

static int destroyFileEditorShell()
{
  if (fileEditorMark != NULL) {
    XtDestroyWidget(fileEditorMark);
    fileEditorMark = NULL;
  }

  if (fileEditorCancel != NULL) {
    XtDestroyWidget(fileEditorCancel);
    fileEditorCancel = NULL;
  }

  if (fileEditorDone != NULL) {
    XtDestroyWidget(fileEditorDone);
    fileEditorDone = NULL;
  }

  if (fileEditorText != NULL) {
    XtDestroyWidget(fileEditorText);
    fileEditorText = NULL;
  }

  if (fileEditorLabel != NULL) {
    XtDestroyWidget(fileEditorLabel);
    fileEditorLabel = NULL;
  }

  if (fileEditorForm != NULL) {
    XtDestroyWidget(fileEditorForm);
    fileEditorForm = NULL;
  }

  if (fileEditorShell != NULL) {
    XtDestroyWidget(fileEditorShell);
    fileEditorShell = NULL;
  }

  return 0;
}

static void doFileEditorDone(int get)
{
  Widget source;
  String string;

  if (get) {
    XtVaGetValues(fileEditorText,
                  XtNtextSource, &source,
                  XtNstring, &string,
                  NULL);

    XawAsciiSave(source);
  }

  XtPopdown(fileEditorShell);

  destroyFileEditorShell();
}

// tool table forward decls
static void toolTableDoneCB(Widget w, XtPointer client_data, XtPointer call_data);
static void toolTableCancelCB(Widget w, XtPointer client_data, XtPointer call_data);
static int createToolTableShell();
static int destroyToolTableShell();
static void doToolTableDone(int get);

static void toolTableDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doToolTableDone(1);
}

static void toolTableCancelCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doToolTableDone(0);
}

static int createToolTableShell()
{
  if (NULL != toolTableShell) {
    return 0;
  }

  toolTableShell =
    XtVaCreatePopupShell("toolTableShell",
                         topLevelShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  toolTableForm =
    XtVaCreateManagedWidget("toolTableForm",
                            formWidgetClass,
                            toolTableShell,
                            NULL);

  toolTableLabel =
    XtVaCreateManagedWidget("toolTableLabel",
                            labelWidgetClass,
                            toolTableForm,
                            XtNlabel, tool_table_file,
                            NULL);

  toolTableText =
    XtVaCreateManagedWidget("toolTableText",
                            asciiTextWidgetClass,
                            toolTableForm,
                            XtNfromVert, toolTableLabel,
                            XtNeditType, XawtextEdit,
                            XtNtype, XawAsciiFile,
                            XtNstring, tool_table_file,
                            XtNscrollVertical, XawtextScrollWhenNeeded,
                            NULL);

  toolTableDone =
    XtVaCreateManagedWidget("toolTableDone",
                            commandWidgetClass,
                            toolTableForm,
                            XtNfromVert, toolTableText,
                            NULL);

  XtAddCallback(toolTableDone, XtNcallback, toolTableDoneCB, toolTableShell);

  toolTableCancel =
    XtVaCreateManagedWidget("toolTableCancel",
                            commandWidgetClass,
                            toolTableForm,
                            XtNfromVert, toolTableText,
                            XtNfromHoriz, toolTableDone,
                            NULL);

  XtAddCallback(toolTableCancel, XtNcallback, toolTableCancelCB, toolTableShell);

  return 0;
}

static int destroyToolTableShell()
{
  if (toolTableCancel != NULL) {
    XtDestroyWidget(toolTableCancel);
    toolTableCancel = NULL;
  }

  if (toolTableDone != NULL) {
    XtDestroyWidget(toolTableDone);
    toolTableDone = NULL;
  }

  if (toolTableText != NULL) {
    XtDestroyWidget(toolTableText);
    toolTableText = NULL;
  }

  if (toolTableLabel != NULL) {
    XtDestroyWidget(toolTableLabel);
    toolTableLabel = NULL;
  }

  if (toolTableForm != NULL) {
    XtDestroyWidget(toolTableForm);
    toolTableForm = NULL;
  }

  if (toolTableShell != NULL) {
    XtDestroyWidget(toolTableShell);
    toolTableShell = NULL;
  }

  return 0;
}

static void doToolTableDone(int get)
{
  Widget source;
  String string;

  if (get) {
    XtVaGetValues(toolTableText,
                  XtNtextSource, &source,
                  XtNstring, &string,
                  NULL);

    XawAsciiSave(source);
    sendLoadToolTable(string);
  }

  XtPopdown(toolTableShell);

  destroyToolTableShell();
}

// forward decls

static void varFileDoneCB(Widget w, XtPointer client_data, XtPointer call_data);
static void varFileCancelCB(Widget w, XtPointer client_data, XtPointer call_data);
static int createVarFileShell();
static int destroyVarFileShell();
static void doVarFileDone(int get);

static void varFileDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doVarFileDone(1);
}

static void varFileCancelCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doVarFileDone(0);
}

static int createVarFileShell()
{
  if (NULL != varFileShell) {
    return 0;
  }

  varFileShell =
    XtVaCreatePopupShell("varFileShell",
                         topLevelShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  varFileForm =
    XtVaCreateManagedWidget("varFileForm",
                            formWidgetClass,
                            varFileShell,
                            NULL);

  varFileLabel =
    XtVaCreateManagedWidget("varFileLabel",
                            labelWidgetClass,
                            varFileForm,
                            XtNlabel, PARAMETER_FILE,
                            NULL);

  varFileText =
    XtVaCreateManagedWidget("varFileText",
                            asciiTextWidgetClass,
                            varFileForm,
                            XtNfromVert, varFileLabel,
                            XtNeditType, XawtextEdit,
                            XtNtype, XawAsciiFile,
                            XtNstring, PARAMETER_FILE,
                            XtNscrollVertical, XawtextScrollWhenNeeded,
                            NULL);

  varFileDone =
    XtVaCreateManagedWidget("varFileDone",
                            commandWidgetClass,
                            varFileForm,
                            XtNfromVert, varFileText,
                            NULL);

  XtAddCallback(varFileDone, XtNcallback, varFileDoneCB, varFileShell);

  varFileCancel =
    XtVaCreateManagedWidget("varFileCancel",
                            commandWidgetClass,
                            varFileForm,
                            XtNfromVert, varFileText,
                            XtNfromHoriz, varFileDone,
                            NULL);

  XtAddCallback(varFileCancel, XtNcallback, varFileCancelCB, varFileShell);

  return 0;
}

static int destroyVarFileShell()
{
  if (varFileCancel != NULL) {
    XtDestroyWidget(varFileCancel);
    varFileCancel = NULL;
  }

  if (varFileDone != NULL) {
    XtDestroyWidget(varFileDone);
    varFileDone = NULL;
  }

  if (varFileText != NULL) {
    XtDestroyWidget(varFileText);
    varFileText = NULL;
  }

  if (varFileLabel != NULL) {
    XtDestroyWidget(varFileLabel);
    varFileLabel = NULL;
  }

  if (varFileForm != NULL) {
    XtDestroyWidget(varFileForm);
    varFileForm = NULL;
  }

  if (varFileShell != NULL) {
    XtDestroyWidget(varFileShell);
    varFileShell = NULL;
  }

  return 0;
}

static void doVarFileDone(int get)
{
  Widget source;
  String string;

  if (get) {
  XtVaGetValues(varFileText,
                XtNtextSource, &source,
                XtNstring, &string,
                NULL);

  XawAsciiSave(source);
  sendTaskPlanInit();
  }

  XtPopdown(varFileShell);

  destroyVarFileShell();
}

// diagnostics dialog

static Widget diagnosticsShell = NULL;
static Widget diagnosticsForm = NULL;
static Widget diagnosticsLabel = NULL;
static Widget diagnosticsTaskHBLabel = NULL;
static Widget diagnosticsTaskHB = NULL;
static Widget diagnosticsIoHBLabel = NULL;
static Widget diagnosticsIoHB = NULL;
static Widget diagnosticsMotionHBLabel = NULL;
static Widget diagnosticsMotionHB= NULL;
static Widget diagnosticsFerrorLabel = NULL;
static Widget diagnosticsFerror = NULL;
static Widget diagnosticsDone = NULL;

// flag that window is up, so we don't waste time updating labels
static int diagnosticsIsPopped = 0;

static void diagnosticsDoDone()
{
  diagnosticsIsPopped = 0;
  XtPopdown(diagnosticsShell);
}

static void diagnosticsDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  diagnosticsDoDone();
}

static void diagnosticsReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  diagnosticsDoDone();
}

static Pixel fileOpenBorderColor;
static int fileOpenDoDone = 1;

static void doFileOpenDone()
{
  String string;

  if (fileOpenDoDone) {
    // note: "*value: " must be set in resources for input box
    // to appear, and for this to be valid (and not dump core)
    string = XawDialogGetValueString(fileOpenDialog);
    // set value to be the same in file edit dialog
    strcpy(EDITOR_FILE, string);
    XtVaSetValues(fileEditDialog, XtNvalue, EDITOR_FILE, NULL);
    sendProgramOpen(string);
  }

  setBorderColor(fileOpenDone, pixelRed);
  setBorderColor(fileOpenCancel, fileOpenBorderColor);
  fileOpenDoDone = 1;

  XtPopdown(fileOpenShell);
}

static void fileOpenDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doFileOpenDone();
}

static void fileOpenReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  doFileOpenDone();
}

static void fileOpenTabAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (fileOpenDoDone) {
    setBorderColor(fileOpenDone, fileOpenBorderColor);
    setBorderColor(fileOpenCancel, pixelRed);
    fileOpenDoDone = 0;
  }
  else {
    setBorderColor(fileOpenDone, pixelRed);
    setBorderColor(fileOpenCancel, fileOpenBorderColor);
    fileOpenDoDone = 1;
  }
}

static Pixel fileQuitBorderColor;
static int fileQuitDoDone = 1;

static void doFileQuitDone()
{
  XtPopdown(fileQuitShell);

  if (fileQuitDoDone) {
    quit();
  }
  else {
    setBorderColor(fileQuitDone, pixelRed);
    setBorderColor(fileQuitCancel, fileQuitBorderColor);
    fileQuitDoDone = 1;
  }
}

static void fileQuitDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doFileQuitDone();
}

static void fileQuitReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  doFileQuitDone();
}

static void fileQuitTabAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (fileQuitDoDone) {
    setBorderColor(fileQuitDone, fileQuitBorderColor);
    setBorderColor(fileQuitCancel, pixelRed);
    fileQuitDoDone = 0;
  }
  else {
    setBorderColor(fileQuitDone, pixelRed);
    setBorderColor(fileQuitCancel, fileQuitBorderColor);
    fileQuitDoDone = 1;
  }
}

static Pixel fileEditBorderColor;
static int fileEditDoDone = 1;

static void doFileEditDone()
{
  String string;
  struct stat buf;
  int fd;

  if (fileEditDoDone) {
    string = XawDialogGetValueString(fileEditDialog);
    // set value to be the same in file open dialog
    strcpy(EDITOR_FILE, string);
    XtVaSetValues(fileOpenDialog, XtNvalue, EDITOR_FILE, NULL);
  }

  XtPopdown(fileEditShell);

  if (fileEditDoDone) {
    if (-1 == stat(EDITOR_FILE, &buf)) {
      // file not found-- it's a new one. Touch it so that it
      // exists and the text widget won't barf
      fd = open(EDITOR_FILE, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
      close(fd);
      createFileEditorShell(1);
      dialogPopup(NULL, fileEditorShell, NULL);
      // trap window kill
      XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileEditorShell), &killAtom, 1);
    }
    else {
      // file is there-- check type and permissions
      if (! S_ISREG(buf.st_mode)) {
        // file is not a regular file
        popupError("Can't edit %s: not a text file", EDITOR_FILE);
      }
      else if ((S_IWUSR & buf.st_mode)) {
        // file is writeable
        createFileEditorShell(1);
        dialogPopup(NULL, fileEditorShell, NULL);
        // trap window kill
        XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileEditorShell), &killAtom, 1);
      }
      else {
        // file is read-only
        createFileEditorShell(0);
        dialogPopup(NULL, fileEditorShell, NULL);
        // trap window kill
        XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileEditorShell), &killAtom, 1);
      }
    }
  }

  setBorderColor(fileEditDone, pixelRed);
  setBorderColor(fileEditCancel, fileEditBorderColor);
  fileEditDoDone = 1;
}

static void fileEditDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  doFileEditDone();
}

static void fileEditReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  doFileEditDone();
}

static void fileEditTabAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (fileEditDoDone) {
    setBorderColor(fileEditDone, fileEditBorderColor);
    setBorderColor(fileEditCancel, pixelRed);
    fileEditDoDone = 0;
  }
  else {
    setBorderColor(fileEditDone, pixelRed);
    setBorderColor(fileEditCancel, fileEditBorderColor);
    fileEditDoDone = 1;
  }
}

static void toolSetOffsetDoDone(int done)
{
  String str1, str2, str3;
  int tool;
  double length;
  double diameter;

  if (done) {
    XtVaGetValues(toolSetOffsetTool, XtNstring, &str1, NULL);
    XtVaGetValues(toolSetOffsetLength, XtNstring, &str2, NULL);
    XtVaGetValues(toolSetOffsetDiameter, XtNstring, &str3, NULL);

    if (1 == sscanf(str1, "%d", &tool) &&
        1 == sscanf(str2, "%lf", &length) &&
        1 == sscanf(str3, "%lf", &diameter)) {
      sendToolSetOffset(tool, length, diameter);
    }
  }

  XtPopdown(toolSetOffsetShell);
}

static void toolSetOffsetUpAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  char string[256];

  sprintf(string, "%d", emcStatus->io.tool.toolInSpindle);
  XtVaSetValues(toolSetOffsetTool, XtNstring, string, NULL);
  sprintf(string, "%f", emcStatus->motion.traj.actualPosition.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z);
  XtVaSetValues(toolSetOffsetLength, XtNstring, string, NULL);
  XtVaSetValues(toolSetOffsetDiameter, XtNstring, "0.0", NULL);

  dialogPopup(NULL, toolSetOffsetShell, NULL);
  // trap window kill
  XSetWMProtocols(XtDisplay(topLevel), XtWindow(toolSetOffsetShell), &killAtom, 1);
}

static Pixel toolSetOffsetFormBorderColor;
static int toolSetOffsetReturnIsDone = 1;

static void toolSetOffsetReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (toolSetOffsetReturnIsDone) {
    toolSetOffsetDoDone(1);
  }
  else {
    toolSetOffsetDoDone(0);
  }
}

static void toolSetOffsetDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  toolSetOffsetDoDone(1);
}

static void toolSetOffsetCancelCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  toolSetOffsetDoDone(0);
}

static void toolSetOffsetTabAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (toolSetOffsetReturnIsDone) {
    setBorderColor(toolSetOffsetDone, toolSetOffsetFormBorderColor);
    setBorderColor(toolSetOffsetCancel, pixelRed);
    toolSetOffsetReturnIsDone = 0;
  }
  else {
    setBorderColor(toolSetOffsetDone, pixelRed);
    setBorderColor(toolSetOffsetCancel, toolSetOffsetFormBorderColor);
    toolSetOffsetReturnIsDone = 1;
  }
}

static void doPosOffsetDone()
{
  String string;
  char cmd[256];
  double val;

  string = XawDialogGetValueString(posOffsetDialog);

  if (1 == sscanf(string, "%lf", &val)) {
    // send MDI command
    // FIXME-- this RS-274-NGC specific; need to add NML way to do this
    sprintf(cmd, "G92%s%f\n",
            activeAxis == 0 ? "X" :
            activeAxis == 1 ? "Y" :
            activeAxis == 2 ? "Z" : "X",
            val);
    sendMdiCmd(cmd);
  }

  XtPopdown(posOffsetShell);
}

static void posOffsetUpAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (w == posLabel[0]) {
    activeAxis = 0;
  }
  else if (w == posLabel[1]) {
    activeAxis = 1;
  }
  else if (w == posLabel[2]) {
    activeAxis = 2;
  }

  XtVaSetValues(posOffsetDialog, XtNvalue, "0.0", NULL);
  dialogPopup(NULL, posOffsetShell, NULL);
  // trap window kill
  XSetWMProtocols(XtDisplay(topLevel), XtWindow(posOffsetShell), &killAtom, 1);
}

static void posOffsetReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  doPosOffsetDone();
}

static void doJogSpeedDone()
{
  String string;
  int val;
  char labelString[80];

  string = XawDialogGetValueString(jogSpeedDialog);

  if (1 == sscanf(string, "%d", &val)) {
    jogSpeed = val;
    if (jogSpeed > maxJogSpeed) {
      jogSpeed = maxJogSpeed;
    }
    sprintf(labelString, "%d", jogSpeed);
    setLabel(jogSpeedLabel, labelString);
    redraw = 1;
  }

  XtPopdown(jogSpeedShell);
}

static void jogSpeedReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  doJogSpeedDone();
}

static void doFeedOverrideDone()
{
  String string;
  double val;

  string = XawDialogGetValueString(feedOverrideDialog);

  if (1 == sscanf(string, "%lf", &val)) {
    sendFeedOverride(val / 100.0);
  }

  XtPopdown(feedOverrideShell);
}

static void feedOverrideReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  doFeedOverrideDone();
}

static void programRunCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  sendProgramRun(programStartLine);
}

static void programPauseCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  sendProgramPause();
}

static void programResumeCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  sendProgramResume();
}

static void programStepCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  sendProgramStep();
}

static void programVerifyCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  programStartLine = -1;
  sendProgramRun(programStartLine);
}

static void helpXemcReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  XtPopdown(helpXemcShell);
}

static void helpAboutReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  XtPopdown(helpAboutShell);
}

static void genericDoneCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  XtPopdown((Widget) client_data);
}

// button handling code

static void downAction(Widget w,
                       XEvent *ev,
                       String *params,
                       Cardinal *numParams)
{
  char string[80];

  if (w == spindleIncLabel) {
    sendSpindleIncrease();
  }
  else if (w == spindleDecLabel) {
    sendSpindleDecrease();
  }
  else if (w == jogMinusLabel) {
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, -jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, -jogSpeed);
    }
  }
  else if (w == jogPlusLabel) {
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, jogSpeed);
    }
  }
  else if (w == posLabel[0]) {
    activeAxis = 0;
  }
  else if (w == posLabel[1]) {
    activeAxis = 1;
  }
  else if (w == posLabel[2]) {
    activeAxis = 2;
  }
  else if (w == jogSpeedLabel) {
    // pop up menu to change jog speed
    XtVaSetValues(jogSpeedDialog,
                  XtNvalue, "",
                  NULL);
    dialogPopup(NULL, jogSpeedShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(jogSpeedShell), &killAtom, 1);
  }
  else if (w == jogSpeedDecLabel) {
    // change it once here and redraw
    if (--jogSpeed <= 0) {
      jogSpeed = 1;
    }
    sprintf(string, "%d", jogSpeed);
    setLabel(jogSpeedLabel, string);
    redraw = 1;

    // set jogSpeedChange so that timer callback will continue to change
    // until button up clears it
    jogSpeedChange = -1;
  }
  else if (w == jogSpeedIncLabel) {
    // change it once here and redraw
    if (++jogSpeed > maxJogSpeed) {
      jogSpeed = maxJogSpeed;
    }
    sprintf(string, "%d", jogSpeed);
    setLabel(jogSpeedLabel, string);
    redraw = 1;

    // set jogSpeedChange so that timer callback will continue to change
    // until button up clears it
    jogSpeedChange = +1;
  }
  else if (w == feedOverrideLabel) {
    // pop up menu to change feed override
    XtVaSetValues(feedOverrideDialog,
                  XtNvalue, "",
                  NULL);
    dialogPopup(NULL, feedOverrideShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(feedOverrideShell), &killAtom, 1);
  }
  else if (w == feedOverrideDecLabel) {
    // change it once here and redraw
    if ((feedOverride -= 10) <= 0) {
      feedOverride = 0;
    }
    sprintf(string, "%3d", feedOverride);
    setLabel(feedOverrideLabel, string);
    redraw = 1;

    // set feedOverrideChange so that timer callback will continue to change
    // until button up clears it
    feedOverrideChange = -1;
  }
  else if (w == feedOverrideIncLabel) {
    // change it once here and redraw
    if ((feedOverride += 10) > maxFeedOverride) {
      feedOverride = maxFeedOverride;
    }
    sprintf(string, "%3d", feedOverride);
    setLabel(feedOverrideLabel, string);
    redraw = 1;

    // set feedOverrideChange so that timer callback will continue to change
    // until button up clears it
    feedOverrideChange = +1;
  }
  else {
    printf("down\n");
  }
}

static void upAction(Widget w,
                     XEvent *ev,
                     String *params,
                     Cardinal *numParams)
{
  if (w == spindleIncLabel) {
    sendSpindleConstant();
  }
  else if (w == spindleDecLabel) {
    sendSpindleConstant();
  }
  else if (w == jogMinusLabel) {
    // only stop it if it's continuous jogging
    if (jogIncrement <= 0.0) {
      sendJogStop(axisJogging);
    }
  }
  else if (w == jogPlusLabel) {
    // only stop it if it's continuous jogging
    if (jogIncrement <= 0.0) {
      sendJogStop(axisJogging);
    }
  }
  else if (w == posLabel[0]) {
  }
  else if (w == posLabel[1]) {
  }
  else if (w == posLabel[2]) {
  }
  else if (w == jogSpeedDecLabel) {
    jogSpeedChange = 0;
  }
  else if (w == jogSpeedIncLabel) {
    jogSpeedChange = 0;
  }
  else if (w == feedOverrideDecLabel) {
    feedOverrideChange = 0;
    sendFeedOverride((double) feedOverride / 100.0);
  }
  else if (w == feedOverrideIncLabel) {
    feedOverrideChange = 0;
    sendFeedOverride((double) feedOverride / 100.0);
  }
  else {
    printf("up\n");
  }
}

static void mdiReturnAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  String string;

  XtVaGetValues(mdiFormText, XtNstring, &string, NULL);

  sendMdiCmd(string);
}

// key handling code

// FIXME-- better way to get these?
#define XEVENT_TYPE_XKEY_KEY_PRESS_EVENT 2
#define XEVENT_TYPE_XKEY_KEY_RELEASE_EVENT 3
#define XKEY_EVENT_STATE_NONE    0x00
#define XKEY_EVENT_STATE_SHIFT   0x01
#define XKEY_EVENT_STATE_CONTROL 0x04
#define XKEY_EVENT_STATE_ALT     0x08

#define KEY_ESC     9
#define KEY_F1     67
#define KEY_F2     68
#define KEY_F3     69
#define KEY_F4     70
#define KEY_F5     71
#define KEY_F6     72
#define KEY_F7     73
#define KEY_F8     74
#define KEY_F9     75
#define KEY_F10    76
#define KEY_F11    95
#define KEY_F12    96
#define KEY_PGUP   99
#define KEY_PGDN  105
#define KEY_LEFT  100
#define KEY_RIGHT 102
#define KEY_UP     98
#define KEY_DOWN  104
#define KEY_X      53
#define KEY_Y      29
#define KEY_Z      52
#define KEY_HOME   97
#define KEY_END   103
#define KEY_1      10
#define KEY_2      11
#define KEY_3      12
#define KEY_4      13
#define KEY_5      14
#define KEY_6      15
#define KEY_7      16
#define KEY_8      17
#define KEY_9      18
#define KEY_0      19
#define KEY_O      32
#define KEY_E      26
#define KEY_R      27
#define KEY_T      28
#define KEY_P      33
#define KEY_V      55
#define KEY_A      38
#define KEY_S      39
#define KEY_L      46
#define KEY_B      56
#define KEY_F      41
#define KEY_COMMA  59
#define KEY_PERIOD 60
#define KEY_I      31
#define KEY_C      54

static int keyReleaseOut = 0;   // set if we saw a release, cleared right away
                                // if the key repeat kicks back on, else it
                                // goes through as a true key release
static double keyReleaseTime = 0.0;
static unsigned int keyPressState;
static unsigned int keyPressKeycode;
static unsigned int keyReleaseState;
static unsigned int keyReleaseKeycode;

static void keyPressAction(unsigned int state, unsigned int keycode)
{
  char string[256];
  static double saveJogIncrement = 0.1000;

  switch (keycode) {

  case KEY_ESC:
    sendAbort();
    break;

  case KEY_F1:
    // estop toggle
    if (emcStatus->task.state == EMC_TASK_STATE_ESTOP) {
      sendEstopReset();
    }
    else {
      sendEstop();
    }
    break;

  case KEY_F2:
    // estop toggle
    if (emcStatus->task.state == EMC_TASK_STATE_ESTOP_RESET) {
      sendMachineOn();
    }
    else {
      sendMachineOff();
    }
    break;

  case KEY_F3:
    // manual mode
    sendManual();
    break;

  case KEY_F4:
    // auto mode
    sendAuto();
    break;

  case KEY_F5:
    // mdi mode
    sendMdi();
    break;

  case KEY_F6:
    sendTaskPlanInit();
    break;

  case KEY_F7:
    // mist toggle
    if (emcStatus->io.coolant.mist) {
      sendMistOff();
    }
    else {
      sendMistOn();
    }
    break;

  case KEY_F8:
    // flood toggle
    if (emcStatus->io.coolant.flood) {
      sendFloodOff();
    }
    else {
      sendFloodOn();
    }
    break;

  case KEY_F9:
    // spindle fwd/off
    if (emcStatus->motion.spindle.direction == 0) {
      // it's off, so turn forward
      sendSpindleForward();
    }
    else
      {
        // it's not off, so turn off
        sendSpindleOff();
      }
    break;

  case KEY_F10:
    // spindle rev/off
    if (emcStatus->motion.spindle.direction == 0) {
      // it's off, so turn reverse
      sendSpindleReverse();
    }
    else
      {
        // it's not off, so turn off
        sendSpindleOff();
      }
    break;

  case KEY_F11:
    // spindle decrease
    if (emcStatus->motion.spindle.direction != 0) {
      sendSpindleDecrease();
    }
    break;

  case KEY_F12:
    // spindle increase
    if (emcStatus->motion.spindle.direction != 0) {
      sendSpindleIncrease();
    }
    break;

  case KEY_PGUP:
    activeAxis = 2;
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, jogSpeed);
    }
    break;

  case KEY_PGDN:
    activeAxis = 2;
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, -jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, -jogSpeed);
    }
    break;

  case KEY_RIGHT:
    activeAxis = 0;
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, jogSpeed);
    }
    break;

  case KEY_LEFT:
    activeAxis = 0;
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, -jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, -jogSpeed);
    }
    break;

  case KEY_UP:
    activeAxis = 1;
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, jogSpeed);
    }
    break;

  case KEY_DOWN:
    activeAxis = 1;
    if (jogIncrement > 0.0) {
      sendJogIncr(activeAxis, -jogSpeed, jogIncrement);
    }
    else {
      sendJogCont(activeAxis, -jogSpeed);
    }
    break;

  case KEY_T:
    switch (state) {
    case XKEY_EVENT_STATE_ALT:
      sprintf(string, "%d", emcStatus->io.tool.toolInSpindle);
      XtVaSetValues(toolSetOffsetTool, XtNstring, string, NULL);
      sprintf(string, "%f", emcStatus->motion.traj.actualPosition.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z);
      XtVaSetValues(toolSetOffsetLength, XtNstring, string, NULL);
      XtVaSetValues(toolSetOffsetDiameter, XtNstring, "0.0", NULL);
      dialogPopup(NULL, toolSetOffsetShell, NULL);
      // trap window kill
      XSetWMProtocols(XtDisplay(topLevel), XtWindow(toolSetOffsetShell), &killAtom, 1);
      break;
    }
    break;

  case KEY_X:
    activeAxis = 0;
    switch (state) {
    case XKEY_EVENT_STATE_ALT:
      XtVaSetValues(posOffsetDialog, XtNvalue, "0.0", NULL);
      dialogPopup(NULL, posOffsetShell, NULL);
      // trap window kill
      XSetWMProtocols(XtDisplay(topLevel), XtWindow(posOffsetShell), &killAtom, 1);
      break;
    }
    break;

  case KEY_Y:
    activeAxis = 1;
    switch (state) {
    case XKEY_EVENT_STATE_ALT:
      XtVaSetValues(posOffsetDialog, XtNvalue, "0.0", NULL);
      dialogPopup(NULL, posOffsetShell, NULL);
      // trap window kill
      XSetWMProtocols(XtDisplay(topLevel), XtWindow(posOffsetShell), &killAtom, 1);
      break;
    }
    break;

  case KEY_Z:
    activeAxis = 2;
    switch (state) {
    case XKEY_EVENT_STATE_ALT:
      XtVaSetValues(posOffsetDialog, XtNvalue, "0.0", NULL);
      dialogPopup(NULL, posOffsetShell, NULL);
      // trap window kill
      XSetWMProtocols(XtDisplay(topLevel), XtWindow(posOffsetShell), &killAtom, 1);
      break;
    }
    break;

  case KEY_HOME:
    sendHome(activeAxis);
    break;

  case KEY_END:
    dialogPopup(NULL, fileQuitShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileQuitShell), &killAtom, 1);
    break;

  case KEY_1:
      sendFeedOverride(0.1);
    break;

  case KEY_2:
    switch (state) {
    case XKEY_EVENT_STATE_NONE:
      sendFeedOverride(0.2);
      break;

    case XKEY_EVENT_STATE_SHIFT:
      // @
      if (posDisplay == POS_DISPLAY_CMD) {
        posDisplay = POS_DISPLAY_ACT;
      }
      else {
        posDisplay = POS_DISPLAY_CMD;
      }
      break;

    }
    break;

  case KEY_3:
    switch (state) {
    case XKEY_EVENT_STATE_NONE:
      sendFeedOverride(0.3);
      break;

    case XKEY_EVENT_STATE_SHIFT:
      if (coords == COORD_RELATIVE) {
        coords = COORD_MACHINE;
      }
      else {
        coords = COORD_RELATIVE;
      }
      break;

    }
    break;

  case KEY_4:
    sendFeedOverride(0.4);
    break;

  case KEY_5:
    sendFeedOverride(0.5);
    break;

  case KEY_6:
    sendFeedOverride(0.6);
    break;

  case KEY_7:
    sendFeedOverride(0.7);
    break;

  case KEY_8:
    sendFeedOverride(0.8);
    break;

  case KEY_9:
    sendFeedOverride(0.9);
    break;

  case KEY_0:
    sendFeedOverride(1.0);
    break;

  case KEY_O:
    dialogPopup(NULL, fileOpenShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileOpenShell), &killAtom, 1);
    break;

  case KEY_E:
    dialogPopup(NULL, fileEditShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileEditShell), &killAtom, 1);
    break;

  case KEY_R:
    sendProgramRun(programStartLine);
    break;

  case KEY_P:
    switch (state) {
    case XKEY_EVENT_STATE_NONE:
    case XKEY_EVENT_STATE_SHIFT:
      sendProgramPause();
      break;
    }
    break;

  case KEY_S:
    switch (state) {
    case XKEY_EVENT_STATE_NONE:
    case XKEY_EVENT_STATE_SHIFT:
      sendProgramResume();
      break;
    }
    break;

  case KEY_V:
    programStartLine = -1;
    sendProgramRun(programStartLine);
    break;
  case KEY_A:
    sendProgramStep();
    break;

  case KEY_L:
    sendOverrideLimits();
    break;

  case KEY_B:
    switch (state) {
    case XKEY_EVENT_STATE_NONE:
      sendBrakeRelease();
      break;

    case XKEY_EVENT_STATE_SHIFT:
      sendBrakeEngage();
      break;

    }
    break;

  case KEY_I:
    // check for entering from continuous jog, and restore saved increment
    if (jogIncrement <= 0.0) {
      jogIncrement = saveJogIncrement;
    }
    // go from 0.1000 to 0.0100
    else if (jogIncrement > 0.0500) {
      jogIncrement = 0.0100;
    }
    // go from 0.0100 to 0.0010
    else if (jogIncrement > 0.0050) {
      jogIncrement = 0.0010;
    }
    // go from 0.0010 to stepIncrement
    else if (jogIncrement > stepIncrement) {
      jogIncrement = stepIncrement;
    }
    // go from 0.0001 to 0.1000
    else {
      jogIncrement = 0.1000;
    }
    saveJogIncrement = jogIncrement;
    break;

  case KEY_C:
    // change to continuous jog, but save increment for restoring
    jogIncrement = 0.0;
    break;

  case KEY_COMMA:
    // change it once here and redraw
    if (--jogSpeed <= 0) {
      jogSpeed = 1;
    }
    sprintf(string, "%d", jogSpeed);
    setLabel(jogSpeedLabel, string);
    redraw = 1;

    // set jogSpeedChange so that timer callback will continue to change
    // until button up clears it
    jogSpeedChange = -1;
    break;

  case KEY_PERIOD:
    // change it once here and redraw
    if (++jogSpeed > maxJogSpeed) {
      jogSpeed = maxJogSpeed;
    }
    sprintf(string, "%d", jogSpeed);
    setLabel(jogSpeedLabel, string);
    redraw = 1;

    // set jogSpeedChange so that timer callback will continue to change
    // until button up clears it
    jogSpeedChange = +1;
    break;

  default:
    printf("key press %d %d\n", state, keycode);
  }
}

static void keyReleaseAction(unsigned int state, unsigned int keycode)
{
  switch (keycode) {

  case KEY_LEFT:
  case KEY_RIGHT:
    if (jogIncrement <= 0.0) {
      // only stop it if it's continuous jogging
      sendJogStop(axisJogging);
    }
    break;

  case KEY_UP:
  case KEY_DOWN:
    if (jogIncrement <= 0.0) {
      // only stop it if it's continuous jogging
      sendJogStop(axisJogging);
    }
    break;

  case KEY_PGUP:
  case KEY_PGDN:
    if (jogIncrement <= 0.0) {
      // only stop it if it's continuous jogging
      sendJogStop(axisJogging);
    }
    break;

  case KEY_F11:
  case KEY_F12:
    sendSpindleConstant();
    break;

  case KEY_COMMA:
  case KEY_PERIOD:
    jogSpeedChange = 0;
    break;

  default:
    printf("key release %d %d\n", state, keycode);
    break;
  }
}

static void keyAction(Widget w,
                      XEvent *ev,
                      String *params,
                      Cardinal *numParams)
{
  if (ev->type == XEVENT_TYPE_XKEY_KEY_PRESS_EVENT) {
    keyPressKeycode = ev->xkey.keycode;
    keyPressState = ev->xkey.state;

    if (keyReleaseOut) {
      if (keyPressKeycode == keyReleaseKeycode &&
          keyPressState == keyReleaseState) {
        // the press matched the release before the timeout, so it's
        // a key repeat on this key and we won't signal a key release
        keyReleaseOut = 0;
      }
      else {
        // it's a key press on a different key than the previous release,
        // so allow the release to go through by leaving keyReleaseOut set
      }
    }
    else {
      keyPressAction(ev->xkey.state, ev->xkey.keycode);
    }
  }
  else if (ev->type == XEVENT_TYPE_XKEY_KEY_RELEASE_EVENT) {
    keyReleaseKeycode = ev->xkey.keycode;
    keyReleaseState = ev->xkey.state;
    keyReleaseTime = etime();
    keyReleaseOut = 1;
  }

  return;
}

static const char * fileMenuEntryNames[] = {
  "[O]    Open...",
  "[E]     Edit...",
  "[F6]   Reset",
  "-",
  "[End] Quit",
  NULL
};

static const char * viewMenuEntryNames[] = {
  "Tools...",
  "Offsets and Variables...",
  "Diagnostics...",
  NULL
};

static const char * settingsMenuEntryNames[] = {
  "[#] Relative Coordinates",
  "[#] Machine Coordinates",
  "-",
  "[@] Actual Position",
  "[@] Commanded Position",
  NULL
};

static const char * helpMenuEntryNames[] = {
  "Help...",
  "-",
  "About...",
  NULL
};

static const char * stateMenuEntryNames[] = {
  "[F1] Estop on",
  "[F1] Estop off",
  "-",
  "[F2] Machine on",
  "[F2] Machine off",
  NULL
};

static const char * modeMenuEntryNames[] = {
  "[F3] Manual",
  "[F4] Auto",
  "[F5] MDI",
  NULL
};

static const char * mistMenuEntryNames[] = {
  "[F7] Mist on",
  "[F7] Mist off",
  NULL
};

static const char * floodMenuEntryNames[] = {
  "[F8] Flood on",
  "[F8] Flood off",
  NULL
};

static const char * spindleMenuEntryNames[] = {
  "[F9]        Spindle forward",
  "[F10]      Spindle reverse",
  "[F9/F10] Spindle off",
  NULL
};

static const char * brakeMenuEntryNames[] = {
  "[B] Brake on",
  "[b] Brake off",
  NULL
};

static char stepIncrementLabel[32] = "0.123456";
static const char * jogIncrementMenuEntryNames[] = {
  "0.1000",
  "0.0100",
  "0.0010",
  stepIncrementLabel,           // this will be overridden by step size
  "-",
  "continuous",
  NULL
};

static void fileMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:                       // Open...
    dialogPopup(NULL, fileOpenShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileOpenShell), &killAtom, 1);
    break;

  case 1:                       // Edit...
    dialogPopup(NULL, fileEditShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileEditShell), &killAtom, 1);
    break;

  case 2:                       // Reset
    sendTaskPlanInit();
    break;

    // case 3 is separator

  case 4:                       // Quit
    dialogPopup(NULL, fileQuitShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(fileQuitShell), &killAtom, 1);
    break;

  default:
    break;
  }
}

static void viewMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  char string[256];

  switch ((long) client_data) {
  case 0:                       // Tools
    createToolTableShell();
    dialogPopup(NULL, toolTableShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(toolTableShell), &killAtom, 1);
    break;

  case 1:                       // Variables
    createVarFileShell();
    dialogPopup(NULL, varFileShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(varFileShell), &killAtom, 1);
    break;

  case 2:                       // Diagnostics
    sprintf(string, "Axis %d Diagnostics", activeAxis);
    setLabel(diagnosticsLabel, string);
    dialogPopup(NULL, diagnosticsShell, NULL);
    diagnosticsIsPopped = 1;
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(diagnosticsShell), &killAtom, 1);
    break;

  default:
    break;
  }
}

// menu select functions

static void settingsMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:                       // Relative
    coords = COORD_RELATIVE;
    break;

  case 1:                       // Machine
    coords = COORD_MACHINE;
    break;

    // case 2 is separator

  case 3:                       // Actual
    posDisplay = POS_DISPLAY_ACT;
    break;

  case 4:                       // Commanded
    posDisplay = POS_DISPLAY_CMD;
    break;

  default:
    break;
  }
}

static void helpMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:                       // Help...
    dialogPopup(w, helpXemcShell, NULL);
    // trap window kill
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(helpXemcShell), &killAtom, 1);
    break;

    // case 1 is separator

  case 2:                       // About...
    dialogPopup(NULL, helpAboutShell, NULL);
    XSetWMProtocols(XtDisplay(topLevel), XtWindow(helpAboutShell), &killAtom, 1);
    break;

  default:
    break;
  }
}

static void stateMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:
    sendEstop();
    break;

  case 1:
    sendEstopReset();
    break;

    // case 2 is separator

  case 3:
    sendMachineOn();
    break;

  case 4:
    sendMachineOff();
    break;

  default:
    break;
  }
}

static void modeMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:
    sendManual();
    break;

  case 1:
    sendAuto();
    break;

  case 2:
    sendMdi();
    break;

  default:
    break;
  }
}

static void mistMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:
    sendMistOn();
    break;

  case 1:
    sendMistOff();
    break;

  default:
    break;
  }
}

static void floodMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:
    sendFloodOn();
    break;

  case 1:
    sendFloodOff();
    break;

  default:
    break;
  }
}

static void spindleMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:
    sendSpindleForward();
    break;

  case 1:
    sendSpindleReverse();
    break;

  case 2:
    sendSpindleOff();
    break;

  default:
    break;
  }
}

static void brakeMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:
    sendBrakeEngage();
    break;

  case 1:
    sendBrakeRelease();
    break;

  default:
    break;
  }
}

static void jogIncrementMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
  switch ((long) client_data) {
  case 0:
    jogIncrement = 0.1000;
    break;

  case 1:
    jogIncrement = 0.0100;
    break;

  case 2:
    jogIncrement = 0.0010;
    break;

  case 3:
    jogIncrement = stepIncrement;
    break;

    // case 4 is separator

  case 5:
    jogIncrement = -1.0;
    break;

  default:
    break;
  }
}

// menu dimming

static void enableAuxMenus(Boolean torf)
{
  static Boolean auxMenusEnabled = True;

  if (auxMenusEnabled != torf) {
    XtSetSensitive(mistMenu, torf);
    XtSetSensitive(floodMenu, torf);
    XtSetSensitive(spindleMenu, torf);
    XtSetSensitive(brakeMenu, torf);
    auxMenusEnabled = torf;
  }
}

static void enableManualMenus(Boolean torf)
{
  static Boolean manualMenusEnabled = True;

  if (manualMenusEnabled != torf) {
    XtSetSensitive(jogMinusLabel, torf);
    XtSetSensitive(homeCommand, torf);
    XtSetSensitive(jogPlusLabel, torf);
    XtSetSensitive(programPauseCommand, torf);
    XtSetSensitive(programResumeCommand, torf);
    manualMenusEnabled = torf;
  }
}

static void enableAutoMenus(Boolean torf)
{
  static Boolean autoMenusEnabled = True;

  if (autoMenusEnabled != torf) {
    XtSetSensitive(programOpenCommand, torf);
    XtSetSensitive(programRunCommand, torf);
    XtSetSensitive(programPauseCommand, torf);
    XtSetSensitive(programResumeCommand, torf);
    XtSetSensitive(programStepCommand, torf);
    XtSetSensitive(programVerifyCommand, torf);
    autoMenusEnabled = torf;
  }
}

static void enableMdiMenus(Boolean torf)
{
  static Boolean mdiMenusEnabled = True;

  if (mdiMenusEnabled != torf) {
    //FIXME    XtSetSensitive(mdiFormText, torf);
    XtSetSensitive(programPauseCommand, torf);
    XtSetSensitive(programResumeCommand, torf);
    mdiMenusEnabled = torf;
  }

  if (torf) {
    /* set focus to mdi window */
    XtSetKeyboardFocus(topForm, mdiFormText);
  }
  else {
    XtSetKeyboardFocus(topForm, None);
  }
}

// saved old values, used to determine if redraw is necessary.
// Make them invalid so that everything is updated at startup, or
// at least make them the same as the initial labels.
static unsigned long int oldTaskHeartbeat = 0;
static unsigned long int oldIoHeartbeat = 0;
static unsigned long int oldMotionHeartbeat = 0;
static double oldAxisFerror[XEMC_NUM_AXES]; // inited to invalid in main()
static int oldOverrideLimits = -1;
static int oldState = -1;
static int oldMode = -1;
static int oldMist = -1;
static int oldFlood = -1;
static int oldSpindleIncreasing = -1;
static int oldSpindleDirection = -2;
static int oldBrake = -1;
static int oldTool = -1;
static double oldToolOffset = DBL_MAX; // invalid val forces first print
static double oldXOffset = DBL_MAX; // ditto
static double oldYOffset = DBL_MAX;
static double oldZOffset = DBL_MAX;
static double oldX = DBL_MAX;
static double oldY = DBL_MAX;
static double oldZ = DBL_MAX;
static int oldCoords = 0; // invalid val forces first print
static int oldPosDisplay = 0; // ditto
static double oldJogIncrement = -1.0; // invalid val forces first print
static Pixel posColor[XEMC_NUM_AXES]; // inited to invalid in main()
static char oldMdiCodes[256] = ""; // FIXME-- harcoded
static int oldProgramActiveLine = -1;
static char oldProgramString[256] = "";

void timeoutCB(XtPointer clientdata, XtIntervalId *id)
{
  double now;
  char string[256];             // FIXME-- hardcoded
  int t;
  int changedPositionType;
  double newX, newY, newZ;
  int code;
  int programActiveLine;
  int programTopLine;
  char mdiCodes[256];           // FIXME-- hardcoded

  // set the time
  now = etime();

  // read the EMC status
  if (0 != updateStatus()) {
    sprintf(error_string, "bad status");
  }

  // read the EMC errors
  if (0 != updateError()) {
    sprintf(error_string, "bad status");
  }

  // print any result stored by updateError() in error_string
  if (error_string[0] != 0) {
    popupError("%s", error_string);
    error_string[0] = 0;
  }

  // handle key up events
  if (keyReleaseOut &&
      now - keyReleaseTime > 0.100) {
    // key up event has occurred
    keyReleaseOut = 0;
    keyReleaseAction(keyReleaseState, keyReleaseKeycode);
  }

  // handle button press repeats

  if (jogSpeedChange < 0) {
    if (--jogSpeed <= 0) {
      jogSpeed = 1;
    }
    sprintf(string, "%d", jogSpeed);
    setLabel(jogSpeedLabel, string);
    redraw = 1;
  }
  else if (jogSpeedChange > 0) {
    if (++jogSpeed > maxJogSpeed) {
      jogSpeed = maxJogSpeed;
    }
    sprintf(string, "%d", jogSpeed);
    setLabel(jogSpeedLabel, string);
    redraw = 1;
  }

  if (feedOverrideChange < 0) {
    if (--feedOverrideDelayCount < 0) {
      if ((feedOverride -= 10) < 0) {
        feedOverride = 0;
      }
      sprintf(string, "%3d", feedOverride);
      setLabel(feedOverrideLabel, string);
      feedOverrideDelayCount = FEED_OVERRIDE_DELAY_COUNT;

      redraw = 1;
    }
  }
  else if (feedOverrideChange > 0) {
    if (--feedOverrideDelayCount < 0) {
      if ((feedOverride += 10) > maxFeedOverride) {
        feedOverride = maxFeedOverride;
      }
      sprintf(string, "%3d", feedOverride);
      setLabel(feedOverrideLabel, string);
      feedOverrideDelayCount = FEED_OVERRIDE_DELAY_COUNT;
      redraw = 1;
    }
  }
  else {
    // we're not changing it, so update it from EMC value
    feedOverride = (int) (emcStatus->motion.traj.scale * 100.0 + 0.5);
    if (feedOverride != oldFeedOverride) {
      oldFeedOverride = feedOverride;
      sprintf(string, "%3d", feedOverride);
      setLabel(feedOverrideLabel, string);
      redraw = 1;
    }
  }

  // handle internal status changes

  if (activeAxis != oldActiveAxis) {
    if (activeAxis == 0) {
      setBorderColor(posLabel[0], posLabelBorderColor[0]);
      setBorderColor(posLabel[1], posLabelBackground[1]);
      setBorderColor(posLabel[2], posLabelBackground[2]);
    }
    else if (activeAxis == 1) {
      setBorderColor(posLabel[0], posLabelBackground[0]);
      setBorderColor(posLabel[1], posLabelBorderColor[1]);
      setBorderColor(posLabel[2], posLabelBackground[2]);
    }
    else if (activeAxis == 2) {
      setBorderColor(posLabel[0], posLabelBackground[0]);
      setBorderColor(posLabel[1], posLabelBackground[1]);
      setBorderColor(posLabel[2], posLabelBorderColor[2]);
    }
    redraw = 1;

    oldActiveAxis = activeAxis;
  }

  if (jogIncrement != oldJogIncrement) {
    if (jogIncrement > stepIncrement) {
      sprintf(string, "%1.4f", jogIncrement);
    }
    else if (jogIncrement > 0.0) {
      sprintf(string, "%1.6f", jogIncrement);
    }
    else {
      sprintf(string, "continuous");
    }
    setLabel(jogIncrementMenu, string);
    redraw = 1;

    oldJogIncrement = jogIncrement;
  }

  // handle EMC status changes
  /*
    Note that changing the label changes the size of the label,
    but not the size of the enclosing box. So, the initial label
    width will set the size of the enclosing box, but won't have
    any other effect during the life of the label. For this reason
    label widths for dynamically labeled labels are not specified,
    and they are set manually before the labels are realized to a
    blank string the same width of the labels. If you use proportional
    fonts for the labels, they will change in size.
  */

  if (diagnosticsIsPopped) {
    if (emcStatus->task.heartbeat != oldTaskHeartbeat) {
      sprintf(string, "%ld %ld %d %d %d",
              emcStatus->task.heartbeat,
              emcStatus->task.command_type,
              emcStatus->task.echo_serial_number,
              emcStatus->task.status,
              emcStatus->task.execState);
      setLabel(diagnosticsTaskHB, string);
      redraw = 1;

      oldTaskHeartbeat = emcStatus->task.heartbeat;
    }

    if (emcStatus->io.heartbeat != oldIoHeartbeat) {
      sprintf(string, "%ld %ld %d %d",
              emcStatus->io.heartbeat,
              emcStatus->io.command_type,
              emcStatus->io.echo_serial_number,
              emcStatus->io.status);
      setLabel(diagnosticsIoHB, string);
      redraw = 1;

      oldIoHeartbeat = emcStatus->io.heartbeat;
    }

    if (emcStatus->motion.heartbeat != oldMotionHeartbeat) {
      sprintf(string, "%ld %ld %d %d",
              emcStatus->motion.heartbeat,
              emcStatus->motion.command_type,
              emcStatus->motion.echo_serial_number,
              emcStatus->motion.status);
      setLabel(diagnosticsMotionHB, string);
      redraw = 1;

      oldMotionHeartbeat = emcStatus->motion.heartbeat;
    }

    // FIXME: We are only displaying the HighMark, it would be nice to
    // see the current ferror also.
    if (emcStatus->motion.axis[activeAxis].ferrorHighMark !=
        oldAxisFerror[activeAxis]) {
      sprintf(string, "%.3f", emcStatus->motion.axis[activeAxis].ferrorHighMark);
      setLabel(diagnosticsFerror, string);
      redraw = 1;

      oldAxisFerror[activeAxis] = emcStatus->motion.axis[activeAxis].ferrorHighMark;
    }
  }

  if (emcStatus->motion.axis[0].overrideLimits &&
      1 != oldOverrideLimits) {
    setColor(limCommand, pixelRed, 0);
    oldOverrideLimits = 1;
  }
  else if (! emcStatus->motion.axis[0].overrideLimits &&
           0 != oldOverrideLimits) {
    setColor(limCommand, pixelWhite, 0);
    oldOverrideLimits = 0;
  }

  if (emcStatus->task.state != oldState) {
    switch(emcStatus->task.state) {
    case EMC_TASK_STATE_OFF:
      sprintf(string, "OFF");
      break;
    case EMC_TASK_STATE_ON:
      sprintf(string, "ON");
      break;
    case EMC_TASK_STATE_ESTOP:
      sprintf(string, "ESTOP");
      break;
    case EMC_TASK_STATE_ESTOP_RESET:
      sprintf(string, "ESTOP RESET");
      break;
    default:
      sprintf(string, "?");
      break;
    }

    setLabel(stateMenu, string);
    redraw = 1;

    oldState = emcStatus->task.state;
  }

  if (emcStatus->task.mode != oldMode) {
    switch(emcStatus->task.mode) {
    case EMC_TASK_MODE_MANUAL:
      sprintf(string, "MANUAL");
      break;
    case EMC_TASK_MODE_AUTO:
      sprintf(string, "AUTO");
      break;
    case EMC_TASK_MODE_MDI:
      sprintf(string, "MDI");
      break;
    default:
      sprintf(string, "?");
      break;
    }

    setLabel(modeMenu, string);
    redraw = 1;

    oldMode = emcStatus->task.mode;
  }

  if (emcStatus->io.coolant.mist != oldMist) {
    if (emcStatus->io.coolant.mist) {
      sprintf(string, "MIST ON");
    }
    else {
      sprintf(string, "MIST OFF");
    }

    setLabel(mistMenu, string);
    redraw = 1;

    oldMist = emcStatus->io.coolant.mist;
  }

  if (emcStatus->io.coolant.flood != oldFlood) {
    if (emcStatus->io.coolant.flood) {
      sprintf(string, "FLOOD ON");
    }
    else {
      sprintf(string, "FLOOD OFF");
    }

    setLabel(floodMenu, string);
    redraw = 1;

    oldFlood = emcStatus->io.coolant.flood;
  }

  if (emcStatus->motion.spindle.increasing != oldSpindleIncreasing) {
    if (emcStatus->motion.spindle.increasing > 0) {
      sprintf(string, "SPINDLE INC");
    }
    else if (emcStatus->motion.spindle.increasing < 0) {
      sprintf(string, "SPINDLE DEC");
    }
    else {
      // spindle constant-- flag invalid oldSpindleDirection
      // so code below will pick up a redraw
      oldSpindleDirection = -2;
    }

    setLabel(spindleMenu, string);
    redraw = 1;

    oldSpindleIncreasing = emcStatus->motion.spindle.increasing;
  }

  if (emcStatus->motion.spindle.direction != oldSpindleDirection) {
    if (emcStatus->motion.spindle.direction > 0) {
      sprintf(string, "SPINDLE FWD");
      XtSetSensitive(spindleDecLabel, True);
      XtSetSensitive(spindleIncLabel, True);
    }
    else if (emcStatus->motion.spindle.direction < 0) {
      sprintf(string, "SPINDLE REV");
      XtSetSensitive(spindleDecLabel, True);
      XtSetSensitive(spindleIncLabel, True);
    }
    else {
      sprintf(string, "SPINDLE OFF");
      XtSetSensitive(spindleDecLabel, False);
      XtSetSensitive(spindleIncLabel, False);
    }

    setLabel(spindleMenu, string);
    redraw = 1;

    oldSpindleDirection = emcStatus->motion.spindle.direction;
  }

  if (emcStatus->motion.spindle.brake != oldBrake) {
    if (emcStatus->motion.spindle.brake) {
      sprintf(string, "BRAKE ON");
    }
    else {
      sprintf(string, "BRAKE OFF");
    }

    setLabel(brakeMenu, string);
    redraw = 1;

    oldBrake = emcStatus->motion.spindle.brake;
  }

  if (emcStatus->io.tool.toolInSpindle != oldTool) {
    sprintf(string, "%d", emcStatus->io.tool.toolInSpindle);
    setLabel(toolNumberFormName, string);

    redraw = 1;

    oldTool = emcStatus->io.tool.toolInSpindle;
  }

  if (emcStatus->task.toolOffset.tran.z != oldToolOffset) {
    sprintf(string, "%.4f", emcStatus->task.toolOffset.tran.z);
    setLabel(toolOffsetFormName, string);

    redraw = 1;

    oldToolOffset = emcStatus->task.toolOffset.tran.z;
  }

  changedPositionType = 0;

  if (posDisplay != oldPosDisplay) {
    changedPositionType = 1;
    oldPosDisplay = posDisplay;
  }

  if (coords != oldCoords) {
    changedPositionType = 1;
    oldCoords = coords;
  }

  // check for
  if (changedPositionType) {
    sprintf(string, "%s %s",
            (posDisplay == POS_DISPLAY_CMD) ? "Commanded" : "Actual",
            (coords == COORD_MACHINE) ? "Machine" : "Relative");
    setLabel(positionTypeFormName, string);
    redraw = 1;
  }

  if (changedPositionType) {
    if (coords == COORD_MACHINE) {
    // switched to machine display, so blank out offsets
      string[0] = 0;
      setLabel(workOffsetFormName, string);
      redraw = 1;
    }
    else {
      // switched to relative display, so redisplay offsets
      sprintf(string, "X%.4f  Y%.4f  Z%.4f",
              emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x,
              emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y,
              emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z);
      setLabel(workOffsetFormName, string);
      redraw = 1;
    }
  }
  else if (emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x != oldXOffset ||
           emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y != oldYOffset ||
           emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z != oldZOffset) {
    if (coords == COORD_MACHINE) {
      // display offsets changed, but we're not displaying them
    }
    else {
      // display offsets changed and we are displaying them
      sprintf(string, "X%.4f  Y%.4f  Z%.4f",
              emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x,
              emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y,
              emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z);
      setLabel(workOffsetFormName, string);
      redraw = 1;

      oldXOffset = emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x;
      oldYOffset = emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y;
      oldZOffset = emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z;
    }
  }

  // compute new XYZ numbers
  if (coords == COORD_MACHINE) {
    if (posDisplay == POS_DISPLAY_ACT) {
      newX = emcStatus->motion.traj.actualPosition.tran.x;
      newY = emcStatus->motion.traj.actualPosition.tran.y;
      newZ = emcStatus->motion.traj.actualPosition.tran.z;
    }
    else {
      // POS_DISPLAY_CMD
      newX = emcStatus->motion.traj.position.tran.x;
      newY = emcStatus->motion.traj.position.tran.y;
      newZ = emcStatus->motion.traj.position.tran.z;
    }
  }
  else {
    // COORD_RELATIVE
    if (posDisplay == POS_DISPLAY_ACT) {
      newX = emcStatus->motion.traj.actualPosition.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.x;
      newY = emcStatus->motion.traj.actualPosition.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.g92_offset.tran.y - emcStatus->task.toolOffset.tran.y;
      newZ = emcStatus->motion.traj.actualPosition.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z - emcStatus->task.toolOffset.tran.z;
    }
    else {
      // POS_DISPLAY_CMD
      newX = emcStatus->motion.traj.position.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.x;
      newY = emcStatus->motion.traj.position.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.g92_offset.tran.y - emcStatus->task.toolOffset.tran.y;
      newZ = emcStatus->motion.traj.position.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z - emcStatus->task.toolOffset.tran.z;
    }
  }

  if (newX != oldX) {
    sprintf(string, "X %9.4f", newX);
    setLabel(posLabel[0], string);
    redraw = 1;

    oldX = newX;
  }

  if (newY != oldY) {
    sprintf(string, "Y %9.4f", newY);
    setLabel(posLabel[1], string);
    redraw = 1;

    oldY = newY;
  }

  if (newZ != oldZ) {
    sprintf(string, "Z %9.4f", newZ);
    setLabel(posLabel[2], string);
    redraw = 1;

    oldZ = newZ;
  }

  // fill in the active G codes
  active_g_codes_string[0] = 0;
  for (t = 1; t < ACTIVE_G_CODES; t++) {
    code = emcStatus->task.activeGCodes[t];
    if (code == -1) {
      continue;
    }
    if (code % 10) {
      sprintf(string, "G%.1f ", (double) code / 10.0);
    }
    else {
      sprintf(string, "G%d ", code / 10);
    }
    strcat(active_g_codes_string, string);
  }

  // fill in the active M codes, settings too
  active_m_codes_string[0] = 0;
  for (t = 1; t < ACTIVE_M_CODES; t++) {
    code = emcStatus->task.activeMCodes[t];
    if (code == -1) {
      continue;
    }
    sprintf(string, "M%d ", code);
    strcat(active_m_codes_string, string);
  }

  sprintf(mdiCodes, "%s %s", active_g_codes_string, active_m_codes_string);

  // fill in F and S codes also
  sprintf(string, " F%.0f", emcStatus->task.activeSettings[1]);
  strcat(mdiCodes, string);
  sprintf(string, " S%.0f", emcStatus->task.activeSettings[2]);
  strcat(mdiCodes, string);

  if (0 != strcmp(oldMdiCodes, mdiCodes)) {
    XtVaSetValues(mdiCodesLabel, XtNlabel, mdiCodes, NULL);
    strcpy(oldMdiCodes, mdiCodes);
  }

  // program text updating

  if (0 != emcStatus->task.file[0] &&
      0 != strcmp(programFile, emcStatus->task.file)) {
    // new program has been opened-- open it here

    fwClose(&programFw);
    oldProgramActiveLine = -1;

    if (0 == fwOpen(&programFw, emcStatus->task.file)) {
      // opened successfully locally
      // save it to run again
      strcpy(lastProgramFile, emcStatus->task.file);
      // set string for label
      strcpy(string, emcStatus->task.file);
    }
    else {
      // couldn't open it locally
      // save it to run again, even though we can't see it
      strcpy(lastProgramFile, emcStatus->task.file);
      // set string for label
      sprintf(string, "(%s not found)", emcStatus->task.file);
    }

    // set the program label
    setLabel(programFormName, string);
    redraw = 1;

    strcpy(programFile, emcStatus->task.file);
  }

  // compose program state string
  switch (emcStatus->task.interpState) {
  case EMC_TASK_INTERP_IDLE:
    strcpy(string, "Idle");
    break;

  case EMC_TASK_INTERP_READING:
  case EMC_TASK_INTERP_WAITING:
    strcpy(string, "Running");
    break;

  case EMC_TASK_INTERP_PAUSED:
    strcpy(string, "PAUSED");
    break;

  default:
    strcpy(string, "(Idle)");   // shouldn't get here; parens for debug
    break;
  }
  // and print it, if new
  if (strcmp(string, oldProgramString)) {
    setLabel(programFormState, string);
    redraw = 1;

    strcpy(oldProgramString, string);
  }

  // print active line and next few in window

  if (programStartLineLast < 0 ||
      emcStatus->task.readLine < programStartLineLast) {
    // controller is skipping lines
    programActiveLine = emcStatus->task.readLine;
  }
  else {                        // controller is not skipping lines
    if (emcStatus->task.currentLine > 0) {
      if (emcStatus->task.motionLine > 0 &&
          emcStatus->task.motionLine < emcStatus->task.currentLine) {
        // active line is the motion line, which lags
        programActiveLine = emcStatus->task.motionLine;
      }
      else {
        // active line is the current line-- no motion lag
        programActiveLine = emcStatus->task.currentLine;
      }
    }
    else {
      // no active line at all
      programActiveLine = 0;
    }
  } // end of else controller is not skipping lines

  if (programActiveLine != oldProgramActiveLine) {
    programTopLine = programActiveLine - 2;
    if (programTopLine < 1) {
      programTopLine = 1;
    }
    fwSyncLine(&programFw, programTopLine);
    fwString(&programFw, programFwString);
    setProgramText(programFwString);
    highlightProgramText(programFwString, programActiveLine - programTopLine);
    redraw = 1;

    oldProgramActiveLine = programActiveLine;
  }

  // set label colors: do red for limit first
  for (t = 0; t < XEMC_NUM_AXES; t++) {
    if (emcStatus->motion.axis[t].minHardLimit ||
        emcStatus->motion.axis[t].minSoftLimit ||
        emcStatus->motion.axis[t].maxSoftLimit ||
        emcStatus->motion.axis[t].maxHardLimit) {
      if (posColor[t] != pixelRed) {
        setColor(posLabel[t], pixelRed, 1);
        posColor[t] = pixelRed;
      }
    }
    else if (emcStatus->motion.axis[t].homed &&
             posColor[t] != pixelGreen) {
      setColor(posLabel[t], pixelGreen, 1);
      posColor[t] = pixelGreen;
    }
    else if (! emcStatus->motion.axis[t].homed &&
             posColor[t] != pixelYellow) {
      setColor(posLabel[t], pixelYellow, 1);
      posColor[t] = pixelYellow;
    }
  }

  // do menu dimming

  if (emcStatus->task.state != EMC_TASK_STATE_ON) {
    // if not on, dim all menus
    enableAuxMenus(False);
    enableManualMenus(False);
    enableAutoMenus(False);
    enableMdiMenus(False);
  }
  else {
    // it's on, so only enable according to mode
    if (emcStatus->task.mode == EMC_TASK_MODE_MANUAL) {
      enableAuxMenus(True);
      enableAutoMenus(False);
      enableMdiMenus(False);
      enableManualMenus(True);
    }
    else if (emcStatus->task.mode == EMC_TASK_MODE_AUTO) {
      if (emcStatus->task.interpState == EMC_TASK_INTERP_IDLE ||
          emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED) {
        enableAuxMenus(True);
      }
      else {
        enableAuxMenus(False);
      }
      enableMdiMenus(False);
      enableManualMenus(False);
      enableAutoMenus(True);
    }
    else if (emcStatus->task.mode == EMC_TASK_MODE_MDI) {
      enableAuxMenus(True);
      enableManualMenus(False);
      enableAutoMenus(False);
      enableMdiMenus(True);
    }
  }

  // flush changes now if necessary, don't wait for event
  if (redraw) {
    XFlush(XtDisplay(topForm));
    redraw = 0;
  }

  // re-register this proc for the timeout
  XtAppAddTimeOut(app_context, UPDATE_MSECS, timeoutCB, NULL);

  return;
}

static void topLevelProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  quit();
}

static void fileOpenShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(fileOpenShell);
}

static void fileQuitShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(fileQuitShell);
}

static void fileEditShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(fileEditShell);
}

static void fileEditorShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(fileEditorShell);
  destroyFileEditorShell();
}

static void toolTableShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(toolTableShell);
  destroyToolTableShell();
}

static void toolSetOffsetShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(toolSetOffsetShell);
}

static void posOffsetShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(posOffsetShell);
}

static void jogSpeedShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(jogSpeedShell);
}

static void feedOverrideShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(feedOverrideShell);
}

static void varFileShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(varFileShell);
  destroyVarFileShell();
}

static void diagnosticsShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  diagnosticsIsPopped = 0;
  XtPopdown(diagnosticsShell);
}

static void helpXemcProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(helpXemcShell);
}

static void helpAboutProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(helpAboutShell);
}

static void errorShellProtocols(Widget w, XEvent *event, String *string, Cardinal *c)
{
  XtPopdown(errorShell);
  errorIsPopped = 0;
  destroyErrorShell();
}

static XtActionsRec actionsTable[] =
{
  {(char*)"downAction", downAction},
  {(char*)"upAction", upAction},
  {(char*)"keyAction", keyAction},
  {(char*)"fileOpenReturnAction", fileOpenReturnAction},
  {(char*)"fileOpenTabAction", fileOpenTabAction},
  {(char*)"fileEditReturnAction", fileEditReturnAction},
  {(char*)"fileEditTabAction", fileEditTabAction},
  {(char*)"fileQuitReturnAction", fileQuitReturnAction},
  {(char*)"fileQuitTabAction", fileQuitTabAction},
  {(char*)"diagnosticsReturnAction", diagnosticsReturnAction},
  {(char*)"toolSetOffsetUpAction", toolSetOffsetUpAction},
  {(char*)"toolSetOffsetReturnAction", toolSetOffsetReturnAction},
  {(char*)"toolSetOffsetTabAction", toolSetOffsetTabAction},
  {(char*)"posOffsetUpAction", posOffsetUpAction},
  {(char*)"posOffsetReturnAction", posOffsetReturnAction},
  {(char*)"jogSpeedReturnAction", jogSpeedReturnAction},
  {(char*)"feedOverrideReturnAction", feedOverrideReturnAction},
  {(char*)"mdiReturnAction", mdiReturnAction},
  {(char*)"helpXemcReturnAction", helpXemcReturnAction},
  {(char*)"helpAboutReturnAction", helpAboutReturnAction},
  {(char*)"errorReturnAction", errorReturnAction},
  {(char*)"topLevelProtocols", topLevelProtocols},
  {(char*)"fileOpenShellProtocols", fileOpenShellProtocols},
  {(char*)"fileQuitShellProtocols", fileQuitShellProtocols},
  {(char*)"fileEditShellProtocols", fileEditShellProtocols},
  {(char*)"fileEditorShellProtocols", fileEditorShellProtocols},
  {(char*)"toolTableShellProtocols", toolTableShellProtocols},
  {(char*)"toolSetOffsetShellProtocols", toolSetOffsetShellProtocols},
  {(char*)"posOffsetShellProtocols", posOffsetShellProtocols},
  {(char*)"jogSpeedShellProtocols", jogSpeedShellProtocols},
  {(char*)"feedOverrideShellProtocols", feedOverrideShellProtocols},
  {(char*)"varFileShellProtocols", varFileShellProtocols},
  {(char*)"diagnosticsShellProtocols", diagnosticsShellProtocols},
  {(char*)"helpXemcProtocols", helpXemcProtocols},
  {(char*)"helpAboutProtocols", helpAboutProtocols},
  {(char*)"errorShellProtocols", errorShellProtocols}
};

typedef void (MenuSelectFunc)(Widget w, XtPointer client_data, XtPointer call_data);

static int setupMenu(Widget *menu, const char *label,
                     const char **labels,
                     Widget which,
                     Widget horiz, Widget vert,
                     MenuSelectFunc *func)
{
  Widget *popup = new Widget;   // this will be freed automatically at exit
  intptr_t t;

  if (NULL != horiz)
    {
      if (NULL != vert)
        {
          *menu =
            XtVaCreateManagedWidget((char*)label,
                                    menuButtonWidgetClass,
                                    which,
                                    XtNfromHoriz, horiz,
                                    XtNfromVert, vert,
                                    NULL);
        }
      else
        {
          *menu =
            XtVaCreateManagedWidget((char*)label,
                                    menuButtonWidgetClass,
                                    which,
                                    XtNfromHoriz, horiz,
                                    NULL);
        }
    }
  else
    {
      if (NULL != vert)
        {
          *menu =
            XtVaCreateManagedWidget((char*)label,
                                    menuButtonWidgetClass,
                                    which,
                                    XtNfromVert, vert,
                                    NULL);
        }
      else
        {
          *menu =
            XtVaCreateManagedWidget((char*)label,
                                    menuButtonWidgetClass,
                                    which,
                                    NULL);
        }
    }

  *popup =
    XtVaCreatePopupShell("menu", // need to hard-code "menu" here
                         simpleMenuWidgetClass,
                         *menu,
                         NULL);

  // set the menu item names
  for (t = 0; labels[t] != NULL; t++)
    {
      if (labels[t][0] == '-' &&
          labels[t][1] == 0)
        {
          // it's a dash
          XtAddCallback(XtVaCreateManagedWidget((char*)labels[t],
                                                smeLineObjectClass,
                                                *popup,
                                                NULL),
                        XtNcallback, func, (XtPointer) t);

        }
      else
        {
          XtAddCallback(XtVaCreateManagedWidget((char*)labels[t],
                                                smeBSBObjectClass,
                                                *popup,
                                                NULL),
                        XtNcallback, func, (XtPointer) t);
        }
    }
  return(0);
}

static String fallbackResources[] =
{
  NULL
};

static void quit()
{
  // clean up NML buffers

  if (emcErrorBuffer != 0)
    {
      delete emcErrorBuffer;
      emcErrorBuffer = 0;
    }

  if (emcStatusBuffer != 0)
    {
      delete emcStatusBuffer;
      emcStatusBuffer = 0;
      emcStatus = 0;
    }

  if (emcCommandBuffer != 0)
    {
      delete emcCommandBuffer;
      emcCommandBuffer = 0;
    }

  // get rid of program file window string
  if (NULL != programFwString) {
    free(programFwString);
    programFwString = 0;
  }

  exit(0);
}

// destructively converts string to its uppercase counterpart
static char *upcase(char *string)
{
  char *ptr = string;

  while (*ptr != 0)
    {
      *ptr = toupper(*ptr);
      ptr++;
    }

  return string;
}

static int iniLoad(const char *filename)
{
  IniFile inifile;
  const char *inistring;
  char machine[LINELEN] = "";
  char version[LINELEN] = "";
  char displayString[LINELEN] = "";
  int t;
  int i;
  double d;

  // open it
  if (!inifile.Open(filename)) {
    return -1;
  }

  if (NULL != (inistring = inifile.Find("MACHINE", "EMC"))) {
    strcpy(machine, inistring);

    if (NULL != (inistring = inifile.Find("VERSION", "EMC"))) {
      sscanf(inistring, "$Revision: %s", version);

      sprintf(version_string, "%s EMC Version %s", machine, version);
    }
  }

  if (NULL != (inistring = inifile.Find("DEBUG", "EMC"))) {
    // copy to global
    if (1 != sscanf(inistring, "%i", &emc_debug)) {
      emc_debug = 0;
    }
  }
  else {
    // not found, use default
    emc_debug = 0;
  }

  if (NULL != (inistring = inifile.Find("NML_FILE", "EMC"))) {
    // copy to global
    strcpy(emc_nmlfile, inistring);
  }
  else {
    // not found, use default
  }

  if (NULL != (inistring = inifile.Find("TOOL_TABLE", "EMCIO"))) {
    strcpy(tool_table_file, inistring);
  }
  else {
    strcpy(tool_table_file, "tool.tbl"); // FIXME-- hardcoded
  }

  if (NULL != (inistring = inifile.Find("PARAMETER_FILE", "RS274NGC"))) {
    strcpy(PARAMETER_FILE, inistring);
  }
  else {
    strcpy(PARAMETER_FILE, "rs274ngc.var"); // FIXME-- hardcoded
  }

  if (NULL != (inistring = inifile.Find("DEFAULT_VELOCITY", "TRAJ"))) {
    if (1 != sscanf(inistring, "%lf", &traj_default_velocity)) {
      traj_default_velocity = DEFAULT_TRAJ_DEFAULT_VELOCITY;
    }
  }
  else {
    traj_default_velocity = DEFAULT_TRAJ_DEFAULT_VELOCITY;
  }
  // round jogSpeed in display to integer, per-minute
  jogSpeed = (int) (traj_default_velocity * 60.0 + 0.5);

  if (NULL != (inistring = inifile.Find("MAX_VELOCITY", "TRAJ"))) {
    if (1 != sscanf(inistring, "%lf", &traj_max_velocity)) {
      traj_max_velocity = DEFAULT_TRAJ_MAX_VELOCITY;
    }
  }
  else {
    traj_max_velocity = DEFAULT_TRAJ_MAX_VELOCITY;
  }
  // round maxJogSpeed in display to integer, per-minute
  maxJogSpeed = (int) (traj_max_velocity * 60.0 + 0.5);

  if (NULL != (inistring = inifile.Find("HELP_FILE", "DISPLAY"))) {
    strcpy(HELP_FILE, inistring);
  }

  if (NULL != (inistring = inifile.Find("PROGRAM_PREFIX", "DISPLAY"))) {
    if (1 != sscanf(inistring, "%s", programPrefix)) {
      programPrefix[0] = 0;
    }
  }
  else if (NULL != (inistring = inifile.Find("PROGRAM_PREFIX", "TASK"))) {
    if (1 != sscanf(inistring, "%s", programPrefix)) {
      programPrefix[0] = 0;
    }
  }
  else {
    programPrefix[0] = 0;
  }

  if (NULL != (inistring = inifile.Find("POSITION_OFFSET", "DISPLAY"))) {
    if (1 == sscanf(inistring, "%s", displayString)) {
      if (! strcmp(upcase(displayString), "MACHINE")) {
        coords = COORD_MACHINE;
      }
      else if (1 == sscanf(inistring, "%s", displayString)) {
        if (! strcmp(upcase(displayString), "RELATIVE")) {
          coords = COORD_RELATIVE;
        }
      }
      else {
        // error-- invalid value
        // ignore
      }
    }
    else {
      // error-- no value provided
      // ignore
    }
  }
  else {
    // no line at all
    // ignore
  }

  if (NULL != (inistring = inifile.Find("POSITION_FEEDBACK", "DISPLAY"))) {
    if (1 == sscanf(inistring, "%s", displayString)) {
      if (! strcmp(upcase(displayString), "ACTUAL")) {
        posDisplay = POS_DISPLAY_ACT;
      }
      else if (1 == sscanf(inistring, "%s", displayString)) {
        if (! strcmp(upcase(displayString), "COMMANDED")) {
          posDisplay = POS_DISPLAY_CMD;
        }
      }
      else {
        // error-- invalid value
        // ignore
      }
    }
    else {
      // error-- no value provided
      // ignore
    }
  }
  else {
    // no line at all
    // ignore
  }

  for (t = 0; t < XEMC_NUM_AXES; t++) {
    jogPol[t] = 1;              // set to default
    sprintf(displayString, "AXIS_%d", t);
    if (NULL != (inistring = inifile.Find("JOGGING_POLARITY", displayString)) &&
        1 == sscanf(inistring, "%d", &i) &&
        i == 0) {
      // it read as 0, so override default
      jogPol[t] = 0;
    }
  }

  if (NULL != (inistring = inifile.Find("MAX_FEED_OVERRIDE", "DISPLAY"))) {
    if (1 == sscanf(inistring, "%lf", &d) &&
        d > 0.0) {
      maxFeedOverride = (int) (d * 100.0 + 0.5);
    }
    else {
      // error-- no value provided
      // ignore
    }
  }
  else {
    // no line at all
    // ignore
  }

  // FIXME-- we're using the first axis scale to set the jog increment.
  // Note that stepIncrement is inited to a reasonable value above, and
  // will only be reset on a good ini file match
  if (NULL != (inistring = inifile.Find("INPUT_SCALE", "AXIS_0"))) {
    if (1 == sscanf(inistring, "%lf", &d)) {
      if (d < 0.0) {
        stepIncrement = -1.0/d; // posify it
      }
      else if (d > 0.0) {
        stepIncrement = 1.0/d;
      }
      // else it's 0, so ignore (this will kill the EMC, by the way)
    }
  }
  // set step increment to be less than 0.0010, the last fixed increment,
  // if it's larger. Set to 0.0001 if so, which will be too small but it
  // can't hurt.
  if (stepIncrement >= 0.0010) {
    stepIncrement = 0.0001;
  }
  sprintf(stepIncrementLabel, "%.6f", stepIncrement);

  // close it
  inifile.Close();

  return 0;
}

int main(int argc, char **argv)
{
  int t;
  double start;
  int good;
  char string[80];
  Dimension cmfbw, sdw, sw, siw, sh, bh, bw;
  Dimension stw, mw;
  Dimension posw;
  Font posfont;

  // process command line args
  if (0 != emcGetArgs(argc, argv)) {
    rcs_print_error("error in argument list\n");
    exit(1);
  }

  // read INI file
  iniLoad(emc_inifile);

  // init NML

#define RETRY_TIME 10.0         // seconds to wait for subsystems to come up
#define RETRY_INTERVAL 1.0      // seconds between wait tries for a subsystem

    if (! (emc_debug & EMC_DEBUG_NML)) {
      set_rcs_print_destination(RCS_PRINT_TO_NULL);     // inhibit diag messages
    }
  start = etime();
  good = 0;
  do {
    if (0 == emcTaskNmlGet()) {
      good = 1;
      break;
    }
    esleep(RETRY_INTERVAL);
  } while (etime() - start < RETRY_TIME);
  if (! (emc_debug & EMC_DEBUG_NML)) {
    set_rcs_print_destination(RCS_PRINT_TO_STDOUT); // restore diag messages
  }
  if (! good) {
    rcs_print_error("can't establish communication with emc\n");
    exit(1);
  }

    if (! (emc_debug & EMC_DEBUG_NML)) {
      set_rcs_print_destination(RCS_PRINT_TO_NULL);     // inhibit diag messages
    }
  start = etime();
  good = 0;
  do {
    if (0 == emcErrorNmlGet()) {
      good = 1;
      break;
    }
    esleep(RETRY_INTERVAL);
  } while (etime() - start < RETRY_TIME);
    if (! (emc_debug & EMC_DEBUG_NML)) {
      set_rcs_print_destination(RCS_PRINT_TO_STDOUT); // restore diag messages
    }
    if (! good) {
    rcs_print_error("can't establish communication with emc\n");
    exit(1);
  }

  // create file window for program text

  programFwString =
    (char *) malloc(PROGRAM_FW_NUM_LINES * PROGRAM_FW_LEN_LINES);

  if (0 != fwInit(&programFw, PROGRAM_FW_NUM_LINES, PROGRAM_FW_LEN_LINES)) {
    fprintf(stderr, "can't init file window\n");
    exit(1);
  }

  // get widgets

  topLevel =
    XtVaAppInitialize(&app_context, /* Application context */
                      "XEmc",   /* Application class */
                      NULL, 0,  /* command line option list */
                      &argc, argv, /* command line args */
                      fallbackResources, /* app defaults string, or NULL */
                      NULL);    /* terminate varargs list */

  topForm =
    XtVaCreateManagedWidget("topForm", /* arbitrary widget name */
                            formWidgetClass, /* widget class from Label.h */
                            topLevel, /* parent widget */
                            NULL); /* terminate varargs list */

  barMenuForm =
    XtVaCreateManagedWidget("barMenuForm",
                            formWidgetClass,
                            topForm,
                            NULL);

  setupMenu(&fileMenu, "fileMenu",
            fileMenuEntryNames,
            barMenuForm,
            NULL, NULL,
            fileMenuSelect);

  setupMenu(&viewMenu, "viewMenu",
            viewMenuEntryNames,
            barMenuForm,
            fileMenu, NULL,
            viewMenuSelect);

  setupMenu(&settingsMenu, "settingsMenu",
            settingsMenuEntryNames,
            barMenuForm,
            viewMenu, NULL,
            settingsMenuSelect);

  setupMenu(&helpMenu, "helpMenu",
            helpMenuEntryNames,
            barMenuForm,
            settingsMenu, NULL,
            helpMenuSelect);

  limCommand =
    XtVaCreateManagedWidget("limCommand",
                            commandWidgetClass,
                            barMenuForm,
                            XtNfromHoriz, helpMenu,
                            NULL);

  XtAddCallback(limCommand, XtNcallback, limCB, NULL);

  commandMenuForm =
    XtVaCreateManagedWidget("commandMenuForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, barMenuForm,
                            NULL);

  setupMenu(&stateMenu, "stateMenu",
            stateMenuEntryNames,
            commandMenuForm,
            NULL, NULL,
            stateMenuSelect);

  XtVaGetValues(stateMenu,
                XtNwidth, &stw,
                NULL);

  setupMenu(&modeMenu, "modeMenu",
            modeMenuEntryNames,
            commandMenuForm,
            NULL, stateMenu,
            modeMenuSelect);

  XtVaSetValues(modeMenu,
                XtNwidth, stw,
                NULL);

  setupMenu(&mistMenu, "mistMenu",
            mistMenuEntryNames,
            commandMenuForm,
            stateMenu, NULL,
            mistMenuSelect);

  XtVaGetValues(mistMenu,
                XtNwidth, &mw,
                NULL);

  setupMenu(&floodMenu, "floodMenu",
            floodMenuEntryNames,
            commandMenuForm,
            stateMenu, mistMenu,
            floodMenuSelect);

  XtVaSetValues(floodMenu,
                XtNwidth, mw,
                NULL);

  spindleDecLabel =
    XtVaCreateManagedWidget("spindleDecLabel",
                            labelWidgetClass,
                            commandMenuForm,
                            XtNfromHoriz, mistMenu,
                            NULL);

  setupMenu(&spindleMenu, "spindleMenu",
            spindleMenuEntryNames,
            commandMenuForm,
            spindleDecLabel, NULL,
            spindleMenuSelect);

  spindleIncLabel =
    XtVaCreateManagedWidget("spindleIncLabel",
                            labelWidgetClass,
                            commandMenuForm,
                            XtNfromHoriz, spindleMenu,
                            NULL);

  XtVaGetValues(spindleMenu,
                XtNwidth, &sw,
                XtNheight, &sh,
                XtNborderWidth, &cmfbw,
                NULL);

  XtVaGetValues(spindleDecLabel,
                XtNwidth, &sdw,
                NULL);

  XtVaGetValues(spindleIncLabel,
                XtNwidth, &siw,
                NULL);

  setupMenu(&brakeMenu, "brakeMenu",
            brakeMenuEntryNames,
            commandMenuForm,
            mistMenu, spindleDecLabel,
            brakeMenuSelect);

  bw = sdw + cmfbw + cmfbw + sw + cmfbw + cmfbw + siw;
  XtVaSetValues(brakeMenu,
                XtNwidth, bw,
                NULL);

  XtVaGetValues(brakeMenu,
                XtNheight, &bh,
                NULL);

  fileOpenShell =
    XtVaCreatePopupShell("fileOpenShell",
                         transientShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  fileOpenDialog =
    XtVaCreateManagedWidget("fileOpenDialog",
                            dialogWidgetClass,
                            fileOpenShell,
                            XtNvalue, programPrefix,
                            NULL);

  fileOpenDone =
    XtVaCreateManagedWidget("fileOpenDone",
                            commandWidgetClass,
                            fileOpenDialog,
                            NULL);

  XtAddCallback(fileOpenDone, XtNcallback, fileOpenDoneCB, fileOpenShell);

  fileOpenCancel =
    XtVaCreateManagedWidget("fileOpenCancel",
                            commandWidgetClass,
                            fileOpenDialog,
                            NULL);

  XtAddCallback(fileOpenCancel, XtNcallback, genericDoneCB, fileOpenShell);

  fileEditShell =
    XtVaCreatePopupShell("fileEditShell",
                         transientShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  fileEditDialog =
    XtVaCreateManagedWidget("fileEditDialog",
                            dialogWidgetClass,
                            fileEditShell,
                            XtNvalue, programPrefix,
                            NULL);

  fileEditDone =
    XtVaCreateManagedWidget("fileEditDone",
                            commandWidgetClass,
                            fileEditDialog,
                            NULL);

  XtAddCallback(fileEditDone, XtNcallback, fileEditDoneCB, fileEditShell);

  fileEditCancel =
    XtVaCreateManagedWidget("fileEditCancel",
                            commandWidgetClass,
                            fileEditDialog,
                            NULL);

  XtAddCallback(fileEditCancel, XtNcallback, genericDoneCB, fileEditShell);

  fileQuitShell =
    XtVaCreatePopupShell("fileQuitShell",
                         transientShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  fileQuitDialog =
    XtVaCreateManagedWidget("fileQuitDialog",
                            dialogWidgetClass,
                            fileQuitShell,
                            NULL);

  fileQuitDone =
    XtVaCreateManagedWidget("fileQuitDone",
                            commandWidgetClass,
                            fileQuitDialog,
                            NULL);

  XtAddCallback(fileQuitDone, XtNcallback, fileQuitDoneCB, fileQuitShell);

  fileQuitCancel =
    XtVaCreateManagedWidget("fileQuitCancel",
                            commandWidgetClass,
                            fileQuitDialog,
                            NULL);

  XtAddCallback(fileQuitCancel, XtNcallback, genericDoneCB, fileQuitShell);

  // diagnostics shell

  diagnosticsShell =
    XtVaCreatePopupShell("diagnosticsShell",
                         topLevelShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  diagnosticsForm =
    XtVaCreateManagedWidget("diagnosticsForm",
                            formWidgetClass,
                            diagnosticsShell,
                            NULL);

  diagnosticsLabel =
    XtVaCreateManagedWidget("diagnosticsLabel",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNborderWidth, 0,
                            NULL);

  diagnosticsTaskHBLabel =
    XtVaCreateManagedWidget("diagnosticsTaskHBLabel",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsLabel,
                            XtNborderWidth, 0,
                            NULL);

  diagnosticsTaskHB =
    XtVaCreateManagedWidget("diagnosticsTaskHB",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsLabel,
                            XtNfromHoriz, diagnosticsTaskHBLabel,
                            NULL);

  diagnosticsIoHBLabel =
    XtVaCreateManagedWidget("diagnosticsIoHBLabel",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsTaskHBLabel,
                            XtNborderWidth, 0,
                            NULL);

  diagnosticsIoHB =
    XtVaCreateManagedWidget("diagnosticsIoHB",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsTaskHBLabel,
                            XtNfromHoriz, diagnosticsIoHBLabel,
                            NULL);

  diagnosticsMotionHBLabel =
    XtVaCreateManagedWidget("diagnosticsMotionHBLabel",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsIoHBLabel,
                            XtNborderWidth, 0,
                            NULL);

  diagnosticsMotionHB =
    XtVaCreateManagedWidget("diagnosticsMotionHB",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsIoHBLabel,
                            XtNfromHoriz, diagnosticsMotionHBLabel,
                            NULL);

  diagnosticsFerrorLabel =
    XtVaCreateManagedWidget("diagnosticsFerrorLabel",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsMotionHBLabel,
                            XtNborderWidth, 0,
                            NULL);

  diagnosticsFerror =
    XtVaCreateManagedWidget("diagnosticsFerror",
                            labelWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsMotionHBLabel,
                            XtNfromHoriz, diagnosticsFerrorLabel,
                            NULL);

  diagnosticsDone =
    XtVaCreateManagedWidget("diagnosticsDone",
                            commandWidgetClass,
                            diagnosticsForm,
                            XtNfromVert, diagnosticsFerrorLabel,
                            NULL);

  XtAddCallback(diagnosticsDone, XtNcallback, diagnosticsDoneCB, NULL);

  helpXemcShell =
    XtVaCreatePopupShell("helpXemcShell",
                         topLevelShellWidgetClass,
                         topLevel,
                         XtNallowShellResize, True,
                         NULL);

  helpXemcForm =
    XtVaCreateManagedWidget("helpXemcForm",
                            formWidgetClass,
                            helpXemcShell,
                            NULL);

  helpXemcLabel =
    XtVaCreateManagedWidget("helpXemcLabel",
                            labelWidgetClass,
                            helpXemcForm,
                            NULL);

  helpXemcText =
    XtVaCreateManagedWidget("helpXemcText",
                            asciiTextWidgetClass,
                            helpXemcForm,
                            XtNfromVert, helpXemcLabel,
                            XtNeditType, XawtextRead,
                            XtNtype, XawAsciiFile,
                            XtNstring, HELP_FILE,
                            XtNscrollVertical, XawtextScrollWhenNeeded,
                            XtNscrollHorizontal, XawtextScrollWhenNeeded,
                            NULL);

  helpXemcDone =
    XtVaCreateManagedWidget("helpXemcDone",
                            commandWidgetClass,
                            helpXemcForm,
                            XtNfromVert, helpXemcText,
                            NULL);

  XtAddCallback(helpXemcDone, XtNcallback, genericDoneCB, helpXemcShell);

  helpAboutShell =
    XtVaCreatePopupShell("helpAboutShell",
                         transientShellWidgetClass,
                         topLevel,
                         NULL);

  helpAboutDialog =
    XtVaCreateManagedWidget("helpAboutDialog",
                            dialogWidgetClass,
                            helpAboutShell,
                            NULL);

  helpAboutDone =
    XtVaCreateManagedWidget("helpAboutDone",
                            commandWidgetClass,
                            helpAboutDialog,
                            NULL);

  XtAddCallback(helpAboutDone, XtNcallback, genericDoneCB, helpAboutShell);

  toolNumberForm =
    XtVaCreateManagedWidget("toolNumberForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, commandMenuForm,
                            NULL);

  toolNumberFormTitle =
    XtVaCreateManagedWidget("toolNumberFormTitle",
                            labelWidgetClass,
                            toolNumberForm,
                            NULL);

  toolNumberFormName =
    XtVaCreateManagedWidget("toolNumberFormName",
                            labelWidgetClass,
                            toolNumberForm,
                            XtNfromHoriz, toolNumberFormTitle,
                            NULL);

  toolOffsetForm =
    XtVaCreateManagedWidget("toolOffsetForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, toolNumberForm,
                            NULL);

  toolOffsetFormTitle =
    XtVaCreateManagedWidget("toolOffsetFormTitle",
                            labelWidgetClass,
                            toolOffsetForm,
                            NULL);

  toolOffsetFormName =
    XtVaCreateManagedWidget("toolOffsetFormName",
                            labelWidgetClass,
                            toolOffsetForm,
                            XtNfromHoriz, toolOffsetFormTitle,
                            NULL);

  toolSetOffsetShell =
    XtVaCreatePopupShell("toolSetOffsetShell",
                         transientShellWidgetClass,
                         topLevel,
                         NULL);

  toolSetOffsetForm =
    XtVaCreateManagedWidget("toolSetOffsetForm",
                            formWidgetClass,
                            toolSetOffsetShell,
                            NULL);

  toolSetOffsetToolLabel =
    XtVaCreateManagedWidget("toolSetOffsetToolLabel",
                            labelWidgetClass,
                            toolSetOffsetForm,
                            XtNborderWidth, 0,
                            NULL);

  toolSetOffsetTool =
    XtVaCreateManagedWidget("toolSetOffsetTool",
                            asciiTextWidgetClass,
                            toolSetOffsetForm,
                            XtNfromHoriz, toolSetOffsetToolLabel,
                            XtNeditType, XawtextEdit,
                            NULL);

  toolSetOffsetLengthLabel =
    XtVaCreateManagedWidget("toolSetOffsetLengthLabel",
                            labelWidgetClass,
                            toolSetOffsetForm,
                            XtNfromVert, toolSetOffsetToolLabel,
                            XtNborderWidth, 0,
                            NULL);

  toolSetOffsetLength =
    XtVaCreateManagedWidget("toolSetOffsetLength",
                            asciiTextWidgetClass,
                            toolSetOffsetForm,
                            XtNfromHoriz, toolSetOffsetLengthLabel,
                            XtNfromVert, toolSetOffsetToolLabel,
                            XtNeditType, XawtextEdit,
                            NULL);

   toolSetOffsetDiameterLabel =
    XtVaCreateManagedWidget("toolSetOffsetDiameterLabel",
                            labelWidgetClass,
                            toolSetOffsetForm,
                            XtNfromVert, toolSetOffsetLengthLabel,
                            XtNborderWidth, 0,
                            NULL);

   toolSetOffsetDiameter =
    XtVaCreateManagedWidget("toolSetOffsetDiameter",
                            asciiTextWidgetClass,
                            toolSetOffsetForm,
                            XtNfromHoriz, toolSetOffsetDiameterLabel,
                            XtNfromVert, toolSetOffsetLengthLabel,
                            XtNeditType, XawtextEdit,
                            NULL);

  toolSetOffsetDone =
    XtVaCreateManagedWidget("toolSetOffsetDone",
                            commandWidgetClass,
                            toolSetOffsetForm,
                            XtNfromVert, toolSetOffsetDiameterLabel,
                            NULL);

  toolSetOffsetCancel =
    XtVaCreateManagedWidget("toolSetOffsetCancel",
                            commandWidgetClass,
                            toolSetOffsetForm,
                            XtNfromVert, toolSetOffsetDiameterLabel,
                            XtNfromHoriz, toolSetOffsetDone,
                            NULL);

  XtAddCallback(toolSetOffsetDone, XtNcallback, toolSetOffsetDoneCB, NULL);
  XtAddCallback(toolSetOffsetCancel, XtNcallback, toolSetOffsetCancelCB, NULL);

 positionTypeForm =
    XtVaCreateManagedWidget("positionTypeForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, commandMenuForm,
                            XtNfromHoriz, toolNumberForm,
                            NULL);

  positionTypeFormTitle =
    XtVaCreateManagedWidget("positionTypeFormTitle",
                            labelWidgetClass,
                            positionTypeForm,
                            NULL);

  positionTypeFormName =
    XtVaCreateManagedWidget("positionTypeFormName",
                            labelWidgetClass,
                            positionTypeForm,
                            XtNfromHoriz, positionTypeFormTitle,
                            NULL);

  workOffsetForm =
    XtVaCreateManagedWidget("workOffsetForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, positionTypeForm,
                            XtNfromHoriz, toolNumberForm,
                            NULL);

  workOffsetFormTitle =
    XtVaCreateManagedWidget("workOffsetFormTitle",
                            labelWidgetClass,
                            workOffsetForm,
                            NULL);

  workOffsetFormName =
    XtVaCreateManagedWidget("workOffsetFormName",
                            labelWidgetClass,
                            workOffsetForm,
                            XtNfromHoriz, workOffsetFormTitle,
                            NULL);

  // set up position labels

  for (t = 0; t < XEMC_NUM_AXES; t++) {
    // make the resource name "posLabel0,1,2,..."
    sprintf(string, "posLabel%d", t);
    if (t == 0) {
      // for first label, create and get resources out
      posLabel[0] =
        XtVaCreateManagedWidget(string,
                                labelWidgetClass,
                                topForm,
                                XtNfromVert, toolOffsetForm,
                                NULL);

      XtVaGetValues(posLabel[0],
                    XtNwidth, &posw,
                    XtNfont, &posfont,
                    NULL);
    }
    else {
      // for other labels, use first label resources
      posLabel[t] =
        XtVaCreateManagedWidget(string,
                                labelWidgetClass,
                                topForm,
                                XtNfromVert, posLabel[t-1],
                                XtNwidth, posw,
                                XtNfont, posfont,
                                NULL);
    }
  }

  abortCommand =
    XtVaCreateManagedWidget("abortCommand",
                            commandWidgetClass,
                            commandMenuForm,
                            XtNheight, sh + cmfbw + cmfbw + bh,
                            XtNwidth, posw - (bw + mw + stw + 8 * cmfbw),
                            XtNfromHoriz, spindleIncLabel,
                            NULL);

  XtAddCallback(abortCommand, XtNcallback, abortCB, NULL);

  posOffsetShell =
    XtVaCreatePopupShell("posOffsetShell",
                         transientShellWidgetClass,
                         topLevel,
                         NULL);

  posOffsetDialog =
    XtVaCreateManagedWidget("posOffsetDialog",
                            dialogWidgetClass,
                            posOffsetShell,
                            NULL);

  jogSpeedForm =
    XtVaCreateManagedWidget("jogSpeedForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, posLabel[XEMC_NUM_AXES - 1],
                            NULL);

  jogSpeedTitleLabel =
    XtVaCreateManagedWidget("jogSpeedTitleLabel",
                            labelWidgetClass,
                            jogSpeedForm,
                            NULL);

  jogSpeedDecLabel =
    XtVaCreateManagedWidget("jogSpeedDecLabel",
                            labelWidgetClass,
                            jogSpeedForm,
                            XtNfromVert, jogSpeedTitleLabel,
                            NULL);

  jogSpeedLabel =
    XtVaCreateManagedWidget("jogSpeedLabel",
                            labelWidgetClass,
                            jogSpeedForm,
                            XtNfromHoriz, jogSpeedDecLabel,
                            XtNfromVert, jogSpeedTitleLabel,
                            NULL);

  jogSpeedIncLabel =
    XtVaCreateManagedWidget("jogSpeedIncLabel",
                            labelWidgetClass,
                            jogSpeedForm,
                            XtNfromHoriz, jogSpeedLabel,
                            XtNfromVert, jogSpeedTitleLabel,
                            NULL);

  jogSpeedShell =
    XtVaCreatePopupShell("jogSpeedShell",
                         transientShellWidgetClass,
                         topLevel,
                         NULL);

  jogSpeedDialog =
    XtVaCreateManagedWidget("jogSpeedDialog",
                            dialogWidgetClass,
                            jogSpeedShell,
                            NULL);

  jogIncrementForm =
    XtVaCreateManagedWidget("jogIncrementForm",
                            formWidgetClass,
                            topForm,
                            XtNfromHoriz, jogSpeedForm,
                            XtNfromVert, posLabel[XEMC_NUM_AXES - 1],
                            NULL);

  jogIncrementTitleLabel =
    XtVaCreateManagedWidget("jogIncrementTitleLabel",
                            labelWidgetClass,
                            jogIncrementForm,
                            NULL);

  setupMenu(&jogIncrementMenu, "jogIncrementMenu",
            jogIncrementMenuEntryNames,
            jogIncrementForm,
            NULL, jogIncrementTitleLabel,
            jogIncrementMenuSelect);

  jogForm =
    XtVaCreateManagedWidget("jogForm",
                            formWidgetClass,
                            topForm,
                            XtNfromHoriz, jogIncrementForm,
                            XtNfromVert, posLabel[XEMC_NUM_AXES - 1],
                            NULL);

  jogTitleLabel =
    XtVaCreateManagedWidget("jogTitleLabel",
                            labelWidgetClass,
                            jogForm,
                            NULL);

  jogMinusLabel =
    XtVaCreateManagedWidget("jogMinusLabel",
                            labelWidgetClass,
                            jogForm,
                            XtNfromVert, jogTitleLabel,
                            NULL);

  homeCommand =
    XtVaCreateManagedWidget("homeCommand",
                            commandWidgetClass,
                            jogForm,
                            XtNfromHoriz, jogMinusLabel,
                            XtNfromVert, jogTitleLabel,
                            NULL);

  XtAddCallback(homeCommand, XtNcallback, homeCB, 0);

  jogPlusLabel =
    XtVaCreateManagedWidget("jogPlusLabel",
                            labelWidgetClass,
                            jogForm,
                            XtNfromHoriz, homeCommand,
                            XtNfromVert, jogTitleLabel,
                            NULL);

  feedOverrideForm =
    XtVaCreateManagedWidget("feedOverrideForm",
                            formWidgetClass,
                            topForm,
                            XtNfromHoriz, jogForm,
                            XtNfromVert, posLabel[XEMC_NUM_AXES - 1],
                            NULL);

  feedOverrideTitleLabel =
    XtVaCreateManagedWidget("feedOverrideTitleLabel",
                            labelWidgetClass,
                            feedOverrideForm,
                            NULL);

  feedOverrideDecLabel =
    XtVaCreateManagedWidget("feedOverrideDecLabel",
                            labelWidgetClass,
                            feedOverrideForm,
                            XtNfromVert, feedOverrideTitleLabel,
                            NULL);

  feedOverrideLabel =
    XtVaCreateManagedWidget("feedOverrideLabel",
                            labelWidgetClass,
                            feedOverrideForm,
                            XtNfromHoriz, feedOverrideDecLabel,
                            XtNfromVert, feedOverrideTitleLabel,
                            NULL);

  feedOverrideIncLabel =
    XtVaCreateManagedWidget("feedOverrideIncLabel",
                            labelWidgetClass,
                            feedOverrideForm,
                            XtNfromHoriz, feedOverrideLabel,
                            XtNfromVert, feedOverrideTitleLabel,
                            NULL);

  feedOverrideShell =
    XtVaCreatePopupShell("feedOverrideShell",
                         transientShellWidgetClass,
                         topLevel,
                         NULL);

  feedOverrideDialog =
    XtVaCreateManagedWidget("feedOverrideDialog",
                            dialogWidgetClass,
                            feedOverrideShell,
                            NULL);

  mdiForm =
    XtVaCreateManagedWidget("mdiForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, jogSpeedForm,
                            NULL);

  mdiFormTitle =
    XtVaCreateManagedWidget("mdiFormTitle",
                            labelWidgetClass,
                            mdiForm,
                            NULL);

  mdiFormText =
    XtVaCreateManagedWidget("mdiFormText",
                            asciiTextWidgetClass,
                            mdiForm,
                            XtNfromHoriz, mdiFormTitle,
                            XtNeditType, XawtextEdit,
                            NULL);

  mdiCodesLabel =
    XtVaCreateManagedWidget("mdiCodesLabel",
                            labelWidgetClass,
                            topForm,
                            XtNfromVert, mdiForm,
                            NULL);

  programForm =
    XtVaCreateManagedWidget("programForm",
                            formWidgetClass,
                            topForm,
                            XtNfromVert, mdiCodesLabel,
                            NULL);

  programFormTitle =
    XtVaCreateManagedWidget("programFormTitle",
                            labelWidgetClass,
                            programForm,
                            NULL);

  programFormName =
    XtVaCreateManagedWidget("programFormName",
                            labelWidgetClass,
                            programForm,
                            XtNfromHoriz, programFormTitle,
                            NULL);

  programFormStateTitle =
    XtVaCreateManagedWidget("programFormStateTitle",
                            labelWidgetClass,
                            programForm,
                            XtNfromHoriz, programFormName,
                            NULL);

  programFormState =
    XtVaCreateManagedWidget("programFormState",
                            labelWidgetClass,
                            programForm,
                            XtNfromHoriz, programFormStateTitle,
                            NULL);

  programOpenCommand =
    XtVaCreateManagedWidget("programOpenCommand",
                            commandWidgetClass,
                            programForm,
                            XtNfromVert, programFormTitle,
                            NULL);

  XtAddCallback(programOpenCommand, XtNcallback, dialogPopup, fileOpenShell);

  programRunCommand =
    XtVaCreateManagedWidget("programRunCommand",
                            commandWidgetClass,
                            programForm,
                            XtNfromHoriz, programOpenCommand,
                            XtNfromVert, programFormTitle,
                            NULL);

  XtAddCallback(programRunCommand, XtNcallback, programRunCB, NULL);

  programPauseCommand =
    XtVaCreateManagedWidget("programPauseCommand",
                            commandWidgetClass,
                            programForm,
                            XtNfromHoriz, programRunCommand,
                            XtNfromVert, programFormTitle,
                            NULL);

  XtAddCallback(programPauseCommand, XtNcallback, programPauseCB, NULL);

  programResumeCommand =
    XtVaCreateManagedWidget("programResumeCommand",
                            commandWidgetClass,
                            programForm,
                            XtNfromHoriz, programPauseCommand,
                            XtNfromVert, programFormTitle,
                            NULL);

  XtAddCallback(programResumeCommand, XtNcallback, programResumeCB, NULL);

  programStepCommand =
    XtVaCreateManagedWidget("programStepCommand",
                            commandWidgetClass,
                            programForm,
                            XtNfromHoriz, programResumeCommand,
                            XtNfromVert, programFormTitle,
                            NULL);

  XtAddCallback(programStepCommand, XtNcallback, programStepCB, NULL);

  programVerifyCommand =
    XtVaCreateManagedWidget("programVerifyCommand",
                            commandWidgetClass,
                            programForm,
                            XtNfromHoriz, programStepCommand,
                            XtNfromVert, programFormTitle,
                            NULL);

  XtAddCallback(programVerifyCommand, XtNcallback, programVerifyCB, NULL);

  programText =
    XtVaCreateManagedWidget("programText",
                            asciiTextWidgetClass,
                            topForm,
                            XtNfromVert, programForm,
                            NULL);

  // add actions table for all action functions
  XtAppAddActions(app_context, actionsTable, XtNumber(actionsTable));

  // create windows for widgets and map them
  XtRealizeWidget(topLevel);

  // init atom to catch window kills
  killAtom = XInternAtom(XtDisplay(topLevel), "WM_DELETE_WINDOW", False);

  // initialize hard-coded colors
  stringToPixel(topForm, "white", &pixelWhite);
  stringToPixel(topForm, "black", &pixelBlack);
  stringToPixel(topForm, "red", &pixelRed);
  stringToPixel(topForm, "yellow", &pixelYellow);
  stringToPixel(topForm, "green", &pixelGreen);

  for (t = 0; t < XEMC_NUM_AXES; t++) {
    // initialize diagnostics ferrors
    oldAxisFerror[t] = -1.0;
    // get position label background and border colors for toggling
    getColor(posLabel[t], &posLabelBackground[t], 0);
    getBorderColor(posLabel[t], &posLabelBorderColor[t]);
    // initialize pos label colors
    posColor[t] = 0;
  }

  // get border color for dialog buttons, for de-highlighting

  getBorderColor(fileOpenDialog, &fileOpenBorderColor);
  setBorderColor(fileOpenDone, pixelRed);

  getBorderColor(fileEditDialog, &fileEditBorderColor);
  setBorderColor(fileEditDone, pixelRed);

  getBorderColor(fileQuitDialog, &fileQuitBorderColor);
  setBorderColor(fileQuitDone, pixelRed);

  getBorderColor(toolSetOffsetForm, &toolSetOffsetFormBorderColor);
  setBorderColor(toolSetOffsetDone, pixelRed);

  // set labels for dynamic labels we can't init in timeoutCB
  sprintf(string, "%d", jogSpeed);
  setLabel(jogSpeedLabel, string);

  // start X interval timer
  XtAppAddTimeOut(app_context, UPDATE_MSECS, timeoutCB, NULL);

  // trap window kill for top level
  XSetWMProtocols(XtDisplay(topLevel), XtWindow(topLevel), &killAtom, 1);

  // loop for events
  XtAppMainLoop(app_context);

  return 0;
}
