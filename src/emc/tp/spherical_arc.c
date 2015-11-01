/********************************************************************
 * Description: spherical_arc.c
 *
 * A simple spherical linear interpolation library and related functions.
 *
 * Author: Robert W. Ellenberg
 * License: GPL Version 2
 * System: Linux
 *    
 * Copyright (c) 2014 All rights reserved.
 *
 ********************************************************************/

#include "posemath.h"
#include "vector6.h"
#include "spherical_arc.h"
#include "tp_types.h"
#include "rtapi_math.h"

#include "tp_debug.h"

int arcInitFromPoints(SphericalArc * const arc, Vector6 const * const start,
        Vector6 const * const end,
        Vector6 const * const center)
{
#ifdef ARC_PEDANTIC
    if (!P0 || !P1 || !center) {
        return TP_ERR_MISSING_INPUT;
    
    if (!arc) {
        return TP_ERR_MISSING_OUTPUT;
    }
#endif

    // Store the start, end, and center
    arc->start = *start;
    arc->end = *end;
    arc->center = *center;

    VecVecSub(start, center, &arc->rStart);
    VecVecSub(end, center, &arc->rEnd);

    // Find the radii at start and end. These are identical for a perfect spherical arc
    double radius0, radius1;
    VecMag(&arc->rStart, &radius0);
    VecMag(&arc->rEnd, &radius1);

    tp_debug_print("radii are %g and %g\n",
            radius0,
            radius1);

    if (radius0 < ARC_MIN_RADIUS || radius1 < ARC_MIN_RADIUS) {
        tp_debug_print("radius below min radius %f, aborting arc\n",
                ARC_MIN_RADIUS);
        return TP_ERR_RADIUS_TOO_SMALL;
    }

    // Choose initial radius as nominal radius
    arc->radius = radius0;

    // Find arc angle
    double dot=0.0;
    VecVecDot(&arc->rStart, &arc->rEnd, &dot);

    // Divide through by magnitudes of each vector
    dot /= radius0;
    dot /= radius1;
    arc->angle = acos(dot);

    tp_debug_print("spherical arc angle = %f\n", arc->angle);

    // Store spiral factor as radial difference. Archimedean spiral coef. a = spiral / angle
    arc->spiral = (radius1 - radius0 );

    if (arc->angle < ARC_MIN_ANGLE) {
        tp_debug_print("angle %f below min angle %f, aborting arc\n",
                arc->angle,
                ARC_MIN_ANGLE);
        return TP_ERR_GEOM;
    }

    // Store sin of arc angle since it is reused many times for SLERP
    arc->Sangle = sin(arc->angle);

    return TP_ERR_OK;
}

int arcPointCart(SphericalArc const * const arc, 
        double angle_in,
        PmCartesian * xyz,
        PmCartesian * uvw)
{

    Vector6 temp;
    int res = arcPoint(arc, angle_in, &temp);
    if (res) {
        return res;
    }

    return VecToCart(&temp, xyz, uvw);
}


/**
 * For a given progress along the arc, get the corresponding XYZUVW point.
 */
int arcPoint(SphericalArc const * const arc, double progress, Vector6 * const out)
{
    //Convert progress to actual progress around the arc. Progress = 0 when the
    //line segment is complete, and the arc is just starting. "Negative"
    //progress means we're on the line segment before the arc. This is an
    //artifact of "consuming" the previous line segment.
    double net_progress = progress - arc->line_length;
    if (net_progress <= 0.0 && arc->line_length > 0) {
        tc_debug_print("net_progress = %f, line_length = %f\n", net_progress, arc->line_length);
        //Get position on line (not actually an angle in this case)
        VecScalMult(&arc->uTan, net_progress, out);
        VecVecAdd(out, &arc->start, out);
    } else {
        double angle_in = net_progress / arc->radius;
        tc_debug_print("angle_in = %f, angle_total = %f\n", angle_in, arc->angle);
        double scale0 = sin(arc->angle - angle_in) / arc->Sangle;
        double scale1 = sin(angle_in) / arc->Sangle;

        Vector6 interp0,interp1;
        VecScalMult(&arc->rStart, scale0, &interp0);
        VecScalMult(&arc->rEnd, scale1, &interp1);

        VecVecAdd(&interp0, &interp1, out);
        VecVecAddEq(out, &arc->center);
    }
    return TP_ERR_OK;
}

int arcLength(SphericalArc const * const arc, double * const length)
{
    *length = arc->radius * arc->angle + arc->line_length;
    tp_debug_print("arc length = %g\n", *length);
    return TP_ERR_OK;
}


int arcConvexTest(PmCartesian const * const center,
        PmCartesian const * const P, PmCartesian const * const uVec, int reverse_dir)
{
    //Check if an arc-line intersection is concave or convex
    double dot;
    PmCartesian diff;
    pmCartCartSub(P, center, &diff);
    pmCartCartDot(&diff, uVec, &dot);

    tp_debug_print("convex test: dot = %f, reverse_dir = %d\n", dot, reverse_dir);
    int convex = (reverse_dir != 0) ^ (dot < 0);
    return convex;
}

int arcTangent(SphericalArc const * const arc, PmCartesian * const tan, int at_end)
{
    if (!arc || !tan) {
        return TP_ERR_MISSING_INPUT;
    }

    // This section implements the derivative of the SLERP formula to get a
    // local tangent vector.
    const double theta = arc->angle;
    const double t = (double)at_end;
    const double k = theta / arc->Sangle;
    const double k0 = -cos( (1.0 - t) * theta);
    const double k1 = cos( t * theta);

    Vector6 dp0, dp1;

    // Ugly sequence to build up tangent vector from components of the derivative
    VecScalMult(&arc->rStart, k * k0, &dp0);
    VecScalMult(&arc->rEnd, k * k1, &dp1);

    // Merge both components into dp1 (reuses dp1)
    VecVecAddEq(&dp1, &dp0);

    // tangential vector complete, now normalize
    VecToCart(&dp1, tan, NULL);

    // Get unit vector after UVW components are stripped away
    pmCartUnitEq(tan);
    return TP_ERR_OK;
}
