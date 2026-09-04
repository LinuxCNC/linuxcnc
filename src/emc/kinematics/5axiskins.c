/********************************************************************
* Description: 5axiskins.c
*   kinematics for XYZBC 5 axis bridge mill
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2007 Chris Radek
*
* Notes:
*  1) pivot_length hal pin must agree with mechanical
*     design (including vismach simulation) and augmented
*     with current tool z offset
*     (typ: mechanical_pivot_length + motion.tooloffset.z)
*  2) C axis: spherical coordinates aziumthal angle (t or theta)
*     projection of radius to xy plane
*  3) B axis: spherical coordinates polar angle (p or phi)
*     wrt z axis
*  4) W axis: tool motion. Negative values increase tool radial
*     motion example: drilling into body at b,c angles
*  5) W axis motion is incorporated into the motion of the
*     joints used for X,Y,Z positioning and no motor or
*     hal pin connections are required for the joint specified
*     as JW.  However, a joint must be configured for W to
*     support display of the W axis letter value for
*     complicated reasons. (motion/control.c computes joint
*     positions only for the number of configured kinematic
*     joints (NO_OF_KINS_JOINTS) and the joint positions
*     are needed to display axis letters via inverse
*     kinematics.
*  6) If no coordinates module parameter is supplied, kins
*     will use the required coordinates XYZBCW mapped
*     to joints 0..5 in sequence.
*  7) Multiple joints may be assigned to an axis letter
*     with the module coordinates parameter
*  8) If a coordinates module parameter is supplied,
*     the kins will map coordinate letters in sequence
*     to joint numbers beginning with joint 0.
*  9) Coordinates XYZBCW are required, AUV may be used
*     if specified with the coordinates parameter and will
*     be mapped one-to-one with the assigned joint.
* 10) The direction of the tilt axis is the opposite of the
*     conventional axis direction. See
*     https://linuxcnc.org/docs/html/gcode/machining-center.html
********************************************************************/

// non-required coordinates (A,U,V) can be set by using
// the module coordinates parameter
#define REQUIRED_COORDINATES "XYZBCW"

#define DEFAULT_PIVOT_LENGTH 250

#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>
#include <emcmotcfg.h>

#include <switchkins.h>

// the geometry, one pin each; the maths reads it from the block
static const kins_param_desc fiveaxis_params[] = {
    { "pivot-length", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_PIVOT_LENGTH },
};
enum { P_PIVOT_LENGTH };

// assignments of principal joints to axis letters, from the block
// (-1 means not defined)
#define JX (p->joint_of_axis[0])
#define JY (p->joint_of_axis[1])
#define JZ (p->joint_of_axis[2])
#define JA (p->joint_of_axis[3])
#define JB (p->joint_of_axis[4])
#define JC (p->joint_of_axis[5])
#define JU (p->joint_of_axis[6])
#define JV (p->joint_of_axis[7])
#define JW (p->joint_of_axis[8])

static PmCartesian s2r(double r, double t, double p) {
    // s2r: spherical coordinates to cartesian coordinates
    // r       = length of vector
    // p=phi   = angle of vector wrt z axis
    // t=theta = angle of vector projected onto xy plane
    //           (projection length in xy plane is r*sin(p)
    PmCartesian c;
    t = TO_RAD*t; p = TO_RAD*p; // degrees to radians

    c.x = r * sin(p) * cos(t);
    c.y = r * sin(p) * sin(t);
    c.z = r * cos(p);

    return c;
} //s2r()

static int fiveaxis_forward(const kins_params *p, kins_scratch *s,
                            const double *joints,
                            EmcPose * pos,
                            const KINEMATICS_FORWARD_FLAGS * fflags,
                            KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)s;
    (void)fflags;
    (void)iflags;
    double pivot_length = p->geometry[P_PIVOT_LENGTH];
    PmCartesian r = s2r(pivot_length + joints[JW],
                        joints[JC],
                        180.0 - joints[JB]);

    // Note: 'principal' joints are used
    pos->tran.x = joints[JX] + r.x;
    pos->tran.y = joints[JY] + r.y;
    pos->tran.z = joints[JZ] + pivot_length + r.z;
    pos->b      = joints[JB];
    pos->c      = joints[JC];
    pos->w      = joints[JW];

    // optional letters (specify with coordinates module parameter)
    pos->a = (JA != -1)? joints[JA] : 0;
    pos->u = (JU != -1)? joints[JU] : 0;
    pos->v = (JV != -1)? joints[JV] : 0;

    return 0;
} // fiveaxis_forward()

static int fiveaxis_inverse(const kins_params *p, kins_scratch *s,
                            const EmcPose * pos,
                            double *joints,
                            const KINEMATICS_INVERSE_FLAGS * iflags,
                            KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)s;
    (void)iflags;
    (void)fflags;
    double pivot_length = p->geometry[P_PIVOT_LENGTH];
    PmCartesian r = s2r(pivot_length + pos->w,
                        pos->c,
                        180.0 - pos->b);

    EmcPose P;  // computed position
    P.tran.x = pos->tran.x - r.x;
    P.tran.y = pos->tran.y - r.y;
    P.tran.z = pos->tran.z - pivot_length - r.z;

    P.b = pos->b;
    P.c = pos->c;
    P.w = pos->w;

    // optional letters (specify with coordinates module parameter)
    P.a = (JA != -1)? pos->a : 0;
    P.u = (JU != -1)? pos->u : 0;
    P.v = (JV != -1)? pos->v : 0;

    // update joints with support for
    // multiple-joints per-coordinate letter:
    // based on computed position
    return kinsPoseToMappedJoints(p, &P, joints);
} // fiveaxis_inverse()

static int fiveaxis_jacobian(const kins_params *p,
                             const double *joints,
                             const EmcPose * pos,
                             double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                             const KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)joints;
    (void)iflags;
    const double R  = p->geometry[P_PIVOT_LENGTH] + pos->w;
    const double sb = sin(TO_RAD*pos->b), cb = cos(TO_RAD*pos->b);
    const double sc = sin(TO_RAD*pos->c), cc = cos(TO_RAD*pos->c);
    double dP[EMCMOT_MAX_AXIS][EMCMOT_MAX_AXIS];
    int a, b;

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        for (b = 0; b < EMCMOT_MAX_AXIS; b++) { dP[a][b] = 0; }
    }

    // the computed position of the inverse is the pose less the pivot
    // vector r = s2r(R, c, 180 - b), which is (R sin b cos c, R sin b sin c,
    // -R cos b); each row is that coordinate differentiated
    dP[0][0] = 1;
    dP[0][4] = -R * cb * cc * TO_RAD;
    dP[0][5] =  R * sb * sc * TO_RAD;
    dP[0][8] = -sb * cc;

    dP[1][1] = 1;
    dP[1][4] = -R * cb * sc * TO_RAD;
    dP[1][5] = -R * sb * cc * TO_RAD;
    dP[1][8] = -sb * sc;

    dP[2][2] = 1;
    dP[2][4] = -R * sb * TO_RAD;
    dP[2][8] =  cb;

    for (a = 3; a < EMCMOT_MAX_AXIS; a++) { dP[a][a] = 1; }

    return kinsJacobianFromMappedAxesP(p, (const double (*)[EMCMOT_MAX_AXIS])dP,
                                       jac);
} // fiveaxis_jacobian()

static const kins_ops fiveaxis_ops = {
    .forward  = fiveaxis_forward,
    .inverse  = fiveaxis_inverse,
    .jacobian = fiveaxis_jacobian,
};

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    (void)kset0; (void)kset1; (void)kset2;
    (void)kfwd0; (void)kfwd1; (void)kfwd2;
    (void)kinv0; (void)kinv1; (void)kinv2;
    kp->kinsname    = "5axiskins"; // !!! must agree with filename
    kp->halprefix   = "5axiskins"; // hal pin names
    kp->required_coordinates = REQUIRED_COORDINATES;
    kp->allow_duplicates     = 1;
    kp->max_joints           = EMCMOT_MAX_JOINTS;
    kp->params               = fiveaxis_params;
    kp->nparams              = sizeof(fiveaxis_params)/sizeof(fiveaxis_params[0]);

    if (kp->sparm && strstr(kp->sparm,"identityfirst")) {
        rtapi_print("\n!!! switchkins-type 0 is IDENTITY\n");
        switchkinsRegisterOps(0, &KINS_IDENTITY_OPS);
        switchkinsRegisterOps(1, &fiveaxis_ops);
    } else {
        rtapi_print("\n!!! switchkins-type 0 is %s\n",kp->kinsname);
        switchkinsRegisterOps(0, &fiveaxis_ops);
        switchkinsRegisterOps(1, &KINS_IDENTITY_OPS);
    }
    switchkinsRegisterOps(2, &USERK_OPS);

    return 0;
} // switchkinsSetup()
