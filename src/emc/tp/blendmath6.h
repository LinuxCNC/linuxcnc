/********************************************************************
* Description: blendmath.h
*   Circular arc blend math functions
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2014-2015 All rights reserved.
*
********************************************************************/
#ifndef BLENDMATH6_H
#define BLENDMATH6_H

//TODO refactor so we don't depend on this
#include "blendmath.h"
#include "vector6.h"
/**
 * 6D Input geometry for a spherical blend arc.
 */
typedef struct {
    Vector6 u1;         /* unit vector along line 1 */
    Vector6 u2;         /* unit vector along line 2 */
    Vector6 P;          /* Intersection point */
    Vector6 normal;
    double theta;
    double v_max1;          /* maximum velocity in direction u1 */
    double v_max2;          /* maximum velocity in direction u2 */
} BlendGeom6;

typedef struct {
    Vector6 arc_start;      /* start point for blend arc */
    Vector6 arc_end;        /* end point for blend arc */
    Vector6 arc_center;     /* center point for blend arc */
    double trim1;               /* length to cut from prev_tc */
    double trim2;               /* length to cut from tc */
} BlendPoints6;


/** @section blend6d 6D blending functions for XYZUVW blending */

int findIntersectionAngle6(Vector6 const * const u1,
        Vector6 const * const u2,
        double * const theta);

int blendCalculateNormals6(BlendGeom6 * const geom);

int blendFindPoints6(BlendPoints6 * const points,
        BlendGeom6 const * const geom,
        BlendParameters const * const param);

int blendGeom6Init(BlendGeom6 * const geom,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc);

int blendParamKinematics6(BlendGeom6 * const geom,
        BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        Vector6 const * const acc_bound,
        Vector6 const * const vel_bound,
        double maxFeedScale);

int blendInit6FromLineLine(BlendGeom6 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        Vector6 const * const acc_bound,
        Vector6 const * const vel_bound,
        double maxFeedScale);

int arcFromBlendPoints6(SphericalArc * const arc, BlendPoints6 const * const points,
        BlendGeom6 const * const geom, BlendParameters const * const param);

int findOrthonormalBasis(Vector6 const * const a,
        Vector6 const * const b,
        Vector6 * const u,
        Vector6 * const v);

int calcBoundScales(Vector6 const * const u,
        Vector6 const * const v,
        Vector6 * scales);

int calcMinBound(Vector6 const * scales,
        Vector6 const * bounds,
        double * min_bound);
#endif
