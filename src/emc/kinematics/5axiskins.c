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

#include "motion.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "rtapi_ctype.h"
#include "kinematics.h"
#include "posemath.h"
#include "switchkins.h"

/* ========================================================================
 * Internal math (was in 5axiskins_math.h)
 * ======================================================================== */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef TO_RAD
#define TO_RAD (M_PI / 180.0)
#endif

typedef struct {
    double pivot_length;
} fiveaxis_params_t;

typedef struct {
    int jx, jy, jz;
    int ja, jb, jc;
    int ju, jv, jw;
} fiveaxis_joints_t;

static void fiveaxis_s2r(double r, double t, double p,
                          double *x, double *y, double *z) {
    double t_rad = TO_RAD * t;
    double p_rad = TO_RAD * p;
    *x = r * sin(p_rad) * cos(t_rad);
    *y = r * sin(p_rad) * sin(t_rad);
    *z = r * cos(p_rad);
}

static int fiveaxis_forward_impl(const fiveaxis_params_t *params,
                                  const fiveaxis_joints_t *jm,
                                  const double *joints,
                                  EmcPose *pos) {
    double rx, ry, rz;
    double b_val = (jm->jb >= 0) ? joints[jm->jb] : 0;
    double c_val = (jm->jc >= 0) ? joints[jm->jc] : 0;
    double w_val = (jm->jw >= 0) ? joints[jm->jw] : 0;

    fiveaxis_s2r(params->pivot_length + w_val, c_val, 180.0 - b_val,
                 &rx, &ry, &rz);

    pos->tran.x = (jm->jx >= 0 ? joints[jm->jx] : 0) + rx;
    pos->tran.y = (jm->jy >= 0 ? joints[jm->jy] : 0) + ry;
    pos->tran.z = (jm->jz >= 0 ? joints[jm->jz] : 0) + params->pivot_length + rz;
    pos->a = (jm->ja >= 0) ? joints[jm->ja] : 0;
    pos->b = b_val;
    pos->c = c_val;
    pos->u = (jm->ju >= 0) ? joints[jm->ju] : 0;
    pos->v = (jm->jv >= 0) ? joints[jm->jv] : 0;
    pos->w = w_val;

    return 0;
}

static int fiveaxis_inverse_impl(const fiveaxis_params_t *params,
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

static void fiveaxis_axis_to_joints(const fiveaxis_joints_t *jm,
                                     const EmcPose *axis_values,
                                     double *joints) {
    if (jm->jx >= 0) joints[jm->jx] = axis_values->tran.x;
    if (jm->jy >= 0) joints[jm->jy] = axis_values->tran.y;
    if (jm->jz >= 0) joints[jm->jz] = axis_values->tran.z;
    if (jm->ja >= 0) joints[jm->ja] = axis_values->a;
    if (jm->jb >= 0) joints[jm->jb] = axis_values->b;
    if (jm->jc >= 0) joints[jm->jc] = axis_values->c;
    if (jm->ju >= 0) joints[jm->ju] = axis_values->u;
    if (jm->jv >= 0) joints[jm->jv] = axis_values->v;
    if (jm->jw >= 0) joints[jm->jw] = axis_values->w;
}

/* ========================================================================
 * RT interface (reads HAL pins)
 * ======================================================================== */

struct haldata {
    hal_float_t *pivot_length;
} *haldata;
static int fiveaxis_max_joints;

// Joint mapping struct
static fiveaxis_joints_t jmap;

// assignments of principal joints to axis letters:
// (-1 means not defined (yet))
static int JX = -1;
static int JY = -1;
static int JZ = -1;
static int JA = -1;
static int JB = -1;
static int JC = -1;
static int JU = -1;
static int JV = -1;
static int JW = -1;

static int fiveaxis_KinematicsForward(const double *joints,
                                      EmcPose * pos,
                                      const KINEMATICS_FORWARD_FLAGS * fflags,
                                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    fiveaxis_params_t params;
    params.pivot_length = *(haldata->pivot_length);
    return fiveaxis_forward_impl(&params, &jmap, joints, pos);
} //fiveaxis_KinematicsForward()

static int fiveaxis_KinematicsInverse(const EmcPose * pos,
                                      double *joints,
                                      const KINEMATICS_INVERSE_FLAGS * iflags,
                                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    fiveaxis_params_t params;
    params.pivot_length = *(haldata->pivot_length);

    // Compute axis values using pure math function
    EmcPose P;
    fiveaxis_inverse_impl(&params, pos, &P);

    // update joints with support for
    // multiple-joints per-coordinate letter:
    // based on computed position
    position_to_mapped_joints(fiveaxis_max_joints,
                              &P,
                              joints);
    return 0;
} // fiveaxis_kinematicsInverse()

int fiveaxis_KinematicsSetup(const  int   comp_id,
                             const  char* coordinates,
                             kparms*      kp)
{
    int result=0;
    int i,jno;
    int axis_idx_for_jno[EMCMOT_MAX_JOINTS];
    int minjoints = strlen(kp->required_coordinates);
    fiveaxis_max_joints = strlen(coordinates); // allow for dup coords

    if (fiveaxis_max_joints > kp->max_joints) {
        rtapi_print_msg(RTAPI_MSG_ERR,
             "ERROR %s: coordinates=%s requires %d joints, max joints=%d\n",
             kp->kinsname,
             coordinates,
             fiveaxis_max_joints,
             kp->max_joints);
        goto error;
    }

    if (map_coordinates_to_jnumbers(coordinates,
                                    kp->max_joints,
                                    kp->allow_duplicates,
                                    axis_idx_for_jno)) {
       goto error;
    }
    // require all chars in reqd_coordinates (order doesn't matter)
    for (i=0; i < minjoints; i++) {
        char  reqd_char;
        reqd_char = *(kp->required_coordinates + i);
        if (   !strchr(coordinates,toupper(reqd_char))
            && !strchr(coordinates,tolower(reqd_char)) ) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                 "ERROR %s:\nrequired  coordinates:%s\n"
                           "specified coordinates:%s\n",
                 kp->kinsname, kp->required_coordinates, coordinates);
            goto error;
        }
    }
    // assign principal joint numbers (first found in coordinates map)
    // duplicates are handled by position_to_mapped_joints()
    for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
        if (axis_idx_for_jno[jno] == 0) {if (JX == -1) JX=jno;}
        if (axis_idx_for_jno[jno] == 1) {if (JY == -1) JY=jno;}
        if (axis_idx_for_jno[jno] == 2) {if (JZ == -1) JZ=jno;}
        if (axis_idx_for_jno[jno] == 3) {if (JA == -1) JA=jno;}
        if (axis_idx_for_jno[jno] == 4) {if (JB == -1) JB=jno;}
        if (axis_idx_for_jno[jno] == 5) {if (JC == -1) JC=jno;}
        if (axis_idx_for_jno[jno] == 6) {if (JU == -1) JU=jno;}
        if (axis_idx_for_jno[jno] == 7) {if (JV == -1) JV=jno;}
        if (axis_idx_for_jno[jno] == 8) {if (JW == -1) JW=jno;}
    }

    // Populate joint map struct for math functions
    jmap.jx = JX; jmap.jy = JY; jmap.jz = JZ;
    jmap.ja = JA; jmap.jb = JB; jmap.jc = JC;
    jmap.ju = JU; jmap.jv = JV; jmap.jw = JW;

    haldata = hal_malloc(sizeof(struct haldata));

    result = hal_pin_float_newf(HAL_IN,&(haldata->pivot_length),comp_id,
                                "%s.pivot-length",kp->halprefix);
    if(result < 0) goto error;

    *haldata->pivot_length = DEFAULT_PIVOT_LENGTH;

    rtapi_print("Kinematics Module %s\n",__FILE__);
    rtapi_print("  module name = %s\n"
                "  coordinates = %s  Requires: [KINS]JOINTS>=%d\n"
                "  sparm       = %s\n",
                kp->kinsname,
                coordinates,fiveaxis_max_joints,
                kp->sparm?kp->sparm:"NOTSPECIFIED");
    rtapi_print("  default pivot-length = %.3f\n",*haldata->pivot_length);

    return 0;

error:
    return -1;
} // fiveaxis_KinematicsSetup()

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    kp->kinsname    = "5axiskins"; // !!! must agree with filename
    kp->halprefix   = "5axiskins"; // hal pin names
    kp->required_coordinates = REQUIRED_COORDINATES;
    kp->allow_duplicates     = 1;
    kp->max_joints           = EMCMOT_MAX_JOINTS;

    if (kp->sparm && strstr(kp->sparm,"identityfirst")) {
        rtapi_print("\n!!! switchkins-type 0 is IDENTITY\n");
        *kset0 = identityKinematicsSetup;
        *kfwd0 = identityKinematicsForward;
        *kinv0 = identityKinematicsInverse;

        *kset1 = fiveaxis_KinematicsSetup;
        *kfwd1 = fiveaxis_KinematicsForward;
        *kinv1 = fiveaxis_KinematicsInverse;
    } else {
        rtapi_print("\n!!! switchkins-type 0 is %s\n",kp->kinsname);
        *kset0 = fiveaxis_KinematicsSetup;
        *kfwd0 = fiveaxis_KinematicsForward;
        *kinv0 = fiveaxis_KinematicsInverse;

        *kset1 = identityKinematicsSetup;
        *kfwd1 = identityKinematicsForward;
        *kinv1 = identityKinematicsInverse;
    }
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
    fiveaxis_params_t p;
    fiveaxis_joints_t jm;

    p.pivot_length = kp->params.fiveaxis.pivot_length;
    jm.jx = kp->axis_to_joint[0]; jm.jy = kp->axis_to_joint[1];
    jm.jz = kp->axis_to_joint[2]; jm.ja = kp->axis_to_joint[3];
    jm.jb = kp->axis_to_joint[4]; jm.jc = kp->axis_to_joint[5];
    jm.ju = kp->axis_to_joint[6]; jm.jv = kp->axis_to_joint[7];
    jm.jw = kp->axis_to_joint[8];

    return fiveaxis_forward_impl(&p, &jm, joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    fiveaxis_params_t p;
    fiveaxis_joints_t jm;
    EmcPose axis_values;

    p.pivot_length = kp->params.fiveaxis.pivot_length;
    jm.jx = kp->axis_to_joint[0]; jm.jy = kp->axis_to_joint[1];
    jm.jz = kp->axis_to_joint[2]; jm.ja = kp->axis_to_joint[3];
    jm.jb = kp->axis_to_joint[4]; jm.jc = kp->axis_to_joint[5];
    jm.ju = kp->axis_to_joint[6]; jm.jv = kp->axis_to_joint[7];
    jm.jw = kp->axis_to_joint[8];

    fiveaxis_inverse_impl(&p, pos, &axis_values);
    fiveaxis_axis_to_joints(&jm, &axis_values, joints);
    return 0;
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    kinematics_params_t *kp = (kinematics_params_t *)params;
    (void)read_bit; (void)read_s32;

    if (read_float("5axiskins.pivot-length",
                   &kp->params.fiveaxis.pivot_length) != 0)
        return -1;

    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
