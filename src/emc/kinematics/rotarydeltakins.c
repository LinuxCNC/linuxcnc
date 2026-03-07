
//    Copyright 2013 Chris Radek <chris@timeguy.com>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <rtapi_math.h>
#include <rtapi_app.h>
#include <hal.h>
#include <kinematics.h>
#include "kinematics_params.h"
#include <string.h>

/* ---- Math types and functions (from rotarydeltakins_math.h) ---- */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef rotdelta_sq
#define rotdelta_sq(a) ((a)*(a))
#endif

#ifndef rotdelta_D2R
#define rotdelta_D2R(d) ((d)*M_PI/180.0)
#endif

typedef struct {
    double platformradius;
    double thighlength;
    double shinlength;
    double footradius;
} rotarydelta_params_t;

static void rotarydelta_rotate(double *x, double *y, double theta)
{
    double xx = *x;
    double yy = *y;
    *x = xx * cos(theta) - yy * sin(theta);
    *y = xx * sin(theta) + yy * cos(theta);
}

static int rotarydelta_inverse_j0(double x, double y, double z,
                                   const rotarydelta_params_t *params,
                                   double *theta)
{
    double a, b, d, knee_y, knee_z;
    double pfr = params->platformradius;
    double tl = params->thighlength;
    double sl = params->shinlength;
    double fr = params->footradius;

    a = 0.5 * (rotdelta_sq(x) + rotdelta_sq(y - fr) + rotdelta_sq(z) +
               rotdelta_sq(tl) - rotdelta_sq(sl) - rotdelta_sq(pfr)) / z;
    b = (fr - pfr - y) / z;

    d = rotdelta_sq(tl) * (rotdelta_sq(b) + 1) - rotdelta_sq(a - b * pfr);
    if (d < 0) return -1;

    knee_y = (pfr + a*b + sqrt(d)) / (rotdelta_sq(b) + 1);
    knee_z = b * knee_y - a;

    *theta = atan2(knee_z, knee_y - pfr);
    *theta *= 180.0/M_PI;
    return 0;
}

static int rotarydelta_inverse_math(const rotarydelta_params_t *params,
                                    const EmcPose *world, double *joints)
{
    double xr, yr;

    if (rotarydelta_inverse_j0(world->tran.x, world->tran.y, world->tran.z,
                                params, &joints[0]))
        return -1;

    xr = world->tran.x;
    yr = world->tran.y;
    rotarydelta_rotate(&xr, &yr, -2*M_PI/3);
    if (rotarydelta_inverse_j0(xr, yr, world->tran.z, params, &joints[1]))
        return -1;

    xr = world->tran.x;
    yr = world->tran.y;
    rotarydelta_rotate(&xr, &yr, 2*M_PI/3);
    if (rotarydelta_inverse_j0(xr, yr, world->tran.z, params, &joints[2]))
        return -1;

    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    return 0;
}

static int rotarydelta_forward_math(const rotarydelta_params_t *params,
                                    const double *joints, EmcPose *world)
{
    double pfr = params->platformradius;
    double tl = params->thighlength;
    double sl = params->shinlength;
    double fr = params->footradius;

    double j0, j1, j2;
    double y1, z1;
    double x2, y2, z2;
    double x3, y3, z3;
    double a1, b1, a2, b2;
    double w1, w2, w3;
    double denom;
    double a, b, c, d;

    j0 = rotdelta_D2R(joints[0]);
    j1 = rotdelta_D2R(joints[1]);
    j2 = rotdelta_D2R(joints[2]);

    y1 = -(pfr - fr + tl * cos(j0));
    z1 = -tl * sin(j0);

    y2 = (pfr - fr + tl * cos(j1)) * 0.5;
    x2 = y2 * sqrt(3);
    z2 = -tl * sin(j1);

    y3 = (pfr - fr + tl * cos(j2)) * 0.5;
    x3 = -y3 * sqrt(3);
    z3 = -tl * sin(j2);

    denom = x3 * (y2 - y1) - x2 * (y3 - y1);

    w1 = rotdelta_sq(y1) + rotdelta_sq(z1);
    w2 = rotdelta_sq(x2) + rotdelta_sq(y2) + rotdelta_sq(z2);
    w3 = rotdelta_sq(x3) + rotdelta_sq(y3) + rotdelta_sq(z3);

    a1 = (z2-z1) * (y3-y1) - (z3-z1) * (y2-y1);
    b1 = -((w2-w1) * (y3-y1) - (w3-w1) * (y2-y1)) / 2.0;

    a2 = -(z2 - z1) * x3 + (z3 - z1) * x2;
    b2 = ((w2 - w1) * x3 - (w3 - w1) * x2) / 2.0;

    /* a*z^2 + b*z + c = 0 */
    a = rotdelta_sq(a1) + rotdelta_sq(a2) + rotdelta_sq(denom);
    b = 2 * (a1 * b1 + a2 * (b2 - y1 * denom) - z1 * rotdelta_sq(denom));
    c = (b2 - y1 * denom) * (b2 - y1 * denom) +
        rotdelta_sq(b1) + rotdelta_sq(denom) * (rotdelta_sq(z1) - rotdelta_sq(sl));

    d = rotdelta_sq(b) - 4 * a * c;
    if (d < 0) return -1;

    world->tran.z = (-b - sqrt(d)) / (2 * a);
    world->tran.x = (a1 * world->tran.z + b1) / denom;
    world->tran.y = (a2 * world->tran.z + b2) / denom;

    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];

    return 0;
}

#define ROTARYDELTA_DEFAULT_PLATFORMRADIUS 10.0
#define ROTARYDELTA_DEFAULT_THIGHLENGTH    10.0
#define ROTARYDELTA_DEFAULT_SHINLENGTH     14.0
#define ROTARYDELTA_DEFAULT_FOOTRADIUS      6.0

/* ---- End math functions ---- */

struct haldata
{
    hal_float_t *pfr;
    hal_float_t *tl;
    hal_float_t *sl;
    hal_float_t *fr;
} *haldata;

int comp_id;
static kinematics_params_t *uspace_params;

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    (void)fflags;
    (void)iflags;
    if (!haldata) {
        kinematics_params_t local;
        KINS_SHMEM_READ(uspace_params, local);
        rotarydelta_params_t p;
        p.platformradius = local.params.rotarydelta.platformradius;
        p.thighlength    = local.params.rotarydelta.thighlength;
        p.shinlength     = local.params.rotarydelta.shinlength;
        p.footradius     = local.params.rotarydelta.footradius;
        return rotarydelta_forward_math(&p, joints, pos);
    }
    rotarydelta_params_t params;
    params.platformradius = *haldata->pfr;
    params.thighlength = *haldata->tl;
    params.shinlength = *haldata->sl;
    params.footradius = *haldata->fr;
    if (uspace_params) {
        uspace_params->head++;
        uspace_params->params.rotarydelta.platformradius = params.platformradius;
        uspace_params->params.rotarydelta.thighlength = params.thighlength;
        uspace_params->params.rotarydelta.shinlength = params.shinlength;
        uspace_params->params.rotarydelta.footradius = params.footradius;
        uspace_params->tail = uspace_params->head;
    }
    return rotarydelta_forward_math(&params, joints, pos);
}

int kinematicsInverse(const EmcPose *pos, double *joints,
        const KINEMATICS_INVERSE_FLAGS *iflags,
        KINEMATICS_FORWARD_FLAGS *fflags) {
    (void)iflags;
    (void)fflags;
    if (!haldata) {
        kinematics_params_t local;
        KINS_SHMEM_READ(uspace_params, local);
        rotarydelta_params_t p;
        p.platformradius = local.params.rotarydelta.platformradius;
        p.thighlength    = local.params.rotarydelta.thighlength;
        p.shinlength     = local.params.rotarydelta.shinlength;
        p.footradius     = local.params.rotarydelta.footradius;
        return rotarydelta_inverse_math(&p, pos, joints);
    }
    rotarydelta_params_t params;
    params.platformradius = *haldata->pfr;
    params.thighlength = *haldata->tl;
    params.shinlength = *haldata->sl;
    params.footradius = *haldata->fr;
    return rotarydelta_inverse_math(&params, pos, joints);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

int rtapi_app_main(void)
{
    int retval = 0;

    comp_id = hal_init("rotarydeltakins");
    if(comp_id < 0) retval = comp_id;

    if(retval == 0)
    {
        haldata = hal_malloc(sizeof(struct haldata));
        retval = !haldata;
    }

    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->pfr, comp_id,
                "rotarydeltakins.platformradius");
    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->tl, comp_id,
                "rotarydeltakins.thighlength");
    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->sl, comp_id,
                "rotarydeltakins.shinlength");
    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->fr, comp_id,
                "rotarydeltakins.footradius");

    if(retval == 0)
    {
        *haldata->pfr = ROTARYDELTA_DEFAULT_PLATFORMRADIUS;
        *haldata->tl = ROTARYDELTA_DEFAULT_THIGHLENGTH;
        *haldata->sl = ROTARYDELTA_DEFAULT_SHINLENGTH;
        *haldata->fr = ROTARYDELTA_DEFAULT_FOOTRADIUS;
    }

    if(retval == 0)
        retval = hal_struct_newf(comp_id, sizeof(kinematics_params_t), NULL,
                                 "rotarydeltakins.params");
    if(retval == 0)
        retval = hal_struct_attach("rotarydeltakins.params", (void **)&uspace_params);
    if(retval == 0)
    {
        uspace_params->num_joints = 3;
        uspace_params->params.rotarydelta.platformradius = ROTARYDELTA_DEFAULT_PLATFORMRADIUS;
        uspace_params->params.rotarydelta.thighlength = ROTARYDELTA_DEFAULT_THIGHLENGTH;
        uspace_params->params.rotarydelta.shinlength = ROTARYDELTA_DEFAULT_SHINLENGTH;
        uspace_params->params.rotarydelta.footradius = ROTARYDELTA_DEFAULT_FOOTRADIUS;
        uspace_params->valid       = 1;
        uspace_params->is_identity = 0;
        hal_ready(comp_id);
    }

    return retval;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

const char* kinematicsGetName(void) { return "rotarydeltakins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

/* ========================================================================
 * Non-RT interface
 *
 * Called by kinematics_user.c after dlopen().  Attaches to the
 * kinematics_params_t registered in HAL shmem by rtapi_app_main()
 * via hal_struct_newf() and returns forward/inverse function pointers.
 * ======================================================================== */

void nonrt_attach(nonrt_ops_t *ops)
{
    hal_struct_attach("rotarydeltakins.params", (void **)&uspace_params);
    ops->forward   = kinematicsForward;
    ops->inverse   = kinematicsInverse;
}

EXPORT_SYMBOL(nonrt_attach);
