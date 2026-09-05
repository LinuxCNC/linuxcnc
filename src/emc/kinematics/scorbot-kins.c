
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


#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>
#include <kinematics.h>
#include <kins_rt.h>


//
// linkage constants, in mm & degrees
//

// Link 0 connects the origin to J1 (shoulder)
// These dimensions come off a drawing I got from Intelitek.
#define L0_HORIZONTAL_DISTANCE 16
#define L0_VERTICAL_DISTANCE   140

#define L1_LENGTH 221  // Link 1 connects J1 (shoulder) to J2 (elbow)
#define L2_LENGTH 221  // Link 2 connects J2 (shoulder) to the wrist


// Compute the cartesian coordinates of J1, given the J0 angle (and the
// fixed, known link L0 between J0 and J1).
static void compute_j1_cartesian_location(double j0, EmcPose *j1_cart) {
    j1_cart->tran.x = L0_HORIZONTAL_DISTANCE * cos(TO_RAD * j0);
    j1_cart->tran.y =  L0_HORIZONTAL_DISTANCE * sin(TO_RAD * j0);
    j1_cart->tran.z = L0_VERTICAL_DISTANCE;
    j1_cart->a = 0;
    j1_cart->b = 0;
    j1_cart->c = 0;
    j1_cart->u = 0;
    j1_cart->v = 0;
    j1_cart->w = 0;
}


// Forward kinematics takes the joint positions and computes the cartesian
// coordinates of the controlled point.
static int scorbot_forward(
    const kins_params *p, kins_scratch *s,
    const double *joints,
    EmcPose *pose,
    const KINEMATICS_FORWARD_FLAGS *fflags,
    KINEMATICS_INVERSE_FLAGS *iflags
) {
    (void)p;
    (void)s;
    (void)fflags;
    (void)iflags;
    EmcPose j1_vector;  // the vector from j0 ("base") to joint 1 ("shoulder", end of link 0)
    EmcPose j2_vector;  // the vector from j1 ("shoulder") to  joint 2 ("elbow", end of link 1)
    EmcPose j3_vector;  // the vector from j2 ("elbow") to joint 3 ("wrist", end of link 2)

    double r;

    compute_j1_cartesian_location(joints[0], &j1_vector);

    // Link 1 connects j1 (shoulder) to j2 (elbow).
    r = L1_LENGTH * cos(TO_RAD * joints[1]);
    j2_vector.tran.x = r * cos(TO_RAD * joints[0]);
    j2_vector.tran.y =  r * sin(TO_RAD * joints[0]);
    j2_vector.tran.z = L1_LENGTH * sin(TO_RAD * joints[1]);

    // Link 2 connects j2 (elbow) to j3 (wrist).
    // J3 is the controlled point.
    r = L2_LENGTH * cos(TO_RAD * joints[2]);
    j3_vector.tran.x = r * cos(TO_RAD * joints[0]);
    j3_vector.tran.y =  r * sin(TO_RAD * joints[0]);
    j3_vector.tran.z = L2_LENGTH * sin(TO_RAD * joints[2]);

    // The end-effector location is the sum of the linkage vectors.
    pose->tran.x = j1_vector.tran.x + j2_vector.tran.x + j3_vector.tran.x;
    pose->tran.y = j1_vector.tran.y + j2_vector.tran.y + j3_vector.tran.y;
    pose->tran.z = j1_vector.tran.z + j2_vector.tran.z + j3_vector.tran.z;

    // A and B are wrist roll and pitch, handled in hal by external kinematics
    pose->a = joints[3];
    pose->b = joints[4];

    return 0;
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
static int scorbot_inverse(
    const kins_params *p, kins_scratch *s,
    const EmcPose *pose,
    double *joints,
    const KINEMATICS_INVERSE_FLAGS *iflags,
    KINEMATICS_FORWARD_FLAGS *fflags
) {
    (void)p;
    (void)s;
    (void)iflags;
    (void)fflags;
    double distance_to_cp, distance_to_center;
    double r_j1, z_j1;   // (r_j1, z_j1) is the location of J1 in the RZ plane
    double r_cp, z_cp;   // (r_cp, z_cp) is the location of the controlled point in the RZ plane
    double angle_to_cp;
    double j1_angle;

    // the location of J2, this is what we're trying to find
    double z_j2;

    // J0 is easy.  Project the (X, Y, Z) of the pose onto the Z=0 plane.
    // J0 points at the projected (X, Y) point.  tan(J0) = Y/X
    // J0 then defines the plane that the rest of the arm operates in.
    joints[0] = TO_DEG * atan2(pose->tran.y, pose->tran.x);

    // FIXME: Until i figure the wrist differential out, the controlled
    //     point will be the location of the wrist joint, J3/J4.

    // The location of J1 (computed above) and the location of the
    // controlled point are separated by J1, L1, J2, and L2.  L1 and L2 are
    // known, but J1 and J2 are not.

    // (r_j1, z_j1) is the location of J1 in the RZ plane (the vertical
    // plane defined by the angle of J0, with the origin at the location
    // of J0.  This is just a known, static vector.
    r_j1 = L0_HORIZONTAL_DISTANCE;
    z_j1 = L0_VERTICAL_DISTANCE;

    // (r_cp, z_cp) is the location of J3 (the controlled point), again in
    // the plane defined by the angle of J0, with the origin of the
    // machine.
    r_cp = sqrt(pow(pose->tran.x, 2) +  pow(pose->tran.y, 2));
    z_cp = pose->tran.z;

    // translate so (r_j1, z_j1) is the origin of the coordinate system
    r_cp -= r_j1;
    z_cp -= z_j1;

    //
    // Now the origin (aka J1), J2, and CP define a triangle in the RZ plane.
    // The triangle is isosceles, because from the origin to J2 is L1, and
    // from J2 to CP is L2, and L1 and L2 are the same length.
    //
    // Bisect the base of that triangle, and call the center point of the
    // base "Center".
    //
    // Draw a line between J2 and Center.  This defines two right
    // triangles: (J1, J2, Center) and (CP, J2, Center).
    //
    // The length of the (J1, Center) and (CP, Center) lines are equal, and
    // are half the distance from the origin to CP.
    //

    distance_to_cp = sqrt(pow(r_cp, 2) + pow(z_cp, 2));
    distance_to_center = distance_to_cp / 2;

    // find the angle of the vector from the origin to the CP
    angle_to_cp = TO_DEG * acos(r_cp / distance_to_cp);
    if (z_cp < 0) {
        angle_to_cp *= -1;
    }

    // find the angle (Center, J1, J2)
    j1_angle = TO_DEG * acos(distance_to_center / L1_LENGTH);

    joints[1] = angle_to_cp + j1_angle;

    // now we can compute the location of J2
    z_j2 = L1_LENGTH * sin(TO_RAD * joints[1]);

    joints[2] = -1.0 * TO_DEG * asin((z_j2 - z_cp) / L2_LENGTH);

    // A and B are wrist roll and pitch, handled in hal by external kinematics
    joints[3] = pose->a;
    joints[4] = pose->b;

    return 0;
}


static int scorbot_jacobian(
    const kins_params *p,
    const double *joints,
    const EmcPose *pose,
    double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
    const KINEMATICS_INVERSE_FLAGS *iflags
) {
    // scorbot_inverse() above, differentiated step by step in the same
    // order, each quantity carried as its gradient over (x, y, z)
    const double x = pose->tran.x, y = pose->tran.y;
    const double rho2 = x*x + y*y;
    const double rho = sqrt(rho2);
    double r_cp, z_cp, dist, angle_to_cp, j1_angle, j1, z_j2, u;
    double d_r_cp[3], d_z_cp[3], d_dist[3], d_angle[3], d_j1a[3], d_j1[3], d_j2[3];
    double q;
    int i, j, a;

    (void)p;
    (void)joints;
    (void)iflags;
    if (rho2 <= 0) { return -1; }
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }

    // j0 = atan2(y, x)
    jac[0][0] = -y/rho2 * TO_DEG;
    jac[0][1] =  x/rho2 * TO_DEG;

    r_cp = rho - L0_HORIZONTAL_DISTANCE;
    z_cp = pose->tran.z - L0_VERTICAL_DISTANCE;
    d_r_cp[0] = x/rho; d_r_cp[1] = y/rho; d_r_cp[2] = 0;
    d_z_cp[0] = 0;     d_z_cp[1] = 0;     d_z_cp[2] = 1;

    dist = sqrt(r_cp*r_cp + z_cp*z_cp);
    if (dist <= 0 || dist >= 2*L1_LENGTH) { return -1; }
    for (i = 0; i < 3; i++) {
        d_dist[i] = (r_cp*d_r_cp[i] + z_cp*d_z_cp[i]) / dist;
    }

    // the signed acos in the inverse is atan2(z_cp, r_cp)
    angle_to_cp = TO_DEG * atan2(z_cp, r_cp);
    for (i = 0; i < 3; i++) {
        d_angle[i] = TO_DEG * (r_cp*d_z_cp[i] - z_cp*d_r_cp[i]) / (dist*dist);
    }

    q = dist / (2*L1_LENGTH);
    j1_angle = TO_DEG * acos(q);
    for (i = 0; i < 3; i++) {
        d_j1a[i] = -TO_DEG / sqrt(1 - q*q) * d_dist[i] / (2*L1_LENGTH);
    }

    j1 = angle_to_cp + j1_angle;
    for (i = 0; i < 3; i++) {
        d_j1[i] = d_angle[i] + d_j1a[i];
        jac[1][i] = d_j1[i];
    }

    z_j2 = L1_LENGTH * sin(TO_RAD * j1);
    u = (z_j2 - z_cp) / L2_LENGTH;
    if (fabs(u) >= 1) { return -1; }
    for (i = 0; i < 3; i++) {
        double d_z_j2 = L1_LENGTH * cos(TO_RAD * j1) * TO_RAD * d_j1[i];
        d_j2[i] = -TO_DEG / sqrt(1 - u*u) * (d_z_j2 - d_z_cp[i]) / L2_LENGTH;
        jac[2][i] = d_j2[i];
    }

    jac[3][3] = 1;
    jac[4][4] = 1;
    return 0;
}

static const kins_ops scorbot_ops = {
    .forward  = scorbot_forward,
    .inverse  = scorbot_inverse,
    .jacobian = scorbot_jacobian,
};

// the arm's dimensions are the constants above; no geometry pins.  The
// entry points come from kins_single.c
const kins_module_info kins_module = {
    .name                 = "scorbot-kins",
    .halprefix            = "scorbot-kins",
    .params               = NULL,
    .nparams              = 0,
    .required_coordinates = "XYZAB",
    .max_joints           = 5,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &scorbot_ops },
};

MODULE_LICENSE("GPL");

static int comp_id;

int rtapi_app_main(void) {
    comp_id = hal_init("scorbot-kins");
    if (comp_id < 0) {
        return comp_id;
    }
    if (kinsSingleInit(comp_id, "XYZAB", KINEMATICS_BOTH)) {
        hal_exit(comp_id);
        return -1;
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}
