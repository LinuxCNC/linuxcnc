/********************************************************************
* Description: rotatekins.c
*   Simple example kinematics for a rotary table in software
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Chris Radek
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/

#include "kinematics.h"		/* these decls */
#include "rotatekins_math.h"

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    return rotate_forward_math(joints, pos);
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    return rotate_inverse_math(pos, joints);
}

/* implemented for these kinematics as giving joints preference */
int kinematicsHome(EmcPose * world,
		   double *joint,
		   KINEMATICS_FORWARD_FLAGS * fflags,
		   KINEMATICS_INVERSE_FLAGS * iflags)
{
    *fflags = 0;
    *iflags = 0;

    return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

const char* kinematicsGetName(void) { return "rotatekins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    comp_id = hal_init("rotatekins");
    if(comp_id > 0) {
	hal_ready(comp_id);
	return 0;
    }
    return comp_id;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
