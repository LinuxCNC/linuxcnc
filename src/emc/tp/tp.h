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
#include "tc_types.h"
#include "tp_types.h"
#include "tcq.h"

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
int tpSetPos(TP_STRUCT * tp, EmcPose const * const pos);
int tpAddCurrentPos(TP_STRUCT * const tp, EmcPose const * const disp);
int tpSetCurrentPos(TP_STRUCT * const tp, EmcPose const * const pos);
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

//Misc utlity functions for motion
int tpFindIntersectionAngle(PmCartesian const * const u1,
        PmCartesian const * const u2, double * const theta);

// for jog-while-paused:
extern int tpIsPaused(TP_STRUCT * tp);
extern int tpSnapshot(TP_STRUCT * from, TP_STRUCT * to);

#endif				/* TP_H */
