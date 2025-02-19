/********************************************************************
* Description: tp_types.h
*   Trajectory planner types and constants
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
#ifndef TP_TYPES_H
#define TP_TYPES_H

#include "posemath.h"
#include "tc_types.h"
#include "tcq.h"

#include <rtapi_bool.h>

#define TP_DEFAULT_QUEUE_SIZE 32
/* Minimum length of a segment in cycles (must be greater than 1 to ensure each
 * segment is hit at least once.) */
#define TP_MIN_SEGMENT_CYCLES 1.02
/* Values chosen for accel ratio to match parabolic blend acceleration
 * limits. */
#define TP_OPTIMIZATION_CUTOFF 4
/* If the queue is shorter than the threshold, assume that we're approaching
 * the end of the program */
#define TP_QUEUE_THRESHOLD 3

/* closeness to zero, for determining if a move is pure rotation */
#define TP_PURE_ROTATION_EPSILON 1e-6

/* "neighborhood" size (if two values differ by less than the epsilon,
 * then they are effectively equal.)*/
#define TP_ACCEL_EPSILON 1e-4
#define TP_VEL_EPSILON   1e-8
#define TP_POS_EPSILON   1e-12
#define TP_TIME_EPSILON  1e-12
#define TP_ANGLE_EPSILON 1e-6
#define TP_ANGLE_EPSILON_SQ (TP_ANGLE_EPSILON * TP_ANGLE_EPSILON)
#define TP_MIN_ARC_ANGLE 1e-3
#define TP_MIN_ARC_LENGTH 1e-6
#define TP_BIG_NUM 1e10

/**
 * TP return codes.
 * This enum is a catch-all for useful return statuses from TP
 * internal functions. This may be replaced with a better system in
 * the future.
 */
typedef enum {
    TP_ERR_INVALID = -9,
    TP_ERR_INPUT_TYPE = -8,
    TP_ERR_TOLERANCE = -7,
    TP_ERR_RADIUS_TOO_SMALL = -6,
    TP_ERR_GEOM = -5,
    TP_ERR_RANGE = -4,
    TP_ERR_MISSING_OUTPUT = -3,
    TP_ERR_MISSING_INPUT = -2,
    TP_ERR_FAIL = -1,
    TP_ERR_OK = 0,
    TP_ERR_NO_ACTION,
    TP_ERR_SLOWING,
    TP_ERR_STOPPED,
    TP_ERR_WAITING,
    TP_ERR_ZERO_LENGTH,
    TP_ERR_REVERSE_EMPTY,
    TP_ERR_LAST
} tp_err_t;

/**
 * Persistent data for spindle status within tpRunCycle.
 * This structure encapsulates some static variables to simplify refactoring of
 * synchronized motion code.
 */
typedef struct {
	 int spindle_num;
     double offset;
     double revs;
     int waiting_for_index;
     int waiting_for_atspeed;
} tp_spindle_t;

/**
 * Trajectory planner state structure.
 * Stores persistent data for the trajectory planner that should be accessible
 * by outside functions.
 */
typedef struct {
    TC_QUEUE_STRUCT queue;
    tp_spindle_t spindle; //Spindle data

    EmcPose currentPos;
    EmcPose goalPos;

    int queueSize;
    double cycleTime;

    double vMax;		/* vel for subsequent moves */
    double ini_maxvel;          /* max velocity allowed by machine
                                   constraints (INI file) for
                                   subsequent moves */
    double vLimit;		/* absolute upper limit on all vels */

    double aMax;        /* max accel (unused) */
    //FIXME this shouldn't be a separate limit,
    double aMaxCartesian; /* max cartesian acceleration by machine bounds */
    double aLimit;        /* max accel (unused) */

    double wMax;		/* rotational velocity max */
    double wDotMax;		/* rotational acceleration max */
    int nextId;
    int execId;
    struct state_tag_t execTag; /* state tag corresponding to running motion */
    int termCond;
    int done;
    int depth;			/* number of total queued motions */
    int activeDepth;		/* number of motions blending */
    int aborting;
    int pausing;
    int reverse_run;      /* Indicates that TP is running in reverse */
    int motionType;
    double tolerance;           /* for subsequent motions, stay within this
                                   distance of the programmed path during
                                   blends */
    int synchronized;       // spindle sync required for this move
    int velocity_mode; 	        /* TRUE if spindle sync is in velocity mode,
				   FALSE if in position mode */
    double uu_per_rev;          /* user units per spindle revolution */


    syncdio_t syncdio; //record tpSetDout's here

} TP_STRUCT;


/**
 * Describes blend modes used in the trajectory planner.
 * @note these values are used as array indices, so make sure valid options
 * start at 0 and increase by one.
 */
typedef enum {
    NO_BLEND = -1,
    PARABOLIC_BLEND,
    TANGENT_SEGMENTS_BLEND,
    ARC_BLEND
} tc_blend_type_t;

#endif				/* TP_TYPES_H */
