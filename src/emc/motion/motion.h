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
#ifndef MOTION_H
#define MOTION_H

#include "posemath.h"		/* PmCartesian, PmPose, pmCartMag() */
#include "emcpos.h"
#include "emcpid.h"		/* PID_STRUCT */
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

/* motion flag type */
    typedef unsigned short EMCMOT_MOTION_FLAG;

/* axis flag type */
    typedef unsigned short EMCMOT_AXIS_FLAG;

/* commands */
    enum {
	EMCMOT_SET_TRAJ_CYCLE_TIME = 1,	/* set the cycle time */
	EMCMOT_SET_SERVO_CYCLE_TIME,	/* set the interpolation rate */
	EMCMOT_SET_POSITION_LIMITS,	/* set the axis position +/- limits */
	EMCMOT_SET_OUTPUT_LIMITS,	/* set the axis output +/- limits */
	EMCMOT_SET_OUTPUT_SCALE,	/* scale factor for outputs */
	EMCMOT_SET_INPUT_SCALE,	/* scale factor for inputs */
	EMCMOT_SET_MIN_FERROR,	/* minimum following error, input units */
	EMCMOT_SET_MAX_FERROR,	/* maximum following error, input units */
	EMCMOT_JOG_CONT,	/* continuous jog */
	EMCMOT_JOG_INCR,	/* incremental jog */
	EMCMOT_JOG_ABS,		/* absolute jog */
	EMCMOT_SET_LINE,	/* queue up a linear move */
	EMCMOT_SET_CIRCLE,	/* queue up a circular move */
	EMCMOT_SET_VEL,		/* set the velocity for subsequent moves */
	EMCMOT_SET_VEL_LIMIT,	/* set the absolute max vel for all moves */
	EMCMOT_SET_AXIS_VEL_LIMIT,	/* set the absolute max axis vel */
	EMCMOT_SET_ACC,		/* set the acceleration for moves */
	EMCMOT_PAUSE,		/* pause motion */
	EMCMOT_RESUME,		/* resume motion */
	EMCMOT_STEP,		/* resume motion until id encountered */
	EMCMOT_ABORT,		/* abort motion */
	EMCMOT_SCALE,		/* scale the speed */
	EMCMOT_ENABLE,		/* enable servos for active axes */
	EMCMOT_DISABLE,		/* disable servos for active axes */
	EMCMOT_SET_PID,		/* set PID gains */
	EMCMOT_ENABLE_AMPLIFIER,	/* enable amp outputs and dac writes */
	EMCMOT_DISABLE_AMPLIFIER,	/* disable amp outputs and dac writes 
					 */
	EMCMOT_OPEN_LOG,	/* open a log */
	EMCMOT_START_LOG,	/* start logging */
	EMCMOT_STOP_LOG,	/* stop logging */
	EMCMOT_CLOSE_LOG,	/* close log */
	EMCMOT_DAC_OUT,		/* write directly to the dacs */
	EMCMOT_HOME,		/* home an axis */
	EMCMOT_FREE,		/* set mode to free (joint) motion */
	EMCMOT_COORD,		/* set mode to coordinated motion */
	EMCMOT_TELEOP,		/* set mode to teleop */
	EMCMOT_ENABLE_WATCHDOG,	/* enable watchdog sound, parport */
	EMCMOT_DISABLE_WATCHDOG,	/* enable watchdog sound, parport */
	EMCMOT_SET_POLARITY,	/* set polarity for axis flags */
	EMCMOT_ACTIVATE_AXIS,	/* make axis active */
	EMCMOT_DEACTIVATE_AXIS,	/* make axis inactive */
	EMCMOT_SET_TERM_COND,	/* set termination condition (stop, blend) */
	EMCMOT_SET_HOMING_VEL,	/* set the axis homing speed */
	EMCMOT_SET_NUM_AXES,	/* set the number of axes */
	EMCMOT_SET_WORLD_HOME,	/* set pose for world home */
	EMCMOT_SET_JOINT_HOME,	/* set value for joint homes */
	EMCMOT_SET_HOME_OFFSET,	/* where to go after a home */
	EMCMOT_OVERRIDE_LIMITS,	/* temporarily ignore limits until jog done */
	EMCMOT_SET_TELEOP_VECTOR,	/* Move at a given velocity but in
					   world cartesian coordinates, not
					   in joint space like EMCMOT_JOG_* */
	EMCMOT_SET_PROBE_INDEX,	/* set which wire the probe signal is on. */
	EMCMOT_SET_PROBE_POLARITY,	/* probe tripped on 0 to 1 transition 
					   or on 1 to 0 transition. */
	EMCMOT_CLEAR_PROBE_FLAGS,	/* clears probeTripped flag */
	EMCMOT_PROBE,		/* go towards a position, stop if the probe
				   is tripped, and record the position where
				   the probe tripped */
	EMCMOT_SET_DEBUG,	/* sets the debug level */
	EMCMOT_SET_AOUT,	/* sets an analog motion point for next move */
	EMCMOT_SET_DOUT,	/* sets a digital motion point for next move */
	EMCMOT_SET_STEP_PARAMS	/* sets setup_time and hold_time for freqtask 
				 */
    };

/* termination conditions for queued motions */
#define EMCMOT_TERM_COND_STOP 1
#define EMCMOT_TERM_COND_BLEND 2

/* command struct */
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */
	int command;		/* one of the command enums above */
	int commandNum;		/* increment this for new command */
	double cycleTime;	/* planning time (not servo time) */
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
	PID_STRUCT pid;		/* gains */
	int logSize;		/* size for log fifo */
	int logSkip;		/* how many to skip, 0 means log all, -1
				   means don't log on cycles */
	int logType;		/* type for logging */
	int logTriggerType;	/* see enum LOG_TRIGGER_TYPES */
	int logTriggerVariable;	/* the variable(s) that can cause the log to
				   trigger. se enum LOG_TRIGGER_VARS */
	double logTriggerThreshold;	/* the value for non manual triggers */
	double dacOut;		/* output to DAC */
	double scale;		/* input or output scale arg */
	double offset;		/* input or output offset arg */
	double minFerror;	/* min following error */
	double maxFerror;	/* max following error */
	int wdWait;		/* cycle to wait before toggling wd */
	EMCMOT_AXIS_FLAG axisFlag;	/* flag to set polarities */
	int level;		/* flag for polarity level */
	int index;		/* Digital IO pin index */
	int probeIndex;		/* which wire the probe signal is on */
	int debug;		/* debug level, from DEBUG in .ini file */
	unsigned char out, start, end;	/* motion index, start, and end bits */
	unsigned char tail;	/* flag count for mutex detect */
	double setup_time;	/* number of periods before step occurs that
				   dir changes */
	double hold_time;	/* number of periods that step line is held
				   low/high after transition */
    } emcmot_command_t;

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
#define EMCMOT_MOTION_TELEOP_BIT       0x0010

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

/* axis flag polarities-- for those axis flag bits that are subject
   to polarity (e.g., EN is, IP, H are not)-- are referenced to these
   bit masks above.

   Note that 1 is normal, 0 is inverted, for sensing. They are reported
   in the axis status flag as documented, regardless of polarity. That
   is, AF will be 1 in the axis status flag for an amp fault, if the
   amp is faulted, regardless of the polarity.

   They default to 1, normal polarity.

   To set them, send EMCMOT_SET_POLARITY, with axis set to
   0..EMCMOT_MAX_AXIS - 1, axisFlag set to polarity bits for axis.

Suggestion: Use TRUE & FALSE definitions... Polarity will be handled in the
            HAL layer.
   
*/

/* values for commandStatus */
#define EMCMOT_COMMAND_OK 0	/* cmd honored */
#define EMCMOT_COMMAND_UNKNOWN_COMMAND 1	/* cmd not understood */
#define EMCMOT_COMMAND_INVALID_COMMAND 2	/* cmd can't be handled now */
#define EMCMOT_COMMAND_INVALID_PARAMS 3	/* bad cmd params */
#define EMCMOT_COMMAND_BAD_EXEC 4	/* error trying to initiate */
/* deprecated symbols */
#define KINEMATICS_SERIAL KINEMATICS_FORWARD_ONLY
#define KINEMATICS_PARALLEL KINEMATICS_INVERSE_ONLY
#define KINEMATICS_CUSTOM KINEMATICS_BOTH

/* status struct */
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */

	/* dynamic status-- changes every cycle */
	unsigned int heartbeat;
	int config_num;		/* incremented whenever configuration
				   changed. */
	double computeTime;
	EmcPose pos;		/* calculated Cartesian position */
	double axisPos[EMCMOT_MAX_AXIS];	/* calculated axis positions */
	double output[EMCMOT_MAX_AXIS];	/* Calculated output velocity command 
					 */
	double input[EMCMOT_MAX_AXIS];	/* actual input */
	EmcPose actualPos;	/* actual Cartesian position */
	int id;			/* id for executing motion */
	int depth;		/* motion queue depth */
	int activeDepth;	/* depth of active blend elements */
	int queueFull;		/* Flag to indicate the tc queue is full */
	EMCMOT_MOTION_FLAG motionFlag;	/* see above for bit details */
	EMCMOT_AXIS_FLAG axisFlag[EMCMOT_MAX_AXIS];	/* see above for bit
							   details */
	int paused;		/* Flag to signal motion paused */
	int overrideLimits;	/* non-zero means limits are ignored */
	int logPoints;		/* how many points currently in log */

	/* static status-- only changes upon input commands, e.g., config */
	int commandEcho;	/* echo of input command */
	int commandNumEcho;	/* echo of input command number */
	unsigned char commandStatus;	/* one of EMCMOT_COMMAND_ defined
					   above */
	double outputScale[EMCMOT_MAX_AXIS];	/* Used to set
						   emcmotDebug->inverseOutputScale 
						   - then used to scale the
						   DAC outputs */
	double outputOffset[EMCMOT_MAX_AXIS];	/* DC offset applied to the
						   DAC outputs to achieve 0V
						   when commanded output is
						   also zero */
	double inputScale[EMCMOT_MAX_AXIS];	/* Scaling applied to the
						   encoder inputs to return
						   position in real world
						   units */
	double inputOffset[EMCMOT_MAX_AXIS];	/* encoder offsets */
	double qVscale;		/* traj velocity scale factor */
	double axVscale[EMCMOT_MAX_AXIS];	/* axis velocity scale factor 
						 */
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
	int probeval;		/* current value of probe wire */
	int probing;		/* Currently looking for a probe signal? */
	EmcPose probedPos;	/* Axis positions stored as soon as possible
				   after last probeTripped */
	int level;
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_status_t;

/* config struct */
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */
	int config_num;		/* Incremented everytime configuration
				   changed, should match status.config_num */
	EMCMOT_AXIS_FLAG axisPolarity[EMCMOT_MAX_AXIS];
	int numAxes;		/* The number of axes in the system (which
				   must be between 1 and EMCMOT_MAX_AXIS,
				   inclusive). Allegedly, holds a copy of the 
				   global NUM_AXES - seems daft to maintain
				   duplicates ! */
	double trajCycleTime;	/* the rate at which the trajectory loop
				   runs.... (maybe) */
	double servoCycleTime;	/* the rate of the servo loop - Not the same
				   as the traj time */
	int interpolationRate;	/* grep control.c for an explanation....
				   approx line 50 */
	double maxLimit[EMCMOT_MAX_AXIS];	/* maximum axis limits,
						   counts */
	double minLimit[EMCMOT_MAX_AXIS];	/* minimum axis limits,
						   counts */
	double minOutput[EMCMOT_MAX_AXIS];	/* minimum output value
						   allowed, volts */
	double maxOutput[EMCMOT_MAX_AXIS];	/* maximum output value
						   allowed, volts */
	double minFerror[EMCMOT_MAX_AXIS];	/* minimum allowable
						   following error */
	double maxFerror[EMCMOT_MAX_AXIS];	/* maximum allowable
						   following error */
	double limitVel;	/* scalar upper limit on vel */
	double axisLimitVel[EMCMOT_MAX_AXIS];	/* scalar upper limit on axis 
						   vels */
	double homingVel[EMCMOT_MAX_AXIS];	/* scalar max homing vels */
	double homeOffset[EMCMOT_MAX_AXIS];	/* where to go after home,
						   user units */
	int probeIndex;		/* Which wire has the probe signal? */
	int probePolarity;	/* Look for 0 or 1. */
	KINEMATICS_TYPE kinematics_type;
	PID_STRUCT pid[EMCMOT_MAX_AXIS];
	int STEPPING_TYPE;	/* 0 = step/direction, 1 = phasing */
	double setup_time[EMCMOT_MAX_AXIS];	/* number of periods before
						   step occurs that dir
						   changes */
	double hold_time[EMCMOT_MAX_AXIS];	/* number of periods that
						   step line is held low/high 
						   after transition */
	int PERIOD;		/* fundamental period for timer interrupts */
	unsigned long int IO_BASE_ADDRESS;
	int debug;		/* copy of DEBUG, from .ini file */

	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_config_t;

/* debug struct */
/* FIXME - this has become a dumping ground for all kinds of stuff */
    typedef struct {
	unsigned char head;	/* flag count for mutex detect */
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
	double ferrorCurrent[EMCMOT_MAX_AXIS];	/* current following error */
	double ferrorHighMark[EMCMOT_MAX_AXIS];	/* magnitude of max following 
						   error */
	int split;		/* number of split command reads */
	/* 
	   stepperCount[] contains the accumulated pulses that have been
	   output, which are used as the "encoder feedback" for open-loop
	   stepping. */
	int stepperCount[EMCMOT_MAX_AXIS];	/* and rest are 0 */

	/* 
	 * 'pdmult' is a global that sets value to be loaded into decrement
	 * counters, for freqfunc() task function. Set this to 0 or 1 for full-out
	 * frequency, 2 for half, 3 for a third, etc.
	 * lpg -
	 * for rtl2x
	 *             1,193,180    / ( +-10          1.0         48000ns)
	 * pdmult = HRTICKS_PER_SEC / (rawoutput * output_scale * PERIOD);
	 * 
	 */
	int pdmult[EMCMOT_MAX_AXIS];
	int enable[EMCMOT_MAX_AXIS];

	/* flag for enabling, disabling watchdog; multiple for down-stepping */
	int wdEnabling;
	int wdEnabled;
	int wdWait;
	int wdCount;
	unsigned char wdToggle;

	/* flag that all active axes are homed */
	unsigned char allHomed;

	/* values for joint home positions */
	double jointHome[EMCMOT_MAX_AXIS];

	int maxLimitSwitchCount[EMCMOT_MAX_AXIS];	/* Counters used for
							   software debounce
							   of the inputs */
	int minLimitSwitchCount[EMCMOT_MAX_AXIS];
	int ampFaultCount[EMCMOT_MAX_AXIS];

	TP_STRUCT queue;	/* coordinated mode planner */
/* the freeAxis TP_STRUCTs are used to store the single joint value,
   in tran.x, that enables us to use the TP_STRUCT functions for axis
   planning. This is overkill, and we need to create a simpler TP_STRUCT
   for single-joint motion planning. */
	TP_STRUCT freeAxis[EMCMOT_MAX_AXIS];
/* freePose is a EmcPose struct that is used to hold a joint value, in
   tran.x, so that the TP_STRUCT functions can be called for scalar joint
   planning */
	EmcPose freePose;
	CUBIC_STRUCT cubic[EMCMOT_MAX_AXIS];

/* space for trajectory planner queues, plus 10 more for safety */
/* FIXME-- default is used; dynamic is not honored */
	TC_STRUCT queueTcSpace[DEFAULT_TC_QUEUE_SIZE + 10];
#define FREE_AXIS_QUEUE_SIZE 4	/* don't really queue free axis motion */
	TC_STRUCT freeAxisTcSpace[EMCMOT_MAX_AXIS][FREE_AXIS_QUEUE_SIZE];

	double rawInput[EMCMOT_MAX_AXIS];	/* raw feedback from sensors */
	double rawOutput[EMCMOT_MAX_AXIS];	/* raw output to actuators */

	double coarseJointPos[EMCMOT_MAX_AXIS];	/* trajectory point, in
						   joints */
	double jointPos[EMCMOT_MAX_AXIS];	/* interpolated point, in
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
	EmcPose oldPos;		/* last position, used for vel differencing */
	EmcPose oldVel, newVel;	/* velocities, used for acc differencing */
	EmcPose newAcc;		/* differenced acc */

	/* value of speed past which we debounce the feedback */
	double bigVel[EMCMOT_MAX_AXIS];	/* set to 10 * max Axis Velocity ...
					   IF we are at the point where the
					   encoder needs debouncing, the max
					   velocity of the axis has been
					   exceeded by a major margin ! */
	int homingPhase[EMCMOT_MAX_AXIS];	/* flags for homing */
	int latchFlag[EMCMOT_MAX_AXIS];	/* flags for axis latched */
	double saveLatch[EMCMOT_MAX_AXIS];	/* saved axis latch values */

	int enabling;		/* starts up disabled */
	int coordinating;	/* starts up in free mode */
	int teleoperating;	/* starts up in free mode */

	int wasOnLimit;		/* non-zero means we already aborted
				   everything due to a soft limit, and need
				   not re-abort. It's cleared only when all
				   limits are cleared. */
	int onLimit;		/* non-zero means at least one axis is on a
				   soft limit */

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

/* big comm structure, for upper memory */
    typedef struct {
	emcmot_command_t command;	/* struct used to pass commands/data to the
				   RT module from usr space */
	emcmot_status_t status;	/* Struct used to store RT status */
	emcmot_config_t config;	/* Struct used to store RT config */
	emcmot_debug_t debug;	/* Struct used to store RT status and debug
				   data - 2nd largest block */
	emcmot_error_t error;	/* ring buffer for error messages */
	emcmot_log_t log;		/* a massive ring buffer for logging RT data */
	emcmot_comp_t comp[EMCMOT_MAX_AXIS];	/* corrections to be applied
						   to input pos */
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
