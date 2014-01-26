
/*----------------------------------------------------------------------
This work derived from alex joni's halui.cc
Copyright: 2013,2014
Author:    Dewey Garrett <dgarrett@panix.com>

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
----------------------------------------------------------------------*/
#include "rcs_print.hh"
#include "emc.hh"
#include <stdio.h>
#include "hal.h"
#include "rtapi.h"
#include "inihal.hh"

static int debug=1;
static int comp_id;
extern value_inihal_data old_inihal_data;

static ptr_inihal_data *the_inihal_data;

#define PREFIX "ini."
#define EPSILON .00001
#define CLOSE(a,b,eps) ((a)-(b) < +(eps) && (a)-(b) > -(eps))

#define NEW(NAME) new_inihal_data.NAME

#define CHANGED(NAME) \
    ((old_inihal_data.NAME) - (new_inihal_data.NAME) > +(EPSILON)) \
    || \
    ((old_inihal_data.NAME) - (new_inihal_data.NAME) < -(EPSILON))

#define CHANGED_IDX(NAME,IDX) \
    ((old_inihal_data.NAME[IDX]) - (new_inihal_data.NAME[IDX]) > +(EPSILON)) \
    || \
    ((old_inihal_data.NAME[IDX]) - (new_inihal_data.NAME[IDX]) < -(EPSILON))

#define UPDATE(NAME) old_inihal_data.NAME = new_inihal_data.NAME

#define UPDATE_IDX(NAME,IDX) old_inihal_data.NAME[IDX] = new_inihal_data.NAME[IDX]

#define SHOW_CHANGE(NAME) \
    fprintf(stderr,"Changed: "#NAME" %f-->%f\n",old_inihal_data.NAME, \
                                                new_inihal_data.NAME);

#define SHOW_CHANGE_IDX(NAME,IDX) \
    fprintf(stderr,"Changed: "#NAME"[%d] %f-->%f\n",IDX,old_inihal_data.NAME[IDX], \
                                                        new_inihal_data.NAME[IDX]);
#define MAKE_FLOAT_PIN(NAME,DIR) \
do { \
     retval = hal_pin_float_newf(DIR,&(the_inihal_data->NAME),comp_id,PREFIX#NAME); \
     if (retval < 0) return retval; \
   } while (0)

#define MAKE_FLOAT_PIN_IDX(NAME,DIR,IDX) \
do {                        \
     retval = hal_pin_float_newf(DIR,&(the_inihal_data->NAME[IDX]),\
                                 comp_id,PREFIX"%d."#NAME,IDX); \
     if (retval < 0) return retval; \
   } while (0)

#define INIT_PIN(NAME) *(the_inihal_data->NAME) = old_inihal_data.NAME;


int ini_hal_init(void)
{
    int retval;

    comp_id = hal_init("inihal");
    if (comp_id < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR,
            "ini_hal_init: ERROR: hal_init() failed\n");
    return -1;
    }

    the_inihal_data = (ptr_inihal_data *) hal_malloc(sizeof(ptr_inihal_data));
    if (the_inihal_data == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                       "ini_hal_init: ERROR: hal_malloc() failed\n");
        hal_exit(comp_id);
        return -1;
    }

    for (int idx = 0; idx < EMCMOT_MAX_JOINTS; idx++) {
        MAKE_FLOAT_PIN_IDX(backlash,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(min_limit,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(max_limit,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(max_velocity,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(max_acceleration,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(ferror,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(min_ferror,HAL_IN,idx);
    }

    MAKE_FLOAT_PIN(traj_default_velocity,HAL_IN);
    MAKE_FLOAT_PIN(traj_max_velocity,HAL_IN);
    MAKE_FLOAT_PIN(traj_default_acceleration,HAL_IN);
    MAKE_FLOAT_PIN(traj_max_acceleration,HAL_IN);

    hal_ready(comp_id);
    return 0;
} // ini_hal_init()

int ini_hal_init_pins()
{
    INIT_PIN(traj_default_velocity);
    INIT_PIN(traj_max_velocity);
    INIT_PIN(traj_default_acceleration);
    INIT_PIN(traj_max_acceleration);

    for (int idx = 0; idx < EMCMOT_MAX_JOINTS; idx++) {
        INIT_PIN(backlash[idx]);
        INIT_PIN(min_limit[idx]);
        INIT_PIN(max_limit[idx]);
        INIT_PIN(max_velocity[idx]);
        INIT_PIN(max_acceleration[idx]);
        INIT_PIN(ferror[idx]);
        INIT_PIN(min_ferror[idx]);
    }

    return 0;
} // ini_hal_init_pins()

static void copy_hal_data(const ptr_inihal_data &i, value_inihal_data &j)
{
    int x;
#define FIELD(t,f) j.f = (i.f)?*i.f:0;
#define ARRAY(t,f,n) do { for (x = 0; x < n; x++) j.f[x] = (i.f[x])?*i.f[x]:0; } while (0);
    HAL_FIELDS
#undef FIELD
#undef ARRAY
} // copy_hal_data()

int check_ini_hal_items()
{
    value_inihal_data new_inihal_data_mutable;
    copy_hal_data(*the_inihal_data, new_inihal_data_mutable);
    const value_inihal_data &new_inihal_data = new_inihal_data_mutable;

    if (CHANGED(traj_default_velocity)) {
        if (debug) SHOW_CHANGE(traj_default_velocity)
        UPDATE(traj_default_velocity);
        if (0 != emcTrajSetVelocity(0, NEW(traj_default_velocity))) {
            rcs_print("check_ini_hal_items:bad return value from emcTrajSetVelocity\n");
        }
    }
    if (CHANGED(traj_max_velocity)) {
        if (debug) SHOW_CHANGE(traj_max_velocity)
        UPDATE(traj_max_velocity);
        if (0 != emcTrajSetMaxVelocity(NEW(traj_max_velocity))) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("check_ini_hal_items:bad return value from emcTrajSetMaxVelocity\n");
            }
        }
    }
    if (CHANGED(traj_default_acceleration)) {
        if (debug) SHOW_CHANGE(traj_default_acceleration)
        UPDATE(traj_default_acceleration);
        if (0 != emcTrajSetAcceleration(NEW(traj_default_acceleration))) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("check_ini_hal_items:bad return value from emcTrajSetAcceleration\n");
            }
        }
    }
    if (CHANGED(traj_max_acceleration)) {
        if (debug) SHOW_CHANGE(traj_max_acceleration)
        UPDATE(traj_max_acceleration);
        if (0 != emcTrajSetMaxAcceleration(NEW(traj_max_acceleration))) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("check_ini_hal_items:bad return value from emcTrajSetMaxAcceleration\n");
            }
        }
    }

    for (int idx = 0; idx < EMCMOT_MAX_JOINTS; idx++) {
        if (CHANGED_IDX(backlash,idx) ) {
            if (debug) SHOW_CHANGE_IDX(backlash,idx);
            UPDATE_IDX(backlash,idx);
            if (0 != emcTrajSetMaxAcceleration(NEW(backlash[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print("check_ini_hal_items:bad return value from emcTrajSetMaxAcceleration\n");
                }
        }
        }
        if (CHANGED_IDX(min_limit,idx) ) {
            if (debug) SHOW_CHANGE_IDX(min_limit,idx);
            UPDATE_IDX(min_limit,idx);
            if (0 != emcAxisSetMinPositionLimit(idx,NEW(min_limit[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMinPositionLimit\n");
                }
            }
        }
        if (CHANGED_IDX(max_limit,idx) ) {
            if (debug) SHOW_CHANGE_IDX(max_limit,idx);
            UPDATE_IDX(max_limit,idx);
            if (0 != emcAxisSetMaxPositionLimit(idx,NEW(max_limit[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMaxPositionLimit\n");
                }
            }
        }
        if (CHANGED_IDX(max_velocity,idx) ) {
            if (debug) SHOW_CHANGE_IDX(max_velocity,idx);
            UPDATE_IDX(max_velocity,idx);
            if (0 != emcAxisSetMaxVelocity(idx, NEW(max_velocity[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMaxVelocity\n");
                }
            }
        }
        if (CHANGED_IDX(max_acceleration,idx) ) {
            if (debug) SHOW_CHANGE_IDX(max_acceleration,idx);
            UPDATE_IDX(max_acceleration,idx);
            if (0 != emcAxisSetMaxAcceleration(idx, NEW(max_acceleration[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMaxAcceleration\n");
                }
            }
        }
        if (CHANGED_IDX(ferror,idx) ) {
            if (debug) SHOW_CHANGE_IDX(ferror,idx);
            UPDATE_IDX(ferror,idx);
            if (0 != emcAxisSetFerror(idx,NEW(ferror[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetFerror\n");
                }
            }
        }
        if (CHANGED_IDX(min_ferror,idx) ) {
            if (debug) SHOW_CHANGE_IDX(min_ferror,idx);
            UPDATE_IDX(min_ferror,idx);
            if (0 != emcAxisSetMinFerror(idx,NEW(min_ferror[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMinFerror\n");
                }
        }
        }
    }

    return 0;
} // check_ini_hal_items

// vim: sts=4 sw=4 et
