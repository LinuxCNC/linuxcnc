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
* Kinematics for 5 axis mill named ’xyzbc’.
* This mill has a tilting table (B axis) and horizontal rotary
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

#define JB 3
#define JC 4

struct haldata {
    hal_float_t *x_offset;
    hal_float_t *z_offset;
    hal_float_t *tool_offset;
} *haldata;

int kinematicsForward(const double *joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    double    dx = *(haldata->x_offset);
    double    dz = *(haldata->z_offset);
    double    dt = *(haldata->tool_offset);
              dz = dz + dt;
    double b_rad = joints[JB]*TO_RAD;
    double c_rad = joints[JC]*TO_RAD;


    pos->tran.x =   cos(c_rad) * cos(b_rad) * (joints[JX] - dx)
                  + sin(c_rad) *              (joints[JY])
                  - cos(c_rad) * sin(b_rad) * (joints[JZ] - dz)
                  + cos(c_rad) * dx;

    pos->tran.y = - sin(c_rad) * cos(b_rad) * (joints[JX] - dx)
                  + cos(c_rad) *              (joints[JY])
                  + sin(c_rad) * sin(b_rad) * (joints[JZ] - dz)
                  - sin(c_rad) * dx;

    pos->tran.z =   sin(b_rad) * (joints[JX] - dx)
                  + cos(b_rad) * (joints[JZ] - dz)
                  + dz;

    pos->b = joints[JB];
    pos->c = joints[JC];

    pos->a = 0;
    pos->u = 0;
    pos->v = 0;
    pos->w = 0;

    return 0;
}

int kinematicsInverse(const EmcPose * pos,
                      double *joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    double    dx = *(haldata->x_offset);
    double    dz = *(haldata->z_offset);
    double    dt = *(haldata->tool_offset);
              dz = dz + dt;
    double b_rad = pos->b*TO_RAD;
    double c_rad = pos->c*TO_RAD;
    double   dpx = -cos(b_rad)*dx - sin(b_rad)*dz + dx;
    double   dpz = sin(b_rad)*dx - cos(b_rad)*dz + dz;


    joints[JX] =   cos(c_rad) * cos(b_rad) * (pos->tran.x)
                 - sin(c_rad) * cos(b_rad) * (pos->tran.y)
                 + sin(b_rad) * (pos->tran.z)
                 + dpx;

    joints[JY] =   sin(c_rad) * (pos->tran.x)
                 + cos(c_rad) * (pos->tran.y);

    joints[JZ] = - cos(c_rad) * sin(b_rad) * (pos->tran.x)
                 + sin(c_rad) * sin(b_rad) * (pos->tran.y)
                 + cos(b_rad) * (pos->tran.z)
                 + dpz;

    joints[JB] = pos->b;
    joints[JC] = pos->c ;

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
    comp_id = hal_init("xyzbc-trt-kins");

    if(comp_id < 0) return comp_id;
        haldata = hal_malloc(sizeof(struct haldata));
        if((res = hal_pin_float_new("xyzbc-trt-kins.x-offset",
                  HAL_IO, &(haldata->x_offset),comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("xyzbc-trt-kins.z-offset",
                  HAL_IO, &(haldata->z_offset), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("xyzbc-trt-kins.tool-offset",
                  HAL_IN, &(haldata->tool_offset), comp_id)) < 0) goto error;
    hal_ready(comp_id);
    return 0;
error:
    hal_exit(comp_id);
    return res;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
