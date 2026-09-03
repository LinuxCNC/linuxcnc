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

********************************************************************/

#include <rtapi_math.h>
#include <rtapi_string.h>
#include <rtapi_ctype.h>
#include <hal.h>
#include <emcmotcfg.h>
#include <kinematics.h>

static int trtfuncs_max_joints;

// joint number assignments (-1 ==> not assigned)
static int JX = -1;
static int JY = -1;
static int JZ = -1;

static int JA = -1;
static int JB = -1;
static int JC = -1;

static int JU = -1;
static int JV = -1;
static int JW = -1;

struct haldata {
    hal_real_t x_rot_point;
    hal_real_t y_rot_point;
    hal_real_t z_rot_point;
    hal_real_t x_offset;
    hal_real_t y_offset;
    hal_real_t z_offset;
    hal_real_t tool_offset;
    hal_bool_t conventional_directions; // default: false
} *haldata;


int trtKinematicsSetup(const int   comp_id,
                       const char* coordinates,
                       kparms*     kp)
{
    int i,jno,res=0;
    int axis_idx_for_jno[EMCMOT_MAX_JOINTS];
    int rqdjoints = strlen(kp->required_coordinates);

    if (rqdjoints > kp->max_joints) {
        rtapi_print_msg(RTAPI_MSG_ERR,
             "ERROR %s: supports %d joints, <%s> requires %d\n",
             kp->kinsname,
             kp->max_joints,
             coordinates,
             rqdjoints);
        goto error;
    }
    trtfuncs_max_joints = kp->max_joints;

    if (map_coordinates_to_jnumbers(coordinates,
                                    kp->max_joints,
                                    kp->allow_duplicates,
                                    axis_idx_for_jno)) {
       goto error;
    }
    // require all chars in reqd_coords (order doesn't matter)
    for (i=0; i < rqdjoints; i++) {
        char  reqd_char;
        reqd_char = *(kp->required_coordinates + i);
        if (   !strchr(coordinates,toupper(reqd_char))
            && !strchr(coordinates,tolower(reqd_char)) ) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                 "ERROR %s:\nrequired  coordinates:%s\n"
                           "specified coordinates:%s\n",
                 kp->kinsname, kp->required_coordinates, coordinates);
            goto error;
        }
    }

    // assign principal joint numbers (first found in coordinates map)
    // duplicates are handled by position_to_mapped_joints()
    for (jno=0; jno < EMCMOT_MAX_JOINTS; jno++) {
       if (axis_idx_for_jno[jno] == 0 && JX==-1) {JX = jno;}
       if (axis_idx_for_jno[jno] == 1 && JY==-1) {JY = jno;}
       if (axis_idx_for_jno[jno] == 2 && JZ==-1) {JZ = jno;}
       if (axis_idx_for_jno[jno] == 3 && JA==-1) {JA = jno;}
       if (axis_idx_for_jno[jno] == 4 && JB==-1) {JB = jno;}
       if (axis_idx_for_jno[jno] == 5 && JC==-1) {JC = jno;}
       if (axis_idx_for_jno[jno] == 6 && JU==-1) {JU = jno;}
       if (axis_idx_for_jno[jno] == 7 && JV==-1) {JV = jno;}
       if (axis_idx_for_jno[jno] == 8 && JW==-1) {JW = jno;}
    }

    rtapi_print("%s coordinates=%s assigns:\n", kp->kinsname,coordinates);
    for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
        if (axis_idx_for_jno[jno] == -1) break; //fini
        rtapi_print("   Joint %d ==> Axis %c\n",
                   jno,"XYZABCUVW"[axis_idx_for_jno[jno]]);
    }

    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata) {goto error;}

    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->x_rot_point),
                            0.0, "%s.x-rot-point",kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->y_rot_point),
                            0.0, "%s.y-rot-point",kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->z_rot_point),
                            0.0, "%s.z-rot-point",kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->x_offset),
                            0.0, "%s.x-offset",kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->y_offset),
                            0.0, "%s.y-offset",kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->z_offset),
                            0.0, "%s.z-offset",kp->halprefix);
    res += hal_pin_new_real(comp_id, HAL_IN, &(haldata->tool_offset),
                            0.0, "%s.tool-offset",kp->halprefix);
    res += hal_pin_new_bool(comp_id, HAL_IN, &(haldata->conventional_directions),
                            0, "%s.conventional-directions", kp->halprefix);
    if (res) {goto error;}
    return 0;

error:
    rtapi_print_msg(RTAPI_MSG_ERR,"trtKinematicsSetup() FAIL\n");
    return -1;
} // trtKinematicsSetup()

int xyzacKinematicsForward(const double *joints,
                           EmcPose * pos,
                           const KINEMATICS_FORWARD_FLAGS * fflags,
                           KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    const double x_rot_point = hal_get_real(haldata->x_rot_point);
    const double y_rot_point = hal_get_real(haldata->y_rot_point);
    const double z_rot_point = hal_get_real(haldata->z_rot_point);
    const double          dt = hal_get_real(haldata->tool_offset);
    const double          dy = hal_get_real(haldata->y_offset);
    const double          dz = hal_get_real(haldata->z_offset) + dt;
    const double       a_rad = joints[JA]*TO_RAD;
    const double       c_rad = joints[JC]*TO_RAD;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

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
} // xyzacKinematicsForward()

int xyzacKinematicsInverse(const EmcPose * pos,
                           double *joints,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    const double x_rot_point = hal_get_real(haldata->x_rot_point);
    const double y_rot_point = hal_get_real(haldata->y_rot_point);
    const double z_rot_point = hal_get_real(haldata->z_rot_point);
    const double         dy  = hal_get_real(haldata->y_offset);
    const double         dt  = hal_get_real(haldata->tool_offset);
    const double         dz  = hal_get_real(haldata->z_offset) + dt;
    const double      a_rad  = pos->a*TO_RAD;
    const double      c_rad  = pos->c*TO_RAD;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

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
    position_to_mapped_joints(trtfuncs_max_joints,
                              &P,
                              joints);

    return 0;
} // xyzacKinematicsInverse()

int xyzacKinematicsWorkFrame(const double *joints,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)fflags;
    // the forward transform's coefficients for a displacement of the X, Y and
    // Z joints are the rotation from machine into work, so the work frame in
    // machine coordinates is their transpose, written out directly here
    const double a_rad = joints[JA]*TO_RAD;
    const double c_rad = joints[JC]*TO_RAD;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

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
} // xyzacKinematicsWorkFrame()

int xyzacKinematicsToolFrame(const double *joints,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)joints;
    (void)fflags;
    // both rotaries carry the work, so the tool never turns in the machine
    *rot = TOOL_FRAME_SPINDLE;
    return 0;
} // xyzacKinematicsToolFrame()

int xyzacKinematicsJacobian(const double *joints,
                            const EmcPose *pos,
                            double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                            const KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)joints;
    (void)iflags;
    const double x_rot_point = hal_get_real(haldata->x_rot_point);
    const double y_rot_point = hal_get_real(haldata->y_rot_point);
    const double z_rot_point = hal_get_real(haldata->z_rot_point);
    const double         dy  = hal_get_real(haldata->y_offset);
    const double         dt  = hal_get_real(haldata->tool_offset);
    const double         dz  = hal_get_real(haldata->z_offset) + dt;
    const double          sa = sin(pos->a*TO_RAD), ca = cos(pos->a*TO_RAD);
    const double          sc = sin(pos->c*TO_RAD), cc = cos(pos->c*TO_RAD);
    const double          X = pos->tran.x - x_rot_point;
    const double          Y = pos->tran.y - y_rot_point;
    const double          Z = pos->tran.z - z_rot_point;
    double dP[EMCMOT_MAX_AXIS][EMCMOT_MAX_AXIS];
    int a, b;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        for (b = 0; b < EMCMOT_MAX_AXIS; b++) { dP[a][b] = 0; }
    }

    // the computed position P of xyzacKinematicsInverse(), differentiated:
    // its coefficients for x, y and z, and the same expressions with the
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

    return kinsJacobianFromMappedAxes(trtfuncs_max_joints,
                                      (const double (*)[EMCMOT_MAX_AXIS])dP,
                                      jac);
} // xyzacKinematicsJacobian()

int xyzbcKinematicsForward(const double *joints,
                           EmcPose * pos,
                           const KINEMATICS_FORWARD_FLAGS * fflags,
                           KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    // Note: 'principal' joints are used
    const double x_rot_point = hal_get_real(haldata->x_rot_point);
    const double y_rot_point = hal_get_real(haldata->y_rot_point);
    const double z_rot_point = hal_get_real(haldata->z_rot_point);
    const double          dx = hal_get_real(haldata->x_offset);
    const double          dt = hal_get_real(haldata->tool_offset);
    const double          dz = hal_get_real(haldata->z_offset) + dt;
    const double       b_rad = joints[JB]*TO_RAD;
    const double       c_rad = joints[JC]*TO_RAD;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

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
} // xyzbcKinematicsForward()

int xyzbcKinematicsInverse(const EmcPose * pos,
                           double *joints,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    const double x_rot_point = hal_get_real(haldata->x_rot_point);
    const double y_rot_point = hal_get_real(haldata->y_rot_point);
    const double z_rot_point = hal_get_real(haldata->z_rot_point);
    const double          dx = hal_get_real(haldata->x_offset);
    const double          dt = hal_get_real(haldata->tool_offset);
    const double          dz = hal_get_real(haldata->z_offset) + dt;
    const double       b_rad = pos->b*TO_RAD;
    const double       c_rad = pos->c*TO_RAD;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

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
    position_to_mapped_joints(trtfuncs_max_joints,
                              &P,
                              joints);

    return 0;
} // xyzbcKinematicsInverse()

int xyzbcKinematicsWorkFrame(const double *joints,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)fflags;
    // see the comment in xyzacKinematicsWorkFrame()
    const double b_rad = joints[JB]*TO_RAD;
    const double c_rad = joints[JC]*TO_RAD;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

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
} // xyzbcKinematicsWorkFrame()

int xyzbcKinematicsToolFrame(const double *joints,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)joints;
    (void)fflags;
    // both rotaries carry the work, so the tool never turns in the machine
    *rot = TOOL_FRAME_SPINDLE;
    return 0;
} // xyzbcKinematicsToolFrame()

int xyzbcKinematicsJacobian(const double *joints,
                            const EmcPose *pos,
                            double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                            const KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)joints;
    (void)iflags;
    const double x_rot_point = hal_get_real(haldata->x_rot_point);
    const double y_rot_point = hal_get_real(haldata->y_rot_point);
    const double z_rot_point = hal_get_real(haldata->z_rot_point);
    const double          dx = hal_get_real(haldata->x_offset);
    const double          dt = hal_get_real(haldata->tool_offset);
    const double          dz = hal_get_real(haldata->z_offset) + dt;
    const double          sb = sin(pos->b*TO_RAD), cb = cos(pos->b*TO_RAD);
    const double          sc = sin(pos->c*TO_RAD), cc = cos(pos->c*TO_RAD);
    const double          X = pos->tran.x - x_rot_point;
    const double          Y = pos->tran.y - y_rot_point;
    const double          Z = pos->tran.z - z_rot_point;
    double dP[EMCMOT_MAX_AXIS][EMCMOT_MAX_AXIS];
    int a, b;

    rtapi_real con = hal_get_bool(haldata->conventional_directions) ? 1.0 : -1.0;

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        for (b = 0; b < EMCMOT_MAX_AXIS; b++) { dP[a][b] = 0; }
    }

    // see the comment in xyzacKinematicsJacobian(); dpx and dpz of the
    // inverse depend on b as well
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

    return kinsJacobianFromMappedAxes(trtfuncs_max_joints,
                                      (const double (*)[EMCMOT_MAX_AXIS])dP,
                                      jac);
} // xyzbcKinematicsJacobian()
