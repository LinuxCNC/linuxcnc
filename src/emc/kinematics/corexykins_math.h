/********************************************************************
 * Description: corexykins_math.h
 *   Pure math functions for CoreXY kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * ref: http://corexy.com/theory.html
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef COREXYKINS_MATH_H
#define COREXYKINS_MATH_H

#include "emcpos.h"

/*
 * CoreXY kinematics - no parameters needed, pure linear transformation
 *
 * Forward:
 *   X = 0.5 * (J0 + J1)
 *   Y = 0.5 * (J0 - J1)
 *
 * Inverse:
 *   J0 = X + Y
 *   J1 = X - Y
 */

/*
 * Pure forward kinematics - joints to world coordinates
 *
 * joints: input joint positions array (9 joints)
 * world: output world position (EmcPose)
 *
 * Returns 0 on success
 */
static inline int corexy_forward_math(const double *joints, EmcPose *world)
{
    world->tran.x = 0.5 * (joints[0] + joints[1]);
    world->tran.y = 0.5 * (joints[0] - joints[1]);
    world->tran.z = joints[2];
    world->a      = joints[3];
    world->b      = joints[4];
    world->c      = joints[5];
    world->u      = joints[6];
    world->v      = joints[7];
    world->w      = joints[8];

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
static inline int corexy_inverse_math(const EmcPose *world, double *joints)
{
    joints[0] = world->tran.x + world->tran.y;
    joints[1] = world->tran.x - world->tran.y;
    joints[2] = world->tran.z;
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    return 0;
}

#endif /* COREXYKINS_MATH_H */
