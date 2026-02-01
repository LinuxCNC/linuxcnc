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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

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

// Configuration defaults (can be overridden via HAL/INI)
static int g_optimization_depth = 8;
static double g_ramp_frequency = 10.0;
static int g_smoothing_passes = 2;

// Static smoothing data (pre-allocated to avoid reallocation)
static SmoothingData g_smoothing_data;

//============================================================================
// INTERNAL HELPER FUNCTIONS
//============================================================================

/**
 * @brief Get maximum feed scale for planning
 *
 * Returns the feed override scale factor, clamped to valid range.
 * This affects the target velocity for motion segments.
 *
 * @param tp Trajectory planner structure
 * @return Feed scale factor (0.0 to 1.0+)
 */
static inline double tpGetFeedScale(TP_STRUCT const * const tp) {
    (void)tp;  // Unused for now - will be used when feed override is implemented
    // TODO: Read from emcmotStatus or tp structure
    // For now, return 1.0 (no override)
    return 1.0;
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

//============================================================================
// CORE OPTIMIZATION FUNCTIONS
//============================================================================

/**
 * @brief Compute optimal velocity at segment boundary
 *
 * Implements the three-constraint algorithm from Tormach:
 * 1. Backward kinematic limit: can we reach v_f_this?
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

    // Get acceleration limit for this segment
    double acc_this = tcGetTangentialMaxAccel_9D_local(tc);
    if (acc_this <= 0.0) return 0.0;

    // Constraint 1: Backward kinematic limit
    // v_s^2 = v_f^2 + 2*a*d
    double vs_back = sqrt(v_f_this * v_f_this + 2.0 * acc_this * tc->target);

    // Constraint 2: Current segment velocity limit
    double max_feed_scale = tpGetFeedScale(nullptr); // TODO: pass TP_STRUCT
    double vf_limit_this = tcGetPlanMaxTargetVel(tc, max_feed_scale);

    // Constraint 3: Previous segment velocity limit (with kink)
    double v_max_prev = tcGetPlanMaxTargetVel(prev_tc, max_feed_scale);
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

#ifdef MOTION_PLANNING_DEBUG
    smoothing.v_opt.push_back(0.0);
    smoothing.unique_id.push_back(-1);
    smoothing.planning_state.push_back(TC_PLAN_UNTOUCHED);
    smoothing.limiting_id.push_back(-1);
    smoothing.smoothed.push_back(false);
    smoothing.touched.push_back(false);
    smoothing.s.push_back(0.0);
#endif

    // Walk backward through queue
    for (int k = 0; k < optimization_depth; ++k) {
        // Access segment at position -k from end using tcqBack
        // tcqBack_user(queue, 0) is the last item, tcqBack_user(queue, -1) is second-to-last, etc.
        TC_STRUCT const *tc = tcqBack_user(queue, -k);
        TC_STRUCT *prev1_tc = tcqBack_user(queue, -(k + 1));

        if (!tc || !prev1_tc) {
            break; // Reached end of queue
        }

        // Check for termination conditions
        // TODO: Check blend mode, dwell, spindle sync, etc.
        // For now, simplified version

        double v_f_prev = 0.0;
        double ds = tc->target;

        // Get final velocity limit for this segment
        double v_f_this = tcGetFinalVelLimit(tc);

        // Compute optimal velocity for previous segment
        v_f_prev = tpComputeOptimalVelocity_9D(tc, prev1_tc, v_f_this, k);

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

#ifdef MOTION_PLANNING_DEBUG
        smoothing.s.push_back(ds + smoothing.s.back());
        smoothing.v_opt.push_back(v_f_prev);
        smoothing.unique_id.push_back(prev1_tc->id);
        smoothing.planning_state.push_back(TC_PLAN_OPTIMIZED);
        smoothing.limiting_id.push_back(k);
        smoothing.smoothed.push_back(false);
        smoothing.touched.push_back(false);
#endif
    }

    // Add tail dummy point for peak finding algorithm
    smoothing.v_smooth.push_back(0.0);
    smoothing.ignore.push_back(true);
    smoothing.t.push_back(smoothing.t.back() + 10.0); // Far in the future
    smoothing.ds.push_back(0.0);

#ifdef MOTION_PLANNING_DEBUG
    smoothing.s.push_back(smoothing.s.back());
    smoothing.v_opt.push_back(0.0);
    smoothing.unique_id.push_back(-1);
    smoothing.planning_state.push_back(TC_PLAN_UNTOUCHED);
    smoothing.limiting_id.push_back(-1);
    smoothing.smoothed.push_back(false);
    smoothing.touched.push_back(false);
#endif

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

#ifdef MOTION_PLANNING_DEBUG
                smoothing.touched[j] = true;
#endif
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

        // Write to shared_9d structure atomically
        atomicStoreDouble(&tc->shared_9d.final_vel, v_new);
        atomicStoreDouble(&tc->shared_9d.final_vel_limit, v_new);

        // Update planning state atomically
        atomicStoreInt((int*)&tc->shared_9d.optimization_state, TC_PLAN_SMOOTHED);

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

#ifdef MOTION_PLANNING_DEBUG
        // Update planning state in debug vectors
        if (k < (int)smoothing.planning_state.size()) {
            atomicStoreInt((int*)&tc->shared_9d.optimization_state,
                          smoothing.planning_state[k]);
        }
#endif
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

    // Get queue and validate it's properly initialized
    TC_QUEUE_STRUCT *queue = &tp->queue;

    // Safety check: ensure queue is valid and initialized
    if (queue->size <= 0) return 0;  // Queue size not set

    int queue_len = tcqLen_user(queue);

    // Check if queue has enough segments for optimization
    if (queue_len < 2) return 0; // Nothing to optimize

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

    // Step 2: Apply peak smoothing (multiple passes)
    double t_window = 1.0 / g_ramp_frequency;
    for (int pass = 0; pass < g_smoothing_passes; ++pass) {
        result = tpDoPeakSmoothing_9D(g_smoothing_data, depth, t_window);
        if (result != 0) return result;
    }

    // Step 3: Apply velocities back to queue
    result = applyLimitingVelocities_9D(queue, g_smoothing_data, depth);
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
    // For Phase 0.1, this is a stub

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

    return 0;
}
