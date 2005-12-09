/********************************************************************
* Description: tc.h
*   Discriminate-based trajectory planning
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
#ifndef TC_H
#define TC_H

#include "posemath.h"
#include "emcpos.h"

/* values for tcFlag */
#define TC_IS_UNSET -1
#define TC_IS_DONE 1
#define TC_IS_ACCEL 2
#define TC_IS_CONST 3
#define TC_IS_DECEL 4
#define TC_IS_PAUSED 5

/* values for endFlag */
#define TC_TERM_COND_STOP 1
#define TC_TERM_COND_BLEND 2

#define TC_LINEAR 1
#define TC_CIRCULAR 2

/* structure for individual trajectory elements */

typedef struct {
    double cycleTime;
    double targetPos;		/* positive motion progession */
    double vMax;		/* max velocity */
    double ini_maxvel;		/* max velocity allowed by machine constraints
                                   (ini file) */
    double vScale;		/* scale factor for vMax */
    double aMax;		/* max accel */
    double preVMax;		/* vel from previous blend */
    double preAMax;		/* decel (negative) from previous blend */
    double vLimit;		/* abs vel limit, including scale */
    double toGo;
    double currentPos;
    double currentVel;
    double currentAccel;
    int tcFlag;			/* TC_IS_DONE,ACCEL,CONST,DECEL */
    int type;			/* TC_LINEAR, TC_CIRCULAR */
    int id;			/* id for motion segment */
    int termCond;		/* TC_END_STOP,BLEND */
    PmLine line;
    PmLine line_abc;
    PmCircle circle;
    double tmag;		/* magnitude of translation */
    double abc_mag;		/* magnitude of rotation */
    double tvMax;		/* maximum translational velocity */
    double taMax;		/* maximum translational accelleration */
    double abc_vMax;		/* maximum rotational velocity */
    double abc_aMax;		/* maximum rotational accelleration */
    PmCartesian unitCart;
    int output_chan;		/* output channel used for HAL stuff */
/*! \todo This is related to synchronous I/O, and will be fixed later */
#if 0
    unsigned char douts;	/* mask for douts to set */
    int doutIndex;		/* index for dout value */
    unsigned char doutstarts;	/* mask for dout start vals */
    unsigned char doutends;	/* mask for dout end vals */
#endif
} TC_STRUCT;

/*! \todo This is related to synchronous I/O, and will be fixed later */
#if 0
extern unsigned char tcDoutByte;
#endif

/* TC_STRUCT functions */

extern int tcInit(TC_STRUCT * tc);
extern int tcSetCycleTime(TC_STRUCT * tc, double secs);
extern int tcSetLine(TC_STRUCT * tc, PmLine line, PmLine line_abc);
extern int tcSetCircle(TC_STRUCT * tc, PmCircle circle, PmLine line_abc);
extern int tcSetTVmax(TC_STRUCT * tc, double vmax, double ini_maxvel);
extern int tcSetRVmax(TC_STRUCT * tc, double vmax);
extern int tcSetVscale(TC_STRUCT * tc, double vscale);
extern int tcSetTAmax(TC_STRUCT * tc, double amax);
extern int tcSetRAmax(TC_STRUCT * tc, double wmax);
extern int tcSetPremax(TC_STRUCT * tc, double vmax, double amax);
extern int tcSetVlimit(TC_STRUCT * tc, double vlimit);
extern int tcSetId(TC_STRUCT * tc, int id);
extern int tcGetId(TC_STRUCT * tc);
extern int tcSetTermCond(TC_STRUCT * tc, int cond);
extern int tcGetTermCond(TC_STRUCT * tc);
extern int tcRunCycle(TC_STRUCT * tc);
extern EmcPose tcGetPos(TC_STRUCT * tc);
extern EmcPose tcGetGoalPos(TC_STRUCT * tc);
extern double tcGetVel(TC_STRUCT * tc);
extern double tcGetAccel(TC_STRUCT * tc);
extern PmCartesian tcGetUnitCart(TC_STRUCT * tc);
extern int tcGetTcFlag(TC_STRUCT * tc);
extern int tcIsDone(TC_STRUCT * tc);
extern int tcIsAccel(TC_STRUCT * tc);
extern int tcIsConst(TC_STRUCT * tc);
extern int tcIsDecel(TC_STRUCT * tc);
extern int tcIsPaused(TC_STRUCT * tc);
extern void tcPrint(TC_STRUCT * tc);
extern double tcRunPreCycle(const TC_STRUCT * tc);
extern int tcForceCycle(TC_STRUCT * tc, double ratio);

/*! \todo This is related to synchronous I/O, and will be fixed later */
#if 0
extern int tcSetDout(TC_STRUCT * tc, int index, unsigned char starts,
		     unsigned char ends);
#endif

/* queue of TC_STRUCT elements*/

typedef struct {
    TC_STRUCT *queue;		/* ptr to the tcs */
    int size;			/* size of queue */
    int _len;			/* number of tcs now in queue */
    int start, end;		/* indices to next to get, next to put */
    int allFull;		/* flag meaning it's actually full */
} TC_QUEUE_STRUCT;

/* TC_QUEUE_STRUCT functions */

/* create queue of _size */
extern int tcqCreate(TC_QUEUE_STRUCT * tcq, int _size,
		     TC_STRUCT * tcSpace);

/* free up queue */
extern int tcqDelete(TC_QUEUE_STRUCT * tcq);

/* reset queue to empty */
extern int tcqInit(TC_QUEUE_STRUCT * tcq);

/* put tc on end */
extern int tcqPut(TC_QUEUE_STRUCT * tcq, TC_STRUCT tc);

/* get tcq from front */
extern TC_STRUCT tcqGet(TC_QUEUE_STRUCT * tcq, int *status);

/* remove n tcs from front */
extern int tcqRemove(TC_QUEUE_STRUCT * tcq, int n);

/* how many tcs on queue */
extern int tcqLen(TC_QUEUE_STRUCT * tcq);

/* look at nth item, first is 0 */
extern TC_STRUCT *tcqItem(TC_QUEUE_STRUCT * tcq, int n, int *status);

/* look at last item */
extern TC_STRUCT *tcqLast(TC_QUEUE_STRUCT * tcq, int *status);

/* get full status */
extern int tcqFull(TC_QUEUE_STRUCT * tcq);

#endif				/* TC_H */
