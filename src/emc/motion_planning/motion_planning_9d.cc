/********************************************************************
* Description: motion_planning_9d.cc
*   Tormach 9D trajectory planner - Userspace planning implementation
*
*   Ported from Tormach LinuxCNC implementation
*   Provides lookahead optimization for 9-axis coordinated motion
*
* Author: Tormach (original), Port by LinuxCNC community
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2022-2026 All rights reserved.
*
* RT SAFETY: This file is USERSPACE ONLY. It uses C++ std::vector
* and dynamic allocation which are FORBIDDEN in RT context.
* Communication with RT layer is via atomic operations only.
*
********************************************************************/

#include "motion_planning_9d.hh"
#include "smoothing_data.hh"
#include "motion.h"           // For emcmot_status_t (must be before tp.h)
#include "tc.h"
#include "tp.h"
#include "atomic_9d.h"
#include "posemath.h"
#include "rtapi.h"            // For rtapi_print_msg
#include "motion_types.h"     // For EMC_MOTION_TYPE_TRAVERSE
#include <ruckig/ruckig.hpp>   // Ruckig integration

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <sys/time.h>         // For gettimeofday (etime equivalent in userspace)

//============================================================================
// USERSPACE IMPLEMENTATIONS OF TC/TCQ FUNCTIONS
// These are userspace-specific versions that don't depend on RT globals
//============================================================================

/**
 * @brief Userspace version of tcqLen for planner_type 2
 * Works directly with atomic queue indices
 */
extern "C" int tcqLen_user(TC_QUEUE_STRUCT const * const tcq)
{
    if (!tcq) return -1;

    // Read atomic indices (planner_type 2 mode)
    int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
    int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

    // Calculate length with circular buffer wraparound
    int len = (current_end - current_start + tcq->size) % tcq->size;
    return len;
}

/**
 * @brief Userspace version of tcqBack for planner_type 2
 * Gets n-th item from back of queue (n=0 is last, n=-1 is second-to-last, etc.)
 */
static TC_STRUCT * tcqBack_user(TC_QUEUE_STRUCT * const tcq, int n)
{
    if (!tcq) return NULL;

    // Read atomic indices
    int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
    int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

    // Calculate length
    int len = (current_end - current_start + tcq->size) % tcq->size;

    // Check for empty queue or invalid index
    if (len == 0 || n > 0) return NULL;

    // Check if index is within valid range
    // n is negative: n=0 means last, n=-1 means second-to-last
    // So we need -n < len, i.e., n > -len
    if (n <= -len) return NULL;

    // Calculate index from end: end + n - 1
    // Fix for negative modulus by adding tcq->size
    int k = current_end + n - 1 + tcq->size;
    int idx = k % tcq->size;
    return &(tcq->queue[idx]);
}

/**
 * @brief Userspace version of tcqItem for planner_type 2
 * Gets item at index n from front of queue (n=0 is first/active, n=1 is next, etc.)
 */
static TC_STRUCT * tcqItem_user(TC_QUEUE_STRUCT * const tcq, int n)
{
    if (!tcq || n < 0) return NULL;

    // Read atomic indices
    int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
    int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

    // Calculate length
    int len = (current_end - current_start + tcq->size) % tcq->size;

    // Check if index is within valid range
    if (n >= len) return NULL;

    // Calculate index from start
    int idx = (current_start + n) % tcq->size;
    return &(tcq->queue[idx]);
}

/**
 * @brief Userspace version of tcqPut for planner_type 2
 * Writes a TC_STRUCT to the shared memory queue
 */
extern "C" int tcqPut_user(TC_QUEUE_STRUCT * const tcq, TC_STRUCT const * const tc)
{
    if (!tcq || !tc) return -1;

    // Read current indices atomically (Tormach pattern: ACQUIRE for reads)
    int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
    int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

    // Calculate next end index
    int next_end = (current_end + 1) % tcq->size;

    // Check if queue is full (with margin for reverse history)
    int available_space = (current_start - next_end + tcq->size) % tcq->size;
    const int TCQ_REVERSE_MARGIN = 100;
    const int QUEUE_MARGIN = 20;
    if (available_space < TCQ_REVERSE_MARGIN + QUEUE_MARGIN) {
        return -1;  // Queue full
    }

    // Copy TC element to queue at current end
    tcq->queue[current_end] = *tc;

    // Atomically update end index using exchange (Tormach pattern)
    // __atomic_exchange_n is a read-modify-write that provides stronger visibility guarantees
    __atomic_exchange_n(&tcq->end_atomic, next_end, __ATOMIC_ACQ_REL);

    return 0;
}

/**
 * @brief Userspace version of tcGetTangentialMaxAccel_9D
 *
 * Returns maximum acceleration considering:
 * 1. Machine acceleration limit (tc->maxaccel)
 * 2. Joint-space acceleration limit from Jacobian analysis (if valid)
 */
static double tcGetTangentialMaxAccel_9D_user(TC_STRUCT const * const tc)
{
    if (!tc) return 0.0;

    double acc = tc->maxaccel;

    // Apply joint-space acceleration limit from userspace kinematics
    if (tc->joint_space.valid) {
        double joint_acc_limit = tc->joint_space.acc_limit;
        if (joint_acc_limit > 0.0 && joint_acc_limit < acc) {
            acc = joint_acc_limit;
        }
    }

    // Reduce tangential acceleration for curved segments (centripetal uses part of budget)
    if (tc->motion_type == TC_CIRCULAR || tc->motion_type == TC_SPHERICAL ||
        tc->motion_type == TC_BEZIER) {
        acc *= tc->acc_ratio_tan;
    }

    return acc;
}

/**
 * @brief Userspace version of tcGetMaxVel_9D
 *
 * Returns maximum velocity considering:
 * 1. Requested velocity with feed override
 * 2. Machine velocity limit (tc->maxvel)
 * 3. Joint-space velocity limit from Jacobian analysis (if valid)
 */
static double tcGetMaxVel_9D_user(TC_STRUCT const * const tc, double max_feed_scale)
{
    if (!tc) return 0.0;

    // Start with requested velocity
    double vel = tc->reqvel;

    // Apply feed override
    vel *= max_feed_scale;

    // Clamp to machine velocity limit
    if (tc->maxvel > 0.0 && vel > tc->maxvel) {
        vel = tc->maxvel;
    }

    // Apply joint-space velocity limit from userspace kinematics
    // This accounts for Jacobian-derived limits near singularities
    // and joint velocity constraints
    if (tc->joint_space.valid) {
        double joint_vel_limit = tc->joint_space.vel_limit_end;
        if (joint_vel_limit > 0.0 && joint_vel_limit < vel) {
            vel = joint_vel_limit;
        }
    }

    return vel;
}

/**
 * @brief Get base velocity limit (before feed scaling)
 *
 * Returns the velocity limit considering:
 * 1. Requested velocity (tc->reqvel from F-word)
 * 2. Machine velocity limit (tc->maxvel)
 *
 * NOTE: Does NOT include vLimit. vLimit is an absolute cap that must be
 * applied AFTER feed scaling, not before. Use applyVLimit() after scaling.
 */
static double getEffectiveVelLimit(TP_STRUCT const * const tp, TC_STRUCT const * const tc)
{
    if (!tp || !tc) return 0.0;

    // Start with reqvel clamped to maxvel
    double vel_limit = tc->reqvel > 0 ? fmin(tc->reqvel, tc->maxvel) : tc->maxvel;

    // NOTE: vLimit is NOT applied here - it's an absolute cap applied after feed scaling
    return vel_limit;
}

/**
 * @brief Apply vLimit as absolute cap after feed scaling
 *
 * vLimit from the max velocity slider is an absolute limit, not scaled by feed override.
 * This must be applied AFTER multiplying by feed_scale.
 *
 * @param tp Trajectory planner (for vLimit value)
 * @param tc Segment (for rotary/sync checks)
 * @param scaled_vel Velocity after feed scaling (vel_limit * feed_scale)
 * @return Velocity capped by vLimit if applicable
 */
static double applyVLimit(TP_STRUCT const * const tp, TC_STRUCT const * const tc, double scaled_vel)
{
    if (!tp || !tc) return scaled_vel;

    // maxvel is an absolute physics cap — feed override cannot exceed it.
    // Matches original planner: tcGetMaxTargetVel returns min(reqvel*scale, maxvel).
    // For arcs, maxvel includes the centripetal jerk velocity limit.
    scaled_vel = fmin(scaled_vel, tc->maxvel);

    // Apply vLimit (max velocity slider) with same conditions as tp.c:
    // - Not a pure rotary move
    // - Not position synchronized (spindle sync)
    if (tp->vLimit > 0.0 && !tcPureRotaryCheck(tc) && tc->synchronized != TC_SYNC_POSITION) {
        return fmin(scaled_vel, tp->vLimit);
    }

    return scaled_vel;
}

// Configuration defaults (can be overridden via HAL/INI)
static int g_optimization_depth = 8;

// Static smoothing data (pre-allocated to avoid reallocation)
static SmoothingData g_smoothing_data;

//============================================================================
// PHASE 3: PREDICTIVE HANDOFF STATE
//============================================================================

// Global configuration (loaded from INI)
static PredictiveHandoffConfig g_handoff_config;


// State for feed override monitoring
static double g_last_feed_scale = 1.0;
static double g_last_replan_time_ms = 0.0;

// Committed feed: the feed scale that has been fully propagated to all segments.
// Updated only when the cursor walk completes a full pass.
// computeRuckigProfiles_9D uses this instead of live feed to avoid overwriting
// profiles that the reactive path (recomputeDownstreamProfiles / cursor walk)
// is actively updating at a newer feed.
static double g_committed_feed = -1.0;   // -1 = uninitialized
static double g_committed_rapid = -1.0;

// Incremental downstream profile recompute cursor
// After a feed change, walk through the queue recomputing profiles a few per tick.
// Both slider values are snapshotted at trigger time so the cursor walk uses a
// consistent target even if the operator keeps moving the slider.
static int g_recompute_cursor = 0;           // 0 = idle, >0 = queue index to resume from
static double g_recompute_feed_scale = 1.0;  // snapshot of feed_scale at trigger time
static double g_recompute_rapid_scale = 1.0; // snapshot of rapid_scale at trigger time
static bool g_recompute_first_batch_done = false; // settling gate: true after first cursor tick completes
static constexpr int RECOMPUTE_MIN_PER_TICK = 1;     // always do at least this many

// Queued feed change: stored when a new feed change arrives while cursor is active.
// Only the next-in-line can be freely overwritten; the working change is locked.
static double g_next_feed_scale = -1.0;    // -1 = no pending change
static double g_next_rapid_scale = -1.0;

// Commit point: queue index where the current feed change takes effect.
// Segments before this index keep old-feed profiles.
static int g_commit_segment = 1;           // default: active+1 (existing behavior)

// Cursor walk throughput tracking (segments per tick, EMA)
static double g_segments_per_tick = 3.0;   // conservative initial estimate

// Ruckig computation timing stats
struct RuckigTimingStats {
    double total_us = 0.0;      // Total computation time in microseconds
    int count = 0;              // Number of computations
    double last_us = 0.0;       // Most recent computation time
    double max_us = 0.0;        // Maximum computation time seen
};
static RuckigTimingStats g_ruckig_timing;

// Adaptive handoff horizon: starts at base_ms, backs off on missed branches
struct AdaptiveHorizon {
    static constexpr double BASE_MS = 5.0;      // Aggressive starting point
    static constexpr double MAX_MS = 100.0;     // Cap to prevent runaway
    static constexpr double BACKOFF = 2.0;      // Multiply on failure
    static constexpr double DECAY = 0.9;        // Multiply on success (decay toward base)

    double current_ms = BASE_MS;
    int consecutive_misses = 0;
    int total_misses = 0;
    int total_takes = 0;

    double get() const { return current_ms; }

    void onMiss() {
        consecutive_misses++;
        total_misses++;
        current_ms = fmin(current_ms * BACKOFF, MAX_MS);
    }

    void onTake() {
        consecutive_misses = 0;
        total_takes++;
        // Decay toward base, but not below
        current_ms = fmax(current_ms * DECAY, BASE_MS);
    }
};
static AdaptiveHorizon g_adaptive_horizon;

// Reference to emcmotStatus for feed scale reading
extern emcmot_status_t *emcmotStatus;

/**
 * @brief Userspace equivalent of etime() - returns current time in seconds
 */
static double etime_user() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

//============================================================================
// INTERNAL HELPER FUNCTIONS
//============================================================================

/**
 * @brief Get per-segment feed scale for planning
 *
 * Returns the correct feed override scale factor for a given segment,
 * based on its motion type. Rapid traverses (G0) use rapid_scale,
 * feed moves (G1/G2/G3) use feed_scale. This avoids the problem
 * where net_feed_scale reflects the currently active segment's motion
 * type, which is wrong for queued segments of a different type.
 *
 * @param tc Trajectory segment (used to determine motion type)
 * @return Feed scale factor (0.0 to 10.0)
 */
static inline double tpGetSegmentFeedScale(TC_STRUCT const * const tc) {
    if (!emcmotStatus) return 1.0;

    double scale;
    if (tc && tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE) {
        scale = emcmotStatus->rapid_scale;
    } else {
        scale = emcmotStatus->feed_scale;
    }
    if (scale < 0.0) scale = 0.0;
    if (scale > 10.0) scale = 10.0;
    return scale;
}

/**
 * @brief Get per-segment feed scale from snapshot values
 *
 * Used by the cursor walk and recomputeDownstreamProfiles to pick the
 * correct snapshotted slider value for a given segment type.  The snapshots
 * are captured once at trigger time so the walk uses a consistent target.
 *
 * @param tc  Trajectory segment (used to determine motion type)
 * @param snap_feed  Snapshotted feed_scale value
 * @param snap_rapid Snapshotted rapid_scale value
 * @return Clamped feed scale for this segment
 */
static inline double tpGetSnapshotFeedScale(TC_STRUCT const * const tc,
                                            double snap_feed, double snap_rapid) {
    double scale;
    if (tc && tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE) {
        scale = snap_rapid;
    } else {
        scale = snap_feed;
    }
    if (scale < 0.0) scale = 0.0;
    if (scale > 10.0) scale = 10.0;
    return scale;
}

/**
 * @brief Get unscaled exit velocity from a profile
 *
 * Reads the profile's final velocity and divides by the feed scale the
 * profile was computed at.  All call sites that need "what base velocity
 * does this profile exit at?" should use this helper so the divisor is
 * always the profile's own feed scale (not some other snapshot).
 */
static inline double profileExitVelUnscaled(const ruckig_profile_t *prof)
{
    double pf = prof->computed_feed_scale;
    if (pf < 0.001) return 0.0;
    return prof->v[RUCKIG_PROFILE_PHASES] / pf;
}

/**
 * @brief Get maximum target velocity for a segment
 *
 * Computes the maximum allowable velocity for this segment
 * considering geometry, feed override, and machine limits.
 *
 * Uses 9D-aware velocity calculation from tc_9d.h
 *
 * @param tc Trajectory component
 * @param max_feed_scale Feed override scale
 * @return Maximum target velocity
 */
static double tcGetPlanMaxTargetVel(TC_STRUCT const * const tc, double max_feed_scale) {
    if (!tc) return 0.0;

    // Use 9D velocity calculation
    return tcGetMaxVel_9D_user(tc, max_feed_scale);
}

/**
 * @brief Get tangential maximum acceleration for a segment (9D wrapper)
 *
 * Returns the maximum acceleration in the direction of motion,
 * considering all axis limits and geometry.
 *
 * Uses userspace 9D-aware calculation
 *
 * @param tc Trajectory component
 * @return Maximum tangential acceleration
 */
static double tcGetTangentialMaxAccel_9D_local(TC_STRUCT const * const tc) {
    if (!tc) return 0.0;

    // Use userspace 9D acceleration calculation
    return tcGetTangentialMaxAccel_9D_user(tc);
}

/**
 * @brief Get final velocity limit for a segment
 *
 * Returns the maximum safe final velocity for this segment,
 * which may be reduced by blending or termination conditions.
 *
 * Uses atomic operations to read from shared_9d structure.
 *
 * @param tc Trajectory component
 * @return Final velocity limit
 */
static double tcGetFinalVelLimit(TC_STRUCT const * const tc) {
    if (!tc) return 0.0;

    // Read final_vel_limit atomically from shared optimization data
    double final_vel_limit = atomicLoadDouble(&tc->shared_9d.final_vel_limit);

    // If not yet set by optimizer, fall back to blend velocity or target
    if (final_vel_limit > 0.0) {
        return final_vel_limit;
    }

    if (tc->blend_vel > 0.0) {
        return tc->blend_vel;
    }

    return tc->target_vel;
}

/**
 * @brief Apply kink velocity limit
 *
 * Reduces velocity at sharp corners (kinks) to avoid violating
 * acceleration limits during direction changes.
 *
 * @param tc Trajectory component
 * @param v_max Unadjusted maximum velocity
 * @return Adjusted velocity accounting for kink
 */
static double applyKinkVelLimit(TC_STRUCT const * const tc, double v_max) {
    if (!tc) return 0.0;

    // TODO: Implement proper kink detection and limiting
    // For now, check if we have a kink velocity computed
    if (tc->kink_vel > 0.0 && tc->kink_vel < v_max) {
        return tc->kink_vel;
    }

    return v_max;
}

/**
 * @brief Apply kink velocity cap with correct feed override handling
 *
 * The backward pass runs at feed_scale=1.0 and stores
 *   final_vel = min(vel_cap, kink_vel)
 * When kink_vel was the binding constraint (not deceleration distance),
 * final_vel equals kink_vel. Simply scaling kink_vel by feed_scale is
 * wrong when feed < 1.0 because the physical kink limit doesn't change.
 *
 * Detection: if unscaled_v_exit >= kink_vel, the backward pass was
 * kink-limited (not decel-limited). In that case, the correct exit
 * velocity is min(max_vel_at_feed, kink_vel).
 *
 * @param scaled_v_exit    Feed-scaled exit velocity (already clamped to max_vel)
 * @param unscaled_v_exit  Unscaled backward pass exit velocity (shared_9d.final_vel)
 * @param max_vel          Feed-scaled programmed max velocity for this segment
 * @param kink_vel         Physical kink limit (tc->kink_vel, negative if unset)
 * @return Adjusted exit velocity
 */
static inline double applyKinkVelCap(double scaled_v_exit, double unscaled_v_exit,
                                      double max_vel, double kink_vel)
{
    if (kink_vel <= 0.0) return scaled_v_exit;

    double result;
    if (unscaled_v_exit >= kink_vel - 1e-6) {
        // Backward pass was kink-limited, not decel-limited.
        // At current feed scale, exit = min(programmed max at feed, physical kink limit)
        result = fmin(max_vel, kink_vel);
    } else {
        // Decel-limited or vel-cap-limited: scale normally, cap by kink
        result = fmin(scaled_v_exit, kink_vel);
    }
    return result;
}

//============================================================================
// CORE OPTIMIZATION FUNCTIONS
//============================================================================

/**
 * @brief Compute minimum distance for jerk-limited velocity change
 *
 * Given start velocity v_s and end velocity v_f, with max acceleration a_max
 * and max jerk j_max, compute the minimum distance required for a jerk-limited
 * deceleration (or acceleration).
 *
 * Two cases:
 *   Triangular accel profile (dv <= a_max²/j_max):
 *     Acceleration never reaches a_max. Peak accel = sqrt(dv * j_max).
 *     t_total = 2 * sqrt(dv / j_max)
 *     d = v_f * t + dv * sqrt(dv / j_max)
 *       (exact: integral of velocity over symmetric jerk profile)
 *
 *   Trapezoidal accel profile (dv > a_max²/j_max):
 *     Acceleration ramps to a_max, holds, then ramps back to 0.
 *     t_ramp = a_max / j_max
 *     dv_ramp = a_max² / j_max  (velocity change during both ramp phases)
 *     t_const = (dv - dv_ramp) / a_max
 *     t_total = 2 * t_ramp + t_const
 *     d = v_f * t_total + 0.5*dv*t_total - (dv² - dv_ramp²)/(6*a_max)
 *       (exact integration, simplified)
 */
static double jerkLimitedBrakingDistance(double v_s, double v_f,
                                         double a_max, double j_max)
{
    double dv = fabs(v_s - v_f);
    if (dv < 1e-9) return 0.0;

    double v_lo = fmin(v_s, v_f);  // lower velocity (endpoint for decel)
    double dv_ramp = a_max * a_max / j_max;  // velocity change in triangular profile

    if (dv <= dv_ramp) {
        // Triangular acceleration profile
        double t_half = sqrt(dv / j_max);
        double t_total = 2.0 * t_half;
        // Exact distance: v_lo * t_total + dv * t_half
        // (the dv*t_half term comes from integrating the triangular velocity bump)
        return v_lo * t_total + dv * t_half;
    } else {
        // Trapezoidal acceleration profile
        double t_ramp = a_max / j_max;
        double t_const = (dv - dv_ramp) / a_max;
        double t_total = 2.0 * t_ramp + t_const;
        // For any symmetric acceleration profile (a(t) = a(T-t)), the area
        // under the velocity-change curve is exactly 0.5 * dv * t_total.
        // Proof: symmetry implies ∫t·a(t)dt = T·dv/2, giving
        // area = ∫(T-t)·a(t)dt = T·dv - T·dv/2 = T·dv/2.
        double area = 0.5 * dv * t_total;
        return v_lo * t_total + area;
    }
}

/**
 * @brief Create a feed hold profile for a segment
 *
 * When feed scale drops below 0.001, the segment needs a minimal profile
 * that indicates a hold state. This creates a uniform 1.0 second profile
 * with feed_scale=0 to signal feed hold to the realtime layer.
 *
 * @param tc Segment to create profile for
 * @param vel_limit Velocity limit to record in profile metadata
 * @param vLimit Machine velocity limit to record in profile metadata
 */
static void createFeedHoldProfile(TC_STRUCT *tc, double vel_limit, double vLimit,
                                   const char *caller = "unknown")
{
    double canonical = tc->shared_9d.canonical_feed_scale;

    // FH_VIOLATION: createFeedHoldProfile should only be called when canonical <= 0.6%
    // (either in Phase 2 after decelerating to 0.1%, or when already at low speed).
    // If canonical > 0.6%, the two-phase guard failed.
    if (canonical > 0.006) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "FH_VIOLATION seg %d: createFeedHoldProfile at canonical=%.3f (should be <=0.006) caller=%s\n",
            tc->id, canonical, caller);
    }

    __atomic_store_n(&tc->shared_9d.profile.valid, 0, __ATOMIC_RELEASE);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    memset(&tc->shared_9d.profile, 0, sizeof(tc->shared_9d.profile));
    tc->shared_9d.profile.duration = 1.0;
    tc->shared_9d.profile.computed_feed_scale = 0.0;
    tc->shared_9d.profile.computed_vel_limit = vel_limit;
    tc->shared_9d.profile.computed_vLimit = vLimit;
    for (int j = 0; j < RUCKIG_PROFILE_PHASES; j++) {
        tc->shared_9d.profile.t[j] = tc->shared_9d.profile.duration / RUCKIG_PROFILE_PHASES;
        tc->shared_9d.profile.t_sum[j] = ((j + 1) * tc->shared_9d.profile.duration) / RUCKIG_PROFILE_PHASES;
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __atomic_store_n(&tc->shared_9d.profile.valid, 1, __ATOMIC_RELEASE);
}

/**
 * @brief Compute max entry velocity given jerk-limited braking to v_f within distance d
 *
 * Binary search: find max v_s such that jerkLimitedBrakingDistance(v_s, v_f) <= d
 * Falls back to trapezoidal formula v² = v_f² + 2*a*d as upper bound.
 */
static double jerkLimitedMaxEntryVelocity(double v_f, double d,
                                           double a_max, double j_max)
{
    if (d <= 0.0) return v_f;

    // Trapezoidal upper bound (always >= jerk-limited answer)
    double vs_trap = sqrt(v_f * v_f + 2.0 * a_max * d);

    // Check if trapezoidal answer is already jerk-feasible
    double d_needed = jerkLimitedBrakingDistance(vs_trap, v_f, a_max, j_max);
    if (d_needed <= d) return vs_trap;

    // Binary search between v_f and vs_trap
    double v_lo = v_f;
    double v_hi = vs_trap;
    for (int i = 0; i < 30; i++) {
        double v_mid = (v_lo + v_hi) * 0.5;
        double d_mid = jerkLimitedBrakingDistance(v_mid, v_f, a_max, j_max);
        if (d_mid <= d) {
            v_lo = v_mid;
        } else {
            v_hi = v_mid;
        }
        if (v_hi - v_lo < 1e-6) break;
    }
    return v_lo;
}

/**
 * @brief Bidirectional reachability cap for entry/exit velocities.
 *
 * Ensures both velocities are physically achievable from each other within
 * the given segment distance, respecting jerk and acceleration limits.
 * By time-reversal symmetry, jerkLimitedMaxEntryVelocity(v, d, a, j) gives
 * the max velocity at the other end — works for both accel and decel.
 * Without this cap, Ruckig returns Working (overshooting profiles).
 */
static void applyBidirectionalReachability(double &v_entry, double &v_exit,
                                            double distance, double max_acc,
                                            double max_jrk)
{
    double v_fwd = jerkLimitedMaxEntryVelocity(v_entry, distance, max_acc, max_jrk);
    double v_bwd = jerkLimitedMaxEntryVelocity(v_exit, distance, max_acc, max_jrk);
    v_exit  = fmin(v_exit,  v_fwd);
    v_entry = fmin(v_entry, v_bwd);
}

/**
 * @brief Check if exit velocity at this queue index is safe for the next segment.
 *
 * "Safe" means the next segment can accept exit_vel as its entry velocity
 * given its kinematic limits and distance.  Used to determine whether a
 * profile chain can be terminated here without creating an impossible
 * junction for the next segment.
 *
 * Returns true when:
 * - This segment has STOP/EXACT term_cond (exit is 0)
 * - End of queue (no next segment)
 * - Next segment can decelerate from exit_vel to its own exit constraint
 *   within its length, at the given feed scale
 */
static bool canStopChainHere(TP_STRUCT *tp, TC_QUEUE_STRUCT *queue,
                              int index, double exit_vel,
                              double snap_feed, double snap_rapid,
                              double default_jerk)
{
    TC_STRUCT *tc = tcqItem_user(queue, index);
    if (!tc) return true;

    // Non-tangent: exit is 0, always safe
    if (tc->term_cond != TC_TERM_COND_TANGENT) return true;

    // Check next segment
    TC_STRUCT *next = tcqItem_user(queue, index + 1);
    if (!next || next->target < 1e-9) return true;  // end of queue

    double next_feed = tpGetSnapshotFeedScale(next, snap_feed, snap_rapid);
    double next_vel_limit = getEffectiveVelLimit(tp, next);
    double next_max_vel = applyVLimit(tp, next, next_vel_limit * next_feed);
    double next_jrk = next->maxjerk > 0 ? next->maxjerk : default_jerk;

    double next_fv = atomicLoadDouble(&next->shared_9d.final_vel);
    double next_exit = (next->term_cond == TC_TERM_COND_TANGENT)
        ? fmin(next_fv * next_feed, next_max_vel) : 0.0;
    if (next->kink_vel > 0) next_exit = fmin(next_exit, next->kink_vel);

    double next_max_entry = jerkLimitedMaxEntryVelocity(
        next_exit, next->target, next->maxaccel, next_jrk);
    next_max_entry = fmin(next_max_entry, next_max_vel);

    // Also consider kink at this junction
    if (tc->kink_vel > 0) next_max_entry = fmin(next_max_entry, tc->kink_vel);

    return (exit_vel <= next_max_entry + 0.5);
}

/**
 * @brief Compute chain exit cap: maximum safe exit velocity for the active
 *        segment, accounting for downstream constraints at the given feed.
 *
 * Walks forward through the queue to find the first "safe endpoint" —
 * a segment whose physics naturally allow stopping (STOP condition, or
 * long enough to decelerate from max_vel to its exit constraint).
 * Then walks backward, cascading reachability limits, to determine the
 * tightest entry constraint that propagates back to the active segment.
 *
 * This replaces the old 1-segment lookahead (which only checked index 1)
 * and prevents the active segment from exiting at a velocity that
 * creates impossible junctions downstream.
 */
static double computeChainExitCap(TP_STRUCT *tp, TC_STRUCT *active_tc,
                                   double new_feed_scale, double default_jerk,
                                   int max_depth = 16, int start_index = 1)
{
    if (active_tc->term_cond != TC_TERM_COND_TANGENT) return 1e10;

    double rapid = emcmotStatus ? emcmotStatus->rapid_scale : 1.0;

    // Phase 1: Walk forward to find chain depth (first safe endpoint).
    // A segment is "safe" if it's STOP/EXACT, end of queue, or long enough
    // to brake from max_vel to its exit constraint within its length.
    int chain_end = 0;
    for (int i = start_index; i <= start_index + max_depth - 1; i++) {
        TC_STRUCT *seg = tcqItem_user(&tp->queue, i);
        if (!seg || seg->target < 1e-9) {
            chain_end = i - 1;
            break;
        }

        chain_end = i;

        // STOP/EXACT condition: exit is 0, natural boundary
        if (seg->term_cond != TC_TERM_COND_TANGENT) break;

        // Long segment: can brake from max_vel to exit constraint
        double seg_feed = tpGetSnapshotFeedScale(seg, new_feed_scale, rapid);
        double seg_vel_limit = getEffectiveVelLimit(tp, seg);
        double seg_max_vel = applyVLimit(tp, seg, seg_vel_limit * seg_feed);
        double seg_jrk = seg->maxjerk > 0 ? seg->maxjerk : default_jerk;
        double seg_fv = atomicLoadDouble(&seg->shared_9d.final_vel);
        double seg_exit = fmin(seg_fv * seg_feed, seg_max_vel);
        if (seg->kink_vel > 0) seg_exit = fmin(seg_exit, seg->kink_vel);

        double brake_dist = jerkLimitedBrakingDistance(
            seg_max_vel, seg_exit, seg->maxaccel, seg_jrk);
        if (seg->target > brake_dist * 1.2) break;  // safe: can absorb any entry
    }

    if (chain_end < start_index) return 1e10;  // no downstream constraints

    // Phase 2: Walk backward from chain_end to start_index, cascading
    // reachability limits.  Each segment's max entry depends on its
    // exit constraint AND what the next segment can accept.
    double next_entry_cap = 1e10;

    for (int i = chain_end; i >= start_index; i--) {
        TC_STRUCT *seg = tcqItem_user(&tp->queue, i);
        if (!seg) continue;

        double seg_feed = tpGetSnapshotFeedScale(seg, new_feed_scale, rapid);
        double seg_vel_limit = getEffectiveVelLimit(tp, seg);
        double seg_max_vel = applyVLimit(tp, seg, seg_vel_limit * seg_feed);
        double seg_jrk = seg->maxjerk > 0 ? seg->maxjerk : default_jerk;

        // This segment's exit constraint (from backward pass + kink)
        double seg_fv = atomicLoadDouble(&seg->shared_9d.final_vel);
        double seg_exit = (seg->term_cond == TC_TERM_COND_TANGENT)
            ? fmin(seg_fv * seg_feed, seg_max_vel) : 0.0;
        if (seg->kink_vel > 0) seg_exit = fmin(seg_exit, seg->kink_vel);

        // Cap exit by what next segment can accept (cascading backward)
        seg_exit = fmin(seg_exit, next_entry_cap);

        // Max entry this segment can accept
        double max_entry = jerkLimitedMaxEntryVelocity(
            seg_exit, seg->target, seg->maxaccel, seg_jrk);
        max_entry = fmin(max_entry, seg_max_vel);

        // Apply kink at the junction before this segment
        TC_STRUCT *prev_seg = tcqItem_user(&tp->queue, i - 1);
        if (prev_seg && prev_seg->kink_vel > 0)
            max_entry = fmin(max_entry, prev_seg->kink_vel);

        next_entry_cap = max_entry;
    }

    return next_entry_cap;
}

/**
 * @brief Compute optimal velocity at segment boundary
 *
 * Implements the three-constraint algorithm:
 * 1. Backward kinematic limit: can we decelerate from entry to v_f_this
 *    within the segment length, respecting jerk limits?
 * 2. Current segment velocity limit
 * 3. Previous segment velocity limit (with kink handling)
 *
 * Returns the minimum of all three constraints.
 */
double tpComputeOptimalVelocity_9D(TC_STRUCT const * const tc,
                                   TC_STRUCT const * const prev_tc,
                                   double v_f_this,
                                   int opt_step)
{
    (void)opt_step;  // Reserved for future multi-pass optimization
    if (!tc || !prev_tc) return 0.0;

    // Get acceleration and jerk limits for this segment
    double acc_this = tcGetTangentialMaxAccel_9D_local(tc);
    if (acc_this <= 0.0) return 0.0;
    double jrk_this = tc->maxjerk > 0 ? tc->maxjerk : g_handoff_config.default_max_jerk;

    // Constraint 1: Backward kinematic limit (jerk-aware)
    // Only apply jerk-aware braking when v_f_this is a real velocity constraint
    // (set by kink computation). When v_f_this=0, the segment just arrived in
    // the queue and its exit velocity hasn't been computed yet — use trapezoidal
    // to avoid over-constraining with an artificially low starting velocity.
    double vs_back;
    if (jrk_this > 0.0 && v_f_this > 1e-6) {
        vs_back = jerkLimitedMaxEntryVelocity(v_f_this, tc->target, acc_this, jrk_this);
    } else {
        // Fallback to trapezoidal if no jerk limit or v_f not yet set
        vs_back = sqrt(v_f_this * v_f_this + 2.0 * acc_this * tc->target);
    }

    // Constraint 2: Current segment velocity limit
    // Use max_feed_scale=1.0: backward pass works in UNSCALED velocity space.
    // Feed override is applied once in the Ruckig profile computation.
    // This ensures shared_9d.final_vel is feed-override-independent and
    // consistent with kink_vel (also absolute/unscaled).
    double vf_limit_this = tcGetPlanMaxTargetVel(tc, 1.0);

    // Constraint 3: Previous segment velocity limit (with kink)
    double v_max_prev = tcGetPlanMaxTargetVel(prev_tc, 1.0);
    double vf_limit_prev = applyKinkVelLimit(prev_tc, v_max_prev);

    // Return minimum of all constraints
    double v_optimal = std::min({vs_back, vf_limit_this, vf_limit_prev});

    return v_optimal;
}

/**
 * @brief Compute limiting velocities using backward propagation
 *
 * This is the core "rising tide" optimization algorithm.
 * Walks backward through the queue and computes maximum safe
 * final velocity for each segment.
 *
 * Also builds time profile for peak smoothing.
 */
int computeLimitingVelocities_9D(TC_QUEUE_STRUCT *queue,
                                  int optimization_depth,
                                  SmoothingData &smoothing)
{
    if (!queue) return -1;
    if (optimization_depth <= 0) return 0;

    static const double LOCAL_VEL_EPSILON = 1e-6;

    // Clear and reset smoothing data
    smoothing = SmoothingData();
    smoothing.reserve(optimization_depth + 10);

    // Initialize with dummy starting point at t=0, v=0
    smoothing.ds.push_back(0.0);
    smoothing.v_smooth.push_back(0.0);
    smoothing.t.push_back(0.0);
    smoothing.t_orig.push_back(0.0);
    smoothing.ignore.push_back(true); // Ignore the dummy point

    // Walk backward through queue, chaining freshly computed exit velocities
    double chained_v_f = -1.0;  // Exit vel computed for tc in previous iteration
    for (int k = 0; k < optimization_depth; ++k) {
        // Access segment at position -k from end using tcqBack
        // tcqBack_user(queue, 0) is the last item, tcqBack_user(queue, -1) is second-to-last, etc.
        TC_STRUCT const *tc = tcqBack_user(queue, -k);
        TC_STRUCT *prev1_tc = tcqBack_user(queue, -(k + 1));

        if (!tc || !prev1_tc) {
            break; // Reached end of queue
        }

        // If current segment is dwell/zero-length, previous must end at v=0
        if (tc->target < 1e-9) {
            atomicStoreDouble(&prev1_tc->shared_9d.final_vel, 0.0);
            chained_v_f = 0.0;
            continue;
        }

        double v_f_prev = 0.0;
        double ds = tc->target;

        // Get final velocity limit for this segment.
        // Chain freshly computed values within this sweep: at k=0 we must read
        // the stored value (nothing computed yet), at k>0 use the min of chained
        // and stored to respect both fresh propagation and external constraints.
        double v_f_stored = tcGetFinalVelLimit(tc);
        double v_f_this = (k == 0) ? v_f_stored
                                   : std::min(chained_v_f, v_f_stored);

        // Compute optimal velocity for previous segment
        v_f_prev = tpComputeOptimalVelocity_9D(tc, prev1_tc, v_f_this, k);

        chained_v_f = v_f_prev;

        // Estimate time for this segment
        // Use average velocity between start and end
        double v_avg_upper_bound = (tcGetFinalVelLimit(prev1_tc) + v_f_prev) / 2.0;
        double v_avg_clamped = std::max(v_avg_upper_bound, LOCAL_VEL_EPSILON);
        double dt = ds / v_avg_clamped;
        double t_prev = smoothing.t.back();

        // Check if this segment should be ignored for smoothing
        // (e.g., rapids, spindle-synchronized moves)
        // TODO: Check tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE
        // TODO: Check tc->synchronized
        bool ignore_smoothing = false; // For now, don't ignore any

        // Add to smoothing data
        smoothing.v_smooth.push_back(v_f_prev);
        smoothing.ds.push_back(ds);
        smoothing.t.push_back(t_prev + dt);
        smoothing.ignore.push_back(ignore_smoothing);
    }

    // Add tail dummy point for peak finding algorithm
    smoothing.v_smooth.push_back(0.0);
    smoothing.ignore.push_back(true);
    smoothing.t.push_back(smoothing.t.back() + 10.0); // Far in the future
    smoothing.ds.push_back(0.0);

    // Save original time profile for comparison
    smoothing.t_orig = smoothing.t;

    return 0;
}

/**
 * @brief Get search end index for optimization
 *
 * Returns the valid range for searching in smoothing data vectors.
 */
static size_t getOptimizationSearchEnd(const SmoothingData &smoothing)
{
    // We pad the data with extra dummy entries for the peak-finding algorithm
    return std::min(smoothing.v_smooth.size() - 1, (size_t)g_optimization_depth);
}

/**
 * @brief Linear interpolation helper
 * Reserved for future use in peak smoothing algorithm
 */
#if 0  // Currently unused - reserved for future peak smoothing implementation
static double interp1(double t0, double t1, double v0, double v1, double t)
{
    static const double LOCAL_POS_EPSILON = 1e-9;
    if (std::abs(t1 - t0) < LOCAL_POS_EPSILON) {
        // If time interval is very short, doesn't matter which side we pick
        return v1;
    }
    return (v1 - v0) / (t1 - t0) * (t - t0) + v0;
}
#endif

/**
 * @brief Apply peak smoothing to velocity profile
 *
 * Flattens local velocity peaks to improve motion smoothness.
 * This reduces jerk and provides more consistent motion.
 *
 * Algorithm:
 * 1. Find local peaks in velocity profile (v[k] >= v[k-1] && v[k] >= v[k+1])
 * 2. Search backward/forward for troughs within t_window
 * 3. Flatten peak to maximum of bracketing edge minimum values
 * 4. Recalculate time base for next iteration
 */
int tpDoPeakSmoothing_9D(SmoothingData &smoothing,
                         int optimization_depth,
                         double t_window)
{
    if (optimization_depth <= 2) return 0; // Need at least 3 points
    if (t_window <= 0.0) return 0;

    // Reference to velocity and time vectors for cleaner code
    SmoothingVector &v_smooth = smoothing.v_smooth;
    const SmoothingVector &t = smoothing.t;

    // Validate vector sizes match
    if (v_smooth.size() != t.size()) {
        return -1; // Error: mismatched vector sizes
    }

    if (v_smooth.empty()) {
        return -1; // Error: empty velocity vector
    }

    size_t search_depth = getOptimizationSearchEnd(smoothing);

    // Tolerance for ripple detection (how much variation we tolerate before declaring a trough)
    static const double tol = 1e-6;

    // Search for local peaks and smooth them
    for (size_t k = 1; k < search_depth; ++k) {
        // Check if this is a local peak
        if (!smoothing.ignore[k] &&
            (v_smooth[k] >= v_smooth[k-1]) &&
            (v_smooth[k] >= v_smooth[k+1]))
        {
            // Found a local peak at k

            // Search backward for the minimum velocity within t_window/2
            double min_v_bwd = v_smooth[k];
            size_t k_bwd = k - 1;

            for (; k_bwd >= 1; --k_bwd) {
                double v_bwd = v_smooth[k_bwd];

                // Stop if we've found a rising edge or ignored segment
                if (v_bwd > (min_v_bwd + tol) || smoothing.ignore[k_bwd]) {
                    break;
                }

                // Stop if we've exceeded the time window
                double t_cutoff = t[k] - t_window / 2.0;
                if (t[k_bwd] < t_cutoff) {
                    break;
                } else {
                    min_v_bwd = std::min(min_v_bwd, v_bwd);
                }
            }
            size_t peak_idx_bwd = k_bwd + 1;

            // Search forward for the minimum velocity within t_window/2
            double min_v_fwd = v_smooth[k];
            size_t k_fwd = k + 1;

            for (; k_fwd < search_depth; ++k_fwd) {
                double v_fwd = v_smooth[k_fwd];

                // Stop if we've found a rising edge or ignored segment
                if (v_fwd > (min_v_fwd + tol) || smoothing.ignore[k_fwd]) {
                    break;
                }

                // Stop if we've exceeded the time window
                double t_cutoff = t[k] + t_window / 2.0;
                if (t[k_fwd] > t_cutoff) {
                    break;
                } else {
                    min_v_fwd = std::min(min_v_fwd, v_fwd);
                }
            }
            size_t peak_idx_fwd = k_fwd - 1;

            // Flatten the peak by forcing intermediate values to the larger of the edge values
            // This ensures we don't create dips below the troughs
            double edge_val = std::max(min_v_bwd, min_v_fwd);

            for (size_t j = peak_idx_bwd; j <= peak_idx_fwd; ++j) {
                if (smoothing.ignore[j]) {
                    return -1; // Error: Cannot smooth an ignored segment
                }

                // Flatten the peak
                if (v_smooth[j] > edge_val) {
                    v_smooth[j] = edge_val;
                }
            }

            // Update k to skip past the smoothed region
            k = std::max(peak_idx_fwd, k);
        }
    }

    // Update timebase for next iteration
    // This is critical - as velocities change, the time profile must update
    static const double LOCAL_VEL_EPSILON = 1e-6;
    for (size_t k = 1; k < v_smooth.size(); ++k) {
        // Average velocity over this segment
        double v_avg_clamped = std::max((v_smooth[k] + v_smooth[k-1]) / 2.0, LOCAL_VEL_EPSILON);

        // Distance for this segment
        double ds_new = smoothing.ds[k];

        // Time = distance / velocity
        double dt_new = ds_new / v_avg_clamped;

        // Accumulate time
        smoothing.t[k] = smoothing.t[k-1] + dt_new;
    }

    return 0;
}

/**
 * @brief Copy Ruckig profile to TC_STRUCT storage
 *
 * Copies Ruckig's phase boundaries directly. These accurately represent the
 * 7-phase jerk-limited trajectory structure.
 */
static void copyRuckigProfile(ruckig::Trajectory<1> &traj, ruckig_profile_t *profile)
{
    // Invalidate profile BEFORE writing data — prevents RT from sampling
    // a partially-written (torn) profile. RT checks valid with acquire
    // semantics and falls back to trapezoidal when invalid.
    __atomic_store_n(&profile->valid, 0, __ATOMIC_RELEASE);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    double traj_duration = traj.get_duration();

    if (traj_duration < 1e-9) {
        profile->duration = traj_duration;
        return;  // valid already 0
    }

    const auto& profiles = traj.get_profiles();
    if (profiles.empty() || profiles[0].empty()) {
        profile->valid = 0;
        return;
    }
    const auto& ruckig_prof = profiles[0][0];

    // 9-phase layout: phases 0-1 = brake pre-trajectory, phases 2-8 = main S-curve.
    // Brake brings initial velocity/acceleration within limits before main profile.
    // Each phase uses native jerk from Ruckig's arrays (not derived from acceleration).
    double brake_dur = ruckig_prof.brake.duration;
    profile->duration = traj_duration;

    // --- Phase durations ---
    // Phases 0-1: brake pre-trajectory (zero-duration when no brake)
    if (brake_dur > 1e-12) {
        profile->t[0] = ruckig_prof.brake.t[0];
        profile->t[1] = (ruckig_prof.brake.t[1] > 0.0) ? ruckig_prof.brake.t[1] : 0.0;
    } else {
        profile->t[0] = 0.0;
        profile->t[1] = 0.0;
    }
    // Phases 2-8: main 7-phase S-curve
    for (int i = 0; i < 7; i++) {
        profile->t[2 + i] = ruckig_prof.t[i];
    }

    // --- Cumulative times ---
    profile->t_sum[0] = profile->t[0];
    for (int i = 1; i < RUCKIG_PROFILE_PHASES; i++) {
        profile->t_sum[i] = profile->t_sum[i - 1] + profile->t[i];
    }

    // --- Native jerk values (NOT derived from acceleration differences) ---
    // This is the key fix: brake phases get brake.j[], main phases get prof.j[]
    if (brake_dur > 1e-12) {
        profile->j[0] = ruckig_prof.brake.j[0];
        profile->j[1] = (ruckig_prof.brake.t[1] > 0.0) ? ruckig_prof.brake.j[1] : 0.0;
    } else {
        profile->j[0] = 0.0;
        profile->j[1] = 0.0;
    }
    for (int i = 0; i < 7; i++) {
        profile->j[2 + i] = ruckig_prof.j[i];
    }

    // --- Boundary p/v/a via traj.at_time() (guaranteed correct) ---
    std::array<double, 1> pos, vel, acc;
    traj.at_time(0.0, pos, vel, acc);
    profile->p[0] = pos[0];
    profile->v[0] = vel[0];
    profile->a[0] = acc[0];

    for (int i = 0; i < RUCKIG_PROFILE_PHASES; i++) {
        traj.at_time(profile->t_sum[i], pos, vel, acc);
        profile->p[i + 1] = pos[0];
        profile->v[i + 1] = vel[0];
        profile->a[i + 1] = acc[0];
    }

    // Increment generation counter so RT can detect profile swap
    // (stopwatch reset mechanism — mirrors manageBranches pattern)
    int gen = __atomic_load_n(&profile->generation, __ATOMIC_RELAXED);
    __atomic_store_n(&profile->generation, gen + 1, __ATOMIC_RELAXED);

    // All data written — publish to RT with release semantics
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __atomic_store_n(&profile->valid, 1, __ATOMIC_RELEASE);
}

/**
 * @brief Parameters for computing and storing a Ruckig profile on a TC segment
 */
struct RuckigProfileParams {
    double v_entry, v_exit;
    double max_vel, max_acc, max_jrk;
    double target_dist;
    double feed_scale, vel_limit, vLimit;
    double desired_fvel;
};

/**
 * @brief Compute a Ruckig profile and store it on a TC segment
 *
 * Sets up Ruckig input from the given parameters, runs the solver, and on
 * success writes metadata + profile data to tc->shared_9d.profile. On failure,
 * marks the profile invalid.
 *
 * Callers still handle velocity scaling, reachability capping, and
 * propagation of exit velocities — this just does the Ruckig call + storage.
 *
 * @param tc Segment to store the profile on
 * @param p Profile parameters (velocities, limits, metadata)
 * @return true on success, false on Ruckig failure (profile marked invalid)
 */
static bool computeAndStoreProfile(TC_STRUCT *tc, const RuckigProfileParams &p)
{
    static ruckig::Ruckig<1> otg(g_handoff_config.servo_cycle_time_sec);
    ruckig::InputParameter<1> input;
    ruckig::Trajectory<1> traj;

    input.current_position = {0.0};
    input.current_velocity = {p.v_entry};
    input.current_acceleration = {0.0};
    input.target_position = {p.target_dist};
    input.target_velocity = {p.v_exit};
    input.target_acceleration = {0.0};
    input.max_velocity = {p.max_vel};
    input.max_acceleration = {p.max_acc};
    input.max_jerk = {p.max_jrk};

    auto result = otg.calculate(input, traj);

    if (result == ruckig::Result::Working || result == ruckig::Result::Finished) {
        tc->shared_9d.profile.computed_feed_scale = p.feed_scale;
        tc->shared_9d.profile.computed_vel_limit = p.vel_limit;
        tc->shared_9d.profile.computed_vLimit = p.vLimit;
        tc->shared_9d.profile.computed_desired_fvel = p.desired_fvel;
        tc->shared_9d.profile.locked = (result == ruckig::Result::Working) ? 1 : 0;
        copyRuckigProfile(traj, &tc->shared_9d.profile);
        return true;
    } else {
        __atomic_store_n(&tc->shared_9d.profile.valid, 0, __ATOMIC_RELEASE);
        return false;
    }
}

/**
 * @brief Find minimum achievable exit velocity without overshooting
 *
 * Given current kinematic state and remaining distance, find the minimum
 * velocity we can reach at the target position without overshooting.
 *
 * This is used for the "achievable feed" cascade: when a segment is too
 * short to achieve the requested feed override, we apply what's achievable
 * and let the next segment continue the deceleration.
 *
 * Algorithm: O(1) Ruckig + O(log n) time-domain search
 * 1. Compute ONE velocity-control trajectory (decel from current to target)
 * 2. Check if total distance <= remaining_dist (target achievable)
 * 3. If not, binary search TIME on the trajectory to find velocity at remaining_dist
 *
 * This replaces the previous O(15 * Ruckig) binary search with O(1 * Ruckig + 20 samples).
 *
 * Phase 4 TODO: With segment blending, this exit velocity becomes the
 * entry velocity constraint for the next segment, enabling efficient
 * cascaded deceleration without the jerk ramp-up/ramp-down overhead
 * at each segment boundary.
 *
 * @param current_vel Current velocity (may exceed target max)
 * @param current_acc Current acceleration
 * @param remaining_dist Distance to segment end
 * @param max_accel Maximum acceleration
 * @param max_jerk Maximum jerk
 * @param max_vel Maximum velocity (profile velocity limit, i.e. new_max_vel)
 * @param target_vel Target exit velocity (e.g., from requested feed scale)
 * @return Achievable exit velocity closest to target_vel given remaining distance.
 *         For deceleration: >= target_vel (can't slow down enough → higher).
 *         For acceleration: <= target_vel (can't speed up enough → lower).
 */
static double findAchievableExitVelocity(double current_vel, double current_acc,
                                          double remaining_dist,
                                          double max_accel, double max_jerk,
                                          double max_vel,
                                          double target_vel)
{
    if (remaining_dist <= 1e-9) return current_vel;  // No room to change velocity

    try {
        static ruckig::Ruckig<1> otg(g_handoff_config.servo_cycle_time_sec);
        ruckig::InputParameter<1> input;
        ruckig::Trajectory<1> traj;

        // Use position-control mode — same as the actual branch profile.
        // This correctly accounts for the dual constraint (reach position AND velocity).
        input.control_interface = ruckig::ControlInterface::Position;
        input.current_position = {0.0};
        input.current_velocity = {current_vel};
        input.current_acceleration = {current_acc};
        input.target_position = {remaining_dist};
        input.target_velocity = {target_vel};
        input.target_acceleration = {0.0};
        input.max_velocity = {max_vel};
        input.max_acceleration = {max_accel};
        input.max_jerk = {max_jerk};

        auto result = otg.calculate(input, traj);
        if (result == ruckig::Result::Working || result == ruckig::Result::Finished) {
            // Verify the profile doesn't reverse direction
            double duration = traj.get_duration();
            std::array<double, 1> p, v, a;
            bool reverses = false;
            for (int i = 1; i <= 16; i++) {
                traj.at_time(duration * i / 16, p, v, a);
                if (v[0] < -0.01) { reverses = true; break; }
            }
            if (!reverses) return target_vel;  // Feasible without reversal
        }

        // Target not feasible — binary search on exit velocity
        double v_lo, v_hi;
        if (current_vel <= target_vel) {
            v_lo = current_vel;   // safe: no change needed
            v_hi = target_vel;    // desired but infeasible
        } else {
            v_lo = target_vel;    // desired but infeasible
            v_hi = current_vel;   // safe: no change needed

            // Validate that v_hi (current_vel) is actually feasible.
            // When current_acc is large and negative, even maintaining
            // current velocity requires reversal — v_hi is NOT safe.
            input.target_velocity = {v_hi};
            result = otg.calculate(input, traj);
            bool v_hi_ok = false;
            if (result == ruckig::Result::Working || result == ruckig::Result::Finished) {
                v_hi_ok = true;
                std::array<double, 1> pv, vv, av;
                double dur = traj.get_duration();
                for (int i = 1; i <= 8; i++) {
                    traj.at_time(dur * i / 8, pv, vv, av);
                    if (vv[0] < -0.01) { v_hi_ok = false; break; }
                }
            }
            if (!v_hi_ok) {
                // current_vel also reverses — search between 0 and current_vel
                v_lo = 0.0;
                v_hi = current_vel;
            }
        }

        // Analytical upper bound: max velocity reachable under constant max_accel
        // (ignores jerk, so always an overestimate). Tightens bracket for short segments.
        double v_energy = sqrt(current_vel * current_vel + 2.0 * max_accel * remaining_dist);
        if (v_energy < v_hi) v_hi = v_energy;

        for (int iter = 0; iter < 20; iter++) {
            if (v_hi - v_lo < 0.5) break;  // 0.5 mm/s precision sufficient (callers use 0.01 tolerance)
            double v_mid = (v_lo + v_hi) * 0.5;
            input.target_velocity = {v_mid};

            result = otg.calculate(input, traj);
            bool feasible = false;
            if (result == ruckig::Result::Working || result == ruckig::Result::Finished) {
                double duration = traj.get_duration();
                std::array<double, 1> p, v, a;
                feasible = true;
                for (int i = 1; i <= 8; i++) {
                    traj.at_time(duration * i / 8, p, v, a);
                    if (v[0] < -0.01) { feasible = false; break; }
                }
            }

            if (current_vel <= target_vel) {
                // Acceleration: feasible → try higher, infeasible → go lower
                if (feasible) v_lo = v_mid; else v_hi = v_mid;
            } else {
                // Deceleration: feasible → try lower, infeasible → go higher
                if (feasible) v_hi = v_mid; else v_lo = v_mid;
            }
        }

        // Return the conservative bound (last known feasible)
        double ret;
        if (current_vel <= target_vel) {
            ret = v_lo;   // Highest confirmed feasible (acceleration)
        } else {
            ret = v_hi;   // Lowest confirmed feasible (deceleration)
        }
        return ret;

    } catch (std::exception &e) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findAchievableExitVelocity exception: %s", e.what());
        return current_vel;
    } catch (...) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findAchievableExitVelocity unknown exception");
        return current_vel;
    }
}

/**
 * @brief Compute kinematic-aware handoff margin for feed override changes
 *
 * Instead of a fixed margin, compute the minimum safe margin based on the
 * kinematic state and required velocity change. This provides:
 * - Fast response for small velocity changes (small margin)
 * - Adequate runway for large velocity changes (larger margin)
 *
 * The margin accounts for:
 * 1. Base RT timing jitter (~5-10ms)
 * 2. Time needed for jerk-limited velocity transition
 * 3. Safety factor for profile stabilization
 *
 * For jerk-limited motion, braking time is approximately:
 *   t_brake ≈ max(2*sqrt(Δv/j), Δv/a)
 *
 * where Δv = current_vel - target_vel, j = max_jerk, a = max_accel
 *
 * @param current_vel Current velocity at estimated handoff point
 * @param target_vel Target velocity after feed change (vel_limit * new_feed_scale)
 * @param max_accel Maximum acceleration limit
 * @param max_jerk Maximum jerk limit
 * @return Minimum safe handoff margin in seconds
 */
static double computeKinematicHandoffMargin(double current_vel, double target_vel,
                                            double max_accel, double max_jerk)
{
    // All timing constants derived from servo cycle time:
    //   1 cycle  — RT detects branch (polls branch->valid each cycle)
    //   1 cycle  — RT takes branch and starts executing new profile
    //   1 cycle  — elapsed_time measurement jitter (read vs actual)
    // = 3 cycles base margin for the handoff mechanics.
    double servo_sec = g_handoff_config.servo_cycle_time_sec;
    double base_margin = servo_sec * 3;
    double min_margin  = base_margin;
    // Cap: half the branch window — beyond this we're waiting too long
    // and should defer to the next segment instead.
    double max_margin  = g_handoff_config.branch_window_ms / 2000.0;
    if (max_margin < min_margin * 2) max_margin = min_margin * 2;

    // If velocity is already at or below target, minimal margin needed
    if (current_vel <= target_vel + 0.01) {
        return min_margin;
    }

    // Delta velocity that needs to be braked
    double delta_v = current_vel - target_vel;

    // Estimate braking time under jerk-limited motion
    // Two regimes:
    // 1. Jerk-dominated (short brakes): t ≈ 2*sqrt(Δv/j)
    // 2. Accel-limited (long brakes): t ≈ Δv/a + a/j
    double t_jerk_dominated = 2.0 * sqrt(delta_v / max_jerk);
    double t_accel_limited = delta_v / max_accel + max_accel / max_jerk;
    double t_brake_estimate = fmax(t_jerk_dominated, t_accel_limited);

    // Margin = base + fraction of brake time for stabilization
    // The 0.5 factor accounts for profile needing time to "settle" after
    // the main deceleration phase (final jerk phase + numerical settling)
    double kinematic_margin = base_margin + t_brake_estimate * 0.5;

    // Clamp to reasonable bounds
    double result = fmax(min_margin, fmin(max_margin, kinematic_margin));

    return result;
}

/**
 * @brief Check whether a segment has enough remaining time for computeBranch()
 *        to place a handoff point.
 *
 * Uses the worst-case handoff margin (the cap from computeKinematicHandoffMargin)
 * so the answer is conservative: if this returns false, computeBranch() will
 * certainly REJECT(seg_done).  If it returns true, computeBranch() may still
 * reject for other reasons, but at least the margin isn't the blocker.
 *
 * @param tc  Active segment to check
 * @return true if the segment has enough remaining time for a branch attempt
 */
static bool segmentHasBranchRoom(const TC_STRUCT *tc)
{
    if (!tc || !tc->shared_9d.profile.valid)
        return false;

    double elapsed = atomicLoadDouble(&tc->elapsed_time);
    double duration = tc->shared_9d.profile.duration;
    double remaining = duration - elapsed;
    if (remaining < 1e-6)
        return false;

    // Worst-case margin: same derivation as computeKinematicHandoffMargin's cap
    double servo_sec = g_handoff_config.servo_cycle_time_sec;
    double worst_margin = g_handoff_config.branch_window_ms / 2000.0;
    if (worst_margin < servo_sec * 6)
        worst_margin = servo_sec * 6;

    return remaining > worst_margin;
}

//============================================================================
// PREDICTIVE HANDOFF IMPLEMENTATION
//============================================================================

/**
 * @brief Set predictive handoff configuration parameters
 *
 * Called from initraj.cc after parsing INI file.
 */
extern "C" void setHandoffConfig(double handoff_horizon_ms,
                                  double branch_window_ms,
                                  double min_buffer_time_ms,
                                  double target_buffer_time_ms,
                                  double max_buffer_time_ms,
                                  double feed_override_debounce_ms,
                                  double servo_cycle_time_sec,
                                  double default_max_jerk)
{
    g_handoff_config.handoff_horizon_ms = handoff_horizon_ms;
    g_handoff_config.branch_window_ms = branch_window_ms;
    g_handoff_config.min_buffer_time_ms = min_buffer_time_ms;
    g_handoff_config.target_buffer_time_ms = target_buffer_time_ms;
    g_handoff_config.max_buffer_time_ms = max_buffer_time_ms;
    g_handoff_config.feed_override_debounce_ms = feed_override_debounce_ms;
    g_handoff_config.servo_cycle_time_sec = servo_cycle_time_sec;
    g_handoff_config.default_max_jerk = default_max_jerk;

    rtapi_print_msg(RTAPI_MSG_DBG,
        "Predictive handoff config: horizon=%.0fms, window=%.0fms, buffer=[%.0f,%.0f,%.0f]ms,"
        "debounce=%.0fms, servo_period=%.3fms, default_jerk=%.0f\n",
        g_handoff_config.handoff_horizon_ms,
        g_handoff_config.branch_window_ms,
        g_handoff_config.min_buffer_time_ms,
        g_handoff_config.target_buffer_time_ms,
        g_handoff_config.max_buffer_time_ms,
        g_handoff_config.feed_override_debounce_ms,
        g_handoff_config.servo_cycle_time_sec * 1000.0,
        g_handoff_config.default_max_jerk);
}

double getDefaultMaxJerk()
{
    return g_handoff_config.default_max_jerk;
}

/**
 * @brief Get minimum buffer time threshold
 */
extern "C" double getBufferMinTimeMs(void)
{
    return g_handoff_config.min_buffer_time_ms;
}

/**
 * @brief Get target buffer time threshold
 */
extern "C" double getBufferTargetTimeMs(void)
{
    return g_handoff_config.target_buffer_time_ms;
}

/**
 * @brief Initialize predictive handoff system
 *
 * Note: Configuration should be set first via setHandoffConfig() from initraj.cc.
 * If not called, defaults from PredictiveHandoffConfig constructor are used.
 */
extern "C" int initPredictiveHandoff(void)
{
    g_last_feed_scale = 1.0;
    g_last_replan_time_ms = 0.0;
    g_recompute_cursor = 0;
    g_recompute_feed_scale = 1.0;
    g_recompute_rapid_scale = 1.0;
    g_recompute_first_batch_done = false;
    g_committed_feed = -1.0;  // Initialized from live on first computeRuckigProfiles_9D call
    g_committed_rapid = -1.0;
    g_next_feed_scale = -1.0;
    g_next_rapid_scale = -1.0;
    g_commit_segment = 1;
    g_segments_per_tick = 3.0;
    return 0;
}

/**
 * @brief Calculate how much buffered motion time remains
 *
 * Sum of remaining time in active segment + all queued segments.
 */
extern "C" double calculateBufferTimeMs(TP_STRUCT *tp)
{
    if (!tp) return 0.0;

    double buffer_time = 0.0;
    TC_QUEUE_STRUCT *queue = &tp->queue;

    int queue_len = tcqLen_user(queue);
    for (int i = 0; i < queue_len; i++) {
        TC_STRUCT *tc = tcqItem_user(queue, i);
        if (!tc) continue;

        if (i == 0) {
            // Active segment: remaining time
            double elapsed = atomicLoadDouble(&tc->elapsed_time);
            if (tc->shared_9d.profile.valid) {
                double remaining = tc->shared_9d.profile.duration - elapsed;
                if (remaining > 0) buffer_time += remaining;
            }
        } else {
            // Queued segment: full duration (if profile valid)
            if (tc->shared_9d.profile.valid) {
                buffer_time += tc->shared_9d.profile.duration;
            } else {
                // Estimate from distance/velocity if profile not yet computed
                double est_vel = tc->reqvel > 0 ? tc->reqvel : 1.0;
                double est_time = tc->target / est_vel;
                buffer_time += est_time;
            }
        }
    }

    return buffer_time * 1000.0;  // Return milliseconds
}

/**
 * @brief Predict kinematic state at a future time
 *
 * Uses sequence number protocol to detect torn reads during RT branch handoff.
 * RT increments copy_sequence before/after profile copy (odd = in progress).
 * If sequence is odd or changes during our read, we retry.
 */
PredictedState predictStateAtTime(TC_STRUCT *tc, double target_elapsed_time)
{
    PredictedState state;

    if (!tc || !tc->shared_9d.profile.valid) {
        return state;
    }

    // Retry loop to handle torn reads during branch handoff
    // RT uses sequence counter: odd = copy in progress, even = stable
    for (int attempts = 0; attempts < 5; attempts++) {
        // Read sequence BEFORE reading any profile data
        unsigned int seq_before = __atomic_load_n(&tc->shared_9d.copy_sequence, __ATOMIC_ACQUIRE);

        // If sequence is odd, RT is mid-copy - spin briefly and retry
        if (seq_before & 1) {
            for (int i = 0; i < 100; i++) {
                __asm__ volatile("" ::: "memory");  // Prevent loop optimization
            }
            continue;
        }

        // Read position base atomically
        double pos_base = atomicLoadDouble(&tc->position_base);

        // Memory barrier to ensure profile is read after pos_base and sequence
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        // Make a local copy of the profile to avoid torn reads during sampling
        ruckig_profile_t profile_copy = tc->shared_9d.profile;

        // Memory barrier before checking sequence again
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        // Read sequence AFTER reading profile data
        unsigned int seq_after = __atomic_load_n(&tc->shared_9d.copy_sequence, __ATOMIC_ACQUIRE);

        // If sequence changed, we got torn data - retry
        if (seq_before != seq_after) {
            continue;
        }

        // Consistent read - use the local copy for sampling
        double sample_time = target_elapsed_time;
        if (sample_time > profile_copy.duration) {
            sample_time = profile_copy.duration;
        }

        // Sample from local copy (avoids any further torn read risk)
        double p, v, a, j;
        int ok = ruckigProfileSample(&profile_copy, sample_time, &p, &v, &a, &j);

        if (ok == 0) {
            state.position = pos_base + p;
            state.velocity = v;
            state.acceleration = a;
            state.jerk = j;
            state.valid = true;
        }
        return state;
    }

    // Failed to get consistent read after retries - return invalid state
    // This is extremely rare and indicates heavy contention
    return state;
}

/**
 * @brief Validate a profile before committing
 *
 * Checks that the profile is well-formed and has expected terminal conditions.
 * Rejects corrupted profiles that could cause velocity explosions.
 *
 * @param profile The profile to validate
 * @param expected_final_vel Expected terminal velocity (0 for position control,
 *                           target_vel for velocity control brake profiles)
 * @return true if profile is valid, false otherwise
 */
static bool validateProfile(const ruckig_profile_t *profile, double expected_final_vel = 0.0)
{
    if (!profile || !profile->valid) return false;

    // Terminal velocity must match expected (with tolerance)
    // For position control: expected ~0
    // For velocity control (brake): expected = target velocity
    // TODO Phase 4: For blending, expected_final_vel = blend velocity into next segment
    const double VEL_TOLERANCE = 0.5;  // mm/s - slightly larger for velocity control
    double v_final = profile->v[RUCKIG_PROFILE_PHASES];
    if (fabs(v_final - expected_final_vel) > VEL_TOLERANCE) return false;

    // Terminal acceleration must be ~0
    const double ACC_TOLERANCE = 10.0;  // mm/s^2
    double a_final = profile->a[RUCKIG_PROFILE_PHASES];
    if (fabs(a_final) > ACC_TOLERANCE) return false;

    // Duration must be positive
    if (profile->duration <= 0.0) return false;

    return true;
}

/**
 * @brief Commit a branch for RT to take
 *
 * Branch/Merge protocol:
 * - Userspace writes branch data and sets valid=1
 * - RT takes branch when elapsed_time reaches handoff_time (if before window_end)
 * - RT sets taken=1 when it takes the branch
 * - Userspace merges (sets canonical_feed_scale) when it sees taken=1
 * - Userspace clears valid=0 and taken=0 after merge
 *
 * Two-stage support:
 * - If brake_profile is provided, RT executes brake first, then main profile
 * - brake_end_position marks where brake ends and main begins
 *
 * @return true if committed, false if previous branch still pending
 */
bool commitBranch(shared_optimization_data_9d_t *shared,
                  const ruckig_profile_t *main_profile,
                  const ruckig_profile_t *brake_profile,  // NULL if no brake needed
                  double brake_target_vel,                // Expected final velocity of brake (0 if no brake)
                  double brake_end_position,              // Position after brake completes
                  double handoff_time,
                  double handoff_position,
                  double feed_scale,
                  double window_end_time,
                  double expected_exit_vel)                 // Expected final velocity of main profile
{
    if (!shared || !main_profile) return false;

    // Validate main profile exit velocity
    // For tangent transitions, exit velocity is non-zero (final_vel * feed_scale)
    if (!validateProfile(main_profile, expected_exit_vel)) {
        return false;
    }

    // Validate brake profile - should end at target velocity (velocity control)
    if (brake_profile && !validateProfile(brake_profile, brake_target_vel)) {
        return false;
    }

    // Check branch state
    int valid = __atomic_load_n(&shared->branch.valid, __ATOMIC_ACQUIRE);
    int taken = __atomic_load_n(&shared->branch.taken, __ATOMIC_ACQUIRE);

    if (taken) {
        // Branch is being executed by RT - don't overwrite.
        // During two-stage brake: merge sets valid=0 but keeps taken=1.
        // RT reads branch.profile and brake_end_position at brake→main
        // transition — overwriting causes position/profile mismatch.
        return false;
    }

    // If previous branch is pending (valid && !taken), invalidate and overwrite
    // This handles rapid feed changes - newer feed always wins
    if (valid && !taken) {
        __atomic_store_n(&shared->branch.valid, 0, __ATOMIC_RELEASE);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
    }

    // Write all branch data (not yet visible to RT)
    shared->branch.profile = *main_profile;
    shared->branch.handoff_time = handoff_time;
    shared->branch.handoff_position = handoff_position;
    shared->branch.feed_scale = feed_scale;
    shared->branch.window_end_time = window_end_time;

    // Two-stage brake handling
    if (brake_profile) {
        shared->branch.brake_profile = *brake_profile;
        shared->branch.has_brake = 1;
        shared->branch.brake_end_position = brake_end_position;
    } else {
        shared->branch.has_brake = 0;
        shared->branch.brake_end_position = 0.0;
    }

    // Clear flags (in case they were set from previous branch)
    __atomic_store_n(&shared->branch.taken, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&shared->branch.brake_done, 0, __ATOMIC_RELEASE);

    // Full memory barrier
    __atomic_thread_fence(__ATOMIC_RELEASE);

    // Set valid flag last - makes everything above visible atomically
    __atomic_store_n(&shared->branch.valid, 1, __ATOMIC_RELEASE);

    return true;
}

/**
 * @brief Estimate the commit point for a feed override change.
 *
 * Finds the earliest queue index where we can guarantee all segments from
 * there forward will be recomputed before RT arrives. If the active segment
 * has enough remaining time for a handoff (adaptive horizon + 3 servo cycles),
 * returns 1 (branch on active). Otherwise scans forward comparing RT arrival
 * time vs our estimated recompute time.
 *
 * Segment durations are scaled by the ratio of their computed feed to the
 * new requested feed, so RT arrival estimates reflect the actual speed
 * segments will run at after recomputation.
 */
static int estimateCommitSegment(TP_STRUCT *tp, double new_feed)
{
    TC_QUEUE_STRUCT *queue = &tp->queue;
    int queue_len = tcqLen_user(queue);
    if (queue_len < 3) return 1;  // short queue: start from active+1

    TC_STRUCT *active = tcqItem_user(queue, 0);
    if (!active) return 1;

    // Determine how much time RT needs to finish the active segment.
    // The train is "parked" if: no profile, zero duration, or schedule expired.
    // In all cases, treat as infinite arrival time → branch on active (return 1).
    double elapsed = atomicLoadDouble(&active->elapsed_time);
    double active_remaining = 0.0;
    if (active->shared_9d.profile.valid && active->shared_9d.profile.duration > 1e-6) {
        active_remaining = active->shared_9d.profile.duration - elapsed;
        if (active_remaining < 0) active_remaining = 0;
        // NOTE: Do NOT scale active_remaining by feed ratio here.
        // The branch hasn't happened yet — RT is still running the old profile
        // at the old speed. The wall-clock time until this segment ends is
        // exactly (duration - elapsed), unscaled. Downstream segments (below)
        // ARE scaled because they'll be recomputed at new_feed.
    } else {
        return 1;  // no schedule → train is parked, must branch on active
    }
    if (active_remaining < 1e-6) return 1;  // schedule expired → branch on active

    // Quick check: does the active segment have enough remaining time for
    // computeBranch() to place a handoff?  Uses worst-case margin so we
    // don't send segments that will certainly be rejected.
    if (segmentHasBranchRoom(active)) {
        return 1;
    }

    double servo_sec = g_handoff_config.servo_cycle_time_sec;
    double cycle_time = tp->cycleTime > 0 ? tp->cycleTime : servo_sec;
    // Safety margin: 10 servo cycles to absorb timing jitter between
    // userspace computation and RT execution.
    double margin = servo_sec * 10;
    double spt = g_segments_per_tick > 0.5 ? g_segments_per_tick : 1.0;

    double rt_time = active_remaining;  // cumulative time until RT reaches segment i

    for (int i = 1; i < queue_len; i++) {
        int segs_to_buffer = queue_len - i;
        double recompute_time = (segs_to_buffer / spt) * cycle_time + margin;

        if (rt_time > recompute_time) {
            // We're in this scan because the active segment failed
            // segmentHasBranchRoom — it's too short for a branch.
            // Never return 1 here: manageBranches would interpret it as
            // "branch on active", which will certainly fail.  Return ≥2
            // to force deferral; the active finishes in <25ms and the
            // next segment becomes branchable.
            int commit = (i >= 2) ? i : 2;
            return commit;
        }

        // Add segment i's duration to RT arrival time.
        // Scale by feed ratio: the stored duration reflects the old feed, but
        // after recompute these segments will run at new_feed.
        TC_STRUCT *tc = tcqItem_user(queue, i);
        if (tc && tc->shared_9d.profile.valid && tc->shared_9d.profile.duration > 1e-6) {
            double duration = tc->shared_9d.profile.duration;
            double seg_feed = tc->shared_9d.profile.computed_feed_scale;
            if (seg_feed > 0.001 && new_feed > 0.001) {
                duration *= seg_feed / new_feed;
            }
            rt_time += duration;
        } else {
            return i;  // blank timetable → train won't pass here soon, commit here
        }
    }

    return queue_len - 1;  // fallback: commit at last segment
}

/**
 * @brief Invalidate next N segments for cascade re-optimization
 */
void invalidateNextNSegments(TP_STRUCT *tp, int n, int start_index)
{
    if (!tp || n <= 0) return;

    TC_QUEUE_STRUCT *queue = &tp->queue;
    int queue_len = tcqLen_user(queue);
    int invalidated = 0;

    // Invalidate n segments starting from start_index (default: skip active at 0)
    for (int i = start_index; i < queue_len && invalidated < n; i++) {
        TC_STRUCT *tc = tcqItem_user(queue, i);
        if (!tc) continue;

        // Mark for re-optimization
        // DON'T invalidate profile.valid immediately!
        // Let RT use old profile until new one is ready
        __atomic_store_n((int*)&tc->shared_9d.optimization_state,
                         TC_PLAN_UNTOUCHED, __ATOMIC_RELEASE);

        invalidated++;
    }
}

/**
 * @brief Synchronously recompute Ruckig profiles for downstream segments
 *
 * Called immediately after a branch is committed or merged, so that downstream
 * segments have fresh profiles before RT reaches them. This eliminates the race
 * condition where RT starts executing a stale profile (computed at old feed scale)
 * before the periodic optimizer gets around to recomputing it.
 *
 * Time-budgeted: computes as many segments as fit within the time budget
 * (half the servo cycle), returns the number actually recomputed so the
 * cursor walk can continue from where this left off.
 *
 * @param tp Trajectory planner
 * @param snap_feed Snapshotted feed_scale (for feed moves)
 * @param snap_rapid Snapshotted rapid_scale (for traverses)
 * @param start_index Queue index to start from (default 1 = first after active)
 * @return Number of segments recomputed
 */
static int recomputeDownstreamProfiles(TP_STRUCT *tp,
                                       double snap_feed, double snap_rapid,
                                       int start_index = 1)
{
    if (!tp) return 0;

    TC_QUEUE_STRUCT *queue = &tp->queue;
    int queue_len = tcqLen_user(queue);
    if (start_index >= queue_len) return 0;

    // Run backward pass at the new feed before writing profiles.
    // Without this, final_vel values reflect the OLD feed's backward pass,
    // and the optimizer will rewrite our profiles with different velocities
    // once it runs its own backward pass at the new committed feed.
    int depth = queue_len;
    if (depth > MAX_LOOKAHEAD_DEPTH) depth = MAX_LOOKAHEAD_DEPTH;
    computeLimitingVelocities_9D(queue, depth, g_smoothing_data);
    applyLimitingVelocities_9D(queue, g_smoothing_data, depth);

    // Seed entry velocity from the predecessor of start_index.
    // Always use final_vel * new_feed rather than reading the predecessor's
    // profile exit velocity. The predecessor's profile may still be at the OLD
    // feed (branch not yet taken by RT, or commit-point predecessor not yet
    // rewritten by optimizer). final_vel is feed-independent (backward pass
    // computes it at feed=1.0), so final_vel * new_feed predicts the exit
    // velocity that will be achieved at the new feed.
    TC_STRUCT *prev = tcqItem_user(queue, start_index - 1);
    if (!prev) return 0;

    double prev_exit_vel_scaled;
    if (prev->term_cond == TC_TERM_COND_TANGENT) {
        double prev_feed = tpGetSnapshotFeedScale(prev, snap_feed, snap_rapid);
        double final_vel = atomicLoadDouble(&prev->shared_9d.final_vel);
        prev_exit_vel_scaled = final_vel * prev_feed;
        // Cap by kink_vel if set
        if (prev->kink_vel > 0)
            prev_exit_vel_scaled = fmin(prev_exit_vel_scaled, prev->kink_vel);
        // On the merge path (start_index == 1), the branch has been taken
        // by RT. achieved_exit_vel IS the actual profile exit velocity —
        // use it directly instead of the final_vel * feed estimate.
        if (start_index == 1 && prev->shared_9d.achieved_exit_vel > 0) {
            prev_exit_vel_scaled = prev->shared_9d.achieved_exit_vel;
        }
    } else {
        prev_exit_vel_scaled = 0.0;
    }

    // Get default jerk
    double default_jerk = prev->maxjerk > 0 ? prev->maxjerk : g_handoff_config.default_max_jerk;
    if (default_jerk < 1.0) default_jerk = g_handoff_config.default_max_jerk;

    int recomputed = 0;
    double budget_us = g_handoff_config.servo_cycle_time_sec * 0.5e6; // half servo cycle
    double tick_start = etime_user();
    for (int i = start_index; i < queue_len; i++) {
        // Time-budget check: after minimum 3 segments, stop if budget exhausted
        // AND we've reached a safe endpoint (next segment can handle our exit).
        // Without the safe-endpoint check, stopping mid-chain leaves downstream
        // segments with stale profiles that expect different entry velocities.
        // Hard cap at 32 segments to prevent unbounded computation.
        if (recomputed >= 32) break;
        if (recomputed >= 3) {
            double elapsed_us = (etime_user() - tick_start) * 1e6;
            if (elapsed_us >= budget_us) {
                // Only stop if previous segment's exit is safe for this one
                if (canStopChainHere(tp, queue, i - 1, prev_exit_vel_scaled,
                                      snap_feed, snap_rapid, default_jerk))
                    break;
            }
        }
        TC_STRUCT *tc = tcqItem_user(queue, i);
        if (!tc || tc->target < 1e-9) {
            prev_exit_vel_scaled = 0.0;
            continue;
        }

        // Per-segment feed scale from snapshots
        double feed_scale = tpGetSnapshotFeedScale(tc, snap_feed, snap_rapid);

        // Entry velocity comes from previous segment's exit (already scaled)
        double scaled_v_entry = prev_exit_vel_scaled;

        // Get previous segment's kink_vel for entry cap check
        TC_STRUCT *prev_tc = (i > 0) ? tcqItem_user(queue, i - 1) : NULL;
        double prev_kink = (prev_tc && prev_tc->kink_vel > 0) ? prev_tc->kink_vel : -1.0;

        // Exit velocity: unscaled final_vel, scale it now
        double v_exit_unscaled = (tc->term_cond == TC_TERM_COND_TANGENT)
            ? atomicLoadDouble(&tc->shared_9d.final_vel) : 0.0;

        // Store entry velocity for RT layer (unscaled)
        double unscaled_entry = (feed_scale > 0.001) ? scaled_v_entry / feed_scale : 0.0;
        atomicStoreDouble(&tc->shared_9d.entry_vel, unscaled_entry);

        double vel_limit = getEffectiveVelLimit(tp, tc);
        double max_jrk = tc->maxjerk > 0 ? tc->maxjerk : default_jerk;

        if (feed_scale < 0.001) {
            // Don't clobber a valid non-hold profile with a feed-hold
            // profile.  Same guard as computeRuckig forward/backward.
            if (tc->shared_9d.profile.valid &&
                tc->shared_9d.profile.computed_feed_scale > 0.001) {
                prev_exit_vel_scaled = 0.0;
                continue;
            }
            createFeedHoldProfile(tc, vel_limit, tp->vLimit, "recomputeDownstream");
            prev_exit_vel_scaled = 0.0;
            atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_FINALIZED);
        } else {
            double max_vel = applyVLimit(tp, tc, vel_limit * feed_scale);
            double max_acc = tcGetTangentialMaxAccel_9D_user(tc);

            scaled_v_entry = fmin(scaled_v_entry, max_vel);

            // Cap entry velocity by previous segment's kink limit
            if (prev_kink > 0)
                scaled_v_entry = fmin(scaled_v_entry, prev_kink);

            double scaled_v_exit = fmin(v_exit_unscaled * feed_scale, max_vel);
            scaled_v_exit = applyKinkVelCap(scaled_v_exit, v_exit_unscaled, max_vel, tc->kink_vel);
            double desired_fvel_for_profile = scaled_v_exit;

            applyBidirectionalReachability(scaled_v_entry, scaled_v_exit,
                tc->target, max_acc, max_jrk);

            // Skip if profile already matches: same feed, entry velocity, and exit target.
            // Avoids redundant Ruckig solves when forward pass already wrote correct profiles.
            if (tc->shared_9d.profile.valid &&
                fabs(tc->shared_9d.profile.computed_feed_scale - feed_scale) < 0.005 &&
                fabs(tc->shared_9d.profile.v[0] - scaled_v_entry) < 0.5 &&
                fabs(tc->shared_9d.profile.computed_desired_fvel - desired_fvel_for_profile) < 0.5) {
                prev_exit_vel_scaled = tc->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
                continue;
            }

            try {
                RuckigProfileParams rp = {scaled_v_entry, scaled_v_exit,
                    max_vel, max_acc, max_jrk, tc->target,
                    feed_scale, vel_limit, tp->vLimit, desired_fvel_for_profile};

                if (computeAndStoreProfile(tc, rp)) {
                    prev_exit_vel_scaled = tc->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
                    atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_FINALIZED);

                    // One-step backtrack: fix backward reachability gap
                    // Skip active segment — rewriting triggers STOPWATCH_RESET
                    // with velocity discontinuity worse than the chain gap.
                    TC_STRUCT *prev_seg = (i > start_index) ? tcqItem_user(queue, i - 1) : NULL;
                    if (prev_seg && !prev_seg->active
                        && prev_seg->term_cond == TC_TERM_COND_TANGENT
                        && prev_seg->shared_9d.profile.valid) {
                        double prev_v_end = prev_seg->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
                        double curr_v0 = tc->shared_9d.profile.v[0];
                        if (prev_v_end > curr_v0 + 0.01) {
                            double p_entry = prev_seg->shared_9d.profile.v[0];
                            double p_vel_limit = getEffectiveVelLimit(tp, prev_seg);
                            double p_max_vel = applyVLimit(tp, prev_seg, p_vel_limit * feed_scale);
                            double p_max_acc = tcGetTangentialMaxAccel_9D_user(prev_seg);
                            double p_max_jrk = prev_seg->maxjerk > 0 ? prev_seg->maxjerk : default_jerk;
                            double p_exit = curr_v0;
                            applyBidirectionalReachability(p_entry, p_exit,
                                prev_seg->target, p_max_acc, p_max_jrk);
                            RuckigProfileParams rp_prev = {p_entry, p_exit,
                                p_max_vel, p_max_acc, p_max_jrk, prev_seg->target,
                                feed_scale, p_vel_limit, tp->vLimit, p_exit};
                            if (computeAndStoreProfile(prev_seg, rp_prev)) {
                                atomicStoreInt((int*)&prev_seg->shared_9d.optimization_state,
                                    TC_PLAN_FINALIZED);
                            }
                        }
                    }
                } else {
                    prev_exit_vel_scaled = 0.0;
                    rtapi_print_msg(RTAPI_MSG_ERR, "Downstream recompute fail seg=%d",
                        tc->id);
                }
            } catch (std::exception &e) {
                __atomic_store_n(&tc->shared_9d.profile.valid, 0, __ATOMIC_RELEASE);
                prev_exit_vel_scaled = 0.0;
                rtapi_print_msg(RTAPI_MSG_ERR, "Downstream recompute exception seg=%d: %s",
                    tc->id, e.what());
            }
        }

        recomputed++;
    }

    return recomputed;
}

/**
 * @brief Check if a profile has negative velocity (backward motion)
 *
 * Scans the profile at uniform time intervals and returns true if any
 * sample has velocity below the threshold. Used as a safety gate to
 * prevent committing profiles that would drive the machine backward.
 */
static bool profileHasNegativeVelocity(const ruckig_profile_t *profile, double threshold = -0.01) {
    if (!profile->valid) return false;
    double dur = profile->duration;
    if (dur < 1e-9) return false;
    int n_pts = 40;
    for (int i = 0; i <= n_pts; i++) {
        double t = dur * i / n_pts;
        double sp, sv, sa, sj;
        ruckigProfileSample(const_cast<ruckig_profile_t*>(profile), t, &sp, &sv, &sa, &sj);
        if (sv < threshold) return true;
    }
    return false;
}

/**
 * @brief Estimate achievable exit velocity at a given feed scale
 *
 * Lightweight estimator used by the feed-limiting binary search.
 * Uses kinematic formulas for brake/stabilize distance estimation and
 * Ruckig-based findAchievableExitVelocity for the main-stage exit.
 * Does NOT build full profiles — just estimates whether a given feed
 * can produce an exit velocity within the specified hard limit.
 *
 * @param tp Trajectory planner
 * @param tc Current segment
 * @param entry_vel Velocity at handoff point (from predicted state)
 * @param entry_acc Acceleration at handoff point
 * @param remaining_dist Distance from handoff to segment end
 * @param vel_limit Base velocity limit (before feed scaling)
 * @param feed_scale Feed scale to test
 * @param default_jerk Default jerk limit
 * @param exit_hard_limit Hard exit velocity constraint (kink or downstream cap)
 * @return Estimated achievable exit velocity at this feed
 */
static double estimateExitAtFeed(TP_STRUCT const *tp, TC_STRUCT const *tc,
                                  double entry_vel, double entry_acc,
                                  double remaining_dist,
                                  double vel_limit, double feed_scale,
                                  double default_jerk,
                                  double exit_hard_limit)
{
    double max_vel = applyVLimit(tp, tc, vel_limit * feed_scale);

    double eff_entry_vel = entry_vel;
    double eff_entry_acc = entry_acc;
    double eff_remaining = remaining_dist;

    // Determine if this feed would need a brake or stabilize stage
    if (entry_vel > max_vel + 0.01) {
        // Two-stage brake: estimate distance kinematically
        double brake_dist = jerkLimitedBrakingDistance(entry_vel, max_vel,
                                                       tc->maxaccel, default_jerk);
        eff_remaining = remaining_dist - brake_dist;
        if (eff_remaining < 1e-6) return entry_vel;  // Can't brake in time
        eff_entry_vel = max_vel;
        eff_entry_acc = 0.0;
    } else if (fabs(entry_acc) > 0.5 * tc->maxaccel) {
        // Stabilize: estimate acceleration-arrest distance
        double stab_vel = fmin(entry_vel, max_vel);
        if (stab_vel < 0.0) stab_vel = 0.0;
        double t_arrest = fabs(entry_acc) / default_jerk;
        double stab_dist = fmax(stab_vel * t_arrest, 0.0);
        eff_remaining = remaining_dist - stab_dist;
        if (eff_remaining < 1e-6) return entry_vel;
        eff_entry_vel = stab_vel;
        eff_entry_acc = 0.0;
    }

    // Target: the hard limit or max_vel, whichever is lower
    double target = fmin(exit_hard_limit, max_vel);

    return findAchievableExitVelocity(eff_entry_vel, eff_entry_acc, eff_remaining,
                                       tc->maxaccel, default_jerk, max_vel, target);
}

/**
 * @brief Write an alternate entry profile for the next segment.
 *
 * When a brake branch on the current segment changes its exit velocity,
 * the next segment's pre-computed profile has a stale entry velocity.
 * This writes an alternate profile with v0 = brake's exit velocity.
 * RT picks whichever profile (main or alt_entry) has v0 closer to the
 * actual junction velocity — smooth regardless of whether brake was taken.
 *
 * Called BEFORE commitBranch so the cushion is in place before the jump.
 */
static void writeAltEntry(TP_STRUCT *tp, TC_STRUCT *tc,
                           double branch_exit_vel, double new_feed_scale)
{
    if (tc->term_cond != TC_TERM_COND_TANGENT) return;

    TC_STRUCT *next = tcqItem_user(&tp->queue, 1);
    if (!next) return;

    // Skip if alt_entry wouldn't differ meaningfully from main profile
    if (__atomic_load_n(&next->shared_9d.profile.valid, __ATOMIC_ACQUIRE)) {
        double main_v0 = next->shared_9d.profile.v[0];
        if (fabs(branch_exit_vel - main_v0) < 0.1) return;
    }

    double next_vel_limit = getEffectiveVelLimit(tp, next);
    double next_max_vel = next_vel_limit * new_feed_scale;
    next_max_vel = applyVLimit(tp, next, next_max_vel);
    double next_jerk = next->maxjerk > 0 ? next->maxjerk : g_handoff_config.default_max_jerk;

    // Determine target exit velocity for next segment
    double unscaled_fv = atomicLoadDouble(&next->shared_9d.final_vel);
    double target_exit_vel = (next->term_cond == TC_TERM_COND_TANGENT)
        ? fmin(unscaled_fv * new_feed_scale, next_max_vel)
        : 0.0;
    target_exit_vel = applyKinkVelCap(target_exit_vel, unscaled_fv, next_max_vel, next->kink_vel);

    double achievable_exit = findAchievableExitVelocity(
        branch_exit_vel, 0.0,
        next->target, next->maxaccel, next_jerk,
        next_max_vel, target_exit_vel);

    // Compute Ruckig profile for next segment with adjusted entry velocity
    try {
        ruckig::Ruckig<1> otg(g_handoff_config.servo_cycle_time_sec);
        ruckig::InputParameter<1> input;
        ruckig::Trajectory<1> traj;

        input.current_position = {0.0};
        input.current_velocity = {branch_exit_vel};
        input.current_acceleration = {0.0};
        input.target_position = {next->target};
        input.target_velocity = {achievable_exit};
        input.target_acceleration = {0.0};
        input.max_velocity = {next_max_vel};
        input.max_acceleration = {next->maxaccel};
        input.max_jerk = {next_jerk};

        auto result = otg.calculate(input, traj);
        if (result != ruckig::Result::Working && result != ruckig::Result::Finished)
            return;

        ruckig_profile_t alt_profile;
        memset(&alt_profile, 0, sizeof(alt_profile));
        copyRuckigProfile(traj, &alt_profile);
        if (!alt_profile.valid) return;
        if (profileHasNegativeVelocity(&alt_profile)) return;

        alt_profile.computed_feed_scale = new_feed_scale;
        alt_profile.computed_vel_limit = next_vel_limit;
        alt_profile.computed_vLimit = tp->vLimit;

        // Write alt_entry — cushion in place before the jump
        next->shared_9d.alt_entry.profile = alt_profile;
        next->shared_9d.alt_entry.v0 = branch_exit_vel;
        __atomic_thread_fence(__ATOMIC_RELEASE);
        __atomic_store_n(&next->shared_9d.alt_entry.valid, 1, __ATOMIC_RELEASE);
    } catch (...) {
        // Alt-entry is best-effort; failure falls back to v0 correction in RT
    }
}

/**
 * @brief Compute a branch trajectory for feed override change
 *
 * Branch/Merge architecture with achievable feed cascade:
 * - Computes a speculative trajectory starting from predicted state at handoff
 * - If requested feed isn't achievable (segment too short), computes best achievable
 * - Commits as a branch that RT can choose to take
 * - RT has a window [handoff_time, window_end_time) to take the branch
 * - If RT doesn't take it by window_end, the branch is stale and discarded
 *
 * Achievable Feed Cascade:
 * - Instead of "can I apply 50%?", asks "what's the best I CAN achieve?"
 * - Applies achievable portion, lets next segment continue deceleration
 * - Never overshoots - physically safe for CNC
 *
 * Feed Limiting:
 * - When requested feed would produce exit velocity exceeding junction constraints
 *   (kink velocity or downstream reachability), binary-searches for the maximum
 *   feed that keeps exit within limits.
 * - Always commits a feasible profile at a reduced feed, instead of rejecting
 *   and leaving the old (potentially worse) profile running.
 *
 * @return true if branch was successfully computed and committed
 */
static void computeSpillOver(const ruckig_profile_t *profile,
                             double remaining_dist,
                             double *spill_vel,
                             double *spill_acc);

bool computeBranch(TP_STRUCT *tp, TC_STRUCT *tc, double new_feed_scale)
{
    if (!tp || !tc) return false;

    // Get base kinematic limits (vLimit applied after feed scaling via applyVLimit)
    double vel_limit = getEffectiveVelLimit(tp, tc);
    double default_jerk = tc->maxjerk > 0 ? tc->maxjerk : g_handoff_config.default_max_jerk;

    // Check if we're resuming from stopped state
    // Two cases marked by computed_feed_scale:
    // -1.0 = pause/abort stopped (cycle-by-cycle), profile is invalid
    //  0.0 = feed hold profile (Ruckig computed), profile is valid
    double feed_scale = tc->shared_9d.profile.computed_feed_scale;
    bool resuming_from_pause = (feed_scale < -0.5);           // -1.0 marker
    bool resuming_from_feed_hold = (feed_scale >= -0.5 && feed_scale < 0.001);  // 0.0 marker

    double elapsed = atomicLoadDouble(&tc->elapsed_time);
    double profile_duration = tc->shared_9d.profile.duration;
    double remaining_time = profile_duration - elapsed;

    // Initial pessimistic profiles (from forward pass with v_exit=0) have
    // computed_feed_scale=0 just like real feed-hold profiles, but the machine
    // is actively decelerating — not stopped. Probe the profile at elapsed time:
    // if velocity is significant, this is NOT a feed-hold resume but a normal
    // profile that needs standard handoff timing. Misclassifying it causes
    // Ruckig to receive (v>0, a<<0) as initial state → negative velocity profile.
    if (resuming_from_feed_hold) {
        PredictedState probe = predictStateAtTime(tc, elapsed);
        if (probe.valid && probe.velocity > 1.0) {
            resuming_from_feed_hold = false;
        }
    }

    // Get current velocity estimate for kinematic margin calculation
    // We need to know velocity early to compute proper handoff margin
    double current_vel_estimate = 0.0;
    if (!resuming_from_feed_hold && !resuming_from_pause) {
        PredictedState current_state = predictStateAtTime(tc, elapsed);
        if (current_state.valid) {
            current_vel_estimate = current_state.velocity;
        }
    }

    // Compute kinematic-aware handoff margin based on velocity change required
    // Small velocity changes -> small margin (fast response)
    // Large velocity changes -> larger margin (stability)
    double new_max_vel = applyVLimit(tp, tc, vel_limit * new_feed_scale);
    double min_handoff_margin = computeKinematicHandoffMargin(
        current_vel_estimate, new_max_vel, tc->maxaccel, default_jerk);

    double horizon_sec = g_adaptive_horizon.get() / 1000.0;
    double window_sec = g_handoff_config.branch_window_ms / 1000.0;
    double handoff_time;
    double window_end_time;

    if (resuming_from_feed_hold || resuming_from_pause) {
        // RESUME FROM STOPPED: Machine is stopped, need immediate handoff.
        // Set handoff_time = elapsed so RT takes branch immediately.
        handoff_time = elapsed;
        window_end_time = handoff_time + window_sec + 1.0;  // Extended window for resume
    } else {
        // Normal case: calculate handoff time based on remaining profile time

        // Adaptive horizon: starts at 5ms, backs off on missed branches
        // This allows fast response while self-tuning if system is slow

        // For short segments, place handoff as early as safely possible
        // Phase 4 TODO: With proper blending, we can be more aggressive here
        // since exit velocity handoff eliminates the need for full deceleration
        if (remaining_time < horizon_sec + min_handoff_margin) {
            // Short segment: handoff at current time + small margin
            // This means "apply override now" rather than "schedule for later"
            horizon_sec = fmax(0.001, remaining_time - min_handoff_margin);
        }

        handoff_time = elapsed + horizon_sec;
        window_end_time = handoff_time + window_sec;

        // Clamp handoff to before segment end
        if (handoff_time >= profile_duration - min_handoff_margin) {
            if (tp->aborting || new_feed_scale < 0.001) {
                // Stop request (abort or feed hold): bypass margin — we must
                // decelerate no matter where we are in the profile. Place handoff
                // at current time for immediate takeover.
                // Note: tpRequestAbortBranch_9D calls us BEFORE tp->aborting is
                // set by RT, so we also check new_feed_scale to catch abort.
                handoff_time = elapsed;
                window_end_time = handoff_time + window_sec + 1.0;
            } else {
                handoff_time = profile_duration - min_handoff_margin;
                if (handoff_time <= elapsed) {
                    // Segment is essentially done - let next segment handle it
                    tc->shared_9d.requested_feed_scale = new_feed_scale;
                    return false;
                }
            }
        }
    }

    // Predict state at handoff time
    PredictedState state;
    if (resuming_from_pause) {
        // PAUSE/ABORT RESUME: Profile is the original motion profile (invalid).
        // Use actual stopped state from RT.
        state.position = atomicLoadDouble(&tc->progress);
        state.velocity = 0.0;
        state.acceleration = 0.0;
        state.jerk = 0.0;
        state.valid = true;
    } else if (resuming_from_feed_hold) {
        // FEED HOLD RESUME: Profile is a valid stop-in-place profile.
        // Sample it - gives correct state whether still decelerating or complete.
        state = predictStateAtTime(tc, handoff_time);
        // Clamp velocity/acceleration to non-negative zero.
        // Feed hold profiles (memset to 0) can return -0.0 from floating point
        // arithmetic, which propagates into Ruckig and causes profileHasNegativeVelocity
        // to reject the resume profile — permanently blocking the segment.
        if (state.valid && fabs(state.velocity) < 1e-6) state.velocity = 0.0;
        if (state.valid && fabs(state.acceleration) < 1e-6) state.acceleration = 0.0;
    } else {
        // NORMAL: Sample the active profile
        state = predictStateAtTime(tc, handoff_time);
    }

    if (!state.valid) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "Branch: failed to predict state at handoff seg=%d reason=state_invalid",
            tc->id);
        return false;
    }

    // NOTE: We do NOT delay handoff when cur_vel > new_max_vel.
    // The branch profile will handle deceleration from cur_vel down to new_max_vel.
    // This is the "track 2 continues, track 3 takes over at handoff" approach:
    // - Track 2 (old profile) runs until handoff at current velocity
    // - Track 3 (new profile) starts at handoff, decelerates to new max velocity
    // The key is that copyRuckigProfile samples at uniform time intervals,
    // avoiding Ruckig's internal phase shift when it prepends a brake trajectory.

    // Calculate remaining distance after handoff
    double remaining = tc->target - state.position;
    if (remaining < 1e-6) {
        rtapi_print_msg(RTAPI_MSG_DBG,
            "Branch: REJECT seg=%d reason=no_dist remaining=%.6f pos=%.6f target=%.6f "
            "resume_fh=%d\n",
            tc->id, remaining, state.position, tc->target, resuming_from_feed_hold);
        tc->shared_9d.requested_feed_scale = new_feed_scale;
        return false;
    }

    // Reject branch when segment is committed to final deceleration.
    // If decelerating at more than half max accel and remaining distance is
    // less than the stopping distance, the existing profile is already locked
    // into its stop — replanning would produce backward-motion profiles.
    if (state.acceleration < -0.5 * tc->maxaccel && state.velocity > 0) {
        double stop_dist = (state.velocity * state.velocity) / (2.0 * tc->maxaccel);
        if (remaining <= stop_dist * 1.5) {
            rtapi_print_msg(RTAPI_MSG_DBG,
                "Branch: REJECT seg=%d reason=decel_locked vel=%.2f acc=%.2f "
                "remaining=%.4f stop_dist=%.4f resume_fh=%d\n",
                tc->id, state.velocity, state.acceleration, remaining, stop_dist,
                resuming_from_feed_hold);
            tc->shared_9d.requested_feed_scale = new_feed_scale;
            return false;
        }
    }

    // Build Ruckig trajectories
    try {
        ruckig::Ruckig<1> otg(g_handoff_config.servo_cycle_time_sec);
        double t_start = etime_user();

        double effective_feed_scale = new_feed_scale;
        bool need_brake = (state.velocity > new_max_vel + 0.01);

        // Fix C: Also route through two-stage path when acceleration is large.
        // Large |acceleration| means velocity will overshoot before Ruckig can
        // arrest it, causing position-control mode to produce backward-motion
        // profiles. The stabilize stage (velocity control) cleanly brings
        // state to (stable_vel, 0 acc) before position control takes over.
        bool need_stabilize = false;
        double stabilize_target_vel = new_max_vel;
        if (!need_brake && fabs(state.acceleration) > 0.5 * tc->maxaccel) {
            need_stabilize = true;
            // Clamp stabilize target: don't exceed new_max_vel
            stabilize_target_vel = fmin(state.velocity, new_max_vel);
            if (stabilize_target_vel < 0.0) stabilize_target_vel = 0.0;
        }

        // Store cascade info
        tc->shared_9d.requested_feed_scale = new_feed_scale;

        ruckig_profile_t brake_profile;
        ruckig_profile_t main_profile;
        memset(&brake_profile, 0, sizeof(brake_profile));
        memset(&main_profile, 0, sizeof(main_profile));

        double brake_end_position = state.position;  // Position after brake (relative to segment start)

        // Feed hold: single-stage velocity control to near-zero
        if (new_feed_scale < 0.001) {
            ruckig::InputParameter<1> input;
            ruckig::Trajectory<1> traj;



            // True feed hold: decelerate to complete stop
            double target_vel = 0.0;

            input.control_interface = ruckig::ControlInterface::Velocity;
            input.current_position = {0.0};
            input.current_velocity = {state.velocity};
            input.current_acceleration = {state.acceleration};  // Must match actual state!
            input.target_velocity = {target_vel};
            input.target_acceleration = {0.0};
            input.max_velocity = {vel_limit};
            input.max_acceleration = {tc->maxaccel};
            input.max_jerk = {default_jerk};

            auto result = otg.calculate(input, traj);
            if (result != ruckig::Result::Working && result != ruckig::Result::Finished) {
                return false;
            }

            copyRuckigProfile(traj, &main_profile);
            if (!main_profile.valid) {
                return false;
            }
            main_profile.computed_feed_scale = 0.0;
            main_profile.computed_vel_limit = vel_limit;
            main_profile.computed_vLimit = tp->vLimit;

            // Fix A: Reject profile if it contains backward motion
            if (profileHasNegativeVelocity(&main_profile)) {
                return false;
            }

            double t_end = etime_user();
            double calc_us = (t_end - t_start) * 1e6;
            g_ruckig_timing.last_us = calc_us;
            g_ruckig_timing.total_us += calc_us;
            g_ruckig_timing.count++;
            if (calc_us > g_ruckig_timing.max_us) g_ruckig_timing.max_us = calc_us;

            bool committed = commitBranch(&tc->shared_9d, &main_profile, NULL, 0.0, 0.0,
                                          handoff_time, state.position, 0.0, window_end_time);

            // Spill-over: if stop distance exceeds remaining segment, the
            // machine will cross into downstream segments mid-deceleration.
            // Write alt-entry stop profiles so RT has correct v0 at each
            // boundary (same pattern as tpRequestAbortBranch_9D).
            if (committed && tc->term_cond == TC_TERM_COND_TANGENT) {
                double spill_vel, spill_acc;
                computeSpillOver(&main_profile, remaining, &spill_vel, &spill_acc);

                if (spill_vel > TP_VEL_EPSILON) {
                    TC_QUEUE_STRUCT *queue = &tp->queue;
                    int queue_len = tcqLen_user(queue);
                    double entry_vel = spill_vel;
                    double entry_acc = spill_acc;

                    for (int i = 1; i < queue_len && entry_vel > TP_VEL_EPSILON; i++) {
                        TC_STRUCT *next = tcqItem_user(queue, i);
                        if (!next || next->target < 1e-9) break;

                        double this_entry_vel = entry_vel;
                        double next_vel_limit = getEffectiveVelLimit(tp, next);
                        double next_jerk = next->maxjerk > 0 ? next->maxjerk : default_jerk;

                        try {
                            ruckig::Ruckig<1> otg_spill(g_handoff_config.servo_cycle_time_sec);
                            ruckig::InputParameter<1> in_spill;
                            ruckig::Trajectory<1> traj_spill;

                            in_spill.control_interface = ruckig::ControlInterface::Velocity;
                            in_spill.current_position = {0.0};
                            in_spill.current_velocity = {entry_vel};
                            in_spill.current_acceleration = {entry_acc};
                            in_spill.target_velocity = {0.0};
                            in_spill.target_acceleration = {0.0};
                            in_spill.max_velocity = {next_vel_limit};
                            in_spill.max_acceleration = {next->maxaccel};
                            in_spill.max_jerk = {next_jerk};

                            auto result = otg_spill.calculate(in_spill, traj_spill);
                            if (result != ruckig::Result::Working &&
                                result != ruckig::Result::Finished)
                                break;

                            ruckig_profile_t stop_prof;
                            memset(&stop_prof, 0, sizeof(stop_prof));
                            copyRuckigProfile(traj_spill, &stop_prof);
                            if (!stop_prof.valid || profileHasNegativeVelocity(&stop_prof))
                                break;
                            stop_prof.computed_feed_scale = 0.0;
                            stop_prof.computed_vel_limit = next_vel_limit;
                            stop_prof.computed_vLimit = tp->vLimit;

                            // Check if this segment also spills over
                            computeSpillOver(&stop_prof, next->target,
                                             &entry_vel, &entry_acc);

                            // Write as alt-entry so RT picks it at the junction
                            next->shared_9d.alt_entry.profile = stop_prof;
                            next->shared_9d.alt_entry.v0 = this_entry_vel;
                            __atomic_thread_fence(__ATOMIC_RELEASE);
                            __atomic_store_n(&next->shared_9d.alt_entry.valid, 1,
                                             __ATOMIC_RELEASE);
                        } catch (...) {
                            break;
                        }
                    }
                }
            }

            return committed;
        }

        // Chain exit cap: walk downstream segments at the NEW feed scale,
        // cascading reachability limits backward to find the tightest
        // constraint on the active segment's exit velocity.  Replaces the
        // old 1-segment lookahead that missed tight constraints further
        // downstream (short segments, sharp kinks at index 2+).
        double downstream_exit_cap = computeChainExitCap(
            tp, tc, new_feed_scale, default_jerk);

        // ================================================================
        // Feed limiting: find max feed where exit constraints are met
        // ================================================================
        // Instead of rejecting branches that exceed kink or downstream limits,
        // binary-search for the highest feed that produces a feasible exit.
        double exit_hard_limit = downstream_exit_cap;
        if (tc->kink_vel > 0)
            exit_hard_limit = fmin(exit_hard_limit, tc->kink_vel);

        if (exit_hard_limit < 1e9 && remaining > 1e-6) {
            double test_exit = estimateExitAtFeed(tp, tc, state.velocity,
                state.acceleration, remaining, vel_limit, new_feed_scale,
                default_jerk, exit_hard_limit);

            if (test_exit > exit_hard_limit + 1.0) {
                // Feed produces exit velocity exceeding junction constraints.
                // Binary search for the maximum feed that satisfies the limit.
                double lo = 0.001, hi = new_feed_scale;
                double best_feed = lo;

                for (int iter = 0; iter < 12; iter++) {
                    double mid = (lo + hi) / 2.0;
                    double mid_exit = estimateExitAtFeed(tp, tc, state.velocity,
                        state.acceleration, remaining, vel_limit, mid,
                        default_jerk, exit_hard_limit);
                    if (mid_exit <= exit_hard_limit + 0.01) {
                        best_feed = mid;
                        lo = mid;
                    } else {
                        hi = mid;
                    }
                }

                if (best_feed > 0.002) {
                    rtapi_print_msg(RTAPI_MSG_DBG,
                        "Branch: FEED_LIMIT seg=%d feed=%.3f->%.3f "
                        "exit_limit=%.2f (kink=%.2f ds_cap=%.2f)\n",
                        tc->id, new_feed_scale, best_feed,
                        exit_hard_limit, tc->kink_vel, downstream_exit_cap);
                    new_feed_scale = best_feed;
                    new_max_vel = applyVLimit(tp, tc, vel_limit * new_feed_scale);
                    effective_feed_scale = new_feed_scale;
                    // Recompute brake/stabilize determination at reduced feed
                    need_brake = (state.velocity > new_max_vel + 0.01);
                    need_stabilize = false;
                    stabilize_target_vel = new_max_vel;
                    if (!need_brake && fabs(state.acceleration) > 0.5 * tc->maxaccel) {
                        need_stabilize = true;
                        stabilize_target_vel = fmin(state.velocity, new_max_vel);
                        if (stabilize_target_vel < 0.0) stabilize_target_vel = 0.0;
                    }
                } else {
                    // Even minimum feed can't satisfy — segment is genuinely locked
                    rtapi_print_msg(RTAPI_MSG_DBG,
                        "Branch: FEED_LIMIT_LOCKED seg=%d exit_limit=%.2f "
                        "min_feed_exit=%.2f vel=%.2f remaining=%.4f\n",
                        tc->id, exit_hard_limit, test_exit,
                        state.velocity, remaining);
                    return false;
                }
            }
        }

        if (need_brake || need_stabilize) {
            // TWO-STAGE APPROACH: Separate brake/stabilize + main profiles
            // This avoids uniform sampling and keeps both profiles exact (7 phases each)
            // need_brake: velocity > max, decelerate to max
            // need_stabilize: |acceleration| is large, arrest it before position control

            // Stage 1: Brake/stabilize profile (velocity control)
            // Bring velocity to target and acceleration to zero
            double stage1_target_vel = need_brake ? new_max_vel : stabilize_target_vel;
            {
                ruckig::InputParameter<1> input;
                ruckig::Trajectory<1> traj;

    
                input.control_interface = ruckig::ControlInterface::Velocity;
                input.current_position = {0.0};
                input.current_velocity = {state.velocity};
                input.current_acceleration = {state.acceleration};
                input.target_velocity = {stage1_target_vel};
                input.target_acceleration = {0.0};
                input.max_velocity = {fmax(state.velocity, new_max_vel)};
                input.max_acceleration = {tc->maxaccel};
                input.max_jerk = {default_jerk};

                auto result = otg.calculate(input, traj);
                if (result != ruckig::Result::Working && result != ruckig::Result::Finished) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "Branch brake Ruckig failed: result=%d v=%.1f->%.1f a=%.1f maxacc=%.1f jerk=%.1f",
                        static_cast<int>(result), state.velocity, stage1_target_vel,
                        state.acceleration, tc->maxaccel, default_jerk);
                    return false;
                }

                // Brake profile is clean 7-phase, use phase-based sampling
                copyRuckigProfile(traj, &brake_profile);
                if (!brake_profile.valid) {
                    rtapi_print_msg(RTAPI_MSG_DBG,
                        "Branch: REJECT seg=%d reason=brake_invalid resume_fh=%d\n",
                        tc->id, resuming_from_feed_hold);
                    return false;
                }

                // Enforce zero final acceleration for brake profile
                // Ruckig velocity control targets a[end]=0 but may have numerical residual
                // This ensures clean handoff to main profile which starts with a=0
                brake_profile.a[RUCKIG_PROFILE_PHASES] = 0.0;

                // Recompute final phase jerk for consistency
                int last_phase = RUCKIG_PROFILE_PHASES - 1;
                if (brake_profile.t[last_phase] > 1e-9) {
                    brake_profile.j[last_phase] =
                        (brake_profile.a[RUCKIG_PROFILE_PHASES] - brake_profile.a[last_phase])
                        / brake_profile.t[last_phase];
                } else {
                    brake_profile.j[last_phase] = 0.0;
                }

                // Get distance traveled during brake
                double brake_dist = brake_profile.p[RUCKIG_PROFILE_PHASES];
                brake_end_position = state.position + brake_dist;

                brake_profile.computed_feed_scale = effective_feed_scale;
                brake_profile.computed_vel_limit = vel_limit;
                brake_profile.computed_vLimit = tp->vLimit;
            }

            bool use_single_stage = false;  // set true if sub-cycle gap detected

            // Stage 2: Main profile (position control)
            // Start from stage1_target_vel (after brake/stabilize), go to segment end
            {
                ruckig::InputParameter<1> input;
                ruckig::Trajectory<1> traj;

    

                double remaining_after_brake = tc->target - brake_end_position;

                // Find achievable exit velocity for main profile
                // For tangent transitions, target the backward-pass exit velocity scaled by feed
                double unscaled_fv = atomicLoadDouble(&tc->shared_9d.final_vel);
                double target_exit_vel = (tc->term_cond == TC_TERM_COND_TANGENT)
                    ? fmin(unscaled_fv * new_feed_scale, new_max_vel)
                    : 0.0;
                target_exit_vel = applyKinkVelCap(target_exit_vel, unscaled_fv, new_max_vel, tc->kink_vel);
                target_exit_vel = fmin(target_exit_vel, downstream_exit_cap);
                double achievable_exit = findAchievableExitVelocity(
                    stage1_target_vel, 0.0,  // Start from stage1 target with zero acc
                    remaining_after_brake, tc->maxaccel, default_jerk,
                    new_max_vel, target_exit_vel);

                // Non-blending segments (STOP, EXACT) must end at zero velocity.
                // If we can't decelerate to zero in the remaining distance,
                // reject the branch and let the existing profile finish.
                // Phase 4 TODO (Blending): review PARABOLIC handling here.
                if (tc->term_cond != TC_TERM_COND_TANGENT && achievable_exit > 0.01) {
                    return false;
                }

                // Kink-limited junctions: reject if can't decel to kink_vel
                if (tc->kink_vel > 0 && achievable_exit > tc->kink_vel + 0.01) {
                    rtapi_print_msg(RTAPI_MSG_DBG,
                        "Branch: REJECT seg=%d reason=kink(2stg) exit=%.2f kink=%.2f resume_fh=%d\n",
                        tc->id, achievable_exit, tc->kink_vel, resuming_from_feed_hold);
                    return false;
                }

                // Downstream reachability: reject if can't decel to what
                // next segment accepts.  Feed change deferred to next cycle.
                if (achievable_exit > downstream_exit_cap + 0.01) {
                    rtapi_print_msg(RTAPI_MSG_DBG,
                        "Branch: REJECT seg=%d reason=downstream(2stg) exit=%.2f cap=%.2f resume_fh=%d\n",
                        tc->id, achievable_exit, downstream_exit_cap, resuming_from_feed_hold);
                    tc->shared_9d.requested_feed_scale = new_feed_scale;
                    return false;
                }

                if (achievable_exit > target_exit_vel + 0.01) {
                    effective_feed_scale = achievable_exit / vel_limit;
                    if (effective_feed_scale > 1.0) effective_feed_scale = 1.0;
                }
                tc->shared_9d.achieved_exit_vel = achievable_exit;

                input.control_interface = ruckig::ControlInterface::Position;
                input.current_position = {0.0};
                input.current_velocity = {stage1_target_vel};  // Start at stabilized vel
                input.current_acceleration = {0.0};             // Stage 1 ends with zero acc
                input.target_position = {remaining_after_brake};
                input.target_velocity = {achievable_exit};
                input.target_acceleration = {0.0};
                input.max_velocity = {new_max_vel};
                input.max_acceleration = {tc->maxaccel};
                input.max_jerk = {default_jerk};

                auto result = otg.calculate(input, traj);
                if (result != ruckig::Result::Working && result != ruckig::Result::Finished) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "Branch main Ruckig failed: result=%d v=%.1f->%.1f dist=%.3f maxvel=%.1f maxacc=%.1f",
                        static_cast<int>(result), stage1_target_vel, achievable_exit,
                        remaining_after_brake, new_max_vel, tc->maxaccel);
                    return false;
                }

                // Main profile starts at/below max_vel, so no brake prepend - use phase-based
                copyRuckigProfile(traj, &main_profile);
                if (!main_profile.valid) return false;
                main_profile.computed_feed_scale = effective_feed_scale;
                main_profile.computed_vel_limit = vel_limit;
                main_profile.computed_vLimit = tp->vLimit;

                // Sub-cycle gap fix: when the main profile is shorter than
                // one servo cycle, the brake→main transition causes a
                // displacement dip (jerk spike).  Replace with a single-stage
                // position-control profile from current state to segment end.
                {
                    double cycle_time = g_handoff_config.servo_cycle_time_sec;
                    if (main_profile.duration < cycle_time) {
                        ruckig::InputParameter<1> alt_input;
                        ruckig::Trajectory<1> alt_traj;

                        double alt_target_exit = (tc->term_cond == TC_TERM_COND_TANGENT)
                            ? fmin(atomicLoadDouble(&tc->shared_9d.final_vel) * new_feed_scale, new_max_vel)
                            : 0.0;
                        alt_target_exit = applyKinkVelCap(alt_target_exit,
                            atomicLoadDouble(&tc->shared_9d.final_vel), new_max_vel, tc->kink_vel);
                        alt_target_exit = fmin(alt_target_exit, downstream_exit_cap);
                        double alt_achievable = findAchievableExitVelocity(
                            state.velocity, state.acceleration,
                            remaining, tc->maxaccel, default_jerk,
                            fmax(state.velocity, new_max_vel), alt_target_exit);

                        alt_input.control_interface = ruckig::ControlInterface::Position;
                        alt_input.current_position = {0.0};
                        alt_input.current_velocity = {state.velocity};
                        alt_input.current_acceleration = {state.acceleration};
                        alt_input.target_position = {remaining};
                        alt_input.target_velocity = {alt_achievable};
                        alt_input.target_acceleration = {0.0};
                        alt_input.max_velocity = {fmax(state.velocity, new_max_vel)};
                        alt_input.max_acceleration = {tc->maxaccel};
                        alt_input.max_jerk = {default_jerk};

                        auto alt_result = otg.calculate(alt_input, alt_traj);
                        bool alt_ok = (alt_result == ruckig::Result::Working ||
                                       alt_result == ruckig::Result::Finished);

                        ruckig_profile_t alt_profile = {};
                        if (alt_ok) {
                            copyRuckigProfile(alt_traj, &alt_profile);
                            alt_ok = alt_profile.valid &&
                                     !profileHasNegativeVelocity(&alt_profile);
                        }

                        if (alt_ok) {
                            // Replace two-stage with single-stage
                            main_profile = alt_profile;
                            main_profile.computed_feed_scale = effective_feed_scale;
                            main_profile.computed_vel_limit = vel_limit;
                            main_profile.computed_vLimit = tp->vLimit;
                            achievable_exit = alt_achievable;
                            if (alt_achievable > alt_target_exit + 0.01) {
                                effective_feed_scale = alt_achievable / vel_limit;
                                if (effective_feed_scale > 1.0) effective_feed_scale = 1.0;
                            }
                            tc->shared_9d.achieved_exit_vel = alt_achievable;
                            use_single_stage = true;
                        }
                    }
                }
            }

            // Fix A: Reject if brake or main profile contains backward motion
            // (skip brake check when using single-stage — no brake profile)
            if (!use_single_stage &&
                (profileHasNegativeVelocity(&brake_profile) ||
                 profileHasNegativeVelocity(&main_profile))) {
                rtapi_print_msg(RTAPI_MSG_DBG,
                    "Branch: REJECT seg=%d reason=negvel(2stg) brake_neg=%d main_neg=%d "
                    "brake_dist=%.4f seg_remaining=%.4f seg_target=%.4f progress=%.4f "
                    "resume_fh=%d\n",
                    tc->id, profileHasNegativeVelocity(&brake_profile),
                    profileHasNegativeVelocity(&main_profile),
                    brake_profile.p[RUCKIG_PROFILE_PHASES],
                    tc->target - state.position,
                    tc->target, state.position, resuming_from_feed_hold);
                return false;
            }

            double t_end = etime_user();
            double calc_us = (t_end - t_start) * 1e6;
            g_ruckig_timing.last_us = calc_us;
            g_ruckig_timing.total_us += calc_us;
            g_ruckig_timing.count++;
            if (calc_us > g_ruckig_timing.max_us) g_ruckig_timing.max_us = calc_us;

            // Cushion before jump: write alt-entry on N+1 before committing brake on N
            writeAltEntry(tp, tc, tc->shared_9d.achieved_exit_vel, effective_feed_scale);

            // Commit profile(s)
            // When use_single_stage, commit as single-stage (no brake).
            // Otherwise commit two-stage with brake_target_vel = stage1_target_vel.
            bool committed = use_single_stage
                ? commitBranch(&tc->shared_9d, &main_profile, NULL, 0.0, 0.0,
                               handoff_time, state.position,
                               effective_feed_scale, window_end_time,
                               tc->shared_9d.achieved_exit_vel)
                : commitBranch(&tc->shared_9d, &main_profile, &brake_profile,
                               stage1_target_vel, brake_end_position,
                               handoff_time, state.position,
                               effective_feed_scale, window_end_time,
                               tc->shared_9d.achieved_exit_vel);

            return committed;

        } else {
            // SINGLE-STAGE: Current velocity already at or below new max
            // Standard position-control profile, no brake needed
            ruckig::InputParameter<1> input;
            ruckig::Trajectory<1> traj;



            // For tangent transitions, target the backward-pass exit velocity scaled by feed
            double unscaled_fv = atomicLoadDouble(&tc->shared_9d.final_vel);
            double target_exit_vel = (tc->term_cond == TC_TERM_COND_TANGENT)
                ? fmin(unscaled_fv * new_feed_scale, new_max_vel)
                : 0.0;
            target_exit_vel = applyKinkVelCap(target_exit_vel, unscaled_fv, new_max_vel, tc->kink_vel);
            target_exit_vel = fmin(target_exit_vel, downstream_exit_cap);
            double achievable_exit = findAchievableExitVelocity(
                state.velocity, state.acceleration,
                remaining, tc->maxaccel, default_jerk,
                new_max_vel, target_exit_vel);

            // Non-blending segments (STOP, EXACT) must end at zero velocity.
            // If we can't decelerate to zero in the remaining distance, reject
            // the branch and let the existing profile (computed with enough
            // room to stop) finish.
            // Phase 4 TODO (Blending): When PARABOLIC is used for planner type 2
            // blending, decide whether it should be treated as zero-exit or
            // non-zero-exit here.
            if (tc->term_cond != TC_TERM_COND_TANGENT && achievable_exit > 0.01) {
                return false;
            }

            // Kink-limited junctions: if we can't decelerate to the kink
            // velocity in the remaining distance, reject the branch.
            // Otherwise the profile exits above kink_vel, downstream gets
            // seeded with an unreachable entry velocity, and RT sees a
            // velocity discontinuity at the segment transition.
            if (tc->kink_vel > 0 && achievable_exit > tc->kink_vel + 0.01) {
                rtapi_print_msg(RTAPI_MSG_DBG,
                    "Branch: REJECT seg=%d reason=kink(1stg) exit=%.2f kink=%.2f resume_fh=%d\n",
                    tc->id, achievable_exit, tc->kink_vel, resuming_from_feed_hold);
                return false;
            }

            // Downstream reachability: reject if can't decel to what
            // next segment accepts.  Feed change deferred to next cycle.
            if (achievable_exit > downstream_exit_cap + 0.01) {
                rtapi_print_msg(RTAPI_MSG_DBG,
                    "Branch: REJECT seg=%d reason=downstream(1stg) exit=%.2f cap=%.2f resume_fh=%d\n",
                    tc->id, achievable_exit, downstream_exit_cap, resuming_from_feed_hold);
                tc->shared_9d.requested_feed_scale = new_feed_scale;
                return false;
            }

            if (achievable_exit > target_exit_vel + 0.01) {
                effective_feed_scale = achievable_exit / vel_limit;
                if (effective_feed_scale > 1.0) effective_feed_scale = 1.0;
            }
            tc->shared_9d.achieved_exit_vel = achievable_exit;

            input.control_interface = ruckig::ControlInterface::Position;
            input.current_position = {0.0};
            input.current_velocity = {state.velocity};
            input.current_acceleration = {state.acceleration};
            input.target_position = {remaining};
            input.target_velocity = {achievable_exit};
            input.target_acceleration = {0.0};
            input.max_velocity = {new_max_vel};
            input.max_acceleration = {tc->maxaccel};
            input.max_jerk = {default_jerk};

            auto result = otg.calculate(input, traj);
            if (result != ruckig::Result::Working && result != ruckig::Result::Finished) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "Branch Ruckig failed: result=%d v=%.1f a=%.1f->v_exit=%.1f dist=%.3f maxvel=%.1f maxacc=%.1f",
                    static_cast<int>(result), state.velocity, state.acceleration,
                    achievable_exit, remaining, new_max_vel, tc->maxaccel);
                return false;
            }

            // No brake prepend - use phase-based sampling
            copyRuckigProfile(traj, &main_profile);
            if (!main_profile.valid) return false;
            main_profile.computed_feed_scale = effective_feed_scale;
            main_profile.computed_vel_limit = vel_limit;
            main_profile.computed_vLimit = tp->vLimit;

            // Fix A: Reject profile if it contains backward motion.
            // Exception: when resuming from feed hold at rest (v≈0, a≈0),
            // Ruckig may produce a trajectory with a brief negative-velocity
            // "wind-up" in the initial jerk phase.  This is a valid position-
            // control artefact — the machine is stationary and the trajectory
            // reaches the target correctly.  Blocking it causes a permanent
            // freeze because computeBranch sets requested_feed_scale on failure,
            // which suppresses retry detection in manageBranches.
            bool skip_negvel_check = (resuming_from_feed_hold || resuming_from_pause)
                                     && fabs(state.velocity) < 1e-3
                                     && fabs(state.acceleration) < 1e-3;
            if (!skip_negvel_check && profileHasNegativeVelocity(&main_profile)) {
                rtapi_print_msg(RTAPI_MSG_DBG,
                    "Branch: REJECT seg=%d reason=negvel(1stg) v=%.2f a=%.2f "
                    "seg_remaining=%.4f seg_target=%.4f progress=%.4f resume_fh=%d\n",
                    tc->id, state.velocity, state.acceleration,
                    remaining, tc->target, state.position, resuming_from_feed_hold);
                return false;
            }

            double t_end = etime_user();
            double calc_us = (t_end - t_start) * 1e6;
            g_ruckig_timing.last_us = calc_us;
            g_ruckig_timing.total_us += calc_us;
            g_ruckig_timing.count++;
            if (calc_us > g_ruckig_timing.max_us) g_ruckig_timing.max_us = calc_us;

            // Cushion before jump: write alt-entry on N+1 before committing branch on N
            writeAltEntry(tp, tc, tc->shared_9d.achieved_exit_vel, effective_feed_scale);

            bool committed = commitBranch(&tc->shared_9d, &main_profile, NULL, 0.0, 0.0,
                                          handoff_time, state.position,
                                          effective_feed_scale, window_end_time,
                                          achievable_exit);

            return committed;
        }

    } catch (std::exception &e) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Branch computation exception: %s", e.what());
        return false;
    }
}

/**
 * @brief Check if feed scale changed from canonical
 *
 * No deadband - any change triggers branch computation.
 * The debounce mechanism prevents rapid recomputation.
 *
 * @param current_feed Current feed scale
 * @param canonical_feed Canonical (last merged) feed scale
 * @return true if feed changed (any amount)
 */
static bool feedChangedSignificantly(double current_feed, double canonical_feed)
{
    // No deadband - any change triggers branch (tiny epsilon for FP noise only)
    double change = fabs(current_feed - canonical_feed);
    return change > 1e-6;
}

/**
 * @brief Manage branches for feed override using branch/merge architecture
 *
 * Called each iteration of the optimization loop. Implements the branch/merge
 * protocol for feed override handling:
 *
 * 1. Check for successful merge: branch.taken=1 means RT took the branch
 *    - Update canonical_feed_scale to branch's feed_scale
 *    - Clear branch.valid and branch.taken
 *    - Invalidate downstream segments for re-optimization
 *
 * 2. Check for missed branch: RT past window_end without taking
 *    - Clear branch.valid
 *    - Fall through to create new branch if feed still different
 *
 * 3. Check if new branch needed: feed changed from canonical
 *    - Only if no branch currently pending
 *    - Compute new branch trajectory
 *
 * @param tp Trajectory planner structure
 */
extern "C" void manageBranches(TP_STRUCT *tp)
{
    if (!tp) return;

    // Get active segment
    TC_STRUCT *tc = tcqItem_user(&tp->queue, 0);
    if (!tc || !tc->active || !tc->shared_9d.profile.valid) {
        return;
    }

    // PAUSE/ABORT: Compute smooth stop branch using Ruckig (same as 0% feed hold)
    // This replaces the cycle-by-cycle stopping in RT with time-optimal deceleration
    //
    // Phase 4 TODO (Blending): When blend segments (bezier) are in the queue,
    // verify that the abort_profiles_written gate in checkFeedOverride() still
    // covers any new optimization paths added for blending. Also verify that
    // tpRequestAbortBranch_9D's spill-over loop correctly walks through
    // interleaved blend segments (line-bezier-line-...) — blend segments are
    // short so spill-over will cross them quickly, but their maxaccel/maxjerk
    // must be valid for the velocity-control stop profiles written there.
    if (tp->pausing || tp->aborting) {
        int branch_valid = __atomic_load_n(&tc->shared_9d.branch.valid, __ATOMIC_ACQUIRE);
        if (!branch_valid) {
            computeBranch(tp, tc, 0.0);
        }
        return;  // Don't process feed override while pausing/aborting
    }

    // Check if active segment has stale profile (race condition fix)
    // This happens when segment becomes active before its profile was recomputed.
    // Two cases:
    //   1. profile_feed != canonical_feed — profile was written at old feed
    //   2. profile_feed ≈ 0 but slider is live — initial pessimistic profile was
    //      never replaced because the cursor walk (from a prior merge) blocked
    //      branching.  canonical_feed is also 0 (never set), so case 1 misses it.
    // This check runs BEFORE the cursor guard so these segments get fixed
    // even while a cursor walk is in progress (cursor never touches index 0).
    double profile_feed = tc->shared_9d.profile.computed_feed_scale;
    double canonical_feed = tc->shared_9d.canonical_feed_scale;
    double current_feed_now = tpGetSegmentFeedScale(tc);
    double feed_diff = fabs(profile_feed - canonical_feed);
    bool stale_initial = (profile_feed < 0.001 && current_feed_now > 0.01);

    if (feed_diff > 0.005 || stale_initial) {
        int branch_valid = __atomic_load_n(&tc->shared_9d.branch.valid, __ATOMIC_ACQUIRE);
        if (!branch_valid) {
            double target_feed = stale_initial ? current_feed_now : canonical_feed;
            bool ok = computeBranch(tp, tc, target_feed);
            if (stale_initial && !ok) {
                double elapsed = atomicLoadDouble(&tc->elapsed_time);
                double dur = tc->shared_9d.profile.duration;
                rtapi_print_msg(RTAPI_MSG_DBG,
                    "STALE_INITIAL seg %d: computeBranch REJECTED feed=%.3f "
                    "elapsed=%.4f dur=%.4f remaining=%.4f cursor=%d\n",
                    tc->id, target_feed, elapsed, dur, dur - elapsed,
                    g_recompute_cursor);
            }
        }
    }

    // Kink velocity check: if active segment's profile exits above its
    // kink constraint, force a branch computation.  The feed limiter
    // inside computeBranch will cap the exit velocity by reducing the
    // feed scale.  This catches segments that became active before the
    // optimizer could fix their exit velocity.
    if (tc->kink_vel > 0 && tc->shared_9d.profile.valid) {
        double prof_exit = tc->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
        if (prof_exit > tc->kink_vel + 0.5) {
            int bv = __atomic_load_n(&tc->shared_9d.branch.valid, __ATOMIC_ACQUIRE);
            if (!bv) {
                if (computeBranch(tp, tc, current_feed_now)) {
                    return;
                }
            }
        }
    }

    // Chain exit cap check: if active segment's profile exits above what
    // the downstream chain can handle at the current feed, force a branch.
    // The backward fixup pass skips the active segment, so this is the
    // only place where the active's exit gets checked against full
    // downstream constraints.
    if (tc->term_cond == TC_TERM_COND_TANGENT && tc->shared_9d.profile.valid) {
        int bv = __atomic_load_n(&tc->shared_9d.branch.valid, __ATOMIC_ACQUIRE);
        if (!bv) {
            double default_jerk = tc->maxjerk > 0 ? tc->maxjerk : g_handoff_config.default_max_jerk;
            double chain_cap = computeChainExitCap(tp, tc, current_feed_now, default_jerk);
            double prof_exit = tc->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
            if (prof_exit > chain_cap + 1.0) {
                computeBranch(tp, tc, current_feed_now);
                // Don't return — branch may be rejected (LOCKED) if the
                // current velocity is already too high.  Let normal flow
                // continue so feed changes are still processed.
            }
        }
    }

    // Blend exit velocity check: if active segment was promoted to TANGENT
    // by tpSetupBlend9D (a downstream blend was created), but its profile
    // still exits at ~0 from Fix 4 (first profile computed before blend existed),
    // trigger a branch to update the exit velocity.  computeBranch reads
    // final_vel (set correctly by tpSetupBlend9D) and produces the right profile.
    if (tc->term_cond == TC_TERM_COND_TANGENT && tc->shared_9d.profile.valid) {
        double prof_exit = tc->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
        double desired_exit = atomicLoadDouble(&tc->shared_9d.final_vel) * current_feed_now;
        if (prof_exit < 1e-6 && desired_exit > 1.0) {
            int bv = __atomic_load_n(&tc->shared_9d.branch.valid, __ATOMIC_ACQUIRE);
            if (!bv) {
                computeBranch(tp, tc, current_feed_now);
            }
        }
    }

    pending_branch_t *branch = &tc->shared_9d.branch;

    // 1. Check for successful merge (RT took the branch)
    int branch_valid = __atomic_load_n(&branch->valid, __ATOMIC_ACQUIRE);
    int branch_taken = __atomic_load_n(&branch->taken, __ATOMIC_ACQUIRE);

    if (branch_valid && branch_taken) {
        // MERGE: branch is now canonical
        double new_canonical = branch->feed_scale;

        // Adaptive horizon: success, decay toward base
        g_adaptive_horizon.onTake();

        tc->shared_9d.canonical_feed_scale = new_canonical;

        // For two-stage profiles, wait until brake is complete before fully clearing
        // RT needs taken=1 to detect it's still in brake phase (via in_brake_phase check)
        int has_brake = branch->has_brake;
        int brake_done = (has_brake ? __atomic_load_n(&branch->brake_done, __ATOMIC_ACQUIRE) : 1);

        // Always clear valid (prevents new branches during execution)
        __atomic_store_n(&branch->valid, 0, __ATOMIC_RELEASE);

        if (!has_brake || brake_done) {
            // Single-stage or brake complete: fully clear
            __atomic_store_n(&branch->taken, 0, __ATOMIC_RELEASE);
        }
        // else: two-stage in brake phase — keep taken=1 for RT's brake→main transition

        // Recompute downstream at the BRANCH feed, not the current slider.
        // The branch exit velocity was computed at branch_feed — downstream
        // must match to avoid velocity mismatch at the junction. If the
        // slider has moved since the branch was created, manageBranches will
        // detect the gap (g_committed_feed != slider) and create a new branch
        // through the normal branch-merge cycle.
        double merge_feed = new_canonical;  // = branch->feed_scale
        double merge_rapid = emcmotStatus ? emcmotStatus->rapid_scale : 1.0;
        // Invalidate all downstream segments and recompute as many as time allows.
        invalidateNextNSegments(tp, INT_MAX);
        int done = recomputeDownstreamProfiles(tp, merge_feed, merge_rapid);

        // Update global tracking
        g_last_feed_scale = new_canonical;

        // Cursor walk continues from where the synchronous recompute left off
        g_recompute_cursor = done + 1;  // +1 to skip active segment (index 0)
        g_recompute_feed_scale = merge_feed;
        g_recompute_rapid_scale = merge_rapid;
        g_recompute_first_batch_done = false;
        return;
    }

    // Cleanup for completed two-stage profiles: clear taken once brake is done
    if (!branch_valid && branch_taken && branch->has_brake) {
        int brake_done = __atomic_load_n(&branch->brake_done, __ATOMIC_ACQUIRE);
        if (brake_done) {
            __atomic_store_n(&branch->taken, 0, __ATOMIC_RELEASE);
        }
    }

    // 2. Check for missed branch (RT past window, didn't take it)
    double rt_elapsed = atomicLoadDouble(&tc->elapsed_time);

    if (branch_valid && !branch_taken && rt_elapsed > branch->window_end_time) {
        // Branch missed - back off the horizon
        g_adaptive_horizon.onMiss();
        __atomic_store_n(&branch->valid, 0, __ATOMIC_RELEASE);
        // Fall through to potentially create new branch
    }

    // 3. Check if new feed change needed
    if (!emcmotStatus) return;

    // Use the per-segment-type feed scale, not the global net_feed_scale.
    // net_feed_scale is computed per RT cycle based on the CURRENT motion type
    // (traverse vs feed), so it reflects whatever RT is executing right now —
    // not the segment we're branching on. This causes phantom feed changes
    // when RT executes a traverse while we're branching on a feed move.
    double current_feed = tpGetSegmentFeedScale(tc);

    // Compare against REQUESTED feed scale, not canonical (effective) feed scale.
    // This is critical for the achievable feed cascade:
    // - If segment was too short, effective_feed != requested_feed
    // - canonical_feed_scale tracks effective (what was achieved)
    // - requested_feed_scale tracks what user actually wants
    // - We should keep trying to achieve requested until segment ends
    // - Using canonical would cause infinite loop: effective != requested -> recompute -> same effective
    double requested_feed = tc->shared_9d.requested_feed_scale;

    // If requested_feed is uninitialized (0), fall back to canonical
    if (requested_feed < 0.001) {
        requested_feed = canonical_feed;
    }

    // Check if feed or velocity limit changed significantly
    double current_vel_limit = getEffectiveVelLimit(tp, tc);
    double current_max_vel = applyVLimit(tp, tc, current_vel_limit * current_feed);

    double profile_vel_limit = tc->shared_9d.profile.computed_vel_limit;
    double profile_feed_scale = tc->shared_9d.profile.computed_feed_scale;

    double effective_profile_feed = (profile_feed_scale < 0.001)
        ? requested_feed : profile_feed_scale;
    double profile_max_vel = applyVLimit(tp, tc,
        profile_vel_limit * effective_profile_feed);

    bool feed_changed = feedChangedSignificantly(current_feed, requested_feed);
    double vel_diff = fabs(current_max_vel - profile_max_vel);
    double vel_threshold = fmax(current_max_vel, profile_max_vel) * 0.005;
    bool vel_changed = (vel_diff > vel_threshold) && (vel_diff > 0.1);

    if (!feed_changed && !vel_changed) return;

    // Feed change detected.
    // If cursor walk is active, queue this change (next-in-line, freely overwritable).
    if (g_recompute_cursor > 0) {
        // Recovery from feed-hold: abort the hold cursor and restart fresh,
        // just like program start.  The hold cursor is walking at feed≈0,
        // skipping segments with valid profiles.  Those profiles are stale
        // (computed at the pre-hold feed) and RT will hit them with wrong v0.
        // By falling through to the normal feed-change path, we get
        // invalidateNextNSegments + synchronous recomputeDownstreamProfiles
        // before RT can advance into the stale segments.
        if (g_recompute_feed_scale < 0.001 && current_feed > 0.001) {
            g_recompute_cursor = 0;
            g_next_feed_scale = -1.0;
            g_next_rapid_scale = -1.0;
            // Fall through to process recovery feed immediately
        } else {
            g_next_feed_scale = current_feed;
            g_next_rapid_scale = emcmotStatus->rapid_scale;
            return;
        }
    }

    // Cursor idle — start processing this feed change.
    // Debounce check (only when starting new work, not for queueing)
    double now_ms = etime_user() * 1000.0;
    if (now_ms - g_last_replan_time_ms < g_handoff_config.feed_override_debounce_ms) {
        return;
    }

    double snap_feed = current_feed;
    double snap_rapid = emcmotStatus->rapid_scale;

    // Two-phase feed hold: when user requests 0%, first decelerate to 0.1%
    // using normal Ruckig (Phase 1), then engage real 0% to stop completely
    // (Phase 2).  Phase 1 is a normal Ruckig branch — no special treatment.
    // Phase 2 only fires once the machine has actually decelerated to near the
    // 0.1% target velocity, checked via predictStateAtTime (not profile feed
    // label, which drops to 0.001 immediately on commit).
    static const double MINIMUM_RUCKIG_FEED = 0.001;  // 0.1%
    if (snap_feed < 0.001) {
        double elapsed = atomicLoadDouble(&tc->elapsed_time);
        PredictedState probe = predictStateAtTime(tc, elapsed);
        double vel_at_min_feed = tc->reqvel * MINIMUM_RUCKIG_FEED;
        if (!probe.valid || probe.velocity > vel_at_min_feed) {
            // Phase 1: still decelerating, use normal Ruckig at 0.1%
            snap_feed = MINIMUM_RUCKIG_FEED;
        }
        // else: Phase 2 — deceleration complete, allow real 0%
    }

    int commit_seg;
    if (current_feed < 0.001) {
        commit_seg = 1;
    } else {
        commit_seg = estimateCommitSegment(tp, current_feed);
    }

    if (commit_seg <= 1) {
        // Big active segment or short queue: branch on active (existing fast path)
        branch_valid = __atomic_load_n(&branch->valid, __ATOMIC_ACQUIRE);

        if (branch_valid) {
            int branch_taken = __atomic_load_n(&branch->taken, __ATOMIC_ACQUIRE);

            if (!branch_taken) {
                // Branch pending but not yet taken by RT.  Safe to overwrite
                // with the newer feed value — the old branch's feed is stale
                // since the user has moved the slider since it was created.
                __atomic_store_n(&branch->valid, 0, __ATOMIC_RELEASE);
                branch_valid = 0;
            } else {
                // RT already took this branch — it's being merged.
                // Don't overwrite during merge.
            }
        }

        if (!branch_valid) {
            double branch_t0 = etime_user();
            if (computeBranch(tp, tc, snap_feed)) {
                g_last_replan_time_ms = now_ms;
                g_commit_segment = 1;
                // Update committed feed so optimizer doesn't overwrite
                // our batch-written profiles with a stale feed value.
                g_committed_feed = snap_feed;
                g_committed_rapid = snap_rapid;

                // Immediately recompute downstream profiles at the branch feed.
                // computeBranch() has set achieved_exit_vel, so
                // recomputeDownstreamProfiles() seeds the correct entry
                // velocity for the next segment (via lines 1798-1800).
                // This closes the 5-10ms gap where nexttc had stale v[0].
                double branch_elapsed_us = (etime_user() - branch_t0) * 1e6;
                double budget_us = g_handoff_config.servo_cycle_time_sec * 0.5e6;

                invalidateNextNSegments(tp, INT_MAX);

                if (branch_elapsed_us < budget_us) {
                    // Have budget: recompute synchronously
                    int done = recomputeDownstreamProfiles(tp, snap_feed, snap_rapid);
                    g_recompute_cursor = done + 1;
                } else {
                    // No budget: cursor walk will handle it next cycle
                    g_recompute_cursor = 1;
                }

                g_recompute_feed_scale = snap_feed;
                g_recompute_rapid_scale = snap_rapid;
                g_recompute_first_batch_done = false;
            }
        }
    } else {
        // Active nearly done: defer feed change until next active has enough
        // remaining time for a safe branch. The slider position persists in
        // emcmotStatus->net_feed_scale — manageBranches will re-detect the
        // feed difference on the next cycle where estimateCommitSegment returns 1.
        // Do NOT update g_last_replan_time_ms — avoids blocking retry via debounce.
    }
}

/**
 * @brief Compute the velocity at which a stop profile crosses a segment boundary.
 *
 * Given a velocity-control stop profile (decelerating to zero) and the remaining
 * distance on the segment, find the velocity/acceleration state at the point
 * where displacement equals remaining distance.  If the stopping distance fits
 * within the remaining distance, spill_vel is 0 (no spill-over).
 *
 * Uses binary search on time to find the crossing point.
 */
static void computeSpillOver(const ruckig_profile_t *profile,
                             double remaining_dist,
                             double *spill_vel,
                             double *spill_acc)
{
    *spill_vel = 0.0;
    *spill_acc = 0.0;

    if (!profile || !profile->valid || remaining_dist <= 0.0) return;

    // Sample at profile end to get total stopping displacement
    double end_pos, end_vel, end_acc, end_jrk;
    int ok = ruckigProfileSample(const_cast<ruckig_profile_t*>(profile),
                                 profile->duration, &end_pos, &end_vel, &end_acc, &end_jrk);
    if (ok != 0) return;

    // If stopping distance fits within segment, no spill-over
    if (end_pos <= remaining_dist) return;

    // Binary search for time when displacement = remaining_dist
    double t_lo = 0.0;
    double t_hi = profile->duration;
    for (int i = 0; i < 50; i++) {
        double t_mid = (t_lo + t_hi) * 0.5;
        double p, v, a, j;
        ruckigProfileSample(const_cast<ruckig_profile_t*>(profile), t_mid, &p, &v, &a, &j);
        if (p < remaining_dist) {
            t_lo = t_mid;
        } else {
            t_hi = t_mid;
        }
        if (t_hi - t_lo < 1e-9) break;
    }

    // Sample at the crossing time
    double p, v, a, j;
    ruckigProfileSample(const_cast<ruckig_profile_t*>(profile), t_hi, &p, &v, &a, &j);
    *spill_vel = fmax(v, 0.0);
    *spill_acc = a;
}

/**
 * @brief Request a stop branch for abort (called from task layer)
 *
 * Called by emcTrajAbort() in the task thread BEFORE the EMCMOT_ABORT
 * command is processed by RT. This guarantees the stop branch is ready
 * when tpAbort() runs in the next servo cycle.
 *
 * Uses the same computeBranch(tp, tc, 0.0) path as feed hold / pause,
 * leveraging the existing branch/merge infrastructure for smooth stop.
 *
 * When the stopping distance exceeds the remaining distance on the active
 * segment, the deceleration spills over into downstream segments.  Each
 * downstream segment gets a velocity-control stop profile that starts from
 * the spill-over velocity, so the machine decelerates smoothly across
 * segment boundaries — just like a train braking across track sections.
 *
 * @param tp Trajectory planner structure (shared memory)
 * @return 0 on success, -1 on error
 */
extern "C" int tpRequestAbortBranch_9D(TP_STRUCT *tp)
{
    if (!tp) return -1;

    TC_STRUCT *tc = tcqItem_user(&tp->queue, 0);
    if (!tc || !tc->active || !tc->shared_9d.profile.valid) {
        // No active segment — RT will handle abort directly
        // (empty queue or segment without profile)
        return 0;
    }

    // Clear any pending branch so we can write the stop branch
    __atomic_store_n(&tc->shared_9d.branch.valid, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&tc->shared_9d.branch.taken, 0, __ATOMIC_RELEASE);

    // Compute stop branch for active segment (same as 0% feed hold).
    // Downstream segments are handled below via spill-over logic and
    // by the optimizer (which writes zero-motion profiles at feed < 0.001).
    if (!computeBranch(tp, tc, 0.0)) {
        return -1;
    }

    // Prevent feed-override merge from overwriting our stop profiles.
    // Both this flag and the merge check run in the same userspace thread,
    // so no atomics needed — no race by construction.
    tp->abort_profiles_written = 1;

    // Check if the stop profile spills over the active segment boundary.
    // The stop branch is now in tc->shared_9d.branch.profile.
    double remaining = tc->target - tc->shared_9d.branch.handoff_position;
    double spill_vel, spill_acc;
    computeSpillOver(&tc->shared_9d.branch.profile, remaining,
                     &spill_vel, &spill_acc);

    if (spill_vel < TP_VEL_EPSILON) {
        // Stopping distance fits within active segment — no spill-over needed
        return 0;
    }

    // Spill-over: the active segment's stop profile can't stop in time.
    // Write velocity-control stop profiles to downstream segments that
    // continue the deceleration from the spill-over point.
    TC_QUEUE_STRUCT *queue = &tp->queue;
    int queue_len = tcqLen_user(queue);
    double default_jerk = tc->maxjerk > 0 ? tc->maxjerk : g_handoff_config.default_max_jerk;

    double entry_vel = spill_vel;
    double entry_acc = spill_acc;

    for (int i = 1; i < queue_len && entry_vel > TP_VEL_EPSILON; i++) {
        TC_STRUCT *next = tcqItem_user(queue, i);
        if (!next || next->target < 1e-9) break;

        double vel_limit = getEffectiveVelLimit(tp, next);
        double max_jrk = next->maxjerk > 0 ? next->maxjerk : default_jerk;

        try {
            ruckig::Ruckig<1> otg(g_handoff_config.servo_cycle_time_sec);
            ruckig::InputParameter<1> input;
            ruckig::Trajectory<1> traj;


            input.control_interface = ruckig::ControlInterface::Velocity;
            input.current_position = {0.0};
            input.current_velocity = {entry_vel};
            input.current_acceleration = {entry_acc};
            input.target_velocity = {0.0};
            input.target_acceleration = {0.0};
            input.max_velocity = {vel_limit};
            input.max_acceleration = {next->maxaccel};
            input.max_jerk = {max_jrk};

            auto result = otg.calculate(input, traj);
            if (result != ruckig::Result::Working && result != ruckig::Result::Finished) {
                break;
            }

            ruckig_profile_t stop_profile;
            memset(&stop_profile, 0, sizeof(stop_profile));
            copyRuckigProfile(traj, &stop_profile);
            if (!stop_profile.valid || profileHasNegativeVelocity(&stop_profile)) {
                break;
            }
            stop_profile.computed_feed_scale = 0.0;
            stop_profile.computed_vel_limit = vel_limit;
            stop_profile.computed_vLimit = tp->vLimit;

            // Check if this stop profile also spills over this segment
            computeSpillOver(&stop_profile, next->target,
                             &entry_vel, &entry_acc);

            // Write the stop profile to this downstream segment
            __atomic_store_n(&next->shared_9d.profile.valid, 0, __ATOMIC_RELEASE);
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            next->shared_9d.profile = stop_profile;
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            __atomic_store_n(&next->shared_9d.profile.valid, 1, __ATOMIC_RELEASE);
            atomicStoreInt((int*)&next->shared_9d.optimization_state, TC_PLAN_FINALIZED);

        } catch (...) {
            break;
        }
    }

    return 0;
}

// Forward declaration (defined later in file)
int computeRuckigProfiles_9D(TP_STRUCT *tp, TC_QUEUE_STRUCT *queue, int optimization_depth);

/**
 * @brief Ensure profiles are computed when buffer runs critically low
 *
 * Called periodically to check if buffer is thin and there are segments
 * without valid profiles. If so, triggers optimization for those segments.
 *
 * This is the adaptive throttling mechanism - when buffer runs thin,
 * userspace speeds up by computing more profiles ahead of time.
 *
 * @param tp Trajectory planner structure
 */
static void ensureProfilesOnLowBuffer(TP_STRUCT *tp)
{
    if (!tp) return;

    // Rate limit this check to avoid overhead
    // NOTE: local static — persists across program runs! (potential non-determinism)
    static double last_check_ms = 0;
    double now_ms = etime_user() * 1000.0;
    if (now_ms - last_check_ms < 20.0) {  // Check at most every 20ms
        return;
    }
    last_check_ms = now_ms;

    TC_QUEUE_STRUCT *queue = &tp->queue;
    int queue_len = tcqLen_user(queue);
    if (queue_len < 2) return;

    int depth = queue_len;
    if (depth > MAX_LOOKAHEAD_DEPTH) depth = MAX_LOOKAHEAD_DEPTH;

    // === Phase 1: Urgent — missing profiles when buffer is critical ===
    double buffer_ms = calculateBufferTimeMs(tp);
    if (buffer_ms < g_handoff_config.min_buffer_time_ms) {
        bool needs_profiles = false;
        for (int i = 1; i < queue_len; i++) {
            TC_STRUCT *tc = tcqItem_user(queue, i);
            if (tc && !tc->shared_9d.profile.valid) {
                needs_profiles = true;
                break;
            }
        }

        if (needs_profiles) {
            rtapi_print_msg(RTAPI_MSG_DBG,
                "Adaptive throttle: buffer=%.0fms < min=%.0fms, computing profiles for %d segments\n",
                buffer_ms, g_handoff_config.min_buffer_time_ms, queue_len);

            int result = computeLimitingVelocities_9D(queue, depth, g_smoothing_data);
            if (result != 0) return;
            result = applyLimitingVelocities_9D(queue, g_smoothing_data, depth);
            if (result != 0) return;
            computeRuckigProfiles_9D(tp, queue, depth);
            return;  // Urgent work done, don't spend more time
        }
    }

    // === Phase 2: Convergence precomputation ===
    // Iterate backward+forward passes until all profiles stabilize.
    // The time budget (half a servo cycle) is the dynamic limiter —
    // no fixed iteration cap needed.  Each iteration propagates the
    // velocity cascade one step further; short-segment groups need
    // as many iterations as there are chained short segments.
    TC_STRUCT *active = tcqItem_user(queue, 0);
    if (!active || !active->active || !active->shared_9d.profile.valid) return;

    double remaining_time_ms = (active->shared_9d.profile.duration - active->elapsed_time) * 1000.0;
    if (remaining_time_ms < 10.0) return;  // Too close to segment end, don't risk it

    // Snapshot exit velocities of ALL non-active segments to detect convergence.
    // Use stack buffer for typical cases, heap for large queues.
    int check_count = queue_len - 1;  // Exclude active segment
    if (check_count <= 0) return;

    double stack_buf[64];
    double *prev_exit_vels = (check_count <= 64) ? stack_buf : new double[check_count];

    auto snapshotExitVels = [&]() {
        for (int i = 0; i < check_count; i++) {
            TC_STRUCT *tc = tcqItem_user(queue, i + 1);
            if (!tc) { prev_exit_vels[i] = 0.0; continue; }
            if (tc->shared_9d.profile.valid && tc->term_cond == TC_TERM_COND_TANGENT) {
                prev_exit_vels[i] = profileExitVelUnscaled(&tc->shared_9d.profile);
            } else {
                prev_exit_vels[i] = 0.0;
            }
        }
    };

    auto checkConverged = [&]() -> bool {
        for (int i = 0; i < check_count; i++) {
            TC_STRUCT *tc = tcqItem_user(queue, i + 1);
            if (!tc) continue;
            double new_exit_vel = 0.0;
            if (tc->shared_9d.profile.valid && tc->term_cond == TC_TERM_COND_TANGENT) {
                new_exit_vel = profileExitVelUnscaled(&tc->shared_9d.profile);
            }
            if (fabs(new_exit_vel - prev_exit_vels[i]) > 0.1) return false;
        }
        return true;
    };

    // Time budget: half a servo cycle of wall time
    double budget_us = g_handoff_config.servo_cycle_time_sec * 0.5e6;
    double iter_start = etime_user();

    // Iterate until converged or time budget exhausted
    int conv_iters = 0;
    bool conv_result = false;
    for (;;) {
        snapshotExitVels();

        int result = computeLimitingVelocities_9D(queue, depth, g_smoothing_data);
        if (result != 0) break;
        result = applyLimitingVelocities_9D(queue, g_smoothing_data, depth);
        if (result != 0) break;
        computeRuckigProfiles_9D(tp, queue, depth);
        conv_iters++;

        if (checkConverged()) { conv_result = true; break; }

        double elapsed_us = (etime_user() - iter_start) * 1e6;
        if (elapsed_us >= budget_us) break;
    }

    (void)conv_result;  // Used only for convergence check above

    if (prev_exit_vels != stack_buf) delete[] prev_exit_vels;
}

/**
 * @brief Check for feed override changes (legacy wrapper)
 *
 * This function is kept for API compatibility. The actual branch/merge
 * logic is now in manageBranches().
 *
 * @deprecated Use manageBranches() directly
 * @param tp Trajectory planner structure
 */
extern "C" void checkFeedOverride(TP_STRUCT *tp)
{
    // Abort profiles written — no feed-override activity allowed.
    // This covers the window between tpRequestAbortBranch_9D() (userspace,
    // sets abort_profiles_written=1) and tpAbort() (RT, sets tp->aborting=1).
    // Once tp->aborting is visible, manageBranches() returns early anyway.
    if (tp && tp->abort_profiles_written) {
        g_recompute_cursor = 0;
        return;
    }

    // Forward to new branch/merge implementation
    manageBranches(tp);

    // Incremental downstream profile recomputation
    // After a feed change, walk through queued segments a few per tick,
    // recomputing Ruckig profiles for the new feed scale.
    // The immediate 3 segments are handled synchronously at merge/commit time;
    // this cursor handles the rest incrementally.
    if (g_recompute_cursor > 0 && tp) {
        TC_QUEUE_STRUCT *queue = &tp->queue;
        int queue_len = tcqLen_user(queue);

        // Get default jerk
        TC_STRUCT *active = tcqItem_user(queue, 0);
        double default_jerk = (active && active->maxjerk > 0)
            ? active->maxjerk : g_handoff_config.default_max_jerk;
        if (default_jerk < 1.0) default_jerk = g_handoff_config.default_max_jerk;

        // Seed prev_exit_vel from segment before cursor.
        // Use unscaled final_vel (consistent with loop body which scales at compute time).
        // For active segment (cursor==1), use profile exit velocity un-scaled since
        // the branch may have changed achievable exit from the backward pass value.
        double prev_exit_vel = 0.0;
        double prev_exit_feed_scale = 1.0;
        if (g_recompute_cursor > 1) {
            TC_STRUCT *prev = tcqItem_user(queue, g_recompute_cursor - 1);
            if (prev && prev->term_cond == TC_TERM_COND_TANGENT) {
                // Non-active segment: profile was computed by us, use its exit vel
                if (prev->shared_9d.profile.valid) {
                    prev_exit_vel = profileExitVelUnscaled(&prev->shared_9d.profile);
                    prev_exit_feed_scale = prev->shared_9d.profile.computed_feed_scale;
                } else {
                    prev_exit_vel = atomicLoadDouble(&prev->shared_9d.final_vel);
                    prev_exit_feed_scale = tpGetSnapshotFeedScale(prev,
                        g_recompute_feed_scale, g_recompute_rapid_scale);
                }
            }
        } else if (g_recompute_cursor == 1 && active) {
            if (active->term_cond == TC_TERM_COND_TANGENT &&
                active->shared_9d.profile.valid) {
                // If a pending branch exists, its exit velocity is the future truth
                int bv = __atomic_load_n(&active->shared_9d.branch.valid, __ATOMIC_ACQUIRE);
                if (bv && active->shared_9d.branch.profile.valid &&
                    active->shared_9d.branch.profile.computed_feed_scale > 0.001) {
                    prev_exit_vel = profileExitVelUnscaled(&active->shared_9d.branch.profile);
                    prev_exit_feed_scale = active->shared_9d.branch.feed_scale;
                } else {
                    prev_exit_vel = profileExitVelUnscaled(&active->shared_9d.profile);
                    prev_exit_feed_scale = active->shared_9d.profile.computed_feed_scale;
                }
            }
        }

        int recomputed = 0;
        int i = g_recompute_cursor;
        for (; i < queue_len; i++) {

            TC_STRUCT *tc = tcqItem_user(queue, i);
            if (!tc || tc->target < 1e-9) {
                prev_exit_vel = 0.0;
                prev_exit_feed_scale = 1.0;
                continue;
            }

            // Per-segment feed scale from snapshots (locked at trigger time)
            double feed_scale = tpGetSnapshotFeedScale(tc,
                g_recompute_feed_scale, g_recompute_rapid_scale);

            // Skip if profile already matches target feed
            double profile_feed = tc->shared_9d.profile.computed_feed_scale;
            if (tc->shared_9d.profile.valid &&
                fabs(profile_feed - feed_scale) < 0.005) {
                // Feed matches — but check if entry velocity also matches
                double expected_v0 = prev_exit_vel * prev_exit_feed_scale;
                double profile_v0 = tc->shared_9d.profile.v[0];
                double v0_delta = fabs(profile_v0 - expected_v0);
                if (v0_delta < 0.5) {
                    // Both feed and entry velocity match — genuinely up-to-date
                    if (tc->term_cond == TC_TERM_COND_TANGENT) {
                        prev_exit_vel = profileExitVelUnscaled(&tc->shared_9d.profile);
                        prev_exit_feed_scale = tc->shared_9d.profile.computed_feed_scale;
                    } else {
                        prev_exit_vel = 0.0;
                        prev_exit_feed_scale = 1.0;
                    }
                    continue;
                }
                // Fall through to recompute — entry velocity is stale
            }

            double v_entry = prev_exit_vel;
            double v_exit = (tc->term_cond == TC_TERM_COND_TANGENT)
                ? atomicLoadDouble(&tc->shared_9d.final_vel) : 0.0;
            prev_exit_vel = v_exit;
            // prev_exit_feed_scale set below after profile compute

            // Get previous segment's kink_vel for entry cap check
            TC_STRUCT *prev_tc = (i > 0) ? tcqItem_user(queue, i - 1) : NULL;
            double prev_kink = (prev_tc && prev_tc->kink_vel > 0) ? prev_tc->kink_vel : -1.0;

            atomicStoreDouble(&tc->shared_9d.entry_vel, v_entry);

            double vel_limit = getEffectiveVelLimit(tp, tc);
            double max_jrk = tc->maxjerk > 0 ? tc->maxjerk : default_jerk;

            if (feed_scale < 0.001) {
                // Don't clobber a valid non-hold profile with a feed-hold
                // profile.  The committed-feed mechanism ensures this pass
                // sees feed_scale≈0 while the reactive path (branch /
                // cursor walk) will restore motion at the new feed.
                // If committed-feed ever drifts, this guard is the
                // last line of defence against erasing good profiles.
                if (tc->shared_9d.profile.valid &&
                    tc->shared_9d.profile.computed_feed_scale > 0.001) {
                    if (tc->term_cond == TC_TERM_COND_TANGENT) {
                        prev_exit_vel = profileExitVelUnscaled(&tc->shared_9d.profile);
                    } else {
                        prev_exit_vel = 0.0;
                    }
                    prev_exit_feed_scale = tc->shared_9d.profile.computed_feed_scale;
                    continue;
                }
                createFeedHoldProfile(tc, vel_limit, tp->vLimit, "computeRuckig_backward");
                atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_FINALIZED);
            } else {
                double max_vel = applyVLimit(tp, tc, vel_limit * feed_scale);
                double max_acc = tcGetTangentialMaxAccel_9D_user(tc);
                // Restore absolute velocity using previous segment's feed scale
                double scaled_v_entry = fmin(v_entry * prev_exit_feed_scale, max_vel);

                // Cap entry velocity by previous segment's kink limit
                if (prev_kink > 0)
                    scaled_v_entry = fmin(scaled_v_entry, prev_kink);

                double scaled_v_exit_pre = fmin(v_exit  * feed_scale, max_vel);
                double scaled_v_exit = applyKinkVelCap(scaled_v_exit_pre, v_exit, max_vel, tc->kink_vel);
                double desired_fvel_for_profile = scaled_v_exit;

                applyBidirectionalReachability(scaled_v_entry, scaled_v_exit,
                    tc->target, max_acc, max_jrk);

                try {
                    RuckigProfileParams rp = {scaled_v_entry, scaled_v_exit,
                        max_vel, max_acc, max_jrk, tc->target,
                        feed_scale, vel_limit, tp->vLimit, desired_fvel_for_profile};

                    if (computeAndStoreProfile(tc, rp)) {
                        atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_FINALIZED);
                        if (tc->term_cond == TC_TERM_COND_TANGENT)
                            prev_exit_vel = profileExitVelUnscaled(&tc->shared_9d.profile);
                        prev_exit_feed_scale = feed_scale;

                        // One-step backtrack: if this segment's entry was
                        // capped by backward reachability (v[0] < predecessor's
                        // v[end]), recompute the predecessor with a lower exit
                        // target so the chain is gap-free at this junction.
                        // The backward pass runs at feed=1.0 and doesn't know
                        // that at higher feed, reachability is tighter.
                        // Skip active segment — rewriting triggers STOPWATCH_RESET
                        // with velocity discontinuity worse than the chain gap.
                        TC_STRUCT *prev_seg = (i > 0) ? tcqItem_user(queue, i - 1) : NULL;
                        if (prev_seg && !prev_seg->active
                            && prev_seg->term_cond == TC_TERM_COND_TANGENT
                            && prev_seg->shared_9d.profile.valid) {
                            double prev_v_end = prev_seg->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
                            double curr_v0 = tc->shared_9d.profile.v[0];
                            if (prev_v_end > curr_v0 + 0.01) {
                                // Recompute predecessor with exit = curr_v0
                                double p_entry = prev_seg->shared_9d.profile.v[0];
                                double p_vel_limit = getEffectiveVelLimit(tp, prev_seg);
                                double p_max_vel = applyVLimit(tp, prev_seg, p_vel_limit * feed_scale);
                                double p_max_acc = tcGetTangentialMaxAccel_9D_user(prev_seg);
                                double p_max_jrk = prev_seg->maxjerk > 0 ? prev_seg->maxjerk : default_jerk;
                                double p_exit = curr_v0;
                                applyBidirectionalReachability(p_entry, p_exit,
                                    prev_seg->target, p_max_acc, p_max_jrk);
                                RuckigProfileParams rp_prev = {p_entry, p_exit,
                                    p_max_vel, p_max_acc, p_max_jrk, prev_seg->target,
                                    feed_scale, p_vel_limit, tp->vLimit, p_exit};
                                if (computeAndStoreProfile(prev_seg, rp_prev)) {
                                    atomicStoreInt((int*)&prev_seg->shared_9d.optimization_state,
                                        TC_PLAN_FINALIZED);
                                }
                            }
                        }
                    } else {
                        prev_exit_vel = 0.0;
                        prev_exit_feed_scale = 1.0;
                        rtapi_print_msg(RTAPI_MSG_ERR, "Cursor recompute fail seg=%d",
                            tc->id);
                    }
                } catch (std::exception &e) {
                    __atomic_store_n(&tc->shared_9d.profile.valid, 0, __ATOMIC_RELEASE);
                    prev_exit_vel = 0.0;
                    prev_exit_feed_scale = 1.0;
                    rtapi_print_msg(RTAPI_MSG_ERR, "Cursor recompute exception seg=%d: %s",
                        tc->id, e.what());
                }
            }

            recomputed++;
        }

        if (recomputed > 0) {
            g_recompute_first_batch_done = true;
            // Update throughput estimate (EMA, alpha=0.3)
            g_segments_per_tick = 0.7 * g_segments_per_tick + 0.3 * recomputed;
        }

        // Advance or finish
        if (i >= queue_len) {
            g_recompute_cursor = 0;  // Done — walked entire queue
            // All segments are now consistently at the cursor walk's feed.
            g_committed_feed = g_recompute_feed_scale;
            g_committed_rapid = g_recompute_rapid_scale;

            // Promote queued feed change if present
            if (g_next_feed_scale >= 0) {
                double next_feed = g_next_feed_scale;
                double next_rapid = g_next_rapid_scale;
                g_next_feed_scale = -1.0;
                g_next_rapid_scale = -1.0;

                // Only start new cursor walk if queued feed differs from committed
                if (fabs(next_feed - g_committed_feed) > 0.005 ||
                    fabs(next_rapid - g_committed_rapid) > 0.005) {
                    int commit_seg = estimateCommitSegment(tp, next_feed);
                    if (commit_seg <= 1) {
                        // Big active segment: let manageBranches handle it next cycle
                    } else {
                        // Defer: manageBranches will re-detect from net_feed_scale
                    }
                }
            }
        } else {
            g_recompute_cursor = i;  // Resume here next tick
        }
    }

    // Adaptive throttling: ensure profiles computed when buffer is thin
    ensureProfilesOnLowBuffer(tp);

}

/**
 * @brief Compute Ruckig trajectory profiles for all segments
 *
 * Forward pass through queue: for each segment, compute Ruckig trajectory
 * using entry velocity from previous segment and exit velocity from
 * backward pass optimization.
 *
 * Phase 4 TODO (Blending): The backward pass currently sets final_vel as a
 * static kink velocity at each junction. With blending, junctions are replaced
 * by blend regions whose entry velocity depends on RT's actual position/speed.
 * The backward pass should constrain final_vel to the blend entry velocity
 * computed by the blend solver, which already accounts for achievable velocity.
 * The forward pass (reading back actual Ruckig exit velocities) will still be
 * needed — the same pattern used by manageBranches for every segment should
 * apply uniformly, including the first segment.
 *
 * @param queue Queue of TC segments
 * @param optimization_depth Number of segments to process
 * @return 0 on success
 */
int computeRuckigProfiles_9D(TP_STRUCT *tp, TC_QUEUE_STRUCT *queue, int optimization_depth)
{
    if (!tp || !queue || optimization_depth <= 0) return 0;

    // Get default jerk from first segment
    // tc->maxjerk is set from INI [JOINT_N] MAX_JERK, transformed through Jacobian
    // in userspace_kinematics.cc computeJointSpaceSegment()
    TC_STRUCT *first_tc = tcqBack_user(queue, -(optimization_depth - 1));
    // Use tc->maxjerk if valid, otherwise fall back to config default
    double default_jerk = first_tc ? first_tc->maxjerk : g_handoff_config.default_max_jerk;
    if (default_jerk < 1.0) default_jerk = g_handoff_config.default_max_jerk;

    // Use committed feed, not live feed.
    // The committed feed is only updated when a cursor walk completes a full pass,
    // meaning all downstream segments have been consistently recomputed.
    // This prevents overwriting profiles that the reactive path
    // (recomputeDownstreamProfiles / cursor walk) is actively updating.
    // During a feed transition, this function defers to the reactive path.
    double live_feed = emcmotStatus ? emcmotStatus->feed_scale : 1.0;
    double live_rapid = emcmotStatus ? emcmotStatus->rapid_scale : 1.0;
    if (g_committed_feed < 0) {
        // First call: initialize from live
        g_committed_feed = live_feed;
        g_committed_rapid = live_rapid;
    }
    double pass_feed_scale = g_committed_feed;
    double pass_rapid_scale = g_committed_rapid;

    // Forward pass: oldest to newest (from -(depth-1) to 0)
    // Entry velocity for segment k comes from exit velocity of segment k-1
    double prev_exit_vel = 0.0;
    bool prev_exit_vel_known = false;
    // Feed scale used to unscale prev_exit_vel.  Needed to restore absolute
    // velocity at feed/rapid boundaries where the arriving segment's feed_scale
    // differs from the departing segment's.
    double prev_exit_feed_scale = 1.0;

    // Seed prev_exit_vel from the segment just before the optimization window.
    // Without this, the oldest segment in the window always gets v_entry=0
    // because no predecessor within the window sets prev_exit_vel.
    {
        TC_STRUCT *pre_window = tcqBack_user(queue, -optimization_depth);
        if (pre_window && pre_window->term_cond == TC_TERM_COND_TANGENT
            && pre_window->shared_9d.profile.valid
            && pre_window->shared_9d.profile.computed_feed_scale > 0.001) {
            prev_exit_vel = profileExitVelUnscaled(&pre_window->shared_9d.profile);
            // If the stored profile has v_exit≈0 but final_vel is non-zero,
            // the profile was poisoned by Fix 4 before a blend was created.
            // Use final_vel (set by tpSetupBlend9D at enqueue time) instead.
            if (prev_exit_vel < 1e-6) {
                double fv = atomicLoadDouble(&pre_window->shared_9d.final_vel);
                if (fv > 1e-6)
                    prev_exit_vel = fv;
            }
            prev_exit_feed_scale = pre_window->shared_9d.profile.computed_feed_scale;
            prev_exit_vel_known = true;
        }
    }

    // Profile swaps are safe because RT detects generation-counter changes
    // and performs a stopwatch reset (position_base = progress, elapsed_time = 0).
    // No frozen zone needed — all segments can be freely recomputed.

    for (int k = optimization_depth - 1; k >= 0; k--) {
        TC_STRUCT *tc = tcqBack_user(queue, -k);
        if (!tc || tc->target < 1e-9) {
            prev_exit_vel = 0.0;  // After dwell/zero-length, next segment starts from rest
            prev_exit_feed_scale = 1.0;
            prev_exit_vel_known = true;
            continue;
        }

        // Skip active segment — its profile is managed by the branch/merge
        // system which accounts for position_base. Overwriting here with
        // target_position=tc->target would ignore position_base, causing
        // the profile to overshoot the segment end.
        if (tc->active) {
            if (tc->term_cond == TC_TERM_COND_TANGENT && tc->shared_9d.profile.valid) {
                // If a pending branch exists, its exit velocity reflects the
                // new feed — use it instead of the stale main profile.
                int bv = __atomic_load_n(&tc->shared_9d.branch.valid, __ATOMIC_ACQUIRE);
                if (bv && tc->shared_9d.branch.profile.valid &&
                    tc->shared_9d.branch.profile.computed_feed_scale > 0.001) {
                    prev_exit_vel = profileExitVelUnscaled(&tc->shared_9d.branch.profile);
                    prev_exit_feed_scale = tc->shared_9d.branch.feed_scale;
                } else {
                    prev_exit_vel = profileExitVelUnscaled(&tc->shared_9d.profile);
                    prev_exit_feed_scale = tc->shared_9d.profile.computed_feed_scale;
                }
            } else {
                prev_exit_vel = 0.0;
                prev_exit_feed_scale = 1.0;
            }
            prev_exit_vel_known = true;
            continue;
        }

        // Check if profile needs (re)computation
        // Recompute if:
        // 1. Profile not valid
        // 2. Marked for recomputation (TC_PLAN_UNTOUCHED)
        // 3. Profile's feed scale doesn't match current feed scale (race condition fix)
        TCPlanningState opt_state = (TCPlanningState)__atomic_load_n(
            (int*)&tc->shared_9d.optimization_state, __ATOMIC_ACQUIRE);

        bool needs_recompute = false;
        bool is_first_profile = !tc->shared_9d.profile.valid;

        if (!tc->shared_9d.profile.valid) {
            needs_recompute = true;
        } else if (opt_state == TC_PLAN_UNTOUCHED) {
            needs_recompute = true;
        }

        // Also check for feed scale mismatch (handles race condition where
        // segments are invalidated by index but RT shifts queue indices).
        // Use pass-level snapshot for consistency across all segments.
        //
        // SKIP this check when a cursor walk is active — the cursor walk is
        // in the process of recomputing all segments at the new feed.
        // Overwriting here would fight the cursor walk with stale committed_feed.
        double seg_feed = tpGetSnapshotFeedScale(tc, pass_feed_scale, pass_rapid_scale);
        if (g_recompute_cursor == 0 && !needs_recompute && tc->shared_9d.profile.valid) {
            double profile_feed = tc->shared_9d.profile.computed_feed_scale;
            if (fabs(profile_feed - seg_feed) > 0.005) {
                needs_recompute = true;
            }
        }

        // Check if final velocity changed since last profile computation
        // (e.g., kink velocity was set by a newly added segment)
        // Compare the current desired exit velocity against what we asked for
        // when the profile was last computed.  This avoids infinite recomputation
        // when Ruckig can't physically reach the desired velocity (reachability
        // limited by segment length) — the achieved != desired gap is permanent,
        // but if the desired hasn't changed, recomputing won't help.
        if (!needs_recompute && tc->shared_9d.profile.valid) {
            double desired_final_vel = atomicLoadDouble(&tc->shared_9d.final_vel);
            double vel_limit_here = getEffectiveVelLimit(tp, tc);
            double max_vel_here = applyVLimit(tp, tc, vel_limit_here * seg_feed);
            double scaled_desired = fmin(desired_final_vel * seg_feed, max_vel_here);
            scaled_desired = applyKinkVelCap(scaled_desired, desired_final_vel, max_vel_here, tc->kink_vel);
            double prev_desired = tc->shared_9d.profile.computed_desired_fvel;
            if (fabs(scaled_desired - prev_desired) > 1e-6) {
                needs_recompute = true;
            }
        }

        // Check if entry velocity changed since last profile computation
        // (e.g., backward pass reduced predecessor's exit velocity)
        // Only check when prev_exit_vel comes from a real source (not the
        // 0.0 default at the start of the optimization window)
        if (!needs_recompute && tc->shared_9d.profile.valid && prev_exit_vel_known) {
            double stored_entry = atomicLoadDouble(&tc->shared_9d.entry_vel);
            if (fabs(prev_exit_vel - stored_entry) > 1e-6) {
                needs_recompute = true;
            }
        }

        if (!needs_recompute) {
            // Profile valid and up-to-date — read actual exit velocity from profile
            if (tc->term_cond == TC_TERM_COND_TANGENT && tc->shared_9d.profile.valid) {
                prev_exit_vel = profileExitVelUnscaled(&tc->shared_9d.profile);
                prev_exit_feed_scale = tc->shared_9d.profile.computed_feed_scale;
            } else {
                prev_exit_vel = 0.0;
                prev_exit_feed_scale = 1.0;
            }
            prev_exit_vel_known = true;
            continue;
        }

        // Get velocities from backward pass
        // v_entry = exit velocity of previous segment (propagated forward)
        // v_exit = this segment's final velocity (computed by backward pass)
        // Only TANGENT segments (promoted G61) have non-zero exit velocity;
        // STOP (G61.1) and unpromoted EXACT segments must decelerate to zero.
        double v_entry = prev_exit_vel;
        double v_exit = (tc->term_cond == TC_TERM_COND_TANGENT)
            ? atomicLoadDouble(&tc->shared_9d.final_vel) : 0.0;

        // --- Fix 4: Pessimistic initial profiles ---
        // When building the FIRST profile for a segment, use exit velocity 0
        // regardless of what the backward pass says.  The backward pass may
        // not have converged yet (it runs iteratively, raising exit velocities
        // over multiple optimizer cycles).  If we use an unconverged optimistic
        // value, the profile allows high-speed traversal — but then when the
        // backward pass corrects downward, the replacement profile arrives
        // while RT is already executing the stale one, causing jerk spikes.
        //
        // By starting pessimistic (decel to 0), the first profile is safe.
        // Subsequent recomputes raise the exit velocity as the backward pass
        // converges.  These recomputes happen while the segment is still far
        // from execution (outside the frozen zone), so the profile swap is safe.
        //
        // Exception 1: TC_BEZIER blend segments have pre-computed exit
        // velocities from tpSetupBlend9D — deterministic, no convergence needed.
        //
        // Exception 2: Segments whose successor is a TC_BEZIER blend also
        // have their final_vel set by tpSetupBlend9D at enqueue time.
        // Forcing v_exit=0 here would poison the blend's v_entry (profiled
        // with v0=0 while actual junction velocity is v_plan).
        {
            TC_STRUCT *next_seg = (k > 0) ? tcqBack_user(queue, -(k-1)) : NULL;
            bool next_is_blend = (next_seg && next_seg->motion_type == TC_BEZIER);
            if (tc->motion_type == TC_BEZIER || next_is_blend)
                is_first_profile = false;
        }
        if (is_first_profile && v_exit > 0.0) {
            v_exit = 0.0;
        }

        // prev_exit_vel is updated AFTER Ruckig compute (below) to use
        // the actual achievable exit velocity, not the backward-pass ideal.

        // Store entry velocity for RT layer
        atomicStoreDouble(&tc->shared_9d.entry_vel, v_entry);

        // Get per-segment feed scale from the pass-level snapshot.
        // Using the same snapshot as the mismatch check above ensures
        // the profile we write matches the feed_scale we decided on.
        double feed_scale = tpGetSnapshotFeedScale(tc, pass_feed_scale, pass_rapid_scale);

        // Get base velocity limit (reqvel clamped to maxvel)
        // vLimit is applied AFTER feed scaling (it's an absolute cap)
        double vel_limit = getEffectiveVelLimit(tp, tc);

        // Handle feed hold (0% override) specially: create a "stay at start" profile
        // This is a minimal valid profile that keeps position at 0 indefinitely
        // RT will detect feed hold via computed_feed_scale and wait for feed restore
        if (feed_scale < 0.001) {
            // Don't clobber a valid non-hold profile with a feed-hold
            // profile.  The committed-feed mechanism ensures this pass
            // sees feed_scale≈0 while the reactive path (branch /
            // cursor walk) will restore motion at the new feed.
            // If committed-feed ever drifts, this guard is the
            // last line of defence against erasing good profiles.
            if (tc->shared_9d.profile.valid &&
                tc->shared_9d.profile.computed_feed_scale > 0.001) {
                prev_exit_vel = (tc->term_cond == TC_TERM_COND_TANGENT)
                    ? profileExitVelUnscaled(&tc->shared_9d.profile) : 0.0;
                prev_exit_feed_scale = tc->shared_9d.profile.computed_feed_scale;
                prev_exit_vel_known = true;
                continue;
            }
            createFeedHoldProfile(tc, vel_limit, tp->vLimit, "computeRuckig_forward");
            atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_FINALIZED);
        } else {
            // Normal profile computation

            // Kinematic limits
            // Apply vLimit AFTER feed scaling (vLimit is absolute, not scaled)
            double max_vel = applyVLimit(tp, tc, vel_limit * feed_scale);
            double max_acc = tcGetTangentialMaxAccel_9D_user(tc);
            double max_jrk = tc->maxjerk > 0 ? tc->maxjerk : default_jerk;

            // Apply feed override to entry/exit velocities.
            // shared_9d.final_vel stores UNSCALED (absolute) velocities —
            // consistent with kink_vel and future blend_vel.
            // Feed override is applied here, once, in the Ruckig profile.
            // Clamp to max_vel to avoid Ruckig ErrorInvalidInput (-100).
            //
            // Restore absolute velocity using the PREVIOUS segment's feed scale
            // (the one profileExitVelUnscaled divided by).  At feed/rapid
            // boundaries (e.g. G1→G0), feed_scale ≠ prev_exit_feed_scale,
            // and using feed_scale would halve/double the entry velocity.
            double scaled_v_entry = fmin(v_entry * prev_exit_feed_scale, max_vel);

            // Cap entry velocity by previous segment's kink limit
            {
                TC_STRUCT *prev_tc_kink = (k < optimization_depth - 1) ? tcqBack_user(queue, -(k+1)) : NULL;
                double prev_kink = (prev_tc_kink && prev_tc_kink->kink_vel > 0) ? prev_tc_kink->kink_vel : -1.0;
                if (prev_kink > 0)
                    scaled_v_entry = fmin(scaled_v_entry, prev_kink);
            }

            double scaled_v_exit  = fmin(v_exit  * feed_scale, max_vel);
            scaled_v_exit = applyKinkVelCap(scaled_v_exit, v_exit, max_vel, tc->kink_vel);

            // Record the desired exit velocity (pre-reachability cap) for
            // convergence detection — see RECOMP_FVEL check above.
            double desired_fvel_for_profile = scaled_v_exit;

            applyBidirectionalReachability(scaled_v_entry, scaled_v_exit,
                tc->target, max_acc, max_jrk);

            try {
                RuckigProfileParams rp = {scaled_v_entry, scaled_v_exit,
                    max_vel, max_acc, max_jrk, tc->target,
                    feed_scale, vel_limit, tp->vLimit, desired_fvel_for_profile};

                if (computeAndStoreProfile(tc, rp)) {
                    atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_FINALIZED);

                    // Propagate actual achievable exit velocity (un-scaled)
                    prev_exit_vel = profileExitVelUnscaled(&tc->shared_9d.profile);
                    prev_exit_feed_scale = feed_scale;
                    prev_exit_vel_known = true;

                    // One-step backtrack: fix backward reachability gap
                    // Skip active segment — rewriting triggers STOPWATCH_RESET
                    // with velocity discontinuity worse than the chain gap.
                    TC_STRUCT *prev_seg = (k < optimization_depth - 1)
                        ? tcqBack_user(queue, -(k+1)) : NULL;
                    if (prev_seg && !prev_seg->active
                        && prev_seg->term_cond == TC_TERM_COND_TANGENT
                        && prev_seg->shared_9d.profile.valid) {
                        double prev_v_end = prev_seg->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
                        double curr_v0 = tc->shared_9d.profile.v[0];
                        if (prev_v_end > curr_v0 + 0.01) {
                            double p_entry = prev_seg->shared_9d.profile.v[0];
                            double p_vel_limit = getEffectiveVelLimit(tp, prev_seg);
                            double p_feed = prev_seg->shared_9d.profile.computed_feed_scale;
                            if (p_feed < 0.001) p_feed = feed_scale;
                            double p_max_vel = applyVLimit(tp, prev_seg, p_vel_limit * p_feed);
                            double p_max_acc = tcGetTangentialMaxAccel_9D_user(prev_seg);
                            double p_max_jrk = prev_seg->maxjerk > 0 ? prev_seg->maxjerk : default_jerk;
                            double p_exit = curr_v0;
                            applyBidirectionalReachability(p_entry, p_exit,
                                prev_seg->target, p_max_acc, p_max_jrk);
                            RuckigProfileParams rp_prev = {p_entry, p_exit,
                                p_max_vel, p_max_acc, p_max_jrk, prev_seg->target,
                                p_feed, p_vel_limit, tp->vLimit, p_exit};
                            if (computeAndStoreProfile(prev_seg, rp_prev)) {
                                atomicStoreInt((int*)&prev_seg->shared_9d.optimization_state,
                                    TC_PLAN_FINALIZED);
                            }
                        }
                    }
                } else {
                    prev_exit_vel = 0.0;
                    prev_exit_feed_scale = 1.0;
                    prev_exit_vel_known = true;
                    rtapi_print_msg(RTAPI_MSG_ERR, "Ruckig fail seg=%d v[%.1f->%.1f] "
                        "max_v=%.2f len=%.3f term=%d",
                        tc->id, v_entry, v_exit,
                        max_vel, tc->target, tc->term_cond);
                }
            } catch (std::exception &e) {
                __atomic_store_n(&tc->shared_9d.profile.valid, 0, __ATOMIC_RELEASE);
                prev_exit_vel = 0.0;
                prev_exit_feed_scale = 1.0;
                prev_exit_vel_known = true;
                rtapi_print_msg(RTAPI_MSG_ERR, "Ruckig exception seg=%d: %s", tc->id, e.what());
            }
        }

        // Feed hold: next segment starts from rest
        if (feed_scale < 0.001) {
            prev_exit_vel = 0.0;
            prev_exit_feed_scale = 1.0;
            prev_exit_vel_known = true;
        }
    }

    // --- Backward fixup pass ---
    // The forward pass computes each segment's profile independently: it scales
    // v_exit by feed_scale and applies reachability caps within the segment.
    // But it doesn't check whether the SUCCESSOR can accept the exit velocity.
    //
    // At feed_scale=1.0 this is fine — the backward pass already ensures
    // consistency at unscaled velocities.  At feed_scale>1.0, scaling can push
    // a predecessor's exit beyond what the successor's reachability allows
    // (short segment + low exit kink = can't decelerate fast enough).
    //
    // This fixup walks backward: for each (A, B) pair, compute the max entry
    // velocity B can accept given B's profile exit velocity, length, and
    // kinematic limits.  If A's profile exit exceeds that, recompute A with
    // the capped exit.
    for (int k = 0; k < optimization_depth - 1; k++) {
        TC_STRUCT *tc_B = tcqBack_user(queue, -k);
        TC_STRUCT *tc_A = tcqBack_user(queue, -(k + 1));
        if (!tc_A || !tc_B) continue;
        if (tc_A->active) continue;  // Active segment managed by branch/merge
        if (!tc_A->shared_9d.profile.valid || !tc_B->shared_9d.profile.valid) continue;
        if (tc_A->term_cond != TC_TERM_COND_TANGENT) continue;  // Only blending segments

        double A_v_exit = tc_A->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
        double B_v_entry = tc_B->shared_9d.profile.v[0];

        // If A's exit already matches B's entry, no fixup needed
        if (A_v_exit <= B_v_entry + 0.01) continue;

        // Compute max entry velocity B can accept
        double B_v_exit_scaled = tc_B->shared_9d.profile.v[RUCKIG_PROFILE_PHASES];
        double B_acc = tcGetTangentialMaxAccel_9D_user(tc_B);
        double B_jrk = tc_B->maxjerk > 0 ? tc_B->maxjerk : default_jerk;
        double B_max_entry = jerkLimitedMaxEntryVelocity(
            B_v_exit_scaled, tc_B->target, B_acc, B_jrk);

        // Also cap by B's max_vel (can't enter faster than segment allows)
        double B_feed = tc_B->shared_9d.profile.computed_feed_scale;
        double B_vel_limit = getEffectiveVelLimit(tp, tc_B);
        double B_max_vel = applyVLimit(tp, tc_B, B_vel_limit * B_feed);
        B_max_entry = fmin(B_max_entry, B_max_vel);

        // Also cap by kink at the junction (stored on A)
        if (tc_A->kink_vel > 0)
            B_max_entry = fmin(B_max_entry, tc_A->kink_vel);

        if (A_v_exit <= B_max_entry + 0.01) continue;

        // A's exit exceeds what B can accept — recompute A with capped exit
        double A_feed = tc_A->shared_9d.profile.computed_feed_scale;
        if (A_feed < 0.001) continue;

        double A_vel_limit = getEffectiveVelLimit(tp, tc_A);
        double A_max_vel = applyVLimit(tp, tc_A, A_vel_limit * A_feed);
        double A_acc = tcGetTangentialMaxAccel_9D_user(tc_A);
        double A_jrk = tc_A->maxjerk > 0 ? tc_A->maxjerk : default_jerk;

        // Reconstruct A's entry from its profile
        double A_v_entry_scaled = tc_A->shared_9d.profile.v[0];

        // Capped exit velocity for A
        double capped_exit = B_max_entry;

        // Bidirectional reachability: cap both entry and exit
        applyBidirectionalReachability(A_v_entry_scaled, capped_exit,
            tc_A->target, A_acc, A_jrk);

        try {
            RuckigProfileParams rp = {A_v_entry_scaled, capped_exit,
                A_max_vel, A_acc, A_jrk, tc_A->target,
                A_feed, A_vel_limit, tp->vLimit, capped_exit};
            computeAndStoreProfile(tc_A, rp);
        } catch (...) {
            // Leave A's profile as-is if Ruckig fails
        }
    }

    // FIXUP12 removed: computeChainExitCap and safe-endpoint chain extension
    // handle downstream constraints, making per-index fixups redundant.

    return 0;
}

/**
 * @brief Apply computed limiting velocities back to queue
 *
 * Writes optimized velocities from SmoothingData back to
 * TC_STRUCT elements using atomic operations.
 */
int applyLimitingVelocities_9D(TC_QUEUE_STRUCT *queue,
                               const SmoothingData &smoothing,
                               int optimization_depth)
{
    if (!queue) return -1;
    if (optimization_depth <= 0) return 0;

    // Walk forward through queue and update velocities
    for (int k = 0; k < optimization_depth && k < (int)smoothing.v_smooth.size(); ++k) {
        // Use tcqBack to access from end of queue
        TC_STRUCT *tc = tcqBack_user(queue, -k);
        if (!tc) break;

        // Update final velocity using atomic operations
        double v_new = smoothing.v_smooth[k];

        // For TANGENT segments (promoted by kink computation), the kink velocity
        // is an upper bound set by tc->finalvel. The backward pass may compute
        // a lower value (it should), but must not exceed the kink velocity.
        // Also ensure we don't overwrite a kink velocity with 0 from a stale
        // backward pass that didn't yet see the promotion.
        if (tc->term_cond == TC_TERM_COND_TANGENT && tc->finalvel > 0.0) {
            // Clamp smoothed velocity to kink limit, but don't reduce below
            // what kink computation set if backward pass returned 0 (stale)
            if (v_new < 1e-6) {
                v_new = tc->finalvel;  // Backward pass was stale, use kink vel
            } else {
                v_new = fmin(v_new, tc->finalvel);
            }
        }

        // Write to shared_9d structure atomically
        atomicStoreDouble(&tc->shared_9d.final_vel, v_new);
        atomicStoreDouble(&tc->shared_9d.final_vel_limit, v_new);

        // Update planning state atomically
        // BUT preserve TC_PLAN_UNTOUCHED - that means "needs profile recomputation"
        TCPlanningState current_state = (TCPlanningState)__atomic_load_n(
            (int*)&tc->shared_9d.optimization_state, __ATOMIC_ACQUIRE);
        if (current_state != TC_PLAN_UNTOUCHED) {
            atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_SMOOTHED);
        }

        // CRITICAL FIX: Update finalvel for segment handoff
        // RT uses tc->finalvel in tpCompleteSegment() when term_cond == TC_TERM_COND_TANGENT
        // to set nexttc->currentvel. If this doesn't match shared_9d.final_vel,
        // we get velocity discontinuities causing 10x acceleration spikes.
        // This field is used for:
        // 1. Segment-to-segment handoff (tp.c:3974)
        // 2. Abort detection (tp.c:3326)
        // 3. Blend velocity calculations
        tc->finalvel = v_new;

        // Also update target_vel for backward compatibility
        // (RT layer may still read this for non-9D planners)
        tc->target_vel = v_new;
    }

    return 0;
}

//============================================================================
// PUBLIC API (C-COMPATIBLE)
//============================================================================

/**
 * @brief Main optimization entry point
 *
 * Runs lookahead optimization on queued motion segments.
 * This is called from userspace context (NOT RT).
 */
extern "C" int tpOptimizePlannedMotions_9D(TP_STRUCT * const tp, int optimization_depth)
{
    if (!tp) return -1;

    // Branch/Merge architecture for feed override handling
    // Handles: merge taken branches, discard missed branches, create new branches
    manageBranches(tp);

    // Get queue and validate it's properly initialized
    TC_QUEUE_STRUCT *queue = &tp->queue;

    // Safety check: ensure queue is valid and initialized
    if (queue->size <= 0) return 0;  // Queue size not set

    int queue_len = tcqLen_user(queue);

    // Need at least 1 segment to optimize
    if (queue_len < 1) return 0;

    // Limit optimization depth to queue length
    int depth = optimization_depth;
    if (depth > queue_len) {
        depth = queue_len;
    }
    if (depth > MAX_LOOKAHEAD_DEPTH) {
        depth = MAX_LOOKAHEAD_DEPTH;
    }

    // Step 1: Compute limiting velocities (backward pass)
    int result = computeLimitingVelocities_9D(queue, depth, g_smoothing_data);
    if (result != 0) return result;

    // Step 2: Apply velocities back to queue (peak smoothing skipped — Ruckig handles jerk-aware smoothing)
    result = applyLimitingVelocities_9D(queue, g_smoothing_data, depth);
    if (result != 0) return result;

    // Step 3: Compute Ruckig trajectories for segments
    // Forward pass: use optimized velocities as entry/exit constraints
    result = computeRuckigProfiles_9D(tp, queue, depth);
    if (result != 0) return result;

    return 0;
}

/**
 * @brief Finalize segment and enqueue with optimization
 *
 * Finalizes a trajectory segment and adds it to the queue,
 * then triggers optimization if queue depth sufficient.
 */
extern "C" int tpFinalizeAndEnqueue_9D(TP_STRUCT * const tp, TC_STRUCT * const tc)
{
    if (!tp || !tc) return -1;

    // TODO: Implement segment finalization
    // This is a stub - segments are finalized in tpAddLine_9D

    // The function should:
    // 1. Finalize segment parameters
    // 2. Add to queue using tcqPut()
    // 3. Trigger optimization if queue depth >= optimization_depth

    return 0;
}

/**
 * @brief Clear planning state (e.g., on abort/reset)
 */
extern "C" int tpClearPlanning_9D(TP_STRUCT * const tp)
{
    if (!tp) return -1;

    // Clear smoothing data
    g_smoothing_data = SmoothingData();

    // Reset all persistent feed override / recomputation state
    // Without this, state from the previous program leaks into the next one,
    // causing non-deterministic behavior between identical runs.
    g_last_feed_scale = 1.0;
    g_last_replan_time_ms = 0.0;
    g_committed_feed = -1.0;
    g_committed_rapid = -1.0;
    g_recompute_cursor = 0;
    g_recompute_feed_scale = 1.0;
    g_recompute_rapid_scale = 1.0;
    g_recompute_first_batch_done = false;
    g_next_feed_scale = -1.0;
    g_next_rapid_scale = -1.0;
    g_commit_segment = 1;
    g_segments_per_tick = 3.0;
    g_adaptive_horizon = AdaptiveHorizon();

    return 0;
}
