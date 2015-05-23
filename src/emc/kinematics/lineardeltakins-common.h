#ifndef LINUXCNCLINEARDELTAKINS_COMMON_H
#define LINUXCNCLINEARDELTAKINS_COMMON_H
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
/*
 * Kinematics for a rostock-style delta robot
 *
 * Towers 0, 1 and 2 are spaced at 120 degrees around the origin
 * at distance R.  A rod of length L (L > R) connects each tower to the
 * platform.  Tower 0 is at (0,R).  (note: this is not at zero radians!)
 *
 * ABCUVW coordinates are passed through in joints[3..8].
 *
 * L is like DELTA_DIAGONAL_ROD and R is like DELTA_RADIUS in
 * Marlin---remember to account for the effector and carriage offsets
 * when changing from the default.
 */

// common routines used by the userspace kinematics and the realtime kinematics
// user must include a math.h-type header first
// Inspired by Marlin delta firmware and https://gist.github.com/kastner/5279172
#include "emcpos.h"
#include "rtapi_math.h"

static double L, R, J0off, J1off, J2off;
static double Ax, Ay, Bx, By, Cx, Cy, L2;

#define SQ3    (rtapi_sqrt(3))

#define SIN_60 (SQ3/2)
#define COS_60 (.5)

static double sq(double x) { return x*x; }

static void set_geometry(double r_, double l_, double j0off_, double j1off_, double j2off_)
{
    if((L == l_) && (R == r_) && (J0off == j0off_) && (J1off == j1off_) && (J2off == j2off_)) return;

    L = l_;
    R = r_;

    L2 = sq(L);
    
    J0off = j0off_;
    J1off = j1off_;
    J2off = j2off_;
	
    Ax = 0.0;
    Ay = R;

    Bx = -SIN_60 * R;
    By = -COS_60 * R;

    Cx = SIN_60 * R;
    Cy = -COS_60 * R;
}

static int kinematics_inverse(const EmcPose *pos, double *joints)
{
    double x = pos->tran.x, y = pos->tran.y, z = pos->tran.z;
    joints[0] = (z + rtapi_sqrt(L2 - sq(Ax-x) - sq(Ay-y))) + J0off;
    joints[1] = (z + rtapi_sqrt(L2 - sq(Bx-x) - sq(By-y))) + J1off;
    joints[2] = (z + rtapi_sqrt(L2 - sq(Cx-x) - sq(Cy-y))) + J2off;
    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return rtapi_isnan(joints[0]) || rtapi_isnan(joints[1]) || rtapi_isnan(joints[2])
	? -1 : 0;
}

static int kinematics_forward(const double *joints, EmcPose *pos)
{
    double q1 = joints[0] - J0off;
    double q2 = joints[1] - J1off;
    double q3 = joints[2] - J2off;

    double den = (By-Ay)*Cx-(Cy-Ay)*Bx;

    double w1 = Ay*Ay + q1*q1; // n.b. assumption that Ax is 0 all through here
    double w2 = Bx*Bx + By*By + q2*q2;
    double w3 = Cx*Cx + Cy*Cy + q3*q3;

    double a1 = (q2-q1)*(Cy-Ay)-(q3-q1)*(By-Ay);
    double b1 = -((w2-w1)*(Cy-Ay)-(w3-w1)*(By-Ay))/2.0;

    double a2 = -(q2-q1)*Cx+(q3-q1)*Bx;
    double b2 = ((w2-w1)*Cx - (w3-w1)*Bx)/2.0;

    // a*z^2 + b*z + c = 0
    double a = a1*a1 + a2*a2 + den*den;
    double b = 2*(a1*b1 + a2*(b2-Ay*den) - q1*den*den);
    double c = (b2-Ay*den)*(b2-Ay*den) + b1*b1 + den*den*(q1*q1 - L*L);

    double discr = b*b - 4.0*a*c;
    if (discr < 0) return -1; // non-existing point

    double z = -0.5*(b+rtapi_sqrt(discr))/a;
    pos->tran.z = z;
    pos->tran.x = (a1*z + b1)/den;
    pos->tran.y = (a2*z + b2)/den;
    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    pos->u = joints[6];
    pos->v = joints[7];
    pos->w = joints[8];

    return 0;
}

// Default values which may correspond to someone's linear delta robot.  To
// change these, use halcmd setp rather than rebuilding the software.

// Center-to-center distance of the holes in the diagonal push rods.
#define DELTA_DIAGONAL_ROD 269.0 // mm

// Horizontal offset from middle of printer to smooth rod center.
#define DELTA_SMOOTH_ROD_OFFSET 198.25 // mm

// Horizontal offset of the universal joints on the end effector.
#define DELTA_EFFECTOR_OFFSET 33.0 // mm

// Horizontal offset of the universal joints on the carriages.
#define DELTA_CARRIAGE_OFFSET 35.0 // mm

// Horizontal offset of the universal joints on the carriages.
#define JOINT_0_OFFSET 0.00 // mm

// Horizontal offset of the universal joints on the carriages.
#define JOINT_1_OFFSET 0.00 // mm

// Horizontal offset of the universal joints on the carriages.
#define JOINT_2_OFFSET 0.00 // mm

// Effective horizontal distance bridged by diagonal push rods.
#define DELTA_RADIUS (DELTA_SMOOTH_ROD_OFFSET-DELTA_EFFECTOR_OFFSET-DELTA_CARRIAGE_OFFSET)
#endif
