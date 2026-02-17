/********************************************************************
* genserkins.c employing switchkins.[ch]
* License: GPL Version 2
*
* NOTEs:
*  1) specify all kparms items
*  2) specify 3 KS,KF,KI functions (setup,forward,inverse)
*/

/********************************************************************
TEST: switchable kinematics: identity or genserkins

1) genser kinematics provided by genserfuncs.c (shared with genserkins.c
2) uses same pin names as genserkins
3) tesing mm configs: increase GO_REAL_EPSILON from 1e-7 to 1e-6

NOTE:
a) requires *exactly* 6 joints
b) identity assignments can use any of xyzabcuvw
   but should agree with [TRAJ]COORDINATES
   and may be confusing

www refs:

frame-larger-than:
https://www.mail-archive.com/emc-developers@lists.sourceforge.net/msg03790.html

angles:
https://www.mail-archive.com/emc-developers@lists.sourceforge.net/msg15285.html
*/

//----------------------------------------------------------------------
// Only gcc/g++ supports the #pragma
#if __GNUC__ && !defined(__clang__)
// genserKinematicsInverse() is 5104 with buster amd64 gcc 8.3.0-6
//#pragma GCC diagnostic error   "-Wframe-larger-than=6000"
  #pragma GCC diagnostic warning "-Wframe-larger-than=11000"
#endif

#include "rtapi.h"
#include <rtapi_string.h>
#include "genserkins.h"
#include "motion.h"
#include "switchkins.h"

//-7 is system defined -3 ok, -4 ok, -5 ok,-6 ok (mm system)
#undef  GO_REAL_EPSILON
#define GO_REAL_EPSILON (1e-6)

//*********************************************************************


int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    kp->kinsname    = "genserkins"; // !!! must agree with filename
    kp->halprefix   = "genserkins"; // hal pin names
    kp->required_coordinates = "xyzabcuvw"; // u,v,w are joints 6,7,8
    kp->max_joints  = strlen(kp->required_coordinates);
    kp->allow_duplicates  = 0;

    *kset0 = genserKinematicsSetup;
    *kfwd0 = genserKinematicsForward;
    *kinv0 = genserKinematicsInverse;

    *kset1 = identityKinematicsSetup;
    *kfwd1 = identityKinematicsForward;
    *kinv1 = identityKinematicsInverse;

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
}

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 *
 * Math functions derived from genserkins_math.h (Jacobian-based
 * iterative inverse solver using Denavit-Hartenberg parameters).
 * ======================================================================== */
#include "kinematics_params.h"
#include "emcpos.h"

#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif

#define NONRT_GENSERKINS_MAX_JOINTS 9

typedef struct {
    int link_num;
    double a[NONRT_GENSERKINS_MAX_JOINTS];
    double alpha[NONRT_GENSERKINS_MAX_JOINTS];
    double d[NONRT_GENSERKINS_MAX_JOINTS];
    int unrotate[NONRT_GENSERKINS_MAX_JOINTS];
    unsigned int max_iterations;
    unsigned int last_iterations;
} nonrt_genserkins_params_t;

typedef struct {
    go_link links[NONRT_GENSERKINS_MAX_JOINTS];
    int link_num;
    unsigned int iterations;
} nonrt_genserkins_state_t;

static int nonrt_genserkins_init_state(nonrt_genserkins_state_t *state,
                                        const nonrt_genserkins_params_t *params)
{
    int i;
    state->link_num = params->link_num;
    state->iterations = 0;
    for (i = 0; i < NONRT_GENSERKINS_MAX_JOINTS; i++) {
        state->links[i].u.dh.a = params->a[i];
        state->links[i].u.dh.alpha = params->alpha[i];
        state->links[i].u.dh.d = params->d[i];
        state->links[i].u.dh.theta = 0.0;
        state->links[i].type = GO_LINK_DH;
        state->links[i].quantity = GO_QUANTITY_ANGLE;
    }
    return GO_RESULT_OK;
}

static int nonrt_genserkins_compute_jfwd(go_link *link_params,
                                          int link_number,
                                          go_matrix *Jfwd,
                                          go_pose *T_L_0)
{
    GO_MATRIX_DECLARE(Jv, Jvstg, 3, NONRT_GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jw, Jwstg, 3, NONRT_GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_i_ip1, R_i_ip1stg, 3, 3);
    GO_MATRIX_DECLARE(scratch, scratchstg, 3, NONRT_GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_inv, R_invstg, 3, 3);
    go_pose pose;
    go_quat quat;
    go_vector P_ip1_i[3];
    int row, col;

    go_matrix_init(Jv, Jvstg, 3, link_number);
    go_matrix_init(Jw, Jwstg, 3, link_number);
    go_matrix_init(R_i_ip1, R_i_ip1stg, 3, 3);
    go_matrix_init(scratch, scratchstg, 3, link_number);
    go_matrix_init(R_inv, R_invstg, 3, 3);

    Jv.el[0][0] = 0; Jv.el[1][0] = 0;
    Jv.el[2][0] = (GO_QUANTITY_LENGTH == link_params[0].quantity ? 1 : 0);
    Jw.el[0][0] = 0; Jw.el[1][0] = 0;
    Jw.el[2][0] = (GO_QUANTITY_ANGLE == link_params[0].quantity ? 1 : 0);

    if (GO_LINK_DH == link_params[0].type)
        go_dh_pose_convert(&link_params[0].u.dh, &pose);
    else if (GO_LINK_PP == link_params[0].type)
        pose = link_params[0].u.pp.pose;
    else
        return GO_RESULT_IMPL_ERROR;

    *T_L_0 = pose;

    for (col = 1; col < link_number; col++) {
        if (GO_LINK_DH == link_params[col].type)
            go_dh_pose_convert(&link_params[col].u.dh, &pose);
        else if (GO_LINK_PP == link_params[col].type)
            pose = link_params[col].u.pp.pose;
        else
            return GO_RESULT_IMPL_ERROR;

        go_cart_vector_convert(&pose.tran, P_ip1_i);
        go_quat_inv(&pose.rot, &quat);
        go_quat_matrix_convert(&quat, &R_i_ip1);

        go_matrix_vector_cross(&Jw, P_ip1_i, &scratch);
        go_matrix_matrix_add(&Jv, &scratch, &scratch);
        go_matrix_matrix_mult(&R_i_ip1, &scratch, &Jv);
        Jv.el[0][col] = 0; Jv.el[1][col] = 0;
        Jv.el[2][col] = (GO_QUANTITY_LENGTH == link_params[col].quantity ? 1 : 0);

        go_matrix_matrix_mult(&R_i_ip1, &Jw, &Jw);
        Jw.el[0][col] = 0; Jw.el[1][col] = 0;
        Jw.el[2][col] = (GO_QUANTITY_ANGLE == link_params[col].quantity ? 1 : 0);

        if (GO_LINK_DH == link_params[col].type)
            go_dh_pose_convert(&link_params[col].u.dh, &pose);
        else if (GO_LINK_PP == link_params[col].type)
            pose = link_params[col].u.pp.pose;
        else
            return GO_RESULT_IMPL_ERROR;
        go_pose_pose_mult(T_L_0, &pose, T_L_0);
    }

    go_quat_matrix_convert(&T_L_0->rot, &R_inv);
    go_matrix_matrix_mult(&R_inv, &Jv, &Jv);
    go_matrix_matrix_mult(&R_inv, &Jw, &Jw);

    for (row = 0; row < 6; row++)
        for (col = 0; col < link_number; col++)
            Jfwd->el[row][col] = (row < 3) ? Jv.el[row][col] : Jw.el[row-3][col];

    return GO_RESULT_OK;
}

static int nonrt_genserkins_compute_jinv(go_matrix *Jfwd, go_matrix *Jinv)
{
    int retval;
    GO_MATRIX_DECLARE(JT, JTstg, NONRT_GENSERKINS_MAX_JOINTS, 6);

    if (Jfwd->rows == Jfwd->cols) {
        retval = go_matrix_inv(Jfwd, Jinv);
        if (GO_RESULT_OK != retval) return retval;
    } else if (Jfwd->rows < Jfwd->cols) {
        GO_MATRIX_DECLARE(JJT, JJTstg, 6, 6);
        go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
        go_matrix_init(JJT, JJTstg, Jfwd->rows, Jfwd->rows);
        go_matrix_transpose(Jfwd, &JT);
        go_matrix_matrix_mult(Jfwd, &JT, &JJT);
        retval = go_matrix_inv(&JJT, &JJT);
        if (GO_RESULT_OK != retval) return retval;
        go_matrix_matrix_mult(&JT, &JJT, Jinv);
    } else {
        GO_MATRIX_DECLARE(JTJ, JTJstg, NONRT_GENSERKINS_MAX_JOINTS, NONRT_GENSERKINS_MAX_JOINTS);
        go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
        go_matrix_init(JTJ, JTJstg, Jfwd->cols, Jfwd->cols);
        go_matrix_transpose(Jfwd, &JT);
        go_matrix_matrix_mult(&JT, Jfwd, &JTJ);
        retval = go_matrix_inv(&JTJ, &JTJ);
        if (GO_RESULT_OK != retval) return retval;
        go_matrix_matrix_mult(&JTJ, &JT, Jinv);
    }
    return GO_RESULT_OK;
}

static int nonrt_genserkins_fwd_internal(nonrt_genserkins_state_t *state,
                                          const go_real *joints,
                                          go_pose *pos)
{
    go_link linkout[NONRT_GENSERKINS_MAX_JOINTS];
    int link, retval;
    for (link = 0; link < state->link_num; link++) {
        retval = go_link_joint_set(&state->links[link], joints[link], &linkout[link]);
        if (GO_RESULT_OK != retval) return retval;
    }
    retval = go_link_pose_build(linkout, state->link_num, pos);
    if (GO_RESULT_OK != retval) return retval;
    return GO_RESULT_OK;
}

static int nonrt_genserkins_fwd(nonrt_genserkins_params_t *params,
                                 const double *joints,
                                 EmcPose *world)
{
    nonrt_genserkins_state_t state;
    go_pose pos;
    go_rpy rpy;
    go_real jcopy[NONRT_GENSERKINS_MAX_JOINTS];
    int i, retval;

    nonrt_genserkins_init_state(&state, params);

    for (i = 0; i < state.link_num && i < 6; i++) {
        jcopy[i] = joints[i] * PM_PI / 180.0;
        if (i > 0 && params->unrotate[i])
            jcopy[i] -= params->unrotate[i] * jcopy[i-1];
    }

    retval = nonrt_genserkins_fwd_internal(&state, jcopy, &pos);
    if (retval != GO_RESULT_OK) return retval;

    retval = go_quat_rpy_convert(&pos.rot, &rpy);
    if (retval != GO_RESULT_OK) return retval;

    world->tran.x = pos.tran.x; world->tran.y = pos.tran.y; world->tran.z = pos.tran.z;
    world->a = rpy.r * 180.0 / PM_PI;
    world->b = rpy.p * 180.0 / PM_PI;
    world->c = rpy.y * 180.0 / PM_PI;
    world->u = (params->link_num > 6) ? joints[6] : 0.0;
    world->v = (params->link_num > 7) ? joints[7] : 0.0;
    world->w = (params->link_num > 8) ? joints[8] : 0.0;
    return 0;
}

static int nonrt_genserkins_inv(nonrt_genserkins_params_t *params,
                                 const EmcPose *world,
                                 double *joints)
{
    nonrt_genserkins_state_t state;
    GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, NONRT_GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jinv, Jinv_stg, NONRT_GENSERKINS_MAX_JOINTS, 6);
    go_pose T_L_0;
    go_real dvw[6], jest[NONRT_GENSERKINS_MAX_JOINTS], dj[NONRT_GENSERKINS_MAX_JOINTS];
    go_pose pest, pestinv, Tdelta, target_pos;
    go_rpy rpy;
    go_rvec rvec;
    go_cart cart;
    go_link linkout[NONRT_GENSERKINS_MAX_JOINTS];
    int link, smalls, retval;
    unsigned int iter;

    nonrt_genserkins_init_state(&state, params);

    rpy.y = world->c * PM_PI / 180.0;
    rpy.p = world->b * PM_PI / 180.0;
    rpy.r = world->a * PM_PI / 180.0;
    go_rpy_quat_convert(&rpy, &target_pos.rot);
    target_pos.tran.x = world->tran.x;
    target_pos.tran.y = world->tran.y;
    target_pos.tran.z = world->tran.z;

    go_matrix_init(Jfwd, Jfwd_stg, 6, state.link_num);
    go_matrix_init(Jinv, Jinv_stg, state.link_num, 6);

    for (link = 0; link < state.link_num && link < 6; link++)
        jest[link] = joints[link] * PM_PI / 180.0;

    for (iter = 0; iter < params->max_iterations; iter++) {
        params->last_iterations = iter;

        for (link = 0; link < state.link_num; link++)
            go_link_joint_set(&state.links[link], jest[link], &linkout[link]);

        retval = nonrt_genserkins_compute_jfwd(linkout, state.link_num, &Jfwd, &T_L_0);
        if (GO_RESULT_OK != retval) return -1;

        retval = nonrt_genserkins_compute_jinv(&Jfwd, &Jinv);
        if (GO_RESULT_OK != retval) return -1;

        nonrt_genserkins_fwd_internal(&state, jest, &pest);
        go_pose_inv(&pest, &pestinv);
        go_pose_pose_mult(&pestinv, &target_pos, &Tdelta);

        go_quat_cart_mult(&pest.rot, &Tdelta.tran, &cart);
        dvw[0] = cart.x; dvw[1] = cart.y; dvw[2] = cart.z;

        go_quat_rvec_convert(&Tdelta.rot, &rvec);
        cart.x = rvec.x; cart.y = rvec.y; cart.z = rvec.z;
        go_quat_cart_mult(&pest.rot, &cart, &cart);
        dvw[3] = cart.x; dvw[4] = cart.y; dvw[5] = cart.z;

        go_matrix_vector_mult(&Jinv, dvw, dj);

        smalls = 0;
        for (link = 0; link < state.link_num; link++) {
            if (GO_QUANTITY_LENGTH == linkout[link].quantity) {
                if (GO_TRAN_SMALL(dj[link])) smalls++;
            } else {
                if (GO_ROT_SMALL(dj[link])) smalls++;
            }
        }

        if (smalls == state.link_num) {
            for (link = 0; link < state.link_num && link < 6; link++) {
                joints[link] = jest[link] * 180.0 / PM_PI;
                if (link > 0 && params->unrotate[link])
                    joints[link] += params->unrotate[link] * joints[link-1];
            }
            if (params->link_num > 6) joints[6] = world->u;
            if (params->link_num > 7) joints[7] = world->v;
            if (params->link_num > 8) joints[8] = world->w;
            return 0;
        }

        for (link = 0; link < state.link_num; link++)
            jest[link] += dj[link];
    }
    return -1;
}

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    nonrt_genserkins_params_t p;
    int i;

    p.link_num = kp->params.genser.link_num;
    p.max_iterations = kp->params.genser.max_iterations;
    p.last_iterations = 0;
    for (i = 0; i < NONRT_GENSERKINS_MAX_JOINTS; i++) {
        p.a[i] = kp->params.genser.a[i];
        p.alpha[i] = kp->params.genser.alpha[i];
        p.d[i] = kp->params.genser.d[i];
        p.unrotate[i] = kp->params.genser.unrotate[i];
    }

    return nonrt_genserkins_fwd(&p, joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    nonrt_genserkins_params_t p;
    int i;

    p.link_num = kp->params.genser.link_num;
    p.max_iterations = kp->params.genser.max_iterations;
    p.last_iterations = 0;
    for (i = 0; i < NONRT_GENSERKINS_MAX_JOINTS; i++) {
        p.a[i] = kp->params.genser.a[i];
        p.alpha[i] = kp->params.genser.alpha[i];
        p.d[i] = kp->params.genser.d[i];
        p.unrotate[i] = kp->params.genser.unrotate[i];
    }

    return nonrt_genserkins_inv(&p, pos, joints);
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    kinematics_params_t *kp = (kinematics_params_t *)params;
    int i;
    char pin_name[64];
    (void)read_bit;

    for (i = 0; i < KINS_GENSER_MAX_JOINTS; i++) {
        rtapi_snprintf(pin_name, sizeof(pin_name), "genserkins.A-%d", i);
        read_float(pin_name, &kp->params.genser.a[i]);
        rtapi_snprintf(pin_name, sizeof(pin_name), "genserkins.ALPHA-%d", i);
        read_float(pin_name, &kp->params.genser.alpha[i]);
        rtapi_snprintf(pin_name, sizeof(pin_name), "genserkins.D-%d", i);
        read_float(pin_name, &kp->params.genser.d[i]);
    }

    if (read_s32) {
        int val;
        for (i = 0; i < KINS_GENSER_MAX_JOINTS; i++) {
            rtapi_snprintf(pin_name, sizeof(pin_name), "genserkins.UNROTATE-%d", i);
            if (read_s32(pin_name, &val) == 0)
                kp->params.genser.unrotate[i] = val;
        }
    }

    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
