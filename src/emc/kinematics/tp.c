/********************************************************************
* Description: tp.c
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

#ifdef ULAPI
#include <stdio.h>
#include <stdlib.h>
#endif

#include "rtapi.h"		/* rtapi_print_msg */
#include "posemath.h"
#include "tc.h"
#include "tp.h"

#include "../motion/motion.h"

/* how close to accel limit we should go */
#define ACCEL_USAGE 0.95

extern emcmot_status_t *emcmotStatus;

int output_chan = 0;

int tpCreate(TP_STRUCT * tp, int _queueSize, TC_STRUCT * tcSpace)
{
    if (0 == tp) {
	return -1;
    }

    if (_queueSize <= 0) {
	tp->queueSize = TP_DEFAULT_QUEUE_SIZE;
    } else {
	tp->queueSize = _queueSize;
    }

    /* create the queue */
    if (-1 == tcqCreate(&tp->queue, tp->queueSize, tcSpace)) {
	return -1;
    }

    /* init the rest of our data */
    return tpInit(tp);
}

/*
  tpClear() is a "soft init" in the sense that the TP_STRUCT configuration
  parameters (cycleTime, vMax, and aMax) are left alone, but the queue is
  cleared, and the flags are set to an empty, ready queue. The currentPos
  is left alone, and goalPos is set to this position.

  vScale is set to vRestore.

  This function is intended to put the motion queue in the state it would
  be if all queued motions finished at the current position.
 */
int tpClear(TP_STRUCT * tp)
{
    tcqInit(&tp->queue);
    tp->goalPos.tran.x = tp->currentPos.tran.x;
    tp->goalPos.tran.y = tp->currentPos.tran.y;
    tp->goalPos.tran.z = tp->currentPos.tran.z;
    tp->goalPos.a = tp->currentPos.a;
    tp->goalPos.b = tp->currentPos.b;
    tp->goalPos.c = tp->currentPos.c;
    tp->nextId = 0;
    tp->execId = 0;
    tp->motionType = 0;
    tp->termCond = TC_TERM_COND_BLEND;
    tp->tolerance = 0.0;
    tp->done = 1;
    tp->depth = tp->activeDepth = 0;
    tp->aborting = 0;
    tp->pausing = 0;
    tp->vScale = tp->vRestore;
    tp->synchronized = 0;
    tp->uu_per_rev = 0.0;

    return 0;
}

int tpInit(TP_STRUCT * tp)
{
    tp->cycleTime = 0.0;
    tp->vLimit = 0.0;
    tp->vScale = tp->vRestore = 1.0;
    tp->aMax = 0.0;
    tp->vMax = 0.0;
    tp->ini_maxvel = 0.0;
    tp->wMax = 0.0;
    tp->wDotMax = 0.0;

    tp->currentPos.tran.x = 0.0;
    tp->currentPos.tran.y = 0.0;
    tp->currentPos.tran.z = 0.0;
    tp->currentPos.a = 0.0;
    tp->currentPos.b = 0.0;
    tp->currentPos.c = 0.0;
    
    return tpClear(tp);
}

int tpSetCycleTime(TP_STRUCT * tp, double secs)
{
    if (0 == tp || secs <= 0.0) {
	return -1;
    }

    tp->cycleTime = secs;

    return 0;
}

// This is called before adding lines or circles, specifying
// vMax (the velocity requested by the F word) and
// ini_maxvel, the max velocity possible before meeting
// a machine constraint caused by an AXIS's max velocity.
// (the TP is allowed to go up to this high when feed 
// override >100% is requested)  These settings apply to
// subsequent moves until changed.

int tpSetVmax(TP_STRUCT * tp, double vMax, double ini_maxvel)
{
    if (0 == tp || vMax <= 0.0 || ini_maxvel <= 0.0) {
	return -1;
    }

    tp->vMax = vMax;
    tp->ini_maxvel = ini_maxvel;

    return 0;
}

// I think this is the [TRAJ] max velocity.  This should
// be the max velocity of the TOOL TIP, not necessarily
// any particular axis.  This applies to subsequent moves
// until changed.

int tpSetVlimit(TP_STRUCT * tp, double vLimit)
{
    if (0 == tp || vLimit <= 0.0) {
	return -1;
    }

    tp->vLimit = vLimit;

    return 0;
}

// feed override, 1.0 = 100%

int tpSetVscale(TP_STRUCT * tp, double scale)
{
    TC_STRUCT *thisTc;
    int t;
    int depth;

    if (0 == tp) {
	return -1;
    }

    if (scale < 0.0) {
	/* clamp it */
	scale = 0.0;
    }

    /* record the scale factor */
    if (tp->pausing) {
	/* if we're pausing, our scale is 0 and needs to stay there so that
	   it's applied to any added motions. We'll put the requested scale
	   in the restore value so that when we resume the new scale is
	   applied. Also, don't send this down to the queued motions--
	   they're already paused */
	tp->vRestore = scale;
    } else {
	/* we're not pausing, so it goes right in and is applied to the
	   global value for subsequent moves, and also all queued moves */
	tp->vScale = scale;

	depth = tcqLen(&tp->queue);
	for (t = 0; t < depth; t++) {
	    thisTc = tcqItem(&tp->queue, t);
            thisTc->feed_override = scale;
	}
    }

    return 0;
}

// Set max accel

int tpSetAmax(TP_STRUCT * tp, double aMax)
{
    if (0 == tp || aMax <= 0.0) {
	return -1;
    }

    tp->aMax = aMax;

    return 0;
}

/*
  tpSetId() sets the id that will be used for the next appended motions.
  nextId is incremented so that the next time a motion is appended its id
  will be one more than the previous one, modulo a signed int. If
  you want your own ids for each motion, call this before each motion
  you append and stick what you want in here.
  */
int tpSetId(TP_STRUCT * tp, int id)
{
    if (0 == tp) {
	return -1;
    }

    tp->nextId = id;

    return 0;
}

/*
  tpGetExecId() returns the id of the last motion that is currently
  executing.
  */
int tpGetExecId(TP_STRUCT * tp)
{
    if (0 == tp) {
	return -1;
    }

    return tp->execId;
}

/*
  tpSetTermCond(tp, cond) sets the termination condition for all subsequent
  queued moves. If cond is TC_TERM_STOP, motion comes to a stop before
  a subsequent move begins. If cond is TC_TERM_BLEND, the following move
  is begun when the current move decelerates.
  */
int tpSetTermCond(TP_STRUCT * tp, int cond, double tolerance)
{
    if (0 == tp) {
	return -1;
    }

    if (cond != TC_TERM_COND_STOP && cond != TC_TERM_COND_BLEND) {
	return -1;
    }

    tp->termCond = cond;
    tp->tolerance = tolerance;

    return 0;
}

// Used to tell the tp the initial position.  It sets
// the current position AND the goal position to be the same.  
// Used only at TP initialization and when switching modes.

int tpSetPos(TP_STRUCT * tp, EmcPose pos)
{
    if (0 == tp) {
	return -1;
    }

    tp->currentPos = pos;
    tp->goalPos = pos;

    return 0;
}

// Add a straight line to the tc queue.  This is a coordinated
// move in any or all of the six axes.  it goes from the end
// of the previous move to the new end specified here at the
// currently-active accel and vel settings from the tp struct.

int tpAddLine(TP_STRUCT * tp, EmcPose end, int type)
{
    TC_STRUCT tc;
    PmLine line_xyz, line_abc;
    PmPose start_xyz, end_xyz, start_abc, end_abc;
    PmQuaternion identity_quat = { 1.0, 0.0, 0.0, 0.0 };

    if (!tp) {
        rtapi_print_msg(RTAPI_MSG_ERR, "TP is null\n");
        return -1;
    }
    if (tp->aborting) {
        rtapi_print_msg(RTAPI_MSG_ERR, "TP is aborting\n");
	return -1;
    }

    start_xyz.tran = tp->goalPos.tran;
    end_xyz.tran = end.tran;

    start_abc.tran.x = tp->goalPos.a;
    start_abc.tran.y = tp->goalPos.b;
    start_abc.tran.z = tp->goalPos.c;
    end_abc.tran.x = end.a;
    end_abc.tran.y = end.b;
    end_abc.tran.z = end.c;

    start_xyz.rot = identity_quat;
    end_xyz.rot = identity_quat;
    start_abc.rot = identity_quat;
    end_abc.rot = identity_quat;

    pmLineInit(&line_xyz, start_xyz, end_xyz);
    pmLineInit(&line_abc, start_abc, end_abc);

    tc.cycle_time = tp->cycleTime;
    tc.target = line_xyz.tmag < 1e-6? line_abc.tmag: line_xyz.tmag;
    tc.progress = 0.0;
    tc.reqvel = tp->vMax;
    tc.maxaccel = tp->aMax * ACCEL_USAGE;
    tc.feed_override = tp->vScale;
    tc.maxvel = tp->ini_maxvel * ACCEL_USAGE;
    tc.id = tp->nextId;
    tc.active = 0;

    tc.coords.line.xyz = line_xyz;
    tc.coords.line.abc = line_abc;
    tc.motion_type = TC_LINEAR;
    tc.canon_motion_type = type;
    tc.blend_with_next = tp->termCond == TC_TERM_COND_BLEND;
    tc.tolerance = tp->tolerance;

    tc.synchronized = tp->synchronized;
    tc.uu_per_rev = tp->uu_per_rev;

    if (tcqPut(&tp->queue, tc) == -1) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tcqPut failed.\n");
	return -1;
    }

    tp->goalPos = end;      // remember the end of this move, as it's
                            // the start of the next one.
    tp->done = 0;
    tp->depth = tcqLen(&tp->queue);
    tp->nextId++;

    return 0;
}

// likewise, this adds a circular (circle, arc, helix) move from
// the end of the last move to this new position.  end is the
// xyzabc of the destination, center/normal/turn specify the arc
// in a way that makes sense to pmCircleInit (we don't care about
// the details here.)  Note that degenerate arcs/circles are not
// allowed; we are guaranteed to have a move in xyz so target is
// always the circle/arc/helical length.

int tpAddCircle(TP_STRUCT * tp, EmcPose end,
		PmCartesian center, PmCartesian normal, int turn, int type)
{
    TC_STRUCT tc;
    PmCircle circle;
    PmLine line_abc;
    PmPose start_xyz, end_xyz, start_abc, end_abc;
    double helix_z_component;   // z of the helix's cylindrical coord system
    double helix_length;
    PmQuaternion identity_quat = { 1.0, 0.0, 0.0, 0.0 };

    if (!tp || tp->aborting) 
	return -1;

    start_xyz.tran = tp->goalPos.tran;
    end_xyz.tran = end.tran;

    start_abc.tran.x = tp->goalPos.a;
    start_abc.tran.y = tp->goalPos.b;
    start_abc.tran.z = tp->goalPos.c;
    end_abc.tran.x = end.a;
    end_abc.tran.y = end.b;
    end_abc.tran.z = end.c;

    start_xyz.rot = identity_quat;
    end_xyz.rot = identity_quat;
    start_abc.rot = identity_quat;
    end_abc.rot = identity_quat;

    pmCircleInit(&circle, start_xyz, end_xyz, center, normal, turn);
    pmLineInit(&line_abc, start_abc, end_abc);

    // find helix length
    pmCartMag(circle.rHelix, &helix_z_component);
    helix_length = pmSqrt(pmSq(circle.angle * circle.radius) +
                          pmSq(helix_z_component));

    tc.cycle_time = tp->cycleTime;
    tc.target = helix_length;
    tc.progress = 0.0;
    tc.reqvel = tp->vMax;
    tc.maxaccel = tp->aMax * ACCEL_USAGE;
    tc.feed_override = tp->vScale;
    tc.maxvel = tp->ini_maxvel * ACCEL_USAGE;
    tc.id = tp->nextId;
    tc.active = 0;

    tc.coords.circle.xyz = circle;
    tc.coords.circle.abc = line_abc;
    tc.motion_type = TC_CIRCULAR;
    tc.canon_motion_type = type;
    tc.blend_with_next = tp->termCond == TC_TERM_COND_BLEND;
    tc.tolerance = tp->tolerance;

    tc.synchronized = tp->synchronized;
    tc.uu_per_rev = tp->uu_per_rev;

    if (tcqPut(&tp->queue, tc) == -1) {
	return -1;
    }

    tp->goalPos = end;
    tp->done = 0;
    tp->depth = tcqLen(&tp->queue);
    tp->nextId++;

    return 0;
}

void tcRunCycle(TC_STRUCT *tc, double *v, int *on_final_decel) {
    double discr, maxnewvel, newvel, newaccel=0;
    discr = 0.5 * tc->cycle_time * tc->currentvel - (tc->target - tc->progress);
    if(discr > 0.0) {
        // should never happen: means we've overshot the target
        newvel = maxnewvel = 0.0;
    } else {
        discr = 0.25 * pmSq(tc->cycle_time) - 2.0 / tc->maxaccel * discr;
        newvel = maxnewvel = -0.5 * tc->maxaccel * tc->cycle_time + 
            tc->maxaccel * pmSqrt(discr);
    }

    if(newvel <= 0.0) {
        // also should never happen - if we already finished this tc, it was
        // caught above
        newvel = newaccel = 0.0;
        tc->progress = tc->target;
    } else {
        // constrain velocity
        if(newvel > tc->reqvel * tc->feed_override) 
            newvel = tc->reqvel * tc->feed_override;
        if(newvel > tc->maxvel) newvel = tc->maxvel;

        if(tc->motion_type == TC_CIRCULAR &&
                newvel > pmSqrt(tc->maxaccel * tc->coords.circle.xyz.radius))
            newvel = pmSqrt(tc->maxaccel * tc->coords.circle.xyz.radius);

        // get resulting acceleration
        newaccel = (newvel - tc->currentvel) / tc->cycle_time;
        
        // constrain acceleration and get resulting velocity
        if(newaccel > 0.0 && newaccel > tc->maxaccel) {
            newaccel = tc->maxaccel;
            newvel = tc->currentvel + newaccel * tc->cycle_time;
        }
        if(newaccel < 0.0 && newaccel < -tc->maxaccel) {
            newaccel = -tc->maxaccel;
            newvel = tc->currentvel + newaccel * tc->cycle_time;
        }
        // update position in this tc
        tc->progress += (newvel + tc->currentvel) * 0.5 * tc->cycle_time;
    }
    tc->currentvel = newvel;
    if(v) *v = newvel;
    if(on_final_decel) *on_final_decel = fabs(maxnewvel - newvel) < 0.001;
}

// This is the brains of the operation.  It's called every TRAJ period
// and is expected to set tp->currentPos to the new machine position.
// Lots of other tp fields (depth, done, etc) have to be twiddled to
// communicate the status; I think those are spelled out here correctly
// and I can't clean it up without breaking the API that the TP presents
// to motion.  It's not THAT bad and in the interest of not touching
// stuff outside this directory, I'm going to leave it for now.

int tpRunCycle(TP_STRUCT * tp)
{
    // vel = (new position - old position) / cycle time
    // (two position points required)
    //
    // acc = (new vel - old vel) / cycle time
    // (three position points required)

    TC_STRUCT *tc, *nexttc;
    double primary_vel;
    int on_final_decel;
    EmcPose primary_before, primary_after;
    EmcPose secondary_before, secondary_after;
    EmcPose primary_displacement, secondary_displacement;
    static double oldrevs, spindleoffset;
    static int waiting = 0;

    tc = tcqItem(&tp->queue, 0);
    if(!tc) {
        // this means the motion queue is empty.  This can represent
        // the end of the program OR QUEUE STARVATION.  In either case,
        // I want to stop.  Some may not agree that's what it should do.
        tcqInit(&tp->queue);
        tp->goalPos = tp->currentPos;
        tp->done = 1;
        tp->depth = tp->activeDepth = 0;
        tp->aborting = 0;
        tp->execId = 0;
        tp->motionType = 0;
        emcmotStatus->spindleSync = 0;
        tpResume(tp);
        return 0;
    }

    if(tp->aborting) {
        // an abort message has come
        if(tc->currentvel == 0.0) {
            tcqInit(&tp->queue);
            tp->goalPos = tp->currentPos;
            tp->done = 1;
            tp->depth = tp->activeDepth = 0;
            tp->aborting = 0;
            tp->execId = 0;
            tp->motionType = 0;
            emcmotStatus->spindleSync = 0;
            tpResume(tp);
            return 0;
        } else {
            tc->reqvel = 0.0;
        }
    }

    if (tc->target == tc->progress) {
        // if we're synced, and this move is ending, save the
        // spindle position so the next synced move can be in
        // the right place.
        if(tc->synchronized)
            spindleoffset += tc->target/tc->uu_per_rev;
        else
            spindleoffset = 0.0;

        // done with this move
        tcqRemove(&tp->queue, 1);

        // so get next move
        tc = tcqItem(&tp->queue, 0);
        if(!tc) return 0;
    }

    // report our line number to the guis
    tp->execId = tc->id;

    if(waiting) {
        double r;
        if((r = emcmotStatus->spindleRevs) >= oldrevs) {
            /* haven't passed index yet */
            oldrevs = r;
            return 0;
        } else {
            /* passed index, start the move */
            emcmotStatus->spindleSync = 1;
            waiting=0;
        }
    }

    if(!tc->synchronized) emcmotStatus->spindleSync = 0;

    // now we have the active tc.  get the upcoming one, if there is one.
    // it's not an error if there isn't another one - we just don't
    // do blending.  This happens in MDI for instance.
    if(tc->blend_with_next) 
        nexttc = tcqItem(&tp->queue, 1);
    else
        nexttc = NULL;

    if(!tc->synchronized && nexttc && nexttc->synchronized) {
        // we'll have to wait for spindle sync; might as well
        // stop at the right place (don't blend)
        tc->blend_with_next = 0;
        nexttc = NULL;
    }

    if(tc->active == 0) {
        // this means this tc is being read for the first time.
        
        tc->currentvel = 0;
        tp->depth = tp->activeDepth = 1;
        tp->motionType = tc->canon_motion_type;
        tc->active = 1;
        tc->blending = 0;

        if(tc->synchronized) {
            if(!emcmotStatus->spindleSync) {
                // if we aren't already synced, wait
                waiting = 1;
                oldrevs = emcmotStatus->spindleRevs;
                spindleoffset = 0.0;
                // don't move: wait
                return 0;
            }
        }
        // clamp motion's velocity at TRAJ MAX_VELOCITY (tooltip maxvel)
        if(tc->maxvel > tp->vLimit) 
            tc->maxvel = tp->vLimit;

        // honor accel constraint if we happen to make an acute angle
        // with the next segment.  A dot product test could often 
        // eliminate this.
        if(tc->blend_with_next) 
            tc->maxaccel /= 2.0;
    }

    if(nexttc && nexttc->active == 0) {
        // this means this tc is being read for the first time.

        nexttc->currentvel = 0;
        tp->depth = tp->activeDepth = 1;
        nexttc->active = 1;
        nexttc->blending = 0;

        // clamp motion's velocity at TRAJ MAX_VELOCITY (tooltip maxvel)
        if(nexttc->maxvel > tp->vLimit) 
            nexttc->maxvel = tp->vLimit;

        // honor accel constraint if we happen to make an acute angle
        // with the above segment or the following one
        if(tc->blend_with_next || nexttc->blend_with_next) 
            nexttc->maxaccel /= 2.0;
    }

    if(tc->synchronized) {
        double pos_error;
        double revs = emcmotStatus->spindleRevs;

        pos_error = (revs - spindleoffset) * tc->uu_per_rev - (tc->progress + nexttc->progress);

        tc->reqvel = pos_error/tc->cycle_time/2.0;
        tc->feed_override = 1.0;
        if(tc->reqvel < 0.0) tc->reqvel = 0.0;
        if(nexttc) {
            nexttc->reqvel = pos_error/nexttc->cycle_time;
            nexttc->feed_override = 1.0;
            if(nexttc->reqvel < 0.0) nexttc->reqvel = 0.0;
        }
    }

    // calculate the approximate peak velocity the nexttc will hit.
    // we know to start blending it in when the current tc goes below
    // this velocity...
    if(nexttc && nexttc->maxaccel) {
        tc->blend_vel = nexttc->maxaccel * 
            pmSqrt(nexttc->target / nexttc->maxaccel);
        if(tc->blend_vel > nexttc->reqvel * nexttc->feed_override) {
            // segment has a cruise phase so let's blend over the 
            // whole accel period if possible
            tc->blend_vel = nexttc->reqvel * nexttc->feed_override;
        } else {
            // segment has a triangular vel profile - blend for somewhat 
            // less than half of it so we're sure to at least touch 
            // the segment's path somewhere
            tc->blend_vel *= 0.8;
        }
        if(tc->maxaccel < nexttc->maxaccel)
            tc->blend_vel *= tc->maxaccel/nexttc->maxaccel;

        if(tc->tolerance) {
            /* see diagram blend.fig.  T (blend tolerance) is given, theta
             * is calculated from dot(s1,s2)
             *
             * blend criteria: we are decelerating at the end of segment s1
             * and we pass distance d from the end.  
             * find the corresponding velocity v when passing d.
             *
             * in the drawing note d = 2T/cos(theta)
             *
             * when v1 is decelerating at a to stop, v = at, t = v/a
             * so required d = .5 a (v/a)^2
             *
             * equate the two expressions for d and solve for v
             */
            double tblend_vel;
            double dot;
            double theta;
            PmCartesian v1, v2;

            v1 = tcGetEndingUnitVector(tc);
            v2 = tcGetStartingUnitVector(nexttc);
            pmCartCartDot(v1, v2, &dot);

            theta = acos(-dot)/2.0; 
            if(cos(theta) > 0.001) {
                tblend_vel = 2.0 * pmSqrt(tc->maxaccel * tc->tolerance / cos(theta));
                if(tblend_vel < tc->blend_vel)
                    tc->blend_vel = tblend_vel;
            }
        }
    }

    primary_before = tcGetPos(tc);
    tcRunCycle(tc, &primary_vel, &on_final_decel);
    primary_after = tcGetPos(tc);
    pmCartCartSub(primary_after.tran, primary_before.tran, 
            &primary_displacement.tran);
    primary_displacement.a = primary_after.a - primary_before.a;
    primary_displacement.b = primary_after.b - primary_before.b;
    primary_displacement.c = primary_after.c - primary_before.c;

    // blend criteria
    if(tc->blending || 
            (nexttc && on_final_decel && primary_vel < tc->blend_vel)) {
        // make sure we continue to blend this segment even when its 
        // accel reaches 0 (at the very end)
        tc->blending = 1;

        // hack to show blends in axis
        // tp->motionType = 0;

        if(tc->currentvel > nexttc->currentvel)
            tp->motionType = tc->canon_motion_type;
        else
            tp->motionType = nexttc->canon_motion_type;

        secondary_before = tcGetPos(nexttc);
        tcRunCycle(nexttc, NULL, NULL);
        secondary_after = tcGetPos(nexttc);
        pmCartCartSub(secondary_after.tran, secondary_before.tran, 
                &secondary_displacement.tran);
        secondary_displacement.a = secondary_after.a - secondary_before.a;
        secondary_displacement.b = secondary_after.b - secondary_before.b;
        secondary_displacement.c = secondary_after.c - secondary_before.c;

        pmCartCartAdd(tp->currentPos.tran, primary_displacement.tran, 
                &tp->currentPos.tran);
        pmCartCartAdd(tp->currentPos.tran, secondary_displacement.tran, 
                &tp->currentPos.tran);
        tp->currentPos.a += primary_displacement.a + secondary_displacement.a;
        tp->currentPos.b += primary_displacement.b + secondary_displacement.b;
        tp->currentPos.c += primary_displacement.c + secondary_displacement.c;
    } else {
        tp->motionType = tc->canon_motion_type;
        tp->currentPos = primary_after;
    }

    return 0;
}

int tpSetSpindleSync(TP_STRUCT * tp, double sync) {
    if(sync) {
        tp->synchronized = 1;
        tp->uu_per_rev = sync;
    } else
        tp->synchronized = 0;

    return 0;
}

int tpPause(TP_STRUCT * tp)
{
    if (0 == tp) {
	return -1;
    }

    if (!tp->pausing) {
	/* save the restore value */
	tp->vRestore = tp->vScale;

	/* apply 0 scale to queued motions */
	tpSetVscale(tp, 0);

	/* mark us pausing-- do this after the call to toSetVscale since this
	   looks at the pausing flag to decide whether to set vRestore (if
	   pausing) or vScale (if not). We want vScale to be set in the call
	   for this one */
	tp->pausing = 1;
    }

    return 0;
}

int tpResume(TP_STRUCT * tp)
{
    if (0 == tp) {
	return -1;
    }

    if (tp->pausing) {
	/* mark us not pausing-- this must be done before the call to
	   tpSetVscale since that function will only apply the restored scale
	   value if we're not pausing */
	tp->pausing = 0;

	/* restore scale value */
	tp->vScale = tp->vRestore;

	/* apply the restored scale value to queued motions */
	tpSetVscale(tp, tp->vScale);
    }

    return 0;
}

int tpAbort(TP_STRUCT * tp)
{
    if (0 == tp) {
	return -1;
    }

    if (!tp->aborting) {
	/* to abort, signal a pause and set our abort flag */
	tpPause(tp);
	tp->aborting = 1;
    }
    return 0;
}

int tpGetMotionType(TP_STRUCT * tp)
{
    return tp->motionType;
}

EmcPose tpGetPos(TP_STRUCT * tp)
{
    EmcPose retval;

    if (0 == tp) {
	retval.tran.x = retval.tran.y = retval.tran.z = 0.0;
	retval.a = retval.b = retval.c = 0.0;
	return retval;
    }

    return tp->currentPos;
}

int tpIsDone(TP_STRUCT * tp)
{
    if (0 == tp) {
	return 0;
    }

    return tp->done;
}

/*
  tpIsPaused() returns 1 only when all active queued motions are paused.
  This is necessary so that abort clears the queue when motions have really
  stopped. If there are no queued motions, it returns 1 if the pausing
  flag is set. */
int tpIsPaused(TP_STRUCT * tp)
{
    int t;

    if (0 == tp) {
	return 0;
    }

    if (0 == tp->depth) {
	return tp->pausing;
    }

    for (t = 0; t < tp->activeDepth; t++) {
        TC_STRUCT *tc;
        tc = tcqItem(&tp->queue, t);
        if (tc->feed_override != 0) {
	    return 0;
	}
    }

    return 1;
}

int tpQueueDepth(TP_STRUCT * tp)
{
    if (0 == tp) {
	return 0;
    }

    return tp->depth;
}

int tpActiveDepth(TP_STRUCT * tp)
{
    if (0 == tp) {
	return 0;
    }

    return tp->activeDepth;
}

int tpSetAout(TP_STRUCT *tp, unsigned char index, double start, double end) {
    return 0;
}

int tpSetDout(TP_STRUCT *tp, int index, unsigned char start, unsigned char end) {
    return 0;
}

