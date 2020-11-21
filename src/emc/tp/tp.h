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

int tpCreate(TP_STRUCT * const tp, int _queueSize, TC_STRUCT * const tcSpace);
int tpClear(TP_STRUCT * const tp);
int tpInit(TP_STRUCT * const tp);
int tpClearDIOs(TP_STRUCT * const tp);
int tpSetCycleTime(TP_STRUCT * tp, double secs);
int tpSetVmax(TP_STRUCT * tp, double vmax, double ini_maxvel);
int tpSetVlimit(TP_STRUCT * tp, double limit);
int tpSetAmax(TP_STRUCT * tp, double amax);
int tpSetId(TP_STRUCT * tp, int id);
int tpGetExecId(TP_STRUCT * tp);
struct state_tag_t tpGetExecTag(TP_STRUCT * const tp);
int tpSetTermCond(TP_STRUCT * tp, int cond, double tolerance);
int tpSetPos(TP_STRUCT * tp, EmcPose const * const pos);
int tpAddCurrentPos(TP_STRUCT * const tp, EmcPose const * const disp);
int tpSetCurrentPos(TP_STRUCT * const tp, EmcPose const * const pos);
int tpRunCycle(TP_STRUCT * tp, long period);
int tpPause(TP_STRUCT * tp);
int tpResume(TP_STRUCT * tp);
int tpAbort(TP_STRUCT * tp);
int tpAddRigidTap(TP_STRUCT * const tp,
        EmcPose end,
        double vel,
        double ini_maxvel,
        double acc,
        unsigned char enables,
        double scale,
        struct state_tag_t tag);
int tpAddLine(TP_STRUCT * const tp, EmcPose end, int canon_motion_type,
	      double vel, double ini_maxvel, double acc, unsigned char enables,
	      char atspeed, int indexrotary, struct state_tag_t tag);
int tpAddCircle(TP_STRUCT * const tp, EmcPose end, PmCartesian center,
		PmCartesian normal, int turn, int canon_motion_type, double vel,
		double ini_maxvel, double acc, unsigned char enables,
		char atspeed, struct state_tag_t tag);
int tpGetPos(TP_STRUCT const  * const tp, EmcPose * const pos);
int tpIsDone(TP_STRUCT * const tp);
int tpQueueDepth(TP_STRUCT * const tp);
int tpActiveDepth(TP_STRUCT * const tp);
int tpGetMotionType(TP_STRUCT * const tp);
int tpSetSpindleSync(TP_STRUCT * const tp, int spindle, double sync, int wait);
void tpToggleDIOs(TC_STRUCT * const tc); //gets called when a new tc is taken from the queue. it checks and toggles all needed DIO's

int tpSetAout(TP_STRUCT * const tp, unsigned char index, double start, double end);
int tpSetDout(TP_STRUCT * const tp, int index, unsigned char start, unsigned char end); //gets called to place DIO toggles on the TC queue

int tpSetRunDir(TP_STRUCT * const tp, tc_direction_t dir);
int tpIsMoving(TP_STRUCT const * const tp);

#endif				/* TP_H */
