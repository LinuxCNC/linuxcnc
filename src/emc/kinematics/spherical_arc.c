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

    return ARC_ERR_OK;
}

int arcSlerp(SphericalArc const * const arc, double angle_in, PmCartesian * const out)
{
    //TODO pedantic

    double scale0 = sin(arc->angle - angle_in) / sin(arc->angle);
    double scale1 = sin(angle_in) / sin(arc->angle);

    PmCartesian interp0,interp1;
    pmCartScalMult(&arc->rStart, scale0, &interp0);
    pmCartScalMult(&arc->rEnd, scale1, &interp1);

    pmCartCartAdd(&interp0, &interp1, out);
    return ARC_ERR_OK;
}
