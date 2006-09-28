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
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef EMCGLB_H
#define EMCGLB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "emcpos.h"		/* EmcPose */
#include "math.h"		/* M_PI */

#define EMC_AXIS_MAX 8

    extern char EMC_INIFILE[LINELEN];

    extern char EMC_NMLFILE[LINELEN];

#define DEFAULT_RS274NGC_STARTUP_CODE ""
    extern char RS274NGC_STARTUP_CODE[LINELEN];


struct nameval {
    char *name;
    double value;
};


/* The next const struct holds pairs for linear units which are 
   valid under the [TRAJ] section. These are of the form {"name", value}.
   If the name "name" is encountered in the ini, the value will be used */
#define MAX_LIN_NV_PAIRS 5
const struct nameval linear_nv_pairs[MAX_LIN_NV_PAIRS] = {
	{ "mm", 	1.0 },
	{ "metric", 	1.0 },
	{ "in", 	1/25.4 },
	{ "inch", 	1/25.4 },
	{ "imperial", 	1/25.4 },
    };
    
/* The next const struct holds pairs for angular units which are 
   valid under the [TRAJ] section. These are of the form {"name", value}.
   If the name "name" is encountered in the ini, the value will be used */
#define MAX_ANG_NV_PAIRS 6
const struct nameval angular_nv_pairs[MAX_ANG_NV_PAIRS] = {
	{ "deg", 	1.0 },
	{ "degree", 	1.0 },
	{ "grad", 	0.9 },
	{ "gon", 	0.9 },
	{ "rad", 	M_PI / 180 },
	{ "radian", 	M_PI / 180 },
    };

/* debug bitflags */
/* Note: these may be hard-code referenced by the GUI (e.g., emcdebug.tcl).
   If you change the assignments here, make sure and reflect that in
   the GUI scripts that use these. Unfortunately there's no easy way to
   get these into Tk automatically */
    extern int EMC_DEBUG;
#define EMC_DEBUG_INVALID           0x00000001
#define EMC_DEBUG_CONFIG            0x00000002
#define EMC_DEBUG_DEFAULTS          0x00000004
#define EMC_DEBUG_VERSIONS          0x00000008
#define EMC_DEBUG_TASK_ISSUE        0x00000010
#define EMC_DEBUG_IO_POINTS         0x00000020
#define EMC_DEBUG_NML               0x00000040
#define EMC_DEBUG_MOTION_TIME       0x00000080
#define EMC_DEBUG_INTERP            0x00000100
#define EMC_DEBUG_RCS               0x00000200
#define EMC_DEBUG_TRAJ              0x00000400
#define EMC_DEBUG_INTERP_LIST       0x00000800
#define EMC_DEBUG_ALL               0x7FFFFFFF	/* it's an int for %i to work 
						 */

    extern double EMC_TASK_CYCLE_TIME;

    extern double EMC_IO_CYCLE_TIME;

    extern char TOOL_TABLE_FILE[LINELEN];

    extern double TRAJ_DEFAULT_VELOCITY;
    extern double TRAJ_MAX_VELOCITY;

    extern double AXIS_MAX_VELOCITY[EMC_AXIS_MAX];
    extern double AXIS_MAX_ACCELERATION[EMC_AXIS_MAX];

    extern double SPINDLE_OFF_WAIT;
    extern double SPINDLE_ON_WAIT;

    extern int SPINDLE_ON_INDEX;
    extern double MIN_VOLTS_PER_RPM;
    extern double MAX_VOLTS_PER_RPM;

    extern EmcPose TOOL_CHANGE_POSITION;
    extern unsigned char HAVE_TOOL_CHANGE_POSITION;
    extern EmcPose TOOL_HOLDER_CLEAR;
    extern unsigned char HAVE_TOOL_HOLDER_CLEAR;

#define DEFAULT_EMCLOG_INCLUDE_HEADER (1)
    extern int EMCLOG_INCLUDE_HEADER;

/*just used to keep track of unneccessary debug printing. */
    extern int taskplanopen;

    extern int emcGetArgs(int argc, char *argv[]);
    extern void emcInitGlobals();

#ifdef __cplusplus
}				/* matches extern "C" at top */
#endif
#endif				/* EMCGLB_H */
