/********************************************************************
* Description: 5axiskins.c
*   kinematics for XYZBC 5 axis bridge mill
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2007 Chris Radek
*
* Last change:
********************************************************************/

#include "kinematics.h"		/* these decls */
#include "posemath.h"
#include "hal.h"
#include "rtapi_math.h"

#define d2r(d) ((d)*PM_PI/180.0)
#define r2d(r) ((r)*180.0/PM_PI)

struct haldata {
    hal_float_t *pivot_length;
} *haldata;

static PmCartesian s2r(double r, double t, double p) {
    PmCartesian c;
    t = d2r(t), p = d2r(p);

    c.x = r * sin(p) * cos(t);
    c.y = r * sin(p) * sin(t);
    c.z = r * cos(p);

    return c;
}


// 6 joints (5axiskins name is misnomer)
#define JOINT_0 0
#define JOINT_1 1
#define JOINT_2 2
// joints with identity to axis letters:
#define JOINT_B 3
#define JOINT_C 4
#define JOINT_W 5

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    PmCartesian r = s2r(*(haldata->pivot_length)+ joints[JOINT_W]
                       ,joints[JOINT_C]
                       ,180.0 - joints[JOINT_B]);

    pos->tran.x = joints[JOINT_0] + r.x;
    pos->tran.y = joints[JOINT_1] + r.y;
    pos->tran.z = joints[JOINT_2] + *(haldata->pivot_length) + r.z;
    pos->b      = joints[JOINT_B];
    pos->c      = joints[JOINT_C];
    pos->w      = joints[JOINT_W];

    pos->a = 0;
    pos->u = 0;
    pos->v = 0;

    return 0;
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{

    PmCartesian r = s2r(*(haldata->pivot_length) + pos->w
                       ,pos->c
                       ,180.0 - pos->b);

    joints[JOINT_0] = pos->tran.x - r.x;
    joints[JOINT_1] = pos->tran.y - r.y;
    joints[JOINT_2] = pos->tran.z - *(haldata->pivot_length) - r.z;

    joints[JOINT_B] = pos->b;
    joints[JOINT_C] = pos->c;
    joints[JOINT_W] = pos->w;

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

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    int result;
    comp_id = hal_init("5axiskins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    result = hal_pin_float_new("5axiskins.pivot-length", HAL_IO, &(haldata->pivot_length), comp_id);
    if(result < 0) goto error;

    *(haldata->pivot_length) = 250.0;

    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return result;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
