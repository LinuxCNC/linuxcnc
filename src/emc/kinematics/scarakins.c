/*****************************************************************
* Description: scarakins.c
*   Kinematics for scara typed robots
*   Set the params using HAL to fit your robot
*
*   Derived from a work by Sagar Behere
*
* Author: Sagar Behere 
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
*******************************************************************
*/

#include "posemath.h"
#include "rtapi_math.h"
#include "kinematics.h"             /* decls for kinematicsForward, etc. */

#ifdef RTAPI
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

struct {
    hal_float_t a1, a2, d1, d4;
} *haldata = 0;


#define SCARA_A1 (haldata->a1)
#define SCARA_A2 (haldata->a2)
#define SCARA_D1 (haldata->d1)
#define SCARA_D4 (haldata->d4)

/*
assuming that joint[0], joint[1] and joint[3] come in degrees, and joint[2] comes in mm
*/
int kinematicsForward(const double * joint,
                      EmcPose * world,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    double q0, q1;
    double x, y, z, c;

/* 
	convert joint[0] and joint[1] to radian, since sin() and cos() requires them so.
*/
    q0 = joint[0] / 180 * PM_PI;
    q1 = joint[1] / 180 * PM_PI;
	
    x = SCARA_A1*cos(q0) + SCARA_A2*cos((q0+q1));
    y = SCARA_A1*sin(q0) + SCARA_A2*sin((q0+q1));
    z = SCARA_D1 - joint[2];
    c = joint[3];
	
    *iflags = 0;
    if (joint[1] < 90)
	*iflags = 1;
	
    world->tran.x = x;
    world->tran.y = y;
    world->tran.z = z;
    world->c = c;
	
    world->a = 0.0;
    world->b = 0.0;
	
    return (0);
}

int kinematicsInverse(const EmcPose * world,
                      double * joint,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    double q0, q1;
    double x,y;

    x = world->tran.x;
    y = world->tran.y;

    if (*iflags)
	q1 = (-1)*acos( (x*x + y*y - SCARA_A1*SCARA_A1 - SCARA_A2*SCARA_A2) / (2*SCARA_A1*SCARA_A2) );
    else
	q1 = acos( (x*x + y*y - SCARA_A1*SCARA_A1 - SCARA_A2*SCARA_A2) / (2*SCARA_A1*SCARA_A2) );
	
    q0 = atan2(y, x) - atan2( (sin(q1) * SCARA_A2), (cos(q1) * SCARA_A2 + SCARA_A1) );
	
    /* q1 and a2 are still in radians. convert them to degree now */
    q0 = q0 / PM_PI * 180;
    q1 = q1 / PM_PI * 180;
	
    joint[0] = q0;
    joint[1] = q1;
    joint[2] = SCARA_D1 - world->tran.z;
    joint[3] = world->c;
	
    *fflags = 0;
	
    return (0);
}

int kinematicsHome(EmcPose * world,
                   double * joint,
                   KINEMATICS_FORWARD_FLAGS * fflags,
                   KINEMATICS_INVERSE_FLAGS * iflags)
{
  /* use joints, set world */
  return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType()
{
  return KINEMATICS_BOTH;
}

#define DEFAULT_SCARA_A1 340
#define DEFAULT_SCARA_A2 250
#define DEFAULT_SCARA_D1 489
#define DEFAULT_SCARA_D4 197

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsHome);

int comp_id;

int rtapi_app_main(void) {
    int res=0;
    
    comp_id = hal_init("scarakins");
    if (comp_id < 0) return comp_id;
    
    haldata = hal_malloc(sizeof(*haldata));
    if (!haldata) goto error;
    SCARA_A1 = DEFAULT_SCARA_A1;
    SCARA_A2 = DEFAULT_SCARA_A2;
    SCARA_D1 = DEFAULT_SCARA_D1;
    SCARA_D4 = DEFAULT_SCARA_D4;

    if((res = hal_param_float_new("scarakins.A1", HAL_RW, &haldata->a1, comp_id)) != HAL_SUCCESS) goto error;
    if((res = hal_param_float_new("scarakins.A2", HAL_RW, &haldata->a2, comp_id)) != HAL_SUCCESS) goto error;
    if((res = hal_param_float_new("scarakins.D1", HAL_RW, &haldata->d1, comp_id)) != HAL_SUCCESS) goto error;
    if((res = hal_param_float_new("scarakins.D4", HAL_RW, &haldata->d4, comp_id)) != HAL_SUCCESS) goto error;
    
    hal_ready(comp_id);
    return 0;
    
error:
    hal_exit(comp_id);
    return res;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
#endif
