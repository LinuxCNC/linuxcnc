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
#define isnan(x) std::isnan(x)
#include "lineardeltakins-common.h"

static object forward(double j0, double j1, double j2)
{
    double joints[9] = {j0, j1, j2};
    EmcPose pos;
    int result = kinematics_forward(joints, &pos);
    if(result == 0)
        return make_tuple(pos.tran.x, pos.tran.y, pos.tran.z);
    return object();
}

static object inverse(double x, double y, double z)
{
    double joints[9];
    EmcPose pos = {{x,y,z}};
    int result = kinematics_inverse(&pos, joints);
    if(result == 0)
        return make_tuple(joints[0], joints[1], joints[2]);
    return object();
}

static object get_geometry()
{
    return make_tuple(R, L);
}

BOOST_PYTHON_MODULE(lineardeltakins)
{
    set_geometry(DELTA_RADIUS, DELTA_DIAGONAL_ROD);
    def("set_geometry", set_geometry);
    def("get_geometry", get_geometry);
    def("forward", forward);
    def("inverse", inverse);
}
