/********************************************************************
* Description: maxkins.c
*   Kinematics for Chris Radek's tabletop 5 axis mill named 'max'.
*   This mill has a tilting head (B axis) and horizontal rotary
*   mounted to the table (C axis).
*
* Author: Chris Radek
* License: GPL Version 2
*    
* Copyright (c) 2007 Chris Radek
********************************************************************/

#include "kinematics.h"		/* these decls */
#include "posemath.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"

#define d2r(d) ((d)*PM_PI/180.0)
#define r2d(r) ((r)*180.0/PM_PI)

#ifndef hypot
#define hypot(a,b) (sqrt((a)*(a)+(b)*(b)))
#endif

struct haldata {
    hal_float_t pivot_length;
} *haldata;

// should not even be called
int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{

    static int printed=0;

    if(!printed) rtapi_print_msg(RTAPI_MSG_ERR, "TP is null\n");
    printed=1;

    pos->tran.x = joints[0];
    pos->tran.y = joints[1];
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
    // B correction
    double zb = (haldata->pivot_length + pos->w) * cos(d2r(pos->b));
    double xb = (haldata->pivot_length + pos->w) * sin(d2r(pos->b));
        
    // C correction
    double xyr = hypot(pos->tran.x, pos->tran.y);
    double xytheta = atan2(pos->tran.y, pos->tran.x);
    xytheta += d2r(pos->c);

    joints[0] = xyr * cos(xytheta) + xb;
    joints[1] = xyr * sin(xytheta);
    joints[2] = pos->tran.z + zb - haldata->pivot_length;

    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_INVERSE_ONLY;
}

#ifdef RTAPI
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    int result;
    comp_id = hal_init("maxkins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    result = hal_param_float_new("maxkins.pivot-length", HAL_RW, &(haldata->pivot_length), comp_id);
    if(result < 0) goto error;

    haldata->pivot_length = 0.666;

    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return result;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
#endif
