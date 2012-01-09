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

char emc_inifile[LINELEN] = DEFAULT_EMC_INIFILE;

char emc_nmlfile[LINELEN] = DEFAULT_EMC_NMLFILE;

char rs274ngc_startup_code[LINELEN] =
    DEFAULT_RS274NGC_STARTUP_CODE;

int emc_debug = 0;		/* initially no debug messages */

double emc_task_cycle_time = DEFAULT_EMC_TASK_CYCLE_TIME;

double emc_io_cycle_time = DEFAULT_EMC_IO_CYCLE_TIME;

int emc_task_interp_max_len = DEFAULT_EMC_TASK_INTERP_MAX_LEN;

char tool_table_file[LINELEN] = DEFAULT_TOOL_TABLE_FILE;

EmcPose tool_change_position;	/* no defaults */
unsigned char have_tool_change_position = 0;	/* default is 'not there' */
EmcPose tool_holder_clear;	/* no defaults */
unsigned char have_tool_holder_clear;	/* default is 'not there' */

int taskplanopen = 0;

void emcInitGlobals()
{
}
