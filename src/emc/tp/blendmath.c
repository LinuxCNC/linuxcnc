/********************************************************************
* Description: blendmath.c
*   Circular arc blend math functions
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*   
* Copyright (c) 2014 All rights reserved.
*
* Last change:
********************************************************************/

#include "posemath.h"
#include "tc_types.h"
#include "tc.h"
#include "tp_types.h"
#include "rtapi_math.h"
#include "spherical_arc.h"
#include "blendmath.h"
#include "tp_debug.h"
#include "tp_enums.h"
#include "stdlib.h"
#include "math_util.h"
#include "emcpose.h"

#include "tp_call_wrappers.h"

static const double NEAR_INFINITY=1e300;

/**
 * @section geomfuncs Geometry check functions
 */

/**
 * Somewhat redundant function to calculate the segment intersection angle.
 * The intersection angle is half of the supplement of the "divergence" angle
 * between unit vectors. If two unit vectors are pointing in the same
 * direction, then the intersection angle is PI/2. This is based on the
 * simple_tp formulation for tolerances.
 */
double findIntersectionAngle(PmVector const * const u1,
        PmVector const * const u2)
{
    double dot = VecVecDot(u1, u2);

    sat_inplace(&dot,1.0);

    return acos(-dot)/2.0;
}

int findAccelScale(PmVector const * const acc,
        PmVector const * const bounds,
        PmVector * const scale)
{
    if (!acc || !bounds ) {
        return TP_ERR_MISSING_INPUT;
    }

    if (!scale ) {
        return TP_ERR_MISSING_OUTPUT;
    }

    for (int i=0; i < PM_VECTOR_SIZE; ++i) {
        double b = bounds->ax[i];
        scale->ax[i] = b ? fabs(acc->ax[i] / b) : 0.0;
    }

    return TP_ERR_OK;
}

// KLUDGE this is a messy workaround for named fields in PmVector
static inline void minBoundRatioIfNonzero(double s, double b, double *m2)
{
    if (s > 0.0 && b > 0.0) {
        //Have to square b here since the scale is also squared
        *m2 = fmin(*m2, pmSq(b) / s);
    }
}

/**
 * Finds the maximum allowed value of the bounded quantity on the given plane.
 *
 * @param plane_envelope_sq is the square of the maximal value of the ith component of a unit
 * vector rotated through the plane. This basically specifies the largest
 * possible contribution of each axis to a direction in the plane. We use squared values here
 * because they are easier to compute, and require only one square root at the end.
 * @param bounds Specifies the per-axis maximum value (e.g. acceleration, velocity)
 * @param max_planar_value maximum length of a vector in the plane that is always within limits
 * @return -1 on error, 0 if completed successfully
 */
int findMaxValueOnPlane(PmVector const * plane_envelope_sq,
        PmVector const * bounds,
        double * max_planar_value)
{
    if (!plane_envelope_sq || !bounds || !max_planar_value) {
        return -1;
    }
    double m2 = NEAR_INFINITY;

    for (int i = 0; i < PM_VECTOR_SIZE; ++i) {
        minBoundRatioIfNonzero(plane_envelope_sq->ax[i], bounds->ax[i], &m2);
    }

    // One square root at the end to get the actual max accel
    *max_planar_value = pmSqrt(m2);
    return 0;
}

/** Find real roots of a quadratic equation in standard form. */
int quadraticFormula(double A, double B, double C, double * const root0,
        double * const root1)
{
    if (A < 0.0) {
        A *=-1;
        B *=-1;
        C *=-1;
    }
    double disc = pmSq(B) - 4.0 * A * C;
    if (disc < 0) {
        tp_debug_print("discriminant %.12g < 0, A=%.12g, B=%.12g,C=%.12g\n", disc, A, B, C);
        return TP_ERR_FAIL;
    }
    double t1 = pmSqrt(disc);
    if (root0) {
        *root0 = ( -B + t1) / (2.0 * A);
    }
    if (root1) {
        *root1 = ( -B - t1) / (2.0 * A);
    }
    return TP_ERR_OK;
}

/**
 * @section blending blend math functions
 */

void findVMaxByAltitude(
    PmVector const * const u1,
    PmVector const * const u2,
    BlendControls const * const controls,
    BlendParameters * const param)
{
    if (fabs(param->phi) < TP_ANGLE_EPSILON) {
        param->v_max_altitude = fmin(controls->v_max_geom1, controls->v_max_geom2);
        return;
    }
    // Clip the angle at a reasonable value (less than 90 deg), to prevent div by zero
    double phi_effective = fmin(param->phi, PM_PI * 0.49);

    // Copy over maximum velocities, clipping velocity to place altitude within base
    double Cp = cos(phi_effective);
    double v_effective1 = fmin(controls->v_max_geom1, controls->v_max_geom2 / Cp);
    double v_effective2 = fmin(controls->v_max_geom2, controls->v_max_geom1 / Cp);

    tp_debug_print("v_max1 = %f, v_max2 = %f\n", v_effective1, v_effective2);

    // Get "altitude"
    double v_area = v_effective1 * v_effective2 / 2.0 * sin(param->phi);
    tp_debug_print("phi = %f\n", param->phi);
    tp_debug_print("v_area = %f\n", v_area);

    // Get "base" of triangle
    PmVector tmp1, tmp2, diff;
    VecScalMult(u1, v_effective1, &tmp1);
    VecScalMult(u2, v_effective2, &tmp2);
    VecVecSub(&tmp2, &tmp1, &diff);
    double base = VecMag(&diff);
    tp_debug_print("v_base = %f\n", base);

    param->v_max_altitude = 2.0 * v_area / base;
}

static inline double normalAccelFromMax(double a_max)
{
    return a_max * BLEND_ACC_RATIO_NORMAL;
}

int find_blend_parameters(
    PmVector const * const u_tan1,
    PmVector const * const u_tan2,
    PmVector const * const acc_bound,
    PmVector const * const vel_bound,
    BlendControls const * const controls,
    BlendParameters * const param)
{
    findVMaxByAltitude(
        u_tan1,
        u_tan2,
        controls,
        param);

    // KLUDGE: common operations, but not exactly kinematics
    param->phi = (PM_PI - param->theta * 2.0);

    int res_limits = find_blend_vel_accel_planar_limits(
        u_tan1,
        u_tan2,
        acc_bound,
        vel_bound,
        &param->a_max_planar,
        &param->v_max_planar);

    // Store max normal acceleration
    param->a_n_max = normalAccelFromMax(param->a_max_planar);

    return res_limits;
}

tp_err_t init_blend_segment_from_points(
    TC_STRUCT * const blend_tc,
    BlendPoints const *points,
    BlendParameters * const param)
{
    blend_tc->motion_type = points->motion_type;

    switch (points->motion_type) {
    case TC_LINEAR:
        pmLine9Init(&blend_tc->coords.line, &points->arc_start, &points->arc_end);
        break;
    case TC_SPHERICAL:
    {
        int res_arc = arc9InitFromPoints(
            &blend_tc->coords.arc,
            &points->arc_start,
            &points->arc_end,
            &points->arc_center,
            &points->u_tan,
            param->line_length);
        if (res_arc != 0) {
            return res_arc;
        }
        double v_max = pmSqrt(blend_tc->coords.arc.radius * param->a_n_max);
        param->v_plan = fmin(v_max, param->v_plan);
    }
        break;
    default:
        return TP_ERR_FAIL;
    }
    return TP_ERR_OK;
}

int blendParamInitVelocities(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    double override_allowance,
    BlendControls * const controls)
{
    // Copy over geometric maximum velocities
    controls->v_max_geom1 = prev_tc->maxvel_geom;
    controls->v_max_geom2 = tc->maxvel_geom;

    // Find the nominal velocity for the blend segment with no overrides
    double v_req_prev = tcGetPlanMaxTargetVel(prev_tc, 1.0);
    double v_req_this = tcGetPlanMaxTargetVel(tc, 1.0);
    controls->v_req = fmax(v_req_prev, v_req_this);

    // Find the worst-case velocity we should reach for either segment
    controls->v_goal = fmax(tcGetPlanMaxTargetVel(prev_tc, override_allowance),
            tcGetPlanMaxTargetVel(tc, override_allowance));

    return 0;
}

/**
 * Check if the previous line segment will be consumed based on the blend arc parameters.
 * @pre assumes non-null arguments
 */
int blendCheckConsume(
    BlendParameters * const param,
    double L_prev,
    TC_STRUCT const * const prev_tc,
    int gap_cycles)
{
    double prev_seg_time = L_prev / param->v_plan;
    double consume_time = fmax(gap_cycles, 1.0) * prev_tc->cycle_time;

    param->is_consumable = tcCanConsume(prev_tc);
    param->consume = ((prev_seg_time <= consume_time) && param->is_consumable);
    param->line_length = param->consume ? L_prev : 0.0;
    return 0;
}

/** @section spiralfuncs Functions to approximate spiral arc length */

double findTrapezoidalDesiredVel(double a_max,
                                 double dx,
                                 double v_final,
                                 double v_current,
                                 double cycle_time)
{
    double dt = fmax(cycle_time, TP_TIME_EPSILON);
    // Discriminant is 3 terms (when final velocity is non-zero)
    double discr_term1 = pmSq(v_final);
    double discr_term2 = a_max * (2.0 * dx - v_current * dt);
    double tmp_adt = a_max * dt * 0.5;
    double discr_term3 = pmSq(tmp_adt);
    double discr = discr_term1 + discr_term2 + discr_term3;

    //Start with -B/2 portion of quadratic formula
    double maxnewvel = -tmp_adt;

    //If the discriminant term brings our velocity above zero, add it to the total
    //We can ignore the calculation otherwise because negative velocities are clipped to zero
    if (discr > discr_term3) {
        maxnewvel += pmSqrt(discr);
    }

    return maxnewvel;
}

EndCondition checkEndCondition(double cycleTime,
                               double dtg,
                               double currentvel,
                               double v_f,
                               double a_max)
{
    // Start with safe defaults (segment will not end next cycle
    EndCondition out = {
        v_f,
        TP_BIG_NUM * cycleTime,
    };

    double v_avg = (currentvel + v_f) / 2.0;

    //Check that we have a non-zero "average" velocity between now and the
    //finish. If not, it means that we have to accelerate from a stop, which
    //will take longer than the minimum 2 timesteps that each segment takes, so
    //we're safely far form the end.

    //Get dt assuming that we can magically reach the final velocity at
    //the end of the move.
    //
    //KLUDGE: start with a value below the cutoff
    double dt = TP_TIME_EPSILON / 2.0;
    if (v_avg > TP_VEL_EPSILON) {
        //Get dt from distance and velocity (avoid div by zero)
        dt = fmax(dt, dtg / v_avg);
    } else {
        if ( dtg > (v_avg * cycleTime) && dtg > TP_POS_EPSILON) {
            tc_pdebug_print(" below velocity threshold, assuming far from end\n");
            return out;
        }
    }

    // Assuming a full timestep:
    double dv = v_f - currentvel;
    double a_f = dv / fmax(dt, TP_TIME_EPSILON);

    //If this is a valid acceleration, then we're done. If not, then we solve
    //for v_f and dt given the max acceleration allowed.
    //If we exceed the maximum acceleration, then the dt estimate is too small.
    double a = a_f;
    int recalc = sat_inplace(&a, a_max);

    //Need to recalculate vf and above
    if (recalc) {
        double disc = pmSq(currentvel / a) + 2.0 / a * dtg;
        if (disc < 0) {
            //Should mean that dx is too big, i.e. we're not close enough
            tc_pdebug_print(" dx = %f, too large, not at end yet\n",dtg);
            return out;
        }

        if (disc < TP_TIME_EPSILON * TP_TIME_EPSILON) {
            tc_pdebug_print("disc too small, skipping sqrt\n");
            dt =  -currentvel / a;
        } else if (a > 0) {
            tc_pdebug_print("using positive sqrt\n");
            dt = -currentvel / a + pmSqrt(disc);
        } else {
            tc_pdebug_print("using negative sqrt\n");
            dt = -currentvel / a - pmSqrt(disc);
        }

        tc_pdebug_print(" revised dt = %f\n", dt);
        //Update final velocity with actual result
        out.v_f = currentvel + dt * a;
    }

    out.dt = dt;
    return out;
}

double pseudo_sqrt(double x)
{
    const double sqrt_c = sqrt(PSEUDO_SQRT_EPSILON);
    const double den = (sqrt(PSEUDO_SQRT_EPSILON + 1) - sqrt_c);
    const double b0 = -sqrt_c;
    const double b1 = 1;
    return (b1 * pmSqrt(x + PSEUDO_SQRT_EPSILON) + b0) / den;
}

/**
 * Force a value to be within the range specified, with that range shrunk by the relative margin.
 * Example: If rel_margin is 0.1, upper = 2, lower = 1, value must be within [1.05,1.95]
 * @pre result is undefined if rel_margin exceeds [0,1], or lower > upper
 */
double clamp(double upper, double lower, double rel_margin, double value)
{
    double m = (upper - lower) * rel_margin / 2.0;
    double u = upper - m;
    double l = lower + m;
    return fmin(fmax(value, l), u);
}

tp_err_t find_blend_points_and_tangents(
    double Rb,
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    blend_boundary_t * const out)
{
    out->s1 = prev_tc->target - Rb;
    out->s2 = Rb;

    tcGetPosReal(prev_tc, out->s1, &out->P1);
    tcGetPosReal(tc, out->s2, &out->P2);

    tcGetTangentUnitVector(prev_tc, out->s1, &out->u1);
    tcGetTangentUnitVector(tc, out->s2, &out->u2);

    return TP_ERR_OK;
}

int find_blend_intermediate_segments(
    blend_boundary_t const * const blend_params,
    biarc_control_points_t * const control_pts)
{
    // A coefficient
    PmVector du12;
    VecVecAdd(&blend_params->u1, &blend_params->u2, &du12);

    PmVector dP12;
    VecVecSub(&blend_params->P1, &blend_params->P2, &dP12);

    // Find B / C first since A could be zero if u1 / u2 are colinear
    double B = VecVecDot(&dP12, &du12);
    B *= 2.;

    double C =VecMagSq(&dP12);

    double A = VecMagSq(&du12);
    A -= 4;

    // true iff u1 / u2 are nearly parallel or anti-parallel
    if (fabs(A) < 1e-12)
    {
        if (fabs(B) < TP_POS_EPSILON) {
            // u vectors are perpendicular to displacement vector between P1/P2
            double dot = VecVecDot(&blend_params->u1, &blend_params->u2);
            if (dot > 0) {
                // u1 and -u2 are in opposite directions, no biarc solution exists
                return -2;
            } else {
                // u1 and -u2 are in the same direction, assume "rectangular" solution
                control_pts->d = pmSqrt(C)/2.0;
            }
        } else {
            control_pts->d = -C/B;
        }
    } else {
        double other;
        int res_qf = quadraticFormula(A, B, C, &control_pts->d, &other);
        if (res_qf || control_pts->d < 0) {
            return -1;
        }
    }

    VecScalMult(&blend_params->u1, control_pts->d, &control_pts->Pb1);
    VecVecAddEq(&control_pts->Pb1, &blend_params->P1);

    VecScalMult(&blend_params->u2, -control_pts->d, &control_pts->Pb2);
    VecVecAddEq(&control_pts->Pb2, &blend_params->P2);

    VecVecDirection(&control_pts->Pb2, &control_pts->Pb1, &control_pts->u_mid);
    VecScalMult(&control_pts->u_mid, control_pts->d, &control_pts->P_mid);
    VecVecAddEq(&control_pts->P_mid, &control_pts->Pb1);

    if (control_pts->d > 0) {
        return 0;
    } else {
        return -3;
    }
}

static inline double find_max_blend_length(TC_STRUCT const * const tc, double v_goal, bool allow_consumption)
{
    double min_remainder = allow_consumption ? 0.0 : v_goal * tc->cycle_time / 2.0;
    double usable_length = fmax(tc->nominal_length / 2.0 - min_remainder, tc->nominal_length / 3.0);

    if (tc->motion_type == TC_CIRCULAR) {
        // FIXME use arc length fit for spiral?
        double phi_max = PM_PI / 3.0;
        double total_arc_length = pmSqrt(pmSq(tc->coords.circle.xyz.height) + pmSq(tc->coords.circle.fit.total_planar_length));
        double l_from_angle = phi_max / tc->coords.circle.xyz.angle * total_arc_length;
        double l_from_length = fmin(tc->target, usable_length);
        return fmin(l_from_angle, l_from_length);
    } else {
        return fmin(tc->target, usable_length);
    }
}

double find_max_blend_region(TC_STRUCT const * const prev_tc, TC_STRUCT const * const tc, double v_goal)
{
    double l_prev = find_max_blend_length(prev_tc, v_goal, tcCanConsume(prev_tc));
    double l_this = find_max_blend_length(tc, v_goal, true);
    return fmin(l_prev, l_this);
}

tp_err_t find_blend_size_from_intermediates(
    blend_boundary_t const * const blend_boundary,
    biarc_control_points_t const * const intermediates,
    double *R_geom,
    double *arc_len_est)
{
    double dot = VecVecDot(&blend_boundary->u1, &intermediates->u_mid);

    double cos_2theta = fmax(-dot, -1.0 + 1.0e-15);

    double sin_theta_approx = pmSqrt((1. - cos_2theta) / 2);
    double cos_theta_approx = pmSqrt((1. + cos_2theta) / 2);
    double tan_theta_approx = sin_theta_approx / fmax(cos_theta_approx, TP_ANGLE_EPSILON);

    // TODO optimize out the tangent calculation if necessary
    // Find radius from the limiting length (assume length is actually symmetrical
    *R_geom = tan_theta_approx * intermediates->d;
    *arc_len_est = 2.0 * intermediates->d * sin_theta_approx;

    return TP_ERR_OK;
}

static const biarc_solution_t zero_region_result = {0};

static inline double get_effective_range(blend_solver_constraints_t const * const constraints)
{
    return constraints->upper.Rb - constraints->lower.Rb;
}

static inline double guess_interpolated_region_size(
    blend_solver_constraints_t const * const constraints,
    double R_goal,
    double T_goal)
{
    double dRb = constraints->upper.Rb - constraints->lower.Rb;
    double dR_plan = constraints->upper.R_plan - constraints->lower.R_plan;
    double dR_goal = R_goal - constraints->lower.R_plan;
    double Rb_interp_R = constraints->lower.Rb + dRb * (dR_goal / fmax(dR_plan, TP_POS_EPSILON));

    double dT_plan = constraints->upper.T_plan - constraints->lower.T_plan;
    double dT_goal = T_goal - constraints->lower.T_plan;
    double Rb_interp_T = constraints->lower.Rb + dRb * (dT_goal / fmax(dT_plan, TP_POS_EPSILON));

    if (abs(constraints->bias_count) > 3 ) {
        return (constraints->upper.Rb + constraints->lower.Rb) / 2.;
    } else {
        return clamp(constraints->upper.Rb, constraints->lower.Rb, 0.1, fmin(Rb_interp_R, Rb_interp_T));
    }
}


static inline double guess_blend_region_size_slope(
    blend_solver_constraints_t * const constraints,
    biarc_solution_t const * const solution,
    double R_goal,
    double T_goal)
{
    if (solution->R_plan < R_goal && solution->T_plan < T_goal) {
        constraints->lower = *solution;
        constraints->bias_count = constraints->bias_count > 0 ? -1 : constraints->bias_count - 1;
    } else {
        constraints->bias_count = constraints->bias_count < 0 ? 1 : constraints->bias_count + 1;
        constraints->upper = *solution;
    }

    return guess_interpolated_region_size(constraints, R_goal, T_goal);
}

/**
 * Given an overall tolerance limit and the tangent vectors at the
 * intersection, find an approximate blend region size that puts the midpoint
 * of the intermediate line within tolerance of the intersection point.
 *
 * @note this assumes that the areas to blend are straight-ish (i.e. the error
 * introduced by circular segments is minimal). This assumes a linear
 * approximation about the intersection point, but better approximations are
 * possible.
 */
double find_blend_region_from_tolerance(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    double tolerance)
{
    PmVector u_tan1, u_tan2;
    // Start by assuming that the blend tangents will be at least 1 tolereance
    // distance away (should always be true because part of the blend will move
    // along the tangential direction until it hits the intermediate segment).
    double s1 = fmax(0, prev_tc->target - tolerance);
    double s2 = fmin(tc->target / 2.0, tolerance);
    tcGetTangentUnitVector(prev_tc, s1, &u_tan1);
    tcGetTangentUnitVector(tc, s2, &u_tan2);

    double eta1 = 0, eta2 = 0;
    if (prev_tc->motion_type == TC_CIRCULAR) {
        eta1 = (prev_tc->target - s1) / prev_tc->coords.circle.xyz.radius;
    }
    if (tc->motion_type == TC_CIRCULAR) {
        eta2 = s2 / tc->coords.circle.xyz.radius;
    }

    double theta = findIntersectionAngle(&u_tan1, &u_tan2);

    // TODO optimize
    double eta = (eta1 + eta2) / 2.0;
    const double theta2_guess = theta + eta;
    double F = 0, theta2 = 0;
    if (theta2_guess < M_PI_2) {
        // "Normal" case where the effective tangent vectors point towards the intersection
        theta2 = theta2_guess;
        F = (sin(eta) + cos(theta))/(sin(theta2) + 1);
    } else {
        // Flipped case, where tangent vectors point away from the intersection
        // If theta2 > pi/2, then the tolerance region "flips" so that the triangles are opposite
        // Redefine theta2 to have the same sense as theta in the flipped triangle
        theta2 = M_PI - theta2_guess;

        F = sin(theta) / (1 + sin(theta2)) * cos(theta2) + cos(theta);
    }
    double Rb = tolerance / F;
    double Rb_max = sin(theta)/sin(theta2) * Rb;

    return Rb_max;
}

double find_blend_region_from_tolerance_simple(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    double tolerance)
{
    PmVector u_tan1, u_tan2;
    // Start by assuming that the blend tangents will be at least 1 tolereance
    // distance away (should always be true because part of the blend will move
    // along the tangential direction until it hits the intermediate segment).
    tcGetEndTangentUnitVector(prev_tc,  &u_tan1);
    tcGetStartTangentUnitVector(tc, &u_tan2);

    double dot = VecVecDot(&u_tan1, &u_tan2);
    double cos_2theta = -dot;

    // avoids acos, sin, cos, but not sure if this is actually faster
    double sin_theta = pmSqrt((1. - cos_2theta) / 2);
    double cos_theta = pmSqrt((1. + cos_2theta) / 2);

    double d_est = sin_theta / cos_theta * tolerance;

    return tolerance / cos_theta + d_est;
}

int find_blend_vel_accel_planar_limits(
    PmVector const * const u_tan1,
    PmVector const * const u_tan2,
    PmVector const * const acc_bound,
    PmVector const * const vel_bound,
    double *a_max_planar,
    double *v_max_planar)
{
    // Find an orthonormal basis and "envelope" vector for the plane that represents
    // the worst-case contribution of each axis to the overall velocity / acceleration
    PmVector u, v, uv_plane_envelope_sq;
    VecVecOrthonormal(u_tan1, u_tan2, &u, &v);
    VecVecHarmonicAddSq(&u, &v, &uv_plane_envelope_sq);

    // Reuse the envelope vector (this is why accel and vel are calculated together)
    int res_dia = findMaxValueOnPlane(&uv_plane_envelope_sq, acc_bound, a_max_planar);
    res_dia |= findMaxValueOnPlane(&uv_plane_envelope_sq, vel_bound, v_max_planar);

    return res_dia;
}

void find_biarc_solution_error(
    double blend_tolerance,
    double R_goal,
    double R_plan,
    double T_plan,
    biarc_solution_errors_t * const err)
{
    err->deviation = blend_tolerance - T_plan;

    // Check for solution convergence based on specified tolerances
    err->radius_rel = (R_plan - R_goal + TP_POS_EPSILON) / (R_goal + TP_POS_EPSILON);
    err->radius_abs = R_plan - R_goal;
}

/**
 * Find the optimal Rb to get the smallest blend that can meet the feed rate requirements.
 * Choose initial Rb = Rb_max
 * Find blend start / end points and tangent vectors, within Rb of intersection P with procedure find_blend_points_and_tangents
 * Find blend segment lengths d, L and intermediate points with procedure find_blend_intermediate_segments
 * Calculate blend kinematics for the two blends.
 * Find blend radii and arc lengths given P1, P2, Pb1, Pb2
 * Compute max velocity on blend arcs from geometry
 * Check blend performance:
 * Verify distance from blend midpoint to original intersection P < tolerance
 * Check if requested velocity is reached (if not, need bigger arc)
 * Check if blend radii are larger than they need to be (e.g. worst-case normal acceleration < a_t_max)
 * Refine solution as necessary:
 * Guess a new Rb based on heuristic function of Rb, Rb_max, v_plan with procedure guess_blend_region_size
 * Stop after N iterations or until Rb converges
 */
tp_err_t optimize_biarc_blend_size(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    biarc_solver_config_t const * const config,
    PmVector const * const vel_bound,
    PmVector const * const acc_bound,
    biarc_solver_results_t * const biarc_results,
    BlendControls * const controls,
    BlendParameters * const param,
    double cycle_time)
{
    if (!prev_tc || !tc || !biarc_results) {
        return TP_ERR_MISSING_INPUT;
    }
    blendParamInitVelocities(prev_tc, tc, config->feed_override_allowance, controls);

    // Find net tolerance that's actually usable (so that we're
    // not passing 0.0 around if tolerance is not specified)
    tcFindBlendTolerance(prev_tc, tc, &controls->tolerance);

    //double Rb_init = find_blend_region_from_tolerance(prev_tc, tc, param->tolerance);
    double Rb_init = find_max_blend_region(prev_tc, tc, controls->v_goal);

    // Initial solver values for Rb range using somewhat dumb (but cheap) values
    biarc_results->solution.Rb = biarc_results->constraints.upper.Rb = Rb_init;
    biarc_results->constraints.upper.R_plan = 0; // To be filled in at first pass through solver
    biarc_results->constraints.lower = zero_region_result;

    const unsigned MAX_ITERATIONS = config->max_iterations;

    double s1_max = prev_tc->target - biarc_results->constraints.upper.Rb;
    double s2_max = biarc_results->constraints.upper.Rb;
    PmVector u1, u2;
    tcGetTangentUnitVector(prev_tc, s1_max, &u1);
    tcGetTangentUnitVector(tc, s2_max, &u2);

    // Approximate the blend kinematics using the initial guess
    // Actual limits will be enforced later
    CHP(find_blend_parameters(
        &u1,
        &u2,
        acc_bound,
        vel_bound,
        controls,
        param
        ));

    double v_max = fmax(param->v_max_planar, param->v_max_altitude);
    param->v_plan = fmin(controls->v_goal, v_max);

    double R_goal = pmSq(controls->v_goal) / param->a_n_max;
    // Scale up solver tolerance to have the same net effect on velocity
    const double solver_radius_rel_tol = config->velocity_rel_tolerance * R_goal / param->v_plan;
    const double solver_radius_abs_tol = pmSq(config->velocity_abs_tolerance) / param->a_n_max; // 2% is pretty good

    // Intersection is at start of next segment, so use startpoint method since it's much faster
    PmVector P;
    CHP(tcGetStartpoint(tc, &P));

    for (biarc_results->iterations = 1; biarc_results->iterations < MAX_ITERATIONS; ++biarc_results->iterations)
    {
        // Find blend start / end points and tangent vectors, within Rb of intersection P with procedure find_blend_points_and_tangents
        find_blend_points_and_tangents(biarc_results->solution.Rb, prev_tc, tc, &biarc_results->solution.boundary);
        // Find blend segment lengths d, L and intermediate points with procedure find_blend_intermediate_segments
        if (TP_ERR_OK != find_blend_intermediate_segments(&biarc_results->solution.boundary, &biarc_results->solution.control_pts))
        {
            biarc_results->result = BIARC_DEGENERATE_SEGMENTS;
            return TP_ERR_FAIL;
        }

        // Verify distance from blend midpoint to original intersection P < tolerance
        biarc_results->solution.T_plan = VecVecDisp(&biarc_results->solution.control_pts.P_mid, &P);

        CHP(find_blend_size_from_intermediates(
            &biarc_results->solution.boundary,
            &biarc_results->solution.control_pts,
            &biarc_results->solution.R_geom,
            &biarc_results->solution.arc_len_est
            ));

        // Cheat by expressing max radius in terms of v_max even though it's not strictly the limiting factor
        double guess_v_max_length = biarc_results->solution.arc_len_est / cycle_time;
        double guess_R_effective_max = pmSq(guess_v_max_length) / param->a_n_max;

        // THis clamping throws off the slope-based update. Need to do slopes based on R_geom, or somehow compensate for it
        biarc_results->solution.R_plan = fmin(biarc_results->solution.R_geom, guess_R_effective_max);
        // Check for solution convergence based on specified tolerances
        find_biarc_solution_error(
            controls->tolerance,
            R_goal,
            biarc_results->solution.R_plan,
            biarc_results->solution.T_plan,
            &biarc_results->solution.err);

        // Need an ok tolerance or we're not done
        if (biarc_results->solution.err.deviation >= -config->deviation_abs_tolerance) {
            // Check if we're close to the goal "radius", i.e. we've met the blend target velocity limit
            if ((fabs(biarc_results->solution.err.radius_rel) < solver_radius_rel_tol
                 || fabs(biarc_results->solution.err.radius_abs) < solver_radius_abs_tol))
            {
                biarc_results->result = BIARC_REACHED_V_GOAL;
                return TP_ERR_OK;
            } else if (biarc_results->solution.err.radius_rel < 0.0 && biarc_results->solution.err.deviation < config->deviation_abs_tolerance) {
                biarc_results->result = BIARC_REACHED_TOLERANCE;
                return TP_ERR_OK;
            } else if (get_effective_range(&biarc_results->constraints) < config->convergence_tolerance) {
                biarc_results->result = BIARC_CONVERGED;
                return TP_ERR_OK;
            }
        } else {
            double R_bound = fmax(biarc_results->constraints.upper.R_plan, biarc_results->constraints.lower.R_plan);
            double v_bound = pmSqrt(R_bound * param->a_n_max);
            if (biarc_results->iterations > 1 && v_bound < config->velocity_cutoff) {
                biarc_results->result = BIARC_MIN_LENGTH_LIMITED;
                return TP_ERR_FAIL;
            }
        }
        // Refine Rb if solution has not converged
        biarc_results->solution.Rb = guess_blend_region_size_slope(
            &biarc_results->constraints,
            &biarc_results->solution,
            R_goal,
            controls->tolerance);
    }

    // Ran out of iterations, try the best solution we have

    biarc_results->solution = biarc_results->constraints.lower;

    if (biarc_results->solution.err.deviation >= -config->deviation_abs_tolerance) {
        biarc_results->result = BIARC_EXCEEDED_MAX_ITERATIONS;
        return TP_ERR_OK;
    } else {
        biarc_results->result = BIARC_FAILED_TO_CONVERGE;
        return TP_ERR_FAIL;
    }
}

#ifdef UNIT_TEST
#include <stdio.h>
#endif
#ifdef DO_BIARC_SCAN
tp_err_t scan_blend_properties(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    biarc_solver_config_t const * const config,
    PmVector const * const vel_bound,
    PmVector const * const acc_bound,
    biarc_solver_results_t * const biarc_results,
    BlendParameters * const param,
    double cycle_time,
    double resolution
    )
{
    if (!prev_tc || !tc || !biarc_results) {
        return TP_ERR_MISSING_INPUT;
    }

    // Find net tolerance that's actually usable (so that we're
    // not passing 0.0 around if tolerance is not specified)
    tcFindBlendTolerance(prev_tc, tc, &param->tolerance);

    blend_solver_constraints_t constraints;

    // Initial solver values for Rb range using somewhat dumb (but cheap) values
    constraints.upper = find_max_blend_region(prev_tc, tc);
    constraints.tolerance_upper = constraints.upper;

    blendParamInitVelocities(prev_tc, tc, config->feed_override_allowance, param);

    double s1_max = prev_tc->target - constraints.tolerance_upper;
    double s2_max = constraints.tolerance_upper;
    PmVector u1, u2;
    tcGetTangentUnitVector(prev_tc, s1_max, &u1);
    tcGetTangentUnitVector(tc, s2_max, &u2);

    findVMaxByAltitude(
        &u1,
        &u2,
        param);
    //TODO do altitude calculation too?
    // Approximate the blend kinematics using the initial guess
    // Actual limits will be enforced later
    CHP(blendParamKinematics(
        &u1,
        &u2,
        param,
        acc_bound,
        vel_bound));

    // Intersection is at start of next segment, so use startpoint method since it's much faster
    EmcPose P;
    CHP(tcGetStartpoint(tc, &P));

#ifdef UNIT_TEST
    printf(">a_n_max %f\n", param->a_n_max);
    printf("|f,%12s,%12s,%12s,%12s,%12s,%12s\n",
           "Rb",
           "tol_dev",
           "R_geom",
           "R_plan",
           "v_plan",
           "d");
#endif
    const unsigned MAX_ITERATIONS = (unsigned)((double)constraints.upper.Rb / resolution);
    for (biarc_results->iterations = 1; biarc_results->iterations < MAX_ITERATIONS; ++biarc_results->iterations)
    {
        biarc_results->Rb = (double)biarc_results->iterations * resolution;
        // Find blend start / end points and tangent vectors, within Rb of intersection P with procedure find_blend_points_and_tangents
        find_blend_points_and_tangents(biarc_results->Rb, prev_tc, tc, &biarc_results->boundary);
        // Find blend segment lengths d, L and intermediate points with procedure find_blend_intermediate_segments
        int failed = 0;
        double T;
        if (TP_ERR_OK != find_blend_intermediate_segments(&biarc_results->boundary, &biarc_results->control_pts))
        {
            failed = 1;
            T=-1;
        } else {

        // Verify distance from blend midpoint to original intersection P < tolerance
        VecVecDisp(&biarc_results->control_pts.P_mid, &P.tran, &T);

        CHP(find_blend_size_from_intermediates(
            &biarc_results->boundary,
            &biarc_results->control_pts,
            &biarc_results->R_geom,
            &biarc_results->arc_len_est));

        // Cheat by expressing max radius in terms of v_max even though it's not strictly the limiting factor
        double guess_v_max_length = biarc_results->arc_len_est / cycle_time;
        double guess_R_effective_max = pmSq(guess_v_max_length) / param->a_n_max;

        biarc_results->R_plan = fmin(biarc_results->R_geom, guess_R_effective_max);
        }
#ifdef UNIT_TEST
            printf("|%d,%12f,%12f,%12f,%12f,%12f,%12f\n",
               failed,
               biarc_results->Rb,
               T,
               biarc_results->R_geom,
               biarc_results->R_plan,
               pmSqrt(param->a_n_max * biarc_results->R_plan),
               biarc_results->control_pts.d);
#endif
    }
    biarc_results->result = BIARC_EXCEEDED_MAX_ITERATIONS;
    return TP_ERR_FAIL;
}
#endif

const char * biarc_result_to_str(BiarcSolverResults r)
{
    switch (r) {
    case BIARC_DEGENERATE_SEGMENTS:
        return "degenerate";
    case BIARC_FAILED_TO_CONVERGE:
        return "diverged";
    case BIARC_EXCEEDED_MAX_ITERATIONS:
        return "max_iterations";
    case BIARC_NOT_SOLVED:
        return "not_solved";
    case BIARC_REACHED_V_GOAL:
        return "reached_v_goal";
    case BIARC_REACHED_TOLERANCE:
        return "reached_tol";
    case BIARC_CONVERGED:
        return "converged";
    case BIARC_MIN_LENGTH_LIMITED:
        return "hit_min_len";
    }
    return "";
}

int find_biarc_points_from_solution(
    PmVector const * const u1,
    PmVector const * const u2,
    PmVector const * const P1,
    PmVector const * const P2,
    PmVector const * const P,
    double d,
    PmVector const * const acc_bound,
    PmVector const * const vel_bound,
    BlendControls const * const controls,
    BlendPoints * const points,
    BlendParameters *const param)
{
    CHP(find_blend_parameters(
        u1,
        u2,
        acc_bound,
        vel_bound,
        controls,
        param));

    param->theta = findIntersectionAngle(
        u1,
        u2);

    double straightness_margin = M_PI_2 - param->theta;

    if (fabs(straightness_margin) < 1e-6) {
        points->motion_type = TC_LINEAR;
        double v_max = fmax(param->v_max_planar, param->v_max_altitude);
        param->v_plan = fmin(controls->v_goal, v_max);
    } else {
        points->motion_type = TC_SPHERICAL;
        PmVector u_normal;
        VecVecSub(
            u2,
            u1,
            &u_normal);
        VecUnitEq(&u_normal);

        double center_dist = d / cos(param->theta);
        param->R_plan = d * tan(param->theta);
        param->v_plan = pmSqrt(param->R_plan * param->a_n_max);

        VecScalMult(&u_normal, center_dist, &points->arc_center);
        VecVecAddEq(&points->arc_center, P);
    }

    points->arc_start = *P1;
    points->arc_end = *P2;
    points->u_tan = *u1;

    return 0;
}

ContinuityCheck calc_C1_continuity(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const next_tc)
{
    ContinuityCheck out = {};
    PmVector u_1, u_2;
    tcGetEndTangentUnitVector(prev_tc, &u_1);
    tcGetStartTangentUnitVector(next_tc, &u_2);
    VecVecSub(&u_2, &u_1, &out.u_diff);
    out.dot = VecVecDot(&u_1, &u_2);

    PmVector p_1, p_2;
    tcGetEndpoint(prev_tc, &p_1);
    tcGetStartpoint(next_tc, &p_2);

    VecVecSub(&p_2, &p_1, &out.p_diff);

    return out;
}
