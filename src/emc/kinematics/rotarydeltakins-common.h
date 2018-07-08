
//    Copyright 2013 Chris Radek <chris@timeguy.com>
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


/*
  Based on work by "mzavatsky" at:
  http://forums.trossenrobotics.com/tutorials/introduction-129/delta-robot-kinematics-3276/

  "You can freely use this code in your applications."

  which was based on:

  Descriptive Geometric Kinematic Analysis of Clavel's "Delta" Robot
  P.J. Zsombor-Murray, McGill University

  "... the purpose of this article: to provide a clear kinematic
  analysis useful to those who may wish to program and employ nice
  little three legged robots ..."


  The platform is on "top", the origin is in the center of the plane
  containing the three hip joints.  Z points upward, so Z coordinates
  are always negative.  Thighs always point outward, straight out
  (knee at Z=0) is considerd zero degrees for the angular hip joint.
  Positive rotation is knee-downward, so if you rotate all knees
  positive, the Z coordinate will get more negative.

  Joint zero is the one whose thigh swings in the YZ plane.
*/

#ifndef LINUXCNCROTARYDELTAKINS_COMMON_H
#define LINUXCNCROTARYDELTAKINS_COMMON_H

#include "emcpos.h"
// distance from origin to a hip joint
static double platformradius;

// thigh connects the hip to the knee
static double thighlength;

// shin (the parallelogram) connects the knee to the foot
static double shinlength;

// distance from center of foot (controlled point) to an ankle joint
static double footradius;

#ifndef sq
#define sq(a) ((a)*(a))
#endif
#ifndef D2R
#define D2R(d) ((d)*M_PI/180.)
#endif

static void set_geometry(double pfr, double tl, double sl, double fr) {
    platformradius = pfr;
    thighlength = tl;
    shinlength = sl;
    footradius = fr;
}

// Given three hip joint angles, find the controlled point
static int kinematics_forward(const double *joints, EmcPose *pos) {
    double
        j0 = joints[0],
        j1 = joints[1],
        j2 = joints[2],
        y1, z1, // x1 is 0
        x2, y2, z2, x3, y3, z3,
        a1, b1, a2, b2,
        w1, w2, w3,
        denom,
        a, b, c, d;

    j0 = D2R(j0);
    j1 = D2R(j1);
    j2 = D2R(j2);

    y1 = -(platformradius - footradius + thighlength * cos(j0));
    z1 = -thighlength * sin(j0);

    y2 = (platformradius - footradius + thighlength * cos(j1)) * 0.5;
    x2 = y2 * sqrt(3);
    z2 = -thighlength * sin(j1);

    y3 = (platformradius - footradius + thighlength * cos(j2)) * 0.5;
    x3 = -y3 * sqrt(3);
    z3 = -thighlength * sin(j2);

    denom = x3 * (y2 - y1)  - x2 * (y3 - y1);

    w1 = sq(y1) + sq(z1);
    w2 = sq(x2) + sq(y2) + sq(z2);
    w3 = sq(x3) + sq(y3) + sq(z3);

    a1 = (z2-z1) * (y3-y1) - (z3-z1) * (y2-y1);
    b1 = -((w2-w1) * (y3-y1) - (w3-w1) * (y2-y1)) / 2.0;

    a2 = -(z2 - z1) * x3 + (z3 - z1) * x2;
    b2 = ((w2 - w1) * x3 - (w3 - w1) * x2) / 2.0;

    // a*z^2 + b*z + c = 0
    a = sq(a1) + sq(a2) + sq(denom);
    b = 2 * (a1 * b1 + a2 * (b2 - y1 * denom) - z1 * sq(denom));
    c = (b2 - y1 * denom) * (b2 - y1 * denom) +
        sq(b1) + sq(denom) * (sq(z1) - sq(shinlength));

    d = sq(b) - 4 * a * c;
    if (d < 0) return -1;

    pos->tran.z = (-b - sqrt(d)) / (2 * a);
    pos->tran.x = (a1 * pos->tran.z + b1) / denom;
    pos->tran.y = (a2 * pos->tran.z + b2) / denom;

    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    pos->u = joints[6];
    pos->v = joints[7];
    pos->w = joints[8];

    return 0;
}


// Given controlled point, find joint zero's angle
// (J0 is the easy one in the ZY plane)
static int inverse_j0(double x, double y, double z, double *theta) {
    double a, b, d, knee_y, knee_z;

    a = 0.5 * (sq(x) + sq(y - footradius) + sq(z) + sq(thighlength) -
               sq(shinlength) - sq(platformradius)) / z;
    b = (footradius - platformradius - y) / z;

    d = sq(thighlength) * (sq(b) + 1) - sq(a - b * platformradius);
    if (d < 0) return -1;

    knee_y = (platformradius + a*b + sqrt(d)) / (sq(b) + 1);
    knee_z = b * knee_y - a;

    *theta = atan2(knee_z, knee_y - platformradius);
    *theta *= 180.0/M_PI;
    return 0;
}

static void rotate(double *x, double *y, double theta) {
    double xx, yy;
    xx = *x, yy = *y;
    *x = xx * cos(theta) - yy * sin(theta);
    *y = xx * sin(theta) + yy * cos(theta);
}

static int kinematics_inverse(const EmcPose *pos, double *joints) {
    double xr, yr;
    if(inverse_j0(pos->tran.x, pos->tran.y, pos->tran.z, &joints[0])) return -1;

    // now use symmetry property to get the other two just as easily...
    xr = pos->tran.x; yr = pos->tran.y;
    rotate(&xr, &yr, -2*M_PI/3);
    if(inverse_j0(xr, yr, pos->tran.z, &joints[1])) return -1;

    xr = pos->tran.x; yr = pos->tran.y;
    rotate(&xr, &yr, 2*M_PI/3);
    if(inverse_j0(xr, yr, pos->tran.z, &joints[2])) return -1;

    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
}

#define RDELTA_PFR 10.0
#define RDELTA_TL 10.0
#define RDELTA_SL 14.0
#define RDELTA_FR 6.0

#endif
