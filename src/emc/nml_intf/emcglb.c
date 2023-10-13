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

#include "config.h"
#include "emcglb.h"		/* these decls */
#include "emccfg.h"		/* their initial values */
#include "emcpos.h"		/* EmcPose */

char emc_inifile[LINELEN] = DEFAULT_EMC_INIFILE;

const char * DEFAULT_EMC_NMLFILE = EMC2_DEFAULT_NMLFILE;
char emc_nmlfile[LINELEN] = EMC2_DEFAULT_NMLFILE;

char rs274ngc_startup_code[LINELEN] =
    DEFAULT_RS274NGC_STARTUP_CODE;

int emc_debug = 0;		/* initially no debug messages */

double emc_task_cycle_time = DEFAULT_EMC_TASK_CYCLE_TIME;

int emc_task_interp_max_len = DEFAULT_EMC_TASK_INTERP_MAX_LEN;

EmcPose tool_change_position;	/* no defaults */
unsigned char have_tool_change_position = 0;	/* default is 'not there' */

int taskplanopen = 0;
