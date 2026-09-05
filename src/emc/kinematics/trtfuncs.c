/**************************************************************************
* Copyright 2016 Rudy du Preez <rudy@asmsa.co.za>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**************************************************************************/

/********************************************************************
* Kinematics functions (forward,inverse) for:
*  1) 5 axis mill (XYZAC)
*     This mill has a tilting table (A axis) and horizontal rotary
*     mounted to the table (C axis).
*  2) 5 axis mill (XYZBC)
*     This mill has a tilting table (B axis) and horizontal rotary
*     mounted to the table (C axis).
*
* Note: The directions of the rotational axes are the opposite of the
* conventional axis directions. See
* https://linuxcnc.org/docs/html/gcode/machining-center.html
*
* Written as pure functions of the parameter block (see kinematics.h):
* the geometry is the table below, the joint map comes from the block,
* and the tool length is p->tool.tran.z.
********************************************************************/

#include <rtapi_math.h>
#include <emcmotcfg.h>
#include <kinematics.h>

// the geometry both machines share, one pin each
const kins_param_desc TRT_PARAMS[] = {
    { "x-rot-point",             KINS_PARAM_FLOAT, KINS_IN, 0, 0.0 },
    { "y-rot-point",             KINS_PARAM_FLOAT, KINS_IN, 0, 0.0 },
    { "z-rot-point",             KINS_PARAM_FLOAT, KINS_IN, 0, 0.0 },
    { "x-offset",                KINS_PARAM_FLOAT, KINS_IN, 0, 0.0 },
    { "y-offset",                KINS_PARAM_FLOAT, KINS_IN, 0, 0.0 },
    { "z-offset",                KINS_PARAM_FLOAT, KINS_IN, 0, 0.0 },
    { "tool-offset",             KINS_PARAM_FLOAT, KINS_IN, 1, 0.0 },
    { "conventional-directions", KINS_PARAM_BIT,   KINS_IN, 0, 0.0 }, // default: false
};
const int TRT_NPARAMS = sizeof(TRT_PARAMS)/sizeof(TRT_PARAMS[0]);

enum { TRT_XR, TRT_YR, TRT_ZR, TRT_XO, TRT_YO, TRT_ZO, TRT_TOOL, TRT_CON };

// joint number assignments from the block (-1 ==> not assigned)
#define JX (p->joint_of_axis[0])
#define JY (p->joint_of_axis[1])
#define JZ (p->joint_of_axis[2])
#define JA (p->joint_of_axis[3])
#define JB (p->joint_of_axis[4])
#define JC (p->joint_of_axis[5])
#define JU (p->joint_of_axis[6])
#define JV (p->joint_of_axis[7])
#define JW (p->joint_of_axis[8])

// the direction sign the conventional-directions pin selects
#define CON(p) ((p)->geometry[TRT_CON] != 0 ? 1.0 : -1.0)

static int xyzac_forward(const kins_params *p, kins_scratch *s,
                         const double *joints,
                         EmcPose * pos,
                         const KINEMATICS_FORWARD_FLAGS * fflags,
                         KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)s;
    (void)fflags;
    (void)iflags;
    const double x_rot_point = p->geometry[TRT_XR];
    const double y_rot_point = p->geometry[TRT_YR];
    const double z_rot_point = p->geometry[TRT_ZR];
    const double          dt = p->tool.tran.z;
    const double          dy = p->geometry[TRT_YO];
    const double          dz = p->geometry[TRT_ZO] + dt;
    const double       a_rad = joints[JA]*TO_RAD;
    const double       c_rad = joints[JC]*TO_RAD;

    const double con = CON(p);

    pos->tran.x = +       cos(c_rad)              * (joints[JX]      - x_rot_point)
                  - con * sin(c_rad) * cos(a_rad) * (joints[JY] - dy - y_rot_point)
                  +       sin(c_rad) * sin(a_rad) * (joints[JZ] - dz - z_rot_point)
                  - con * sin(c_rad) * dy
                  + x_rot_point;

    pos->tran.y = + con * sin(c_rad)              * (joints[JX]      - x_rot_point)
                  +       cos(c_rad) * cos(a_rad) * (joints[JY] - dy - y_rot_point)
                  - con * cos(c_rad) * sin(a_rad) * (joints[JZ] - dz - z_rot_point)
                  +       cos(c_rad) * dy
                  + y_rot_point;

    pos->tran.z = + 0
                  + con * sin(a_rad) * (joints[JY] - dy - y_rot_point)
                  + cos(a_rad) * (joints[JZ] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->a = joints[JA];
    pos->c = joints[JC];

    // optional letters (specify with coordinates module parameter)
    pos->b = (JB != -1)? joints[JB] : 0;
    pos->u = (JU != -1)? joints[JU] : 0;
    pos->v = (JV != -1)? joints[JV] : 0;
    pos->w = (JW != -1)? joints[JW] : 0;

    return 0;
} // xyzac_forward()

static int xyzac_inverse(const kins_params *p, kins_scratch *s,
                         const EmcPose * pos,
                         double *joints,
                         const KINEMATICS_INVERSE_FLAGS * iflags,
                         KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)s;
    (void)iflags;
    (void)fflags;
    const double x_rot_point = p->geometry[TRT_XR];
    const double y_rot_point = p->geometry[TRT_YR];
    const double z_rot_point = p->geometry[TRT_ZR];
    const double         dy  = p->geometry[TRT_YO];
    const double         dt  = p->tool.tran.z;
    const double         dz  = p->geometry[TRT_ZO] + dt;
    const double      a_rad  = pos->a*TO_RAD;
    const double      c_rad  = pos->c*TO_RAD;

    const double con = CON(p);

    EmcPose P; // computed position

    P.tran.x   = +       cos(c_rad)              * (pos->tran.x - x_rot_point)
                 + con * sin(c_rad)              * (pos->tran.y - y_rot_point)
                 + x_rot_point;

    P.tran.y   = - con * sin(c_rad) * cos(a_rad) * (pos->tran.x - x_rot_point)
                 +       cos(c_rad) * cos(a_rad) * (pos->tran.y - y_rot_point)
                 + con *              sin(a_rad) * (pos->tran.z - z_rot_point)
                 -                    cos(a_rad) * dy
                 - con *              sin(a_rad) * dz
                 + dy
                 + y_rot_point;

    P.tran.z   = +       sin(c_rad) * sin(a_rad) * (pos->tran.x - x_rot_point)
                 - con * cos(c_rad) * sin(a_rad) * (pos->tran.y - y_rot_point)
                 +                    cos(a_rad) * (pos->tran.z - z_rot_point)
                 + con *              sin(a_rad) * dy
                 -                    cos(a_rad) * dz
                 + dz
                 + z_rot_point;


    P.a        = pos->a;
    P.c        = pos->c;

    // optional letters (specify with coordinates module parameter)
    P.b = (JB != -1)? pos->b : 0;
    P.u = (JU != -1)? pos->u : 0;
    P.v = (JV != -1)? pos->v : 0;
    P.w = (JW != -1)? pos->w : 0;

    // update joints with support for
    // multiple-joints per-coordinate letter:
    // based on computed position
    return kinsPoseToMappedJoints(p, &P, joints);
} // xyzac_inverse()

static int xyzac_work_frame(const kins_params *p, const double *joints,
                            PmRotationMatrix *rot,
                            const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)fflags;
    // the forward transform's coefficients for a displacement of the X, Y and
    // Z joints are the rotation from machine into work, so the work frame in
    // machine coordinates is their transpose, written out directly here
    const double a_rad = joints[JA]*TO_RAD;
    const double c_rad = joints[JC]*TO_RAD;

    const double con = CON(p);

    rot->x.x =       cos(c_rad);
    rot->y.x = con * sin(c_rad);
    rot->z.x = 0;

    rot->x.y = - con * sin(c_rad) * cos(a_rad);
    rot->y.y =         cos(c_rad) * cos(a_rad);
    rot->z.y =   con *              sin(a_rad);

    rot->x.z =         sin(c_rad) * sin(a_rad);
    rot->y.z = - con * cos(c_rad) * sin(a_rad);
    rot->z.z =                      cos(a_rad);

    return 0;
} // xyzac_work_frame()

static int xyzac_jacobian(const kins_params *p, const double *joints,
                          const EmcPose *pos,
                          double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                          const KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)joints;
    (void)iflags;
    const double x_rot_point = p->geometry[TRT_XR];
    const double y_rot_point = p->geometry[TRT_YR];
    const double z_rot_point = p->geometry[TRT_ZR];
    const double         dy  = p->geometry[TRT_YO];
    const double         dt  = p->tool.tran.z;
    const double         dz  = p->geometry[TRT_ZO] + dt;
    const double          sa = sin(pos->a*TO_RAD), ca = cos(pos->a*TO_RAD);
    const double          sc = sin(pos->c*TO_RAD), cc = cos(pos->c*TO_RAD);
    const double          X = pos->tran.x - x_rot_point;
    const double          Y = pos->tran.y - y_rot_point;
    const double          Z = pos->tran.z - z_rot_point;
    double dP[EMCMOT_MAX_AXIS][EMCMOT_MAX_AXIS];
    int a, b;

    const double con = CON(p);

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        for (b = 0; b < EMCMOT_MAX_AXIS; b++) { dP[a][b] = 0; }
    }

    // the computed position P of xyzac_inverse(), differentiated: its
    // coefficients for x, y and z, and the same expressions with the
    // rotation taken a quarter turn on for a and for c
    dP[0][0] =       cc;
    dP[0][1] = con * sc;
    dP[0][5] = (-sc*X + con*cc*Y) * TO_RAD;

    dP[1][0] = - con * sc * ca;
    dP[1][1] =         cc * ca;
    dP[1][2] =   con *      sa;
    dP[1][3] = (con*sc*sa*X - cc*sa*Y + con*ca*Z + sa*dy - con*ca*dz) * TO_RAD;
    dP[1][5] = (-con*cc*ca*X - sc*ca*Y) * TO_RAD;

    dP[2][0] =         sc * sa;
    dP[2][1] = - con * cc * sa;
    dP[2][2] =              ca;
    dP[2][3] = (sc*ca*X - con*cc*ca*Y - sa*Z + con*ca*dy + sa*dz) * TO_RAD;
    dP[2][5] = (cc*sa*X + con*sc*sa*Y) * TO_RAD;

    for (a = 3; a < EMCMOT_MAX_AXIS; a++) { dP[a][a] = 1; }

    return kinsJacobianFromMappedAxesP(p, (const double (*)[EMCMOT_MAX_AXIS])dP,
                                       jac);
} // xyzac_jacobian()

// both rotaries carry the work, so the tool never turns in the machine:
// the tool frame is the shared identity one
const kins_ops XYZAC_OPS = {
    .forward  = xyzac_forward,
    .inverse  = xyzac_inverse,
    .work     = xyzac_work_frame,
    .tool     = kinsIdentityFrame,
    .native   = &TOOL_FRAME_SPINDLE,
    .jacobian = xyzac_jacobian,
};

static int xyzbc_forward(const kins_params *p, kins_scratch *s,
                         const double *joints,
                         EmcPose * pos,
                         const KINEMATICS_FORWARD_FLAGS * fflags,
                         KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)s;
    (void)fflags;
    (void)iflags;
    // Note: 'principal' joints are used
    const double x_rot_point = p->geometry[TRT_XR];
    const double y_rot_point = p->geometry[TRT_YR];
    const double z_rot_point = p->geometry[TRT_ZR];
    const double          dx = p->geometry[TRT_XO];
    const double          dt = p->tool.tran.z;
    const double          dz = p->geometry[TRT_ZO] + dt;
    const double       b_rad = joints[JB]*TO_RAD;
    const double       c_rad = joints[JC]*TO_RAD;

    const double con = CON(p);

    pos->tran.x =         cos(c_rad) * cos(b_rad) * (joints[JX] - dx - x_rot_point)
                  - con * sin(c_rad) *              (joints[JY]      - y_rot_point)
                  + con * cos(c_rad) * sin(b_rad) * (joints[JZ] - dz - z_rot_point)
                  +       cos(c_rad) * dx
                  + x_rot_point;

    pos->tran.y = + con * sin(c_rad) * cos(b_rad) * (joints[JX] - dx - x_rot_point)
                  +       cos(c_rad) *              (joints[JY]      - y_rot_point)
                  +       sin(c_rad) * sin(b_rad) * (joints[JZ] - dz - z_rot_point)
                  + con * sin(c_rad) * dx
                  + y_rot_point;

    pos->tran.z = - con * sin(b_rad) * (joints[JX] - dx - x_rot_point)
                  +       cos(b_rad) * (joints[JZ] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->b = joints[JB];
    pos->c = joints[JC];

    // optional letters (specify with coordinates module parameter)
    pos->a = (JA != -1)? joints[JA] : 0;
    pos->u = (JU != -1)? joints[JU] : 0;
    pos->v = (JV != -1)? joints[JV] : 0;
    pos->w = (JW != -1)? joints[JW] : 0;

    return 0;
} // xyzbc_forward()

static int xyzbc_inverse(const kins_params *p, kins_scratch *s,
                         const EmcPose * pos,
                         double *joints,
                         const KINEMATICS_INVERSE_FLAGS * iflags,
                         KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)s;
    (void)iflags;
    (void)fflags;
    const double x_rot_point = p->geometry[TRT_XR];
    const double y_rot_point = p->geometry[TRT_YR];
    const double z_rot_point = p->geometry[TRT_ZR];
    const double          dx = p->geometry[TRT_XO];
    const double          dt = p->tool.tran.z;
    const double          dz = p->geometry[TRT_ZO] + dt;
    const double       b_rad = pos->b*TO_RAD;
    const double       c_rad = pos->c*TO_RAD;

    const double con = CON(p);

    // the offsets seen from the tilted table: the same rotation the
    // forward applies to them, in the same sense
    const double         dpx = -cos(b_rad)*dx + con * sin(b_rad)*dz + dx;
    const double         dpz = -con * sin(b_rad)*dx - cos(b_rad)*dz + dz;

    EmcPose P; // computed position

    P.tran.x   = +       cos(c_rad) * cos(b_rad) * (pos->tran.x - x_rot_point)
                 + con * sin(c_rad) * cos(b_rad) * (pos->tran.y - y_rot_point)
                 - con *              sin(b_rad) * (pos->tran.z - z_rot_point)
                 + dpx
                 + x_rot_point;

    P.tran.y   = - con * sin(c_rad) * (pos->tran.x - x_rot_point)
                 +       cos(c_rad) * (pos->tran.y - y_rot_point)
                 + y_rot_point;

    P.tran.z   = + con * cos(c_rad) * sin(b_rad) * (pos->tran.x - x_rot_point)
                 +       sin(c_rad) * sin(b_rad) * (pos->tran.y - y_rot_point)
                 +                    cos(b_rad) * (pos->tran.z - z_rot_point)
                 + dpz
                 + z_rot_point;

    P.b        = pos->b;
    P.c        = pos->c;

    // optional letters (specify with coordinates module parameter)
    P.a = (JA != -1)? pos->a : 0;
    P.u = (JU != -1)? pos->u : 0;
    P.v = (JV != -1)? pos->v : 0;
    P.w = (JW != -1)? pos->w : 0;

    // update joints with support for
    // multiple-joints per-coordinate letter:
    // based on computed position
    return kinsPoseToMappedJoints(p, &P, joints);
} // xyzbc_inverse()

static int xyzbc_work_frame(const kins_params *p, const double *joints,
                            PmRotationMatrix *rot,
                            const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)fflags;
    // see the comment in xyzac_work_frame()
    const double b_rad = joints[JB]*TO_RAD;
    const double c_rad = joints[JC]*TO_RAD;

    const double con = CON(p);

    rot->x.x =         cos(c_rad) * cos(b_rad);
    rot->y.x =   con * sin(c_rad) * cos(b_rad);
    rot->z.x = - con *              sin(b_rad);

    rot->x.y = - con * sin(c_rad);
    rot->y.y =         cos(c_rad);
    rot->z.y = 0;

    rot->x.z =   con * cos(c_rad) * sin(b_rad);
    rot->y.z =         sin(c_rad) * sin(b_rad);
    rot->z.z =                      cos(b_rad);

    return 0;
} // xyzbc_work_frame()

static int xyzbc_jacobian(const kins_params *p, const double *joints,
                          const EmcPose *pos,
                          double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                          const KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)joints;
    (void)iflags;
    const double x_rot_point = p->geometry[TRT_XR];
    const double y_rot_point = p->geometry[TRT_YR];
    const double z_rot_point = p->geometry[TRT_ZR];
    const double          dx = p->geometry[TRT_XO];
    const double          dt = p->tool.tran.z;
    const double          dz = p->geometry[TRT_ZO] + dt;
    const double          sb = sin(pos->b*TO_RAD), cb = cos(pos->b*TO_RAD);
    const double          sc = sin(pos->c*TO_RAD), cc = cos(pos->c*TO_RAD);
    const double          X = pos->tran.x - x_rot_point;
    const double          Y = pos->tran.y - y_rot_point;
    const double          Z = pos->tran.z - z_rot_point;
    double dP[EMCMOT_MAX_AXIS][EMCMOT_MAX_AXIS];
    int a, b;

    const double con = CON(p);

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        for (b = 0; b < EMCMOT_MAX_AXIS; b++) { dP[a][b] = 0; }
    }

    // see the comment in xyzac_jacobian(); dpx and dpz of the inverse
    // depend on b as well
    dP[0][0] =       cc * cb;
    dP[0][1] = con * sc * cb;
    dP[0][2] = - con *    sb;
    dP[0][4] = (-cc*sb*X - con*sc*sb*Y - con*cb*Z + sb*dx + con*cb*dz) * TO_RAD;
    dP[0][5] = (-sc*cb*X + con*cc*cb*Y) * TO_RAD;

    dP[1][0] = - con * sc;
    dP[1][1] =         cc;
    dP[1][5] = (-con*cc*X - sc*Y) * TO_RAD;

    dP[2][0] = con * cc * sb;
    dP[2][1] =       sc * sb;
    dP[2][2] =            cb;
    dP[2][4] = (con*cc*cb*X + sc*cb*Y - sb*Z - con*cb*dx + sb*dz) * TO_RAD;
    dP[2][5] = (-con*sc*sb*X + cc*sb*Y) * TO_RAD;

    for (a = 3; a < EMCMOT_MAX_AXIS; a++) { dP[a][a] = 1; }

    return kinsJacobianFromMappedAxesP(p, (const double (*)[EMCMOT_MAX_AXIS])dP,
                                       jac);
} // xyzbc_jacobian()

const kins_ops XYZBC_OPS = {
    .forward  = xyzbc_forward,
    .inverse  = xyzbc_inverse,
    .work     = xyzbc_work_frame,
    .tool     = kinsIdentityFrame,
    .native   = &TOOL_FRAME_SPINDLE,
    .jacobian = xyzbc_jacobian,
};
