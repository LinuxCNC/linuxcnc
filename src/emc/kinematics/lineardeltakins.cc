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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <cmath>
#include "lineardeltakins-common.h"
#include <boost/python.hpp>
using namespace boost::python;

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
    return make_tuple(R, L ,J0off, J1off, J2off);
}

BOOST_PYTHON_MODULE(lineardeltakins)
{
    set_geometry(DELTA_RADIUS, DELTA_DIAGONAL_ROD,JOINT_0_OFFSET, JOINT_1_OFFSET, JOINT_2_OFFSET);
    def("set_geometry", set_geometry);
    def("get_geometry", get_geometry);
    def("forward", forward);
    def("inverse", inverse);
}
