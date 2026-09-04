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
*********************************************************************

  These are the forward and inverse kinematic functions for a class of
  machines referred to as "Stewart Platforms".

  The functions are general enough to be configured for any platform
  configuration.  In the functions "genhex_forward" and "genhex_inverse"
  are arrays "a[i]" and "b[i]".  The values stored in these arrays
  correspond to the positions of the ends of the i'th strut. The value
  stored in a[i] is the position of the end of the i'th strut attached
  to the platform, in platform coordinates. The value stored in b[i] is
  the position of the end of the i'th strut attached to the base, in
  base (world) coordinates.

  The default values for base and platform joints positions are defined
  in the header file genhexkins.h.  The actual values for a particular
  machine can be adjusted by hal pins:

  genhexkins.base.N.x
  genhexkins.base.N.y
  genhexkins.base.N.z - base joint coordinates.

  genhexkins.platform.N.x
  genhexkins.platform.N.y
  genhexkins.platform.N.z - platform joint coordinates.

  genhexkins.spindle-offset - added to Z coordinates of all joints to
                              change the machine origin. Facilitates
                              adjusting spindle position.

  genhexkins.tool-offset - tool length offset (TCP offset along Z),
                           implements RTCP function when connected to
                           motion.tooloffset.Z.

  To avoid joints jump change tool offset (G43, G49) only when the
  platform is not tilted (A = B = 0).

  Some hexapods use non-captive screw actuators and universal (cardanic)
  joints, thus the strut lengths depend on orientation of joints axes.
  Strut length correction is implemented to compensate for this.
  The calculations use orientation (unit vectors) of base and platform
  joint axes and the lead of actuator screws:

  genhexkins.base-n.N.x
  genhexkins.base-n.N.y
  genhexkins.base-n.N.z - unit vectors of base joint axes;

  genhexkins.platform-n.N.x
  genhexkins.platform-n.N.y
  genhexkins.platform-n.N.z - unit vectors of platform joint axes
                              in platform CS.
  genhexkins.screw-lead - lead of strut actuator screw, positive for
                          right-hand thread. Default is 0 (strut length
                          correction disabled).
  genhexkins.correction.N - pins showing current values of strut length
                            correction.

  The genhex_inverse function solves the inverse kinematics using
  a closed form algorithm.  The inverse kinematics problem is given
  the pose of the platform and returns the strut lengths. For this
  problem there is only one solution that is always returned correctly.

  The genhex_forward function solves the forward kinematics using
  an iterative algorithm.  Due to the iterative nature of this algorithm
  the genhex_forward function requires an initial value to begin the
  iterative routine and then converges to the "nearest" solution. The
  forward kinematics problem is given the strut lengths and returns the
  pose of the platform.  For this problem there arein multiple
  solutions.  The genhex_forward function will return only one of
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

  The maths is written as pure functions of the parameter block (see
  kinematics.h): the pins above are the table below, read into the block
  before every call and written from the scratch after it.

 ----------------------------------------------------------------------------*/

#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>
#include <emcmotcfg.h>

#include "genhexkins.h"
#include <switchkins.h>

// the table: thirteen entries per strut, then the iteration controls,
// the offsets and the reports.  The macros index it.
#define STRUT_ENTRIES 13
#define P_BASE_X(i)   (STRUT_ENTRIES*(i) + 0)
#define P_BASE_Y(i)   (STRUT_ENTRIES*(i) + 1)
#define P_BASE_Z(i)   (STRUT_ENTRIES*(i) + 2)
#define P_PLAT_X(i)   (STRUT_ENTRIES*(i) + 3)
#define P_PLAT_Y(i)   (STRUT_ENTRIES*(i) + 4)
#define P_PLAT_Z(i)   (STRUT_ENTRIES*(i) + 5)
#define P_BASE_NX(i)  (STRUT_ENTRIES*(i) + 6)
#define P_BASE_NY(i)  (STRUT_ENTRIES*(i) + 7)
#define P_BASE_NZ(i)  (STRUT_ENTRIES*(i) + 8)
#define P_PLAT_NX(i)  (STRUT_ENTRIES*(i) + 9)
#define P_PLAT_NY(i)  (STRUT_ENTRIES*(i) + 10)
#define P_PLAT_NZ(i)  (STRUT_ENTRIES*(i) + 11)
#define P_CORR(i)     (STRUT_ENTRIES*(i) + 12)
enum {
    P_LAST_ITER = STRUT_ENTRIES*NUM_STRUTS,
    P_MAX_ITER,
    P_MAX_ERROR,
    P_CONV_CRITERION,
    P_ITER_LIMIT,
    P_TOOL_OFFSET,
    P_SPINDLE_OFFSET,
    P_SCREW_LEAD,
    P_GUI_X, P_GUI_Y, P_GUI_Z, P_GUI_A, P_GUI_B, P_GUI_C,
    P_FWD_FAIL,
    P_COUNT
};

#define STRUT_ROWS(i, bx, by, bz, px, py, pz, bnx, bny, bnz, pnx, pny, pnz) \
    { "base." #i ".x",       KINS_PARAM_FLOAT, KINS_IN,  0, bx }, \
    { "base." #i ".y",       KINS_PARAM_FLOAT, KINS_IN,  0, by }, \
    { "base." #i ".z",       KINS_PARAM_FLOAT, KINS_IN,  0, bz }, \
    { "platform." #i ".x",   KINS_PARAM_FLOAT, KINS_IN,  0, px }, \
    { "platform." #i ".y",   KINS_PARAM_FLOAT, KINS_IN,  0, py }, \
    { "platform." #i ".z",   KINS_PARAM_FLOAT, KINS_IN,  0, pz }, \
    { "base-n." #i ".x",     KINS_PARAM_FLOAT, KINS_IN,  0, bnx }, \
    { "base-n." #i ".y",     KINS_PARAM_FLOAT, KINS_IN,  0, bny }, \
    { "base-n." #i ".z",     KINS_PARAM_FLOAT, KINS_IN,  0, bnz }, \
    { "platform-n." #i ".x", KINS_PARAM_FLOAT, KINS_IN,  0, pnx }, \
    { "platform-n." #i ".y", KINS_PARAM_FLOAT, KINS_IN,  0, pny }, \
    { "platform-n." #i ".z", KINS_PARAM_FLOAT, KINS_IN,  0, pnz }, \
    { "correction." #i,      KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 }

static const kins_param_desc genhex_params[P_COUNT] = {
    STRUT_ROWS(0, DEFAULT_BASE_0_X, DEFAULT_BASE_0_Y, DEFAULT_BASE_0_Z,
                  DEFAULT_PLATFORM_0_X, DEFAULT_PLATFORM_0_Y, DEFAULT_PLATFORM_0_Z,
                  DEFAULT_BASE_0_NX, DEFAULT_BASE_0_NY, DEFAULT_BASE_0_NZ,
                  DEFAULT_PLATFORM_0_NX, DEFAULT_PLATFORM_0_NY, DEFAULT_PLATFORM_0_NZ),
    STRUT_ROWS(1, DEFAULT_BASE_1_X, DEFAULT_BASE_1_Y, DEFAULT_BASE_1_Z,
                  DEFAULT_PLATFORM_1_X, DEFAULT_PLATFORM_1_Y, DEFAULT_PLATFORM_1_Z,
                  DEFAULT_BASE_1_NX, DEFAULT_BASE_1_NY, DEFAULT_BASE_1_NZ,
                  DEFAULT_PLATFORM_1_NX, DEFAULT_PLATFORM_1_NY, DEFAULT_PLATFORM_1_NZ),
    STRUT_ROWS(2, DEFAULT_BASE_2_X, DEFAULT_BASE_2_Y, DEFAULT_BASE_2_Z,
                  DEFAULT_PLATFORM_2_X, DEFAULT_PLATFORM_2_Y, DEFAULT_PLATFORM_2_Z,
                  DEFAULT_BASE_2_NX, DEFAULT_BASE_2_NY, DEFAULT_BASE_2_NZ,
                  DEFAULT_PLATFORM_2_NX, DEFAULT_PLATFORM_2_NY, DEFAULT_PLATFORM_2_NZ),
    STRUT_ROWS(3, DEFAULT_BASE_3_X, DEFAULT_BASE_3_Y, DEFAULT_BASE_3_Z,
                  DEFAULT_PLATFORM_3_X, DEFAULT_PLATFORM_3_Y, DEFAULT_PLATFORM_3_Z,
                  DEFAULT_BASE_3_NX, DEFAULT_BASE_3_NY, DEFAULT_BASE_3_NZ,
                  DEFAULT_PLATFORM_3_NX, DEFAULT_PLATFORM_3_NY, DEFAULT_PLATFORM_3_NZ),
    STRUT_ROWS(4, DEFAULT_BASE_4_X, DEFAULT_BASE_4_Y, DEFAULT_BASE_4_Z,
                  DEFAULT_PLATFORM_4_X, DEFAULT_PLATFORM_4_Y, DEFAULT_PLATFORM_4_Z,
                  DEFAULT_BASE_4_NX, DEFAULT_BASE_4_NY, DEFAULT_BASE_4_NZ,
                  DEFAULT_PLATFORM_4_NX, DEFAULT_PLATFORM_4_NY, DEFAULT_PLATFORM_4_NZ),
    STRUT_ROWS(5, DEFAULT_BASE_5_X, DEFAULT_BASE_5_Y, DEFAULT_BASE_5_Z,
                  DEFAULT_PLATFORM_5_X, DEFAULT_PLATFORM_5_Y, DEFAULT_PLATFORM_5_Z,
                  DEFAULT_BASE_5_NX, DEFAULT_BASE_5_NY, DEFAULT_BASE_5_NZ,
                  DEFAULT_PLATFORM_5_NX, DEFAULT_PLATFORM_5_NY, DEFAULT_PLATFORM_5_NZ),
    [P_LAST_ITER]      = { "last-iterations",       KINS_PARAM_U32,   KINS_OUT, 0, 0 },
    [P_MAX_ITER]       = { "max-iterations",        KINS_PARAM_U32,   KINS_OUT, 0, 0 },
    [P_MAX_ERROR]      = { "max-error",             KINS_PARAM_FLOAT, KINS_IN,  0, 500.0 },
    [P_CONV_CRITERION] = { "convergence-criterion", KINS_PARAM_FLOAT, KINS_IN,  0, 1e-9 },
    [P_ITER_LIMIT]     = { "limit-iterations",      KINS_PARAM_U32,   KINS_IN,  0, 120 },
    [P_TOOL_OFFSET]    = { "tool-offset",           KINS_PARAM_FLOAT, KINS_IN,  1, 0.0 },
    [P_SPINDLE_OFFSET] = { "spindle-offset",        KINS_PARAM_FLOAT, KINS_IN,  0, 0.0 },
    [P_SCREW_LEAD]     = { "screw-lead",            KINS_PARAM_FLOAT, KINS_IN,  0, DEFAULT_SCREW_LEAD },
    // the pose the forward found, for a vismach gui; switchkins provides
    // the skgui.* pins for the same purpose
    [P_GUI_X]          = { "x",                     KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    [P_GUI_Y]          = { "y",                     KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    [P_GUI_Z]          = { "z",                     KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    [P_GUI_A]          = { "a",                     KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    [P_GUI_B]          = { "b",                     KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    [P_GUI_C]          = { "c",                     KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    [P_FWD_FAIL]       = { "fwd-kins-fail",         KINS_PARAM_BIT,   KINS_OUT, 0, 0 },
};

// the most iterations a converged solution has taken this session, kept
// in the caller's scratch so each caller reports its own
#define MAX_ITER_SEEN(s) ((s)->aux[0])

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
} // MatInvert()

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
} // MatMult()

/* the geometry of one call, taken from the block: base and platform
   coordinates, the joint axes vectors and the screw lead */
typedef struct {
    PmCartesian b[NUM_STRUTS];
    PmCartesian a[NUM_STRUTS];
    PmCartesian nb1[NUM_STRUTS];
    PmCartesian na0[NUM_STRUTS];
    double screw_lead;
} genhex_geometry;

static void geometry_of(const kins_params *p, genhex_geometry *g) {
    int t;

  /* set the base and platform coordinates from the block */
    const double spindle_offset = p->geometry[P_SPINDLE_OFFSET];
    const double tool_offset = p->tool.tran.z;
    for (t = 0; t < NUM_STRUTS; t++) {
        g->b[t].x   = p->geometry[P_BASE_X(t)];
        g->b[t].y   = p->geometry[P_BASE_Y(t)];
        g->b[t].z   = p->geometry[P_BASE_Z(t)] + spindle_offset + tool_offset;
        g->a[t].x   = p->geometry[P_PLAT_X(t)];
        g->a[t].y   = p->geometry[P_PLAT_Y(t)];
        g->a[t].z   = p->geometry[P_PLAT_Z(t)] + spindle_offset + tool_offset;

        g->nb1[t].x = p->geometry[P_BASE_NX(t)];
        g->nb1[t].y = p->geometry[P_BASE_NY(t)];
        g->nb1[t].z = p->geometry[P_BASE_NZ(t)];
        g->na0[t].x = p->geometry[P_PLAT_NX(t)];
        g->na0[t].y = p->geometry[P_PLAT_NY(t)];
        g->na0[t].z = p->geometry[P_PLAT_NZ(t)];

    }
    g->screw_lead = p->geometry[P_SCREW_LEAD];
} // geometry_of()

/***************************StrutLengthCorrection***************************/

static int StrutLengthCorrection(const genhex_geometry *g,
                                 const PmCartesian * StrutVectUnit,
                                 const PmRotationMatrix * RMatrix,
                                 const int strut_number,
                                 double * correction)
{
  PmCartesian nb2, nb3, na1, na2;
  double dotprod;

  /* define base joints axis vectors */
  pmCartCartCross(&g->nb1[strut_number], StrutVectUnit, &nb2);
  pmCartCartCross(StrutVectUnit, &nb2, &nb3);
  pmCartUnitEq(&nb3);

  /* define platform joints axis vectors */
  pmMatCartMult(RMatrix, &g->na0[strut_number], &na1);
  pmCartCartCross(&na1, StrutVectUnit, &na2);
  pmCartUnitEq(&na2);

  /* define dot product */
  pmCartCartDot(&nb3, &na2, &dotprod);

  *correction = g->screw_lead * asin(dotprod) / PM_2_PI;

  return 0;
} // StrutLengthCorrection()


/**************** genhex_forward() *****************/
static int genhex_forward(const kins_params *p, kins_scratch *s,
                          const double * joints,
                          EmcPose * pos,
                          const KINEMATICS_FORWARD_FLAGS * fflags,
                          KINEMATICS_INVERSE_FLAGS * iflags)
{
  (void)fflags;
  (void)iflags;
  genhex_geometry g;
  PmCartesian aw;
  PmCartesian InvKinStrutVect,InvKinStrutVectUnit;
  PmCartesian q_trans, RMatrix_a, RMatrix_a_cross_Strut;

  double Jacobian[NUM_STRUTS][NUM_STRUTS];
  double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
  double InvKinStrutLength, StrutLengthDiff[NUM_STRUTS];
  double delta[NUM_STRUTS];
  double conv_err = 1.0;
  double corr;

  PmRotationMatrix RMatrix;
  PmRpy q_RPY;

  int iterate = 1;
  int i;
  unsigned iteration = 0;

  geometry_of(p, &g);

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
  const double max_error = p->geometry[P_MAX_ERROR];
  const unsigned iter_limit = (unsigned)p->geometry[P_ITER_LIMIT];
  const double conv_criterion = p->geometry[P_CONV_CRITERION];
  while (iterate) {
    /* check for large error and return error flag if no convergence */
    if ((conv_err > +max_error) ||
        (conv_err < -max_error)) {
      /* we can't converge */
      s->failed = 1;
      s->out[P_FWD_FAIL] = 1;
      return -2;
    };

    iteration++;

    /* check iteration to see if the kinematics can reach the
       convergence criterion and return error flag if it can't */
    if (iteration > iter_limit) {
      /* we can't converge */
      s->failed = 1;
      s->out[P_FWD_FAIL] = 1;
      return -5;
    }

    /* Convert q_RPY to Rotation Matrix */
    pmRpyMatConvert(&q_RPY, &RMatrix);

    /* compute StrutLengthDiff[] by running inverse kins on Cartesian
     estimate to get joint estimate, subtract joints to get joint deltas,
     and compute inv J while we're at it */
    for (i = 0; i < NUM_STRUTS; i++) {
      pmMatCartMult(&RMatrix, &g.a[i], &RMatrix_a);
      pmCartCartAdd(&q_trans, &RMatrix_a, &aw);
      pmCartCartSub(&aw, &g.b[i], &InvKinStrutVect);
      if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit)) {
        s->failed = 1;
        s->out[P_FWD_FAIL] = 1;
        return -1;
      }
      pmCartMag(&InvKinStrutVect, &InvKinStrutLength);

      if (g.screw_lead != 0.0) {
        /* enable strut length correction */
        StrutLengthCorrection(&g, &InvKinStrutVectUnit, &RMatrix, i, &corr);
        /* define corrected joint lengths */
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
      if (fabs(StrutLengthDiff[i]) > conv_criterion) {
    iterate = 1;
      }
    }
  } /* exit Newton-Raphson Iterative loop */

  /* assign r,p,y to a,b,c */
  pos->a = q_RPY.r * 180.0 / PM_PI;
  pos->b = q_RPY.p * 180.0 / PM_PI;
  pos->c = q_RPY.y * 180.0 / PM_PI;

  /* assign q_trans to pos */
  pos->tran.x = q_trans.x;
  pos->tran.y = q_trans.y;
  pos->tran.z = q_trans.z;

  s->iterations = iteration;
  s->failed = 0;
  s->out[P_LAST_ITER] = iteration;
  if (iteration > MAX_ITER_SEEN(s)) {
    MAX_ITER_SEEN(s) = iteration;
  }
  s->out[P_MAX_ITER] = MAX_ITER_SEEN(s);
  s->out[P_FWD_FAIL] = 0;

  s->out[P_GUI_X] = pos->tran.x;
  s->out[P_GUI_Y] = pos->tran.y;
  s->out[P_GUI_Z] = pos->tran.z;
  s->out[P_GUI_A] = pos->a;
  s->out[P_GUI_B] = pos->b;
  s->out[P_GUI_C] = pos->c;

  return 0;
} // genhex_forward()


/************************ genhex_inverse() ************************/
/* the inverse kinematics take world coordinates and determine joint values,
   given the inverse kinematics flags to resolve any ambiguities. The forward
   flags are set to indicate their value appropriate to the world coordinates
   passed in. */

static int genhex_inverse(const kins_params *p, kins_scratch *s,
                          const EmcPose * pos,
                          double * joints,
                          const KINEMATICS_INVERSE_FLAGS * iflags,
                          KINEMATICS_FORWARD_FLAGS * fflags)
{
  (void)iflags;
  (void)fflags;

  genhex_geometry g;
  PmCartesian aw, temp;
  PmCartesian InvKinStrutVect, InvKinStrutVectUnit;
  PmRotationMatrix RMatrix;
  PmRpy rpy;
  int i;
  double InvKinStrutLength, corr;

  geometry_of(p, &g);

  /* define Rotation Matrix */
  rpy.r = pos->a * PM_PI / 180.0;
  rpy.p = pos->b * PM_PI / 180.0;
  rpy.y = pos->c * PM_PI / 180.0;
  pmRpyMatConvert(&rpy, &RMatrix);

  /* enter for loop to calculate joints (strut lengths) */
  for (i = 0; i < NUM_STRUTS; i++) {
    /* convert location of platform strut end from platform
       to world coordinates */
    pmMatCartMult(&RMatrix, &g.a[i], &temp);
    pmCartCartAdd(&pos->tran, &temp, &aw);

    /* define strut lengths */
    pmCartCartSub(&aw, &g.b[i], &InvKinStrutVect);
    pmCartMag(&InvKinStrutVect, &InvKinStrutLength);

    if (g.screw_lead != 0.0) {
      /* enable strut length correction */
      /* define unit strut vector */
      if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit)) {
          return -1;
      }
      /* define correction value and corrected joint lengths */
      StrutLengthCorrection(&g, &InvKinStrutVectUnit, &RMatrix, i, &corr);
      s->out[P_CORR(i)] = corr;
      InvKinStrutLength += corr;
    }

    joints[i] = InvKinStrutLength;
  }

  return 0;
} //genhex_inverse()

/************************ genhex_jacobian() ***********************/
/* A strut length changes by the component of its platform end's motion
   along the strut.  That end moves with the platform, dP + w x (R a), so
   the row for strut i is [u_i, (R a_i x u_i) . E] with u_i the unit strut
   vector and E the matrix taking the rates of the roll, pitch and yaw
   words to the angular velocity w for R = Rz(c) Ry(b) Rx(a).  The forward
   kinematics builds the same rows for its Newton step, in radians. */

// the inverse alone, for differencing where the closed form does not apply
static const kins_ops genhex_diff_ops = {
    .forward = genhex_forward,
    .inverse = genhex_inverse,
};

static int genhex_jacobian(const kins_params *p, const double * joints,
                           const EmcPose * pos,
                           double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                           const KINEMATICS_INVERSE_FLAGS * iflags)
{
  genhex_geometry g;
  PmCartesian aw, RMatrix_a, strut, u, moment;
  PmRotationMatrix RMatrix;
  PmRpy rpy;
  PmCartesian E[3];
  double sb, cb, sc, cc;
  int i, j, col, m;

  geometry_of(p, &g);

  /* the screw lead correction is a function of the pose too, and this
     does not differentiate it; difference the inverse instead */
  if (g.screw_lead != 0.0) {
    kins_scratch scratch;
    kinsScratchInit(&scratch);
    return kinsOpsJacobian(&genhex_diff_ops, p, &scratch, joints, pos, jac, iflags);
  }

  for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
    for (col = 0; col < EMCMOT_MAX_AXIS; col++) { jac[j][col] = 0; }
  }

  rpy.r = pos->a * PM_PI / 180.0;
  rpy.p = pos->b * PM_PI / 180.0;
  rpy.y = pos->c * PM_PI / 180.0;
  pmRpyMatConvert(&rpy, &RMatrix);

  /* w = E [da db dc]: the roll axis carried by pitch and yaw, the pitch
     axis carried by yaw, and the yaw axis fixed */
  sb = sin(rpy.p); cb = cos(rpy.p);
  sc = sin(rpy.y); cc = cos(rpy.y);
  E[0].x = cb*cc; E[0].y = cb*sc; E[0].z = -sb;
  E[1].x = -sc;   E[1].y = cc;    E[1].z = 0;
  E[2].x = 0;     E[2].y = 0;     E[2].z = 1;

  for (i = 0; i < NUM_STRUTS; i++) {
    double len;

    pmMatCartMult(&RMatrix, &g.a[i], &RMatrix_a);
    pmCartCartAdd(&pos->tran, &RMatrix_a, &aw);
    pmCartCartSub(&aw, &g.b[i], &strut);
    pmCartMag(&strut, &len);
    if (len <= 0) { return -1; }
    pmCartScalMult(&strut, 1.0/len, &u);
    pmCartCartCross(&RMatrix_a, &u, &moment);

    jac[i][0] = u.x;
    jac[i][1] = u.y;
    jac[i][2] = u.z;
    for (m = 0; m < 3; m++) {
      double dot;
      pmCartCartDot(&moment, &E[m], &dot);
      jac[i][3+m] = dot * PM_PI / 180.0;
    }
  }
  return 0;
} // genhex_jacobian()

// the forward iterates from the pose it is handed, so it is seeded with
// the last answer after a switch
static const kins_ops genhex_ops = {
    .forward      = genhex_forward,
    .inverse      = genhex_inverse,
    .jacobian     = genhex_jacobian,
    .fwd_iterates = 1,
};

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    (void)kset0; (void)kset1; (void)kset2;
    (void)kfwd0; (void)kfwd1; (void)kfwd2;
    (void)kinv0; (void)kinv1; (void)kinv2;
    kp->kinsname    = "genhexkins"; // !!! must agree with filename
    kp->halprefix   = "genhexkins"; // hal pin names
    kp->required_coordinates = "xyzabc";
    kp->max_joints  = strlen(kp->required_coordinates);
    kp->allow_duplicates  = 0;
    kp->fwd_iterates_mask = 0x1; //genhexkins switchkins_type==0
    kp->gui_kinstype      = 0;   //vismach gui for switchkins_type==0
    kp->params            = genhex_params;
    kp->nparams           = P_COUNT;

    // switchkins_type==0 is startup default
    // kins with iterative forward algorithm should be switchkins_type==0
    switchkinsRegisterOps(0, &genhex_ops);
    switchkinsRegisterOps(1, &KINS_IDENTITY_OPS);
    switchkinsRegisterOps(2, &USERK_OPS);

    return 0;
} //switchkinsSetup()
