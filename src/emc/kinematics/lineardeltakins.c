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

#include "lineardeltakins-common.h"

static struct haldata
{
    hal_real_t r;
    hal_real_t l;
} *haldata;

static int comp_id;

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    (void)fflags;
    (void)iflags;
    set_geometry(hal_get_real(haldata->r), hal_get_real(haldata->l));
    return kinematics_forward(joints, pos);
}

int kinematicsInverse(const EmcPose *pos, double *joints,
        const KINEMATICS_INVERSE_FLAGS *iflags,
        KINEMATICS_FORWARD_FLAGS *fflags) {
    (void)iflags;
    (void)fflags;
    set_geometry(hal_get_real(haldata->r), hal_get_real(haldata->l));
    return kinematics_inverse(pos, joints);
}

int kinematicsJacobian(const double *joints,
                       const EmcPose *pos,
                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                       const KINEMATICS_INVERSE_FLAGS *iflags) {
    double x = pos->tran.x, y = pos->tran.y, z = pos->tran.z;
    int i, j, a;
    (void)iflags;
    set_geometry(hal_get_real(haldata->r), hal_get_real(haldata->l));
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }
    // each carriage is the platform height plus the rise of its rod, and
    // the rise changes with the horizontal offset from the tower
    for (i = 0; i < 3; i++) {
        double tx = (i == 0) ? Ax : (i == 1) ? Bx : Cx;
        double ty = (i == 0) ? Ay : (i == 1) ? By : Cy;
        double rise = joints[i] - z;
        if (rise <= 0) { return -1; }
        jac[i][0] = (tx - x)/rise;
        jac[i][1] = (ty - y)/rise;
        jac[i][2] = 1;
    }
    for (j = 3; j < 9; j++) { jac[j][j] = 1; }
    return 0;
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

int rtapi_app_main(void)
{
    int retval;

    comp_id = hal_init("lineardeltakins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(*haldata));
    if(!haldata) { retval = -ENOMEM; goto error; }

    if((retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->r, DELTA_RADIUS, "lineardeltakins.R")) < 0)
        goto error;
    if((retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->l, DELTA_DIAGONAL_ROD, "lineardeltakins.L")) < 0)
        goto error;

    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return retval;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsJacobian);
MODULE_LICENSE("GPL");
