/********************************************************************
* Description: emcglb.h
*   Declarations for globals found in emcglb.c
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
#ifndef EMCGLB_H
#define EMCGLB_H

#include "config.h"             /* LINELEN */
#include "rtapi_math.h"		/* M_PI */
#include "emcmotcfg.h"          /* EMCMOT_MAX_DIO */
#include "debugflags.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMC_AXIS_MAX EMCMOT_MAX_AXIS

#define EMC_MAX_DIO EMCMOT_MAX_DIO
#define EMC_MAX_AIO EMCMOT_MAX_AIO

    extern char emc_inifile[LINELEN];

    extern char emc_nmlfile[LINELEN];

#define DEFAULT_RS274NGC_STARTUP_CODE ""
    extern char rs274ngc_startup_code[LINELEN];

/* debug bitflags */
/* Note: these may be hard-code referenced by the GUI (e.g., emcdebug.tcl).
   If you change the assignments here, make sure and reflect that in
   the GUI scripts that use these. Unfortunately there's no easy way to
   get these into Tk automatically */

    // there's also an emc_Debug function in emc/usr_intf/emcsh.cc
    extern int emc_debug;

    // EMC_DEBUG_* flag definitions moved to debugflags.h

    extern double emc_task_cycle_time;	

    extern double emc_io_cycle_time;

    extern int emc_task_interp_max_len;

    extern char tool_table_file[LINELEN];

    extern double traj_default_velocity;
    extern double traj_max_velocity;

    extern double axis_max_velocity[EMC_AXIS_MAX];
    extern double axis_max_acceleration[EMC_AXIS_MAX];

    extern struct EmcPose tool_change_position;
    extern unsigned char have_tool_change_position;
    extern struct EmcPose tool_holder_clear;
    extern unsigned char have_tool_holder_clear;

/*just used to keep track of unneccessary debug printing. */
    extern int taskplanopen;

    extern int emcGetArgs(int argc, char *argv[]);
    extern void emcInitGlobals();

#ifdef __cplusplus
}				/* matches extern "C" at top */
#endif
#endif				/* EMCGLB_H */
