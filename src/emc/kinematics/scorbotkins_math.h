/********************************************************************
 * Description: scorbotkins_math.h
 *   Pure math functions for Scorbot ER-3 robot kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: Sebastian Kuzminsky
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2015-2016 All rights reserved.
 ********************************************************************/

#ifndef SCORBOTKINS_MATH_H
#define SCORBOTKINS_MATH_H

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

#ifndef TO_DEG
#define TO_DEG (180.0 / M_PI)
#endif

/*
 * Scorbot ER-3 kinematics
 *
 * The origin of the coordinate system is at the center of rotation of
 * joint J0, and at the bottom of the base plate.
 *
 * Joint 0: rotation around Z axis (base rotation)
 * Joint 1: shoulder
 * Joint 2: elbow
 * Joint 3: wrist pitch (handled externally)
 * Joint 4: wrist roll (handled externally)
 */

/* Parameters struct for Scorbot kinematics */
typedef struct {
    double l0_horizontal;  /* Horizontal distance from J0 to J1 */
    double l0_vertical;    /* Vertical distance from ground to J1 */
    double l1_length;      /* Link 1: J1 (shoulder) to J2 (elbow) */
    double l2_length;      /* Link 2: J2 (elbow) to wrist */
} scorbot_params_t;

/* Default values for Scorbot ER-3 (in mm) */
#define SCORBOT_DEFAULT_L0_HORIZONTAL  16.0
#define SCORBOT_DEFAULT_L0_VERTICAL   140.0
#define SCORBOT_DEFAULT_L1_LENGTH     221.0
#define SCORBOT_DEFAULT_L2_LENGTH     221.0

/*
 * Pure forward kinematics - joints to world coordinates
 *
 * params: kinematics parameters
 * joints: input joint positions array (5 joints, angles in degrees)
 * world: output world position (EmcPose)
 *
 * Returns 0 on success
 */
static inline int scorbot_forward_math(const scorbot_params_t *params,
                                        const double *joints,
                                        EmcPose *world)
{
    double j0_rad = joints[0] * TO_RAD;
    double j1_rad = joints[1] * TO_RAD;
    double j2_rad = joints[2] * TO_RAD;

    /* J1 location (shoulder) - fixed offset from base */
    double j1_x = params->l0_horizontal * cos(j0_rad);
    double j1_y = params->l0_horizontal * sin(j0_rad);
    double j1_z = params->l0_vertical;

    /* J2 location (elbow) - Link 1 from shoulder */
    double r1 = params->l1_length * cos(j1_rad);
    double j2_x = r1 * cos(j0_rad);
    double j2_y = r1 * sin(j0_rad);
    double j2_z = params->l1_length * sin(j1_rad);

    /* Wrist location (controlled point) - Link 2 from elbow */
    double r2 = params->l2_length * cos(j2_rad);
    double j3_x = r2 * cos(j0_rad);
    double j3_y = r2 * sin(j0_rad);
    double j3_z = params->l2_length * sin(j2_rad);

    /* End-effector is sum of all linkage vectors */
    world->tran.x = j1_x + j2_x + j3_x;
    world->tran.y = j1_y + j2_y + j3_y;
    world->tran.z = j1_z + j2_z + j3_z;

    /* Wrist pitch and roll passed through */
    world->a = joints[3];
    world->b = joints[4];
    world->c = 0.0;
    world->u = 0.0;
    world->v = 0.0;
    world->w = 0.0;

    return 0;
}

/*
 * Pure inverse kinematics - world coordinates to joints
 *
 * params: kinematics parameters
 * world: input world position (EmcPose)
 * joints: output joint positions array (5 joints)
 *
 * Returns 0 on success, -1 on error (out of reach)
 */
static inline int scorbot_inverse_math(const scorbot_params_t *params,
                                        const EmcPose *world,
                                        double *joints)
{
    double r_j1, z_j1;   /* J1 location in RZ plane */
    double r_cp, z_cp;   /* Controlled point in RZ plane */
    double distance_to_cp, distance_to_center;
    double angle_to_cp;
    double j1_angle;
    double z_j2;

    /* J0: base rotation - project pose onto XY plane */
    joints[0] = TO_DEG * atan2(world->tran.y, world->tran.x);

    /* Work in the RZ plane (vertical plane defined by J0 angle) */
    /* J1 location is a known, static vector */
    r_j1 = params->l0_horizontal;
    z_j1 = params->l0_vertical;

    /* Controlled point in RZ plane */
    r_cp = sqrt(world->tran.x * world->tran.x + world->tran.y * world->tran.y);
    z_cp = world->tran.z;

    /* Translate so J1 is the origin */
    r_cp -= r_j1;
    z_cp -= z_j1;

    /*
     * Now the origin (J1), J2, and CP define an isosceles triangle
     * (since L1 == L2 for Scorbot ER-3, but we handle general case).
     *
     * Bisect the base and use law of cosines.
     */
    distance_to_cp = sqrt(r_cp * r_cp + z_cp * z_cp);
    distance_to_center = distance_to_cp / 2.0;

    /* Check reach limits */
    if (distance_to_cp > (params->l1_length + params->l2_length)) {
        /* Out of reach - too far */
        return -1;
    }
    if (distance_to_cp < fabs(params->l1_length - params->l2_length)) {
        /* Out of reach - too close (armpit) */
        return -1;
    }

    /* Angle from J1 to controlled point */
    if (distance_to_cp > 0.0) {
        angle_to_cp = TO_DEG * acos(r_cp / distance_to_cp);
        if (z_cp < 0.0) {
            angle_to_cp = -angle_to_cp;
        }
    } else {
        angle_to_cp = 0.0;
    }

    /* Angle at J1 in the (J1, Center, J2) right triangle */
    if (params->l1_length > 0.0) {
        double cos_val = distance_to_center / params->l1_length;
        if (cos_val > 1.0) cos_val = 1.0;
        if (cos_val < -1.0) cos_val = -1.0;
        j1_angle = TO_DEG * acos(cos_val);
    } else {
        j1_angle = 0.0;
    }

    joints[1] = angle_to_cp + j1_angle;

    /* Compute J2 location to find J2 angle */
    z_j2 = params->l1_length * sin(joints[1] * TO_RAD);

    if (params->l2_length > 0.0) {
        double sin_val = (z_j2 - z_cp) / params->l2_length;
        if (sin_val > 1.0) sin_val = 1.0;
        if (sin_val < -1.0) sin_val = -1.0;
        joints[2] = -1.0 * TO_DEG * asin(sin_val);
    } else {
        joints[2] = 0.0;
    }

    /* Wrist pitch and roll passed through */
    joints[3] = world->a;
    joints[4] = world->b;

    return 0;
}

#endif /* SCORBOTKINS_MATH_H */
