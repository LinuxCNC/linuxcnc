/********************************************************************
* Description: genhexkins_math.h
*   Pure math functions for genhexkins (6-DOF hexapod/Stewart platform)
*   Extracted from genhexkins.c for userspace kinematics support
*
* Author: R. Brian Register, Andrew Kyrychenko (original genhexkins.c)
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#ifndef GENHEXKINS_MATH_H
#define GENHEXKINS_MATH_H

#include "posemath.h"
#include <math.h>

#define GENHEX_NUM_STRUTS 6

/* Genhexkins configuration parameters */
typedef struct {
    /* Base coordinates (world frame) */
    PmCartesian base[GENHEX_NUM_STRUTS];

    /* Platform coordinates (platform frame) */
    PmCartesian platform[GENHEX_NUM_STRUTS];

    /* Base joint axes (world frame) - for strut length correction */
    PmCartesian base_n[GENHEX_NUM_STRUTS];

    /* Platform joint axes (platform frame) - for strut length correction */
    PmCartesian platform_n[GENHEX_NUM_STRUTS];

    /* Iteration control parameters */
    double conv_criterion;      /* convergence criterion */
    unsigned int iter_limit;    /* iteration limit */
    double max_error;           /* maximum error threshold */
    double tool_offset;         /* tool length offset */
    double spindle_offset;      /* spindle offset */
    double screw_lead;          /* lead of actuator screw (for length correction) */

    /* Output/status */
    unsigned int last_iterations;
    unsigned int max_iterations;
    double correction[GENHEX_NUM_STRUTS];  /* strut length corrections */
} genhex_params_t;

/* Default parameter values */
static inline void genhex_init_params(genhex_params_t *params) {
    int i;

    /* Default base positions */
    params->base[0].x = -22.950; params->base[0].y =  13.250; params->base[0].z = 0.000;
    params->base[1].x =  22.950; params->base[1].y =  13.250; params->base[1].z = 0.000;
    params->base[2].x =  22.950; params->base[2].y =  13.250; params->base[2].z = 0.000;
    params->base[3].x =   0.000; params->base[3].y = -26.500; params->base[3].z = 0.000;
    params->base[4].x =   0.000; params->base[4].y = -26.500; params->base[4].z = 0.000;
    params->base[5].x = -22.950; params->base[5].y =  13.250; params->base[5].z = 0.000;

    /* Default platform positions */
    params->platform[0].x =  -1.000; params->platform[0].y =  11.500; params->platform[0].z = 0.000;
    params->platform[1].x =   1.000; params->platform[1].y =  11.500; params->platform[1].z = 0.000;
    params->platform[2].x =  10.459; params->platform[2].y =  -4.884; params->platform[2].z = 0.000;
    params->platform[3].x =   9.459; params->platform[3].y =  -6.616; params->platform[3].z = 0.000;
    params->platform[4].x =  -9.459; params->platform[4].y =  -6.616; params->platform[4].z = 0.000;
    params->platform[5].x = -10.459; params->platform[5].y =  -4.884; params->platform[5].z = 0.000;

    /* Default base joint axes */
    params->base_n[0].x =  0.707107; params->base_n[0].y =  0.0;      params->base_n[0].z = 0.707107;
    params->base_n[1].x =  0.0;      params->base_n[1].y = -0.707107; params->base_n[1].z = 0.707107;
    params->base_n[2].x = -0.707107; params->base_n[2].y =  0.0;      params->base_n[2].z = 0.707107;
    params->base_n[3].x = -0.707107; params->base_n[3].y =  0.0;      params->base_n[3].z = 0.707107;
    params->base_n[4].x =  0.0;      params->base_n[4].y =  0.707107; params->base_n[4].z = 0.707107;
    params->base_n[5].x =  0.707107; params->base_n[5].y =  0.0;      params->base_n[5].z = 0.707107;

    /* Default platform joint axes */
    params->platform_n[0].x = -1.0;     params->platform_n[0].y =  0.0; params->platform_n[0].z = 0.0;
    params->platform_n[1].x =  0.866025; params->platform_n[1].y =  0.5; params->platform_n[1].z = 0.0;
    params->platform_n[2].x =  0.866025; params->platform_n[2].y =  0.5; params->platform_n[2].z = 0.0;
    params->platform_n[3].x =  0.866025; params->platform_n[3].y = -0.5; params->platform_n[3].z = 0.0;
    params->platform_n[4].x =  0.866025; params->platform_n[4].y = -0.5; params->platform_n[4].z = 0.0;
    params->platform_n[5].x = -1.0;     params->platform_n[5].y =  0.0; params->platform_n[5].z = 0.0;

    /* Iteration parameters */
    params->conv_criterion = 1e-9;
    params->iter_limit = 120;
    params->max_error = 500.0;
    params->tool_offset = 0.0;
    params->spindle_offset = 0.0;
    params->screw_lead = 0.0;  /* Default: no strut length correction */

    /* Status */
    params->last_iterations = 0;
    params->max_iterations = 0;
    for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
        params->correction[i] = 0.0;
    }
}

/******************************* MatInvert() ***************************/
/* Matrix inversion using Gauss-Jordan elimination for 6x6 matrix */
static inline int genhex_mat_invert(double J[][GENHEX_NUM_STRUTS],
                                    double InvJ[][GENHEX_NUM_STRUTS])
{
    double JAug[GENHEX_NUM_STRUTS][12], m, temp;
    int j, k, n;

    /* Augment the Identity matrix to the Jacobian matrix */
    for (j = 0; j <= 5; ++j) {
        for (k = 0; k <= 5; ++k) {
            JAug[j][k] = J[j][k];
        }
        for (k = 6; k <= 11; ++k) {
            if (k - 6 == j) {
                JAug[j][k] = 1;
            } else {
                JAug[j][k] = 0;
            }
        }
    }

    /* Perform Gauss elimination */
    for (k = 0; k <= 4; ++k) {
        if ((JAug[k][k] < 0.01) && (JAug[k][k] > -0.01)) {
            for (j = k + 1; j <= 5; ++j) {
                if ((JAug[j][k] > 0.01) || (JAug[j][k] < -0.01)) {
                    for (n = 0; n <= 11; ++n) {
                        temp = JAug[k][n];
                        JAug[k][n] = JAug[j][n];
                        JAug[j][n] = temp;
                    }
                    break;
                }
            }
        }
        for (j = k + 1; j <= 5; ++j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 11; ++n) {
                JAug[j][n] = JAug[j][n] + m * JAug[k][n];
                if ((JAug[j][n] < 0.000001) && (JAug[j][n] > -0.000001)) {
                    JAug[j][n] = 0;
                }
            }
        }
    }

    /* Normalization of Diagonal Terms */
    for (j = 0; j <= 5; ++j) {
        m = 1 / JAug[j][j];
        for (k = 0; k <= 11; ++k) {
            JAug[j][k] = m * JAug[j][k];
        }
    }

    /* Perform Gauss Jordan Steps */
    for (k = 5; k >= 0; --k) {
        for (j = k - 1; j >= 0; --j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 11; ++n) {
                JAug[j][n] = JAug[j][n] + m * JAug[k][n];
            }
        }
    }

    /* Assign last 6 columns of JAug to InvJ */
    for (j = 0; j <= 5; ++j) {
        for (k = 0; k <= 5; ++k) {
            InvJ[j][k] = JAug[j][k + 6];
        }
    }

    return 0;
}

/******************************** MatMult() *********************************/
/* Matrix-vector multiplication: 6x6 matrix by 1x6 vector */
static inline void genhex_mat_mult(double J[][6], const double x[], double Ans[])
{
    int j, k;
    for (j = 0; j <= 5; ++j) {
        Ans[j] = 0;
        for (k = 0; k <= 5; ++k) {
            Ans[j] = J[j][k] * x[k] + Ans[j];
        }
    }
}

/*********************** StrutLengthCorrection() *************************/
/* Calculate strut length correction based on joint orientations */
static inline int genhex_strut_length_correction(const genhex_params_t *params,
                                                  const PmCartesian *StrutVectUnit,
                                                  const PmRotationMatrix *RMatrix,
                                                  int strut_number,
                                                  double *correction)
{
    PmCartesian nb2, nb3, na1, na2;
    double dotprod;

    /* Define base joints axis vectors */
    pmCartCartCross(&params->base_n[strut_number], StrutVectUnit, &nb2);
    pmCartCartCross(StrutVectUnit, &nb2, &nb3);
    pmCartUnitEq(&nb3);

    /* Define platform joints axis vectors */
    pmMatCartMult(RMatrix, &params->platform_n[strut_number], &na1);
    pmCartCartCross(&na1, StrutVectUnit, &na2);
    pmCartUnitEq(&na2);

    /* Define dot product */
    pmCartCartDot(&nb3, &na2, &dotprod);

    *correction = params->screw_lead * asin(dotprod) / PM_2_PI;

    return 0;
}

/************************ genhex_fwd() ********************************/
/* Forward kinematics using Newton-Raphson iteration */
static inline int genhex_fwd(genhex_params_t *params,
                             const double joints[GENHEX_NUM_STRUTS],
                             EmcPose *pos)
{
    PmCartesian aw;
    PmCartesian InvKinStrutVect, InvKinStrutVectUnit;
    PmCartesian q_trans, RMatrix_a, RMatrix_a_cross_Strut;

    double Jacobian[GENHEX_NUM_STRUTS][GENHEX_NUM_STRUTS];
    double InverseJacobian[GENHEX_NUM_STRUTS][GENHEX_NUM_STRUTS];
    double InvKinStrutLength, StrutLengthDiff[GENHEX_NUM_STRUTS];
    double delta[GENHEX_NUM_STRUTS];
    double conv_err = 1.0;
    double corr;

    PmRotationMatrix RMatrix;
    PmRpy q_RPY;

    int iterate = 1;
    int i;
    unsigned iteration = 0;

    /* Abort on obvious problems, like joints <= 0 */
    if (joints[0] <= 0.0 || joints[1] <= 0.0 || joints[2] <= 0.0 ||
        joints[3] <= 0.0 || joints[4] <= 0.0 || joints[5] <= 0.0) {
        return -1;
    }

    /* Assign a,b,c to roll, pitch, yaw angles */
    q_RPY.r = pos->a * PM_PI / 180.0;
    q_RPY.p = pos->b * PM_PI / 180.0;
    q_RPY.y = pos->c * PM_PI / 180.0;

    /* Assign translation values in pos to q_trans */
    q_trans.x = pos->tran.x;
    q_trans.y = pos->tran.y;
    q_trans.z = pos->tran.z;

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

        /* Convert q_RPY to Rotation Matrix */
        pmRpyMatConvert(&q_RPY, &RMatrix);

        /* Compute StrutLengthDiff[] by running inverse kins on Cartesian estimate */
        for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
            /* Adjust base and platform coordinates for offsets */
            PmCartesian b_adjusted, a_adjusted;
            b_adjusted.x = params->base[i].x;
            b_adjusted.y = params->base[i].y;
            b_adjusted.z = params->base[i].z + params->spindle_offset + params->tool_offset;
            a_adjusted.x = params->platform[i].x;
            a_adjusted.y = params->platform[i].y;
            a_adjusted.z = params->platform[i].z + params->spindle_offset + params->tool_offset;

            pmMatCartMult(&RMatrix, &a_adjusted, &RMatrix_a);
            pmCartCartAdd(&q_trans, &RMatrix_a, &aw);
            pmCartCartSub(&aw, &b_adjusted, &InvKinStrutVect);

            if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit)) {
                return -1;
            }
            pmCartMag(&InvKinStrutVect, &InvKinStrutLength);

            /* Apply strut length correction if enabled */
            if (params->screw_lead != 0.0) {
                genhex_strut_length_correction(params, &InvKinStrutVectUnit,
                                               &RMatrix, i, &corr);
                InvKinStrutLength += corr;
            }

            StrutLengthDiff[i] = InvKinStrutLength - joints[i];

            /* Determine RMatrix_a_cross_strut */
            pmCartCartCross(&RMatrix_a, &InvKinStrutVectUnit, &RMatrix_a_cross_Strut);

            /* Build Inverse Jacobian Matrix */
            InverseJacobian[i][0] = InvKinStrutVectUnit.x;
            InverseJacobian[i][1] = InvKinStrutVectUnit.y;
            InverseJacobian[i][2] = InvKinStrutVectUnit.z;
            InverseJacobian[i][3] = RMatrix_a_cross_Strut.x;
            InverseJacobian[i][4] = RMatrix_a_cross_Strut.y;
            InverseJacobian[i][5] = RMatrix_a_cross_Strut.z;
        }

        /* Invert Inverse Jacobian */
        genhex_mat_invert(InverseJacobian, Jacobian);

        /* Multiply Jacobian by StrutLengthDiff */
        genhex_mat_mult(Jacobian, StrutLengthDiff, delta);

        /* Subtract delta from last iteration's pos values */
        q_trans.x -= delta[0];
        q_trans.y -= delta[1];
        q_trans.z -= delta[2];
        q_RPY.r   -= delta[3];
        q_RPY.p   -= delta[4];
        q_RPY.y   -= delta[5];

        /* Determine value of conv_error */
        conv_err = 0.0;
        for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
            conv_err += fabs(StrutLengthDiff[i]);
        }

        /* Check if another iteration is needed */
        iterate = 0;
        for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
            if (fabs(StrutLengthDiff[i]) > params->conv_criterion) {
                iterate = 1;
            }
        }
    }

    /* Assign r,p,y to a,b,c */
    pos->a = q_RPY.r * 180.0 / PM_PI;
    pos->b = q_RPY.p * 180.0 / PM_PI;
    pos->c = q_RPY.y * 180.0 / PM_PI;

    /* Assign q_trans to pos */
    pos->tran.x = q_trans.x;
    pos->tran.y = q_trans.y;
    pos->tran.z = q_trans.z;

    /* Update iteration statistics */
    params->last_iterations = iteration;
    if (iteration > params->max_iterations) {
        params->max_iterations = iteration;
    }

    return 0;
}

/************************ genhex_inv() ********************************/
/* Inverse kinematics - closed form solution */
static inline int genhex_inv(genhex_params_t *params,
                             const EmcPose *pos,
                             double joints[GENHEX_NUM_STRUTS])
{
    PmCartesian aw, temp;
    PmCartesian InvKinStrutVect, InvKinStrutVectUnit;
    PmRotationMatrix RMatrix;
    PmRpy rpy;
    int i;
    double InvKinStrutLength, corr;

    /* Define Rotation Matrix */
    rpy.r = pos->a * PM_PI / 180.0;
    rpy.p = pos->b * PM_PI / 180.0;
    rpy.y = pos->c * PM_PI / 180.0;
    pmRpyMatConvert(&rpy, &RMatrix);

    /* Calculate joints (strut lengths) */
    for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
        /* Adjust base and platform coordinates for offsets */
        PmCartesian b_adjusted, a_adjusted;
        b_adjusted.x = params->base[i].x;
        b_adjusted.y = params->base[i].y;
        b_adjusted.z = params->base[i].z + params->spindle_offset + params->tool_offset;
        a_adjusted.x = params->platform[i].x;
        a_adjusted.y = params->platform[i].y;
        a_adjusted.z = params->platform[i].z + params->spindle_offset + params->tool_offset;

        /* Convert location of platform strut end from platform to world coordinates */
        pmMatCartMult(&RMatrix, &a_adjusted, &temp);
        pmCartCartAdd(&pos->tran, &temp, &aw);

        /* Define strut lengths */
        pmCartCartSub(&aw, &b_adjusted, &InvKinStrutVect);
        pmCartMag(&InvKinStrutVect, &InvKinStrutLength);

        /* Apply strut length correction if enabled */
        if (params->screw_lead != 0.0) {
            if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit)) {
                return -1;
            }
            genhex_strut_length_correction(params, &InvKinStrutVectUnit,
                                           &RMatrix, i, &corr);
            params->correction[i] = corr;
            InvKinStrutLength += corr;
        }

        joints[i] = InvKinStrutLength;
    }

    return 0;
}

#endif /* GENHEXKINS_MATH_H */
