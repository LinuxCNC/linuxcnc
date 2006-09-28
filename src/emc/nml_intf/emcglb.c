/********************************************************************
* Description: emcglb.c
*   Globals initialized to values in emccfg.h
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

#include <string.h>		/* strcpy() */
#include "emcglb.h"		/* these decls */
#include "emccfg.h"		/* their initial values */
#include "emcpos.h"		/* EmcPose */

char EMC_INIFILE[LINELEN] = DEFAULT_EMC_INIFILE;

char EMC_NMLFILE[LINELEN] = DEFAULT_EMC_NMLFILE;

char RS274NGC_STARTUP_CODE[LINELEN] =
    DEFAULT_RS274NGC_STARTUP_CODE;

int EMC_DEBUG = 0;		/* initially no debug messages */

double EMC_TASK_CYCLE_TIME = DEFAULT_EMC_TASK_CYCLE_TIME;

double EMC_IO_CYCLE_TIME = DEFAULT_EMC_IO_CYCLE_TIME;

char TOOL_TABLE_FILE[LINELEN] = DEFAULT_TOOL_TABLE_FILE;

double TRAJ_DEFAULT_VELOCITY = DEFAULT_TRAJ_DEFAULT_VELOCITY;
double TRAJ_MAX_VELOCITY = DEFAULT_TRAJ_MAX_VELOCITY;

double AXIS_MAX_VELOCITY[EMC_AXIS_MAX] = { 1.0 };	/*! \todo FIXME - I think
							   these should be
							   0.0 */
double AXIS_MAX_ACCELERATION[EMC_AXIS_MAX] = { 1.0 };

double SPINDLE_OFF_WAIT = DEFAULT_SPINDLE_OFF_WAIT;
double SPINDLE_ON_WAIT = DEFAULT_SPINDLE_ON_WAIT;

int SPINDLE_ON_INDEX = DEFAULT_SPINDLE_ON_INDEX;
double MIN_VOLTS_PER_RPM = DEFAULT_MIN_VOLTS_PER_RPM;
double MAX_VOLTS_PER_RPM = DEFAULT_MAX_VOLTS_PER_RPM;

EmcPose TOOL_CHANGE_POSITION;	/* no defaults */
unsigned char HAVE_TOOL_CHANGE_POSITION = 0;	/* default is 'not there' */
EmcPose TOOL_HOLDER_CLEAR;	/* no defaults */
unsigned char HAVE_TOOL_HOLDER_CLEAR;	/* default is 'not there' */

int taskplanopen = 0;

void emcInitGlobals()
{
    int t;

    for (t = 0; t < EMC_AXIS_MAX; t++) {
	AXIS_MAX_VELOCITY[t] = DEFAULT_AXIS_MAX_VELOCITY;
    }
}
