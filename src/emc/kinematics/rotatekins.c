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

#include "rtapi_math.h"
#include "kinematics.h"		/* these decls */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

#define VTVERSION VTKINEMATICS_VERSION1


int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    double c_rad = -joints[5]*M_PI/180;
    pos->tran.x = joints[0] * rtapi_cos(c_rad) - joints[1] * rtapi_sin(c_rad);
    pos->tran.y = joints[0] * rtapi_sin(c_rad) + joints[1] * rtapi_cos(c_rad);
    pos->tran.z = joints[2];
    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    pos->u = joints[6];
    pos->v = joints[7];
    pos->w = joints[8];

    return 0;
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
    double c_rad = pos->c*M_PI/180;
    joints[0] = pos->tran.x * rtapi_cos(c_rad) - pos->tran.y * rtapi_sin(c_rad);
    joints[1] = pos->tran.x * rtapi_sin(c_rad) + pos->tran.y * rtapi_cos(c_rad);
    joints[2] = pos->tran.z;
    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
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

KINEMATICS_TYPE kinematicsType(void)
{
    return KINEMATICS_BOTH;
}

static vtkins_t vtk = {
    .kinematicsForward = kinematicsForward,
    .kinematicsInverse  = kinematicsInverse,
    // .kinematicsHome = kinematicsHome,
    .kinematicsType = kinematicsType
};

static int comp_id, vtable_id;
static const char *name = "rotatekins";

MODULE_LICENSE("GPL");

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
