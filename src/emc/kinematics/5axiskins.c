/********************************************************************
* Description: 5axiskins.c
*   Trivial kinematics for 3 axis Cartesian machine
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

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "rtapi_math.h"

#include "kinematics.h"
#include "posemath.h"
#include "hal.h"

#define d2r(d) ((d)*PM_PI/180.0)
#define r2d(r) ((r)*180.0/PM_PI)

struct haldata {
    hal_float_t *pivot_length;
} *haldata;

static PmCartesian s2r(double r, double t, double p) {
    PmCartesian c;
    t = d2r(t), p = d2r(p);

    c.x = r * rtapi_sin(p) * rtapi_cos(t);
    c.y = r * rtapi_sin(p) * rtapi_sin(t);
    c.z = r * rtapi_cos(p);

    return c;
}

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    PmCartesian r = s2r(*(haldata->pivot_length) + joints[8], joints[5], 180.0 - joints[4]);

    pos->tran.x = joints[0] + r.x;
    pos->tran.y = joints[1] + r.y;
    pos->tran.z = joints[2] + *(haldata->pivot_length) + r.z;
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

    PmCartesian r = s2r(*(haldata->pivot_length) + pos->w, pos->c, 180.0 - pos->b);

    joints[0] = pos->tran.x - r.x;
    joints[1] = pos->tran.y - r.y;
    joints[2] = pos->tran.z - *(haldata->pivot_length) - r.z;
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

MODULE_LICENSE("GPL");
#define VTVERSION VTKINEMATICS_VERSION1

static vtkins_t vtk = {
    .kinematicsForward = kinematicsForward,
    .kinematicsInverse  = kinematicsInverse,
    // .kinematicsHome = kinematicsHome,
    .kinematicsType = kinematicsType
};

static int comp_id, vtable_id;
static const char *name = "5axiskins";

int rtapi_app_main(void) {
    int result;
    comp_id = hal_init(name);
    if(comp_id < 0) return comp_id;

    vtable_id = hal_export_vtable(name, VTVERSION, &vtk, comp_id);
    if (vtable_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_export_vtable(%s,%d,%p) failed: %d\n",
			name, name, VTVERSION, &vtk, vtable_id );
	return -ENOENT;
    }

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

void rtapi_app_exit(void)
{
    hal_remove_vtable(vtable_id);
    hal_exit(comp_id);
}
