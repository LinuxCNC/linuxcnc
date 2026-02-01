//
// This is a kinematics module for the Scorbot ER 3.
//
// Copyright (C) 2015-2016 Sebastian Kuzminsky <seb@highlab.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

//
// The origin of the G53 coordinate system is at the center of rotation of
// joint J0, and at the bottom of the base plate.
//
// FIXME: The origin should probably be at the bottom of the base (part 5
// in the parts diagram on page 7-11 of the SCORBOT-ER III User's Manual).
//
// Joint 0 is rotation around the Z axis.  It chooses the plane that
// the rest of the arm moves in.
//
// Joint 1 is the shoulder.
//
// Joint 2 is the elbow.
//
// Joint 3 is pitch of the wrist, joint 4 is roll of the wrist.  These are
// converted to motor actuations by an external differential comp in HAL.
//


#include "kinematics.h"
#include "scorbotkins_math.h"

// Static params struct with default Scorbot ER-3 dimensions
static scorbot_params_t scorbot_params = {
    .l0_horizontal = SCORBOT_DEFAULT_L0_HORIZONTAL,
    .l0_vertical   = SCORBOT_DEFAULT_L0_VERTICAL,
    .l1_length     = SCORBOT_DEFAULT_L1_LENGTH,
    .l2_length     = SCORBOT_DEFAULT_L2_LENGTH
};


// Forward kinematics takes the joint positions and computes the cartesian
// coordinates of the controlled point.
int kinematicsForward(
    const double *joints,
    EmcPose *pose,
    const KINEMATICS_FORWARD_FLAGS *fflags,
    KINEMATICS_INVERSE_FLAGS *iflags
) {
    (void)fflags;
    (void)iflags;
    return scorbot_forward_math(&scorbot_params, joints, pose);
}


//
// Inverse kinematics takes the cartesian coordinates of the controlled
// point and computes corresponding the joint positions.
//
// Joint 0 rotates the arm around the base.  The rest of the joints are
// confined to the vertical plane containing J0, and rotated around the
// vertical at J0.  This kinematics code calls this plane the "RZ" plane.
// The Z coordinate in this plane is the same as the Z coordinate in the
// "cartesian" coordinates of LinuxCNC's world space.  The R coordinate
// is the horizontal distance (ie, in the XY plane) of the controlled
// point from J0.
//
int kinematicsInverse(
    const EmcPose *pose,
    double *joints,
    const KINEMATICS_INVERSE_FLAGS *iflags,
    KINEMATICS_FORWARD_FLAGS *fflags
) {
    (void)iflags;
    (void)fflags;
    return scorbot_inverse_math(&scorbot_params, pose, joints);
}


KINEMATICS_TYPE kinematicsType(void) {
    return KINEMATICS_BOTH;
}


#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

const char* kinematicsGetName(void) { return "scorbot-kins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

static int comp_id;

int rtapi_app_main(void) {
    comp_id = hal_init("scorbot-kins");
    if (comp_id < 0) {
        return comp_id;
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}

