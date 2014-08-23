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

/* closeness to zero, for determining if a move is pure rotation */
#define TP_PURE_ROTATION_EPSILON 1e-6

/* closeness to zero, for determining if vel and accel are effectively zero */
#define TP_VEL_EPSILON 1e-6
#define TP_ACCEL_EPSILON 1e-6

typedef struct {
    TC_QUEUE_STRUCT queue;
    int queueSize;
    double cycleTime;
    double vMax;		/* vel for subsequent moves */
    double ini_maxvel;          /* max velocity allowed by machine 
                                   constraints (ini file) for
                                   subsequent moves */
    double vScale;		/* feed override value */
    double aMax;
    double vLimit;		/* absolute upper limit on all vels */
    double wMax;		/* rotational velocity max */
    double wDotMax;		/* rotational accelleration max */
    int nextId;
    int execId;
    int termCond;
    EmcPose currentPos;
    EmcPose goalPos;
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
} TP_STRUCT;

extern int tpCreate(TP_STRUCT * tp, int _queueSize, TC_STRUCT * tcSpace);
extern int tpClear(TP_STRUCT * tp);
extern int tpInit(TP_STRUCT * tp);
extern int tpClearDIOs(void);
extern int tpSetCycleTime(TP_STRUCT * tp, double secs);
extern int tpSetVmax(TP_STRUCT * tp, double vmax, double ini_maxvel);
extern int tpSetVlimit(TP_STRUCT * tp, double limit);
extern int tpSetAmax(TP_STRUCT * tp, double amax);
extern int tpSetId(TP_STRUCT * tp, int id);
extern int tpGetExecId(TP_STRUCT * tp);
extern int tpSetTermCond(TP_STRUCT * tp, int cond, double tolerance);
extern int tpSetPos(TP_STRUCT * tp, EmcPose pos);
extern int tpAddRigidTap(TP_STRUCT * tp, EmcPose end, double vel, double
        ini_maxvel, double acc, unsigned char enables);
extern int tpAddLine(TP_STRUCT * tp, EmcPose end, int type, double vel, double
                     ini_maxvel, double acc, unsigned char enables, char atspeed, int indexrotary);
extern int tpAddCircle(TP_STRUCT * tp, EmcPose end, PmCartesian center,
        PmCartesian normal, int turn, int type, double vel, double ini_maxvel,
                       double acc, unsigned char enables, char atspeed);
extern int tpRunCycle(TP_STRUCT * tp, long period);
extern int tpPause(TP_STRUCT * tp);
extern int tpResume(TP_STRUCT * tp);
extern int tpAbort(TP_STRUCT * tp);
extern EmcPose tpGetPos(TP_STRUCT * tp);
extern int tpIsDone(TP_STRUCT * tp);
extern int tpQueueDepth(TP_STRUCT * tp);
extern int tpActiveDepth(TP_STRUCT * tp);
extern int tpGetMotionType(TP_STRUCT * tp);
extern int tpSetSpindleSync(TP_STRUCT * tp, double sync, int wait);
extern void tpToggleDIOs(TC_STRUCT * tc); //gets called when a new tc is taken from the queue. it checks and toggles all needed DIO's

extern int tpSetAout(TP_STRUCT * tp, unsigned char index, double start, double end);
extern int tpSetDout(TP_STRUCT * tp, int index, unsigned char start, unsigned char end); //gets called to place DIO toggles on the TC queue

#endif				/* TP_H */
