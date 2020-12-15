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

********************************************************************/

#include "motion.h"
#include "hal.h"
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "rtapi_ctype.h"

// joint number assignments (-1 ==> not assigned)
static int JX = -1;
static int JY = -1;
static int JZ = -1;

static int JA = -1;
static int JB = -1;
static int JC = -1;

struct haldata {
    hal_float_t *x_rot_point;
    hal_float_t *y_rot_point;
    hal_float_t *z_rot_point;
    hal_float_t *x_offset;
    hal_float_t *y_offset;
    hal_float_t *z_offset;
    hal_float_t *tool_offset;
} *haldata;

int trtKinematicsSetup(const int   comp_id,
                       const char* coordinates,
                       kparms*     kp)
{
    int i,jno,res=0;
    int axis_idx_for_jno[EMCMOT_MAX_JOINTS];
    int njoints = strlen(kp->required_coordinates);

    if (   njoints != strlen(coordinates)
        || njoints > kp->max_joints) {
        rtapi_print_msg(RTAPI_MSG_ERR,
             "ERROR %s: requires %d letters from set: <%s> Invalid: %s\n",
             kp->kinsname,
             kp->max_joints,
             kp->required_coordinates,
             coordinates);
        goto error;
    }

    if (map_coordinates_to_jnumbers(coordinates,
                                    kp->max_joints,
                                    kp->allow_duplicates,
                                    axis_idx_for_jno)) {
       goto error;
    }
    // require all chars in reqd_coords (order doesn't matter)
    for (i=0; i < njoints; i++) {
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

    for (jno=0; jno < njoints; jno++) {
       if (axis_idx_for_jno[jno] == 0) {JX = jno;}
       if (axis_idx_for_jno[jno] == 1) {JY = jno;}
       if (axis_idx_for_jno[jno] == 2) {JZ = jno;}
       if (axis_idx_for_jno[jno] == 3) {JA = jno;}
       if (axis_idx_for_jno[jno] == 4) {JB = jno;}
       if (axis_idx_for_jno[jno] == 5) {JC = jno;}
    }

    rtapi_print("%s coordinates=%s assigns:\n", kp->kinsname,coordinates);
    for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
        if (axis_idx_for_jno[jno] == -1) break; //fini
        rtapi_print("   Joint %d ==> Axis %c\n",
                   jno,*("XYZABCUVW"+axis_idx_for_jno[jno]));
    }

    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata) {goto error;}

    res += hal_pin_float_newf(HAL_IN, &(haldata->x_rot_point), comp_id,
                 "%s.x-rot-point",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->y_rot_point), comp_id,
                 "%s.y-rot-point",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->z_rot_point), comp_id,
                 "%s.z-rot-point",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->x_offset),    comp_id,
                 "%s.x-offset",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->y_offset),    comp_id,
                 "%s.y-offset",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->z_offset),    comp_id,
                 "%s.z-offset",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->tool_offset), comp_id,
                 "%s.tool-offset",kp->halprefix);
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
    double x_rot_point = *(haldata->x_rot_point);
    double y_rot_point = *(haldata->y_rot_point);
    double z_rot_point = *(haldata->z_rot_point);
    double          dt = *(haldata->tool_offset);
    double          dy = *(haldata->y_offset);
    double          dz = *(haldata->z_offset);
    double       a_rad = joints[JA]*TO_RAD;
    double       c_rad = joints[JC]*TO_RAD;

    dz = dz + dt;

    pos->tran.x = + cos(c_rad)              * (joints[JX]      - x_rot_point)
                  + sin(c_rad) * cos(a_rad) * (joints[JY] - dy - y_rot_point)
                  + sin(c_rad) * sin(a_rad) * (joints[JZ] - dz - z_rot_point)
                  + sin(c_rad) * dy
                  + x_rot_point;

    pos->tran.y = - sin(c_rad)              * (joints[JX]      - x_rot_point)
                  + cos(c_rad) * cos(a_rad) * (joints[JY] - dy - y_rot_point)
                  + cos(c_rad) * sin(a_rad) * (joints[JZ] - dz - z_rot_point)
                  + cos(c_rad) * dy
                  + y_rot_point;

    pos->tran.z = + 0
                  - sin(a_rad) * (joints[JY] - dy - y_rot_point)
                  + cos(a_rad) * (joints[JZ] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->a = joints[JA];
    pos->c = joints[JC];

    pos->b = 0;
    pos->w = 0;
    pos->u = 0;
    pos->v = 0;

    return 0;
} // xyzacKinematicsForward()

int xyzacKinematicsInverse(const EmcPose * pos,
                           double *joints,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
    double x_rot_point = *(haldata->x_rot_point);
    double y_rot_point = *(haldata->y_rot_point);
    double z_rot_point = *(haldata->z_rot_point);
    double         dy  = *(haldata->y_offset);
    double         dz  = *(haldata->z_offset);
    double         dt  = *(haldata->tool_offset);
    double      a_rad  = pos->a*TO_RAD;
    double      c_rad  = pos->c*TO_RAD;

    dz = dz + dt;

    joints[JX] = + cos(c_rad)              * (pos->tran.x - x_rot_point)
                 - sin(c_rad)              * (pos->tran.y - y_rot_point)
                 + x_rot_point;

    joints[JY] = + sin(c_rad) * cos(a_rad) * (pos->tran.x - x_rot_point)
                 + cos(c_rad) * cos(a_rad) * (pos->tran.y - y_rot_point)
                 -              sin(a_rad) * (pos->tran.z - z_rot_point)
                 -              cos(a_rad) * dy
                 +              sin(a_rad) * dz
                 + dy
                 + y_rot_point;

    joints[JZ] = + sin(c_rad) * sin(a_rad) * (pos->tran.x - x_rot_point)
                 + cos(c_rad) * sin(a_rad) * (pos->tran.y - y_rot_point)
                 +              cos(a_rad) * (pos->tran.z - z_rot_point)
                 -              sin(a_rad) * dy
                 -              cos(a_rad) * dz
                 + dz
                 + z_rot_point;


    joints[JA] = pos->a;
    joints[JC] = pos->c;

    return 0;
} // xyzacKinematicsInverse()

int xyzbcKinematicsForward(const double *joints,
                           EmcPose * pos,
                           const KINEMATICS_FORWARD_FLAGS * fflags,
                           KINEMATICS_INVERSE_FLAGS * iflags)
{
    double x_rot_point = *(haldata->x_rot_point);
    double y_rot_point = *(haldata->y_rot_point);
    double z_rot_point = *(haldata->z_rot_point);
    double          dx = *(haldata->x_offset);
    double          dz = *(haldata->z_offset);
    double          dt = *(haldata->tool_offset);
                    dz = dz + dt;
    double       b_rad = joints[JB]*TO_RAD;
    double       c_rad = joints[JC]*TO_RAD;


    pos->tran.x =   cos(c_rad) * cos(b_rad) * (joints[JX] - dx - x_rot_point)
                  + sin(c_rad) *              (joints[JY]      - y_rot_point)
                  - cos(c_rad) * sin(b_rad) * (joints[JZ] - dz - z_rot_point)
                  + cos(c_rad) * dx
                  + x_rot_point;

    pos->tran.y = - sin(c_rad) * cos(b_rad) * (joints[JX] - dx - x_rot_point)
                  + cos(c_rad) *              (joints[JY]      - y_rot_point)
                  + sin(c_rad) * sin(b_rad) * (joints[JZ] - dz - z_rot_point)
                  - sin(c_rad) * dx
                  + y_rot_point;

    pos->tran.z =   sin(b_rad) * (joints[JX] - dx - x_rot_point)
                  + cos(b_rad) * (joints[JZ] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->b = joints[JB];
    pos->c = joints[JC];

    pos->a = 0;
    pos->u = 0;
    pos->v = 0;
    pos->w = 0;

    return 0;
} // xyzbcKinematicsForward()

int xyzbcKinematicsInverse(const EmcPose * pos,
                           double *joints,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
    double x_rot_point = *(haldata->x_rot_point);
    double y_rot_point = *(haldata->y_rot_point);
    double z_rot_point = *(haldata->z_rot_point);
    double          dx = *(haldata->x_offset);
    double          dz = *(haldata->z_offset);
    double          dt = *(haldata->tool_offset);
                    dz = dz + dt;
    double       b_rad = pos->b*TO_RAD;
    double       c_rad = pos->c*TO_RAD;
    double         dpx = -cos(b_rad)*dx - sin(b_rad)*dz + dx;
    double         dpz =  sin(b_rad)*dx - cos(b_rad)*dz + dz;

    joints[JX] =   cos(c_rad) * cos(b_rad) * (pos->tran.x - x_rot_point)
                 - sin(c_rad) * cos(b_rad) * (pos->tran.y - y_rot_point)
                 + sin(b_rad) *              (pos->tran.z - z_rot_point)
                 + dpx
                 + x_rot_point;

    joints[JY] =   sin(c_rad) * (pos->tran.x - x_rot_point)
                 + cos(c_rad) * (pos->tran.y - y_rot_point)
                 + y_rot_point;

    joints[JZ] = - cos(c_rad) * sin(b_rad) * (pos->tran.x - x_rot_point)
                 + sin(c_rad) * sin(b_rad) * (pos->tran.y - y_rot_point)
                 + cos(b_rad) *              (pos->tran.z - z_rot_point)
                 + dpz
                 + z_rot_point;

    joints[JB] = pos->b;
    joints[JC] = pos->c;

    return 0;
} // xyzbcKinematicsInverse()
