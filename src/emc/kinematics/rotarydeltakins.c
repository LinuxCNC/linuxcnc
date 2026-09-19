
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

#include "rotarydeltakins-common.h"

static struct haldata
{
    hal_real_t pfr;
    hal_real_t tl;
    hal_real_t sl;
    hal_real_t fr;
} *haldata;

static int comp_id;

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    (void)fflags;
    (void)iflags;
    set_geometry(hal_get_real(haldata->pfr), hal_get_real(haldata->tl), hal_get_real(haldata->sl), hal_get_real(haldata->fr));
    return kinematics_forward(joints, pos);
}

int kinematicsInverse(const EmcPose *pos, double *joints,
        const KINEMATICS_INVERSE_FLAGS *iflags,
        KINEMATICS_FORWARD_FLAGS *fflags) {
    (void)iflags;
    (void)fflags;
    set_geometry(hal_get_real(haldata->pfr), hal_get_real(haldata->tl), hal_get_real(haldata->sl), hal_get_real(haldata->fr));
    return kinematics_inverse(pos, joints);
}

int kinematicsJacobian(const double *joints,
                       const EmcPose *pos,
                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                       const KINEMATICS_INVERSE_FLAGS *iflags) {
    int i, j, a;
    (void)iflags;
    set_geometry(hal_get_real(haldata->pfr), hal_get_real(haldata->tl), hal_get_real(haldata->sl), hal_get_real(haldata->fr));
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }
    // The foot stays a shin length from each knee, so along a leg the
    // motion of the foot and the motion of the knee agree:
    //     (P - K) . dP = (P - K) . dK/dq dq
    // K is the knee less the foot offset, written as kinematics_forward()
    // writes it, and q the hip angle that swings it.
    for (i = 0; i < 3; i++) {
        double q = D2R(joints[i]);
        double reach = platformradius - footradius + thighlength * cos(q);
        double kx, ky, kz, dkx, dky, dkz, px, py, pz, denom;
        switch (i) {
        case 0:
            kx = 0;               ky = -reach;
            dkx = 0;              dky = thighlength * sin(q);
            break;
        case 1:
            kx = reach * 0.5 * sqrt(3);  ky = reach * 0.5;
            dkx = -thighlength * sin(q) * 0.5 * sqrt(3);
            dky = -thighlength * sin(q) * 0.5;
            break;
        default:
            kx = -reach * 0.5 * sqrt(3); ky = reach * 0.5;
            dkx = thighlength * sin(q) * 0.5 * sqrt(3);
            dky = -thighlength * sin(q) * 0.5;
            break;
        }
        kz = -thighlength * sin(q);
        dkz = -thighlength * cos(q);
        px = pos->tran.x - kx;
        py = pos->tran.y - ky;
        pz = pos->tran.z - kz;
        denom = (px*dkx + py*dky + pz*dkz) * (M_PI/180.);
        // the shin at right angles to the thigh's swing: the knee cannot
        // move the foot, so no finite hip rate follows the foot
        if (fabs(denom) < 1e-12) { return -1; }
        jac[i][0] = px/denom;
        jac[i][1] = py/denom;
        jac[i][2] = pz/denom;
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

    comp_id = hal_init("rotarydeltakins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(*haldata));
    if(!haldata) { retval = -ENOMEM; goto error; }

    if((retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->pfr, RDELTA_PFR, "rotarydeltakins.platformradius")) < 0)
        goto error;
    if((retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->tl, RDELTA_TL, "rotarydeltakins.thighlength")) < 0)
        goto error;
    if((retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->sl, RDELTA_SL, "rotarydeltakins.shinlength")) < 0)
        goto error;
    if((retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->fr, RDELTA_FR, "rotarydeltakins.footradius")) < 0)
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
