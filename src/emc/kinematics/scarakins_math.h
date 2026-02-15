/********************************************************************
 * Description: scarakins_math.h
 *   Pure math functions for SCARA robot kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef SCARAKINS_MATH_H
#define SCARAKINS_MATH_H

#include "emcpos.h"

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif

/* Parameters struct - matches kinematics_params.h kins_scara_params_t */
typedef struct {
    double d1;  /* Vertical distance from ground to inner arm center */
    double d2;  /* Length of inner arm */
    double d3;  /* Vertical offset between inner and outer arm */
    double d4;  /* Length of outer arm */
    double d5;  /* Vertical distance from end effector to tooltip */
    double d6;  /* Horizontal offset from end effector axis to tooltip */
} scara_params_t;

/* Flag for elbow configuration (joint[1] < 90 degrees) */
#define SCARA_ELBOW_UP 0x01

/*
 * Pure forward kinematics - joints to world coordinates
 *
 * params: kinematics parameters (d1-d6)
 * joints: input joint positions array (6 joints: j0,j1 in degrees, j2 in length, j3-j5 in degrees)
 * world: output world position (EmcPose)
 * iflags: output inverse kinematics flags (can be NULL)
 *
 * Returns 0 on success
 */
static inline int scara_forward_math(const scara_params_t *params,
                                      const double *joints,
                                      EmcPose *world,
                                      int *iflags)
{
    double a0, a1, a3;
    double x, y, z, c;
    int flags = 0;

    /* convert joint angles to radians for sin() and cos() */
    a0 = joints[0] * (PM_PI / 180.0);
    a1 = joints[1] * (PM_PI / 180.0);
    a3 = joints[3] * (PM_PI / 180.0);

    /* convert angles into world coords */
    a1 = a1 + a0;
    a3 = a3 + a1;

    x = params->d2*cos(a0) + params->d4*cos(a1) + params->d6*cos(a3);
    y = params->d2*sin(a0) + params->d4*sin(a1) + params->d6*sin(a3);
    z = params->d1 + params->d3 - joints[2] - params->d5;
    c = a3;

    if (joints[1] < 90.0) {
        flags = SCARA_ELBOW_UP;
    }

    world->tran.x = x;
    world->tran.y = y;
    world->tran.z = z;
    world->c = c * 180.0 / PM_PI;

    world->a = joints[4];
    world->b = joints[5];
    world->u = 0.0;
    world->v = 0.0;
    world->w = 0.0;

    /* Store flags if requested */
    if (iflags != NULL) {
        *iflags = flags;
    }

    return 0;
}

/*
 * Pure inverse kinematics - world coordinates to joints
 *
 * params: kinematics parameters (d1-d6)
 * world: input world position (EmcPose)
 * joints: output joint positions array (6 joints)
 * iflags: input inverse kinematics flags (elbow configuration)
 * fflags: output forward kinematics flags (can be NULL)
 *
 * Returns 0 on success, -1 on failure
 */
static inline int scara_inverse_math(const scara_params_t *params,
                                      const EmcPose *world,
                                      double *joints,
                                      int iflags,
                                      int *fflags)
{
    double a3;
    double q0, q1;
    double xt, yt, rsq, cc;
    double x, y, z, c;

    x = world->tran.x;
    y = world->tran.y;
    z = world->tran.z;
    c = world->c;

    /* convert degrees to radians */
    a3 = c * (PM_PI / 180.0);

    /* center of end effector (correct for D6) */
    xt = x - params->d6*cos(a3);
    yt = y - params->d6*sin(a3);

    /* horizontal distance (squared) from end effector centerline
        to main column centerline */
    rsq = xt*xt + yt*yt;

    /* joint 1 angle needed to make arm length match sqrt(rsq) */
    cc = (rsq - params->d2*params->d2 - params->d4*params->d4) / (2.0*params->d2*params->d4);
    if (cc < -1.0) cc = -1.0;
    if (cc > 1.0) cc = 1.0;
    q1 = acos(cc);

    if (iflags & SCARA_ELBOW_UP) {
        q1 = -q1;
    }

    /* angle to end effector */
    q0 = atan2(yt, xt);

    /* end effector coords in inner arm coord system */
    xt = params->d2 + params->d4*cos(q1);
    yt = params->d4*sin(q1);

    /* inner arm angle */
    q0 = q0 - atan2(yt, xt);

    /* q0 and q1 are still in radians. convert them to degrees */
    q0 = q0 * (180.0 / PM_PI);
    q1 = q1 * (180.0 / PM_PI);

    joints[0] = q0;
    joints[1] = q1;
    joints[2] = params->d1 + params->d3 - params->d5 - z;
    joints[3] = c - (q0 + q1);
    joints[4] = world->a;
    joints[5] = world->b;

    /* Store flags if requested */
    if (fflags != NULL) {
        *fflags = 0;
    }

    return 0;
}

#endif /* SCARAKINS_MATH_H */
