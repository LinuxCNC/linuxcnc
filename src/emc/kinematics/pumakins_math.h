/********************************************************************
 * Description: pumakins_math.h
 *   Pure math functions for PUMA robot kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef PUMAKINS_MATH_H
#define PUMAKINS_MATH_H

#include "emcpos.h"
#include "posemath.h"

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif

#define SINGULAR_FUZZ 0.000001
#define FLAG_FUZZ     0.000001

/* flags for inverse kinematics */
#define PUMA_SHOULDER_RIGHT 0x01
#define PUMA_ELBOW_DOWN     0x02
#define PUMA_WRIST_FLIP     0x04
#define PUMA_SINGULAR       0x08  /* joints at a singularity */

/* flags for forward kinematics */
#define PUMA_REACH          0x01  /* pose out of reach */

/* Parameters struct - matches kinematics_params.h kins_puma_params_t */
typedef struct {
    double a2;  /* Link length A2 */
    double a3;  /* Link length A3 */
    double d3;  /* Link offset D3 */
    double d4;  /* Link offset D4 */
    double d6;  /* Tool offset D6 */
} puma_params_t;

/*
 * Pure forward kinematics - joints to world coordinates
 *
 * params: kinematics parameters (a2, a3, d3, d4, d6)
 * joints: input joint positions array (6 joints, in degrees)
 * world: output world position (EmcPose)
 * iflags: output inverse kinematics flags (can be NULL)
 *
 * Returns 0 on success
 */
static inline int puma_forward_math(const puma_params_t *params,
                                     const double *joints,
                                     EmcPose *world,
                                     int *iflags)
{
   double s1, s2, s3, s4, s5, s6;
   double c1, c2, c3, c4, c5, c6;
   double s23;
   double c23;
   double t1, t2, t3, t4, t5;
   double sumSq, k;
   PmHomogeneous hom;
   PmPose worldPose;
   PmRpy rpy;
   int flags = 0;

   /* Calculate sin of joints for future use */
   s1 = sin(joints[0]*PM_PI/180.0);
   s2 = sin(joints[1]*PM_PI/180.0);
   s3 = sin(joints[2]*PM_PI/180.0);
   s4 = sin(joints[3]*PM_PI/180.0);
   s5 = sin(joints[4]*PM_PI/180.0);
   s6 = sin(joints[5]*PM_PI/180.0);

   /* Calculate cos of joints for future use */
   c1 = cos(joints[0]*PM_PI/180.0);
   c2 = cos(joints[1]*PM_PI/180.0);
   c3 = cos(joints[2]*PM_PI/180.0);
   c4 = cos(joints[3]*PM_PI/180.0);
   c5 = cos(joints[4]*PM_PI/180.0);
   c6 = cos(joints[5]*PM_PI/180.0);

   s23 = c2 * s3 + s2 * c3;
   c23 = c2 * c3 - s2 * s3;

   /* Calculate terms to be used in definition of... */
   /* first column of rotation matrix.               */
   t1 = c4 * c5 * c6 - s4 * s6;
   t2 = s23 * s5 * c6;
   t3 = s4 * c5 * c6 + c4 * s6;
   t4 = c23 * t1 - t2;
   t5 = c23 * s5 * c6;

   /* Define first column of rotation matrix */
   hom.rot.x.x = c1 * t4 + s1 * t3;
   hom.rot.x.y = s1 * t4 - c1 * t3;
   hom.rot.x.z = -s23 * t1 - t5;

   /* Calculate terms to be used in definition of...  */
   /* second column of rotation matrix.               */
   t1 = -c4 * c5 * s6 - s4 * c6;
   t2 = s23 * s5 * s6;
   t3 = c4 * c6 - s4 * c5 * s6;
   t4 = c23 * t1 + t2;
   t5 = c23 * s5 * s6;

   /* Define second column of rotation matrix */
   hom.rot.y.x = c1 * t4 + s1 * t3;
   hom.rot.y.y = s1 * t4 - c1 * t3;
   hom.rot.y.z = -s23 * t1 + t5;

   /* Calculate term to be used in definition of... */
   /* third column of rotation matrix.              */
   t1 = c23 * c4 * s5 + s23 * c5;

   /* Define third column of rotation matrix */
   hom.rot.z.x = -c1 * t1 - s1 * s4 * s5;
   hom.rot.z.y = -s1 * t1 + c1 * s4 * s5;
   hom.rot.z.z = s23 * c4 * s5 - c23 * c5;

   /* Calculate term to be used in definition of...  */
   /* position vector.                               */
   t1 = params->a2 * c2 + params->a3 * c23 - params->d4 * s23;

   /* Define position vector */
   hom.tran.x = c1 * t1 - params->d3 * s1;
   hom.tran.y = s1 * t1 + params->d3 * c1;
   hom.tran.z = -params->a3 * s23 - params->a2 * s2 - params->d4 * c23;

   /* Calculate terms to be used to...   */
   /* determine flags.                   */
   sumSq = hom.tran.x * hom.tran.x + hom.tran.y * hom.tran.y -
           params->d3 * params->d3;
   k = (sumSq + hom.tran.z * hom.tran.z - params->a2 * params->a2 -
       params->a3 * params->a3 - params->d4 * params->d4) /
       (2.0 * params->a2);

   /* Set shoulder-up flag if necessary */
   if (fabs(joints[0]*PM_PI/180.0 - atan2(hom.tran.y, hom.tran.x) +
       atan2(params->d3, -sqrt(sumSq))) < FLAG_FUZZ)
   {
     flags |= PUMA_SHOULDER_RIGHT;
   }

   /* Set elbow down flag if necessary */
   if (fabs(joints[2]*PM_PI/180.0 - atan2(params->a3, params->d4) +
       atan2(k, -sqrt(params->a3 * params->a3 +
       params->d4 * params->d4 - k * k))) < FLAG_FUZZ)
   {
      flags |= PUMA_ELBOW_DOWN;
   }

   /* set singular flag if necessary */
   t1 = -hom.rot.z.x * s1 + hom.rot.z.y * c1;
   t2 = -hom.rot.z.x * c1 * c23 - hom.rot.z.y * s1 * c23 +
         hom.rot.z.z * s23;
   if (fabs(t1) < SINGULAR_FUZZ && fabs(t2) < SINGULAR_FUZZ)
   {
      flags |= PUMA_SINGULAR;
   }

   /* if not singular set wrist flip flag if necessary */
   else{
     if (! (fabs(joints[3]*PM_PI/180.0 - atan2(t1, t2)) < FLAG_FUZZ))
     {
       flags |= PUMA_WRIST_FLIP;
     }
   }

   /*  add effect of d6 parameter */
   hom.tran.x = hom.tran.x + hom.rot.z.x*params->d6;
   hom.tran.y = hom.tran.y + hom.rot.z.y*params->d6;
   hom.tran.z = hom.tran.z + hom.rot.z.z*params->d6;

   /* convert hom.rot to world->quat */
   pmHomPoseConvert(&hom, &worldPose);
   pmQuatRpyConvert(&worldPose.rot,&rpy);
   world->tran = worldPose.tran;
   world->a = rpy.r * 180.0/PM_PI;
   world->b = rpy.p * 180.0/PM_PI;
   world->c = rpy.y * 180.0/PM_PI;
   world->u = 0.0;
   world->v = 0.0;
   world->w = 0.0;

   /* Store flags if requested */
   if (iflags != NULL) {
       *iflags = flags;
   }

   return 0;
}

/*
 * Pure inverse kinematics - world coordinates to joints
 *
 * params: kinematics parameters (a2, a3, d3, d4, d6)
 * world: input world position (EmcPose)
 * joints: output joint positions array (6 joints, in degrees)
 * current_joints: current joint positions for singularity resolution (can be NULL)
 * iflags: input inverse kinematics flags (shoulder/elbow/wrist configuration)
 * fflags: output forward kinematics flags (can be NULL)
 *
 * Returns 0 on success, -1 on failure
 */
static inline int puma_inverse_math(const puma_params_t *params,
                                     const EmcPose *world,
                                     double *joints,
                                     const double *current_joints,
                                     int iflags,
                                     int *fflags)
{
   PmHomogeneous hom;
   PmPose worldPose;
   PmRpy rpy;

   double t1, t2, t3;
   double k;
   double sumSq;

   double th1;
   double th3;
   double th23;
   double th2;
   double th4;
   double th5;
   double th6;

   double s1, c1;
   double s3, c3;
   double s23, c23;
   double s4, c4;
   double s5, c5;
   double s6, c6;
   double px, py, pz;
   int flags = 0;

   /* convert pose to hom */
   worldPose.tran = world->tran;
   rpy.r = world->a*PM_PI/180.0;
   rpy.p = world->b*PM_PI/180.0;
   rpy.y = world->c*PM_PI/180.0;
   pmRpyQuatConvert(&rpy,&worldPose.rot);
   pmPoseHomConvert(&worldPose, &hom);

   /* remove effect of d6 parameter */
   px = hom.tran.x - params->d6*hom.rot.z.x;
   py = hom.tran.y - params->d6*hom.rot.z.y;
   pz = hom.tran.z - params->d6*hom.rot.z.z;

   /* Joint 1 (2 independent solutions) */

   /* save sum of squares for this and subsequent calcs */
   sumSq = px * px + py * py - params->d3 * params->d3;

   if (iflags & PUMA_SHOULDER_RIGHT){
     th1 = atan2(py, px) - atan2(params->d3, -sqrt(sumSq));
   }
   else{
     th1 = atan2(py, px) - atan2(params->d3, sqrt(sumSq));
   }

   /* save sin, cos for later calcs */
   s1 = sin(th1);
   c1 = cos(th1);

   /* Joint 3 (2 independent solutions) */

   k = (sumSq + pz * pz - params->a2 * params->a2 -
       params->a3 * params->a3 - params->d4 * params->d4) / (2.0 * params->a2);

   if (iflags & PUMA_ELBOW_DOWN){
     th3 = atan2(params->a3, params->d4) - atan2(k, -sqrt(params->a3 * params->a3 + params->d4 * params->d4 - k * k));
   }
   else{
     th3 = atan2(params->a3, params->d4) -
           atan2(k, sqrt(params->a3 * params->a3 + params->d4 * params->d4 - k * k));
   }

   /* compute sin, cos for later calcs */
   s3 = sin(th3);
   c3 = cos(th3);

   /* Joint 2 */

   t1 = (-params->a3 - params->a2 * c3) * pz +
        (c1 * px + s1 * py) * (params->a2 * s3 - params->d4);
   t2 = (params->a2 * s3 - params->d4) * pz +
        (params->a3 + params->a2 * c3) * (c1 * px + s1 * py);
   t3 = pz * pz + (c1 * px + s1 * py) *
        (c1 * px + s1 * py);

   th23 = atan2(t1, t2);
   th2 = th23 - th3;

   /* compute sin, cos for later calcs */
   s23 = t1 / t3;
   c23 = t2 / t3;

   /* Joint 4 */

   t1 = -hom.rot.z.x * s1 + hom.rot.z.y * c1;
   t2 = -hom.rot.z.x * c1 * c23 - hom.rot.z.y * s1 * c23 + hom.rot.z.z * s23;
   if (fabs(t1) < SINGULAR_FUZZ && fabs(t2) < SINGULAR_FUZZ){
     flags |= PUMA_REACH;
     /* use current value if available */
     th4 = (current_joints != NULL) ? current_joints[3]*PM_PI/180.0 : 0.0;
   }
   else{
     th4 = atan2(t1, t2);
   }

   /* compute sin, cos for later calcs */
   s4 = sin(th4);
   c4 = cos(th4);

   /* Joint 5 */

   s5 = hom.rot.z.z * (s23 * c4) -
        hom.rot.z.x * (c1 * c23 * c4 + s1 * s4) -
        hom.rot.z.y * (s1 * c23 * c4 - c1 * s4);
   c5 =-hom.rot.z.x * (c1 * s23) - hom.rot.z.y *
        (s1 * s23) - hom.rot.z.z * c23;
   th5 = atan2(s5, c5);

   /* Joint 6 */

   s6 = hom.rot.x.z * (s23 * s4) - hom.rot.x.x *
        (c1 * c23 * s4 - s1 * c4) - hom.rot.x.y *
        (s1 * c23 * s4 + c1 * c4);
   c6 = hom.rot.x.x * ((c1 * c23 * c4 + s1 * s4) *
        c5 - c1 * s23 * s5) + hom.rot.x.y *
        ((s1 * c23 * c4 - c1 * s4) * c5 - s1 * s23 * s5) -
        hom.rot.x.z * (s23 * c4 * c5 + c23 * s5);
   th6 = atan2(s6, c6);

   if (iflags & PUMA_WRIST_FLIP){
     th4 = th4 + PM_PI;
     th5 = -th5;
     th6 = th6 + PM_PI;
   }

   /* copy out */
   joints[0] = th1*180.0/PM_PI;
   joints[1] = th2*180.0/PM_PI;
   joints[2] = th3*180.0/PM_PI;
   joints[3] = th4*180.0/PM_PI;
   joints[4] = th5*180.0/PM_PI;
   joints[5] = th6*180.0/PM_PI;

   /* Store flags if requested */
   if (fflags != NULL) {
       *fflags = flags;
   }

   return 0;
}

#endif /* PUMAKINS_MATH_H */
