
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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
----------------------------------------------------------------------*/
#include "rcs_print.hh"
#include "emc.hh"
#include "emcglb.h"
#include <stdio.h>
#include "hal.h"
#include "rtapi.h"
#include "inihal.hh"
#include "iniaxis.hh"

static int debug=0;
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
    fprintf(stderr,"Changed: "#NAME" %g-->%g\n",old_inihal_data.NAME, \
                                                new_inihal_data.NAME);
#define SHOW_CHANGE_ARC_BLEND() \
    fprintf(stderr,"Changed: blend_enable:          %d-->%d\n"\
                   "         blend_fallback_enable: %d-->%d\n"\
                   "         optimization_depth:    %d-->%d\n"\
                   "         gap_cycles:            %f-->%f\n"\
                   "         ramp_freq:             %f-->%f\n"\
           ,old_inihal_data.traj_arc_blend_enable \
           ,new_inihal_data.traj_arc_blend_enable \
           ,old_inihal_data.traj_arc_blend_fallback_enable \
           ,new_inihal_data.traj_arc_blend_fallback_enable \
           ,old_inihal_data.traj_arc_blend_optimization_depth \
           ,new_inihal_data.traj_arc_blend_optimization_depth \
           ,old_inihal_data.traj_arc_blend_gap_cycles \
           ,new_inihal_data.traj_arc_blend_gap_cycles \
           ,old_inihal_data.traj_arc_blend_ramp_freq \
           ,new_inihal_data.traj_arc_blend_ramp_freq \
          );

#define SHOW_CHANGE_IDX(NAME,IDX) \
    fprintf(stderr,"Changed: "#NAME"[%d] %g-->%g\n",IDX,old_inihal_data.NAME[IDX], \
                                                        new_inihal_data.NAME[IDX]);
#define SHOW_CHANGE_IDX_INT(NAME,IDX) \
    fprintf(stderr,"Changed: "#NAME"[%d] %d-->%d\n",IDX,old_inihal_data.NAME[IDX], \
                                                        new_inihal_data.NAME[IDX]);
#define MAKE_BIT_PIN(NAME,DIR) \
do { \
     retval = hal_pin_bit_newf(DIR,&(the_inihal_data->NAME),comp_id,PREFIX#NAME); \
     if (retval < 0) return retval; \
   } while (0)

#define MAKE_S32_PIN(NAME,DIR) \
do { \
     retval = hal_pin_s32_newf(DIR,&(the_inihal_data->NAME),comp_id,PREFIX#NAME); \
     if (retval < 0) return retval; \
   } while (0)

#define MAKE_S32_PIN_IDX(NAME,HALPIN_NAME,DIR,IDX) \
do { \
     retval = hal_pin_s32_newf(DIR,&(the_inihal_data->NAME[IDX]),\
                               comp_id,PREFIX"%d."#HALPIN_NAME,IDX); \
     if (retval < 0) return retval; \
   } while (0)

#define MAKE_FLOAT_PIN(NAME,DIR) \
do { \
     retval = hal_pin_float_newf(DIR,&(the_inihal_data->NAME),comp_id,PREFIX#NAME); \
     if (retval < 0) return retval; \
   } while (0)

#define MAKE_FLOAT_PIN_IDX(NAME,HALPIN_NAME,DIR,IDX) \
do {                        \
     retval = hal_pin_float_newf(DIR,&(the_inihal_data->NAME[IDX]),\
                                 comp_id,PREFIX"%d."#HALPIN_NAME,IDX); \
     if (retval < 0) return retval; \
   } while (0)

#define MAKE_FLOAT_PIN_LETTER(NAME,HALPIN_NAME,DIR,IDX,LETTER) \
do {                        \
     retval = hal_pin_float_newf(DIR,&(the_inihal_data->NAME[IDX]),\
                                 comp_id,PREFIX"%c."#HALPIN_NAME,LETTER); \
     if (retval < 0) return retval; \
   } while (0)

#define INIT_PIN(NAME) *(the_inihal_data->NAME) = old_inihal_data.NAME;

int ini_hal_exit(void)
{
    hal_exit(comp_id);
    comp_id = -1;
    return 0;
}

int ini_hal_init(int numjoints)
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

    for (int idx = 0; idx < numjoints; idx++) {
        MAKE_FLOAT_PIN_IDX(joint_backlash,backlash,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_ferror,ferror,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_min_ferror,min_ferror,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_min_limit,min_limit,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_max_limit,max_limit,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_max_velocity,max_velocity,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_max_acceleration,max_acceleration,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_home,home,HAL_IN,idx);
        MAKE_FLOAT_PIN_IDX(joint_home_offset,home_offset,HAL_IN,idx);
        MAKE_S32_PIN_IDX(  joint_home_sequence,home_sequence,HAL_IN,idx);
    }
    for (int idx = 0; idx < EMCMOT_MAX_AXIS; idx++) {
        char letter = "xyzabcuvw"[idx];
        MAKE_FLOAT_PIN_LETTER(axis_min_limit,min_limit,HAL_IN,idx,letter);
        MAKE_FLOAT_PIN_LETTER(axis_max_limit,max_limit,HAL_IN,idx,letter);
        MAKE_FLOAT_PIN_LETTER(axis_max_velocity,max_velocity,HAL_IN,idx,letter);
        MAKE_FLOAT_PIN_LETTER(axis_max_acceleration,max_acceleration,HAL_IN,idx,letter);
    }

    MAKE_FLOAT_PIN(traj_default_velocity,HAL_IN);
    MAKE_FLOAT_PIN(traj_max_velocity,HAL_IN);
    MAKE_FLOAT_PIN(traj_default_acceleration,HAL_IN);
    MAKE_FLOAT_PIN(traj_max_acceleration,HAL_IN);

    MAKE_BIT_PIN(traj_arc_blend_enable,HAL_IN);
    MAKE_BIT_PIN(traj_arc_blend_fallback_enable,HAL_IN);
    MAKE_S32_PIN(traj_arc_blend_optimization_depth,HAL_IN);
    MAKE_FLOAT_PIN(traj_arc_blend_gap_cycles,HAL_IN);
    MAKE_FLOAT_PIN(traj_arc_blend_ramp_freq,HAL_IN);
    MAKE_FLOAT_PIN(traj_arc_blend_tangent_kink_ratio,HAL_IN);

    hal_ready(comp_id);
    return 0;
} // ini_hal_init()

int ini_hal_init_pins(int numjoints)
{
    INIT_PIN(traj_default_velocity);
    INIT_PIN(traj_max_velocity);
    INIT_PIN(traj_default_acceleration);
    INIT_PIN(traj_max_acceleration);

    INIT_PIN(traj_arc_blend_enable);
    INIT_PIN(traj_arc_blend_fallback_enable);
    INIT_PIN(traj_arc_blend_optimization_depth);
    INIT_PIN(traj_arc_blend_gap_cycles);
    INIT_PIN(traj_arc_blend_ramp_freq);
    INIT_PIN(traj_arc_blend_tangent_kink_ratio);

    for (int idx = 0; idx < numjoints; idx++) {
        INIT_PIN(joint_backlash[idx]);
        INIT_PIN(joint_ferror[idx]);
        INIT_PIN(joint_min_ferror[idx]);
        INIT_PIN(joint_min_limit[idx]);
        INIT_PIN(joint_max_limit[idx]);
        INIT_PIN(joint_max_velocity[idx]);
        INIT_PIN(joint_max_acceleration[idx]);
        INIT_PIN(joint_home[idx]);
        INIT_PIN(joint_home_offset[idx]);
        INIT_PIN(joint_home_sequence[idx]);
    }
    for (int idx = 0; idx < EMCMOT_MAX_AXIS; idx++) {
        INIT_PIN(axis_min_limit[idx]);
        INIT_PIN(axis_max_limit[idx]);
        INIT_PIN(axis_max_velocity[idx]);
        INIT_PIN(axis_max_acceleration[idx]);
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

int check_ini_hal_items(int numjoints)
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

    if (   CHANGED(traj_arc_blend_enable)
        || CHANGED(traj_arc_blend_fallback_enable)
        || CHANGED(traj_arc_blend_optimization_depth)
        || CHANGED(traj_arc_blend_gap_cycles)
        || CHANGED(traj_arc_blend_ramp_freq)
        || CHANGED(traj_arc_blend_tangent_kink_ratio)
       ) {
        if (debug) SHOW_CHANGE_ARC_BLEND()
        UPDATE(traj_arc_blend_enable);
        UPDATE(traj_arc_blend_fallback_enable);
        UPDATE(traj_arc_blend_optimization_depth);
        UPDATE(traj_arc_blend_gap_cycles);
        UPDATE(traj_arc_blend_ramp_freq);
        UPDATE(traj_arc_blend_tangent_kink_ratio);
        if (0 != emcSetupArcBlends(old_inihal_data.traj_arc_blend_enable
                                  ,old_inihal_data.traj_arc_blend_fallback_enable
                                  ,old_inihal_data.traj_arc_blend_optimization_depth
                                  ,old_inihal_data.traj_arc_blend_gap_cycles
                                  ,old_inihal_data.traj_arc_blend_ramp_freq
                                  ,old_inihal_data.traj_arc_blend_tangent_kink_ratio
                                  )) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcSetupArcBlends\n");
            }
            return -1;
        }
    }
    for (int idx = 0; idx < numjoints; idx++) {
        if (CHANGED_IDX(joint_backlash,idx) ) {
            if (debug) SHOW_CHANGE_IDX(joint_backlash,idx);
            UPDATE_IDX(joint_backlash,idx);
            if (0 != emcJointSetBacklash(idx,NEW(joint_backlash[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print("check_ini_hal_items:bad return value from emcJointSetBacklash\n");
                }
        }
        }
        if (CHANGED_IDX(joint_min_limit,idx) ) {
            if (debug) SHOW_CHANGE_IDX(joint_min_limit,idx);
            UPDATE_IDX(joint_min_limit,idx);
            if (0 != emcJointSetMinPositionLimit(idx,NEW(joint_min_limit[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcJointSetMinPositionLimit\n");
                }
            }
        }
        if (CHANGED_IDX(joint_max_limit,idx) ) {
            if (debug) SHOW_CHANGE_IDX(joint_max_limit,idx);
            UPDATE_IDX(joint_max_limit,idx);
            if (0 != emcJointSetMaxPositionLimit(idx,NEW(joint_max_limit[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcJointSetMaxPositionLimit\n");
                }
            }
        }
        if (CHANGED_IDX(joint_max_velocity,idx) ) {
            if (debug) SHOW_CHANGE_IDX(joint_max_velocity,idx);
            UPDATE_IDX(joint_max_velocity,idx);
            if (0 != emcJointSetMaxVelocity(idx, NEW(joint_max_velocity[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcJointSetMaxVelocity\n");
                }
            }
        }
        if (CHANGED_IDX(joint_max_acceleration,idx) ) {
            if (debug) SHOW_CHANGE_IDX(joint_max_acceleration,idx);
            UPDATE_IDX(joint_max_acceleration,idx);
            if (0 != emcJointSetMaxAcceleration(idx, NEW(joint_max_acceleration[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcJointSetMaxAcceleration\n");
                }
            }
        }
        if (   CHANGED_IDX(joint_home,idx)
            || CHANGED_IDX(joint_home_offset,idx)
            || CHANGED_IDX(joint_home_sequence,idx)
           ) {
            if (debug) {
                SHOW_CHANGE_IDX(joint_home,idx);
                SHOW_CHANGE_IDX(joint_home_offset,idx);
                SHOW_CHANGE_IDX_INT(joint_home_sequence,idx);
            }
            UPDATE_IDX(joint_home,idx);
            UPDATE_IDX(joint_home_offset,idx);
            UPDATE_IDX(joint_home_sequence,idx);
            if  (0 != emcJointUpdateHomingParams(idx, NEW(joint_home[idx]),
                                                      NEW(joint_home_offset[idx]),
                                                      NEW(joint_home_sequence[idx]))
                ) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcJointUpdateHomingParams\n");
                }
            }
        }
        if (CHANGED_IDX(joint_ferror,idx) ) {
            if (debug) SHOW_CHANGE_IDX(joint_ferror,idx);
            UPDATE_IDX(joint_ferror,idx);
            if (0 != emcJointSetFerror(idx,NEW(joint_ferror[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcJointSetFerror\n");
                }
            }
        }
        if (CHANGED_IDX(joint_min_ferror,idx) ) {
            if (debug) SHOW_CHANGE_IDX(joint_min_ferror,idx);
            UPDATE_IDX(joint_min_ferror,idx);
            if (0 != emcJointSetMinFerror(idx,NEW(joint_min_ferror[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcJointSetMinFerror\n");
                }
        }
        }
    } // numjoints

    for (int idx = 0; idx < EMCMOT_MAX_AXIS; idx++) {
        if (CHANGED_IDX(axis_min_limit,idx) ) {
            if (debug) SHOW_CHANGE_IDX(axis_min_limit,idx);
            UPDATE_IDX(axis_min_limit,idx);
            if (0 != emcAxisSetMinPositionLimit(idx,NEW(axis_min_limit[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMinPositionLimit\n");
                }
            }
        }
        if (CHANGED_IDX(axis_max_limit,idx) ) {
            if (debug) SHOW_CHANGE_IDX(axis_max_limit,idx);
            UPDATE_IDX(axis_max_limit,idx);
            if (0 != emcAxisSetMaxPositionLimit(idx,NEW(axis_max_limit[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMaxPositionLimit\n");
                }
            }
        }
        if (CHANGED_IDX(axis_max_velocity,idx) ) {
            if (debug) SHOW_CHANGE_IDX(axis_max_velocity,idx);
            UPDATE_IDX(axis_max_velocity,idx);
            if (0 != emcAxisSetMaxVelocity(idx,
                  (1 - ext_offset_a_or_v_ratio[idx]) * NEW(axis_max_velocity[idx]),
                  (    ext_offset_a_or_v_ratio[idx]) * NEW(axis_max_velocity[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMaxVelocity\n");
                }
            }
        }
        if (CHANGED_IDX(axis_max_acceleration,idx) ) {
            if (debug) SHOW_CHANGE_IDX(axis_max_acceleration,idx);
            UPDATE_IDX(axis_max_acceleration,idx);
            if (0 != emcAxisSetMaxAcceleration(idx,
                  (1 - ext_offset_a_or_v_ratio[idx]) * NEW(axis_max_acceleration[idx]),
                  (    ext_offset_a_or_v_ratio[idx]) * NEW(axis_max_acceleration[idx]))) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("check_ini_hal_items:bad return from emcAxisSetMaxAcceleration\n");
                }
            }
        }
    } // EMCMOT_MAX_AXIS

    return 0;
} // check_ini_hal_items

// vim: sts=4 sw=4 et
