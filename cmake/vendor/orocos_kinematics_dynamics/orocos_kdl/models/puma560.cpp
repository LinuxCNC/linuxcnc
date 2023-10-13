// Copyright  (C)  2009  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>

// Version: 1.0
// Author: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <chain.hpp>
#include "models.hpp"
#include <utilities/utility.h>

namespace KDL{
    Chain Puma560(){
        Chain puma560;
        puma560.addSegment(Segment());
        puma560.addSegment(Segment(Joint(Joint::RotZ),
                                   Frame::DH(0.0,PI_2,0.0,0.0),
                                   RigidBodyInertia(0,Vector::Zero(),RotationalInertia(0,0.35,0,0,0,0))));
        puma560.addSegment(Segment(Joint(Joint::RotZ),
                                   Frame::DH(0.4318,0.0,0.0,0.0),
                                   RigidBodyInertia(17.4,Vector(-.3638,.006,.2275),RotationalInertia(0.13,0.524,0.539,0,0,0))));
        puma560.addSegment(Segment());
        puma560.addSegment(Segment(Joint(Joint::RotZ),
                                   Frame::DH(0.0203,-PI_2,0.15005,0.0),
                                   RigidBodyInertia(4.8,Vector(-.0203,-.0141,.070),RotationalInertia(0.066,0.086,0.0125,0,0,0))));
        puma560.addSegment(Segment(Joint(Joint::RotZ),
                                   Frame::DH(0.0,PI_2,0.4318,0.0),
                                   RigidBodyInertia(0.82,Vector(0,.019,0),RotationalInertia(1.8e-3,1.3e-3,1.8e-3,0,0,0))));
        puma560.addSegment(Segment());
        puma560.addSegment(Segment());
        puma560.addSegment(Segment(Joint(Joint::RotZ),
                                   Frame::DH(0.0,-PI_2,0.0,0.0),
                                   RigidBodyInertia(0.34,Vector::Zero(),RotationalInertia(.3e-3,.4e-3,.3e-3,0,0,0))));
        puma560.addSegment(Segment(Joint(Joint::RotZ),
                                   Frame::DH(0.0,0.0,0.0,0.0),
                                   RigidBodyInertia(0.09,Vector(0,0,.032),RotationalInertia(.15e-3,0.15e-3,.04e-3,0,0,0))));
        puma560.addSegment(Segment());
        return puma560;
    }
    
}
