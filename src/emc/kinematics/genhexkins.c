/********************************************************************
* Description: genhexkins.c
*
*   Kinematics for a generalised hexapod machine
*
*   Derived from a work by R. Brian Register
*
* Adapting Author: Andrew Kyrychenko
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 2014.12.22
*********************************************************************

  These are the forward and inverse kinematic functions for a class of
  machines referred to as "Stewart Platforms".

  The functions are general enough to be configured for any platform
  configuration.  In the functions "kinematicsForward" and
  "kinematicsInverse" are arrays "a[i]" and "b[i]".  The values stored
  in these arrays correspond to the positions of the ends of the i'th
  strut. The value stored in a[i] is the position of the end of the i'th
  strut attatched to the platform, in platform coordinates. The value
  stored in b[i] is the position of the end of the i'th strut attached
  to the base, in base (world) coordinates.

  The default values for base and platform joints positions are defined
  in the header file genhexkins.h.  The actual values for a particular
  machine can be adjusted by hal parameters:

  genhexkins.base.N.x
  genhexkins.base.N.y
  genhexkins.base.N.z
  genhexkins.platform.N.x
  genhexkins.platform.N.y
  genhexkins.platform.N.z

  The kinematicsInverse function solves the inverse kinematics using
  a closed form algorithm.  The inverse kinematics problem is given
  the pose of the platform and returns the strut lengths. For this
  problem there is only one solution that is always returned correctly.

  The kinematicsForward function solves the forward kinematics using
  an iterative algorithm.  Due to the iterative nature of this algorithm
  the kinematicsForward function requires an initial value to begin the
  iterative routine and then converges to the "nearest" solution. The
  forward kinematics problem is given the strut lengths and returns the
  pose of the platform.  For this problem there arein multiple
  solutions.  The kinematicsForward function will return only one of
  these solutions which will be the solution nearest to the initial
  value given.  It is possible that there are no solutions "near" the
  given initial value and the iteration will not converge and no
  solution will be returned.  Assuming there is a solution "near" the
  initial value, the function will always return one correct solution
  out of the multiple possible solutions.

  Hal pins to control and observe forward kinematics iterations:

  genhexkins.convergence-criterion - minimum error value that ends
                    iterations with converged solution;

  genhexkins.limit-iterations - limit of iterations, if exceeded
                    iterations stop with no convergence;

  genhexkins.max-error - maximum error value, if exceeded iterations
                    stop with no convergence;

  genhexkins.last-iterations - number of iterations spent for the
                    last forward kinematics solution;

  genhexkins.max-iterations - maximum number of iterations spent for
                    a converged solution during current session.

 ----------------------------------------------------------------------------*/

#include "rtapi_math.h"
#include "posemath.h"
#include "genhexkins.h"
#include "kinematics.h"             /* these decls, KINEMATICS_FORWARD_FLAGS */
#include "hal.h"

struct haldata {
    hal_float_t basex[NUM_STRUTS];
    hal_float_t basey[NUM_STRUTS];
    hal_float_t basez[NUM_STRUTS];
    hal_float_t platformx[NUM_STRUTS];
    hal_float_t platformy[NUM_STRUTS];
    hal_float_t platformz[NUM_STRUTS];
    hal_u32_t *last_iter;
    hal_u32_t *max_iter;
    hal_u32_t *iter_limit;
    hal_float_t *max_error;
    hal_float_t *conv_criterion;
} *haldata;


/******************************* MatInvert() ***************************/

/*-----------------------------------------------------------------------------
 This is a function that inverts a 6x6 matrix.
-----------------------------------------------------------------------------*/

static int MatInvert(double J[][NUM_STRUTS], double InvJ[][NUM_STRUTS])
{
  double JAug[NUM_STRUTS][12], m, temp;
  int j, k, n;

  /* This function determines the inverse of a 6x6 matrix using
     Gauss-Jordan elimination */

  /* Augment the Identity matrix to the Jacobian matrix */

  for (j=0; j<=5; ++j){
    for (k=0; k<=5; ++k){     /* Assign J matrix to first 6 columns of AugJ */
      JAug[j][k] = J[j][k];
    }
    for(k=6; k<=11; ++k){    /* Assign I matrix to last six columns of AugJ */
      if (k-6 == j){
        JAug[j][k]=1;
      }
      else{
        JAug[j][k]=0;
      }
    }
  }

  /* Perform Gauss elimination */
  for (k=0; k<=4; ++k){               /* Pivot        */
    if ((JAug[k][k]< 0.01) && (JAug[k][k] > -0.01)){
      for (j=k+1;j<=5; ++j){
        if ((JAug[j][k]>0.01) || (JAug[j][k]<-0.01)){
          for (n=0; n<=11;++n){
            temp = JAug[k][n];
            JAug[k][n] = JAug[j][n];
            JAug[j][n] = temp;
          }
          break;
        }
      }
    }
    for (j=k+1; j<=5; ++j){            /* Pivot */
      m = -JAug[j][k] / JAug[k][k];
      for (n=0; n<=11; ++n){
        JAug[j][n]=JAug[j][n] + m*JAug[k][n];   /* (Row j) + m * (Row k) */
        if ((JAug[j][n] < 0.000001) && (JAug[j][n] > -0.000001)){
          JAug[j][n] = 0;
        }
      }
    }
  }

  /* Normalization of Diagonal Terms */
  for (j=0; j<=5; ++j){
    m=1/JAug[j][j];
    for(k=0; k<=11; ++k){
      JAug[j][k] = m * JAug[j][k];
    }
  }

  /* Perform Gauss Jordan Steps */
  for (k=5; k>=0; --k){
    for(j=k-1; j>=0; --j){
      m = -JAug[j][k]/JAug[k][k];
      for (n=0; n<=11; ++n){
        JAug[j][n] = JAug[j][n] + m * JAug[k][n];
      }
    }
  }

  /* Assign last 6 columns of JAug to InvJ */
  for (j=0; j<=5; ++j){
    for (k=0; k<=5; ++k){
      InvJ[j][k] = JAug[j][k+6];

    }
  }

  return 0;         /* FIXME-- check divisors for 0 above */
}

/******************************** MatMult() *********************************/

/*---------------------------------------------------------------------------
  This function simply multiplies a 6x6 matrix by a 1x6 vector
  ---------------------------------------------------------------------------*/

static void MatMult(double J[][6], const double x[], double Ans[])
{
  int j, k;
  for (j=0; j<=5; ++j){
    Ans[j] = 0;
    for (k=0; k<=5; ++k){
      Ans[j] = J[j][k]*x[k]+Ans[j];
    }
  }
}

/* declare arrays for base and platform coordinates */
static PmCartesian b[NUM_STRUTS];
static PmCartesian a[NUM_STRUTS];

/************************genhexkins_read_hal_pins**************************/

int genhexkins_read_hal_pins(void) {
    int t;

  /* set the base and platform coordinates from hal pin values */
    for (t = 0; t < NUM_STRUTS; t++) {
        b[t].x = haldata->basex[t];
        b[t].y = haldata->basey[t];
        b[t].z = haldata->basez[t];
        a[t].x = haldata->platformx[t];
        a[t].y = haldata->platformy[t];
        a[t].z = haldata->platformz[t];
    }
    return 0;
}

/**************************** jacobianInverse() ***************************/

static int JInvMat(const EmcPose * pos,
           double InverseJacobian[][NUM_STRUTS])
{
  int i;
  PmRpy rpy;
  PmRotationMatrix RMatrix;
  PmCartesian aw, RMatrix_a;
  PmCartesian InvKinStrutVect,InvKinStrutVectUnit;
  PmCartesian RMatrix_a_cross_Strut;

  genhexkins_read_hal_pins();

  /* assign a, b, c to roll, pitch, yaw angles and convert to rot matrix */
  rpy.r = pos->a * PM_PI / 180.0;
  rpy.p = pos->b * PM_PI / 180.0;
  rpy.y = pos->c * PM_PI / 180.0;
  pmRpyMatConvert(&rpy, &RMatrix);

  /* Enter for loop to build Inverse Jacobian */
  for (i = 0; i < NUM_STRUTS; i++) {
    /* run part of inverse kins to get strut vectors */
    pmMatCartMult(&RMatrix, &a[i], &RMatrix_a);
    pmCartCartAdd(&pos->tran, &RMatrix_a, &aw);
    pmCartCartSub(&aw, &b[i], &InvKinStrutVect);

    /* Determine RMatrix_a_cross_strut */
    if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit)) {
      return -1;
    }
    pmCartCartCross(&RMatrix_a, &InvKinStrutVectUnit, &RMatrix_a_cross_Strut);

    /* Build Inverse Jacobian Matrix */
    InverseJacobian[i][0] = InvKinStrutVectUnit.x;
    InverseJacobian[i][1] = InvKinStrutVectUnit.y;
    InverseJacobian[i][2] = InvKinStrutVectUnit.z;
    InverseJacobian[i][3] = RMatrix_a_cross_Strut.x;
    InverseJacobian[i][4] = RMatrix_a_cross_Strut.y;
    InverseJacobian[i][5] = RMatrix_a_cross_Strut.z;
  }

  return 0;
}

int jacobianInverse(const EmcPose * pos,
            const EmcPose * vel,
            const double * joints,
            double * jointvels)
{
  double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
  double velmatrix[6];

  if (0 != JInvMat(pos, InverseJacobian)) {
    return -1;
  }

  /* Multiply Jinv[] by vel[] to get jointvels */
  velmatrix[0] = vel->tran.x;   /* dx/dt */
  velmatrix[1] = vel->tran.y;   /* dy/dt */
  velmatrix[2] = vel->tran.z;   /* dz/dt */
  velmatrix[3] = vel->a;    /* droll/dt */
  velmatrix[4] = vel->b;    /* dpitch/dt */
  velmatrix[5] = vel->c;    /* dyaw/dt */
  MatMult(InverseJacobian, velmatrix, jointvels);

  return 0;
}

/**************************** jacobianForward() ***************************/

/* FIXME-- could use a better implementation than computing the
   inverse and then inverting it */
int jacobianForward(const double * joints,
            const double * jointvels,
            const EmcPose * pos,
            EmcPose * vel)
{
  double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
  double Jacobian[NUM_STRUTS][NUM_STRUTS];
  double velmatrix[6];

  if (0 != JInvMat(pos, InverseJacobian)) {
    return -1;
  }
  if (0 != MatInvert(InverseJacobian, Jacobian)) {
    return -1;
  }

  /* Multiply J[] by jointvels to get vels */
  MatMult(Jacobian, jointvels, velmatrix);
  vel->tran.x = velmatrix[0];
  vel->tran.y = velmatrix[1];
  vel->tran.z = velmatrix[2];
  vel->a = velmatrix[3];
  vel->b = velmatrix[4];
  vel->c = velmatrix[5];

  return 0;
}

/**************************** kinematicsForward() ***************************/

/* the inverse kinematics take world coordinates and determine joint values,
   given the inverse kinematics flags to resolve any ambiguities. The forward
   flags are set to indicate their value appropriate to the world coordinates
   passed in. */

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{

  PmCartesian aw;
  PmCartesian InvKinStrutVect,InvKinStrutVectUnit;
  PmCartesian q_trans, RMatrix_a, RMatrix_a_cross_Strut;

  double Jacobian[NUM_STRUTS][NUM_STRUTS];
  double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
  double InvKinStrutLength, StrutLengthDiff[NUM_STRUTS];
  double delta[NUM_STRUTS];
  double conv_err = 1.0;

  PmRotationMatrix RMatrix;
  PmRpy q_RPY;

  int iterate = 1;
  int i;
  int iteration = 0;

  genhexkins_read_hal_pins();

  /* abort on obvious problems, like joints <= 0 */
  /* FIXME-- should check against triangle inequality, so that joints
     are never too short to span shared base and platform sides */
  if (joints[0] <= 0.0 ||
      joints[1] <= 0.0 ||
      joints[2] <= 0.0 ||
      joints[3] <= 0.0 ||
      joints[4] <= 0.0 ||
      joints[5] <= 0.0) {
    return -1;
  }

  /* assign a,b,c to roll, pitch, yaw angles */
  q_RPY.r = pos->a * PM_PI / 180.0;
  q_RPY.p = pos->b * PM_PI / 180.0;
  q_RPY.y = pos->c * PM_PI / 180.0;

  /* Assign translation values in pos to q_trans */
  q_trans.x = pos->tran.x;
  q_trans.y = pos->tran.y;
  q_trans.z = pos->tran.z;

  /* Enter Newton-Raphson iterative method   */
  while (iterate) {
    /* check for large error and return error flag if no convergence */
    if ((conv_err > +(*haldata->max_error)) ||
    (conv_err < -(*haldata->max_error))) {
      /* we can't converge */
      return -2;
    };

    iteration++;

    /* check iteration to see if the kinematics can reach the
       convergence criterion and return error flag if it can't */
    if (iteration > *haldata->iter_limit) {
      /* we can't converge */
      return -5;
    }

    /* Convert q_RPY to Rotation Matrix */
    pmRpyMatConvert(&q_RPY, &RMatrix);

    /* compute StrutLengthDiff[] by running inverse kins on Cartesian
     estimate to get joint estimate, subtract joints to get joint deltas,
     and compute inv J while we're at it */
    for (i = 0; i < NUM_STRUTS; i++) {
      pmMatCartMult(&RMatrix, &a[i], &RMatrix_a);
      pmCartCartAdd(&q_trans, &RMatrix_a, &aw);
      pmCartCartSub(&aw, &b[i], &InvKinStrutVect);
      if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit)) {
    return -1;
      }
      pmCartMag(&InvKinStrutVect, &InvKinStrutLength);
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

    /* invert Inverse Jacobian */
    MatInvert(InverseJacobian, Jacobian);

    /* multiply Jacobian by LegLengthDiff */
    MatMult(Jacobian, StrutLengthDiff, delta);

    /* subtract delta from last iterations pos values */
    q_trans.x -= delta[0];
    q_trans.y -= delta[1];
    q_trans.z -= delta[2];
    q_RPY.r   -= delta[3];
    q_RPY.p   -= delta[4];
    q_RPY.y   -= delta[5];

    /* determine value of conv_error (used to determine if no convergence) */
    conv_err = 0.0;
    for (i = 0; i < NUM_STRUTS; i++) {
      conv_err += fabs(StrutLengthDiff[i]);
    }

    /* enter loop to determine if a strut needs another iteration */
    iterate = 0;            /*assume iteration is done */
    for (i = 0; i < NUM_STRUTS; i++) {
      if (fabs(StrutLengthDiff[i]) > *haldata->conv_criterion) {
    iterate = 1;
      }
    }
  } /* exit Newton-Raphson Iterative loop */

  /* assign r,p,w to a,b,c */
  pos->a = q_RPY.r * 180.0 / PM_PI;
  pos->b = q_RPY.p * 180.0 / PM_PI;
  pos->c = q_RPY.y * 180.0 / PM_PI;

  /* assign q_trans to pos */
  pos->tran.x = q_trans.x;
  pos->tran.y = q_trans.y;
  pos->tran.z = q_trans.z;

  *haldata->last_iter = iteration;

  if (iteration > *haldata->max_iter){
    *haldata->max_iter = iteration;
  }
  return 0;
}


/************************ kinematicsInverse() ********************************/

int kinematicsInverse(const EmcPose * pos,
                      double * joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{

  PmCartesian aw, temp;
  PmRotationMatrix RMatrix;
  PmRpy rpy;
  int i;

  genhexkins_read_hal_pins();

  /* define Rotation Matrix */
  rpy.r = pos->a * PM_PI / 180.0;
  rpy.p = pos->b * PM_PI / 180.0;
  rpy.y = pos->c * PM_PI / 180.0;
  pmRpyMatConvert(&rpy, &RMatrix);

  /* enter for loop to calculate joints (strut lengths) */
  for (i = 0; i < NUM_STRUTS; i++) {
    /* convert location of platform strut end from platform
       to world coordinates */
    pmMatCartMult(&RMatrix, &a[i], &temp);
    pmCartCartAdd(&pos->tran, &temp, &aw);

    /* define strut lengths */
    pmCartCartSub(&aw, &b[i], &temp);
    pmCartMag(&temp, &joints[i]);
  }

  return 0;
}


KINEMATICS_TYPE kinematicsType()
{
  return KINEMATICS_BOTH;
}


#include "rtapi.h"      /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);

MODULE_LICENSE("GPL");

int comp_id;

int rtapi_app_main(void)
{
    int res = 0, i;

    comp_id = hal_init("genhexkins");
    if (comp_id < 0)
    return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata)
    goto error;


    for (i = 0; i < 6; i++) {

        if ((res = hal_param_float_newf(HAL_RW, &(haldata->basex[i]), comp_id,
            "genhexkins.base.%d.x", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->basey[i], comp_id,
            "genhexkins.base.%d.y", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->basez[i], comp_id,
            "genhexkins.base.%d.z", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->platformx[i], comp_id,
            "genhexkins.platform.%d.x", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->platformy[i], comp_id,
            "genhexkins.platform.%d.y", i)) < 0)
        goto error;

        if ((res = hal_param_float_newf(HAL_RW, &haldata->platformz[i], comp_id,
            "genhexkins.platform.%d.z", i)) < 0)
        goto error;
    }

    if ((res = hal_pin_u32_newf(HAL_OUT, &haldata->last_iter, comp_id,
        "genhexkins.last-iterations")) < 0)
    goto error;
    *haldata->last_iter = 0;

    if ((res = hal_pin_u32_newf(HAL_OUT, &haldata->max_iter, comp_id,
        "genhexkins.max-iterations")) < 0)
    goto error;
    *haldata->max_iter = 0;

    if ((res = hal_pin_float_newf(HAL_IO, &haldata->max_error, comp_id,
        "genhexkins.max-error")) < 0)
    goto error;
    *haldata->max_error = 100;

    if ((res = hal_pin_float_newf(HAL_IO, &haldata->conv_criterion, comp_id,
        "genhexkins.convergence-criterion")) < 0)
    goto error;
    *haldata->conv_criterion = 1e-9;

    if ((res = hal_pin_u32_newf(HAL_IO, &haldata->iter_limit, comp_id,
        "genhexkins.limit-iterations")) < 0)
    goto error;
    *haldata->iter_limit = 120;

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
    haldata->basex[5] = DEFAULT_BASE_5_X;
    haldata->basey[5] = DEFAULT_BASE_5_Y;
    haldata->basez[5] = DEFAULT_BASE_5_Z;

    haldata->platformx[0] = DEFAULT_PLATFORM_0_X;
    haldata->platformy[0] = DEFAULT_PLATFORM_0_Y;
    haldata->platformz[0] = DEFAULT_PLATFORM_0_Z;
    haldata->platformx[1] = DEFAULT_PLATFORM_1_X;
    haldata->platformy[1] = DEFAULT_PLATFORM_1_Y;
    haldata->platformz[1] = DEFAULT_PLATFORM_1_Z;
    haldata->platformx[2] = DEFAULT_PLATFORM_2_X;
    haldata->platformy[2] = DEFAULT_PLATFORM_2_Y;
    haldata->platformz[2] = DEFAULT_PLATFORM_2_Z;
    haldata->platformx[3] = DEFAULT_PLATFORM_3_X;
    haldata->platformy[3] = DEFAULT_PLATFORM_3_Y;
    haldata->platformz[3] = DEFAULT_PLATFORM_3_Z;
    haldata->platformx[4] = DEFAULT_PLATFORM_4_X;
    haldata->platformy[4] = DEFAULT_PLATFORM_4_Y;
    haldata->platformz[4] = DEFAULT_PLATFORM_4_Z;
    haldata->platformx[5] = DEFAULT_PLATFORM_5_X;
    haldata->platformy[5] = DEFAULT_PLATFORM_5_Y;
    haldata->platformz[5] = DEFAULT_PLATFORM_5_Z;

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
