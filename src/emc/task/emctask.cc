/*
  emctask.cc

  Mode and state management for EMC_TASK class

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

#include <stdlib.h>
#include <string.h>		// strncpy()

#include "emc.hh"		// EMC NML
#include "emcglb.h"		// EMC_INIFILE
#include "interpl.hh"		// NML_INTERP_LIST, interp_list
#include "canon.hh"		// CANON_VECTOR, GET_PROGRAM_ORIGIN()
#include "rs274ngc.hh"		// the interpreter
#include "rs274ngc_return.hh"	// NCE_FILE_NOT_OPEN

// ident tag
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) ident[] =
    "$Id$";

/* flag for how we want to interpret traj coord mode, as mdi or auto */
static int mdiOrAuto = EMC_TASK_MODE_AUTO;

// EMC_TASK interface

int emcTaskInit()
{
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
	// turn the machine servos off-- go into ESTOP_RESET state
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
	if (emcStatus->io.aux.estopIn) {
	    rcs_print
		("Can't come out of estop while the estop button is in.");
	}
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

static char rs274ngc_error_text_buf[256];
static char rs274ngc_stack_buf[256];

static void print_rs274ngc_error(int retval)
{
    int index = 0;
    if (retval == 0) {
	return;
    }

    if (0 != emcStatus) {
	emcStatus->task.interpreter_errcode = retval;
    }

    rs274ngc_error_text_buf[0] = 0;
    rs274ngc_error_text(retval, rs274ngc_error_text_buf, 256);
    if (0 != rs274ngc_error_text_buf[0]) {
	rcs_print_error("rs274ngc_error: %s\n", rs274ngc_error_text_buf);
    }
    emcOperatorError(0, rs274ngc_error_text_buf);
    index = 0;
    if (EMC_DEBUG & EMC_DEBUG_INTERP) {
	rcs_print("rs274ngc_stack: \t");
	while (index < 5) {
	    rs274ngc_stack_buf[0] = 0;
	    rs274ngc_stack_name(index, rs274ngc_stack_buf, 256);
	    if (0 == rs274ngc_stack_buf[0]) {
		break;
	    }
	    rcs_print(" - %s ", rs274ngc_stack_buf);
	    index++;
	}
	rcs_print("\n");
    }
}

int emcTaskPlanInit()
{
    rs274ngc_ini_load(EMC_INIFILE);
    waitFlag = 0;

    int retval = rs274ngc_init();
    if (retval > RS274NGC_MIN_ERROR) {
	print_rs274ngc_error(retval);
    } else {
	if (0 != RS274NGC_STARTUP_CODE[0]) {
	    retval = rs274ngc_execute(RS274NGC_STARTUP_CODE);
	    if (retval > RS274NGC_MIN_ERROR) {
		print_rs274ngc_error(retval);
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
    return rs274ngc_synch();
}

int emcTaskPlanExit()
{
    return rs274ngc_exit();
}

int emcTaskPlanOpen(const char *file)
{
    if (emcStatus != 0) {
	emcStatus->task.motionLine = 0;
	emcStatus->task.currentLine = 0;
	emcStatus->task.readLine = 0;
    }

    int retval = rs274ngc_open(file);
    if (retval > RS274NGC_MIN_ERROR) {
	print_rs274ngc_error(retval);
	return retval;
    }
    taskplanopen = 1;
    return retval;
}

int emcTaskPlanRead()
{
    int retval = rs274ngc_read();
    if (retval == NCE_FILE_NOT_OPEN) {
	if (emcStatus->task.file[0] != 0) {
	    retval = rs274ngc_open(emcStatus->task.file);
	    if (retval > RS274NGC_MIN_ERROR) {
		print_rs274ngc_error(retval);
	    }
	    retval = rs274ngc_read();
	}
    }
    if (retval > RS274NGC_MIN_ERROR) {
	print_rs274ngc_error(retval);
    }
    return retval;
}

int emcTaskPlanExecute(const char *command)
{
    if (command != 0) {
	if (*command != 0) {
	    rs274ngc_synch();
	}
    }
    int retval = rs274ngc_execute(command);
    if (retval > RS274NGC_MIN_ERROR) {
	print_rs274ngc_error(retval);
    }
    return retval;
}

int emcTaskPlanClose()
{
    int retval = rs274ngc_close();
    if (retval > RS274NGC_MIN_ERROR) {
	print_rs274ngc_error(retval);
    }

    taskplanopen = 0;
    return retval;
}

int emcTaskPlanLine()
{
    return rs274ngc_line();
}

int emcTaskPlanCommand(char *cmd)
{
    strcpy(cmd, rs274ngc_command());
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

    strcpy(stat->file, rs274ngc_file());
    // command set in main

    // update active G and M codes
    rs274ngc_active_g_codes(&stat->activeGCodes[0]);
    rs274ngc_active_m_codes(&stat->activeMCodes[0]);
    rs274ngc_active_settings(&stat->activeSettings[0]);

    stat->heartbeat++;

    return 0;
}
