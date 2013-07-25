/********************************************************************
* Description:   keystick.cc
*   Curses-based keyboard control panel
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
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>           // struct itimerval
#include <sys/ioctl.h>          // ioctl(), TIOCGWINSZ, struct winsize
#include <unistd.h>             // STDIN_FILENO

#include "rcs.hh"               // rcs_print_error(), esleep()
#include "emc.hh"               // EMC NML
#include "emc_nml.hh"
#include "emcglb.h"             // EMC_NMLFILE, TRAJ_MAX_VELOCITY
#include "emccfg.h"             // DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"           // INIFILE
#include "rcs_print.hh"
#include "nml_oi.hh"
#include "timer.hh"

#include <ncurses.h>
#define ESC 27
#define TAB 9
#define RETURN 13
#define ALT(ch) ((ch) + 128)
#define CTL(ch) ((ch) - 64)

// paths to awk and xgraph, using to plot logs
// fill these in for your system, if these are not in user's path
#define AWK_PATH "awk"
#define XGRAPH_PATH "xgraph"
#define SYSTEM_STRING_LEN 1024


// the NML channels to the EMC task
static RCS_CMD_CHANNEL *emcCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcStatusBuffer = 0;
EMC_STAT *emcStatus = 0;

// the NML channel for errors
static NML *emcErrorBuffer = 0;
static char error_string[NML_ERROR_LEN] = "";

// the current command numbers, set up updateStatus(), used in main()
static int emcCommandSerialNumber = 0;

// NML messages
static EMC_AXIS_HOME emc_axis_home_msg;
static EMC_AXIS_ABORT emc_axis_abort_msg;
static EMC_AXIS_JOG emc_axis_jog_msg;
static EMC_AXIS_INCR_JOG emc_axis_incr_jog_msg;
static EMC_TRAJ_SET_SCALE emc_traj_set_scale_msg;
static EMC_TRAJ_ABORT emc_traj_abort_msg;
static EMC_SPINDLE_ON emc_spindle_on_msg;
static EMC_SPINDLE_OFF emc_spindle_off_msg;
static EMC_SPINDLE_INCREASE emc_spindle_increase_msg;
static EMC_SPINDLE_DECREASE emc_spindle_decrease_msg;
static EMC_SPINDLE_CONSTANT emc_spindle_constant_msg;
static EMC_SPINDLE_BRAKE_RELEASE emc_spindle_brake_release_msg;
static EMC_SPINDLE_BRAKE_ENGAGE emc_spindle_brake_engage_msg;
static EMC_COOLANT_MIST_ON emc_coolant_mist_on_msg;
static EMC_COOLANT_MIST_OFF emc_coolant_mist_off_msg;
static EMC_COOLANT_FLOOD_ON emc_coolant_flood_on_msg;
static EMC_COOLANT_FLOOD_OFF emc_coolant_flood_off_msg;
static EMC_LUBE_ON emc_lube_on_msg;
static EMC_LUBE_OFF emc_lube_off_msg;
static EMC_TOOL_LOAD_TOOL_TABLE emc_tool_load_tool_table_msg;
static EMC_TASK_SET_MODE mode_msg;
static EMC_TASK_SET_STATE state_msg;
static EMC_TASK_ABORT task_abort_msg;
static EMC_TASK_PLAN_INIT task_plan_init_msg;
static EMC_TASK_PLAN_OPEN task_plan_open_msg;
static EMC_TASK_PLAN_RUN task_plan_run_msg;
static EMC_TASK_PLAN_EXECUTE task_plan_execute_msg;
static EMC_TASK_PLAN_PAUSE task_plan_pause_msg;
static EMC_TASK_PLAN_RESUME task_plan_resume_msg;
static EMC_TASK_PLAN_STEP task_plan_step_msg;

// critical section flag-- set to non-zero to prevent sig handler
// from interrupting your window printing
static unsigned char critFlag = 0;

// the program path prefix
static char programPrefix[LINELEN] = "";
// the saved program name
static char programFile[LINELEN] = "";
static int programOpened = 0;
static FILE * programFp = 0;
static int programFpLine = 0;
static int programActiveLine = 0;
static char programLineText[LINELEN] = "";


// error log file
static FILE *errorFp = NULL;
#define ERROR_FILE "keystick.err"

typedef enum {
  JOG_CONTINUOUS = 1,
  JOG_INCREMENTAL
} JOG_MODE;

static JOG_MODE jogMode
 = JOG_CONTINUOUS;
static double jogIncrement = 0.001;
static double jogSpeed = 60.0;

static int xJogPol = 1;
static int yJogPol = 1;
static int zJogPol = 1;

typedef enum {
  AXIS_NONE = 1,
  AXIS_X,
  AXIS_Y,
  AXIS_Z
} AXIS_TYPE;

static AXIS_TYPE axisSelected = AXIS_X;
static AXIS_TYPE axisJogging = AXIS_NONE;

static const char * axisString(AXIS_TYPE a)
{
  switch (a)
    {
    case AXIS_X:
      return "    X SELECTED    ";
    case AXIS_Y:
      return "    Y SELECTED    ";
    case AXIS_Z:
      return "    Z SELECTED    ";
    default:
      return "    ? SELECTED    ";
    }
}

static int axisIndex(AXIS_TYPE a)
{
  if (a == AXIS_X)
    return 0;
  if (a == AXIS_Y)
    return 1;
  if (a == AXIS_Z)
    return 2;

  return 0;
}

typedef enum {
  COORD_RELATIVE = 1,
  COORD_ABSOLUTE
} COORD_TYPE;

static COORD_TYPE coords = COORD_RELATIVE;

typedef enum {
  POS_DISPLAY_ACT = 1,
  POS_DISPLAY_CMD
} POS_DISPLAY_TYPE;

static POS_DISPLAY_TYPE posDisplay = POS_DISPLAY_ACT;

static int spindleChanging = 0;

#define INTERRUPT_USECS 50000
static int usecs = INTERRUPT_USECS;

static chtype ch = 0, oldch = 0;

#define ASCLINELEN 80
static char line_blank[ASCLINELEN + 1];

static char scratch_string[ASCLINELEN] = "";
static char state_string[ASCLINELEN] = "";
static char mode_string[ASCLINELEN] = "";
static char spindle_string[ASCLINELEN] = "";
static char brake_string[ASCLINELEN] = "";
static char mist_string[ASCLINELEN] = "";
static char flood_string[ASCLINELEN] = "";
static char lube_on_string[ASCLINELEN] = "";
static char lube_level_string[ASCLINELEN] = "";
static char home_string[ASCLINELEN] = "";
static char pos_string[ASCLINELEN] = "";
static char origin_string[ASCLINELEN] = "";
static char speed_string[ASCLINELEN] = "";
static char incr_string[ASCLINELEN] = "";
static char prog_string[ASCLINELEN] = "";
static char line_string[ASCLINELEN] = "";
static char interp_string[ASCLINELEN] = "";
static char active_g_codes_string[ASCLINELEN] = "";
static char active_m_codes_string[ASCLINELEN] = "";

// bottom string gill be set to "---Machine Version---"
static char bottom_string[ASCLINELEN + 1] = "";

// key repeat delays, in microseconds
#define DEFAULT_FIRST_KEYUP_DELAY 300000 // works w/ 50000 alarm
static int FIRST_KEYUP_DELAY = DEFAULT_FIRST_KEYUP_DELAY;
#define DEFAULT_NEXT_KEYUP_DELAY  100000 // works w/ 50000 alarm
static int NEXT_KEYUP_DELAY = DEFAULT_NEXT_KEYUP_DELAY;

static int keyup_count = 0;

static enum {DIAG_USECS = 1, DIAG_FIRST_KEYUP_DELAY, DIAG_NEXT_KEYUP_DELAY} diagtab = DIAG_USECS;

static WINDOW * window = 0;
static WINDOW * helpwin = 0;
static WINDOW * diagwin = 0;
static WINDOW * toolwin = 0;
static WINDOW * logwin = 0;
static WINDOW * progwin = 0;

static int wbegy, wbegx;
static int wmaxy, wmaxx;

static void printFkeys()
{
  wattrset(window, A_BOLD);
  mvwaddstr(window, 0, 1,  "F1 ");
  mvwaddstr(window, 1, 1,  "F2 ");
  mvwaddstr(window, 2, 1,  "F3 ");
  mvwaddstr(window, 3, 1,  "F4 ");
  mvwaddstr(window, 0, 21, "F5 ");
  mvwaddstr(window, 1, 21, "F6 ");
  mvwaddstr(window, 2, 21, "F7 ");
  mvwaddstr(window, 3, 21, "F8 ");
  mvwaddstr(window, 0, 41, "F9 ");
  mvwaddstr(window, 1, 41, "F10");
  mvwaddstr(window, 2, 41, "F11");
  mvwaddstr(window, 3, 41, "F12");
  mvwaddstr(window, 0, 61, "ESC");
  mvwaddstr(window, 1, 61, "TAB");
  mvwaddstr(window, 2, 61, "END");
  mvwaddstr(window, 3, 61, " ? ");

  wattrset(window, 0);
  mvwaddstr(window, 0, 5,      "Estop On/Off  ");
  mvwaddstr(window, 1, 5,      "Machine On/Off");
  mvwaddstr(window, 2, 5,      "Manual Mode   ");
  mvwaddstr(window, 3, 5,      "Auto Mode     ");

  mvwaddstr(window, 0, 25,     "MDI Mode      ");
  mvwaddstr(window, 1, 25,     "Reset Interp  ");
  mvwaddstr(window, 2, 25,     "Mist On/Off   ");
  mvwaddstr(window, 3, 25,     "Flood On/Off  ");

  mvwaddstr(window, 0, 45,     "Spndl Fwd/Off ");
  mvwaddstr(window, 1, 45,     "Spndl Rev/Off ");
  mvwaddstr(window, 2, 45,     "Spndl Decrease");
  mvwaddstr(window, 3, 45,     "Spndl Increase");

  mvwaddstr(window, 0, 65,     "Aborts Actions");
  mvwaddstr(window, 1, 65,     "Selects Params");
  mvwaddstr(window, 2, 65,     "Quits Display ");
  mvwaddstr(window, 3, 65,     "Toggles Help  ");
}

#define ERR_Y ((wmaxy) - 2)
#define ERR_X (wbegx)

static void clearWindow()
{
  int t;

  critFlag = 1;

  wattrset(window, A_BOLD);
  for (t = wbegy; t <= wmaxy; t++)
    {
      mvwaddstr(window, t, 0, line_blank);
    }
  wrefresh(window);

  wattrset(window, 0);
  for (t = wbegy; t <= wmaxy; t++)
    {
      mvwaddstr(window, t, 0, line_blank);
    }
  wmove(window, wmaxy, wbegx);
  wrefresh(window);

  critFlag = 0;
}

static void printError(const char * errstring)
{
  int savey, savex;
  chtype saveattr;

  // log to error file
  if (errorFp)
    {
      fprintf(errorFp, "%f\t%s\n", etime(), errstring);
    }

  if (0 == window)
    {
      // no window yet
      return;
    }

  critFlag = 1;

  getyx(window, savey, savex);
  saveattr = getattrs(window);

  mvwaddstr(window, ERR_Y, ERR_X, line_blank);
  wattrset(window, A_BOLD);
  mvwaddstr(window, ERR_Y, ERR_X, errstring);
  (void)wattrset(window, (int) saveattr);
  wmove(window, savey, savex);
  wrefresh(window);

  critFlag = 0;
}

static void printStatus()
{
  int savey, savex;
  int t;
  int line;
  int override;
  int len;
  int code;

  getyx(window, savey, savex);

  if (window == helpwin)
    {
      printFkeys();

      wattrset(window, 0);
      mvwaddstr(window, 5, 1, "x/X y/Y z/Z");
      mvwaddstr(window, 6, 1, "HOME");
      mvwaddstr(window, 7, 1, "< > or , .");
      mvwaddstr(window, 8, 1, "1-9, 0");
      mvwaddstr(window, 9, 1, "<- arrow keys ->");
      mvwaddstr(window, 10, 1, "^ arrow keys V");
      mvwaddstr(window, 11, 1, "PageUp/PageDown");
      mvwaddstr(window, 12, 1, "c/C");
      mvwaddstr(window, 13, 1, "i/I");
      mvwaddstr(window, 14, 1, "#");
      mvwaddstr(window, 15, 1, "@");

      wattrset(window, A_UNDERLINE);
      mvwaddstr(window, 5, 19, "selects axis");
      mvwaddstr(window, 6, 19, "homes selected axis");
      mvwaddstr(window, 7, 19, "change jog speed");
      mvwaddstr(window, 8, 19, "10%-90%, 100% feed");
      mvwaddstr(window, 9, 19, "jog X");
      mvwaddstr(window, 10, 19, "jog Y");
      mvwaddstr(window, 11, 19, "jog Z");
      mvwaddstr(window, 12, 19, "sets continuous jog");
      mvwaddstr(window, 13, 19, "toggles incr jog");
      mvwaddstr(window, 14, 19, "toggles abs/rel");
      mvwaddstr(window, 15, 19, "toggles cmd/act");

      wattrset(window, 0);
      mvwaddstr(window, 5, 41, "B");
      mvwaddstr(window, 6, 41, "b");
      mvwaddstr(window, 7, 41, "o/O");
      mvwaddstr(window, 8, 41, "r/R");
      mvwaddstr(window, 9, 41, "p/P");
      mvwaddstr(window, 10, 41, "s/S");
      mvwaddstr(window, 11, 41, "quote/XYZGM");
      mvwaddstr(window, 12, 41, "l/L");
      mvwaddstr(window, 13, 41, "u/U");

      wattrset(window, A_UNDERLINE);
      mvwaddstr(window, 5, 54, "turns spindle brake on");
      mvwaddstr(window, 6, 54, "turns spindle brake off");
      mvwaddstr(window, 7, 54, "prompts for program");
      mvwaddstr(window, 8, 54, "runs selected program");
      mvwaddstr(window, 9, 54, "pauses motion");
      mvwaddstr(window, 10, 54, "starts motion again");
      mvwaddstr(window, 11, 54, "prompts for MDI command");
      mvwaddstr(window, 12, 54, "prompts for tool table");
      mvwaddstr(window, 13, 54, "turns lube off/on");

      if (error_string[0])
        {
          printError(error_string);
        }

      wattrset(window, A_REVERSE);
      mvwaddstr(window, wmaxy - 1, wbegx, bottom_string);
      wattrset(window, 0);

      // restore cursor position
      wmove(window, savey, savex);
      wrefresh(window);
    }
  else if (window == diagwin)
    {
      wattrset(window, A_BOLD);
      mvwaddstr(window, 0, 34, "Diagnostics");

      wattrset(window, 0);
      mvwaddstr(window, 2, 1, "Task Heartbeat/Cmd:");
      mvwaddstr(window, 3, 1, "IO Heartbeat/Cmd:");
      mvwaddstr(window, 4, 1, "Motion Heartbeat/Cmd:");

      if (diagtab == DIAG_USECS)
          wattrset(window, A_BOLD);
      mvwaddstr(window, 6, 1,  "Polling Period (usecs):");
      wattrset(window, 0);
      if (diagtab == DIAG_FIRST_KEYUP_DELAY)
          wattrset(window, A_BOLD);
      mvwaddstr(window, 7, 1, "Kbd Delay Until Repeat:");
      wattrset(window, 0);
      if (diagtab == DIAG_NEXT_KEYUP_DELAY)
          wattrset(window, A_BOLD);
      mvwaddstr(window, 8, 1, "Kbd Delay Between Repeats:");
      wattrset(window, 0);

      mvwaddstr(window, 10, 1, "Task Execution State:");

      mvwaddstr(window, 12, 1, "Traj Scale:");
      mvwaddstr(window, 13, 1, "X Scale:");
      mvwaddstr(window, 14, 1, "Y Scale:");
      mvwaddstr(window, 15, 1, "Z Scale:");

      mvwaddstr(window, 17, 1, "V/Max V:");
      mvwaddstr(window, 18, 1, "A/Max A:");

      wattrset(window, A_UNDERLINE);

      sprintf(scratch_string, "%10ld %10d %10d", emcStatus->task.heartbeat,
              emcStatus->echo_serial_number,
              emcStatus->status);
      mvwaddstr(window, 2, 28, scratch_string);
      sprintf(scratch_string, "%10ld %10d", emcStatus->io.heartbeat,
              emcStatus->io.echo_serial_number);
      mvwaddstr(window, 3, 28, scratch_string);
      sprintf(scratch_string, "%10ld %10d", emcStatus->motion.heartbeat,
              emcStatus->motion.echo_serial_number);
      mvwaddstr(window, 4, 28, scratch_string);

      sprintf(scratch_string, "%10d", usecs);
      mvwaddstr(window, 6, 28, scratch_string);
      sprintf(scratch_string, "%10d", FIRST_KEYUP_DELAY);
      mvwaddstr(window, 7, 28, scratch_string);
      sprintf(scratch_string, "%10d", NEXT_KEYUP_DELAY);
      mvwaddstr(window, 8, 28, scratch_string);

      sprintf(scratch_string, "%10d", emcStatus->task.execState);
      mvwaddstr(window, 10, 28, scratch_string);

      sprintf(scratch_string, "%10.3f", emcStatus->motion.traj.scale);
      mvwaddstr(window, 12, 28, scratch_string);

      sprintf(scratch_string, "%10.3f%10.3f",
              emcStatus->motion.traj.velocity, emcStatus->motion.traj.maxVelocity);
      mvwaddstr(window, 17, 28, scratch_string);
      sprintf(scratch_string, "%10.3f%10.3f",
              emcStatus->motion.traj.acceleration, emcStatus->motion.traj.maxAcceleration);
      mvwaddstr(window, 18, 28, scratch_string);

      wattrset(window, 0);
      if (error_string[0])
        {
          printError(error_string);
        }

      wattrset(window, A_REVERSE);
      mvwaddstr(window, wmaxy - 1, wbegx, bottom_string);
      wattrset(window, 0);

      // restore cursor position
      wmove(window, savey, savex);
      wrefresh(window);
    }
  else if (window == toolwin)
    {
      wattrset(window, A_BOLD);
      mvwaddstr(window, 0, 34, "Tool Table");

      wattrset(window, 0);
      mvwaddstr(window, 2, 1, "Pocket    ToolNo    Length  Diameter");

      wattrset(window, A_UNDERLINE);
      line = 4;
      for (t = 0; t < CANON_POCKETS_MAX; t++)
        {
          if (emcStatus->io.tool.toolTable[t].toolno)
            {
              sprintf(scratch_string, "%4d%10d%10.4f%10.4f",
                      t,
                      emcStatus->io.tool.toolTable[t].toolno,
                      emcStatus->io.tool.toolTable[t].offset.tran.z,
                      emcStatus->io.tool.toolTable[t].diameter);
              mvwaddstr(window, line++, 3, scratch_string);
            }
        }

      wattrset(window, 0);
      if (error_string[0])
        {
          printError(error_string);
        }

      wattrset(window, A_REVERSE);
      mvwaddstr(window, wmaxy - 1, wbegx, bottom_string);
      wattrset(window, 0);

      // restore cursor position
      wmove(window, savey, savex);
      wrefresh(window);
    }
  else if (window == progwin)
    {
      mvwaddstr(window, 0, 0, line_blank);
      wattrset(window, A_BOLD);
      if (emcStatus->task.file[0] == 0)
        {
          mvwaddstr(window, 0, 36, "(no program)");
        }
      else
        {
          mvwaddstr(window, 0, 36, emcStatus->task.file);
        }
      wattrset(window, 0);

      if (emcStatus->task.currentLine > 0)
        {
          if (emcStatus->task.motionLine > 0 &&
              emcStatus->task.motionLine < emcStatus->task.currentLine)
            {
              programActiveLine = emcStatus->task.motionLine;
            }
          else
            {
              programActiveLine = emcStatus->task.currentLine;
            }

          if (programFp)
            {
              if (programFpLine > programActiveLine)
                {
                  rewind(programFp);
                  programFpLine = 0;
                  programLineText[0] = 0;
                }

              // fast forward over past lines
              while (programFpLine < programActiveLine)
                {
                  if (fgets(programLineText, LINELEN, programFp) != NULL)
		    programFpLine++;
                }

              // now we have the current line
              wattrset(window, A_BOLD);
              for (t = 0; t < 20; t++)
                {
                  // knock off CR, LF
                  len = strlen(programLineText) - 1;
                  while (len >= 0)
                    {
                      if (isspace(programLineText[len]))
                        {
                          programLineText[len] = 0;
                          len--;
                        }
                      else
                        {
                          break;
                        }
                    }

                  // print this line
                  mvwaddstr(window, t+2, wbegx, line_blank);
                  mvwaddstr(window, t+2, wbegx, programLineText);
                  wattrset(window, 0);

                  // get the next line
                  if (fgets(programLineText, LINELEN, programFp))
                    {
                      programFpLine++;
                    }
                  else
                    {
                      // make it a blank line to clear any old stuff
                      strcpy(programLineText, line_blank);
                    }
                }
            }
          else
            {
              programLineText[0] = 0;
            }
        }
      else
        {
          programActiveLine = 0;
          programLineText[0] = 0;
          line_string[0] = 0;
        }

      wattrset(window, 0);
      if (error_string[0])
        {
          printError(error_string);
        }

      wattrset(window, A_REVERSE);
      mvwaddstr(window, wmaxy - 1, wbegx, bottom_string);
      wattrset(window, 0);

      // restore cursor position
      wmove(window, savey, savex);
      wrefresh(window);
    }
  else // if (window == stdscr)
    {
      // print the function key labels
      printFkeys();

      // print the status labels

      wattrset(window, 0);

      mvwaddstr(window, 6, 1, "Override:");
      mvwaddstr(window, 7, 1, "Tool:");
      mvwaddstr(window, 8, 1, "Offset:");

      mvwaddstr(window, 7, 61, "Speed:");
      mvwaddstr(window, 8, 61, "Incr:             ");

      strcpy(scratch_string, "--X--");
      if (emcStatus->motion.axis[0].minHardLimit)
        scratch_string[0] = '*';
      if (emcStatus->motion.axis[0].minSoftLimit)
        scratch_string[1] = '*';
      if (emcStatus->motion.axis[0].maxSoftLimit)
        scratch_string[3] = '*';
      if (emcStatus->motion.axis[0].maxHardLimit)
        scratch_string[4] = '*';
      mvwaddstr(window, 10, 27, scratch_string);

      strcpy(scratch_string, "--Y--");
      if (emcStatus->motion.axis[1].minHardLimit)
        scratch_string[0] = '*';
      if (emcStatus->motion.axis[1].minSoftLimit)
        scratch_string[1] = '*';
      if (emcStatus->motion.axis[1].maxSoftLimit)
        scratch_string[3] = '*';
      if (emcStatus->motion.axis[1].maxHardLimit)
        scratch_string[4] = '*';
      mvwaddstr(window, 10, 47, scratch_string);

      strcpy(scratch_string, "--Z--");
      if (emcStatus->motion.axis[2].minHardLimit)
        scratch_string[0] = '*';
      if (emcStatus->motion.axis[2].minSoftLimit)
        scratch_string[1] = '*';
      if (emcStatus->motion.axis[2].maxSoftLimit)
        scratch_string[3] = '*';
      if (emcStatus->motion.axis[2].maxHardLimit)
        scratch_string[4] = '*';
      mvwaddstr(window, 10, 67, scratch_string);

      if (coords == COORD_ABSOLUTE)
        {
          if (posDisplay == POS_DISPLAY_CMD)
            {
              mvwaddstr(window, 11, 1, "Absolute Cmd Pos:");
            }
          else
            {
              mvwaddstr(window, 11, 1, "Absolute Act Pos:");
            }
          mvwaddstr(window, 12, 0, line_blank);
        }
      else
        {
          coords = COORD_RELATIVE;
          if (posDisplay == POS_DISPLAY_CMD)
            {
              mvwaddstr(window, 11, 1, "Relative Cmd Pos:");
            }
          else
            {
              mvwaddstr(window, 11, 1, "Relative Act Pos:");
            }
        }

      mvwaddstr(window, 14, 0, line_blank);
      mvwaddstr(window, 15, 0, line_blank);
      mvwaddstr(window, 16, 0, line_blank);
      mvwaddstr(window, 17, 0, line_blank);
      mvwaddstr(window, 18, 0, line_blank);
      mvwaddstr(window, 19, 0, line_blank);
      if (emcStatus->task.mode == EMC_TASK_MODE_AUTO)
        {
          mvwaddstr(window, 14, 1, "Program:");
          mvwaddstr(window, 15, 1, "Line:");
          mvwaddstr(window, 16, 1, "Command:");
          mvwaddstr(window, 17, 1, "Interpreter:");
          mvwaddstr(window, 18, 1, "Modal G Codes:");
          mvwaddstr(window, 19, 1, "Modal M Codes:");
        }
      else if (emcStatus->task.mode == EMC_TASK_MODE_MDI)
        {
          mvwaddstr(window, 16, 1, "Command:");
          mvwaddstr(window, 17, 1, "Interpreter:");
          mvwaddstr(window, 18, 1, "Modal G Codes:");
          mvwaddstr(window, 19, 1, "Modal M Codes:");
        }

      // end of labels

      // fill the status strings in

      switch(emcStatus->task.state)
        {
        case EMC_TASK_STATE_OFF:
          sprintf(state_string, "       OFF        ");
          break;
        case EMC_TASK_STATE_ON:
          sprintf(state_string, "       ON         ");
          break;
        case EMC_TASK_STATE_ESTOP:
          sprintf(state_string, "      ESTOP       ");
          break;
        case EMC_TASK_STATE_ESTOP_RESET:
          sprintf(state_string, "   ESTOP RESET    ");
          break;
        default:
          sprintf(state_string, "        ?         ");
          break;
        }

      switch(emcStatus->task.mode)
        {
        case EMC_TASK_MODE_MANUAL:
          sprintf(mode_string, "      MANUAL      ");
          break;
        case EMC_TASK_MODE_AUTO:
          sprintf(mode_string, "       AUTO       ");
          break;
        case EMC_TASK_MODE_MDI:
          sprintf(mode_string, "       MDI        ");
          break;
        default:
          sprintf(mode_string, "        ?         ");
          break;
        }

      if (emcStatus->motion.spindle.increasing > 0)
        sprintf(spindle_string, " SPINDLE INCREASE ");
      else if (emcStatus->motion.spindle.increasing < 0)
        sprintf(spindle_string, " SPINDLE DECREASE ");
      else if (emcStatus->motion.spindle.direction > 0)
        sprintf(spindle_string, " SPINDLE FORWARD  ");
      else if (emcStatus->motion.spindle.direction < 0)
        sprintf(spindle_string, " SPINDLE REVERSE  ");
      else
        sprintf(spindle_string, " SPINDLE STOPPED  ");

      if (emcStatus->motion.spindle.brake)
        sprintf(brake_string,   "    BRAKE ON      ");
      else
        sprintf(brake_string,   "    BRAKE OFF     ");

      if (emcStatus->io.coolant.mist)
        sprintf(mist_string,    "    MIST ON       ");
      else
        sprintf(mist_string,    "    MIST OFF      ");

      if (emcStatus->io.coolant.flood)
        sprintf(flood_string,   "    FLOOD ON      ");
      else
        sprintf(flood_string,   "    FLOOD OFF     ");

      if (emcStatus->io.lube.on)
        sprintf(lube_on_string,    "     LUBE ON      ");
      else
        sprintf(lube_on_string,    "     LUBE OFF     ");

      if (! emcStatus->io.lube.level)
        sprintf(lube_level_string,    "     LUBE OK      ");
      else
        sprintf(lube_level_string,    "     LUBE LOW     ");

      sprintf(home_string, "    --- HOMED     ");
      if (emcStatus->motion.axis[0].homed)
        {
          home_string[4] = 'X';
        }
      if (emcStatus->motion.axis[1].homed)
        {
          home_string[5] = 'Y';
        }
      if (emcStatus->motion.axis[2].homed)
        {
          home_string[6] = 'Z';
        }

      if (coords == COORD_ABSOLUTE)
        {
          if (posDisplay == POS_DISPLAY_ACT)
            {
              sprintf(pos_string, "%13.4f  %18.4f  %18.4f",
                      emcStatus->motion.traj.actualPosition.tran.x,
                      emcStatus->motion.traj.actualPosition.tran.y,
                      emcStatus->motion.traj.actualPosition.tran.z);
            }
          else
            {
              sprintf(pos_string, "%13.4f  %18.4f  %18.4f",
                      emcStatus->motion.traj.position.tran.x,
                      emcStatus->motion.traj.position.tran.y,
                      emcStatus->motion.traj.position.tran.z);
            }
        }
      else
        {
          coords = COORD_RELATIVE;
          if (posDisplay == POS_DISPLAY_ACT)
            {
              sprintf(pos_string, "%13.4f  %18.4f  %18.4f",
                      emcStatus->motion.traj.actualPosition.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.x,
                      emcStatus->motion.traj.actualPosition.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.y,
                      emcStatus->motion.traj.actualPosition.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.z);
            }
          else
            {
              sprintf(pos_string, "%13.4f  %18.4f  %18.4f",
                      emcStatus->motion.traj.position.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.x,
                      emcStatus->motion.traj.position.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.y,
                      emcStatus->motion.traj.position.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.z);
            }
        }

      sprintf(origin_string, "%13.4f  %18.4f  %18.4f",
              emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x,
              emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y,
              emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z);

      sprintf(speed_string, "%10.1f", jogSpeed);
      if (jogMode == JOG_INCREMENTAL)
        {
          sprintf(incr_string,  "%10.4f", jogIncrement);
        }
      else
        {
          sprintf(incr_string, "continuous");
        }

      if (! programOpened)
        {
          // print the last one opened, since we'll send this by default
          strcpy(prog_string, programFile);
        }
      else
        {
          // print the one the controller knows about
          strcpy(prog_string, emcStatus->task.file);
        }

      if (emcStatus->task.currentLine > 0)
        {
          if (emcStatus->task.motionLine > 0 &&
              emcStatus->task.motionLine < emcStatus->task.currentLine)
            {
              programActiveLine = emcStatus->task.motionLine;
            }
          else
            {
              programActiveLine = emcStatus->task.currentLine;
            }
          sprintf(line_string, "%d", programActiveLine);
          if (programFp)
            {
              if (programFpLine > programActiveLine)
                {
                  rewind(programFp);
                  programFpLine = 0;
                  programLineText[0] = 0;
                }

              // fast forward over past lines
              while (programFpLine < programActiveLine)
                {
                  if (fgets(programLineText, LINELEN, programFp) != NULL)
            	    programFpLine++;
                }

              // now we have the current line
              // knock off CR, LF
              len = strlen(programLineText) - 1;
              while (len >= 0)
                {
                  if (isspace(programLineText[len]))
                    {
                      programLineText[len] = 0;
                      len--;
                    }
                  else
                    {
                      break;
                    }
                }
            }
          else
            {
              programLineText[0] = 0;
            }
        }
      else
        {
          programActiveLine = 0;
          programLineText[0] = 0;
          line_string[0] = 0;
        }

      switch (emcStatus->task.interpState)
        {
        case EMC_TASK_INTERP_IDLE:
          sprintf(interp_string, "%s", "IDLE    ");
          break;
        case EMC_TASK_INTERP_READING:
          sprintf(interp_string, "%s", "RUNNING ");
          break;
        case EMC_TASK_INTERP_PAUSED:
          sprintf(interp_string, "%s", "PAUSED  ");
          break;
        case EMC_TASK_INTERP_WAITING:
          sprintf(interp_string, "%s", "RUNNING ");
          break;
        default:
          sprintf(interp_string, "%s", "?       ");
          break;
        }

      // fill in the active G codes
      active_g_codes_string[0] = 0;
      for (t = 1; t < ACTIVE_G_CODES; t++)
        {
          code = emcStatus->task.activeGCodes[t];
          if (code == -1)
            continue;
          if (code % 10)
            sprintf(scratch_string, "G%.1f ", (double) code / 10.0);
          else
            sprintf(scratch_string, "G%d ", code / 10);
          strcat(active_g_codes_string, scratch_string);
        }

      // fill in the active M codes, settings too
      active_m_codes_string[0] = 0;
      for (t = 1; t < ACTIVE_M_CODES; t++)
        {
          code = emcStatus->task.activeMCodes[t];
          if (code == -1)
            continue;
          sprintf(scratch_string, "M%d ", code);
          strcat(active_m_codes_string, scratch_string);
        }
      sprintf(scratch_string, "F%.0f ", emcStatus->task.activeSettings[1]);
      strcat(active_m_codes_string, scratch_string);
      sprintf(scratch_string, "S%.0f ", emcStatus->task.activeSettings[2]);
      strcat(active_m_codes_string, scratch_string);

      // fill the screen in

      override = (int) (emcStatus->motion.traj.scale * 100.0 + 0.5);
      sprintf(scratch_string, "%4d%%", override);
      if (override < 100)
        wattrset(window, A_BOLD);
      else
        wattrset(window, A_UNDERLINE);
      mvwaddstr(window, 6, 14, scratch_string);

      sprintf(scratch_string, "%8d", emcStatus->io.tool.toolInSpindle);
      wattrset(window, A_UNDERLINE);
      mvwaddstr(window, 7, 11, scratch_string);

      sprintf(scratch_string, "%8.4f", emcStatus->task.toolOffset.tran.z);
      wattrset(window, A_UNDERLINE);
      mvwaddstr(window, 8, 11, scratch_string);

      wattrset(window, A_REVERSE);

      mvwaddstr(window, 5, 1, state_string);
      mvwaddstr(window, 5, 21, mode_string);
      mvwaddstr(window, 6, 21, lube_on_string);
      mvwaddstr(window, 7, 21, lube_level_string);
      mvwaddstr(window, 5, 41, spindle_string);
      mvwaddstr(window, 6, 41, brake_string);
      mvwaddstr(window, 7, 41, mist_string);
      mvwaddstr(window, 8, 41, flood_string);
      mvwaddstr(window, 5, 61, home_string);
      mvwaddstr(window, 6, 61, axisString(axisSelected));

      wattrset(window, A_UNDERLINE);

      mvwaddstr(window, 11, 21, pos_string);

      if (coords == COORD_RELATIVE)
        {
          wattrset(window, 0);
          mvwaddstr(window, 12, 21, origin_string);
        }

      wattrset(window, A_UNDERLINE);

      mvwaddstr(window, 7, 69, speed_string);
      mvwaddstr(window, 8, 69, incr_string);

      if (emcStatus->task.mode == EMC_TASK_MODE_AUTO)
        {
          mvwaddstr(window, 14, 21, prog_string);
          mvwaddstr(window, 15, 21, line_string);
          if (emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED)
            wattrset(window, A_BOLD);
          mvwaddstr(window, 16, 21, programLineText);
          mvwaddstr(window, 17, 21, interp_string);
          wattrset(window, A_UNDERLINE);
          mvwaddstr(window, 18, 21, active_g_codes_string);
          mvwaddstr(window, 19, 21, active_m_codes_string);
        }
      else if (emcStatus->task.mode == EMC_TASK_MODE_MDI)
        {
          if (emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED)
            wattrset(window, A_BOLD);
          mvwaddstr(window, 16, 21, emcStatus->task.command);
          mvwaddstr(window, 17, 21, interp_string);
          wattrset(window, A_UNDERLINE);
          mvwaddstr(window, 18, 21, active_g_codes_string);
          mvwaddstr(window, 19, 21, active_m_codes_string);
        }

      if (error_string[0])
        {
          printError(error_string);
        }

      wattrset(window, A_REVERSE);
      mvwaddstr(window, wmaxy - 1, wbegx, bottom_string);
      wattrset(window, 0);

      // restore cursor position
      wmove(window, savey, savex);
      wrefresh(window);
    }
}

static int catchErrors = 1;

static int updateStatus()
{
  NMLTYPE type;

  if (0 == emcStatus ||
      0 == emcStatusBuffer ||
      ! emcStatusBuffer->valid())
    {
      return -1;
    }

  if (catchErrors)
    {
      if (0 == emcErrorBuffer ||
          ! emcErrorBuffer->valid())
        {
          return -1;
        }
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

  if (catchErrors)
    {
      switch (type = emcErrorBuffer->read())
        {
        case -1:
          // error reading channel
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
          strcpy(error_string, "unrecognized error");
          break;
        }
    }

  return 0;
}

/*
  waits until the EMC reports that it's got the command with the
  indicated serial_number. Sleeps between queries.
*/

#define EMC_COMMAND_TIMEOUT 1.0 // how long to wait until timeout
#define EMC_COMMAND_DELAY   0.1 // how long to sleep between checks

static int emcCommandWait(int serial_number)
{
  double start = etime();

  while (etime() - start < EMC_COMMAND_TIMEOUT)
    {
      updateStatus();

      if (emcStatus->echo_serial_number == serial_number)
        {
          return 0;
        }

      esleep(EMC_COMMAND_DELAY);
    }

  printError("timeout sending command");

  return -1;
}

/*
  startTimer starts the timer to generate SIGALRM events, or stops the timer
  if 'us' is 0. Enable a signal handler for SIGALRM before you call this.
*/
void startTimer(int us)
{
    timeout(us < 1000 ? 1 : us/1000);
}

/*
  idleHandler is called when no key is received after timeout and handles the
  reading of NML status, the update of the status window, and key-up simulation
  and handling
*/
static void idleHandler()
{
  // read NML status
  updateStatus();

  // only print if main is not printing, so we don't clobber in the middle
  if (! critFlag)
    {
      printStatus();
    }

  // simulate key-up event, as per comment below
  keyup_count -= usecs;
  if (keyup_count < 0)
    {
      keyup_count = 0;
      oldch = 0;
    }

  /*
    Key-up events are simulated as follows: each time a key is received,
    keyup_count is loaded. If it's a new key, FIRST_KEYUP_DELAY is
    loaded. If it's the same as the last key, NEXT_KEYUP_DELAY is loaded.
    This is to handle the different delay between the first and subsequent
    repeats.

    Each time through this handler, keyup_count is decremented. If it
    reaches 0, it means no key has been pressed before the delay expires,
    and we see this as a key-up event.

    If you have code that needs to respond to a key-up event, set a flag
    when you do your key-down stuff, and put the key-up code in here,
    like this:

    if (myFlag && keyup_count == 0)
      {
        // do stuff for key up here

        // and clear your flag
        myFlag = 0;
      }
   */

  // key up for jogs
  if (axisJogging != AXIS_NONE && keyup_count == 0)
    {
      emc_axis_abort_msg.axis = axisIndex(axisJogging);
      emc_axis_abort_msg.serial_number = ++emcCommandSerialNumber;
      emcCommandBuffer->write(emc_axis_abort_msg);
      emcCommandWait(emcCommandSerialNumber);
      axisJogging = AXIS_NONE;
    }

  // key up for spindle speed changes
  if (spindleChanging && keyup_count == 0)
    {
      emc_spindle_constant_msg.serial_number = ++emcCommandSerialNumber;
      emcCommandBuffer->write(emc_spindle_constant_msg);
      emcCommandWait(emcCommandSerialNumber);
      spindleChanging = 0;
    }
  return;
}

static int done = 0;
static void quit(int sig)
{
  // clean up curses windows
  delwin(progwin);
  progwin = 0;
  delwin(logwin);
  logwin = 0;
  delwin(toolwin);
  toolwin = 0;
  delwin(diagwin);
  diagwin = 0;
  delwin(helpwin);
  helpwin = 0;
  endwin();

  // clean up NML buffers

  if (emcErrorBuffer)
    {
      delete emcErrorBuffer;
      emcErrorBuffer = 0;
    }

  if (emcStatusBuffer)
    {
      delete emcStatusBuffer;
      emcStatusBuffer = 0;
      emcStatus = 0;
    }

  if (emcCommandBuffer)
    {
      delete emcCommandBuffer;
      emcCommandBuffer = 0;
    }

  // close program file
  if (programFp)
    {
      fclose(programFp);
      programFp = 0;
    }

  // close error log file
  if (errorFp)
    {
      fclose(errorFp);
      errorFp = NULL;
    }

  // reset signal handlers to default
  signal(SIGINT, SIG_DFL);

  exit(0);
}

static int emcTaskNmlGet()
{
  int retval = 0;

  // try to connect to EMC cmd
  if (emcCommandBuffer == 0)
    {
      emcCommandBuffer = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", emc_nmlfile);
      if (! emcCommandBuffer->valid())
        {
          rcs_print_error("emcCommand buffer not available\n");
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
          rcs_print_error("emcStatus buffer not available\n");
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
          rcs_print_error("emcError buffer not available\n");
          delete emcErrorBuffer;
          emcErrorBuffer = 0;
          retval = -1;
        }
    }

  return retval;
}

// string for ini file version num
static char version_string[LINELEN] = "";

// destructively converts string to its uppercase counterpart
static char *upcase(char *string)
{
  char *ptr = string;

  while (*ptr)
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
  int jogPol;

  // open it
  if (!inifile.Open(filename))
    {
      return -1;
    }

  if ((inistring = inifile.Find("MACHINE", "EMC")))
    {
      strcpy(machine, inistring);

      if ((inistring = inifile.Find("VERSION", "EMC")))
        {
          sscanf(inistring, "$Revision: %s", version);

          sprintf(version_string, "%s EMC Version %s", machine, version);
        }
    }

  if ((inistring = inifile.Find("MAX_VELOCITY", "TRAJ")))
    {
      if (1 != sscanf(inistring, "%lf", &traj_max_velocity))
        {
          traj_max_velocity = DEFAULT_TRAJ_MAX_VELOCITY;
        }
    }
  else
    {
      traj_max_velocity = DEFAULT_TRAJ_MAX_VELOCITY;
    }

  if ((inistring = inifile.Find("PROGRAM_PREFIX", "DISPLAY")))
    {
      if (1 != sscanf(inistring, "%s", programPrefix))
        {
          programPrefix[0] = 0;
        }
    }
  else
    {
      programPrefix[0] = 0;
    }

  if ((inistring = inifile.Find("POSITION_OFFSET", "DISPLAY")))
    {
      if (1 == sscanf(inistring, "%s", displayString))
        {
          upcase(displayString);
          if (! strcmp(displayString, "ABSOLUTE"))
            {
              coords = COORD_ABSOLUTE;
            }
          else if (! strcmp(displayString, "RELATIVE"))
            {
              coords = COORD_RELATIVE;
            }
          else
            {
              // error-- invalid value
              // ignore
            }
        }
      else
        {
          // error-- no value provided
          // ignore
        }
    }
  else
    {
      // no line at all
      // ignore
    }

  if ((inistring = inifile.Find("POSITION_FEEDBACK", "DISPLAY")))
    {
      if (1 == sscanf(inistring, "%s", displayString))
        {
          upcase(displayString);
          if (! strcmp(displayString, "ACTUAL"))
            {
              posDisplay = POS_DISPLAY_ACT;
            }
          else if (! strcmp(displayString, "COMMANDED"))
            {
              posDisplay = POS_DISPLAY_CMD;
            }
          else
            {
              // error-- invalid value
              // ignore
            }
        }
      else
        {
          // error-- no value provided
          // ignore
        }
    }
  else
    {
      // no line at all
      // ignore
    }

  xJogPol = 1;                  // set to default
  if ((inistring = inifile.Find("JOGGING_POLARITY", "AXIS_0")) &&
      1 == sscanf(inistring, "%d", &jogPol) &&
      jogPol == 0)
    {
      // it read as 0, so override default
      xJogPol = 0;
    }

  yJogPol = 1;                  // set to default
  if ((inistring = inifile.Find("JOGGING_POLARITY", "AXIS_1")) &&
      1 == sscanf(inistring, "%d", &jogPol) &&
      jogPol == 0)
    {
      // it read as 0, so override default
      yJogPol = 0;
    }

  zJogPol = 1;                  // set to default
  if ((inistring = inifile.Find("JOGGING_POLARITY", "AXIS_2")) &&
      1 == sscanf(inistring, "%d", &jogPol) &&
      jogPol == 0)
    {
      // it read as 0, so override default
      zJogPol = 0;
    }

  // close it
  inifile.Close();

  return 0;;
}

int tryNml()
{
    double end;
    int good;
#define RETRY_TIME 10.0		// seconds to wait for subsystems to come up
#define RETRY_INTERVAL 1.0	// seconds between wait tries for a subsystem

    if ((emc_debug & EMC_DEBUG_NML) == 0) {
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
    if ((emc_debug & EMC_DEBUG_NML) == 0) {
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

int main(int argc, char *argv[])
{
  int dump = 0;
  struct winsize size;
  int curx, cury;
  int t;
  int typing = 0;
#define TYPEBUFFERSIZE ASCLINELEN
  char typebuffer[TYPEBUFFERSIZE];
  char lastmdi[TYPEBUFFERSIZE] = "";
  int typeindex = 0;
  enum {IACT_NONE = 1, IACT_OPEN, IACT_MDI, IACT_LOAD_TOOL, IACT_OPEN_LOG,
        IACT_END} interactive = IACT_NONE;
  //char keystick[] = "keystick";
  int charHandled;

  // process command line args, indexing argv[] from [1]
  for (t = 1; t < argc; t++)
    {
      // try -dump
      if (!strcmp(argv[t], "-dump"))
        {
          dump = 1;
          continue;
        }

      // try -nml
      if (!strcmp(argv[t], "-nml"))
        {
          if (t == argc - 1)    // if last, can't read past it
            {
              printf("syntax: -nml <nmlfile>\n");
              exit(1);
            }
          else
            {
              strcpy(emc_nmlfile, argv[t+1]);
              t++;              // step over nmlfile
              continue;
            }
        }

      // try -ini
      if (!strcmp(argv[t], "-ini"))
        {
          if (t == argc - 1)
            {
              printf("syntax: -ini <inifile\n");
              exit(1);
            }
          else
            {
              strcpy(emc_inifile, argv[t+1]);
              t++;              // step over inifile
              continue;
            }
        }

      // try -noerror
      if (!strcmp(argv[t], "-noerror"))
        {
          catchErrors = 0;
          continue;
        }

      // try -usecs for cycle time in microseconds
      if (!strcmp(argv[t], "-usecs"))
        {
          if (t == argc - 1 ||
              1 != sscanf(argv[t + 1], "%d", &usecs) ||
              usecs <= 0 ||
              usecs >= 1000000)
            {
              printf("syntax: -usecs <1..999999 microsecond polling period>\n");
              exit(1);
            }
          else
            {
              t++;
              continue;
            }
        }

      // try -dur for delay until repeat
      if (!strcmp(argv[t], "-dur"))
        {
          if (t == argc - 1 ||
              1 != sscanf(argv[t + 1], "%d", &FIRST_KEYUP_DELAY) ||
              FIRST_KEYUP_DELAY < 0)
            {
              printf("syntax: -dur <usecs delay until first repeat>\n");
              exit(1);
            }
          else
            {
              t++;
              continue;
            }
        }

      // try -dbr for delay between repeats
      if (!strcmp(argv[t], "-dbr"))
        {
          if (t == argc - 1 ||
              1 != sscanf(argv[t + 1], "%d", &NEXT_KEYUP_DELAY) ||
              NEXT_KEYUP_DELAY < 0)
            {
              printf("syntax: -dbr <usecs delay between repeats>\n");
              exit(1);
            }
          else
            {
              t++;
              continue;
            }
        }

    }

  // read INI file
  iniLoad(emc_inifile);

  // trap SIGINT
  signal(SIGINT, quit);

#ifdef LOG
  errorFp = fopen(ERROR_FILE, "w");
  // failure here just disables logging
#endif

  // init NML
  if (! dump)
    {
        if(tryNml())
        {
          exit(1);
        }
    }

  // set up curses
  initscr();
  cbreak();
  noecho();
  nonl();
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE);
  helpwin = newwin(0, 0, 0, 0);
  diagwin = newwin(0, 0, 0, 0);
  toolwin = newwin(0, 0, 0, 0);
  logwin = newwin(0, 0, 0, 0);
  progwin = newwin(0, 0, 0, 0);
  window = stdscr;

  // fill in strings

  for (t = 0; t < ASCLINELEN; t++)
    {
      line_blank[t] = ' ';
    }
  line_blank[ASCLINELEN] = 0;

  for (t = 0; t < ASCLINELEN; t++)
    {
      bottom_string[t] = '-';
    }
  bottom_string[ASCLINELEN] = 0;
  t = (ASCLINELEN - strlen(version_string)) / 2;
  if (t >= 0)
    {
      memcpy(&bottom_string[t], version_string, strlen(version_string));
    }

  // get screen width and height
  wbegy = 0;
  wbegx = 0;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) < 0)
    {
      // use 80x24 as default
      wmaxy = 23;
      wmaxx = 79;
    }
  else
    {
      wmaxy = size.ws_row - 1;
      wmaxx = size.ws_col - 1;
    }

  // and set them here
  cury = wmaxy;
  curx = wbegx;
  wmove(window, cury, curx);
  wrefresh(window);

  // set up interval timer
  if (!dump)
    {
      startTimer(usecs);
    }

  while (! done)
    {
      oldch = ch;
      int keypress = getch();
      if(keypress == ERR) 
      {
        idleHandler(); 
        continue;
      }
      ch = (chtype) keypress;

      // check for ^C that may happen during blocking read
      if (done)
        {
          break;
        }

      if (dump)
        {
          mvwaddstr(window, wmaxy, wbegx, line_blank);
          sprintf(scratch_string, "%12o", (int) ch);
          mvwaddstr(window, wmaxy, wbegx, scratch_string);
          wmove(window, wmaxy, wbegx);
          wrefresh(window);

          if (ch == 'q')
            break;
          else
            continue;
        }

      if (ch != oldch)
        {
          keyup_count = FIRST_KEYUP_DELAY;
        }
      else
        {
          keyup_count = NEXT_KEYUP_DELAY;
        }

      // set up a first switch on ch, for those characters that
      // are to be looked at before the interactive typing string.
      // set flag signifying the char has been handled, which will
      // be reset in the default (not handled) case
      charHandled = 1;
      switch (ch)
        {
          /*
            To implement a "repeated key," whose actions are done
            during the first and every repeated key, do this:

            case MY_REPEATED_KEY:
            // do action
            // you need do nothing else
            break;

            To implement a "single key," whose actions are done
            once and not repeated if the key repeats, do this:

            case MY_SINGLE_KEY:
            if (oldch != ch)
            {
            // do action
            // you need do nothing else
            }
            break;

            The simulated key-up event and setting of oldch is done
            automatically elsewhere.
          */

        case ESC:               // task abort (abort everything)
          task_abort_msg.serial_number = ++emcCommandSerialNumber;
          emcCommandBuffer->write(task_abort_msg);
          emcCommandWait(emcCommandSerialNumber);
          break;

        case TAB:               // toggle highlight
	  if (window == diagwin)
            {
              // toggle dianostics highlight
              if (diagtab == DIAG_USECS)
                diagtab = DIAG_FIRST_KEYUP_DELAY;
              else if (diagtab == DIAG_FIRST_KEYUP_DELAY)
                diagtab = DIAG_NEXT_KEYUP_DELAY;
              else
                diagtab = DIAG_USECS;
            }
          break;

        case ALT('o'):          // open log
        case ALT('O'):
          if (oldch != ch)
            {
              typing = 1;
              typeindex = 0;
              interactive = IACT_OPEN_LOG;

              critFlag = 1;
              mvwaddstr(window, wmaxy, wbegx, line_blank);
              mvwaddstr(window, wmaxy, wbegx, "log file to open: ");
              wrefresh(window);
              critFlag = 0;
            }
          break;


        case CTL('L'):          // redraw current window
          clearWindow();
          printStatus();
          break;

        case KEY_F(1):          // toggle estop
          if (oldch != ch)
            {
              if (emcStatus->task.state == EMC_TASK_STATE_ESTOP)
                {
                  state_msg.state = EMC_TASK_STATE_ESTOP_RESET;
                }
              else
                {
                  state_msg.state = EMC_TASK_STATE_ESTOP;
                }
              state_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(state_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case KEY_F(2):          // toggle servos
          if (oldch != ch)
            {
              if (emcStatus->task.state == EMC_TASK_STATE_ESTOP_RESET)
                {
                  state_msg.state = EMC_TASK_STATE_ON;
                }
              else
                {
                  state_msg.state = EMC_TASK_STATE_OFF;
                }
              state_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(state_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case KEY_F(3):          // to into manual mode
          if (oldch != ch)
            {
              mode_msg.mode = EMC_TASK_MODE_MANUAL;
              mode_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(mode_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case KEY_F(4):          // go into auto mode
          if (oldch != ch)
            {
              mode_msg.mode = EMC_TASK_MODE_AUTO;
              mode_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(mode_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case KEY_F(5):          // go into mdi mode
          if (oldch != ch)
            {
              mode_msg.mode = EMC_TASK_MODE_MDI;
              mode_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(mode_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case KEY_F(6):          // reset interpreter
          if (oldch != ch)
            {
              task_plan_init_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(task_plan_init_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case KEY_F(7):          // toggle mist
          if (oldch != ch)
            {
              if (emcStatus->io.coolant.mist)
                {
                  emc_coolant_mist_off_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_coolant_mist_off_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
              else
                {
                  emc_coolant_mist_on_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_coolant_mist_on_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
            }
          break;

        case KEY_F(8):          // toggle flood
          if (oldch != ch)
            {
              if (emcStatus->io.coolant.flood)
                {
                  emc_coolant_flood_off_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_coolant_flood_off_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
              else
                {
                  emc_coolant_flood_on_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_coolant_flood_on_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
            }
          break;

        case KEY_F(9):          // toggle spindle forward/off
          if (oldch != ch)
            {
              if (emcStatus->motion.spindle.direction == 0)
                {
                  // it's off, so turn forward
                  emc_spindle_on_msg.speed = 1;
                  emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_spindle_on_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
              else
                {
                  // it's not off, so turn off
                  emc_spindle_off_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_spindle_off_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
            }
          break;

        case KEY_F(10):         // toggle spindle reverse/off
          if (oldch != ch)
            {
              if (emcStatus->motion.spindle.direction == 0)
                {
                  // it's off, so turn reverse
                  emc_spindle_on_msg.speed = -1;
                  emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_spindle_on_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
              else
                {
                  // it's not off, so turn off
                  emc_spindle_off_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_spindle_off_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
            }
          break;

        case KEY_F(11):         // spindle speed decrease
          if (oldch != ch)
            {
              // check for running first
              if (emcStatus->motion.spindle.direction == 0)
                break;

              emc_spindle_decrease_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_spindle_decrease_msg);
              emcCommandWait(emcCommandSerialNumber);
              spindleChanging = 1;
            }
          break;

        case KEY_F(12):         // spindle speed increase
          if (oldch != ch)
            {
              // check for running first
              if (emcStatus->motion.spindle.direction == 0)
                break;

              emc_spindle_increase_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_spindle_increase_msg);
              emcCommandWait(emcCommandSerialNumber);
              spindleChanging = 1;
            }
          break;

        case KEY_RIGHT:
          if (oldch != ch &&
              axisJogging == AXIS_NONE)
            {
              if (jogMode == JOG_INCREMENTAL)
                {
                  emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_incr_jog_msg.axis = axisIndex(AXIS_X);
                  if (xJogPol)
                    emc_axis_incr_jog_msg.vel = jogSpeed / 60.0;
                  else
                    emc_axis_incr_jog_msg.vel = - jogSpeed / 60.0;
                  emc_axis_incr_jog_msg.incr = jogIncrement;
                  emcCommandBuffer->write(emc_axis_incr_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  // don't set axisJogging, since key up will abort
                }
              else
                {
                  jogMode = JOG_CONTINUOUS;
                  emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_jog_msg.axis = axisIndex(AXIS_X);
                  if (xJogPol)
                    emc_axis_jog_msg.vel = jogSpeed / 60.0;
                  else
                    emc_axis_jog_msg.vel = - jogSpeed / 60.0;
                  emcCommandBuffer->write(emc_axis_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  axisJogging = AXIS_X;
                }
              axisSelected = AXIS_X;
            }
          break;

        case KEY_LEFT:
          if (oldch != ch &&
              axisJogging == AXIS_NONE)
            {
              if (jogMode == JOG_INCREMENTAL)
                {
                  emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_incr_jog_msg.axis = axisIndex(AXIS_X);
                  if (xJogPol)
                    emc_axis_incr_jog_msg.vel = - jogSpeed / 60.0;
                  else
                    emc_axis_incr_jog_msg.vel = jogSpeed / 60.0;
                  emc_axis_incr_jog_msg.incr = jogIncrement;
                  emcCommandBuffer->write(emc_axis_incr_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  // don't set axisJogging, since key up will abort
                }
              else
                {
                  jogMode = JOG_CONTINUOUS;
                  emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_jog_msg.axis = axisIndex(AXIS_X);
                  if (xJogPol)
                    emc_axis_jog_msg.vel = - jogSpeed / 60.0;
                  else
                    emc_axis_jog_msg.vel = jogSpeed / 60.0;
                  emcCommandBuffer->write(emc_axis_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  axisJogging = AXIS_X;
                }
              axisSelected = AXIS_X;
            }
          break;

        case KEY_UP:
          if (oldch != ch &&
              axisJogging == AXIS_NONE)
            {
              if (jogMode == JOG_INCREMENTAL)
                {
                  emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_incr_jog_msg.axis = axisIndex(AXIS_Y);
                  if (yJogPol)
                    emc_axis_incr_jog_msg.vel = jogSpeed / 60.0;
                  else
                    emc_axis_incr_jog_msg.vel = - jogSpeed / 60.0;
                  emc_axis_incr_jog_msg.incr = jogIncrement;
                  emcCommandBuffer->write(emc_axis_incr_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  // don't set axisJogging, since key up will abort
                }
              else
                {
                  jogMode = JOG_CONTINUOUS;
                  emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_jog_msg.axis = axisIndex(AXIS_Y);
                  if (yJogPol)
                    emc_axis_jog_msg.vel = jogSpeed / 60.0;
                  else
                    emc_axis_jog_msg.vel = - jogSpeed / 60.0;
                  emcCommandBuffer->write(emc_axis_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  axisJogging = AXIS_Y;
                }
              axisSelected = AXIS_Y;
            }
          break;

        case KEY_DOWN:
          if (oldch != ch &&
              axisJogging == AXIS_NONE)
            {
              if (jogMode == JOG_INCREMENTAL)
                {
                  emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_incr_jog_msg.axis = axisIndex(AXIS_Y);
                  if (yJogPol)
                    emc_axis_incr_jog_msg.vel = - jogSpeed / 60.0;
                  else
                    emc_axis_incr_jog_msg.vel = jogSpeed / 60.0;
                  emc_axis_incr_jog_msg.incr = jogIncrement;
                  emcCommandBuffer->write(emc_axis_incr_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  // don't set axisJogging, since key up will abort
                }
              else
                {
                  jogMode = JOG_CONTINUOUS;
                  emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_jog_msg.axis = axisIndex(AXIS_Y);
                  if (yJogPol)
                    emc_axis_jog_msg.vel = - jogSpeed / 60.0;
                  else
                    emc_axis_jog_msg.vel = jogSpeed / 60.0;
                  emcCommandBuffer->write(emc_axis_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  axisJogging = AXIS_Y;
                }
              axisSelected = AXIS_Y;
            }
          break;

        case KEY_PPAGE:
          if (oldch != ch &&
              axisJogging == AXIS_NONE)
            {
              if (jogMode == JOG_INCREMENTAL)
                {
                  emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_incr_jog_msg.axis = axisIndex(AXIS_Z);
                  if (zJogPol)
                    emc_axis_incr_jog_msg.vel = jogSpeed / 60.0;
                  else
                    emc_axis_incr_jog_msg.vel = - jogSpeed / 60.0;
                  emc_axis_incr_jog_msg.incr = jogIncrement;
                  emcCommandBuffer->write(emc_axis_incr_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  // don't set axisJogging, since key up will abort
                }
              else
                {
                  jogMode = JOG_CONTINUOUS;
                  emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_jog_msg.axis = axisIndex(AXIS_Z);
                  if (zJogPol)
                    emc_axis_jog_msg.vel = jogSpeed / 60.0;
                  else
                    emc_axis_jog_msg.vel = - jogSpeed / 60.0;
                  emcCommandBuffer->write(emc_axis_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  axisJogging = AXIS_Z;
                }
              axisSelected = AXIS_Z;
            }
          break;

        case KEY_NPAGE:
          if (oldch != ch &&
              axisJogging == AXIS_NONE)
            {
              if (jogMode == JOG_INCREMENTAL)
                {
                  emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_incr_jog_msg.axis = axisIndex(AXIS_Z);
                  if (zJogPol)
                    emc_axis_incr_jog_msg.vel = - jogSpeed / 60.0;
                  else
                    emc_axis_incr_jog_msg.vel = jogSpeed / 60.0;
                  emc_axis_incr_jog_msg.incr = jogIncrement;
                  emcCommandBuffer->write(emc_axis_incr_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  // don't set axisJogging, since key up will abort
                }
              else
                {
                  jogMode = JOG_CONTINUOUS;
                  emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
                  emc_axis_jog_msg.axis = axisIndex(AXIS_Z);
                  if (zJogPol)
                    emc_axis_jog_msg.vel = - jogSpeed / 60.0;
                  else
                    emc_axis_jog_msg.vel = jogSpeed / 60.0;
                  emcCommandBuffer->write(emc_axis_jog_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  axisJogging = AXIS_Z;
                }
              axisSelected = AXIS_Z;
            }
          break;

        case KEY_HOME:          // home selected axis
          if (oldch != ch)
            {
              emc_axis_home_msg.axis = axisIndex(axisSelected);
              emc_axis_home_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_axis_home_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case KEY_END:           // end this program
          if (oldch != ch)
            {
              typing = 1;
              typeindex = 0;
              interactive = IACT_END;

              critFlag = 1;
              mvwaddstr(window, wmaxy, wbegx, line_blank);
              mvwaddstr(window, wmaxy, wbegx, "really quit? (y/n): ");
              wrefresh(window);
              critFlag = 0;
            }
          break;

        default:
          charHandled = 0;
          break;
        } // end of first switch (ch)

      // now handle interactive typing, since immediate chars have
      // just been handled

      if (typing &&
          ! charHandled)
        {
          if (ch == RETURN)
            {
              typing = 0;
              typebuffer[typeindex] = 0;
              typeindex = 0;

              critFlag = 1;
              mvwaddstr(window, wmaxy, wbegx, line_blank);
              wmove(window, wmaxy, wbegx);
              wrefresh(window);
              critFlag = 0;

              // now handle typed string
              switch (interactive)
                {
                case IACT_OPEN:
                  strcpy(task_plan_open_msg.file, typebuffer);
                  task_plan_open_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(task_plan_open_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  strcpy(programFile, task_plan_open_msg.file);
                  programOpened = 1;
                  if (programFp)
                    {
                      fclose(programFp);
                    }
                  programFp = fopen(programFile, "r");
                  programLineText[0] = 0;
                  programFpLine = 0;
                  interactive = IACT_NONE;
                  break;

                case IACT_MDI:
                  if (typebuffer[0] == 0)
                    break;      // ignore blank lines
                  strcpy(lastmdi, typebuffer); // save it
                  strcpy(task_plan_execute_msg.command, typebuffer);
                  task_plan_execute_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(task_plan_execute_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  interactive = IACT_NONE;
                  break;

                case IACT_LOAD_TOOL:
                  strcpy(emc_tool_load_tool_table_msg.file, typebuffer);
                  emc_tool_load_tool_table_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(emc_tool_load_tool_table_msg);
                  emcCommandWait(emcCommandSerialNumber);
                  interactive = IACT_NONE;
                  break;

                case IACT_END:
                  if (typebuffer[0] == 'y' ||
                      typebuffer[0] == 'Y')
                    {
                      done = 1;
                    }

                  interactive = IACT_NONE;
                  break;

                default:        // ignore
                  break;
                }
            }
          else if (ch == KEY_BACKSPACE || // try all the backspace chars
                   ch == 8 ||
                   ch == 127)
            {
              if (typeindex == 0)
                {
                  // we're at first spot-- ignore
                  continue;
                }
              --typeindex;
              critFlag = 1;
              getyx(window, cury, curx);
              --curx;
              mvwaddch(window, cury, curx, ' ');
              wmove(window, cury, curx);
              wrefresh(window);
              critFlag = 0;
            }
          else
            {
              if (typeindex >= TYPEBUFFERSIZE - 1)
                {
                  // we're at last spot, which we need for null-- ignore
                  continue;
                }
              typebuffer[typeindex++] = ch;
              critFlag = 1;
              waddch(window, ch);
              wrefresh(window);
              critFlag = 0;
            }

          // continue with while (! done), so we skip over the normal
          // handling of alphanumeric chars
          continue;

        } // end of if (typing)

      // set up a second switch on ch, for those alphanumeric chars that
      // can be part of an interactive string
      switch (ch)
        {
        case RETURN:
          if (oldch != ch)
            {
              // clear all error buffers
              critFlag = 1;
              mvwaddstr(stdscr, ERR_Y, ERR_X, line_blank);
              mvwaddstr(helpwin, ERR_Y, ERR_X, line_blank);
              mvwaddstr(diagwin, ERR_Y, ERR_X, line_blank);
              mvwaddstr(toolwin, ERR_Y, ERR_X, line_blank);
              mvwaddstr(logwin, ERR_Y, ERR_X, line_blank);
              mvwaddstr(progwin, ERR_Y, ERR_X, line_blank);
              wmove(window, wmaxy, wbegx);
              wrefresh(window);
              critFlag = 0;
            }
          break;

        case '?':               // toggle help screen
          critFlag = 1;
          if (window == stdscr)
            {
              window = helpwin;
            }
          else if (window == helpwin)
            {
              window = toolwin;
            }
          else if (window == toolwin)
            {
              window = logwin;
            }
          else if (window == logwin)
            {
              window = diagwin;
            }
          else if (window == diagwin)
            {
              window = progwin;
            }
          else if (window == progwin)
            {
              window = stdscr;
            }
          else
            {
              window = stdscr;
            }
          critFlag = 0;
          clearWindow();
          printStatus();
          break;

        case '#':               // toggle abs/rel mode
          if (oldch != ch)
            {
              switch (coords)
                {
                case COORD_RELATIVE:
                  coords = COORD_ABSOLUTE;
                  break;
                case COORD_ABSOLUTE:
                  coords = COORD_RELATIVE;
                  break;
                default:
                  coords = COORD_RELATIVE;
                  break;
                }
            }
          break;

        case '@':               // toggle cmd/act mode
          if (oldch != ch)
            {
              switch (posDisplay)
                {
                case POS_DISPLAY_ACT:
                  posDisplay = POS_DISPLAY_CMD;
                  break;
                case POS_DISPLAY_CMD:
                  posDisplay = POS_DISPLAY_ACT;
                  break;
                default:
                  posDisplay = POS_DISPLAY_ACT;
                  break;
                }
            }
          break;

        case 'x':
        case 'X':
          if (oldch != ch)
            {
              if (emcStatus->task.mode == EMC_TASK_MODE_MDI)
                {
                  typing = 1;
                  typeindex = 1;
                  typebuffer[0] = ch;
                  typebuffer[1] = 0;
                  interactive = IACT_MDI;

                  critFlag = 1;
                  mvwaddstr(window, wmaxy, wbegx, line_blank);
                  mvwaddstr(window, wmaxy, wbegx, "mdi command: ");
                  waddch(window, ch);
                  wrefresh(window);
                  critFlag = 0;
                }
              else
                {
                  axisSelected = AXIS_X;
                }
            }
          break;

        case 'y':
        case 'Y':
          if (oldch != ch)
            {
              if (emcStatus->task.mode == EMC_TASK_MODE_MDI)
                {
                  typing = 1;
                  typeindex = 1;
                  typebuffer[0] = ch;
                  typebuffer[1] = 0;
                  interactive = IACT_MDI;

                  critFlag = 1;
                  mvwaddstr(window, wmaxy, wbegx, line_blank);
                  mvwaddstr(window, wmaxy, wbegx, "mdi command: ");
                  waddch(window, ch);
                  wrefresh(window);
                  critFlag = 0;
                }
              else
                {
                  axisSelected = AXIS_Y;
                }
            }
          break;

        case 'z':
        case 'Z':
          if (oldch != ch)
            {
              if (emcStatus->task.mode == EMC_TASK_MODE_MDI)
                {
                  typing = 1;
                  typeindex = 1;
                  typebuffer[0] = ch;
                  typebuffer[1] = 0;
                  interactive = IACT_MDI;

                  critFlag = 1;
                  mvwaddstr(window, wmaxy, wbegx, line_blank);
                  mvwaddstr(window, wmaxy, wbegx, "mdi command: ");
                  waddch(window, ch);
                  wrefresh(window);
                  critFlag = 0;
                }
              else
                {
                  axisSelected = AXIS_Z;
                }
            }
          break;

        case 'i':               // incremental jog toggle
        case 'I':
          if (jogMode == JOG_INCREMENTAL)
            {
              jogIncrement *= 10.0;
              if (jogIncrement >= 0.9)
                {
                  jogIncrement = 0.0001;
                }
            }
          jogMode = JOG_INCREMENTAL;
          break;

        case 'c':               // continuous jog
        case 'C':
          if (oldch != ch)
            {
              jogMode = JOG_CONTINUOUS;
            }
          break;

        case '<':
        case ',':
	  if (window == diagwin)
            {
              if (diagtab == DIAG_USECS)
                {
                  usecs -= 10000;
                  if (usecs < 10000)
                    usecs = 10000;

                  startTimer(usecs);
                }
              else if (diagtab == DIAG_FIRST_KEYUP_DELAY)
                {
                  FIRST_KEYUP_DELAY -= 10000;
                  if (FIRST_KEYUP_DELAY <= 0)
                    FIRST_KEYUP_DELAY = 0;
                }
              else
                {
                  NEXT_KEYUP_DELAY -= 10000;
                  if (NEXT_KEYUP_DELAY <= 0)
                    NEXT_KEYUP_DELAY = 0;
                }
            }
          else
            {
              jogSpeed -= 1;
              if (jogSpeed < 1)
                jogSpeed = 1;
            }
          break;

        case '>':
        case '.':
	  if (window == diagwin)
            {
              if (diagtab == DIAG_USECS)
                {
                  usecs += 10000;
                  if (usecs > 900000)
                    usecs = 900000;

                  startTimer(usecs);
                }
              else if (diagtab == DIAG_FIRST_KEYUP_DELAY)
                {
                  FIRST_KEYUP_DELAY += 10000;
                }
              else
                {
                  NEXT_KEYUP_DELAY += 10000;
                }
            }
          else
            {
              jogSpeed += 1;
              if (jogSpeed > traj_max_velocity * 60.0)
                {
                  jogSpeed = traj_max_velocity * 60.0;
                }
            }
          break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          // feed override
          if (oldch != ch)
            {
              if (ch == '0')
                {
                  emc_traj_set_scale_msg.scale = 1.0;
                }
              else
                {
                  emc_traj_set_scale_msg.scale = double (ch - '1' + 1) / 10.0;
                }
              emc_traj_set_scale_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_traj_set_scale_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case 'b':               // spindle brake off
          if (oldch != ch)
            {
              emc_spindle_brake_release_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_spindle_brake_release_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case 'B':               // spindle brake on
          if (oldch != ch)
            {
              emc_spindle_brake_engage_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_spindle_brake_engage_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case 'o':
        case 'O':
          if (oldch != ch)
            {
              if (emcStatus->task.mode != EMC_TASK_MODE_AUTO)
                break;

              typing = 1;
              strcpy(typebuffer, programPrefix);
              typeindex = strlen(programPrefix);
              interactive = IACT_OPEN;

              critFlag = 1;
              mvwaddstr(window, wmaxy, wbegx, line_blank);
              mvwaddstr(window, wmaxy, wbegx, "program to open: ");
              waddstr(window, typebuffer);
              wrefresh(window);
              critFlag = 0;
            }
          break;

        case 'r':
        case 'R':
          if (oldch != ch)
            {
              if (emcStatus->task.mode != EMC_TASK_MODE_AUTO)
                break;

              if (! programOpened)
                {
                  // send a request to open the last one
                  strcpy(task_plan_open_msg.file, programFile);
                  task_plan_open_msg.serial_number = ++emcCommandSerialNumber;
                  emcCommandBuffer->write(task_plan_open_msg);
                  emcCommandWait(emcCommandSerialNumber);
                }
              task_plan_run_msg.serial_number = ++emcCommandSerialNumber;
              task_plan_run_msg.line = 0;
              emcCommandBuffer->write(task_plan_run_msg);
              emcCommandWait(emcCommandSerialNumber);
              programOpened = 0;
            }
          break;

        case 'p':
        case 'P':
          if (oldch != ch)
            {
              task_plan_pause_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(task_plan_pause_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case 's':
        case 'S':
          if (oldch != ch)
            {
              task_plan_resume_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(task_plan_resume_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case 'a':
        case 'A':
          if (oldch != ch)
            {
              task_plan_step_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(task_plan_step_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case 'u':
          if (oldch != ch)
            {
              emc_lube_off_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_lube_off_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case 'U':
          if (oldch != ch)
            {
              emc_lube_on_msg.serial_number = ++emcCommandSerialNumber;
              emcCommandBuffer->write(emc_lube_on_msg);
              emcCommandWait(emcCommandSerialNumber);
            }
          break;

        case '\'':
        case '\"':
          if (oldch != ch)
            {
              if (emcStatus->task.mode != EMC_TASK_MODE_MDI)
                break;

              typing = 1;
              interactive = IACT_MDI;

              // stuff last command
              strcpy(typebuffer, lastmdi);
              typeindex = strlen(typebuffer);

              critFlag = 1;
              mvwaddstr(window, wmaxy, wbegx, line_blank);
              mvwaddstr(window, wmaxy, wbegx, "mdi command: ");
              waddstr(window, typebuffer);
              wrefresh(window);
              critFlag = 0;
            }
          break;

        case 'm':
        case 'M':
        case 'g':
        case 'G':
          if (oldch != ch)
            {
              if (emcStatus->task.mode != EMC_TASK_MODE_MDI)
                break;

              typing = 1;
              typeindex = 1;
              typebuffer[0] = ch;
              typebuffer[1] = 0;
              interactive = IACT_MDI;

              critFlag = 1;
              mvwaddstr(window, wmaxy, wbegx, line_blank);
              mvwaddstr(window, wmaxy, wbegx, "mdi command: ");
              waddch(window, ch);
              wrefresh(window);
              critFlag = 0;
            }
          break;

        case 'l':
        case 'L':
          if (oldch != ch)
            {
              typing = 1;
              typeindex = 0;
              interactive = IACT_LOAD_TOOL;

              critFlag = 1;
              mvwaddstr(window, wmaxy, wbegx, line_blank);
              mvwaddstr(window, wmaxy, wbegx, "tool file to load: ");
              wrefresh(window);
              critFlag = 0;
            }
          break;

        default:
          break;
        } // end of second switch (ch)

    } // end of while (! done)

  // call ^C signal handler directly to terminate cleanly
  quit(0);

  return 0;
}
