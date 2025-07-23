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
********************************************************************/
#include "rtapi.h"              /* rtapi_print_msg */
#include "posemath.h"           /* Geometry types & functions */
#include "tc.h"
#include "tp.h"
#include "emcpose.h"
#include "rtapi_math.h"
#include "mot_priv.h"
#include "motion_debug.h"
#include "motion_types.h"
#include "spherical_arc.h"
#include "blendmath.h"
#include "math_util.h"
#include "joint_util.h"
#include "string.h"
#include "tp_priv.h"
#include "motion_shared.h"
#include "tp_call_wrappers.h"
#include "error_util.h"

// TODO make these configurable as machine parameters via INI to give integrators more control
// Absolute maximum distance (in revolutions) to add to the end of a rigidtap move to account for spindle reversal
static const double RIGIDTAP_MAX_OVERSHOOT_REVS = 10.0;

/**
 * @section tpdebugflags TP debugging flags
 * Enable / disable various debugging functions here.
 * These flags control debug printing from RTAPI. These functions are
 * admittedly kludged on top of the existing rtapi_print framework. As written,
 * though, it's an easy way to selectively compile functions as static or not,
 * and selectively compile in assertions and debug printing.
 */

#include "tp_debug.h"

#ifdef TP_DEBUG
static const bool _tp_debug = true;
#else
static const bool _tp_debug = false;
#endif

#ifdef TC_DEBUG
static const bool _tc_debug = true;
#else
static const bool _tc_debug = false;
#endif

// NOTE: turned off this feature, which causes blends between rapids to
// use the feed override instead of the rapid override
#undef TP_SHOW_BLENDS

#define TP_OPTIMIZATION_LAZY

static TC_STRUCT blend_tc1 = {};
static TC_STRUCT blend_tc2 = {};

void clearPositionSyncErrors(void);

/** static function primitives (ugly but less of a pain than moving code around)*/

int tpComputeBlendVelocity(
        TC_STRUCT const *tc,
        TC_STRUCT const *nexttc,
        double v_target_this,
        double v_target_next,
        double *v_blend_this,
        double *v_blend_next,
        double *v_blend_net);

static inline double findVPeak(double a_t_max, double distance)
{
    return pmSqrt(a_t_max * distance);
}

void reportTPAxisError(TP_STRUCT const *tp, unsigned failed_axes, const char *msg_prefix)
{
    if (failed_axes)
    {
        AxisMaskString failed_axes_str = axisBitMaskToString(failed_axes);
        rtapi_print_msg(RTAPI_MSG_ERR, "%s, ax%cs [%s], line %d, %g sec\n",
                        msg_prefix ?: "unknown error",
                        failed_axes_str.len > 1 ? 'e' : 'i', // Ugly workaround for english grammar
                        failed_axes_str.axes,
                        tp->execTag.fields[GM_FIELD_LINE_NUMBER],
                        tp->time_elapsed_sec);
    }
}

static bool needConsistencyCheck(ConsistencyCheckMask mask)
{
    return emcmotConfig->consistencyCheckConfig.extraConsistencyChecks & mask;
}

/**
 * @section tpgetset Internal Get/Set functions
 * @brief Calculation / status functions for commonly used values.
 * These functions return the "actual" values of things like a trajectory
 * segment's feed override, while taking into account the status of tp itself.
 */

/**
 * Wrapper to bounds-check the tangent kink ratio from HAL.
 */
double tpGetTangentKinkRatio(void) {
    const double max_ratio = 0.7071;
    const double min_ratio = 0.001;

    return fmax(fmin(emcmotConfig->arc_blend_cfg.tangent_kink_ratio,max_ratio),min_ratio);
}


/**
 * Get a segment's feed scale based on the current planner state and emcmotStatus.
 * @note depends on emcmotStatus for system information.
 */
double tpGetRealAbsFeedScale(TP_STRUCT const * const tp,
        TC_STRUCT const * const tc) {
    if (!tc) {
        return 0.0;
    }

    double net_feed_scale = fabs(emcmotStatus->net_feed_scale);

    //All reasons to disable feed override go here
    bool pausing = tp->pausing && (tc->synchronized == TC_SYNC_NONE || tc->synchronized == TC_SYNC_VELOCITY);
    bool aborting = tp->aborting;
    if (pausing)  {
        return 0.0;
    } else if (aborting) {
        return 0.0;
    } else if (tc->synchronized == TC_SYNC_POSITION ) {
        return 1.0;
    } else {
        return net_feed_scale;
    }
}

/**
 * Get target velocity for a tc based on the trajectory planner state.
 * This gives the requested velocity, capped by the segments maximum velocity.
 * @note returns the magnitude of velocity (reverse run is handled at a higher level)
 */
double tpGetRealAbsTargetVel(TP_STRUCT const * const tp,
        TC_STRUCT const * const tc) {

    if (!tc) {
        return 0.0;
    }
    // Start with the scaled target velocity based on the current feed scale
    double v_target = tc->synchronized ? tc->target_vel : tc->reqvel;
    /*tc_debug_print("Initial v_target = %f\n",v_target);*/

    // Get the maximum allowed target velocity, and make sure we're below it
    return fmin(v_target * tpGetRealAbsFeedScale(tp,tc), tpGetRealMaxTargetVel(tp, tc));
}

double getMaxBlendFeedScale(TC_STRUCT const * prev_tc, TC_STRUCT const * tc)
{
    //All reasons to disable feed override go here
    if ((tc && tc->synchronized == TC_SYNC_POSITION) ||
            (prev_tc && prev_tc->synchronized == TC_SYNC_POSITION)) {
        return 1.0;
    } else {
        return fmin(emcmotConfig->biarc_solver_cfg.feed_override_allowance, emcmotConfig->maxFeedScale);
    }
}

/**
 * Get the worst-case target velocity for a segment based on the trajectory planner state.
 * Note that this factors in the user-specified velocity limit.
 */
double tpGetRealMaxTargetVel(TP_STRUCT const * const tp, TC_STRUCT const * const tc)
{
    if (!tc) {
        return 0.0;
    }

    double max_scale = (tc->synchronized == TC_SYNC_POSITION) ? 1.0 : emcmotConfig->maxFeedScale;

    double v_max = tcGetMaxVelFromLength(tc);

    // Get maximum reachable velocity from max feed override
    double v_max_target = tc->target_vel * max_scale;

    /* Check if the cartesian velocity limit applies and clip the maximum
     * velocity. The vLimit is from the max velocity slider, and should
     * restrict the maximum velocity during non-synced moves and velocity
     * synchronization. However, position-synced moves have the target velocity
     * computed in the TP, so it would disrupt position tracking to apply this
     * limit here.
     */
    double v_limited = tcGetVLimit(tc, v_max_target, tp->vLimit, tp->vLimitAng);
    return fmin(v_limited, v_max);
}


/**
 * Get final velocity for a tc based on the trajectory planner state.
 * This function factors in the feed override and TC limits. It clamps the
 * final velocity to the maximum velocity and the next segment's target velocity
 */
double tpGetRealFinalVel(TP_STRUCT const * const tp,
        TC_STRUCT const * const tc, TC_STRUCT const * const nexttc) {
    /* If we're stepping, then it doesn't matter what the optimization says, we want to end at a stop.
     * If the term_cond gets changed out from under us, detect this and force final velocity to zero
     */
    if (emcmotDebug->stepping || tc->term_cond != TC_TERM_COND_TANGENT || tp->reverse_run) {
        return 0.0;
    } 
    
    // Get target velocities for this segment and next segment
    double v_target_this = tpGetRealAbsTargetVel(tp, tc);
    double v_target_next = 0.0;
    if (nexttc) {
        v_target_next = tpGetRealAbsTargetVel(tp, nexttc);
    }

    // Limit final velocity to minimum of this and next target velocities
    double v_target = fmin(v_target_this, v_target_next);
    double finalvel = fmin(tc->finalvel, v_target);
    return finalvel;
}

/**
 * Set up a spindle origin based on the current spindle COMMANDED direction and the given position.
 *
 * The origin is used to calculate displacements used in spindle position tracking.
 * The direction is stored as part of the origin to prevent discontinuous
 * changes in displacement due to sign flips
 */
void setSpindleOrigin(spindle_origin_t *origin, double position)
{
    if (!origin) {
        return;
    }
    origin->position = position;
    origin->direction = get_spindle_command_direction(emcmotStatus);
}

void updateSpindlePositionFromProgress(spindle_origin_t *origin, TC_STRUCT const * const tc)
{
    if (!origin || !tc) {
        return;
    }
    origin->position += tc->progress * origin->direction / tc->uu_per_rev;
    origin->direction = get_spindle_command_direction(emcmotStatus);
}

/**
 * @section tpaccess tp class-like API
 */

/**
 * Create the trajectory planner structure with an empty queue.
 */
int tpCreate(TP_STRUCT * const tp, int _queueSize, TC_STRUCT * const tcSpace)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }

    if (_queueSize <= 0) {
        tp->queueSize = TP_DEFAULT_QUEUE_SIZE;
    } else {
        tp->queueSize = _queueSize;
    }

    /* create the queue */
    if (-1 == tcqCreate(&tp->queue, tp->queueSize, tcSpace)) {
        return TP_ERR_FAIL;
    }

    tp->nextUniqueId = 0;
    /* init the rest of our data */
    return tpInit(tp);
}

/**
 * Clears any potential DIO toggles and anychanged.
 * If any DIOs need to be changed: dios[i] = 1, DIO needs to get turned on, -1
 * = off
 */
int tpClearDIOs(TP_STRUCT * const tp) {
    //XXX: All IO's will be flushed on next synced aio/dio! Is it ok?
    int i;
    tp->syncdio.anychanged = 0;
    tp->syncdio.dio_mask = 0;
    tp->syncdio.aio_mask = 0;
    for (i = 0; i < num_dio; i++) {
        tp->syncdio.dios[i] = 0;
    }
    for (i = 0; i < num_aio; i++) {
        tp->syncdio.aios[i] = 0;
    }

    return TP_ERR_OK;
}

void clearPosTrackingStatus()
{
    emcmotStatus->pos_tracking_error = 0;
    emcmotStatus->pos_tracking_velocity = 0;
}

/**
 *    "Soft initialize" the trajectory planner tp.
 *    This is a "soft" initialization in that TP_STRUCT configuration
 *    parameters (cycleTime, vMax, and aMax) are left alone, but the queue is
 *    cleared, and the flags are set to an empty, ready queue. The currentPos
 *    is left alone, and goalPos is set to this position.  This function is
 *    intended to put the motion queue in the state it would be if all queued
 *    motions finished at the current position.
 */
int tpClear(TP_STRUCT * const tp)
{
    if (!tp) {
        return 0;
    }
    // Soft-reset the queue (previous pointers to TC_STRUCTS should be considered invalid even though the memory still exists)
    tcqInit(&tp->queue);

    // Reset the TP goal position to whereever it is (since it's not moving), and all the side-effects of that (zero velocity / acceleration)
    tp->goalPos = tp->currentPos;
    emcmotStatus->requested_vel = 0.0;
    emcmotStatus->current_vel = 0.0;
    tp->currentVel = PmVector_zero;
    emcmotStatus->distance_to_go = 0.0;
    ZERO_EMC_POSE(emcmotStatus->dtg);

    // Clear out elapsed time (for debugging)
    tp->time_elapsed_sec = 0.0;
    tp->time_elapsed_ticks = 0;

    // Clear out all status related to motion IDs / tags since the queue is cleared
    tp->nextId = 0;
    tp->execId = 0;
    tp->nextexecId = 0;
    static const struct state_tag_t empty_tag = {};
    tp->execTag = empty_tag;
    tp->motionType = 0;
    tp->done = 1;
    tp->depth = tp->activeDepth = 0;

    tp->aborting = 0;
    tp->pausing = 0;
    tp->reverse_run = TC_DIR_FORWARD;

    // Clear spindle synchronization and related status
    tpSetSpindleSync(tp, 0.0, 0);
    emcmotStatus->spindle_fb.synced = 0;
    emcmotStatus->dwell_time_remaining = 0.0;
    clearPosTrackingStatus();

    // Clear any waits for spindle index / at-speed (since the motions that would be waiting no longer exist)
    tp->spindle.waiting_for_index = MOTION_INVALID_ID;
    tp->spindle.waiting_for_atspeed = MOTION_INVALID_ID;

    // TODO determine if this really needs to be here (because later in get_pos_cmds() it will be set anyway).
    SET_MOTION_INPOS_FLAG(1);

#ifdef TP_DEBUG
    PmCartesian axis_vel_limit = getXYZVelBounds();
    PmCartesian axis_accel_limit = getXYZAccelBounds();
    print_json5_log_start(tpClear);
    print_json5_PmCartesian(axis_vel_limit);
    print_json5_PmCartesian(axis_accel_limit);
    print_json5_log_end();
#endif
    return tpClearDIOs(tp);
}

/**
 * Fully initialize the tp structure.
 * Sets tp configuration to default values and calls tpClear to create a fresh,
 * empty queue.
 */
int tpInit(TP_STRUCT * const tp)
{
    tp->cycleTime = 0.0;
    //Velocity limits
    tp->vLimit = 0.0;
    tp->vLimitAng = 0.0;

    setSpindleOrigin(&tp->spindle.origin, 0.0);
    tp->spindle.trigger_revs = 0;

    tp->reverse_run = TC_DIR_FORWARD;

    // Initialize the current state (used during tpClear to initialize other state)
    tp->currentPos = PmVector_zero;
    tp->currentVel = PmVector_zero;

    // Only set up TP defaults for tolerance once (after that, it must match what interp / task specifies)
    tp->termCond = TC_TERM_COND_PARABOLIC;
    tp->tolerance = 0.0;

    return tpClear(tp);
}

/**
 * Set the cycle time for the trajectory planner.
 */
int tpSetCycleTime(TP_STRUCT * const tp, double secs)
{
    if (0 == tp || secs <= 0.0) {
        return TP_ERR_FAIL;
    }

    tp->cycleTime = secs;

    return TP_ERR_OK;
}

/**
 * Set the maximum velocity for linear and rotary-only moves.
 * I think this is the [TRAJ] max velocity. This should be the max velocity of
 * const the TOOL TIP, not necessarily any particular axis. This applies to
 * subsequent moves until changed.
 */
int tpSetVlimit(TP_STRUCT * const tp, double vLimit, double vLimitAng)
{
    if (!tp) return TP_ERR_FAIL;

    tp->vLimit = fmax(vLimit, 0.0);
    tp->vLimitAng = fmax(vLimitAng, 0.0);
    tp_debug_print("Setting Vlimit %f %f\n", tp->vLimit, tp->vLimitAng);

    return TP_ERR_OK;
}

/**
 * Sets the id that will be used for the next appended motions.
 * nextId is incremented so that the next time a motion is appended its id will
 * be one more than the previous one, modulo a signed int. If you want your own
 * ids for each motion, call this before each motion you append and stick what
 * you want in here.
 */
int tpSetId(TP_STRUCT * const tp, int id)
{

    if (!MOTION_ID_VALID(id)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpSetId: invalid motion id %d\n", id);
        return TP_ERR_FAIL;
    }

    if (0 == tp) {
        return TP_ERR_FAIL;
    }

    tp->nextId = id;

    return TP_ERR_OK;
}

tc_unique_id_t tpGetNextUniqueId(TP_STRUCT * const tp)
{
    return tp->nextUniqueId++;
}

void tcSetId(TP_STRUCT * const tp, TC_STRUCT * const tc, struct state_tag_t tag)
{
    // KLUDGE always number the possible blend arcs sequentially
    blend_tc1.unique_id = tpGetNextUniqueId(tp);
    blend_tc2.unique_id = tpGetNextUniqueId(tp);
    tc->unique_id = tpGetNextUniqueId(tp);
    blend_tc1.id = blend_tc2.id = tc->id = tp->nextId;
    tc->tag = tag;
    blend_tc1.tag = tag;
    blend_tc2.tag = tag;
}


/** Returns the id of the last motion that is currently
  executing.*/
int tpGetExecId(const TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }

    return tp->execId;
}

int tpGetCompletedId(const TP_STRUCT * const tp)
{
    if (!tp) {
        return 0;
    }

    //Ugly but direct approach
    //Alternative is to store ID locally
    return tp->tc_completed_id;
}

int tpGetQueuedId(TP_STRUCT const * const tp)
{
    if (!tp) {
        return 0;
    }

    TC_STRUCT const * const tc_last = tcqLast(&tp->queue);
    if (!tc_last) {
        return 0;
    }
    return tc_last->id;
}

struct state_tag_t tpGetExecTag(TP_STRUCT const * const tp)
{
    if (0 == tp) {
        struct state_tag_t empty = {};
        return empty;
    }

    return tp->execTag;
}

int tpGetExecSrcLine(const TP_STRUCT * const tp)
{
    if (tp) {
        return tp->execTag.fields[GM_FIELD_LINE_NUMBER];
    }
    return 0;
}

int tpGetNextExecId(const TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }

    return tp->nextexecId;
}

/**
 * Sets the termination condition for all subsequent queued moves.
 * If cond is TC_TERM_COND_STOP, motion comes to a stop before a subsequent move
 * begins. If cond is TC_TERM_COND_PARABOLIC, the following move is begun when the
 * current move slows below a calculated blend velocity.
 */
int tpSetTermCond(TP_STRUCT * const tp, tc_term_cond_t cond, double tolerance)
{
    tp_err_t res = TP_ERR_FAIL;

    switch (cond) {
        //Purposeful waterfall for now
        case TC_TERM_COND_PARABOLIC:
        case TC_TERM_COND_TANGENT:
        case TC_TERM_COND_EXACT:
        case TC_TERM_COND_STOP:
        if (tp) {
            tp->termCond = cond;
            tp->tolerance = tolerance;
            res = TP_ERR_OK;
        }
            break;
    }

    return res;
}

/**
 * Used to tell the tp the initial position.
 * It sets the current position AND the goal position to be the same.  Used
 * only at TP initialization and when switching modes.
 */
int tpSetPos(TP_STRUCT * const tp, EmcPose const * const pos)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }

    int res_invalid = tpSetCurrentPos(tp, pos);
    if (res_invalid) {
        return TP_ERR_FAIL;
    }

    emcPoseToPmVector(pos, &tp->goalPos);
    return TP_ERR_OK;
}


/**
 * Set current position.
 * It sets the current position AND the goal position to be the same.  Used
 * only at TP initialization and when switching modes.
 */
int tpSetCurrentPos(TP_STRUCT * const tp, EmcPose const * const pos)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }

    if (emcPoseValid(pos)) {
        emcPoseToPmVector(pos, &tp->currentPos);
        return TP_ERR_OK;
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "Tried to set invalid pose in tpSetCurrentPos on id %d!"
                "pos is %.12g, %.12g, %.12g\n",
                tp->execId,
                pos->tran.x,
                pos->tran.y,
                pos->tran.z);
        return TP_ERR_INVALID;
    }
}

int tpAddCurrentPos(TP_STRUCT * const tp, PmVector const * const disp)
{
    if (!tp || !disp) {
        return TP_ERR_MISSING_INPUT;
    }

    if (!VecHasNAN(disp)) {
        VecVecAddEq(&tp->currentPos, disp);
        return TP_ERR_OK;
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "Tried to set invalid pose in tpAddCurrentPos on id %d!"
                "disp is %.12g, %.12g, %.12g\n",
                tp->execId,
                disp->ax[0],
                disp->ax[1],
                disp->ax[2]);
        return TP_ERR_INVALID;
    }
}

/**
 * Check for valid tp before queueing additional moves.
 */
int tpErrorCheck(TP_STRUCT const * const tp) {

    if (!tp) {
        rtapi_print_msg(RTAPI_MSG_ERR, "TP is null\n");
        return TP_ERR_FAIL;
    }
    if (tp->aborting) {
        rtapi_print_msg(RTAPI_MSG_ERR, "TP is aborting\n");
        return TP_ERR_FAIL;
    }
    return TP_ERR_OK;
}


/**
 * Find the "peak" velocity a segment can achieve if its velocity profile is triangular.
 * This is used to estimate blend velocity, though by itself is not enough
 * (since requested velocity and max velocity could be lower).
 */
double tpCalculateTriangleVel(TC_STRUCT const *tc) {
    //Compute peak velocity for blend calculations
    double acc_scaled = tcGetTangentialMaxAccel(tc);
    double length = tc->target;
    if (!tc->finalized) {
        // blending may remove up to 1/2 of the segment
        length /= 2.0;
    }
    return findVPeak(acc_scaled, length);
}


double calculateOptimizationInitialVel(TC_STRUCT const * const prev_tc, TC_STRUCT const * const tc)
{
    double acc_scaled = tcGetTangentialMaxAccel(tc);
    double triangle_vel = findVPeak(acc_scaled, tc->target);
    double v_max_this = tcGetPlanMaxTargetVel(tc, emcmotConfig->maxFeedScale);
    double v_max_prev = tcGetPlanMaxTargetVel(prev_tc, emcmotConfig->maxFeedScale);
    double optim_init_vel = fmin(fmin(triangle_vel, v_max_this), v_max_prev);

#ifdef TP_DEBUG
    print_json5_log_start(calculateOptimizationInitialVel);
    print_json5_double(triangle_vel);
    print_json5_double(optim_init_vel);
    print_json5_log_end();
#endif
    return optim_init_vel;
}


/**
 * Initialize a blend arc from its parent lines.
 * This copies and initializes properties from the previous and next lines to
 * initialize a blend arc. This function does not handle connecting the
 * segments together, however.
 */
int tpInitBlendArcFromAdjacent(
    TP_STRUCT const * const tp,
    TC_STRUCT const * const adjacent_tc,
    TC_STRUCT* const blend_tc,
    double vel,
    double ini_maxvel,
    double acc,
    tc_motion_type_t motion_type) {

#ifdef TP_SHOW_BLENDS
    int canon_motion_type = EMC_MOTION_TYPE_ARC;
#else
    int canon_motion_type = adjacent_tc->canon_motion_type;
#endif

    tcInit(blend_tc,
            motion_type,
            canon_motion_type,
            tp->cycleTime,
            adjacent_tc->enables,
            false); // NOTE: blend arc never needs the atspeed flag, since the previous line will have it (and cannot be consumed).

    blend_tc->tag = adjacent_tc->tag;
    // KLUDGE tell an observer that the current segment is generated by the TP as part of a blend
    blend_tc->tag.packed_flags |= (0x1 << GM_FLAG_TP_BLEND);

    // Copy over state data from TP
    tcSetupState(blend_tc, tp);
    blend_tc->term_cond = TC_TERM_COND_TANGENT;

    // Set kinematics parameters from blend calculations
    tcSetupMotion(blend_tc,
            vel,
            ini_maxvel,
            acc);

    // find "helix" length for target
    double length;
    switch (motion_type) {
    case TC_LINEAR:
        length = blend_tc->coords.line.tmag;
        break;
    case TC_SPHERICAL:
        length = arc9Length(&blend_tc->coords.arc);
        break;
    default:
        return TP_ERR_FAIL;
    }
    blend_tc->target = length;
    blend_tc->nominal_length = length;
    blend_tc->syncdio = adjacent_tc->syncdio;

    //NOTE: blend arc radius and everything else is finalized, so set this to 1.
    //In the future, radius may be adjustable.
    CHP(tcFinalizeLength(blend_tc, emcmotConfig->maxFeedScale));

    return TP_ERR_OK;
}

#if 0
//TODO
int tcSetLineXYZ(TC_STRUCT * const tc, PmCartLine const * const line)
{

    //Update targets with new arc length
    if (!line || tc->motion_type != TC_LINEAR) {
        return TP_ERR_FAIL;
    }
    if (!tc->coords.line.abc.tmag_zero || !tc->coords.line.uvw.tmag_zero) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SetLineXYZ does not supportABC or UVW motion\n");
        return TP_ERR_FAIL;
    }

    tc->coords.line = *line;
    tc->target = line->tmag;
    return TP_ERR_OK;
}
#endif

int find_max_element(double arr[], int sz)
{
    if (sz < 1) {
        return -1;
    }
    // Assumes at least one element
    int max_idx = 0;
    for (int idx = 0; idx < sz; ++idx) {
        if (arr[idx] > arr[max_idx]) {
            max_idx = idx;
        }
    }
    return max_idx;
}

const char *blendTypeAsString(tc_blend_type_t c)
{
    switch(c) {
    case NO_BLEND:
        return "NO_BLEND";
    case PARABOLIC_BLEND:
        return "PARABOLIC_BLEND";
    case TANGENT_SEGMENTS_BLEND:
        return "TANGENT_SEGMENTS";
    case ARC_BLEND:
        return "ARC_BLEND";
    }
    return "";
}

bool isArcBlendFaster(TC_STRUCT const * const prev_tc, double expected_v_max)
{
    return fmax(prev_tc->parabolic_equiv_vel, prev_tc->kink_vel) < expected_v_max;
}

/**
 * Compare performance of blend arc and equivalent tangent speed.
 * If we can go faster by assuming the segments are already tangent (and
 * slowing down some), then prefer this over using the blend arc. This is
 * mostly useful for some odd arc-to-arc cases where the blend arc becomes very
 * short (and therefore slow).
 */
tc_blend_type_t tpChooseBestBlend(
        TC_STRUCT * const prev_tc,
        TC_STRUCT * const tc,
        double arc_blend_maxvel)
{
    if (!tc || !prev_tc) {
        return NO_BLEND;
    }

    switch  (prev_tc->term_cond)
    {
    case TC_TERM_COND_STOP:
    case TC_TERM_COND_EXACT:
        // Can't blend segments that are explicitly disallowed
        return NO_BLEND;
    case TC_TERM_COND_PARABOLIC:
    case TC_TERM_COND_TANGENT:
        break;
    }

    // KLUDGE Order the performance measurements so that they match the enum values
    double perf[] = {
        0.0, // No parabolic blends anymore
        prev_tc->kink_vel,
        arc_blend_maxvel,
    };

    tc_blend_type_t best_blend = find_max_element(perf, sizeof (perf)/sizeof (double));

    switch (best_blend) {
        case TANGENT_SEGMENTS_BLEND: // tangent
            // NOTE: acceleration / velocity reduction is done dynamically in functions that access TC_STRUCT properties
            tcSetTermCond(prev_tc, TC_TERM_COND_TANGENT);
            prev_tc->use_kink = 1;
            break;
        case ARC_BLEND: // arc blend
            break;
        case NO_BLEND:
        case PARABOLIC_BLEND:
            tcSetTermCond(prev_tc, TC_TERM_COND_STOP);
            break;
    }

#ifdef TP_DEBUG
    print_json5_log_start(tpChooseBestBlend);
    print_json5_double_("perf_parabolic", perf[PARABOLIC_BLEND]);
    print_json5_double_("perf_tangent", perf[TANGENT_SEGMENTS_BLEND]);
    print_json5_double_("perf_arc_blend", perf[ARC_BLEND]);
    print_json5_string_("best_blend", blendTypeAsString(best_blend));
    print_json5_log_end();
#endif

    return best_blend;
}

bool checkBiarcContinuity(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const this_tc)
{
    ContinuityCheck c1 = calc_C1_continuity(prev_tc, &blend_tc1);
    ContinuityCheck cmid = calc_C1_continuity(&blend_tc1, &blend_tc2);
    ContinuityCheck c2 = calc_C1_continuity(&blend_tc2, this_tc);
    double const min_dot_limit = 1.0 - emcmotConfig->consistencyCheckConfig.continuityAngleLimit_rad;
    bool c1_error = c1.dot < min_dot_limit
                    || cmid.dot < min_dot_limit
                    || c2.dot < min_dot_limit;

    if (c1_error) {
        rtapi_print_msg(RTAPI_MSG_WARN, "BiArc blend C1 continuity exceeds limits at line %d, unique_id %lld\n", prev_tc->tag.fields[GM_FIELD_LINE_NUMBER], prev_tc->unique_id);
    }
    if (_tp_debug || c1_error)
    {
        print_json5_log_start(checkBiarcContinuity);
        print_json5_long_long_("prev_unique_id", prev_tc->unique_id);
        print_json5_string_("b1_motion_type", tcMotionTypeAsString(prev_tc->motion_type));
        print_json5_string_("b2_motion_type", tcMotionTypeAsString(this_tc->motion_type));
        print_json5_PmVector_("u1_diff", c1.u_diff);
        print_json5_PmVector_("p1_diff", c1.p_diff);
        print_json5_double_("u1_diff_dot", c1.dot);
        print_json5_PmVector_("u_mid_diff", cmid.u_diff);
        print_json5_PmVector_("p_mid_diff", cmid.p_diff);
        print_json5_double_("u_mid_diff_dot", cmid.dot);
        print_json5_PmVector_("u2_diff", c2.u_diff);
        print_json5_double_("u2_diff_dot", c2.dot);
        print_json5_PmVector_("p2_diff", c2.p_diff);
        print_json5_log_end();
    }
    return !c1_error;
}

tp_err_t tpCreateBiarcBlend(TP_STRUCT * const tp, TC_STRUCT * const prev_tc, TC_STRUCT * const this_tc)
{
#ifdef TP_DEBUG
    double prev_len = prev_tc->target;
    double this_len = this_tc->target;
    int prev_can_consume = tcCanConsume(prev_tc);
    int this_can_consume = tcCanConsume(this_tc);

    double maxvel = fmin(
        tcGetPlanMaxTargetVel(prev_tc, emcmotConfig->biarc_solver_cfg.feed_override_allowance),
        tcGetPlanMaxTargetVel(this_tc, emcmotConfig->biarc_solver_cfg.feed_override_allowance));
    double reqvel = fmin(prev_tc->reqvel, this_tc->reqvel);
    tc_motion_type_t prev_motion_type = prev_tc->motion_type;
    tc_motion_type_t this_motion_type = this_tc->motion_type;
    PmVector u1_stretch={}, u2_stretch={};
    PmVector p1_stretch={}, p2_stretch={};
#endif

    PmVector vel_bound = getVelBounds();
    PmVector acc_bound = getAccelBounds();

    static biarc_solver_results_t biarc_results = {};
    memset(&biarc_results, 0, sizeof(biarc_solver_results_t));

    BlendParameters param_guess = {};
    BlendControls controls = {};
    CHP(optimize_biarc_blend_size(
        prev_tc,
        this_tc,
        &emcmotConfig->biarc_solver_cfg,
        &vel_bound,
        &acc_bound,
        &biarc_results,
        &controls,
        &param_guess,
        tp->cycleTime));

    biarc_control_points_t const * const control_pts = &biarc_results.solution.control_pts;
    blend_boundary_t const * const boundary = &biarc_results.solution.boundary;

    BlendPoints points1 = {}, points2 = {};
    BlendParameters param1 = {}, param2 = {};

    CHP(find_biarc_points_from_solution(
        &boundary->u1,
        &control_pts->u_mid,
        &boundary->P1,
        &control_pts->P_mid,
        &control_pts->Pb1,
        control_pts->d,
        &acc_bound,
        &vel_bound,
        &controls,
        &points1,
        &param1));

    // Check if previous segment can be consumed
    blendCheckConsume(&param1, boundary->s1, prev_tc, emcmotConfig->arc_blend_cfg.gap_cycles);
    if (param1.consume && blend_tc1.motion_type == TC_LINEAR) {
        // KLUDGE force the start point to be the previous line's starting point since it's going to be consumed
        points1.arc_start = prev_tc->coords.line.start;
    }
    CHP(init_blend_segment_from_points(&blend_tc1, &points1, &param1));

    CHP(find_biarc_points_from_solution(
        &control_pts->u_mid,
        &boundary->u2,
        &control_pts->P_mid,
        &boundary->P2,
        &control_pts->Pb2,
        control_pts->d,
        &acc_bound,
        &vel_bound,
        &controls,
        &points2,
        &param2));
    CHP(init_blend_segment_from_points(&blend_tc2, &points2, &param2));

    // Don't bother if another solution is better
    double expected_v_max = fmin(param1.v_plan, param2.v_plan);
    if (emcmotConfig->arc_blend_cfg.allow_fallback && !isArcBlendFaster(prev_tc, expected_v_max)) {
        return TP_ERR_NO_ACTION;
    }

    union {PmLine9 line9; PmCircle9 circle9;} prev_geom={}, this_geom={};

    switch (prev_tc->motion_type) {
    case TC_LINEAR:
        prev_geom.line9 = prev_tc->coords.line;
        double new_len = boundary->s1;
        pmLine9Cut(&prev_geom.line9, new_len, 0);
        break;
    case TC_CIRCULAR:
    {
        prev_geom.circle9 = prev_tc->coords.circle;
        pmCircle9Cut(&prev_geom.circle9, boundary->s1, 0);
        break;
    }
    default:
        return TP_ERR_FAIL;
    }

    switch (this_tc->motion_type) {
    case TC_LINEAR:
        this_geom.line9 = this_tc->coords.line;
        double new_len = boundary->s2;
        pmLine9Cut(&this_geom.line9, new_len, 1);
        break;
    case TC_CIRCULAR:
    {
        this_geom.circle9 = this_tc->coords.circle;
        pmCircle9Cut(&this_geom.circle9, boundary->s2, 1);
        break;
    }
    default:
        return TP_ERR_FAIL;
    }

    CHP(tpInitBlendArcFromAdjacent(
        tp,
        prev_tc,
        &blend_tc1,
        controls.v_req,
        fmin(param1.v_plan, fmax(param1.v_max_planar, param1.v_max_altitude)),
        param1.a_max_planar,
        blend_tc1.motion_type));
    CHP(tpInitBlendArcFromAdjacent(
        tp,
        this_tc,
        &blend_tc2,
        controls.v_req,
        fmin(param2.v_plan, fmax(param2.v_max_planar, param2.v_max_altitude)),
        param2.a_max_planar,
        blend_tc2.motion_type));

    // Passed all the pre-checks, ready to commit to changing the line segments
    if (prev_tc->motion_type == TC_LINEAR) {
        tcSetLine9(prev_tc, &prev_geom.line9);
    } else if (prev_tc->motion_type == TC_CIRCULAR) {
        tcSetCircle9(prev_tc, &prev_geom.circle9);
    }

    if (this_tc->motion_type == TC_LINEAR) {
        tcSetLine9(this_tc, &this_geom.line9);
    } else if (this_tc->motion_type == TC_CIRCULAR) {
        tcSetCircle9(this_tc, &this_geom.circle9);
    }

    // Update end conditions
    tcSetTermCond(prev_tc, TC_TERM_COND_TANGENT);
    tcSetTermCond(&blend_tc1,TC_TERM_COND_TANGENT);
    tcSetTermCond(&blend_tc2, TC_TERM_COND_TANGENT);

    // The DIO's only change if the previous segment is consumed
    blend_tc1.syncdio.anychanged &= param1.consume;
    // Second arc should follow DIO's of the following segment (since the midpoint is the closest to the ideal endpoint)
    // DIOs have already changed by the time the next segment starts, so ignore changes here
    this_tc->syncdio.anychanged = 0;

    if (_tp_debug || needConsistencyCheck(CCHECK_C1_CONTINUITY)) {
        checkBiarcContinuity(prev_tc, this_tc);
    }

#ifdef TP_DEBUG
    tp_debug_json5_log_start(tpCreateBiarcBlend);
    if (param1.consume) {
        tcGetStartTangentUnitVector(prev_tc, &u1_stretch);
        tcGetStartpoint(prev_tc, &p1_stretch);
    } else {
        tcGetEndTangentUnitVector(prev_tc, &u1_stretch);
        tcGetEndpoint(prev_tc, &p1_stretch);
    }
    tcGetStartTangentUnitVector(this_tc, &u2_stretch);
    tcGetStartpoint(this_tc, &p2_stretch);
    print_json5_double(prev_len);
    print_json5_double(this_len);
    print_json5_double_("prev_len_new", prev_tc->target);
    print_json5_double_("this_len_new", this_tc->target);
    print_json5_int(prev_can_consume);
    print_json5_int(this_can_consume);
    print_json5_double_("param_line_length", param1.line_length);
    print_json5_double_("line_length1", blend_tc1.coords.arc.line_length);
    print_json5_double_("line_length2", blend_tc2.coords.arc.line_length);
    print_json5_int(prev_motion_type);
    print_json5_int(this_motion_type);
    print_json5_long_long_("src_line", this_tc->tag.fields[GM_FIELD_LINE_NUMBER]);
    print_json5_long_long_("unique_id1", blend_tc1.unique_id);
    print_json5_long_long_("unique_id2", blend_tc2.unique_id);
    print_json5_biarc_solution_t("solution", &biarc_results.solution);
    print_json5_PmVector(u1_stretch);
    print_json5_PmVector(u2_stretch);
    print_json5_PmVector_("p1_stretch", p1_stretch);
    PmVector p11_blend;
    tcGetStartpoint(&blend_tc1, &p11_blend);
    PmVector p12_blend;
    tcGetEndpoint(&blend_tc1, &p12_blend);
    PmVector p21_blend;
    tcGetStartpoint(&blend_tc2, &p21_blend);
    PmVector p22_blend;
    tcGetEndpoint(&blend_tc2, &p22_blend);
    print_json5_PmVector_("p11_blend", p11_blend);
    print_json5_PmVector_("p12_blend", p12_blend);
    print_json5_PmVector_("p21_blend", p21_blend);
    print_json5_PmVector_("p22_blend", p22_blend);
    print_json5_PmVector_("p2_stretch", p2_stretch);
    print_json5_biarc_control_points_t("control_pts", &biarc_results.solution.control_pts);
    print_json5_blend_boundary_t("boundary", &biarc_results.solution.boundary);
    print_json5_int_("iterations", biarc_results.iterations);
    print_json5_string_("result", biarc_result_to_str(biarc_results.result));
    print_json5_TC_STRUCT_kinematics("blend1", &blend_tc1);
    print_json5_TC_STRUCT_kinematics("blend2", &blend_tc2);
    print_json5_double(maxvel);
    print_json5_double(reqvel);
    print_json5_bool_("consume", param1.consume);
    print_json5_SphericalArc9("arc1", &blend_tc1.coords.arc);
    print_json5_SphericalArc9("arc2", &blend_tc2.coords.arc);
    print_json5_end_();
#endif

    if (param1.consume) {
        if (tcqPopBack(&tp->queue)) {
            //This is unrecoverable since we've already changed the line. Something is wrong if we get here...
            tpStopWithError(tp, "Motion internal error: previous motion segment at line %d (id %d) is missing, trajectory is invalid", this_tc->tag.fields[GM_FIELD_LINE_NUMBER], this_tc->id);
            return TP_ERR_UNRECOVERABLE;
        }
    }

    return TP_ERR_OK;
}

/**
 * Add a newly created motion segment to the tp queue.
 * Returns an error code if the queue operation fails, otherwise adds a new
 * segment to the queue and updates the end point of the trajectory planner.
 */
int tpAddSegmentToQueue(TP_STRUCT * const tp, TC_STRUCT * const tc) {

    if (tcqPut(&tp->queue, tc) == -1) {
        tpStopWithError(tp, "Motion internal error: tcqPut() failed");
        return TP_ERR_FAIL;
    }

    // Store end of current move as new final goal of TP
    // KLUDGE: endpoint is garbage for rigid tap since it's supposed to retract past the start point.
    if (tc->motion_type != TC_RIGIDTAP) {
        tcGetEndpoint(tc, &tp->goalPos);
    }
    tp->done = 0;
    tp->depth = tcqLen(&tp->queue);
    //Fixing issue with duplicate id's?
#ifdef TP_DEBUG
    print_json5_log_start(Enqueue);
    print_json5_tc_id_data_(tc);
    print_json5_string_("motion_type", tcMotionTypeAsString(tc->motion_type));
    print_json5_double_("target", tc->target);
    print_json5_string_("term_cond", tcTermCondAsString(tc->term_cond));
    print_json5_end_();
#endif

    return TP_ERR_OK;
}

int handlePrevTermCondition(TC_STRUCT *prev_tc, TC_STRUCT *tc)
{
    if (!tc) {
        return TP_ERR_FAIL;
    }

    switch (tc->motion_type) {
    case TC_RIGIDTAP:
        if (prev_tc && prev_tc->motion_type != TC_RIGIDTAP) {
            tcSetTermCond(prev_tc, TC_TERM_COND_STOP);
        }
        // Rigidtap motions are always exact-path to allow pecking
        tcSetTermCond(tc, TC_TERM_COND_EXACT);
        break;
    case TC_LINEAR:
    case TC_CIRCULAR:
    case TC_SPHERICAL:
    {
        tc_term_cond_t prev_term = prev_tc ? prev_tc->term_cond : TC_TERM_COND_STOP;
        tcSetTermCond(prev_tc, prev_term);
    }
        break;
    case TC_DWELL:
        // Previous motion must do an exact stop at the dwell point since that's the whole point
        tcSetTermCond(tc, TC_TERM_COND_STOP);
        return TP_ERR_OK;
    }
    return TP_ERR_OK;
}

int handleModeChange(TC_STRUCT *prev_tc, TC_STRUCT *tc)
{
    if (!tc || !prev_tc) {
        return TP_ERR_FAIL;
    }

    // Can't blend across feed / rapid transitions
    if ((prev_tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE) ^
            (tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE)) {
        tp_debug_print("Blending disabled: rapid / feed transition must follow exact path\n");
        tcSetTermCond(prev_tc, TC_TERM_COND_EXACT);
        if (tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE) {
            // Force retraction moves to follow exact path (to avoid gouging after a hole retraction)
            tcSetTermCond(tc, TC_TERM_COND_EXACT);
        }
    }

    if (prev_tc->synchronized != TC_SYNC_POSITION &&
            tc->synchronized == TC_SYNC_POSITION) {
        tp_debug_print("Blending disabled: entering position-sync mode\n");
        // NOTE: blending IS allowed when exiting position sync mode, so be
        // careful of tolerances here to avoid making thicker threads
        // (particularly if ;the lead-out is perpendicular)
        return tcSetTermCond(prev_tc, TC_TERM_COND_STOP);
    }

    if(tc->atspeed) {
        // Need to wait for spindle before starting tc, so it's not safe to
        // allow any blending from the previous to current segment
        tp_debug_print("Blending disabled: waiting on spindle atspeed for tc %d, unique_id %llu\n",
                       tc->id, tc->unique_id);
        return tcSetTermCond(prev_tc, TC_TERM_COND_STOP);
    }

    return TP_ERR_OK;
}

int tpSetupSyncedIO(TP_STRUCT * const tp, TC_STRUCT * const tc) {
    if (tp->syncdio.anychanged != 0) {
        tc->syncdio = tp->syncdio; //enqueue the list of DIOs that need toggling
        tpClearDIOs(tp); // clear out the list, in order to prepare for the next time we need to use it
        return TP_ERR_OK;
    } else {
        tc->syncdio.anychanged = 0;
        return TP_ERR_NO_ACTION;
    }
}

tp_err_t tpFinalizeAndEnqueue(TP_STRUCT * const tp, TC_STRUCT * const tc, PmVector const *nominal_goal)
{
    //TODO refactor this into its own function
    TC_STRUCT *prev_tc;
    prev_tc = tcqLast(&tp->queue);

    // Make sure the blending flags are consistent w/ previous segment
    handlePrevTermCondition(prev_tc, tc);

    // Prevent blends for specific mode changes (where blending isn't possible anyway)
    handleModeChange(prev_tc, tc);

    CHP(tpSetupSegmentBlend(tp, prev_tc, tc));
    // KLUDGE order is important here, the parabolic blend check has to
    // happen after all other steps that affect the terminal condition
    CHP(tcFinalizeLength(prev_tc, emcmotConfig->maxFeedScale));

    CHP(tpAddSegmentToQueue(tp, tc));
    //Run speed optimization (will abort safely if there are no tangent segments)
    tpRunOptimization(tp);
    if (nominal_goal) {
        // Update the goal position so that future motions know what their planned start point is
        tp->goalPos = *nominal_goal;
    }

    return TP_ERR_OK;
}

void clearBlendStructs()
{
    memset(&blend_tc1, 0, sizeof(TC_STRUCT));
    memset(&blend_tc2, 0, sizeof(TC_STRUCT));
}

static void addRigidTapOverrun(TC_STRUCT * const tc, double revolutions, double uu_per_rev)
{
    PmCartLine *actual_xyz = &tc->coords.rigidtap.actual_xyz;
    pmCartLineStretch(actual_xyz, actual_xyz->tmag + revolutions * uu_per_rev, 0);
    tc->target = actual_xyz->tmag;
}

/**
 * Adds a rigid tap cycle to the motion queue.
 */
int tpAddRigidTap(
    TP_STRUCT * const tp,
    EmcPose end_p,
    double tap_uu_per_rev,
    double retract_uu_per_rev,
    double ini_maxvel,
    double acc,
    unsigned char enables,
    char atspeed,
    double scale,
    struct state_tag_t tag) {
    if (tpErrorCheck(tp)) {
        return TP_ERR_FAIL;
    }

    PmVector end;
    emcPoseToPmVector(&end_p, &end);

    tp_info_print("== AddRigidTap ==\n");

    if(!tp->synchronized) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Cannot add unsynchronized rigid tap move.\n");
        return TP_ERR_FAIL;
    }

    TC_STRUCT tc = {};
    clearBlendStructs();

    tcSetId(tp, &tc, tag);
    /* Initialize rigid tap move.
     * NOTE: rigid tapping does not have a canonical type.
     * NOTE: always need atspeed since this is a synchronized movement.
     * */
    tcInit(&tc,
            TC_RIGIDTAP,
            0,
            tp->cycleTime,
            enables,
            atspeed);

    // Setup any synced IO for this move
    tpSetupSyncedIO(tp, &tc);

    // Copy over state data from the trajectory planner
    tcSetupState(&tc, tp);

    // Copy in motion parameters
    tcSetupMotion(&tc,
            ini_maxvel,
            ini_maxvel,
            acc);

    tc.coords.rigidtap.tap_uu_per_rev = tap_uu_per_rev;
    tc.coords.rigidtap.retract_uu_per_rev = retract_uu_per_rev;

    // Setup rigid tap geometry
    CHP(pmRigidTapInit(
        &tc.coords.rigidtap,
        &tp->goalPos,
        &end,
        scale));

    addRigidTapOverrun(&tc, RIGIDTAP_MAX_OVERSHOOT_REVS, tc.coords.rigidtap.tap_uu_per_rev);

    // Do NOT update the goal position with a rigid tap move
    return tpFinalizeAndEnqueue(tp, &tc, NULL);
}

int tpAddDwell(TP_STRUCT * const tp, double time_sec, double delta_rpm, struct state_tag_t tag)
{
    // NOTE: it's not possible to dwell for less than 1 servo timestep due to how dwells are implemented
    TC_STRUCT tc={};
    tcInit(&tc,
            TC_DWELL,
            0,
            tp->cycleTime,
            0,
            0);
    tc.motion_type = TC_DWELL;
    tc.coords.dwell.dwell_time_req = fmax(time_sec, tp->cycleTime);
    tc.coords.dwell.delta_rpm_req = fabs(delta_rpm);
    tc.coords.dwell.dwell_pos = tp->goalPos;
    // Initialize with invalid values (dwell time computed at segment activation)
    tc.coords.dwell.dwell_time = -1;
    tc.coords.dwell.remaining_time = -1;
    tc.term_cond = TC_TERM_COND_STOP;
    tc.tag = tag;
    tpAddSegmentToQueue(tp, &tc);

    return TP_ERR_OK;
}


static double applyKinkVelLimit(TC_STRUCT const * const tc, double vel_in)
{
    if (tc->use_kink && tc->kink_vel >= 0  && tc->term_cond == TC_TERM_COND_TANGENT) {
        // Only care about kink_vel with tangent segments that have not been arc blended
        return fmin(vel_in, tc->kink_vel);
    }
    return vel_in;
}

/**
 * Based on the nth and (n-1)th segment, find a safe final velocity for the (n-1)th segment.
 * This function also caps the target velocity if velocity ramping is enabled. If we
 * don't do this, then the linear segments (with higher tangential
 * acceleration) will speed up and slow down to reach their target velocity,
 * creating "humps" in the velocity profile.
 */
int tpComputeOptimalVelocity(TC_STRUCT * const tc, TC_STRUCT * const prev1_tc) {
    //Calculate the maximum starting velocity vs_back of segment tc, given the
    //trajectory parameters
    double acc_this = tcGetTangentialMaxAccel(tc);

    // Find the reachable velocity of tc, moving backwards in time
    double vs_back = pmSqrt(pmSq(tc->finalvel) + 2.0 * acc_this * tc->target);
    // Find the reachable velocity of prev1_tc, moving forwards in time

    double vf_limit_this = tcGetPlanMaxTargetVel(tc, emcmotConfig->maxFeedScale);
    double v_max_prev = tcGetPlanMaxTargetVel(prev1_tc, emcmotConfig->maxFeedScale);
    double vf_limit_prev = applyKinkVelLimit(prev1_tc, v_max_prev);
    //Limit the PREVIOUS velocity by how much we can overshoot into
    double vf_limit = fmin(vf_limit_this, vf_limit_prev);

    if (vs_back >= vf_limit ) {
        //If we've hit the requested velocity, then prev_tc is definitely a "peak"
        vs_back = vf_limit;
        prev1_tc->optimization_state = TC_OPTIM_AT_MAX;
        tp_debug_print("found peak due to v_limit %f\n", vf_limit);
    }

    //Limit tc's target velocity to avoid creating "humps" in the velocity profile
    prev1_tc->finalvel = vs_back;

    tp_info_print(" prev1_tc-> fv = %f, tc->fv = %f\n",
            prev1_tc->finalvel, tc->finalvel);

    return TP_ERR_OK;
}


/**
 * Do "rising tide" optimization to find allowable final velocities for each queued segment.
 * Walk along the queue from the back to the front. Based on the "current"
 * segment's final velocity, calculate the previous segment's maximum allowable
 * final velocity. The depth we walk along the queue is controlled by the
 * TP_LOOKAHEAD_DEPTH constant for now. The process safetly aborts early due to
 * a short queue or other conflicts.
 */
int tpRunOptimization(TP_STRUCT * const tp) {
    // Pointers to the "current", previous, and 2nd previous trajectory
    // components. Current in this context means the segment being optimized,
    // NOT the currently excecuting segment.

    TC_STRUCT *tc;
    TC_STRUCT *prev1_tc;

    int ind, x;
    int len = tcqLen(&tp->queue);
    //TODO make lookahead depth configurable from the INI file

    int hit_peaks = 0;
    // Flag that says we've hit at least 1 non-tangent segment
    bool hit_non_tangent = false;

    /* Starting at the 2nd to last element in the queue, work backwards towards
     * the front. We can't do anything with the very last element because its
     * length may change if a new line is added to the queue.*/

    for (x = 1; x < emcmotConfig->arc_blend_cfg.optimization_depth + 2; ++x) {
        tp_info_print("==== Optimization step %d ====\n",x);

        // Update the pointers to the trajectory segments in use
        ind = len-x;
        tc = tcqItem(&tp->queue, ind);
        prev1_tc = tcqItem(&tp->queue, ind-1);

        if ( !prev1_tc || !tc) {
            tp_debug_print(" Reached end of queue in optimization\n");
            return TP_ERR_OK;
        }

        // stop optimizing if we hit a non-tangent segment (final velocity
        // stays zero)
        if (prev1_tc->term_cond != TC_TERM_COND_TANGENT) {
            if (hit_non_tangent) {
                // 2 or more non-tangent segments means we're past where the optimizer can help
                tp_debug_print("Found 2nd non-tangent segment, stopping optimization\n");
                return TP_ERR_OK;
            } else  {
                tp_debug_print("Found first non-tangent segment, contining\n");
                hit_non_tangent = true;
                continue;
            }
        }

        double progress_ratio = prev1_tc->progress / prev1_tc->target;
        // can safely decelerate to halfway point of segment from 25% of segment
        double cutoff_ratio = BLEND_DIST_FRACTION / 2.0;

        if (progress_ratio >= cutoff_ratio) {
            tp_debug_print("segment %d has moved past %f percent progress, cannot blend safely!\n",
                    ind-1, cutoff_ratio * 100.0);
            return TP_ERR_OK;
        }

        //Somewhat pedantic check for other conditions that would make blending unsafe
        if (prev1_tc->splitting) {
            tp_debug_print("segment %d is already blending, cannot optimize safely!\n",
                    ind-1);
            return TP_ERR_OK;
        }

        tp_info_print("  current term = %u, type = %u, id = %u, accel_mode = %d\n",
                tc->term_cond, tc->motion_type, tc->id, tc->accel_mode);
        tp_info_print("  prev term = %u, type = %u, id = %u, accel_mode = %d\n",
                prev1_tc->term_cond, prev1_tc->motion_type, prev1_tc->id, prev1_tc->accel_mode);

        if (tc->atspeed) {
            //Assume worst case that we have a stop at this point. This may cause a
            //slight hiccup, but the alternative is a sudden hard stop.
            tp_debug_print("Found atspeed at segment %lld\n", tc->unique_id);
            prev1_tc->finalvel = 0.0;
        } else if (tc->finalized) {
            tpComputeOptimalVelocity(tc, prev1_tc);
        } else {
            tp_debug_print("Segment %lld, type %d not finalized, continuing\n", tc->unique_id, tc->motion_type);
            // use worst-case final velocity that allows for up to 1/2 of a segment to be consumed.

            prev1_tc->finalvel = calculateOptimizationInitialVel(prev1_tc, tc);

            // Fixes acceleration violations when last segment is not finalized, and previous segment is tangent.

            tc->finalvel = 0.0;
            prev1_tc->finalvel = applyKinkVelLimit(prev1_tc, prev1_tc->finalvel);

        }

        tc->active_depth = x - 2 - hit_peaks;
#ifdef TP_OPTIMIZATION_LAZY
        if (tc->optimization_state == TC_OPTIM_AT_MAX) {
            hit_peaks++;
        }
        if (hit_peaks > TP_OPTIMIZATION_CUTOFF) {
            return TP_ERR_OK;
        }
#endif

    }
    tp_debug_print("Reached optimization depth limit\n");
    return TP_ERR_OK;
}


/**
 * Check for tangency between the current segment and previous segment.
 * If the current and previous segment are tangent, then flag the previous
 * segment as tangent, and limit the current segment's velocity by the sampling
 * rate.
 */
TCIntersectType tpSetupTangent(TP_STRUCT const * const tp,
        TC_STRUCT * const prev_tc, TC_STRUCT * const tc) {
    tp_debug_json5_log_start(tpSetupTangent);
    if (!tc || !prev_tc) {
        tp_debug_json5_log_end("missing tc or prev tc in tangent check");
        return TC_INTERSECT_INCOMPATIBLE;
    }
    if (emcmotConfig->arc_blend_cfg.optimization_depth < 2) {
        tp_debug_json5_log_end("Optimization depth %d too low for tangent optimization",
                emcmotConfig->arc_blend_cfg.optimization_depth);
        return TC_INTERSECT_INCOMPATIBLE;
    }

    if (prev_tc->term_cond == TC_TERM_COND_STOP) {
        tp_debug_json5_log_end("Found exact stop condition");
        return TC_INTERSECT_INCOMPATIBLE;
    }

    if (prev_tc->indexrotary != INDEX_NONE || tc->indexrotary != INDEX_NONE) {
        tp_debug_json5_log_end("rotary axis move requires indexing");
        tcSetTermCond(prev_tc, TC_TERM_COND_STOP);
        return TC_INTERSECT_INCOMPATIBLE;
    }

    // TODO see if this can go after the tangent check
    if (prev_tc->progress > prev_tc->target / 2.0) {
        tp_debug_json5_log_end(" prev_tc progress (%f) is too large, aborting blend arc\n", prev_tc->progress);
        return TC_INTERSECT_INCOMPATIBLE;
    }
    PmVector prev_tan, this_tan;

    CHP(tcGetEndTangentUnitVector(prev_tc, &prev_tan));
    CHP(tcGetStartTangentUnitVector(tc, &this_tan));

    tp_debug_json5_PmVector(prev_tan);
    tp_debug_json5_PmVector(this_tan);

    // Assume small angle approximation here
    const double SHARP_CORNER_DEG = 2.0;
    const double SHARP_CORNER_EPSILON = pmSq(PM_PI * ( SHARP_CORNER_DEG / 180.0));

    if (VecVecUnitAntiParallel(&prev_tan, &this_tan, SHARP_CORNER_EPSILON))
    {
        tp_debug_json5_log_end("Found sharp corner");
        tcSetTermCond(prev_tc, TC_TERM_COND_STOP);
        return TC_INTERSECT_INCOMPATIBLE;
    }

    // Calculate instantaneous acceleration required for change in direction
    // from v1 to v2, assuming constant speed
    double v_max1 = tcGetPlanMaxTargetVel(prev_tc, emcmotConfig->maxFeedScale);
    double v_max2 = tcGetPlanMaxTargetVel(tc, emcmotConfig->maxFeedScale);
    // Note that this is a minimum since the velocity at the intersection must
    // be the slower of the two segments not to violate constraints.
    double v_max_tangent = fmin(v_max1, v_max2);
    tp_debug_json5_double(v_max_tangent);

    // Account for acceleration past final velocity during a split cycle
    // (e.g. next segment starts accelerating again so the average velocity is higher at the end of the split cycle)
    double a_inst = v_max_tangent / tp->cycleTime + tc->maxaccel;

    // Set up worst-case final velocity
    // Compute the actual magnitude of acceleration required given the tangent directions
    // Do this by assuming that we decelerate to a stop on the previous segment,
    // and simultaneously accelerate up to the maximum speed on the next one.
    PmVector acc1, acc2, acc_diff;
    VecScalMult(&prev_tan, a_inst, &acc1);
    VecScalMult(&this_tan, a_inst, &acc2);
    VecVecSub(&acc2, &acc1, &acc_diff);

    tp_debug_json5_PmVector(acc1);
    tp_debug_json5_PmVector(acc2);
    tp_debug_json5_PmVector(acc_diff);

    PmVector acc_bound = getAccelBounds();

    PmVector acc_scales;
    findAccelScale(&acc_diff, &acc_bound, &acc_scales);
    double acc_scale_max = VecAbsMax(&acc_scales);

    if (prev_tc->motion_type == TC_CIRCULAR || tc->motion_type == TC_CIRCULAR) {
        acc_scale_max /= BLEND_ACC_RATIO_TANGENTIAL;
    }
    tp_debug_json5_double(acc_scale_max);

    // Controls the tradeoff between reduction of final velocity, and reduction of allowed segment acceleration
    // TODO: this should ideally depend on some function of segment length and acceleration for better optimization
    const double kink_ratio = tpGetTangentKinkRatio();

    tp_debug_json5_double(kink_ratio);

    if (acc_scale_max < kink_ratio) {
        tp_debug_json5_log_end(" Kink acceleration within %g, using tangent blend", kink_ratio);
        tcSetTermCond(prev_tc, TC_TERM_COND_TANGENT);
        tcSetKinkProperties(prev_tc, tc, v_max_tangent, acc_scale_max);
        return TC_INTERSECT_TANGENT;
    } else {
        switch (prev_tc->term_cond) {
        case TC_TERM_COND_STOP:
        case TC_TERM_COND_EXACT:
            tp_debug_print(" Corner too sharp for exact-path, forcing exact stop ");
            return TC_INTERSECT_INCOMPATIBLE;
        case TC_TERM_COND_PARABOLIC:
        case TC_TERM_COND_TANGENT:
            break;
        }
    }
    if (prev_tc->term_cond == TC_TERM_COND_EXACT) {
        tp_debug_json5_log_end("Found exact path condition with non-tangent intersection");
        return  TC_INTERSECT_INCOMPATIBLE;
    }

    tcSetKinkProperties(prev_tc, tc, v_max_tangent * kink_ratio / acc_scale_max, kink_ratio);

#ifdef TP_DEBUG
    print_json5_double_field(prev_tc, kink_vel);
#endif

    if (!emcmotConfig->arc_blend_cfg.enable) {
        tp_debug_json5_log_end("Arc blending disabled");
        return  TC_INTERSECT_INCOMPATIBLE;
    }

    if (tc->finalized || prev_tc->finalized) {
        tp_debug_json5_log_end("Can't create blend when segment lengths are finalized");
        return TC_INTERSECT_INCOMPATIBLE;
    }

    tp_debug_json5_log_end("Nontangent intersection needs blend");

    // NOTE: acceleration will be reduced later if tangent blend is used
    return TC_INTERSECT_NONTANGENT;
}

/**
 * Handle creating a blend arc when a new line segment is about to enter the queue.
 * This function handles the checks, setup, and calculations for creating a new
 * blend arc. Essentially all of the blend arc functions are called through
 * here to isolate the process.
 */
int tpSetupSegmentBlend(
    TP_STRUCT * const tp,
    TC_STRUCT * const prev_tc,
    TC_STRUCT * const tc) {

    // Check the intersection type and handle tangency if the two segments are very nearly tangent
    TCIntersectType res_tan = tpSetupTangent(tp, prev_tc, tc);

    int blends_added = 0;
    if (res_tan == TC_INTERSECT_NONTANGENT) {
        blends_added = tpCreateBiarcBlend(tp, prev_tc, tc) == TP_ERR_OK ? 2 : 0;
    }

    switch (blends_added) {
    case 0:
        tpChooseBestBlend(prev_tc, tc, 0.0);
        return TP_ERR_OK;
    case 1:
        CHP(tpAddSegmentToQueue(tp, &blend_tc1));
        return TP_ERR_OK;
    case 2:
        CHP(tpAddSegmentToQueue(tp, &blend_tc1));
        CHP(tpAddSegmentToQueue(tp, &blend_tc2));
        return TP_ERR_OK;
    }
    return TP_ERR_FAIL;
}

/**
 * Add a straight line to the tc queue.
 * end of the previous move to the new end specified here at the
 * currently-active accel and vel settings from the tp struct.
 */
int tpAddLine(
    TP_STRUCT * const tp,
    EmcPose end_p,
    EMCMotionTypes canon_motion_type,
    double vel,
    double ini_maxvel,
    double acc,
    unsigned char enables,
    char atspeed,
    IndexRotaryAxis indexrotary,
    struct state_tag_t tag)
{
    PmVector end;
    emcPoseToPmVector(&end_p, &end);
    if (tpErrorCheck(tp) < 0) {
        return TP_ERR_FAIL;
    }
    // Initialize new tc struct for the line segment
    TC_STRUCT tc = {};

    clearBlendStructs();
    tcSetId(tp, &tc, tag);

    tcInit(&tc,
            TC_LINEAR,
            canon_motion_type,
            tp->cycleTime,
            enables,
            atspeed);

    // Setup any synced IO for this move
    tpSetupSyncedIO(tp, &tc);

    // Copy over state data from the trajectory planner
    tcSetupState(&tc, tp);
    tcSetTermCond(&tc, tp->termCond);

    // Copy in motion parameters
    tcSetupMotion(&tc,
            vel,
            ini_maxvel,
            acc);

    // Setup line geometry
    pmLine9Init(
        &tc.coords.line,
        &tp->goalPos,
        &end);
    tc.target = pmLine9Length(&tc.coords.line);
    tc.nominal_length = tc.target;
    tc.indexrotary = indexrotary;

#ifdef TP_DEBUG
    {
        // macros use the variable name, need a plain name to please the JSON5 parser
        print_json5_log_start(tpAddLine);
        print_json5_tc_id_data_(&tc);
        print_json5_PmVector_("start", tp->goalPos);
        print_json5_PmVector(end);
        print_json5_double(vel);
        print_json5_double(ini_maxvel);
        print_json5_double(acc);
        print_json5_unsigned(enables);
        print_json5_long(indexrotary);
        print_json5_long(atspeed);
        print_json5_long(canon_motion_type);
        PmVector delta = tp->goalPos;
        VecVecSub(&end, &tp->goalPos, &delta);
        print_json5_PmVector(delta);
        print_json5_PmLine9_("line", tc.coords.line);
        print_json5_log_end();
    }
#endif

    if (tc.target < TP_POS_EPSILON) {
        rtapi_print_msg(RTAPI_MSG_DBG,"failed to create line id %d, zero-length segment\n",tp->nextId);
        return TP_ERR_ZERO_LENGTH;
    } else {
        return tpFinalizeAndEnqueue(tp, &tc, &end);
    }
}


/**
 * Adds a circular (circle, arc, helix) move from the end of the
 * last move to this new position.
 *
 * @param end is the xyz/abc point of the destination.
 *
 * see pmCircleInit for further details on how arcs are specified. Note that
 * degenerate arcs/circles are not allowed. We are guaranteed to have a move in
 * xyz so the target is always the circle/arc/helical length.
 */
int tpAddCircle(TP_STRUCT * const tp,
        EmcPose end_p,
        PmCartesian center,
        PmCartesian normal,
        int turn,
        double expected_angle_rad,
        EMCMotionTypes canon_motion_type,
        double vel,
        double ini_maxvel,
        double acc,
        double acc_normal,
        unsigned char enables,
        char atspeed,
        struct state_tag_t tag)
{
    if (tpErrorCheck(tp)<0) {
        return TP_ERR_FAIL;
    }
    PmVector end;
    emcPoseToPmVector(&end_p, &end);

    TC_STRUCT tc = {};

    clearBlendStructs();

    tcSetId(tp, &tc, tag);

    tcInit(&tc,
            TC_CIRCULAR,
            canon_motion_type,
            tp->cycleTime,
            enables,
            atspeed);
    // Setup any synced IO for this move
    tpSetupSyncedIO(tp, &tc);

    // Copy over state data from the trajectory planner
    tcSetupState(&tc, tp);
    tcSetTermCond(&tc, tp->termCond);

    // Setup circle geometry
    int res_init = pmCircle9Init(&tc.coords.circle,
            &tp->goalPos,
            &end,
            &center,
            &normal,
            turn,
            expected_angle_rad);

    if (res_init) return res_init;

    tc.acc_normal_max = acc_normal;

    // Copy in motion parameters
    tcSetupMotion(&tc,
            vel,
            ini_maxvel,
            acc);

    pmCircle9LengthAndRatios(&tc.coords.circle);
    // Update tc target with existing circular segment
    tc.target = tc.coords.circle.total_length;
    tc.nominal_length = tc.target;

#ifdef TP_DEBUG
    {
        // macros use the variable name, need a plain name to please the JSON5 parser
        print_json5_log_start(tpAddCircle);
        print_json5_tc_id_data_(&tc);
        print_json5_PmVector_("start", tp->goalPos);
        print_json5_PmVector(end);
        print_json5_PmCartesian(center);
        print_json5_PmCartesian(normal);
        print_json5_long(turn);
        print_json5_double(expected_angle_rad);
        print_json5_double(vel);
        print_json5_double(ini_maxvel);
        print_json5_double(acc);
        print_json5_double(acc_normal);
        print_json5_unsigned(enables);
        print_json5_long(atspeed);
        print_json5_long(canon_motion_type);
        PmVector delta = tp->goalPos;
        VecVecSub(&end, &tp->goalPos, &delta);
        print_json5_PmVector(delta);
        print_json5_PmCircle_("xyz_circle", tc.coords.circle.xyz);
        print_json5_log_end();
    }
#endif

    if (tc.target < TP_POS_EPSILON) {
        return TP_ERR_ZERO_LENGTH;
    } else {
        return tpFinalizeAndEnqueue(tp, &tc, &end);
    }
}

/**
 * Calculate distance update from velocity and acceleration.
 */
int tcUpdateDistFromAccel(TC_STRUCT * const tc, double acc, double vel_desired, int reverse_run)
{
    // If the resulting velocity is less than zero, than we're done. This
    // causes a small overshoot, but in practice it is very small.
    double v_next = tc->currentvel + acc * tc->cycle_time;
    // update position in this tc using trapezoidal integration
    // Note that progress can be greater than the target after this step.
    if (v_next < 0.0) {
        v_next = 0.0;
        //KLUDGE: the trapezoidal planner undershoots by half a cycle time, so
        //forcing the endpoint here is necessary. However, velocity undershoot
        //also occurs during pausing and stopping, which can happen far from
        //the end. If we could "cruise" to the endpoint within a cycle at our
        //current speed, then assume that we want to be at the end.
        if (tcGetDistanceToGo(tc,reverse_run) < (tc->currentvel *  tc->cycle_time)) {
            tc->progress = tcGetTarget(tc,reverse_run);
        }
    } else {
        double displacement = (v_next + tc->currentvel) * 0.5 * tc->cycle_time;
        // Account for reverse run (flip sign if need be)
        double disp_sign = reverse_run ? -1 : 1;
        tc->progress += (disp_sign * displacement);

        //Progress has to be within the allowable range
        tc->progress = bound(tc->progress, tc->target, 0.0);
    }
    tc->currentvel = v_next;

    // Check if we can make the desired velocity
    tc->on_final_decel = (fabs(vel_desired - tc->currentvel) < TP_VEL_EPSILON) && (acc < 0.0);

    return TP_ERR_OK;
}

const char *cycleModeToString(UpdateCycleMode mode)
{
    switch (mode) {
    case UPDATE_NORMAL:
        return "normal";
    case UPDATE_PARABOLIC_BLEND:
        return "parabolic_blend";
    case UPDATE_SPLIT:
        return "split_cycle";
    }
    return "unknown";
}

void tpDebugCycleInfo(TP_STRUCT const * const tp, TC_STRUCT const * const tc, TC_STRUCT const * const nexttc, double acc, int accel_mode, UpdateCycleMode cycle) {
#ifdef TC_DEBUG
    // Find maximum allowed velocity from feed and machine limits
    const double v_target = tpGetRealAbsTargetVel(tp, tc);
    // Store a copy of final velocity
    const double v_final = tpGetRealFinalVel(tp, tc, nexttc);
    const double v_max = tpGetRealMaxTargetVel(tp, tc);

    /* Debug Output (inserted into caller's output)*/
    print_json5_long_long_("time_ticks", tp->time_elapsed_ticks);
    print_json5_tc_id_data_(tc);
    print_json5_string_("motion_type", tcMotionTypeAsString(tc->motion_type));
    print_json5_double(v_target);
    print_json5_double(v_final);
    print_json5_double(v_max);
    print_json5_double_("target", tcGetTarget(tc, tp->reverse_run));
    print_json5_double_("distance_to_go", tcGetDistanceToGo(tc, tp->reverse_run));
    print_json5_double_("v_current", tc->currentvel);
    print_json5_double_("a_current", acc);
    print_json5_double_("feed_scale", tpGetRealAbsFeedScale(tp, tc));
    print_json5_double_("dt", tc->cycle_time);
    print_json5_bool_("reverse_run", tp->reverse_run);
    print_json5_string_("term_cond", tcTermCondAsString((tc_term_cond_t)tc->term_cond));
    print_json5_bool_("final_decel", tc->on_final_decel);
    print_json5_bool_("need_split", tc->splitting);
    print_json5_long_("canon_type", tc->canon_motion_type);
    print_json5_string_("accel_mode",accel_mode ? "ramp" : "trapezoidal");
    print_json5_string_("cycle", cycleModeToString(cycle));
#endif
}

/**
 * Compute updated position and velocity for a timestep based on a trapezoidal
 * motion profile.
 * @param tc trajectory segment being processed.
 *
 * Creates the trapezoidal velocity profile based on the segment's velocity and
 * acceleration limits. The formula has been tweaked slightly to allow a
 * non-zero velocity at the instant the target is reached.
 */
void tpCalculateTrapezoidalAccel(TP_STRUCT const * const tp, TC_STRUCT * const tc, TC_STRUCT const * const nexttc,
        double * const acc, double * const vel_desired)
{
    // Find maximum allowed velocity from feed and machine limits
    double tc_target_vel = tpGetRealAbsTargetVel(tp, tc);
    // Store a copy of final velocity
    double tc_finalvel = tpGetRealFinalVel(tp, tc, nexttc);

#ifdef TP_PEDANTIC
    if (tc_finalvel > 0.0 && tc->term_cond != TC_TERM_COND_TANGENT) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Final velocity of %f with non-tangent segment!\n",tc_finalvel);
        tc_finalvel = 0.0;
    }
#endif

    /* Calculations for desired velocity based on trapezoidal profile */
    double dx = tcGetDistanceToGo(tc, tp->reverse_run);
    double maxaccel = tcGetTangentialMaxAccel(tc);

    double maxnewvel = findTrapezoidalDesiredVel(
		maxaccel, dx, tc_finalvel, tc->currentvel, tc->cycle_time);

    // Find bounded new velocity based on target velocity
    // Note that we use a separate variable later to check if we're on final decel
    double newvel = saturate(maxnewvel, tc_target_vel);

    // Calculate acceleration needed to reach newvel, bounded by machine maximum
    double dt = fmax(tc->cycle_time, TP_TIME_EPSILON);
    double maxnewaccel = (newvel - tc->currentvel) / dt;
    *acc = saturate(maxnewaccel, maxaccel);
    *vel_desired = maxnewvel;
}

/**
 * Calculate "ramp" acceleration for a cycle.
 */
int tpCalculateRampAccel(TP_STRUCT const * const tp,
        TC_STRUCT * const tc,
        TC_STRUCT const * const nexttc,
        double * const acc,
        double * const vel_desired)
{
    // displacement remaining in this segment
    double dx = tcGetDistanceToGo(tc, tp->reverse_run);

    double vel_final = tpGetRealFinalVel(tp, tc, nexttc);

    /* Check if the final velocity is too low to properly ramp up.*/
    if (vel_final < TP_VEL_EPSILON) {
        tp_debug_print(" vel_final %f too low for velocity ramping\n", vel_final);
        return TP_ERR_FAIL;
    }

    double vel_avg = (tc->currentvel + vel_final) / 2.0;

    // Calculate time remaining in this segment assuming constant acceleration
    double dt = 1e-16;
    if (vel_avg > TP_VEL_EPSILON) {
        dt = fmax( dx / vel_avg, 1e-16);
    }

    // Calculate velocity change between final and current velocity
    double dv = vel_final - tc->currentvel;

    // Estimate constant acceleration required
    double acc_final = dv / dt;

    // Saturate estimated acceleration against maximum allowed by segment
    double acc_max = tcGetTangentialMaxAccel(tc);

    // Output acceleration and velocity for position update
    *acc = saturate(acc_final, acc_max);
    *vel_desired = vel_final;

    return TP_ERR_OK;
}

void tpToggleDIOs(TC_STRUCT * const tc) {

    int i=0;
    if (tc->syncdio.anychanged != 0) { // we have DIO's to turn on or off
        for (i=0; i < num_dio; i++) {
            if (!(tc->syncdio.dio_mask & (1 << i))) continue;
            if (tc->syncdio.dios[i] > 0) emcmotDigitalOutWrite(i, 1); // turn DIO[i] on
            if (tc->syncdio.dios[i] < 0) emcmotDigitalOutWrite(i, 0); // turn DIO[i] off
        }
        for (i=0; i < num_aio; i++) {
            if (!(tc->syncdio.aio_mask & (1 << i))) continue;
            emcmotAioWrite(i, tc->syncdio.aios[i]); // set AIO[i]
        }
        tc->syncdio.anychanged = 0; //we have turned them all on/off, nothing else to do for this TC the next time
    }
}
/**
 * The displacement is always computed with respect to a specie
 */
double findSpindleDisplacement(
        double new_pos,
        spindle_origin_t origin
        )
{
    return origin.direction * (new_pos - origin.position);
}

double findSpindleVelocity(
        double spindle_velocity,
        spindle_origin_t origin
        )
{
    return origin.direction * spindle_velocity;
}

/**
 * Helper function to compare commanded and actual spindle velocity.
 * If the signs of velocity don't match, then the spindle is reversing direction.
 */
bool spindleReversed(spindle_origin_t origin, double prev_pos, double current_pos)
{
    return origin.direction * (current_pos - prev_pos) < 0;
}

/**
 * Commands a spindle reverse during rigid tapping.
 * Does not affect the commanded spindle speed,
 */
void cmdReverseSpindle(double reversal_scale)
{
    // Flip sign on commanded velocity
    emcmotStatus->spindle_cmd.velocity_rpm_out *= -1 * reversal_scale;

}

/**
 * Safely reverses rigid tap motion towards the starting point, preserving the existing tracking error.
 * @note extra motion distance for overrun must be added separately
 */
static inline void reverseRigidTapMotion(TC_STRUCT * const tc,
                                  spindle_origin_t * const spindle_origin)
{
    // we've stopped, so set a new target at the original position
    PmCartesian start, end;
    PmCartLine *actual_xyz = &tc->coords.rigidtap.actual_xyz;

    // Set a new spindle origin at the approximate reversal point, and keep the current tracking error as the new offset
    updateSpindlePositionFromProgress(spindle_origin, tc);

    pmCartLinePoint(&tc->coords.rigidtap.actual_xyz, tc->progress, &start);
    end = tc->coords.rigidtap.nominal_xyz.start;
    pmCartLineInit(actual_xyz, &start, &end);
    tc->coords.rigidtap.reversal_target = tc->target = actual_xyz->tmag;
    // NOTE: reset both progress and sync location:
    // At the point of reversal, the spindle is already synchronized, so
    // store the current position tracking error (in user units) as the sync offset
    // This way any accumulated error is not forgotten during the retraction
    tc->progress = 0.0;
}

double estimate_rigidtap_decel_distance(double vel, double uu_per_rev)
{
    double cmd_latency_dist = fmax(vel * emcmotStatus->spindle_cmd_latency_sec, 0.);
    double decel_distance = emcmotStatus->spindle_max_acceleration_rps2 > 0.0 ? pmSq(vel) / (2.0 * fabs(uu_per_rev) * emcmotStatus->spindle_max_acceleration_rps2) : 0.0;
    return cmd_latency_dist + decel_distance;
}

/**
 * Handle special cases for rigid tapping.
 * This function deals with updating the goal position and spindle position
 * during a rigid tap cycle. In particular, the target and spindle goal need to
 * be carefully handled since we're reversing direction.
 */
void tpUpdateRigidTapState(
    TP_STRUCT * const tp,
    TC_STRUCT * const tc,
    TC_STRUCT * const nexttc)
{
    static double old_spindle_pos = 0.0;
    double spindle_pos = emcmotStatus->spindle_fb.position_rev;
    static double retract_allowance = 0.0;

    rigid_tap_state_t const initial_state = tc->coords.rigidtap.state;
    switch (initial_state) {
        case RIGIDTAP_INACTIVE:
            return;
        case RIGIDTAP_TAPPING:
        {
            tc->uu_per_rev = tc->coords.rigidtap.tap_uu_per_rev;
            double decel_distance = estimate_rigidtap_decel_distance(tc->currentvel, tc->uu_per_rev);
            double reversal_target = tc->coords.rigidtap.reversal_target - decel_distance;
            if (tc->progress >= reversal_target) {
                // command reversal to stop / reverse at the bottom of the hole
                emcmotStatus->rigid_tap_reversal_vel_rps = emcmotStatus->spindle_fb.velocity_rps;
                cmdReverseSpindle(tc->coords.rigidtap.reversal_scale);
                tc->coords.rigidtap.state = RIGIDTAP_REVERSING;
                retract_allowance = estimate_rigidtap_decel_distance(tc->currentvel, tc->coords.rigidtap.retract_uu_per_rev);
            }
            break;
        }
        case RIGIDTAP_REVERSING:
            if (spindleReversed(tp->spindle.origin, old_spindle_pos, spindle_pos) && tc->currentvel <= 0.0) {
                emcmotStatus->rigid_tap_overshoot = tc->progress - tc->coords.rigidtap.reversal_target;
                reverseRigidTapMotion(tc, &tp->spindle.origin);
                double expected_reversal_overshoot = fmax(retract_allowance / tc->coords.rigidtap.retract_uu_per_rev, 0);
                // Anticipate the overshoot at the top and add a safety factor for spindle uncertainty
                addRigidTapOverrun(tc, RIGIDTAP_MAX_OVERSHOOT_REVS + expected_reversal_overshoot, tc->coords.rigidtap.retract_uu_per_rev);
                tc->coords.rigidtap.state = RIGIDTAP_RETRACTION;
                tc->uu_per_rev = tc->coords.rigidtap.retract_uu_per_rev;
            }
            break;
        case RIGIDTAP_RETRACTION:
            if (tc->progress >= tc->coords.rigidtap.reversal_target) {
                // Flip spindle direction again to start final reversal
                cmdReverseSpindle(1.0/tc->coords.rigidtap.reversal_scale);
                tc->coords.rigidtap.state = RIGIDTAP_FINAL_REVERSAL;
                // Once we've cleared the hole, there's no reason to keep synched with the spindle unless we're peck tapping
                if (!nexttc
                        || nexttc->motion_type != TC_RIGIDTAP
                        || tc->term_cond != TC_TERM_COND_TANGENT) {
                    // Stop synchronized motion if not peck tapping
                    tc->synchronized = 0;
                    tc->target_vel = 0; // Stop as fast as possible during final reversal
                    tc->canon_motion_type = EMC_MOTION_TYPE_TRAVERSE;
                }
            }
            break;
        case RIGIDTAP_FINAL_REVERSAL:
            if (spindleReversed(tp->spindle.origin, old_spindle_pos, spindle_pos) && tc->currentvel <= 0.0) {
                reverseRigidTapMotion(tc, &tp->spindle.origin);
                tc->uu_per_rev = tc->coords.rigidtap.tap_uu_per_rev;
                tc->coords.rigidtap.state = RIGIDTAP_FINAL_PLACEMENT;
                if (!nexttc
                        || nexttc->motion_type != TC_RIGIDTAP
                        || tc->term_cond != TC_TERM_COND_TANGENT) {
                    // Stop synchronized motion if not peck tapping
                    tc->synchronized = 0;
                    tc->target_vel = tc->maxvel_geom; // Move as fast as possible to the final position
                    tc->canon_motion_type = EMC_MOTION_TYPE_TRAVERSE;
                }
            }
            break;
        case RIGIDTAP_FINAL_PLACEMENT:
            // this is a regular move now, it'll stop at target above.
            old_spindle_pos = 0.0;
            retract_allowance = 0.0;
            break;
    }
    old_spindle_pos = spindle_pos;
#ifdef TC_DEBUG
    rigid_tap_state_t current_state = tc->coords.rigidtap.state;
    print_json5_log_start(tpUpdateRigidTapState);
    print_json5_unsigned(current_state);
    print_json5_log_end();
#endif
}


/**
 * Update emcMotStatus with information about trajectory motion.
 * Based on the specified trajectory segment tc, read its progress and status
 * flags. Then, update the emcmotStatus structure with this information.
 */
int tpUpdateMovementStatus(TP_STRUCT * const tp,
        TC_STRUCT const * const tc,
        TC_STRUCT const * const nexttc)
{

    if (!tp) {
        return TP_ERR_FAIL;
    }

    if (!tc) {
        // Assume that we have no active segment, so we should clear out the status fields
        emcmotStatus->distance_to_go = 0;
        emcmotStatus->enables_queued = emcmotStatus->enables_new;
        emcmotStatus->requested_vel = 0;
        emcmotStatus->current_vel = 0;
        emcmotStatus->dwell_time_remaining = 0.0;
        emcPoseZero(&emcmotStatus->dtg);

        tp->motionType = 0;
        tp->activeDepth = 0;
        return TP_ERR_STOPPED;
    }

    PmVector tc_pos;
    tcGetEndpoint(tc, &tc_pos);

    tp->motionType = tc->canon_motion_type;
    tp->activeDepth = tc->active_depth;
    emcmotStatus->distance_to_go = tc->target - tc->progress;
    emcmotStatus->enables_queued = tc->enables;
    // report our line number to the guis
    tp->execId = tc->tag.fields[GM_FIELD_LINE_NUMBER];
    // Store the next ID as well to show what will execute next to the GUI
    int nextexecId = tp->nextexecId = tc->tag.fields[GM_FIELD_NEXT_LINE];
    if (0 == nextexecId && nexttc) {
        tp->nextexecId = nexttc->tag.fields[GM_FIELD_LINE_NUMBER];
    } else {
        tp->nextexecId = nextexecId;
    }

    emcmotStatus->requested_vel = tc->reqvel;
    emcmotStatus->dwell_time_remaining = (tc->motion_type == TC_DWELL) ? tc->coords.dwell.remaining_time : 0.0;
    emcmotStatus->waiting_on_spindle = tpIsWaitingOnSpindle(tp);

    // KLUDGE Zero out any EMC status that doesn't apply
    if (tc->synchronized != TC_SYNC_POSITION) {
        clearPosTrackingStatus();
    }
    emcmotStatus->rigid_tap_state = tc->motion_type == TC_RIGIDTAP ? tc->coords.rigidtap.state : RIGIDTAP_INACTIVE;

    PmVector dtg;
    VecVecSub(&tc_pos, &tp->currentPos, &dtg);
    pmVectorToEmcPose(&dtg, &emcmotStatus->dtg);
    return TP_ERR_OK;
}
/**
 * Cleanup if tc is not valid (empty queue).
 * If the program ends, or we hit QUEUE STARVATION, do a soft reset on the trajectory planner.
 * TODO merge with tpClear?
 */
void tpHandleEmptyQueue(TP_STRUCT * const tp)
{

    tcqInit(&tp->queue);
    tp->goalPos = tp->currentPos;
    tp->done = 1;
    tp->depth = tp->activeDepth = 0;
    tp->aborting = 0;
    tp->execId = 0;
    tp->motionType = 0;

    tpUpdateMovementStatus(tp, NULL, NULL);

    tpResume(tp);
    // when not executing a move, use the current enable flags
    emcmotStatus->enables_queued = emcmotStatus->enables_new;
}

/** Wrapper function to unlock rotary axes */
void tpSetRotaryUnlock(IndexRotaryAxis axis, int unlock) {
    emcmotSetRotaryUnlock(axis, unlock);
}

/** Wrapper function to check rotary axis lock */
int tpGetRotaryIsUnlocked(IndexRotaryAxis axis) {
    return emcmotGetRotaryIsUnlocked(axis);
}

/**
 * Cleanup after a trajectory segment is complete.
 * If the current move is complete and we're not waiting on the spindle for
 * const this move, then pop if off the queue and perform cleanup operations.
 * Finally, get the next move in the queue.
 */
int tpCompleteSegment(TP_STRUCT * const tp,
        TC_STRUCT * const tc) {

    if (tp->spindle.waiting_for_atspeed == tc->id) {
        return TP_ERR_FAIL;
    }

    // if we're synced, and this move is ending, save the
    // spindle position so the next synced move can be in
    // the right place.
    if(tc->synchronized != TC_SYNC_NONE) {
        updateSpindlePositionFromProgress(&tp->spindle.origin, tc);
    } else {
        setSpindleOrigin(&tp->spindle.origin, 0.0);
    }

    if(tc->indexrotary != INDEX_NONE) {
        // this was an indexing move, so before we remove it we must
        // relock the axis
        tpSetRotaryUnlock(tc->indexrotary, 0);
        // if it is now locked, fall through and remove the finished move.
        // otherwise, just come back later and check again
        if(tpGetRotaryIsUnlocked(tc->indexrotary)) {
            return TP_ERR_FAIL;
        }
    }

    //Clear status flags associated since segment is done
    //TODO stuff into helper function?
    tc->active = 0;
    tc->remove = 0;
    tc->splitting = 0;
    tc->cycle_time = tp->cycleTime;
    //Velocities are by definition zero for a non-active segment
    tc->currentvel = 0.0;
    tp->tc_completed_id = tc->id;
    tc->term_vel = 0.0;
    //TODO make progress to match target?
    // done with this move
    if (tp->reverse_run) {
        tcqBackStep(&tp->queue);
        tp_debug_print("Finished reverse segment %lld\n", tc->unique_id);
    } else {
        int res_pop = tcqPop(&tp->queue);
        if (res_pop) rtapi_print_msg(RTAPI_MSG_ERR,"Got error %d from tcqPop!\n", res_pop);
        tp_debug_print("Finished forward segment %lld\n", tc->unique_id);
    }

    return TP_ERR_OK;
}

/**
 * Handle an abort command.
 * Based on the current motion state, handle the consequences of an abort command.
 */
tp_err_t tpHandleAbort(TP_STRUCT * const tp, TC_STRUCT * const tc,
        TC_STRUCT * const nexttc) {

    if(!tp->aborting) {
        //Don't need to do anything if not aborting
        return TP_ERR_NO_ACTION;
    }

    int movement_stopped = (!tc || tc->currentvel == 0.0) && (!nexttc || nexttc->currentvel == 0.0);

    //If the motion has stopped, then it's safe to reset the TP struct.
    if( MOTION_ID_VALID(tp->spindle.waiting_for_index) ||
            MOTION_ID_VALID(tp->spindle.waiting_for_atspeed) ||
            movement_stopped) {
        tpClear(tp);
        return TP_ERR_STOPPED;
    }
    return TP_ERR_SLOWING;
}

void setSpindleTrigger(TP_STRUCT * const tp, TC_STRUCT * const tc)
{
    const double spindle_vel_rps_raw = get_spindle_speed_out_rpm(emcmotStatus) / 60.0;

    // WARNING: this assumes that max acceleration is constant over the segment to estimate the position error
    // This won't be true in the future if we switch to s-curve planning
    double a_max = tcGetTangentialMaxAccel(tc);
    double accel_revs_est = fabs(tc->uu_per_rev * pmSq(spindle_vel_rps_raw) / (2.0 * a_max));
    // Advance the spindle origin by at least enough
    double spindle_offset_turns = ceil(accel_revs_est + 0.25);
    setSpindleOrigin(&tp->spindle.origin, spindle_offset_turns * get_spindle_command_direction(emcmotStatus));

    // Setup spindle trigger conditions
    tp->spindle.trigger_revs = -accel_revs_est;
}

/**
 * Check if the spindle has reached the required speed for a move.
 * Returns a "wait" code if the spindle needs to spin up before a move and it
 * has not reached the requested speed, or the spindle index has not been
 * detected.
 */
tp_err_t tpCheckAtSpeed(TP_STRUCT * const tp, TC_STRUCT * const tc)
{
    // this is no longer the segment we were waiting_for_index for
    if (MOTION_ID_VALID(tp->spindle.waiting_for_index) && tp->spindle.waiting_for_index != tc->id)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "Was waiting for index on motion id %d, but reached id %d\n",
                tp->spindle.waiting_for_index, tc->id);
        tp->spindle.waiting_for_index = MOTION_INVALID_ID;
    }

    if (MOTION_ID_VALID(tp->spindle.waiting_for_atspeed) && tp->spindle.waiting_for_atspeed != tc->id)
    {

        rtapi_print_msg(RTAPI_MSG_ERR,
                "Was waiting for atspeed on motion id %d, but reached id %d\n",
                tp->spindle.waiting_for_atspeed, tc->id);
        tp->spindle.waiting_for_atspeed = MOTION_INVALID_ID;
    }

    if (MOTION_ID_VALID(tp->spindle.waiting_for_atspeed)) {
        if(!emcmotStatus->spindle_is_atspeed) {
            // spindle is still not at the right speed, so wait another cycle
            double waited_time = (double)(tp->time_elapsed_ticks - tp->time_at_wait)*tp->cycleTime;
            if ( waited_time > emcmotConfig->timeout_cfg.atspeed_wait_timeout_sec) {
                LineDescriptor linedesc;
                formatLinePrefix(&tc->tag, &linedesc);
                tpStopWithError(tp, "%sSpindle did not reach desired speed %0.2f RPM within %0.2f seconds (spindle-at-speed-timeout)",
                    linedesc.buf,
                    emcmotStatus->spindle_cmd.velocity_rpm_out,
                    emcmotConfig->timeout_cfg.atspeed_wait_timeout_sec);
            } else {
                return TP_ERR_WAITING;
            }
        } else {
            tp->spindle.waiting_for_atspeed = MOTION_INVALID_ID;
        }
    }

    if (MOTION_ID_VALID(tp->spindle.waiting_for_index)) {
        if (emcmotStatus->spindle_fb.index_enable) {
            double waited_time = (double)(tp->time_elapsed_ticks - tp->time_at_wait)*tp->cycleTime;
            if (waited_time > emcmotConfig->timeout_cfg.index_wait_timeout_sec) {
                LineDescriptor linedesc;
                formatLinePrefix(&tc->tag, &linedesc);
                tpStopWithError(tp, "%sSpindle index signal was not detected within %.2f seconds (spindle-index-timeout)",
                    linedesc.buf,
                    waited_time);
            } else {
                /* haven't passed index yet */
                return TP_ERR_WAITING;
            }
        } else {
            /* passed index, start the move */
            emcmotStatus->spindle_fb.synced = 1;
            tp->spindle.waiting_for_index = MOTION_INVALID_ID;
            setSpindleTrigger(tp, tc);
            emcmotStatus->spindle_sync_state = SYNC_TRIGGER_WAIT;
        }
    }

    return TP_ERR_OK;
}

/**
 * Return true if a spindle-synched move is waiting on the spindle
 */
inline int tpIsWaitingOnSpindle(TP_STRUCT const * const tp)
{
    if (MOTION_ID_VALID(tp->spindle.waiting_for_index)) {
        return tp->spindle.waiting_for_index;
    } else if (MOTION_ID_VALID(tp->spindle.waiting_for_atspeed)) {
        return tp->spindle.waiting_for_atspeed;
    }
    return 0;
}

void checkPositionMatch(TP_STRUCT const *tp, TC_STRUCT const *tc)
{
    unsigned need_print = _tp_debug;
    PmVector tp_position_error={};
    if (needConsistencyCheck(CCHECK_C0_CONTINUITY)){
        tcGetPos(tc, &tp_position_error);
        VecVecSubEq(&tp_position_error, &tp->currentPos);

        unsigned position_mismatch_axes = findAbsThresholdViolations(tp_position_error, emcmotConfig->consistencyCheckConfig.maxPositionDriftError);
        need_print |= position_mismatch_axes;
        reportTPAxisError(tp, position_mismatch_axes, "Motion start position difference exceeds threshold");
    }

    // Log a bunch of TP internal state if required by debug level or position error
    if (need_print) {
        print_json5_log_start(tpActivateSegment);
        print_json5_long_long_("time_ticks", tp->time_elapsed_ticks);
        print_json5_tc_id_data_(tc);
        print_json5_string_("motion_type_name", tcMotionTypeAsString(tc->motion_type));
        print_json5_int_("motion_type", tc->motion_type);
        print_json5_int_("active_axes", tc->tag.fields[GM_FIELD_FEED_AXES]);

        // Position settings

        print_json5_PmVector(tp_position_error);
        print_json5_double_field(tc, target);
        print_json5_double_field(tc, progress);
        if (tc->motion_type == TC_SPHERICAL) {
            print_json5_double_field(&tc->coords.arc, line_length);
        }
        // Velocity settings
        print_json5_double_field(tc, reqvel);
        print_json5_double_field(tc, target_vel);
        print_json5_double_field(tc, finalvel);
        print_json5_double_field(tc, kink_vel);
        print_json5_int_field(tc, use_kink);
        print_json5_double_field(tc, parabolic_equiv_vel);
        int blend_mode = 0;
        if (tc->term_cond == TC_TERM_COND_PARABOLIC) {
            blend_mode = 2;
        } else if (tc->term_cond == TC_TERM_COND_TANGENT) {
            blend_mode = tc->use_kink ? 3 : 4;
        }
        print_json5_int(blend_mode);
        // Acceleration settings
        print_json5_double_("accel_scale", tcGetAccelScale(tc));
        print_json5_double_("acc_overall", tcGetOverallMaxAccel(tc));
        print_json5_double_("acc_tangential", tcGetTangentialMaxAccel(tc));
        print_json5_bool_("accel_ramp", tc->accel_mode);
        print_json5_string_("sync_mode", tcSyncModeAsString(tc->synchronized));

        print_json5_int_field(tc, finalized);
        print_json5_int_field(tc, term_cond);
        print_json5_int_field(tc, id);
        print_json5_int_field(tc, atspeed);

        print_json5_log_end();
    }
}

/**
 * "Activate" a segment being read for the first time.
 * This function handles initial setup of a new segment read off of the queue
 * for the first time.
 */
tp_err_t tpActivateSegment(TP_STRUCT * const tp, TC_STRUCT * const tc) {
    //Check if already active
    if (!tc || tc->active) {
        return TP_ERR_OK;
    }

#ifdef TP_PEDANTIC
    if (!tp) {
        return TP_ERR_MISSING_INPUT;
    }
#endif

    if (tp->reverse_run && (tc->motion_type == TC_RIGIDTAP || tc->synchronized != TC_SYNC_NONE || tc->atspeed)) {
        //Can't activate a segment with synced motion in reverse
        return TP_ERR_REVERSE_EMPTY;
    }

    /* Based on the INI setting for "cutoff frequency", this calculation finds
     * short segments that can have their acceleration be simple ramps, instead
     * of a trapezoidal motion. This leads to fewer jerk spikes, at a slight
     * performance cost.
     * */
    double cutoff_time = 1.0 / (fmax(emcmotConfig->arc_blend_cfg.ramp_frequency, TP_TIME_EPSILON));

    double length = tcGetDistanceToGo(tc, tp->reverse_run);
    // Given what velocities we can actually reach, estimate the total time for the segment under ramp conditions
    double segment_time = 2.0 * length / (tc->currentvel + fmin(tc->finalvel,tpGetRealAbsTargetVel(tp,tc)));

    if (segment_time < cutoff_time &&
            tc->canon_motion_type != EMC_MOTION_TYPE_TRAVERSE &&
            tc->term_cond == TC_TERM_COND_TANGENT &&
            !tc->synchronized)
    {
        tc->accel_mode = TC_ACCEL_RAMP;
    }

    // Do at speed checks that only happen once
    int needs_atspeed = tc->atspeed ||
        (tc->synchronized == TC_SYNC_POSITION && !(emcmotStatus->spindle_fb.synced));

    if ( needs_atspeed && !(emcmotStatus->spindle_is_atspeed)) {
        tp->spindle.waiting_for_atspeed = tc->id;
        return TP_ERR_WAITING;
    }

// Disable this check until we can do a controlled stop during peck tapping, or do this check earlier
#if 0
    if (tc->motion_type == TC_RIGIDTAP) {
        double tap_uu_per_rev = tc->coords.rigidtap.tap_uu_per_rev;
        double nominal_axis_vel = emcmotStatus->spindle_fb.velocity_rps * tap_uu_per_rev;
        // How long will it take at the current spindle speed (in axis distance) to fully decelerate to a stop
        double decel_distance = estimate_rigidtap_decel_distance(nominal_axis_vel, tap_uu_per_rev);
        // Nominal depth of the hole with no overshoot
        // TODO add adjustable overshoot warning cutoff
        double nominal_depth = tc->coords.rigidtap.nominal_xyz.tmag;

        if (decel_distance > nominal_depth) {
            // Add a bit of padding to the suggested margin
            double height_margin = decel_distance * 1.05 - nominal_depth;
            LineDescriptor linedesc;
            formatLinePrefix(&tc->tag, &linedesc);
            tpStopWithError(tp, "%sRigid tap cycle depth %.3f in is not enough to fully reverse the spindle. Increase starting height by at least %.3f in or reduce spindle speed",
                            linedesc.buf,
                            nominal_depth,
                            height_margin);
            return TP_ERR_FAIL;
        }
    }
#endif

    if (tc->indexrotary != INDEX_NONE) {
        // request that the axis unlock
        tpSetRotaryUnlock(tc->indexrotary, 1);
        // if it is unlocked, fall through and start the move.
        // otherwise, just come back later and check again
        if (!tpGetRotaryIsUnlocked(tc->indexrotary))
            return TP_ERR_WAITING;
    }

    tc->active = 1;
    //Do not change initial velocity here, since tangent blending already sets this up
    tp->motionType = tc->canon_motion_type;
    tc->on_final_decel = 0;

    tp_err_t res = TP_ERR_OK;

    if (TC_SYNC_POSITION == tc->synchronized && !(emcmotStatus->spindle_fb.synced)) {
        tp_debug_print("Setting up position sync\n");
        // if we aren't already synced, wait
        tp->spindle.waiting_for_index = tc->id;
        // ask for an index reset
        emcmotStatus->spindle_fb.index_enable = 1;


        setSpindleOrigin(&tp->spindle.origin, 0.0);
        emcmotStatus->spindle_sync_state = SYNC_SEEK_INDEX;
        rtapi_print_msg(RTAPI_MSG_DBG, "Waiting on sync...\n");
        res = TP_ERR_WAITING;
    }

    if (tc->motion_type == TC_DWELL) {
        // Dwell the requested amount of time, plus any extra time required by the spindle rpm delta
        double dwell_time = tc->coords.dwell.dwell_time_req;
        if (tc->coords.dwell.delta_rpm_req > 0
        && emcmotStatus->spindle_max_acceleration_rps2 > 0) {
            // Get the current spindle acceleration (changes based on belt position, for example) and compute a dwell time to wait for the spindle to reach the speed delta
            // This avoids the at-speed handshake which may not be consistent enough for soft-tapping
            double delta_rps = tc->coords.dwell.delta_rpm_req / 60.0;
            dwell_time += delta_rps / emcmotStatus->spindle_max_acceleration_rps2 + emcmotStatus->spindle_cmd_latency_sec;
        }
        tc->coords.dwell.dwell_time = tc->coords.dwell.remaining_time = dwell_time;
    }

    // Update the modal state displayed by the TP
    tp->execTag = tc->tag;
    clearPositionSyncErrors();

    checkPositionMatch(tp, tc);
    return res;
}

/**
 * Run velocity mode synchronization.
 * Update requested velocity to follow the spindle's velocity (scaled by feed rate).
 */
void tpSyncVelocityMode(TC_STRUCT * const tc, TC_STRUCT * const nexttc) {
    double speed = fabs(emcmotStatus->spindle_fb.velocity_rps);
    double pos_error = speed * tc->uu_per_rev;
    // Account for movement due to parabolic blending with next segment
    if(nexttc) {
        pos_error -= nexttc->progress;
    }
    tc->target_vel = pos_error;
    emcmotStatus->spindle_sync_state = SYNC_VELOCITY;

    if (nexttc && nexttc->synchronized) {
        //If the next move is synchronized too, then match it's
        //requested velocity to the current move
        nexttc->target_vel = tc->target_vel;
    }
}

#if 0
/**
 * A function that looks like sqrt but is flatter, and does not have infinite slope at x = 0 for c > 0
 * @pre c >= 0, x >= 0
 * @return
 */
static inline double pseudo_sqrt_cexpr(double x, double c)
{
    const double den = (pmSqrt(c + 1) - pmSqrt(c));
    const double b0 = -pmSqrt(c
    const double b1 = 1;
    return (b1 * pmSqrt(x+c) + b0) / den;
}
#endif

static int pos_sync_error_reported = 0;
void checkPositionSyncError(TP_STRUCT const *tp, TC_STRUCT const *tc)
{
    const double max_allowed_error = emcmotConfig->maxPositionTrackingError;
    if (!pos_sync_error_reported
            && emcmotStatus->spindle_sync_state == SYNC_POSITION
            && fabs(emcmotStatus->pos_tracking_error) > max_allowed_error) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Spindle position tracking error exceeds limit of %f on line %d\n",
                        max_allowed_error,
                        tc->id);
        print_json5_log_start(tpSyncPosition);
        print_json5_long_long_("time_ticks", tp->time_elapsed_ticks);
        print_json5_double_("current_vel", emcmotStatus->current_vel);
        print_json5_double_("time", tp->time_elapsed_sec);
        print_json5_double_("spindle_revs", emcmotStatus->spindle_fb.position_rev);
        print_json5_double_("spindle_speed_rps", get_spindle_speed_out_rpm(emcmotStatus) / 60.);
        print_json5_double_("spindle_speed_out", get_spindle_speed_out_rpm(emcmotStatus));
        print_json5_double_("spindle_speed_cmd_rpm", emcmotStatus->spindle_cmd.velocity_rpm_out);
        print_json5_double_("spindle_tracking_lookahead_steps", emcmotStatus->spindle_tracking_lookahead_steps);
        print_json5_long_("pos_tracking_mode", emcmotStatus->pos_tracking_mode);
        print_json5_double_("pos_tracking_velocity", emcmotStatus->pos_tracking_velocity);
        print_json5_double_("pos_tracking_error", emcmotStatus->pos_tracking_error);
        print_json5_end_();
        pos_sync_error_reported = 1;
    }
}

void clearPositionSyncErrors()
{
    pos_sync_error_reported = 0;
}

/**
 * Run position mode synchronization.
 * Updates requested velocity for a trajectory segment to track the spindle's position.
 */
void tpSyncPositionMode(
    TP_STRUCT * const tp,
    TC_STRUCT * const tc,
    TC_STRUCT * const nexttc )
{

    // Start with raw spindle position and our saved offset
    double spindle_pos_raw = emcmotStatus->spindle_fb.position_rev;

    const double spindle_vel_rps = findSpindleVelocity(emcmotStatus->spindle_fb.velocity_rps, tp->spindle.origin);
    // Estimate spindle position delta (2 steps seems to minimize physical position error
    const double lookahead_steps = bound(emcmotStatus->spindle_tracking_lookahead_steps, 100.0, 0);
    const double spindle_lookahead_delta = spindle_vel_rps * tp->cycleTime * lookahead_steps;

    // Note that this quantity should be non-negative under normal conditions.
    double spindle_displacement_measured = findSpindleDisplacement(spindle_pos_raw,
                                                          tp->spindle.origin);
    double spindle_displacement = spindle_displacement_measured + spindle_lookahead_delta;

    double v_final = spindle_vel_rps * tc->uu_per_rev;

    // Multiply by user feed rate to get equivalent desired position
    const double pos_desired = (spindle_displacement) * tc->uu_per_rev;
    double net_progress = tc->progress;

    if(nexttc) {
        // If we're in a parabolic blend, the next segment will be active too,
        // so make sure to account for its progress
        net_progress += nexttc->progress;
    }
    const double pos_error = pos_desired - net_progress;

    double a_max = tcGetTangentialMaxAccel(tc);

#ifdef TC_DEBUG
    print_json5_log_start(tpSyncPositionMode);
    const double vel_error_before = v_final - tc->target_vel;
    print_json5_double(vel_error_before);
    print_json5_double(pos_error);
    print_json5_double(spindle_displacement);
    print_json5_double(spindle_lookahead_delta);
    print_json5_double(spindle_pos_raw);
    print_json5_double(pos_desired);
    print_json5_double(net_progress);
#endif

    switch (emcmotStatus->spindle_sync_state) {
    case SYNC_INACTIVE:
    case SYNC_VELOCITY:
    case SYNC_SEEK_INDEX:
        // not valid here
        break;

    case SYNC_TRIGGER_WAIT:
        tc->target_vel = 0.0;
        if (spindle_displacement < tp->spindle.trigger_revs) {
            break;
        }
        emcmotStatus->spindle_sync_state = SYNC_ACCEL_RAMP;
    case SYNC_ACCEL_RAMP:
        // Axis has caught up to spindle once position error goes positive
        // note that it may not be perfectly synced at this point, but it should be pretty close
        if (pos_error >= 0) {
            emcmotStatus->spindle_sync_state = SYNC_POSITION;
        }
    case SYNC_POSITION:
        if (tc->on_final_decel) {
            emcmotStatus->spindle_sync_state = SYNC_FINAL_DECEL;
        }
    case SYNC_FINAL_DECEL:
        // we have synced the beginning of the move as best we can -
        // track position (minimize pos_error).
        // This is the velocity we should be at when the position error is c0

        /*
         * In general, position tracking works by perturbing the
         * velocity-synced feed up or down to correct for transient position
         * errors.  If position error is 0 (e.g. a perfect spindle and
         * encoder), then the behavior is identical to velocity-synced motion.
         *
         * velocity
         * |          v_p
         * |         /\
         * |        /..\         v_0
         * |--------....-----------
         * |        ....
         * |        ....
         * |_________________________
         *         |----| t      time
         *
         * To correct a position error x_err (shaded area above), we need to
         * momentarily increase the velocity to catch up, then drop back to the
         * sync velocity.
         *
         * In effect, this is the trapezoidal velocity planning problem, if:
         * 1) remaining distance dx = x_err
         * 2) "final" velocity = v_0
         * 3) max velocity / acceleration from motion segment
         */

        switch(emcmotStatus->pos_tracking_mode) {
        case POS_TRACK_MODE_LEGACY:
        {
            // LinuxCNC 2.6 approach to spindle tracking (high jitter when
            // position error is very small due to slope of sqrt for small
            // values)
            double v_sq = a_max * pos_error;
            double v_target_stock = signum(v_sq) * pmSqrt(fabs(v_sq)) + v_final;
            tc->target_vel = v_target_stock;
            break;
        }
        case POS_TRACK_MODE_FLATTENED:
        {
            // Experimental spindle tracking that adds correction terms using a pythagorean sum
            double v_sq = a_max * pos_error;
            double v_target_flat = signum(v_sq) * pseudo_sqrt(fabs(v_sq)) + v_final;
            tc->target_vel = v_target_flat;
            break;
        }
        case POS_TRACK_MODE_TRAPEZOIDAL:
        {
            double v_max = tc->maxvel_geom;
            // Use trapezoidal velocity calculation to find target velocity
            // NOTE: this tracking method is smoother but has larger average tracking errors; it's here mostly for compatibility
            double v_target_trapz = fmin(findTrapezoidalDesiredVel(a_max, pos_error, v_final, tc->currentvel, tc->cycle_time), v_max);
            tc->target_vel = v_target_trapz;
            break;
        }
        }
    }
    emcmotStatus->pos_tracking_velocity = tc->target_vel;
    emcmotStatus->pos_tracking_error = pos_error;

    //Finally, clip requested velocity at zero
    if (tc->target_vel < 0.0) {
        tc->target_vel = 0.0;
    }

    if (nexttc && nexttc->synchronized) {
        //If the next move is synchronized too, then match it's
        //requested velocity to the current move
        nexttc->target_vel = tc->target_vel;
    }
#ifdef TC_DEBUG
    print_json5_double_("target_vel", tc->target_vel);
    tp_debug_json5_log_end("sync position updated");
#endif
}

/**
        TC_STRUCT *next2tc = tcqItem(&tp->queue, 2);
 * Do a complete update on one segment.
 * Handles the majority of updates on a single segment for the current cycle.
 */
int tpUpdateCycle(TP_STRUCT * const tp,
    TC_STRUCT * const tc,
    TC_STRUCT const * const nexttc,
    UpdateCycleMode cycle_mode)
{
    //placeholders for position for this update
    PmVector before;

    //Store the current position due to this TC
    tcGetPos(tc, &before);

    // Run cycle update with stored cycle time
    int res_accel = 1;
    double acc=0, vel_desired=0;
    
    // If the slowdown is not too great, use velocity ramping instead of trapezoidal velocity
    // Also, don't ramp up for parabolic blends
    if (tc->accel_mode && tc->term_cond == TC_TERM_COND_TANGENT) {
        res_accel = tpCalculateRampAccel(tp, tc, nexttc, &acc, &vel_desired);
    }

    // Check the return in case the ramp calculation failed, fall back to trapezoidal
    if (res_accel != TP_ERR_OK) {
        tpCalculateTrapezoidalAccel(tp, tc, nexttc, &acc, &vel_desired);
    }

    if (   tc->motion_type == TC_RIGIDTAP
        && tc->coords.rigidtap.state == RIGIDTAP_REVERSING
        && !tp->aborting
        && (tc->on_final_decel || tcGetDistanceToGo(tc, tp->reverse_run) < tc->coords.rigidtap.tap_uu_per_rev)) {
        // We're going to hit the bottom of the hole (spindle has overshot) and are losing synchronization
        LineDescriptor linedesc;
        formatLinePrefix(&tc->tag, &linedesc);
        tpStopWithError(tp, "%sSpindle reversal exceeded maximum overshoot in rigid tapping cycle. Increase cycle depth or reduce spindle speed.",
                        linedesc.buf);
    }

    int accel_mode_ramp = (res_accel == TP_ERR_OK);
    tcUpdateDistFromAccel(tc, acc, vel_desired, tp->reverse_run);
    tpDebugCycleInfo(tp, tc, nexttc, acc, accel_mode_ramp, cycle_mode);

    //Check if we're near the end of the cycle and set appropriate changes
    tpCheckEndCondition(tp, tc, nexttc);

    PmVector displacement;

    // Calculate displacement
    tcGetPos(tc, &displacement);
    VecVecSubEq(&displacement, &before);

    //Store displacement (checking for valid pose)
    int res_set = tpAddCurrentPos(tp, &displacement);
    return res_set;
}

/**
 * Send default values to status structure.
 */
int tpUpdateInitialStatus(TP_STRUCT const * const tp) {
    // Update queue length
    emcmotStatus->tcqlen = tcqLen(&tp->queue);
    // Set default value for requested speed
    emcmotStatus->requested_vel = 0.0;
    emcmotStatus->current_vel = 0.0;
    emcmotStatus->dwell_time_remaining = 0.0;
    // Set synched move waiting on spindle flag
    emcmotStatus->waiting_on_spindle = tpIsWaitingOnSpindle(tp);
    return TP_ERR_OK;
}

/**
 * Flag a segment as needing a split cycle.
 * In addition to flagging a segment as splitting, do any preparations to store
 * data for the next cycle.
 */
int tcSetSplitCycle(TC_STRUCT * const tc, double split_time,
        double v_f)
{
    if (tc->splitting != 0 && split_time > 0.0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"already splitting on segment %lld with cycle time %.16g, dx = %.16g, split time %.12g\n",
                tc->unique_id,
                tc->cycle_time,
                tc->target-tc->progress,
                split_time);
        return TP_ERR_FAIL;
    }
    tc->splitting = 1;
    tc->cycle_time = split_time;
    tc->term_vel = v_f;
    return 0;
}

/**
 * Check remaining time in a segment and calculate split cycle if necessary.
 * This function estimates how much time we need to complete the next segment.
 * If it's greater than one timestep, then we do nothing and carry on. If not,
 * then we flag the segment as "splitting", so that during the next cycle,
 * it handles the transition to the next segment.
 *
 * @warning not reentrant since this is where we process TP-based dwells
 */
int tpCheckEndCondition(TP_STRUCT const * const tp, TC_STRUCT * const tc, TC_STRUCT const * const nexttc)
{
    EndCondition ec;
    if (tc->motion_type == TC_DWELL) {
        // Knock one cycle off the remaining dwell time
        ec.dt = fmax(tc->coords.dwell.remaining_time -= tp->cycleTime, 0.0);
        ec.v_f = 0.0;
    } else {
        ec = checkEndCondition(
            tp->cycleTime,
            tcGetDistanceToGo(tc, tp->reverse_run),
            tc->currentvel,
            tpGetRealFinalVel(tp, tc, nexttc),
            tcGetTangentialMaxAccel(tc)
            );
    }

    double dt = ec.dt;
    int splitting = dt < tp->cycleTime;
    if (splitting) {
        if (dt < TP_TIME_EPSILON) {
            dt = 0.0;
        }
        tcSetSplitCycle(tc, dt, ec.v_f);
    }
#ifdef TC_DEBUG
    {
        print_json5_object_start_("tpCheckEndCondition");
        print_json5_tc_id_data_(tc);
        print_json5_double_("v_final", ec.v_f);
        print_json5_double_("t_remaining", ec.dt);
        print_json5_double_("dt_used", dt);
        print_json5_bool_("remove", tc->remove);
        print_json5_bool_("need_split", splitting);
        print_json5_object_end_();
    }
#endif

    return TP_ERR_OK;
}

int tpHandleSplitCycle(TP_STRUCT * const tp, TC_STRUCT * const tc,
        TC_STRUCT * const nexttc)
{
    tp_debug_json5_log_start(tpHandleSplitCycle);
    if (tc->remove) {
        tp_debug_json5_log_end("segment flagged for removal");
        //Don't need to update since this segment is flagged for removal
        return TP_ERR_NO_ACTION;
    }

    //Pose data to calculate movement due to finishing current TC
    PmVector before;
    tcGetPos(tc, &before);

#ifdef TP_DEBUG
    /* Debug Output */
    print_json5_tc_id_data_(tc);
    print_json5_double_("target", tc->target);
    print_json5_double_("progress", tc->progress);
    print_json5_double_("v_terminal", tc->term_vel);
    print_json5_double_("dt", tc->cycle_time);
#endif

    //Shortcut tc update by assuming we arrive at end
    tc->progress = tcGetTarget(tc,tp->reverse_run);
    //Get displacement from prev. position
    PmVector displacement;
    tcGetPos(tc, &displacement);
    VecVecSubEq(&displacement, &before);

    // Update tp's position (checking for valid pose)
    tpAddCurrentPos(tp, &displacement);

    // Trigger removal of current segment at the end of the cycle
    tc->remove = 1;

    if (!nexttc) {
        tp_debug_json5_log_end("no nexttc in split cycle");
        return TP_ERR_OK;
    }

    // Handle various cases of updates for split cycles
    //  Tangent: next segment gets a partial update for the remaining cycle time
    //  Exact: NO motion in next segment here since current segment must stop completely
    switch (tc->term_cond) {
        case TC_TERM_COND_TANGENT:
            nexttc->cycle_time = tp->cycleTime - tc->cycle_time;
            nexttc->currentvel = tc->term_vel;
        {
            // TODO allow access to next segment when reverse blending is implemented
            TC_STRUCT *next2tc = tp->reverse_run ? NULL : tcqItem(&tp->queue, 2);
            tpUpdateCycle(tp, nexttc, next2tc, UPDATE_SPLIT);
         }
         break;
        case TC_TERM_COND_PARABOLIC:
            tp_debug_print("can't split with parabolic blends");
            return -1;
        case TC_TERM_COND_STOP:
        case TC_TERM_COND_EXACT:
            break;
    }

    // Update status for the split portion
    // FIXME redundant tangent check, refactor to switch
    if (tc->term_cond == TC_TERM_COND_STOP
            || tc->term_cond == TC_TERM_COND_EXACT
            || (tc->cycle_time > nexttc->cycle_time && tc->term_cond == TC_TERM_COND_TANGENT)) {
        //Majority of time spent in current segment
        tpToggleDIOs(tc);
        tpUpdateMovementStatus(tp, tc, nexttc);
    } else {
        tpToggleDIOs(nexttc);
        // TODO allow access to next segment when reverse blending is implemented
        TC_STRUCT *next2tc = tp->reverse_run ? NULL : tcqItem(&tp->queue, 2);
        tpUpdateMovementStatus(tp, nexttc, next2tc);
    }

    tp_debug_json5_log_end("splitting with next segment");
    return TP_ERR_OK;
}

int tpHandleRegularCycle(TP_STRUCT * const tp,
        TC_STRUCT * const tc,
        TC_STRUCT * const nexttc)
{
    if (!tc) {
        return TP_ERR_FAIL;
    }
    if (tc->remove) {
        //Don't need to update since this segment is flagged for removal
        return TP_ERR_NO_ACTION;
    }
    tp_debug_json5_log_start(tpHandleRegularCycle);
    //Run with full cycle time
    tc->cycle_time = tp->cycleTime;
    tpUpdateCycle(tp, tc, nexttc, UPDATE_NORMAL);

    //Update status for a normal step
    tpToggleDIOs(tc);
    tpUpdateMovementStatus(tp, tc, nexttc);
    tp_debug_json5_log_end("cycle done");
    return TP_ERR_OK;
}

int tpSteppingCheck(TP_STRUCT * const tp, TC_STRUCT * const tc, TC_STRUCT * const nexttc)
{
    if (!tc || !tp) {
        return TP_ERR_MISSING_INPUT;
    }

    if (nexttc && nexttc->tag.fields[GM_FIELD_STEPPING_ID] == tc->tag.fields[GM_FIELD_STEPPING_ID]) {
        //Not stepping since next segment is the same ID, so don't enforce exact stop
        return TP_ERR_NO_ACTION;
    }

    if (tc->motion_type != TC_LINEAR && tc->motion_type != TC_CIRCULAR) {
        //No special case for stepping
        return TP_ERR_NO_ACTION;
    }

    return TP_ERR_OK;
}
/**
 * Calculate an updated goal position for the next timestep.
 * This is the brains of the operation. It's called every TRAJ period and is
 * expected to set tp->currentPos to the new machine position. Lots of other
 * const tp fields (depth, done, etc) have to be twiddled to communicate the
 * status; I think those are spelled out here correctly and I can't clean it up
 * without breaking the API that the TP presents to motion.
 */
tp_err_t updateSyncTargets(TP_STRUCT *tp, TC_STRUCT *tc, TC_STRUCT *nexttc)
{
    // Clear old tracking status (will be updated in the appopriate handler)
    clearPosTrackingStatus();

    switch (tc->synchronized) {
        case TC_SYNC_NONE:
            emcmotStatus->spindle_sync_state = SYNC_INACTIVE;
            emcmotStatus->spindle_fb.synced = 0;
            return TP_ERR_OK;
        case TC_SYNC_VELOCITY:
            tpSyncVelocityMode(tc, nexttc);
            return TP_ERR_OK;
        case TC_SYNC_POSITION:
            tpSyncPositionMode(tp, tc, nexttc);
            checkPositionSyncError(tp, tc);

            return TP_ERR_OK;
    }
    emcmotStatus->spindle_sync_state = SYNC_INACTIVE;
    return TP_ERR_INVALID;
}

int tpRunCycleInternal(TP_STRUCT * const tp)
{
    /* Get pointers to current and relevant future segments. It's ok here if
     * future segments don't exist (NULL pointers) as we check for this later).
     */
    TC_STRUCT *tc = tcqItem(&tp->queue, 0); //!< Pointer to current motion segment or NULL
    // TODO allow access to next segment when reverse blending is implemented
    TC_STRUCT *nexttc = tp->reverse_run ? NULL : tcqItem(&tp->queue, 1); //!< Pointer to "next" motion segment or NULL

    //Set GUI status to "zero" state
    tpUpdateInitialStatus(tp);

    /* If we're aborting or pausing and the velocity has reached zero, then we
     * don't need additional planning and can abort here. */
    if (tpHandleAbort(tp, tc, nexttc) == TP_ERR_STOPPED) {
        return TP_ERR_STOPPED;
    }

    // If the TP is not aborting, then check for an empty queue (which means there's nothing to do)
    if(!tc) {
        tpHandleEmptyQueue(tp);
        return TP_ERR_WAITING;
    }

    //Return early if we have a reason to wait (i.e. not ready for motion)
    if (tpCheckAtSpeed(tp, tc) != TP_ERR_OK){
        return TP_ERR_WAITING;
    }

    int res_activate = tpActivateSegment(tp, tc);
    if (res_activate != TP_ERR_OK ) {
        tp->time_at_wait = tp->time_elapsed_ticks;
        return res_activate;
    }

    // Preprocess rigid tap move (handles threading direction reversals)
    if (tc->motion_type == TC_RIGIDTAP) {
        tpUpdateRigidTapState(tp, tc, nexttc);
    }

    // If synchronized with spindle, calculate requested velocity to track spindle motion
    updateSyncTargets(tp, tc, nexttc);

    tpSteppingCheck(tp, tc, nexttc);

    // Update the current tc
    if (tc->splitting) {
        tpHandleSplitCycle(tp, tc, nexttc);
    } else {
        tpHandleRegularCycle(tp, tc, nexttc);
    }

    // If TC is complete, remove it from the queue.
    if (tc->remove) {
        tpCompleteSegment(tp, tc);
    }

    return TP_ERR_OK;
}

double tpGetCurrentVel(TP_STRUCT const * const tp, PmVector const * const v_current, int *pure_angular)
{
    PmCartesian xyz, abc, uvw;
    VecToCart(v_current, &xyz, &abc, &uvw);
    double v_out = 0.0;
    *pure_angular = 0;
    switch (tpGetExecTag(tp).fields[GM_FIELD_FEED_AXES]) {
    case 0:
        break;
    case 1:
        pmCartMag(&xyz, &v_out);
        break;
    case 2:
        pmCartMag(&uvw, &v_out);
        break;
    case 3:
        pmCartMag(&abc, &v_out);
        *pure_angular = 1;
        break;
    }
    return v_out;
}

int tpRunCycle(TP_STRUCT *tp)
{
    // Before every TP update, ensure that elapsed time and
    // TP measurements are stored for error checks
    tp->time_elapsed_sec+=tp->cycleTime;
    ++tp->time_elapsed_ticks;
    PmVector const axis_pos_old = tp->currentPos;

    int res = tpRunCycleInternal(tp);

    // After update (even a no-op), update pos / vel / accel
    PmVector const axis_vel_old = tp->currentVel;
    PmVector const axis_pos = tp->currentPos;

    PmVector axis_vel;
    VecVecSub(&axis_pos, &axis_pos_old, &axis_vel);
    VecScalMultEq(&axis_vel, 1.0 / tp->cycleTime);
    tp->currentVel = axis_vel;

    emcmotStatus->current_vel = tpGetCurrentVel(tp, &tp->currentVel, &emcmotStatus->pure_angular_move);

    if (needConsistencyCheck(CCHECK_AXIS_LIMITS)) {
        PmVector axis_accel;
        VecVecSub(&axis_vel, &axis_vel_old, &axis_accel);
        VecScalMultEq(&axis_accel, 1.0 / tp->cycleTime);

        unsigned accel_error_mask = findAccelViolations(axis_accel);
        unsigned vel_error_mask = findVelocityViolations(axis_vel);
        unsigned pos_limit_error_mask = findPositionViolations(axis_pos);

        reportTPAxisError(tp, accel_error_mask, "Acceleration limit exceeded");
        reportTPAxisError(tp, vel_error_mask, "Velocity limit exceeded");
        reportTPAxisError(tp, pos_limit_error_mask, "Position limits exceeded");

        if (_tc_debug || (
                    accel_error_mask | vel_error_mask | pos_limit_error_mask)
                ) {
            print_json5_log_start(tpRunCycle);
            print_json5_long_long_("time_ticks", tp->time_elapsed_ticks);
            print_json5_PmVector(axis_pos);
            print_json5_PmVector(axis_vel);
            print_json5_PmVector(axis_accel);
            double current_vel = emcmotStatus->current_vel;
            print_json5_double(current_vel);
            print_json5_double_("time", tp->time_elapsed_sec);
            print_json5_end_();
        }
    }
    return res;
}

int tpSetSpindleSync(TP_STRUCT * const tp, double sync, int velocity_mode) {
    // WARNING assumes positive sync
    if(sync > 0) {
        if (velocity_mode) {
            tp->synchronized = TC_SYNC_VELOCITY;
        } else {
            tp->synchronized = TC_SYNC_POSITION;
        }
        tp->uu_per_rev = sync;
    } else {
        tp->synchronized = TC_SYNC_NONE;
        tp->uu_per_rev = sync;
    }

    return TP_ERR_OK;
}

int tpPause(TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }
    tp->pausing = 1;
    return TP_ERR_OK;
}

int tpResume(TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }
    tp->pausing = 0;
    return TP_ERR_OK;
}

int tpAbort(TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_FAIL;
    }

    if (!tp->aborting) {
        /* const to abort, signal a pause and set our abort flag */
        tpPause(tp);
        tp->aborting = 1;
    }
    return tpClearDIOs(tp); //clears out any already cached DIOs
}

int tpGetMotionType(TP_STRUCT * const tp)
{
    return tp->motionType;
}

int tpGetPos(TP_STRUCT const * const tp, EmcPose * const pos)
{

    if (0 == tp) {
        ZERO_EMC_POSE((*pos));
        return TP_ERR_FAIL;
    } else {
        pmVectorToEmcPose(&tp->currentPos, pos);
    }

    return TP_ERR_OK;
}

EmcPose tpGetGoalPos(TP_STRUCT const  * const tp)
{
    EmcPose out={};
    if (tp) {
        pmVectorToEmcPose(&tp->goalPos, &out);
    }

    return out;
}

int tpIsDone(TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_OK;
    }

    return tp->done;
}

int tpQueueDepth(TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_OK;
    }

    return tp->depth;
}

int tpActiveDepth(TP_STRUCT * const tp)
{
    if (0 == tp) {
        return TP_ERR_OK;
    }

    return tp->activeDepth;
}

int tpSetAout(TP_STRUCT * const tp, unsigned char index, double start, double end) {
    if (0 == tp) {
        return TP_ERR_FAIL;
    }
    tp->syncdio.anychanged = 1; //something has changed
    tp->syncdio.aio_mask |= (1 << index);
    tp->syncdio.aios[index] = start;
    return TP_ERR_OK;
}

int tpSetDout(TP_STRUCT * const tp, int index, unsigned char start, unsigned char end) {
    if (0 == tp) {
        return TP_ERR_FAIL;
    }
    tp->syncdio.anychanged = 1; //something has changed
    tp->syncdio.dio_mask |= (1 << index);
    if (start > 0)
        tp->syncdio.dios[index] = 1; // the end value can't be set from canon currently, and has the same value as start
    else
        tp->syncdio.dios[index] = -1;
    return TP_ERR_OK;
}

int tpSetRunDir(TP_STRUCT * const tp, tc_direction_t dir)
{
    if (tpIsMoving(tp)) {
        return TP_ERR_FAIL;
    }

    switch (dir) {
        case TC_DIR_FORWARD:
        case TC_DIR_REVERSE:
            tp->reverse_run = dir;
            return TP_ERR_OK;
    }
    rtapi_print_msg(RTAPI_MSG_ERR,"Invalid direction flag in SetRunDir");
    return TP_ERR_FAIL;
}

int tpIsMoving(TP_STRUCT const * const tp)
{

    //TODO may be better to explicitly check velocities on the first 2 segments, but this is messy
    if (emcmotStatus->current_vel >= TP_VEL_EPSILON ) {
        tp_debug_print("TP moving, current_vel = %.16g\n", emcmotStatus->current_vel);
        return true;
    } else if (tp->spindle.waiting_for_index != MOTION_INVALID_ID || tp->spindle.waiting_for_atspeed != MOTION_INVALID_ID) {
        tp_debug_print("TP moving, waiting for index or atspeed\n");
        return true;
    }
    return false;
}

void formatLinePrefix(struct state_tag_t const *tag, LineDescriptor *linebuf)
{
    linebuf->buf[0]='\0';
    int line = tag->fields[GM_FIELD_LINE_NUMBER];
    const int len = sizeof(linebuf->buf);
    if (line > 0) {
        rtapi_snprintf(linebuf->buf, len, "Line %d: ", line);
    }
    linebuf->buf[len-1]='\0';
}

void tpStopWithError(TP_STRUCT *tp, const char *fmt, ...)
{
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        enqueueError(fmt, args);
        va_end(args);
    }
    tpAbort(tp);
    SET_MOTION_ERROR_FLAG(1);
}


// vim:sw=4:sts=4:et:
