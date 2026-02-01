/********************************************************************
 * Description: 5axiskins_math.h
 *   Pure math functions for XYZBC 5-axis bridge mill kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef FIVEAXISKINS_MATH_H
#define FIVEAXISKINS_MATH_H

#include "emcpos.h"

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef TO_RAD
#define TO_RAD (M_PI / 180.0)
#endif

/* Parameters struct - matches kinematics_params.h kins_5axis_params_t */
typedef struct {
    double pivot_length;
} fiveaxis_params_t;

/* Joint index struct - allows flexible joint mapping */
typedef struct {
    int jx, jy, jz;
    int ja, jb, jc;
    int ju, jv, jw;
} fiveaxis_joints_t;

/*
 * Helper: spherical to rectangular coordinate conversion
 *
 * s2r: spherical coordinates to cartesian coordinates
 * r       = length of vector
 * p=phi   = angle of vector wrt z axis (degrees)
 * t=theta = angle of vector projected onto xy plane (degrees)
 *           (projection length in xy plane is r*sin(p))
 */
static inline void fiveaxis_s2r(double r, double t, double p,
                                double *x, double *y, double *z) {
    double t_rad = TO_RAD * t;
    double p_rad = TO_RAD * p;
    *x = r * sin(p_rad) * cos(t_rad);
    *y = r * sin(p_rad) * sin(t_rad);
    *z = r * cos(p_rad);
}

/*
 * Pure forward kinematics - joints to world coordinates
 *
 * params: kinematics parameters (pivot_length)
 * jmap: joint index mapping (-1 means axis not configured)
 * joints: input joint positions array
 * pos: output world position (EmcPose)
 *
 * Returns 0 on success
 */
static inline int fiveaxis_forward_math(const fiveaxis_params_t *params,
                                        const fiveaxis_joints_t *jmap,
                                        const double *joints,
                                        EmcPose *pos) {
    double rx, ry, rz;
    double b_val = (jmap->jb >= 0) ? joints[jmap->jb] : 0;
    double c_val = (jmap->jc >= 0) ? joints[jmap->jc] : 0;
    double w_val = (jmap->jw >= 0) ? joints[jmap->jw] : 0;

    fiveaxis_s2r(params->pivot_length + w_val, c_val, 180.0 - b_val,
                 &rx, &ry, &rz);

    pos->tran.x = (jmap->jx >= 0 ? joints[jmap->jx] : 0) + rx;
    pos->tran.y = (jmap->jy >= 0 ? joints[jmap->jy] : 0) + ry;
    pos->tran.z = (jmap->jz >= 0 ? joints[jmap->jz] : 0) + params->pivot_length + rz;
    pos->a = (jmap->ja >= 0) ? joints[jmap->ja] : 0;
    pos->b = b_val;
    pos->c = c_val;
    pos->u = (jmap->ju >= 0) ? joints[jmap->ju] : 0;
    pos->v = (jmap->jv >= 0) ? joints[jmap->jv] : 0;
    pos->w = w_val;

    return 0;
}

/*
 * Pure inverse kinematics - world coordinates to axis values
 *
 * This function computes the axis values (stored in EmcPose format).
 * For RT: caller passes result to position_to_mapped_joints() for multi-joint handling
 * For userspace: caller uses fiveaxis_axis_to_joints() to write to joints array
 *
 * params: kinematics parameters (pivot_length)
 * world: input world position (EmcPose)
 * axis_values: output axis values (EmcPose format, not actual joints)
 *
 * Returns 0 on success
 */
static inline int fiveaxis_inverse_math(const fiveaxis_params_t *params,
                                        const EmcPose *world,
                                        EmcPose *axis_values) {
    double rx, ry, rz;

    fiveaxis_s2r(params->pivot_length + world->w, world->c, 180.0 - world->b,
                 &rx, &ry, &rz);

    axis_values->tran.x = world->tran.x - rx;
    axis_values->tran.y = world->tran.y - ry;
    axis_values->tran.z = world->tran.z - params->pivot_length - rz;
    axis_values->a = world->a;
    axis_values->b = world->b;
    axis_values->c = world->c;
    axis_values->u = world->u;
    axis_values->v = world->v;
    axis_values->w = world->w;

    return 0;
}

/*
 * Convenience function for userspace: writes axis_values to joints via jmap
 *
 * jmap: joint index mapping (-1 means axis not configured)
 * axis_values: computed axis values from fiveaxis_inverse_math
 * joints: output joint positions array
 */
static inline void fiveaxis_axis_to_joints(const fiveaxis_joints_t *jmap,
                                           const EmcPose *axis_values,
                                           double *joints) {
    if (jmap->jx >= 0) joints[jmap->jx] = axis_values->tran.x;
    if (jmap->jy >= 0) joints[jmap->jy] = axis_values->tran.y;
    if (jmap->jz >= 0) joints[jmap->jz] = axis_values->tran.z;
    if (jmap->ja >= 0) joints[jmap->ja] = axis_values->a;
    if (jmap->jb >= 0) joints[jmap->jb] = axis_values->b;
    if (jmap->jc >= 0) joints[jmap->jc] = axis_values->c;
    if (jmap->ju >= 0) joints[jmap->ju] = axis_values->u;
    if (jmap->jv >= 0) joints[jmap->jv] = axis_values->v;
    if (jmap->jw >= 0) joints[jmap->jw] = axis_values->w;
}

#endif /* FIVEAXISKINS_MATH_H */
