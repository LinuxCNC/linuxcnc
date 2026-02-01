/********************************************************************
 * Description: rotatekins_math.h
 *   Pure math functions for rotary table kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: Chris Radek
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2006 All rights reserved.
 ********************************************************************/

#ifndef ROTATEKINS_MATH_H
#define ROTATEKINS_MATH_H

#include "emcpos.h"

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Rotatekins - simple rotary table kinematics
 *
 * The C axis (joint 5) rotates the XY plane.
 *
 * Forward: Rotate (J0, J1) by -C to get (X, Y)
 *   X = J0 * cos(-C) - J1 * sin(-C)
 *   Y = J0 * sin(-C) + J1 * cos(-C)
 *
 * Inverse: Rotate (X, Y) by +C to get (J0, J1)
 *   J0 = X * cos(C) - Y * sin(C)
 *   J1 = X * sin(C) + Y * cos(C)
 */

/*
 * Pure forward kinematics - joints to world coordinates
 *
 * joints: input joint positions array (9 joints, angles in degrees)
 * world: output world position (EmcPose)
 *
 * Returns 0 on success
 */
static inline int rotate_forward_math(const double *joints, EmcPose *world)
{
    double c_rad = -joints[5] * M_PI / 180.0;

    world->tran.x = joints[0] * cos(c_rad) - joints[1] * sin(c_rad);
    world->tran.y = joints[0] * sin(c_rad) + joints[1] * cos(c_rad);
    world->tran.z = joints[2];
    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];

    return 0;
}

/*
 * Pure inverse kinematics - world coordinates to joints
 *
 * world: input world position (EmcPose)
 * joints: output joint positions array (9 joints)
 *
 * Returns 0 on success
 */
static inline int rotate_inverse_math(const EmcPose *world, double *joints)
{
    double c_rad = world->c * M_PI / 180.0;

    joints[0] = world->tran.x * cos(c_rad) - world->tran.y * sin(c_rad);
    joints[1] = world->tran.x * sin(c_rad) + world->tran.y * cos(c_rad);
    joints[2] = world->tran.z;
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    return 0;
}

#endif /* ROTATEKINS_MATH_H */
