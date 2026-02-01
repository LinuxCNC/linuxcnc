/********************************************************************
 * Description: tripodkins_math.h
 *   Pure math functions for 3-axis Tripod kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 *
 * Notes:
 *   Vertices A, B, and C are the base, and vertex D is the controlled point.
 *   Three tripod strut lengths AD, BD, and CD are the joints that move point D.
 *
 *   Point A is the origin (0, 0, 0).
 *   Point B lies on the x axis at (Bx, 0, 0).
 *   Point C lies in the xy plane at (Cx, Cy, 0).
 *   Point D has coordinates (Dx, Dy, Dz).
 *
 *   joints[0] = AD (strut length from A to D)
 *   joints[1] = BD (strut length from B to D)
 *   joints[2] = CD (strut length from C to D)
 ********************************************************************/

#ifndef TRIPODKINS_MATH_H
#define TRIPODKINS_MATH_H

#include "emcpos.h"
#include "kinematics.h"

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

/* Parameters struct - matches kinematics_params.h kins_tripod_params_t */
typedef struct {
    double bx;  /* X coordinate of point B */
    double cx;  /* X coordinate of point C */
    double cy;  /* Y coordinate of point C */
} tripod_params_t;

#ifndef tripod_sq
#define tripod_sq(x) ((x)*(x))
#endif

/*
 * Pure forward kinematics - strut lengths to Cartesian position
 *
 * Takes three strut lengths and computes Dx, Dy, and Dz.
 * The forward flag is used to resolve D above/below the xy plane.
 *
 * params: kinematics parameters (bx, cx, cy)
 * joints: input joint positions (strut lengths AD, BD, CD)
 * pos: output world position (EmcPose)
 * fflags: forward flags (0 = above xy plane, != 0 = below)
 * iflags: inverse flags output (not used by tripodkins)
 *
 * Returns 0 on success, -1 on singularity
 */
static inline int tripod_forward_math(const tripod_params_t *params,
                                      const double *joints,
                                      EmcPose *pos,
                                      const KINEMATICS_FORWARD_FLAGS *fflags,
                                      KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)iflags;
    double P, Q, R;
    double s, t, u;
    double AD = joints[0];
    double BD = joints[1];
    double CD = joints[2];
    double Bx = params->bx;
    double Cx = params->cx;
    double Cy = params->cy;

    P = tripod_sq(AD);
    Q = tripod_sq(BD) - tripod_sq(Bx);
    R = tripod_sq(CD) - tripod_sq(Cx) - tripod_sq(Cy);
    s = -2.0 * Bx;
    t = -2.0 * Cx;
    u = -2.0 * Cy;

    if (s == 0.0) {
        /* points A and B coincident */
        return -1;
    }
    pos->tran.x = (Q - P) / s;

    if (u == 0.0) {
        /* points A B C are colinear */
        return -1;
    }
    pos->tran.y = (R - Q - (t - s) * pos->tran.x) / u;
    pos->tran.z = P - tripod_sq(pos->tran.x) - tripod_sq(pos->tran.y);
    if (pos->tran.z < 0.0) {
        /* triangle inequality violated */
        return -1;
    }
    pos->tran.z = sqrt(pos->tran.z);
    if (fflags && *fflags) {
        pos->tran.z = -pos->tran.z;
    }

    pos->a = 0.0;
    pos->b = 0.0;
    pos->c = 0.0;
    pos->u = 0.0;
    pos->v = 0.0;
    pos->w = 0.0;

    return 0;
}

/*
 * Pure inverse kinematics - Cartesian position to strut lengths
 *
 * params: kinematics parameters (bx, cx, cy)
 * pos: input world position (EmcPose)
 * joints: output joint positions (strut lengths AD, BD, CD)
 * iflags: inverse flags (not used by tripodkins)
 * fflags: forward flags output (set based on Dz sign)
 *
 * Returns 0 on success
 */
static inline int tripod_inverse_math(const tripod_params_t *params,
                                      const EmcPose *pos,
                                      double *joints,
                                      const KINEMATICS_INVERSE_FLAGS *iflags,
                                      KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)iflags;
    double Dx = pos->tran.x;
    double Dy = pos->tran.y;
    double Dz = pos->tran.z;
    double Bx = params->bx;
    double Cx = params->cx;
    double Cy = params->cy;

    joints[0] = sqrt(tripod_sq(Dx) + tripod_sq(Dy) + tripod_sq(Dz));
    joints[1] = sqrt(tripod_sq(Dx - Bx) + tripod_sq(Dy) + tripod_sq(Dz));
    joints[2] = sqrt(tripod_sq(Dx - Cx) + tripod_sq(Dy - Cy) + tripod_sq(Dz));

    if (fflags) {
        *fflags = 0;
        if (Dz < 0.0) {
            *fflags = 1;
        }
    }

    return 0;
}

#endif /* TRIPODKINS_MATH_H */
