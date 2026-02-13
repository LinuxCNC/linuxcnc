/********************************************************************
* Description: motion_planning_9d.hh
*   Tormach 9D trajectory planner - Userspace planning layer
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
* ARCHITECTURE:
*   This is the USERSPACE planning layer that runs optimization
*   algorithms with lookahead. It uses C++ with dynamic allocation
*   (std::vector) which is FORBIDDEN in RT context.
*
*   Communication with RT layer is via atomic operations only.
*   RT layer (tp.c) only READS pre-computed velocities.
*
********************************************************************/
#ifndef MOTION_PLANNING_9D_HH
#define MOTION_PLANNING_9D_HH

#include "posemath.h"
#include "tc_types.h"
#include "tp_types.h"

#include <cstddef>
#include <vector>

// Type alias for velocity/distance vectors
using SmoothingVector = std::vector<double>;

/**
 * @struct SmoothingData
 * @brief Lookahead buffer for velocity optimization
 *
 * Caches velocity, distance, and time data for segments in the
 * lookahead window. Used by backward velocity pass and peak smoothing.
 *
 * This structure uses C++ std::vector for dynamic allocation.
 * IT IS ONLY USED IN USERSPACE - never accessed from RT context.
 */
struct SmoothingData {
    // Default constructor creates empty vectors.
    // computeLimitingVelocities_9D pushes the head dummy element.
    SmoothingData() = default;

    /**
     * Pre-allocate capacity to avoid reallocation during optimization
     * @param capacity Maximum expected lookahead depth
     */
    void reserve(size_t capacity) {
        ignore.reserve(capacity);
        ds.reserve(capacity);
        v_smooth.reserve(capacity);
        t.reserve(capacity);
        t_orig.reserve(capacity);
    }

    // Core optimization data (always present)
    std::vector<bool> ignore;               // Ignore rapid moves in smoothing
    SmoothingVector ds;                     // Distance samples
    SmoothingVector v_smooth;               // Smoothed velocity profile
    SmoothingVector t;                      // Time at each sample
    SmoothingVector t_orig;                 // Original time values
};


//============================================================================
// C-COMPATIBLE INTERFACE
// These functions are called from C code (tp.c) in the RT layer
//============================================================================

//============================================================================
// PREDICTIVE HANDOFF CONFIGURATION
//============================================================================

/**
 * @struct PredictiveHandoffConfig
 * @brief Configuration parameters for branch/merge feed override system
 *
 * These parameters control timing and buffer management for
 * real-time trajectory replanning (feed override, etc.).
 *
 * Branch/Merge Timing:
 *   handoff_horizon_ms: How far ahead to place the handoff point
 *   branch_window_ms: Size of the window RT has to take the branch
 *
 * Example: horizon=100ms, window=50ms
 *   - Branch handoff at elapsed_time + 100ms
 *   - RT must take it between [100ms, 150ms)
 *   - If RT reaches 150ms without taking, branch is discarded
 */
struct PredictiveHandoffConfig {
    // Branch/Merge timing (milliseconds)
    double handoff_horizon_ms;          // How far ahead to predict (must exceed worst-case latency)
    double branch_window_ms;            // Window size for RT to take branch
    double min_buffer_time_ms;          // Alarm if buffer drops below this
    double target_buffer_time_ms;       // Optimizer aims to maintain this
    double max_buffer_time_ms;          // Stop adding segments above this
    double feed_override_debounce_ms;   // Minimum time between branch computations (ignores rapid changes)

    // System parameters (from INI/motion config)
    double servo_cycle_time_sec;        // Servo thread period in seconds (from [EMCMOT] SERVO_PERIOD)
    double default_max_jerk;            // Fallback jerk limit (from [TRAJ] MAX_LINEAR_JERK)

    // Default constructor
    PredictiveHandoffConfig() :
        handoff_horizon_ms(100.0),
        branch_window_ms(50.0),
        min_buffer_time_ms(100.0),
        target_buffer_time_ms(200.0),
        max_buffer_time_ms(500.0),
        feed_override_debounce_ms(50.0),
        servo_cycle_time_sec(0.001),    // 1ms default (conservative)
        default_max_jerk(1e9)           // Match initraj.cc default
    {}
};

/**
 * @struct PredictedState
 * @brief Predicted kinematic state at a future time
 */
struct PredictedState {
    double position;        // Absolute position along segment
    double velocity;
    double acceleration;
    double jerk;
    bool valid;

    PredictedState() : position(0), velocity(0), acceleration(0), jerk(0), valid(false) {}
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Add linear move to shared memory queue (userspace planning)
 *
 * This is the userspace version of tpAddLine for the 9D dual-layer architecture.
 * It creates a TC_STRUCT, writes it to shared memory, and triggers optimization.
 *
 * @param tp Trajectory planner structure (from usrmotGetEmcmotInternal()->coord_tp)
 * @param end End position
 * @param type Motion type (EMC_MOTION_TYPE_FEED, etc.)
 * @param vel Requested velocity
 * @param ini_maxvel Maximum velocity
 * @param acc Acceleration
 * @param enables Feed/spindle scale enable bits (FS_ENABLED, SS_ENABLED, etc.)
 * @param tag State tag for tracking
 * @return 0 on success, error code otherwise
 */
int tpAddLine_9D(
    TP_STRUCT * const tp,
    EmcPose end,
    int type,
    double vel,
    double ini_maxvel,
    double acc,
    unsigned char enables,
    struct state_tag_t const &tag);

/**
 * @brief Add circular arc segment to planner queue (9D userspace version)
 *
 * Userspace function that initializes arc geometry, computes joint-space
 * segment via userspace kinematics (if enabled), and enqueues the segment.
 *
 * @param tp Trajectory planner structure (shared memory)
 * @param end End position of the arc
 * @param center Arc center point
 * @param normal Arc plane normal vector
 * @param turn Number of full turns (+ for CCW, - for CW looking along normal)
 * @param type Canon motion type
 * @param vel Requested velocity
 * @param ini_maxvel Maximum velocity from INI
 * @param acc Acceleration limit
 * @param enables Feed/spindle scale enable bits
 * @param tag State tag for tracking
 * @return 0 on success, error code otherwise
 */
int tpAddCircle_9D(
    TP_STRUCT * const tp,
    EmcPose end,
    PmCartesian center,
    PmCartesian normal,
    int turn,
    int type,
    double vel,
    double ini_maxvel,
    double acc,
    unsigned char enables,
    struct state_tag_t const &tag);

/**
 * @brief Main optimization entry point
 *
 * Runs lookahead optimization on queued motion segments.
 * Called from userspace context (NOT RT).
 *
 * @param tp Trajectory planner structure
 * @param optimization_depth Number of segments to look ahead
 * @return 0 on success, error code otherwise
 */
int tpOptimizePlannedMotions_9D(TP_STRUCT * const tp, int optimization_depth);

/**
 * @brief Finalize segment and enqueue with optimization
 *
 * Finalizes a trajectory segment and adds it to the queue,
 * then triggers optimization if queue depth sufficient.
 *
 * @param tp Trajectory planner structure
 * @param tc Trajectory component to finalize
 * @return 0 on success, error code otherwise
 */
int tpFinalizeAndEnqueue_9D(TP_STRUCT * const tp, TC_STRUCT * const tc);

/**
 * @brief Clear planning state (e.g., on abort/reset)
 *
 * @param tp Trajectory planner structure
 * @return 0 on success
 */
int tpClearPlanning_9D(TP_STRUCT * const tp);

//============================================================================
// PREDICTIVE HANDOFF FUNCTIONS (C-compatible)
//============================================================================

/**
 * @brief Set predictive handoff configuration parameters
 *
 * Called from initraj.cc after parsing INI file. All timing values are in milliseconds
 * except servo_cycle_time_sec which is in seconds.
 *
 * @param handoff_horizon_ms How far ahead to predict handoff point
 * @param branch_window_ms Window size for RT to take branch
 * @param min_buffer_time_ms Alarm if buffer drops below this
 * @param target_buffer_time_ms Optimizer aims to maintain this
 * @param max_buffer_time_ms Stop adding segments above this
 * @param feed_override_debounce_ms Minimum time between branch computations
 * @param servo_cycle_time_sec Servo thread period in seconds
 * @param default_max_jerk Fallback jerk limit (in user units, typically mm/s³)
 */
void setHandoffConfig(double handoff_horizon_ms,
                      double branch_window_ms,
                      double min_buffer_time_ms,
                      double target_buffer_time_ms,
                      double max_buffer_time_ms,
                      double feed_override_debounce_ms,
                      double servo_cycle_time_sec,
                      double default_max_jerk);

/**
 * @brief Calculate how much buffered motion time remains
 *
 * Sum of remaining time in active segment + all queued segments.
 *
 * @param tp Trajectory planner structure
 * @return Buffer time in milliseconds
 */
double calculateBufferTimeMs(TP_STRUCT *tp);

/**
 * @brief Get the configured default max jerk (from handoff config)
 * @return Jerk limit in user units/s³ (e.g. mm/s³)
 */
double getDefaultMaxJerk();

/**
 * @brief Manage branches for feed override using branch/merge architecture
 *
 * Called each iteration of the optimization loop. Handles:
 * 1. Merging taken branches (branch.taken=1 -> canonical)
 * 2. Discarding missed branches (elapsed > window_end)
 * 3. Creating new branches when feed scale changes
 *
 * @param tp Trajectory planner structure
 */
void manageBranches(TP_STRUCT *tp);

/**
 * @brief Request a stop branch for abort (called from task layer)
 *
 * Called by emcTrajAbort() before sending EMCMOT_ABORT to RT.
 * Computes a smooth stop branch using existing Ruckig infrastructure.
 *
 * @param tp Trajectory planner structure (shared memory)
 * @return 0 on success, -1 on error
 */
int tpRequestAbortBranch_9D(TP_STRUCT *tp);

/**
 * @brief Check for feed override changes (legacy wrapper)
 *
 * This function is kept for API compatibility with existing callers.
 * It simply forwards to manageBranches().
 *
 * @deprecated Use manageBranches() directly
 * @param tp Trajectory planner structure
 */
void checkFeedOverride(TP_STRUCT *tp);

/**
 * @brief Get minimum buffer time threshold (for critical alarms)
 * @return Minimum buffer time in milliseconds
 */
double getBufferMinTimeMs(void);

/**
 * @brief Get target buffer time threshold (for adaptive optimization)
 * @return Target buffer time in milliseconds
 */
double getBufferTargetTimeMs(void);

/**
 * @brief Initialize predictive handoff system for feed override handling
 *
 * Should be called after setHandoffConfig() has been used to set parameters.
 * If setHandoffConfig() was not called, defaults are used.
 *
 * @return 0 on success
 */
int initPredictiveHandoff(void);

#ifdef __cplusplus
}
#endif


//============================================================================
// C++ INTERNAL FUNCTIONS
// These are only called from within the planning layer (C++ code)
//============================================================================

#ifdef __cplusplus

/**
 * @brief Compute limiting velocities using backward propagation
 *
 * Walks backward through the lookahead window and computes the
 * maximum safe final velocity for each segment based on kinematics.
 *
 * This is the core "rising tide" optimization algorithm.
 *
 * @param queue Segment queue
 * @param optimization_depth Number of segments to optimize
 * @param smoothing Output smoothing data
 * @return 0 on success
 */
int computeLimitingVelocities_9D(TC_QUEUE_STRUCT *queue,
                                  int optimization_depth,
                                  SmoothingData &smoothing);

/**
 * @brief Apply peak smoothing to velocity profile
 *
 * Flattens local velocity peaks to improve motion smoothness.
 * Searches for peaks and troughs within time window and clamps
 * peaks to minimum of bracketing edges.
 *
 * @param smoothing Smoothing data with velocity profile
 * @param optimization_depth Number of segments
 * @param t_window Time window for peak detection
 * @return 0 on success
 */
int tpDoPeakSmoothing_9D(SmoothingData &smoothing,
                         int optimization_depth,
                         double t_window);

/**
 * @brief Compute optimal velocity at segment boundary
 *
 * Applies three constraints to determine safe velocity:
 * 1. Backward kinematic limit (can we reach final velocity?)
 * 2. Current segment velocity limit (geometry + feed override)
 * 3. Previous segment velocity limit (with kink handling)
 *
 * @param tc Current segment
 * @param prev_tc Previous segment
 * @param v_f_this Final velocity target for current segment
 * @param opt_step Optimization step index (for debugging)
 * @return Optimal velocity (minimum of all constraints)
 */
double tpComputeOptimalVelocity_9D(TC_STRUCT const * const tc,
                                   TC_STRUCT const * const prev_tc,
                                   double v_f_this,
                                   int opt_step);

/**
 * @brief Apply computed limiting velocities back to queue
 *
 * Writes optimized velocities from SmoothingData back to
 * TC_STRUCT elements in queue using atomic operations.
 *
 * @param queue Segment queue
 * @param smoothing Smoothing data with optimized velocities
 * @param optimization_depth Number of segments
 * @return 0 on success
 */
int applyLimitingVelocities_9D(TC_QUEUE_STRUCT *queue,
                               const SmoothingData &smoothing,
                               int optimization_depth);

//============================================================================
// PREDICTIVE HANDOFF INTERNAL FUNCTIONS (C++ only)
//============================================================================

/**
 * @brief Predict kinematic state at a future time
 *
 * Samples the current Ruckig profile at the specified elapsed time.
 *
 * @param tc Trajectory component
 * @param target_elapsed_time Time to predict state at
 * @return Predicted state (check .valid flag)
 */
PredictedState predictStateAtTime(TC_STRUCT *tc, double target_elapsed_time);

/**
 * @brief Commit a branch for RT to take
 *
 * Writes branch data and sets the branch.valid flag.
 * RT will take this branch when handoff_time is reached (if before window_end).
 *
 * @param shared Shared optimization data
 * @param main_profile Main trajectory (position control, ends at ~0 velocity)
 * @param brake_profile Optional brake trajectory (velocity control), NULL if not needed
 * @param brake_target_vel Expected final velocity of brake profile (0 if no brake)
 * @param brake_end_position Position where brake ends and main begins
 * @param handoff_time RT elapsed_time when branch handoff should occur
 * @param handoff_position Position at handoff (for merge reconciliation)
 * @param feed_scale Feed scale this branch was computed for
 * @param window_end_time Deadline - branch is stale if RT past this
 * @return true if committed, false if previous branch still pending (valid && !taken)
 */
bool commitBranch(shared_optimization_data_9d_t *shared,
                  const ruckig_profile_t *main_profile,
                  const ruckig_profile_t *brake_profile,
                  double brake_target_vel,
                  double brake_end_position,
                  double handoff_time,
                  double handoff_position,
                  double feed_scale,
                  double window_end_time,
                  double expected_exit_vel = 0.0);

/**
 * @brief Compute a branch trajectory for feed override change
 *
 * Computes new Ruckig trajectory starting from predicted state
 * at handoff horizon, with new feed scale applied. Uses achievable
 * feed cascade: if segment is too short for requested feed, applies
 * what's achievable and passes remainder to next segment.
 *
 * Achievable Feed Cascade:
 * - Calculates minimum exit velocity achievable without overshoot
 * - If requested feed isn't achievable, uses achievable portion
 * - Stores achieved_exit_vel for cascade to next segment
 * - Never overshoots - physically safe for CNC applications
 *
 * Phase 4 TODO (Blending Integration):
 * - With segment blending, exit velocity becomes non-zero
 * - achieved_exit_vel becomes entry velocity constraint for next segment
 * - This eliminates jerk ramp-up/ramp-down overhead at boundaries
 * - Short segment sequences will converge to target feed much faster
 * - See also: computeRuckigProfiles_9D for Phase 4 entry velocity handling
 *
 * @param tp Trajectory planner structure
 * @param tc Active trajectory component
 * @param new_feed_scale New feed scale (0.0 to 1.0+)
 * @return true if branch was successfully computed and committed
 */
bool computeBranch(TP_STRUCT *tp, TC_STRUCT *tc, double new_feed_scale);

/**
 * @brief Invalidate next N segments for cascade re-optimization
 *
 * Marks segments for re-optimization after feed override change.
 * Only invalidates next N segments to prevent starvation.
 *
 * Phase 4 TODO: When blending is implemented, this should also
 * propagate achieved_exit_vel from current segment to next segment's
 * entry velocity constraint.
 *
 * @param tp Trajectory planner structure
 * @param n Number of segments to invalidate
 */
void invalidateNextNSegments(TP_STRUCT *tp, int n, int start_index = 1);

//============================================================================
// PHASE 4 PLANNING: SEGMENT BLENDING FOR FEED OVERRIDE
//============================================================================
//
// Current State:
// - Feed override uses achievable feed cascade
// - Each segment stops at end (target_velocity = 0)
// - Cascade through short segments is inefficient due to jerk overhead
//
// Phase 4 Goals:
// - Enable non-zero exit velocities with segment blending
// - achieved_exit_vel from segment N becomes entry velocity for segment N+1
// - Eliminates repeated jerk ramp-up/ramp-down at segment boundaries
// - Short segment sequences converge to target feed in O(1) distance
//   instead of O(n) where n = number of segments
//
// Implementation Steps:
// 1. computeRuckigProfiles_9D: Read prev segment's achieved_exit_vel as
//    entry velocity constraint (instead of always starting from 0)
// 2. computeBranch: Set target_velocity = tc->finalvel * new_feed_scale
//    (non-zero) when blending is enabled
// 3. RT handoff: Ensure velocity continuity when taking branch
// 4. Segment transition: Pass exit velocity to next segment atomically
//
// Key Constraint:
// - Exit velocity must be consumable by next segment (path continuity)
// - If next segment is too short or has sharp angle, must reduce exit vel
// - This is already handled by existing lookahead optimization
//
//============================================================================

#endif // __cplusplus

#endif // MOTION_PLANNING_9D_HH
