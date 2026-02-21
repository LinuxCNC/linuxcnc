/********************************************************************
* Description: pentakins.c
*
*   Kinematics for a pentapod machine
*
*   Derived from genhexkins.c
*
* Author: Andrew Kyrychenko
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2016 All rights reserved.
*********************************************************************

  These are the forward and inverse kinematic functions for a pentapod
  parallel kinematics machine.

  The default values for base and effector joints positions are defined
  in the header file pentakins.h.  The actual values for a particular
  machine can be adjusted by hal parameters:

  pentakins.base.N.x
  pentakins.base.N.y
  pentakins.base.N.z
  pentakins.effector.N.r
  pentakins.effector.N.z

  Hal pins:

  pentakins.convergence-criterion - minimum error value that ends
                    iterations with converged solution;

  pentakins.limit-iterations - limit of iterations, if exceeded
                    iterations stop with no convergence;

  pentakins.max-error - maximum error value, if exceeded iterations
                    stop with no convergence;

  pentakins.last-iterations - number of iterations spent for the
                    last forward kinematics solution;

  pentakins.max-iterations - maximum number of iterations spent for
                    a converged solution during current session.

  pentakins.tool-offset - tool length from the origin along z axis,
                    changes the effector pivot point.

 ----------------------------------------------------------------------------*/

#include "rtapi_math.h"
#include "posemath.h"
#include "pentakins.h"
#include "kinematics.h"             /* these decls, KINEMATICS_FORWARD_FLAGS */
#include "hal.h"

/* ========================================================================
 * Math types and functions (was in pentakins_math.h)
 * ======================================================================== */

#define PENTAKINS_NUM_STRUTS 5
#define PENTAKINS_NUM_JOINTS 5

typedef struct {
    PmCartesian base[PENTAKINS_NUM_STRUTS];
    double effector_r[PENTAKINS_NUM_STRUTS];
    double effector_z[PENTAKINS_NUM_STRUTS];
    double conv_criterion;
    unsigned int iter_limit;
    double max_error;
    double tool_offset;
    unsigned int last_iterations;
    unsigned int max_iterations;
} pentakins_params_t;

static int pentakins_mat_invert5(double J[][PENTAKINS_NUM_STRUTS],
                                 double InvJ[][PENTAKINS_NUM_STRUTS])
{
    double JAug[PENTAKINS_NUM_STRUTS][10], m, temp;
    int j, k, n;
    for (j = 0; j <= 4; ++j) {
        for (k = 0; k <= 4; ++k) JAug[j][k] = J[j][k];
        for (k = 5; k <= 9; ++k) JAug[j][k] = (k - 5 == j) ? 1 : 0;
    }
    for (k = 0; k <= 3; ++k) {
        if ((JAug[k][k] < 0.01) && (JAug[k][k] > -0.01)) {
            for (j = k + 1; j <= 4; ++j) {
                if ((JAug[j][k] > 0.01) || (JAug[j][k] < -0.01)) {
                    for (n = 0; n <= 9; ++n) {
                        temp = JAug[k][n]; JAug[k][n] = JAug[j][n]; JAug[j][n] = temp;
                    }
                    break;
                }
            }
        }
        for (j = k + 1; j <= 4; ++j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 9; ++n) {
                JAug[j][n] = JAug[j][n] + m * JAug[k][n];
                if ((JAug[j][n] < 0.000001) && (JAug[j][n] > -0.000001)) JAug[j][n] = 0;
            }
        }
    }
    for (j = 0; j <= 4; ++j) {
        m = 1 / JAug[j][j];
        for (k = 0; k <= 9; ++k) JAug[j][k] = m * JAug[j][k];
    }
    for (k = 4; k >= 0; --k) {
        for (j = k - 1; j >= 0; --j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 9; ++n) JAug[j][n] = JAug[j][n] + m * JAug[k][n];
        }
    }
    for (j = 0; j <= 4; ++j)
        for (k = 0; k <= 4; ++k) InvJ[j][k] = JAug[j][k + 5];
    return 0;
}

static void pentakins_mat_mult5(double J[][5], const double x[], double Ans[])
{
    int j, k;
    for (j = 0; j <= 4; ++j) {
        Ans[j] = 0;
        for (k = 0; k <= 4; ++k) Ans[j] = J[j][k] * x[k] + Ans[j];
    }
}

static double pentakins_sqr(double x) { return x * x; }

static int pentakins_inv_kins(const pentakins_params_t *params,
                              const double coord[PENTAKINS_NUM_JOINTS],
                              double struts[PENTAKINS_NUM_STRUTS])
{
    PmCartesian xyz, pmcoord, temp;
    PmRotationMatrix RMatrix, InvRMatrix;
    PmRpy rpy;
    int i;

    pmcoord.x = coord[0]; pmcoord.y = coord[1]; pmcoord.z = coord[2];
    rpy.r = coord[3]; rpy.p = coord[4]; rpy.y = 0;
    pmRpyMatConvert(&rpy, &RMatrix);

    for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
        pmCartCartSub(&params->base[i], &pmcoord, &temp);
        pmMatInv(&RMatrix, &InvRMatrix);
        pmMatCartMult(&InvRMatrix, &temp, &xyz);
        double r_actual = sqrt(pentakins_sqr(xyz.x) + pentakins_sqr(xyz.y));
        double z_diff = xyz.z - (params->effector_z[i] + params->tool_offset);
        double r_diff = r_actual - params->effector_r[i];
        struts[i] = sqrt(pentakins_sqr(z_diff) + pentakins_sqr(r_diff));
    }
    return 0;
}

static int pentakins_fwd(pentakins_params_t *params,
                         const double joints[PENTAKINS_NUM_STRUTS],
                         EmcPose *pos)
{
    double Jacobian[PENTAKINS_NUM_STRUTS][PENTAKINS_NUM_STRUTS];
    double InverseJacobian[PENTAKINS_NUM_STRUTS][PENTAKINS_NUM_STRUTS];
    double InvKinStrutLength[PENTAKINS_NUM_STRUTS];
    double StrutLengthDiff[PENTAKINS_NUM_STRUTS];
    double delta[PENTAKINS_NUM_STRUTS], jointdelta[PENTAKINS_NUM_STRUTS];
    double coord[PENTAKINS_NUM_JOINTS];
    double conv_err = 1.0;
    int iterate = 1, i, j;
    unsigned iteration = 0;

    if (joints[0] <= 0.0 || joints[1] <= 0.0 || joints[2] <= 0.0 ||
        joints[3] <= 0.0 || joints[4] <= 0.0) return -1;

    coord[0] = pos->tran.x; coord[1] = pos->tran.y; coord[2] = pos->tran.z;
    coord[3] = pos->a * PM_PI / 180.0; coord[4] = pos->b * PM_PI / 180.0;

    while (iterate) {
        if ((conv_err > params->max_error) || (conv_err < -params->max_error)) return -2;
        iteration++;
        if (iteration > params->iter_limit) return -5;

        pentakins_inv_kins(params, coord, InvKinStrutLength);
        for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
            StrutLengthDiff[i] = InvKinStrutLength[i] - joints[i];
            coord[i] += 1e-4;
            pentakins_inv_kins(params, coord, jointdelta);
            coord[i] -= 1e-4;
            for (j = 0; j < PENTAKINS_NUM_STRUTS; j++)
                InverseJacobian[j][i] = (jointdelta[j] - InvKinStrutLength[j]) * 1e4;
        }
        pentakins_mat_invert5(InverseJacobian, Jacobian);
        pentakins_mat_mult5(Jacobian, StrutLengthDiff, delta);
        for (i = 0; i < PENTAKINS_NUM_JOINTS; i++) coord[i] -= delta[i];
        conv_err = 0.0;
        for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) conv_err += fabs(StrutLengthDiff[i]);
        iterate = 0;
        for (i = 0; i < PENTAKINS_NUM_STRUTS; i++)
            if (fabs(StrutLengthDiff[i]) > params->conv_criterion) iterate = 1;
    }

    pos->tran.x = coord[0]; pos->tran.y = coord[1]; pos->tran.z = coord[2];
    pos->a = coord[3] * 180.0 / PM_PI; pos->b = coord[4] * 180.0 / PM_PI;
    params->last_iterations = iteration;
    if (iteration > params->max_iterations) params->max_iterations = iteration;
    return 0;
}

static int pentakins_inv(pentakins_params_t *params,
                         const EmcPose *pos,
                         double joints[PENTAKINS_NUM_STRUTS])
{
    double coord[PENTAKINS_NUM_JOINTS];
    coord[0] = pos->tran.x; coord[1] = pos->tran.y; coord[2] = pos->tran.z;
    coord[3] = pos->a * PM_PI / 180.0; coord[4] = pos->b * PM_PI / 180.0;
    return pentakins_inv_kins(params, coord, joints);
}

/* ========================================================================
 * RT interface
 * ======================================================================== */

struct haldata {
    hal_float_t basex[NUM_STRUTS];
    hal_float_t basey[NUM_STRUTS];
    hal_float_t basez[NUM_STRUTS];
    hal_float_t effectorr[NUM_STRUTS];
    hal_float_t effectorz[NUM_STRUTS];
    hal_u32_t *last_iter;
    hal_u32_t *max_iter;
    hal_u32_t *iter_limit;
    hal_float_t *max_error;
    hal_float_t *conv_criterion;
    hal_float_t *tool_offset;
} *haldata;

/* Global parameters structure for math functions */
static pentakins_params_t params;

/************************pentakins_read_hal_pins**************************/

int pentakins_read_hal_pins(void) {
    int t;

    /* Set the base and effector coordinates from hal pin values */
    for (t = 0; t < NUM_STRUTS; t++) {
        params.base[t].x = haldata->basex[t];
        params.base[t].y = haldata->basey[t];
        params.base[t].z = haldata->basez[t];
        params.effector_r[t] = haldata->effectorr[t];
        params.effector_z[t] = haldata->effectorz[t];
    }

    /* Update iteration parameters */
    params.conv_criterion = *haldata->conv_criterion;
    params.iter_limit = *haldata->iter_limit;
    params.max_error = *haldata->max_error;
    params.tool_offset = *haldata->tool_offset;

    return 0;
}

/**************************** kinematicsForward() ***************************/

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
  (void)fflags;
  (void)iflags;
  int result;

  pentakins_read_hal_pins();

  /* Call pure math function */
  result = pentakins_fwd(&params, joints, pos);

  /* Update HAL pins with iteration statistics */
  *haldata->last_iter = params.last_iterations;
  if (params.last_iterations > *haldata->max_iter) {
    *haldata->max_iter = params.last_iterations;
  }

  return result;
}


/************************ kinematicsInverse() ********************************/

int kinematicsInverse(const EmcPose * pos,
                      double * joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
  (void)iflags;
  (void)fflags;

  pentakins_read_hal_pins();

  /* Call pure math function */
  return pentakins_inv(&params, pos, joints);
}

KINEMATICS_TYPE kinematicsType()
{
  return KINEMATICS_BOTH;
}


#include "rtapi.h"      /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */

const char* kinematicsGetName(void) { return "pentakins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);

MODULE_LICENSE("GPL");

int comp_id;

int rtapi_app_main(void)
{
    int res = 0, i;

    comp_id = hal_init("pentakins");
    if (comp_id < 0)
    return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata)
    goto error;


    for (i = 0; i < 6; i++) {

        if ((res = hal_param_float_newf(HAL_RW, &(haldata->basex[i]), comp_id,
            "pentakins.base.%d.x", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->basey[i], comp_id,
            "pentakins.base.%d.y", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->basez[i], comp_id,
            "pentakins.base.%d.z", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->effectorr[i], comp_id,
            "pentakins.effector.%d.r", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->effectorz[i], comp_id,
            "pentakins.effector.%d.z", i)) < 0)
        goto error;
    }

    if ((res = hal_pin_u32_newf(HAL_OUT, &haldata->last_iter, comp_id,
        "pentakins.last-iterations")) < 0)
    goto error;
    *haldata->last_iter = 0;

    if ((res = hal_pin_u32_newf(HAL_OUT, &haldata->max_iter, comp_id,
        "pentakins.max-iterations")) < 0)
    goto error;
    *haldata->max_iter = 0;

    if ((res = hal_pin_float_newf(HAL_IO, &haldata->max_error, comp_id,
        "pentakins.max-error")) < 0)
    goto error;
    *haldata->max_error = 100;

    if ((res = hal_pin_float_newf(HAL_IO, &haldata->conv_criterion, comp_id,
        "pentakins.convergence-criterion")) < 0)
    goto error;
    *haldata->conv_criterion = 1e-9;

    if ((res = hal_pin_u32_newf(HAL_IO, &haldata->iter_limit, comp_id,
        "pentakins.limit-iterations")) < 0)
    goto error;
    *haldata->iter_limit = 120;

    if ((res = hal_pin_float_newf(HAL_IN, &haldata->tool_offset, comp_id,
        "pentakins.tool-offset")) < 0)
    goto error;
    *haldata->tool_offset = 0.0;

    haldata->basex[0] = DEFAULT_BASE_0_X;
    haldata->basey[0] = DEFAULT_BASE_0_Y;
    haldata->basez[0] = DEFAULT_BASE_0_Z;
    haldata->basex[1] = DEFAULT_BASE_1_X;
    haldata->basey[1] = DEFAULT_BASE_1_Y;
    haldata->basez[1] = DEFAULT_BASE_1_Z;
    haldata->basex[2] = DEFAULT_BASE_2_X;
    haldata->basey[2] = DEFAULT_BASE_2_Y;
    haldata->basez[2] = DEFAULT_BASE_2_Z;
    haldata->basex[3] = DEFAULT_BASE_3_X;
    haldata->basey[3] = DEFAULT_BASE_3_Y;
    haldata->basez[3] = DEFAULT_BASE_3_Z;
    haldata->basex[4] = DEFAULT_BASE_4_X;
    haldata->basey[4] = DEFAULT_BASE_4_Y;
    haldata->basez[4] = DEFAULT_BASE_4_Z;

    haldata->effectorz[0] = DEFAULT_EFFECTOR_0_Z;
    haldata->effectorz[1] = DEFAULT_EFFECTOR_1_Z;
    haldata->effectorz[2] = DEFAULT_EFFECTOR_2_Z;
    haldata->effectorz[3] = DEFAULT_EFFECTOR_3_Z;
    haldata->effectorz[4] = DEFAULT_EFFECTOR_4_Z;

    haldata->effectorr[0] = DEFAULT_EFFECTOR_0_R;
    haldata->effectorr[1] = DEFAULT_EFFECTOR_1_R;
    haldata->effectorr[2] = DEFAULT_EFFECTOR_2_R;
    haldata->effectorr[3] = DEFAULT_EFFECTOR_3_R;
    haldata->effectorr[4] = DEFAULT_EFFECTOR_4_R;

    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return res;
}


void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"

static void nonrt_build_penta(const kinematics_params_t *kp, pentakins_params_t *p)
{
    int i;
    for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
        p->base[i].x = kp->params.penta.basex[i];
        p->base[i].y = kp->params.penta.basey[i];
        p->base[i].z = kp->params.penta.basez[i];
        p->effector_r[i] = kp->params.penta.effectorr[i];
        p->effector_z[i] = kp->params.penta.effectorz[i];
    }
    p->conv_criterion = kp->params.penta.conv_criterion;
    p->iter_limit = kp->params.penta.iter_limit;
    p->max_error = 100.0;
    p->tool_offset = 0.0;
    p->last_iterations = 0;
    p->max_iterations = 0;
}

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    pentakins_params_t p;
    nonrt_build_penta(kp, &p);
    return pentakins_fwd(&p, joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    pentakins_params_t p;
    nonrt_build_penta(kp, &p);
    return pentakins_inv(&p, pos, joints);
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

    for (i = 0; i < KINS_PENTA_NUM_STRUTS; i++) {
        rtapi_snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.x", i);
        read_float(pin_name, &kp->params.penta.basex[i]);
        rtapi_snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.y", i);
        read_float(pin_name, &kp->params.penta.basey[i]);
        rtapi_snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.z", i);
        read_float(pin_name, &kp->params.penta.basez[i]);
        rtapi_snprintf(pin_name, sizeof(pin_name), "pentakins.effector.%d.r", i);
        read_float(pin_name, &kp->params.penta.effectorr[i]);
        rtapi_snprintf(pin_name, sizeof(pin_name), "pentakins.effector.%d.z", i);
        read_float(pin_name, &kp->params.penta.effectorz[i]);
    }

    read_float("pentakins.convergence-criterion", &kp->params.penta.conv_criterion);

    if (read_s32) {
        int val;
        if (read_s32("pentakins.limit-iterations", &val) == 0)
            kp->params.penta.iter_limit = (unsigned int)val;
    }

    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
