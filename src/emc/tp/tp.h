/********************************************************************
* Description: tp.h
*   Trajectory planner based on TC elements
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
#ifndef TP_H
#define TP_H

#include "posemath.h"
#include "tc.h"

#define TP_DEFAULT_QUEUE_SIZE 32
//Minimum length of a segment in cycles (must be greater than 1 to ensure each segment is hit at least once.)
#define TP_MIN_SEGMENT_CYCLES 1.02
//Values chosen for accel ratio to match parabolic blend acceleration
//limits.
#define TP_ACC_RATIO_TANGENTIAL 0.5
#define TP_ACC_RATIO_NORMAL (pmSqrt(1.0 - pmSq(TP_ACC_RATIO_TANGENTIAL)))
#define TP_OPTIMIZATION_CUTOFF 4

/* closeness to zero, for determining if a move is pure rotation */
#define TP_PURE_ROTATION_EPSILON 1e-6

/* "neighborhood" size (if two values differ by less than the epsilon,
 * then they are effectively equal.)*/
#define TP_ACCEL_EPSILON 1e-4
#define TP_VEL_EPSILON   1e-8
#define TP_POS_EPSILON   1e-12
#define TP_TIME_EPSILON  1e-12
#define TP_ANGLE_EPSILON 1e-6
#define TP_MIN_ARC_ANGLE 1e-3
#define TP_BIG_NUM 1e10


/**
 * TP return codes.
 * This enum is a catch-all for useful return statuses from TP
 * internal functions. This may be replaced with a better system in
 * the future.
 */
typedef enum {
    TP_ERR_MISSING_OUTPUT = -3,
    TP_ERR_MISSING_INPUT = -2,
    TP_ERR_FAIL = -1,
    TP_ERR_OK = 0,
    TP_ERR_NO_ACTION,
    TP_ERR_SLOWING,
    TP_ERR_STOPPED,
    TP_ERR_WAITING,
    TP_ERR_LAST
} tp_err_code_t;

/**
 * Persistant data for spindle status within tpRunCycle.
 * This structure encapsulates some static variables to simplify refactoring of
 * synchronized motion code.
 */
typedef struct {
     double offset;
     double revs;
     int waiting_for_index;
     int waiting_for_atspeed;
} tp_spindle_status_t;


/**
 * Trajectory planner state structure.
 * Stores persistant data for the trajectory planner that should be accessible
 * by outside functions.
 */
typedef struct {
    TC_QUEUE_STRUCT queue;
    tp_spindle_status_t spindle; //Spindle data

    EmcPose currentPos;
    EmcPose goalPos;

    int queueSize;
    double cycleTime;

    double vMax;		/* vel for subsequent moves */
    double ini_maxvel;          /* max velocity allowed by machine
                                   constraints (ini file) for
                                   subsequent moves */
    double vLimit;		/* absolute upper limit on all vels */

    double aMax;        /* max accel (unused) */
    //FIXME this shouldn't be a separate limit,
    double aMaxCartesian; /* max cartesian acceleration by machine bounds */
    double aLimit;        /* max accel (unused) */

    double wMax;		/* rotational velocity max */
    double wDotMax;		/* rotational accelleration max */
    int nextId;
    int execId;
    int termCond;
    int done;
    int depth;			/* number of total queued motions */
    int activeDepth;		/* number of motions blending */
    int aborting;
    int pausing;
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

int tpCreate(TP_STRUCT * tp, int _queueSize, TC_STRUCT * tcSpace);
int tpClear(TP_STRUCT * tp);
int tpInit(TP_STRUCT * tp);
int tpClearDIOs(TP_STRUCT * const tp);
int tpSetCycleTime(TP_STRUCT * tp, double secs);
int tpSetVmax(TP_STRUCT * tp, double vmax, double ini_maxvel);
int tpSetVlimit(TP_STRUCT * tp, double limit);
int tpSetAmax(TP_STRUCT * tp, double amax);
int tpSetId(TP_STRUCT * tp, int id);
int tpGetExecId(TP_STRUCT * tp);
int tpSetTermCond(TP_STRUCT * tp, int cond, double tolerance);
int tpSetPos(TP_STRUCT * tp, EmcPose pos);
int tpAddRigidTap(TP_STRUCT * tp, EmcPose end, double vel, double
        ini_maxvel, double acc, unsigned char enables);
int tpAddLine(TP_STRUCT * tp, EmcPose end, int type, double vel, double
                     ini_maxvel, double acc, unsigned char enables, char atspeed, int indexrotary);
int tpAddCircle(TP_STRUCT * tp, EmcPose end, PmCartesian center,
        PmCartesian normal, int turn, int type, double vel, double ini_maxvel,
                       double acc, unsigned char enables, char atspeed);
int tpRunCycle(TP_STRUCT * tp, long period);
int tpPause(TP_STRUCT * tp);
int tpResume(TP_STRUCT * tp);
int tpAbort(TP_STRUCT * tp);
int tpGetPos(TP_STRUCT const  * const tp, EmcPose * const pos);
int tpIsDone(TP_STRUCT * tp);
int tpQueueDepth(TP_STRUCT * tp);
int tpActiveDepth(TP_STRUCT * tp);
int tpGetMotionType(TP_STRUCT * tp);
int tpSetSpindleSync(TP_STRUCT * tp, double sync, int wait);
void tpToggleDIOs(TC_STRUCT * tc); //gets called when a new tc is taken from the queue. it checks and toggles all needed DIO's

int tpSetAout(TP_STRUCT * tp, unsigned char index, double start, double end);
int tpSetDout(TP_STRUCT * tp, int index, unsigned char start, unsigned char end); //gets called to place DIO toggles on the TC queue

#endif				/* TP_H */
