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

    if (fabs(mag0-mag1) > ARC_POS_EPSILON) {
        tp_debug_print("radii %f and %f are different, aborting...\n", mag0, mag1);
        return ARC_ERR_GEOM;
    }

    //Assign radius and check for validity
    arc->radius = mag0;

    if (mag0 < ARC_MIN_RADIUS) {
        return ARC_ERR_RADIUS;
    }

    //TODO take angle as input? might save computations
    // Find angle between vectors to get arc angle
    PmCartesian u0,u1;
    pmCartUnit(&arc->rStart, &u0);
    pmCartUnit(&arc->rEnd, &u1);

    double dot;
    pmCartCartDot(&u0,&u1,&dot);
    arc->angle = acos(dot);

    if (arc->angle < ARC_MIN_ANGLE) {
        return ARC_ERR_GEOM;
    }
    arc->Sangle = sin(arc->angle);

    return ARC_ERR_OK;
}

int arcPoint(SphericalArc const * const arc, double progress, PmCartesian * const out)
{
    //TODO pedantic
    tp_debug_print("progress = %f, angle_total = %f\n", progress, arc->angle);

    //Convert progress to actual progress around the arc
    double net_progress = progress - arc->line_length;
    if (net_progress <= 0.0 && arc->line_length > 0) {
        //Get position on line (not actually an angle in this case)
        pmCartScalMult(&arc->uTan, -net_progress, out);
        pmCartCartAdd(out, &arc->start, out);
    } else {
        double angle_in = net_progress / arc->radius;
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
    pmCartUnit(&normal,&normal);
    pmCartScalMult(&normal, center_dist, &normal);
    pmCartCartAdd(middle, &normal, &center);

    //Calculate the binormal (vector perpendicular to the plane of the
    //arc)
    pmCartCartCross(&line1->uVec, &line2->uVec, &binormal);
    pmCartUnit(&binormal, &binormal);

    // Start point is blend_dist away from middle point in the
    // negative direction of line1
    pmCartScalMult(&line1->uVec, -blend_dist, start);
    pmCartCartAdd(start, middle, start);

    // End point is blend_dist away from middle point in the positive
    // direction of line2
    pmCartScalMult(&line2->uVec, blend_dist, end);
    pmCartCartAdd(end, middle, end);

    //Handle line portion of line-arc
    arc->uTan = line1->uVec;
    if (consume) {
        arc->line_length = line1->tmag - blend_dist;
    } else {
        arc->line_length = 0;
    }

    return arcInitFromPoints(arc, start, end, &center);
}

