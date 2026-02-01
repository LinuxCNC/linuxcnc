/********************************************************************
 * Description: trtfuncs_math.h
 *   Pure math functions for Tilting Rotary Table (TRT) kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 *
 * Notes:
 *   Two 5-axis mill configurations are supported:
 *   1) XYZAC - Tilting table (A axis) with horizontal rotary (C axis)
 *   2) XYZBC - Tilting table (B axis) with horizontal rotary (C axis)
 *
 *   The directions of the rotational axes are the opposite of the
 *   conventional axis directions.
 ********************************************************************/

#ifndef TRTFUNCS_MATH_H
#define TRTFUNCS_MATH_H

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

/* Parameters struct - matches kinematics_params.h kins_trt_params_t */
typedef struct {
    double x_rot_point;
    double y_rot_point;
    double z_rot_point;
    double x_offset;
    double y_offset;
    double z_offset;
    double tool_offset;
    int conventional_directions;  /* 0 = unconventional (default), 1 = conventional */
} trt_params_t;

/* Joint index struct - allows flexible joint mapping */
typedef struct {
    int jx, jy, jz;
    int ja, jb, jc;
    int ju, jv, jw;
} trt_joints_t;

/*
 * XYZAC Forward kinematics - joints to world coordinates
 *
 * params: kinematics parameters
 * jmap: joint index mapping (-1 means axis not configured)
 * joints: input joint positions array
 * pos: output world position (EmcPose)
 *
 * Returns 0 on success
 */
static inline int xyzac_forward_math(const trt_params_t *params,
                                     const trt_joints_t *jmap,
                                     const double *joints,
                                     EmcPose *pos)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dt = params->tool_offset;
    const double dy = params->y_offset;
    const double dz = params->z_offset + dt;
    const double a_rad = joints[jmap->ja] * TO_RAD;
    const double c_rad = joints[jmap->jc] * TO_RAD;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    pos->tran.x = +       cos(c_rad)              * (joints[jmap->jx]      - x_rot_point)
                  - con * sin(c_rad) * cos(a_rad) * (joints[jmap->jy] - dy - y_rot_point)
                  +       sin(c_rad) * sin(a_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  - con * sin(c_rad) * dy
                  + x_rot_point;

    pos->tran.y = + con * sin(c_rad)              * (joints[jmap->jx]      - x_rot_point)
                  +       cos(c_rad) * cos(a_rad) * (joints[jmap->jy] - dy - y_rot_point)
                  - con * cos(c_rad) * sin(a_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  +       cos(c_rad) * dy
                  + y_rot_point;

    pos->tran.z = + 0
                  + con * sin(a_rad) * (joints[jmap->jy] - dy - y_rot_point)
                  + cos(a_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->a = joints[jmap->ja];
    pos->c = joints[jmap->jc];

    /* Optional axes */
    pos->b = (jmap->jb >= 0) ? joints[jmap->jb] : 0;
    pos->u = (jmap->ju >= 0) ? joints[jmap->ju] : 0;
    pos->v = (jmap->jv >= 0) ? joints[jmap->jv] : 0;
    pos->w = (jmap->jw >= 0) ? joints[jmap->jw] : 0;

    return 0;
}

/*
 * XYZAC Inverse kinematics - world coordinates to axis values
 *
 * Computes axis values in EmcPose format.
 * RT caller passes result to position_to_mapped_joints() for multi-joint handling.
 * Userspace caller uses trt_axis_to_joints() to write to joints array.
 *
 * params: kinematics parameters
 * jmap: joint index mapping (-1 means axis not configured)
 * pos: input world position (EmcPose)
 * axis_values: output axis values (EmcPose format)
 *
 * Returns 0 on success
 */
static inline int xyzac_inverse_math(const trt_params_t *params,
                                     const trt_joints_t *jmap,
                                     const EmcPose *pos,
                                     EmcPose *axis_values)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dy = params->y_offset;
    const double dt = params->tool_offset;
    const double dz = params->z_offset + dt;
    const double a_rad = pos->a * TO_RAD;
    const double c_rad = pos->c * TO_RAD;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    axis_values->tran.x = +       cos(c_rad)              * (pos->tran.x - x_rot_point)
                          + con * sin(c_rad)              * (pos->tran.y - y_rot_point)
                          + x_rot_point;

    axis_values->tran.y = - con * sin(c_rad) * cos(a_rad) * (pos->tran.x - x_rot_point)
                          +       cos(c_rad) * cos(a_rad) * (pos->tran.y - y_rot_point)
                          + con *              sin(a_rad) * (pos->tran.z - z_rot_point)
                          -                    cos(a_rad) * dy
                          - con *              sin(a_rad) * dz
                          + dy
                          + y_rot_point;

    axis_values->tran.z = +       sin(c_rad) * sin(a_rad) * (pos->tran.x - x_rot_point)
                          - con * cos(c_rad) * sin(a_rad) * (pos->tran.y - y_rot_point)
                          +                    cos(a_rad) * (pos->tran.z - z_rot_point)
                          + con *              sin(a_rad) * dy
                          -                    cos(a_rad) * dz
                          + dz
                          + z_rot_point;

    axis_values->a = pos->a;
    axis_values->c = pos->c;

    /* Optional axes */
    axis_values->b = (jmap->jb >= 0) ? pos->b : 0;
    axis_values->u = (jmap->ju >= 0) ? pos->u : 0;
    axis_values->v = (jmap->jv >= 0) ? pos->v : 0;
    axis_values->w = (jmap->jw >= 0) ? pos->w : 0;

    return 0;
}

/*
 * XYZBC Forward kinematics - joints to world coordinates
 */
static inline int xyzbc_forward_math(const trt_params_t *params,
                                     const trt_joints_t *jmap,
                                     const double *joints,
                                     EmcPose *pos)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dx = params->x_offset;
    const double dt = params->tool_offset;
    const double dz = params->z_offset + dt;
    const double b_rad = joints[jmap->jb] * TO_RAD;
    const double c_rad = joints[jmap->jc] * TO_RAD;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    pos->tran.x =         cos(c_rad) * cos(b_rad) * (joints[jmap->jx] - dx - x_rot_point)
                  - con * sin(c_rad) *              (joints[jmap->jy]      - y_rot_point)
                  + con * cos(c_rad) * sin(b_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  +       cos(c_rad) * dx
                  + x_rot_point;

    pos->tran.y = + con * sin(c_rad) * cos(b_rad) * (joints[jmap->jx] - dx - x_rot_point)
                  +       cos(c_rad) *              (joints[jmap->jy]      - y_rot_point)
                  +       sin(c_rad) * sin(b_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  + con * sin(c_rad) * dx
                  + y_rot_point;

    pos->tran.z = - con * sin(b_rad) * (joints[jmap->jx] - dx - x_rot_point)
                  +       cos(b_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->b = joints[jmap->jb];
    pos->c = joints[jmap->jc];

    /* Optional axes */
    pos->a = (jmap->ja >= 0) ? joints[jmap->ja] : 0;
    pos->u = (jmap->ju >= 0) ? joints[jmap->ju] : 0;
    pos->v = (jmap->jv >= 0) ? joints[jmap->jv] : 0;
    pos->w = (jmap->jw >= 0) ? joints[jmap->jw] : 0;

    return 0;
}

/*
 * XYZBC Inverse kinematics - world coordinates to axis values
 */
static inline int xyzbc_inverse_math(const trt_params_t *params,
                                     const trt_joints_t *jmap,
                                     const EmcPose *pos,
                                     EmcPose *axis_values)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dx = params->x_offset;
    const double dt = params->tool_offset;
    const double dz = params->z_offset + dt;
    const double b_rad = pos->b * TO_RAD;
    const double c_rad = pos->c * TO_RAD;
    const double dpx = -cos(b_rad)*dx + sin(b_rad)*dz + dx;
    const double dpz = -sin(b_rad)*dx - cos(b_rad)*dz + dz;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    axis_values->tran.x = +       cos(c_rad) * cos(b_rad) * (pos->tran.x - x_rot_point)
                          + con * sin(c_rad) * cos(b_rad) * (pos->tran.y - y_rot_point)
                          - con *              sin(b_rad) * (pos->tran.z - z_rot_point)
                          + dpx
                          + x_rot_point;

    axis_values->tran.y = - con * sin(c_rad) * (pos->tran.x - x_rot_point)
                          +       cos(c_rad) * (pos->tran.y - y_rot_point)
                          + y_rot_point;

    axis_values->tran.z = + con * cos(c_rad) * sin(b_rad) * (pos->tran.x - x_rot_point)
                          +       sin(c_rad) * sin(b_rad) * (pos->tran.y - y_rot_point)
                          +                    cos(b_rad) * (pos->tran.z - z_rot_point)
                          + dpz
                          + z_rot_point;

    axis_values->b = pos->b;
    axis_values->c = pos->c;

    /* Optional axes */
    axis_values->a = (jmap->ja >= 0) ? pos->a : 0;
    axis_values->u = (jmap->ju >= 0) ? pos->u : 0;
    axis_values->v = (jmap->jv >= 0) ? pos->v : 0;
    axis_values->w = (jmap->jw >= 0) ? pos->w : 0;

    return 0;
}

/*
 * Convenience function for userspace: writes axis_values to joints via jmap
 */
static inline void trt_axis_to_joints(const trt_joints_t *jmap,
                                      const EmcPose *axis_values,
                                      double *joints)
{
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

#endif /* TRTFUNCS_MATH_H */
