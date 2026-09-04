/*
  Copyright 2016 Dewey Garrett <dgarrett@panix.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_app.h>
#include <hal.h>
#include <kinematics.h>
#include <kins_rt.h>

MODULE_LICENSE("GPL");

#ifndef hypot
#define hypot(a,b) (sqrt((a)*(a)+(b)*(b)))
#endif

// the inverse reports the turn count it keeps and the angles it saw
static const kins_param_desc rose_params[] = {
    { "revolutions",      KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    { "theta_degrees",    KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
    { "bigtheta_degrees", KINS_PARAM_FLOAT, KINS_OUT, 0, 0.0 },
};
enum { O_REVOLUTIONS, O_THETA, O_BIGTHETA };

// what the inverse carries from one call to the next: the quadrant it
// last saw and the turns it has counted.  In the scratch, so that each
// caller counts its own.
#define OLDQUAD(s)     ((s)->aux[0])
#define REVOLUTIONS(s) ((s)->aux[1])

static int rose_forward(const kins_params *p, kins_scratch *s,
                        const double *joints,
                        EmcPose * pos,
                        const KINEMATICS_FORWARD_FLAGS * fflags,
                        KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)p;
    (void)s;
    (void)fflags;
    (void)iflags;
    double radius,z,theta;

    radius = joints[0];
    z      = joints[1];
    theta  = TO_RAD * joints[2];

    pos->tran.x = radius * cos(theta);
    pos->tran.y = radius * sin(theta);
    pos->tran.z = z;
    pos->a = 0;
    pos->b = 0;
    pos->c = 0;
    pos->u = 0;
    pos->v = 0;
    pos->w = 0;

    return 0;
}

static int rose_inverse(const kins_params *p, kins_scratch *s,
                        const EmcPose * pos,
                        double *joints,
                        const KINEMATICS_INVERSE_FLAGS * iflags,
                        KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)p;
    (void)iflags;
    (void)fflags;
// There is a potential problem when accumulating bigtheta -- loss of
// precision based on size of mantissa -- but in practice, it is probably ok

    int        oldquad = (int)OLDQUAD(s);
    int        revolutions = (int)REVOLUTIONS(s);

    double     theta,bigtheta;
    int        nowquad = 0;
    double     x = pos->tran.x;
    double     y = pos->tran.y;
    double     z = pos->tran.z;

    if      (x >= 0 && y >= 0) nowquad = 1;
    else if (x <  0 && y >= 0) nowquad = 2;
    else if (x <  0 && y <  0) nowquad = 3;
    else if (x >= 0 && y <  0) nowquad = 4;

    if (oldquad == 2 && nowquad == 3) {revolutions += 1;}
    if (oldquad == 3 && nowquad == 2) {revolutions -= 1;}

    theta     = atan2(y,x);
    bigtheta  = theta + PM_2_PI * revolutions;

    s->out[O_REVOLUTIONS] = revolutions;
    s->out[O_THETA]       = theta * TO_DEG;
    s->out[O_BIGTHETA]    = bigtheta * TO_DEG;

    joints[0] = hypot(x,y);
    joints[1] = z;
    joints[2] = TO_DEG * bigtheta;
    joints[3] = 0;
    joints[4] = 0;
    joints[5] = 0;
    joints[6] = 0;
    joints[7] = 0;
    joints[8] = 0;

    OLDQUAD(s)     = nowquad;
    REVOLUTIONS(s) = revolutions;
    return 0;
}

static int rose_jacobian(const kins_params *p, const double *joints,
                         const EmcPose *pos,
                         double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                         const KINEMATICS_INVERSE_FLAGS *iflags)
{
    double x = pos->tran.x, y = pos->tran.y;
    double r2 = x*x + y*y;
    double r = sqrt(r2);
    int j, a;
    (void)p;
    (void)joints;
    (void)iflags;
    // on the axis the angle is undefined and its rate unbounded
    if (r2 <= 0) { return -1; }
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }
    jac[0][0] = x/r;  jac[0][1] = y/r;
    jac[1][2] = 1;
    jac[2][0] = -y/r2 * TO_DEG;
    jac[2][1] =  x/r2 * TO_DEG;
    return 0;
}

static const kins_ops rose_ops = {
    .forward  = rose_forward,
    .inverse  = rose_inverse,
    .jacobian = rose_jacobian,
};

// joints 0..2 are radius, z and the unwrapped angle; the entry points
// come from kins_single.c
const kins_module_info kins_module = {
    .name                 = "rosekins",
    .halprefix            = "rosekins",
    .params               = rose_params,
    .nparams              = sizeof(rose_params)/sizeof(rose_params[0]),
    .required_coordinates = "XYZ",
    .max_joints           = 3,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &rose_ops },
};

static int comp_id;

void rtapi_app_exit(void) { hal_exit(comp_id); }

int rtapi_app_main(void) {
    comp_id = hal_init("rosekins");
    if(comp_id < 0) return comp_id;

    if (kinsSingleInit(comp_id, "XYZ", KINEMATICS_BOTH)) {
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}
