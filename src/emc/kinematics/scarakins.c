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

#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>

#include <switchkins.h>

static struct scara_data {
    hal_real_t d1, d2, d3, d4, d5, d6;
} *haldata = NULL;

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

/* joint[0], joint[1] and joint[3] are in degrees and joint[2] is in length units */
static
int scaraKinematicsForward(const double * joint,
                      EmcPose * world,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    double a0, a1, a3;
    double x, y, z, c;

/* convert joint angles to radians for sin() and cos() */

    a0 = joint[0] * ( PM_PI / 180 );
    a1 = joint[1] * ( PM_PI / 180 );
    a3 = joint[3] * ( PM_PI / 180 );
/* convert angles into world coords */

    a1 = a1 + a0;
    a3 = a3 + a1;

    rtapi_real D1 = hal_get_real(haldata->d1);
    rtapi_real D2 = hal_get_real(haldata->d2);
    rtapi_real D3 = hal_get_real(haldata->d3);
    rtapi_real D4 = hal_get_real(haldata->d4);
    rtapi_real D5 = hal_get_real(haldata->d5);
    rtapi_real D6 = hal_get_real(haldata->d6);

    x = D2*cos(a0) + D4*cos(a1) + D6*cos(a3);
    y = D2*sin(a0) + D4*sin(a1) + D6*sin(a3);
    z = D1 + D3 - joint[2] - D5;
    c = a3;

    // the elbow flag: which sign the inverse gives the acos of joint 1
    *iflags = 0;
    if (joint[1] < 0)
        *iflags = 1;

    world->tran.x = x;
    world->tran.y = y;
    world->tran.z = z;
    world->c = c * 180 / PM_PI;

    world->a = joint[4];
    world->b = joint[5];

    return (0);
} //scaraKinematicsForward()

static int scaraKinematicsInverse(const EmcPose * world,
                                  double * joint,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags)
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
    a3 = c * ( PM_PI / 180 );

    rtapi_real D1 = hal_get_real(haldata->d1);
    rtapi_real D2 = hal_get_real(haldata->d2);
    rtapi_real D3 = hal_get_real(haldata->d3);
    rtapi_real D4 = hal_get_real(haldata->d4);
    rtapi_real D5 = hal_get_real(haldata->d5);
    rtapi_real D6 = hal_get_real(haldata->d6);

    /* center of end effector (correct for D6) */
    xt = x - D6*cos(a3);
    yt = y - D6*sin(a3);

    /* horizontal distance (squared) from end effector centerline
        to main column centerline */
    rsq = xt*xt + yt*yt;
    /* joint 1 angle needed to make arm length match sqrt(rsq) */
    cc = (rsq - D2*D2 - D4*D4) / (2*D2*D4);
    if(cc < -1) cc = -1;
    if(cc > 1) cc = 1;
    q1 = acos(cc);

    if (*iflags)
        q1 = -q1;

    /* angle to end effector */
    q0 = atan2(yt, xt);

    /* end effector coords in inner arm coord system */
    xt = D2 + D4*cos(q1);
    yt = D4*sin(q1);

    /* inner arm angle */
    q0 = q0 - atan2(yt, xt);

    /* q0 and q1 are still in radians. convert them to degrees */
    q0 = q0 * (180 / PM_PI);
    q1 = q1 * (180 / PM_PI);

    joint[0] = q0;
    joint[1] = q1;
    joint[2] = D1 + D3 - D5 - z;
    joint[3] = c - ( q0 + q1);
    joint[4] = world->a;
    joint[5] = world->b;

    *fflags = 0;

    return (0);
} // scaraKinematicsInverse()

static int scaraKinematicsJacobian(const double * joint,
                                   const EmcPose * world,
                                   double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                                   const KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)iflags;
    rtapi_real D2 = hal_get_real(haldata->d2);
    rtapi_real D4 = hal_get_real(haldata->d4);
    rtapi_real D6 = hal_get_real(haldata->d6);
    const double a3 = world->c * (PM_PI / 180);
    const double q1 = joint[1] * (PM_PI / 180);
    const double xt = world->tran.x - D6*cos(a3);
    const double yt = world->tran.y - D6*sin(a3);
    const double rsq = xt*xt + yt*yt;
    /* gradients over (x, y, c) of the quantities the inverse builds */
    double d_xt[3] = { 1, 0,  D6*sin(a3) * (PM_PI/180) };
    double d_yt[3] = { 0, 1, -D6*cos(a3) * (PM_PI/180) };
    double d_q1[3], d_q0[3], dphi_dq1;
    int i, j, a;

    if (rsq <= 0 || fabs(sin(q1)) < 1e-12) {
        /* the arm folded or straight out: the elbow rate is unbounded */
        return -1;
    }
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }

    /* rsq = D2^2 + D4^2 + 2 D2 D4 cos(q1), so q1 follows rsq; q0 is the
       bearing of the end effector less the angle the outer arm subtends,
       whose rate over q1 is (D2 D4 cos(q1) + D4^2) / rsq */
    dphi_dq1 = (D2*D4*cos(q1) + D4*D4) / rsq;
    for (i = 0; i < 3; i++) {
        double d_rsq = 2*xt*d_xt[i] + 2*yt*d_yt[i];
        d_q1[i] = -d_rsq / (2*D2*D4*sin(q1));
        d_q0[i] = (xt*d_yt[i] - yt*d_xt[i]) / rsq - dphi_dq1 * d_q1[i];
    }

    /* columns x, y, c; the rest of the pose does not reach these joints */
    for (i = 0; i < 3; i++) {
        int col = (i == 2) ? 5 : i;
        jac[0][col] = d_q0[i] * (180 / PM_PI);
        jac[1][col] = d_q1[i] * (180 / PM_PI);
        jac[3][col] = -(jac[0][col] + jac[1][col]);
    }
    jac[3][5] += 1;
    jac[2][2] = -1;
    jac[4][3] = 1;
    jac[5][4] = 1;
    return 0;
} // scaraKinematicsJacobian()

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

    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->d1), DEFAULT_D1, "%s.D1", kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->d2), DEFAULT_D2, "%s.D2", kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->d3), DEFAULT_D3, "%s.D3", kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->d4), DEFAULT_D4, "%s.D4", kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->d5), DEFAULT_D5, "%s.D5", kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->d6), DEFAULT_D6, "%s.D6", kp->halprefix);
    if (res) { goto error; }

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
    switchkinsRegisterJacobian(0, scaraKinematicsJacobian);

    *kset1 = identityKinematicsSetup;
    *kfwd1 = identityKinematicsForward;
    *kinv1 = identityKinematicsInverse;

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
} // switchkinsSetup()
