/********************************************************************
* Description: pentakins_math.h
*   Pure math functions for pentakins (5-strut parallel kinematics)
*   Extracted from pentakins.c for userspace kinematics support
*
* Author: Andrew Kyrychenko (original pentakins.c)
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2016 All rights reserved.
********************************************************************/

#ifndef PENTAKINS_MATH_H
#define PENTAKINS_MATH_H

#include "posemath.h"
#include <math.h>

#define PENTAKINS_NUM_STRUTS 5
#define PENTAKINS_NUM_JOINTS 5  /* X, Y, Z, A, B (no C rotation for pentapod) */

/* Pentakins configuration parameters */
typedef struct {
    /* Base coordinates (world frame) */
    PmCartesian base[PENTAKINS_NUM_STRUTS];

    /* Effector coordinates (platform frame) */
    double effector_r[PENTAKINS_NUM_STRUTS];  /* radius from center */
    double effector_z[PENTAKINS_NUM_STRUTS];  /* z offset */

    /* Iteration control parameters */
    double conv_criterion;      /* convergence criterion */
    unsigned int iter_limit;    /* iteration limit */
    double max_error;           /* maximum error threshold */
    double tool_offset;         /* tool length offset */

    /* Output/status */
    unsigned int last_iterations;
    unsigned int max_iterations;
} pentakins_params_t;

/* Default parameter values */
static inline void pentakins_init_params(pentakins_params_t *params) {
    /* Default base positions */
    params->base[0].x = -418.03; params->base[0].y =  324.56; params->base[0].z = 895.56;
    params->base[1].x =  417.96; params->base[1].y =  324.56; params->base[1].z = 895.56;
    params->base[2].x = -418.03; params->base[2].y = -325.44; params->base[2].z = 895.56;
    params->base[3].x =  417.96; params->base[3].y = -325.44; params->base[3].z = 895.56;
    params->base[4].x =   -0.06; params->base[4].y = -492.96; params->base[4].z = 895.56;

    /* Default effector radii */
    params->effector_r[0] = 80.32;
    params->effector_r[1] = 80.32;
    params->effector_r[2] = 80.32;
    params->effector_r[3] = 80.32;
    params->effector_r[4] = 80.32;

    /* Default effector z positions */
    params->effector_z[0] = -185.50;
    params->effector_z[1] = -159.50;
    params->effector_z[2] =  -67.50;
    params->effector_z[3] =  -41.50;
    params->effector_z[4] =  -14.00;

    /* Iteration parameters */
    params->conv_criterion = 1e-9;
    params->iter_limit = 120;
    params->max_error = 100.0;
    params->tool_offset = 0.0;

    /* Status */
    params->last_iterations = 0;
    params->max_iterations = 0;
}

/******************************* MatInvert5() ***************************/
/* Matrix inversion using Gauss-Jordan elimination for 5x5 matrix */
static inline int pentakins_mat_invert5(double J[][PENTAKINS_NUM_STRUTS],
                                        double InvJ[][PENTAKINS_NUM_STRUTS])
{
    double JAug[PENTAKINS_NUM_STRUTS][10], m, temp;
    int j, k, n;

    /* Augment the Identity matrix to the Jacobian matrix */
    for (j = 0; j <= 4; ++j) {
        for (k = 0; k <= 4; ++k) {
            JAug[j][k] = J[j][k];
        }
        for (k = 5; k <= 9; ++k) {
            if (k - 5 == j) {
                JAug[j][k] = 1;
            } else {
                JAug[j][k] = 0;
            }
        }
    }

    /* Perform Gauss elimination */
    for (k = 0; k <= 3; ++k) {
        if ((JAug[k][k] < 0.01) && (JAug[k][k] > -0.01)) {
            for (j = k + 1; j <= 4; ++j) {
                if ((JAug[j][k] > 0.01) || (JAug[j][k] < -0.01)) {
                    for (n = 0; n <= 9; ++n) {
                        temp = JAug[k][n];
                        JAug[k][n] = JAug[j][n];
                        JAug[j][n] = temp;
                    }
                    break;
                }
            }
        }
        for (j = k + 1; j <= 4; ++j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 9; ++n) {
                JAug[j][n] = JAug[j][n] + m * JAug[k][n];
                if ((JAug[j][n] < 0.000001) && (JAug[j][n] > -0.000001)) {
                    JAug[j][n] = 0;
                }
            }
        }
    }

    /* Normalization of Diagonal Terms */
    for (j = 0; j <= 4; ++j) {
        m = 1 / JAug[j][j];
        for (k = 0; k <= 9; ++k) {
            JAug[j][k] = m * JAug[j][k];
        }
    }

    /* Perform Gauss Jordan Steps */
    for (k = 4; k >= 0; --k) {
        for (j = k - 1; j >= 0; --j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 9; ++n) {
                JAug[j][n] = JAug[j][n] + m * JAug[k][n];
            }
        }
    }

    /* Assign last 5 columns of JAug to InvJ */
    for (j = 0; j <= 4; ++j) {
        for (k = 0; k <= 4; ++k) {
            InvJ[j][k] = JAug[j][k + 5];
        }
    }

    return 0;
}

/******************************** MatMult5() *********************************/
/* Matrix-vector multiplication: 5x5 matrix by 1x5 vector */
static inline void pentakins_mat_mult5(double J[][5], const double x[], double Ans[])
{
    int j, k;
    for (j = 0; j <= 4; ++j) {
        Ans[j] = 0;
        for (k = 0; k <= 4; ++k) {
            Ans[j] = J[j][k] * x[k] + Ans[j];
        }
    }
}

/* Helper function: square */
static inline double pentakins_sqr(double x)
{
    return x * x;
}

/************************ pentakins_inv_kins() ********************************/
/* Internal inverse kinematics helper */
static inline int pentakins_inv_kins(const pentakins_params_t *params,
                                     const double coord[PENTAKINS_NUM_JOINTS],
                                     double struts[PENTAKINS_NUM_STRUTS])
{
    PmCartesian xyz, pmcoord, temp;
    PmRotationMatrix RMatrix, InvRMatrix;
    PmRpy rpy;
    int i;

    /* Define Rotation Matrix */
    pmcoord.x = coord[0];
    pmcoord.y = coord[1];
    pmcoord.z = coord[2];
    rpy.r = coord[3];  /* A axis (roll) */
    rpy.p = coord[4];  /* B axis (pitch) */
    rpy.y = 0;         /* No C axis rotation for pentapod */
    pmRpyMatConvert(&rpy, &RMatrix);

    /* Calculate strut lengths */
    for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
        /* Convert location of effector strut end from effector to world coordinates */
        pmCartCartSub(&params->base[i], &pmcoord, &temp);
        pmMatInv(&RMatrix, &InvRMatrix);
        pmMatCartMult(&InvRMatrix, &temp, &xyz);

        /* Define strut lengths using cylindrical geometry */
        double r_actual = sqrt(pentakins_sqr(xyz.x) + pentakins_sqr(xyz.y));
        double z_diff = xyz.z - (params->effector_z[i] + params->tool_offset);
        double r_diff = r_actual - params->effector_r[i];

        struts[i] = sqrt(pentakins_sqr(z_diff) + pentakins_sqr(r_diff));
    }

    return 0;
}

/************************ pentakins_fwd() ********************************/
/* Forward kinematics using Newton-Raphson iteration */
static inline int pentakins_fwd(pentakins_params_t *params,
                                const double joints[PENTAKINS_NUM_STRUTS],
                                EmcPose *pos)
{
    double Jacobian[PENTAKINS_NUM_STRUTS][PENTAKINS_NUM_STRUTS];
    double InverseJacobian[PENTAKINS_NUM_STRUTS][PENTAKINS_NUM_STRUTS];
    double InvKinStrutLength[PENTAKINS_NUM_STRUTS];
    double StrutLengthDiff[PENTAKINS_NUM_STRUTS];
    double delta[PENTAKINS_NUM_STRUTS];
    double jointdelta[PENTAKINS_NUM_STRUTS];
    double coord[PENTAKINS_NUM_JOINTS];
    double conv_err = 1.0;

    int iterate = 1;
    int i, j;
    unsigned iteration = 0;

    /* Abort on obvious problems, like joints <= 0 */
    if (joints[0] <= 0.0 || joints[1] <= 0.0 || joints[2] <= 0.0 ||
        joints[3] <= 0.0 || joints[4] <= 0.0) {
        return -1;
    }

    /* Initialize coordinate array from current position estimate */
    coord[0] = pos->tran.x;
    coord[1] = pos->tran.y;
    coord[2] = pos->tran.z;
    coord[3] = pos->a * PM_PI / 180.0;
    coord[4] = pos->b * PM_PI / 180.0;

    /* Enter Newton-Raphson iterative method */
    while (iterate) {
        /* Check for large error and return error flag if no convergence */
        if ((conv_err > params->max_error) || (conv_err < -params->max_error)) {
            return -2;
        }

        iteration++;

        /* Check iteration count */
        if (iteration > params->iter_limit) {
            return -5;
        }

        /* Compute StrutLengthDiff[] by running inverse kins on Cartesian estimate */
        pentakins_inv_kins(params, coord, InvKinStrutLength);

        for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
            StrutLengthDiff[i] = InvKinStrutLength[i] - joints[i];

            /* Build Inverse Jacobian Matrix using numerical differentiation */
            coord[i] += 1e-4;
            pentakins_inv_kins(params, coord, jointdelta);
            coord[i] -= 1e-4;

            for (j = 0; j < PENTAKINS_NUM_STRUTS; j++) {
                InverseJacobian[j][i] = (jointdelta[j] - InvKinStrutLength[j]) * 1e4;
            }
        }

        /* Invert Inverse Jacobian */
        pentakins_mat_invert5(InverseJacobian, Jacobian);

        /* Multiply Jacobian by StrutLengthDiff */
        pentakins_mat_mult5(Jacobian, StrutLengthDiff, delta);

        /* Subtract delta from last iteration's pos values */
        coord[0] -= delta[0];
        coord[1] -= delta[1];
        coord[2] -= delta[2];
        coord[3] -= delta[3];
        coord[4] -= delta[4];

        /* Determine value of conv_error */
        conv_err = 0.0;
        for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
            conv_err += fabs(StrutLengthDiff[i]);
        }

        /* Check if another iteration is needed */
        iterate = 0;
        for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
            if (fabs(StrutLengthDiff[i]) > params->conv_criterion) {
                iterate = 1;
            }
        }
    }

    /* Assign results to pos */
    pos->tran.x = coord[0];
    pos->tran.y = coord[1];
    pos->tran.z = coord[2];
    pos->a = coord[3] * 180.0 / PM_PI;
    pos->b = coord[4] * 180.0 / PM_PI;

    /* Update iteration statistics */
    params->last_iterations = iteration;
    if (iteration > params->max_iterations) {
        params->max_iterations = iteration;
    }

    return 0;
}

/************************ pentakins_inv() ********************************/
/* Inverse kinematics - closed form solution */
static inline int pentakins_inv(pentakins_params_t *params,
                                const EmcPose *pos,
                                double joints[PENTAKINS_NUM_STRUTS])
{
    double coord[PENTAKINS_NUM_JOINTS];

    coord[0] = pos->tran.x;
    coord[1] = pos->tran.y;
    coord[2] = pos->tran.z;
    coord[3] = pos->a * PM_PI / 180.0;
    coord[4] = pos->b * PM_PI / 180.0;

    if (0 != pentakins_inv_kins(params, coord, joints)) {
        return -1;
    }

    return 0;
}

#endif /* PENTAKINS_MATH_H */
