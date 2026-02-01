/********************************************************************
 * Description: maxkins_math.h
 *   Pure math functions for Chris Radek's tabletop 5-axis mill 'max'
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 *
 * Notes:
 *   This mill has a tilting head (B axis) and horizontal rotary
 *   mounted to the table (C axis).
 *
 *   Joint mapping is fixed (not configurable):
 *   joints[0..2] = X, Y, Z
 *   joints[3..5] = A, B, C
 *   joints[6..8] = U, V, W
 *
 *   The direction of the B axis is the opposite of the
 *   conventional axis direction.
 ********************************************************************/

#ifndef MAXKINS_MATH_H
#define MAXKINS_MATH_H

#include "emcpos.h"

#ifdef RTAPI
#include "rtapi_math.h"
#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif
#else
#include <math.h>
#ifndef PM_PI
#define PM_PI M_PI
#endif
#endif

/* Parameters struct - matches kinematics_params.h kins_max_params_t */
typedef struct {
    double pivot_length;
    int conventional_directions;  /* 0 = unconventional (default), 1 = conventional */
} maxkins_params_t;

/* Angle conversion macros */
#ifndef maxkins_d2r
#define maxkins_d2r(d) ((d)*PM_PI/180.0)
#endif
#ifndef maxkins_r2d
#define maxkins_r2d(r) ((r)*180.0/PM_PI)
#endif

/* Hypotenuse helper */
#ifndef maxkins_hypot
#define maxkins_hypot(a,b) (sqrt((a)*(a)+(b)*(b)))
#endif

/*
 * Pure forward kinematics - joints to world coordinates
 *
 * params: kinematics parameters (pivot_length, conventional_directions)
 * joints: input joint positions array (fixed mapping: 0-8 = X,Y,Z,A,B,C,U,V,W)
 * pos: output world position (EmcPose)
 *
 * Returns 0 on success
 */
static inline int maxkins_forward_math(const maxkins_params_t *params,
                                       const double *joints,
                                       EmcPose *pos)
{
    const double con = params->conventional_directions ? 1.0 : -1.0;

    /* B correction */
    const double zb = (params->pivot_length + joints[8]) * cos(maxkins_d2r(joints[4]));
    const double xb = (params->pivot_length + joints[8]) * sin(maxkins_d2r(joints[4]));

    /* C correction */
    const double xyr = maxkins_hypot(joints[0], joints[1]);
    const double xytheta = atan2(joints[1], joints[0]) + maxkins_d2r(joints[5]);

    /* U correction */
    const double zv = joints[6] * sin(maxkins_d2r(joints[4]));
    const double xv = joints[6] * cos(maxkins_d2r(joints[4]));

    /* V correction is always in joint 1 only */

    pos->tran.x = xyr * cos(xytheta) - (con * xb) - xv;
    pos->tran.y = xyr * sin(xytheta) - joints[7];
    pos->tran.z = joints[2] - zb - (con * zv) + params->pivot_length;

    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    pos->u = joints[6];
    pos->v = joints[7];
    pos->w = joints[8];

    return 0;
}

/*
 * Pure inverse kinematics - world coordinates to joints
 *
 * params: kinematics parameters (pivot_length, conventional_directions)
 * pos: input world position (EmcPose)
 * joints: output joint positions array (fixed mapping: 0-8 = X,Y,Z,A,B,C,U,V,W)
 *
 * Returns 0 on success
 */
static inline int maxkins_inverse_math(const maxkins_params_t *params,
                                       const EmcPose *pos,
                                       double *joints)
{
    const double con = params->conventional_directions ? 1.0 : -1.0;

    /* B correction */
    const double zb = (params->pivot_length + pos->w) * cos(maxkins_d2r(pos->b));
    const double xb = (params->pivot_length + pos->w) * sin(maxkins_d2r(pos->b));

    /* C correction */
    const double xyr = maxkins_hypot(pos->tran.x, pos->tran.y);
    const double xytheta = atan2(pos->tran.y, pos->tran.x) - maxkins_d2r(pos->c);

    /* U correction */
    const double zv = pos->u * sin(maxkins_d2r(pos->b));
    const double xv = pos->u * cos(maxkins_d2r(pos->b));

    /* V correction is always in joint 1 only */

    joints[0] = xyr * cos(xytheta) + (con * xb) + xv;
    joints[1] = xyr * sin(xytheta) + pos->v;
    joints[2] = pos->tran.z + zb - (con * zv) - params->pivot_length;

    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
}

#endif /* MAXKINS_MATH_H */
