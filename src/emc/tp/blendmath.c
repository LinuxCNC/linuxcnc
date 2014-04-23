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

/** @section utilityfuncs Utility functions */

/**
 * Find the maximum angle allowed between "tangent" segments.
 * @param v speed of motion in worst case (i.e. at max feed).
 * @param acc magnitude of acceleration allowed during "kink".
 *
 * Since we are discretized by a timestep, the maximum allowable
 * "kink" in a trajectory is bounded by normal acceleration. A small
 * kink will effectively be one step along the tightest radius arc
 * possible at a given speed.
 */
double findMaxTangentAngle(double v_plan, double acc_limit, double cycle_time)
{
    //Find acc hiccup we're allowed to get
    double acc_margin = BLEND_ACC_RATIO_NORMAL * BLEND_KINK_FACTOR * acc_limit;
    double dx = v_plan / cycle_time;
    if (dx > 0.0) {
        return (acc_margin / dx);
    } else {
        tp_debug_print(" Velocity or period is negative!\n");
        //Should not happen...
        return TP_ANGLE_EPSILON;
    }
}

double findKinkAccel(double kink_angle, double v_plan, double cycle_time)
{
    double dx = v_plan / cycle_time;
    if (dx > 0.0) {
        return (dx * kink_angle);
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "dx < 0 in KinkAccel\n");
        return 0;
    }
}


double fsign(double f)
{
    if (f>0) {
        return 1.0;
    } else if (f < 0) {
        return -1.0;
    } else {
        //Technically this should be NAN but that's a useless result for tp purposes
        return 0;
    }
}

/** negate a value (or not) based on a bool parameter */
inline double negate(double f, int neg)
{
    return (neg) ? -f : f;
}

/** Clip the input at the specified minimum (in place). */
int clip_min(double * const x, double min) {
    if ( *x < min ) {
        *x = min;
        return 1;
    }
    return 0;
}


/** Clip the input at the specified maximum (in place). */
int clip_max(double * const x, double max) {
    if ( *x > max ) {
        *x = max;
        return 1;
    }
    return 0;
}

/**
 * Saturate a value x to be within +/- max.
 */
double saturate(double x, double max) {
    if ( x > max ) {
        return max;
    }
    else if ( x < (-max) ) {
        return -max;
    }
    else {
        return x;
    }
}


/** In-place saturation function */
int sat_inplace(double * const x, double max) {
    if ( *x > max ) {
        *x = max;
        return 1;
    }
    else if ( *x < -max ) {
        *x = -max;
        return -1;
    }
    return 0;
}

#if 0
static int pmCirclePrint(PmCircle const * const circ) {
    tp_debug_print(" center = %f %f %f\n",
            circ->center.x,
            circ->center.y,
            circ->center.z);
    tp_debug_print(" radius = %f\n", circ->radius);
    tp_debug_print(" spiral = %f\n", circ->spiral);
    tp_debug_print(" angle = %f\n", circ->angle);
    //TODO add other debug data here as needed
    return TP_ERR_OK;
}
#endif

/**
 * @section geomfuncs Geometry check functions
 */


/**
 * Calculate the best-fit circle to the spiral segment.
 * @param circ spiral to be approximated
 * @param base_pt the point about which the circle is fit
 * @param u_tan tangent unit vector at the base point of the approximation
 * @param[out] center_out displaced center for circular approximation
 * @param[out] radius_out adjusted radius
 *
 * The adjusted center for the circle fit is found by displacing the center
 * along the spiral tangent by the spiral coefficient. The adjusted radius is
 * the distance between the base point and this new center.
 *
 */
static inline int findSpiralApproximation(PmCircle const * const circ,
        PmCartesian const * const base_pt,
        PmCartesian const * const u_tan,
        PmCartesian * const center_out,
        double * const radius_out)
{
    double dr = circ->spiral / circ->angle;
    pmCartScalMult(u_tan, dr, center_out);
    pmCartCartAddEq(center_out, &circ->center);

    PmCartesian r_adjust;
    pmCartCartSub(base_pt, center_out, &r_adjust);
    pmCartMag(&r_adjust, radius_out);

    tp_debug_print(" adjusted center = %f %f %f\n",
            center_out->x,
            center_out->y,
            center_out->z);

    tp_debug_print(" adjusted radius = %f\n", *radius_out);

    return TP_ERR_OK;
}


/**
 * Calculate the angle to trim from a circle based on the blend geometry.
 *
 * @param P intersection point
 * @param arc_center calculated center of blend arc
 * @param center actual center of circle (not spiral approximated center)
 * @return trim angle
 */
static inline double findTrimAngle(PmCartesian const * const P,
        PmCartesian const * const arc_center,
        PmCartesian const * const center)
{
    //Define vectors relative to circle center
    PmCartesian u_P;
    pmCartCartSub(P, center, &u_P);
    pmCartUnitEq(&u_P);

    PmCartesian u_arccenter;
    pmCartCartSub(arc_center, center, &u_arccenter);
    pmCartUnitEq(&u_arccenter);

    double dot;
    pmCartCartDot(&u_arccenter, &u_P, &dot);
    double dphi = acos(saturate(dot,1.0));
    tp_debug_print(" dphi = %g\n",dphi);
    return dphi;
}


int checkTangentAngle(PmCircle const * const circ, SphericalArc const * const arc, BlendGeom3 const * const geom, BlendParameters const * const param, double cycle_time, int at_end)
{
    // Debug Information to diagnose tangent issues
    PmCartesian u_circ, u_arc;
    arcTangent(arc, &u_arc, at_end);

    if (at_end) {
        pmCircleTangentVector(circ, 0, &u_circ);
    } else {
        pmCircleTangentVector(circ, circ->angle, &u_circ);
    }

    pmCartUnitEq(&u_arc);

    //TODO fail if theta is too large
    double dot;
    pmCartCartDot(&u_circ, &u_arc, &dot);
    double blend_angle = acos(saturate(dot,1.0));
    double angle_max = findMaxTangentAngle(param->v_plan, param->a_max, cycle_time);

    tp_debug_print("tangent angle = %f, max = %f\n",
            blend_angle,
            angle_max);

    tp_debug_print("circ_tan = [%g %g %g]\n",
            u_circ.x,
            u_circ.y,
            u_circ.z);
    tp_debug_print("arc_tan = [%g %g %g]\n",
            u_arc.x,
            u_arc.y,
            u_arc.z);

    PmCartesian diff;
    pmCartCartSub(&u_arc,&u_circ,&diff);
    tp_debug_print("diff = [%g %g %g]\n",
            diff.x,
            diff.y,
            diff.z);

    if (blend_angle > angle_max) {
        tp_debug_print("angle too large\n");
        return TP_ERR_FAIL;
    }

    return TP_ERR_OK;
}



/**
 * Check if two cartesian vectors are parallel.
 * The input tolerance specifies what the maximum angle between the
 * lines containing two vectors is. Note that vectors pointing in
 * opposite directions are still considered parallel, since their
 * containing lines are parallel.
 * @param v1 input vector 1
 * @param v2 input vector 2
 * @param tol angle tolerance for parallelism
 */
int pmCartCartParallel(PmCartesian const * const v1,
        PmCartesian const * const v2, double tol)
{
    PmCartesian u1,u2;
    pmCartUnit(v1,&u1);
    pmCartUnit(v2,&u2);
    double dot;
    pmCartCartDot(&u1, &u2, &dot);
    double theta = acos(fabs(dot));
    if (theta < tol) {
        return 1;
    } else {
        return 0;
    }
}


/**
 * Check if a Circle and line are coplanar.
 *
 * @param circ PmCircle input
 * @param line PmCartLine input
 * @param tol deviation tolerance (magnitude of error component)
 */
int pmCircLineCoplanar(PmCircle const * const circ,
        PmCartLine const * const line, double tol)
{
    double dot;
    pmCartCartDot(&circ->normal, &line->uVec, &dot);
    if (fabs(dot) < tol) {
        return 1;
    } else {
        return 0;
    }
}


/**
 * Somewhat redundant function to calculate the segment intersection angle.
 * The intersection angle is half of the supplement of the "divergence" angle
 * between unit vectors. If two unit vectors are pointing in the same
 * direction, then the intersection angle is PI/2. This is based on the
 * simple_tp formulation for tolerances.
 */
int findIntersectionAngle(PmCartesian const * const u1,
        PmCartesian const * const u2, double * const theta)
{
    double dot;
    pmCartCartDot(u1, u2, &dot);

    /*tp_debug_print("u1 = %f %f %f u2 = %f %f %f\n", u1->x, u1->y, u1->z, u2->x, u2->y, u2->z);*/

    if (dot > 1.0 || dot < -1.0) {
        tp_debug_print("dot product %f outside domain of acos!\n",dot);
        sat_inplace(&dot,1.0);
    }

    *theta = acos(-dot)/2.0;
    return TP_ERR_OK;
}


/** Calculate the minimum of the three values in a PmCartesian. */
double pmCartMin(PmCartesian const * const in)
{
    return fmin(fmin(in->x,in->y),in->z);
}


/**
 * Calculate the diameter of a circle incscribed on a central cross section of a 3D
 * rectangular prism.
 *
 * @param normal normal direction of plane slicing prism.
 * @param extents distance from center to one corner of the prism.
 * @param diameter diameter of inscribed circle on cross section.
 *
 */
int calculateInscribedDiameter(PmCartesian const * const normal,
        PmCartesian const * const bounds, double * const diameter)
{
    if (!normal ) {
        return TP_ERR_MISSING_INPUT;
    }

    PmCartesian planar_x,planar_y,planar_z;

    //Find perpendicular component of unit directions
    // FIXME Assumes normal is unit length
    // FIXME use plane project?
    pmCartScalMult(normal, -normal->x, &planar_x);
    pmCartScalMult(normal, -normal->y, &planar_y);
    pmCartScalMult(normal, -normal->z, &planar_z);

    planar_x.x+=1.0;
    planar_y.y+=1.0;
    planar_z.z+=1.0;

    pmCartAbs(&planar_x, &planar_x);
    pmCartAbs(&planar_y, &planar_y);
    pmCartAbs(&planar_z, &planar_z);

    PmCartesian planar_scales;
    pmCartMag(&planar_x, &planar_scales.x);
    pmCartMag(&planar_y, &planar_scales.y);
    pmCartMag(&planar_z, &planar_scales.z);

    PmCartesian extents;
    pmCartCartDiv(bounds, &planar_scales, &extents);

    *diameter = pmCartMin(&extents);
    return TP_ERR_OK;
}


/** Find real roots of a quadratic equation in standard form. */
int quadraticFormula(double A, double B, double C, double * const root0,
        double * const root1)
{
    double disc = pmSq(B) - 4.0 * A * C;
    if (disc < 0) {
        tp_debug_print("discriminant < 0\n");
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

/**
 * Setup common geom parameters based on trajectory segments.
 * This function populates the geom structure and "input" fields of
 * the blend parameter structure. It returns an error if the segments
 * are not coplanar, or if one or both segments is not a circular arc.
 *
 * @param geom Stores simplified geometry used to calculate blend params.
 * @param prev_tc first linear move to blend
 * @param tc second linear move to blend
 */
int blendGeom3Init(BlendGeom3 * const geom,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc)
{
    geom->v_max1 = prev_tc->maxvel;
    geom->v_max2 = tc->maxvel;

    // Get tangent unit vectors to each arc at the intersection point
    int res_u1 = tcGetEndTangentUnitVector(prev_tc, &geom->u_tan1);
    int res_u2 = tcGetStartTangentUnitVector(tc, &geom->u_tan2);

    // Initialize u1 and u2 by assuming they match the tangent direction
    geom->u1 = geom->u_tan1;
    geom->u2 = geom->u_tan2;

    int res_intersect = tcGetIntersectionPoint(prev_tc, tc, &geom->P);

    tp_debug_print("Intersection point P = %f %f %f\n",
            geom->P.x,
            geom->P.y,
            geom->P.z);

    // Find angle between tangent vectors
    int res_angle = findIntersectionAngle(&geom->u_tan1,
            &geom->u_tan2,
            &geom->theta_tan);

    blendCalculateNormals3(geom);

    return res_u1 |
        res_u2 |
        res_intersect |
        res_angle;
}


/**
 * Initialize common fields in parameters structure.
 *
 * @param geom Stores simplified geometry used to calculate blend params.
 * @param param Abstracted parameters for blending calculations
 * @param acc_bound maximum X, Y, Z machine acceleration
 * @param vel_bound maximum X, Y, Z machine velocity
 * @param maxFeedScale maximum allowed feed override (set in INI)
 */
int blendParamKinematics(BlendGeom3 * const geom,
        BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{

    // KLUDGE: common operations, but not exactly kinematics
    param->phi = (PM_PI - param->theta * 2.0);

    double nominal_tolerance;
    tcFindBlendTolerance(prev_tc, tc, &param->tolerance, &nominal_tolerance);

    // Calculate max acceleration based on plane containing lines
    int res_dia = calculateInscribedDiameter(&geom->binormal, acc_bound, &param->a_max);

    // Store max normal acceleration
    param->a_n_max = param->a_max * BLEND_ACC_RATIO_NORMAL;
    tp_debug_print("a_max = %f, a_n_max = %f\n", param->a_max,
            param->a_n_max);

    // Find common velocity and acceleration
    param->v_req = fmax(prev_tc->reqvel, tc->reqvel);
    param->v_goal = param->v_req * maxFeedScale;

    // Calculate the maximum planar velocity
    double v_planar_max;
    calculateInscribedDiameter(&geom->binormal, vel_bound, &v_planar_max);
    tp_debug_print("v_planar_max = %f\n", v_planar_max);

    // Clip the angle at a reasonable value (less than 90 deg), to prevent div by zero
    double phi_effective = fmin(param->phi, PM_PI * 0.49);

    // Copy over maximum velocities, clipping velocity to place altitude within base
    double v_max1 = fmin(prev_tc->maxvel, tc->maxvel / cos(phi_effective));
    double v_max2 = fmin(tc->maxvel, prev_tc->maxvel / cos(phi_effective));

    tp_debug_print("v_max1 = %f, v_max2 = %f\n", v_max1, v_max2);

    // Get "altitude"
    double v_area = v_max1 * v_max2 / 2.0 * sin(param->phi);
    tp_debug_print("phi = %f\n", param->phi);
    tp_debug_print("v_area = %f\n", v_area);

    // Get "base" of triangle
    PmCartesian tmp1, tmp2, diff;
    pmCartScalMult(&geom->u1, v_max1, &tmp1);
    pmCartScalMult(&geom->u2, v_max2, &tmp2);
    pmCartCartSub(&tmp2, &tmp1, &diff);
    double base;
    pmCartMag(&diff, &base);
    tp_debug_print("v_base = %f\n", base);

    double v_max_alt = 2.0 * v_area / base;

    // Can't do altitude-based velocity calculation if we have arcs
    if (prev_tc->motion_type != TC_LINEAR || tc->motion_type != TC_LINEAR) {
        v_max_alt = 0.0;
    }

    tp_debug_print("v_max_alt = %f\n", v_max_alt);
    double v_max = fmax(v_max_alt, v_planar_max);

    tp_debug_print("v_max = %f\n", v_max);
    param->v_goal = fmin(param->v_goal, v_max);

    tp_debug_print("vr1 = %f, vr2 = %f\n", prev_tc->reqvel, tc->reqvel);
    tp_debug_print("v_goal = %f, max scale = %f\n", param->v_goal, maxFeedScale);

    return res_dia;
}

/**
 * Setup blend parameters based on a line and an arc.
 * This function populates the geom structure and "input" fields of
 * the blend parameter structure. It returns an error if the segments
 * are not coplanar, or if one or both segments is not a circular arc.
 *
 * @param geom Stores simplified geometry used to calculate blend params.
 * @param param Abstracted parameters for blending calculations
 * @param prev_tc first linear move to blend
 * @param tc second linear move to blend
 * @param acc_bound maximum X, Y, Z machine acceleration
 * @param vel_bound maximum X, Y, Z machine velocity
 * @param maxFeedScale maximum allowed feed override (set in INI)
 */
int blendInit3FromLineArc(BlendGeom3 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{

    if (tc->motion_type != TC_CIRCULAR || prev_tc->motion_type != TC_LINEAR) {
        return TP_ERR_FAIL;
    }

    int res_init = blendGeom3Init(geom, prev_tc, tc);
    if (res_init != TP_ERR_OK) {
        return res_init;
    }

    // Handle convexity
    param->convex2 = arcConvexTest(&tc->coords.circle.xyz.center, &geom->P, &geom->u_tan1, true);
    tp_debug_print("circ2 convex: %d\n",
            param->convex2);

    //Identify max angle for first arc by blend limits
    // TODO better name?
    double blend_angle_2 = param->convex2 ? geom->theta_tan : PM_PI / 2.0;

    param->phi2_max = fmin(tc->coords.circle.xyz.angle / 3.0, blend_angle_2);
    param->theta = geom->theta_tan;

    if (param->convex2) {
        PmCartesian blend_point;
        pmCirclePoint(&tc->coords.circle.xyz,
                param->phi2_max / 2.0,
                &blend_point);
        //Create new unit vector based on secant line
        // Direction is away from P (at start of segment)
        pmCartCartSub(&blend_point, &geom->P,  &geom->u2);
        pmCartUnitEq(&geom->u2);
        //Reduce theta proportionally to the angle between the secant and the normal
        param->theta = fmin(param->theta, geom->theta_tan - param->phi2_max / 4.0);
    }

    tp_debug_print("phi2_max = %f\n", param->phi2_max);
    blendGeom3Print(geom);

    // Check that we're not below the minimum intersection angle (making too tight an arc)
    // FIXME make this an INI setting?
    const double theta_min = PM_PI / 6.0;
    if (param->theta < theta_min) {
        tp_debug_print("theta = %f < min %f, aborting arc...\n",
                param->theta,
                theta_min);
    }

    tp_debug_print("theta = %f\n", param->theta);

    param->phi = (PM_PI - param->theta * 2.0);

    param->L1 = fmin(prev_tc->target, prev_tc->nominal_length / 2.0);

    if (param->convex2) {
        //use half of the length of the chord
        param->L2 = sin(param->phi2_max/4.0) * tc->coords.circle.xyz.radius;
    } else {
        param->L2 = param->phi2_max * tc->coords.circle.xyz.radius;
    }

    tp_debug_print("L1 = %f, L2 = %f\n", param->L1, param->L2);

    // Setup common parameters
    blendParamKinematics(geom,
            param,
            prev_tc,
            tc,
            acc_bound,
            vel_bound,
            maxFeedScale);

    return TP_ERR_OK;
}

int blendInit3FromArcLine(BlendGeom3 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{

    if (tc->motion_type != TC_LINEAR || prev_tc->motion_type != TC_CIRCULAR) {
        return TP_ERR_FAIL;
    }

    int res_init = blendGeom3Init(geom, prev_tc, tc);
    if (res_init != TP_ERR_OK) {
        return res_init;
    }

    param->convex1 = arcConvexTest(&prev_tc->coords.circle.xyz.center, &geom->P, &geom->u_tan2, false);
    tp_debug_print("circ1 convex: %d\n",
            param->convex1);

    //Identify max angle for first arc by blend limits
    // TODO better name?
    double blend_angle_1 = param->convex1 ? geom->theta_tan : PM_PI / 2.0;

    param->phi1_max = fmin(prev_tc->coords.circle.xyz.angle * 2.0 / 3.0, blend_angle_1);
    param->theta = geom->theta_tan;

    // Build the correct unit vector for the linear approximation
    if (param->convex1) {
        PmCartesian blend_point;
        pmCirclePoint(&prev_tc->coords.circle.xyz,
                prev_tc->coords.circle.xyz.angle - param->phi1_max / 2.0 ,
                &blend_point);
        //Create new unit vector based on secant line
        // Direction is toward P (at end of segment)
        pmCartCartSub(&geom->P, &blend_point, &geom->u1);
        pmCartUnitEq(&geom->u1);

        //Reduce theta proportionally to the angle between the secant and the normal
        param->theta = fmin(param->theta, geom->theta_tan - param->phi1_max / 4.0);
    }

    blendGeom3Print(geom);
    tp_debug_print("phi1_max = %f\n", param->phi1_max);

    // Check that we're not below the minimum intersection angle (making too tight an arc)
    // FIXME make this an INI setting?
    const double theta_min = PM_PI / 6.0;
    if (param->theta < theta_min) {
        tp_debug_print("theta = %f < min %f, aborting arc...\n",
                param->theta,
                theta_min);
    }

    tp_debug_print("theta = %f\n", param->theta);


    param->L1 = param->phi1_max * prev_tc->coords.circle.xyz.radius;
    param->L2 = tc->nominal_length / 2.0;

    if (param->convex1) {
        //use half of the length of the chord
        param->L1 = sin(param->phi1_max/4.0) * prev_tc->coords.circle.xyz.radius;
    }
    tp_debug_print("L1 = %f, L2 = %f\n", param->L1, param->L2);

    // Setup common parameters
    blendParamKinematics(geom,
            param,
            prev_tc,
            tc,
            acc_bound,
            vel_bound,
            maxFeedScale);

    return TP_ERR_OK;
}


/**
 * Setup blend paramaters based on two circular arc segments.
 * This function populates the geom structure and "input" fields of
 * the blend parameter structure. It returns an error if the segments
 * are not coplanar, or if one or both segments is not a circular arc.
 *
 * @param geom Stores simplified geometry used to calculate blend params.
 * @param param Abstracted parameters for blending calculations
 * @param prev_tc first linear move to blend
 * @param tc second linear move to blend
 * @param acc_bound maximum X, Y, Z machine acceleration
 * @param vel_bound maximum X, Y, Z machine velocity
 * @param maxFeedScale maximum allowed feed override (set in INI)
 */
int blendInit3FromArcArc(BlendGeom3 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{
    if (tc->motion_type != TC_CIRCULAR || prev_tc->motion_type != TC_CIRCULAR) {
        return TP_ERR_FAIL;
    }

    int res_init = blendGeom3Init(geom, prev_tc, tc);
    if (res_init != TP_ERR_OK) {
        return res_init;
    }

    //Do normal calculation here since we need this information for accel / vel limits
    blendCalculateNormals3(geom);

    // Get intersection point from circle start
    pmCirclePoint(&tc->coords.circle.xyz, 0.0, &geom->P);
    tp_debug_print("Intersection point P = %f %f %f\n",
            geom->P.x,
            geom->P.y,
            geom->P.z);

    param->convex1 = arcConvexTest(&prev_tc->coords.circle.xyz.center, &geom->P, &geom->u_tan2, false);
    param->convex2 = arcConvexTest(&tc->coords.circle.xyz.center, &geom->P, &geom->u_tan1, true);
    tp_debug_print("circ1 convex: %d, circ2 convex: %d\n",
            param->convex1,
            param->convex2);

    //Identify max angle for first arc by blend limits
    // TODO better name?
    double blend_angle_1 = param->convex1 ? geom->theta_tan : PM_PI / 2.0;
    double blend_angle_2 = param->convex2 ? geom->theta_tan : PM_PI / 2.0;

    param->phi1_max = fmin(prev_tc->coords.circle.xyz.angle * 2.0 / 3.0, blend_angle_1);
    param->phi2_max = fmin(tc->coords.circle.xyz.angle / 3.0, blend_angle_2);

    param->theta = geom->theta_tan;

    // Build the correct unit vector for the linear approximation
    if (param->convex1) {
        PmCartesian blend_point;
        pmCirclePoint(&prev_tc->coords.circle.xyz,
                prev_tc->coords.circle.xyz.angle - param->phi1_max / 2.0,
                &blend_point);
        //Create new unit vector based on secant line
        // Direction is toward P (at end of segment)
        pmCartCartSub(&geom->P, &blend_point, &geom->u1);
        pmCartUnitEq(&geom->u1);

        //Reduce theta proportionally to the angle between the secant and the normal
        param->theta = fmin(param->theta, geom->theta_tan - param->phi1_max / 4.0);

    }

    if (param->convex2) {
        PmCartesian blend_point;
        pmCirclePoint(&tc->coords.circle.xyz,
                param->phi2_max / 2.0,
                &blend_point);
        //Create new unit vector based on secant line
        // Direction is away from P (at start of segment)
        pmCartCartSub(&blend_point, &geom->P,  &geom->u2);
        pmCartUnitEq(&geom->u2);

        //Reduce theta proportionally to the angle between the secant and the normal
        param->theta = fmin(param->theta, geom->theta_tan - param->phi2_max / 4.0);
    }
    blendGeom3Print(geom);


    // Check that we're not below the minimum intersection angle (making too tight an arc)
    // FIXME make this an INI setting?
    const double theta_min = PM_PI / 12.0;
    if (param->theta < theta_min) {
        tp_debug_print("theta = %f < min %f, aborting arc...\n",
                param->theta,
                theta_min);
        return TP_ERR_FAIL;
    }

    tp_debug_print("theta = %f\n", param->theta);

    param->phi = (PM_PI - param->theta * 2.0);

    param->L1 = param->phi1_max * prev_tc->coords.circle.xyz.radius;
    param->L2 = param->phi2_max * tc->coords.circle.xyz.radius;

    if (param->convex1) {
        //use half of the length of the chord
        param->L1 = sin(param->phi1_max/4.0) * prev_tc->coords.circle.xyz.radius;
    }
    if (param->convex2) {
        //use half of the length of the chord
        param->L2 = sin(param->phi2_max/4.0) * tc->coords.circle.xyz.radius;
    }
    tp_debug_print("L1 = %f, L2 = %f\n", param->L1, param->L2);
    tp_debug_print("phi1_max = %f\n",param->phi1_max);
    tp_debug_print("phi2_max = %f\n",param->phi2_max);

    // Setup common parameters
    blendParamKinematics(geom,
            param,
            prev_tc,
            tc,
            acc_bound,
            vel_bound,
            maxFeedScale);

    return TP_ERR_OK;
}

/**
 * Setup blend paramaters based on two linear segments.
 * This function populates the geom structure and "input" fields of the blend parameter structure based.
 * @param geom Stores simplified geometry used to calculate blend params.
 * @param param Abstracted parameters for blending calculations
 * @param prev_tc first linear move to blend
 * @param tc second linear move to blend
 * @param acc_bound maximum X, Y, Z machine acceleration
 * @param vel_bound maximum X, Y, Z machine velocity
 * @param maxFeedScale maximum allowed feed override (set in INI)
 */
int blendInit3FromLineLine(BlendGeom3 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{

    if (tc->motion_type != TC_LINEAR || prev_tc->motion_type != TC_LINEAR) {
        return TP_ERR_FAIL;
    }

    int res_init = blendGeom3Init(geom, prev_tc, tc);
    if (res_init != TP_ERR_OK) {
        return res_init;
    }

    param->theta = geom->theta_tan;

    tp_debug_print("theta = %f\n", param->theta);

    param->phi = (PM_PI - param->theta * 2.0);

    blendGeom3Print(geom);

    const double greediness = 0.5;
    //Nominal length restriction prevents gobbling too much of parabolic blends
    param->L1 = fmin(prev_tc->target, prev_tc->nominal_length * greediness);
    param->L2 = tc->target * greediness;
    tp_debug_print("prev. nominal length = %f, next nominal_length = %f\n",
            prev_tc->nominal_length, tc->nominal_length);
    tp_debug_print("L1 = %f, L2 = %f\n", param->L1, param->L2);

    // Setup common parameters
    blendParamKinematics(geom,
            param,
            prev_tc,
            tc,
            acc_bound,
            vel_bound,
            maxFeedScale);

    return TP_ERR_OK;
}


/**
 * Calculate plane normal and binormal based on unit direction vectors.
 */
int blendCalculateNormals3(BlendGeom3 * const geom)
{

    int err_cross = pmCartCartCross(&geom->u_tan1,
            &geom->u_tan2,
            &geom->binormal);
    int err_unit_b = pmCartUnitEq(&geom->binormal);

    tp_debug_print("binormal = [%f %f %f]\n", geom->binormal.x,
            geom->binormal.y,
            geom->binormal.z);

    pmCartCartSub(&geom->u_tan2, &geom->u_tan1, &geom->normal);
    int err_unit_n = pmCartUnitEq(&geom->normal);

    tp_debug_print("normal = [%f %f %f]\n", geom->normal.x,
            geom->normal.y,
            geom->normal.z);
    return (err_cross || err_unit_b || err_unit_n);
}

/**
 * Compute blend parameters based on line data.
 * Blend arc parameters such as radius and velocity are calculated here. These
 * parameters are later used to create the actual arc geometry in other
 * functions.
 */
int blendComputeParameters(BlendParameters * const param)
{

    // Find maximum distance h from arc center to intersection point
    double h_tol = param->tolerance / (1.0 - sin(param->theta));

    // Find maximum distance along lines allowed by tolerance
    double d_tol = cos(param->theta) * h_tol;
    tp_debug_print(" d_tol = %f\n", d_tol);

    // Find minimum distance by blend length constraints
    double d_lengths = fmin(param->L1, param->L2);
    double d_geom = fmin(d_lengths, d_tol);
    // Find radius from the limiting length
    double R_geom = tan(param->theta) * d_geom;

    // Find maximum velocity allowed by accel and radius
    double v_normal = pmSqrt(param->a_n_max * R_geom);
    tp_debug_print("v_normal = %f\n", v_normal);

    param->v_plan = fmin(v_normal, param->v_goal);

    /*Get the limiting velocity of the equivalent parabolic blend. We use the
     * time it would take to do a "stock" parabolic blend as a metric for how
     * much of the segment to consume. A long segment will have a high
     * "triangle" velocity, so the radius will only be as large as is needed to
     * reach the cornering speed. A short segment will have a low triangle
     * velocity, much lower than the actual curvature limit, which can be used
     * to calculate an equivalent blend radius.
     * */
    double a_parabolic = param->a_max * 0.5;
    double v_triangle = pmSqrt(2.0 * a_parabolic  * d_geom);
    double t_blend = fmin(v_triangle, param->v_plan) / (a_parabolic);
    double s_blend = t_blend * param->v_plan;
    double R_blend = fmin(s_blend / param->phi, R_geom);   //Clamp by limiting radius

    param->R_plan = fmax(pmSq(param->v_plan) / param->a_n_max, R_blend);
    param->d_plan = param->R_plan / tan(param->theta);

    tp_debug_print("v_plan = %f\n", param->v_plan);
    tp_debug_print("R_plan = %f\n", param->R_plan);
    tp_debug_print("d_plan = %f\n", param->d_plan);

    /* "Actual" velocity means the velocity when feed override is 1.0.  Recall
     * that v_plan may be greater than v_req by the max feed override. If our
     * worst-case planned velocity is higher than the requested velocity, then
     * clip at the requested velocity. This allows us to increase speed above
     * the feed override limits.
     */
    if (param->v_plan > param->v_req) {
        param->v_actual = param->v_req;
    } else {
        param->v_actual = param->v_plan;
    }

    // Store arc length of blend arc for future checks
    param->s_arc = param->R_plan * param->phi;

    if (param->R_plan < TP_POS_EPSILON) {
        tp_debug_print("#Blend radius too small, aborting arc\n");
        return TP_ERR_FAIL;
    }

    if (param->s_arc < TP_MIN_ARC_LENGTH) {
        tp_debug_print("#Blend arc length too small, aborting arc\n");
        return TP_ERR_FAIL;
    }
    return TP_ERR_OK;
}


/** Check if the previous line segment will be consumed based on the blend arc parameters. */
int blendCheckConsume(BlendParameters * const param,
        BlendPoints3 const * const points,
        TC_STRUCT const * const prev_tc, int gap_cycles)
{
    //Initialize values
    param->consume = 0;
    param->line_length = 0;
    if (!prev_tc) {
        return -1;
    }

    if (prev_tc->motion_type != TC_LINEAR) {
        return 0;
    }

    //Check for segment length limits
    double L_prev = prev_tc->target - points->trim1;
    double prev_seg_time = L_prev / param->v_plan;

    param->consume = (prev_seg_time < gap_cycles * prev_tc->cycle_time);
    if (param->consume) {
        tp_debug_print("consuming prev line, L_prev = %g\n",
                L_prev);
        param->line_length = L_prev;
    }
    return 0;
}


/**
 * Compute spherical arc points based on blend arc data.
 * Once blend parameters are computed, the three arc points are calculated
 * here.
 */
int blendFindPoints3(BlendPoints3 * const points, BlendGeom3 const * const geom,
        BlendParameters const * const param)
{
    // Find center of blend arc along normal vector
    double center_dist = param->R_plan / sin(param->theta);
    tp_debug_print("center_dist = %f\n", center_dist);

    pmCartScalMult(&geom->normal, center_dist, &points->arc_center);
    pmCartCartAddEq(&points->arc_center, &geom->P);
    tp_debug_print("arc_center = %f %f %f\n",
            points->arc_center.x,
            points->arc_center.y,
            points->arc_center.z);

    // Start point is d_plan away from intersection P in the
    // negative direction of u1
    pmCartScalMult(&geom->u1, -param->d_plan, &points->arc_start);
    pmCartCartAddEq(&points->arc_start, &geom->P);
    tp_debug_print("arc_start = %f %f %f\n",
            points->arc_start.x,
            points->arc_start.y,
            points->arc_start.z);

    // End point is d_plan away from intersection P in the
    // positive direction of u1
    pmCartScalMult(&geom->u2, param->d_plan, &points->arc_end);
    pmCartCartAddEq(&points->arc_end, &geom->P);
    tp_debug_print("arc_end = %f %f %f\n",
            points->arc_end.x,
            points->arc_end.y,
            points->arc_end.z);

    //For line case, just copy over d_plan since it's the same
    points->trim1 = param->d_plan;
    points->trim2 = param->d_plan;

    return TP_ERR_OK;
}


/**
 * Take results of line blend calculation and project onto circular arc and line
 */
int blendLineArcPostProcess(BlendPoints3 * const points, BlendPoints3 const * const points_in,
        BlendParameters * const param, BlendGeom3 const * const geom,
        PmCartLine const * const line1, PmCircle const * const circ2)
{

    PmCartesian center2;
    double radius2;

    findSpiralApproximation(circ2,
            &geom->P,
            &geom->u_tan2,
            &center2,
            &radius2);

    // Define distances from actual center to circle centers
    double d2 = negate(param->R_plan, param->convex2) + radius2;
    tp_debug_print("d2 = %f\n", d2);

    //Get unit vector normal to line in plane, towards arc center
    PmCartesian n1;
    pmCartCartCross(&geom->binormal, &geom->u1, &n1);
    pmCartUnitEq(&n1);

    tp_debug_print("n1 = %f %f %f\n",
            n1.x,
            n1.y,
            n1.z);

    PmCartesian r_PC2;
    pmCartCartSub(&center2, &geom->P, &r_PC2);

    double c2_u,c2_n; //Components of C2-P on u1 and n1
    pmCartCartDot(&r_PC2, &geom->u1, &c2_u);
    pmCartCartDot(&r_PC2, &n1, &c2_n);

    tp_debug_print("c2_u = %f, c2_n = %f\n",
            c2_u,
            c2_n);

    double d_L; // badly named distance along line to intersection
    double A = 1;
    double B = 2.0 * c2_u;
    double C = pmSq(c2_u) - pmSq(d2) + pmSq(param->R_plan - c2_n);
    double root0,root1;
    int res_dist = quadraticFormula(A, B, C, &root0, &root1);
    if (res_dist) {
        return TP_ERR_FAIL;
    }

    tp_debug_print("root0 = %f, root1 = %f\n", root0,
            root1);
    d_L = fmin(fabs(root0),fabs(root1));
    if (d_L < 0) {
        tp_debug_print("d_L can't be < 0, aborting...\n");
        return TP_ERR_FAIL;
    }

    PmCartesian C_u, C_n;

    pmCartScalMult(&geom->u1, -d_L, &C_u);
    pmCartScalMult(&n1, param->R_plan, &C_n);

    PmCartesian r_PC;
    //Continue with correct solution, get actual center
    pmCartCartAdd(&C_u, &C_n, &r_PC);
    pmCartCartAdd(&geom->P, &r_PC, &points->arc_center);
    tp_debug_print("arc center = %f %f %f\n",
            points->arc_center.x,
            points->arc_center.y,
            points->arc_center.z);

    //Verify tolerances
    double h;
    pmCartMag(&r_PC, &h);
    tp_debug_print("center_dist = %f\n", h);

    double T_final = h - param->R_plan;
    tp_debug_print("T_final = %f\n",T_final);
    if (T_final > param->tolerance) {
        tp_debug_print("Projected circle T (%f) exceeds tolerance %f, aborting blend arc\n",
                T_final,
                param->tolerance);
        return TP_ERR_FAIL;
    }

    points->trim1 = d_L;

    points->trim2 = findTrimAngle(&geom->P,
            &points->arc_center,
            &center2);

    return TP_ERR_OK;
}


/**
 * Take results of line blend calculation and project onto circular arc and line
 */
int blendArcLinePostProcess(BlendPoints3 * const points, BlendPoints3 const * const points_in,
        BlendParameters * const param, BlendGeom3 const * const geom,
        PmCircle const * const circ1, PmCartLine const * const line2)
{

    //Create "shifted center" approximation of spiral circles
    PmCartesian center1;
    double radius1;

    findSpiralApproximation(circ1,
            &geom->P,
            &geom->u_tan1,
            &center1,
            &radius1);

    // Define distance from actual arc center to circle center
    double d1 = negate(param->R_plan, param->convex1) + radius1;
    tp_debug_print("d1 = %f\n", d1);

    //Get unit vector normal to line in plane, towards arc center
    PmCartesian n2;
    pmCartCartCross(&geom->binormal, &geom->u2, &n2);
    pmCartUnitEq(&n2);

    tp_debug_print("n2 = %f %f %f\n",
            n2.x,
            n2.y,
            n2.z);

    PmCartesian r_PC1;
    pmCartCartSub(&center1, &geom->P, &r_PC1);
    double c1_u, c1_n; //Components of C1-P on u2 and n2
    pmCartCartDot(&r_PC1, &geom->u2, &c1_u);
    pmCartCartDot(&r_PC1, &n2, &c1_n);

    double d_L; // badly named distance along line to intersection
    double A = 1;
    double B = 2.0 * c1_u;
    double C = pmSq(c1_u) - pmSq(d1) + pmSq(param->R_plan - c1_n);
    double root0,root1;
    int res_dist = quadraticFormula(A, B, C, &root0, &root1);
    if (res_dist) {
        return TP_ERR_FAIL;
    }

    tp_debug_print("root0 = %f, root1 = %f\n", root0,
            root1);
    d_L = fmin(fabs(root0),fabs(root1));
    if (d_L < 0) {
        tp_debug_print("d_L can't be < 0, aborting...\n");
        return TP_ERR_FAIL;
    }

    PmCartesian C_u, C_n;

    pmCartScalMult(&geom->u2, d_L, &C_u);
    pmCartScalMult(&n2, param->R_plan, &C_n);

    PmCartesian r_PC;
    //Continue with correct solution, get actual center
    pmCartCartAdd(&C_u, &C_n, &r_PC);
    pmCartCartAdd(&geom->P, &r_PC, &points->arc_center);
    tp_debug_print("arc center = %f %f %f\n",
            points->arc_center.x,
            points->arc_center.y,
            points->arc_center.z);

    //Verify tolerances
    double h;
    pmCartMag(&r_PC, &h);
    tp_debug_print("center_dist = %f\n", h);

    double T_final = h - param->R_plan;
    if (T_final > param->tolerance) {
        tp_debug_print("Projected circle T (%f) exceeds tolerance %f, aborting blend arc\n",
                T_final,
                param->tolerance);
        return TP_ERR_FAIL;
    }
    tp_debug_print("T_final = %f\n",T_final);

    points->trim1 = findTrimAngle(&geom->P,
            &points->arc_center,
            &center1);

    points->trim2 = d_L;

    return TP_ERR_OK;
}


/**
 * "Post-process" results from linear approximation to fit the circular segments.
 * This step handles the projection from the linear approximation of each
 * circle. Given the solved radius and tolerance, this function updates the
 * points structure with the exact trim angles for each segment.
 */
int blendArcArcPostProcess(BlendPoints3 * const points, BlendPoints3 const * const points_in,
        BlendParameters * const param, BlendGeom3 const * const geom,
        PmCircle const * const circ1, PmCircle const * const circ2)
{

    // Create "shifted center" approximation of spiral circles
    // TODO refers to u1 instead of utan?
    PmCartesian center1, center2;
    double radius1, radius2;

    findSpiralApproximation(circ1,
            &geom->P,
            &geom->u_tan1,
            &center1,
            &radius1);

    findSpiralApproximation(circ2,
            &geom->P,
            &geom->u_tan2,
            &center2,
            &radius2);

    // Define distances from actual center to adjusted circle centers
    double d1 = negate(param->R_plan, param->convex1) + radius1;
    double d2 = negate(param->R_plan, param->convex2) + radius2;
    tp_debug_print("d1 = %f, d2 = %f\n", d1, d2);

    //Find "x" distance between C1 and C2
    PmCartesian r_C1C2;
    pmCartCartSub(&center2, &center1, &r_C1C2);
    double c2x;
    pmCartMag(&r_C1C2, &c2x);

    // Compute the new center location

    double Cx = (-pmSq(d1) + pmSq(d2)-pmSq(c2x)) / (-2.0 * c2x);
    double Cy = pmSqrt(pmSq(d1) - pmSq(Cx));

    tp_debug_print("Cx = %f, Cy = %f\n",Cx,Cy);

    // Find the basis vector uc from center1 to center2
    PmCartesian uc;
    //TODO catch failures here
    int norm_err = pmCartUnit(&r_C1C2, &uc);
    if (norm_err) {
        return TP_ERR_FAIL;
    }

    // Find the basis vector perpendicular to the binormal and uc
    PmCartesian nc;
    pmCartCartCross(&geom->binormal, &uc, &nc);

    //Check if nc is in the same half-plane as the intersection normal. if not,
    //we need to flip it around to choose the correct solution.
    double dot1;
    pmCartCartDot(&geom->normal, &nc, &dot1);
    if (dot1 < 0) {
        pmCartNegEq(&nc);
    }
    norm_err = pmCartUnitEq(&nc);
    if (norm_err) {
        return TP_ERR_FAIL;
    }

    //Find components of center position wrt circle 1 center.
    PmCartesian c_x, c_y;
    pmCartScalMult(&uc, Cx, &c_x);
    pmCartScalMult(&nc, Cy, &c_y);

    //Get vector from P to first center
    PmCartesian r_PC1;
    pmCartCartSub(&center1, &geom->P, &r_PC1);

    // Get "test vectors, relative distance from solution center to P
    PmCartesian test1, test2;
    pmCartCartAdd(&r_PC1, &c_x, &test1);
    test2=test1;

    //Add and subtract c_y component to get equivalent of two Y solutions
    pmCartCartAddEq(&test1, &c_y);
    pmCartCartSubEq(&test2, &c_y);

    double mag1,mag2;
    pmCartMag(&test1, &mag1);
    pmCartMag(&test2, &mag2);

    if (mag2 < mag1)
    {
        //negative solution is closer
        pmCartNegEq(&c_y);
    }

    //Continue with correct solution, get actual center
    PmCartesian r_C1C;
    pmCartCartAdd(&c_x, &c_y, &r_C1C);
    pmCartCartAdd(&center1, &r_C1C, &points->arc_center);
    tp_debug_print("arc center = %f %f %f\n",
            points->arc_center.x,
            points->arc_center.y,
            points->arc_center.z);

    //Find components of center position wrt circle 2 center.
    PmCartesian r_C2C;
    pmCartCartSub(&points->arc_center, &center2, &r_C2C);

    PmCartesian r_PC;
    pmCartCartSub(&points->arc_center, &geom->P, &r_PC);

    //Verify tolerances
    double h;
    pmCartMag(&r_PC, &h);
    tp_debug_print("center_dist = %f\n", h);

    double T_final = h - param->R_plan;
    if (T_final > param->tolerance) {
        tp_debug_print("Projected circle T (%f) exceeds tolerance %f, aborting blend arc\n",
                T_final,
                param->tolerance);
        return TP_ERR_FAIL;
    }
    tp_debug_print("T_final = %f\n",T_final);

    points->trim1 = findTrimAngle(&geom->P,
            &points->arc_center,
            &center1);
    points->trim2 = findTrimAngle(&geom->P,
            &points->arc_center,
            &center2);

    tp_debug_print("trim1 = %f, trim2 = %f\n",
            points->trim1,
            points->trim2);

    return TP_ERR_OK;
}


/**
 * Setup the spherical arc struct based on the blend arc data.
 */
int arcFromBlendPoints3(SphericalArc * const arc, BlendPoints3 const * const points,
        BlendGeom3 const * const geom, BlendParameters const * const param)
{
    // If we consume the previous line, the remaining line length gets added here
    arc->uTan = geom->u_tan1;
    arc->line_length = param->line_length;
    arc->binormal = geom->binormal;

    // Create the arc from the processed points
    return arcInitFromPoints(arc, &points->arc_start,
            &points->arc_end, &points->arc_center);
}

int blendGeom3Print(BlendGeom3 const * const geom)
{
    tp_debug_print("u1 = %f %f %f\n",
            geom->u1.x,
            geom->u1.y,
            geom->u1.z);

    tp_debug_print("u2 = %f %f %f\n",
            geom->u2.x,
            geom->u2.y,
            geom->u2.z);
    return 0;
}

int blendPoints3Print(BlendPoints3 const * const points)
{
    tp_debug_print("arc_start = %f %f %f\n",
            points->arc_start.x,
            points->arc_start.y,
            points->arc_start.z);

    tp_debug_print("arc_center = %f %f %f\n",
            points->arc_center.x,
            points->arc_center.y,
            points->arc_center.z);

    tp_debug_print("arc_end = %f %f %f\n",
            points->arc_end.x,
            points->arc_end.y,
            points->arc_end.z);

    return 0;

}

double pmCircleActualMaxVel(PmCircle * const circle, double v_max, double a_max, int parabolic)
{
    if (parabolic) {
        a_max /= 2.0;
    }
    double a_n_max = BLEND_ACC_RATIO_NORMAL * a_max;
    double v_max_acc = pmSqrt(a_n_max * circle->radius);
    if (v_max_acc < v_max) {
        tp_debug_print("Maxvel limited from %f to %f for tangential acceleration\n", v_max, v_max_acc);
        return v_max_acc;
    } else {
        tp_debug_print("v_max %f is within limit of v_max_acc %f\n",v_max, v_max_acc);
        return v_max;
    }
}

