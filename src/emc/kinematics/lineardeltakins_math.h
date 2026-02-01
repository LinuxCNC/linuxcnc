/********************************************************************
 * Description: lineardeltakins_math.h
 *   Pure math functions for linear delta (Rostock-style) kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC (refactored from lineardeltakins-common.h)
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2013 Jeff Epler <jepler@unpythonic.net>
 * Copyright (c) 2024 All rights reserved.
 *
 * Notes:
 *   Towers 0, 1 and 2 are spaced at 120 degrees around the origin
 *   at distance R. A rod of length L (L > R) connects each tower to the
 *   platform. Tower 0 is at (0,R). (note: this is not at zero radians!)
 *
 *   ABCUVW coordinates are passed through in joints[3..8].
 *
 *   L is like DELTA_DIAGONAL_ROD and R is like DELTA_RADIUS in
 *   Marlin---remember to account for the effector and carriage offsets
 *   when changing from the default.
 *
 * Inspired by Marlin delta firmware and https://gist.github.com/kastner/5279172
 ********************************************************************/

#ifndef LINEARDELTAKINS_MATH_H
#define LINEARDELTAKINS_MATH_H

#include "emcpos.h"

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef M_SQRT3
#define M_SQRT3 1.7320508075688772935
#endif

#define LINDELTA_SIN_60 (M_SQRT3/2.0)
#define LINDELTA_COS_60 (0.5)

/* Parameters struct - matches kinematics_params.h kins_lineardelta_params_t */
typedef struct {
    double radius;          /* R - horizontal distance from origin to tower */
    double rod_length;      /* L - length of diagonal rod */
} lineardelta_params_t;

/* Pre-computed geometry for efficiency (derived from params) */
typedef struct {
    double ax, ay;      /* Tower A position (0, R) */
    double bx, by;      /* Tower B position (-sin60*R, -cos60*R) */
    double cx, cy;      /* Tower C position (sin60*R, -cos60*R) */
    double l2;          /* L squared */
} lineardelta_geometry_t;

/* Helper: square function */
#ifndef lindelta_sq
#define lindelta_sq(x) ((x)*(x))
#endif

/*
 * Compute derived geometry from parameters
 *
 * params: kinematics parameters (radius, rod_length)
 * geom: output geometry struct
 */
static inline void lineardelta_compute_geometry(const lineardelta_params_t *params,
                                                  lineardelta_geometry_t *geom)
{
    double R = params->radius;
    double L = params->rod_length;

    geom->ax = 0.0;
    geom->ay = R;

    geom->bx = -LINDELTA_SIN_60 * R;
    geom->by = -LINDELTA_COS_60 * R;

    geom->cx = LINDELTA_SIN_60 * R;
    geom->cy = -LINDELTA_COS_60 * R;

    geom->l2 = lindelta_sq(L);
}

/*
 * Pure inverse kinematics - Cartesian to joint positions
 *
 * Given controlled point position, compute the three tower carriage heights.
 *
 * params: kinematics parameters (radius, rod_length)
 * world: input world position (EmcPose)
 * joints: output joint positions array
 *
 * Returns 0 on success, -1 if position is out of reach
 */
static inline int lineardelta_inverse_math(const lineardelta_params_t *params,
                                            const EmcPose *world,
                                            double *joints)
{
    lineardelta_geometry_t geom;
    double x = world->tran.x;
    double y = world->tran.y;
    double z = world->tran.z;

    lineardelta_compute_geometry(params, &geom);

    joints[0] = z + sqrt(geom.l2 - lindelta_sq(geom.ax - x) - lindelta_sq(geom.ay - y));
    joints[1] = z + sqrt(geom.l2 - lindelta_sq(geom.bx - x) - lindelta_sq(geom.by - y));
    joints[2] = z + sqrt(geom.l2 - lindelta_sq(geom.cx - x) - lindelta_sq(geom.cy - y));
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    if (isnan(joints[0]) || isnan(joints[1]) || isnan(joints[2]))
        return -1;

    return 0;
}

/*
 * Pure forward kinematics - joint positions to Cartesian
 *
 * Given three tower carriage heights, compute the controlled point position.
 *
 * params: kinematics parameters (radius, rod_length)
 * joints: input joint positions array
 * world: output world position (EmcPose)
 *
 * Returns 0 on success, -1 if position is invalid (triangle inequality violated)
 */
static inline int lineardelta_forward_math(const lineardelta_params_t *params,
                                            const double *joints,
                                            EmcPose *world)
{
    lineardelta_geometry_t geom;
    double q1, q2, q3;
    double den, w1, w2, w3;
    double a1, b1, a2, b2;
    double a, b, c, discr, z;

    lineardelta_compute_geometry(params, &geom);

    q1 = joints[0];
    q2 = joints[1];
    q3 = joints[2];

    den = (geom.by - geom.ay) * geom.cx - (geom.cy - geom.ay) * geom.bx;

    /* n.b. assumption that Ax is 0 all through here */
    w1 = lindelta_sq(geom.ay) + lindelta_sq(q1);
    w2 = lindelta_sq(geom.bx) + lindelta_sq(geom.by) + lindelta_sq(q2);
    w3 = lindelta_sq(geom.cx) + lindelta_sq(geom.cy) + lindelta_sq(q3);

    a1 = (q2 - q1) * (geom.cy - geom.ay) - (q3 - q1) * (geom.by - geom.ay);
    b1 = -((w2 - w1) * (geom.cy - geom.ay) - (w3 - w1) * (geom.by - geom.ay)) / 2.0;

    a2 = -(q2 - q1) * geom.cx + (q3 - q1) * geom.bx;
    b2 = ((w2 - w1) * geom.cx - (w3 - w1) * geom.bx) / 2.0;

    /* a*z^2 + b*z + c = 0 */
    a = lindelta_sq(a1) + lindelta_sq(a2) + lindelta_sq(den);
    b = 2 * (a1 * b1 + a2 * (b2 - geom.ay * den) - q1 * lindelta_sq(den));
    c = lindelta_sq(b2 - geom.ay * den) + lindelta_sq(b1) +
        lindelta_sq(den) * (lindelta_sq(q1) - geom.l2);

    discr = lindelta_sq(b) - 4.0 * a * c;
    if (discr < 0)
        return -1;  /* non-existing point */

    z = -0.5 * (b + sqrt(discr)) / a;
    world->tran.z = z;
    world->tran.x = (a1 * z + b1) / den;
    world->tran.y = (a2 * z + b2) / den;
    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];

    return 0;
}

/* Default values which may correspond to someone's linear delta robot.
 * To change these, use halcmd setp rather than rebuilding the software.
 */

/* Center-to-center distance of the holes in the diagonal push rods. */
#define LINEARDELTA_DEFAULT_ROD_LENGTH 269.0  /* mm (DELTA_DIAGONAL_ROD) */

/* Horizontal offset from middle of printer to smooth rod center. */
#define LINEARDELTA_SMOOTH_ROD_OFFSET 198.25  /* mm */

/* Horizontal offset of the universal joints on the end effector. */
#define LINEARDELTA_EFFECTOR_OFFSET 33.0  /* mm */

/* Horizontal offset of the universal joints on the carriages. */
#define LINEARDELTA_CARRIAGE_OFFSET 35.0  /* mm */

/* Effective horizontal distance bridged by diagonal push rods. */
#define LINEARDELTA_DEFAULT_RADIUS (LINEARDELTA_SMOOTH_ROD_OFFSET - \
                                     LINEARDELTA_EFFECTOR_OFFSET - \
                                     LINEARDELTA_CARRIAGE_OFFSET)  /* mm */

#endif /* LINEARDELTAKINS_MATH_H */
