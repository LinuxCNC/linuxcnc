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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "hal.h"
#include "kinematics.h"
#include "rtapi_math.h"
#include "rtapi_app.h"

#include "lineardeltakins_math.h"

struct haldata
{
    hal_float_t *r, *l;
} *haldata;

int comp_id;

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    (void)fflags;
    (void)iflags;
    lineardelta_params_t params;
    params.radius = *haldata->r;
    params.rod_length = *haldata->l;
    return lineardelta_forward_math(&params, joints, pos);
}

int kinematicsInverse(const EmcPose *pos, double *joints,
        const KINEMATICS_INVERSE_FLAGS *iflags,
        KINEMATICS_FORWARD_FLAGS *fflags) {
    (void)iflags;
    (void)fflags;
    lineardelta_params_t params;
    params.radius = *haldata->r;
    params.rod_length = *haldata->l;
    return lineardelta_inverse_math(&params, pos, joints);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

int rtapi_app_main(void)
{
    int retval = 0;

    comp_id = hal_init("lineardeltakins");
    if(comp_id < 0) retval = comp_id;

    if(retval == 0)
    {
        haldata = hal_malloc(sizeof(struct haldata));
        retval = !haldata;
    }

    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->r, comp_id,
                "lineardeltakins.R");
    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->l, comp_id,
                "lineardeltakins.L");

    if(retval == 0)
    {
        *haldata->r = LINEARDELTA_DEFAULT_RADIUS;
        *haldata->l = LINEARDELTA_DEFAULT_ROD_LENGTH;
    }

    if(retval == 0)
    {
        hal_ready(comp_id);
    }

    return retval;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

const char* kinematicsGetName(void) { return "lineardeltakins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");
