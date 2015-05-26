/*****************************************************************
* Description: pumakins.c
*   Kinematics for puma typed robots
*   Set the params using HAL to fit your robot
*
*   Derived from a work by Fred Proctor
*
* Author: 
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
*******************************************************************
*/

#include "rtapi_math.h"
#include "posemath.h"
#include "pumakins.h"
#include "kinematics.h"             /* decls for kinematicsForward, etc. */

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

#define VTVERSION VTKINEMATICS_VERSION1

struct haldata {
    hal_float_t *a2, *a3, *d3, *d4;
} *haldata = 0;


#define PUMA_A2 (*(haldata->a2))
#define PUMA_A3 (*(haldata->a3))
#define PUMA_D3 (*(haldata->d3))
#define PUMA_D4 (*(haldata->d4))

MODULE_LICENSE("GPL");


int kinematicsForward(const double * joint,
                      EmcPose * world,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
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

   /* Calculate sin of joints for future use */
   s1 = rtapi_sin(joint[0]*PM_PI/180);
   s2 = rtapi_sin(joint[1]*PM_PI/180);
   s3 = rtapi_sin(joint[2]*PM_PI/180);
   s4 = rtapi_sin(joint[3]*PM_PI/180);
   s5 = rtapi_sin(joint[4]*PM_PI/180);
   s6 = rtapi_sin(joint[5]*PM_PI/180);

   /* Calculate cos of joints for future use */
   c1 = rtapi_cos(joint[0]*PM_PI/180);
   c2 = rtapi_cos(joint[1]*PM_PI/180);
   c3 = rtapi_cos(joint[2]*PM_PI/180);
   c4 = rtapi_cos(joint[3]*PM_PI/180);
   c5 = rtapi_cos(joint[4]*PM_PI/180);
   c6 = rtapi_cos(joint[5]*PM_PI/180);

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
   t1 = PUMA_A2 * c2 + PUMA_A3 * c23 - PUMA_D4 * s23;

   /* Define position vector */
   hom.tran.x = c1 * t1 - PUMA_D3 * s1;
   hom.tran.y = s1 * t1 + PUMA_D3 * c1;
   hom.tran.z = -PUMA_A3 * s23 - PUMA_A2 * s2 - PUMA_D4 * c23;

   /* Calculate terms to be used to...   */
   /* determine flags.                   */
   sumSq = hom.tran.x * hom.tran.x + hom.tran.y * hom.tran.y -
           PUMA_D3 * PUMA_D3;
   k = (sumSq + hom.tran.z * hom.tran.z - PUMA_A2 * PUMA_A2 -
       PUMA_A3 * PUMA_A3 - PUMA_D4 * PUMA_D4) /
       (2.0 * PUMA_A2);

   /* reset flags */
   *iflags = 0;

   /* Set shoulder-up flag if necessary */
   if (rtapi_fabs(joint[0]*PM_PI/180 - rtapi_atan2(hom.tran.y, hom.tran.x) +
       rtapi_atan2(PUMA_D3, -rtapi_sqrt(sumSq))) < FLAG_FUZZ)
   {
     *iflags |= PUMA_SHOULDER_RIGHT;
   }

   /* Set elbow down flag if necessary */
   if (rtapi_fabs(joint[2]*PM_PI/180 - rtapi_atan2(PUMA_A3, PUMA_D4) +
       rtapi_atan2(k, -rtapi_sqrt(PUMA_A3 * PUMA_A3 +
       PUMA_D4 * PUMA_D4 - k * k))) < FLAG_FUZZ)
   {
      *iflags |= PUMA_ELBOW_DOWN;
   }

   /* set singular flag if necessary */
   t1 = -hom.rot.z.x * s1 + hom.rot.z.y * c1;
   t2 = -hom.rot.z.x * c1 * c23 - hom.rot.z.y * s1 * c23 +
         hom.rot.z.z * s23;
   if (rtapi_fabs(t1) < SINGULAR_FUZZ && rtapi_fabs(t2) < SINGULAR_FUZZ)
   {
      *iflags |= PUMA_SINGULAR;
   }

   /* if not singular set wrist flip flag if necessary */
   else{
     if (! (rtapi_fabs(joint[3]*PM_PI/180 - rtapi_atan2(t1, t2)) < FLAG_FUZZ))
     {
       *iflags |= PUMA_WRIST_FLIP;
     }
   }

   /* convert hom.rot to world->quat */
   pmHomPoseConvert(&hom, &worldPose);
   pmQuatRpyConvert(&worldPose.rot,&rpy);
   world->tran = worldPose.tran;
   world->a = rpy.r * 180.0/PM_PI;
   world->b = rpy.p * 180.0/PM_PI;
   world->c = rpy.y * 180.0/PM_PI;

   
   /* return 0 and exit */
   return 0;
}

int kinematicsInverse(const EmcPose * world,
                      double * joint,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
   PmHomogeneous hom;
   PmPose worldPose;
   PmRpy rpy;
   int singular;

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

   /* reset flags */
   *fflags = 0;

   /* convert pose to hom */
   worldPose.tran = world->tran;
   rpy.r = world->a*PM_PI/180.0;
   rpy.p = world->b*PM_PI/180.0;
   rpy.y = world->c*PM_PI/180.0;
   pmRpyQuatConvert(&rpy,&worldPose.rot);
   pmPoseHomConvert(&worldPose, &hom);

   /* Joint 1 (2 independent solutions) */

   /* save sum of squares for this and subsequent calcs */
   sumSq = hom.tran.x * hom.tran.x + hom.tran.y * hom.tran.y -
           PUMA_D3 * PUMA_D3;

   /* FIXME-- is use of + sqrt shoulder right or left? */
   if (*iflags & PUMA_SHOULDER_RIGHT){
     th1 = rtapi_atan2(hom.tran.y, hom.tran.x) - rtapi_atan2(PUMA_D3, -rtapi_sqrt(sumSq));
   }
   else{
     th1 = rtapi_atan2(hom.tran.y, hom.tran.x) - rtapi_atan2(PUMA_D3, rtapi_sqrt(sumSq));
   }

   /* save sin, cos for later calcs */
   s1 = rtapi_sin(th1);
   c1 = rtapi_cos(th1);

   /* Joint 3 (2 independent solutions) */

   k = (sumSq + hom.tran.z * hom.tran.z - PUMA_A2 * PUMA_A2 -
       PUMA_A3 * PUMA_A3 - PUMA_D4 * PUMA_D4) / (2.0 * PUMA_A2);

   /* FIXME-- is use of + sqrt elbow up or down? */
   if (*iflags & PUMA_ELBOW_DOWN){
     th3 = rtapi_atan2(PUMA_A3, PUMA_D4) - rtapi_atan2(k, -rtapi_sqrt(PUMA_A3 * PUMA_A3 + PUMA_D4 * PUMA_D4 - k * k));
   }
   else{
     th3 = rtapi_atan2(PUMA_A3, PUMA_D4) -
           rtapi_atan2(k, rtapi_sqrt(PUMA_A3 * PUMA_A3 + PUMA_D4 * PUMA_D4 - k * k));
   }

   /* compute sin, cos for later calcs */
   s3 = rtapi_sin(th3);
   c3 = rtapi_cos(th3);

   /* Joint 2 */

   t1 = (-PUMA_A3 - PUMA_A2 * c3) * hom.tran.z +
        (c1 * hom.tran.x + s1 * hom.tran.y) * (PUMA_A2 * s3 - PUMA_D4);
   t2 = (PUMA_A2 * s3 - PUMA_D4) * hom.tran.z +
        (PUMA_A3 + PUMA_A2 * c3) * (c1 * hom.tran.x + s1 * hom.tran.y);
   t3 = hom.tran.z * hom.tran.z + (c1 * hom.tran.x + s1 * hom.tran.y) *
        (c1 * hom.tran.x + s1 * hom.tran.y);

   th23 = rtapi_atan2(t1, t2);
   th2 = th23 - th3;

   /* compute sin, cos for later calcs */
   s23 = t1 / t3;
   c23 = t2 / t3;

   /* Joint 4 */

   t1 = -hom.rot.z.x * s1 + hom.rot.z.y * c1;
   t2 = -hom.rot.z.x * c1 * c23 - hom.rot.z.y * s1 * c23 + hom.rot.z.z * s23;
   if (rtapi_fabs(t1) < SINGULAR_FUZZ && rtapi_fabs(t2) < SINGULAR_FUZZ){
     singular = 1;
     *fflags |= PUMA_REACH;
     th4 = joint[3]*PM_PI/180;            /* use current value */
   }
   else{
     singular = 0;
     th4 = rtapi_atan2(t1, t2);
   }

   /* compute sin, cos for later calcs */
   s4 = rtapi_sin(th4);
   c4 = rtapi_cos(th4);

   /* Joint 5 */

   s5 = hom.rot.z.z * (s23 * c4) -
        hom.rot.z.x * (c1 * c23 * c4 + s1 * s4) -
        hom.rot.z.y * (s1 * c23 * c4 - c1 * s4);
   c5 =-hom.rot.z.x * (c1 * s23) - hom.rot.z.y *
        (s1 * s23) - hom.rot.z.z * c23;
   th5 = rtapi_atan2(s5, c5);

   /* Joint 6 */

   s6 = hom.rot.x.z * (s23 * s4) - hom.rot.x.x *
        (c1 * c23 * s4 - s1 * c4) - hom.rot.x.y *
        (s1 * c23 * s4 + c1 * c4);
   c6 = hom.rot.x.x * ((c1 * c23 * c4 + s1 * s4) *
        c5 - c1 * s23 * s5) + hom.rot.x.y *
        ((s1 * c23 * c4 - c1 * s4) * c5 - s1 * s23 * s5) -
        hom.rot.x.z * (s23 * c4 * c5 + c23 * s5);
   th6 = rtapi_atan2(s6, c6);

   /* FIXME-- is wrist flip the normal or offset results? */
   if (*iflags & PUMA_WRIST_FLIP){
     th4 = th4 + PM_PI;
     th5 = -th5;
     th6 = th6 + PM_PI;
   }

   /* copy out */
   joint[0] = th1*180/PM_PI;
   joint[1] = th2*180/PM_PI;
   joint[2] = th3*180/PM_PI;
   joint[3] = th4*180/PM_PI;
   joint[4] = th5*180/PM_PI;
   joint[5] = th6*180/PM_PI;

   return singular == 0 ? 0 : -1;
}

int kinematicsHome(EmcPose * world,
                   double * joint,
                   KINEMATICS_FORWARD_FLAGS * fflags,
                   KINEMATICS_INVERSE_FLAGS * iflags)
{
  /* use joints, set world */
  return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType(void)
{
//  return KINEMATICS_FORWARD_ONLY;
  return KINEMATICS_BOTH;
}

static vtkins_t vtk = {
    .kinematicsForward = kinematicsForward,
    .kinematicsInverse  = kinematicsInverse,
    // .kinematicsHome = kinematicsHome,
    .kinematicsType = kinematicsType
};

static int comp_id, vtable_id;
static const char *name = "pumakins";

int rtapi_app_main(void) {
    int res=0;
    
    comp_id = hal_init(name);
    if (comp_id < 0) return comp_id;

    vtable_id = hal_export_vtable(name, VTVERSION, &vtk, comp_id);
    if (vtable_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_export_vtable(%s,%d,%p) failed: %d\n",
			name, name, VTVERSION, &vtk, vtable_id );
	return -ENOENT;
    }

    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata) goto error;

    if((res = hal_pin_float_new("pumakins.A2", HAL_IO, &(haldata->a2), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("pumakins.A3", HAL_IO, &(haldata->a3), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("pumakins.D3", HAL_IO, &(haldata->d3), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("pumakins.D4", HAL_IO, &(haldata->d4), comp_id)) < 0) goto error;
    
    PUMA_A2 = DEFAULT_PUMA560_A2;
    PUMA_A3 = DEFAULT_PUMA560_A3;
    PUMA_D3 = DEFAULT_PUMA560_D3;
    PUMA_D4 = DEFAULT_PUMA560_D4;
    hal_ready(comp_id);
    return 0;
    
error:
    hal_exit(comp_id);
    return res;
}

void rtapi_app_exit(void)
{
    hal_remove_vtable(vtable_id);
    hal_exit(comp_id);
}
