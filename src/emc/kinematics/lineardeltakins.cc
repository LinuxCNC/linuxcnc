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

#define lindelta_sq(x) ((x)*(x))

static void lineardelta_compute_geometry(const lineardelta_params_t *params,
                                         lineardelta_geometry_t *geom)
{
    double R = params->radius;
    double L = params->rod_length;
    geom->ax = 0.0; geom->ay = R;
    geom->bx = -LINDELTA_SIN_60 * R; geom->by = -LINDELTA_COS_60 * R;
    geom->cx = LINDELTA_SIN_60 * R; geom->cy = -LINDELTA_COS_60 * R;
    geom->l2 = lindelta_sq(L);
}

static int lineardelta_inverse_math(const lineardelta_params_t *params,
                                    const EmcPose *world, double *joints)
{
    lineardelta_geometry_t geom;
    double x = world->tran.x, y = world->tran.y, z = world->tran.z;
    lineardelta_compute_geometry(params, &geom);
    joints[0] = z + sqrt(geom.l2 - lindelta_sq(geom.ax - x) - lindelta_sq(geom.ay - y));
    joints[1] = z + sqrt(geom.l2 - lindelta_sq(geom.bx - x) - lindelta_sq(geom.by - y));
    joints[2] = z + sqrt(geom.l2 - lindelta_sq(geom.cx - x) - lindelta_sq(geom.cy - y));
    joints[3] = world->a; joints[4] = world->b; joints[5] = world->c;
    joints[6] = world->u; joints[7] = world->v; joints[8] = world->w;
    if (std::isnan(joints[0]) || std::isnan(joints[1]) || std::isnan(joints[2]))
        return -1;
    return 0;
}

static int lineardelta_forward_math(const lineardelta_params_t *params,
                                    const double *joints, EmcPose *world)
{
    lineardelta_geometry_t geom;
    double q1, q2, q3, den, w1, w2, w3, a1, b1, a2, b2, a, b, c, discr, z;
    lineardelta_compute_geometry(params, &geom);
    q1 = joints[0]; q2 = joints[1]; q3 = joints[2];
    den = (geom.by - geom.ay) * geom.cx - (geom.cy - geom.ay) * geom.bx;
    w1 = lindelta_sq(geom.ay) + lindelta_sq(q1);
    w2 = lindelta_sq(geom.bx) + lindelta_sq(geom.by) + lindelta_sq(q2);
    w3 = lindelta_sq(geom.cx) + lindelta_sq(geom.cy) + lindelta_sq(q3);
    a1 = (q2 - q1) * (geom.cy - geom.ay) - (q3 - q1) * (geom.by - geom.ay);
    b1 = -((w2 - w1) * (geom.cy - geom.ay) - (w3 - w1) * (geom.by - geom.ay)) / 2.0;
    a2 = -(q2 - q1) * geom.cx + (q3 - q1) * geom.bx;
    b2 = ((w2 - w1) * geom.cx - (w3 - w1) * geom.bx) / 2.0;
    a = lindelta_sq(a1) + lindelta_sq(a2) + lindelta_sq(den);
    b = 2 * (a1 * b1 + a2 * (b2 - geom.ay * den) - q1 * lindelta_sq(den));
    c = lindelta_sq(b2 - geom.ay * den) + lindelta_sq(b1) +
        lindelta_sq(den) * (lindelta_sq(q1) - geom.l2);
    discr = lindelta_sq(b) - 4.0 * a * c;
    if (discr < 0) return -1;
    z = -0.5 * (b + sqrt(discr)) / a;
    world->tran.z = z;
    world->tran.x = (a1 * z + b1) / den;
    world->tran.y = (a2 * z + b2) / den;
    world->a = joints[3]; world->b = joints[4]; world->c = joints[5];
    world->u = joints[6]; world->v = joints[7]; world->w = joints[8];
    return 0;
}

#define LINEARDELTA_SMOOTH_ROD_OFFSET 198.25
#define LINEARDELTA_EFFECTOR_OFFSET 33.0
#define LINEARDELTA_CARRIAGE_OFFSET 35.0
#define LINEARDELTA_DEFAULT_RADIUS (LINEARDELTA_SMOOTH_ROD_OFFSET - \
                                     LINEARDELTA_EFFECTOR_OFFSET - \
                                     LINEARDELTA_CARRIAGE_OFFSET)
#define LINEARDELTA_DEFAULT_ROD_LENGTH 269.0

/* Module-level geometry parameters */
static lineardelta_params_t g_params = {
    LINEARDELTA_DEFAULT_RADIUS,
    LINEARDELTA_DEFAULT_ROD_LENGTH
};

static void py_set_geometry(double r, double l)
{
    g_params.radius = r;
    g_params.rod_length = l;
}

static object forward(double j0, double j1, double j2)
{
    double joints[9] = {j0, j1, j2, 0, 0, 0, 0, 0, 0};
    EmcPose pos;
    int result = lineardelta_forward_math(&g_params, joints, &pos);
    if(result == 0)
        return make_tuple(pos.tran.x, pos.tran.y, pos.tran.z);
    return object();
}

static object inverse(double x, double y, double z)
{
    double joints[9];
    EmcPose pos = {{x,y,z}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int result = lineardelta_inverse_math(&g_params, &pos, joints);
    if(result == 0)
        return make_tuple(joints[0], joints[1], joints[2]);
    return object();
}

static object get_geometry()
{
    return make_tuple(g_params.radius, g_params.rod_length);
}

BOOST_PYTHON_MODULE(lineardeltakins)
{
    def("set_geometry", py_set_geometry);
    def("get_geometry", get_geometry);
    def("forward", forward);
    def("inverse", inverse);
}
