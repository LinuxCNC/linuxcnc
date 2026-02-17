/*****************************************************************
* Description: scarakins.c
*   Kinematics for scara typed robots
*   Set the params using HAL to fit your robot
*
*   Derived from a work by Sagar Behere
*
* Author: Sagar Behere
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2003 All rights reserved.
*
* Last change:
*******************************************************************
*/

#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "posemath.h"
#include "hal.h"
#include "kinematics.h"
#include "switchkins.h"

/* ========================================================================
 * Internal math (was in scarakins_math.h)
 * ======================================================================== */

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
static int scara_forward_math(const scara_params_t *params,
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
static int scara_inverse_math(const scara_params_t *params,
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

/* ========================================================================
 * RT interface (reads HAL pins)
 * ======================================================================== */

struct scara_data {
    hal_float_t *d1, *d2, *d3, *d4, *d5, *d6;
} *haldata = 0;

/* key dimensions

   joint[0] = Entire arm rotates around a vertical axis at its inner end
                which is attached to the earth.  A value of zero means the
                inner arm is pointing along the X axis.
   D1 = Vertical distance from the ground plane to the center of the inner
                arm.
   D2 = Horizontal distance between joint[0] axis and joint[1] axis, ie.
                the length of the inner arm.
   joint[1] = Outer arm rotates around a vertical axis at its inner end
                which is attached to the outer end of the inner arm.  A
                value of zero means the outer arm is parallel to the
                inner arm (and extending outward).
   D3 = Vertical distance from the center of the inner arm to the center
                of the outer arm.  May be positive or negative depending
                on the structure of the robot.
   joint[2] = End effector slides along a vertical axis at the outer end
                of the outer arm.  A value of zero means the end effector
                is at the same height as the center of the outer arm, and
                positive values mean downward movement.
   D4 = Horizontal distance between joint[1] axis and joint[2] axis, ie.
                the length of the outer arm
   joint[3] = End effector rotates around the same vertical axis that it
                slides along.  A value of zero means that the tooltip (if
                offset from the axis) is pointing in the same direction
                as the centerline of the outer arm.
   D5 = Vertical distance from the end effector to the tooltip.  Positive
                means the tooltip is lower than the end effector, and is
                the normal case.
   D6 = Horizontal distance from the centerline of the end effector (and
                the joints 2 and 3 axis) and the tooltip.  Zero means the
                tooltip is on the centerline.  Non-zero values should be
                positive, if negative they introduce a 180 degree offset
                on the value of joint[3].
*/

#define D1 (*(haldata->d1))
#define D2 (*(haldata->d2))
#define D3 (*(haldata->d3))
#define D4 (*(haldata->d4))
#define D5 (*(haldata->d5))
#define D6 (*(haldata->d6))

/* joint[0], joint[1] and joint[3] are in degrees and joint[2] is in length units */
static
int scaraKinematicsForward(const double * joint,
                      EmcPose * world,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    scara_params_t params;
    int flags_out = 0;

    params.d1 = D1;
    params.d2 = D2;
    params.d3 = D3;
    params.d4 = D4;
    params.d5 = D5;
    params.d6 = D6;

    scara_forward_math(&params, joint, world, &flags_out);

    *iflags = flags_out;
    return 0;
} //scaraKinematicsForward()

static int scaraKinematicsInverse(const EmcPose * world,
                                  double * joint,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags)
{
    scara_params_t params;
    int flags_out = 0;

    params.d1 = D1;
    params.d2 = D2;
    params.d3 = D3;
    params.d4 = D4;
    params.d5 = D5;
    params.d6 = D6;

    scara_inverse_math(&params, world, joint, *iflags, &flags_out);

    *fflags = flags_out;
    return 0;
} // scaraKinematicsInverse()

#define DEFAULT_D1 490
#define DEFAULT_D2 340
#define DEFAULT_D3  50
#define DEFAULT_D4 250
#define DEFAULT_D5  50
#define DEFAULT_D6  50

static int scaraKinematicsSetup(const  int   comp_id,
                                const  char* coordinates,
                                kparms*      kp)
{
    (void)coordinates;
    int res=0;

    haldata = hal_malloc(sizeof(*haldata));
    if (!haldata) goto error;

    res += hal_pin_float_newf(HAL_IN, &(haldata->d1), comp_id,"%s.D1",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d2), comp_id,"%s.D2",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d3), comp_id,"%s.D3",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d4), comp_id,"%s.D4",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d5), comp_id,"%s.D5",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d6), comp_id,"%s.D6",kp->halprefix);
    if (res) { goto error; }

    D1 = DEFAULT_D1;
    D2 = DEFAULT_D2;
    D3 = DEFAULT_D3;
    D4 = DEFAULT_D4;
    D5 = DEFAULT_D5;
    D6 = DEFAULT_D6;

    return 0;

error:
    return -1;
} // scaraKinematicsSetup()

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    kp->kinsname    = "scarakins"; // !!! must agree with filename
    kp->halprefix   = "scarakins"; // hal pin names
    kp->required_coordinates = "xyzabc"; // ab are scaragui table tilts
    kp->allow_duplicates     = 0;
    kp->max_joints = strlen(kp->required_coordinates);

    rtapi_print("\n!!! switchkins-type 0 is %s\n",kp->kinsname);
    *kset0 = scaraKinematicsSetup;
    *kfwd0 = scaraKinematicsForward;
    *kinv0 = scaraKinematicsInverse;

    *kset1 = identityKinematicsSetup;
    *kfwd1 = identityKinematicsForward;
    *kinv1 = identityKinematicsInverse;

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
} // switchkinsSetup()

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    scara_params_t p;
    p.d1 = kp->params.scara.d1;
    p.d2 = kp->params.scara.d2;
    p.d3 = kp->params.scara.d3;
    p.d4 = kp->params.scara.d4;
    p.d5 = kp->params.scara.d5;
    p.d6 = kp->params.scara.d6;
    return scara_forward_math(&p, joints, pos, NULL);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    scara_params_t p;
    p.d1 = kp->params.scara.d1;
    p.d2 = kp->params.scara.d2;
    p.d3 = kp->params.scara.d3;
    p.d4 = kp->params.scara.d4;
    p.d5 = kp->params.scara.d5;
    p.d6 = kp->params.scara.d6;
    return scara_inverse_math(&p, pos, joints, 0, NULL);
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    kinematics_params_t *kp = (kinematics_params_t *)params;
    (void)read_bit;
    (void)read_s32;

    read_float("scarakins.D1", &kp->params.scara.d1);
    read_float("scarakins.D2", &kp->params.scara.d2);
    read_float("scarakins.D3", &kp->params.scara.d3);
    read_float("scarakins.D4", &kp->params.scara.d4);
    read_float("scarakins.D5", &kp->params.scara.d5);
    read_float("scarakins.D6", &kp->params.scara.d6);

    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
