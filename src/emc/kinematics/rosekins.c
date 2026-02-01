/*
  Copyright 2016 Dewey Garrett <dgarrett@panix.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "kinematics.h"
#include "posemath.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_app.h"
#include "rosekins_math.h"

const char* kinematicsGetName(void) { return "rosekins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

struct haldata {
    hal_float_t *revolutions;
    hal_float_t *theta_degrees;
    hal_float_t *bigtheta_degrees;
} *haldata;

/* State for revolution tracking */
static rosekins_state_t kins_state;

int kinematicsForward(const double *joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    return rosekins_forward_math(joints, pos);
}

int kinematicsInverse(const EmcPose * pos,
                      double *joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    rosekins_output_t output;
    int result = rosekins_inverse_math(pos, joints, &kins_state, &output);

    /* Write diagnostic values to HAL pins */
    *(haldata->revolutions) = output.revolutions;
    *(haldata->theta_degrees) = output.theta_degrees;
    *(haldata->bigtheta_degrees) = output.bigtheta_degrees;

    return result;
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

static int comp_id;

void rtapi_app_exit(void) { hal_exit(comp_id); }

int rtapi_app_main(void) {
    int ans;
    comp_id = hal_init("rosekins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    if((ans = hal_pin_float_new("rosekins.revolutions",
              HAL_OUT, &(haldata->revolutions), comp_id)) < 0) goto error;
    if((ans = hal_pin_float_new("rosekins.theta_degrees",
              HAL_OUT, &(haldata->theta_degrees), comp_id)) < 0) goto error;
    if((ans = hal_pin_float_new("rosekins.bigtheta_degrees",
              HAL_OUT, &(haldata->bigtheta_degrees), comp_id)) < 0) goto error;

    hal_ready(comp_id);
    return 0;

error:
    return ans;
}
