
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
#include <kins_rt.h>

#include "rotarydeltakins-common.h"

// the four lengths, one pin each
static const kins_param_desc rd_params[] = {
    { "platformradius", KINS_PARAM_FLOAT, KINS_IN, 0, RDELTA_PFR },
    { "thighlength",    KINS_PARAM_FLOAT, KINS_IN, 0, RDELTA_TL },
    { "shinlength",     KINS_PARAM_FLOAT, KINS_IN, 0, RDELTA_SL },
    { "footradius",     KINS_PARAM_FLOAT, KINS_IN, 0, RDELTA_FR },
};
enum { P_PFR, P_TL, P_SL, P_FR };

static int comp_id;

static void geometry_of(const kins_params *p, rotarydelta_geometry *g)
{
    rotarydelta_set_geometry(g, p->geometry[P_PFR], p->geometry[P_TL],
                             p->geometry[P_SL], p->geometry[P_FR]);
}

static int rd_forward(const kins_params *p, kins_scratch *s,
                      const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    rotarydelta_geometry g;
    (void)s;
    (void)fflags;
    (void)iflags;
    geometry_of(p, &g);
    return rotarydelta_forward(&g, joints, pos);
}

static int rd_inverse(const kins_params *p, kins_scratch *s,
                      const EmcPose *pos, double *joints,
                      const KINEMATICS_INVERSE_FLAGS *iflags,
                      KINEMATICS_FORWARD_FLAGS *fflags) {
    rotarydelta_geometry g;
    (void)s;
    (void)iflags;
    (void)fflags;
    geometry_of(p, &g);
    return rotarydelta_inverse(&g, pos, joints);
}

static int rd_jacobian(const kins_params *p, const double *joints,
                       const EmcPose *pos,
                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                       const KINEMATICS_INVERSE_FLAGS *iflags) {
    rotarydelta_geometry g;
    int i, j, a;
    (void)iflags;
    geometry_of(p, &g);
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }
    // The foot stays a shin length from each knee, so along a leg the
    // motion of the foot and the motion of the knee agree:
    //     (P - K) . dP = (P - K) . dK/dq dq
    // K is the knee less the foot offset, written as rotarydelta_forward()
    // writes it, and q the hip angle that swings it.
    for (i = 0; i < 3; i++) {
        double q = D2R(joints[i]);
        double reach = g.platformradius - g.footradius + g.thighlength * cos(q);
        double kx, ky, kz, dkx, dky, dkz, px, py, pz, denom;
        switch (i) {
        case 0:
            kx = 0;               ky = -reach;
            dkx = 0;              dky = g.thighlength * sin(q);
            break;
        case 1:
            kx = reach * 0.5 * sqrt(3);  ky = reach * 0.5;
            dkx = -g.thighlength * sin(q) * 0.5 * sqrt(3);
            dky = -g.thighlength * sin(q) * 0.5;
            break;
        default:
            kx = -reach * 0.5 * sqrt(3); ky = reach * 0.5;
            dkx = g.thighlength * sin(q) * 0.5 * sqrt(3);
            dky = -g.thighlength * sin(q) * 0.5;
            break;
        }
        kz = -g.thighlength * sin(q);
        dkz = -g.thighlength * cos(q);
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

static const kins_ops rd_ops = {
    .forward  = rd_forward,
    .inverse  = rd_inverse,
    .jacobian = rd_jacobian,
};

// three hips for the three linear coordinates, the rest passed through;
// the entry points come from kins_single.c
const kins_module_info kins_module = {
    .name                 = "rotarydeltakins",
    .halprefix            = "rotarydeltakins",
    .params               = rd_params,
    .nparams              = sizeof(rd_params)/sizeof(rd_params[0]),
    .required_coordinates = "XYZABCUVW",
    .max_joints           = 9,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &rd_ops },
};

int rtapi_app_main(void)
{
    comp_id = hal_init("rotarydeltakins");
    if(comp_id < 0) return comp_id;

    if (kinsSingleInit(comp_id, "XYZABCUVW", KINEMATICS_BOTH)) {
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

MODULE_LICENSE("GPL");
