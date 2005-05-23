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
* Last change:
* $Revision$
* $Author$
* $Date$
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
    double vScale, vRestore;
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
#if 0
/* FIXME - needed for synchronous I/O */
    unsigned char douts;        /* mask for douts to set */
    int doutIndex;              /* index for dout values */
    unsigned char doutstart;    /* mask for dout start vals */
    unsigned char doutend;      /* mask for dout end vals */
#endif
} TP_STRUCT;

extern int tpCreate(TP_STRUCT * tp, int _queueSize, TC_STRUCT * tcSpace);
extern int tpDelete(TP_STRUCT * tp);
extern int tpClear(TP_STRUCT * tp);
extern int tpInit(TP_STRUCT * tp);
extern int tpSetCycleTime(TP_STRUCT * tp, double secs);
extern int tpSetVmax(TP_STRUCT * tp, double vmax);
extern int tpSetWmax(TP_STRUCT * tp, double vmax);
extern int tpSetVlimit(TP_STRUCT * tp, double limit);
extern int tpSetVscale(TP_STRUCT * tp, double scale);	/* 0.0 .. large */
extern int tpSetAmax(TP_STRUCT * tp, double amax);
extern int tpSetWDotmax(TP_STRUCT * tp, double amax);
extern int tpSetId(TP_STRUCT * tp, int id);
extern int tpGetNextId(TP_STRUCT * tp);
extern int tpGetExecId(TP_STRUCT * tp);
extern int tpSetTermCond(TP_STRUCT * tp, int cond);
extern int tpGetTermCond(TP_STRUCT * tp);
extern int tpSetPos(TP_STRUCT * tp, EmcPose pos);
extern int tpAddLine(TP_STRUCT * tp, EmcPose end);
extern int tpAddCircle(TP_STRUCT * tp, EmcPose end,
    PmCartesian center, PmCartesian normal, int turn);
extern int tpRunCycle(TP_STRUCT * tp);
extern int tpPause(TP_STRUCT * tp);
extern int tpResume(TP_STRUCT * tp);
extern int tpAbort(TP_STRUCT * tp);
extern EmcPose tpGetPos(TP_STRUCT * tp);
extern int tpIsDone(TP_STRUCT * tp);
extern int tpIsPaused(TP_STRUCT * tp);
extern int tpQueueDepth(TP_STRUCT * tp);
extern int tpActiveDepth(TP_STRUCT * tp);
extern void tpPrint(TP_STRUCT * tp);
#if 0
/* FIXME - needed for synchronous I/O */
extern int tpSetAout(TP_STRUCT * tp, unsigned char index, double start, double end);
extern int tpSetDout(TP_STRUCT * tp, int index, unsigned char start, unsigned char end);
#endif

#endif /* TP_H */
