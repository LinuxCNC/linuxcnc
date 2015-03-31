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

typedef struct tp_shared_t tp_shared_t;

// tp API method signatures
typedef int (*tpCreate_t)(TP_STRUCT * tp, int _queueSize, TC_STRUCT * tcSpace,
			  tp_shared_t *shared);
typedef int (*tpClear_t)(TP_STRUCT * tp);
typedef int (*tpInit_t)(TP_STRUCT * tp);
typedef int (*tpClearDIOs_t)(TP_STRUCT * const tp);
typedef int (*tpSetCycleTime_t)(TP_STRUCT * tp, double secs);
typedef int (*tpSetVmax_t)(TP_STRUCT * tp, double vmax, double ini_maxvel);
typedef int (*tpSetVlimit_t)(TP_STRUCT * tp, double limit);
typedef int (*tpSetAmax_t)(TP_STRUCT * tp, double amax);
typedef int (*tpSetId_t)(TP_STRUCT * tp, int id);
typedef int (*tpGetExecId_t)(TP_STRUCT * tp);
typedef struct state_tag_t (*tpGetExecTag_t)(TP_STRUCT * const tp);
typedef int (*tpSetTermCond_t)(TP_STRUCT * tp, int cond, double tolerance);
typedef int (*tpSetPos_t)(TP_STRUCT * tp, EmcPose const * const pos);
typedef int (*tpAddCurrentPos_t)(TP_STRUCT * const tp, EmcPose const * const disp);
typedef int (*tpSetCurrentPos_t)(TP_STRUCT * const tp, EmcPose const * const pos);
typedef int (*tpAddRigidTap_t)(TP_STRUCT * tp,
			       EmcPose end,
			       double vel,
			       double ini_maxvel,
			       double acc,
			       unsigned char enables,
			       struct state_tag_t tag);
typedef int (*tpAddLine_t)(TP_STRUCT * tp,
			   EmcPose end,
			   int type,
			   double vel,
			   double ini_maxvel,
			   double acc,
			   unsigned char enables,
			   char atspeed,
			   int indexrotary,
			    struct state_tag_t tag);
typedef int (*tpAddCircle_t)(TP_STRUCT * tp,
			     EmcPose end,
			     PmCartesian center,
			     PmCartesian normal,
			     int turn,
			     int type,
			     double vel,
			     double ini_maxvel,
			     double acc,
			     unsigned char enables,
			     char atspeed,
			    struct state_tag_t tag);
typedef int (*tpRunCycle_t)(TP_STRUCT * tp, long period);
typedef int (*tpPause_t)(TP_STRUCT * tp);
typedef int (*tpResume_t)(TP_STRUCT * tp);
typedef int (*tpAbort_t)(TP_STRUCT * tp);
typedef int (*tpGetPos_t)(TP_STRUCT const  * const tp, EmcPose * const pos);
typedef int (*tpIsDone_t)(TP_STRUCT * tp);
typedef int (*tpQueueDepth_t)(TP_STRUCT * tp);
typedef int (*tpActiveDepth_t)(TP_STRUCT * tp);
typedef int (*tpGetMotionType_t)(TP_STRUCT * tp);
typedef int (*tpSetSpindleSync_t)(TP_STRUCT * tp, double sync, int wait);
typedef int (*tcqFull_t)(TC_QUEUE_STRUCT const * const tcq);

//gets called when a new tc is taken from the queue. it checks and toggles all needed DIO's
typedef void (*tpToggleDIOs_t)(TP_STRUCT const * const tp,TC_STRUCT * tc);

typedef int (*tpSetAout_t)(TP_STRUCT * tp, unsigned int index, double start, double end);

 //gets called to place DIO toggles on the TC queue
typedef int (*tpSetDout_t)(TP_STRUCT * tp, unsigned int index, unsigned char start, unsigned char end);

// for jog-while-paused:
typedef int (*tpIsPaused_t)(TP_STRUCT * tp);
typedef int (*tpSnapshot_t)(TP_STRUCT * from, TP_STRUCT * to);


// the tp API vtable
typedef struct {
    tpCreate_t          tpCreate;
    tpClear_t           tpClear;
    tpInit_t            tpInit;
    tpClearDIOs_t	tpClearDIOs;
    tpSetCycleTime_t    tpSetCycleTime;
    tpSetVmax_t	        tpSetVmax;
    tpSetVlimit_t	tpSetVlimit;
    tpSetAmax_t		tpSetAmax;
    tpSetId_t	        tpSetId;
    tpGetExecId_t	tpGetExecId;
    tpGetExecTag_t      tpGetExecTag;
    tpSetTermCond_t	tpSetTermCond;
    tpSetPos_t          tpSetPos;
    tpAddCurrentPos_t   tpAddCurrentPos;
    tpSetCurrentPos_t   tpSetCurrentPos;
    tpAddRigidTap_t	tpAddRigidTap;
    tpAddLine_t	        tpAddLine;
    tpAddCircle_t	tpAddCircle;
    tpRunCycle_t	tpRunCycle;
    tpPause_t	        tpPause;
    tpResume_t	        tpResume;
    tpAbort_t	        tpAbort;
    tpGetPos_t          tpGetPos;
    tpIsDone_t          tpIsDone;
    tpQueueDepth_t	tpQueueDepth;
    tpActiveDepth_t	tpActiveDepth;
    tpGetMotionType_t   tpGetMotionType;
    tpSetSpindleSync_t  tpSetSpindleSync;
    tpToggleDIOs_t	tpToggleDIOs;
    tpSetAout_t	        tpSetAout;
    tpSetDout_t	        tpSetDout;
    tpIsPaused_t	tpIsPaused;
    tpSnapshot_t	tpSnapshot;
    tcqFull_t           tcqFull;
} vtp_t;


#endif				/* TP_H */
