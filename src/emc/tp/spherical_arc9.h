#ifndef SPHERICAL_ARC9_H
#define SPHERICAL_ARC9_H

#include "pm_vector.h"

#define ARC_POS_EPSILON 1e-12
#define ARC_MIN_RADIUS 1e-12
#define ARC_MIN_ANGLE 1e-6

typedef struct {
    // Three defining points for the arc
    PmVector start;
    PmVector end;
    PmVector center;
    // Relative vectors from center to start and center to end
    // These are cached here since they'll be reused during SLERP
    PmVector rStart;
    PmVector rEnd;
    PmVector uTan;   /* Tangent vector at start of arc (copied from
                           prev. tangent line)*/
    double radius;
    double spiral;
    // Angle that the arc encloses
    double angle;
    double Sangle;
    double line_length;
} SphericalArc9;

int arc9InitFromPoints(
    SphericalArc9 * const arc,
    PmVector const * const start,
    PmVector const * const end,
    PmVector const * const center,
    const PmVector * const uTan,
    double prev_line_length);

int arc9Point(SphericalArc9 const * const arc, double angle_in, PmVector * const out);

double arc9Length(SphericalArc9 const * const arc);

int arc9Tangent(SphericalArc9 const * const arc, const double t, PmVector * const out);

double arc9VLimit(SphericalArc9 const * const arc, double v_target, double v_limit_linear, double v_limit_angular);
#endif // SPHERICAL_ARC9_H
