//    Copyright 2013 Jeff Epler <jepler@unpythonic.net>
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

/* ---- Math types and functions (from lineardeltakins_math.h) ---- */

#ifndef M_SQRT3
#define M_SQRT3 1.7320508075688772935
#endif

#define LINDELTA_SIN_60 (M_SQRT3/2.0)
#define LINDELTA_COS_60 (0.5)

typedef struct {
    double radius;
    double rod_length;
} lineardelta_params_t;

typedef struct {
    double ax, ay;
    double bx, by;
    double cx, cy;
    double l2;
} lineardelta_geometry_t;

#ifndef lindelta_sq
#define lindelta_sq(x) ((x)*(x))
#endif

static void lineardelta_compute_geometry(const lineardelta_params_t *params,
                                         lineardelta_geometry_t *geom)
{
    double R = params->radius;
    double L = params->rod_length;

    geom->ax = 0.0;
    geom->ay = R;

    geom->bx = -LINDELTA_SIN_60 * R;
    geom->by = -LINDELTA_COS_60 * R;

    geom->cx = LINDELTA_SIN_60 * R;
    geom->cy = -LINDELTA_COS_60 * R;

    geom->l2 = lindelta_sq(L);
}

static int lineardelta_inverse_math(const lineardelta_params_t *params,
                                    const EmcPose *world, double *joints)
{
    lineardelta_geometry_t geom;
    double x = world->tran.x;
    double y = world->tran.y;
    double z = world->tran.z;

    lineardelta_compute_geometry(params, &geom);

    joints[0] = z + sqrt(geom.l2 - lindelta_sq(geom.ax - x) - lindelta_sq(geom.ay - y));
    joints[1] = z + sqrt(geom.l2 - lindelta_sq(geom.bx - x) - lindelta_sq(geom.by - y));
    joints[2] = z + sqrt(geom.l2 - lindelta_sq(geom.cx - x) - lindelta_sq(geom.cy - y));
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    if (isnan(joints[0]) || isnan(joints[1]) || isnan(joints[2]))
        return -1;

    return 0;
}

static int lineardelta_forward_math(const lineardelta_params_t *params,
                                    const double *joints, EmcPose *world)
{
    lineardelta_geometry_t geom;
    double q1, q2, q3;
    double den, w1, w2, w3;
    double a1, b1, a2, b2;
    double a, b, c, discr, z;

    lineardelta_compute_geometry(params, &geom);

    q1 = joints[0];
    q2 = joints[1];
    q3 = joints[2];

    den = (geom.by - geom.ay) * geom.cx - (geom.cy - geom.ay) * geom.bx;

    /* n.b. assumption that Ax is 0 all through here */
    w1 = lindelta_sq(geom.ay) + lindelta_sq(q1);
    w2 = lindelta_sq(geom.bx) + lindelta_sq(geom.by) + lindelta_sq(q2);
    w3 = lindelta_sq(geom.cx) + lindelta_sq(geom.cy) + lindelta_sq(q3);

    a1 = (q2 - q1) * (geom.cy - geom.ay) - (q3 - q1) * (geom.by - geom.ay);
    b1 = -((w2 - w1) * (geom.cy - geom.ay) - (w3 - w1) * (geom.by - geom.ay)) / 2.0;

    a2 = -(q2 - q1) * geom.cx + (q3 - q1) * geom.bx;
    b2 = ((w2 - w1) * geom.cx - (w3 - w1) * geom.bx) / 2.0;

    /* a*z^2 + b*z + c = 0 */
    a = lindelta_sq(a1) + lindelta_sq(a2) + lindelta_sq(den);
    b = 2 * (a1 * b1 + a2 * (b2 - geom.ay * den) - q1 * lindelta_sq(den));
    c = lindelta_sq(b2 - geom.ay * den) + lindelta_sq(b1) +
        lindelta_sq(den) * (lindelta_sq(q1) - geom.l2);

    discr = lindelta_sq(b) - 4.0 * a * c;
    if (discr < 0)
        return -1;

    z = -0.5 * (b + sqrt(discr)) / a;
    world->tran.z = z;
    world->tran.x = (a1 * z + b1) / den;
    world->tran.y = (a2 * z + b2) / den;
    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];

    return 0;
}

/* Default values which may correspond to someone's linear delta robot.
 * To change these, use halcmd setp rather than rebuilding the software.
 */
#define LINEARDELTA_DEFAULT_ROD_LENGTH 269.0  /* mm (DELTA_DIAGONAL_ROD) */
#define LINEARDELTA_SMOOTH_ROD_OFFSET 198.25  /* mm */
#define LINEARDELTA_EFFECTOR_OFFSET 33.0  /* mm */
#define LINEARDELTA_CARRIAGE_OFFSET 35.0  /* mm */
#define LINEARDELTA_DEFAULT_RADIUS (LINEARDELTA_SMOOTH_ROD_OFFSET - \
                                     LINEARDELTA_EFFECTOR_OFFSET - \
                                     LINEARDELTA_CARRIAGE_OFFSET)  /* mm */

/* ---- End math functions ---- */

struct haldata
{
    hal_float_t *r, *l;
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
        lineardelta_params_t p;
        p.radius = local.params.lineardelta.radius;
        p.rod_length = local.params.lineardelta.jointradius;
        return lineardelta_forward_math(&p, joints, pos);
    }
    lineardelta_params_t params;
    params.radius = *haldata->r;
    params.rod_length = *haldata->l;
    if (uspace_params) {
        uspace_params->head++;
        uspace_params->params.lineardelta.radius = params.radius;
        uspace_params->params.lineardelta.jointradius = params.rod_length;
        uspace_params->tail = uspace_params->head;
    }
    return lineardelta_forward_math(&params, joints, pos);
}

int kinematicsInverse(const EmcPose *pos, double *joints,
        const KINEMATICS_INVERSE_FLAGS *iflags,
        KINEMATICS_FORWARD_FLAGS *fflags) {
    (void)iflags;
    (void)fflags;
    if (!haldata) {
        kinematics_params_t local;
        KINS_SHMEM_READ(uspace_params, local);
        lineardelta_params_t p;
        p.radius = local.params.lineardelta.radius;
        p.rod_length = local.params.lineardelta.jointradius;
        return lineardelta_inverse_math(&p, pos, joints);
    }
    lineardelta_params_t params;
    params.radius = *haldata->r;
    params.rod_length = *haldata->l;
    return lineardelta_inverse_math(&params, pos, joints);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

int rtapi_app_main(void)
{
    int retval = 0;

    comp_id = hal_init("lineardeltakins");
    if(comp_id < 0) retval = comp_id;

    if(retval == 0)
    {
        haldata = hal_malloc(sizeof(struct haldata));
        retval = !haldata;
    }

    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->r, comp_id,
                "lineardeltakins.R");
    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->l, comp_id,
                "lineardeltakins.L");

    if(retval == 0)
    {
        *haldata->r = LINEARDELTA_DEFAULT_RADIUS;
        *haldata->l = LINEARDELTA_DEFAULT_ROD_LENGTH;
    }

    if(retval == 0)
        retval = hal_struct_newf(comp_id, sizeof(kinematics_params_t), NULL,
                                 "lineardeltakins.params");
    if(retval == 0)
        retval = hal_struct_attach("lineardeltakins.params", (void **)&uspace_params);
    if(retval == 0)
    {
        uspace_params->num_joints = 3;
        uspace_params->params.lineardelta.radius = LINEARDELTA_DEFAULT_RADIUS;
        uspace_params->params.lineardelta.jointradius = LINEARDELTA_DEFAULT_ROD_LENGTH;
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

const char* kinematicsGetName(void) { return "lineardeltakins"; }

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
    hal_struct_attach("lineardeltakins.params", (void **)&uspace_params);
    ops->forward   = kinematicsForward;
    ops->inverse   = kinematicsInverse;
}

EXPORT_SYMBOL(nonrt_attach);
