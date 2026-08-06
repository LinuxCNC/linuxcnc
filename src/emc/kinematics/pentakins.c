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

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>
#include <kinematics.h>             /* these decls, KINEMATICS_FORWARD_FLAGS */

#include "pentakins.h"

struct haldata {
    hal_real_t basex[NUM_STRUTS];
    hal_real_t basey[NUM_STRUTS];
    hal_real_t basez[NUM_STRUTS];
    hal_real_t effectorr[NUM_STRUTS];
    hal_real_t effectorz[NUM_STRUTS];
    hal_uint_t last_iter;
    hal_uint_t max_iter;
    hal_uint_t iter_limit;
    hal_real_t max_error;
    hal_real_t conv_criterion;
    hal_real_t tool_offset;
} *haldata;


/******************************* MatInvert5() ***************************/

/*-----------------------------------------------------------------------------
 This is a function that inverts a 5x5 matrix.
-----------------------------------------------------------------------------*/

static int MatInvert5(double J[][NUM_STRUTS], double InvJ[][NUM_STRUTS])
{
  double JAug[NUM_STRUTS][10], m, temp;
  int j, k, n;

  /* This function determines the inverse of a 6x6 matrix using
     Gauss-Jordan elimination */

  /* Augment the Identity matrix to the Jacobian matrix */

  for (j=0; j<=4; ++j){
    for (k=0; k<=4; ++k){     /* Assign J matrix to first 6 columns of AugJ */
      JAug[j][k] = J[j][k];
    }
    for(k=5; k<=9; ++k){    /* Assign I matrix to last six columns of AugJ */
      if (k-5 == j){
        JAug[j][k]=1;
      }
      else{
        JAug[j][k]=0;
      }
    }
  }

  /* Perform Gauss elimination */
  for (k=0; k<=3; ++k){               /* Pivot        */
    if ((JAug[k][k]< 0.01) && (JAug[k][k] > -0.01)){
      for (j=k+1;j<=4; ++j){
        if ((JAug[j][k]>0.01) || (JAug[j][k]<-0.01)){
          for (n=0; n<=9;++n){
            temp = JAug[k][n];
            JAug[k][n] = JAug[j][n];
            JAug[j][n] = temp;
          }
          break;
        }
      }
    }
    for (j=k+1; j<=4; ++j){            /* Pivot */
      m = -JAug[j][k] / JAug[k][k];
      for (n=0; n<=9; ++n){
        JAug[j][n]=JAug[j][n] + m*JAug[k][n];   /* (Row j) + m * (Row k) */
        if ((JAug[j][n] < 0.000001) && (JAug[j][n] > -0.000001)){
          JAug[j][n] = 0;
        }
      }
    }
  }

  /* Normalization of Diagonal Terms */
  for (j=0; j<=4; ++j){
    m=1/JAug[j][j];
    for(k=0; k<=9; ++k){
      JAug[j][k] = m * JAug[j][k];
    }
  }

  /* Perform Gauss Jordan Steps */
  for (k=4; k>=0; --k){
    for(j=k-1; j>=0; --j){
      m = -JAug[j][k]/JAug[k][k];
      for (n=0; n<=9; ++n){
        JAug[j][n] = JAug[j][n] + m * JAug[k][n];
      }
    }
  }

  /* Assign last 4 columns of JAug to InvJ */
  for (j=0; j<=4; ++j){
    for (k=0; k<=4; ++k){
      InvJ[j][k] = JAug[j][k+5];

    }
  }

  return 0;         /* FIXME-- check divisors for 0 above */
}

/******************************** MatMult() *********************************/

/*---------------------------------------------------------------------------
  This function simply multiplies a 6x6 matrix by a 1x6 vector
  ---------------------------------------------------------------------------*/

static void MatMult5(double J[][5], const double x[], double Ans[])
{
  int j, k;
  for (j=0; j<=4; ++j){
    Ans[j] = 0;
    for (k=0; k<=4; ++k){
      Ans[j] = J[j][k]*x[k]+Ans[j];
    }
  }
}

/*--------------
------square-----*/

static double sqr(double x)
{
	return (x)*(x);
}

/* declare arrays for base and effector coordinates */
static PmCartesian b[NUM_STRUTS];
static double za[NUM_STRUTS], ra[NUM_STRUTS];

/************************pentakins_read_hal_pins**************************/

int pentakins_read_hal_pins(void) {
    int t;

  /* set the base and effector coordinates from hal pin values */
    rtapi_real tool_offset = hal_get_real(haldata->tool_offset);
    for (t = 0; t < NUM_STRUTS; t++) {
        b[t].x = hal_get_real(haldata->basex[t]);
        b[t].y = hal_get_real(haldata->basey[t]);
        b[t].z = hal_get_real(haldata->basez[t]) + tool_offset;
        ra[t] = hal_get_real(haldata->effectorr[t]);
        za[t] = hal_get_real(haldata->effectorz[t]) + tool_offset;
    }
    return 0;
}

/************************ InvKins() ********************************/

int InvKins(const double * coord,
            double * struts)
{

  PmCartesian xyz, pmcoord, temp;
  PmRotationMatrix RMatrix, InvRMatrix;
  PmRpy rpy;
  int i;

//  pentakins_read_hal_pins();

  /* define Rotation Matrix */
  pmcoord.x = coord[0];
  pmcoord.y = coord[1];
  pmcoord.z = coord[2];
  rpy.r = coord[3];
  rpy.p = coord[4];
  rpy.y = 0;
  pmRpyMatConvert(&rpy, &RMatrix);

  /* enter for loop to calculate joints (strut lengths) */
  for (i = 0; i < NUM_STRUTS; i++) {
    /* convert location of effector strut end from effector
       to world coordinates */
    pmCartCartSub(&b[i], &pmcoord, &temp);
    pmMatInv(&RMatrix, &InvRMatrix);
    pmMatCartMult(&InvRMatrix, &temp, &xyz);

    /* define strut lengths */
    struts[i] = sqrt( sqr(xyz.z - za[i]) + sqr( sqrt(sqr(xyz.x) + sqr(xyz.y)) - ra[i]) );
  }

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

//  PmCartesian aw;
//  PmCartesian InvKinStrutVect,InvKinStrutVectUnit;
//  PmCartesian q_trans, RMatrix_a, RMatrix_a_cross_Strut;

  double Jacobian[NUM_STRUTS][NUM_STRUTS];
  double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
  double InvKinStrutLength[NUM_STRUTS], StrutLengthDiff[NUM_STRUTS];
  double delta[NUM_STRUTS];
  double jointdelta[NUM_STRUTS];
  double coord[NUM_STRUTS];
  double conv_err = 1.0;

//  PmRotationMatrix RMatrix;
//  PmRpy q_RPY;

  int iterate = 1;
  int i, j;
  unsigned iteration = 0;

  pentakins_read_hal_pins();

  /* abort on obvious problems, like joints <= 0 */
  if (joints[0] <= 0.0 ||
      joints[1] <= 0.0 ||
      joints[2] <= 0.0 ||
      joints[3] <= 0.0 ||
      joints[4] <= 0.0 ) {
    return -1;
  }

  /* assign a,b,c to roll, pitch, yaw angles */
  coord[0] = pos->tran.x;
  coord[1] = pos->tran.y;
  coord[2] = pos->tran.z;
  coord[3] = pos->a * PM_PI / 180.0;
  coord[4] = pos->b * PM_PI / 180.0;

  /* Enter Newton-Raphson iterative method   */
  rtapi_real max_error = hal_get_real(haldata->max_error);
  while (iterate) {
    /* check for large error and return error flag if no convergence */
    if ((conv_err > +(max_error)) ||
    (conv_err < -(max_error))) {
      /* we can't converge */
      return -2;
    };

    iteration++;

    /* check iteration to see if the kinematics can reach the
       convergence criterion and return error flag if it can't */
    if (iteration > hal_get_ui32(haldata->iter_limit)) {
      /* we can't converge */
      return -5;
    }

    /* compute StrutLengthDiff[] by running inverse kins on Cartesian
     estimate to get joint estimate, subtract joints to get joint deltas,
     and compute inv J while we're at it */
    InvKins(coord, InvKinStrutLength);

    for (i = 0; i < NUM_STRUTS; i++) {
      StrutLengthDiff[i] = InvKinStrutLength[i] - joints[i];

      /* Build Inverse Jacobian Matrix */
      coord[i] += 1e-4;
      InvKins(coord, jointdelta);
      coord[i] -= 1e-4;
      for (j = 0; j < NUM_STRUTS; j++) {
        InverseJacobian[j][i] = (jointdelta[j] - InvKinStrutLength[j]) * 1e4;
      }
    }

    /* invert Inverse Jacobian */
    MatInvert5(InverseJacobian, Jacobian);

    /* multiply Jacobian by LegLengthDiff */
    MatMult5(Jacobian, StrutLengthDiff, delta);

    /* subtract delta from last iterations pos values */
    coord[0] -= delta[0];
    coord[1] -= delta[1];
    coord[2] -= delta[2];
    coord[3] -= delta[3];
    coord[4] -= delta[4];

    /* determine value of conv_error (used to determine if no convergence) */
    conv_err = 0.0;
    for (i = 0; i < NUM_STRUTS; i++) {
      conv_err += fabs(StrutLengthDiff[i]);
    }

    /* enter loop to determine if a strut needs another iteration */
    iterate = 0;            /*assume iteration is done */
    rtapi_real conv_criterion = hal_get_real(haldata->conv_criterion);
    for (i = 0; i < NUM_STRUTS; i++) {
      if (fabs(StrutLengthDiff[i]) > conv_criterion) {
    iterate = 1;
      }
    }
  } /* exit Newton-Raphson Iterative loop */

  /* assign coord to pos */
  pos->tran.x = coord[0];
  pos->tran.y = coord[1];
  pos->tran.z = coord[2];
  pos->a = coord[3] * 180.0 / PM_PI;
  pos->b = coord[4] * 180.0 / PM_PI;

  hal_set_ui32(haldata->last_iter, iteration);

  if (iteration > hal_get_ui32(haldata->max_iter)){
    hal_set_ui32(haldata->max_iter, iteration);
  }
  return 0;
}


/************************ kinematicsInverse() ********************************/
/* the inverse kinematics take world coordinates and determine joint values,
   given the inverse kinematics flags to resolve any ambiguities. The forward
   flags are set to indicate their value appropriate to the world coordinates
   passed in. */

/************************ kinematicsInverse() ********************************/

int kinematicsInverse(const EmcPose * pos,
                      double * joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
  (void)iflags;
  (void)fflags;

  double coord[NUM_STRUTS];

  pentakins_read_hal_pins();

  coord[0] = pos->tran.x;
  coord[1] = pos->tran.y;
  coord[2] = pos->tran.z;
  coord[3] = pos->a * PM_PI / 180.0;
  coord[4] = pos->b * PM_PI / 180.0;

  if (0 != InvKins(coord,joints)) {
    return -1;
  }

  return 0;
}

KINEMATICS_TYPE kinematicsType()
{
  return KINEMATICS_BOTH;
}

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);

MODULE_LICENSE("GPL");

int comp_id;

static const rtapi_real init_basex[NUM_STRUTS] = {
    DEFAULT_BASE_0_X, DEFAULT_BASE_1_X, DEFAULT_BASE_2_X, DEFAULT_BASE_3_X, DEFAULT_BASE_4_X
};
static const rtapi_real init_basey[NUM_STRUTS] = {
    DEFAULT_BASE_0_Y, DEFAULT_BASE_1_Y, DEFAULT_BASE_2_Y, DEFAULT_BASE_3_Y, DEFAULT_BASE_4_Y
};
static const rtapi_real init_basez[NUM_STRUTS] = {
    DEFAULT_BASE_0_Z, DEFAULT_BASE_1_Z, DEFAULT_BASE_2_Z, DEFAULT_BASE_3_Z, DEFAULT_BASE_4_Z
};
static const rtapi_real init_effectorr[NUM_STRUTS] = {
    DEFAULT_EFFECTOR_0_R, DEFAULT_EFFECTOR_1_R, DEFAULT_EFFECTOR_2_R, DEFAULT_EFFECTOR_3_R, DEFAULT_EFFECTOR_4_R
};
static const rtapi_real init_effectorz[NUM_STRUTS] = {
    DEFAULT_EFFECTOR_0_Z, DEFAULT_EFFECTOR_1_Z, DEFAULT_EFFECTOR_2_Z, DEFAULT_EFFECTOR_3_Z, DEFAULT_EFFECTOR_4_Z
};

int rtapi_app_main(void)
{
    int res = 0, i;

    comp_id = hal_init("pentakins");
    if (comp_id < 0)
    return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata)
    goto error;


    for (i = 0; i < NUM_STRUTS; i++) {

        if ((res = hal_param_new_real(comp_id, HAL_RW, &(haldata->basex[i]),
                                      init_basex[i], "pentakins.base.%d.x", i)) < 0)
            goto error;

        if ((res = hal_param_new_real(comp_id, HAL_RW, &haldata->basey[i],
                                      init_basey[i], "pentakins.base.%d.y", i)) < 0)
            goto error;

        if ((res = hal_param_new_real(comp_id, HAL_RW, &haldata->basez[i],
                                      init_basez[i], "pentakins.base.%d.z", i)) < 0)
            goto error;

        if ((res = hal_param_new_real(comp_id, HAL_RW, &haldata->effectorr[i],
                                      init_effectorr[i], "pentakins.effector.%d.r", i)) < 0)
            goto error;

        if ((res = hal_param_new_real(comp_id, HAL_RW, &haldata->effectorz[i],
                                      init_effectorz[i], "pentakins.effector.%d.z", i)) < 0)
            goto error;
    }

    if ((res = hal_pin_new_ui32(comp_id, HAL_OUT, &haldata->last_iter,
                                0, "pentakins.last-iterations")) < 0)
        goto error;

    if ((res = hal_pin_new_ui32(comp_id, HAL_OUT, &haldata->max_iter,
                                0, "pentakins.max-iterations")) < 0)
        goto error;

    if ((res = hal_pin_new_real(comp_id, HAL_IO, &haldata->max_error,
                                100.0, "pentakins.max-error")) < 0)
        goto error;

    if ((res = hal_pin_new_real(comp_id, HAL_IO, &haldata->conv_criterion,
                                1e-9, "pentakins.convergence-criterion")) < 0)
        goto error;

    if ((res = hal_pin_new_ui32(comp_id, HAL_IO, &haldata->iter_limit,
                                120, "pentakins.limit-iterations")) < 0)
        goto error;

    if ((res = hal_pin_new_real(comp_id, HAL_IN, &haldata->tool_offset,
                                0.0, "pentakins.tool-offset")) < 0)
        goto error;

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
