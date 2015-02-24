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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "hal.h"
#include "kinematics.h"
#include "rtapi_math.h"
#include "rtapi_app.h"

#include "lineardeltakins-common.h"
#define VTVERSION VTKINEMATICS_VERSION1

struct haldata
{
    hal_float_t *r, *l, *j0off, *j1off, *j2off;
} *haldata;

int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags) {
    set_geometry(*haldata->r, *haldata->l,*haldata ->j0off,*haldata ->j1off,*haldata ->j2off);
    return kinematics_forward(joints, pos);
}

int kinematicsInverse(const EmcPose *pos, double *joints,
        const KINEMATICS_INVERSE_FLAGS *iflags,
        KINEMATICS_FORWARD_FLAGS *fflags) {
    set_geometry(*haldata->r, *haldata->l,*haldata ->j0off,*haldata ->j1off,*haldata ->j2off);
    return kinematics_inverse(pos, joints);
}

KINEMATICS_TYPE kinematicsType(void)
{
    return KINEMATICS_BOTH;
}

static vtkins_t vtk = {
    .kinematicsForward = kinematicsForward,
    .kinematicsInverse  = kinematicsInverse,
    // .kinematicsHome = kinematicsHome,
    .kinematicsType = kinematicsType
};

static int comp_id, vtable_id;
static const char *name = "lineardeltakins";

int rtapi_app_main(void)
{
    int retval = 0;

    comp_id = hal_init(name);
    if(comp_id < 0) retval = comp_id;

    vtable_id = hal_export_vtable(name, VTVERSION, &vtk, comp_id);
    if (vtable_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_export_vtable(%s,%d,%p) failed: %d\n",
			name, name, VTVERSION, &vtk, vtable_id );
	return -ENOENT;
    }

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
        retval = hal_pin_float_newf(HAL_IN, &haldata->j0off, comp_id,
                "lineardeltakins.J0off");
    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->j1off, comp_id,
                "lineardeltakins.J1off");
    if(retval == 0)
        retval = hal_pin_float_newf(HAL_IN, &haldata->j2off, comp_id,
                "lineardeltakins.J2off");

    if(retval == 0)
    {
        *haldata->r = DELTA_RADIUS;
        *haldata->l = DELTA_DIAGONAL_ROD;
	*haldata->j0off = JOINT_0_OFFSET;
	*haldata->j1off = JOINT_1_OFFSET;
	*haldata->j2off = JOINT_2_OFFSET;
    }

    if(retval == 0)
    {
        hal_ready(comp_id);
    }

    return retval;
}

void rtapi_app_exit(void)
{
    hal_remove_vtable(vtable_id);
    hal_exit(comp_id);
}
