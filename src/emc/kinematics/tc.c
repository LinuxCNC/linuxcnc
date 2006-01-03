/********************************************************************
* Description: tc.c
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

#ifdef ULAPI
#include <stdio.h>
#endif
/*
  FIXME-- should include <stdlib.h> for sizeof(), but conflicts with
  a bunch of <linux> headers
  */
#include "rtapi.h"		/* rtapi_print_msg */
#include "posemath.h"
#include "emcpos.h"
#include "tc.h"


// the motion's target is its length; progress starts at zero and represents
// the progress along the length toward target.  This function calculates 
// the machine position along the motion's path corresponding to the current
// progress.

EmcPose tcGetPos(TC_STRUCT * tc)
{
    EmcPose pos;
    PmPose xyz;
    PmPose abc;

    if (tc->motion_type == TC_LINEAR) {
        if (tc->coords.line.xyz.tmag < 1e-6) {
            // no xyz move, so progress is along the abc line
            pmLinePoint(&tc->coords.line.xyz, 0.0, &xyz);
            pmLinePoint(&tc->coords.line.abc, tc->progress, &abc);
        } else {
            // progress is along xyz, and abc moves proportionally in order
            // to end at the same time.
            pmLinePoint(&tc->coords.line.xyz, tc->progress, &xyz);
            pmLinePoint(&tc->coords.line.abc, 
                        tc->progress * tc->coords.line.abc.tmag / tc->target,
                        &abc);
        }
    } else {
        // progress is always along the xyz circle.  This simplification 
        // is possible since zero-radius arcs are not allowed by the interp.
	pmCirclePoint(&tc->coords.circle.xyz,
		      tc->progress * tc->coords.circle.xyz.angle / tc->target, 
                      &xyz);
        // abc moves proportionally in order to end at the same time as the 
        // circular xyz move.
        pmLinePoint(&tc->coords.circle.abc,
                    tc->progress * tc->coords.circle.abc.tmag / tc->target, 
                    &abc);
    }

    pos.tran = xyz.tran;
    pos.a = abc.tran.x;
    pos.b = abc.tran.y;
    pos.c = abc.tran.z;

    return pos;
}

// These following functions implement the motion queue that
// is fed by tpAddLine/tpAddCircle and consumed by tpRunCycle.
// They have been fully working for a long time and a wise programmer
// won't mess with them.

int tcqCreate(TC_QUEUE_STRUCT * tcq, int _size, TC_STRUCT * tcSpace)
{
    if (_size <= 0 || 0 == tcq) {
	return -1;
    } else {
	tcq->queue = tcSpace;
	tcq->size = _size;
	tcq->_len = 0;
	tcq->start = tcq->end = 0;
	tcq->allFull = 0;

	if (0 == tcq->queue) {
	    return -1;
	}
	return 0;
    }
}

int tcqDelete(TC_QUEUE_STRUCT * tcq)
{
    if (0 != tcq && 0 != tcq->queue) {
	/* free(tcq->queue); */
	tcq->queue = 0;
    }

    return 0;
}

int tcqInit(TC_QUEUE_STRUCT * tcq)
{
    if (0 == tcq) {
	return -1;
    }

    tcq->_len = 0;
    tcq->start = tcq->end = 0;
    tcq->allFull = 0;

    return 0;
}

/* put sometc on the end of the queue */
int tcqPut(TC_QUEUE_STRUCT * tcq, TC_STRUCT tc)
{

    /* check for initialized */
    if (0 == tcq || 0 == tcq->queue) {
	return -1;
    }

    /* check for allFull, so we don't overflow the queue */
    if (tcq->allFull) {
	return -1;
    }

    /* add it */
    tcq->queue[tcq->end] = tc;
    tcq->_len++;

    /* update end ptr, modulo size of queue */
    tcq->end = (tcq->end + 1) % tcq->size;

    /* set allFull flag if we're really full */
    if (tcq->end == tcq->start) {
	tcq->allFull = 1;
    }

    return 0;
}

/* remove n items from the queue */
int tcqRemove(TC_QUEUE_STRUCT * tcq, int n)
{

    if (n <= 0) {
	return 0;		/* okay to remove 0 or fewer */
    }

    if ((0 == tcq) || (0 == tcq->queue) ||	/* not initialized */
	((tcq->start == tcq->end) && !tcq->allFull) ||	/* empty queue */
	(n > tcq->_len)) {	/* too many requested */
	return -1;
    }

    /* update start ptr and reset allFull flag and len */
    tcq->start = (tcq->start + n) % tcq->size;
    tcq->allFull = 0;
    tcq->_len -= n;

    return 0;
}

/* how many items are in the queue? */
int tcqLen(TC_QUEUE_STRUCT * tcq)
{
    if (0 == tcq) {
	return -1;
    }

    return tcq->_len;
}

/* get the nth item from the queue, [0..len-1], without removing */
TC_STRUCT *tcqItem(TC_QUEUE_STRUCT * tcq, int n)
{
    if ((0 == tcq) || (0 == tcq->queue) ||	/* not initialized */
	(n < 0) || (n >= tcq->_len)) {	/* n too large */
	return (TC_STRUCT *) 0;
    }
    return &(tcq->queue[(tcq->start + n) % tcq->size]);
}

/* get the full status of the queue */
#define TC_QUEUE_MARGIN 10
int tcqFull(TC_QUEUE_STRUCT * tcq)
{
    if (0 == tcq) {
	return 1;		/* null queue is full, for safety */
    }

    /* call the queue full if the length is into the margin, so reduce the
       effect of a race condition where the appending process may not see the 
       full status immediately and send another motion */

    if (tcq->size <= TC_QUEUE_MARGIN) {
	/* no margin available, so full means really all full */
	return tcq->allFull;
    }

    if (tcq->_len >= tcq->size - TC_QUEUE_MARGIN) {
	/* we're into the margin, so call it full */
	return 1;
    }

    /* we're not into the margin */
    return 0;
}

