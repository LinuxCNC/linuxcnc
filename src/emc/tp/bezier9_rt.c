/**
 * bezier9_rt.c — RT-safe subset of Bezier9 evaluation functions
 *
 * This file provides the functions needed by the RT trajectory planner
 * (tpmod.so) to evaluate pre-built Bezier9 curves during execution.
 * Heavy initialization (Gauss-Legendre quadrature, curvature analysis)
 * is done in userspace by bezier9.cc; RT only evaluates using the
 * precomputed lookup tables stored in the Bezier9 struct.
 *
 * Functions provided:
 *   bezier9Point    — position at arc-length parameter
 *   bezier9Tangent  — unit tangent vectors at arc-length parameter
 *   bezier9AccLimit — velocity limit from cached curvature
 *   bezier9Length   — total arc length accessor
 *
 * NOT provided (userspace only):
 *   bezier9Init, bezier9Curvature, bezier9MaxCurvature, bezier9Deviation
 */

#include "posemath.h"
#include "bezier9.h"
#include "blendmath.h"      /* BLEND_ACC_RATIO_NORMAL */
#include "rtapi_math.h"
#include "tp_types.h"

/**
 * bezier5_eval — Evaluate quintic Bezier curve at parameter t
 *
 * B(t) = (1-t)^5 P0 + 5(1-t)^4 t P1 + 10(1-t)^3 t^2 P2
 *      + 10(1-t)^2 t^3 P3 + 5(1-t) t^4 P4 + t^5 P5
 */
static void bezier5_eval(PmCartesian const * const P,
                         double t,
                         PmCartesian * const out)
{
    double s = 1.0 - t;
    double s2 = s * s;
    double s3 = s2 * s;
    double s4 = s3 * s;
    double s5 = s4 * s;
    double t2 = t * t;
    double t3 = t2 * t;
    double t4 = t3 * t;
    double t5 = t4 * t;

    double b0 = s5;
    double b1 = 5.0 * s4 * t;
    double b2 = 10.0 * s3 * t2;
    double b3 = 10.0 * s2 * t3;
    double b4 = 5.0 * s * t4;
    double b5 = t5;

    out->x = b0*P[0].x + b1*P[1].x + b2*P[2].x + b3*P[3].x + b4*P[4].x + b5*P[5].x;
    out->y = b0*P[0].y + b1*P[1].y + b2*P[2].y + b3*P[3].y + b4*P[4].y + b5*P[5].y;
    out->z = b0*P[0].z + b1*P[1].z + b2*P[2].z + b3*P[3].z + b4*P[4].z + b5*P[5].z;
}

/**
 * bezier5_deriv — Evaluate first derivative of quintic Bezier at t
 *
 * B'(t) = 5 * sum_{i=0}^{4} C(4,i) (1-t)^(4-i) t^i (P_{i+1} - P_i)
 */
static void bezier5_deriv(PmCartesian const * const P,
                          double t,
                          PmCartesian * const out)
{
    double s = 1.0 - t;
    double s2 = s * s;
    double s3 = s2 * s;
    double s4 = s3 * s;
    double t2 = t * t;
    double t3 = t2 * t;
    double t4 = t3 * t;

    double c0 = 5.0 * s4;
    double c1 = 5.0 * 4.0 * s3 * t;
    double c2 = 5.0 * 6.0 * s2 * t2;
    double c3 = 5.0 * 4.0 * s * t3;
    double c4 = 5.0 * t4;

    PmCartesian D[5];
    int i;
    for (i = 0; i < 5; i++) {
        D[i].x = P[i+1].x - P[i].x;
        D[i].y = P[i+1].y - P[i].y;
        D[i].z = P[i+1].z - P[i].z;
    }

    out->x = c0*D[0].x + c1*D[1].x + c2*D[2].x + c3*D[3].x + c4*D[4].x;
    out->y = c0*D[0].y + c1*D[1].y + c2*D[2].y + c3*D[3].y + c4*D[4].y;
    out->z = c0*D[0].z + c1*D[1].z + c2*D[2].z + c3*D[3].z + c4*D[4].z;
}

/**
 * arc_length_to_t — Map arc length s to Bezier parameter t
 *
 * Uses binary search on precomputed lookup table, then linear interpolation.
 */
static double arc_length_to_t(Bezier9 const * const b, double s)
{
    if (s <= 0.0) return 0.0;
    if (s >= b->total_length) return 1.0;

    int lo = 0;
    int hi = BEZIER9_ARC_LENGTH_SAMPLES;

    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (b->s_table[mid] < s) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    double s0 = b->s_table[lo];
    double s1 = b->s_table[hi];
    double t0 = b->t_table[lo];
    double t1 = b->t_table[hi];

    double ds = s1 - s0;
    if (ds < BEZIER9_MIN_LENGTH) {
        return t0;
    }

    double alpha = (s - s0) / ds;
    return t0 + alpha * (t1 - t0);
}

/* ----------------------------------------------------------------
 * Public API — called from tc.c dispatch (tcGetPosReal, etc.)
 * ---------------------------------------------------------------- */

int bezier9Point(Bezier9 const * const b,
                 double progress,
                 EmcPose * const out)
{
    if (!b || !out) {
        return TP_ERR_MISSING_INPUT;
    }

    double t = arc_length_to_t(b, progress);

    PmCartesian xyz, abc, uvw;
    bezier5_eval(b->P, t, &xyz);
    bezier5_eval(b->A, t, &abc);
    bezier5_eval(b->U, t, &uvw);

    out->tran.x = xyz.x;
    out->tran.y = xyz.y;
    out->tran.z = xyz.z;
    out->a = abc.x;
    out->b = abc.y;
    out->c = abc.z;
    out->u = uvw.x;
    out->v = uvw.y;
    out->w = uvw.z;

    return TP_ERR_OK;
}

int bezier9Tangent(Bezier9 const * const b,
                   double progress,
                   PmCartesian * const xyz_out,
                   PmCartesian * const abc_out,
                   PmCartesian * const uvw_out)
{
    if (!b) {
        return TP_ERR_MISSING_INPUT;
    }

    double t = arc_length_to_t(b, progress);

    PmCartesian dP, dA, dU;
    bezier5_deriv(b->P, t, &dP);
    bezier5_deriv(b->A, t, &dA);
    bezier5_deriv(b->U, t, &dU);

    if (xyz_out) {
        double mag = sqrt(dP.x * dP.x + dP.y * dP.y + dP.z * dP.z);
        if (mag > BEZIER9_POS_EPSILON) {
            xyz_out->x = dP.x / mag;
            xyz_out->y = dP.y / mag;
            xyz_out->z = dP.z / mag;
        } else {
            xyz_out->x = 0.0;
            xyz_out->y = 0.0;
            xyz_out->z = 0.0;
        }
    }

    if (abc_out) {
        double mag = sqrt(dA.x * dA.x + dA.y * dA.y + dA.z * dA.z);
        if (mag > BEZIER9_POS_EPSILON) {
            abc_out->x = dA.x / mag;
            abc_out->y = dA.y / mag;
            abc_out->z = dA.z / mag;
        } else {
            abc_out->x = 0.0;
            abc_out->y = 0.0;
            abc_out->z = 0.0;
        }
    }

    if (uvw_out) {
        double mag = sqrt(dU.x * dU.x + dU.y * dU.y + dU.z * dU.z);
        if (mag > BEZIER9_POS_EPSILON) {
            uvw_out->x = dU.x / mag;
            uvw_out->y = dU.y / mag;
            uvw_out->z = dU.z / mag;
        } else {
            uvw_out->x = 0.0;
            uvw_out->y = 0.0;
            uvw_out->z = 0.0;
        }
    }

    return TP_ERR_OK;
}

double bezier9Length(Bezier9 const * const b)
{
    if (!b) {
        return 0.0;
    }
    return b->total_length;
}

double bezier9AccLimit(Bezier9 const * const b,
                       double v_req,
                       double a_max,
                       double j_max)
{
    (void)v_req;

    if (!b || a_max <= 0.0) {
        return 0.0;
    }

    double a_normal = BLEND_ACC_RATIO_NORMAL * a_max;
    double v_limit = sqrt(a_normal * b->min_radius);

    if (j_max > 0.0 && b->max_dkappa_ds > BEZIER9_POS_EPSILON) {
        double j_normal = BLEND_ACC_RATIO_NORMAL * j_max;
        double v_jerk = cbrt(j_normal / b->max_dkappa_ds);
        if (v_jerk < v_limit) {
            v_limit = v_jerk;
        }
    }

    return v_limit;
}
