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
#include "spherical_arc.h"
#include "rtapi_math.h"
#include "rtapi.h"
#include "tp_debug.h"

int arcInitFromPoints(SphericalArc * const arc, PmCartesian const * const start,
        PmCartesian const * const end,
        PmCartesian const * const center)
{
#ifdef ARC_PEDANTIC
    if (!P0 || !P1 || !center) {
        return ARC_ERR_MISSING_INPUT;
    
    if (!arc) {
        return ARC_ERR_MISSING_OUTPUT;
    }
#endif
    arc->start = *start;
    arc->end = *end;
    arc->center = *center;

    pmCartCartSub(start, center, &arc->rStart);
    pmCartCartSub(end, center, &arc->rEnd);

    //Check that center is equidistant from both start and end. If not, the arc is not circular, and won't be tangent at the ends.
    //TODO more formal tolerance here? current limit is very strict
    double mag0,mag1;
    pmCartMag(&arc->rStart, &mag0);
    pmCartMag(&arc->rEnd, &mag1);

    //TODO take angle as input? might save computations
    // Find angle between vectors to get arc angle
    PmCartesian u0,u1;
    pmCartUnit(&arc->rStart, &u0);
    pmCartUnit(&arc->rEnd, &u1);

    //Correct center by 1/2 error method
    double err = mag1-mag0;
    /*PmCartesian diff;*/
    /*pmCartCartSub(end,start,&diff);*/

    tp_debug_print("radii are %f and %f, difference is %g\n",mag0,mag1,mag1-mag0);
    /*tp_debug_print("new center is = %f %f %f\n",*/
            /*arc->center.x,*/
            /*arc->center.y,*/
            /*arc->center.z);*/

    // estimate radius of spiral shape by average of two lengths
    arc->radius = (mag0 + mag1) / 2.0;

    if (mag0 < ARC_MIN_RADIUS) {
        tp_debug_print("radius %f below min radius %f, aborting arc\n",
                arc->radius,
                ARC_MIN_RADIUS);
        return ARC_ERR_RADIUS;
    }

    double dot;
    pmCartCartDot(&u0,&u1,&dot);
    arc->angle = acos(dot);
    tp_debug_print("angle = %f\n", arc->angle);

    if (arc->angle < ARC_MIN_ANGLE) {
        tp_debug_print("angle %f below min angle %f, aborting arc\n",
                arc->angle,
                ARC_MIN_ANGLE);
        return ARC_ERR_GEOM;
    }
    arc->Sangle = sin(arc->angle);

    return ARC_ERR_OK;
}

int arcPoint(SphericalArc const * const arc, double progress, PmCartesian * const out)
{
    //TODO pedantic

    //Convert progress to actual progress around the arc
    double net_progress = progress - arc->line_length;
    if (net_progress <= 0.0 && arc->line_length > 0) {
        tp_debug_print("net_progress = %f, line_length = %f\n", net_progress, arc->line_length);
        //Get position on line (not actually an angle in this case)
        pmCartScalMult(&arc->uTan, net_progress, out);
        pmCartCartAdd(out, &arc->start, out);
    } else {
        double angle_in = net_progress / arc->radius;
        tp_debug_print("angle_in = %f, angle_total = %f\n", angle_in, arc->angle);
        double scale0 = sin(arc->angle - angle_in) / arc->Sangle;
        double scale1 = sin(angle_in) / arc->Sangle;

        PmCartesian interp0,interp1;
        pmCartScalMult(&arc->rStart, scale0, &interp0);
        pmCartScalMult(&arc->rEnd, scale1, &interp1);

        pmCartCartAdd(&interp0, &interp1, out);
        pmCartCartAdd(&arc->center, out, out);
    }
    return ARC_ERR_OK;
}

int arcLength(SphericalArc const * const arc, double * const length)
{
    *length = arc->radius * arc->angle + arc->line_length;
    tp_debug_print("arc length = %g\n", *length);
    return ARC_ERR_OK;
}

int arcFromLines(SphericalArc * const arc, PmCartLine const * const line1,
        PmCartLine const * const line2, double radius,
        double blend_dist, double center_dist, PmCartesian * const start, PmCartesian * const end, int consume) {

    PmCartesian center, normal, binormal;

    // Pointer to middle point of line segment pair
    PmCartesian const * const middle = &line1->end;
    //TODO assert line1 end = line2 start?

    //Calculate the normal direction of the arc from the difference
    //between the unit vectors
    pmCartCartSub(&line2->uVec, &line1->uVec, &normal);
    pmCartUnitEq(&normal);
    pmCartScalMultEq(&normal, center_dist);
    pmCartCartAdd(middle, &normal, &center);

    //Calculate the binormal (vector perpendicular to the plane of the
    //arc)
    pmCartCartCross(&line1->uVec, &line2->uVec, &binormal);
    pmCartUnitEq(&binormal);

    // Start point is blend_dist away from middle point in the
    // negative direction of line1
    pmCartScalMult(&line1->uVec, -blend_dist, start);
    pmCartCartAdd(start, middle, start);

    // End point is blend_dist away from middle point in the positive
    // direction of line2
    pmCartScalMult(&line2->uVec, blend_dist, end);
    pmCartCartAddEq(end, middle);

    //Handle line portion of line-arc
    arc->uTan = line1->uVec;
    if (consume) {
        arc->line_length = line1->tmag - blend_dist;
    } else {
        arc->line_length = 0;
    }

    return arcInitFromPoints(arc, start, end, &center);
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
