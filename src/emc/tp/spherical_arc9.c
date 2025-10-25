/**
 * @file spherical_arc9.c
 * 
 * API for 9D version of "spherical arc" (an arc on the surface of a 9D sphere)
 *
 * @author Robert W. Ellenberg <rwe24g@gmail.com>
 *
 * @copyright Copyright 2019, Robert W. Ellenberg
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License (V2) as published by the Free Software Foundation.
 */

#include "spherical_arc9.h"
#include "posemath.h"
#include "pm_vector.h"
#include "rtapi_math.h"
#include "tp_enums.h"

int arc9InitFromPoints(
    SphericalArc9 * const arc,
    PmVector const * const start,
    PmVector const * const end,
    PmVector const * const center,
    PmVector const * const uTan,
    double prev_line_length)
{
#ifdef ARC_PEDANTIC
    if (!start || !end || !center) {
        return TP_ERR_MISSING_INPUT;
    }
    if (!arc) {
        return TP_ERR_MISSING_OUTPUT;
    }
#endif

    if (prev_line_length < -1e-17) {
        return TP_ERR_INVALID;
    }

    arc->uTan = *uTan;
    arc->line_length = fmax(prev_line_length, 0.0);

    // Store the start, end, and center
    arc->start = *start;
    arc->end = *end;
    arc->center = *center;

    VecVecSub(start, center, &arc->rStart);
    VecVecSub(end, center, &arc->rEnd);

    // Find the radii at start and end. These are identical for a perfect spherical arc
    double radius0 = VecMag(&arc->rStart);
    double radius1 = VecMag(&arc->rEnd);

    if (radius0 < ARC_MIN_RADIUS || radius1 < ARC_MIN_RADIUS) {
        return TP_ERR_RADIUS_TOO_SMALL;
    }

    // Choose initial radius as nominal radius
    arc->radius = radius0;

    // Get unit vectors from center to start and center to end
    PmVector u0, u1;
    VecScalMult(&arc->rStart, 1.0 / radius0, &u0);
    VecScalMult(&arc->rEnd, 1.0 / radius1, &u1);

    // Find arc angle
    double dot = VecVecDot(&u0, &u1);
    arc->angle = acos(dot);

    // Store spiral factor as radial difference. Archimedean spiral coef. a = spiral / angle
    arc->spiral = (radius1 - radius0 );

    if (arc->angle < ARC_MIN_ANGLE) {
        return TP_ERR_GEOM;
    }

    // Store sin of arc angle since it is reused many times for SLERP
    arc->Sangle = sin(arc->angle);

    return TP_ERR_OK;
}

int arc9Point(SphericalArc9 const * const arc, double progress, PmVector * const out)
{
#ifdef ARC_PEDANTIC
    if (!arc) {return TP_ERR_MISSING_INPUT;}
    if (!out) {return TP_ERR_MISSING_OUTPUT;}
#endif

    //Convert progress to actual progress around the arc
    double net_progress = progress - arc->line_length;
    if (net_progress <= 0.0 && arc->line_length > 0) {
        //Get position on line (not actually an angle in this case)
        VecScalMult(&arc->uTan, net_progress, out);
        VecVecAdd(out, &arc->start, out);
    } else {
        double angle_in = net_progress / arc->radius;
        double scale0 = sin(arc->angle - angle_in) / arc->Sangle;
        double scale1 = sin(angle_in) / arc->Sangle;

        PmVector interp0,interp1;
        VecScalMult(&arc->rStart, scale0, &interp0);
        VecScalMult(&arc->rEnd, scale1, &interp1);

        VecVecAdd(&interp0, &interp1, out);
        VecVecAdd(&arc->center, out, out);
    }
    return TP_ERR_OK;
}

double arc9Length(SphericalArc9 const * const arc)
{
    return arc->radius * arc->angle + arc->line_length;
}

int arc9Tangent(SphericalArc9 const * const arc, double const t, PmVector * const out)
{
    if (!arc || !out) {
        return TP_ERR_MISSING_INPUT;
    }

    // This section implements the derivative of the SLERP formula to get a
    // local tangent vector.
    const double theta = arc->angle;
    const double k = theta / arc->Sangle;
    const double k0 = -cos( (1.0 - t) * theta);
    const double k1 = cos( t * theta);

    PmVector dp0,dp1;

    // Ugly sequence to build up tangent vector from components of the derivative
    VecScalMult(&arc->rStart, k * k0, &dp0);
    VecScalMult(&arc->rEnd, k * k1, &dp1);
    VecVecAdd(&dp0, &dp1, out);

    // tangential vector complete, now normalize
    VecUnitEq(out);

    return TP_ERR_OK;
}

double arc9VLimit(const SphericalArc9 * const arc, double v_target, double v_limit_linear, double v_limit_angular)
{
    return VecVLimit(&arc->uTan, v_target, v_limit_linear, v_limit_angular);
}
