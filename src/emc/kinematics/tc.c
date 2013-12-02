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
#include "emcpos.h"
#include "tc.h"
#include "motion_types.h"
#include "rtapi_math.h"

//Debug output
#include "tp_debug.h"

//Empty function to act as an assert for GDB in simulation
int tcGetStartAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    if(tc->motion_type == TC_LINEAR || tc->motion_type == TC_RIGIDTAP) {
        *out=tc->coords.line.xyz.uVec;
    } else {
        PmCartesian startpoint;
        PmCartesian radius;
        PmCartesian tan, perp;

        pmCirclePoint(&tc->coords.circle.xyz, 0.0, &startpoint);
        pmCartCartSub(&startpoint, &tc->coords.circle.xyz.center, &radius);
        pmCartCartCross(&tc->coords.circle.xyz.normal, &radius, &tan);
        pmCartUnit(&tan, &tan);
        //The unit vector's actual direction is adjusted by the normal
        //acceleration here. This unit vector is NOT simply the tangent
        //direction.
        pmCartCartSub(&tc->coords.circle.xyz.center, &startpoint, &perp);
        pmCartUnit(&perp, &perp);

        pmCartScalMult(&tan, tc->maxaccel, &tan);
        pmCartScalMult(&perp, pmSq(0.5 * tc->reqvel)/tc->coords.circle.xyz.radius, &perp);
        pmCartCartAdd(&tan, &perp, out);
        pmCartUnit(out, out);
    }
    return 0;
}

int tcGetEndAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    if(tc->motion_type == TC_LINEAR) {
        *out=tc->coords.line.xyz.uVec;
    } else if(tc->motion_type == TC_RIGIDTAP) {
        // comes out the other way
        pmCartScalMult(&tc->coords.line.xyz.uVec, -1.0, out);
    } else {
        PmCartesian endpoint;
        PmCartesian radius;

        pmCirclePoint(&tc->coords.circle.xyz, tc->coords.circle.xyz.angle, &endpoint);
        pmCartCartSub(&endpoint, &tc->coords.circle.xyz.center, &radius);
        pmCartCartCross(&tc->coords.circle.xyz.normal, &radius, out);
        pmCartUnit(out, out);
    }
    return 0;
}

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
        pmCartUnit(&tan, &tan);
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

int tcGetStartTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    if(tc->motion_type == TC_LINEAR || tc->motion_type == TC_RIGIDTAP) {
        *out=tc->coords.line.xyz.uVec;
    } else if (tc->motion_type == TC_CIRCULAR) {
        tcGetHelicalTangentVector(&tc->coords.circle.xyz, 0.0, out);
    }
    else {
        rtapi_print_msg(RTAPI_MSG_ERR, "Invalid motion type %d!\n",tc->motion_type);
        return -1;
    }
    return 0;
}

int tcGetEndTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {

    if(tc->motion_type == TC_LINEAR) {
        *out=tc->coords.line.xyz.uVec;
    } else if(tc->motion_type == TC_RIGIDTAP) {
        // comes out the other way
        pmCartScalMult(&tc->coords.line.xyz.uVec, -1.0, out);
    } else if (tc->motion_type == TC_CIRCULAR){
        tcGetHelicalTangentVector(&tc->coords.circle.xyz,
                tc->coords.circle.xyz.angle, out);
    } else {
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
            //FIXME why is this here?
#if 0
            // if this is rapid move, don't use feed override settings (max velocity override is still honoured)
            if(tc->canon_motion_type==EMC_MOTION_TYPE_TRAVERSE) {tc->feed_override = 1.0;}
#endif
            if (tc->coords.line.xyz.tmag > 0.) {
                // progress is along xyz, so uvw and abc move proportionally in order
                // to end at the same time.
                pmCartLinePoint(&tc->coords.line.xyz, progress, &xyz);
                pmCartLinePoint(&tc->coords.line.uvw,
                        progress * tc->coords.line.uvw.tmag / tc->target,
                        &uvw);
                pmCartLinePoint(&tc->coords.line.abc,
                        progress * tc->coords.line.abc.tmag / tc->target,
                        &abc);
            } else if (tc->coords.line.uvw.tmag > 0.) {
                // xyz is not moving
                pmCartLinePoint(&tc->coords.line.xyz, 0.0, &xyz);
                pmCartLinePoint(&tc->coords.line.uvw, progress, &uvw);
                // abc moves proportionally in order to end at the same time
                pmCartLinePoint(&tc->coords.line.abc,
                        progress * tc->coords.line.abc.tmag / tc->target,
                        &abc);
            } else {
                // if all else fails, it's along abc only
                pmCartLinePoint(&tc->coords.line.xyz, 0.0, &xyz);
                pmCartLinePoint(&tc->coords.line.uvw, 0.0, &uvw);
                pmCartLinePoint(&tc->coords.line.abc, progress, &abc);
            }
            break;
        case TC_CIRCULAR:
            // progress is always along the xyz circle.  This simplification 
            // is possible since zero-radius arcs are not allowed by the interp.
            pmCirclePoint(&tc->coords.circle.xyz,
                    progress * tc->coords.circle.xyz.angle / tc->target, 
                    &xyz);
            // abc moves proportionally in order to end at the same time as the 
            // circular xyz move.
            pmCartLinePoint(&tc->coords.circle.abc,
                    progress * tc->coords.circle.abc.tmag / tc->target,
                    &abc);
            // same for uvw
            pmCartLinePoint(&tc->coords.circle.uvw,
                    progress * tc->coords.circle.uvw.tmag / tc->target,
                    &uvw);
            break;
        case TC_SPHERICAL:
            break;

    }

    pos->tran = xyz;
    pos->a = abc.x;
    pos->b = abc.y;
    pos->c = abc.z;
    pos->u = uvw.x;
    pos->v = uvw.y;
    pos->w = uvw.z;

    return 0;
}

/* Arc stuff */

/**
 * Define a 3D spherical arc based on three points and a radius.
 */
int pmCircleFromPoints(PmCircle * const arc, PmCartesian const * const start,
        PmCartesian const * const middle, PmCartesian const * const end,
        double radius) {

    //TODO macro this?
    if (NULL == arc) {
        rtapi_print_msg(RTAPI_MSG_ERR,"pmCircleFromPoints circle pointer is null\n");
        return -1;
    }

    PmCartesian v1, v2;
    tp_posemath_debug(" start = %f,%f,%f\n",start->x,start->y, start->z);
    tp_posemath_debug(" middle = %f,%f,%f\n",middle->x,middle->y, middle->z);
    tp_posemath_debug(" end = %f,%f,%f\n",end->x,end->y, end->z);

    //Find relative vectors from start to midpoint and mid to end point
    pmCartCartSub(middle, start, &v1);
    pmCartCartSub(end, middle, &v2);
    
    tp_posemath_debug(" Initial vectors\n");
    tp_posemath_debug(" v1 = %f,%f,%f\n",v1.x,v1.y, v1.z);
    tp_posemath_debug(" v2 = %f,%f,%f\n",v2.x,v2.y, v2.z);

    //Calculate gram-schmidt orthonormals 
    //For n2
    PmCartesian u1, u2, n1, n2;
    
    pmCartCartProj(&v2, &v1, &u2);
    pmCartCartSub(&v2, &u2, &n1);

    int res;
    res = pmCartUnit(&n1, &n1);

    if (res) {
        return res;
    }

    tp_posemath_debug(" n1 = %f,%f,%f\n",n1.x,n1.y, n1.z);

    //For n1

    pmCartCartProj(&v1, &v2, &u1);
    pmCartCartSub(&v1, &u1, &n2);
    res = pmCartUnit(&n2, &n2);

    if (res) {
        return res;
    }
    pmCartScalMult(&n2, -1.0, &n2);
    tp_posemath_debug(" n2 = %f,%f,%f\n",n2.x,n2.y, n2.z);

    PmCartesian binormal;

    pmCartCartCross(&v1, &v2, &binormal);

    res=pmCartUnit(&binormal, &binormal);

    if (res) {
        return res;
    }

    tp_posemath_debug(" v1 = %f,%f,%f\n",v1.x,v1.y, v1.z);
    tp_posemath_debug(" v2 = %f,%f,%f\n",v2.x,v2.y, v2.z);
    tp_posemath_debug(" binormal = %f,%f,%f\n",binormal.x,binormal.y, binormal.z);

    //Find the angle between the two vectors
    double dot;
    //TODO function here
    pmCartCartDot(&n1, &n2, &dot);
    // Check if the vectors are valid and compute the angle between them
    if (dot<1 && dot>-1) arc->angle=acos(dot);
    else return -1;

    //Overwrite v1 and v2 with normed v's
    pmCartUnit(&v1, &v1);
    pmCartUnit(&v2, &v2);

    PmCartesian dv, dn;
      
    //Subtract vectors
    pmCartCartSub(&n1, &n2, &dn);
    pmCartCartAdd(&v1, &v2, &dv);

    //Store the norms of each vector
    double dv_mag, dn_mag;
    pmCartMag(&dn, &dn_mag);
    pmCartMag(&dv, &dv_mag);

    arc->inscr_ratio = dn_mag / dv_mag;
    double d = arc->inscr_ratio * radius;

    //Prescale the unit vectors from before (not unit length anymore)
    pmCartScalMult(&v2, d, &v2);
    pmCartScalMult(&v1, -d, &v1);
    pmCartScalMult(&n1, radius, &n1);
    pmCartScalMult(&n2, radius, &n2);

    PmCartesian center;
    PmCartesian circ_start;
    PmCartesian circ_end;

    //Add one set of vectors to get the center
    tp_posemath_debug("v2 = %f, %f,%f\n",v2.x,v2.y,v2.z);
    tp_posemath_debug("n2 = %f, %f,%f\n",n2.x,n2.y,n2.z);
    pmCartCartAdd(&v2, &n2, &center);

    tp_posemath_debug("v2 + n2 = %f, %f,%f\n",center.x,center.y,center.z);
    pmCartCartAdd(middle, &center, &center);
    pmCartCartAdd(middle, &v1, &circ_start);
    pmCartCartAdd(middle, &v2, &circ_end);

    tp_posemath_debug("d = %f\n",d);
    tp_posemath_debug("center = %f, %f,%f\n",center.x,center.y,center.z);
    tp_posemath_debug("circ_start = %f, %f,%f\n",circ_start.x,circ_start.y,circ_start.z);
    tp_posemath_debug("circ_end = %f, %f,%f\n",circ_end.x,circ_end.y,circ_end.z);

    pmCircleInit(arc,&circ_start,&circ_end,&center,&binormal,0);

    tp_posemath_debug("center = %f, %f,%f\n",arc->center.x,arc->center.y,arc->center.z);
    tp_posemath_debug("rTan = %f, %f,%f\n",arc->rTan.x,arc->rTan.y,arc->rTan.z);
    tp_posemath_debug("rPerp = %f, %f,%f\n",arc->rPerp.x,arc->rPerp.y,arc->rPerp.z);

    return 0;
}


int tcSetTermCond(TC_STRUCT * const tc, int term_cond) {
    tc_debug_print("setting term condition %d on tc id %d, type %d\n", term_cond, tc->id, tc->motion_type);
    tc->term_cond = term_cond;
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
 * @param    tcq       pointer to the new TC_QUEUE_STRUCT
 * @param	 tc        the new TC element to be added
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

    //HACK - check if segment is started and abort?
    /*if ((tcq->queue[tcq->end]).progress > 0.0) {*/
        /*return -1;*/
    /*}*/
    tcq->end = (tcq->end -1) % tcq->size;
    tcq->allFull = 0;
    tcq->_len -= 1;

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

