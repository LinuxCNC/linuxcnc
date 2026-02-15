/********************************************************************
 * Description: rotarydeltakins_math.h
 *   Pure math functions for rotary delta robot kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC (refactored from rotarydeltakins-common.h)
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2013 Chris Radek <chris@timeguy.com>
 * Copyright (c) 2024 All rights reserved.
 *
 * Notes:
 *   Based on work by "mzavatsky" at:
 *   http://forums.trossenrobotics.com/tutorials/introduction-129/delta-robot-kinematics-3276/
 *   "You can freely use this code in your applications."
 *
 *   Which was based on:
 *   Descriptive Geometric Kinematic Analysis of Clavel's "Delta" Robot
 *   P.J. Zsombor-Murray, McGill University
 *
 *   The platform is on "top", the origin is in the center of the plane
 *   containing the three hip joints.  Z points upward, so Z coordinates
 *   are always negative.  Thighs always point outward, straight out
 *   (knee at Z=0) is considered zero degrees for the angular hip joint.
 *   Positive rotation is knee-downward, so if you rotate all knees
 *   positive, the Z coordinate will get more negative.
 *
 *   Joint zero is the one whose thigh swings in the YZ plane.
 *
 *   ABCUVW coordinates are passed through in joints[3..8].
 ********************************************************************/

#ifndef ROTARYDELTAKINS_MATH_H
#define ROTARYDELTAKINS_MATH_H

#include "emcpos.h"

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Helper macros */
#ifndef rotdelta_sq
#define rotdelta_sq(a) ((a)*(a))
#endif

#ifndef rotdelta_D2R
#define rotdelta_D2R(d) ((d)*M_PI/180.0)
#endif

/* Parameters struct - matches kinematics_params.h kins_rotarydelta_params_t */
typedef struct {
    double platformradius;  /* distance from origin to a hip joint */
    double thighlength;     /* thigh connects the hip to the knee */
    double shinlength;      /* shin (the parallelogram) connects the knee to the foot */
    double footradius;      /* distance from center of foot (controlled point) to an ankle joint */
} rotarydelta_params_t;

/*
 * Helper: rotate a 2D point around the origin
 *
 * x, y: pointer to coordinates (modified in place)
 * theta: rotation angle in radians
 */
static inline void rotarydelta_rotate(double *x, double *y, double theta)
{
    double xx = *x;
    double yy = *y;
    *x = xx * cos(theta) - yy * sin(theta);
    *y = xx * sin(theta) + yy * cos(theta);
}

/*
 * Helper: compute single joint angle (J0 is in the ZY plane)
 *
 * x, y, z: controlled point position
 * params: kinematics parameters
 * theta: output joint angle in degrees
 *
 * Returns 0 on success, -1 if position is out of reach
 */
static inline int rotarydelta_inverse_j0(double x, double y, double z,
                                          const rotarydelta_params_t *params,
                                          double *theta)
{
    double a, b, d, knee_y, knee_z;
    double pfr = params->platformradius;
    double tl = params->thighlength;
    double sl = params->shinlength;
    double fr = params->footradius;

    a = 0.5 * (rotdelta_sq(x) + rotdelta_sq(y - fr) + rotdelta_sq(z) +
               rotdelta_sq(tl) - rotdelta_sq(sl) - rotdelta_sq(pfr)) / z;
    b = (fr - pfr - y) / z;

    d = rotdelta_sq(tl) * (rotdelta_sq(b) + 1) - rotdelta_sq(a - b * pfr);
    if (d < 0) return -1;

    knee_y = (pfr + a*b + sqrt(d)) / (rotdelta_sq(b) + 1);
    knee_z = b * knee_y - a;

    *theta = atan2(knee_z, knee_y - pfr);
    *theta *= 180.0/M_PI;
    return 0;
}

/*
 * Pure inverse kinematics - Cartesian to joint positions
 *
 * Given controlled point position, compute the three hip joint angles.
 *
 * params: kinematics parameters
 * world: input world position (EmcPose)
 * joints: output joint positions array
 *
 * Returns 0 on success, -1 if position is out of reach
 */
static inline int rotarydelta_inverse_math(const rotarydelta_params_t *params,
                                            const EmcPose *world,
                                            double *joints)
{
    double xr, yr;

    /* Joint 0 is in the ZY plane, compute directly */
    if (rotarydelta_inverse_j0(world->tran.x, world->tran.y, world->tran.z,
                                params, &joints[0]))
        return -1;

    /* Use symmetry property to get the other two joints */
    xr = world->tran.x;
    yr = world->tran.y;
    rotarydelta_rotate(&xr, &yr, -2*M_PI/3);
    if (rotarydelta_inverse_j0(xr, yr, world->tran.z, params, &joints[1]))
        return -1;

    xr = world->tran.x;
    yr = world->tran.y;
    rotarydelta_rotate(&xr, &yr, 2*M_PI/3);
    if (rotarydelta_inverse_j0(xr, yr, world->tran.z, params, &joints[2]))
        return -1;

    /* Pass through ABCUVW */
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    return 0;
}

/*
 * Pure forward kinematics - joint positions to Cartesian
 *
 * Given three hip joint angles, compute the controlled point position.
 *
 * params: kinematics parameters
 * joints: input joint positions array
 * world: output world position (EmcPose)
 *
 * Returns 0 on success, -1 if position is invalid
 */
static inline int rotarydelta_forward_math(const rotarydelta_params_t *params,
                                            const double *joints,
                                            EmcPose *world)
{
    double pfr = params->platformradius;
    double tl = params->thighlength;
    double sl = params->shinlength;
    double fr = params->footradius;

    double j0, j1, j2;
    double y1, z1;              /* x1 is 0 */
    double x2, y2, z2;
    double x3, y3, z3;
    double a1, b1, a2, b2;
    double w1, w2, w3;
    double denom;
    double a, b, c, d;

    j0 = rotdelta_D2R(joints[0]);
    j1 = rotdelta_D2R(joints[1]);
    j2 = rotdelta_D2R(joints[2]);

    y1 = -(pfr - fr + tl * cos(j0));
    z1 = -tl * sin(j0);

    y2 = (pfr - fr + tl * cos(j1)) * 0.5;
    x2 = y2 * sqrt(3);
    z2 = -tl * sin(j1);

    y3 = (pfr - fr + tl * cos(j2)) * 0.5;
    x3 = -y3 * sqrt(3);
    z3 = -tl * sin(j2);

    denom = x3 * (y2 - y1) - x2 * (y3 - y1);

    w1 = rotdelta_sq(y1) + rotdelta_sq(z1);
    w2 = rotdelta_sq(x2) + rotdelta_sq(y2) + rotdelta_sq(z2);
    w3 = rotdelta_sq(x3) + rotdelta_sq(y3) + rotdelta_sq(z3);

    a1 = (z2-z1) * (y3-y1) - (z3-z1) * (y2-y1);
    b1 = -((w2-w1) * (y3-y1) - (w3-w1) * (y2-y1)) / 2.0;

    a2 = -(z2 - z1) * x3 + (z3 - z1) * x2;
    b2 = ((w2 - w1) * x3 - (w3 - w1) * x2) / 2.0;

    /* a*z^2 + b*z + c = 0 */
    a = rotdelta_sq(a1) + rotdelta_sq(a2) + rotdelta_sq(denom);
    b = 2 * (a1 * b1 + a2 * (b2 - y1 * denom) - z1 * rotdelta_sq(denom));
    c = (b2 - y1 * denom) * (b2 - y1 * denom) +
        rotdelta_sq(b1) + rotdelta_sq(denom) * (rotdelta_sq(z1) - rotdelta_sq(sl));

    d = rotdelta_sq(b) - 4 * a * c;
    if (d < 0) return -1;

    world->tran.z = (-b - sqrt(d)) / (2 * a);
    world->tran.x = (a1 * world->tran.z + b1) / denom;
    world->tran.y = (a2 * world->tran.z + b2) / denom;

    /* Pass through ABCUVW */
    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];

    return 0;
}

/* Default values for rotary delta robot parameters.
 * To change these, use halcmd setp rather than rebuilding the software.
 */
#define ROTARYDELTA_DEFAULT_PLATFORMRADIUS 10.0
#define ROTARYDELTA_DEFAULT_THIGHLENGTH    10.0
#define ROTARYDELTA_DEFAULT_SHINLENGTH     14.0
#define ROTARYDELTA_DEFAULT_FOOTRADIUS      6.0

#endif /* ROTARYDELTAKINS_MATH_H */
