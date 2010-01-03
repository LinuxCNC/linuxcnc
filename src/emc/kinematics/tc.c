/*!
********************************************************************
* Description: tc.c
*\brief Discriminate-based trajectory planning
*
*\author Derived from a work by Fred Proctor & Will Shackleford
*\author rewritten by Chris Radek
*
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
********************************************************************/

/*
  FIXME-- should include <stdlib.h> for sizeof(), but conflicts with
  a bunch of <linux> headers
  */
#include "rtapi.h"		/* rtapi_print_msg */
#include "posemath.h"
#include "emcpos.h"
#include "tc.h"

PmCartesian tcGetStartingUnitVector(TC_STRUCT *tc) {
    PmCartesian v;

    if(tc->motion_type == TC_LINEAR || tc->motion_type == TC_RIGIDTAP) {
        pmCartCartSub(tc->coords.line.xyz.end.tran, tc->coords.line.xyz.start.tran, &v);
    } else {
        PmPose startpoint;
        PmCartesian radius;

        pmCirclePoint(&tc->coords.circle.xyz, 0.0, &startpoint);
        pmCartCartSub(startpoint.tran, tc->coords.circle.xyz.center, &radius);
        pmCartCartCross(tc->coords.circle.xyz.normal, radius, &v);
    }
    pmCartUnit(v, &v);
    return v;
}

PmCartesian tcGetEndingUnitVector(TC_STRUCT *tc) {
    PmCartesian v;

    if(tc->motion_type == TC_LINEAR) {
        pmCartCartSub(tc->coords.line.xyz.end.tran, tc->coords.line.xyz.start.tran, &v);
    } else if(tc->motion_type == TC_RIGIDTAP) {
        // comes out the other way
        pmCartCartSub(tc->coords.line.xyz.start.tran, tc->coords.line.xyz.end.tran, &v);
    } else {
        PmPose endpoint;
        PmCartesian radius;

        pmCirclePoint(&tc->coords.circle.xyz, tc->coords.circle.xyz.angle, &endpoint);
        pmCartCartSub(endpoint.tran, tc->coords.circle.xyz.center, &radius);
        pmCartCartCross(tc->coords.circle.xyz.normal, radius, &v);
    }
    pmCartUnit(v, &v);
    return v;
}

/*! tcGetPos() function
 *
 * \brief This function calculates the machine position along the motion's path.
 *
 * As we move along a TC, from zero to its length, we call this function repeatedly,
 * with an increasing tc->progress.
 * This function calculates the machine position along the motion's path 
 * corresponding to the current progress.
 * It gets called at the end of tpRunCycle()
 * 
 * @param    tc    the current TC that is being planned
 *
 * @return	 EmcPose   returns a position (\ref EmcPose = datatype carrying XYZABC information
 */   

EmcPose tcGetPos(TC_STRUCT * tc) {
    return tcGetPosReal(tc, 0);
}

EmcPose tcGetEndpoint(TC_STRUCT * tc) {
    return tcGetPosReal(tc, 1);
}

EmcPose tcGetPosReal(TC_STRUCT * tc, int of_endpoint)
{
    EmcPose pos;
    PmPose xyz;
    PmPose abc;
    PmPose uvw;
    
    double progress = of_endpoint? tc->target: tc->progress;

    if (tc->motion_type == TC_RIGIDTAP) {
        if(tc->coords.rigidtap.state > REVERSING) {
            pmLinePoint(&tc->coords.rigidtap.aux_xyz, progress, &xyz);
        } else {
            pmLinePoint(&tc->coords.rigidtap.xyz, progress, &xyz);
        }
        // no rotary move allowed while tapping
        abc.tran = tc->coords.rigidtap.abc;
        uvw.tran = tc->coords.rigidtap.uvw;
    } else if (tc->motion_type == TC_LINEAR) {
        if (tc->coords.line.xyz.tmag > 0.) {
            // progress is along xyz, so uvw and abc move proportionally in order
            // to end at the same time.
            pmLinePoint(&tc->coords.line.xyz, progress, &xyz);
            pmLinePoint(&tc->coords.line.uvw,
                        progress * tc->coords.line.uvw.tmag / tc->target,
                        &uvw);
            pmLinePoint(&tc->coords.line.abc,
                        progress * tc->coords.line.abc.tmag / tc->target,
                        &abc);
        } else if (tc->coords.line.uvw.tmag > 0.) {
            // xyz is not moving
            pmLinePoint(&tc->coords.line.xyz, 0.0, &xyz);
            pmLinePoint(&tc->coords.line.uvw, progress, &uvw);
            // abc moves proportionally in order to end at the same time
            pmLinePoint(&tc->coords.line.abc,
                        progress * tc->coords.line.abc.tmag / tc->target,
                        &abc);
        } else {
            // if all else fails, it's along abc only
            pmLinePoint(&tc->coords.line.xyz, 0.0, &xyz);
            pmLinePoint(&tc->coords.line.uvw, 0.0, &uvw);
            pmLinePoint(&tc->coords.line.abc, progress, &abc);
        }
    } else { //we have TC_CIRCULAR
        // progress is always along the xyz circle.  This simplification 
        // is possible since zero-radius arcs are not allowed by the interp.
        pmCirclePoint(&tc->coords.circle.xyz,
		      progress * tc->coords.circle.xyz.angle / tc->target, 
                      &xyz);
        // abc moves proportionally in order to end at the same time as the 
        // circular xyz move.
        pmLinePoint(&tc->coords.circle.abc,
                    progress * tc->coords.circle.abc.tmag / tc->target, 
                    &abc);
        // same for uvw
        pmLinePoint(&tc->coords.circle.uvw,
                    progress * tc->coords.circle.uvw.tmag / tc->target, 
                    &uvw);
    }

    pos.tran = xyz.tran;
    pos.a = abc.tran.x;
    pos.b = abc.tran.y;
    pos.c = abc.tran.z;
    pos.u = uvw.tran.x;
    pos.v = uvw.tran.y;
    pos.w = uvw.tran.z;

    return pos;
}


/*!
 * \subsection TC queue functions
 * These following functions implement the motion queue that
 * is fed by tpAddLine/tpAddCircle and consumed by tpRunCycle.
 * They have been fully working for a long time and a wise programmer
 * won't mess with them.
 */


/*! tcqCreate() function
 *
 * \brief Creates a new queue for TC elements.
 *
 * This function creates a new queue for TC elements. 
 * It gets called by tpCreate()
 * 
 * @param    tcq       pointer to the new TC_QUEUE_STRUCT
 * @param	 _size	   size of the new queue
 * @param	 tcSpace   holds the space allocated for the new queue, allocated in motion.c
 *
 * @return	 int	   returns success or failure
 */   
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

/*! tcqDelete() function
 *
 * \brief Deletes a queue holding TC elements.
 *
 * This function creates deletes a queue. It doesn't free the space
 * only throws the pointer away. 
 * It gets called by tpDelete() 
 * \todo FIXME, it seems tpDelete() is gone, and this function isn't used.
 * 
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns success
 */   
int tcqDelete(TC_QUEUE_STRUCT * tcq)
{
    if (0 != tcq && 0 != tcq->queue) {
	/* free(tcq->queue); */
	tcq->queue = 0;
    }

    return 0;
}

/*! tcqInit() function
 *
 * \brief Initializes a queue with TC elements.
 *
 * This function initializes a queue with TC elements. 
 * It gets called by tpClear() and  
 * 	  	   		  by tpRunCycle() when we are aborting
 * 
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns success or failure (if no tcq found)
 */
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

/*! tcqPut() function
 *
 * \brief puts a TC element at the end of the queue
 *
 * This function adds a tc element at the end of the queue. 
 * It gets called by tpAddLine() and tpAddCircle()
 * 
 * @param    tcq       pointer to the new TC_QUEUE_STRUCT
 * @param	 tc        the new TC element to be added
 *
 * @return	 int	   returns success or failure
 */   
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

/*! tcqRemove() function
 *
 * \brief removes n items from the queue
 *
 * This function removes the first n items from the queue,
 * after checking that they can be removed 
 * (queue initialized, queue not empty, enough elements in it) 
 * Function gets called by tpRunCycle() with n=1
 * \todo FIXME: Optimize the code to remove only 1 element, might speed it up
 * 
 * @param    tcq       pointer to the new TC_QUEUE_STRUCT
 * @param	 n         the number of TC elements to be removed
 *
 * @return	 int	   returns success or failure
 */   
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

/*! tcqLen() function
 *
 * \brief returns the number of elements in the queue
 *
 * Function gets called by tpSetVScale(), tpAddLine(), tpAddCircle()
 * 
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns number of elements
 */   
int tcqLen(TC_QUEUE_STRUCT * tcq)
{
    if (0 == tcq) {
	    return -1;
    }

    return tcq->_len;
}

/*! tcqItem() function
 *
 * \brief gets the n-th TC element in the queue, without removing it
 *
 * Function gets called by tpSetVScale(), tpRunCycle(), tpIsPaused()
 * 
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 TC_STRUCT returns the TC elements
 */   
TC_STRUCT *tcqItem(TC_QUEUE_STRUCT * tcq, int n, long period)
{
    TC_STRUCT *t;
    if ((0 == tcq) || (0 == tcq->queue) ||	/* not initialized */
	(n < 0) || (n >= tcq->_len)) {	/* n too large */
	return (TC_STRUCT *) 0;
    }
    t = &(tcq->queue[(tcq->start + n) % tcq->size]);
    return t;
}

/*! 
 * \def TC_QUEUE_MARGIN
 * sets up a margin at the end of the queue, to reduce effects of race conditions
 */
#define TC_QUEUE_MARGIN 10

/*! tcqFull() function
 *
 * \brief get the full status of the queue 
 * Function returns full if the count is closer to the end of the queue than TC_QUEUE_MARGIN
 *
 * Function called by update_status() in control.c 
 * 
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int       returns status (0==not full, 1==full)
 */   
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

