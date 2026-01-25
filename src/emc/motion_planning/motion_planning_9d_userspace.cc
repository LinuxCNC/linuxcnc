/**
 * @file motion_planning_9d_userspace.cc
 * @brief Userspace motion planning functions for 9D dual-layer architecture
 *
 * This file contains the userspace planning functions (tpAddLine_9D, tpAddCircle_9D)
 * that write segments to the shared memory queue and trigger optimization.
 *
 * Phase 0.1: Minimal implementation - no blending, just queue segments and optimize.
 */

#include "motion_planning_9d.hh"
#include <string.h>  // memset
#include <time.h>    // clock_gettime for timing
#include "rtapi.h"   // rtapi_print_msg for error reporting

// C headers need extern "C" when included from C++
extern "C" {
#include "motion.h"     // emcmot_status_t, emcmot_config_t (needed before tp.h)
#include "tp.h"
#include "tc.h"
#include "tc_types.h"
#include "emcpos.h"
#include "posemath.h"
}

// External queue functions (from motion_planning_9d.cc - will be made public)
extern "C" {
    extern int tcqPut_user(TC_QUEUE_STRUCT * const tcq, TC_STRUCT const * const tc);
    extern int tcqLen_user(TC_QUEUE_STRUCT const * const tcq);

    // Access to global pointers (defined in usrmotintf.cc, initialized by milltask)
    extern struct emcmot_struct_t *emcmotStruct;
    extern struct emcmot_internal_t *emcmotInternal;
}

/**
 * @brief Initialize TC_STRUCT with basic parameters
 *
 * IMPORTANT: Must set indexer_jnum = -1 (no indexer) otherwise
 * tpActivateSegment() will wait forever for rotary unlock.
 */
static int tcInit_9D(TC_STRUCT * const tc,
                     int motion_type,
                     int canon_motion_type,
                     double cycle_time)
{
    if (!tc) return -1;

    memset(tc, 0, sizeof(TC_STRUCT));

    tc->motion_type = motion_type;
    tc->canon_motion_type = canon_motion_type;
    tc->cycle_time = cycle_time;
    tc->active_depth = 1;

    /* Critical: -1 means no rotary indexer. If 0, RT thinks joint 0 is
     * a locking indexer and waits forever for unlock (TP_ERR_WAITING). */
    tc->indexer_jnum = -1;

    /* Also set id to -1 like original tcInit() does */
    tc->id = -1;

    return 0;
}

/**
 * @brief Set motion parameters (velocity, acceleration)
 */
static int tcSetupMotion_9D(TC_STRUCT * const tc,
                            double vel,
                            double ini_maxvel,
                            double acc)
{
    if (!tc) return -1;

    tc->reqvel = vel;
    tc->maxvel = ini_maxvel;  // Use maxvel, not maxvel_geom (LinuxCNC difference)
    tc->maxaccel = acc;

    return 0;
}

/**
 * @brief Compute line geometry and length (using LinuxCNC's pmLine9Init)
 */
static int computeLineLengthAndTarget_9D(TC_STRUCT * tc,
                                         EmcPose const *start,
                                         EmcPose const *end)
{
    if (!tc || !start || !end) return -1;

    // Initialize PmLine9 structure (LinuxCNC version takes EmcPose, not PmVector)
    pmLine9Init(&tc->coords.line, start, end);

    // Compute length (LinuxCNC uses pmLine9Target, not pmLine9Length)
    tc->target = tc->nominal_length = pmLine9Target(&tc->coords.line);

    return 0;
}

/**
 * @brief Add linear move to shared memory queue (userspace planning)
 *
 * Minimal Phase 0.1 implementation:
 * - Initialize TC_STRUCT
 * - Set geometry
 * - Write to queue
 * - Trigger optimization
 * - NO blending (that's Phase 1+)
 */
extern "C" int tpAddLine_9D(
    TP_STRUCT * const tp,
    EmcPose end_pose,
    int type,
    double vel,
    double ini_maxvel,
    double acc,
    struct state_tag_t const &tag)
{
    // CRITICAL: Safety check - shared memory must be initialized
    if (!tp) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - tp pointer is NULL\n");
        return -1;
    }

    // Phase 1: Comprehensive validation to detect uninitialized shared memory
    // Check magic number (set by tpInit())
    if (tp->magic != TP_MAGIC) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - magic number mismatch (got 0x%08x, expected 0x%08x)\n",
               tp->magic, TP_MAGIC);
        return -1;
    }

    // Check queue ready flag (set after queue allocation)
    if (!tp->queue_ready) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - queue_ready flag not set (value=%d)\n", tp->queue_ready);
        return -1;
    }

    TC_QUEUE_STRUCT *queue = &tp->queue;

    // Validate queue size (reasonable range check)
    // LinuxCNC allows queue sizes from 32 up to several thousand
    if (queue->size <= 0 || queue->size > 10000) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - invalid queue size (%d)\n", queue->size);
        return -1;
    }

    // Validate atomic indices are within valid range
    // This catches garbage values from uninitialized memory
    int current_start = __atomic_load_n(&queue->start_atomic, __ATOMIC_ACQUIRE);
    int current_end = __atomic_load_n(&queue->end_atomic, __ATOMIC_ACQUIRE);

    if (current_start < 0 || current_start >= queue->size) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - start index out of bounds (%d, size=%d)\n",
               current_start, queue->size);
        return -1;
    }

    if (current_end < 0 || current_end >= queue->size) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - end index out of bounds (%d, size=%d)\n",
               current_end, queue->size);
        return -1;
    }

    // CRITICAL FIX: Sync goalPos to currentPos when queue is empty.
    // This ensures the first segment of a new program/run computes from
    // the actual current machine position, not stale goalPos from a previous run.
    // (Following Tormach's tpResetAtModeChange pattern, but in userspace)
    int queue_len = (current_end - current_start + queue->size) % queue->size;
    if (queue_len == 0) {
        tp->goalPos = tp->currentPos;
    }

    // Initialize new TC_STRUCT for the line segment
    TC_STRUCT tc;
    tcInit_9D(&tc, TC_LINEAR, type, tp->cycleTime);

    // Set motion parameters
    tcSetupMotion_9D(&tc, vel, ini_maxvel, acc);

    // Set state tag for tracking
    tc.tag = tag;
    tc.id = tag.fields[0];  // Simple ID assignment

    // Compute line geometry (goalPos is EmcPose in LinuxCNC)
    int result = computeLineLengthAndTarget_9D(&tc, &tp->goalPos, &end_pose);
    if (result != 0) {
        return -1;
    }

    // Check for zero-length segment
    if (tc.target < 1e-6) {
        return -1;  // Zero-length segment
    }

    // PHASE 0: Force exact stop - NO blending until Phase 4
    // Blending requires blend geometry (Bezier arcs) which isn't implemented yet.
    // Using TC_TERM_COND_TANGENT without blend geometry causes velocity discontinuities
    // at segment boundaries, leading to 10x acceleration spikes.
    tc.term_cond = TC_TERM_COND_STOP;  // Force exact stop at end of segment
    tc.finalvel = 0.0;  // Come to complete stop (prevents velocity discontinuity)

    // Write segment to shared memory queue
    result = tcqPut_user(queue, &tc);
    if (result != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - tcqPut_user failed (queue full?)\n");
        return -1;  // Queue full or error
    }

    // Update goal position for next segment (use LinuxCNC's tcGetEndpoint)
    tcGetEndpoint(&tc, &tp->goalPos);

    // Trigger optimization (if queue has enough segments)
    // ENABLED for Phase 0.6+ - optimizer computes proper final velocities for blending
    #if 1  // Changed from 0 to 1 - optimizer now active
    int queue_len_opt = tcqLen_user(queue);  // Renamed to avoid shadowing warning
    if (queue_len_opt >= 2) {
        tpOptimizePlannedMotions_9D(tp, 8);  // optimization_depth = 8
    }
    #endif

    return 0;
}
