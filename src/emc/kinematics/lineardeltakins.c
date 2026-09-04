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
#include <kins_rt.h>

#include "lineardeltakins-common.h"

// the two lengths, one pin each
static const kins_param_desc ld_params[] = {
    { "R", KINS_PARAM_FLOAT, KINS_IN, 0, DELTA_RADIUS },
    { "L", KINS_PARAM_FLOAT, KINS_IN, 0, DELTA_DIAGONAL_ROD },
};
enum { P_R, P_L };

static int comp_id;

// the tower positions follow from the block's two lengths
static void geometry_of(const kins_params *p, lineardelta_geometry *g)
{
    lineardelta_set_geometry(g, p->geometry[P_R], p->geometry[P_L]);
}

static int ld_forward(const kins_params *p, kins_scratch *s,
                      const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    lineardelta_geometry g;
    (void)s;
    (void)fflags;
    (void)iflags;
    geometry_of(p, &g);
    return lineardelta_forward(&g, joints, pos);
}

static int ld_inverse(const kins_params *p, kins_scratch *s,
                      const EmcPose *pos, double *joints,
                      const KINEMATICS_INVERSE_FLAGS *iflags,
                      KINEMATICS_FORWARD_FLAGS *fflags) {
    lineardelta_geometry g;
    (void)s;
    (void)iflags;
    (void)fflags;
    geometry_of(p, &g);
    return lineardelta_inverse(&g, pos, joints);
}

static int ld_jacobian(const kins_params *p, const double *joints,
                       const EmcPose *pos,
                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                       const KINEMATICS_INVERSE_FLAGS *iflags) {
    lineardelta_geometry g;
    double x = pos->tran.x, y = pos->tran.y, z = pos->tran.z;
    int i, j, a;
    (void)iflags;
    geometry_of(p, &g);
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }
    // each carriage is the platform height plus the rise of its rod, and
    // the rise changes with the horizontal offset from the tower
    for (i = 0; i < 3; i++) {
        double tx = (i == 0) ? g.Ax : (i == 1) ? g.Bx : g.Cx;
        double ty = (i == 0) ? g.Ay : (i == 1) ? g.By : g.Cy;
        double rise = joints[i] - z;
        if (rise <= 0) { return -1; }
        jac[i][0] = (tx - x)/rise;
        jac[i][1] = (ty - y)/rise;
        jac[i][2] = 1;
    }
    for (j = 3; j < 9; j++) { jac[j][j] = 1; }
    return 0;
}

static const kins_ops ld_ops = {
    .forward  = ld_forward,
    .inverse  = ld_inverse,
    .jacobian = ld_jacobian,
};

// three towers for the three linear coordinates, the rest passed
// through; the entry points come from kins_single.c
const kins_module_info kins_module = {
    .name                 = "lineardeltakins",
    .halprefix            = "lineardeltakins",
    .params               = ld_params,
    .nparams              = sizeof(ld_params)/sizeof(ld_params[0]),
    .required_coordinates = "XYZABCUVW",
    .max_joints           = 9,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &ld_ops },
};

int rtapi_app_main(void)
{
    comp_id = hal_init("lineardeltakins");
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
