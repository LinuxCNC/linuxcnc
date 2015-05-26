/********************************************************************
* Description: genhexkins.c
*   Kinematics for a generalised hexapod machine
*
*   Derived from a work by R. Brian Register
*
* Author: 
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
*******************************************************************

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

  To have these functions solve the kinematics of a particular
  platform configuration, adjust the values stored in arrays "a[i]" and
  "b[i]".  The values stored in these arrays are defined in the header
  file genhex.h.  The kinematicsInverse function solves the inverse
  kinematics using a closed form algorithm.  The inverse kinematics
  problem is given the pose of the platform and returns the strut
  lengths. For this problem there is only one solution that is always
  returned correctly.

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
  -----------------------------------------------------------------------------*/

#include "rtapi_math.h"
#include "posemath.h"
#include "genhexkins.h"
#include "kinematics.h"             /* these decls, KINEMATICS_FORWARD_FLAGS */

#define VTVERSION VTKINEMATICS_VERSION1

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

  return 0;			/* FIXME-- check divisors for 0 above */
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

/* define position of base strut ends in base (world) coordinate system */
static PmCartesian b[6] = {{BASE_0_X, BASE_0_Y, BASE_0_Z},
			   {BASE_1_X, BASE_1_Y, BASE_1_Z},
			   {BASE_2_X, BASE_2_Y, BASE_2_Z},
			   {BASE_3_X, BASE_3_Y, BASE_3_Z},
			   {BASE_4_X, BASE_4_Y, BASE_4_Z},
			   {BASE_5_X, BASE_5_Y, BASE_5_Z}};

static PmCartesian a[6] = {{PLATFORM_0_X, PLATFORM_0_Y, PLATFORM_0_Z},
			   {PLATFORM_1_X, PLATFORM_1_Y, PLATFORM_1_Z},
			   {PLATFORM_2_X, PLATFORM_2_Y, PLATFORM_2_Z},
			   {PLATFORM_3_X, PLATFORM_3_Y, PLATFORM_3_Z},
			   {PLATFORM_4_X, PLATFORM_4_Y, PLATFORM_4_Z},
			   {PLATFORM_5_X, PLATFORM_5_Y, PLATFORM_5_Z}};

const char * kinematicsGetName(void)
{
  return "genhex";
}

int kinematicsSetParameters(const double *params)
{
  int t;
  PmCartesian base[6];
  PmCartesian platform[6];

   for(t=0; t<6; t++)
    {
      base[t].x=params[t*3+0];
      base[t].y=params[t*3+1];
      base[t].z=params[t*3+2];
    }
  for(t=0; t<6; t++)
    {
      platform[t].x=params[18+t*3+0];
      platform[t].y=params[18+t*3+1];
      platform[t].z=params[18+t*3+2];
    }

  return genhexSetParams(base,platform);
}


int genhexSetParams(const PmCartesian base[], const PmCartesian platform[])
{
  int t;

  for (t = 0; t < 6; t++) {
    b[t] = base[t];
    a[t] = platform[t];
  }

  return 0;
}

int genhexGetParams(PmCartesian base[], PmCartesian platform[])
{
  int t;

  for (t = 0; t < 6; t++) {
    base[t] = b[t];
    platform[t] = b[t];
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
  velmatrix[0] = vel->tran.x;	/* dx/dt */
  velmatrix[1] = vel->tran.y;	/* dy/dt */
  velmatrix[2] = vel->tran.z;	/* dz/dt */
  velmatrix[3] = vel->a;	/* droll/dt */
  velmatrix[4] = vel->b;	/* dpitch/dt */
  velmatrix[5] = vel->c;	/* dyaw/dt */
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

static int iteration = 0;	/* global so we can report it */
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
  int retval = 0;

#define HIGH_CONV_CRITERION   (1e-12)
#define MEDIUM_CONV_CRITERION (1e-5)
#define LOW_CONV_CRITERION    (1e-3)
#define MEDIUM_CONV_ITERATIONS  50
#define LOW_CONV_ITERATIONS    100
#define FAIL_CONV_ITERATIONS   150
#define LARGE_CONV_ERROR 10000
  double conv_criterion = HIGH_CONV_CRITERION;

  iteration = 0;

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
    if ((conv_err > +LARGE_CONV_ERROR) || 
	(conv_err < -LARGE_CONV_ERROR)) {
      /* we can't converge */
      return -2;
    };

    iteration++;

#if 0
    /* if forward kinematics are having a difficult time converging
       ease the restrictions on the convergence criterion */
    if (iteration == MEDIUM_CONV_ITERATIONS) {
      conv_criterion = MEDIUM_CONV_CRITERION;
      retval = -3;		/* this means if we eventually converge,
				 the result is sloppy */
    }

    if (iteration == LOW_CONV_ITERATIONS) {
      conv_criterion = LOW_CONV_CRITERION;
      retval = -4;		/* this means if we eventually converge,
				 the result is even sloppier */
    }
#endif

    /* check iteration to see if the kinematics can reach the
       convergence criterion and return error flag if it can't */
    if (iteration > FAIL_CONV_ITERATIONS) {
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
      conv_err += rtapi_fabs(StrutLengthDiff[i]);
    }

    /* enter loop to determine if a strut needs another iteration */
    iterate = 0;			/*assume iteration is done */
    for (i = 0; i < NUM_STRUTS; i++) {
      if (rtapi_fabs(StrutLengthDiff[i]) > conv_criterion) {
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

  return retval;
}

int genhexKinematicsForwardIterations(void)
{
  return iteration;
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

/*
  kinematicsHome() is implemented by taking the desired world coordinates,
  which are passed as an arg, and running the inverse kinematics to get
  the resulting joints. The flags are set to zero.
*/
int kinematicsHome(EmcPose * world,
                   double * joint,
                   KINEMATICS_FORWARD_FLAGS * fflags,
                   KINEMATICS_INVERSE_FLAGS * iflags)
{
  *fflags = 0;
  *iflags = 0;

  return kinematicsInverse(world, joint, iflags, fflags);
}

KINEMATICS_TYPE kinematicsType(void)
{
#if 1
  return KINEMATICS_BOTH;
#else
  return KINEMATICS_INVERSE_ONLY;
#endif
}

#ifdef MAIN

#include <stdio.h>
/* FIXME-- this works for Unix only */
#include <sys/time.h>		/* struct timeval */
#include <unistd.h>		/* gettimeofday() */

static double timestamp(void)
{
  struct timeval tp;

  if (0 != gettimeofday(&tp, NULL)) {
    return 0.0;
  }
  return ((double) tp.tv_sec) + ((double) tp.tv_usec) / 1000000.0;
}

int main(int argc, char *argv[])
{
#define BUFFERLEN 256
  char buffer[BUFFERLEN];
  int inverse = 1;
  int jacobian = 0;
  EmcPose pos = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  EmcPose vel = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  double joints[6] = {0.0};
  double jointvels[6] = {0.0};
  KINEMATICS_INVERSE_FLAGS iflags = 0;
  KINEMATICS_FORWARD_FLAGS fflags = 0;
  int t;
  int retval;
#define ITERATIONS 100000
  double start, end;

  /* syntax is a.out {i|f # # # # # #} */
  if (argc == 8) {
    if (argv[1][0] == 'f') {
      /* joints passed, so do interations on forward kins for timing */
      for (t = 0; t < 6; t++) {
	if (1 != sscanf(argv[t + 2], "%lf", &joints[t])) {
	  fprintf(stderr, "bad value: %s\n", argv[t + 2]);
	  return 1;
	}
      }
      inverse = 0;
    }
    else if (argv[1][0] == 'i') {
      /* world coords passed, so do iterations on inverse kins for timing */
      if (1 != sscanf(argv[2], "%lf", &pos.tran.x)) {
	fprintf(stderr, "bad value: %s\n", argv[2]);
	return 1;
      }
      if (1 != sscanf(argv[3], "%lf", &pos.tran.y)) {
	fprintf(stderr, "bad value: %s\n", argv[3]);
	return 1;
      }
      if (1 != sscanf(argv[4], "%lf", &pos.tran.z)) {
	fprintf(stderr, "bad value: %s\n", argv[4]);
	return 1;
      }
      if (1 != sscanf(argv[5], "%lf", &pos.a)) {
	fprintf(stderr, "bad value: %s\n", argv[5]);
	return 1;
      }
      if (1 != sscanf(argv[6], "%lf", &pos.b)) {
	fprintf(stderr, "bad value: %s\n", argv[6]);
	return 1;
      }
      if (1 != sscanf(argv[7], "%lf", &pos.c)) {
	fprintf(stderr, "bad value: %s\n", argv[7]);
	return 1;
      }
      inverse = 1;
    }
    else {
      fprintf(stderr, "syntax: %s {i|f # # # # # #}\n", argv[0]);
      return 1;
    }

    /* need an initial estimate for the forward kins, so ask for it */
    if (inverse == 0) {
      do {
	printf("initial estimate for Cartesian position, xyzrpw: ");
	fflush(stdout);
	if (NULL == fgets(buffer, BUFFERLEN, stdin)) {
	  return 0;
	}
      } while (6 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf",
			   &pos.tran.x,
			   &pos.tran.y,
			   &pos.tran.z,
			   &pos.a,
			   &pos.b,
			   &pos.c));
    }

    start = timestamp();
    for (t = 0; t < ITERATIONS; t++) {
      if (inverse) {
	retval = kinematicsInverse(&pos, joints, &iflags, &fflags);
	if (0 != retval) {
	  printf("inv kins error %d\n", retval);
	  break;
	}
      }
      else {
	retval = kinematicsForward(joints, &pos, &fflags, &iflags);
	if (0 != retval) {
	  printf("fwd kins error %d\n", retval);
	  break;
	}
      }
    }
    end = timestamp();

    printf("calculation time: %f secs\n",
	   (end - start) / ((double) ITERATIONS));
    return 0;
  } /* end of if args for timestamping */

  /* else we're interactive */
  while (! feof(stdin)) {
    if (inverse) {
      if (jacobian) {
	printf("jinv> ");
      }
      else {
	printf("inv> ");
      }
    }
    else {
      if (jacobian) {
	printf("jfwd> ");
      }
      else {
	printf("fwd> ");
      }
    }
    fflush(stdout);

    if (NULL == fgets(buffer, BUFFERLEN, stdin)) {
      break;
    }

    if (buffer[0] == 'i') {
      inverse = 1;
      continue;
    }
    else if (buffer[0] == 'f') {
      inverse = 0;
      continue;
    }
    else if (buffer[0] == 'j') {
      jacobian = ! jacobian;
      continue;
    }
    else if (buffer[0] == 'q') {
      break;
    }

    if (inverse) {
      if (jacobian) {
	if (12 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&pos.tran.x,
			&pos.tran.y,
			&pos.tran.z,
			&pos.a,
			&pos.b,
			&pos.c,
			&vel.tran.x,
			&vel.tran.y,
			&vel.tran.z,
			&vel.a,
			&vel.b,
			&vel.c)) {
	  printf("?\n");
	}
	else {
	  retval = jacobianInverse(&pos, &vel, joints, jointvels);
	  printf("%f %f %f %f %f %f\n",
		 jointvels[0],
		 jointvels[1],
		 jointvels[2],
		 jointvels[3],
		 jointvels[4],
		 jointvels[5]);
	  if (0 != retval) {
	    printf("inv Jacobian error %d\n", retval);
	  }
	  else {
	    retval = jacobianForward(joints, jointvels, &pos, &vel);
	    printf("%f %f %f %f %f %f\n",
		   vel.tran.x,
		   vel.tran.y,
		   vel.tran.z,
		   vel.a,
		   vel.b,
		   vel.c);
	    if (0 != retval) {
	      printf("fwd kins error %d\n", retval);
	    }
	  }
	}
      }
      else {
	if (6 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf",
			&pos.tran.x,
			&pos.tran.y,
			&pos.tran.z,
			&pos.a,
			&pos.b,
			&pos.c)) {
	  printf("?\n");
	}
	else {
	  retval = kinematicsInverse(&pos, joints, &iflags, &fflags);
	  printf("%f %f %f %f %f %f\n",
		 joints[0],
		 joints[1],
		 joints[2],
		 joints[3],
		 joints[4],
		 joints[5]);
	  if (0 != retval) {
	    printf("inv kins error %d\n", retval);
	  }
	  else {
	    retval = kinematicsForward(joints, &pos, &fflags, &iflags);
	    printf("%f %f %f %f %f %f\n",
		   pos.tran.x,
		   pos.tran.y,
		   pos.tran.z,
		   pos.a,
		   pos.b,
		   pos.c);
	    if (0 != retval) {
	      printf("fwd kins error %d\n", retval);
	    }
	  }
	}
      }
    }
    else {
      if (jacobian) {
	if (12 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&joints[0],
			&joints[1],
			&joints[2],
			&joints[3],
			&joints[4],
			&joints[5],
			&jointvels[0],
			&jointvels[1],
			&jointvels[2],
			&jointvels[3],
			&jointvels[4],
			&jointvels[5])) {
	  printf("?\n");
	}
	else {
	  retval = jacobianForward(joints, jointvels, &pos, &vel);
	  printf("%f %f %f %f %f %f\n",
		 vel.tran.x,
		 vel.tran.y,
		 vel.tran.z,
		 vel.a,
		 vel.b,
		 vel.c);
	  if (0 != retval) {
	    printf("fwd kins error %d\n", retval);
	  }
	  else {
	    retval = jacobianInverse(&pos, &vel, joints, jointvels);
	    printf("%f %f %f %f %f %f\n",
		   jointvels[0],
		   jointvels[1],
		   jointvels[2],
		   jointvels[3],
		   jointvels[4],
		   jointvels[5]);
	    if (0 != retval) {
	      printf("inv kins error %d\n", retval);
	    }
	  }
	}
      }
      else {
	if (6 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf",
			&joints[0],
			&joints[1],
			&joints[2],
			&joints[3],
			&joints[4],
			&joints[5])) {
	  printf("?\n");
	}
	else {
	  retval = kinematicsForward(joints, &pos, &fflags, &iflags);
	  printf("%f %f %f %f %f %f\n",
		 pos.tran.x,
		 pos.tran.y,
		 pos.tran.z,
		 pos.a,
		 pos.b,
		 pos.c);
	  if (0 != retval) {
	    printf("fwd kins error %d\n", retval);
	  }
	  else {
	    retval = kinematicsInverse(&pos, joints, &iflags, &fflags);
	    printf("%f %f %f %f %f %f\n",
		   joints[0],
		   joints[1],
		   joints[2],
		   joints[3],
		   joints[4],
		   joints[5]);
	    if (0 != retval) {
	      printf("inv kins error %d\n", retval);
	    }
	  }
	}
      }
    }
  }

  return 0;

#undef ITERATIONS
#undef BUFFERLEN
}

#endif /* MAIN */

#ifdef RTAPI
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

MODULE_LICENSE("GPL");

static vtkins_t vtk = {
    .kinematicsForward = kinematicsForward,
    .kinematicsInverse  = kinematicsInverse,
    // .kinematicsHome = kinematicsHome,
    .kinematicsType = kinematicsType
};

static int comp_id, vtable_id;
static const char *name = "genhexkins";

int rtapi_app_main(void) {
    comp_id = hal_init(name);
    if(comp_id > 0) {
	vtable_id = hal_export_vtable(name, VTVERSION, &vtk, comp_id);
	if (vtable_id < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: hal_export_vtable(%s,%d,%p) failed: %d\n",
			    name, name, VTVERSION, &vtk, vtable_id );
	    return -ENOENT;
	}
	hal_ready(comp_id);
	return 0;
    }
    return comp_id;
}

void rtapi_app_exit(void)
{
    hal_remove_vtable(vtable_id);
    hal_exit(comp_id);
}
#endif
