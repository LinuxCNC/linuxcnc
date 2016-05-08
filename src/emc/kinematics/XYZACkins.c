/********************************************************************
 * Description: XYZACkins.c
 * Kinematics for 5 axis mill named XYZAC.
 * This mill has a tilting table (A axis) and horizontal rotary
 * mounted to the table (C axis).
 * with rotary axis offsets
 *
 * Author: Rudy du Preez
 * License: GPL Version 2
 *
 ********************************************************************/
#include "kinematics.h" /* these decls */
#include "posemath.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"

struct haldata {
    hal_float_t *Y_offset;
    hal_float_t *Z_offset;
    hal_float_t *Tool_offset;
} *haldata;

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    double dy = *(haldata->Y_offset);
    double dz = *(haldata->Z_offset);
    double dT = *(haldata->Tool_offset);
    double a_rad = joints[3]*M_PI/180;
    double c_rad = joints[5]*M_PI/180;
    dz = dz + dT;
    pos->tran.x = cos(c_rad)*(joints[0]) +
	sin(c_rad)*cos(a_rad)*(joints[1] - dy) +
	sin(c_rad)*sin(a_rad)*(joints[2] - dz) +
	sin(c_rad)*dy;
    pos->tran.y = -sin(c_rad)*(joints[0]) +
	cos(c_rad)*cos(a_rad)*(joints[1] - dy) +
	cos(c_rad)*sin(a_rad)*(joints[2] - dz) +
	cos(c_rad)*dy;
    pos->tran.z = -sin(a_rad)*(joints[1] - dy) +
	cos(a_rad)*(joints[2] - dz) + dz;
    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    return 0;
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
    double dy = *(haldata->Y_offset);
    double dz = *(haldata->Z_offset);
    double dT = *(haldata->Tool_offset);
    double c_rad = pos->c*M_PI/180;
    double a_rad = pos->a*M_PI/180;
    dz = dz + dT;
    joints[0] = cos(c_rad)*pos->tran.x -
	sin(c_rad)*pos->tran.y;
    joints[1] = sin(c_rad)*cos(a_rad)*pos->tran.x +
	cos(c_rad)*cos(a_rad)*pos->tran.y -
	sin(a_rad)*pos->tran.z -
	cos(a_rad)*dy + sin(a_rad)*dz + dy;
    joints[2] = sin(c_rad)*sin(a_rad)*pos->tran.x +
	cos(c_rad)*sin(a_rad)*pos->tran.y +
	cos(a_rad)*pos->tran.z -
	sin(a_rad)*dy - cos(a_rad)*dz + dz;
    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    return 0;
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi.h" /* RTAPI realtime OS API */
#include "rtapi_app.h" /* RTAPI realtime module decls */
#include "hal.h"
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
MODULE_LICENSE("GPL");

int comp_id;

int rtapi_app_main(void) {
    int res = 0;
    comp_id = hal_init("XYZACkins");
    if(comp_id < 0) return comp_id;
    haldata = hal_malloc(sizeof(struct haldata));
    if((res = hal_pin_float_new("XYZACkins.Tool-offset",
				HAL_IN, &(haldata->Tool_offset),
				comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("XYZACkins.Y-offset",
				HAL_IO, &(haldata->Y_offset),
				comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("XYZACkins.Z-offset",
				HAL_IO, &(haldata->Z_offset),
				comp_id)) < 0) goto error;
    hal_ready(comp_id);
    return 0;
 error:
    hal_exit(comp_id);
    return res;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}
