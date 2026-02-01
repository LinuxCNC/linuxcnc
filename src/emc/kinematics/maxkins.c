/********************************************************************
* Description: maxkins.c
*   Kinematics for Chris Radek's tabletop 5 axis mill named 'max'.
*   This mill has a tilting head (B axis) and horizontal rotary
*   mounted to the table (C axis).
*
* Author: Chris Radek
* License: GPL Version 2
*    
* Copyright (c) 2007 Chris Radek
********************************************************************/

/********************************************************************
* Note: The direction of the B axis is the opposite of the 
* conventional axis direction. See 
* https://linuxcnc.org/docs/html/gcode/machining-center.html
********************************************************************/


#include "kinematics.h"		/* these decls */
#include "posemath.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include "maxkins_math.h"

struct haldata {
    hal_float_t *pivot_length;
    hal_bit_t *conventional_directions; //default is false
} *haldata;

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    maxkins_params_t params;
    params.pivot_length = *(haldata->pivot_length);
    params.conventional_directions = *(haldata->conventional_directions);
    return maxkins_forward_math(&params, joints, pos);
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    maxkins_params_t params;
    params.pivot_length = *(haldata->pivot_length);
    params.conventional_directions = *(haldata->conventional_directions);
    return maxkins_inverse_math(&params, pos, joints);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */

const char* kinematicsGetName(void) { return "maxkins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    int result;
    comp_id = hal_init("maxkins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    result = hal_pin_float_new("maxkins.pivot-length", HAL_IO, &(haldata->pivot_length), comp_id);

    result += hal_pin_bit_new("maxkins.conventional-directions", HAL_IN, &(haldata->conventional_directions), comp_id);

    if(result < 0) goto error;

    *(haldata->pivot_length) = 0.666;
    *(haldata->conventional_directions) = 0; // default is unconventional
    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return result;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
