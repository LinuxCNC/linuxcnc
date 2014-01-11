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

typedef enum {
    ARC_ERR_OK = 0,
    ARC_ERR_FAIL = -1,
    ARC_ERR_MISSING_INPUT = -2,
    ARC_ERR_MISSING_OUTPUT = -3,
    ARC_ERR_RANGE = -4,
    ARC_ERR_GEOM = -5,
    ARC_ERR_RADIUS = -6
} SphericalArcErr;

typedef struct {
    // Three defining points for the arc
    PmCartesian start;
    PmCartesian end;
    PmCartesian center;
    // Relative vectors from center to start and center to end
    // These are cached here since they'll be reused during SLERP
    PmCartesian rStart;
    PmCartesian rEnd;
    double radius;
    // Angle that the arc encloses
    double angle;
} SphericalArc;

typedef struct {
    SphericalArc xyz;
    PmCartLine abc;
    PmCartLine uvw;
} Arc9;

int arcInitFromPoints(SphericalArc * const arc, PmCartesian const * const start,
        PmCartesian const * const end, PmCartesian const * const center);

int arcInitFromVectors(SphericalArc * const arc, PmCartesian const * const vec0,
        PmCartesian const * const vec1,
        PmCartesian const * const center);

int arcPoint(SphericalArc const * const arc, double angle_in, PmCartesian * const out);

int arcNormalizedSlerp(SphericalArc const * const arc, double t, PmCartesian * const out);

int arcLength(SphericalArc const * const arc, double * const length);
#endif
