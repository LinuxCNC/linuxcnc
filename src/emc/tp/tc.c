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
#include "spherical_arc9.h"
#include "motion_types.h"

//Debug output
#include "tp_debug.h"
#include "tp_enums.h"
#include "tp_call_wrappers.h"


double tcGetMaxVelFromLength(TC_STRUCT const * const tc)
{
    double sample_maxvel = tc->target / (tc->cycle_time * TP_MIN_SEGMENT_CYCLES);
    return fmin(tc->maxvel_geom, sample_maxvel);
}

/**
 * Planning-time maximum velocity for the given segment.
 * This function ignores runtime conditions like if a segment is on final deceleration, actively blending, etc.
 */
double tcGetPlanMaxTargetVel(
    TC_STRUCT const * const tc,
    double max_feed_scale)
{
    double v_max = tcGetMaxVelFromLength(tc);
    double v_max_target = v_max;

    switch (tc->synchronized) {
        case TC_SYNC_NONE:
            // Get maximum reachable velocity from max feed override
            v_max_target = tc->reqvel * max_feed_scale;
            break;

        case TC_SYNC_VELOCITY: //Fallthrough
        case TC_SYNC_POSITION:
            // Assume no spindle override during blend target
            // max feed override here is variability of the spindle speed, rather than a user feed override
            v_max_target = tc->uu_per_rev * tc->tag.speed * max_feed_scale;
            break;
    }

    // Clip maximum velocity by the segment's own maximum velocity
    return fmin(v_max_target, v_max);
}

double tcGetAccelScale(const TC_STRUCT *tc)
{
    // Handle any acceleration reduction due to an approximate-tangent "blend" with the previous or next segment
    double a_scale = (1.0 - fmax(tc->kink_accel_reduce, tc->kink_accel_reduce_prev));
    return a_scale;
}

double tcGetOverallMaxAccel(const TC_STRUCT *tc)
{
    return tc->maxaccel * tcGetAccelScale(tc);
}

/**
 * Get acceleration for a tc based on the trajectory planner state.
 */
double tcGetTangentialMaxAccel(TC_STRUCT const * const tc)
{
    double a_scale = tcGetAccelScale(tc);

    // Reduce allowed tangential acceleration in circular motions to stay
    // within overall limits (accounts for centripetal acceleration while
    // moving along the circular path).
    if (tc->motion_type == TC_CIRCULAR || tc->motion_type == TC_SPHERICAL) {
        //Limit acceleration for cirular arcs to allow for normal acceleration
        a_scale *= tc->acc_ratio_tan;
    }
    return tc->maxaccel * a_scale;
}


int tcSetKinkProperties(TC_STRUCT *prev_tc, TC_STRUCT *tc, double kink_vel, double accel_reduction)
{
  // NOTE: use_kink field is not set until later, if we choose tangent blending without an arc connector
  prev_tc->kink_vel = kink_vel;
  prev_tc->kink_accel_reduce = fmax(accel_reduction, prev_tc->kink_accel_reduce);
  tc->kink_accel_reduce_prev = fmax(accel_reduction, tc->kink_accel_reduce_prev);

  return 0;
}

/**
 * Check if a segment can be consumed without disrupting motion or synced IO.
 */
int tcCanConsume(TC_STRUCT const * const tc)
{
    if (!tc) {
        return false;
    }

    if (tc->motion_type != TC_LINEAR) {
        return false;
    }

    if (tc->syncdio.anychanged || tc->atspeed) {
        //TODO add other conditions here (for any segment that should not be consumed by blending
        return false;
    }

    return true;
}

/**
 * Calulate the unit tangent vector at the start of a move for any segment.
 */
int tcGetStartTangentUnitVector(TC_STRUCT const * const tc, PmVector * const out)
{
    // TODO Optimize
    return tcGetTangentUnitVector(tc, 0, out);
}

/**
 * Calculate the unit tangent vector at the end of a move for any segment.
 */
int tcGetEndTangentUnitVector(
    TC_STRUCT const * const tc,
    PmVector * const out) {

    // TODO Optimize
    return tcGetTangentUnitVector(tc, tc->target, out);
}

int tcGetTangentUnitVector(TC_STRUCT const * const tc, double progress, PmVector * const out) {
    static const PmCartesian zero={0,0,0};
    switch (tc->motion_type) {
        case TC_LINEAR:
            *out = tc->coords.line.uVec;
            return 0;
        case TC_RIGIDTAP:
            // Augment with zero vector, no re-scaling necessary
            return CartToVec(&tc->coords.rigidtap.nominal_xyz.uVec, &zero, &zero, out);
        case TC_DWELL:
            return -1;
        case TC_CIRCULAR:
        {
            return pmCircle9TangentVector(
                &tc->coords.circle,
                progress,
                out);
        }
        case TC_SPHERICAL:
        {
            double t = progress / tc->target;
            return arc9Tangent(&tc->coords.arc, t, out);
        }
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "Invalid motion type %d!\n",tc->motion_type);
    return TP_ERR_FAIL;
}

double tcGetVLimit(TC_STRUCT const * const tc, double v_target, double v_limit_linear, double v_limit_angular)
{
    if ((tc->synchronized == TC_SYNC_POSITION)){
        // No limits applied during position sync
        return v_target;
    }
    switch (tc->motion_type) {
    case TC_LINEAR:
        return pmLine9VLimit(&tc->coords.line, v_target, v_limit_linear, v_limit_angular);
    case TC_CIRCULAR:
        return pmCircle9VLimit(&tc->coords.circle, v_target, v_limit_linear, v_limit_angular);
    case TC_SPHERICAL:
        return arc9VLimit(&tc->coords.arc, v_target, v_limit_linear, v_limit_angular);
    case TC_RIGIDTAP:
    case TC_DWELL:
        break;
        // for the non-synched portion of rigid tapping
    }
    return fmin(v_target, v_limit_linear);
}

/**
 * Calculate the distance left in the trajectory segment in the indicated
 * direction.
 */
double tcGetDistanceToGo(TC_STRUCT const * const tc, int direction)
{
    double distance;
    if (direction == TC_DIR_FORWARD) {
        // Return standard distance to go
        distance = tc->target - tc->progress;
    } else {
        // Reverse direction, distance from zero instead of target
        distance = tc->progress;
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
 * @return	 PmVector   returns a position (\ref PmVector = datatype carrying XYZABC information
 */

int tcGetPos(TC_STRUCT const * const tc, PmVector * const out) {
    tcGetPosReal(tc, tc->progress, out);
    return 0;
}

int tcGetStartpoint(TC_STRUCT const * const tc, PmVector * const pos)
{
    PmCartesian xyz;
    PmCartesian abc;
    PmCartesian uvw;

    switch (tc->motion_type){
        case TC_RIGIDTAP:
            xyz = tc->coords.rigidtap.nominal_xyz.start;
            abc = tc->coords.rigidtap.abc;
            uvw = tc->coords.rigidtap.uvw;
            return CartToVec(&xyz, &abc, &uvw, pos);
        case TC_LINEAR:
            *pos = tc->coords.line.start;
            return 0;
        case TC_CIRCULAR:
            pmCircleStartPoint(&tc->coords.circle.xyz, &xyz);
            abc = tc->coords.circle.abc.start;
            uvw = tc->coords.circle.uvw.start;
            return CartToVec(&xyz, &abc, &uvw, pos);
        case TC_SPHERICAL:
            return arc9Point(&tc->coords.arc,
                    0,
                    pos);
        case TC_DWELL:
            *pos = tc->coords.dwell.dwell_pos;
            return TP_ERR_OK;
    }

    return TP_ERR_FAIL;
}

int tcGetEndpoint(TC_STRUCT const * const tc, PmVector * const out) {
    tcGetPosReal(tc, tc->target, out);
    return 0;
}

int tcGetPosReal(TC_STRUCT const * const tc, double progress, PmVector * const pos)
{
    PmCartesian xyz;
    PmCartesian abc;
    PmCartesian uvw;

    switch (tc->motion_type){
        case TC_RIGIDTAP:
            pmCartLinePoint(&tc->coords.rigidtap.actual_xyz, progress, &xyz);
            // no rotary move allowed while tapping
            abc = tc->coords.rigidtap.abc;
            uvw = tc->coords.rigidtap.uvw;
            return CartToVec(&xyz, &abc, &uvw, pos);
        case TC_LINEAR:
            return pmLine9Point(&tc->coords.line, progress, pos);
        case TC_CIRCULAR:
            return pmCircle9Point(&tc->coords.circle, progress, pos);
        case TC_SPHERICAL:
            return arc9Point(&tc->coords.arc,
                    progress,
                    pos);
        case TC_DWELL:
            *pos = tc->coords.dwell.dwell_pos;
            return TP_ERR_OK;
    }
    return -1;
}


/**
 * Set the terminal condition (i.e. blend or stop) for the given motion segment.
 * Also sets flags on the next segment relevant to blending (e.g. parabolic blend sets the blend_prev flag).
 */
int tcSetTermCond(TC_STRUCT *tc, tc_term_cond_t term_cond)
{
    if (!tc) {
        return -1;
    }

    tc->term_cond = term_cond;
    return 0;
}

int tcFindBlendTolerance(TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc, double * const T_blend)
{
    const double tolerance_ratio = 0.25;
    double T1 = prev_tc->tolerance;
    double T2 = tc->tolerance;
    //Detect zero tolerance = no tolerance and force to reasonable maximum
    if (T1 <= 0) {
        T1 = prev_tc->nominal_length * tolerance_ratio;
    }
    if (T2 <= 0) {
        T2 = tc->nominal_length * tolerance_ratio;
    }
    double nominal_tolerance = fmin(T1,T2);
    //Blend tolerance is the limit of what we can reach by blending alone,
    //consuming half a segment or less (parabolic equivalent)
    double blend_tolerance = fmin(fmin(nominal_tolerance,
                prev_tc->nominal_length * tolerance_ratio),
            tc->nominal_length * tolerance_ratio);
    *T_blend = blend_tolerance;
    return 0;
}

/**
 * Initialize a new trajectory segment with common parameters.
 *
 * NOTE: this function only sets default values that are non-zero. Make sure
 * the struct is properly initialized BEFORE calling this function.
 */
int tcInit(TC_STRUCT * const tc,
        tc_motion_type_t motion_type,
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

    /** Segment settings (given values later during setup / optimization) */
    tc->indexrotary = INDEX_NONE;

    tc->active_depth = 1;

    tc->acc_ratio_tan = BLEND_ACC_RATIO_TANGENTIAL;

    return TP_ERR_OK;
}

/**
 * Set kinematic properties for a trajectory segment.
 */
int tcSetupMotion(TC_STRUCT * const tc,
        double vel,
        double ini_maxvel,
        double acc)
{
    //FIXME assumes that state is already set up in TC_STRUCT, which depends on external order of function calls.

    tc->maxaccel = acc;

    tc->maxvel_geom = ini_maxvel;

    tc->reqvel = vel;
    // Initial guess at target velocity is just the requested velocity
    tc->target_vel = vel;

    return TP_ERR_OK;
}

int tcSetupState(TC_STRUCT * const tc, TP_STRUCT const * const tp)
{
    tc->tolerance = tp->tolerance;
    tc->synchronized = tp->synchronized;
    tc->uu_per_rev = tp->uu_per_rev;
    return TP_ERR_OK;
}

int tcUpdateCircleAccRatio(TC_STRUCT * tc, double v_max_path)
{
    if (tc->motion_type != TC_CIRCULAR) {
        return TP_ERR_OK;
    }
    if (tc->coords.circle.xyz_ratio <= 0.0) {
        return TP_ERR_FAIL;
    }

    double v_max3 = v_max_path * tc->coords.circle.xyz_ratio;
    PmCircleLimits limits = pmCircleActualMaxVel(&tc->coords.circle.xyz,
                                                 v_max3,
                                                 tc->acc_normal_max * tcGetAccelScale(tc));
    tc->maxvel_geom = limits.v_max / tc->coords.circle.xyz_ratio;
    tc->acc_ratio_tan = limits.acc_ratio;
    return TP_ERR_OK;
}

/**
 * "Finalizes" a segment so that its length can't change.
 * By setting the finalized flag, we tell the optimizer that this segment's
 * length won't change anymore. Since any blends are already set up, we can
 * trust that the length will be the same, and so can use the length in the
 * velocity optimization.
 */
int tcFinalizeLength(TC_STRUCT * const tc, double max_feed_override)
{
    //Apply velocity corrections
    if (!tc) {
        return TP_ERR_OK;
    }

    tp_debug_json5_log_start(tcFinalizeLength);
    if (tc->finalized) {
        tp_debug_json5_log_end("tc %d already finalized", tc->id);
        return TP_ERR_OK;
    }

#ifdef TP_DEBUG
    double maxvel_old = tc->maxvel_geom;
#endif

    CHP(tcUpdateCircleAccRatio(tc, tcGetPlanMaxTargetVel(tc, max_feed_override)));

    if (tc->term_cond == TC_TERM_COND_PARABOLIC) {
        tcSetTermCond(tc, TC_TERM_COND_EXACT);
    }
    tc->finalized = 1;

#ifdef TP_DEBUG
    print_json5_tc_id_data_(tc);
    print_json5_string_("motion_type", tcMotionTypeAsString((tc_motion_type_t)tc->motion_type));
    print_json5_string_("term_cond", tcTermCondAsString((tc_term_cond_t)tc->term_cond));
    print_json5_double(maxvel_old);
    print_json5_double_("maxvel", tc->maxvel_geom);
    print_json5_double_("acc_ratio_tan", tc->acc_ratio_tan);
#endif

    tp_debug_json5_log_end("tc %llu finalized", tc->unique_id);
    return TP_ERR_OK;
}

int pmRigidTapInit(PmRigidTap * const tap,
        PmVector const * const start,
        PmVector const * const end,
        double reversal_scale)
{
    PmCartesian start_xyz, end_xyz;
    PmCartesian abc, uvw;

    //Slightly more allocation this way, but much easier to read
    VecToCart(start, &start_xyz, &tap->abc, &tap->uvw);
    VecToCart(end, &end_xyz, &abc, &uvw);

    // Setup XYZ motion
    pmCartLineInit(&tap->nominal_xyz, &start_xyz, &end_xyz);
    tap->actual_xyz = tap->nominal_xyz;

    // Setup initial tap state
    tap->reversal_target = tap->nominal_xyz.tmag;
    tap->reversal_scale = reversal_scale;
    tap->state = RIGIDTAP_TAPPING;

    if (!pmCartCartCompare(&tap->abc, &abc) || !pmCartCartCompare(&tap->uvw, &uvw)) {
        return TP_ERR_RANGE;
    }
    return TP_ERR_OK;

}

/** Returns true if segment has ONLY rotary motion, false otherwise. */
int tcPureRotaryCheck(TC_STRUCT const * const tc)
{
    return (tc->motion_type == TC_LINEAR) &&
           pmLine9LinearAxisLength(&tc->coords.line) == 0;
}
#if 0
// TODO replace with 9D
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

    // Store the new circular segment (or use the current one)

    if (!circ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SetCircleXYZ missing new circle definition\n");
        return TP_ERR_FAIL;
    }

    tc->coords.circle.xyz = *circ;
    // Update the arc length fit to this new segment
    findSpiralArcLengthFit(&tc->coords.circle.xyz, &tc->coords.circle.fit, TP_ANGLE_EPSILON);

    // compute the new total arc length using the fit and store as new
    // target distance
    tc->target = pmCircle9Length(&tc->coords.circle);

    return TP_ERR_OK;
}
#endif

// Helper functions to convert enums to pretty-print for debug output

const char *tcTermCondAsString(tc_term_cond_t c)
{
    switch (c)
    {
        case TC_TERM_COND_STOP:
            return "EXACT_STOP";
        case TC_TERM_COND_EXACT:
            return "EXACT_PATH";
        case TC_TERM_COND_PARABOLIC:
            return "PARABOLIC";
        case TC_TERM_COND_TANGENT:
            return "TANGENT";
    }
    return "NONE";
}

const char *tcMotionTypeAsString(tc_motion_type_t c)
{
    switch (c)
    {
        case TC_LINEAR:
            return "Linear";
        case TC_CIRCULAR:
            return "Circular";
        case TC_RIGIDTAP:
            return "RigidTap";
        case TC_SPHERICAL:
            return "SphericalArc";
        case TC_DWELL:
            return "Dwell";
    }
    return "NONE";
}

const char *tcSyncModeAsString(tc_spindle_sync_t c)
{
    switch (c)
    {
        case TC_SYNC_NONE:
            return "sync_none";
        case TC_SYNC_VELOCITY:
            return "sync_velocity";
        case TC_SYNC_POSITION:
            return "sync_position";
    }
    return "NONE";
}

int tcSetLine9(TC_STRUCT * const tc, PmLine9 const * const line9)
{
    if (tc->motion_type != TC_LINEAR) { return -1; }
    tc->coords.line = *line9;
    tc->target = pmLine9Length(line9);
    return 0;
}

int tcSetCircle9(TC_STRUCT * const tc, PmCircle9 const * const circle9)
{
    if (tc->motion_type != TC_CIRCULAR) { return -1; }
    tc->coords.circle = *circle9;
    tc->target = circle9->total_length;
    return 0;
}
