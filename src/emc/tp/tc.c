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

#include "rtapi.h"		/* rtapi_print_msg */
#include "rtapi_math.h"
#include "posemath.h"
#include "blendmath.h"
#include "emcpose.h"
#include "tc.h"
#include "tp_types.h"
#include "spherical_arc.h"
#include "motion_types.h"
#include "motion.h"
#include "ruckig_wrapper.h"

//Debug output
#include "tp_debug.h"

// For jerk-limited arc velocity (planner_type 1)
extern emcmot_status_t *emcmotStatus;

#ifndef GET_TRAJ_PLANNER_TYPE
#define GET_TRAJ_PLANNER_TYPE() (emcmotStatus->planner_type)
#endif


double tcGetMaxTargetVel(TC_STRUCT const * const tc,
        double max_scale)
{
    double v_max_target;

    switch (tc->synchronized) {
        case TC_SYNC_NONE:
            // Get maximum reachable velocity from max feed override
            v_max_target = tc->reqvel * max_scale;
            break;

        case TC_SYNC_VELOCITY: //Fallthrough
            max_scale = 1.0;
            /* Fallthrough */
        case TC_SYNC_POSITION:
            // Assume no spindle override during blend target
        default:
            v_max_target = tc->maxvel;
            break;
    }

    // Clip maximum velocity by the segment's own maximum velocity
    return fmin(v_max_target, tc->maxvel);
}

double tcGetOverallMaxAccel(const TC_STRUCT *tc)
{
    // Handle any acceleration reduction due to an approximate-tangent "blend" with the previous or next segment
    double a_scale = (1.0 - fmax(tc->kink_accel_reduce, tc->kink_accel_reduce_prev));

    // Parabolic blending conditions: If the next segment or previous segment
    // has a parabolic blend with this one, acceleration is scaled down by 1/2
    // so that the sum of the two does not exceed the maximum.
    if (tc->blend_prev || TC_TERM_COND_PARABOLIC == tc->term_cond) {
        a_scale *= 0.5;
    }

    return tc->maxaccel * a_scale;
}

/**
 * Get acceleration for a tc based on the trajectory planner state.
 */
double tcGetTangentialMaxAccel(TC_STRUCT const * const tc)
{
    double a_scale = tcGetOverallMaxAccel(tc);

    // Reduce allowed tangential acceleration in circular motions to stay
    // within overall limits (accounts for centripetal acceleration while
    // moving along the circular path).
    if (tc->motion_type == TC_CIRCULAR || tc->motion_type == TC_SPHERICAL) {
        //Limit acceleration for circular arcs to allow for normal acceleration
        a_scale *= tc->acc_ratio_tan;
    }
    return a_scale;
}


int tcSetKinkProperties(TC_STRUCT *prev_tc, TC_STRUCT *tc, double kink_vel, double accel_reduction)
{
  prev_tc->kink_vel = kink_vel;
  //
  prev_tc->kink_accel_reduce = fmax(accel_reduction, prev_tc->kink_accel_reduce);
  tc->kink_accel_reduce_prev = fmax(accel_reduction, tc->kink_accel_reduce_prev);

  return 0;
}

int tcInitKinkProperties(TC_STRUCT *tc)
{
    tc->kink_vel = -1.0;
    tc->kink_accel_reduce = 0.0;
    tc->kink_accel_reduce_prev = 0.0;
    return 0;
}

int tcRemoveKinkProperties(TC_STRUCT *prev_tc, TC_STRUCT *tc)
{
    prev_tc->kink_vel = -1.0;
    prev_tc->kink_accel_reduce = 0.0;
    tc->kink_accel_reduce_prev = 0.0;
    return 0;
}


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

    pmCartScalMult(&tan, tcGetOverallMaxAccel(tc), &tan);
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

int tcGetIntersectionPoint(TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc, PmCartesian * const point)
{
    // TODO NULL pointer check?
    // Get intersection point from geometry
    if (tc->motion_type == TC_LINEAR) {
        *point = tc->coords.line.xyz.start;
    } else if (prev_tc->motion_type == TC_LINEAR) {
        *point = prev_tc->coords.line.xyz.end;
    } else if (tc->motion_type == TC_CIRCULAR){
        pmCirclePoint(&tc->coords.circle.xyz, 0.0, point);
    } else {
        return TP_ERR_FAIL;
    }
    return TP_ERR_OK;
}


/**
 * Check if a segment can be consumed without disrupting motion or synced IO.
 */
int tcCanConsume(TC_STRUCT const * const tc)
{
    if (!tc) {
        return false;
    }

    if (tc->syncdio.anychanged || tc->blend_prev || tc->atspeed) {
        //TODO add other conditions here (for any segment that should not be consumed by blending
        return false;
    }

    return true;

}

/**
 * Find the geometric tangent vector to a helical arc.
 * Unlike the acceleration vector, the result of this calculation is a vector
 * tangent to the helical arc. This is called by wrapper functions for the case of a circular or helical arc.
 */
int pmCircleTangentVector(PmCircle const * const circle,
        double angle_in, PmCartesian * const out)
{

    PmCartesian startpoint;
    PmCartesian radius;
    PmCartesian uTan, dHelix, dRadial;

    // Get vector in radial direction
    pmCirclePoint(circle, angle_in, &startpoint);
    pmCartCartSub(&startpoint, &circle->center, &radius);

    /* Find local tangent vector using planar normal. Assuming a differential
     * angle dtheta, the tangential component of the tangent vector is r *
     * dtheta. Since we're normalizing the vector anyway, assume dtheta = 1.
     */
    pmCartCartCross(&circle->normal, &radius, &uTan);

    /* the binormal component of the tangent vector is (dz / dtheta) * dtheta.
     */
    double dz = 1.0 / circle->angle;
    pmCartScalMult(&circle->rHelix, dz, &dHelix);

    pmCartCartAddEq(&uTan, &dHelix);

    /* The normal component is (dr / dtheta) * dtheta.
     */
    double dr = circle->spiral / circle->angle;
    pmCartUnit(&radius, &dRadial);
    pmCartScalMultEq(&dRadial, dr);
    pmCartCartAddEq(&uTan, &dRadial);

    //Normalize final output vector
    pmCartUnit(&uTan, out);
    return 0;
}


/**
 * Calculate the unit tangent vector at the start of a move for any segment.
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
            pmCircleTangentVector(&tc->coords.circle.xyz, 0.0, out);
            break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid motion type %d!\n",tc->motion_type);
            return -1;
    }
    return 0;
}

/**
 * Calculate the unit tangent vector at the end of a move for any segment.
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
            pmCircleTangentVector(&tc->coords.circle.xyz,
                    tc->coords.circle.xyz.angle, out);
            break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid motion type %d!\n",tc->motion_type);
            return -1;
    }
    return 0;
}

/**
 * Calculate the unit tangent vector at the current progress position of a move.
 * This gives the accurate direction at tc->progress, important for accurate jerk output.
 */
int tcGetCurrentTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out) {
    switch (tc->motion_type) {
        case TC_LINEAR:
            *out = tc->coords.line.xyz.uVec;
            break;
        case TC_RIGIDTAP:
            if (tc->progress > 0.5 * tc->target) {
                // Returning from tap, direction is reversed
                pmCartScalMult(&tc->coords.rigidtap.xyz.uVec, -1.0, out);
            } else {
                *out = tc->coords.rigidtap.xyz.uVec;
            }
            break;
        case TC_CIRCULAR:
            {
                // Calculate current angle based on progress
                PmCircle const * const circle = &tc->coords.circle.xyz;
                double angle = tc->progress / circle->radius;
                // Handle spiral (radius varies)
                if (circle->spiral != 0.0) {
                    // For spiral, use approximation
                    double r_ratio = circle->spiral * angle / (2.0 * PM_PI);
                    double avg_radius = circle->radius * (1.0 + r_ratio / 2.0);
                    angle = tc->progress / avg_radius;
                }
                pmCircleTangentVector(circle, angle, out);
            }
            break;
        case TC_SPHERICAL:
            {
                // For spherical blend arcs, calculate current tangent
                SphericalArc const * const arc = &tc->coords.arc.xyz;

                // Calculate progress fraction (0 to 1)
                double total_length = arc->radius * arc->angle + arc->line_length;
                double progress_frac = (total_length > DOUBLE_FUZZ) ?
                                       tc->progress / total_length : 0.0;

                // Use endpoint tangent as approximation based on progress
                // (arcTangent only supports at_end=0 or 1)
                int at_end = (progress_frac > 0.5) ? 1 : 0;
                arcTangent(arc, out, at_end);
            }
            break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid motion type %d in tcGetCurrentTangentUnitVector!\n", tc->motion_type);
            return -1;
    }
    return 0;
}

/**
 * Calculate the distance left in the trajectory segment in the indicated
 * direction.
 */
double tcGetDistanceToGo(TC_STRUCT const * const tc, int direction)
{
    double distance = tcGetTarget(tc, direction) - tc->progress;
    if (direction == TC_DIR_REVERSE) {
        distance *=-1.0;
    }
    return distance;
}

double tcGetTarget(TC_STRUCT const * const tc, int direction)
{
    return (direction == TC_DIR_REVERSE) ? 0.0 : tc->target;
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


    // Used for arc-length to angle conversion with spiral segments
    double angle = 0.0;
    int res_fit = TP_ERR_OK;

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
            res_fit = pmCircleAngleFromProgress(&tc->coords.circle.xyz,
                    &tc->coords.circle.fit,
                    progress, &angle);
            pmCirclePoint(&tc->coords.circle.xyz,
                    angle,
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
        case TC_DWELL:
            // Dwell holds position - return start point
            xyz = tc->coords.line.xyz.start;
            abc = tc->coords.line.abc.start;
            uvw = tc->coords.line.uvw.start;
            break;
    }

    if (res_fit == TP_ERR_OK) {
        // Don't touch pos unless we know the value is good
        pmCartesianToEmcPose(&xyz, &abc, &uvw, pos);
    }
    return res_fit;
}


/**
 * Set the terminal condition of a segment.
 * This function will eventually handle state changes associated with altering a terminal condition.
 */
int tcSetTermCond(TC_STRUCT *prev_tc, TC_STRUCT *tc, int term_cond) {
    switch (term_cond) {
    case TC_TERM_COND_STOP:
    case TC_TERM_COND_EXACT:
    case TC_TERM_COND_TANGENT:
        if (tc) {tc->blend_prev = 0;}
        break;
    case TC_TERM_COND_PARABOLIC:
        if (tc) {tc->blend_prev = 1;}
        break;
    default:
        break;

    }
    if (prev_tc) {
        tp_debug_print("setting term condition %d on tc id %d, type %d\n", term_cond, prev_tc->id, prev_tc->motion_type);
        prev_tc->term_cond = term_cond;
    }
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
        tcSetTermCond(prev_tc, tc, TC_TERM_COND_TANGENT);
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

    tcSetTermCond(prev_tc, tc, TC_TERM_COND_TANGENT);

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


/**
 * Check for early stop conditions.
 * If a variety of conditions are true, then we can't do blending as we expect.
 * This function checks for any conditions that force us to stop on the current
 * segment. This is different from pausing or aborting, which can happen any
 * time.
 */
int tcFlagEarlyStop(TC_STRUCT * const tc,
        TC_STRUCT * const nexttc)
{

    if (!tc || !nexttc) {
        return TP_ERR_NO_ACTION;
    }

    if(tc->synchronized != TC_SYNC_POSITION && nexttc->synchronized == TC_SYNC_POSITION) {
        // we'll have to wait for spindle sync; might as well
        // stop at the right place (don't blend)
        tp_debug_print("waiting on spindle sync for tc %d\n", tc->id);
        tcSetTermCond(tc, nexttc, TC_TERM_COND_STOP);
    }

    if(nexttc->atspeed) {
        // we'll have to wait for the spindle to be at-speed; might as well
        // stop at the right place (don't blend), like above
        // FIXME change the values so that 0 is exact stop mode
        tp_debug_print("waiting on spindle atspeed for tc %d\n", tc->id);
        tcSetTermCond(tc, nexttc, TC_TERM_COND_STOP);
    }

    return TP_ERR_OK;
}

double pmLine9Target(PmLine9 * const line9)
{
    if (!line9->xyz.tmag_zero) {
        return line9->xyz.tmag;
    } else if (!line9->uvw.tmag_zero) {
        return line9->uvw.tmag;
    } else if (!line9->abc.tmag_zero) {
        return line9->abc.tmag;
    } else {
        return 0.0;
    }
}


/**
 * Initialize a new trajectory segment with common parameters.
 *
 * NOTE: this function only sets default values that are non-zero. Make sure
 * the struct is properly initialized BEFORE calling this function.
 */
int tcInit(TC_STRUCT * const tc,
        int motion_type,
        int canon_motion_type,
        double cycle_time,
        unsigned char enables,
        char atspeed)
{

    /** Motion type setup */
    tc->motion_type = motion_type;
    tc->canon_motion_type = canon_motion_type;
    tc->atspeed = atspeed;

    /** Segment settings passed down from interpreter*/
    tc->enables = enables;
    tc->cycle_time = cycle_time;

    tc->id = -1; //ID to be set when added to queue (may change before due to blend arcs)

    /** Segment settings (given values later during setup / optimization) */
    tc->indexer_jnum = -1;

    tc->active_depth = 1;

    tc->acc_ratio_tan = BLEND_ACC_RATIO_TANGENTIAL;

    // Initialize Ruckig planner fields
    tc->ruckig_planner = NULL;
    tc->ruckig_trajectory_time = 0.0;
    tc->ruckig_planned = 0;
    tc->ruckig_last_maxaccel = 0.0;
    tc->ruckig_last_maxjerk = 0.0;
    tc->ruckig_last_target_vel = 0.0;
    tc->ruckig_last_final_vel = 0.0;
    tc->ruckig_last_target_pos = 0.0;
    tc->ruckig_last_use_velocity_control = 0;
    tc->ruckig_last_req_pos = 0.0;
    tc->ruckig_last_feed_override = 0.0;

    return TP_ERR_OK;
}


/**
 * Set kinematic properties for a trajectory segment.
 */
int tcSetupMotion(TC_STRUCT * const tc,
        double vel,
        double ini_maxvel,
        double acc,
        double ini_maxjerk)
{
    //FIXME assumes that state is already set up in TC_STRUCT, which depends on external order of function calls.

    tc->maxaccel = acc;
    tc->maxjerk = ini_maxjerk;
    tc->blend_maxjerk = ini_maxjerk;  // default equals maxjerk, look-ahead adjusts as needed

    tc->maxvel = ini_maxvel;

    tc->reqvel = vel;
    // To be computed by velocity optimization / spindle-sync calculations
    tc->target_vel = 0;
    // To be filled in by tangent calculation, negative = invalid (KLUDGE)
    tcInitKinkProperties(tc);

    return TP_ERR_OK;
}


int tcSetupState(TC_STRUCT * const tc, TP_STRUCT const * const tp)
{
    tcSetTermCond(tc, NULL, tp->termCond);
    tc->tolerance = tp->tolerance;
    tc->synchronized = tp->synchronized;
    tc->uu_per_rev = tp->uu_per_rev;
    return TP_ERR_OK;
}

int pmLine9Init(PmLine9 * const line9,
        EmcPose const * const start,
        EmcPose const * const end)
{
    // Scratch variables
    PmCartesian start_xyz, end_xyz;
    PmCartesian start_uvw, end_uvw;
    PmCartesian start_abc, end_abc;

    // Convert endpoint to cartesian representation
    emcPoseToPmCartesian(start, &start_xyz, &start_abc, &start_uvw);
    emcPoseToPmCartesian(end, &end_xyz, &end_abc, &end_uvw);

    // Initialize cartesian line members
    int xyz_fail = pmCartLineInit(&line9->xyz, &start_xyz, &end_xyz);
    int abc_fail = pmCartLineInit(&line9->abc, &start_abc, &end_abc);
    int uvw_fail = pmCartLineInit(&line9->uvw, &start_uvw, &end_uvw);

    if (xyz_fail || abc_fail || uvw_fail) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Failed to initialize Line9, err codes %d, %d, %d\n",
                xyz_fail,abc_fail,uvw_fail);
        return TP_ERR_FAIL;
    }
    return TP_ERR_OK;
}

int pmCircle9Init(PmCircle9 * const circ9,
        EmcPose const * const start,
        EmcPose const * const end,
        PmCartesian const * const center,
        PmCartesian const * const normal,
        int turn)
{
    PmCartesian start_xyz, end_xyz;
    PmCartesian start_uvw, end_uvw;
    PmCartesian start_abc, end_abc;

    emcPoseToPmCartesian(start, &start_xyz, &start_abc, &start_uvw);
    emcPoseToPmCartesian(end, &end_xyz, &end_abc, &end_uvw);

    int xyz_fail = pmCircleInit(&circ9->xyz, &start_xyz, &end_xyz, center, normal, turn);
    //Initialize line parts of Circle9
    int abc_fail = pmCartLineInit(&circ9->abc, &start_abc, &end_abc);
    int uvw_fail = pmCartLineInit(&circ9->uvw, &start_uvw, &end_uvw);

    int res_fit = findSpiralArcLengthFit(&circ9->xyz,&circ9->fit);

    if (xyz_fail || abc_fail || uvw_fail || res_fit) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Failed to initialize Circle9, err codes %d, %d, %d, %d\n",
                xyz_fail, abc_fail, uvw_fail, res_fit);
        return TP_ERR_FAIL;
    }
    return TP_ERR_OK;
}

double pmCircle9Target(PmCircle9 const * const circ9)
{

    double h2;
    pmCartMagSq(&circ9->xyz.rHelix, &h2);
    double helical_length = pmSqrt(pmSq(circ9->fit.total_planar_length) + h2);

    return helical_length;
}

/**
 * Apply acceleration and jerk limits to circular/spherical arc segments.
 *
 * For any arc (TC_CIRCULAR or TC_SPHERICAL), this function:
 * 1. Limits velocity based on centripetal acceleration budget
 * 2. For planner_type 1 (S-curve), applies three jerk constraints:
 *    - Steady-state rotational jerk: v³/R²
 *    - Normal jerk from tangential acceleration coupling: 3·v·a_t/R
 *    - Entry/exit transition jerk at arc boundaries
 * 3. Calculates the tangential acceleration ratio for the arc
 *
 * This unified approach ensures consistent jerk limiting for both
 * programmed arcs (G2/G3) and blend arcs at segment corners.
 */
int tcUpdateArcLimits(TC_STRUCT * tc)
{
    double radius, angle;

    // Extract radius and angle based on motion type
    switch (tc->motion_type) {
        case TC_CIRCULAR:
            radius = pmCircleEffectiveMinRadius(&tc->coords.circle.xyz);
            angle = tc->coords.circle.xyz.angle;
            break;
        case TC_SPHERICAL:
            radius = tc->coords.arc.xyz.radius;
            angle = tc->coords.arc.xyz.angle;
            break;
        default:
            return 1; // Not an arc, nothing to do
    }

    if (radius < DOUBLE_FUZZ || angle < TP_ANGLE_EPSILON) {
        return 1; // Degenerate arc
    }

    double a_max = tcGetOverallMaxAccel(tc);
    double a_n_max_cutoff = BLEND_ACC_RATIO_NORMAL * a_max;

    // Find the acceleration necessary to reach the maximum velocity
    double a_n_vmax = pmSq(tc->maxvel) / radius;

    // Find the maximum velocity that still obeys our desired normal/total acceleration ratio
    double v_max_cutoff = pmSqrt(a_n_max_cutoff * radius);

    double v_max_actual = tc->maxvel;
    double acc_ratio_tan = BLEND_ACC_RATIO_TANGENTIAL;

    if (a_n_vmax > a_n_max_cutoff) {
        v_max_actual = v_max_cutoff;
    } else {
        acc_ratio_tan = pmSqrt(1.0 - pmSq(a_n_vmax / a_max));
    }

    // Jerk-based velocity limiting for S-curve planner (planner_type 1)
    if (GET_TRAJ_PLANNER_TYPE() == 1 && emcmotStatus->jerk > TP_POS_EPSILON &&
        tc->cycle_time > TP_TIME_EPSILON) {

        double jerk = emcmotStatus->jerk;
        double R_sq = pmSq(radius);

        // Constraint 1: Steady-state rotational jerk + entry/exit transitions
        // The jerk budget is shared between steady-state (v³/R²) and transitions.
        // Solving: v³ ≤ R² × j × φ / (2 + φ)
        // The (2 + φ) term: 2 for two transitions, φ for steady-state budget
        double v_max_jerk_steady = cbrt(R_sq * jerk * angle / (2.0 + angle));

        // Constraint 2: Normal jerk from tangential acceleration coupling
        // During S-curve ramps on arc: j_n = 3·v·a_t/R
        // Using BLEND_ACC_RATIO_TANGENTIAL as max tangential accel ratio
        double a_t_max = BLEND_ACC_RATIO_TANGENTIAL * a_max;
        double v_max_jerk_tan = jerk * radius / (3.0 * a_t_max);

        // Constraint 3: Entry/exit transition jerk (centripetal accel ramp)
        // At line-arc boundary, centripetal accel changes from 0 to v²/R
        // j_entry = (v²/R) / cycle_time ≤ j_max
        double v_max_jerk_entry = pmSqrt(jerk * radius * tc->cycle_time);

        double v_max_jerk = fmin(fmin(v_max_jerk_steady, v_max_jerk_tan), v_max_jerk_entry);

        tp_debug_print("tcUpdateArcLimits: type=%d R=%f phi=%f j=%f\n",
                       tc->motion_type, radius, angle, jerk);
        tp_debug_print("  v_jerk: steady=%f tan=%f entry=%f => min=%f\n",
                       v_max_jerk_steady, v_max_jerk_tan, v_max_jerk_entry, v_max_jerk);

        if (v_max_jerk < v_max_actual) {
            tp_debug_print("  Limiting v_max from %f to %f for jerk\n",
                           v_max_actual, v_max_jerk);
            v_max_actual = v_max_jerk;

            // Recalculate acc_ratio_tan for jerk-limited velocity
            double a_n_at_jerk_vel = pmSq(v_max_actual) / radius;
            if (a_n_at_jerk_vel < a_max) {
                acc_ratio_tan = pmSqrt(1.0 - pmSq(a_n_at_jerk_vel / a_max));
            }
        }
    }

    tc->maxvel = v_max_actual;
    tc->acc_ratio_tan = acc_ratio_tan;

    tp_debug_print("tcUpdateArcLimits: final v_max=%f acc_ratio_tan=%f\n",
                   tc->maxvel, tc->acc_ratio_tan);

    return 0;
}

/**
 * "Finalizes" a segment so that its length can't change.
 * By setting the finalized flag, we tell the optimizer that this segment's
 * length won't change anymore. Since any blends are already set up, we can
 * trust that the length will be the same, and so can use the length in the
 * velocity optimization.
 */
int tcFinalizeLength(TC_STRUCT * const tc)
{
    //Apply velocity corrections
    if (!tc) {
        return TP_ERR_FAIL;
    }

    if (tc->finalized) {
        tp_debug_print("tc %d already finalized\n", tc->id);
        return TP_ERR_NO_ACTION;
    }

    tp_debug_print("Finalizing motion id %d, type %d\n", tc->id, tc->motion_type);

    tcClampVelocityByLength(tc);

    tcUpdateArcLimits(tc);

    tc->finalized = 1;
    return TP_ERR_OK;
}


int tcClampVelocityByLength(TC_STRUCT * const tc)
{
    //Apply velocity corrections
    if (!tc) {
        return TP_ERR_FAIL;
    }

    //Reduce max velocity to match sample rate
    //Assume that cycle time is valid here
    double sample_maxvel = tc->target / tc->cycle_time;
    tp_debug_print("sample_maxvel = %f\n",sample_maxvel);
    tc->maxvel = fmin(tc->maxvel, sample_maxvel);
    return TP_ERR_OK;
}

/**
 * compute the total arc length of a circle segment
 */
int tcUpdateTargetFromCircle(TC_STRUCT * const tc)
{
    if (!tc || tc->motion_type !=TC_CIRCULAR) {
        return TP_ERR_FAIL;
    }

    double h2;
    pmCartMagSq(&tc->coords.circle.xyz.rHelix, &h2);
    double helical_length = pmSqrt(pmSq(tc->coords.circle.fit.total_planar_length) + h2);

    tc->target = helical_length;
    return TP_ERR_OK;
}



int pmRigidTapInit(PmRigidTap * const tap,
        EmcPose const * const start,
        EmcPose const * const end,
        double reversal_scale)
{
    PmCartesian start_xyz, end_xyz;
    PmCartesian abc, uvw;

    //Slightly more allocation this way, but much easier to read
    emcPoseToPmCartesian(start, &start_xyz, &abc, &uvw);
    emcPoseGetXYZ(end, &end_xyz);

    // Setup XYZ motion
    pmCartLineInit(&tap->xyz, &start_xyz, &end_xyz);

    // Copy over fixed ABC and UVW points
    tap->abc = abc;
    tap->uvw = uvw;

    // Setup initial tap state
    tap->reversal_target = tap->xyz.tmag;
    tap->reversal_scale = reversal_scale;
    tap->state = RIGIDTAP_START;
    return TP_ERR_OK;

}

double pmRigidTapTarget(PmRigidTap * const tap, double uu_per_rev)
{
    // allow 10 turns of the spindle to stop - we don't want to just go on forever
    double overrun = 10. * uu_per_rev;
    double target = tap->xyz.tmag + overrun;
    tp_debug_print("initial tmag = %.12g, added %.12g for overrun, target = %.12g\n",
            tap->xyz.tmag, overrun,target);
    return target;
}

/** Returns true if segment has ONLY rotary motion, false otherwise. */
int tcPureRotaryCheck(TC_STRUCT const * const tc)
{
    return (tc->motion_type == TC_LINEAR) &&
        (tc->coords.line.xyz.tmag_zero) &&
        (tc->coords.line.uvw.tmag_zero);
}


/**
 * Given a PmCircle and a circular segment, copy the circle in as the XYZ portion of the segment, then update the motion parameters.
 * NOTE: does not yet support ABC or UVW motion!
 */
int tcSetCircleXYZ(TC_STRUCT * const tc, PmCircle const * const circ)
{

    //Update targets with new arc length
    if (!circ || tc->motion_type != TC_CIRCULAR) {
        return TP_ERR_FAIL;
    }
    if (!tc->coords.circle.abc.tmag_zero || !tc->coords.circle.uvw.tmag_zero) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SetCircleXYZ does not supportABC or UVW motion\n");
        return TP_ERR_FAIL;
    }

    // Store the new circular segment (or use the current one)

    if (!circ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SetCircleXYZ missing new circle definition\n");
        return TP_ERR_FAIL;
    }

    tc->coords.circle.xyz = *circ;
    // Update the arc length fit to this new segment
    findSpiralArcLengthFit(&tc->coords.circle.xyz, &tc->coords.circle.fit);

    // compute the new total arc length using the fit and store as new
    // target distance
    tc->target = pmCircle9Target(&tc->coords.circle);

    return TP_ERR_OK;
}

/**
 * Clean up Ruckig planner resources in a TC_STRUCT.
 */
void tcCleanupRuckig(TC_STRUCT * const tc)
{
    if (!tc) {
        return;
    }

    if (tc->ruckig_planner) {
        ruckig_destroy(tc->ruckig_planner);
        tc->ruckig_planner = NULL;
    }
    tc->ruckig_planned = 0;
    tc->ruckig_trajectory_time = 0.0;
}

int tcClearFlags(TC_STRUCT * const tc)
{
    if (!tc) {
        return TP_ERR_MISSING_INPUT;
    }

    //KLUDGE this will need to be updated manually if any other flags are added.
    tc->is_blending = false;

    return TP_ERR_OK;
}

/**
 * Sample Ruckig profile at given time
 *
 * Evaluates the pre-computed Ruckig trajectory profile at time t.
 * The profile consists of 7 phases with constant jerk in each phase.
 * Within each phase, we integrate:
 *   a(dt) = a0 + j * dt
 *   v(dt) = v0 + a0*dt + 0.5*j*dt^2
 *   p(dt) = p0 + v0*dt + 0.5*a0*dt^2 + (1/6)*j*dt^3
 *
 * @param profile   Pointer to ruckig_profile_t
 * @param t         Time since segment start (seconds)
 * @param pos       Output: position at time t
 * @param vel       Output: velocity at time t
 * @param acc       Output: acceleration at time t
 * @param jerk      Output: jerk at time t
 * @return          0 on success, -1 if profile invalid
 */
int ruckigProfileSample(ruckig_profile_t const * const profile,
                        double t,
                        double *pos,
                        double *vel,
                        double *acc,
                        double *jerk)
{
    if (!profile || !profile->valid) {
        return -1;
    }

    // Clamp time to valid range
    if (t < 0.0) t = 0.0;
    if (t >= profile->duration) {
        // At or past end - return final state with zero jerk
        *pos = profile->p[RUCKIG_PROFILE_PHASES];
        *vel = profile->v[RUCKIG_PROFILE_PHASES];
        *acc = profile->a[RUCKIG_PROFILE_PHASES];
        *jerk = 0.0;
        return 0;
    }

    // Find which phase we're in
    int phase = 0;
    double t_phase_start = 0.0;

    for (int i = 0; i < RUCKIG_PROFILE_PHASES; i++) {
        if (t < profile->t_sum[i]) {
            phase = i;
            break;
        }
        t_phase_start = profile->t_sum[i];
        phase = i + 1;
    }

    // Clamp phase to valid range
    if (phase >= RUCKIG_PROFILE_PHASES) {
        phase = RUCKIG_PROFILE_PHASES - 1;
        t_phase_start = (phase > 0) ? profile->t_sum[phase - 1] : 0.0;
    }

    // Time within this phase
    double dt = t - t_phase_start;

    // Get phase boundary conditions
    double p0 = profile->p[phase];
    double v0 = profile->v[phase];
    double a0 = profile->a[phase];
    double j = profile->j[phase];

    // Polynomial evaluation for phase-based profiles
    // Constant jerk per phase is exact for Ruckig's native phases
    *jerk = j;
    *acc = a0 + j * dt;
    *vel = v0 + a0 * dt + 0.5 * j * dt * dt;
    *pos = p0 + v0 * dt + 0.5 * a0 * dt * dt + (1.0/6.0) * j * dt * dt * dt;

    return 0;
}

/**
 * RT-safe jerk-limited emergency stop.
 *
 * Computes one cycle of jerk-limited deceleration to bring velocity to zero.
 * Called each RT cycle when abort/pause is active during Ruckig execution.
 *
 * Algorithm: sqrt-profile targeting
 * - Far from stop: ramp up to max deceleration
 * - Close to stop: use sqrt(2*j*v) to compute smooth approach
 * - Clamp jerk to limits, prevent velocity overshoot through zero
 *
 * Known limitations:
 * - Not time-optimal (proper 3-phase profile would be better)
 * - Edge cases near v=0 use heuristic clamping
 * - Does not pre-compute full stopping trajectory
 *
 * Future improvement: Have userspace compute stopping trajectory via
 * Ruckig when abort detected, hand off to RT via predictive mechanism.
 */
void tcComputeJerkLimitedStop(double v0, double a0,
                              double j_max, double a_max,
                              double dt,
                              double *v_out, double *a_out,
                              double *j_out, double *dist_out)
{
    // Handle edge cases
    if (j_max <= 0.0) j_max = 1e6;
    if (a_max <= 0.0) a_max = 1e6;
    if (dt <= 0.0) dt = 0.001;

    // Already stopped?
    if (fabs(v0) < 1e-9 && fabs(a0) < 1e-9) {
        *v_out = 0.0;
        *a_out = 0.0;
        *j_out = 0.0;
        *dist_out = 0.0;
        return;
    }

    // Direction of motion
    double sign_v = (v0 >= 0.0) ? 1.0 : -1.0;
    double v_abs = fabs(v0);

    // Velocity that can be removed in one jerk phase (ramp down accel to 0):
    // From a = j*t and v = 0.5*j*t^2, we get v = 0.5*a^2/j
    double v_jerk_phase = 0.5 * a_max * a_max / j_max;

    double j_cmd;
    double a_target;

    if (v_abs > v_jerk_phase * 2.0) {
        // Far from stop: build up to max deceleration
        a_target = -sign_v * a_max;
    } else {
        // Close to stop: use sqrt profile for smooth approach
        // Target accel that will bring us to zero smoothly
        a_target = -sign_v * sqrt(2.0 * j_max * v_abs);
        if (fabs(a_target) > a_max) {
            a_target = -sign_v * a_max;
        }
    }

    // Compute jerk needed to move toward target acceleration
    double a_error = a_target - a0;
    double j_needed = a_error / dt;

    // Clamp jerk to limits
    if (j_needed > j_max) j_needed = j_max;
    if (j_needed < -j_max) j_needed = -j_max;
    j_cmd = j_needed;

    // Check if we'd overshoot zero velocity this cycle
    double v_predicted = v0 + a0 * dt + 0.5 * j_cmd * dt * dt;
    if ((v0 > 0 && v_predicted < 0) || (v0 < 0 && v_predicted > 0)) {
        // Would cross zero - compute jerk to land exactly at v=0
        // v0 + a0*dt + 0.5*j*dt^2 = 0
        // j = -2*(v0 + a0*dt) / dt^2
        j_cmd = -2.0 * (v0 + a0 * dt) / (dt * dt);
        if (j_cmd > j_max) j_cmd = j_max;
        if (j_cmd < -j_max) j_cmd = -j_max;

        // Recompute with clamped jerk
        v_predicted = v0 + a0 * dt + 0.5 * j_cmd * dt * dt;
        if ((v0 > 0 && v_predicted < 0) || (v0 < 0 && v_predicted > 0)) {
            v_predicted = 0.0;
        }
    }

    // Integrate one cycle
    double a_new = a0 + j_cmd * dt;
    double v_new = v0 + a0 * dt + 0.5 * j_cmd * dt * dt;
    double d_new = v0 * dt + 0.5 * a0 * dt * dt + (1.0/6.0) * j_cmd * dt * dt * dt;

    // Clamp acceleration
    if (a_new > a_max) a_new = a_max;
    if (a_new < -a_max) a_new = -a_max;

    // Final velocity clamp - don't reverse
    if ((v0 > 0 && v_new < 0) || (v0 < 0 && v_new > 0)) {
        v_new = 0.0;
        a_new = 0.0;
    }

    // If velocity is zero, zero acceleration too
    if (fabs(v_new) < 1e-9) {
        v_new = 0.0;
        a_new = 0.0;
    }

    *v_out = v_new;
    *a_out = a_new;
    *j_out = j_cmd;
    *dist_out = (d_new > 0) ? d_new : 0.0;  // Don't go backwards
}


