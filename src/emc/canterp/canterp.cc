/********************************************************************
* Description:  canterp.cc
*               This file, 'canterp.cc', implements an interpreter 
*               of a file of printed canonical interfaces
*
* Author: proctor
* License: GPL Version 2
*    
* Copyright (c) 2005 All rights reserved.
*
* Last change: 
********************************************************************/
/*
  canterp.cc

  Straight-through interpreter of a file of printed canonical interface
  commands like these:

  1 N..... USE_LENGTH_UNITS(CANON_UNITS_MM)
  2 N..... SET_ORIGIN_OFFSETS(0.0000, 0.0000, 0.0000)
  3 N..... SET_FEED_REFERENCE(CANON_XYZ)
  4 N..... COMMENT("Circle Diamond Square Program")
  5 N..... COMMENT("Tom Kramer")
  6 N..... COMMENT("26-Sep-1994")
  7 N..... COMMENT("Assumes 4"x4"x2" finished stock")
  8 N..... COMMENT("Top of stock at Z=2"")
  9 N..... COMMENT("Cutter does not descend more than 0.94" below top")
  10 N0080  MIST_OFF()
  11 N0080  FLOOD_OFF()
  12 N0090  USE_TOOL_LENGTH_OFFSET(1.0000)
  13 N0110  STRAIGHT_TRAVERSE(0.0000, 0.0000, 3.0000)
  14 N0140  SET_FEED_RATE(16.0000)
  15 N0140  SET_SPINDLE_SPEED(3500.0000)
  16 N0140  START_SPINDLE_CLOCKWISE()
  17 N0150  COMMENT("MILLING AN ENCLOSED POCKET")
  18 N0160  STRAIGHT_TRAVERSE(0.0000, 3.9150, 3.0000)
  19 N0170  STRAIGHT_TRAVERSE(0.0000, 3.9150, 2.1000)
  20 N0180  COMMENT("start left circle zigzag")
  21 N0180  STRAIGHT_FEED(0.0000, 3.9150, 1.6875)
  22 N0190  STRAIGHT_FEED(4.0000, 3.9150, 1.6875)
  23 N0200  STRAIGHT_FEED(4.0000, 3.7250, 1.6875)
  24 N0210  STRAIGHT_FEED(0.0000, 3.7250, 1.6875)
  25 N0220  STRAIGHT_FEED(0.0000, 3.5350, 1.6875)
  26 N0230  STRAIGHT_FEED(1.4370, 3.5350, 1.6875)
  27 N0240  ARC_FEED(1.0704, 3.3450, 2.0000, 2.0000, 1, 1.6875)

  which typically come out of one of Tom Kramer's interpreters.
  The first two columns are ignored, the rest is converted to
  equivalent canonical calls.
*/

#include <stdio.h>		// FILE, fopen(), fclose()
#include <string.h>		// strcpy()
#include <ctype.h>		// isspace()
#include "emc.hh"		// emcStatus
#include "emc_nml.hh"
#include "rcs_print.hh"
#include "canon.hh"		// CANON_VECTOR, GET_PROGRAM_ORIGIN()
#include "emc/task/task.hh"	// emcTaskCommand etc

static FILE *the_file = NULL;	// our file pointer
static char the_file_name[LINELEN] = { 0 };	// our file pointer
static char the_command[LINELEN] = { 0 };	// our current command
static char the_command_name[LINELEN] = { 0 };	// just the name part
static char the_command_args[LINELEN] = { 0 };	// just the args part
static int the_line_number = 0;	// current line number
static int waitFlag = 0;	// local wait flag

// flag for how we want to interpret traj coord mode, as mdi or auto
static int mdiOrAuto = EMC_TASK_MODE_AUTO;

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

    // clear out the pending command
    emcTaskCommand = 0;

    // clear out the interpreter state
    emcStatus->task.interpState = EMC_TASK_INTERP_IDLE;
    emcStatus->task.execState = EMC_TASK_EXEC_DONE;
    emcStatus->task.motionLine = 0;
    emcStatus->task.readLine = 0;
    stepping = 0;
    steppingWait = 0;

    // now queue up command to resynch interpreter
    EMC_TASK_PLAN_SYNCH taskPlanSynchCmd;
    emcTaskQueueCommand(&taskPlanSynchCmd);

    // without emcTaskPlanClose(), a new run command resumes at
    // aborted line-- feature that may be considered later
    {
	int was_open = taskplanopen;
	emcTaskPlanClose();
	if (EMC_DEBUG & EMC_DEBUG_INTERP && was_open) {
	    rcs_print("emcTaskPlanClose() called at %s:%d\n", __FILE__,
		      __LINE__);
	}
    }

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
	emcTaskAbort();
	emcTaskPlanSynch();
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
	emcTaskAbort();
	emcTaskPlanSynch();
	break;

    case EMC_TASK_STATE_ESTOP:
	// go into estop-- do both IO estop and machine servos off
	emcAuxEstopOn();
	for (t = 0; t < emcStatus->motion.traj.axes; t++) {
	    emcAxisDisable(t);
	}
	emcTrajDisable();
	emcLubeOff();
	emcTaskAbort();
	emcTaskPlanSynch();
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

int emcTaskPlanInit(void)
{
    if (the_file != NULL) {
	fclose(the_file);
	the_file = NULL;
    }
    the_file_name[0] = 0;
    the_command[0] = 0;
    the_command_name[0] = 0;
    the_command_args[0] = 0;
    the_line_number = 0;
    waitFlag = 0;
    taskplanopen = 1;

    return 0;
}

int emcTaskPlanSetWait(void)
{
    waitFlag = 1;

    return 0;
}

int emcTaskPlanIsWait(void)
{
    return waitFlag;
}

int emcTaskPlanClearWait(void)
{
    waitFlag = 0;

    return 0;
}

int emcTaskPlanSynch(void)
{
    return 0;
}

int emcTaskPlanSetOptionalStop(bool state)
{
    SET_OPTIONAL_PROGRAM_STOP(state);
    return 0;
}

int emcTaskPlanSetBlockDelete(bool state)
{
    SET_BLOCK_DELETE(state);
    return 0;
}

int emcTaskPlanExit(void)
{
    return 0;
}

int emcTaskPlanOpen(const char *file)
{
    if (the_file != NULL) {
	fclose(the_file);
    }

    if (NULL == (the_file = fopen(file, "r"))) {
	return 1;
    }
    strcpy(the_file_name, file);
    the_command[0] = 0;
    the_command_name[0] = 0;
    the_command_args[0] = 0;
    the_line_number = 0;
    waitFlag = 0;
    taskplanopen = 1;

    if (emcStatus != 0) {
	emcStatus->task.motionLine = 0;
	emcStatus->task.currentLine = 0;
	emcStatus->task.readLine = 0;
    }

    return 0;
}

/*
  We expect lines like this:

  {ws}1<white>N1<ws>cmd{ws}({ws}{arg1{ws}}){extra}

 */

char *skipwhite(char *ptr)
{
    while (isspace(*ptr))
	ptr++;
    return ptr;
}

char *findwhite(char *ptr)
{
    while (!isspace(*ptr) && 0 != *ptr)
	ptr++;
    return ptr;
}

static int canterp_parse(char *buffer)
{
    char *ptr = buffer;
    char *cmd_ptr = the_command;
    char *name_ptr = the_command_name;
    char *args_ptr = the_command_args;
    char *last_quote_ptr;
    int inquote;

    *cmd_ptr = 0;
    *name_ptr = 0;
    *args_ptr = 0;

    // skip leading white space, return if nothing else found
    if (0 == *(ptr = skipwhite(ptr)))
	return 0;

    // ---cut here if no leading line number, N-number columns---

    // skip the first column, return if nothing else found
    if (0 == *(ptr = findwhite(ptr)))
	return 0;

    // skip following white space, return if nothing else found
    if (0 == *(ptr = skipwhite(ptr)))
	return 0;

    // skip the second column, return if nothing else found
    if (0 == *(ptr = findwhite(ptr)))
	return 0;

    // skip following white space, return if nothing else found
    if (0 == *(ptr = skipwhite(ptr)))
	return 0;

    // ---cut to here---

    // we got something; store the name, up to space or the '('
    while (!isspace(*ptr) && '(' != *ptr && 0 != *ptr) {
	*name_ptr++ = *ptr;
	*cmd_ptr++ = *ptr++;
    }
    if (isspace(*ptr))
	ptr = skipwhite(ptr);

    if (0 == *ptr) {
	// no parens, just a command name
	*name_ptr = 0;
	*cmd_ptr = 0;
	return 0;
    }

    if ('(' != *ptr) {
	// we're missing the '(', so flag an error
	*name_ptr = 0;
	*cmd_ptr = 0;
	return 1;
    }
    // we got the '(', so keep going
    *name_ptr = 0;		// terminate the name
    *cmd_ptr++ = *ptr++;	// add the '(' to the full command

    /*
       now we're at the args; skip first and last quotes when building
       args_ptr, and make commas spaces for easy parsing later
     */
    last_quote_ptr = 0;
    inquote = 0;
    while (')' != *ptr && 0 != *ptr) {
	if ('"' == *ptr) {
	    if (!inquote) {
		// here's the first quote, so suppress it in args_ptr
		inquote = 1;
		*cmd_ptr++ = *ptr++;
		continue;
	    }
	    /*
	       else it's a quote-in-quote, so mark it as the last so we
	       can delete it, thus handling any internal quotes, perhaps
	       used for inch marks
	     */
	    last_quote_ptr = args_ptr;
	}
	*args_ptr++ = (*ptr == ',' ? ' ' : *ptr);
	*cmd_ptr++ = *ptr++;
    }
    if (0 == *ptr) {
	// finished args without ')', so error
	*args_ptr = 0;
	*cmd_ptr = 0;
	return 1;
    }
    if (0 != last_quote_ptr)
	*last_quote_ptr = 0;
    *args_ptr = 0;
    *cmd_ptr++ = ')';
    *cmd_ptr = 0;

    return 0;
}

int emcTaskPlanRead(void)
{
    char buffer[LINELEN];

    if (NULL == fgets(buffer, LINELEN, the_file)) {
	return 1;
    }

    the_line_number++;

    return canterp_parse(buffer);
}

int emcTaskPlanExecute(const char *command, int line_number)
{
    return emcTaskPlanExecute(command);
}

int emcTaskPlanExecute(const char *command)
{
    int retval;
    double d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11;
    int i1, ln=-1;
    char s1[256];

    if (command) {
	retval = canterp_parse((char *) command);
	if (retval)
	    return retval;
    }

    if (!strcmp(the_command_name, "STRAIGHT_FEED")) {
	if (6 != sscanf(the_command_args, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9)) {
	    return 1;
	}
	STRAIGHT_FEED(ln, d1, d2, d3, d4, d5, d6, d7, d8, d9);
	return 0;
    }

    if (!strcmp(the_command_name, "ARC_FEED")) {
	if (9 != sscanf(the_command_args,
			"%lf %lf %lf %lf %d %lf %lf %lf %lf %lf %lf %lf",
			&d1, &d2, &d3, &d4, &i1, &d5, &d6, &d7, &d8, &d9, &d10, &d11)) {
	    return 1;
	}
	ARC_FEED(ln, d1, d2, d3, d4, i1, d5, d6, d7, d8, d9, d10, d11);
	return 0;
    }

    if (!strcmp(the_command_name, "STRAIGHT_TRAVERSE")) {
	if (6 != sscanf(the_command_args, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9)) {
	    return 1;
	}
	STRAIGHT_TRAVERSE(ln, d1, d2, d3, d4, d5, d6, d7, d8, d9);
	return 0;
    }

    if (!strcmp(the_command_name, "STRAIGHT_PROBE")) {
	if (6 != sscanf(the_command_args, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9)) {
	    return 1;
	}
	STRAIGHT_PROBE(ln, d1, d2, d3, d4, d5, d6, d7, d8, d9, 0);
	return 0;
    }

    if (!strcmp(the_command_name, "USE_LENGTH_UNITS")) {
	if (!strcmp(the_command_args, "CANON_UNITS_MM")) {
	    USE_LENGTH_UNITS(CANON_UNITS_MM);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_UNITS_CM")) {
	    USE_LENGTH_UNITS(CANON_UNITS_MM);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_UNITS_INCHES")) {
	    USE_LENGTH_UNITS(CANON_UNITS_INCHES);
	    return 0;
	}
	return 1;
    }

    if (!strcmp(the_command_name, "SET_ORIGIN_OFFSETS")) {
	if (6 != sscanf(the_command_args, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9)) {
	    return 1;
	}
	SET_ORIGIN_OFFSETS(d1, d2, d3, d4, d5, d6, d7, d8, d9);
	return 0;
    }

    if (!strcmp(the_command_name, "SET_FEED_REFERENCE")) {
	if (!strcmp(the_command_args, "CANON_WORKPIECE")) {
	    SET_FEED_REFERENCE(CANON_WORKPIECE);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_XYZ")) {
	    SET_FEED_REFERENCE(CANON_XYZ);
	    return 0;
	}
	return 1;
    }

    if (!strcmp(the_command_name, "SELECT_PLANE")) {
	if (!strcmp(the_command_args, "CANON_PLANE_XY")) {
	    SELECT_PLANE(CANON_PLANE_XY);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_PLANE_YZ")) {
	    SELECT_PLANE(CANON_PLANE_YZ);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_PLANE_XZ")) {
	    SELECT_PLANE(CANON_PLANE_XZ);
	    return 0;
	}
	return 1;
    }

    if (!strcmp(the_command_name, "COMMENT")) {
	COMMENT(the_command_args);
	return 0;
    }

    if (!strcmp(the_command_name, "MIST_OFF")) {
	MIST_OFF();
	return 0;
    }

    if (!strcmp(the_command_name, "FLOOD_OFF")) {
	FLOOD_OFF();
	return 0;
    }

    if (!strcmp(the_command_name, "MIST_ON")) {
	MIST_ON();
	return 0;
    }

    if (!strcmp(the_command_name, "FLOOD_ON")) {
	FLOOD_ON();
	return 0;
    }

    if (!strcmp(the_command_name, "USE_TOOL_LENGTH_OFFSET")) {
	if (1 != sscanf(the_command_args, "%lf %lf %lf", &d1, &d2, &d3)) {
	    return 1;
	}
	USE_TOOL_LENGTH_OFFSET(d1, d2, d3);
	return 0;
    }

    if (!strcmp(the_command_name, "SET_FEED_RATE")) {
	if (1 != sscanf(the_command_args, "%lf", &d1)) {
	    return 1;
	}
	SET_FEED_RATE(d1);
	return 0;
    }

    if (!strcmp(the_command_name, "SET_TRAVERSE_RATE")) {
	if (1 != sscanf(the_command_args, "%lf", &d1)) {
	    return 1;
	}
	SET_TRAVERSE_RATE(d1);
	return 0;
    }

    if (!strcmp(the_command_name, "SELECT_POCKET")) {
	if (1 != sscanf(the_command_args, "%d", &i1)) {
	    return 1;
	}
	SELECT_POCKET(i1);
	return 0;
    }

    if (!strcmp(the_command_name, "CHANGE_TOOL")) {
	if (1 != sscanf(the_command_args, "%d", &i1)) {
	    return 1;
	}
	CHANGE_TOOL(i1);
	return 0;
    }

    if (!strcmp(the_command_name, "DWELL")) {
	if (1 != sscanf(the_command_args, "%lf", &d1)) {
	    return 1;
	}
	DWELL(d1);
	return 0;
    }

    if (!strcmp(the_command_name, "SPINDLE_RETRACT")) {
	SPINDLE_RETRACT();
	return 0;
    }

    if (!strcmp(the_command_name, "SPINDLE_RETRACT_TRAVERSE")) {
	SPINDLE_RETRACT_TRAVERSE();
	return 0;
    }

    if (!strcmp(the_command_name, "LOCK_SPINDLE_Z")) {
	LOCK_SPINDLE_Z();
	return 0;
    }

    if (!strcmp(the_command_name, "USE_SPINDLE_FORCE")) {
	USE_SPINDLE_FORCE();
	return 0;
    }

    if (!strcmp(the_command_name, "USE_NO_SPINDLE_FORCE")) {
	USE_NO_SPINDLE_FORCE();
	return 0;
    }

    if (!strcmp(the_command_name, "CLAMP_AXIS")) {
	if (!strcmp(the_command_args, "CANON_AXIS_X")) {
	    CLAMP_AXIS(CANON_AXIS_X);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_Y")) {
	    CLAMP_AXIS(CANON_AXIS_Y);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_Z")) {
	    CLAMP_AXIS(CANON_AXIS_Z);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_A")) {
	    CLAMP_AXIS(CANON_AXIS_A);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_B")) {
	    CLAMP_AXIS(CANON_AXIS_B);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_C")) {
	    CLAMP_AXIS(CANON_AXIS_C);
	    return 0;
	}
	return 1;
    }

    if (!strcmp(the_command_name, "UNCLAMP_AXIS")) {
	if (!strcmp(the_command_args, "CANON_AXIS_X")) {
	    UNCLAMP_AXIS(CANON_AXIS_X);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_Y")) {
	    UNCLAMP_AXIS(CANON_AXIS_Y);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_Z")) {
	    UNCLAMP_AXIS(CANON_AXIS_Z);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_A")) {
	    UNCLAMP_AXIS(CANON_AXIS_A);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_B")) {
	    UNCLAMP_AXIS(CANON_AXIS_B);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_AXIS_C")) {
	    UNCLAMP_AXIS(CANON_AXIS_C);
	    return 0;
	}
	return 1;
    }

    if (!strcmp(the_command_name, "SET_CUTTER_RADIUS_COMPENSATION")) {
	if (1 != sscanf(the_command_args, "%lf", &d1)) {
	    return 1;
	}
	SET_CUTTER_RADIUS_COMPENSATION(d1);
	return 0;
    }

    if (!strcmp(the_command_name, "START_CUTTER_RADIUS_COMPENSATION")) {
	if (1 != sscanf(the_command_args, "%d", &i1)) {
	    return 1;
	}
	START_CUTTER_RADIUS_COMPENSATION(i1);
	return 0;
    }

    if (!strcmp(the_command_name, "STOP_CUTTER_RADIUS_COMPENSATION")) {
	STOP_CUTTER_RADIUS_COMPENSATION();
	return 0;
    }

    if (!strcmp(the_command_name, "START_SPEED_FEED_SYNCH")) {
	if (2 != sscanf(the_command_args, "%lf %d", &d1, &i1)) {
            return 1;
        }
	START_SPEED_FEED_SYNCH(d1, i1);
	return 0;
    }

    if (!strcmp(the_command_name, "STOP_SPEED_FEED_SYNCH")) {
	STOP_SPEED_FEED_SYNCH();
	return 0;
    }

    if (!strcmp(the_command_name, "SET_SPINDLE_SPEED")) {
	if (1 != sscanf(the_command_args, "%lf", &d1)) {
	    return 1;
	}
	SET_SPINDLE_SPEED(d1);
	return 0;
    }

    if (!strcmp(the_command_name, "START_SPINDLE_CLOCKWISE")) {
	START_SPINDLE_CLOCKWISE();
	return 0;
    }

    if (!strcmp(the_command_name, "START_SPINDLE_COUNTERCLOCKWISE")) {
	START_SPINDLE_COUNTERCLOCKWISE();
	return 0;
    }

    if (!strcmp(the_command_name, "STOP_SPINDLE_TURNING")) {
	STOP_SPINDLE_TURNING();
	return 0;
    }

    if (!strcmp(the_command_name, "ORIENT_SPINDLE")) {
	if (2 != sscanf(the_command_args, "%lf %s", &d1, s1)) {
	    return 1;
	}
	if (!strcmp(s1, "CANON_CLOCKWISE")) {
	    ORIENT_SPINDLE(d1, CANON_CLOCKWISE);
	    return 0;
	}
	if (!strcmp(s1, "CANON_COUNTERCLOCKWISE")) {
	    ORIENT_SPINDLE(d1, CANON_COUNTERCLOCKWISE);
	    return 0;
	}
	return 1;
    }

    if (!strcmp(the_command_name, "DISABLE_SPEED_OVERRIDE")) {
	DISABLE_SPEED_OVERRIDE();
	return 0;
    }

    if (!strcmp(the_command_name, "DISABLE_FEED_OVERRIDE")) {
	DISABLE_FEED_OVERRIDE();
	return 0;
    }

    if (!strcmp(the_command_name, "ENABLE_SPEED_OVERRIDE")) {
	ENABLE_SPEED_OVERRIDE();
	return 0;
    }

    if (!strcmp(the_command_name, "ENABLE_FEED_OVERRIDE")) {
	ENABLE_FEED_OVERRIDE();
	return 0;
    }

    if (!strcmp(the_command_name, "PROGRAM_STOP")) {
	PROGRAM_STOP();
	return 0;
    }

    if (!strcmp(the_command_name, "OPTIONAL_PROGRAM_STOP")) {
	OPTIONAL_PROGRAM_STOP();
	return 0;
    }

    if (!strcmp(the_command_name, "PROGRAM_END")) {
	PROGRAM_END();
	return 0;
    }

    if (!strcmp(the_command_name, "PALLET_SHUTTLE")) {
	PALLET_SHUTTLE();
	return 0;
    }

    if (!strcmp(the_command_name, "SET_MOTION_CONTROL_MODE")) {
	if (!strcmp(the_command_args, "CANON_EXACT_PATH")) {
	    SET_MOTION_CONTROL_MODE(CANON_EXACT_PATH, 0);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_EXACT_STOP")) {
	    SET_MOTION_CONTROL_MODE(CANON_EXACT_STOP, 0);
	    return 0;
	}
	if (!strcmp(the_command_args, "CANON_CONTINUOUS")) {
	    SET_MOTION_CONTROL_MODE(CANON_CONTINUOUS, 0);
	    return 0;
	}
	return 1;
    }

    if (!strcmp(the_command_name, "MESSAGE")) {
	MESSAGE(the_command_args);
	return 0;
    }

    if (!strcmp(the_command_name, "INIT_CANON")) {
	INIT_CANON();
	return 0;
    }

    if (!strcmp(the_command_name, "TURN_PROBE_OFF")) {
	TURN_PROBE_OFF();
	return 0;
    }

    if (!strcmp(the_command_name, "TURN_PROBE_ON")) {
	TURN_PROBE_ON();
	return 0;
    }

    fprintf(stderr, "canterp: unrecognized canonical command %s\n",
	    the_command);
    return 1;
}

int emcTaskPlanClose(void)
{
    taskplanopen = 0;

    return 0;
}

int emcTaskPlanLine(void)
{
    return the_line_number;
}

int emcTaskPlanCommand(char *cmd)
{
    strcpy(cmd, the_command);

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

    strcpy(stat->file, the_file_name);
    // command set in main

    stat->heartbeat++;

    return 0;
}

