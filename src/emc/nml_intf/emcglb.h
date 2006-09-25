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

#include "emcpos.h"		/* EmcPose */
#include "math.h"		/* M_PI */

#define EMC_AXIS_MAX 8

#define EMC_INIFILE_LEN 256
    extern char EMC_INIFILE[EMC_INIFILE_LEN];

#define EMC_NMLFILE_LEN 256
    extern char EMC_NMLFILE[EMC_NMLFILE_LEN];

#define DEFAULT_RS274NGC_STARTUP_CODE ""
#define RS274NGC_STARTUP_CODE_MAX 256
    extern char RS274NGC_STARTUP_CODE[RS274NGC_STARTUP_CODE_MAX];


struct nameval {
    char *name;
    double value;
};

#define MAX_LIN_NV_PAIRS 5
const struct nameval linear_nv_pairs[MAX_LIN_NV_PAIRS] = {
	{ "mm", 	1.0 },
	{ "metric", 	1.0 },
	{ "in", 	1/25.4 },
	{ "inch", 	1/25.4 },
	{ "imperial", 	1/25.4 },
    };

#define MAX_CIR_NV_PAIRS 6
const struct nameval circular_nv_pairs[MAX_CIR_NV_PAIRS] = {
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

#define TOOL_TABLE_FILE_LEN 256
    extern char TOOL_TABLE_FILE[TOOL_TABLE_FILE_LEN];

    extern double TRAJ_DEFAULT_VELOCITY;
    extern double TRAJ_MAX_VELOCITY;

    extern double AXIS_MAX_VELOCITY[EMC_AXIS_MAX];
    extern double AXIS_MAX_ACCELERATION[EMC_AXIS_MAX];

    extern double SPINDLE_OFF_WAIT;
    extern double SPINDLE_ON_WAIT;

    extern int ESTOP_SENSE_INDEX;
    extern int LUBE_SENSE_INDEX;

    extern int ESTOP_SENSE_POLARITY;
    extern int LUBE_SENSE_POLARITY;

    extern int SPINDLE_ON_INDEX;
    extern double MIN_VOLTS_PER_RPM;
    extern double MAX_VOLTS_PER_RPM;

    extern int SPINDLE_FORWARD_INDEX;
    extern int SPINDLE_REVERSE_INDEX;
    extern int SPINDLE_BRAKE_INDEX;
    extern int SPINDLE_DECREASE_INDEX;
    extern int SPINDLE_INCREASE_INDEX;
    extern int SPINDLE_ENABLE_INDEX;
    extern int MIST_COOLANT_INDEX;
    extern int FLOOD_COOLANT_INDEX;
    extern int ESTOP_WRITE_INDEX;
    extern int LUBE_WRITE_INDEX;

    extern int SPINDLE_FORWARD_POLARITY;
    extern int SPINDLE_REVERSE_POLARITY;
    extern int MIST_COOLANT_POLARITY;
    extern int FLOOD_COOLANT_POLARITY;
    extern int SPINDLE_DECREASE_POLARITY;
    extern int SPINDLE_INCREASE_POLARITY;
    extern int ESTOP_WRITE_POLARITY;
    extern int SPINDLE_BRAKE_POLARITY;
    extern int SPINDLE_ENABLE_POLARITY;
    extern int LUBE_WRITE_POLARITY;

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
