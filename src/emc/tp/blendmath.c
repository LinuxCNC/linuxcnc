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
#include "blendmath.h"
#include "spherical_arc.h"
#include "rtapi_math.h"
#include "tc.h"
#include "tp.h"
#include "tp_debug.h"

/** @section utilityfuncs Utility functions */

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

double negate(double f, bool neg) 
{
    if (neg) {
        return -f;
    }
    return f;
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

int quadraticFormula(double A, double B, double C, double * const root0,
        double * const root1)
{
    double disc = pmSq(B) - 4.0 * A * C;
    if (disc < 0) {
        tp_debug_print("discriminant < 0\n");
        return TP_ERR_FAIL;
    }
    double t1 = pmSqrt(disc);
    *root0 = ( -B + t1) / (2.0 * A);
    *root1 = ( -B - t1) / (2.0 * A);
    return TP_ERR_OK;
}

/**
 * @section linearc Line-Arc blending functions
 */

STATIC int findArcLineDist(double a, double b, double R1, double T,
        int convex, double * const d)
{
    /* Compute distance along line segment where tangent arc with tolerance T will hit.*/
    /*   a = (P-C1) . u2*/
    /*   b = (P-C1) . n2*/
    if (!d) {
        return TP_ERR_MISSING_OUTPUT;
    }
    double sgn = 1.0;
    if (convex) {
        sgn=-1.0;
    }

    double A = T/(b-sgn*R1)-1.0;
    double B = T*a/(b-sgn*R1);
    double C = pmSq(T);;

    double d0 = 0;
    double d1 = 0;

    int err = quadraticFormula(A, B, C, &d0, &d1);
    if (d0>0 && d1>0) {
        *d=fmin(d0,d1);
    } else {
        *d=fmax(d0,d1);
    }
    return err;

}

STATIC int findRadiusFromDist(double a, double b, double R1, double d, int convex, double * const R)
{

    if (!R) {
        return TP_ERR_MISSING_OUTPUT;
    }
    //For the arc-line case, when a distance d is specified, find the corresponding radius
    double sgn = convex ? -1.0 : 1.0;

    double den = (R1-sgn*b);
    if (fabs(den) < TP_POS_EPSILON) {
        tp_debug_print("den = %g, near 0\n", den);
        return TP_ERR_FAIL;
    }

    *R = sgn * (pmSq(d) / 2.0 + a * d) / den;

    return TP_ERR_OK;

}

STATIC int findDistFromRadius(double a, double b, double R1, double R, int convex, double * const d)
{
    if (!d) {
        return TP_ERR_MISSING_OUTPUT;
    }
    double sgn = 1.0;
    if (convex) {
        sgn=-1;
    }

    double A = 1.0;
    double B = 2.0*a;
    double C = 2.0*(-sgn * R1 + b) * R;
    double d0=0;
    double d1=0;

    int err = quadraticFormula(A,B,C,&d0,&d1);
    if (d0>0 && d1>0){
        *d=fmin(d0,d1);
    } else {
        *d=fmax(d0,d1);
    }
    return err;
}

#if 0
int bmLineArcProcess(LineArcData * const data)
{
    // Check for coplanarity
    // arc-line equations
    pmCartCartCross(&data->u1, &data->u2, &data->binormal);
    pmCartUnitEq(&data->binormal);
    pmCartCartCross(&data->binormal, &data->u2, &data->normal);

    bool convex = arcConvexTest(&data->C1, &data->P,
            &data->u2, false);
    double sgn = 1.0;
    if (convex) {
        tp_debug_print("Arc is convex wrt. line\n");
        sgn=-1.0;
    } else {
        tp_debug_print("Arc is concave wrt. line\n");
    }

    //Parallel and perp. components of P-C1
    PmCartesian r_C1P;
    pmCartCartSub(&data->P,&data->C1, &r_C1P);

    //Project C1 - P onto u2 and normal
    double a, b;
    pmCartCartDot(&r_C1P, &data->u2, &a);
    pmCartCartDot(&r_C1P, &data->normal, &b);
    tp_debug_print("r_C1P components: a = %f, b = %f\n",a,b);

    double d_tol;
    int err = findArcLineDist(a, b, data->R1, data->tolerance, convex, &d_tol);
    if (err) {
        return err;
    }

    double d_arc = 0;
    if (!convex) {
        //phi1 is the blendable length
        d_arc = data->phi1 * data->R1;
    } else {
        //Calculate "beta" angle = deviation from tangent
        // Find minimum angle for convex case
        double dot;
        pmCartCartDot(&data->u1, &data->u2, &dot);
        double angle_from_tan = acos(dot);
        double gamma = fmin(PM_PI - angle_from_tan, data->phi1);
        d_arc = data->R1 * sin(gamma);
    }
    //Find distance bounded by length of line move
    double d_line = data->L2;
    tp_debug_print("d_arc = %f, d_tol = %f, d_line = %f\n",d_arc,d_tol,d_line);

    double d_geom = fmin(fmin(d_line,d_tol),d_arc);

    //Find corresponding radius to d_line
    double R_line = 0;
    err = findRadiusFromDist(a,b,data->R1,d_geom,convex, &R_line);
    if (err)  {
        return err;
    }

    //New upper bound is the lower of the two
    //FIXME -code upper bound until we figure out a better way
    double R_bound = 10000;
    double R_geom = fmin(R_line, R_bound);
    tp_debug_print("R_geom = %f\n",R_geom);

    //The new radius and line distance is found based on limits of v_req
    // Based on motion segments, compute the maximum velocity we can get based
    //on the requested blend radius and the normal acceleration bounds. Use this
    //to compute the actual upper limit on blend radius.

    //The nominal speed of the blend arc should be the higher of the two segment speeds

    double a_max;
    tpGetMachineAccelLimit(&a_max);
    data->a_max = a_max;

    double a_n_max=a_max * TP_ACC_RATIO_NORMAL;

    //Calculate limiting velocity due to radius and normal acceleration
    double v_normal = pmSqrt(a_n_max * R_geom);
    tp_debug_print("v_normal = %f\n",v_normal);

    //If the requested feed is lower than the peak velocity, reduce the arc size to match
    double v_upper = fmin(data->v_req, v_normal);

    double R_upper = pmSq(v_upper)/a_n_max;
    data->R = R_upper;

    double d_upper=0;
    err = findDistFromRadius(a,b, data->R1, R_upper, convex, &d_upper);
    if (err) {
        return err;
    }
    data->d = d_upper;

    tp_debug_print("R_upper = %f, d_upper = %f\n",R_upper,d_upper);

    //Store velocity
    data->v_plan = v_upper;
    tp_debug_print("v_upper = %f\n", v_upper);

    //Find the blend arc's center
    /*double C = P + d_upper*u2 + R_upper*normal*/
    PmCartesian tmp;
    pmCartScalMult(&data->u2, d_upper, &data->C);
    pmCartScalMult(&data->normal, R_upper, &tmp);
    pmCartCartAdd(&data->P, &data->C, &data->C);
    pmCartCartAdd(&tmp, &data->C, &data->C);

    PmCartesian r_CC1, uc;
    pmCartCartSub(&data->C1, &data->C, &r_CC1);
    pmCartUnit(&r_CC1, &uc);

    //Calculate blend arc intersections with original segments
    //Q1=C+sgn*uc*R_upper
    pmCartScalMult(&uc, R_upper*sgn, &data->Q1);
    pmCartCartAdd(&data->Q1, &data->C, &data->Q1);
    //Q2=P+d_upper*u2
    pmCartScalMult(&data->u2, d_upper, &data->Q2);
    pmCartCartAdd(&data->P, &data->Q2, &data->Q2);

    //Calculate angle reduction for arc
    PmCartesian up;
    pmCartUnit(&r_C1P,&up);
   
    double dot = 0;
    pmCartCartDot(&up,&uc,&dot);
    //FIXME domain bound
    data->dphi1 = acos(-dot);

    tp_debug_print("dphi1 = %f, phi1 = %f, ratio = %f\n",
            data->dphi1, data->phi1, data->dphi1 / data->phi1);
    return 0;
}
#endif

/**
 * @section bmLineLine LineLineData functions
 */

int blendInit3FromArcs(BlendGeom3 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{
   
    if (tc->motion_type != TC_CIRCULAR || prev_tc->motion_type != TC_CIRCULAR) {
        return TP_ERR_FAIL;
    }

    //Get tangent unit vectors at endpoint
    PmCartesian u_tan1, u_tan2;
    tcGetEndTangentUnitVector(prev_tc, &u_tan1);
    tcGetStartTangentUnitVector(tc, &u_tan2);

    //Find angle between tangent lines
    double theta_tan;
    findIntersectionAngle(&u_tan1, &u_tan2, &theta_tan);

    //Get intersection point
    pmCirclePoint(&tc->coords.circle.xyz, 0.0, &geom->P);
    tp_debug_print("Intersection point P = %f %f %f\n",
            geom->P.x,
            geom->P.y,
            geom->P.z);

    param->convex1 = arcConvexTest(&prev_tc->coords.circle.xyz.center, &geom->P, &u_tan2, false);
    param->convex2 = arcConvexTest(&tc->coords.circle.xyz.center, &geom->P, &u_tan1, true);
    tp_debug_print("circ1 convexity: %d, circ2 convexity: %d\n",
            param->convex1,
            param->convex2);

    //Identify max angle for first arc by blend limits
    // TODO better name?
    double blend_angle_1 = param->convex1 ? theta_tan : PM_PI / 2.0;
    double blend_angle_2 = param->convex2 ? theta_tan : PM_PI / 2.0;

    double phi1_max = fmin(prev_tc->coords.circle.xyz.angle / 3.0, blend_angle_1);
    double phi2_max = fmin(tc->coords.circle.xyz.angle / 3.0, blend_angle_2);
    // Build the correct unit vector for the linear approximation
    if (param->convex1) {
        PmCartesian blend_point;
        pmCirclePoint(&prev_tc->coords.circle.xyz,
                prev_tc->coords.circle.xyz.angle - phi1_max,
                &blend_point);
        //Create new unit vector based on secant line
        // Direction is toward P (at end of segment)
        pmCartCartSub(&geom->P, &blend_point, &geom->u1);
        pmCartUnitEq(&geom->u1);
    } else {
        geom->u1 = u_tan1;
    }

    blendGeom3Print(geom);

    if (param->convex2) {
        PmCartesian blend_point;
        pmCirclePoint(&tc->coords.circle.xyz,
                phi2_max,
                &blend_point);
        //Create new unit vector based on secant line
        // Direction is away from P (at start of segment)
        pmCartCartSub(&blend_point, &geom->P,  &geom->u2);
        pmCartUnitEq(&geom->u2);
    } else {
        geom->u2 = u_tan2;
    }

    //Calculate angles between lines
    int res = findIntersectionAngle(&geom->u1,
            &geom->u2, &param->theta);
    if (res) {
        return TP_ERR_FAIL;
    }
    tp_debug_print("theta = %f\n", param->theta);

    param->phi = (PM_PI - param->theta * 2.0);

    //Do normal calculation here since we need this information for accel / vel limits
    blendCalculateNormals3(geom);

    // Calculate max acceleration based on plane containing lines
    calculateInscribedDiameter(&geom->binormal, acc_bound, &param->a_max);

    // Store max normal acceleration
    param->a_n_max = param->a_max * TP_ACC_RATIO_NORMAL;
    tp_debug_print("a_max = %f, a_n_max = %f\n", param->a_max,
            param->a_n_max);

    //Find common velocity and acceleration
    param->v_req = fmin(prev_tc->reqvel, tc->reqvel);
    param->v_goal = param->v_req * maxFeedScale;

    //Calculate the maximum planar velocity
    double v_max;
    calculateInscribedDiameter(&geom->binormal, vel_bound, &v_max);
    param->v_goal = fmin(param->v_goal, v_max);

    tp_debug_print("vr1 = %f, vr2 = %f\n", prev_tc->reqvel, tc->reqvel);
    tp_debug_print("v_goal = %f, max scale = %f\n", param->v_goal, maxFeedScale);

    //Nominal length restriction prevents gobbling too much of parabolic blends
    param->L1 = phi1_max * prev_tc->coords.circle.xyz.radius;
    param->L2 = phi2_max * tc->coords.circle.xyz.radius;

    if (param->convex1) {
        param->L1 /= 2.0;
    }
    if (param->convex2) {
        param->L2 /= 2.0;
    }
    tp_debug_print("L1 = %f, L2 = %f\n", param->L1, param->L2);

    double nominal_tolerance;
    tcFindBlendTolerance(prev_tc, tc, &param->tolerance, &nominal_tolerance);

    return TP_ERR_OK;
}

int blendInit3FromLines(BlendGeom3 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{

    if (tc->motion_type != TC_LINEAR || prev_tc->motion_type != TC_LINEAR) {
        return TP_ERR_FAIL;
    }
    //Copy over unit vectors and intersection point
    geom->u1 = prev_tc->coords.line.xyz.uVec;
    geom->u2 = tc->coords.line.xyz.uVec;
    geom->P = prev_tc->coords.line.xyz.end;

    //Calculate angles between lines
    int res = findIntersectionAngle(&geom->u1,
            &geom->u2, &param->theta);
    if (res) {
        return TP_ERR_FAIL;
    }
    tp_debug_print("theta = %f\n", param->theta);

    param->phi = (PM_PI - param->theta * 2.0);

    //Do normal calculation here since we need this information for accel / vel limits
    blendCalculateNormals3(geom);

    // Calculate max acceleration based on plane containing lines
    calculateInscribedDiameter(&geom->binormal, acc_bound, &param->a_max);

    // Store max normal acceleration
    param->a_n_max = param->a_max * TP_ACC_RATIO_NORMAL;
    tp_debug_print("a_max = %f, a_n_max = %f\n", param->a_max,
            param->a_n_max);

    //Find common velocity and acceleration
    param->v_req = fmin(prev_tc->reqvel, tc->reqvel);
    param->v_goal = param->v_req * maxFeedScale;

    //Calculate the maximum planar velocity
    double v_max;
    calculateInscribedDiameter(&geom->binormal, vel_bound, &v_max);
    param->v_goal = fmin(param->v_goal, v_max);

    tp_debug_print("vr1 = %f, vr2 = %f\n", prev_tc->reqvel, tc->reqvel);
    tp_debug_print("v_goal = %f, max scale = %f\n", param->v_goal, maxFeedScale);

    //FIXME greediness really should be 0.5 anyway
    const double greediness = 0.5;
    //Nominal length restriction prevents gobbling too much of parabolic blends
    param->L1 = fmin(prev_tc->target, prev_tc->nominal_length * greediness);
    param->L2 = tc->target * greediness;
    tp_debug_print("prev. nominal length = %f, next nominal_length = %f\n",
            prev_tc->nominal_length, tc->nominal_length);
    tp_debug_print("L1 = %f, L2 = %f\n", param->L1, param->L2);

    double nominal_tolerance;
    tcFindBlendTolerance(prev_tc, tc, &param->tolerance, &nominal_tolerance);

    return TP_ERR_OK;
}



/**
 * Calculate plane normal and binormal based on unit direction vectors.
 */
int blendCalculateNormals3(BlendGeom3 * const geom)
{

    int err_cross = pmCartCartCross(&geom->u1,
            &geom->u2,
            &geom->binormal);
    int err_unit_b = pmCartUnitEq(&geom->binormal);

    tp_debug_print("binormal = [%f %f %f]\n", geom->binormal.x,
            geom->binormal.y,
            geom->binormal.z);

    pmCartCartSub(&geom->u2, &geom->u1, &geom->normal);
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
    param->R_plan = pmSq(param->v_plan) / param->a_n_max;
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
        TC_STRUCT const * const prev_tc, int gap_cycles)
{
    if (!prev_tc) {
        return -1;
    }
    //Check for segment length limits
    double L_prev = prev_tc->target - param->d_plan;
    double prev_seg_time = L_prev / param->v_plan;

    param->consume = (prev_seg_time < gap_cycles * prev_tc->cycle_time);
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
 * Take results of line blend calculation and project onto circular arcs.
 */
int blendArcArcPostProcess(BlendPoints3 * const points, BlendPoints3 const * const points_in,
        BlendParameters * const param, BlendGeom3 const * const geom,
        PmCircle const * const circ1, PmCircle const * const circ2)
{

    tp_debug_print("circ1 center = %f %f %f\n",
            circ1->center.x,
            circ1->center.y,
            circ1->center.z);

    tp_debug_print("circ2 center = %f %f %f\n",
            circ2->center.x,
            circ2->center.y,
            circ2->center.z);
    tp_debug_print("circ1 radius = %f\n", circ1->radius);
    tp_debug_print("circ2 radius = %f\n", circ2->radius);


    //TODO need convex1 / convex2 here to flip signs on radius
    //Find the distance from the approximate center to each circle center
    double d_guess1, d_guess2;
    PmCartesian diff;
    pmCartCartSub(&points_in->arc_center, &circ1->center, &diff);
    pmCartMag(&diff, &d_guess1);

    pmCartCartSub(&points_in->arc_center, &circ2->center, &diff);
    pmCartMag(&diff, &d_guess2);

    // From the guessed center, find the minimum radius required to intersect the circles
    double R_final = fmin(negate(d_guess1 - circ1->radius, param->convex1),
            negate(d_guess2 - circ2->radius, param->convex2));
    //KLUDGE force to less than original radius or solution breaks down
    R_final = fmin(R_final, circ1->radius);
    R_final = fmin(R_final, circ2->radius);
    tp_debug_print("d_guess1 = %f, d_guess2 = %f\n", d_guess1, d_guess2);
    tp_debug_print("R_final = %f, R_guess = %f\n", R_final, param->R_plan);

    // Define distances from actual center to circle centers
    double d1 = negate(R_final, param->convex1) + circ1->radius;
    double d2 = negate(R_final, param->convex2) + circ2->radius;
    tp_debug_print("d1 = %f, d2 = %f\n", d1, d2);


    //Find "x" distance between C1 and C2
    PmCartesian r_C1C2;
    pmCartCartSub(&circ2->center, &circ1->center, &r_C1C2);
    double c2x;
    pmCartMag(&r_C1C2, &c2x);

    // Compute the new center location

    double Cx = (-pmSq(d1) + pmSq(d2)-pmSq(c2x)) / (-2.0 * c2x);
    double Cy = pmSqrt(pmSq(d1) - pmSq(Cx));

    tp_debug_print("Cx = %f, Cy = %f\n",Cx,Cy);

    // Find the basis vector uc from center1 to center2
    PmCartesian uc;
    //TODO catch failures here
    pmCartUnit(&r_C1C2, &uc);

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
    int norm_err = pmCartUnitEq(&nc);
    if (norm_err) {
        return TP_ERR_FAIL;
    }

    //Find components of center position wrt circle 1 center.
    PmCartesian c_x, c_y, r_C1C;
    pmCartScalMult(&uc, Cx, &c_x);
    pmCartScalMult(&nc, Cy, &c_y);

    //Get vector from P to first center
    PmCartesian r_PC1;
    pmCartCartSub(&circ1->center, &geom->P, &r_PC1);
    
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
    pmCartCartAdd(&c_x, &c_y, &r_C1C);
    pmCartCartAdd(&circ1->center, &r_C1C, &points->arc_center);
    tp_debug_print("arc center = %f %f %f\n",
            points->arc_center.x,
            points->arc_center.y,
            points->arc_center.z);

    //Find components of center position wrt circle 2 center.
    PmCartesian r_C2C;
    pmCartCartSub(&points->arc_center, &circ2->center, &r_C2C);




    PmCartesian r_PC;
    pmCartCartSub(&points->arc_center, &geom->P, &r_PC);

    //Verify tolerances
    double h;
    pmCartMag(&r_PC, &h);
    tp_debug_print("center_dist = %f\n", h);

    double T_final = h - R_final;
   if (T_final > param->tolerance) {
       tp_debug_print("Projected circle T (%f) exceeds tolerance %f, aborting blend arc\n",
               T_final,
               param->tolerance);
       return TP_ERR_FAIL;
   }
    tp_debug_print("T_final = %f\n",T_final);

    //Find intersection points from
    double scale1 = circ1->radius / d1;
    pmCartScalMult(&r_C1C, scale1, &points->arc_start);
    double scale2 = circ2->radius / d2;
    pmCartScalMult(&r_C2C, scale2, &points->arc_end);
    pmCartCartAddEq(&points->arc_start, &circ1->center);
    pmCartCartAddEq(&points->arc_end, &circ2->center);

    tp_debug_print("arc start = %f %f %f\n",
            points->arc_start.x,
            points->arc_start.y,
            points->arc_start.z);
    tp_debug_print("arc end = %f %f %f\n",
            points->arc_end.x,
            points->arc_end.y,
            points->arc_end.z);


    PmCartesian r_C1P;
    pmCartCartSub(&geom->P, &circ1->center, &r_C1P);

    double dot;
    pmCartCartDot(&r_C1P, &r_C1C, &dot);
    double dphi1=acos(dot / (circ1->radius * d1));

    PmCartesian r_C2P;
    pmCartCartSub(&geom->P, &circ2->center, &r_C2P);
    pmCartCartDot(&r_C2P, &r_C2C, &dot);
    double dphi2=acos(dot / (circ2->radius * d2));

    tp_debug_print("dphi1 = %f, dphi2 = %f\n", dphi1, dphi2);

    points->trim1 = dphi1;
    points->trim2 = dphi2;

    //Update param structure with new values
    param->R_plan = R_final;
    param->v_plan = pmSqrt(param->R_plan * param->a_n_max);
    param->v_actual = fmin(param->v_plan, param->v_actual);

    return TP_ERR_OK;

}


/**
 * Setup the spherical arc struct based on the blend arc data.
 */
int arcFromBlendPoints3(SphericalArc * const arc, BlendPoints3 const * const points,
        BlendGeom3 const * const geom, BlendParameters const * const param)
{
    // If we consume the previous line, the remaining line length gets added here
    arc->uTan = geom->u1;
    if (param->consume) {
        arc->line_length = param->L1 - param->d_plan;
    } else {
        arc->line_length = 0;
    }

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
