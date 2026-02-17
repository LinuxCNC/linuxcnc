//
// This is a kinematics module for the Scorbot ER 3.
//
// Copyright (C) 2015-2016 Sebastian Kuzminsky <seb@highlab.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

//
// The origin of the G53 coordinate system is at the center of rotation of
// joint J0, and at the bottom of the base plate.
//
// FIXME: The origin should probably be at the bottom of the base (part 5
// in the parts diagram on page 7-11 of the SCORBOT-ER III User's Manual).
//
// Joint 0 is rotation around the Z axis.  It chooses the plane that
// the rest of the arm moves in.
//
// Joint 1 is the shoulder.
//
// Joint 2 is the elbow.
//
// Joint 3 is pitch of the wrist, joint 4 is roll of the wrist.  These are
// converted to motor actuations by an external differential comp in HAL.
//


#include "kinematics.h"

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
 * Returns 0 on success
 */
static int scorbot_forward_math(const scorbot_params_t *params,
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
 * Returns 0 on success, -1 on error (out of reach)
 */
static int scorbot_inverse_math(const scorbot_params_t *params,
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
     * Now the origin (J1), J2, and CP define a triangle.
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

// Static params struct with default Scorbot ER-3 dimensions
static scorbot_params_t scorbot_params = {
    .l0_horizontal = SCORBOT_DEFAULT_L0_HORIZONTAL,
    .l0_vertical   = SCORBOT_DEFAULT_L0_VERTICAL,
    .l1_length     = SCORBOT_DEFAULT_L1_LENGTH,
    .l2_length     = SCORBOT_DEFAULT_L2_LENGTH
};


// Forward kinematics takes the joint positions and computes the cartesian
// coordinates of the controlled point.
int kinematicsForward(
    const double *joints,
    EmcPose *pose,
    const KINEMATICS_FORWARD_FLAGS *fflags,
    KINEMATICS_INVERSE_FLAGS *iflags
) {
    (void)fflags;
    (void)iflags;
    return scorbot_forward_math(&scorbot_params, joints, pose);
}


//
// Inverse kinematics takes the cartesian coordinates of the controlled
// point and computes corresponding the joint positions.
//
// Joint 0 rotates the arm around the base.  The rest of the joints are
// confined to the vertical plane containing J0, and rotated around the
// vertical at J0.  This kinematics code calls this plane the "RZ" plane.
// The Z coordinate in this plane is the same as the Z coordinate in the
// "cartesian" coordinates of LinuxCNC's world space.  The R coordinate
// is the horizontal distance (ie, in the XY plane) of the controlled
// point from J0.
//
int kinematicsInverse(
    const EmcPose *pose,
    double *joints,
    const KINEMATICS_INVERSE_FLAGS *iflags,
    KINEMATICS_FORWARD_FLAGS *fflags
) {
    (void)iflags;
    (void)fflags;
    return scorbot_inverse_math(&scorbot_params, pose, joints);
}


KINEMATICS_TYPE kinematicsType(void) {
    return KINEMATICS_BOTH;
}


#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

const char* kinematicsGetName(void) { return "scorbot-kins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

static int comp_id;

int rtapi_app_main(void) {
    comp_id = hal_init("scorbot-kins");
    if (comp_id < 0) {
        return comp_id;
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    scorbot_params_t p;
    p.l0_horizontal = kp->params.scorbot.l0_horizontal;
    p.l0_vertical   = kp->params.scorbot.l0_vertical;
    p.l1_length     = kp->params.scorbot.l1_length;
    p.l2_length     = kp->params.scorbot.l2_length;
    return scorbot_forward_math(&p, joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    scorbot_params_t p;
    p.l0_horizontal = kp->params.scorbot.l0_horizontal;
    p.l0_vertical   = kp->params.scorbot.l0_vertical;
    p.l1_length     = kp->params.scorbot.l1_length;
    p.l2_length     = kp->params.scorbot.l2_length;
    return scorbot_inverse_math(&p, pos, joints);
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    (void)params; (void)read_float; (void)read_bit; (void)read_s32;
    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);

