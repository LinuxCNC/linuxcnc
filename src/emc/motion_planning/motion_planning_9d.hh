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
    // Constructor initializes with dummy endpoint
    SmoothingData() :
#ifdef MOTION_PLANNING_DEBUG
        motion_line{0},
        unique_id{0},
        planning_state{TC_PLAN_UNTOUCHED},
        limiting_id{0},
        smoothed{false},
        touched{false},
        v_opt{0.0},
        s{0.0},
#endif
        ignore{false},
        ds{0.0},
        v_smooth{0.0},
        t{0.0},
        t_orig{0.0}
        {}

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
#ifdef MOTION_PLANNING_DEBUG
        motion_line.reserve(capacity);
        unique_id.reserve(capacity);
        planning_state.reserve(capacity);
        limiting_id.reserve(capacity);
        smoothed.reserve(capacity);
        touched.reserve(capacity);
        v_opt.reserve(capacity);
        s.reserve(capacity);
#endif
    }

#ifdef MOTION_PLANNING_DEBUG
    std::vector<int> motion_line;           // Source line number for debugging
    std::vector<int> unique_id;             // Segment unique ID
    std::vector<TCPlanningState> planning_state; // Optimization state
    std::vector<int> limiting_id;           // ID of limiting segment
    std::vector<bool> smoothed;             // Segment has been smoothed
    std::vector<bool> touched;              // Segment has been touched by optimizer
    SmoothingVector v_opt;                  // Optimal velocity profile
    SmoothingVector s;                      // Position (for debugging)
#endif

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

#endif // __cplusplus

#endif // MOTION_PLANNING_9D_HH
