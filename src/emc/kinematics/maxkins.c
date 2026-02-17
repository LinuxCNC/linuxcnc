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

/********************************************************************
* Note: The direction of the B axis is the opposite of the
* conventional axis direction. See
* https://linuxcnc.org/docs/html/gcode/machining-center.html
********************************************************************/


#include "kinematics.h"		/* these decls */
#include "posemath.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"

/* ========================================================================
 * Internal math (was in maxkins_math.h)
 * ======================================================================== */

#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif
#define maxkins_d2r(d) ((d)*PM_PI/180.0)
#define maxkins_r2d(r) ((r)*180.0/PM_PI)
#define maxkins_hypot(a,b) (sqrt((a)*(a)+(b)*(b)))

static int maxkins_forward_impl(double pivot_length, int conventional_directions,
                                const double *joints, EmcPose *pos)
{
    const double con = conventional_directions ? 1.0 : -1.0;

    const double zb = (pivot_length + joints[8]) * cos(maxkins_d2r(joints[4]));
    const double xb = (pivot_length + joints[8]) * sin(maxkins_d2r(joints[4]));

    const double xyr = maxkins_hypot(joints[0], joints[1]);
    const double xytheta = atan2(joints[1], joints[0]) + maxkins_d2r(joints[5]);

    const double zv = joints[6] * sin(maxkins_d2r(joints[4]));
    const double xv = joints[6] * cos(maxkins_d2r(joints[4]));

    pos->tran.x = xyr * cos(xytheta) - (con * xb) - xv;
    pos->tran.y = xyr * sin(xytheta) - joints[7];
    pos->tran.z = joints[2] - zb - (con * zv) + pivot_length;

    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    pos->u = joints[6];
    pos->v = joints[7];
    pos->w = joints[8];

    return 0;
}

static int maxkins_inverse_impl(double pivot_length, int conventional_directions,
                                const EmcPose *pos, double *joints)
{
    const double con = conventional_directions ? 1.0 : -1.0;

    const double zb = (pivot_length + pos->w) * cos(maxkins_d2r(pos->b));
    const double xb = (pivot_length + pos->w) * sin(maxkins_d2r(pos->b));

    const double xyr = maxkins_hypot(pos->tran.x, pos->tran.y);
    const double xytheta = atan2(pos->tran.y, pos->tran.x) - maxkins_d2r(pos->c);

    const double zv = pos->u * sin(maxkins_d2r(pos->b));
    const double xv = pos->u * cos(maxkins_d2r(pos->b));

    joints[0] = xyr * cos(xytheta) + (con * xb) + xv;
    joints[1] = xyr * sin(xytheta) + pos->v;
    joints[2] = pos->tran.z + zb - (con * zv) - pivot_length;

    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
}

/* ========================================================================
 * RT interface (reads HAL pins)
 * ======================================================================== */

struct haldata {
    hal_float_t *pivot_length;
    hal_bit_t *conventional_directions; //default is false
} *haldata;

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    return maxkins_forward_impl(*(haldata->pivot_length),
                                *(haldata->conventional_directions),
                                joints, pos);
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    return maxkins_inverse_impl(*(haldata->pivot_length),
                                *(haldata->conventional_directions),
                                pos, joints);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi_app.h"		/* RTAPI realtime module decls */

const char* kinematicsGetName(void) { return "maxkins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    int result;
    comp_id = hal_init("maxkins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    result = hal_pin_float_new("maxkins.pivot-length", HAL_IO, &(haldata->pivot_length), comp_id);

    result += hal_pin_bit_new("maxkins.conventional-directions", HAL_IN, &(haldata->conventional_directions), comp_id);

    if(result < 0) goto error;

    *(haldata->pivot_length) = 0.666;
    *(haldata->conventional_directions) = 0; // default is unconventional
    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return result;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    return maxkins_forward_impl(kp->params.maxkins.pivot_length,
                                kp->params.maxkins.conventional_directions,
                                joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    return maxkins_inverse_impl(kp->params.maxkins.pivot_length,
                                kp->params.maxkins.conventional_directions,
                                pos, joints);
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    kinematics_params_t *kp = (kinematics_params_t *)params;
    (void)read_s32;

    if (read_float("maxkins.pivot-length",
                   &kp->params.maxkins.pivot_length) != 0)
        return -1;

    if (read_bit("maxkins.conventional-directions",
                 &kp->params.maxkins.conventional_directions) != 0)
        return -1;

    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
