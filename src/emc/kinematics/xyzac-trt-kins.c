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
* Kinematics for  5 axis mill named 'xyzac-trt'.
* This mill has a tilting table (A axis) and horizontal rotary
* mounted to the table (C axis).
********************************************************************/

#include "kinematics.h"
#include "posemath.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"

// sequential joint number assignments
#define JX 0
#define JY 1
#define JZ 2

#define JA 3
#define JC 4

struct haldata {
    hal_float_t *z_offset;
    hal_float_t *y_offset;
    hal_float_t *tool_offset;
} *haldata;

int kinematicsForward(const double *joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    double    dy = *(haldata->y_offset);
    double    dz = *(haldata->z_offset);
    double    dt = *(haldata->tool_offset);
    double a_rad = joints[JA]*TO_RAD;
    double c_rad = joints[JC]*TO_RAD;

    dz = dz + dt;

    pos->tran.x = + cos(c_rad)              * (joints[JX]     )
                  + sin(c_rad) * cos(a_rad) * (joints[JY] - dy)
                  + sin(c_rad) * sin(a_rad) * (joints[JZ] - dz)
                  + sin(c_rad) * dy;

    pos->tran.y = - sin(c_rad)              * (joints[JX]     )
                  + cos(c_rad) * cos(a_rad) * (joints[JY] - dy)
                  + cos(c_rad) * sin(a_rad) * (joints[JZ] - dz)
                  + cos(c_rad) * dy;

    pos->tran.z = + 0
                  - sin(a_rad) * (joints[JY] - dy)
                  + cos(a_rad) * (joints[JZ] - dz)
                  + dz;

    pos->a = joints[JA];
    pos->c = joints[JC];

    pos->b = 0;
    pos->w = 0;
    pos->u = 0;
    pos->v = 0;

    return 0;
}

int kinematicsInverse(const EmcPose * pos,
                      double *joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    double    dz = *(haldata->z_offset);
    double    dy = *(haldata->y_offset);
    double    dt = *(haldata->tool_offset);
    double c_rad = pos->c*TO_RAD;
    double a_rad = pos->a*TO_RAD;

    dz = dz + dt;

    joints[JX] = + cos(c_rad) * pos->tran.x
                 - sin(c_rad) * pos->tran.y;

    joints[JY] = + sin(c_rad) * cos(a_rad) * pos->tran.x
                 + cos(c_rad) * cos(a_rad) * pos->tran.y
                 - sin(a_rad)              * pos->tran.z
                 - cos(a_rad) * dy
                 + sin(a_rad) * dz + dy;

    joints[JZ] = + sin(c_rad) * sin(a_rad) * pos->tran.x
                 + cos(c_rad) * sin(a_rad) * pos->tran.y
                 + cos(a_rad)              * pos->tran.z
                 - sin(a_rad) * dy
                 - cos(a_rad) * dz
                 + dz;

    joints[JA] = pos->a;
    joints[JC] = pos->c;

    return 0;
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    int res = 0;
    comp_id = hal_init("xyzac-trt-kins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    if((res = hal_pin_float_new("xyzac-trt-kins.y-offset",
              HAL_IO, &(haldata->y_offset), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("xyzac-trt-kins.z-offset",
              HAL_IO, &(haldata->z_offset), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("xyzac-trt-kins.tool-offset",
              HAL_IN, &(haldata->tool_offset), comp_id)) < 0) goto error;

    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return res;

}

void rtapi_app_exit(void) { hal_exit(comp_id); }
