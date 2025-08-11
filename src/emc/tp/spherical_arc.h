/********************************************************************
 * Description: spherical_arc.h
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
#ifndef SPHERICAL_ARC_H
#define SPHERICAL_ARC_H

#include "posemath.h"

#define ARC_POS_EPSILON 1e-12
#define ARC_MIN_RADIUS 1e-12
#define ARC_MIN_ANGLE 1e-6

typedef struct {
    // Three defining points for the arc
    PmCartesian start;
    PmCartesian end;
    PmCartesian center;
    // Relative vectors from center to start and center to end
    // These are cached here since they'll be reused during SLERP
    PmCartesian rStart;
    PmCartesian rEnd;
    PmCartesian uTan;   /* Tangent vector at start of arc (copied from
                           prev. tangent line)*/
    double radius;
    double spiral;
    // Angle that the arc encloses
    double angle;
    double Sangle;
    double line_length;
} SphericalArc;

int arcInitFromPoints(SphericalArc * const arc,
    PmCartesian const * const start,
    PmCartesian const * const end,
    PmCartesian const * const center, const PmCartesian * const uTan, double prev_line_length);

int arcPoint(SphericalArc const * const arc, double angle_in, PmCartesian * const out);

double arcLength(SphericalArc const * const arc);

int arcTangent(SphericalArc const * const arc, const double t, PmCartesian * const out);
#endif
