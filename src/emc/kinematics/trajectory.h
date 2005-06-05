/********************************************************************
* Description: trajectory.h
*
* Author: Paul Corner
* Created on: Sat Jun  4 16:19:25 BST 2005
* Computer: Babylon.node 
* System: Linux 2.6.10-adeos on i686
*    
* Last change:
* $Revision$
* $Author$
* $Date$
*
* Copyright (c) 2005 Paul Corner  All rights reserved.
*
********************************************************************/
#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include "math.h"
#include "vectorlib.h"

#define ABORT		0xF0000000
#define PAUSED		0x00000001
#define EXACT_STOP	0x00000010
#define MIN_BLEND	0x00000020
#define BSPLINE_POINT	0x00000040
#define KNOT_POINT	0x00000080
#define STRAIGHT_LINE	0x00000100
#define CLOCKWISE_ARC	0x00000200
#define ANTICLOCK_ARC	0x00000400
#define PLANE_G17	0x00001000
#define PLANE_G18	0x00002000
#define PLANE_G19	0x00004000

typedef struct {
    vector3 pos;
    vector3 rot;
    int flags;
    int id;
} point;

typedef struct {
    vector3 vel;
    vector3 accel;
    vector3 jerk;
} bounds;

/* FIX-ME wrappers around new functions - Not all are implemented... */
extern int tpFull(TP_STRUCT *tp);
extern int tpAbort(TP_STRUCT *tp);
extern int tpAddCircle(TP_STRUCT *tp, EmcPose end, PmCartesian center, PmCartesian normal, int turn);
extern int tpAddLine(TP_STRUCT *tp, EmcPose end);
extern int tpClear(TP_STRUCT *tp);
extern int tpCreate(TP_STRUCT *tp, int _queueSize, TC_STRUCT *tcSpace);
extern int tpGetExecId(TP_STRUCT *tp);
extern EmcPose tpGetPos(TP_STRUCT *tp);
extern int tpInit(TP_STRUCT *tp);
extern int tpIsDone(TP_STRUCT *tp);
extern int tpPause(TP_STRUCT *tp);
extern int tpResume(TP_STRUCT *tp);
extern int tpRunCycle(TP_STRUCT *tp);
extern int tpSetAmax(TP_STRUCT *tp, double amax);
extern int tpSetAout(TP_STRUCT *tp, unsigned char index, double start, double end);
extern int tpSetCycleTime(TP_STRUCT *tp, double secs);
extern int tpSetDout(TP_STRUCT *tp, int index, unsigned char start, unsigned char end);
extern int tpSetId(TP_STRUCT *tp, int id);
extern int tpSetPos(TP_STRUCT *tp, EmcPose pos);
extern int tpSetTermCond(TP_STRUCT *tp, int cond);
extern int tpSetVlimit(TP_STRUCT *tp, double limit);
extern int tpSetVmax(TP_STRUCT *tp, double vmax);
extern int tpSetVscale(TP_STRUCT *tp, double scale);

#endif
