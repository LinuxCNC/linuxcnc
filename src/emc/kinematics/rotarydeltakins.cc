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

#define BOOST_PYTHON_MAX_ARITY 4
#include <cmath>
#include <boost/python.hpp>
using namespace boost::python;
#include "emcpos.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define rotdelta_sq(a) ((a)*(a))
#define rotdelta_D2R(d) ((d)*M_PI/180.0)

typedef struct {
    double platformradius;
    double thighlength;
    double shinlength;
    double footradius;
} rotarydelta_params_t;

static void rotarydelta_rotate(double *x, double *y, double theta)
{
    double xx = *x, yy = *y;
    *x = xx * cos(theta) - yy * sin(theta);
    *y = xx * sin(theta) + yy * cos(theta);
}

static int rotarydelta_inverse_j0(double x, double y, double z,
                                   const rotarydelta_params_t *params,
                                   double *theta)
{
    double a, b, d, knee_y, knee_z;
    double pfr = params->platformradius, tl = params->thighlength;
    double sl = params->shinlength, fr = params->footradius;
    a = 0.5 * (rotdelta_sq(x) + rotdelta_sq(y - fr) + rotdelta_sq(z) +
               rotdelta_sq(tl) - rotdelta_sq(sl) - rotdelta_sq(pfr)) / z;
    b = (fr - pfr - y) / z;
    d = rotdelta_sq(tl) * (rotdelta_sq(b) + 1) - rotdelta_sq(a - b * pfr);
    if (d < 0) return -1;
    knee_y = (pfr + a*b + sqrt(d)) / (rotdelta_sq(b) + 1);
    knee_z = b * knee_y - a;
    *theta = atan2(knee_z, knee_y - pfr) * 180.0/M_PI;
    return 0;
}

static int rotarydelta_inverse_math(const rotarydelta_params_t *params,
                                    const EmcPose *world, double *joints)
{
    double xr, yr;
    if (rotarydelta_inverse_j0(world->tran.x, world->tran.y, world->tran.z,
                                params, &joints[0]))
        return -1;
    xr = world->tran.x; yr = world->tran.y;
    rotarydelta_rotate(&xr, &yr, -2*M_PI/3);
    if (rotarydelta_inverse_j0(xr, yr, world->tran.z, params, &joints[1]))
        return -1;
    xr = world->tran.x; yr = world->tran.y;
    rotarydelta_rotate(&xr, &yr, 2*M_PI/3);
    if (rotarydelta_inverse_j0(xr, yr, world->tran.z, params, &joints[2]))
        return -1;
    joints[3] = world->a; joints[4] = world->b; joints[5] = world->c;
    joints[6] = world->u; joints[7] = world->v; joints[8] = world->w;
    return 0;
}

static int rotarydelta_forward_math(const rotarydelta_params_t *params,
                                    const double *joints, EmcPose *world)
{
    double pfr = params->platformradius, tl = params->thighlength;
    double sl = params->shinlength, fr = params->footradius;
    double j0, j1, j2, y1, z1, x2, y2, z2, x3, y3, z3;
    double a1, b1, a2, b2, w1, w2, w3, denom, a, b, c, d;
    j0 = rotdelta_D2R(joints[0]); j1 = rotdelta_D2R(joints[1]); j2 = rotdelta_D2R(joints[2]);
    y1 = -(pfr - fr + tl * cos(j0)); z1 = -tl * sin(j0);
    y2 = (pfr - fr + tl * cos(j1)) * 0.5; x2 = y2 * sqrt(3); z2 = -tl * sin(j1);
    y3 = (pfr - fr + tl * cos(j2)) * 0.5; x3 = -y3 * sqrt(3); z3 = -tl * sin(j2);
    denom = x3 * (y2 - y1) - x2 * (y3 - y1);
    w1 = rotdelta_sq(y1) + rotdelta_sq(z1);
    w2 = rotdelta_sq(x2) + rotdelta_sq(y2) + rotdelta_sq(z2);
    w3 = rotdelta_sq(x3) + rotdelta_sq(y3) + rotdelta_sq(z3);
    a1 = (z2-z1)*(y3-y1) - (z3-z1)*(y2-y1);
    b1 = -((w2-w1)*(y3-y1) - (w3-w1)*(y2-y1)) / 2.0;
    a2 = -(z2 - z1)*x3 + (z3 - z1)*x2;
    b2 = ((w2 - w1)*x3 - (w3 - w1)*x2) / 2.0;
    a = rotdelta_sq(a1) + rotdelta_sq(a2) + rotdelta_sq(denom);
    b = 2 * (a1*b1 + a2*(b2 - y1*denom) - z1*rotdelta_sq(denom));
    c = (b2 - y1*denom)*(b2 - y1*denom) +
        rotdelta_sq(b1) + rotdelta_sq(denom)*(rotdelta_sq(z1) - rotdelta_sq(sl));
    d = rotdelta_sq(b) - 4*a*c;
    if (d < 0) return -1;
    world->tran.z = (-b - sqrt(d)) / (2*a);
    world->tran.x = (a1*world->tran.z + b1) / denom;
    world->tran.y = (a2*world->tran.z + b2) / denom;
    world->a = joints[3]; world->b = joints[4]; world->c = joints[5];
    world->u = joints[6]; world->v = joints[7]; world->w = joints[8];
    return 0;
}

#define ROTARYDELTA_DEFAULT_PLATFORMRADIUS 10.0
#define ROTARYDELTA_DEFAULT_THIGHLENGTH    10.0
#define ROTARYDELTA_DEFAULT_SHINLENGTH     14.0
#define ROTARYDELTA_DEFAULT_FOOTRADIUS      6.0

/* Global parameters for Python bindings */
static rotarydelta_params_t params = {
    ROTARYDELTA_DEFAULT_PLATFORMRADIUS,
    ROTARYDELTA_DEFAULT_THIGHLENGTH,
    ROTARYDELTA_DEFAULT_SHINLENGTH,
    ROTARYDELTA_DEFAULT_FOOTRADIUS
};

static void set_geometry(double pfr, double tl, double sl, double fr)
{
    params.platformradius = pfr;
    params.thighlength = tl;
    params.shinlength = sl;
    params.footradius = fr;
}

static object forward(double j0, double j1, double j2)
{
    double joints[9] = {j0, j1, j2};
    EmcPose pos;
    int result = rotarydelta_forward_math(&params, joints, &pos);
    if(result == 0)
        return make_tuple(pos.tran.x, pos.tran.y, pos.tran.z);
    return object();
}

static object inverse(double x, double y, double z)
{
    double joints[9];
    EmcPose pos = {{x,y,z}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int result = rotarydelta_inverse_math(&params, &pos, joints);
    if(result == 0)
        return make_tuple(joints[0], joints[1], joints[2]);
    return object();
}

static object get_geometry()
{
    return make_tuple(params.platformradius, params.thighlength,
                      params.shinlength, params.footradius);
}

BOOST_PYTHON_MODULE(rotarydeltakins)
{
    def("set_geometry", set_geometry);
    def("get_geometry", get_geometry);
    def("forward", forward);
    def("inverse", inverse);
}
