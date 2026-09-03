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

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>
#include <kinematics.h>		/* these decls */

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    double c_rad = -joints[5]*M_PI/180;
    pos->tran.x = joints[0] * cos(c_rad) - joints[1] * sin(c_rad);
    pos->tran.y = joints[0] * sin(c_rad) + joints[1] * cos(c_rad);
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
    (void)iflags;
    (void)fflags;
    double c_rad = pos->c*M_PI/180;
    joints[0] = pos->tran.x * cos(c_rad) - pos->tran.y * sin(c_rad);
    joints[1] = pos->tran.x * sin(c_rad) + pos->tran.y * cos(c_rad);
    joints[2] = pos->tran.z;
    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
}

int kinematicsJacobian(const double *joints,
                       const EmcPose *pos,
                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                       const KINEMATICS_INVERSE_FLAGS *iflags)
{
    double c_rad = pos->c*M_PI/180;
    double cc = cos(c_rad), sc = sin(c_rad);
    int j, a;
    (void)joints;
    (void)iflags;
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }
    // the inverse above, differentiated: the rotation itself for x and y,
    // and the rotated point turned a quarter turn for c
    jac[0][0] =  cc; jac[0][1] = -sc;
    jac[0][5] = (-pos->tran.x*sc - pos->tran.y*cc) * (M_PI/180);
    jac[1][0] =  sc; jac[1][1] =  cc;
    jac[1][5] = ( pos->tran.x*cc - pos->tran.y*sc) * (M_PI/180);
    for (j = 2; j < 9; j++) { jac[j][j] = 1; }
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

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsJacobian);
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
