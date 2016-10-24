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

#define VTVERSION VTKINEMATICS_VERSION1

struct haldata {
    hal_float_t *Y_offset;
    hal_float_t *Z_offset;
    hal_float_t *Tool_offset;
} *haldata;

static int kinematicsForward(const double *joints,
			     EmcPose * pos,
			     const KINEMATICS_FORWARD_FLAGS * fflags,
			     KINEMATICS_INVERSE_FLAGS * iflags)
{
    double dy = *(haldata->Y_offset);
    double dz = *(haldata->Z_offset);
    double dT = *(haldata->Tool_offset);
    double a_rad = joints[3] * M_PI/180;
    double c_rad = joints[5] * M_PI/180;
    dz = dz + dT;
    pos->tran.x = rtapi_cos(c_rad) * (joints[0]) +
	rtapi_sin(c_rad) * rtapi_cos(a_rad) * (joints[1] - dy) +
	rtapi_sin(c_rad) * rtapi_sin(a_rad) * (joints[2] - dz) +
	rtapi_sin(c_rad) * dy;
    pos->tran.y = -rtapi_sin(c_rad) * (joints[0]) +
	rtapi_cos(c_rad) * rtapi_cos(a_rad) * (joints[1] - dy) +
	rtapi_cos(c_rad) * rtapi_sin(a_rad) * (joints[2] - dz) +
	rtapi_cos(c_rad) * dy;
    pos->tran.z = -rtapi_sin(a_rad) * (joints[1] - dy) +
	rtapi_cos(a_rad) * (joints[2] - dz) + dz;
    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    return 0;
}

static int kinematicsInverse(const EmcPose * pos,
			     double *joints,
			     const KINEMATICS_INVERSE_FLAGS * iflags,
			     KINEMATICS_FORWARD_FLAGS * fflags)
{
    double dy = *(haldata->Y_offset);
    double dz = *(haldata->Z_offset);
    double dT = *(haldata->Tool_offset);
    double c_rad = pos->c * M_PI/180;
    double a_rad = pos->a * M_PI/180;
    dz = dz + dT;
    joints[0] = rtapi_cos(c_rad) * pos->tran.x -
	rtapi_sin(c_rad) * pos->tran.y;
    joints[1] = rtapi_sin(c_rad) * rtapi_cos(a_rad) * pos->tran.x +
	rtapi_cos(c_rad) * rtapi_cos(a_rad) * pos->tran.y -
	rtapi_sin(a_rad) * pos->tran.z -
	rtapi_cos(a_rad) * dy + rtapi_sin(a_rad) * dz + dy;
    joints[2] = rtapi_sin(c_rad) * rtapi_sin(a_rad) * pos->tran.x +
	rtapi_cos(c_rad) * rtapi_sin(a_rad) * pos->tran.y +
	rtapi_cos(a_rad) * pos->tran.z -
	rtapi_sin(a_rad) * dy - rtapi_cos(a_rad) * dz + dz;
    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    return 0;
}

static KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi.h" /* RTAPI realtime OS API */
#include "rtapi_app.h" /* RTAPI realtime module decls */
#include "hal.h"

MODULE_LICENSE("GPL");

static vtkins_t vtk = {
    .kinematicsForward = kinematicsForward,
    .kinematicsInverse  = kinematicsInverse,
    // .kinematicsHome = kinematicsHome,
    .kinematicsType = kinematicsType
};

static int comp_id, vtable_id;
static const char *name = "XYZACkins";

int rtapi_app_main(void)
{
    int res = 0;
    comp_id = hal_init(name);
    if (comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    if (((res = hal_pin_newf(HAL_FLOAT, HAL_IN, (void **) &(haldata->Tool_offset),
			     comp_id, "%s.Tool-offset", name)) < 0) ||
	((res = hal_pin_newf(HAL_FLOAT, HAL_IN, (void **)  &(haldata->Y_offset),
			     comp_id, "%s.Y-offset", name)) < 0) ||
	((res = hal_pin_newf(HAL_FLOAT, HAL_IN, (void **)  &(haldata->Z_offset),
			     comp_id, "%s.Z-offset", name)) < 0))
	goto error;

    vtable_id = hal_export_vtable(name, VTVERSION, &vtk, comp_id);

    if (vtable_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_export_vtable(%s,%d,%p) failed: %d\n",
			name, name, VTVERSION, &vtk, vtable_id );
	return -ENOENT;
    }

    hal_ready(comp_id);
    return 0;
 error:
    hal_exit(comp_id);
    return res;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}
