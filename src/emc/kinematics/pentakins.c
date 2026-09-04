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
  machine can be adjusted by hal pins:

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

  The maths is written as pure functions of the parameter block (see
  kinematics.h): the pins above are the table below, read into the block
  before every call, and the entry points come from kins_single.c.

 ----------------------------------------------------------------------------*/

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>
#include <kinematics.h>             /* these decls, KINEMATICS_FORWARD_FLAGS */
#include <kins_rt.h>

#include "pentakins.h"

// the table: five struts' worth of geometry, then the iteration controls
// and reports.  P_BASE_X(i) and the rest index it.
#define P_BASE_X(i)  (5*(i) + 0)
#define P_BASE_Y(i)  (5*(i) + 1)
#define P_BASE_Z(i)  (5*(i) + 2)
#define P_EFF_R(i)   (5*(i) + 3)
#define P_EFF_Z(i)   (5*(i) + 4)
enum {
    P_LAST_ITER = 5*NUM_STRUTS,
    P_MAX_ITER,
    P_MAX_ERROR,
    P_CONV_CRITERION,
    P_ITER_LIMIT,
    P_TOOL_OFFSET,
    P_COUNT
};

#define STRUT_ROWS(i, bx, by, bz, er, ez) \
    { "base." #i ".x",     KINS_PARAM_FLOAT, KINS_IN, 0, bx }, \
    { "base." #i ".y",     KINS_PARAM_FLOAT, KINS_IN, 0, by }, \
    { "base." #i ".z",     KINS_PARAM_FLOAT, KINS_IN, 0, bz }, \
    { "effector." #i ".r", KINS_PARAM_FLOAT, KINS_IN, 0, er }, \
    { "effector." #i ".z", KINS_PARAM_FLOAT, KINS_IN, 0, ez }

static const kins_param_desc penta_params[P_COUNT] = {
    STRUT_ROWS(0, DEFAULT_BASE_0_X, DEFAULT_BASE_0_Y, DEFAULT_BASE_0_Z, DEFAULT_EFFECTOR_0_R, DEFAULT_EFFECTOR_0_Z),
    STRUT_ROWS(1, DEFAULT_BASE_1_X, DEFAULT_BASE_1_Y, DEFAULT_BASE_1_Z, DEFAULT_EFFECTOR_1_R, DEFAULT_EFFECTOR_1_Z),
    STRUT_ROWS(2, DEFAULT_BASE_2_X, DEFAULT_BASE_2_Y, DEFAULT_BASE_2_Z, DEFAULT_EFFECTOR_2_R, DEFAULT_EFFECTOR_2_Z),
    STRUT_ROWS(3, DEFAULT_BASE_3_X, DEFAULT_BASE_3_Y, DEFAULT_BASE_3_Z, DEFAULT_EFFECTOR_3_R, DEFAULT_EFFECTOR_3_Z),
    STRUT_ROWS(4, DEFAULT_BASE_4_X, DEFAULT_BASE_4_Y, DEFAULT_BASE_4_Z, DEFAULT_EFFECTOR_4_R, DEFAULT_EFFECTOR_4_Z),
    [P_LAST_ITER]      = { "last-iterations",       KINS_PARAM_U32,   KINS_OUT, 0, 0 },
    [P_MAX_ITER]       = { "max-iterations",        KINS_PARAM_U32,   KINS_OUT, 0, 0 },
    [P_MAX_ERROR]      = { "max-error",             KINS_PARAM_FLOAT, KINS_IO,  0, 100.0 },
    [P_CONV_CRITERION] = { "convergence-criterion", KINS_PARAM_FLOAT, KINS_IO,  0, 1e-9 },
    [P_ITER_LIMIT]     = { "limit-iterations",      KINS_PARAM_U32,   KINS_IO,  0, 120 },
    [P_TOOL_OFFSET]    = { "tool-offset",           KINS_PARAM_FLOAT, KINS_IN,  1, 0.0 },
};

// the most iterations a converged solution has taken this session, kept
// in the caller's scratch so each caller reports its own
#define MAX_ITER_SEEN(s) ((s)->aux[0])

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

/* the base and effector geometry of one call, taken from the block */
typedef struct {
    PmCartesian b[NUM_STRUTS];
    double za[NUM_STRUTS], ra[NUM_STRUTS];
} penta_geometry;

static void geometry_of(const kins_params *p, penta_geometry *g) {
    int t;
    const double tool_offset = p->tool.tran.z;
    for (t = 0; t < NUM_STRUTS; t++) {
        g->b[t].x = p->geometry[P_BASE_X(t)];
        g->b[t].y = p->geometry[P_BASE_Y(t)];
        g->b[t].z = p->geometry[P_BASE_Z(t)] + tool_offset;
        g->ra[t]  = p->geometry[P_EFF_R(t)];
        g->za[t]  = p->geometry[P_EFF_Z(t)] + tool_offset;
    }
}

/************************ InvKins() ********************************/

static int InvKins(const penta_geometry *g,
                   const double * coord,
                   double * struts)
{

  PmCartesian xyz, pmcoord, temp;
  PmRotationMatrix RMatrix, InvRMatrix;
  PmRpy rpy;
  int i;

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
    pmCartCartSub(&g->b[i], &pmcoord, &temp);
    pmMatInv(&RMatrix, &InvRMatrix);
    pmMatCartMult(&InvRMatrix, &temp, &xyz);

    /* define strut lengths */
    struts[i] = sqrt( sqr(xyz.z - g->za[i]) + sqr( sqrt(sqr(xyz.x) + sqr(xyz.y)) - g->ra[i]) );
  }

  return 0;
}


/**************************** penta_forward() ***************************/

static int penta_forward(const kins_params *p, kins_scratch *s,
                         const double * joints,
                         EmcPose * pos,
                         const KINEMATICS_FORWARD_FLAGS * fflags,
                         KINEMATICS_INVERSE_FLAGS * iflags)
{
  (void)fflags;
  (void)iflags;

  penta_geometry g;
  double Jacobian[NUM_STRUTS][NUM_STRUTS];
  double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
  double InvKinStrutLength[NUM_STRUTS], StrutLengthDiff[NUM_STRUTS];
  double delta[NUM_STRUTS];
  double jointdelta[NUM_STRUTS];
  double coord[NUM_STRUTS];
  double conv_err = 1.0;

  int iterate = 1;
  int i, j;
  unsigned iteration = 0;

  geometry_of(p, &g);

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
  const double max_error = p->geometry[P_MAX_ERROR];
  const unsigned iter_limit = (unsigned)p->geometry[P_ITER_LIMIT];
  const double conv_criterion = p->geometry[P_CONV_CRITERION];
  while (iterate) {
    /* check for large error and return error flag if no convergence */
    if ((conv_err > +(max_error)) ||
    (conv_err < -(max_error))) {
      /* we can't converge */
      s->failed = 1;
      return -2;
    };

    iteration++;

    /* check iteration to see if the kinematics can reach the
       convergence criterion and return error flag if it can't */
    if (iteration > iter_limit) {
      /* we can't converge */
      s->failed = 1;
      return -5;
    }

    /* compute StrutLengthDiff[] by running inverse kins on Cartesian
     estimate to get joint estimate, subtract joints to get joint deltas,
     and compute inv J while we're at it */
    InvKins(&g, coord, InvKinStrutLength);

    for (i = 0; i < NUM_STRUTS; i++) {
      StrutLengthDiff[i] = InvKinStrutLength[i] - joints[i];

      /* Build Inverse Jacobian Matrix */
      coord[i] += 1e-4;
      InvKins(&g, coord, jointdelta);
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

  s->iterations = iteration;
  s->failed = 0;
  s->out[P_LAST_ITER] = iteration;
  if (iteration > MAX_ITER_SEEN(s)) {
    MAX_ITER_SEEN(s) = iteration;
  }
  s->out[P_MAX_ITER] = MAX_ITER_SEEN(s);
  return 0;
}


/************************ penta_inverse() ********************************/
/* the inverse kinematics take world coordinates and determine joint values,
   given the inverse kinematics flags to resolve any ambiguities. The forward
   flags are set to indicate their value appropriate to the world coordinates
   passed in. */

static int penta_inverse(const kins_params *p, kins_scratch *s,
                         const EmcPose * pos,
                         double * joints,
                         const KINEMATICS_INVERSE_FLAGS * iflags,
                         KINEMATICS_FORWARD_FLAGS * fflags)
{
  (void)s;
  (void)iflags;
  (void)fflags;

  penta_geometry g;
  double coord[NUM_STRUTS];

  geometry_of(p, &g);

  coord[0] = pos->tran.x;
  coord[1] = pos->tran.y;
  coord[2] = pos->tran.z;
  coord[3] = pos->a * PM_PI / 180.0;
  coord[4] = pos->b * PM_PI / 180.0;

  if (0 != InvKins(&g, coord, joints)) {
    return -1;
  }

  return 0;
}

static int penta_jacobian(const kins_params *p, const double * joints,
                          const EmcPose * pos,
                          double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                          const KINEMATICS_INVERSE_FLAGS * iflags)
{
  penta_geometry g;
  PmRotationMatrix R;
  PmRpy rpy;
  PmCartesian P, d, xyz, wa, wb, dxyz[5];
  int i, j, a, col;

  (void)joints;
  (void)iflags;
  geometry_of(p, &g);
  for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
    for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
  }

  /* InvKins() differentiated.  The effector end of each strut is found in
     effector coordinates as xyz = R^T (b - P) with R = Ry(b) Rx(a), so a
     pose translation moves it by -R^T and a pose rotation about w moves
     it by -R^T (w x (b - P)); the strut length is then the distance from
     that point to the strut's pivot circle of radius ra at height za. */
  P = pos->tran;
  rpy.r = pos->a * PM_PI / 180.0;
  rpy.p = pos->b * PM_PI / 180.0;
  rpy.y = 0;
  pmRpyMatConvert(&rpy, &R);

  /* rotation axes for a and b, in world coordinates */
  wa.x = cos(rpy.p); wa.y = 0; wa.z = -sin(rpy.p);
  wb.x = 0;          wb.y = 1; wb.z = 0;

  for (i = 0; i < NUM_STRUTS; i++) {
    double rho, A, B, len;

    pmCartCartSub(&g.b[i], &P, &d);
    /* R^T d, written out since pmMatCartMult applies R */
    xyz.x = R.x.x*d.x + R.x.y*d.y + R.x.z*d.z;
    xyz.y = R.y.x*d.x + R.y.y*d.y + R.y.z*d.z;
    xyz.z = R.z.x*d.x + R.z.y*d.y + R.z.z*d.z;

    /* d xyz / d pose, one PmCartesian per pose column x y z a b */
    for (col = 0; col < 3; col++) {
      /* -R^T e_col, which is minus row col of R^T, i.e. minus column
         col of R read as a row of R^T */
      PmCartesian e = {0, 0, 0}, w;
      if (col == 0) e.x = 1; else if (col == 1) e.y = 1; else e.z = 1;
      w.x = -(R.x.x*e.x + R.x.y*e.y + R.x.z*e.z);
      w.y = -(R.y.x*e.x + R.y.y*e.y + R.y.z*e.z);
      w.z = -(R.z.x*e.x + R.z.y*e.y + R.z.z*e.z);
      dxyz[col] = w;
    }
    for (col = 3; col < 5; col++) {
      PmCartesian cr, w;
      pmCartCartCross(col == 3 ? &wa : &wb, &d, &cr);
      w.x = -(R.x.x*cr.x + R.x.y*cr.y + R.x.z*cr.z) * (PM_PI/180.0);
      w.y = -(R.y.x*cr.x + R.y.y*cr.y + R.y.z*cr.z) * (PM_PI/180.0);
      w.z = -(R.z.x*cr.x + R.z.y*cr.y + R.z.z*cr.z) * (PM_PI/180.0);
      dxyz[col] = w;
    }

    rho = sqrt(sqr(xyz.x) + sqr(xyz.y));
    A = xyz.z - g.za[i];
    B = rho - g.ra[i];
    len = sqrt(sqr(A) + sqr(B));
    if (len <= 0 || rho <= 0) { return -1; }
    for (col = 0; col < 5; col++) {
      jac[i][col] = (A*dxyz[col].z
                     + B*(xyz.x*dxyz[col].x + xyz.y*dxyz[col].y)/rho) / len;
    }
  }
  return 0;
}

// the forward iterates from the pose it is handed
static const kins_ops penta_ops = {
    .forward      = penta_forward,
    .inverse      = penta_inverse,
    .jacobian     = penta_jacobian,
    .fwd_iterates = 1,
};

const kins_module_info kins_module = {
    .name                 = "pentakins",
    .halprefix            = "pentakins",
    .params               = penta_params,
    .nparams              = P_COUNT,
    .required_coordinates = "XYZAB",
    .max_joints           = NUM_STRUTS,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &penta_ops },
};

MODULE_LICENSE("GPL");

int comp_id;

int rtapi_app_main(void)
{
    comp_id = hal_init("pentakins");
    if (comp_id < 0)
    return comp_id;

    if (kinsSingleInit(comp_id, "XYZAB", KINEMATICS_BOTH)) {
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}


void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
