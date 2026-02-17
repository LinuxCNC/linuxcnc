#include "bezier9.h"
#include "tp_types.h"
#include "tp_debug.h"
#include "posemath.h"
#include "blendmath.h"
#include "rtapi_math.h"
#include <math.h>

// Gauss-Legendre quadrature nodes and weights for 16-point integration
// Nodes are symmetric around 0, weights sum to 2
static const double GL16_NODES[16] = {
    -0.989400934991649932596, -0.944575023073232576078,
    -0.865631202387831743880, -0.755404408355003033895,
    -0.617876244402643748447, -0.458016777657227386342,
    -0.281603550779258913230, -0.095012509837637440185,
     0.095012509837637440185,  0.281603550779258913230,
     0.458016777657227386342,  0.617876244402643748447,
     0.755404408355003033895,  0.865631202387831743880,
     0.944575023073232576078,  0.989400934991649932596
};

static const double GL16_WEIGHTS[16] = {
    0.027152459411754094852, 0.062253523938647892863,
    0.095158511682492784810, 0.124628971255533872052,
    0.149595988816576732081, 0.169156519395002538189,
    0.182603415044923588867, 0.189450610455068496285,
    0.189450610455068496285, 0.182603415044923588867,
    0.169156519395002538189, 0.149595988816576732081,
    0.124628971255533872052, 0.095158511682492784810,
    0.062253523938647892863, 0.027152459411754094852
};

/**
 * bezier5_eval - Evaluate quintic Bezier curve at parameter t
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
 * bezier5_deriv - Evaluate first derivative of quintic Bezier at t
 *
 * B'(t) = 5 * sum_{i=0}^{4} C(4,i) (1-t)^(4-i) t^i (P_{i+1} - P_i)
 *
 * This is a degree-4 Bezier on the forward-difference control points.
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

    // Coefficients: 5 * C(4,i) * s^(4-i) * t^i
    double c0 = 5.0 * s4;
    double c1 = 5.0 * 4.0 * s3 * t;
    double c2 = 5.0 * 6.0 * s2 * t2;
    double c3 = 5.0 * 4.0 * s * t3;
    double c4 = 5.0 * t4;

    // Forward differences D_i = P_{i+1} - P_i
    PmCartesian D[5];
    for (int i = 0; i < 5; i++) {
        D[i].x = P[i+1].x - P[i].x;
        D[i].y = P[i+1].y - P[i].y;
        D[i].z = P[i+1].z - P[i].z;
    }

    out->x = c0*D[0].x + c1*D[1].x + c2*D[2].x + c3*D[3].x + c4*D[4].x;
    out->y = c0*D[0].y + c1*D[1].y + c2*D[2].y + c3*D[3].y + c4*D[4].y;
    out->z = c0*D[0].z + c1*D[1].z + c2*D[2].z + c3*D[3].z + c4*D[4].z;
}

/**
 * bezier5_deriv2 - Evaluate second derivative of quintic Bezier at t
 *
 * B''(t) = 20 * sum_{i=0}^{3} C(3,i) (1-t)^(3-i) t^i E_i
 *
 * where E_i = P_{i+2} - 2 P_{i+1} + P_i (second forward differences).
 * This is a degree-3 Bezier on E_i.
 */
static void bezier5_deriv2(PmCartesian const * const P,
                           double t,
                           PmCartesian * const out)
{
    double s = 1.0 - t;
    double s2 = s * s;
    double s3 = s2 * s;
    double t2 = t * t;
    double t3 = t2 * t;

    // Coefficients: 20 * C(3,i) * s^(3-i) * t^i
    double c0 = 20.0 * s3;
    double c1 = 20.0 * 3.0 * s2 * t;
    double c2 = 20.0 * 3.0 * s * t2;
    double c3 = 20.0 * t3;

    // Second forward differences E_i = P_{i+2} - 2*P_{i+1} + P_i
    PmCartesian E[4];
    for (int i = 0; i < 4; i++) {
        E[i].x = P[i+2].x - 2.0*P[i+1].x + P[i].x;
        E[i].y = P[i+2].y - 2.0*P[i+1].y + P[i].y;
        E[i].z = P[i+2].z - 2.0*P[i+1].z + P[i].z;
    }

    out->x = c0*E[0].x + c1*E[1].x + c2*E[2].x + c3*E[3].x;
    out->y = c0*E[0].y + c1*E[1].y + c2*E[2].y + c3*E[3].y;
    out->z = c0*E[0].z + c1*E[1].z + c2*E[2].z + c3*E[3].z;
}

/**
 * bezier9_deriv_mag - Compute 9D magnitude of derivative
 *
 * |B'(t)| = sqrt(|B'_xyz|² + |B'_abc|² + |B'_uvw|²)
 */
static double bezier9_deriv_mag(Bezier9 const * const b, double t)
{
    PmCartesian dP, dA, dU;
    bezier5_deriv(b->P, t, &dP);
    bezier5_deriv(b->A, t, &dA);
    bezier5_deriv(b->U, t, &dU);

    double mag_xyz2 = dP.x * dP.x + dP.y * dP.y + dP.z * dP.z;
    double mag_abc2 = dA.x * dA.x + dA.y * dA.y + dA.z * dA.z;
    double mag_uvw2 = dU.x * dU.x + dU.y * dU.y + dU.z * dU.z;

    return sqrt(mag_xyz2 + mag_abc2 + mag_uvw2);
}

/**
 * gauss_legendre_integrate - Integrate function over [t_start, t_end]
 *
 * Transforms 16-point Gauss-Legendre rule from [-1, 1] to [t_start, t_end]:
 * ∫ f(t) dt ≈ half_range * Σᵢ wᵢ f(mid + half_range * xᵢ)
 */
static double gauss_legendre_integrate(Bezier9 const * const b,
                                       double t_start,
                                       double t_end)
{
    double mid = 0.5 * (t_end + t_start);
    double half_range = 0.5 * (t_end - t_start);
    double sum = 0.0;

    for (int i = 0; i < 16; i++) {
        double t = mid + half_range * GL16_NODES[i];
        sum += GL16_WEIGHTS[i] * bezier9_deriv_mag(b, t);
    }

    return half_range * sum;
}

/**
 * build_arc_length_table - Precompute arc-length parameterization
 *
 * Builds lookup tables mapping Bezier parameter t to arc length s.
 * Uses Gauss-Legendre quadrature to compute cumulative arc length.
 */
static void build_arc_length_table(Bezier9 * const b)
{
    b->t_table[0] = 0.0;
    b->s_table[0] = 0.0;

    for (int i = 1; i <= BEZIER9_ARC_LENGTH_SAMPLES; i++) {
        double t_prev = (double)(i - 1) / BEZIER9_ARC_LENGTH_SAMPLES;
        double t_curr = (double)i / BEZIER9_ARC_LENGTH_SAMPLES;

        double ds = gauss_legendre_integrate(b, t_prev, t_curr);

        b->t_table[i] = t_curr;
        b->s_table[i] = b->s_table[i - 1] + ds;
    }

    b->total_length = b->s_table[BEZIER9_ARC_LENGTH_SAMPLES];
}

/**
 * arc_length_to_t - Map arc length s to Bezier parameter t
 *
 * Uses binary search on precomputed lookup table, then linear interpolation.
 */
static double arc_length_to_t(Bezier9 const * const b, double s)
{
    // Clamp to valid range
    if (s <= 0.0) return 0.0;
    if (s >= b->total_length) return 1.0;

    // Binary search for interval containing s
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

    // Linear interpolation within interval
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

/**
 * Set 6 control points for one 3D subspace with G2 continuity.
 *
 *   Q[0] = start
 *   Q[1] = start + alpha * u_start
 *   Q[2] = start + 2*alpha * u_start    (collinear → B''(0) = 0)
 *   Q[3] = end   - 2*alpha * u_end      (collinear → B''(1) = 0)
 *   Q[4] = end   - alpha * u_end
 *   Q[5] = end
 */
static void set_quintic_control_points(PmCartesian * Q,
                                       PmCartesian const * start,
                                       PmCartesian const * end,
                                       PmCartesian const * u_start,
                                       PmCartesian const * u_end,
                                       double alpha)
{
    double a2 = 2.0 * alpha;

    Q[0] = *start;

    Q[1].x = start->x + alpha * u_start->x;
    Q[1].y = start->y + alpha * u_start->y;
    Q[1].z = start->z + alpha * u_start->z;

    Q[2].x = start->x + a2 * u_start->x;
    Q[2].y = start->y + a2 * u_start->y;
    Q[2].z = start->z + a2 * u_start->z;

    Q[3].x = end->x - a2 * u_end->x;
    Q[3].y = end->y - a2 * u_end->y;
    Q[3].z = end->z - a2 * u_end->z;

    Q[4].x = end->x - alpha * u_end->x;
    Q[4].y = end->y - alpha * u_end->y;
    Q[4].z = end->z - alpha * u_end->z;

    Q[5] = *end;
}

int bezier9Init(Bezier9 * const b,
                EmcPose const * const start,
                EmcPose const * const end,
                PmCartesian const * const u_start_xyz,
                PmCartesian const * const u_end_xyz,
                PmCartesian const * const u_start_abc,
                PmCartesian const * const u_end_abc,
                PmCartesian const * const u_start_uvw,
                PmCartesian const * const u_end_uvw,
                double alpha)
{
    // Validate inputs
    if (!b || !start || !end) {
        return TP_ERR_MISSING_INPUT;
    }
    if (!u_start_xyz || !u_end_xyz || !u_start_abc || !u_end_abc ||
        !u_start_uvw || !u_end_uvw) {
        return TP_ERR_MISSING_INPUT;
    }
    if (alpha <= 0.0) {
        tp_debug_print("bezier9Init: invalid alpha %g, must be > 0\n", alpha);
        return TP_ERR_FAIL;
    }

    // Extract 3D start/end for each subspace
    PmCartesian start_xyz = {start->tran.x, start->tran.y, start->tran.z};
    PmCartesian end_xyz   = {end->tran.x,   end->tran.y,   end->tran.z};
    PmCartesian start_abc = {start->a, start->b, start->c};
    PmCartesian end_abc   = {end->a,   end->b,   end->c};
    PmCartesian start_uvw = {start->u, start->v, start->w};
    PmCartesian end_uvw   = {end->u,   end->v,   end->w};

    // Set quintic control points for each subspace (G2 continuity)
    set_quintic_control_points(b->P, &start_xyz, &end_xyz,
                               u_start_xyz, u_end_xyz, alpha);
    set_quintic_control_points(b->A, &start_abc, &end_abc,
                               u_start_abc, u_end_abc, alpha);
    set_quintic_control_points(b->U, &start_uvw, &end_uvw,
                               u_start_uvw, u_end_uvw, alpha);

    // Build arc-length parameterization table
    build_arc_length_table(b);

    // Check for degenerate curve
    if (b->total_length < BEZIER9_MIN_LENGTH) {
        tp_debug_print("bezier9Init: curve length %g below minimum %g\n",
                       b->total_length, BEZIER9_MIN_LENGTH);
        return TP_ERR_ZERO_LENGTH;
    }

    // Compute maximum curvature
    int ret = bezier9MaxCurvature(b);
    if (ret != TP_ERR_OK) {
        return ret;
    }

    tp_debug_print("bezier9Init: length=%g, max_kappa=%g, min_radius=%g, max_dkappa_ds=%g\n",
                   b->total_length, b->max_kappa, b->min_radius, b->max_dkappa_ds);

    return TP_ERR_OK;
}

int bezier9Point(Bezier9 const * const b,
                 double progress,
                 EmcPose * const out)
{
    if (!b || !out) {
        return TP_ERR_MISSING_INPUT;
    }

    // Map arc length to Bezier parameter
    double t = arc_length_to_t(b, progress);

    // Evaluate position in all three subspaces
    PmCartesian xyz, abc, uvw;
    bezier5_eval(b->P, t, &xyz);
    bezier5_eval(b->A, t, &abc);
    bezier5_eval(b->U, t, &uvw);

    // Pack into EmcPose
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

    // Map arc length to Bezier parameter
    double t = arc_length_to_t(b, progress);

    // Evaluate derivatives in all three subspaces
    PmCartesian dP, dA, dU;
    bezier5_deriv(b->P, t, &dP);
    bezier5_deriv(b->A, t, &dA);
    bezier5_deriv(b->U, t, &dU);

    // Normalize to unit tangent vectors
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

double bezier9Curvature(Bezier9 const * const b, double t)
{
    if (!b) {
        return 0.0;
    }

    // Evaluate first and second derivatives (xyz only)
    PmCartesian dP, ddP;
    bezier5_deriv(b->P, t, &dP);
    bezier5_deriv2(b->P, t, &ddP);

    // |B'(t)|²
    double dP_mag2 = dP.x * dP.x + dP.y * dP.y + dP.z * dP.z;
    if (dP_mag2 < BEZIER9_CURVATURE_EPSILON) {
        return 0.0;
    }

    // B' · B''
    double dot = dP.x * ddP.x + dP.y * ddP.y + dP.z * ddP.z;

    // B'' - (B'·B''/|B'|²)B'
    double scale = dot / dP_mag2;
    PmCartesian perp;
    perp.x = ddP.x - scale * dP.x;
    perp.y = ddP.y - scale * dP.y;
    perp.z = ddP.z - scale * dP.z;

    // |perp|
    double perp_mag = sqrt(perp.x * perp.x + perp.y * perp.y + perp.z * perp.z);

    // κ = |perp| / |B'|²
    return perp_mag / dP_mag2;
}

int bezier9MaxCurvature(Bezier9 * const b)
{
    if (!b) {
        return TP_ERR_MISSING_INPUT;
    }

    // Sample curvature at N evenly-spaced points
    double max_kappa = 0.0;
    int max_idx = 0;

    for (int i = 0; i <= BEZIER9_CURVATURE_SAMPLES; i++) {
        double t = (double)i / BEZIER9_CURVATURE_SAMPLES;
        double kappa = bezier9Curvature(b, t);

        if (kappa > max_kappa) {
            max_kappa = kappa;
            max_idx = i;
        }
    }

    // Refine using golden section search in neighborhood of maximum
    double t_lo, t_hi;
    if (max_idx == 0) {
        t_lo = 0.0;
        t_hi = 2.0 / BEZIER9_CURVATURE_SAMPLES;
    } else if (max_idx == BEZIER9_CURVATURE_SAMPLES) {
        t_lo = 1.0 - 2.0 / BEZIER9_CURVATURE_SAMPLES;
        t_hi = 1.0;
    } else {
        t_lo = (double)(max_idx - 1) / BEZIER9_CURVATURE_SAMPLES;
        t_hi = (double)(max_idx + 1) / BEZIER9_CURVATURE_SAMPLES;
    }

    // Golden section search for 10 iterations
    double phi = BEZIER9_GOLDEN_RATIO;
    double t1 = t_lo + (1.0 - phi) * (t_hi - t_lo);
    double t2 = t_lo + phi * (t_hi - t_lo);
    double k1 = bezier9Curvature(b, t1);
    double k2 = bezier9Curvature(b, t2);

    for (int iter = 0; iter < 10; iter++) {
        if (k1 > k2) {
            t_hi = t2;
            t2 = t1;
            k2 = k1;
            t1 = t_lo + (1.0 - phi) * (t_hi - t_lo);
            k1 = bezier9Curvature(b, t1);
        } else {
            t_lo = t1;
            t1 = t2;
            k1 = k2;
            t2 = t_lo + phi * (t_hi - t_lo);
            k2 = bezier9Curvature(b, t2);
        }
    }

    max_kappa = (k1 > k2) ? k1 : k2;

    // Cache results
    b->max_kappa = max_kappa;
    if (max_kappa > BEZIER9_CURVATURE_EPSILON) {
        b->min_radius = 1.0 / max_kappa;
    } else {
        b->min_radius = 1.0e12; // Effectively infinite radius
    }

    // Compute max |dκ/ds| using arc-length table nodes.
    // The curvature rate determines the centripetal jerk = v³ · dκ/ds,
    // which must be bounded for smooth motion at blend entry/exit.
    double max_dkds = 0.0;
    double prev_kappa = bezier9Curvature(b, 0.0);
    for (int i = 1; i <= BEZIER9_ARC_LENGTH_SAMPLES; i++) {
        double kappa_i = bezier9Curvature(b, b->t_table[i]);
        double ds = b->s_table[i] - b->s_table[i - 1];
        if (ds > BEZIER9_MIN_LENGTH) {
            double dk_ds = fabs(kappa_i - prev_kappa) / ds;
            if (dk_ds > max_dkds) {
                max_dkds = dk_ds;
            }
        }
        prev_kappa = kappa_i;
    }
    b->max_dkappa_ds = max_dkds;

    return TP_ERR_OK;
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

    // Centripetal acceleration limit: v² · κ ≤ a_normal
    double a_normal = BLEND_ACC_RATIO_NORMAL * a_max;
    double v_limit = sqrt(a_normal * b->min_radius);

    // Centripetal jerk limit: v³ · dκ/ds ≤ j_normal
    // This bounds the rate of centripetal acceleration change through
    // the blend, preventing jerk spikes at entry/exit where curvature
    // ramps from zero (G2 boundary) to peak.
    if (j_max > 0.0 && b->max_dkappa_ds > BEZIER9_CURVATURE_EPSILON) {
        double j_normal = BLEND_ACC_RATIO_NORMAL * j_max;
        double v_jerk = cbrt(j_normal / b->max_dkappa_ds);
        if (v_jerk < v_limit) {
            v_limit = v_jerk;
        }
    }

    return v_limit;
}

double bezier9Deviation(Bezier9 const * const b,
                        PmCartesian const * const intersection_point)
{
    if (!b || !intersection_point) {
        return 0.0;
    }

    // Evaluate Bezier at t = 0.5 (midpoint)
    PmCartesian midpoint;
    bezier5_eval(b->P, 0.5, &midpoint);

    // Compute xyz distance to intersection point
    double dx = midpoint.x - intersection_point->x;
    double dy = midpoint.y - intersection_point->y;
    double dz = midpoint.z - intersection_point->z;

    return sqrt(dx * dx + dy * dy + dz * dz);
}
