/********************************************************************
* Description: motion.h
*   Data structures used throughout emc2.
*
* Author:
* License: GPL Version 2
* Created on:
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
*
********************************************************************/

/* This file is a mess! */

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

/* define NEW_STRUCTS if you want to use the new
   structure definitions - eventually the old ones
   will be deleted, and this line and the #ifdefs
   can be removed.
*/

#define NEW_STRUCTS

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _EMC_TELEOP_DATA {
	EmcPose currentVel;
	EmcPose currentAccell;
	EmcPose desiredVel;
	EmcPose desiredAccell;
    } EMC_TELEOP_DATA;

/* This enum lists all the possible commands */

    typedef enum {
	EMCMOT_ABORT = 1,	/* abort motion */
	EMCMOT_ENABLE,		/* enable servos for active axes */
	EMCMOT_DISABLE,		/* disable servos for active axes */
	EMCMOT_ENABLE_AMPLIFIER,	/* enable amp outputs */
	EMCMOT_DISABLE_AMPLIFIER,	/* disable amp outputs */
	EMCMOT_ENABLE_WATCHDOG,	/* enable watchdog sound, parport */
	EMCMOT_DISABLE_WATCHDOG,	/* enable watchdog sound, parport */
	EMCMOT_ACTIVATE_JOINT,	/* make axis active */
	EMCMOT_DEACTIVATE_JOINT,	/* make axis inactive */

	EMCMOT_PAUSE,		/* pause motion */
	EMCMOT_RESUME,		/* resume motion */
	EMCMOT_STEP,		/* resume motion until id encountered */
	EMCMOT_FREE,		/* set mode to free (joint) motion */
	EMCMOT_COORD,		/* set mode to coordinated motion */
	EMCMOT_TELEOP,		/* set mode to teleop */

	EMCMOT_OPEN_LOG,	/* open a log */
	EMCMOT_START_LOG,	/* start logging */
	EMCMOT_STOP_LOG,	/* stop logging */
	EMCMOT_CLOSE_LOG,	/* close log */

	EMCMOT_SCALE,		/* scale the speed */
	EMCMOT_OVERRIDE_LIMITS,	/* temporarily ignore limits until jog done */

	EMCMOT_HOME,		/* home an axis */
	EMCMOT_JOG_CONT,	/* continuous jog */
	EMCMOT_JOG_INCR,	/* incremental jog */
	EMCMOT_JOG_ABS,		/* absolute jog */
	EMCMOT_SET_LINE,	/* queue up a linear move */
	EMCMOT_SET_CIRCLE,	/* queue up a circular move */
	EMCMOT_SET_TELEOP_VECTOR,	/* Move at a given velocity but in
					   world cartesian coordinates, not
					   in joint space like EMCMOT_JOG_* */

	EMCMOT_CLEAR_PROBE_FLAGS,	/* clears probeTripped flag */
	EMCMOT_PROBE,		/* go to pos, stop if probe trips, record
				   trip pos */

	EMCMOT_SET_POSITION_LIMITS,	/* set the axis position +/- limits */
	EMCMOT_SET_MIN_FERROR,	/* minimum following error, input units */
	EMCMOT_SET_MAX_FERROR,	/* maximum following error, input units */
	EMCMOT_SET_VEL,		/* set the velocity for subsequent moves */
	EMCMOT_SET_VEL_LIMIT,	/* set the max vel for all moves (tooltip) */
	EMCMOT_SET_JOINT_VEL_LIMIT,	/* set the max axis vel */
	EMCMOT_SET_JOINT_ACC_LIMIT,	/* set the max axis accel */
	EMCMOT_SET_ACC,		/* set the max accel for moves (tooltip) */
	EMCMOT_SET_TERM_COND,	/* set termination condition (stop, blend) */
	EMCMOT_SET_HOMING_VEL,	/* set the axis homing speed */
	EMCMOT_SET_NUM_AXES,	/* set the number of axes */
	EMCMOT_SET_WORLD_HOME,	/* set pose for world home */
	EMCMOT_SET_JOINT_HOME,	/* set value for joint homes */
	EMCMOT_SET_HOME_OFFSET,	/* where to go after a home */
	EMCMOT_SET_DEBUG	/* sets the debug level */
    } cmd_code_t;

/* this enum lists the possible results of a command */

    typedef enum {
	EMCMOT_COMMAND_OK = 0,	/* cmd honored */
	EMCMOT_COMMAND_UNKNOWN_COMMAND,	/* cmd not understood */
	EMCMOT_COMMAND_INVALID_COMMAND,	/* cmd can't be handled now */
	EMCMOT_COMMAND_INVALID_PARAMS,	/* bad cmd params */
	EMCMOT_COMMAND_BAD_EXEC	/* error trying to initiate */
    } cmd_status_t;

/* termination conditions for queued motions */
#define EMCMOT_TERM_COND_STOP 1
#define EMCMOT_TERM_COND_BLEND 2

/*********************************
       COMMAND STRUCTURE
*********************************/

/* This is the command structure.  There is one of these in shared
   memory, and all commands from higher level code come thru it.
*/
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */
	cmd_code_t command;	/* command code (enum) */
	int commandNum;		/* increment this for new command */
	double maxLimit;	/* pos value for position limit, output */
	double minLimit;	/* neg value for position limit, output */
	EmcPose pos;		/* end for line, circle */
	PmCartesian center;	/* center for circle */
	PmCartesian normal;	/* normal vec for circle */
	int turn;		/* turns for circle */
	double vel;		/* max velocity */
	double acc;		/* max acceleration */
	int id;			/* id for motion */
	int termCond;		/* termination condition */
	int axis;		/* which index to use for below */
/* FIXME - logging stuff will be radically reduced later */
	int logSize;		/* size for log fifo */
	int logSkip;		/* how many to skip, 0 means log all, -1
				   means don't log on cycles */
	int logType;		/* type for logging */
	int logTriggerType;	/* see enum LOG_TRIGGER_TYPES */
	int logTriggerVariable;	/* the variable(s) that can cause the log to
				   trigger. se enum LOG_TRIGGER_VARS */
	double logTriggerThreshold;	/* the value for non manual triggers */

	double scale;		/* velocity scale arg */
/* FIXME - offset might go away */
	double offset;		/* input or output offset arg */
	double minFerror;	/* min following error */
	double maxFerror;	/* max following error */
	int wdWait;		/* cycle to wait before toggling wd */
	int debug;		/* debug level, from DEBUG in .ini file */
	unsigned char out, start, end;	/* motion index, start, and end bits */
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_command_t;

/* FIXME - these packed bits might be replaced with chars
   memory is cheap, and being able to access them without those
   damn macros would be nice
*/

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
  | AF | FE |   | HD | H | HS | NHL | PHL | NSL | PSL | ER | IP | AC | EN |
  ----------^-----------------^-----------------------^-------------------^
    *         x        *   *     *     *                               *

  x = unused
  * = has a polarity associated with it

  where:

  EN  is 1 if axis amplifier is enabled, 0 if not
  AC  is 1 if axis is active for calculations, 0 if not
  IP  is 1 if axis is in position, 0 if not
  ER  is 1 if axis has an error, 0 if not

  PSL is 1 if axis is on maximum software limit, 0 if not
  NSL is 1 if axis is on minimum software limit, 0 if not
  PHL is 1 if axis is on maximum hardware limit, 0 if not
  NHL is 1 if axis is on minimum hardware limit, 0 if not

  HS  is 1 if axis home switch is tripped, 0 if not
  H   is 1 if axis is homing, 0 if not
  HD  is 1 if axis has been homed, 0 if not

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

#define EMCMOT_AXIS_FERROR_BIT         0x1000
#define EMCMOT_AXIS_FAULT_BIT          0x2000

/* FIXME - the terms "teleop", "coord", and "free" are poorly
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
    } emcmot_comp_t;

/* states for homing - this list is incomplete */
    typedef enum {
	EMCMOT_NOT_HOMING = 0,
	EMCMOT_HOME_START,
	EMCMOT_SET_COARSE_HOME,
	EMCMOT_SET_FINAL_HOME,
	EMCMOT_START_MOVE_TO_HOME,
	EMCMOT_WAITFOR_MOVE_TO_HOME,
	EMCMOT_HOME_FINISHED
    } home_state_t;

/* NEW_STRUCTS  - I'm trying something different.  As it stands,
   per-axis (really per-joint) info is stored in a whole bunch of
   individual arrays, each with [EMCMOT_MAX_AXIS] values.  Some
   arrays store config info, and are in the config struct, others
   store status info and are in the status struct, some are in
   the debug struct, etc, etc.

   The alternative is to put ALL data for a joint in a single
   joint data structure.  Then when it's time to work on a joint,
   just get a pointer to the struct and access the members.
   The code can be cleaner and easier to read, and it will run
   faster because the array indexing overhead is gone and things
   that are accessed together are near each other in memory and
   thus in cache.
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
	double home_search_vel;	/* dir/spd to look for home switch */
	double home_index_vel;	/* dir/spd to latch switch/index pulse */
	double home_offset;	/* dir/dist from switch to zero point */
	double backlash;	/* amount of backlash */
	emcmot_comp_t comp;	/* leadscrew correction data */

	/* status info - changes regularly */
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
	int free_tp_enable;	/* if zero, axis stops ASAP */

	/* internal info - changes regularly, not usually accessed from user
	   space */
	CUBIC_STRUCT cubic;	/* cubic interpolator data */
	int pos_limit_debounce;	/* debounce counters for switches */
	int neg_limit_debounce;
	int home_sw_debounce;
	int amp_fault_debounce;	/* debounce counter for fault input */
	home_state_t home_state;	/* state machine for homing */
	double motor_offset;	/* diff between internal and motor pos, used
				   to set position to zero during homing */

	/* stuff moved from the other structs that might be needed (or might
	   not!) */
	double joint_home;	/* how is this different than home_offset? */
	double big_vel;		/* used for "debouncing" velocity */
//	double vel_scale;	/* axis velocity scale factor */

    } emcmot_joint_t;

/* FIXME - the beginnings of a state machine */

    typedef enum {
	EMCMOT_MOTION_DISABLED = 0,
	EMCMOT_MOTION_FREE,
	EMCMOT_MOTION_TELEOP,
	EMCMOT_MOTION_COORD
    } motion_state_t;

/*********************************
        STATUS STRUCTURE
*********************************/

/* This is the status structure.  There is one of these in shared
   memory, and it reports motion controller status to higher level
   code in user space.  For the most part, this structure contains
   higher level variables - low level stuff is made visible to the
   HAL and troubleshooting, etc, is done using the HAL oscilliscope.
*/

/* FIXME - this struct is broken into two parts... at the top are
   structure members that I understand, and that are needed for emc2.
   Other structure members follow.  All the later ones need to be
   evaluated - either they move up, or they go away.
*/

    typedef struct {
	unsigned char head;	/* flag count for mutex detect */
	/* these three are updated only when a new command is handled */
	cmd_code_t commandEcho;	/* echo of input command */
	int commandNumEcho;	/* echo of input command number */
	cmd_status_t commandStatus;	/* result of most recent command */
	/* these are config info, updated when a command changes them */
	double qVscale;		/* velocity scale factor for all motion */
	/* the rest are updated every cycle */
	EMCMOT_MOTION_FLAG motionFlag;	/* see above for bit details */
#ifndef NEW_STRUCTS
	EMCMOT_AXIS_FLAG axisFlag[EMCMOT_MAX_AXIS];	/* see above for bit
							   details */
	double joint_pos_cmd[EMCMOT_MAX_AXIS];	/* replaces "axisPos" */
	double joint_pos_fb[EMCMOT_MAX_AXIS];	/* replaces "input" */
	double joint_vel_cmd[EMCMOT_MAX_AXIS];	/* replaces "output" */
	double ferrorCurrent[EMCMOT_MAX_AXIS];	/* current following error */
	double ferrorLimit[EMCMOT_MAX_AXIS];	/* allowable following error */
	double ferrorHighMark[EMCMOT_MAX_AXIS];	/* max following error */
#endif
	int onSoftLimit;	/* non-zero if any axis is on soft limit */

	int probeVal;		/* debounced value of probe input */

/* FIXME - all structure members beyond this point are in limbo */

	/* dynamic status-- changes every cycle */
	unsigned int heartbeat;
	int config_num;		/* incremented whenever configuration
				   changed. */
	double computeTime;
	EmcPose pos;		/* calculated Cartesian position */
	EmcPose actualPos;	/* actual Cartesian position */
	int id;			/* id for executing motion */
	int depth;		/* motion queue depth */
	int activeDepth;	/* depth of active blend elements */
	int queueFull;		/* Flag to indicate the tc queue is full */
	int paused;		/* Flag to signal motion paused */
	int overrideLimits;	/* non-zero means limits are ignored */
	int logPoints;		/* how many points currently in log */

	/* static status-- only changes upon input commands, e.g., config */
#ifndef NEW_STRUCTS
	double axVscale[EMCMOT_MAX_AXIS];	/* axis velocity scale factor 
						 */
#endif
	double vel;		/* scalar max vel */
	double acc;		/* scalar max accel */

	int logOpen;		/* Logging stuff that will eventually end up
				   in hal_scope */
	int logStarted;
	int logSize;		/* size in entries, not bytes */
	int logSkip;
	int logType;		/* type being logged */
	int logTriggerType;	/* 0=manual, 1 =abs(change) > threshold,
				   2=var < threshold, 3 var>threshold */
	int logTriggerVariable;	/* The variable(s) that can cause the log to
				   trigger. */
	double logTriggerThreshold;	/* The value for non manual triggers. 
					 */
	double logStartVal;	/* value use for delta trigger */

	int probeTripped;	/* Has the probe signal changed since start
				   of probe command? */
	int probing;		/* Currently looking for a probe signal? */
	EmcPose probedPos;	/* Axis positions stored as soon as possible
				   after last probeTripped */
	int level;
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_status_t;

/*********************************
        CONFIG STRUCTURE
*********************************/
#ifndef NEW_STRUCTS
/* first a sub-structure with config info for a single joint */

    typedef struct {
	int type;		/* 0 = linear, 1 = rotary */
	double max_pos_limit;	/* upper soft limit on joint pos */
	double min_pos_limit;	/* lower soft limit on joint pos */
	double vel_limit;	/* upper limit of joint speed */
	double acc_limit;	/* upper limit of joint accel */
	double min_ferror;	/* zero speed following error limit */
	double max_ferror;	/* max speed following error limit */
	double home_search_vel;	/* dir/spd to look for home switch */
	double home_index_vel;	/* dir/spd to latch switch/index pulse */
	double home_offset;	/* dir/dist from switch to zero point */

	double backlash;

    } emcmot_joint_config_t;
#endif

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

/* FIXME - this struct is broken into two parts... at the top are
   structure members that I understand, and that are needed for emc2.
   Other structure members follow.  All the later ones need to be
   evaluated - either they move up, or they go away.
*/
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */

/* FIXME - all structure members beyond this point are in limbo */

	int config_num;		/* Incremented everytime configuration
				   changed, should match status.config_num */
	int numAxes;		/* The number of axes in the system (which
				   must be between 1 and EMCMOT_MAX_AXIS,
				   inclusive). Allegedly, holds a copy of the
				   global num_axes - seems daft to maintain
				   duplicates ! */

	double trajCycleTime;	/* the rate at which the trajectory loop
				   runs.... (maybe) */
	double servoCycleTime;	/* the rate of the servo loop - Not the same
				   as the traj time */

	int interpolationRate;	/* grep control.c for an explanation....
				   approx line 50 */

	double limitVel;	/* scalar upper limit on vel */
	KINEMATICS_TYPE kinematics_type;
	int STEPPING_TYPE;	/* 0 = step/direction, 1 = phasing */
	int PERIOD;		/* fundamental period for timer interrupts */
	unsigned long int IO_BASE_ADDRESS;
	int debug;		/* copy of DEBUG, from .ini file */
#ifndef NEW_STRUCTS
	emcmot_joint_config_t joints[EMCMOT_MAX_AXIS];	/* config info for
							   each joint */
#endif
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_config_t;

/*********************************
      INTERNAL STRUCTURE
*********************************/

/* This is the internal structure.  It contains stuff that is used
   internally by the motion controller that does not need to be in
   shared memory.  It will wind up with a lot of the stuff that got
   tossed into the debug structure.
*/

    typedef struct {
	unsigned char head;	/* flag count for mutex detect */

	int probe_debounce_cntr;
#ifndef NEW_STRUCTS
	double old_joint_pos_cmd[EMCMOT_MAX_AXIS];
	int pos_limit_debounce_cntr[EMCMOT_MAX_AXIS];
	int neg_limit_debounce_cntr[EMCMOT_MAX_AXIS];
	int home_sw_debounce_cntr[EMCMOT_MAX_AXIS];
	int amp_fault_debounce_cntr[EMCMOT_MAX_AXIS];
	double ferrorAbs[EMCMOT_MAX_AXIS];	/* absolute val of ferror */
#endif
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_internal_t;

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

/* FIXME - this struct is broken into two parts... at the top are
   structure members that I understand, and that are needed for emc2.
   Other structure members follow.  All the later ones need to be
   evaluated - either they move up, or they go away.
*/

/* FIXME - this has become a dumping ground for all kinds of stuff */

    typedef struct {
	unsigned char head;	/* flag count for mutex detect */

/* FIXME - all structure members beyond this point are in limbo */

	double tMin, tMax, tAvg;	/* trajectory min, max, avg times */
	double sMin, sMax, sAvg;	/* servo min, max, avg times */
	double nMin, nMax, nAvg;	/* min, max, avg times in DISABLED
					   mode */
	double yMin, yMax, yAvg;	/* min, max, avg times cycle times
					   rather than compute */
	double fMin, fMax, fAvg;	/* min, max, avg times frequency */
	double fyMin, fyMax, fyAvg;	/* min, max, avg times frequency
					   cycle times rather than compute */

	EMC_TELEOP_DATA teleop_data;
	int split;		/* number of split command reads */
#ifndef NEW_STRUCTS
	int enable[EMCMOT_MAX_AXIS];
#endif
	/* flag for enabling, disabling watchdog; multiple for down-stepping */
	int wdEnabling;
	int wdEnabled;
	int wdWait;
	int wdCount;
	unsigned char wdToggle;

	/* flag that all active axes are homed */
	unsigned char allHomed;
#ifndef NEW_STRUCTS
	/* values for joint home positions */
	double jointHome[EMCMOT_MAX_AXIS];
#endif
	/* Counters used for software debounce of the inputs */
//      int maxLimitSwitchCount[EMCMOT_MAX_AXIS];
//      int minLimitSwitchCount[EMCMOT_MAX_AXIS];
//      int ampFaultCount[EMCMOT_MAX_AXIS];

	TP_STRUCT queue;	/* coordinated mode planner */
#ifndef NEW_STRUCTS
	CUBIC_STRUCT cubic[EMCMOT_MAX_AXIS];
#endif

/* space for trajectory planner queues, plus 10 more for safety */
/* FIXME-- default is used; dynamic is not honored */
	TC_STRUCT queueTcSpace[DEFAULT_TC_QUEUE_SIZE + 10];

#ifndef NEW_STRUCTS
	double rawInput[EMCMOT_MAX_AXIS];	/* raw feedback from sensors */
	double rawOutput[EMCMOT_MAX_AXIS];	/* raw output to actuators */

	double coarseJointPos[EMCMOT_MAX_AXIS];	/* trajectory point, in
						   joints */
	double jointVel[EMCMOT_MAX_AXIS];	/* joint velocity */
	double oldJointPos[EMCMOT_MAX_AXIS];	/* ones from last cycle, for
						   vel */
	double outJointPos[EMCMOT_MAX_AXIS];	/* rounded and
						   backlash-comped */
	double oldInput[EMCMOT_MAX_AXIS];	/* ones for actual pos, last
						   cycle */
	char oldInputValid[EMCMOT_MAX_AXIS];
	/* inverseInputScale[] is 1/inputScale[], and lets us use a
	   multiplication instead of a division each servo cycle */
	double inverseInputScale[EMCMOT_MAX_AXIS];
	double inverseOutputScale[EMCMOT_MAX_AXIS];
#endif
	EmcPose oldPos;		/* last position, used for vel differencing */
	EmcPose oldVel, newVel;	/* velocities, used for acc differencing */
	EmcPose newAcc;		/* differenced acc */

#ifndef NEW_STRUCTS
	/* value of speed past which we debounce the feedback */
	double bigVel[EMCMOT_MAX_AXIS];	/* set to 10 * max Axis Velocity ...
					   IF we are at the point where the
					   encoder needs debouncing, the max
					   velocity of the axis has been
					   exceeded by a major margin ! */
	int homingPhase[EMCMOT_MAX_AXIS];	/* flags for homing */
	int latchFlag[EMCMOT_MAX_AXIS];	/* flags for axis latched */
	double saveLatch[EMCMOT_MAX_AXIS];	/* saved axis latch values */
#endif
	int enabling;		/* starts up disabled */
	int coordinating;	/* starts up in free mode */
	int teleoperating;	/* starts up in free mode */
#if 0
	int wasOnLimit;		/* non-zero means we already aborted
				   everything due to a soft limit, and need
				   not re-abort. It's cleared only when all
				   limits are cleared. */
	int onLimit;		/* non-zero means at least one axis is on a
				   soft limit */
#endif

	int overriding;		/* non-zero means we've initiated an axis
				   move while overriding limits */

	int stepping;
	int idForStep;

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
#if 0
	/* backlash stuff */
	double bcomp[EMCMOT_MAX_AXIS];	/* backlash comp value */
	char bcompdir[EMCMOT_MAX_AXIS];	/* 0=none, 1=pos, -1=neg */
	double bcompincr[EMCMOT_MAX_AXIS];	/* incremental backlash comp */
	char bac_done[EMCMOT_MAX_AXIS];
	double bac_d[EMCMOT_MAX_AXIS];
	double bac_di[EMCMOT_MAX_AXIS];
	double bac_D[EMCMOT_MAX_AXIS];
	double bac_halfD[EMCMOT_MAX_AXIS];
	double bac_incrincr[EMCMOT_MAX_AXIS];
	double bac_incr[EMCMOT_MAX_AXIS];
#endif
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_debug_t;

/* error structure - A ring buffer used to pass formatted printf stings to usr space */
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */
	char error[EMCMOT_ERROR_NUM][EMCMOT_ERROR_LEN];
	int start;		/* index of oldest error */
	int end;		/* index of newest error */
	int num;		/* number of items */
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_error_t;

/* big comm structure, for upper memory */
    typedef struct {
	emcmot_command_t command;	/* struct used to pass commands/data
					   to the RT module from usr space */
	emcmot_status_t status;	/* Struct used to store RT status */
	emcmot_config_t config;	/* Struct used to store RT config */
	emcmot_debug_t debug;	/* Struct used to store RT status and debug
				   data - 2nd largest block */
	emcmot_internal_t internal;	/* FIXME - doesn't need to be in
					   shared memory */
	emcmot_error_t error;	/* ring buffer for error messages */
	emcmot_log_t log;	/* a massive ring buffer for logging RT data */
#ifndef NEW_STRUCTS
	emcmot_comp_t comp[EMCMOT_MAX_AXIS];	/* corrections to be applied
						   to input pos */
#endif
#ifdef NEW_STRUCTS
	emcmot_joint_t joints[EMCMOT_MAX_AXIS];	/* all joint related data */
#endif
    } emcmot_struct_t;

/*
  function prototypes for emcmot code
*/

/* error ring buffer access functions */
    extern int emcmotErrorInit(emcmot_error_t * errlog);
    extern int emcmotErrorPut(emcmot_error_t * errlog, const char *error);
    extern int emcmotErrorGet(emcmot_error_t * errlog, char *error);

#ifdef __cplusplus
}
#endif
#endif				/* EMCMOT_H */
