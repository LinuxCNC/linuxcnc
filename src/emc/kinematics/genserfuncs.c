/********************************************************************
* Description: genserfuncs.c
*   (originally part of genserkins.c)
*   Kinematics for a generalised serial kinematics machine
*
*   Derived from a work by Fred Proctor,
*   changed to work with emc2 and HAL
*
* Adapting Author: Alex Joni
* License: GPL Version 2
* System: Linux
*
* Users:
*    1) genserkins.c  -- kinematics modules
*    2) ugenserkins.c -- usermode test program (needs work)
*******************************************************************

  These are the forward and inverse kinematic functions for a general
  serial-link manipulator. Thanks to Herman Bruyninckx and John
  Hallam at http://www.roble.info/ for this.

  The functions are general enough to be configured for any serial
  configuration.
  The kinematics use Denavit-Hartenberg definition for the joint and
  links. The DH definitions are the ones used by John J Craig in
  "Introduction to Robotics: Mechanics and Control"
  The parameters for the manipulator are defined by hal pins.
  Currently the type of the joints is hardcoded to ANGULAR, although
  the kins support both ANGULAR and LINEAR axes.

  The maths is written as pure functions of the parameter block (see
  kinematics.h): the pins are the table below, read into the block
  before every call, and the link description is built from the block
  on each call.

  TODO:
    * make number of joints a loadtime parameter
    * add HAL pins for all settable parameters, including joint type: ANGULAR / LINEAR
*/

#ifdef RTAPI
#include <rtapi.h>
#endif
#include <rtapi_math.h>
#include <hal.h>
#include "libposemath/gotypes.h"    /* go_result, go_integer */
#include "libposemath/gomath.h"     /* go_pose */
#include <kinematics.h>

#include "genserkins.h" /* these decls */

// Only gcc/g++ supports the #pragma
#if __GNUC__ && !defined(__clang__)
// The matrix and vector storage is just big.
// genser_kin_jac_inv() is 2112
// genser_inverse() is 2640 plus the link description it builds
  #pragma GCC diagnostic warning "-Wframe-larger-than=3400"
#endif

// the table: four entries per joint, then the iteration count in and out
#define P_A(i)     (4*(i) + 0)
#define P_ALPHA(i) (4*(i) + 1)
#define P_D(i)     (4*(i) + 2)
#define P_UNROT(i) (4*(i) + 3)
enum {
    P_LAST_ITER = 4*GENSER_MAX_JOINTS,
    P_MAX_ITER,
    P_COUNT
};

#define JOINT_ROWS(i, a, alpha, d) \
    { "A-" #i,        KINS_PARAM_FLOAT, KINS_IN, 0, a }, \
    { "ALPHA-" #i,    KINS_PARAM_FLOAT, KINS_IN, 0, alpha }, \
    { "D-" #i,        KINS_PARAM_FLOAT, KINS_IN, 0, d }, \
    { "unrotate-" #i, KINS_PARAM_S32,   KINS_IN, 0, 0 }

const kins_param_desc GENSER_PARAMS[P_COUNT] = {
    JOINT_ROWS(0, DEFAULT_A1, DEFAULT_ALPHA1, DEFAULT_D1),
    JOINT_ROWS(1, DEFAULT_A2, DEFAULT_ALPHA2, DEFAULT_D2),
    JOINT_ROWS(2, DEFAULT_A3, DEFAULT_ALPHA3, DEFAULT_D3),
    JOINT_ROWS(3, DEFAULT_A4, DEFAULT_ALPHA4, DEFAULT_D4),
    JOINT_ROWS(4, DEFAULT_A5, DEFAULT_ALPHA5, DEFAULT_D5),
    JOINT_ROWS(5, DEFAULT_A6, DEFAULT_ALPHA6, DEFAULT_D6),
    [P_LAST_ITER] = { "last-iterations", KINS_PARAM_U32, KINS_OUT, 0, 0 },
    [P_MAX_ITER]  = { "max-iterations",  KINS_PARAM_U32, KINS_IN,  0, GENSER_DEFAULT_MAX_ITERATIONS },
};
const int GENSER_NPARAMS = P_COUNT;

#if GENSER_MAX_JOINTS < 6
#error GENSER_MAX_JOINTS must be at least 6; fix genserkins.h
#endif

void genser_links_of(const kins_params *p, genser_struct *genser) {
    int t;

    static volatile double tst=0;tst=sqrt(tst); // ensure -lm used
    /* init them all and make them revolute joints */
    /* FIXME: should allow LINEAR joints based on HAL param too */
    for (t = 0; t < GENSER_MAX_JOINTS; t++) {
        genser->links[t].u.dh.a = p->geometry[P_A(t)];
        genser->links[t].u.dh.alpha = p->geometry[P_ALPHA(t)];
        genser->links[t].u.dh.d = p->geometry[P_D(t)];
        genser->links[t].u.dh.theta = 0;
        genser->links[t].type = GO_LINK_DH;
        genser->links[t].quantity = GO_QUANTITY_ANGLE;
    }

    /* set a select few to make it PUMA-like */
    // FIXME-AJ: make a hal pin, also set number of joints based on it
    genser->link_num = 6;
    genser->iterations = 0;
} // genser_links_of()

/* the unrotate coupling of one joint, from the block */
static rtapi_s32 unrotate_of(const kins_params *p, int link)
{
    return (rtapi_s32)p->geometry[P_UNROT(link)];
}

/* compute the forward jacobian function:
   the jacobian is a linear approximation of the kinematics function.
   It is calculated using derivation of the position transformation matrix,
   and usually used for feeding velocities through it.
   It is analytically possible to calculate the inverse of the jacobian
   (sometimes only the pseudoinverse) and to use that for the inverse kinematics.
*/
int compute_jfwd(go_link * link_params,
                 int link_number,
                 go_matrix * Jfwd,
                 go_pose * T_L_0)
{
    GO_MATRIX_DECLARE(Jv, Jvstg, 3, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jw, Jwstg, 3, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_i_ip1, R_i_ip1stg, 3, 3);
    GO_MATRIX_DECLARE(scratch, scratchstg, 3, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_inv, R_invstg, 3, 3);
    go_pose pose;
    go_quat quat;
    go_vector P_ip1_i[3];
    int row, col;

    /* init matrices to possibly smaller size */
    go_matrix_init(Jv, Jvstg, 3, link_number);
    go_matrix_init(Jw, Jwstg, 3, link_number);
    go_matrix_init(R_i_ip1, R_i_ip1stg, 3, 3);
    go_matrix_init(scratch, scratchstg, 3, link_number);
    go_matrix_init(R_inv, R_invstg, 3, 3);

    Jv.el[0][0] = 0, Jv.el[1][0] = 0, Jv.el[2][0] = (GO_QUANTITY_LENGTH == link_params[0].quantity ? 1 : 0);
    Jw.el[0][0] = 0, Jw.el[1][0] = 0, Jw.el[2][0] = (GO_QUANTITY_ANGLE == link_params[0].quantity ? 1 : 0);

    /* initialize inverse rotational transform */
    if (GO_LINK_DH == link_params[0].type) {
        go_dh_pose_convert(&link_params[0].u.dh, &pose);
    } else if (GO_LINK_PP == link_params[0].type) {
        pose = link_params[0].u.pp.pose;
    } else {
        return GO_RESULT_IMPL_ERROR;
    }

    *T_L_0 = pose;

    for (col = 1; col < link_number; col++) {
        /* T_ip1_i */
        if (GO_LINK_DH == link_params[col].type) {
            go_dh_pose_convert(&link_params[col].u.dh, &pose);
        } else if (GO_LINK_PP == link_params[col].type) {
            pose = link_params[col].u.pp.pose;
        } else {
            return GO_RESULT_IMPL_ERROR;
        }

        go_cart_vector_convert(&pose.tran, P_ip1_i);
        go_quat_inv(&pose.rot, &quat);
        go_quat_matrix_convert(&quat, &R_i_ip1);

        /* Jv */
        go_matrix_vector_cross(&Jw, P_ip1_i, &scratch);
        go_matrix_matrix_add(&Jv, &scratch, &scratch);
        go_matrix_matrix_mult(&R_i_ip1, &scratch, &Jv);
        Jv.el[0][col] = 0, Jv.el[1][col] = 0, Jv.el[2][col] = (GO_QUANTITY_LENGTH == link_params[col].quantity ? 1 : 0);
        /* Jw */
        go_matrix_matrix_mult(&R_i_ip1, &Jw, &Jw);
        Jw.el[0][col] = 0, Jw.el[1][col] = 0, Jw.el[2][col] = (GO_QUANTITY_ANGLE == link_params[col].quantity ? 1 : 0);
        if (GO_LINK_DH == link_params[col].type) {
            go_dh_pose_convert(&link_params[col].u.dh, &pose);
        } else if (GO_LINK_PP == link_params[col].type) {
            pose = link_params[col].u.pp.pose;
        } else {
            return GO_RESULT_IMPL_ERROR;
        }
        go_pose_pose_mult(T_L_0, &pose, T_L_0);
    }

    /* rotate back into {0} frame */
    go_quat_matrix_convert(&T_L_0->rot, &R_inv);
    go_matrix_matrix_mult(&R_inv, &Jv, &Jv);
    go_matrix_matrix_mult(&R_inv, &Jw, &Jw);

    /* put Jv atop Jw in J */
    for (row = 0; row < 6; row++) {
        for (col = 0; col < link_number; col++) {
            if (row < 3) {
                Jfwd->el[row][col] = Jv.el[row][col];
            } else {
                Jfwd->el[row][col] = Jw.el[row - 3][col];
            }
        }
    }

    return GO_RESULT_OK;
}

/* compute the inverse of the jacobian matrix */
int compute_jinv(go_matrix * Jfwd, go_matrix * Jinv)
{
    int retval;
    GO_MATRIX_DECLARE(JT, JTstg, GENSER_MAX_JOINTS, 6);

    /* compute inverse, or pseudo-inverse */
    if (Jfwd->rows == Jfwd->cols) {
        retval = go_matrix_inv(Jfwd, Jinv);
        if (GO_RESULT_OK != retval)
            return retval;
    } else if (Jfwd->rows < Jfwd->cols) {
        /* underdetermined, optimize on smallest sum of square of speeds */
        /* JT(JJT)inv */
        GO_MATRIX_DECLARE(JJT, JJTstg, 6, 6);

        go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
        go_matrix_init(JJT, JJTstg, Jfwd->rows, Jfwd->rows);
        go_matrix_transpose(Jfwd, &JT);
        go_matrix_matrix_mult(Jfwd, &JT, &JJT);
        retval = go_matrix_inv(&JJT, &JJT);
        if (GO_RESULT_OK != retval)
            return retval;
        go_matrix_matrix_mult(&JT, &JJT, Jinv);
    } else {
        /* overdetermined, do least-squares best fit */
        /* (JTJ)invJT */
        GO_MATRIX_DECLARE(JTJ, JTJstg, GENSER_MAX_JOINTS, GENSER_MAX_JOINTS);

        go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
        go_matrix_init(JTJ, JTJstg, Jfwd->cols, Jfwd->cols);
        go_matrix_transpose(Jfwd, &JT);
        go_matrix_matrix_mult(&JT, Jfwd, &JTJ);
        retval = go_matrix_inv(&JTJ, &JTJ);
        if (GO_RESULT_OK != retval)
            return retval;
        go_matrix_matrix_mult(&JTJ, &JT, Jinv);
    }

    return GO_RESULT_OK;
}

int genser_kin_jac_inv(void *kins,
    const go_pose * pos,
    const go_screw * vel, const go_real * joints, go_real * jointvels)
{
    (void)pos;
    genser_struct *genser = (genser_struct *) kins;
    GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);
    go_pose T_L_0;
    go_link linkout[GENSER_MAX_JOINTS] = {};
    go_real vw[6];
    int link;
    int retval;

    go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);
    go_matrix_init(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);

    for (link = 0; link < genser->link_num; link++) {
        retval =
            go_link_joint_set(&genser->links[link], joints[link],
            &linkout[link]);
        if (GO_RESULT_OK != retval)
            return retval;
    }
    retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
    if (GO_RESULT_OK != retval)
        return retval;
    retval = compute_jinv(&Jfwd, &Jinv);
    if (GO_RESULT_OK != retval)
        return retval;

    vw[0] = vel->v.x;
    vw[1] = vel->v.y;
    vw[2] = vel->v.z;
    vw[3] = vel->w.x;
    vw[4] = vel->w.y;
    vw[5] = vel->w.z;

    return go_matrix_vector_mult(&Jinv, vw, jointvels);
}

int genser_kin_jac_fwd(void *kins,
    const go_real * joints,
    const go_real * jointvels, const go_pose * pos, go_screw * vel)
{
    (void)pos;
    genser_struct *genser = (genser_struct *) kins;
    GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
    go_pose T_L_0;
    go_link linkout[GENSER_MAX_JOINTS] = {};
    go_real vw[6];
    int link;
    int retval;

    go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);

    for (link = 0; link < genser->link_num; link++) {
        retval =
            go_link_joint_set(&genser->links[link], joints[link],
            &linkout[link]);
        if (GO_RESULT_OK != retval)
            return retval;
    }

    retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
    if (GO_RESULT_OK != retval)
        return retval;

    go_matrix_vector_mult(&Jfwd, jointvels, vw);
    vel->v.x = vw[0];
    vel->v.y = vw[1];
    vel->v.z = vw[2];
    vel->w.x = vw[3];
    vel->w.y = vw[4];
    vel->w.z = vw[5];

    return GO_RESULT_OK;
}

/* The Jacobian in the terms of kinematics.h: joints in degrees per pose
   word in EmcPose units, the derivative of genser_inverse().

   compute_jinv() gives the geometric inverse Jacobian, radians of joint per
   unit of base-frame twist.  A pose word rate is not a twist: the roll,
   pitch and yaw rates reach the angular velocity through E, the matrix of
   the axes each one turns about, for the RPY convention of go_rpy_mat_convert,
   R = Rz(yaw) Ry(pitch) Rx(roll).  So

       dq/dp = unrotate . deg . Jinv . blockdiag(I, E . rad)

   with the unit conversions and the unrotate coupling applied in the order
   the inverse applies them. */
static int genser_jacobian(const kins_params *p, const double *joint,
                           const EmcPose *world,
                           double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                           const KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)iflags;
    genser_struct genser_stg;
    genser_struct *genser = &genser_stg;
    GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);
    go_pose T_L_0;
    go_link linkout[GENSER_MAX_JOINTS] = {};
    go_real jest[GENSER_MAX_JOINTS];
    double E[3][3];
    double sb, cb, sc, cc;
    int link, i, j, a, m, retval;

    genser_links_of(p, genser);

    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }

    // the kinematic joint angles, in radians and with the unrotate
    // coupling removed, exactly as the forward prepares them
    for (link = 0; link < genser->link_num; link++) {
        rtapi_s32 unrotate = unrotate_of(p, link);
        jest[link] = joint[link] * (PM_PI / 180);
        if (link && unrotate)
            jest[link] -= unrotate * jest[link-1];
    }

    go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);
    go_matrix_init(Jinv, Jinv_stg, genser->link_num, 6);

    for (link = 0; link < genser->link_num; link++) {
        retval = go_link_joint_set(&genser->links[link], jest[link], &linkout[link]);
        if (GO_RESULT_OK != retval)
            return -1;
    }
    retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
    if (GO_RESULT_OK != retval)
        return -1;
    retval = compute_jinv(&Jfwd, &Jinv);
    if (GO_RESULT_OK != retval)
        return -1;   // singular: no finite joint rate follows the pose

    // E columns: the roll axis carried by pitch and yaw, the pitch axis
    // carried by yaw, and the yaw axis fixed
    sb = sin(world->b * PM_PI / 180); cb = cos(world->b * PM_PI / 180);
    sc = sin(world->c * PM_PI / 180); cc = cos(world->c * PM_PI / 180);
    E[0][0] = cb*cc; E[1][0] = cb*sc; E[2][0] = -sb;
    E[0][1] = -sc;   E[1][1] = cc;    E[2][1] = 0;
    E[0][2] = 0;     E[1][2] = 0;     E[2][2] = 1;

    for (i = 0; i < genser->link_num; i++) {
        // linear pose words: the twist column is the pose column, and the
        // joint comes out in radians
        for (a = 0; a < 3; a++) {
            jac[i][a] = Jinv.el[i][a] * (180 / PM_PI);
        }
        // angular pose words: through E, radians of pose word per degree
        // of pose word and degrees of joint per radian of joint cancel
        for (m = 0; m < 3; m++) {
            double s = 0;
            for (a = 0; a < 3; a++) { s += Jinv.el[i][3+a] * E[a][m]; }
            jac[i][3+m] = s;
        }
    }

    // the unrotate coupling, in link order as the inverse applies it
    for (link = 1; link < genser->link_num; link++) {
        rtapi_s32 unrotate = unrotate_of(p, link);
        if (unrotate) {
            for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
                jac[link][a] += unrotate * jac[link-1][a];
            }
        }
    }

    // uvw pass through as joints 6, 7, 8
    if (p->max_joints > 6) jac[6][6] = 1;
    if (p->max_joints > 7) jac[7][7] = 1;
    if (p->max_joints > 8) jac[8][8] = 1;

    return 0;
} // genser_jacobian()

/* main function called by emc2 for forward Kins */
static int genser_forward(const kins_params *p, kins_scratch *s,
                          const double *joint,
                          EmcPose * world,
                          const KINEMATICS_FORWARD_FLAGS * fflags,
                          KINEMATICS_INVERSE_FLAGS * iflags) {
    (void)s;
    (void)fflags;
    (void)iflags;

    genser_struct genser;
    go_pose pos;
    go_rpy rpy;
    go_real jcopy[GENSER_MAX_JOINTS]; // will hold the radian conversion of joints
    int ret = 0;
    int i;

    genser_links_of(p, &genser);

    for (i=0; i< 6; i++)  {
        // convert to radians to pass to genser_kin_fwd
        jcopy[i] = joint[i] * PM_PI / 180;
        rtapi_s32 unrotate = unrotate_of(p, i);
        if ((i) && unrotate)
            jcopy[i] -= unrotate * jcopy[i-1];
    }

    // AJ: convert from emc2 coords (XYZABC - which are actually rpy euler
    // angles)
    // to go angles (quaternions)
    rpy.y = world->c * PM_PI / 180;
    rpy.p = world->b * PM_PI / 180;
    rpy.r = world->a * PM_PI / 180;

    go_rpy_quat_convert(&rpy, &pos.rot);
    pos.tran.x = world->tran.x;
    pos.tran.y = world->tran.y;
    pos.tran.z = world->tran.z;

    //pass through unused 678 as uvw
    if (p->max_joints > 6) world->u = joint[6];
    if (p->max_joints > 7) world->v = joint[7];
    if (p->max_joints > 8) world->w = joint[8];

    // pos will be the world location
    // jcopy: joitn position in radians
    ret = genser_kin_fwd(&genser, jcopy, &pos);
    if (ret < 0)
        return ret;

    // AJ: convert back to emc2 coords
    ret = go_quat_rpy_convert(&pos.rot, &rpy);
    if (ret < 0)
        return ret;
    world->tran.x = pos.tran.x;
    world->tran.y = pos.tran.y;
    world->tran.z = pos.tran.z;
    world->a = rpy.r * 180 / PM_PI;
    world->b = rpy.p * 180 / PM_PI;
    world->c = rpy.y * 180 / PM_PI;

    return 0;
}

int genser_kin_fwd(void *kins, const go_real * joints, go_pose * pos)
{
    genser_struct *genser = kins;
    go_link linkout[GENSER_MAX_JOINTS] = {};

    int link;
    int retval;

    for (link = 0; link < genser->link_num; link++) {
        retval = go_link_joint_set(&genser->links[link], joints[link], &linkout[link]);
        if (GO_RESULT_OK != retval)
            return retval;
    }

    retval = go_link_pose_build(linkout, genser->link_num, pos);
    if (GO_RESULT_OK != retval)
        return retval;

    return GO_RESULT_OK;
}

static int genser_inverse(const kins_params *p, kins_scratch *s,
                          const EmcPose * world,
                          double *joints,
                          const KINEMATICS_INVERSE_FLAGS * iflags,
                          KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;

    genser_struct genser_stg;
    genser_struct *genser = &genser_stg;
    GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);
    go_pose T_L_0;
    go_real dvw[6];
    go_real jest[GENSER_MAX_JOINTS];
    go_real dj[GENSER_MAX_JOINTS];
    go_pose pos; // converted pose from EmcPose
    go_pose pest, pestinv, Tdelta;
    go_rpy rpy;
    go_rvec rvec;
    go_cart cart;
    go_link linkout[GENSER_MAX_JOINTS];
    int link;
    int smalls;
    int retval;
    const unsigned max_iterations = (unsigned)p->geometry[P_MAX_ITER];

    genser_links_of(p, genser);

    // FIXME-AJ: rpy or zyx ?
    rpy.y = world->c * PM_PI / 180;
    rpy.p = world->b * PM_PI / 180;
    rpy.r = world->a * PM_PI / 180;

    go_rpy_quat_convert(&rpy, &pos.rot);
    pos.tran.x = world->tran.x;
    pos.tran.y = world->tran.y;
    pos.tran.z = world->tran.z;

    go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);
    go_matrix_init(Jinv, Jinv_stg, genser->link_num, 6);

    /* jest[] is a copy of joints[], which is the joint estimate */
    for (link = 0; link < genser->link_num; link++) {
        // jest, and the rest of joint related calcs are in radians
        jest[link] = joints[link] * (PM_PI / 180);
    }

    for (genser->iterations = 0;
         genser->iterations < max_iterations;
         genser->iterations++) {
        s->iterations = genser->iterations;
        s->out[P_LAST_ITER] = genser->iterations;
        /* update the Jacobians */
        for (link = 0; link < genser->link_num; link++) {
            go_link_joint_set(&genser->links[link], jest[link], &linkout[link]);
        }
        retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
        if (GO_RESULT_OK != retval) {
            rtapi_print("ERR kI - compute_jfwd (joints: %f %f %f %f %f %f), (iterations=%d)\n",
                 joints[0],joints[1],joints[2],joints[3],joints[4],joints[5], genser->iterations);
            return retval;
        }
        retval = compute_jinv(&Jfwd, &Jinv);
        if (GO_RESULT_OK != retval) {
            rtapi_print("ERR kI - compute_jinv (joints: %f %f %f %f %f %f), (iterations=%d)\n",
                 joints[0],joints[1],joints[2],joints[3],joints[4],joints[5], genser->iterations);
            return retval;
        }

        /* pest is the resulting pose estimate given joint estimate */
        genser_kin_fwd(genser, jest, &pest);
        /* pestinv is its inverse */
        go_pose_inv(&pest, &pestinv);
        /*
            Tdelta is the incremental pose from pest to pos, such that

            0        L         0
            . pest *  Tdelta =  pos, or
            L        L         L

            L         L          0
            .Tdelta =  pestinv *  pos
            L         0          L
        */
        go_pose_pose_mult(&pestinv, &pos, &Tdelta);

        /*
            We need Tdelta in 0 frame, not pest frame, so rotate it
            back. Since it's effectively a velocity, we just rotate it, and
            don't translate it.
        */

        /* first rotate the translation differential */
        go_quat_cart_mult(&pest.rot, &Tdelta.tran, &cart);
        dvw[0] = cart.x;
        dvw[1] = cart.y;
        dvw[2] = cart.z;

        /* to rotate the rotation differential, convert it to a
           velocity screw and rotate that */
        go_quat_rvec_convert(&Tdelta.rot, &rvec);
        cart.x = rvec.x;
        cart.y = rvec.y;
        cart.z = rvec.z;
        go_quat_cart_mult(&pest.rot, &cart, &cart);
        dvw[3] = cart.x;
        dvw[4] = cart.y;
        dvw[5] = cart.z;

        /* push the Cartesian velocity vector through the inverse Jacobian */
        go_matrix_vector_mult(&Jinv, dvw, dj);

        //pass through 678 as uvw
        if (p->max_joints > 6) joints[6] = world->u;
        if (p->max_joints > 7) joints[7] = world->v;
        if (p->max_joints > 8) joints[8] = world->w;

        /* check for small joint increments, if so we're done */
        for (link = 0, smalls = 0; link < genser->link_num; link++) {
            if (GO_QUANTITY_LENGTH == linkout[link].quantity) {
            if (GO_TRAN_SMALL(dj[link]))
                smalls++;
            } else {
                if (GO_ROT_SMALL(dj[link]))
                    smalls++;
            }
        }
        if (smalls == genser->link_num) {
            /* converged, copy jest[] out */
            for (link = 0; link < genser->link_num; link++) {
                // convert from radians back to angles
                joints[link] = jest[link] * 180 / PM_PI;
                rtapi_s32 unrotate = unrotate_of(p, link);
                if ((link) && unrotate)
                    joints[link] += unrotate * joints[link-1];
            }
            return GO_RESULT_OK;
        }
        /* else keep iterating */
        for (link = 0; link < genser->link_num; link++) {
            jest[link] += dj[link]; //still in radians
        }
    } /* for (iterations) */

    rtapi_print("ERRkineInverse(joints: %f %f %f %f %f %f), (iterations=%d)\n",
         joints[0],joints[1],joints[2],joints[3],joints[4],joints[5], genser->iterations);
    return GO_RESULT_ERROR;
}

const kins_ops GENSER_OPS = {
    .forward  = genser_forward,
    .inverse  = genser_inverse,
    .jacobian = genser_jacobian,
};

/*
  Extras, not callable using go_kin_ wrapper but if you know you have
  linked in these kinematics, go ahead and call these for your ad hoc
  purposes.
*/

int genser_kin_inv_iterations(genser_struct * genser)
{
    return genser->iterations;
}
