/********************************************************************
 * Description: genserkins_math.h
 *   Pure math functions for generic serial robot kinematics
 *   Extracted from genserfuncs.c for userspace kinematics support
 *
 *   Uses Denavit-Hartenberg (DH) parameters for kinematic chain.
 *   Implements Jacobian-based iterative inverse kinematics solver.
 *
 * Original Authors: Fred Proctor, Alex Joni
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef GENSERKINS_MATH_H
#define GENSERKINS_MATH_H

#include "gotypes.h"    /* go_result, go_integer, go_real, GO_RESULT_OK, etc. */
#include "gomath.h"     /* go_pose, go_matrix, go_link, go_dh, etc. */
#include "posemath.h"   /* PmRpy, pmRpyQuatConvert, etc. */
#include "emcpos.h"     /* EmcPose */

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif

/* Only gcc/g++ supports the #pragma */
#if __GNUC__ && !defined(__clang__)
/* The inline kinematics functions use large matrix storage on the stack.
 * genserkins_compute_jinv() inlined into callers creates ~2768 byte frames.
 * genserkins_inv() creates ~10560 byte frames (large due to 9D kinematics).
 * These are non-realtime computation functions, so large stack usage is acceptable.
 */
  #pragma GCC diagnostic warning "-Wframe-larger-than=11000"
#endif

/* Maximum number of joints for generic serial kinematics */
#define GENSERKINS_MAX_JOINTS 9

/* Default max iterations for inverse kinematics */
#define GENSERKINS_DEFAULT_MAX_ITERATIONS 100

/* Default DH parameters - PUMA-like configuration from Craig */
#define GENSERKINS_DEFAULT_A1     0.0
#define GENSERKINS_DEFAULT_ALPHA1 0.0
#define GENSERKINS_DEFAULT_D1     0.0

#define GENSERKINS_DEFAULT_A2     0.0
#define GENSERKINS_DEFAULT_ALPHA2 (-GO_PI_2)
#define GENSERKINS_DEFAULT_D2     0.0

#define GENSERKINS_DEFAULT_A3     300.0
#define GENSERKINS_DEFAULT_ALPHA3 0.0
#define GENSERKINS_DEFAULT_D3     70.0

#define GENSERKINS_DEFAULT_A4     50.0
#define GENSERKINS_DEFAULT_ALPHA4 (-GO_PI_2)
#define GENSERKINS_DEFAULT_D4     400.0

#define GENSERKINS_DEFAULT_A5     0.0
#define GENSERKINS_DEFAULT_ALPHA5 GO_PI_2
#define GENSERKINS_DEFAULT_D5     0.0

#define GENSERKINS_DEFAULT_A6     0.0
#define GENSERKINS_DEFAULT_ALPHA6 (-GO_PI_2)
#define GENSERKINS_DEFAULT_D6     0.0

/* Parameters structure for generic serial kinematics */
typedef struct {
    int link_num;                           /* Number of active links (up to GENSERKINS_MAX_JOINTS) */

    /* DH parameters for each joint */
    double a[GENSERKINS_MAX_JOINTS];        /* Link length */
    double alpha[GENSERKINS_MAX_JOINTS];    /* Link twist (radians) */
    double d[GENSERKINS_MAX_JOINTS];        /* Link offset */
    int unrotate[GENSERKINS_MAX_JOINTS];    /* Unrotate flags for wrist */

    /* Iteration control */
    unsigned int max_iterations;

    /* Status/output */
    unsigned int last_iterations;
} genserkins_params_t;

/* Internal state structure for kinematics computations */
typedef struct {
    go_link links[GENSERKINS_MAX_JOINTS];   /* Link description */
    int link_num;                            /* Number of active links */
    unsigned int iterations;                 /* Iterations used in last inverse solve */
} genserkins_state_t;

/* ========================================================================
 * Default parameter initialization
 * ======================================================================== */

static inline void genserkins_init_params(genserkins_params_t *params)
{
    int i;

    params->link_num = 6;  /* Default to 6-DOF robot */
    params->max_iterations = GENSERKINS_DEFAULT_MAX_ITERATIONS;
    params->last_iterations = 0;

    /* Initialize all joints to zero first */
    for (i = 0; i < GENSERKINS_MAX_JOINTS; i++) {
        params->a[i] = 0.0;
        params->alpha[i] = 0.0;
        params->d[i] = 0.0;
        params->unrotate[i] = 0;
    }

    /* Set PUMA-like defaults for first 6 joints */
    params->a[0] = GENSERKINS_DEFAULT_A1;
    params->alpha[0] = GENSERKINS_DEFAULT_ALPHA1;
    params->d[0] = GENSERKINS_DEFAULT_D1;

    params->a[1] = GENSERKINS_DEFAULT_A2;
    params->alpha[1] = GENSERKINS_DEFAULT_ALPHA2;
    params->d[1] = GENSERKINS_DEFAULT_D2;

    params->a[2] = GENSERKINS_DEFAULT_A3;
    params->alpha[2] = GENSERKINS_DEFAULT_ALPHA3;
    params->d[2] = GENSERKINS_DEFAULT_D3;

    params->a[3] = GENSERKINS_DEFAULT_A4;
    params->alpha[3] = GENSERKINS_DEFAULT_ALPHA4;
    params->d[3] = GENSERKINS_DEFAULT_D4;

    params->a[4] = GENSERKINS_DEFAULT_A5;
    params->alpha[4] = GENSERKINS_DEFAULT_ALPHA5;
    params->d[4] = GENSERKINS_DEFAULT_D5;

    params->a[5] = GENSERKINS_DEFAULT_A6;
    params->alpha[5] = GENSERKINS_DEFAULT_ALPHA6;
    params->d[5] = GENSERKINS_DEFAULT_D6;
}

/* ========================================================================
 * Initialize state from parameters
 * ======================================================================== */

static inline int genserkins_init_state(genserkins_state_t *state,
                                         const genserkins_params_t *params)
{
    int i;

    state->link_num = params->link_num;
    state->iterations = 0;

    /* Initialize all links as revolute joints with DH parameters */
    for (i = 0; i < GENSERKINS_MAX_JOINTS; i++) {
        state->links[i].u.dh.a = params->a[i];
        state->links[i].u.dh.alpha = params->alpha[i];
        state->links[i].u.dh.d = params->d[i];
        state->links[i].u.dh.theta = 0.0;
        state->links[i].type = GO_LINK_DH;
        state->links[i].quantity = GO_QUANTITY_ANGLE;  /* Revolute joints */
    }

    return GO_RESULT_OK;
}

/* ========================================================================
 * Compute forward Jacobian
 *
 * Calculates the Jacobian matrix which is a linear approximation of the
 * kinematics function. Used for feeding velocities and for inverse kinematics.
 * ======================================================================== */

static inline int genserkins_compute_jfwd(go_link *link_params,
                                           int link_number,
                                           go_matrix *Jfwd,
                                           go_pose *T_L_0)
{
    GO_MATRIX_DECLARE(Jv, Jvstg, 3, GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jw, Jwstg, 3, GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_i_ip1, R_i_ip1stg, 3, 3);
    GO_MATRIX_DECLARE(scratch, scratchstg, 3, GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_inv, R_invstg, 3, 3);
    go_pose pose;
    go_quat quat;
    go_vector P_ip1_i[3];
    int row, col;

    /* Initialize matrices to possibly smaller size */
    go_matrix_init(Jv, Jvstg, 3, link_number);
    go_matrix_init(Jw, Jwstg, 3, link_number);
    go_matrix_init(R_i_ip1, R_i_ip1stg, 3, 3);
    go_matrix_init(scratch, scratchstg, 3, link_number);
    go_matrix_init(R_inv, R_invstg, 3, 3);

    Jv.el[0][0] = 0;
    Jv.el[1][0] = 0;
    Jv.el[2][0] = (GO_QUANTITY_LENGTH == link_params[0].quantity ? 1 : 0);
    Jw.el[0][0] = 0;
    Jw.el[1][0] = 0;
    Jw.el[2][0] = (GO_QUANTITY_ANGLE == link_params[0].quantity ? 1 : 0);

    /* Initialize inverse rotational transform */
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
        Jv.el[0][col] = 0;
        Jv.el[1][col] = 0;
        Jv.el[2][col] = (GO_QUANTITY_LENGTH == link_params[col].quantity ? 1 : 0);

        /* Jw */
        go_matrix_matrix_mult(&R_i_ip1, &Jw, &Jw);
        Jw.el[0][col] = 0;
        Jw.el[1][col] = 0;
        Jw.el[2][col] = (GO_QUANTITY_ANGLE == link_params[col].quantity ? 1 : 0);

        if (GO_LINK_DH == link_params[col].type) {
            go_dh_pose_convert(&link_params[col].u.dh, &pose);
        } else if (GO_LINK_PP == link_params[col].type) {
            pose = link_params[col].u.pp.pose;
        } else {
            return GO_RESULT_IMPL_ERROR;
        }
        go_pose_pose_mult(T_L_0, &pose, T_L_0);
    }

    /* Rotate back into {0} frame */
    go_quat_matrix_convert(&T_L_0->rot, &R_inv);
    go_matrix_matrix_mult(&R_inv, &Jv, &Jv);
    go_matrix_matrix_mult(&R_inv, &Jw, &Jw);

    /* Put Jv atop Jw in J */
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

/* ========================================================================
 * Compute inverse Jacobian (or pseudoinverse)
 * ======================================================================== */

static inline int genserkins_compute_jinv(go_matrix *Jfwd, go_matrix *Jinv)
{
    int retval;
    GO_MATRIX_DECLARE(JT, JTstg, GENSERKINS_MAX_JOINTS, 6);

    /* Compute inverse, or pseudo-inverse */
    if (Jfwd->rows == Jfwd->cols) {
        retval = go_matrix_inv(Jfwd, Jinv);
        if (GO_RESULT_OK != retval)
            return retval;
    } else if (Jfwd->rows < Jfwd->cols) {
        /* Underdetermined, optimize on smallest sum of square of speeds */
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
        /* Overdetermined, do least-squares best fit */
        /* (JTJ)invJT */
        GO_MATRIX_DECLARE(JTJ, JTJstg, GENSERKINS_MAX_JOINTS, GENSERKINS_MAX_JOINTS);

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

/* ========================================================================
 * Forward kinematics: joints -> pose (internal, works with go_pose)
 * ======================================================================== */

static inline int genserkins_fwd_internal(genserkins_state_t *state,
                                           const go_real *joints,
                                           go_pose *pos)
{
    go_link linkout[GENSERKINS_MAX_JOINTS];
    int link;
    int retval;

    for (link = 0; link < state->link_num; link++) {
        retval = go_link_joint_set(&state->links[link], joints[link], &linkout[link]);
        if (GO_RESULT_OK != retval)
            return retval;
    }

    retval = go_link_pose_build(linkout, state->link_num, pos);
    if (GO_RESULT_OK != retval)
        return retval;

    return GO_RESULT_OK;
}

/* ========================================================================
 * Forward kinematics: joints -> pose (works directly with go_link array)
 * Used by HAL kinematics that manage their own link structures
 * ======================================================================== */

static inline int genserkins_fwd_internal_links(go_link *links,
                                                 int link_num,
                                                 const go_real *joints,
                                                 go_pose *pos)
{
    go_link linkout[GENSERKINS_MAX_JOINTS];
    int link;
    int retval;

    for (link = 0; link < link_num; link++) {
        retval = go_link_joint_set(&links[link], joints[link], &linkout[link]);
        if (GO_RESULT_OK != retval)
            return retval;
    }

    retval = go_link_pose_build(linkout, link_num, pos);
    if (GO_RESULT_OK != retval)
        return retval;

    return GO_RESULT_OK;
}

/* ========================================================================
 * Forward kinematics: joints (degrees) -> EmcPose
 *
 * Converts joint positions in degrees to world coordinates.
 * Joints 6, 7, 8 are passed through as U, V, W if present.
 * ======================================================================== */

static inline int genserkins_fwd(genserkins_params_t *params,
                                  const double *joints,
                                  EmcPose *world)
{
    genserkins_state_t state;
    go_pose pos;
    go_rpy rpy;
    go_real jcopy[GENSERKINS_MAX_JOINTS];
    int i;
    int retval;

    /* Initialize state from parameters */
    genserkins_init_state(&state, params);

    /* Convert joints from degrees to radians */
    for (i = 0; i < state.link_num && i < 6; i++) {
        jcopy[i] = joints[i] * PM_PI / 180.0;
        /* Handle unrotate for wrist joints */
        if (i > 0 && params->unrotate[i]) {
            jcopy[i] -= params->unrotate[i] * jcopy[i-1];
        }
    }

    /* Compute forward kinematics */
    retval = genserkins_fwd_internal(&state, jcopy, &pos);
    if (retval != GO_RESULT_OK)
        return retval;

    /* Convert quaternion to RPY angles */
    retval = go_quat_rpy_convert(&pos.rot, &rpy);
    if (retval != GO_RESULT_OK)
        return retval;

    /* Fill EmcPose */
    world->tran.x = pos.tran.x;
    world->tran.y = pos.tran.y;
    world->tran.z = pos.tran.z;
    world->a = rpy.r * 180.0 / PM_PI;
    world->b = rpy.p * 180.0 / PM_PI;
    world->c = rpy.y * 180.0 / PM_PI;

    /* Pass through extra joints as UVW */
    world->u = (params->link_num > 6) ? joints[6] : 0.0;
    world->v = (params->link_num > 7) ? joints[7] : 0.0;
    world->w = (params->link_num > 8) ? joints[8] : 0.0;

    return 0;
}

/* ========================================================================
 * Inverse kinematics: EmcPose -> joints (degrees)
 *
 * Uses iterative Jacobian-based solver (Newton-Raphson).
 * Joints array must contain initial estimate on entry.
 * ======================================================================== */

static inline int genserkins_inv(genserkins_params_t *params,
                                  const EmcPose *world,
                                  double *joints)
{
    genserkins_state_t state;
    GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSERKINS_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jinv, Jinv_stg, GENSERKINS_MAX_JOINTS, 6);
    go_pose T_L_0;
    go_real dvw[6];
    go_real jest[GENSERKINS_MAX_JOINTS];
    go_real dj[GENSERKINS_MAX_JOINTS];
    go_pose pest, pestinv, Tdelta;
    go_pose target_pos;
    go_rpy rpy;
    go_rvec rvec;
    go_cart cart;
    go_link linkout[GENSERKINS_MAX_JOINTS];
    int link;
    int smalls;
    int retval;
    unsigned int iter;

    /* Initialize state from parameters */
    genserkins_init_state(&state, params);

    /* Convert target pose from EmcPose to go_pose */
    rpy.y = world->c * PM_PI / 180.0;
    rpy.p = world->b * PM_PI / 180.0;
    rpy.r = world->a * PM_PI / 180.0;
    go_rpy_quat_convert(&rpy, &target_pos.rot);
    target_pos.tran.x = world->tran.x;
    target_pos.tran.y = world->tran.y;
    target_pos.tran.z = world->tran.z;

    go_matrix_init(Jfwd, Jfwd_stg, 6, state.link_num);
    go_matrix_init(Jinv, Jinv_stg, state.link_num, 6);

    /* Initialize joint estimate from current joints (converted to radians) */
    for (link = 0; link < state.link_num && link < 6; link++) {
        jest[link] = joints[link] * PM_PI / 180.0;
    }

    /* Newton-Raphson iteration */
    for (iter = 0; iter < params->max_iterations; iter++) {
        params->last_iterations = iter;

        /* Update the Jacobians */
        for (link = 0; link < state.link_num; link++) {
            go_link_joint_set(&state.links[link], jest[link], &linkout[link]);
        }

        retval = genserkins_compute_jfwd(linkout, state.link_num, &Jfwd, &T_L_0);
        if (GO_RESULT_OK != retval) {
            return -1;
        }

        retval = genserkins_compute_jinv(&Jfwd, &Jinv);
        if (GO_RESULT_OK != retval) {
            return -1;
        }

        /* pest is the resulting pose estimate given joint estimate */
        genserkins_fwd_internal(&state, jest, &pest);

        /* pestinv is its inverse */
        go_pose_inv(&pest, &pestinv);

        /*
         * Tdelta is the incremental pose from pest to target_pos, such that
         * pest * Tdelta = target_pos, or
         * Tdelta = pestinv * target_pos
         */
        go_pose_pose_mult(&pestinv, &target_pos, &Tdelta);

        /*
         * We need Tdelta in base frame, not pest frame, so rotate it back.
         * Since it's effectively a velocity, we just rotate it, don't translate.
         */

        /* First rotate the translation differential */
        go_quat_cart_mult(&pest.rot, &Tdelta.tran, &cart);
        dvw[0] = cart.x;
        dvw[1] = cart.y;
        dvw[2] = cart.z;

        /* To rotate the rotation differential, convert to rotation vector and rotate */
        go_quat_rvec_convert(&Tdelta.rot, &rvec);
        cart.x = rvec.x;
        cart.y = rvec.y;
        cart.z = rvec.z;
        go_quat_cart_mult(&pest.rot, &cart, &cart);
        dvw[3] = cart.x;
        dvw[4] = cart.y;
        dvw[5] = cart.z;

        /* Push the Cartesian velocity vector through the inverse Jacobian */
        go_matrix_vector_mult(&Jinv, dvw, dj);

        /* Check for small joint increments - if so we're done */
        smalls = 0;
        for (link = 0; link < state.link_num; link++) {
            if (GO_QUANTITY_LENGTH == linkout[link].quantity) {
                if (GO_TRAN_SMALL(dj[link]))
                    smalls++;
            } else {
                if (GO_ROT_SMALL(dj[link]))
                    smalls++;
            }
        }

        if (smalls == state.link_num) {
            /* Converged, copy jest[] out (convert from radians to degrees) */
            for (link = 0; link < state.link_num && link < 6; link++) {
                joints[link] = jest[link] * 180.0 / PM_PI;
                if (link > 0 && params->unrotate[link]) {
                    joints[link] += params->unrotate[link] * joints[link-1];
                }
            }

            /* Pass through extra joints as UVW */
            if (params->link_num > 6) joints[6] = world->u;
            if (params->link_num > 7) joints[7] = world->v;
            if (params->link_num > 8) joints[8] = world->w;

            return 0;
        }

        /* Else keep iterating */
        for (link = 0; link < state.link_num; link++) {
            jest[link] += dj[link];
        }
    }

    /* Failed to converge */
    return -1;
}

#endif /* GENSERKINS_MATH_H */
