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
#include "pentakins_math.h"

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
