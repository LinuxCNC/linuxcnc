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
#include "vector6.h"

#define ARC_POS_EPSILON 1e-12
#define ARC_MIN_RADIUS 1e-12
#define ARC_MIN_ANGLE 1e-6
//FIXME relate this to cornering acceleration?
#define ARC_ABS_ERR 5e-4
#define ARC_REL_ERR 5e-4

typedef struct {
    // Three defining points for the arc
    Vector6 start;
    Vector6 end;
    Vector6 center;
    // Relative vectors from center to start and center to end
    // These are cached here since they'll be reused during SLERP
    Vector6 rStart;
    Vector6 rEnd;
    Vector6 uTan;   /* Tangent vector at start of arc (copied from
                           prev. tangent line)*/
    double radius;
    double spiral;
    // Angle that the arc encloses
    double angle;
    double Sangle;
    double line_length;
} SphericalArc;


int arcInitFromPoints(SphericalArc * const arc, Vector6 const * const start,
        Vector6 const * const end, Vector6 const * const center);

int arcInitFromVectors(SphericalArc * const arc, Vector6 const * const vec0,
        Vector6 const * const vec1,
        Vector6 const * const center);

int arcPoint(SphericalArc const * const arc, double angle_in, Vector6 * const out);

int arcPointCart(SphericalArc const * const arc, 
        double angle_in,
        PmCartesian * xyz,
        PmCartesian * uvw);

int arcNormalizedSlerp(SphericalArc const * const arc, double t, Vector6 * const out);

int arcLength(SphericalArc const * const arc, double * const length);

int arcConvexTest(PmCartesian const * const center,
        PmCartesian const * const P, PmCartesian const * const uVec, int reverse_dir);

int arcTangent(SphericalArc const * const arc, PmCartesian * const tan, int at_end);
#endif
