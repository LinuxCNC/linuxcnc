/********************************************************************
 * Description: blend_sizing.cc
 *   Blend sizing and infrastructure for 9D Bezier blend curves.
 *
 *   Ported from Tormach PathPilot's biarc blend sizing logic,
 *   adapted to produce a single Bezier9 blend curve instead of
 *   two biarc segments.
 *
 * Author: aitekcnc
 * License: GPL Version 2
 * System: Linux
 ********************************************************************/

#include "blend_sizing.h"

extern "C" {
#include "tp_types.h"
#include "tp_debug.h"
#include "tc.h"
#include "blendmath.h"
#include "emcpose.h"
#include "rtapi_math.h"
}

#include <math.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Static helpers: per-axis planar bounds (Tormach port)
 * ---------------------------------------------------------------- */

/**
 * Compute the maximum scalar value (velocity or acceleration) achievable
 * in a 2D blend plane without exceeding per-axis bounds.
 *
 * Given orthonormal basis vectors (u, v) spanning the blend plane in 9D,
 * the worst-case per-axis loading for any direction in the plane is
 * sqrt(u_i^2 + v_i^2) on axis i. The maximum scalar is constrained by
 * the axis with the tightest bound relative to its loading.
 *
 * Port of Tormach's findMaxValueOnPlane + VecVecHarmonicAddSq.
 */
static double findMaxValueOnPlane9(
        PmCartesian const *u_xyz, PmCartesian const *u_abc, PmCartesian const *u_uvw,
        PmCartesian const *v_xyz, PmCartesian const *v_abc, PmCartesian const *v_uvw,
        AxisBounds9 const *bounds)
{
    double m2 = TP_BIG_NUM * TP_BIG_NUM;

    /* For each axis i: if loading s_i = u_i^2+v_i^2 > 0 and bound b_i > 0,
     * then max_value^2 <= b_i^2 / s_i.  Track the minimum. */
#define CHECK_AXIS(u_comp, v_comp, b_comp) do { \
    double s = (u_comp)*(u_comp) + (v_comp)*(v_comp); \
    double b = (b_comp); \
    if (s > 0.0 && b > 0.0) { \
        double ratio = (b*b) / s; \
        if (ratio < m2) m2 = ratio; \
    } \
} while(0)

    CHECK_AXIS(u_xyz->x, v_xyz->x, bounds->xyz.x);
    CHECK_AXIS(u_xyz->y, v_xyz->y, bounds->xyz.y);
    CHECK_AXIS(u_xyz->z, v_xyz->z, bounds->xyz.z);
    CHECK_AXIS(u_abc->x, v_abc->x, bounds->abc.x);
    CHECK_AXIS(u_abc->y, v_abc->y, bounds->abc.y);
    CHECK_AXIS(u_abc->z, v_abc->z, bounds->abc.z);
    CHECK_AXIS(u_uvw->x, v_uvw->x, bounds->uvw.x);
    CHECK_AXIS(u_uvw->y, v_uvw->y, bounds->uvw.y);
    CHECK_AXIS(u_uvw->z, v_uvw->z, bounds->uvw.z);

#undef CHECK_AXIS

    return sqrt(m2);
}

/**
 * Compute the maximum scalar value along a single direction in 9D
 * without exceeding per-axis bounds.
 *
 * For collinear tangents (1D motion), the constraint is simply:
 * max_value = min over active axes of (bound_i / |dir_i|).
 *
 * Port of Tormach's 1/findAbsMaxScale.
 */
static double findMaxValueAlongDir9(
        PmCartesian const *dir_xyz, PmCartesian const *dir_abc, PmCartesian const *dir_uvw,
        AxisBounds9 const *bounds)
{
    double max_scale = 0.0;

#define CHECK_DIR(d_comp, b_comp) do { \
    double b = (b_comp); \
    if (b > 0.0) { \
        double s = fabs(d_comp) / b; \
        if (s > max_scale) max_scale = s; \
    } \
} while(0)

    CHECK_DIR(dir_xyz->x, bounds->xyz.x);
    CHECK_DIR(dir_xyz->y, bounds->xyz.y);
    CHECK_DIR(dir_xyz->z, bounds->xyz.z);
    CHECK_DIR(dir_abc->x, bounds->abc.x);
    CHECK_DIR(dir_abc->y, bounds->abc.y);
    CHECK_DIR(dir_abc->z, bounds->abc.z);
    CHECK_DIR(dir_uvw->x, bounds->uvw.x);
    CHECK_DIR(dir_uvw->y, bounds->uvw.y);
    CHECK_DIR(dir_uvw->z, bounds->uvw.z);

#undef CHECK_DIR

    return (max_scale > 0.0) ? (1.0 / max_scale) : TP_BIG_NUM;
}

/**
 * Gram-Schmidt orthonormalization of two 9D tangent vectors.
 *
 * Produces orthonormal basis (u, v) spanning the same plane as (u1, u2).
 * Returns 0 on success, -1 if vectors are collinear (only u is valid).
 *
 * Port of Tormach's VecVecOrthonormal.
 */
static int orthonormalize9(
        PmCartesian const *u1_xyz, PmCartesian const *u1_abc, PmCartesian const *u1_uvw,
        PmCartesian const *u2_xyz, PmCartesian const *u2_abc, PmCartesian const *u2_uvw,
        PmCartesian *u_xyz, PmCartesian *u_abc, PmCartesian *u_uvw,
        PmCartesian *v_xyz, PmCartesian *v_abc, PmCartesian *v_uvw)
{
    /* u = normalize(u1) in 9D */
    double mag1_sq = pmSq(u1_xyz->x) + pmSq(u1_xyz->y) + pmSq(u1_xyz->z)
                   + pmSq(u1_abc->x) + pmSq(u1_abc->y) + pmSq(u1_abc->z)
                   + pmSq(u1_uvw->x) + pmSq(u1_uvw->y) + pmSq(u1_uvw->z);
    double mag1 = sqrt(mag1_sq);
    if (mag1 < TP_POS_EPSILON) return -1;
    double inv1 = 1.0 / mag1;

    u_xyz->x = u1_xyz->x * inv1; u_xyz->y = u1_xyz->y * inv1; u_xyz->z = u1_xyz->z * inv1;
    u_abc->x = u1_abc->x * inv1; u_abc->y = u1_abc->y * inv1; u_abc->z = u1_abc->z * inv1;
    u_uvw->x = u1_uvw->x * inv1; u_uvw->y = u1_uvw->y * inv1; u_uvw->z = u1_uvw->z * inv1;

    /* proj = dot(u2, u) in 9D */
    double proj = u2_xyz->x * u_xyz->x + u2_xyz->y * u_xyz->y + u2_xyz->z * u_xyz->z
                + u2_abc->x * u_abc->x + u2_abc->y * u_abc->y + u2_abc->z * u_abc->z
                + u2_uvw->x * u_uvw->x + u2_uvw->y * u_uvw->y + u2_uvw->z * u_uvw->z;

    /* v = u2 - proj*u */
    v_xyz->x = u2_xyz->x - proj * u_xyz->x;
    v_xyz->y = u2_xyz->y - proj * u_xyz->y;
    v_xyz->z = u2_xyz->z - proj * u_xyz->z;
    v_abc->x = u2_abc->x - proj * u_abc->x;
    v_abc->y = u2_abc->y - proj * u_abc->y;
    v_abc->z = u2_abc->z - proj * u_abc->z;
    v_uvw->x = u2_uvw->x - proj * u_uvw->x;
    v_uvw->y = u2_uvw->y - proj * u_uvw->y;
    v_uvw->z = u2_uvw->z - proj * u_uvw->z;

    /* normalize v */
    double mag2_sq = pmSq(v_xyz->x) + pmSq(v_xyz->y) + pmSq(v_xyz->z)
                   + pmSq(v_abc->x) + pmSq(v_abc->y) + pmSq(v_abc->z)
                   + pmSq(v_uvw->x) + pmSq(v_uvw->y) + pmSq(v_uvw->z);
    double mag2 = sqrt(mag2_sq);
    if (mag2 < TP_POS_EPSILON) return -1;  /* collinear */
    double inv2 = 1.0 / mag2;

    v_xyz->x *= inv2; v_xyz->y *= inv2; v_xyz->z *= inv2;
    v_abc->x *= inv2; v_abc->y *= inv2; v_abc->z *= inv2;
    v_uvw->x *= inv2; v_uvw->y *= inv2; v_uvw->z *= inv2;

    return 0;
}

/**
 * Compute per-axis constrained velocity and acceleration limits
 * for a blend in the plane defined by two 9D tangent vectors.
 *
 * Port of Tormach's find_blend_vel_accel_planar_limits.
 */
static int findBlendPlanarLimits9(
        BlendBoundary9 const *boundary,
        AxisBounds9 const *vel_bounds,
        AxisBounds9 const *acc_bounds,
        double *v_max_planar,
        double *a_max_planar)
{
    PmCartesian u_xyz = {0,0,0}, u_abc = {0,0,0}, u_uvw = {0,0,0};
    PmCartesian v_xyz = {0,0,0}, v_abc = {0,0,0}, v_uvw = {0,0,0};

    int res = orthonormalize9(
            &boundary->u_start_xyz, &boundary->u_start_abc, &boundary->u_start_uvw,
            &boundary->u_end_xyz, &boundary->u_end_abc, &boundary->u_end_uvw,
            &u_xyz, &u_abc, &u_uvw,
            &v_xyz, &v_abc, &v_uvw);

    if (res == 0) {
        /* 2D blend plane: use planar envelope */
        *v_max_planar = findMaxValueOnPlane9(
                &u_xyz, &u_abc, &u_uvw,
                &v_xyz, &v_abc, &v_uvw,
                vel_bounds);
        *a_max_planar = findMaxValueOnPlane9(
                &u_xyz, &u_abc, &u_uvw,
                &v_xyz, &v_abc, &v_uvw,
                acc_bounds);
    } else {
        /* Collinear tangents: 1D motion, use directional constraint */
        *v_max_planar = findMaxValueAlongDir9(
                &u_xyz, &u_abc, &u_uvw, vel_bounds);
        *a_max_planar = findMaxValueAlongDir9(
                &u_xyz, &u_abc, &u_uvw, acc_bounds);
    }

    return TP_ERR_OK;
}

/* ----------------------------------------------------------------
 * Static helpers: helix-aware circle trimming
 * ---------------------------------------------------------------- */

/**
 * Trim a PmCircle to a new angle, properly handling helices and spirals.
 *
 * Port of Tormach's pmCircleCut. Unlike aitekcnc's pmCircleStretch,
 * this correctly scales rHelix proportionally on trim.
 *
 * @param circ      Circle to trim (modified in place)
 * @param new_angle New angular span
 * @param from_end  1 = keep end (trim from start), 0 = keep start (trim from end)
 */
static int pmCircleCut9(PmCircle * const circ, double new_angle, int from_end)
{
    if (!circ || new_angle < 1e-12) {
        return PM_ERR;
    }

    if (from_end) {
        /* Keep end point, create new start: more complex geometry update */
        double cut_angle = circ->angle - new_angle;

        PmCartesian new_start;
        pmCirclePoint(circ, cut_angle, &new_start);

        PmCartesian tmp;
        pmCartCartSub(&new_start, &circ->center, &tmp);

        /* Find new rPerp and rTan perpendicular to circle normal */
        pmCartCartCross(&circ->normal, &tmp, &circ->rPerp);
        pmCartCartCross(&circ->rPerp, &circ->normal, &circ->rTan);

        /* Center is displaced if there is a helical component */
        pmCartCartSubEq(&tmp, &circ->rTan);
        pmCartCartAddEq(&circ->center, &tmp);

        /* Update radius (may change due to spiral) */
        pmCartMag(&circ->rTan, &circ->radius);
    }

    /* Scale spiral and helix proportionally */
    double angle_ratio = new_angle / circ->angle;
    circ->spiral *= angle_ratio;
    pmCartScalMultEq(&circ->rHelix, angle_ratio);
    circ->angle = new_angle;

    return PM_OK;
}

/* ----------------------------------------------------------------
 * Static helpers: position and tangent at arbitrary progress
 * ---------------------------------------------------------------- */

/**
 * Compute tangent of a SphericalArc at arbitrary progress.
 *
 * Extends arcTangent (which only supports start/end) to any arc-length
 * progress by interpolating the radial vector via SLERP.
 */
static int arcTangentAtProgress(SphericalArc const * const arc,
                                double progress,
                                PmCartesian * const out)
{
    double net_progress = progress - arc->line_length;

    if (net_progress <= 0.0 && arc->line_length > 0.0) {
        /* Still on the lead-in line segment */
        *out = arc->uTan;
        return TP_ERR_OK;
    }

    /* Convert progress to angle */
    double angle_in = (arc->radius > TP_POS_EPSILON) ?
                      (net_progress / arc->radius) : 0.0;
    if (angle_in < 0.0) angle_in = 0.0;
    if (angle_in > arc->angle) angle_in = arc->angle;

    /* Interpolate radius vector via SLERP */
    if (arc->Sangle < TP_POS_EPSILON) {
        *out = arc->uTan;
        return TP_ERR_OK;
    }
    double s0 = sin(arc->angle - angle_in) / arc->Sangle;
    double s1 = sin(angle_in) / arc->Sangle;

    PmCartesian interp0, interp1, r_at_angle;
    pmCartScalMult(&arc->rStart, s0, &interp0);
    pmCartScalMult(&arc->rEnd, s1, &interp1);
    pmCartCartAdd(&interp0, &interp1, &r_at_angle);

    /* Tangent = binormal x r (tangential direction) */
    PmCartesian r_tan;
    pmCartCartCross(&arc->binormal, &r_at_angle, &r_tan);

    /* Spiral correction */
    if (fabs(arc->spiral) > TP_POS_EPSILON && arc->angle > TP_POS_EPSILON) {
        double dr = arc->spiral / arc->angle;
        PmCartesian d_perp;
        pmCartUnit(&r_at_angle, &d_perp);
        pmCartScalMultEq(&d_perp, dr);
        pmCartCartAddEq(&r_tan, &d_perp);
    }

    pmCartUnit(&r_tan, out);
    return TP_ERR_OK;
}

/**
 * Get 9D position at an arbitrary progress along a segment.
 *
 * Handles all blendable motion types: TC_LINEAR, TC_CIRCULAR,
 * TC_SPHERICAL, and TC_BEZIER.
 */
static int tcGetPosAtProgress(TC_STRUCT const * const tc,
                              double progress,
                              EmcPose * const pos)
{
    PmCartesian xyz = {0, 0, 0};
    PmCartesian abc = {0, 0, 0};
    PmCartesian uvw = {0, 0, 0};

    switch (tc->motion_type) {
        case TC_LINEAR:
            if (!tc->coords.line.xyz.tmag_zero && tc->target > TP_POS_EPSILON) {
                pmCartLinePoint(&tc->coords.line.xyz,
                        progress * tc->coords.line.xyz.tmag / tc->target,
                        &xyz);
            } else {
                xyz = tc->coords.line.xyz.start;
            }
            if (!tc->coords.line.abc.tmag_zero && tc->target > TP_POS_EPSILON) {
                pmCartLinePoint(&tc->coords.line.abc,
                        progress * tc->coords.line.abc.tmag / tc->target,
                        &abc);
            } else {
                abc = tc->coords.line.abc.start;
            }
            if (!tc->coords.line.uvw.tmag_zero && tc->target > TP_POS_EPSILON) {
                pmCartLinePoint(&tc->coords.line.uvw,
                        progress * tc->coords.line.uvw.tmag / tc->target,
                        &uvw);
            } else {
                uvw = tc->coords.line.uvw.start;
            }
            break;

        case TC_CIRCULAR: {
            double angle = 0.0;
            pmCircleAngleFromProgress(&tc->coords.circle.xyz,
                    &tc->coords.circle.fit,
                    progress, &angle);
            pmCirclePoint(&tc->coords.circle.xyz, angle, &xyz);

            if (!tc->coords.circle.abc.tmag_zero && tc->target > TP_POS_EPSILON) {
                pmCartLinePoint(&tc->coords.circle.abc,
                        progress * tc->coords.circle.abc.tmag / tc->target,
                        &abc);
            } else {
                abc = tc->coords.circle.abc.start;
            }
            if (!tc->coords.circle.uvw.tmag_zero && tc->target > TP_POS_EPSILON) {
                pmCartLinePoint(&tc->coords.circle.uvw,
                        progress * tc->coords.circle.uvw.tmag / tc->target,
                        &uvw);
            } else {
                uvw = tc->coords.circle.uvw.start;
            }
            break;
        }

        case TC_SPHERICAL:
            arcPoint(&tc->coords.arc.xyz, progress, &xyz);
            abc = tc->coords.arc.abc;
            uvw = tc->coords.arc.uvw;
            break;

        case TC_BEZIER:
            bezier9Point(&tc->coords.bezier, progress, pos);
            return TP_ERR_OK;

        default:
            return TP_ERR_FAIL;
    }

    pmCartesianToEmcPose(&xyz, &abc, &uvw, pos);
    return TP_ERR_OK;
}

/**
 * Get 9D tangent unit vectors at an arbitrary progress along a segment.
 *
 * Handles all blendable motion types. For TC_SPHERICAL, computes tangent
 * at arbitrary progress via SLERP interpolation of the radial vector.
 * Zero-magnitude subspaces produce zero output vectors.
 */
static int tcGetTangentAtProgress(TC_STRUCT const * const tc,
                                  double progress,
                                  PmCartesian * const xyz_out,
                                  PmCartesian * const abc_out,
                                  PmCartesian * const uvw_out)
{
    switch (tc->motion_type) {
        case TC_LINEAR:
            if (xyz_out) *xyz_out = tc->coords.line.xyz.uVec;
            if (abc_out) {
                if (tc->coords.line.abc.tmag_zero) {
                    abc_out->x = abc_out->y = abc_out->z = 0.0;
                } else {
                    *abc_out = tc->coords.line.abc.uVec;
                }
            }
            if (uvw_out) {
                if (tc->coords.line.uvw.tmag_zero) {
                    uvw_out->x = uvw_out->y = uvw_out->z = 0.0;
                } else {
                    *uvw_out = tc->coords.line.uvw.uVec;
                }
            }
            break;

        case TC_CIRCULAR: {
            if (xyz_out) {
                double angle = 0.0;
                pmCircleAngleFromProgress(&tc->coords.circle.xyz,
                        &tc->coords.circle.fit,
                        progress, &angle);
                pmCircleTangentVector(&tc->coords.circle.xyz, angle, xyz_out);
            }
            if (abc_out) {
                if (tc->coords.circle.abc.tmag_zero) {
                    abc_out->x = abc_out->y = abc_out->z = 0.0;
                } else {
                    *abc_out = tc->coords.circle.abc.uVec;
                }
            }
            if (uvw_out) {
                if (tc->coords.circle.uvw.tmag_zero) {
                    uvw_out->x = uvw_out->y = uvw_out->z = 0.0;
                } else {
                    *uvw_out = tc->coords.circle.uvw.uVec;
                }
            }
            break;
        }

        case TC_SPHERICAL:
            if (xyz_out) {
                arcTangentAtProgress(&tc->coords.arc.xyz, progress, xyz_out);
            }
            /* SphericalArc abc/uvw are static offsets, no tangent component */
            if (abc_out) abc_out->x = abc_out->y = abc_out->z = 0.0;
            if (uvw_out) uvw_out->x = uvw_out->y = uvw_out->z = 0.0;
            break;

        case TC_BEZIER:
            bezier9Tangent(&tc->coords.bezier, progress, xyz_out, abc_out, uvw_out);
            break;

        default:
            if (xyz_out) xyz_out->x = xyz_out->y = xyz_out->z = 0.0;
            if (abc_out) abc_out->x = abc_out->y = abc_out->z = 0.0;
            if (uvw_out) uvw_out->x = uvw_out->y = uvw_out->z = 0.0;
            return TP_ERR_FAIL;
    }

    return TP_ERR_OK;
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

/**
 * Compute blend tolerance from G64 P values.
 *
 * Uses tolerance_ratio = 0.5 (Tormach convention). If no explicit
 * tolerance is set (G64 without P word), falls back to half the
 * segment's nominal length. Final tolerance is capped at half of
 * each segment's nominal length to prevent over-consumption.
 */
int tcFindBlendTolerance9(TC_STRUCT const * const prev_tc,
                          TC_STRUCT const * const tc,
                          double * const tolerance)
{
    if (!prev_tc || !tc || !tolerance) {
        return TP_ERR_MISSING_INPUT;
    }

    double T1 = prev_tc->tolerance;
    double T2 = tc->tolerance;

    /* If no tolerance set (G64 without P word), use fraction of segment length */
    if (T1 <= 0.0) {
        T1 = prev_tc->nominal_length * BLEND9_TOLERANCE_RATIO;
    }
    if (T2 <= 0.0) {
        T2 = tc->nominal_length * BLEND9_TOLERANCE_RATIO;
    }

    /* Blend tolerance is minimum of both, further capped by half-segment */
    double T_min = fmin(T1, T2);
    T_min = fmin(T_min, prev_tc->nominal_length * BLEND9_TOLERANCE_RATIO);
    T_min = fmin(T_min, tc->nominal_length * BLEND9_TOLERANCE_RATIO);

    *tolerance = T_min;
    return TP_ERR_OK;
}

/**
 * Compute maximum blend region size (upper bound for optimizer).
 *
 * For each segment, the usable length is min(target, nominal_length/2).
 * For circular arcs, additionally limit to 60 degrees of arc.
 * Returns the minimum of both segments (symmetrical search).
 */
double findMaxBlendRegion9(TC_STRUCT const * const prev_tc,
                           TC_STRUCT const * const tc)
{
    if (!prev_tc || !tc) {
        return 0.0;
    }

    /* Per-segment usable length.
     * Each blend can claim up to BLEND9_MAX_SEGMENT_USE of the original
     * segment, so two blends on the same segment leave a guaranteed
     * remnant (≥10% at default 0.45). Unit-free, works in mm or inches. */
    double l_prev = fmin(prev_tc->target,
                         prev_tc->nominal_length * BLEND9_MAX_SEGMENT_USE);
    double l_tc   = fmin(tc->target,
                         tc->nominal_length * BLEND9_MAX_SEGMENT_USE);

    /* For circular arcs, limit to 60 degrees */
    if (prev_tc->motion_type == TC_CIRCULAR) {
        double radius = prev_tc->coords.circle.xyz.radius;
        if (radius > TP_POS_EPSILON) {
            double circ_limit = BLEND9_CIRC_MAX_ANGLE * radius;
            l_prev = fmin(l_prev, circ_limit);
        }
    }

    if (tc->motion_type == TC_CIRCULAR) {
        double radius = tc->coords.circle.xyz.radius;
        if (radius > TP_POS_EPSILON) {
            double circ_limit = BLEND9_CIRC_MAX_ANGLE * radius;
            l_tc = fmin(l_tc, circ_limit);
        }
    }

    /* Symmetrical search: return minimum */
    double Rb_max = fmin(l_prev, l_tc);

    tp_debug_print("findMaxBlendRegion9: l_prev=%g, l_tc=%g, Rb_max=%g\n",
                   l_prev, l_tc, Rb_max);

    return Rb_max;
}

/**
 * Sample 9D positions and tangent vectors at blend boundary points.
 *
 * Given Rb (blend region size), computes where the blend starts on prev_tc
 * (distance Rb from its end) and where it ends on tc (distance Rb from
 * its start). Samples 9D positions and tangent vectors at those points.
 *
 * The intersection point is computed as the start of tc (progress=0),
 * which is the shared corner point. This works for any motion type.
 */
int findBlendPointsAndTangents9(double Rb,
                                TC_STRUCT const * const prev_tc,
                                TC_STRUCT const * const tc,
                                BlendBoundary9 * const boundary)
{
    if (!prev_tc || !tc || !boundary) {
        return TP_ERR_MISSING_INPUT;
    }

    /* Compute progress values on each segment */
    boundary->s_prev = fmax(prev_tc->target - Rb, 0.0);
    boundary->s_tc = fmin(Rb, tc->target);

    /* Sample 9D position at blend start (near end of prev_tc) */
    int res = tcGetPosAtProgress(prev_tc, boundary->s_prev, &boundary->P_start);
    if (res != TP_ERR_OK) {
        return res;
    }

    /* Sample 9D position at blend end (near start of tc) */
    res = tcGetPosAtProgress(tc, boundary->s_tc, &boundary->P_end);
    if (res != TP_ERR_OK) {
        return res;
    }

    /* Get 9D tangent vectors at blend start */
    res = tcGetTangentAtProgress(prev_tc, boundary->s_prev,
                                 &boundary->u_start_xyz,
                                 &boundary->u_start_abc,
                                 &boundary->u_start_uvw);
    if (res != TP_ERR_OK) {
        return res;
    }

    /* Get 9D tangent vectors at blend end */
    res = tcGetTangentAtProgress(tc, boundary->s_tc,
                                 &boundary->u_end_xyz,
                                 &boundary->u_end_abc,
                                 &boundary->u_end_uvw);
    if (res != TP_ERR_OK) {
        return res;
    }

    /* Compute intersection point (xyz corner) from start of tc.
     * This is the shared endpoint, works for any motion type. */
    EmcPose corner;
    res = tcGetPosAtProgress(tc, 0.0, &corner);
    if (res == TP_ERR_OK) {
        boundary->intersection_point = corner.tran;
    } else {
        /* Fallback to legacy function for TC_LINEAR/TC_CIRCULAR */
        tcGetIntersectionPoint(prev_tc, tc, &boundary->intersection_point);
    }

    return TP_ERR_OK;
}

/**
 * Compute kinematic parameters from boundary conditions.
 *
 * Finds the intersection angle between xyz tangent vectors, computes
 * normal acceleration limit, velocity goal, and target radius.
 * Per-axis bounds across all 9 dimensions constrain the velocity
 * and acceleration to stay within each axis's individual limits.
 */
int findBlendParameters9(BlendBoundary9 const * const boundary,
                         TC_STRUCT const * const prev_tc,
                         TC_STRUCT const * const tc,
                         double max_feed_scale,
                         AxisBounds9 const * const vel_bounds,
                         AxisBounds9 const * const acc_bounds,
                         BlendParams9 * const params)
{
    if (!boundary || !prev_tc || !tc || !params) {
        return TP_ERR_MISSING_INPUT;
    }

    /* Compute intersection angle from xyz tangent vectors */
    int res = findIntersectionAngle(&boundary->u_start_xyz,
                                    &boundary->u_end_xyz,
                                    &params->theta);
    if (res != TP_ERR_OK) {
        return res;
    }

    /* Check for nearly-collinear segments */
    if (params->theta < BLEND9_MIN_THETA) {
        tp_debug_print("findBlendParameters9: theta %g too small\n", params->theta);
        return TP_ERR_TOLERANCE;
    }

    /* Normal acceleration budget from segment limits */
    double a_prev = prev_tc->maxaccel;
    double a_tc = tc->maxaccel;
    params->a_n_max = fmin(a_prev, a_tc) * BLEND_ACC_RATIO_NORMAL;

    /* Goal velocity (worst case at max feed override) */
    double v_prev = tcGetMaxTargetVel(prev_tc, max_feed_scale);
    double v_tc = tcGetMaxTargetVel(tc, max_feed_scale);
    params->v_goal = fmax(v_prev, v_tc);

    /* Apply per-axis bounds if provided */
    if (vel_bounds && acc_bounds) {
        double v_max_planar, a_max_planar;
        findBlendPlanarLimits9(boundary, vel_bounds, acc_bounds,
                               &v_max_planar, &a_max_planar);

        /* Constrain v_goal and a_n_max by per-axis limits */
        params->v_goal = fmin(params->v_goal, v_max_planar);
        params->a_n_max = fmin(params->a_n_max, a_max_planar * BLEND_ACC_RATIO_NORMAL);

        tp_debug_print("  per-axis limits: v_max_planar=%g, a_max_planar=%g\n",
                       v_max_planar, a_max_planar);
    }

    /* Goal radius for centripetal limit */
    if (params->a_n_max > TP_ACCEL_EPSILON) {
        params->R_goal = (params->v_goal * params->v_goal) / params->a_n_max;
    } else {
        params->R_goal = TP_BIG_NUM;
    }

    /* v_plan starts at v_goal, refined by optimizer */
    params->v_plan = params->v_goal;

    tp_debug_print("findBlendParameters9: theta=%g, a_n_max=%g, v_goal=%g, R_goal=%g\n",
                   params->theta, params->a_n_max, params->v_goal, params->R_goal);

    return TP_ERR_OK;
}

/**
 * Golden section search for optimal alpha (control point distance).
 *
 * bezier9AccLimit(alpha) = min(v_curvature, v_dkds) is unimodal in alpha:
 *   - Small alpha → steep curvature ramp at endpoints → high dκ/ds → low v_dkds
 *   - Large alpha → sharp peak curvature at midpoint → high κ_max → low v_curvature
 * The maximum is where the two limits cross.
 *
 * Returns the best velocity limit, stores the winning Bezier9 and alpha.
 */
static double findOptimalAlpha9(double Rb,
                                BlendBoundary9 const * const boundary,
                                double a_max,
                                double j_max,
                                double v_goal,
                                Bezier9 * const best_bezier,
                                double * const best_alpha)
{
    double a_lo = Rb * BLEND9_ALPHA_MIN_RATIO;
    double a_hi = Rb * BLEND9_ALPHA_MAX_RATIO;

    double gr = BEZIER9_GOLDEN_RATIO;
    double a1 = a_hi - gr * (a_hi - a_lo);
    double a2 = a_lo + gr * (a_hi - a_lo);

    /* Evaluate at initial probe points */
    Bezier9 trial1, trial2;
    double v1 = 0.0, v2 = 0.0;

    if (bezier9Init(&trial1, &boundary->P_start, &boundary->P_end,
                    &boundary->u_start_xyz, &boundary->u_end_xyz,
                    &boundary->u_start_abc, &boundary->u_end_abc,
                    &boundary->u_start_uvw, &boundary->u_end_uvw,
                    a1) == TP_ERR_OK) {
        v1 = bezier9AccLimit(&trial1, v_goal, a_max, j_max);
    }

    if (bezier9Init(&trial2, &boundary->P_start, &boundary->P_end,
                    &boundary->u_start_xyz, &boundary->u_end_xyz,
                    &boundary->u_start_abc, &boundary->u_end_abc,
                    &boundary->u_start_uvw, &boundary->u_end_uvw,
                    a2) == TP_ERR_OK) {
        v2 = bezier9AccLimit(&trial2, v_goal, a_max, j_max);
    }

    for (int i = 0; i < BLEND9_ALPHA_SEARCH_ITERS; i++) {
        if (v1 < v2) {
            /* Maximum is in [a1, a_hi] */
            a_lo = a1;
            a1 = a2;
            v1 = v2;
            trial1 = trial2;
            a2 = a_lo + gr * (a_hi - a_lo);
            if (bezier9Init(&trial2, &boundary->P_start, &boundary->P_end,
                            &boundary->u_start_xyz, &boundary->u_end_xyz,
                            &boundary->u_start_abc, &boundary->u_end_abc,
                            &boundary->u_start_uvw, &boundary->u_end_uvw,
                            a2) == TP_ERR_OK) {
                v2 = bezier9AccLimit(&trial2, v_goal, a_max, j_max);
            } else {
                v2 = 0.0;
            }
        } else {
            /* Maximum is in [a_lo, a2] */
            a_hi = a2;
            a2 = a1;
            v2 = v1;
            trial2 = trial1;
            a1 = a_hi - gr * (a_hi - a_lo);
            if (bezier9Init(&trial1, &boundary->P_start, &boundary->P_end,
                            &boundary->u_start_xyz, &boundary->u_end_xyz,
                            &boundary->u_start_abc, &boundary->u_end_abc,
                            &boundary->u_start_uvw, &boundary->u_end_uvw,
                            a1) == TP_ERR_OK) {
                v1 = bezier9AccLimit(&trial1, v_goal, a_max, j_max);
            } else {
                v1 = 0.0;
            }
        }
    }

    /* Pick the better of the two final probes */
    if (v1 >= v2) {
        *best_bezier = trial1;
        *best_alpha = a1;
        return v1;
    } else {
        *best_bezier = trial2;
        *best_alpha = a2;
        return v2;
    }
}

/**
 * Binary search optimizer for blend region size.
 *
 * Searches for the largest Rb (blend region size) that satisfies both:
 * 1. G64 P tolerance: bezier9Deviation <= tolerance
 * 2. Velocity goal: bezier9AccLimit >= v_goal (within tolerance)
 *
 * Growing Rb increases blend radius (better velocity) but also increases
 * deviation from the corner. The search finds the sweet spot.
 */
int optimizeBlendSize9(TC_STRUCT const * const prev_tc,
                       TC_STRUCT const * const tc,
                       double tolerance,
                       double max_feed_scale,
                       AxisBounds9 const * const vel_bounds,
                       AxisBounds9 const * const acc_bounds,
                       double j_max,
                       BlendSolution9 * const result)
{
    if (!prev_tc || !tc || !result) {
        return TP_ERR_MISSING_INPUT;
    }

    memset(result, 0, sizeof(BlendSolution9));
    result->status = BLEND9_FAIL;

    /* Get maximum blend region */
    double Rb_max = findMaxBlendRegion9(prev_tc, tc);
    if (Rb_max < TP_POS_EPSILON) {
        tp_debug_print("optimizeBlendSize9: Rb_max too small (%g)\n", Rb_max);
        result->status = BLEND9_ZERO_LENGTH;
        return TP_ERR_FAIL;
    }

    /* First pass: check if blend is geometrically feasible */
    BlendBoundary9 boundary;
    BlendParams9 params;

    int res = findBlendPointsAndTangents9(Rb_max, prev_tc, tc, &boundary);
    if (res != TP_ERR_OK) {
        return res;
    }

    res = findBlendParameters9(&boundary, prev_tc, tc, max_feed_scale,
                               vel_bounds, acc_bounds, &params);
    if (res != TP_ERR_OK) {
        if (res == TP_ERR_TOLERANCE) {
            result->status = BLEND9_COLLINEAR;
        }
        return res;
    }

    /* Kinematic limits (constant across iterations) */
    double a_max_blend = fmin(prev_tc->maxaccel, tc->maxaccel);
    double j_max_blend = j_max;

    /* Binary search between epsilon and Rb_max */
    double Rb_lo = TP_POS_EPSILON;
    double Rb_hi = Rb_max;
    int found_valid = 0;

    for (int iter = 0; iter < BLEND9_MAX_ITERATIONS; iter++) {
        double Rb = (Rb_lo + Rb_hi) / 2.0;

        /* Sample boundary at this Rb */
        res = findBlendPointsAndTangents9(Rb, prev_tc, tc, &boundary);
        if (res != TP_ERR_OK) {
            Rb_hi = Rb;
            continue;
        }

        /* Compute parameters */
        res = findBlendParameters9(&boundary, prev_tc, tc, max_feed_scale,
                                   vel_bounds, acc_bounds, &params);
        if (res != TP_ERR_OK) {
            Rb_hi = Rb;
            continue;
        }

        /* Find optimal alpha for this Rb via golden section search */
        Bezier9 trial;
        double alpha;
        double v_limit = findOptimalAlpha9(Rb, &boundary, a_max_blend,
                                           j_max_blend, params.v_goal,
                                           &trial, &alpha);
        if (v_limit <= 0.0) {
            Rb_hi = Rb;
            continue;
        }

        /* Check deviation against tolerance */
        double deviation = bezier9Deviation(&trial, &boundary.intersection_point);

        tp_debug_print("  iter=%d Rb=%g alpha=%g dev=%g tol=%g v_limit=%g v_goal=%g len=%g\n",
                       iter, Rb, alpha, deviation, tolerance, v_limit, params.v_goal,
                       bezier9Length(&trial));

        /* Convergence logic */
        int tol_ok = (deviation <= tolerance);
        int vel_ok = (v_limit >= params.v_goal * (1.0 - BLEND9_VEL_REL_TOL))
                     || (v_limit >= params.v_goal - BLEND9_VEL_ABS_TOL);

        if (tol_ok && vel_ok) {
            /* Found a good solution -- try to grow for smoother blend */
            result->Rb = Rb;
            result->bezier = trial;
            result->boundary = boundary;
            result->params = params;
            result->params.v_plan = fmin(v_limit, params.v_goal);
            found_valid = 1;
            Rb_lo = Rb;
        } else if (!tol_ok) {
            /* Deviation too large -- shrink */
            Rb_hi = Rb;
        } else {
            /* Velocity too low but tolerance OK -- accept with reduced
             * velocity. Sharp corners can't reach full feed, and that's
             * fine: the blend still smooths the path. Try to grow for
             * a larger radius (better velocity). */
            result->Rb = Rb;
            result->bezier = trial;
            result->boundary = boundary;
            result->params = params;
            result->params.v_plan = v_limit;
            found_valid = 1;
            Rb_lo = Rb;
        }

        /* Check if search interval has converged */
        if ((Rb_hi - Rb_lo) < (Rb_max * 0.01) && found_valid) {
            tp_debug_print("  converged at iter=%d, Rb=%g\n", iter, result->Rb);
            break;
        }
    }

    /* If no valid solution found during search, try Rb_max as fallback */
    if (!found_valid) {
        double Rb = Rb_max;
        res = findBlendPointsAndTangents9(Rb, prev_tc, tc, &boundary);
        if (res == TP_ERR_OK) {
            res = findBlendParameters9(&boundary, prev_tc, tc, max_feed_scale,
                                       vel_bounds, acc_bounds, &params);
        }
        if (res == TP_ERR_OK) {
            Bezier9 trial;
            double alpha;
            double v_limit = findOptimalAlpha9(Rb, &boundary, a_max_blend,
                                               j_max_blend, params.v_goal,
                                               &trial, &alpha);
            if (v_limit > 0.0) {
                double deviation = bezier9Deviation(&trial, &boundary.intersection_point);
                if (deviation <= tolerance) {
                    result->Rb = Rb;
                    result->bezier = trial;
                    result->boundary = boundary;
                    result->params = params;
                    result->params.v_plan = fmin(v_limit, params.v_goal);
                    found_valid = 1;
                }
            }
        }
    }

    if (!found_valid) {
        tp_debug_print("optimizeBlendSize9: no valid solution found\n");
        result->status = BLEND9_TOLERANCE_EXCEEDED;
        return TP_ERR_FAIL;
    }

    /* Compute trim amounts */
    result->trim_prev = prev_tc->target - result->boundary.s_prev;
    result->trim_tc = result->boundary.s_tc;
    result->status = BLEND9_OK;

    tp_debug_print("optimizeBlendSize9: Rb=%g, v_plan=%g, length=%g, trim_prev=%g, trim_tc=%g\n",
                   result->Rb, result->params.v_plan,
                   bezier9Length(&result->bezier),
                   result->trim_prev, result->trim_tc);

    return TP_ERR_OK;
}

/**
 * Create a TC_STRUCT for the blend segment from optimization result.
 *
 * Initializes a new segment with motion_type = TC_BEZIER, sets kinematic
 * limits from the Bezier curvature analysis, and marks the segment for
 * tangent continuity with adjacent segments.
 */
int createBlendSegment9(TC_STRUCT const * const prev_tc,
                        TC_STRUCT const * const tc,
                        BlendSolution9 const * const solution,
                        TC_STRUCT * const blend_tc,
                        double cycle_time)
{
    if (!prev_tc || !tc || !solution || !blend_tc) {
        return TP_ERR_MISSING_INPUT;
    }

    /* Zero-initialize */
    memset(blend_tc, 0, sizeof(TC_STRUCT));

    /* Initialize as TC_BEZIER */
    tcInit(blend_tc,
           TC_BEZIER,
           prev_tc->canon_motion_type,
           cycle_time,
           prev_tc->enables,
           0);  /* no atspeed for blend */

    /* Set tolerance from parent segments */
    blend_tc->tolerance = fmin(prev_tc->tolerance, tc->tolerance);
    blend_tc->synchronized = TC_SYNC_NONE;

    /* Kinematic limits */
    double a_max = fmin(prev_tc->maxaccel, tc->maxaccel);
    double v_plan = solution->params.v_plan;
    double v_req = fmax(prev_tc->reqvel, tc->reqvel);
    double jerk = fmin(prev_tc->maxjerk, tc->maxjerk);

    tcSetupMotion(blend_tc, v_req, v_plan, a_max, jerk);

    /* Hard cap: curvature-rate jerk limit (like kink_vel, ignores feed override).
     * Centripetal jerk = v³ · dκ/ds must stay within the jerk budget. */
    double v_jerk_limit = bezier9AccLimit(&solution->bezier, v_req, a_max, jerk);
    blend_tc->kink_vel = v_jerk_limit;

    /* Store bezier curve in coords union */
    blend_tc->coords.bezier = solution->bezier;

    /* Set target length */
    double length = bezier9Length(&solution->bezier);
    blend_tc->target = length;
    blend_tc->nominal_length = length;

    /* Terminal condition: tangent continuity with next segment */
    blend_tc->term_cond = TC_TERM_COND_TANGENT;

    /* Tangential acceleration ratio for bezier
     * Accounts for centripetal acceleration consuming part of the budget */
    if (solution->bezier.min_radius > TP_POS_EPSILON) {
        double a_n = (v_plan * v_plan) / solution->bezier.min_radius;
        if (a_n < a_max) {
            blend_tc->acc_ratio_tan = sqrt(1.0 - (a_n * a_n) / (a_max * a_max));
        } else {
            blend_tc->acc_ratio_tan = BLEND_ACC_RATIO_TANGENTIAL;
        }
    } else {
        blend_tc->acc_ratio_tan = BLEND_ACC_RATIO_TANGENTIAL;
    }

    /* Clamp velocity by segment length (prevents exceeding one sample per cycle) */
    tcClampVelocityByLength(blend_tc);

    /* Copy metadata from parent segments */
    blend_tc->syncdio = prev_tc->syncdio;
    blend_tc->tag = prev_tc->tag;
    blend_tc->indexer_jnum = -1;
    blend_tc->active_depth = 1;

    tp_debug_print("createBlendSegment9: target=%g, v_plan=%g, acc=%g, acc_ratio_tan=%g\n",
                   blend_tc->target, v_plan, a_max, blend_tc->acc_ratio_tan);

    return TP_ERR_OK;
}

/**
 * Trim a TC_STRUCT by removing length from its end or start.
 *
 * Dispatches by motion type to handle xyz, abc, uvw components:
 * - TC_LINEAR: pmCartLineStretch on each PmCartLine component
 * - TC_CIRCULAR: pmCircleCut9 on xyz circle (helix-safe), pmCartLineStretch on abc/uvw
 * - TC_SPHERICAL: adjusts arc angle range
 *
 * @param tc          Segment to trim (modified in place)
 * @param trim_amount Length to remove
 * @param from_end    1 = keep end (trim start), 0 = keep start (trim end)
 */
int trimSegment9(TC_STRUCT * const tc,
                 double trim_amount,
                 int from_end)
{
    if (!tc || trim_amount < TP_POS_EPSILON) {
        return TP_ERR_OK;  /* Nothing to trim */
    }

    double old_target = tc->target;
    double new_target = old_target - trim_amount;

    if (new_target < TP_POS_EPSILON) {
        tp_debug_print("trimSegment9: trim_amount %g exceeds target %g\n",
                       trim_amount, old_target);
        return TP_ERR_FAIL;
    }

    double ratio = new_target / old_target;

    switch (tc->motion_type) {
        case TC_LINEAR: {
            /* Trim xyz */
            if (!tc->coords.line.xyz.tmag_zero) {
                double xyz_new = tc->coords.line.xyz.tmag * ratio;
                pmCartLineStretch(&tc->coords.line.xyz, xyz_new, from_end);
            }

            /* Trim abc proportionally */
            if (!tc->coords.line.abc.tmag_zero) {
                double abc_new = tc->coords.line.abc.tmag * ratio;
                pmCartLineStretch(&tc->coords.line.abc, abc_new, from_end);
            }

            /* Trim uvw proportionally */
            if (!tc->coords.line.uvw.tmag_zero) {
                double uvw_new = tc->coords.line.uvw.tmag * ratio;
                pmCartLineStretch(&tc->coords.line.uvw, uvw_new, from_end);
            }

            tc->target = new_target;
            /* Keep nominal_length as original untrimmed length so that
             * findMaxBlendRegion9's "half the original segment" guard
             * gives equal Rb budget to blends on both sides. */
            break;
        }

        case TC_CIRCULAR: {
            /* Convert trim ratio to new angle */
            double new_angle = tc->coords.circle.xyz.angle * ratio;

            /* Trim xyz circle with helix-safe cut */
            int res = pmCircleCut9(&tc->coords.circle.xyz, new_angle, from_end);
            if (res != PM_OK) {
                tp_debug_print("trimSegment9: pmCircleCut9 failed\n");
                return TP_ERR_FAIL;
            }

            /* Recompute spiral arc length fit */
            findSpiralArcLengthFit(&tc->coords.circle.xyz, &tc->coords.circle.fit);

            /* Trim abc proportionally */
            if (!tc->coords.circle.abc.tmag_zero) {
                double abc_new = tc->coords.circle.abc.tmag * ratio;
                pmCartLineStretch(&tc->coords.circle.abc, abc_new, from_end);
            }

            /* Trim uvw proportionally */
            if (!tc->coords.circle.uvw.tmag_zero) {
                double uvw_new = tc->coords.circle.uvw.tmag * ratio;
                pmCartLineStretch(&tc->coords.circle.uvw, uvw_new, from_end);
            }

            /* Recompute target from new geometry */
            tc->target = pmCircle9Target(&tc->coords.circle);
            break;
        }

        case TC_SPHERICAL: {
            /* SphericalArc: adjust angular range.
             * The arc is parameterized by progress (arc length).
             * Trimming means reducing the angular span and updating
             * start/end radial vectors via SLERP. */
            SphericalArc *arc = &tc->coords.arc.xyz;
            double new_angle = arc->angle * ratio;

            if (from_end) {
                /* Keep end, trim start: compute new rStart via SLERP */
                double cut_angle = arc->angle - new_angle;
                if (arc->Sangle > TP_POS_EPSILON) {
                    double s0 = sin(arc->angle - cut_angle) / arc->Sangle;
                    double s1 = sin(cut_angle) / arc->Sangle;
                    PmCartesian new_rStart;
                    PmCartesian i0, i1;
                    pmCartScalMult(&arc->rStart, s0, &i0);
                    pmCartScalMult(&arc->rEnd, s1, &i1);
                    pmCartCartAdd(&i0, &i1, &new_rStart);
                    arc->rStart = new_rStart;

                    /* Update start point and tangent */
                    pmCartCartAdd(&arc->center, &new_rStart, &arc->start);
                    arcTangent(arc, &arc->uTan, 0);
                }
            } else {
                /* Keep start, trim end: compute new rEnd via SLERP */
                if (arc->Sangle > TP_POS_EPSILON) {
                    double s0 = sin(arc->angle - new_angle) / arc->Sangle;
                    double s1 = sin(new_angle) / arc->Sangle;
                    PmCartesian new_rEnd;
                    PmCartesian i0, i1;
                    pmCartScalMult(&arc->rStart, s0, &i0);
                    pmCartScalMult(&arc->rEnd, s1, &i1);
                    pmCartCartAdd(&i0, &i1, &new_rEnd);
                    arc->rEnd = new_rEnd;

                    /* Update end point */
                    pmCartCartAdd(&arc->center, &new_rEnd, &arc->end);
                }
            }

            /* Scale spiral proportionally */
            double angle_ratio = new_angle / arc->angle;
            arc->spiral *= angle_ratio;
            arc->angle = new_angle;
            arc->Sangle = sin(new_angle);

            /* Update arc target */
            tc->target = new_angle * arc->radius + arc->line_length * ratio;
            break;
        }

        default:
            tp_debug_print("trimSegment9: unsupported motion type %d\n",
                           tc->motion_type);
            return TP_ERR_FAIL;
    }

    tp_debug_print("trimSegment9: type=%d, from_end=%d, trim=%g, new_target=%g\n",
                   tc->motion_type, from_end, trim_amount, tc->target);

    return TP_ERR_OK;
}
