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

#if 0
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

STATIC int arcConvexTest(PmCartesian const * const center,
        PmCartesian const * const P, PmCartesian const * const uVec, bool reverse_dir)
{
    //Check if an arc-line intersection is concave or convex
    double dot;
    PmCartesian diff;
    pmCartCartSub(P, center, &diff);
    pmCartCartDot(&diff, uVec, &dot);

    int convex = reverse_dir ^ (dot < 0);
    return convex;
}

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

/**
 * Copy over parameters into LineLineData struct.
 * This function initializes the LineLineData stuct from existing line segments
 */
int bmLineLineInit(LineLineData * const data, TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale)
{
    
    //Copy over unit vectors
    data->u1 = prev_tc->coords.line.xyz.uVec;
    data->u2 = tc->coords.line.xyz.uVec;

    data->P = prev_tc->coords.line.xyz.end;

    //Calculate angles between lines
    int res = findIntersectionAngle(&data->u1,
            &data->u2, &data->theta);
    if (res) {
        return TP_ERR_FAIL;
    }
    tp_debug_print("theta = %f\n", data->theta);

    data->phi = (PM_PI - data->theta * 2.0);

    // Calculate max acceleration based on plane containing lines
    calculateInscribedDiameter(&data->binormal, acc_bound, &data->a_max);

    // Store max normal acceleration
    data->a_n_max = data->a_max * TP_ACC_RATIO_NORMAL;
    tp_debug_print("a_max = %f, a_n_max = %f\n", data->a_max,
            data->a_n_max);

    //Find common velocity and acceleration
    data->v_req = fmin(prev_tc->reqvel, tc->reqvel);
    data->v_goal = data->v_req * maxFeedScale;

    //Calculate the maximum planar velocity
    double v_max;
    calculateInscribedDiameter(&data->binormal, vel_bound, &v_max);
    data->v_goal = fmin(data->v_goal, v_max);

    tp_debug_print("vr1 = %f, vr2 = %f\n", prev_tc->reqvel, tc->reqvel);
    tp_debug_print("v_goal = %f, max scale = %f\n", data->v_goal, maxFeedScale);

    //FIXME greediness really should be 0.5 anyway
    const double greediness = 0.5;
    //Nominal length restriction prevents gobbling too much of parabolic blends
    data->L1 = fmin(prev_tc->target, prev_tc->nominal_length * greediness);
    data->L2 = tc->target * greediness;
    tp_debug_print("prev. nominal length = %f, next nominal_length = %f\n",
            prev_tc->nominal_length, tc->nominal_length);
    tp_debug_print("L1 = %f, L2 = %f\n", data->L1, data->L2);

    //TODO get tolerance from blend here 
    double nominal_tolerance;
    tcFindBlendTolerance(prev_tc, tc, &data->tolerance, &nominal_tolerance);
    return TP_ERR_OK;
}


/**
 * Calculate plane normal and binormal based on unit direction vectors.
 */
int bmLineLineCalculateNormals(LineLineData * const data)
{

    pmCartCartCross(&data->u1,
            &data->u2,
            &data->binormal);
    pmCartUnitEq(&data->binormal);

    tp_debug_print("binormal = [%f %f %f]\n", data->binormal.x,
            data->binormal.y,
            data->binormal.z);

    pmCartCartSub(&data->u2, &data->u1, &data->normal);
    pmCartUnitEq(&data->normal);

    tp_debug_print("normal = [%f %f %f]\n", data->normal.x,
            data->normal.y,
            data->normal.z);
    return 0;
}

/**
 * Compute blend parameters based on line data.
 * Blend arc parameters such as radius and velocity are calculated here. These
 * parameters are later used to create the actual arc geometry in other
 * functions.
 */
int bmLineLineParameters(LineLineData * const data)
{

    double h_tol = data->tolerance / (1.0 - sin(data->theta));
    double d_tol = cos(data->theta) * h_tol;
    // Debug output for tolerances
    tp_debug_print(" d_tol = %f\n",d_tol);

    //Find min length due to segment limits
    double d_lengths = fmin(data->L1, data->L2);
    double d_geom = fmin(d_lengths, d_tol); 
    double R_geom = tan(data->theta) * d_geom;

    double v_normal = pmSqrt(data->a_n_max * R_geom);
    tp_debug_print("v_normal = %f\n", v_normal);

    data->v_plan = v_normal;
    data->R_plan = R_geom;

    // If our goal velocity is lower, reduce the arc size proportionally
    if (v_normal > data->v_goal) {
        data->v_plan = data->v_goal;
        tp_debug_print("v_goal = %f\n", data->v_goal);
        //At this new limiting velocity, find the radius by the reverse formula
        data->R_plan = pmSq(data->v_plan) / data->a_n_max;
    }
    tp_debug_print("R_plan = %f\n", data->R_plan);
    data->d_plan = data->R_plan / tan(data->theta);

    tp_debug_print("v_plan = %f\n", data->v_plan);

    //Now we store the "actual" velocity. Recall that v_plan may be greater
    //than v_req by the max feed override. If our worst-case planned velocity is higher than the requested velocity, then clip at the requested velocity. This allows us to increase speed above the feed override limits
    if (data->v_plan > data->v_req) {
        data->v_actual = data->v_req;
    } else {
        data->v_actual = data->v_plan;
    }

    if (data->R_plan < TP_POS_EPSILON) {
        tp_debug_print("#Blend radius too small, aborting...\n");
        return TP_ERR_FAIL;
    }
    return TP_ERR_OK;
}


/** Check if the previous line segment will be consumed based on the blend arc parameters. */
int bmLineLineCheckConsume(LineLineData * const data,
        TC_STRUCT const * const prev_tc, int gap_cycles)
{
    if (!prev_tc) {
        return -1;
    }
    //Check for segment length limits
    double L_prev = prev_tc->target - data->d_plan;
    double prev_seg_time = L_prev / data->v_plan;

    data->consume = (prev_seg_time < gap_cycles * prev_tc->cycle_time);
    return 0;
}


/**
 * Compute spherical arc points based on blend arc data.
 * Once blend parameters are computed, the three arc points are calculated
 * here.
 */
int bmLineLinePoints(LineLineData * const data)
{
    //Find center of blend arc along normal vector
    double h_plan = data->R_plan / sin(data->theta);
    tp_debug_print("h_plan = %f\n",h_plan);
    pmCartScalMult(&data->normal, h_plan, &data->arc_center );
    pmCartCartAddEq(&data->arc_center, &data->P);

    // Start point is d_plan away from intersection P in the
    // negative direction of u1
    pmCartScalMult(&data->u1, -data->d_plan, &data->arc_start);
    pmCartCartAddEq(&data->arc_start, &data->P);

    // End point is d_plan away from intersection P in the
    // positive direction of u1
    pmCartScalMult(&data->u2, data->d_plan, &data->arc_end);
    pmCartCartAddEq(&data->arc_end, &data->P);
    return TP_ERR_OK;
}


/**
 * Setup the spherical arc struct based on the blend arc data.
 */
int bmArcFromLineLine(SphericalArc * const arc, LineLineData const * const data)
{

    // If we consume the previous line, the remaining line length gets added here
    arc->uTan = data->u1;
    if (data->consume) {
        arc->line_length = data->L1 - data->d_plan;
    } else {
        arc->line_length = 0;
    }

    //Create the arc from the processed points
    return arcInitFromPoints(arc, &data->arc_start, &data->arc_end, &data->arc_center);
}

