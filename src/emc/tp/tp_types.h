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
#define TP_VEL_EPSILON   DOUBLE_FUZZ
#define TP_POS_EPSILON   1e-12
#define TP_TIME_EPSILON  1e-12
#define TP_ANGLE_EPSILON 1e-6
#define TP_ANGLE_EPSILON_SQ (TP_ANGLE_EPSILON * TP_ANGLE_EPSILON)
#define TP_MIN_ARC_ANGLE 1e-3
#define TP_MIN_ARC_LENGTH 1e-6
#define TP_BIG_NUM 1e10

/**
 * Persistant data for spindle status within tpRunCycle.
 * This structure encapsulates some static variables to simplify refactoring of
 * synchronized motion code.
 */
typedef struct {
    spindle_origin_t origin; //!< initial position of spindle during synchronization (direction-aware)

    double trigger_revs;
    int waiting_for_index;
    int waiting_for_atspeed;
} tp_spindle_t;

/**
 * Trajectory planner state structure.
 * Stores persistant data for the trajectory planner that should be accessible
 * by outside functions.
 */
typedef struct {
    TC_QUEUE_STRUCT queue;
    tp_spindle_t spindle; //Spindle data

    int tc_completed_id; /* ID of most recent completed segment, i.e. "-1" in the queue"*/

    PmVector currentPos;
    PmVector goalPos;
    PmVector currentVel;

    int queueSize;
    double cycleTime;

    double vLimit;		/* absolute upper limit on all linear vels */
    double vLimitAng;		/* absolute upper limit on all angular vels */

    int nextId;
    int execId;
    tc_unique_id_t nextUniqueId;
    struct state_tag_t execTag; /* state tag corresponding to running motion */
    int nextexecId;
    tc_term_cond_t termCond;
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
    tc_spindle_sync_t synchronized;       // spindle sync required for this move
    double uu_per_rev;          /* user units per spindle revolution */


    syncdio_t syncdio; //record tpSetDout's here

    double time_elapsed_sec; // Total elapsed TP run time in seconds
    long long time_elapsed_ticks; // Total elapsed TP run time in cycles (ticks)
    long long time_at_wait; // Time when TP started to wait for spindle

} TP_STRUCT;

#endif				/* TP_TYPES_H */
