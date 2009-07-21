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

int EMC_TASK_INTERP_MAX_LEN = DEFAULT_EMC_TASK_INTERP_MAX_LEN;

char TOOL_TABLE_FILE[LINELEN] = DEFAULT_TOOL_TABLE_FILE;

EmcPose TOOL_CHANGE_POSITION;	/* no defaults */
unsigned char HAVE_TOOL_CHANGE_POSITION = 0;	/* default is 'not there' */
EmcPose TOOL_HOLDER_CLEAR;	/* no defaults */
unsigned char HAVE_TOOL_HOLDER_CLEAR;	/* default is 'not there' */

int taskplanopen = 0;

void emcInitGlobals()
{
}
