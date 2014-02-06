/********************************************************************
* Description: blendmath.h
*   Circular arc blend math functions
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2014 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef BLENDMATH_H
#define BLENDMATH_H

#include "posemath.h"
#include "tc.h"

/* LineArc data */
//TODO add other useful stuff here as well, like normals.
//TODO make this a general struct for all blend types?
typedef struct {
    PmCartesian u1;
    PmCartesian u2;
    PmCartesian P;
    PmCartesian C1;
    double R1;
    double phi1;
    double tolerance;
    double L2;
    double v_req;
    double a_max;

    //These fields are populated by the ProcessLineArc function
    double v_plan;
    double v_actual;
    double dphi1;
    double R_plan;
    double d_plan;
    PmCartesian C;
    PmCartesian Q1;
    PmCartesian Q2;
    PmCartesian normal;
    PmCartesian binormal;
} LineArcData;

typedef struct {
    PmCartesian u1;     /* unit vector along line 1 */
    PmCartesian u2;     /* unit vector along line 2 */
    PmCartesian P;      /* Intersection point */
    
    double tolerance;   /* Net blend tolerance (min of line 1 and 2) */
    double L1;          /* Available part of line 1 to blend over */
    double L2;          /* Available part of line 2 to blend over */
    double v_req;       /* requsted velocity for the blend arc */
    double a_max;       /* max acceleration allowed for blend */

    // These fields are considered "output"
    double theta;       /* Intersection angle, half of angle between -u1 and u2 */
    double phi;         /* supplement of intersection angle, angle between u1 and u2 */
    double a_n_max;     /* max normal acceleration allowed */
    PmCartesian arc_start;     /* start point for blend arc */
    PmCartesian arc_end;     /* end point for blend arc */
    PmCartesian arc_center;     /* center point for blend arc */

    double R_plan;      /* planned radius for blend arc */
    double d_plan;      /* distance along each line to Q1, Q2 */

    double v_goal;      /* desired velocity at max feed override */
    double v_plan;      /* planned max velocity at max feed override */
    double v_actual;    /* velocity at feedscale = 1.0 */
    int consume;        /* Consume the previous segment */

    PmCartesian normal;
    PmCartesian binormal;
} LineLineData;


double fsign(double f);

int clip_min(double * const x, double min);

int clip_max(double * const x, double max);

double saturate(double x, double max);

int sat_inplace(double * const x, double max);

int bmLineArcProcess(LineArcData * const);

int findIntersectionAngle(PmCartesian const * const u1,
        PmCartesian const * const u2, double * const theta);

double pmCartMin(PmCartesian const * const in);

int calculateInscribedDiameter(PmCartesian const * const normal,
        PmCartesian const * const bounds, double * const diameter);

int bmLineLineInit(LineLineData * const data, TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        PmCartesian const * const acc_bound,
        PmCartesian const * const vel_bound,
        double maxFeedScale);

int bmLineLineCalculateNormals(LineLineData * const data);

int bmLineLineParameters(LineLineData * const data);

int bmLineLineCheckConsume(LineLineData * const data,
        TC_STRUCT const * const prev_tc, int gap_cycles);

int bmLineLinePoints(LineLineData * const data);

int bmArcFromLineLine(SphericalArc * const arc, LineLineData const * const data);

#endif
