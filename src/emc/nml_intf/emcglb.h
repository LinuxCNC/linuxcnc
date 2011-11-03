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
#include "math.h"		/* M_PI */
#include "emcmotcfg.h"          /* EMCMOT_MAX_DIO */
#include "debugflags.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMC_AXIS_MAX EMCMOT_MAX_AXIS

#define EMC_MAX_DIO EMCMOT_MAX_DIO
#define EMC_MAX_AIO EMCMOT_MAX_AIO

    extern char EMC_INIFILE[LINELEN];

    extern char EMC_NMLFILE[LINELEN];

#define DEFAULT_RS274NGC_STARTUP_CODE ""
    extern char RS274NGC_STARTUP_CODE[LINELEN];

/* debug bitflags */
/* Note: these may be hard-code referenced by the GUI (e.g., emcdebug.tcl).
   If you change the assignments here, make sure and reflect that in
   the GUI scripts that use these. Unfortunately there's no easy way to
   get these into Tk automatically */
    extern int EMC_DEBUG;

    // EMC_DEBUG_* flag definitions moved to debugflags.h

    extern double EMC_TASK_CYCLE_TIME;

    extern double EMC_IO_CYCLE_TIME;

    extern int EMC_TASK_INTERP_MAX_LEN;

    extern char TOOL_TABLE_FILE[LINELEN];

    extern double TRAJ_DEFAULT_VELOCITY;
    extern double TRAJ_MAX_VELOCITY;

    extern double AXIS_MAX_VELOCITY[EMC_AXIS_MAX];
    extern double AXIS_MAX_ACCELERATION[EMC_AXIS_MAX];

    extern struct EmcPose TOOL_CHANGE_POSITION;
    extern unsigned char HAVE_TOOL_CHANGE_POSITION;
    extern struct EmcPose TOOL_HOLDER_CLEAR;
    extern unsigned char HAVE_TOOL_HOLDER_CLEAR;

/*just used to keep track of unneccessary debug printing. */
    extern int taskplanopen;

    extern int emcGetArgs(int argc, char *argv[]);
    extern void emcInitGlobals();

#ifdef __cplusplus
}				/* matches extern "C" at top */
#endif
#endif				/* EMCGLB_H */
