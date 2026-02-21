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

#include "motion.h"
#include "hal.h"
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "rtapi_ctype.h"

/* ========================================================================
 * TRT math types and functions (was in trtfuncs_math.h)
 * ======================================================================== */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef TO_RAD
#define TO_RAD (M_PI / 180.0)
#endif

typedef struct {
    double x_rot_point;
    double y_rot_point;
    double z_rot_point;
    double x_offset;
    double y_offset;
    double z_offset;
    double tool_offset;
    int conventional_directions;
} trt_params_t;

typedef struct {
    int jx, jy, jz;
    int ja, jb, jc;
    int ju, jv, jw;
} trt_joints_t;

int xyzac_forward_math(const trt_params_t *params,
                       const trt_joints_t *jmap,
                       const double *joints,
                       EmcPose *pos)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dt = params->tool_offset;
    const double dy = params->y_offset;
    const double dz = params->z_offset + dt;
    const double a_rad = joints[jmap->ja] * TO_RAD;
    const double c_rad = joints[jmap->jc] * TO_RAD;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    pos->tran.x = +       cos(c_rad)              * (joints[jmap->jx]      - x_rot_point)
                  - con * sin(c_rad) * cos(a_rad) * (joints[jmap->jy] - dy - y_rot_point)
                  +       sin(c_rad) * sin(a_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  - con * sin(c_rad) * dy
                  + x_rot_point;

    pos->tran.y = + con * sin(c_rad)              * (joints[jmap->jx]      - x_rot_point)
                  +       cos(c_rad) * cos(a_rad) * (joints[jmap->jy] - dy - y_rot_point)
                  - con * cos(c_rad) * sin(a_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  +       cos(c_rad) * dy
                  + y_rot_point;

    pos->tran.z = + 0
                  + con * sin(a_rad) * (joints[jmap->jy] - dy - y_rot_point)
                  + cos(a_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->a = joints[jmap->ja];
    pos->c = joints[jmap->jc];

    pos->b = (jmap->jb >= 0) ? joints[jmap->jb] : 0;
    pos->u = (jmap->ju >= 0) ? joints[jmap->ju] : 0;
    pos->v = (jmap->jv >= 0) ? joints[jmap->jv] : 0;
    pos->w = (jmap->jw >= 0) ? joints[jmap->jw] : 0;

    return 0;
}

int xyzac_inverse_math(const trt_params_t *params,
                       const trt_joints_t *jmap,
                       const EmcPose *pos,
                       EmcPose *axis_values)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dy = params->y_offset;
    const double dt = params->tool_offset;
    const double dz = params->z_offset + dt;
    const double a_rad = pos->a * TO_RAD;
    const double c_rad = pos->c * TO_RAD;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    axis_values->tran.x = +       cos(c_rad)              * (pos->tran.x - x_rot_point)
                          + con * sin(c_rad)              * (pos->tran.y - y_rot_point)
                          + x_rot_point;

    axis_values->tran.y = - con * sin(c_rad) * cos(a_rad) * (pos->tran.x - x_rot_point)
                          +       cos(c_rad) * cos(a_rad) * (pos->tran.y - y_rot_point)
                          + con *              sin(a_rad) * (pos->tran.z - z_rot_point)
                          -                    cos(a_rad) * dy
                          - con *              sin(a_rad) * dz
                          + dy
                          + y_rot_point;

    axis_values->tran.z = +       sin(c_rad) * sin(a_rad) * (pos->tran.x - x_rot_point)
                          - con * cos(c_rad) * sin(a_rad) * (pos->tran.y - y_rot_point)
                          +                    cos(a_rad) * (pos->tran.z - z_rot_point)
                          + con *              sin(a_rad) * dy
                          -                    cos(a_rad) * dz
                          + dz
                          + z_rot_point;

    axis_values->a = pos->a;
    axis_values->c = pos->c;

    axis_values->b = (jmap->jb >= 0) ? pos->b : 0;
    axis_values->u = (jmap->ju >= 0) ? pos->u : 0;
    axis_values->v = (jmap->jv >= 0) ? pos->v : 0;
    axis_values->w = (jmap->jw >= 0) ? pos->w : 0;

    return 0;
}

int xyzbc_forward_math(const trt_params_t *params,
                       const trt_joints_t *jmap,
                       const double *joints,
                       EmcPose *pos)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dx = params->x_offset;
    const double dt = params->tool_offset;
    const double dz = params->z_offset + dt;
    const double b_rad = joints[jmap->jb] * TO_RAD;
    const double c_rad = joints[jmap->jc] * TO_RAD;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    pos->tran.x =         cos(c_rad) * cos(b_rad) * (joints[jmap->jx] - dx - x_rot_point)
                  - con * sin(c_rad) *              (joints[jmap->jy]      - y_rot_point)
                  + con * cos(c_rad) * sin(b_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  +       cos(c_rad) * dx
                  + x_rot_point;

    pos->tran.y = + con * sin(c_rad) * cos(b_rad) * (joints[jmap->jx] - dx - x_rot_point)
                  +       cos(c_rad) *              (joints[jmap->jy]      - y_rot_point)
                  +       sin(c_rad) * sin(b_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  + con * sin(c_rad) * dx
                  + y_rot_point;

    pos->tran.z = - con * sin(b_rad) * (joints[jmap->jx] - dx - x_rot_point)
                  +       cos(b_rad) * (joints[jmap->jz] - dz - z_rot_point)
                  + dz
                  + z_rot_point;

    pos->b = joints[jmap->jb];
    pos->c = joints[jmap->jc];

    pos->a = (jmap->ja >= 0) ? joints[jmap->ja] : 0;
    pos->u = (jmap->ju >= 0) ? joints[jmap->ju] : 0;
    pos->v = (jmap->jv >= 0) ? joints[jmap->jv] : 0;
    pos->w = (jmap->jw >= 0) ? joints[jmap->jw] : 0;

    return 0;
}

int xyzbc_inverse_math(const trt_params_t *params,
                       const trt_joints_t *jmap,
                       const EmcPose *pos,
                       EmcPose *axis_values)
{
    const double x_rot_point = params->x_rot_point;
    const double y_rot_point = params->y_rot_point;
    const double z_rot_point = params->z_rot_point;
    const double dx = params->x_offset;
    const double dt = params->tool_offset;
    const double dz = params->z_offset + dt;
    const double b_rad = pos->b * TO_RAD;
    const double c_rad = pos->c * TO_RAD;
    const double dpx = -cos(b_rad)*dx + sin(b_rad)*dz + dx;
    const double dpz = -sin(b_rad)*dx - cos(b_rad)*dz + dz;

    const double con = params->conventional_directions ? 1.0 : -1.0;

    axis_values->tran.x = +       cos(c_rad) * cos(b_rad) * (pos->tran.x - x_rot_point)
                          + con * sin(c_rad) * cos(b_rad) * (pos->tran.y - y_rot_point)
                          - con *              sin(b_rad) * (pos->tran.z - z_rot_point)
                          + dpx
                          + x_rot_point;

    axis_values->tran.y = - con * sin(c_rad) * (pos->tran.x - x_rot_point)
                          +       cos(c_rad) * (pos->tran.y - y_rot_point)
                          + y_rot_point;

    axis_values->tran.z = + con * cos(c_rad) * sin(b_rad) * (pos->tran.x - x_rot_point)
                          +       sin(c_rad) * sin(b_rad) * (pos->tran.y - y_rot_point)
                          +                    cos(b_rad) * (pos->tran.z - z_rot_point)
                          + dpz
                          + z_rot_point;

    axis_values->b = pos->b;
    axis_values->c = pos->c;

    axis_values->a = (jmap->ja >= 0) ? pos->a : 0;
    axis_values->u = (jmap->ju >= 0) ? pos->u : 0;
    axis_values->v = (jmap->jv >= 0) ? pos->v : 0;
    axis_values->w = (jmap->jw >= 0) ? pos->w : 0;

    return 0;
}

void trt_axis_to_joints(const trt_joints_t *jmap,
                        const EmcPose *axis_values,
                        double *joints)
{
    if (jmap->jx >= 0) joints[jmap->jx] = axis_values->tran.x;
    if (jmap->jy >= 0) joints[jmap->jy] = axis_values->tran.y;
    if (jmap->jz >= 0) joints[jmap->jz] = axis_values->tran.z;
    if (jmap->ja >= 0) joints[jmap->ja] = axis_values->a;
    if (jmap->jb >= 0) joints[jmap->jb] = axis_values->b;
    if (jmap->jc >= 0) joints[jmap->jc] = axis_values->c;
    if (jmap->ju >= 0) joints[jmap->ju] = axis_values->u;
    if (jmap->jv >= 0) joints[jmap->jv] = axis_values->v;
    if (jmap->jw >= 0) joints[jmap->jw] = axis_values->w;
}

/* ========================================================================
 * RT interface (reads HAL pins)
 * ======================================================================== */

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

// Joint mapping struct for math functions
static trt_joints_t jmap;

struct haldata {
    hal_float_t *x_rot_point;
    hal_float_t *y_rot_point;
    hal_float_t *z_rot_point;
    hal_float_t *x_offset;
    hal_float_t *y_offset;
    hal_float_t *z_offset;
    hal_float_t *tool_offset;
    hal_bit_t *conventional_directions; // default: false
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

    // Populate joint map struct for math functions
    jmap.jx = JX; jmap.jy = JY; jmap.jz = JZ;
    jmap.ja = JA; jmap.jb = JB; jmap.jc = JC;
    jmap.ju = JU; jmap.jv = JV; jmap.jw = JW;

    rtapi_print("%s coordinates=%s assigns:\n", kp->kinsname,coordinates);
    for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
        if (axis_idx_for_jno[jno] == -1) break; //fini
        rtapi_print("   Joint %d ==> Axis %c\n",
                   jno,"XYZABCUVW"[axis_idx_for_jno[jno]]);
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
    res += hal_pin_bit_newf(HAL_IN, &(haldata->conventional_directions), comp_id,
                 "%s.conventional-directions", kp->halprefix);
    if (res) {goto error;}
    return 0;

error:
    rtapi_print_msg(RTAPI_MSG_ERR,"trtKinematicsSetup() FAIL\n");
    return -1;
} // trtKinematicsSetup()

/* Helper to populate trt_params_t from HAL pins */
static inline void trt_get_params(trt_params_t *params)
{
    params->x_rot_point = *(haldata->x_rot_point);
    params->y_rot_point = *(haldata->y_rot_point);
    params->z_rot_point = *(haldata->z_rot_point);
    params->x_offset = *(haldata->x_offset);
    params->y_offset = *(haldata->y_offset);
    params->z_offset = *(haldata->z_offset);
    params->tool_offset = *(haldata->tool_offset);
    params->conventional_directions = *(haldata->conventional_directions);
}

int xyzacKinematicsForward(const double *joints,
                           EmcPose * pos,
                           const KINEMATICS_FORWARD_FLAGS * fflags,
                           KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    trt_params_t params;
    trt_get_params(&params);
    return xyzac_forward_math(&params, &jmap, joints, pos);
} // xyzacKinematicsForward()

int xyzacKinematicsInverse(const EmcPose * pos,
                           double *joints,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    trt_params_t params;
    trt_get_params(&params);

    // Compute axis values using pure math function
    EmcPose P;
    xyzac_inverse_math(&params, &jmap, pos, &P);

    // update joints with support for
    // multiple-joints per-coordinate letter:
    // based on computed position
    position_to_mapped_joints(trtfuncs_max_joints,
                              &P,
                              joints);

    return 0;
} // xyzacKinematicsInverse()

int xyzbcKinematicsForward(const double *joints,
                           EmcPose * pos,
                           const KINEMATICS_FORWARD_FLAGS * fflags,
                           KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    trt_params_t params;
    trt_get_params(&params);
    return xyzbc_forward_math(&params, &jmap, joints, pos);
} // xyzbcKinematicsForward()

int xyzbcKinematicsInverse(const EmcPose * pos,
                           double *joints,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    trt_params_t params;
    trt_get_params(&params);

    // Compute axis values using pure math function
    EmcPose P;
    xyzbc_inverse_math(&params, &jmap, pos, &P);

    // update joints with support for
    // multiple-joints per-coordinate letter:
    // based on computed position
    position_to_mapped_joints(trtfuncs_max_joints,
                              &P,
                              joints);

    return 0;
} // xyzbcKinematicsInverse()
