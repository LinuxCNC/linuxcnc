/********************************************************************
* Description: motion.h
*   Data structures used throughout emc2.
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

/* jmk says: This file is a mess! */

/*

Misc ramblings:

The terms axis and joint are used inconsistently throughout EMC.
For all new code, the usages are as follows:

    axis - one of the six degrees of freedom, x, y, z, a, b, c
	these refer to axes in Cartesian space, which may or
	may not match up with joints (see below). On Cartesian
	machines they do match up, but for hexapods, robots, and
	other non-Cartesian machines they don't.
    joint - one of the physical degrees of freedom of the machine
	these might be linear (leadscrews) or rotary (rotary
	tables, robot arm joints).  There can be any number of
	joints.  The kinematics code is responsible for translating
	from axis space to joint space and back.

There are three main kinds of data needed by the motion controller

1) data shared with higher level stuff - commands, status, etc.
2) data that is local to the motion controller
3) data shared with lower level stuff - hal pins

In addition, some internal data (2) should be shared for trouble
shooting purposes, even though it is "internal" to the motion
controller.  Depending on the type of data, it can either be
treated as type (1), and made available to the higher level
code, or it can be treated as type (3), and made available to
the hal, so that halscope can monitor it.

This file should ONLY contain structures and declarations for
type (1) items - those that are shared with higher level code.

Type (2) items should be declared in mot_priv.h, along
with type (3) items.

In the interest of retaining my sanity, I'm not gonna attempt
to move everything to its proper location yet....

However, all new items will be defined in the proper place,
and some existing items may be moved from one struct definition
to another.

*/

/* the following line can be used to control where some of the
   "internal" motion controller data is stored.  By default,
   it is stored in staticlly allocated kernel memory.  However,
   if STRUCTS_IN_SHMEM is defined, it will be stored in the
   emcmotStruct shared memory area, for debugging purposes.
*/

// #define STRUCTS_IN_SHMEM



#ifndef MOTION_H
#define MOTION_H

#include "posemath.h"		/* PmCartesian, PmPose, pmCartMag() */
#include "emcpos.h"		/* EmcPose */
#include "cubic.h"		/* CUBIC_STRUCT, CUBIC_COEFF */
#include "emcmotcfg.h"		/* EMCMOT_MAX_AXIS */
#include "emcmotlog.h"
#include "tp.h"			/* TP_STRUCT */
#include "tc.h"			/* TC_STRUCT, TC_QUEUE_STRUCT */
#include "mmxavg.h"		/* MMXAVG_STRUCT */
#include "kinematics.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _EMC_TELEOP_DATA {
	EmcPose currentVel;
	EmcPose currentAccell;
	EmcPose desiredVel;
	EmcPose desiredAccell;
    } EMC_TELEOP_DATA;

/* This list is all the possible commands */
enum {
  EMCMOT_SET_TRAJ_CYCLE_TIME = 1, /* set the cycle time */
  EMCMOT_SET_SERVO_CYCLE_TIME,  /* set the interpolation rate */
  EMCMOT_SET_POSITION_LIMITS,   /* set the axis position +/- limits */
  EMCMOT_SET_OUTPUT_LIMITS,     /* set the axis output +/- limits */
  EMCMOT_SET_OUTPUT_SCALE,      /* scale factor for outputs */
  EMCMOT_SET_INPUT_SCALE,       /* scale factor for inputs */
  EMCMOT_SET_MIN_FERROR,        /* minimum following error, input units */
  EMCMOT_SET_MAX_FERROR,        /* maximum following error, input units */
  EMCMOT_JOG_CONT,              /* continuous jog */
  EMCMOT_JOG_INCR,              /* incremental jog */
  EMCMOT_JOG_ABS,               /* absolute jog */
  EMCMOT_SET_LINE,              /* queue up a linear move */
  EMCMOT_SET_CIRCLE,            /* queue up a circular move */
  EMCMOT_SET_VEL,               /* set the velocity for subsequent moves */
  EMCMOT_SET_VEL_LIMIT,         /* set the absolute max vel for all moves */
  EMCMOT_SET_AXIS_VEL_LIMIT,	/* set the absolute max vel for each axis */
  EMCMOT_SET_AXIS_ACC_LIMIT,	/* set the absolute max vel for each axis */
  EMCMOT_SET_ACC,               /* set the acceleration for moves */
  EMCMOT_PAUSE,                 /* pause motion */
  EMCMOT_RESUME,                /* resume motion */
  EMCMOT_STEP,                  /* resume motion until id encountered */
  EMCMOT_ABORT,                 /* abort motion */
  EMCMOT_SCALE,                 /* scale the speed */
  EMCMOT_ENABLE,                /* enable servos for active axes */
  EMCMOT_DISABLE,               /* disable servos for active axes */
  EMCMOT_SET_PID,               /* set PID gains */
  EMCMOT_ENABLE_AMPLIFIER,      /* enable amp outputs and dac writes */
  EMCMOT_DISABLE_AMPLIFIER,     /* disable amp outputs and dac writes */
  EMCMOT_OPEN_LOG,              /* open a log */
  EMCMOT_START_LOG,             /* start logging */
  EMCMOT_STOP_LOG,              /* stop logging */
  EMCMOT_CLOSE_LOG,             /* close log */
  EMCMOT_DAC_OUT,               /* write directly to the dacs */
  EMCMOT_HOME,                  /* home an axis */
  EMCMOT_FREE,                  /* set mode to free (joint) motion */
  EMCMOT_COORD,                 /* set mode to coordinated motion */
  EMCMOT_TELEOP,                 /* set mode to teleop*/
  EMCMOT_ENABLE_WATCHDOG,       /* enable watchdog sound, parport */
  EMCMOT_DISABLE_WATCHDOG,      /* enable watchdog sound, parport */
  EMCMOT_SET_POLARITY,          /* set polarity for axis flags */
  EMCMOT_ACTIVATE_AXIS,         /* make axis active */
  EMCMOT_DEACTIVATE_AXIS,       /* make axis inactive */
  EMCMOT_SET_TERM_COND,         /* set termination condition (stop, blend) */
  EMCMOT_SET_HOMING_VEL,        /* set the axis homing speed */
  EMCMOT_SET_NUM_AXES,          /* set the number of axes */
  EMCMOT_SET_WORLD_HOME,        /* set pose for world home */
  EMCMOT_SET_JOINT_HOME,        /* set value for joint homes */
  EMCMOT_SET_HOME_OFFSET,       /* where to go after a home */
  EMCMOT_OVERRIDE_LIMITS,       /* temporarily ignore limits until jog done */
  EMCMOT_SET_TELEOP_VECTOR,	/* Move at a given velocity  but in 
				   world cartesian coordinates, not in joint 
				   space like EMCMOT_JOG_* */
  EMCMOT_SET_PROBE_INDEX,       /* set which wire the probe signal is on. */
  EMCMOT_SET_PROBE_POLARITY,    /* probe tripped on 0 to 1 transition or on
                                   1 to 0 transition. */
  EMCMOT_CLEAR_PROBE_FLAGS,     /* clears probeTripped flag */
  EMCMOT_PROBE,                  /* go towards a position, stop if the probe
                                   is tripped, and record the position where
                                   the probe tripped */
  EMCMOT_SET_DEBUG,		/* sets the debug level */
  EMCMOT_SET_AOUT,		/* sets an analog motion point for next move */
  EMCMOT_SET_DOUT,		/* sets a digital motion point for next move */
  EMCMOT_SET_INDEX_BIT,		/* Sets or clears a digital IO pin */
  EMCMOT_READ_INDEX_BIT,	/* Reads a digital IO pin */
  EMCMOT_CHECK_INDEX_BIT,	/* Checks a digital IO pin for the last value written */
  EMCMOT_SET_STEP_PARAMS        /* sets setup_time and hold_time for freqtask */
};

#define EMCMOT_COMMAND_OK 0
#define EMCMOT_COMMAND_UNKNOWN_COMMAND 1
#define EMCMOT_COMMAND_INVALID_COMMAND 2
#define EMCMOT_COMMAND_INVALID_PARAMS 3
#define EMCMOT_COMMAND_BAD_EXEC 4

/* termination conditions for queued motions */
#define EMCMOT_TERM_COND_STOP 1
#define EMCMOT_TERM_COND_BLEND 2

/*********************************
       COMMAND STRUCTURE
*********************************/
typedef struct
{
  unsigned char head;           /* flag count for mutex detect */
  int command;                  /* one of enum above */
  int commandNum;               /* increment this for new command */
  double cycleTime;             /* planning time (not servo time) */
  double maxLimit;              /* pos value for position limit, output */
  double minLimit;              /* neg value for position limit, output */
  EmcPose pos;                  /* end for line, circle */
  PmCartesian center;           /* center for circle */
  PmCartesian normal;           /* normal vec for circle */
  int turn;                     /* turns for circle */
  double vel;                   /* max velocity */
  double acc;                   /* max acceleration */
  int id;                       /* id for motion */
  int termCond;                 /* termination condition */
  int axis;                     /* which index to use for below */
  PID_STRUCT pid;               /* gains */
  int logSize;                  /* size for log fifo */
  int logSkip;                  /* how many to skip, 0 means log all,
                                   -1 means don't log on cycles */
  int logType;                  /* type for logging */
  int logTriggerType;           /* see enum LOG_TRIGGER_TYPES */
  int logTriggerVariable;       /* the variable(s) that can cause the log to 
				   trigger. se enum LOG_TRIGGER_VARS */
  double logTriggerThreshold;   /* the value for non manual triggers */
  double dacOut;                /* output to DAC */
  double scale;                 /* input or output scale arg */
  double offset;                /* input or output offset arg */
  double minFerror;             /* min following error */
  double maxFerror;             /* max following error */
  int wdWait;                   /* cycle to wait before toggling wd */
  EMCMOT_AXIS_FLAG axisFlag;    /* flag to set polarities */
  int level;                    /* flag for polarity level */
  int index;			/* Digital IO pin index */
#ifdef ENABLE_PROBING
  int probeIndex;               /* which wire the probe signal is on */
#endif
  int debug;		       	/* debug level, from DEBUG in .ini file */
  unsigned char start, end, now; /* start/end bits, immediate flag */
  unsigned char tail;           /* flag count for mutex detect */
  double setup_time;               /* number of periods before step occurs that dir changes */
  double hold_time;                /* number of periods that step line is held low/high after transition */
} EMCMOT_COMMAND;


/* motion flag type */
typedef unsigned short EMCMOT_MOTION_FLAG;

/*
  motion status flag structure-- looks like:

  MSB                             LSB
  v---------------v------------------v
  |   |   |   | T | CE | C | IP | EN |
  ^---------------^------------------^

  where:

  EN is 1 if calculations are enabled, 0 if not
  IP is 1 if all axes in position, 0 if not
  C is 1 if coordinated mode, 0 if in free mode
  CE is 1 if coordinated mode error, 0 if not
  T is 1 if we are in teleop mode.
  */

/* bit masks */
#define EMCMOT_MOTION_ENABLE_BIT      0x0001
#define EMCMOT_MOTION_INPOS_BIT       0x0002
#define EMCMOT_MOTION_COORD_BIT       0x0004
#define EMCMOT_MOTION_ERROR_BIT       0x0008
#define EMCMOT_MOTION_TELEOP_BIT      0x0010

/* axis flag type */
typedef unsigned short EMCMOT_AXIS_FLAG;
/*
  axis status flag structure-- looks like:

  MSB                                                          LSB
  ----------v-----------------v-----------------------v-------------------v
  | AF | FE | AH | HD | H | HS | NHL | PHL | NSL | PSL | ER | IP | AC | EN |
  ----------^-----------------^-----------------------^-------------------^
               

  x = unused

  where:

  EN  is 1 if axis amplifier is enabled, 0 if not
  AC  is 1 if axis is active for calculations, 0 if not
  IP  is 1 if axis is in position, 0 if not (free mode only)
  ER  is 1 if axis has an error, 0 if not

  PSL is 1 if axis is on maximum software limit, 0 if not
  NSL is 1 if axis is on minimum software limit, 0 if not
  PHL is 1 if axis is on maximum hardware limit, 0 if not
  NHL is 1 if axis is on minimum hardware limit, 0 if not

  HS  is 1 if axis home switch is tripped, 0 if not
  H   is 1 if axis is homing, 0 if not
  HD  is 1 if axis has been homed, 0 if not
  AH  is 1 if axis is at home position, 0 if not

  FE  is 1 if axis exceeded following error, 0 if not
  AF  is 1 if amplifier is faulted, 0 if not

Suggestion: Split this in to an Error and a Status flag register..
             Then a simple test on each of the two flags can be performed
             rather than testing each bit... Saving on a global per axis
             fault and ready status flag.
  */

/* bit masks */
#define EMCMOT_AXIS_ENABLE_BIT         0x0001
#define EMCMOT_AXIS_ACTIVE_BIT         0x0002
#define EMCMOT_AXIS_INPOS_BIT          0x0004
#define EMCMOT_AXIS_ERROR_BIT          0x0008

#define EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT 0x0010
#define EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT 0x0020
#define EMCMOT_AXIS_MAX_HARD_LIMIT_BIT 0x0040
#define EMCMOT_AXIS_MIN_HARD_LIMIT_BIT 0x0080

#define EMCMOT_AXIS_HOME_SWITCH_BIT    0x0100
#define EMCMOT_AXIS_HOMING_BIT         0x0200
#define EMCMOT_AXIS_HOMED_BIT          0x0400

/*! \todo FIXME - I'm not sure AT_HOME is being reported correctly.
   AT_HOME is cleared when you jog in free mode, but not if
   you do a coordinated move... perhaps that is the intended
   behavior.
*/
#define EMCMOT_AXIS_AT_HOME_BIT        0x0800

#define EMCMOT_AXIS_FERROR_BIT         0x1000
#define EMCMOT_AXIS_FAULT_BIT          0x2000

/*! \todo FIXME - the terms "teleop", "coord", and "free" are poorly
   documented.  This is my feeble attempt to understand exactly
   what they mean.

   According to Fred, teleop is never used with machine tools,
   although that may not be true for machines with non-trivial
   kinematics.

   "coord", or coordinated mode, means that all the axis are
   synchronized, and move together as commanded by the higher
   level code.  It is the normal mode when machining.  In
   coordinated mode, commands are assumed to be in the cartesean
   reference frame, and if the machine is non-cartesean, the
   commands are translated by the kinematics to drive each
   axis in joint space as needed.

   "free" mode means commands are interpreted in joint space.
   It is used for jogging individual axes (joints), although
   it does not preclude multiple axes moving at once (I think).
   Homing is also done in free mode, in fact machines with
   non-trivial kinematics must be homed before they can go
   into either coord or teleop mode.

   'teleop' is what you probably want if you are 'jogging'
   a hexapod.  The jog commands as implemented by the motion
   controller are joint jogs, which work in free mode.  But
   if you want to jog a hexapod or similar machine along
   one particular cartesean axis, you need to operate more
   than one joint.  That's what 'teleop' is for.

*/

/* compensation structure */
#define EMCMOT_COMP_SIZE 256
    typedef struct {
	int total;		/* how many comp points */
	double avgint;		/* average interval between points */
	double nominal[EMCMOT_COMP_SIZE];	/* nominal points */
	double forward[EMCMOT_COMP_SIZE];	/* forward comp points */
	double reverse[EMCMOT_COMP_SIZE];	/* reverse comp points */
	double alter;		/* additive dynamic compensation */
    } emcmot_comp;

/* motion controller states */

    typedef enum {
	EMCMOT_MOTION_DISABLED = 0,
	EMCMOT_MOTION_FREE,
	EMCMOT_MOTION_TELEOP,
	EMCMOT_MOTION_COORD
    } motion_state;

/* states for homing */
    typedef enum {
	HOME_IDLE = 0,
	HOME_START,		// 1
	HOME_INITIAL_BACKOFF_START,	// 2
	HOME_INITIAL_BACKOFF_WAIT,	// 3
	HOME_INITIAL_SEARCH_START,	// 4
	HOME_INITIAL_SEARCH_WAIT,	// 5
	HOME_SET_COARSE_POSITION,	// 6
	HOME_FINAL_BACKOFF_START,	// 7
	HOME_FINAL_BACKOFF_WAIT,	// 8
	HOME_RISE_SEARCH_START,	// 9
	HOME_RISE_SEARCH_WAIT,	// 10
	HOME_FALL_SEARCH_START,	// 11
	HOME_FALL_SEARCH_WAIT,	// 12
	HOME_INDEX_SEARCH_WAIT,	// 13
	HOME_SET_FINAL_POSITION,	// 14
	HOME_FINAL_MOVE_START,	// 15
	HOME_FINAL_MOVE_WAIT,	// 16
	HOME_FINISHED,		// 17
	HOME_ABORT		// 18
    } home_state;

/* flags for homing */
#define HOME_IGNORE_LIMITS	1
#define HOME_USE_INDEX		2

/* flags for switch config */
#define SWITCHES_LATCH_LIMITS	16

/* This structure contains all of the data associated with
   a single joint.  Note that this structure does not need
   to be in shared memory (but it can, if desired for debugging
   reasons).  The portions of this structure that are considered
   "status" and need to be made available to user space are
   copied to a much smaller struct called emcmot_joint_status
   which is located in shared memory.

*/
    typedef struct {

	/* configuration info - changes rarely */
	int type;		/* 0 = linear, 1 = rotary */
	double max_pos_limit;	/* upper soft limit on joint pos */
	double min_pos_limit;	/* lower soft limit on joint pos */
	double vel_limit;	/* upper limit of joint speed */
	double acc_limit;	/* upper limit of joint accel */
	double min_ferror;	/* zero speed following error limit */
	double max_ferror;	/* max speed following error limit */
	int switch_flags;	/* config flags for limit switches */
	double home_search_vel;	/* dir/spd to look for home switch */
	double home_latch_vel;	/* dir/spd to latch switch/index pulse */
	double home_offset;	/* dir/dist from switch to home point */
	double home;		/* joint coordinate of home point */
	int home_flags;		/* flags for various homing options */
	double backlash;	/* amount of backlash */
	emcmot_comp comp;	/* leadscrew correction data */

	/* status info - changes regularly */
	/* many of these need to be made available to higher levels */
	/* they can either be copied to the status struct, or an array of
	   joint structs can be made part of the status */
	EMCMOT_AXIS_FLAG flag;	/* see above for bit details */
	double coarse_pos;	/* trajectory point, before interp */
	double pos_cmd;		/* commanded joint position */
	double vel_cmd;		/* comanded joint velocity */
	double backlash_corr;	/* correction for backlash */
	double backlash_filt;	/* filtered backlash correction */
	double motor_pos_cmd;	/* commanded position, with comp */
	double motor_pos_fb;	/* position feedback, with comp */
	double pos_fb;		/* position feedback, comp removed */
	double ferror;		/* following error */
	double ferror_limit;	/* limit depends on speed */
	double ferror_high_mark;	/* max following error */
	double free_pos_cmd;	/* position command for free mode TP */
	double free_vel_lim;	/* velocity limit for free mode TP */
	int free_tp_enable;	/* if zero, joint stops ASAP */
	int free_tp_active;	/* if non-zero, move in progress */

	/* internal info - changes regularly, not usually accessed from user
	   space */
	CUBIC_STRUCT cubic;	/* cubic interpolator data */

	double pos_limit_pos;	/* latched position of limit sw */
	int pos_limit_latch;	/* non-zero if on limit */
	double neg_limit_pos;	/* latched position of limit sw */
	int neg_limit_latch;	/* non-zero if on limit */
	double home_sw_pos;	/* latched position of home sw */
	int home_pause_timer;	/* used to delay between homing states */
	char home_sw_old;	/* previous value, for edge detection */
	int index_pulse;	/* current state of index pulse input */
	int index_pulse_edge;	/* non-zero if rising edge detected */

	home_state home_state;	/* state machine for homing */
	double motor_offset;	/* diff between internal and motor pos, used
				   to set position to zero during homing */

	/* stuff moved from the other structs that might be needed (or might
	   not!) */
	double big_vel;		/* used for "debouncing" velocity */
    } emcmot_joint;

/* This structure contains only the "status" data associated with
   a joint.  "Status" data is that data that should be reported to
   user space on a continuous basis.  An array of these structs is
   part of the main status structure, and is filled in with data
   copied from the emcmot_joint structs every servo period.

   For now this struct contains more data than it really needs, but
   paring it down will take time (and probably needs to be done one
   or two items at a time, with much testing).  My main goal right
   now is to get get the large joint struct out of status.

*/
    typedef struct {

	EMCMOT_AXIS_FLAG flag;	/* see above for bit details */
	double pos_cmd;		/* commanded joint position */
	double pos_fb;		/* position feedback, comp removed */
	double ferror;		/* following error */
	double ferror_high_mark;	/* max following error */

/*! \todo FIXME - the following are not really "status", but taskintf.cc expects
   them to be in the status structure.  I don't know how or if they are
   used by the user space code.  Ideally they will be removed from here,
   but each one will need to be investigated individually.
*/
	double backlash;	/* amount of backlash */
	double max_pos_limit;	/* upper soft limit on joint pos */
	double min_pos_limit;	/* lower soft limit on joint pos */
	double min_ferror;	/* zero speed following error limit */
	double max_ferror;	/* max speed following error limit */
	double home_offset;	/* dir/dist from switch to home point */
    } emcmot_joint_status;

/*********************************
        STATUS STRUCTURE
*********************************/

/* This is the status structure.  There is one of these in shared
   memory, and it reports motion controller status to higher level
   code in user space.  For the most part, this structure contains
   higher level variables - low level stuff is made visible to the
   HAL and troubleshooting, etc, is done using the HAL oscilliscope.
*/

/*! \todo FIXME - this struct is broken into two parts... at the top are
   structure members that I understand, and that are needed for emc2.
   Other structure members follow.  All the later ones need to be
   evaluated - either they move up, or they go away.
*/

typedef struct
{
  unsigned char head;           /* flag count for mutex detect */

  /* dynamic status-- changes every cycle */
  unsigned int heartbeat;
  int config_num;		/* incremented whenever configuration changed. */
  double computeTime;
  EmcPose pos;                   /* calculated Cartesian position */
  double axisPos[EMCMOT_MAX_AXIS]; /* calculated axis positions */
  double output[EMCMOT_MAX_AXIS];
  double input[EMCMOT_MAX_AXIS]; /* actual input */
  EmcPose actualPos;             /* actual Cartesian position */
  int id;                       /* id for executing motion */
  int depth;                    /* motion queue depth */
  int activeDepth;              /* depth of active blend elements */
  int queueFull;
  EMCMOT_MOTION_FLAG motionFlag;
  EMCMOT_AXIS_FLAG axisFlag[EMCMOT_MAX_AXIS];
  int paused;
  int overrideLimits;           /* non-zero means limits are ignored */
  int logPoints;                /* how many points currently in log */

  /* static status-- only changes upon input commands, e.g., config */
  int commandEcho;              /* echo of input command */
  int commandNumEcho;           /* echo of input command number */
  unsigned char commandStatus;  /* one of EMCMOT_COMMAND_ defined above */
  double outputScale[EMCMOT_MAX_AXIS];
  double outputOffset[EMCMOT_MAX_AXIS];
  double inputScale[EMCMOT_MAX_AXIS];
  double inputOffset[EMCMOT_MAX_AXIS]; /* encoder offsets */
  double qVscale;               /* traj velocity scale factor */
  double axVscale[EMCMOT_MAX_AXIS]; /* axis velocity scale factor */
  double vel;                   /* scalar max vel */
  double acc;                   /* scalar max accel */
  int logOpen;
  int logStarted;
  int logSize;                  /* size in entries, not bytes */
  int logSkip;
  int logType;                  /* type being logged */
  int logTriggerType;           /* 0=manual, 1 =abs(change) > threshold,
				   2=var < threshold, 3 var>threshold */
  int logTriggerVariable;       /* The variable(s) that can cause the log to 
				   trigger. */
  double logTriggerThreshold;   /* The value for non manual triggers. */
  double logStartVal;   /* value use for delta trigger */
  int probeTripped;             /* Has the probe signal changed since
                                 start of probe command? */
  int probeval;                 /* current value of probe wire */
  int probing;                  /* Currently looking for a probe signal? */
  EmcPose probedPos;             /* Axis positions stored as soon as possible
                                   after last probeTripped */
  int level;
  unsigned char tail;           /* flag count for mutex detect */
} EMCMOT_STATUS;

/*********************************
        CONFIG STRUCTURE
*********************************/

/* This is the config structure.  This is currently in shared memory,
   but I have no idea why... there are commands to set most of the
   items in this structure.  It seems we should either put the struct
   in private memory and manipulate it with commands, or we should
   put it in shared memory and manipulate it directly - not both.
   The structure contains static or rarely changed information that
   describes the machine configuration.

   later: I think I get it now - the struct is in shared memory so
   user space can read the config at any time, but commands are used
   to change the config so they only take effect when the realtime
   code processes the command.
*/

/*! \todo FIXME - this struct is broken into two parts... at the top are
   structure members that I understand, and that are needed for emc2.
   Other structure members follow.  All the later ones need to be
   evaluated - either they move up, or they go away.
*/
typedef struct 
{
  unsigned char head;                   /* flag count for mutex detect */
  int config_num;	        	/* Incremented everytime configuration changed, should match status.config_num */
  EMCMOT_AXIS_FLAG axisPolarity[EMCMOT_MAX_AXIS];
  int numAxes;
  double trajCycleTime;
  double servoCycleTime;
  int interpolationRate;
  double maxLimit[EMCMOT_MAX_AXIS];     /* maximum axis limits, counts */
  double minLimit[EMCMOT_MAX_AXIS];     /* minimum axis limits, counts */
  double minOutput[EMCMOT_MAX_AXIS];    /* minimum output value allowed, volts */
  double maxOutput[EMCMOT_MAX_AXIS];    /* maximum output value allowed, volts */
  double minFerror[EMCMOT_MAX_AXIS];    /* minimum allowable following error */
  double maxFerror[EMCMOT_MAX_AXIS];    /* maximum allowable following error */
  double limitVel;                      /* scalar upper limit on vel */
  double axisLimitVel[EMCMOT_MAX_AXIS];	/* scalar upper limit on axis vels */
  double axisAcc[EMCMOT_MAX_AXIS];	/* axis acceleration */
  double homingVel[EMCMOT_MAX_AXIS];    /* scalar max homing vels */
  double homeOffset[EMCMOT_MAX_AXIS];   /* where to go after home, user units */
  int probeIndex;                       /* Which wire has the probe signal? */
  int probePolarity;                    /* Look for 0 or 1. */
  KINEMATICS_TYPE kinematics_type;
  PID_STRUCT pid[EMCMOT_MAX_AXIS];
  int STEPPING_TYPE;                    /* 0 = step/direction, 1 = phasing */
  double setup_time[EMCMOT_MAX_AXIS];      /* number of periods before step occurs that dir changes */
  double hold_time[EMCMOT_MAX_AXIS];       /* number of periods that step line is held low/high after transition */
  int PERIOD;                           /* fundamental period for timer interrupts */
  int IO_BASE_ADDRESS;
  int debug;		            	/* copy of DEBUG, from .ini file */

  unsigned char tail;	        	/* flag count for mutex detect */
} EMCMOT_CONFIG;

/*********************************
      INTERNAL STRUCTURE
*********************************/

/* This is the internal structure.  It contains stuff that is used
   internally by the motion controller that does not need to be in
   shared memory.  It will wind up with a lot of the stuff that got
   tossed into the debug structure.

   FIXME - so far most if the stuff that was tossed in here got
   moved back out, maybe don't need it after all?
*/

    typedef struct {
	unsigned char head;	/* flag count for mutex detect */

	int probe_debounce_cntr;
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_internal;

/*********************************
        DEBUG STRUCTURE
*********************************/

/* This is the debug structure.  I guess it was intended to make some
   of the motion controller's internal variables visible from user
   space for debugging, but it has evolved into a monster.
   180K last time I checked - most of it (174K or so) is the traj
   planner queues... each entry is 720 bytes, and there are 210
   entries in the main queue.
   I'll figure it out eventually though.
   Low level things will be exported thru the HAL so they can be
   monitored with halscope.  High level things will remain here,
   and things that are internal will be moved to a private structure.
*/

/*! \todo FIXME - this struct is broken into two parts... at the top are
   structure members that I understand, and that are needed for emc2.
   Other structure members follow.  All the later ones need to be
   evaluated - either they move up, or they go away.
*/

/*! \todo FIXME - this has become a dumping ground for all kinds of stuff */
typedef struct
{
  unsigned char head;           /* flag count for mutex detect */
  double tMin, tMax, tAvg;      /* trajectory min, max, avg times */
  double sMin, sMax, sAvg;      /* servo min, max, avg times */
  double nMin, nMax, nAvg;      /* min, max, avg times in DISABLED mode */
  double yMin, yMax, yAvg;      /* min, max, avg times cycle times rather than compute  */
  double fMin, fMax, fAvg;      /* min, max, avg times frequency */
  double fyMin, fyMax, fyAvg;      /* min, max, avg times frequency cycle times rather than compute  */

  EMC_TELEOP_DATA teleop_data;
  double ferrorCurrent[EMCMOT_MAX_AXIS]; /* current  following error */
  double ferrorHighMark[EMCMOT_MAX_AXIS]; /* magnitude of max following error */
  int split;                    /* number of split command reads */
  int stepperCount[EMCMOT_MAX_AXIS]; /* and rest are 0 */

  int pdmult[EMCMOT_MAX_AXIS];
  int enable[EMCMOT_MAX_AXIS];

  /* flag for enabling, disabling watchdog; multiple for down-stepping */
  int wdEnabling ;
  int wdEnabled ;
  int wdWait ;
  int wdCount ;
  unsigned char wdToggle ;
  
  /* flag that all active axes are homed */
  unsigned char allHomed ;
  
  /* values for joint home positions */
  double jointHome[EMCMOT_MAX_AXIS];

  int maxLimitSwitchCount[EMCMOT_MAX_AXIS];
  int minLimitSwitchCount[EMCMOT_MAX_AXIS];
  int ampFaultCount[EMCMOT_MAX_AXIS];

  TP_STRUCT queue;         /* coordinated mode planner */
 TP_STRUCT freeAxis[EMCMOT_MAX_AXIS];
 EmcPose freePose;
 CUBIC_STRUCT cubic[EMCMOT_MAX_AXIS];

/* space for trajectory planner queues, plus 10 more for safety */
/* FIXME-- default is used; dynamic is not honored */
 TC_STRUCT queueTcSpace[DEFAULT_TC_QUEUE_SIZE + 10];
#define FREE_AXIS_QUEUE_SIZE 4  /* don't really queue free axis motion */
 TC_STRUCT freeAxisTcSpace[EMCMOT_MAX_AXIS][FREE_AXIS_QUEUE_SIZE];

 double rawInput[EMCMOT_MAX_AXIS];        /* raw feedback from sensors */
 double rawOutput[EMCMOT_MAX_AXIS]; /* raw output to actuators */

 double coarseJointPos[EMCMOT_MAX_AXIS];  /* trajectory point, in joints */
 double jointPos[EMCMOT_MAX_AXIS]; /* interpolated point, in joints */
 double jointVel[EMCMOT_MAX_AXIS]; /* joint velocity */
 double oldJointPos[EMCMOT_MAX_AXIS]; /* ones from last cycle, for vel */
 double outJointPos[EMCMOT_MAX_AXIS]; /* rounded and backlash-comped */
 double oldInput[EMCMOT_MAX_AXIS]; /* ones for actual pos, last cycle */
  char oldInputValid[EMCMOT_MAX_AXIS];
/* inverseInputScale[] is 1/inputScale[], and lets us use a multiplication
   instead of a division each servo cycle */
 double inverseInputScale[EMCMOT_MAX_AXIS];
 double inverseOutputScale[EMCMOT_MAX_AXIS];
 EmcPose oldPos;           /* last position, used for vel differencing */
 EmcPose oldVel, newVel;   /* velocities, used for acc differencing */
 EmcPose newAcc;           /* differenced acc */

/* value of speed past which we debounce the feedback */
 double bigVel[EMCMOT_MAX_AXIS];

 int homingPhase[EMCMOT_MAX_AXIS]; /*flags for homing */
 int latchFlag[EMCMOT_MAX_AXIS]; /* flags for axis latched */
 double saveLatch[EMCMOT_MAX_AXIS]; /* saved axis latch values */

 int enabling;        /* starts up disabled */
 int coordinating;    /* starts up in free mode */
 int teleoperating  ;    /* starts up in free mode */

 int wasOnLimit;      /* non-zero means we already aborted
                                   everything due to a soft
                                   limit, and need not re-abort. It's
                                   cleared only when all limits are
                                   cleared. */
 int onLimit;         /* non-zero means at least one axis is
                                   on a soft limit */

 int overriding;      /* non-zero means we've initiated an axis
                                   move while overriding limits */

 int stepping;	/* When true, motion is single stepped through queued motion */
 int idForStep; /* Used in conjunction with stepping - When idForStep != current motion ID, motion is paused. */

  /* min-max-avg structs for traj and servo cycles */
  MMXAVG_STRUCT tMmxavg;
  MMXAVG_STRUCT sMmxavg;
  MMXAVG_STRUCT nMmxavg;
  MMXAVG_STRUCT yMmxavg;
  MMXAVG_STRUCT fMmxavg;
  MMXAVG_STRUCT fyMmxavg;
  
  double tMmxavgSpace[DEFAULT_MMXAVG_SIZE];
  double sMmxavgSpace[DEFAULT_MMXAVG_SIZE];
  double nMmxavgSpace[DEFAULT_MMXAVG_SIZE];
  double yMmxavgSpace[DEFAULT_MMXAVG_SIZE];
  double fMmxavgSpace[DEFAULT_MMXAVG_SIZE];
  double fyMmxavgSpace[DEFAULT_MMXAVG_SIZE];

  double start_time;
  double running_time;
  double cur_time;
  double last_time;

    /* backlash stuff */
  double bcomp[EMCMOT_MAX_AXIS];  /* backlash comp value */
  char bcompdir[EMCMOT_MAX_AXIS]; /* 0=none, 1=pos, -1=neg */
  double bcompincr[EMCMOT_MAX_AXIS];  /* incremental backlash comp */
  char bac_done[EMCMOT_MAX_AXIS]; 
  double bac_d[EMCMOT_MAX_AXIS];
  double bac_di[EMCMOT_MAX_AXIS];
  double bac_D[EMCMOT_MAX_AXIS];
  double bac_halfD[EMCMOT_MAX_AXIS];
  double bac_incrincr[EMCMOT_MAX_AXIS];
  double bac_incr[EMCMOT_MAX_AXIS];

  unsigned char tail;		/* flag count for mutex detect */
} EMCMOT_DEBUG;

/* error structure - A ring buffer used to pass formatted printf stings to usr space */
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */
	char error[EMCMOT_ERROR_NUM][EMCMOT_ERROR_LEN];
	int start;		/* index of oldest error */
	int end;		/* index of newest error */
	int num;		/* number of items */
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_error;

/* big comm structure, for upper memory */
    typedef struct {
	emcmot_command command;	/* struct used to pass commands/data
					   to the RT module from usr space */
	emcmot_status status;	/* Struct used to store RT status */
	emcmot_config config;	/* Struct used to store RT config */
	emcmot_debug debug;	/* Struct used to store RT status and debug
				   data - 2nd largest block */
	emcmot_internal internal;	/*! \todo FIXME - doesn't need to be in
					   shared memory */
	emcmot_error error;	/* ring buffer for error messages */
	emcmot_log log;	/* a massive ring buffer for logging RT data */
    } emcmot_struct;

/*
  function prototypes for emcmot code
*/

/* error ring buffer access functions */
    extern int emcmotErrorInit(emcmot_error * errlog);
    extern int emcmotErrorPut(emcmot_error * errlog, const char *error);
    extern int emcmotErrorGet(emcmot_error * errlog, char *error);

#ifdef __cplusplus
}
#endif
#endif				/* EMCMOT_H */
