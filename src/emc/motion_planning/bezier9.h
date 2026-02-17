#ifndef BEZIER9_H
#define BEZIER9_H

#include "posemath.h"
#include "emcpos.h"

#ifdef __cplusplus
extern "C" {
#endif

// Numerical tolerances for Bezier blend curves
#define BEZIER9_MIN_CONTROL_DIST 1e-12
#define BEZIER9_MIN_LENGTH 1e-12
#define BEZIER9_CURVATURE_EPSILON 1e-8
#define BEZIER9_POS_EPSILON 1e-12
#define BEZIER9_ARC_LENGTH_SAMPLES 1024
#define BEZIER9_CURVATURE_SAMPLES 30
#define BEZIER9_GOLDEN_RATIO 0.618033988749895

/**
 * Bezier9 - 9D quintic Bezier curve for trajectory blending
 *
 * Represents a smooth blend between two motion segments with G2 continuity
 * (matching position, tangent direction, AND curvature at entry/exit).
 * The curve is defined by 18 control points (6 per subspace) split across
 * xyz (translation), abc (rotation), and uvw (auxiliary) coordinate spaces.
 *
 * G2 continuity is achieved by placing P0,P1,P2 collinear along the
 * incoming tangent and P3,P4,P5 collinear along the outgoing tangent.
 * This gives B''(0) = B''(1) = 0, so curvature is zero at both endpoints
 * — matching the zero curvature of adjacent line segments and eliminating
 * the centripetal acceleration discontinuity that cubic blends suffer from.
 *
 * Arc-length parameterization is precomputed during initialization to enable
 * constant-velocity traversal. Curvature analysis is performed on the xyz
 * components only, as centripetal acceleration is physical (not rotational).
 */
typedef struct {
    // Control points for quintic Bezier in each 3D subspace
    PmCartesian P[6];        // xyz control points (translation)
    PmCartesian A[6];        // abc control points (rotation)
    PmCartesian U[6];        // uvw control points (auxiliary)

    // Cached geometric properties
    double total_length;     // Total arc length (9D magnitude)
    double max_kappa;        // Maximum curvature (xyz only)
    double min_radius;       // Minimum radius = 1/max_kappa
    double max_dkappa_ds;    // Maximum |dκ/ds| (curvature rate wrt arc length)

    // Arc-length parameterization lookup tables
    // Map arc-length s to Bezier parameter t via binary search
    double t_table[BEZIER9_ARC_LENGTH_SAMPLES + 1];
    double s_table[BEZIER9_ARC_LENGTH_SAMPLES + 1];
} Bezier9;

/**
 * bezier9Init - Initialize a 9D quintic Bezier blend curve
 *
 * Constructs 6 control points from boundary conditions (start/end positions
 * and tangent unit vectors) using a shape parameter alpha. Control points
 * are placed for G2 continuity (zero curvature at endpoints):
 *
 *   P[0] = start                          (on curve)
 *   P[1] = start + alpha * u_start        (tangent control)
 *   P[2] = start + 2*alpha * u_start      (curvature = 0)
 *   P[3] = end   - 2*alpha * u_end        (curvature = 0)
 *   P[4] = end   - alpha * u_end          (tangent control)
 *   P[5] = end                            (on curve)
 *
 * Precomputes arc-length parameterization table and maximum curvature.
 *
 * @param b Output Bezier9 structure to initialize
 * @param start Start pose (9D position)
 * @param end End pose (9D position)
 * @param u_start_xyz Unit tangent vector at start (xyz)
 * @param u_end_xyz Unit tangent vector at end (xyz)
 * @param u_start_abc Unit tangent vector at start (abc)
 * @param u_end_abc Unit tangent vector at end (abc)
 * @param u_start_uvw Unit tangent vector at start (uvw)
 * @param u_end_uvw Unit tangent vector at end (uvw)
 * @param alpha Shape parameter controlling control point placement (> 0)
 * @return TP_ERR_OK on success, error code otherwise
 */
int bezier9Init(Bezier9 * const b,
                EmcPose const * const start,
                EmcPose const * const end,
                PmCartesian const * const u_start_xyz,
                PmCartesian const * const u_end_xyz,
                PmCartesian const * const u_start_abc,
                PmCartesian const * const u_end_abc,
                PmCartesian const * const u_start_uvw,
                PmCartesian const * const u_end_uvw,
                double alpha);

/**
 * bezier9Point - Evaluate position on curve at arc-length parameter
 *
 * Uses precomputed arc-length parameterization to map progress (in length
 * units) to the Bezier parameter t, then evaluates B(t) in all 9 dimensions.
 *
 * @param b Input Bezier9 curve
 * @param progress Arc-length parameter (0 to total_length)
 * @param out Output pose at the given arc length
 * @return TP_ERR_OK on success, error code otherwise
 */
int bezier9Point(Bezier9 const * const b,
                 double progress,
                 EmcPose * const out);

/**
 * bezier9Tangent - Compute unit tangent vectors at arc-length parameter
 *
 * Evaluates the derivative B'(t) at the Bezier parameter corresponding to
 * the given arc length, then normalizes to unit length in each subspace.
 *
 * @param b Input Bezier9 curve
 * @param progress Arc-length parameter (0 to total_length)
 * @param xyz_out Output unit tangent vector (xyz), can be NULL
 * @param abc_out Output unit tangent vector (abc), can be NULL
 * @param uvw_out Output unit tangent vector (uvw), can be NULL
 * @return TP_ERR_OK on success, error code otherwise
 */
int bezier9Tangent(Bezier9 const * const b,
                   double progress,
                   PmCartesian * const xyz_out,
                   PmCartesian * const abc_out,
                   PmCartesian * const uvw_out);

/**
 * bezier9Length - Get total arc length of curve
 *
 * Returns the cached total arc length computed during initialization.
 *
 * @param b Input Bezier9 curve
 * @return Total arc length (always >= 0)
 */
double bezier9Length(Bezier9 const * const b);

/**
 * bezier9Curvature - Compute curvature at Bezier parameter t
 *
 * Computes curvature κ(t) = |B'' - (B'·B''/|B'|²)B'| / |B'|² using xyz
 * components only. Curvature is geometric property of translation path.
 *
 * @param b Input Bezier9 curve
 * @param t Bezier parameter (0 to 1)
 * @return Curvature value (always >= 0)
 */
double bezier9Curvature(Bezier9 const * const b, double t);

/**
 * bezier9MaxCurvature - Find maximum curvature over entire curve
 *
 * Samples curvature at N points (BEZIER9_CURVATURE_SAMPLES), identifies
 * the region with maximum curvature, then refines using golden section
 * search. Caches result in max_kappa and min_radius fields.
 *
 * @param b Bezier9 curve to analyze (modified: caches max_kappa, min_radius)
 * @return TP_ERR_OK on success, error code otherwise
 */
int bezier9MaxCurvature(Bezier9 * const b);

/**
 * bezier9AccLimit - Compute velocity limit based on centripetal acceleration
 *
 * Uses minimum radius (1/max_kappa) and normal acceleration budget to compute
 * maximum safe velocity: v_max = sqrt(a_normal * min_radius). Follows the
 * same pattern as arc9AccLimit for consistency with existing blend code.
 *
 * @param b Input Bezier9 curve
 * @param v_req Requested velocity (unused, for API compatibility)
 * @param a_max Maximum total acceleration budget
 * @param j_max Maximum jerk (for curvature-rate limiting), 0 to skip
 * @return Velocity limit (physical velocity units)
 */
double bezier9AccLimit(Bezier9 const * const b,
                       double v_req,
                       double a_max,
                       double j_max);

/**
 * bezier9Deviation - Compute maximum deviation from original corner
 *
 * Evaluates the Bezier curve at t=0.5 (midpoint) and measures xyz distance
 * to the intersection point. This provides an estimate of blend deviation
 * for G64 P tolerance checking.
 *
 * @param b Input Bezier9 curve
 * @param intersection_point Original corner intersection point (xyz only)
 * @return Deviation distance (always >= 0)
 */
double bezier9Deviation(Bezier9 const * const b,
                        PmCartesian const * const intersection_point);

#ifdef __cplusplus
}
#endif

#endif // BEZIER9_H
