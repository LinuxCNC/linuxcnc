/**
 * @file motion_planning_9d_userspace.cc
 * @brief Userspace motion planning functions for 9D dual-layer architecture
 *
 * This file contains the userspace planning functions (tpAddLine_9D, tpAddCircle_9D)
 * that write segments to the shared memory queue and trigger optimization.
 *
 * Includes blend creation: when adjacent segments can be blended, a Bezier9
 * blend segment is inserted between them for velocity continuity.
 */

#include "motion_planning_9d.hh"
#include "userspace_kinematics.hh"
#include <string.h>  // memset
#include <time.h>    // clock_gettime for timing
#include <cmath>     // acos, sin, sqrt, fmin
#include "rtapi.h"   // rtapi_print_msg for error reporting

// blend_sizing.h has extern "C" guards
#include "blend_sizing.h"

// C headers need extern "C" when included from C++
extern "C" {
#include "motion.h"     // emcmot_status_t, emcmot_config_t (needed before tp.h)
#include "tp.h"
#include "tc.h"
#include "tc_types.h"
#include "emcpos.h"
#include "emcpose.h"    // pmCartesianToEmcPose
#include "posemath.h"
#include "blendmath.h"  // pmCircleEffectiveMinRadius, findIntersectionAngle
#include "atomic_9d.h"  // atomicStoreDouble
#include "motion_types.h" // EMC_MOTION_TYPE_TRAVERSE
}

// External queue functions (from motion_planning_9d.cc - will be made public)
extern "C" {
    extern int tcqPut_user(TC_QUEUE_STRUCT * const tcq, TC_STRUCT const * const tc);
    extern int tcqLen_user(TC_QUEUE_STRUCT const * const tcq);

    // Access to global pointers (defined in usrmotintf.cc, initialized by milltask)
    extern struct emcmot_struct_t *emcmotStruct;
    extern struct emcmot_internal_t *emcmotInternal;
    extern emcmot_config_t *emcmotConfig;
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

    /* Default tangential accel ratio (memset zeroed it).
     * Lines use 1.0 (no centripetal); arcs compute proper ratio in tpAddCircle_9D. */
    tc->acc_ratio_tan = 1.0;

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
 * @brief Validate shared memory queue state
 *
 * Performs comprehensive validation to detect uninitialized shared memory:
 * - NULL pointer check
 * - Magic number verification
 * - queue_ready flag check
 * - Queue size range validation
 * - Atomic index bounds checks
 * - Syncs goalPos to currentPos when queue is empty
 *
 * @param tp Trajectory planner structure
 * @param out_queue_len Output: current queue length
 * @param out_current_end Output: current end index
 * @param func_name Function name for error messages (e.g., "tpAddLine_9D")
 * @return 0 on success, -1 on error (with rtapi_print_msg already called)
 */
static int validateQueueState_9D(TP_STRUCT *tp, int *out_queue_len, int *out_current_end, const char *func_name)
{
    // CRITICAL: Safety check - shared memory must be initialized
    if (!tp) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: FAIL - tp pointer is NULL\n", func_name);
        return -1;
    }

    // Comprehensive validation to detect uninitialized shared memory
    // Check magic number (set by tpInit())
    if (tp->magic != TP_MAGIC) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: FAIL - magic number mismatch (got 0x%08x, expected 0x%08x)\n",
               func_name, tp->magic, TP_MAGIC);
        return -1;
    }

    // Check queue ready flag (set after queue allocation)
    if (!tp->queue_ready) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: FAIL - queue_ready flag not set (value=%d)\n", func_name, tp->queue_ready);
        return -1;
    }

    TC_QUEUE_STRUCT *queue = &tp->queue;

    // Validate queue size (reasonable range check)
    // LinuxCNC allows queue sizes from 32 up to several thousand
    if (queue->size <= 0 || queue->size > 10000) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: FAIL - invalid queue size (%d)\n", func_name, queue->size);
        return -1;
    }

    // Validate atomic indices are within valid range
    // This catches garbage values from uninitialized memory
    int current_start = __atomic_load_n(&queue->start_atomic, __ATOMIC_ACQUIRE);
    int current_end = __atomic_load_n(&queue->end_atomic, __ATOMIC_ACQUIRE);

    if (current_start < 0 || current_start >= queue->size) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: FAIL - start index out of bounds (%d, size=%d)\n",
               func_name, current_start, queue->size);
        return -1;
    }

    if (current_end < 0 || current_end >= queue->size) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: FAIL - end index out of bounds (%d, size=%d)\n",
               func_name, current_end, queue->size);
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

    if (out_queue_len) {
        *out_queue_len = queue_len;
    }
    if (out_current_end) {
        *out_current_end = current_end;
    }

    return 0;
}

/**
 * @brief Trigger adaptive optimization based on buffer time
 *
 * Adjusts optimization depth based on current buffer level:
 * - Below min_buffer: optimize all queued segments (full depth)
 * - Below target_buffer: optimize 16 segments
 * - Above target_buffer: optimize 8 segments
 *
 * @param tp Trajectory planner structure
 */
static void triggerAdaptiveOptimization_9D(TP_STRUCT *tp)
{
    TC_QUEUE_STRUCT *queue = &tp->queue;
    int queue_len_opt = tcqLen_user(queue);

    if (queue_len_opt >= 1) {
        double buffer_ms = calculateBufferTimeMs(tp);
        double min_buffer = getBufferMinTimeMs();
        double target_buffer = getBufferTargetTimeMs();

        int opt_depth;
        if (buffer_ms < min_buffer) {
            opt_depth = queue_len_opt;
        } else if (buffer_ms < target_buffer) {
            opt_depth = 16;
        } else {
            opt_depth = 8;
        }

        if (opt_depth > queue_len_opt) {
            opt_depth = queue_len_opt;
        }

        tpOptimizePlannedMotions_9D(tp, opt_depth);
    }
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
 * @brief Compute 9D tangent rate vector at the start or end of a segment
 *
 * Returns a 9-element vector representing the rate of axis position change
 * per unit path velocity: axis_vel[a] = v_path * tangent9[a]
 *
 * For XYZ: unit tangent direction (magnitude 1 for xyz-dominant moves)
 * For ABC/UVW: scaled by (subspace_length / segment_target) since these
 * subspaces are interpolated proportionally to the dominant subspace.
 *
 * @param tc       Segment to compute tangent for
 * @param at_end   true for end tangent, false for start tangent
 * @param out      Output 9-element tangent rate vector
 * @return 0 on success, -1 on error
 */
static int tcGetTangentRate9D(TC_STRUCT const * const tc, int at_end, double out[9])
{
    for (int i = 0; i < 9; i++) out[i] = 0.0;

    if (tc->target < 1e-15) return -1;
    double inv_target = 1.0 / tc->target;

    PmCartesian xyz_tan;

    switch (tc->motion_type) {
        case TC_LINEAR: {
            // XYZ tangent: unit direction vector
            xyz_tan = tc->coords.line.xyz.uVec;
            // ABC/UVW: linear subspaces scaled by their proportion of path length
            double abc_scale = tc->coords.line.abc.tmag_zero ? 0.0
                             : tc->coords.line.abc.tmag * inv_target;
            double uvw_scale = tc->coords.line.uvw.tmag_zero ? 0.0
                             : tc->coords.line.uvw.tmag * inv_target;
            out[0] = xyz_tan.x; out[1] = xyz_tan.y; out[2] = xyz_tan.z;
            out[3] = tc->coords.line.abc.uVec.x * abc_scale;
            out[4] = tc->coords.line.abc.uVec.y * abc_scale;
            out[5] = tc->coords.line.abc.uVec.z * abc_scale;
            out[6] = tc->coords.line.uvw.uVec.x * uvw_scale;
            out[7] = tc->coords.line.uvw.uVec.y * uvw_scale;
            out[8] = tc->coords.line.uvw.uVec.z * uvw_scale;
            // For xyz-zero moves (abc or uvw dominant), re-normalize
            if (tc->coords.line.xyz.tmag_zero) {
                out[0] = out[1] = out[2] = 0.0;
                if (!tc->coords.line.uvw.tmag_zero) {
                    out[6] = tc->coords.line.uvw.uVec.x;
                    out[7] = tc->coords.line.uvw.uVec.y;
                    out[8] = tc->coords.line.uvw.uVec.z;
                    double abc_uvw_scale = tc->coords.line.abc.tmag_zero ? 0.0
                                         : tc->coords.line.abc.tmag / tc->coords.line.uvw.tmag;
                    out[3] = tc->coords.line.abc.uVec.x * abc_uvw_scale;
                    out[4] = tc->coords.line.abc.uVec.y * abc_uvw_scale;
                    out[5] = tc->coords.line.abc.uVec.z * abc_uvw_scale;
                } else {
                    // Pure ABC move
                    out[3] = tc->coords.line.abc.uVec.x;
                    out[4] = tc->coords.line.abc.uVec.y;
                    out[5] = tc->coords.line.abc.uVec.z;
                }
            }
            break;
        }
        case TC_CIRCULAR: {
            // XYZ tangent varies along the arc
            double angle = at_end ? tc->coords.circle.xyz.angle : 0.0;
            pmCircleTangentVector(&tc->coords.circle.xyz, angle, &xyz_tan);
            // ABC/UVW: linear subspaces, constant rate
            double abc_scale = tc->coords.circle.abc.tmag_zero ? 0.0
                             : tc->coords.circle.abc.tmag * inv_target;
            double uvw_scale = tc->coords.circle.uvw.tmag_zero ? 0.0
                             : tc->coords.circle.uvw.tmag * inv_target;
            out[0] = xyz_tan.x; out[1] = xyz_tan.y; out[2] = xyz_tan.z;
            out[3] = tc->coords.circle.abc.uVec.x * abc_scale;
            out[4] = tc->coords.circle.abc.uVec.y * abc_scale;
            out[5] = tc->coords.circle.abc.uVec.z * abc_scale;
            out[6] = tc->coords.circle.uvw.uVec.x * uvw_scale;
            out[7] = tc->coords.circle.uvw.uVec.y * uvw_scale;
            out[8] = tc->coords.circle.uvw.uVec.z * uvw_scale;
            break;
        }
        case TC_BEZIER: {
            // Bezier blend: get tangent at start or end
            PmCartesian xyz_tan = {0,0,0}, abc_tan = {0,0,0}, uvw_tan = {0,0,0};
            double progress = at_end ? tc->coords.bezier.total_length : 0.0;
            bezier9Tangent(&tc->coords.bezier, progress, &xyz_tan, &abc_tan, &uvw_tan);
            out[0] = xyz_tan.x; out[1] = xyz_tan.y; out[2] = xyz_tan.z;
            out[3] = abc_tan.x; out[4] = abc_tan.y; out[5] = abc_tan.z;
            out[6] = uvw_tan.x; out[7] = uvw_tan.y; out[8] = uvw_tan.z;
            break;
        }
        default:
            return -1;
    }
    return 0;
}

/**
 * @brief Compute centripetal acceleration vector per unit v² at segment endpoint
 *
 * Returns a 3-element XYZ vector such that: a_centripetal = v² * result
 * The magnitude is 1/R_effective and direction is toward the center of curvature.
 * For lines (zero curvature), returns zero vector.
 * ABC/UVW subspaces are linearly interpolated and have no centripetal component.
 *
 * Uses rTan·cos + rPerp·sin directly (not pmCirclePoint) to avoid
 * spiral and helix contamination of the centripetal direction.
 *
 * @param tc       Segment
 * @param at_end   true for end of segment, false for start
 * @param out      Output 3-element vector (XYZ centripetal accel per v²)
 * @return 0 on success, -1 if segment type not supported
 */
static int tcGetCentripetalAccelPerV2(TC_STRUCT const * const tc,
                                       int at_end, double out[3])
{
    out[0] = out[1] = out[2] = 0.0;

    switch (tc->motion_type) {
        case TC_LINEAR:
            // Zero curvature — no centripetal acceleration
            return 0;

        case TC_CIRCULAR: {
            PmCircle const *circ = &tc->coords.circle.xyz;
            double R_eff = pmCircleEffectiveMinRadius(circ);
            if (R_eff < 1e-9) return -1;  // degenerate arc

            double angle = at_end ? circ->angle : 0.0;

            // Compute in-plane radial vector at the endpoint
            // radial = rTan*cos(angle) + rPerp*sin(angle)  (points outward)
            PmCartesian par, perp, centripetal;
            pmCartScalMult(&circ->rTan, cos(angle), &par);
            pmCartScalMult(&circ->rPerp, sin(angle), &perp);
            pmCartCartAdd(&par, &perp, &centripetal);
            // Centripetal direction is toward center (negate radial)
            pmCartNegEq(&centripetal);
            pmCartUnitEq(&centripetal);

            double inv_R = 1.0 / R_eff;
            out[0] = centripetal.x * inv_R;
            out[1] = centripetal.y * inv_R;
            out[2] = centripetal.z * inv_R;
            return 0;
        }

        case TC_SPHERICAL: {
            SphericalArc const *arc = &tc->coords.arc.xyz;
            double R = arc->radius;
            if (R < 1e-9) return -1;

            // Use cached start/end points directly (no need for arcPoint)
            PmCartesian const *point = at_end ? &arc->end : &arc->start;

            // Centripetal: toward center
            PmCartesian centripetal;
            pmCartCartSub(&arc->center, point, &centripetal);
            pmCartUnitEq(&centripetal);

            double inv_R = 1.0 / R;
            out[0] = centripetal.x * inv_R;
            out[1] = centripetal.y * inv_R;
            out[2] = centripetal.z * inv_R;
            return 0;
        }

        case TC_BEZIER: {
            // Bezier blend: use precomputed max curvature
            double R = tc->coords.bezier.min_radius;
            if (R < 1e-9) return -1;

            // Get centripetal direction at endpoint from tangent cross second derivative
            // Simplified: use tangent direction rotated 90° (approximate for small blends)
            PmCartesian xyz_tan;
            double progress = at_end ? tc->coords.bezier.total_length : 0.0;
            bezier9Tangent(&tc->coords.bezier, progress, &xyz_tan, NULL, NULL);

            // Centripetal is perpendicular to tangent, magnitude 1/R
            // For blend velocity limiting, the magnitude matters more than direction
            double inv_R = 1.0 / R;
            out[0] = inv_R;  // Conservative: use max curvature on primary axis
            out[1] = 0.0;
            out[2] = 0.0;
            return 0;
        }

        default:
            // TC_DWELL, TC_RIGIDTAP: not supported
            return -1;
    }
}

/**
 * @brief Get last element in the userspace queue
 *
 * Returns a pointer to the most recently enqueued TC_STRUCT,
 * or NULL if the queue is empty.
 */
static TC_STRUCT * tcqLast_user(TC_QUEUE_STRUCT * const tcq)
{
    if (!tcq) return NULL;

    int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
    int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);
    int len = (current_end - current_start + tcq->size) % tcq->size;
    if (len == 0) return NULL;

    int idx = (current_end - 1 + tcq->size) % tcq->size;
    return &(tcq->queue[idx]);
}

/**
 * @brief Attempt to create a blend between adjacent segments
 *
 * Called from tpAddLine_9D and tpAddCircle_9D after the current segment
 * is fully built but BEFORE it is enqueued. If blending succeeds:
 * - prev_tc is trimmed from its end
 * - A Bezier9 blend segment is enqueued
 * - tc is trimmed from its start
 * - prev_tc and blend get TC_TERM_COND_TANGENT with planned blend velocity
 *
 * If blending fails, returns 0 so caller can fall back to kink velocity.
 *
 * @param tp Trajectory planner
 * @param prev_tc Previous segment (already in queue, modified in place)
 * @param tc Current segment (NOT yet enqueued, modified in place)
 * @return 1 on blend created, 0 on no blend (fallback to kink), -1 on error
 */
static int tpSetupBlend9D(TP_STRUCT *tp, TC_STRUCT *prev_tc, TC_STRUCT *tc)
{
    if (!tp || !prev_tc || !tc) return 0;

    // Guard: skip RIGIDTAP and DWELL — they cannot participate in blends
    if (prev_tc->motion_type == TC_RIGIDTAP || prev_tc->motion_type == TC_DWELL ||
        tc->motion_type == TC_RIGIDTAP || tc->motion_type == TC_DWELL) {
        return 0;
    }

    // Guard: skip if prev_tc is already a blend segment
    if (prev_tc->motion_type == TC_BEZIER) {
        return 0;
    }

    // Guard: don't blend between rapid (G0) and feed (G1) moves.
    // Velocity mismatch creates unreachable junction velocities.
    if ((prev_tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE) ^
        (tc->canon_motion_type == EMC_MOTION_TYPE_TRAVERSE)) {
        return 0;
    }

    // Guard: skip if either segment is too short for a meaningful blend
    // (e.g. near-zero-length move like "g1 x0 y0" when already at origin)
    if (prev_tc->target < 0.1 || tc->target < 0.1) {
        return 0;
    }

    // G61.1 (CANON_EXACT_STOP → TC_TERM_COND_STOP):
    // Strictest mode — always decel to zero, no blending.
    if (tp->termCond == TC_TERM_COND_STOP) {
        return 0;
    }

    // G61 (EXACT): use kink velocity, no blend geometry
    if (tp->termCond == TC_TERM_COND_EXACT) {
        return 0;
    }

    // Only G64 (PARABOLIC) mode creates blend segments
    // Compute blend tolerance from G64 P value
    double tolerance;
    int res = tcFindBlendTolerance9(prev_tc, tc, &tolerance);
    if (res != TP_ERR_OK || tolerance <= 0.0) {
        return 0;
    }

    // Build per-axis velocity and acceleration bounds
    // Use minimum of both segments' limits
    AxisBounds9 vel_bounds, acc_bounds;
    memset(&vel_bounds, 0, sizeof(vel_bounds));
    memset(&acc_bounds, 0, sizeof(acc_bounds));

    double v_min = fmin(prev_tc->maxvel, tc->maxvel);
    double a_min = fmin(prev_tc->maxaccel, tc->maxaccel);
    vel_bounds.xyz.x = vel_bounds.xyz.y = vel_bounds.xyz.z = v_min;
    acc_bounds.xyz.x = acc_bounds.xyz.y = acc_bounds.xyz.z = a_min;

    // Maximum feed scale for velocity planning (accounts for possible feed override)
    double max_feed_scale = 1.0;
    if (emcmotConfig) {
        max_feed_scale = emcmotConfig->maxFeedScale;
    }

    // Compute blend normal direction and Jacobian projection.
    // The blend curvature is in the plane of (u_prev, u_next).
    // delta_u = u_next - u_prev is the curvature normal direction.
    // Joints with zero projection onto this direction are not loaded
    // by the blend's centripetal jerk and should not constrain velocity.
    double blend_proj[9] = {0};  // per-joint projection of curvature normal
    int blend_num_joints = 0;
    {
        using motion_planning::g_userspace_kins_planner;
        blend_num_joints = g_userspace_kins_planner.isEnabled()
                         ? g_userspace_kins_planner.getNumJoints() : 9;
        if (blend_num_joints > 9) blend_num_joints = 9;

        // Get tangent vectors at junction
        double u_prev[9], u_next[9];
        int have_tangents = (tcGetTangentRate9D(prev_tc, 1, u_prev) == 0 &&
                             tcGetTangentRate9D(tc, 0, u_next) == 0);

        if (have_tangents) {
            double delta_u[9];
            double delta_mag_sq = 0.0;
            for (int a = 0; a < 9; a++) {
                delta_u[a] = u_next[a] - u_prev[a];
                delta_mag_sq += delta_u[a] * delta_u[a];
            }

            if (delta_mag_sq >= 1e-12) {
                // Compute Jacobian at junction for non-identity kinematics
                double J[9][9] = {{0}};
                bool have_jacobian = false;
                if (g_userspace_kins_planner.isEnabled() &&
                    !g_userspace_kins_planner.isIdentity()) {
                    EmcPose junction;
                    memset(&junction, 0, sizeof(junction));
                    if (tc->motion_type == TC_LINEAR) {
                        junction.tran = tc->coords.line.xyz.start;
                        junction.a = tc->coords.line.abc.start.x;
                        junction.b = tc->coords.line.abc.start.y;
                        junction.c = tc->coords.line.abc.start.z;
                        junction.u = tc->coords.line.uvw.start.x;
                        junction.v = tc->coords.line.uvw.start.y;
                        junction.w = tc->coords.line.uvw.start.z;
                    } else if (tc->motion_type == TC_CIRCULAR) {
                        pmCirclePoint(&tc->coords.circle.xyz, 0.0, &junction.tran);
                        junction.a = tc->coords.circle.abc.start.x;
                        junction.b = tc->coords.circle.abc.start.y;
                        junction.c = tc->coords.circle.abc.start.z;
                        junction.u = tc->coords.circle.uvw.start.x;
                        junction.v = tc->coords.circle.uvw.start.y;
                        junction.w = tc->coords.circle.uvw.start.z;
                    }
                    have_jacobian = g_userspace_kins_planner.computeJacobian(junction, J);
                }

                // Project curvature normal through Jacobian
                for (int j = 0; j < blend_num_joints; j++) {
                    if (have_jacobian) {
                        double p = 0.0;
                        for (int a = 0; a < 9; a++)
                            p += fabs(J[j][a]) * fabs(delta_u[a]);
                        blend_proj[j] = p;
                    } else {
                        // Trivkins: J = identity → proj[j] = |delta_u[j]|
                        blend_proj[j] = (j < 9) ? fabs(delta_u[j]) : 0.0;
                    }
                }
            }
        }
    }

    // Compute direction-aware per-joint jerk limit for the alpha optimizer.
    // Only joints with non-zero projection onto the curvature normal direction
    // constrain blend velocity.  bezier9AccLimit applies BLEND_ACC_RATIO_NORMAL
    // internally, so we divide by that ratio to make them match:
    //   bezier9AccLimit: cbrt(RATIO * j_eff / dkds) → equals cbrt(jlim_eff / dkds)
    double j_max_for_blend = 0.0;
    {
        using motion_planning::g_userspace_kins_planner;
        double jlim_eff_min = 1e9;
        for (int j = 0; j < blend_num_joints; j++) {
            if (blend_proj[j] < 1e-15) continue;  // joint not loaded
            double jlim = g_userspace_kins_planner.isEnabled()
                        ? g_userspace_kins_planner.getJointJerkLimit(j) : 1e9;
            if (jlim > 0 && jlim < 1e9) {
                double jlim_eff = jlim / blend_proj[j];
                if (jlim_eff < jlim_eff_min)
                    jlim_eff_min = jlim_eff;
            }
        }
        if (jlim_eff_min < 1e9)
            j_max_for_blend = jlim_eff_min / BLEND_ACC_RATIO_NORMAL;
    }

    // Run blend optimizer
    BlendSolution9 solution;
    res = optimizeBlendSize9(prev_tc, tc, tolerance, max_feed_scale,
                             &vel_bounds, &acc_bounds, j_max_for_blend,
                             &solution);
    if (res != TP_ERR_OK || solution.status != BLEND9_OK) {
        return 0;  // Blend failed — fall back to kink velocity
    }

    // Trim previous segment from its end
    res = trimSegment9(prev_tc, solution.trim_prev, 0);
    if (res != TP_ERR_OK) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpSetupBlend9D: failed to trim prev_tc\n");
        return -1;
    }

    // Trim current segment from its start
    res = trimSegment9(tc, solution.trim_tc, 1);
    if (res != TP_ERR_OK) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpSetupBlend9D: failed to trim tc\n");
        return -1;
    }

    // Create blend segment
    TC_STRUCT blend_tc;
    res = createBlendSegment9(prev_tc, tc, &solution, &blend_tc, tp->cycleTime);
    if (res != TP_ERR_OK) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpSetupBlend9D: failed to create blend segment\n");
        return -1;
    }

    // Junction velocity = v_plan from the blend optimizer.
    // The optimizer already ensures v_plan respects the centripetal acceleration
    // limit (v² / R_min ≤ a_normal) via bezier9AccLimit.  The quintic Bezier
    // provides G2 continuity: B''(0) = B''(1) = 0, so curvature is zero at both
    // endpoints.  This eliminates the centripetal acceleration discontinuity that
    // cubic (G1-only) blends suffered from — no jerk spike at segment boundaries.
    double v_plan = solution.params.v_plan;

    // Per-joint curvature-rate jerk limit (hard cap, like kink_vel).
    // Centripetal jerk = v³ · dκ/ds · proj[j] must stay within per-joint jerk budgets.
    // Uses Jacobian-projected blend normal to only constrain loaded joints.
    double v_jerk_cap = 1e9;
    {
        using motion_planning::g_userspace_kins_planner;
        double dkds = solution.bezier.max_dkappa_ds;
        if (dkds > BEZIER9_CURVATURE_EPSILON) {
            for (int j = 0; j < blend_num_joints; j++) {
                if (blend_proj[j] < 1e-15) continue;  // joint not loaded
                double jlim = g_userspace_kins_planner.isEnabled()
                            ? g_userspace_kins_planner.getJointJerkLimit(j) : 1e9;
                if (jlim > 0 && jlim < 1e9) {
                    double v_j = cbrt(jlim / (blend_proj[j] * dkds));
                    if (v_j < v_jerk_cap)
                        v_jerk_cap = v_j;
                }
            }
            if (v_jerk_cap < v_plan) {
                v_plan = v_jerk_cap;
                blend_tc.maxvel = v_plan;
            }
            // Hard cap on blend exit (like kink_vel, ignores feed override).
            // Take the min of the per-joint Jacobian cap (v_jerk_cap) and
            // the INI-based aggregate cap (set by createBlendSegment9).
            if (blend_tc.kink_vel <= 0 || v_jerk_cap < blend_tc.kink_vel)
                blend_tc.kink_vel = v_jerk_cap;
            // Hard cap on prev_tc exit → blend entry
            if (prev_tc->kink_vel <= 0 || v_jerk_cap < prev_tc->kink_vel)
                prev_tc->kink_vel = v_jerk_cap;
        }
    }

    double v_entry = v_plan;
    double v_exit  = v_plan;

    // Set up velocity continuity:
    // prev_tc → blend at v_entry,  blend → tc at v_exit
    prev_tc->term_cond = TC_TERM_COND_TANGENT;
    prev_tc->finalvel = v_entry;
    atomicStoreDouble(&prev_tc->shared_9d.final_vel, v_entry);
    atomicStoreDouble(&prev_tc->shared_9d.final_vel_limit, v_entry);

    // Mark prev_tc for Ruckig recomputation (was profiled with finalvel=0)
    __atomic_store_n((int*)&prev_tc->shared_9d.optimization_state,
                     TC_PLAN_UNTOUCHED, __ATOMIC_RELEASE);

    // Blend segment: TANGENT exit to tc at v_exit
    blend_tc.term_cond = TC_TERM_COND_TANGENT;
    blend_tc.finalvel = v_exit;
    atomicStoreDouble(&blend_tc.shared_9d.final_vel, v_exit);
    atomicStoreDouble(&blend_tc.shared_9d.final_vel_limit, v_exit);

    // Set jerk limit on blend from per-joint tangential jerk budget.
    // The Pythagorean split BLEND_ACC_RATIO_TANGENTIAL (0.5) allocates
    // the tangential portion of each joint's jerk budget to Ruckig's
    // velocity transitions through the blend curve.
    {
        using motion_planning::g_userspace_kins_planner;
        double tangent_proj[9];
        tangent_proj[0] = fmax(fabs(solution.boundary.u_start_xyz.x),
                               fabs(solution.boundary.u_end_xyz.x));
        tangent_proj[1] = fmax(fabs(solution.boundary.u_start_xyz.y),
                               fabs(solution.boundary.u_end_xyz.y));
        tangent_proj[2] = fmax(fabs(solution.boundary.u_start_xyz.z),
                               fabs(solution.boundary.u_end_xyz.z));
        tangent_proj[3] = fmax(fabs(solution.boundary.u_start_abc.x),
                               fabs(solution.boundary.u_end_abc.x));
        tangent_proj[4] = fmax(fabs(solution.boundary.u_start_abc.y),
                               fabs(solution.boundary.u_end_abc.y));
        tangent_proj[5] = fmax(fabs(solution.boundary.u_start_abc.z),
                               fabs(solution.boundary.u_end_abc.z));
        tangent_proj[6] = fmax(fabs(solution.boundary.u_start_uvw.x),
                               fabs(solution.boundary.u_end_uvw.x));
        tangent_proj[7] = fmax(fabs(solution.boundary.u_start_uvw.y),
                               fabs(solution.boundary.u_end_uvw.y));
        tangent_proj[8] = fmax(fabs(solution.boundary.u_start_uvw.z),
                               fabs(solution.boundary.u_end_uvw.z));

        double tan_fraction = BLEND_ACC_RATIO_TANGENTIAL;
        double j_tan = 1e9;
        int nj = g_userspace_kins_planner.isEnabled()
               ? g_userspace_kins_planner.getNumJoints() : 0;
        if (nj > 9) nj = 9;
        for (int j = 0; j < nj; j++) {
            if (tangent_proj[j] < 1e-6) continue;
            double jlim = g_userspace_kins_planner.getJointJerkLimit(j);
            if (jlim > 0 && jlim < 1e9) {
                double j_path = tan_fraction * jlim / tangent_proj[j];
                if (j_path < j_tan)
                    j_tan = j_path;
            }
        }
        blend_tc.maxjerk = (j_tan < 1e8) ? j_tan
                                          : fmin(prev_tc->maxjerk, tc->maxjerk);
    }

    // Enqueue blend segment
    TC_QUEUE_STRUCT *queue = &tp->queue;
    res = tcqPut_user(queue, &blend_tc);
    if (res != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpSetupBlend9D: failed to enqueue blend segment\n");
        return -1;
    }

    // tc will be enqueued by caller as usual (after this function returns)
    // Set tc's tolerance for the blend
    tc->tolerance = tp->tolerance;

    return 1;  // Blend created successfully
}

/**
 * @brief Compute kink velocity at junction between prev_tc and tc
 *
 * For G61 (Exact Path) mode: compute the maximum safe velocity at the
 * junction such that per-joint jerk and acceleration limits are respected.
 *
 * At the junction, the velocity direction changes instantaneously (over one
 * servo period dt). For each joint j, the direction change delta_u projected
 * through the Jacobian gives the velocity impulse per unit path speed:
 *
 *   joint_acc_j = v * |J[j] . delta_u| / dt     (must be <= acc_limit[j])
 *   joint_jerk_j = v * |J[j] . delta_u| / dt^2  (must be <= jerk_limit[j])
 *
 * For trivkins (J = identity), this simplifies to per-axis limiting.
 * The kink_vel is the minimum over all joints of both constraints.
 *
 * The result is an absolute (unscaled) velocity. Feed override is applied
 * downstream at Ruckig profile computation time.
 *
 * @param tp        Trajectory planner state
 * @param queue     Segment queue
 * @param tc        New segment being added (local, not yet enqueued)
 * @param queue_len Current number of elements in queue
 * @param current_end Current end index of queue (tc will be placed here)
 */
static void tpComputeKinkVelocity_9D(TP_STRUCT *tp, TC_QUEUE_STRUCT *queue,
                                      TC_STRUCT *tc, int queue_len, int current_end)
{
    // Only compute for EXACT mode (G61)
    if (tp->termCond != TC_TERM_COND_EXACT) return;

    // Need at least 1 element in queue (the previous segment)
    if (queue_len < 1) return;

    // Get previous segment from queue (last enqueued element)
    int prev_idx = (current_end - 1 + queue->size) % queue->size;
    TC_STRUCT *prev_tc = &queue->queue[prev_idx];

    // Skip if either segment can't participate in kink computation
    if (prev_tc->motion_type == TC_RIGIDTAP || prev_tc->motion_type == TC_DWELL) return;
    if (tc->motion_type == TC_RIGIDTAP || tc->motion_type == TC_DWELL) return;

    // Compute 9D tangent rate vectors at the junction
    double u_prev[9], u_next[9];
    if (tcGetTangentRate9D(prev_tc, 1, u_prev) != 0) return;
    if (tcGetTangentRate9D(tc, 0, u_next) != 0) return;

    // Compute 9D direction-change vector
    double delta_u[9];
    double delta_mag_sq = 0.0;
    for (int a = 0; a < 9; a++) {
        delta_u[a] = u_next[a] - u_prev[a];
        delta_mag_sq += delta_u[a] * delta_u[a];
    }

    double vel_cap = fmin(prev_tc->maxvel, tc->maxvel);
    double dt = tp->cycleTime;
    // Physical limit starts unconstrained — only jerk/accel physics determine it.
    // vel_cap (programmed feed) is applied separately for the backward pass.
    double kink_vel = 1e9;

    using motion_planning::g_userspace_kins_planner;
    int num_joints = g_userspace_kins_planner.isEnabled()
                   ? g_userspace_kins_planner.getNumJoints() : 9;
    if (num_joints > 9) num_joints = 9;

    // Compute Jacobian at junction for non-identity kinematics
    // Used by both Part A (direction-change) and Part B (centripetal)
    double J[9][9] = {{0}};
    bool have_jacobian = false;
    if (g_userspace_kins_planner.isEnabled() &&
        !g_userspace_kins_planner.isIdentity()) {
        EmcPose junction;
        memset(&junction, 0, sizeof(junction));
        if (tc->motion_type == TC_LINEAR) {
            junction.tran = tc->coords.line.xyz.start;
            junction.a = tc->coords.line.abc.start.x;
            junction.b = tc->coords.line.abc.start.y;
            junction.c = tc->coords.line.abc.start.z;
            junction.u = tc->coords.line.uvw.start.x;
            junction.v = tc->coords.line.uvw.start.y;
            junction.w = tc->coords.line.uvw.start.z;
        } else if (tc->motion_type == TC_CIRCULAR) {
            pmCirclePoint(&tc->coords.circle.xyz, 0.0, &junction.tran);
            junction.a = tc->coords.circle.abc.start.x;
            junction.b = tc->coords.circle.abc.start.y;
            junction.c = tc->coords.circle.abc.start.z;
            junction.u = tc->coords.circle.uvw.start.x;
            junction.v = tc->coords.circle.uvw.start.y;
            junction.w = tc->coords.circle.uvw.start.z;
        }
        have_jacobian = g_userspace_kins_planner.computeJacobian(junction, J);
    }

    // --- Part A: Direction-change limiting ---
    // For non-trivkins: joint velocity impulse = v * sum_a(|J[j][a]| * |delta_u[a]|) / dt
    // For trivkins (J = identity): simplifies to v * |delta_u[j]| / dt
    double dt_sq = dt * dt;
    if (delta_mag_sq >= 1e-12) {
        for (int j = 0; j < num_joints; j++) {
            double projection;
            if (have_jacobian) {
                // Jacobian projection: sum_a(|J[j][a]| * |delta_u[a]|)
                projection = 0.0;
                for (int a = 0; a < 9; a++) {
                    projection += fabs(J[j][a]) * fabs(delta_u[a]);
                }
            } else {
                projection = fabs(delta_u[j]);  // trivkins: J = identity
            }
            if (projection < 1e-15) continue;

            double jerk_lim = g_userspace_kins_planner.isEnabled()
                            ? g_userspace_kins_planner.getJointJerkLimit(j) : 1e9;
            if (jerk_lim > 0 && jerk_lim < 1e9) {
                double v_jerk = jerk_lim * dt_sq / projection;
                if (v_jerk < kink_vel) kink_vel = v_jerk;
            }

            double acc_lim = g_userspace_kins_planner.isEnabled()
                           ? g_userspace_kins_planner.getJointAccLimit(j) : 1e9;
            if (acc_lim > 0 && acc_lim < 1e9) {
                double v_acc = acc_lim * dt / projection;
                if (v_acc < kink_vel) kink_vel = v_acc;
            }
        }
    }

    // --- Part B: Centripetal jerk limiting ---
    // Even when tangent-continuous (delta_u ~ 0), curvature can change at
    // the junction (line->arc, arc->line, arc->arc with different R or center).
    // Centripetal acceleration = v^2 * kappa * n_hat, so when kappa changes
    // instantaneously: j_centripetal = v^2 * |delta(kappa*n)| / dt
    // Constraint is quadratic in v: v <= sqrt(jerk_limit * dt / |delta_ac|)
    {
        double ac_prev[3], ac_next[3];
        int have_prev = tcGetCentripetalAccelPerV2(prev_tc, 1, ac_prev);
        int have_next = tcGetCentripetalAccelPerV2(tc, 0, ac_next);

        if (have_prev == 0 && have_next == 0) {
            // Compute per-joint centripetal projections via Jacobian
            // ac_prev/ac_next are 3-element XYZ vectors (world space)
            // Joint centripetal = sum_a(|J[j][a]| * |ac[a]|) for a=0..2
            for (int j = 0; j < num_joints; j++) {
                double delta_proj, max_proj;
                if (have_jacobian) {
                    double dp = 0.0, mp = 0.0;
                    for (int a = 0; a < 3; a++) {
                        double jabs = fabs(J[j][a]);
                        dp += jabs * fabs(ac_next[a] - ac_prev[a]);
                        mp += jabs * fmax(fabs(ac_prev[a]), fabs(ac_next[a]));
                    }
                    delta_proj = dp;
                    max_proj = mp;
                } else {
                    if (j >= 3) continue;  // trivkins: XYZ only
                    delta_proj = fabs(ac_next[j] - ac_prev[j]);
                    max_proj = fmax(fabs(ac_prev[j]), fabs(ac_next[j]));
                }

                // Curvature-change jerk: v^2 * delta_proj / dt <= jerk_limit
                if (delta_proj > 1e-15) {
                    double jerk_lim = g_userspace_kins_planner.isEnabled()
                                    ? g_userspace_kins_planner.getJointJerkLimit(j) : 1e9;
                    if (jerk_lim > 0 && jerk_lim < 1e9) {
                        double v_curv_jerk = sqrt(jerk_lim * dt / delta_proj);
                        if (v_curv_jerk < kink_vel) kink_vel = v_curv_jerk;
                    }
                }

                // Per-joint centripetal acceleration: v^2 * max_proj <= acc_limit
                if (max_proj > 1e-15) {
                    double acc_lim = g_userspace_kins_planner.isEnabled()
                                   ? g_userspace_kins_planner.getJointAccLimit(j) : 1e9;
                    if (acc_lim > 0 && acc_lim < 1e9) {
                        double v_curv_acc = sqrt(acc_lim / max_proj);
                        if (v_curv_acc < kink_vel) kink_vel = v_curv_acc;
                    }
                }
            }
        }
    }

    // --- Part C: Virtual arc curvature for line-line junctions ---
    // Consecutive small line segments approximate a smooth curve. Compute the
    // discrete curvature from three points (start of prev, junction, end of tc)
    // and apply per-joint jerk and centripetal acceleration limits.
    // This caps kink_vel at the physically safe velocity for the approximated
    // curve's curvature, preventing kink_ratio from exceeding geometric limits.
    // Ported from planner 0/1 (tp.c:2266-2337) with Jacobian extension.
    if (prev_tc->motion_type == TC_LINEAR && tc->motion_type == TC_LINEAR) {
        PmCartesian P0 = prev_tc->coords.line.xyz.start;
        PmCartesian P1 = prev_tc->coords.line.xyz.end;    // junction point
        PmCartesian P2 = tc->coords.line.xyz.end;

        PmCartesian A, B;
        pmCartCartSub(&P1, &P0, &A);
        pmCartCartSub(&P2, &P1, &B);

        double A_mag, B_mag;
        pmCartMag(&A, &A_mag);
        pmCartMag(&B, &B_mag);

        if (A_mag > 1e-12 && B_mag > 1e-12) {
            // kappa = 2*(t_out - t_in) / (|A| + |B|)
            // Equals the curvature of the circumscribed circle through P0, P1, P2.
            PmCartesian t_in, t_out, delta_t, curv_vec;
            pmCartScalMult(&A, 1.0 / A_mag, &t_in);
            pmCartScalMult(&B, 1.0 / B_mag, &t_out);
            pmCartCartSub(&t_out, &t_in, &delta_t);
            pmCartScalMult(&delta_t, 2.0 / (A_mag + B_mag), &curv_vec);

            double curv_xyz[3] = {curv_vec.x, curv_vec.y, curv_vec.z};

            for (int j = 0; j < num_joints; j++) {
                double curv_proj;
                if (have_jacobian) {
                    curv_proj = 0.0;
                    for (int a = 0; a < 3; a++)
                        curv_proj += fabs(J[j][a]) * fabs(curv_xyz[a]);
                } else {
                    if (j >= 3) continue;  // trivkins: XYZ curvature only
                    curv_proj = fabs(curv_xyz[j]);
                }
                if (curv_proj < 1e-15) continue;

                // Curvature-change jerk: v^2 * curv_proj / dt <= jerk_limit
                double jerk_lim = g_userspace_kins_planner.isEnabled()
                                ? g_userspace_kins_planner.getJointJerkLimit(j) : 1e9;
                if (jerk_lim > 0 && jerk_lim < 1e9) {
                    double v_curv_jerk = sqrt(jerk_lim * dt / curv_proj);
                    if (v_curv_jerk < kink_vel) kink_vel = v_curv_jerk;
                }

                // Centripetal acceleration: v^2 * curv_proj <= acc_limit
                double acc_lim = g_userspace_kins_planner.isEnabled()
                               ? g_userspace_kins_planner.getJointAccLimit(j) : 1e9;
                if (acc_lim > 0 && acc_lim < 1e9) {
                    double v_curv_acc = sqrt(acc_lim / curv_proj);
                    if (v_curv_acc < kink_vel) kink_vel = v_curv_acc;
                }
            }
        }
    }

    // Floor at a small positive value to avoid numerical issues
    if (kink_vel < 1e-6) kink_vel = 1e-6;

    // kink_vel is the physical limit (from jerk/accel constraints).
    // Store it in tc->kink_vel as a hard cap NOT scaled by feed override.
    // The backward pass velocity is capped to the programmed feed (vel_cap)
    // since the backward pass operates in unscaled space (feed_scale=1.0).
    tcSetKinkProperties(prev_tc, tc, kink_vel, 0.0);

    double backward_pass_vel = fmin(vel_cap, kink_vel);

    // Promote prev_tc to TANGENT to allow non-zero exit velocity
    prev_tc->term_cond = TC_TERM_COND_TANGENT;
    prev_tc->finalvel = backward_pass_vel;
    atomicStoreDouble(&prev_tc->shared_9d.final_vel, backward_pass_vel);
    atomicStoreDouble(&prev_tc->shared_9d.final_vel_limit, backward_pass_vel);

    // Mark prev_tc for Ruckig recomputation (was profiled with finalvel=0)
    __atomic_store_n((int*)&prev_tc->shared_9d.optimization_state,
                     TC_PLAN_UNTOUCHED, __ATOMIC_RELEASE);
}

/**
 * @brief Check if two LINE segments can be consolidated (merged).
 *
 * Two adjacent LINE segments are consolidatable when they have:
 * - Same motion type (both TC_LINEAR)
 * - Same canon motion type (don't merge G0 with G1)
 * - Compatible feed rate (same F word)
 * - Collinear direction in all active 9D subspaces (< 1° angle)
 *
 * @return 1 if segments can be merged, 0 otherwise
 */
static int canConsolidateLines(TC_STRUCT const *prev_tc, TC_STRUCT const *tc)
{
    if (prev_tc->motion_type != TC_LINEAR || tc->motion_type != TC_LINEAR)
        return 0;

    if (prev_tc->canon_motion_type != tc->canon_motion_type)
        return 0;

    if (fabs(prev_tc->reqvel - tc->reqvel) > 1e-6)
        return 0;

    // cos(1°) ≈ 0.99985 — matches BLEND9_MIN_THETA threshold
    double const cos_thresh = 0.99985;
    double dot;

    // XYZ subspace
    if (!prev_tc->coords.line.xyz.tmag_zero && !tc->coords.line.xyz.tmag_zero) {
        pmCartCartDot(&prev_tc->coords.line.xyz.uVec,
                      &tc->coords.line.xyz.uVec, &dot);
        if (dot < cos_thresh) return 0;
    } else if (prev_tc->coords.line.xyz.tmag_zero != tc->coords.line.xyz.tmag_zero) {
        return 0;
    }

    // ABC subspace
    if (!prev_tc->coords.line.abc.tmag_zero && !tc->coords.line.abc.tmag_zero) {
        pmCartCartDot(&prev_tc->coords.line.abc.uVec,
                      &tc->coords.line.abc.uVec, &dot);
        if (dot < cos_thresh) return 0;
    } else if (prev_tc->coords.line.abc.tmag_zero != tc->coords.line.abc.tmag_zero) {
        return 0;
    }

    // UVW subspace
    if (!prev_tc->coords.line.uvw.tmag_zero && !tc->coords.line.uvw.tmag_zero) {
        pmCartCartDot(&prev_tc->coords.line.uvw.uVec,
                      &tc->coords.line.uvw.uVec, &dot);
        if (dot < cos_thresh) return 0;
    } else if (prev_tc->coords.line.uvw.tmag_zero != tc->coords.line.uvw.tmag_zero) {
        return 0;
    }

    return 1;
}

/**
 * @brief Add linear move to shared memory queue (userspace planning)
 *
 * - Initialize TC_STRUCT
 * - Set geometry
 * - Consolidate with previous segment if collinear
 * - Write to queue
 * - Trigger optimization
 */
extern "C" int tpAddLine_9D(
    TP_STRUCT * const tp,
    EmcPose end_pose,
    int type,
    double vel,
    double ini_maxvel,
    double acc,
    unsigned char enables,
    struct state_tag_t const &tag)
{
    int queue_len, current_end;
    if (validateQueueState_9D(tp, &queue_len, &current_end, "tpAddLine_9D") != 0) {
        return -1;
    }

    // First segment of a new program: reset userspace planning state.
    // tpClear() runs in RT context where tpClearPlanning_9D is unavailable,
    // so we detect program-start here by an empty queue.
    if (queue_len == 0) {
        tpClearPlanning_9D(tp);
    }

    TC_QUEUE_STRUCT *queue = &tp->queue;

    // Initialize new TC_STRUCT for the line segment
    TC_STRUCT tc;
    tcInit_9D(&tc, TC_LINEAR, type, tp->cycleTime);

    // CRITICAL: Set enables for feed/spindle scaling (FS_ENABLED, SS_ENABLED, etc.)
    // Without this, feed override won't work!
    tc.enables = enables;

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

    // Zero-length move (e.g. G0Z0 when already at Z0): treat as no-op success.
    // Update goalPos to match command endpoint (handles FP rounding) and return
    // success so the task controller advances without falling through to the
    // RT path (which would call tpSetId/tpAddLine on the shared queue).
    if (tc.target < 1e-6) {
        tp->goalPos = end_pose;
        return 0;
    }

    // Collinear segment consolidation: if the new LINE goes the same direction
    // as the previous LINE, extend the previous segment instead of creating a
    // new segment boundary.  Eliminates split cycles, blends, and velocity
    // sawtooth in CAM-generated toolpaths with many tiny collinear segments.
    if (tp->termCond != TC_TERM_COND_STOP) {  // G61.1 must stop at every junction
        TC_STRUCT *prev_tc = tcqLast_user(queue);
        if (prev_tc && canConsolidateLines(prev_tc, &tc)) {
            // Extend prev_tc geometry: keep current start, use new end
            pmCartLineInit(&prev_tc->coords.line.xyz,
                           &prev_tc->coords.line.xyz.start,
                           &tc.coords.line.xyz.end);
            pmCartLineInit(&prev_tc->coords.line.abc,
                           &prev_tc->coords.line.abc.start,
                           &tc.coords.line.abc.end);
            pmCartLineInit(&prev_tc->coords.line.uvw,
                           &prev_tc->coords.line.uvw.start,
                           &tc.coords.line.uvw.end);

            // Update lengths
            prev_tc->target = pmLine9Target(&prev_tc->coords.line);
            prev_tc->nominal_length = prev_tc->target;

            // Recompute joint-space segment for extended geometry
            if (motion_planning::userspace_kins_is_enabled()) {
                EmcPose start_pose;
                pmCartesianToEmcPose(&prev_tc->coords.line.xyz.start,
                                     &prev_tc->coords.line.abc.start,
                                     &prev_tc->coords.line.uvw.start,
                                     &start_pose);
                if (motion_planning::userspace_kins_compute_joint_segment(
                        &start_pose, &end_pose, prev_tc) != 0) {
                    prev_tc->joint_space.valid = 0;
                }
            }

            // Mark for re-optimization (geometry changed)
            __atomic_store_n((int*)&prev_tc->shared_9d.optimization_state,
                             TC_PLAN_UNTOUCHED, __ATOMIC_RELEASE);

            tp->goalPos = end_pose;
            triggerAdaptiveOptimization_9D(tp);
            return 0;  // No new segment needed
        }
    }

    // Set termination condition from G-code mode (G61/G61.1/G64):
    // G61.1 (STOP): decelerate to zero at every junction
    // G61 (EXACT): exact path with kink velocity
    // G64 (PARABOLIC): continuous blending via Bezier blend segments
    tc.term_cond = tp->termCond;
    if (tc.term_cond == TC_TERM_COND_PARABOLIC) {
        // G64: will be promoted to TANGENT if blend succeeds, else STOP
        tc.term_cond = TC_TERM_COND_STOP;
    }
    tc.tolerance = tp->tolerance;
    tc.finalvel = 0.0;  // Default: decelerate to zero

    // Attempt blend with previous segment (G64 mode) or kink velocity (G61 mode).
    // tpSetupBlend9D handles G64: creates Bezier blend, trims both segments.
    // tpComputeKinkVelocity_9D handles G61: computes junction velocity limit.
    {
        TC_STRUCT *prev_tc = tcqLast_user(queue);
        if (prev_tc) {
            int blend_result = tpSetupBlend9D(tp, prev_tc, &tc);
            if (blend_result <= 0) {
                // No blend created — compute kink velocity as fallback
                tpComputeKinkVelocity_9D(tp, queue, &tc, queue_len, current_end);
            }
        }
    }

    // Compute joint-space segment if userspace kinematics enabled
    // This populates tc.joint_space with start/end joint positions and
    // velocity/acceleration limits derived from Jacobian analysis.
    if (motion_planning::userspace_kins_is_enabled()) {
        if (motion_planning::userspace_kins_compute_joint_segment(&tp->goalPos, &end_pose, &tc) != 0) {
            tc.joint_space.valid = 0;
        }
    } else {
        tc.joint_space.valid = 0;
    }

    // Write segment to shared memory queue
    result = tcqPut_user(queue, &tc);
    if (result != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddLine_9D: FAIL - tcqPut_user failed (queue full?)\n");
        return -1;  // Queue full or error
    }

    // Update goal position for next segment (use LinuxCNC's tcGetEndpoint)
    tcGetEndpoint(&tc, &tp->goalPos);

    // Trigger optimization for all queued segments
    // Even single segments need profile computation
    triggerAdaptiveOptimization_9D(tp);

    return 0;
}

/**
 * @brief Add circular arc move to shared memory queue (userspace planning)
 *
 * - Initialize TC_STRUCT for circular motion
 * - Set arc geometry using pmCircle9Init
 * - Write to queue
 * - Trigger optimization
 * - NO blending (requires blend geometry implementation)
 */
extern "C" int tpAddCircle_9D(
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
    struct state_tag_t const &tag)
{
    int queue_len, current_end;
    if (validateQueueState_9D(tp, &queue_len, &current_end, "tpAddCircle_9D") != 0) {
        return -1;
    }

    // First segment of a new program: reset userspace planning state.
    if (queue_len == 0) {
        tpClearPlanning_9D(tp);
    }

    TC_QUEUE_STRUCT *queue = &tp->queue;

    // Initialize new TC_STRUCT for the circular segment
    TC_STRUCT tc;
    tcInit_9D(&tc, TC_CIRCULAR, type, tp->cycleTime);

    // CRITICAL: Set enables for feed/spindle scaling
    tc.enables = enables;

    // Set motion parameters
    tcSetupMotion_9D(&tc, vel, ini_maxvel, acc);

    // Set state tag for tracking
    tc.tag = tag;
    tc.id = tag.fields[0];

    // Initialize circle geometry using pmCircle9Init
    int res_init = pmCircle9Init(&tc.coords.circle,
            &tp->goalPos,
            &end,
            &center,
            &normal,
            turn);

    if (res_init != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddCircle_9D: FAIL - pmCircle9Init failed (%d)\n", res_init);
        return -1;
    }

    // Compute target as arc length
    tc.target = tc.nominal_length = pmCircle9Target(&tc.coords.circle);

    // Zero-length arc: treat as no-op success (same logic as zero-length line).
    if (tc.target < 1e-6) {
        tp->goalPos = end;
        return 0;
    }

    // Centripetal jerk velocity limit: v³/R² ≤ jerk_limit → v ≤ cbrt(jerk * R²)
    // On a circular arc, the centripetal acceleration vector rotates at rate v/R,
    // producing world-space jerk of magnitude v³/R². Cap velocity so this
    // doesn't exceed the configured jerk limit.
    {
        double radius = pmCircleEffectiveMinRadius(&tc.coords.circle.xyz);
        if (radius > TP_POS_EPSILON) {
            double jerk_limit = getDefaultMaxJerk();
            double v_max_cj = cbrt(jerk_limit * radius * radius);
            if (v_max_cj < tc.maxvel) {
                tc.maxvel = v_max_cj;
            }
            if (tc.reqvel > tc.maxvel) {
                tc.reqvel = tc.maxvel;
            }

            // Compute acc_ratio_tan: fraction of accel budget available for tangential use.
            // Centripetal accel = v²/R; tangential budget = sqrt(a_max² - a_centripetal²)
            double a_max = tc.maxaccel;
            double a_n = tc.maxvel * tc.maxvel / radius;
            if (a_n < a_max) {
                tc.acc_ratio_tan = sqrt(1.0 - (a_n * a_n) / (a_max * a_max));
            } else {
                // Centripetal alone saturates accel budget — cap velocity further
                tc.maxvel = sqrt(a_max * radius);
                if (tc.reqvel > tc.maxvel) tc.reqvel = tc.maxvel;
                tc.acc_ratio_tan = 0.0;
            }

        }
    }

    // Set termination condition from G-code mode (same as tpAddLine_9D)
    tc.term_cond = tp->termCond;
    if (tc.term_cond == TC_TERM_COND_PARABOLIC) {
        tc.term_cond = TC_TERM_COND_STOP;
    }
    tc.tolerance = tp->tolerance;
    tc.finalvel = 0.0;  // Default: decelerate to zero

    // Attempt blend with previous segment (G64) or kink velocity (G61)
    {
        TC_STRUCT *prev_tc = tcqLast_user(queue);
        if (prev_tc) {
            int blend_result = tpSetupBlend9D(tp, prev_tc, &tc);
            if (blend_result <= 0) {
                tpComputeKinkVelocity_9D(tp, queue, &tc, queue_len, current_end);
            }
        }
    }

    // Compute joint-space segment if userspace kinematics enabled
    if (motion_planning::userspace_kins_is_enabled()) {
        if (motion_planning::userspace_kins_compute_joint_segment(&tp->goalPos, &end, &tc) != 0) {
            tc.joint_space.valid = 0;
        }
    } else {
        tc.joint_space.valid = 0;
    }

    // Write segment to shared memory queue
    int result = tcqPut_user(queue, &tc);
    if (result != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "tpAddCircle_9D: FAIL - tcqPut_user failed (queue full?)\n");
        return -1;
    }

    // Update goal position for next segment
    tcGetEndpoint(&tc, &tp->goalPos);

    // Trigger optimization for all queued segments
    triggerAdaptiveOptimization_9D(tp);

    return 0;
}
