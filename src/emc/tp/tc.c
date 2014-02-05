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
*
* Last change:
********************************************************************/

/*
  FIXME-- should include <stdlib.h> for sizeof(), but conflicts with
  a bunch of <linux> headers
  */
#include "rtapi.h"		/* rtapi_print_msg */
#include "posemath.h"
#include "emcpose.h"
#include "tc.h"
#include "spherical_arc.h"
#include "motion_types.h"
#include "rtapi_math.h"

//Debug output
#include "tp_debug.h"


int tcCircleStartAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out)
{
    PmCartesian startpoint;
    PmCartesian radius;
    PmCartesian tan, perp;

    pmCirclePoint(&tc->coords.circle.xyz, 0.0, &startpoint);
    pmCartCartSub(&startpoint, &tc->coords.circle.xyz.center, &radius);
    pmCartCartCross(&tc->coords.circle.xyz.normal, &radius, &tan);
    pmCartUnitEq(&tan);
    //The unit vector's actual direction is adjusted by the normal
    //acceleration here. This unit vector is NOT simply the tangent
    //direction.
    pmCartCartSub(&tc->coords.circle.xyz.center, &startpoint, &perp);
    pmCartUnitEq(&perp);

    pmCartScalMult(&tan, tc->maxaccel, &tan);
    pmCartScalMultEq(&perp, pmSq(0.5 * tc->reqvel)/tc->coords.circle.xyz.radius);
    pmCartCartAdd(&tan, &perp, out);
    pmCartUnitEq(out);
    return 0;
}

int tcCircleEndAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out)
{
    PmCartesian endpoint;
    PmCartesian radius;

    pmCirclePoint(&tc->coords.circle.xyz, tc->coords.circle.xyz.angle, &endpoint);
    pmCartCartSub(&endpoint, &tc->coords.circle.xyz.center, &radius);
    pmCartCartCross(&tc->coords.circle.xyz.normal, &radius, out);
    pmCartUnitEq(out);
    return 0;
}

/**
 * Get the acceleration direction unit vector for blend velocity calculations.
 * This calculates the direction of acceleration at the start of a segment.
 */
int tcGetStartAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    switch (tc->motion_type) {
        case TC_LINEAR:
        case TC_RIGIDTAP:
            *out=tc->coords.line.xyz.uVec;
            break;
        case TC_CIRCULAR:
            tcCircleStartAccelUnitVector(tc,out);
            break;
        case TC_SPHERICAL:
            return -1;
        default:
            return -1;
    }
    return 0;
}

/**
 * Get the acceleration direction unit vector for blend velocity calculations.
 * This calculates the direction of acceleration at the end of a segment.
 */
int tcGetEndAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    switch (tc->motion_type) {
        case TC_LINEAR:
            *out=tc->coords.line.xyz.uVec;
            break;
        case TC_RIGIDTAP:
            pmCartScalMult(&tc->coords.line.xyz.uVec, -1.0, out);
            break;
        case TC_CIRCULAR:
            tcCircleEndAccelUnitVector(tc,out);
            break;
       case TC_SPHERICAL:
            return -1;
       default:
            return -1;
    }
    return 0;
}


/**
 * Find the geometric tangent vector to a helical arc.
 * Unlike the acceleration vector, the result of this calculation is a vector
 * tangent to the helical arc. This is called by wrapper functions for the case of a circular or helical arc.
 */
static int tcGetHelicalTangentVector(PmCircle const * const circle, double progress,
        PmCartesian * const out) {

    PmCartesian startpoint;
    PmCartesian radius;
    PmCartesian tan, helix;

    pmCirclePoint(circle, progress, &startpoint);
    pmCartCartSub(&startpoint, &circle->center, &radius);
    pmCartCartCross(&circle->normal, &radius, &tan);

    //Helix component
    double h;
    pmCartMag(&circle->rHelix, &h);
    if (h>0.0){
        //Pre-scale tangent vector to unit length
        pmCartUnitEq(&tan);
        //No degeneracy because we have nonzero angle and radius
        double ratio = 1.0 / (circle->radius * circle->angle);
        //Scale the helix vector to be proportional to the unit tangent vector
        pmCartScalMult(&circle->rHelix, ratio, &helix);
        //Add scaled helix vector to "plane tangent" to get curve tangent vector
        pmCartCartAdd(&helix, &tan, &tan);
    }
    //Normalize final output vector
    pmCartUnit(&tan, out);
    return 0;
}


/**
 * Calulate the unit tangent vector at the start of a move for any segment.
 */
int tcGetStartTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    switch (tc->motion_type) {
        case TC_LINEAR:
            *out=tc->coords.line.xyz.uVec;
            break;
        case TC_RIGIDTAP:
            *out=tc->coords.rigidtap.xyz.uVec;
            break;
        case TC_CIRCULAR:
            tcGetHelicalTangentVector(&tc->coords.circle.xyz, 0.0, out);
            break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid motion type %d!\n",tc->motion_type);
            return -1;
    }
    return 0;
}

/**
 * Calulate the unit tangent vector at the end of a move for any segment.
 */
int tcGetEndTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    switch (tc->motion_type) {
        case TC_LINEAR:
            *out=tc->coords.line.xyz.uVec;
            break;
        case TC_RIGIDTAP:
            pmCartScalMult(&tc->coords.rigidtap.xyz.uVec, -1.0, out);
            break;
        case TC_CIRCULAR:
            tcGetHelicalTangentVector(&tc->coords.circle.xyz,
                    tc->coords.circle.xyz.angle, out);
            break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid motion type %d!\n",tc->motion_type);
            return -1;
    }
    return 0;
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

int tcGetPos(TC_STRUCT const * const tc, EmcPose * const out) {
    tcGetPosReal(tc, TC_GET_PROGRESS, out);
    return 0;
}

int tcGetStartpoint(TC_STRUCT const * const tc, EmcPose * const out) {
    tcGetPosReal(tc, TC_GET_STARTPOINT, out);
    return 0;
}

int tcGetEndpoint(TC_STRUCT const * const tc, EmcPose * const out) {
    tcGetPosReal(tc, TC_GET_ENDPOINT, out);
    return 0;
}

int tcGetPosReal(TC_STRUCT const * const tc, int of_point, EmcPose * const pos)
{
    PmCartesian xyz;
    PmCartesian abc;
    PmCartesian uvw;
    double progress=0.0;

    switch (of_point) {
        case TC_GET_PROGRESS:
            progress = tc->progress;
            break;
        case TC_GET_ENDPOINT:
            progress = tc->target;
            break;
        case TC_GET_STARTPOINT:
            progress = 0.0;
            break;
    }

    switch (tc->motion_type){
        case TC_RIGIDTAP:
            if(tc->coords.rigidtap.state > REVERSING) {
                pmCartLinePoint(&tc->coords.rigidtap.aux_xyz, progress, &xyz);
            } else {
                pmCartLinePoint(&tc->coords.rigidtap.xyz, progress, &xyz);
            }
            // no rotary move allowed while tapping
            abc = tc->coords.rigidtap.abc;
            uvw = tc->coords.rigidtap.uvw;
            break;
        case TC_LINEAR:
            pmCartLinePoint(&tc->coords.line.xyz,
                    progress * tc->coords.line.xyz.tmag / tc->target,
                    &xyz);
            pmCartLinePoint(&tc->coords.line.uvw,
                    progress * tc->coords.line.uvw.tmag / tc->target,
                    &uvw);
            pmCartLinePoint(&tc->coords.line.abc,
                    progress * tc->coords.line.abc.tmag / tc->target,
                    &abc);
            break;
        case TC_CIRCULAR:
            pmCirclePoint(&tc->coords.circle.xyz,
                    progress * tc->coords.circle.xyz.angle / tc->target,
                    &xyz);
            pmCartLinePoint(&tc->coords.circle.abc,
                    progress * tc->coords.circle.abc.tmag / tc->target,
                    &abc);
            pmCartLinePoint(&tc->coords.circle.uvw,
                    progress * tc->coords.circle.uvw.tmag / tc->target,
                    &uvw);
            break;
        case TC_SPHERICAL:
            arcPoint(&tc->coords.arc.xyz,
                    progress,
                    &xyz);
            abc = tc->coords.arc.abc;
            uvw = tc->coords.arc.uvw;
            break;
    }

    pmCartesianToEmcPose(&xyz, &abc, &uvw, pos);
    return 0;
}


/**
 * Set the terminal condition of a segment.
 * This function will eventually handle state changes associated with altering a terminal condition.
 */
int tcSetTermCond(TC_STRUCT * const tc, int term_cond) {
    tc_debug_print("setting term condition %d on tc id %d, type %d\n", term_cond, tc->id, tc->motion_type);
    tc->term_cond = term_cond;
    return 0;
}


/**
 * Connect a blend arc to the two line segments it blends.
 * Starting with two adjacent line segments, this function shortens each
 * segment to connect them with the newly created blend arc. The "previous"
 * segment gets a new end point, while the next segment gets a new start point.
 * After the operation is complete the result is a set of 3 connected segments
 * (line-arc-line).
 */
int tcConnectBlendArc(TC_STRUCT * const prev_tc, TC_STRUCT * const tc,
        PmCartesian const * const circ_start,
        PmCartesian const * const circ_end) {

    /* Only shift XYZ for now*/
    if (prev_tc) {
        tp_debug_print("connect: keep prev_tc\n");
        //Have prev line, need to shorten it
        pmCartLineInit(&prev_tc->coords.line.xyz,
                &prev_tc->coords.line.xyz.start, circ_start);
        tp_debug_print("Old target = %f\n", prev_tc->target);
        prev_tc->target = prev_tc->coords.line.xyz.tmag;
        tp_debug_print("Target = %f\n",prev_tc->target);
        //Setup tangent blending constraints
        tcSetTermCond(prev_tc, TC_TERM_COND_TANGENT);
        tp_debug_print(" L1 end  : %f %f %f\n",prev_tc->coords.line.xyz.end.x,
                prev_tc->coords.line.xyz.end.y,
                prev_tc->coords.line.xyz.end.z);
    } else {
        tp_debug_print("connect: consume prev_tc\n");
    }

    //Shorten next line
    pmCartLineInit(&tc->coords.line.xyz, circ_end, &tc->coords.line.xyz.end);

    tp_info_print(" L2: old target = %f\n", tc->target);
    tc->target = tc->coords.line.xyz.tmag;
    tp_info_print(" L2: new target = %f\n", tc->target);
    tp_debug_print(" L2 start  : %f %f %f\n",tc->coords.line.xyz.start.x,
            tc->coords.line.xyz.start.y,
            tc->coords.line.xyz.start.z);

    //Disable flag for parabolic blending with previous
    tc->blend_prev = 0;

    tp_info_print("       Q1: %f %f %f\n",circ_start->x,circ_start->y,circ_start->z);
    tp_info_print("       Q2: %f %f %f\n",circ_end->x,circ_end->y,circ_end->z);

    return 0;
}


/**
 * Check if the current segment is actively blending.
 * Checks if a blend should start based on acceleration and velocity criteria.
 * Also saves this status so that the blend continues until the segment is
 * done.
 */
int tcIsBlending(TC_STRUCT * const tc) {
    //FIXME Disabling blends for rigid tap cycle until changes can be verified.
    int is_blending_next = (tc->term_cond == TC_TERM_COND_PARABOLIC ) &&
        tc->on_final_decel && (tc->currentvel < tc->blend_vel) &&
        tc->motion_type != TC_RIGIDTAP;

    //Latch up the blending_next status here, so that even if the prev conditions
    //aren't necessarily true we still blend to completion once the blend
    //starts.
    tc->blending_next |= is_blending_next;

    return tc->blending_next;
}

int tcFindBlendTolerance(TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc, double * const T_blend, double * const nominal_tolerance)
{
    const double tolerance_ratio = 0.25;
    double T1 = prev_tc->tolerance;
    double T2 = tc->tolerance;
    //Detect zero tolerance = no tolerance and force to reasonable maximum
    if (T1 == 0) {
        T1 = prev_tc->nominal_length * tolerance_ratio;
    }
    if (T2 == 0) {
        T2 = tc->nominal_length * tolerance_ratio;
    }
    *nominal_tolerance = fmin(T1,T2);
    //Blend tolerance is the limit of what we can reach by blending alone,
    //consuming half a segment or less (parabolic equivalent)
    double blend_tolerance = fmin(fmin(*nominal_tolerance, 
                prev_tc->nominal_length * tolerance_ratio),
            tc->nominal_length * tolerance_ratio);
    *T_blend = blend_tolerance;
    return 0;
}



/*!
 * \subsection TC queue functions
 * These following functions implement the motion queue that
 * is fed by tpAddLine/tpAddCircle and consumed by tpRunCycle.
 * They have been fully working for a long time and a wise programmer
 * won't mess with them.
 */

/** Return 0 if queue is valid, -1 if not */
static inline int tcqCheck(TC_QUEUE_STRUCT const * const tcq)
{
    if ((0 == tcq) || (0 == tcq->queue))
    {
        return -1;
    }
    return 0;
}

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
int tcqCreate(TC_QUEUE_STRUCT * const tcq, int _size, TC_STRUCT * const tcSpace)
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
int tcqDelete(TC_QUEUE_STRUCT * const tcq)
{
    if (!tcqCheck(tcq)) {
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
int tcqInit(TC_QUEUE_STRUCT * const tcq)
{
    if (tcqCheck(tcq)) return -1;

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
int tcqPut(TC_QUEUE_STRUCT * const tcq, TC_STRUCT const * const tc)
{
    /* check for initialized */
    if (tcqCheck(tcq)) return -1;

    /* check for allFull, so we don't overflow the queue */
    if (tcq->allFull) {
	    return -1;
    }

    /* add it */
    tcq->queue[tcq->end] = *tc;
    tcq->_len++;

    /* update end ptr, modulo size of queue */
    tcq->end = (tcq->end + 1) % tcq->size;

    /* set allFull flag if we're really full */
    if (tcq->end == tcq->start) {
	tcq->allFull = 1;
    }

    return 0;
}


/*! tcqPopBack() function
 *
 * \brief removes the newest TC element (converse of tcqRemove)
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns success or failure
 */
int tcqPopBack(TC_QUEUE_STRUCT * const tcq)
{
    /* check for initialized */
    if (tcqCheck(tcq)) return -1;

    /* Too short to pop! */
    if (tcq->_len < 1) {
        return -1;
    }

    int n = tcq->end - 1 + tcq->size;
    tcq->end = n % tcq->size;
    tcq->_len--;

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
int tcqRemove(TC_QUEUE_STRUCT * const tcq, int n)
{

    if (n <= 0) {
	    return 0;		/* okay to remove 0 or fewer */
    }

    if (tcqCheck(tcq) || ((tcq->start == tcq->end) && !tcq->allFull) ||
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
int tcqLen(TC_QUEUE_STRUCT const * const tcq)
{
    if (tcqCheck(tcq)) return -1;

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
TC_STRUCT * tcqItem(TC_QUEUE_STRUCT const * const tcq, int n)
{
    if (tcqCheck(tcq) || (n < 0) || (n >= tcq->_len)) return NULL;

    return &(tcq->queue[(tcq->start + n) % tcq->size]);
}

/*!
 * \def TC_QUEUE_MARGIN
 * sets up a margin at the end of the queue, to reduce effects of race conditions
 */
#define TC_QUEUE_MARGIN 20

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
int tcqFull(TC_QUEUE_STRUCT const * const tcq)
{
    if (tcqCheck(tcq)) {
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

/*! tcqLast() function
 *
 * \brief gets the last TC element in the queue, without removing it
 *
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 TC_STRUCT returns the TC element
 */
TC_STRUCT *tcqLast(TC_QUEUE_STRUCT const * const tcq)
{
    if (tcqCheck(tcq)) {
        return NULL;
    }
    if (tcq->_len == 0) {
        return NULL;
    }
    //Fix for negative modulus error
    int n = tcq->end-1 + tcq->size;
    return &(tcq->queue[n % tcq->size]);

}

