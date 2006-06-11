/********************************************************************
* Description: emctask.cc
*   Mode and state management for EMC_TASK class
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
********************************************************************/

#include <stdlib.h>
#include <string.h>		// strncpy()
#include <sys/stat.h>		// struct stat
#include <unistd.h>		// stat()

#include "rcs.hh"		// INIFILE
#include "emc.hh"		// EMC NML
#include "emcglb.h"		// EMC_INIFILE
#include "interpl.hh"		// NML_INTERP_LIST, interp_list
#include "canon.hh"		// CANON_VECTOR, GET_PROGRAM_ORIGIN()
#include "rs274ngc.hh"		// the interpreter
#include "interp_return.hh"	// INTERP_FILE_NOT_OPEN

/* flag for how we want to interpret traj coord mode, as mdi or auto */
static int mdiOrAuto = EMC_TASK_MODE_AUTO;

Interp interp;

// EMC_TASK interface

/*
  format string for user-defined programs, e.g., "programs/M1%02d" means
  user-defined programs are in the programs/ directory and are named
  M1XX, where XX is a two-digit string.
*/

static char user_defined_fmt[EMC_SYSTEM_CMD_LEN] = "nc_files/M1%02d";

static void user_defined_add_m_code(int num, double arg1, double arg2)
{
    char fmt[EMC_SYSTEM_CMD_LEN];
    EMC_SYSTEM_CMD system_cmd;

    strcpy(fmt, user_defined_fmt);
    strcat(fmt, " %f %f");
    sprintf(system_cmd.string, fmt, num, arg1, arg2);
    interp_list.append(system_cmd);
}

int emcTaskInit()
{
    int index;
    char path[EMC_SYSTEM_CMD_LEN];
    struct stat buf;
    Inifile inifile;
    const char *inistring;

    // read out directory where programs are located
    inifile.open(EMC_INIFILE);
    inistring = inifile.find("PROGRAM_PREFIX", "DISPLAY");
    inifile.close();

    // if we have a program prefix, override the default user_defined_fmt
    // string with program prefix, then "M1%02d", e.g.
    // nc_files/M101, where the %%02d means 2 digits after the M code
    // and we need two % to get the literal %
    if (NULL != inistring) {
	sprintf(user_defined_fmt, "%s/M1%%02d", inistring);
    }

    /* check for programs named programs/M100 .. programs/M199 and add
       any to the user defined functions list */
    for (index = 0; index < USER_DEFINED_FUNCTION_NUM; index++) {
	sprintf(path, user_defined_fmt, index);
	if (0 == stat(path, &buf)) {
	    if (buf.st_mode & S_IXUSR) {
		USER_DEFINED_FUNCTION_ADD(user_defined_add_m_code, index);
		if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
		    rcs_print
			("emcTaskInit: adding user-defined function %s\n",
			 path);
		}
	    } else {
		if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
		    rcs_print
			("emcTaskInit: user-defined function %s found, but not executable, so ignoring\n",
			 path);
		}
	    }
	}
    }

    return 0;
}

int emcTaskHalt()
{
    return 0;
}

int emcTaskAbort()
{
    emcMotionAbort();
    emcIoAbort();

    return 0;
}

int emcTaskSetMode(int mode)
{
    int retval = 0;

    switch (mode) {
    case EMC_TASK_MODE_MANUAL:
	// go to manual mode
	emcTrajSetMode(EMC_TRAJ_MODE_FREE);
	mdiOrAuto = EMC_TASK_MODE_AUTO;	// we'll default back to here
	break;

    case EMC_TASK_MODE_MDI:
	// go to mdi mode
	emcTrajSetMode(EMC_TRAJ_MODE_COORD);
	emcTaskPlanSynch();
	mdiOrAuto = EMC_TASK_MODE_MDI;
	break;

    case EMC_TASK_MODE_AUTO:
	// go to auto mode
	emcTrajSetMode(EMC_TRAJ_MODE_COORD);
	emcTaskPlanSynch();
	mdiOrAuto = EMC_TASK_MODE_AUTO;
	break;

    default:
	retval = -1;
	break;
    }

    return retval;
}

int emcTaskSetState(int state)
{
    int t;
    int retval = 0;

    switch (state) {
    case EMC_TASK_STATE_OFF:
	// turn the machine servos off-- go into READY state
	for (t = 0; t < emcStatus->motion.traj.axes; t++) {
	    emcAxisDisable(t);
	}
	emcTrajDisable();
	emcLubeOff();
	break;

    case EMC_TASK_STATE_ON:
	// turn the machine servos on
	emcTrajEnable();
	for (t = 0; t < emcStatus->motion.traj.axes; t++) {
	    emcAxisEnable(t);
	}
	emcLubeOn();
	break;

    case EMC_TASK_STATE_ESTOP_RESET:
	// reset the estop
	emcAuxEstopOff();
	emcLubeOff();
	break;

    case EMC_TASK_STATE_ESTOP:
	// go into estop-- do both IO estop and machine servos off
	emcAuxEstopOn();
	for (t = 0; t < emcStatus->motion.traj.axes; t++) {
	    emcAxisDisable(t);
	}
	emcTrajDisable();
	emcLubeOff();
	break;

    default:
	retval = -1;
	break;
    }

    return retval;
}

// WM access functions

/*
  determineMode()

  Looks at mode of subsystems, and returns associated mode

  Depends on traj mode, and mdiOrAuto flag

  traj mode   mdiOrAuto     mode
  ---------   ---------     ----
  FREE        XXX           MANUAL
  COORD       MDI           MDI
  COORD       AUTO          AUTO
  */
static int determineMode()
{
    // if traj is in free mode, then we're in manual mode
    if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE_FREE ||
	emcStatus->motion.traj.mode == EMC_TRAJ_MODE_TELEOP) {
	return EMC_TASK_MODE_MANUAL;
    }
    // else traj is in coord mode-- we can be in either mdi or auto
    return mdiOrAuto;
}

/*
  determineState()

  Looks at state of subsystems, and returns associated state

  Depends on traj enabled, io estop, and desired task state

  traj enabled   io estop      state
  ------------   --------      -----
  DISABLED       ESTOP         ESTOP
  ENABLED        ESTOP         ESTOP
  DISABLED       OUT OF ESTOP  ESTOP_RESET
  ENABLED        OUT OF ESTOP  ON
  */
static int determineState()
{
    if (emcStatus->io.aux.estop) {
	return EMC_TASK_STATE_ESTOP;
    }

    if (!emcStatus->motion.traj.enabled) {
	return EMC_TASK_STATE_ESTOP_RESET;
    }

    return EMC_TASK_STATE_ON;
}

static int waitFlag = 0;

static char interp_error_text_buf[LINELEN];
static char interp_stack_buf[LINELEN];

static void print_interp_error(int retval)
{
    int index = 0;
    if (retval == 0) {
	return;
    }

    if (0 != emcStatus) {
	emcStatus->task.interpreter_errcode = retval;
    }

    interp_error_text_buf[0] = 0;
    interp.error_text(retval, interp_error_text_buf, LINELEN);
    if (0 != interp_error_text_buf[0]) {
	rcs_print_error("interp_error: %s\n", interp_error_text_buf);
    }
    emcOperatorError(0, interp_error_text_buf);
    index = 0;
    if (EMC_DEBUG & EMC_DEBUG_INTERP) {
	rcs_print("Interpreter stack: \t");
	while (index < 5) {
	    interp_stack_buf[0] = 0;
	    interp.stack_name(index, interp_stack_buf, LINELEN);
	    if (0 == interp_stack_buf[0]) {
		break;
	    }
	    rcs_print(" - %s ", interp_stack_buf);
	    index++;
	}
	rcs_print("\n");
    }
}

int emcTaskPlanInit()
{
    interp.ini_load(EMC_INIFILE);
    waitFlag = 0;

    int retval = interp.init();
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    } else {
	if (0 != RS274NGC_STARTUP_CODE[0]) {
	    retval = interp.execute(RS274NGC_STARTUP_CODE);
	    if (retval > INTERP_MIN_ERROR) {
		print_interp_error(retval);
	    }
	}
    }
    return retval;
}

int emcTaskPlanSetWait()
{
    waitFlag = 1;

    return 0;
}

int emcTaskPlanIsWait()
{
    return waitFlag;
}

int emcTaskPlanClearWait()
{
    waitFlag = 0;

    return 0;
}

int emcTaskPlanSynch()
{
    return interp.synch();
}

int emcTaskPlanExit()
{
    return interp.exit();
}

int emcTaskPlanOpen(const char *file)
{
    if (emcStatus != 0) {
	emcStatus->task.motionLine = 0;
	emcStatus->task.currentLine = 0;
	emcStatus->task.readLine = 0;
    }

    int retval = interp.open(file);
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
	return retval;
    }
    taskplanopen = 1;
    return retval;
}

int emcTaskPlanRead()
{
    int retval = interp.read();
    if (retval == INTERP_FILE_NOT_OPEN) {
	if (emcStatus->task.file[0] != 0) {
	    retval = interp.open(emcStatus->task.file);
	    if (retval > INTERP_MIN_ERROR) {
		print_interp_error(retval);
	    }
	    retval = interp.read();
	}
    }
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }
    return retval;
}

int emcTaskPlanExecute(const char *command)
{
    int inpos = emcStatus->motion.traj.inpos;	// 1 if in position, 0 if not.

    if (command != 0) {		// Command is 0 if in AUTO mode, non-null if in MDI mode.
	// Don't sync if not in position.
	if ((*command != 0) && (inpos)) {
	    interp.synch();
	}
    }
    int retval = interp.execute(command);
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }
    if(command != 0) {
	FINISH();
    }
    return retval;
}

int emcTaskPlanClose()
{
    int retval = interp.close();
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }

    taskplanopen = 0;
    return retval;
}

int emcTaskPlanLine()
{
    return interp.line();
}

int emcTaskPlanCommand(char *cmd)
{
    char buf[LINELEN];

    strcpy(cmd, interp.command(buf, LINELEN));
    return 0;
}

int emcTaskUpdate(EMC_TASK_STAT * stat)
{
    stat->mode = (enum EMC_TASK_MODE_ENUM) determineMode();
    stat->state = (enum EMC_TASK_STATE_ENUM) determineState();

    // execState set in main
    // interpState set in main
    if (emcStatus->motion.traj.id > 0) {
	stat->motionLine = emcStatus->motion.traj.id;
    }
    // currentLine set in main
    // readLine set in main

    char buf[LINELEN];
    strcpy(stat->file, interp.file(buf, LINELEN));
    // command set in main

    // update active G and M codes
    interp.active_g_codes(&stat->activeGCodes[0]);
    interp.active_m_codes(&stat->activeMCodes[0]);
    interp.active_settings(&stat->activeSettings[0]);

    stat->heartbeat++;

    return 0;
}

