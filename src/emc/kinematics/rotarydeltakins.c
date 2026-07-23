
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

#include <rtapi_math.h>
#include <rtapi_app.h>
#include <hal.h>
#include <kinematics.h>

#include "rotarydeltakins-common.h"

struct haldata
{
    hal_real_t pfr;
    hal_real_t tl;
    hal_real_t sl;
    hal_real_t fr;
} *haldata;

int comp_id;

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    (void)fflags;
    (void)iflags;
    set_geometry(hal_get_real(haldata->pfr), hal_get_real(haldata->tl), hal_get_real(haldata->sl), hal_get_real(haldata->fr));
    return kinematics_forward(joints, pos);
}

int kinematicsInverse(const EmcPose *pos, double *joints,
        const KINEMATICS_INVERSE_FLAGS *iflags,
        KINEMATICS_FORWARD_FLAGS *fflags) {
    (void)iflags;
    (void)fflags;
    set_geometry(hal_get_real(haldata->pfr), hal_get_real(haldata->tl), hal_get_real(haldata->sl), hal_get_real(haldata->fr));
    return kinematics_inverse(pos, joints);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

int rtapi_app_main(void)
{
    int retval = 0;

    comp_id = hal_init("rotarydeltakins");
    if(comp_id < 0) retval = comp_id;

    if(retval == 0)
    {
        haldata = hal_malloc(sizeof(struct haldata));
        retval = !haldata;
    }

    if(retval == 0)
        retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->pfr,
                                  RDELTA_PFR, "rotarydeltakins.platformradius");
    if(retval == 0)
        retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->tl,
                                  RDELTA_TL, "rotarydeltakins.thighlength");
    if(retval == 0)
        retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->sl,
                                  RDELTA_SL, "rotarydeltakins.shinlength");
    if(retval == 0)
        retval = hal_pin_new_real(comp_id, HAL_IN, &haldata->fr,
                                  RDELTA_FR, "rotarydeltakins.footradius");

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

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
MODULE_LICENSE("GPL");
