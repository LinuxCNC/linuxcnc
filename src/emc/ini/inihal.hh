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
#ifndef INIHAL_H
#define INIHAL_H
#include <hal.h>
#include <emcmotcfg.h>

int check_ini_hal_items(int numjoints);
int ini_hal_init(int numjoints);
int ini_hal_exit(void);
int ini_hal_init_pins(int numjoints);

/* not doing these fields (yet,ever?):
[TRAJ]COORDINATES
[TRAJ]LINEAR_UNITS
[TRAJ]ANGULAR_UNITS
[JOINT_n]TYPE
[JOINT_n]UNITS
[JOINT_n]HOME
[JOINT_n]HOME_OFFSET
[JOINT_n]HOME_SEARCH_VEL
[JOINT_n]HOME_LATCH_VEL
[JOINT_n]HOME_FINAL_VEL
[JOINT_n]HOME_IS_SHARED
[JOINT_n]HOME_USE_INDEX
[JOINT_n]HOME_IGNORE_LIMITS
[JOINT_n]HOME_SEQUENCE
[JOINT_n]VOLATILE_HOME
[JOINT_n]LOCKING_INDEXER
[JOINT_n]COMP_FILE_TYPE
[JOINT_n]COMP
*/
// FIXME: This will have to go again when we do proper 64-bit
// The typedefs are necessary to map to hal_[gs]et_[su]i32() in the expansion
// of below macros because they are selected based on text-concatenation in the
// preprocessor. Even the 32-bit version use sint/uint, but the expansion would
// then select the 64-bit versions.
typedef hal_sint_t hal_si32_t;
typedef hal_uint_t hal_ui32_t;

#define HAL_FIELDS \
    FIELD(real,traj_default_velocity) \
    FIELD(real,traj_max_velocity) \
    FIELD(real,traj_default_acceleration) \
    FIELD(real,traj_max_acceleration) \
    FIELD(real,traj_max_jerk) \
    FIELD(si32,traj_planner_type) \
\
    FIELD(bool,traj_arc_blend_enable) \
    FIELD(bool,traj_arc_blend_fallback_enable) \
    FIELD(si32,traj_arc_blend_optimization_depth) \
    FIELD(real,traj_arc_blend_gap_cycles) \
    FIELD(real,traj_arc_blend_ramp_freq) \
    FIELD(real,traj_arc_blend_tangent_kink_ratio) \
\
    ARRAY(real,joint_backlash,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_ferror,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_min_ferror,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_min_limit,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_max_limit,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_max_velocity,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_max_acceleration,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_jerk,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_home,EMCMOT_MAX_JOINTS) \
    ARRAY(real,joint_home_offset,EMCMOT_MAX_JOINTS) \
    ARRAY(si32,joint_home_sequence,EMCMOT_MAX_JOINTS) \
\
    ARRAY(real,axis_min_limit,EMCMOT_MAX_AXIS) \
    ARRAY(real,axis_max_limit,EMCMOT_MAX_AXIS) \
    ARRAY(real,axis_max_velocity,EMCMOT_MAX_AXIS) \
    ARRAY(real,axis_max_acceleration,EMCMOT_MAX_AXIS) \
    ARRAY(real,axis_jerk,EMCMOT_MAX_AXIS) \

struct PTR {
    template<class T>
    struct field { typedef T type; };
};

#pragma GCC diagnostic push
#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
template<class T> struct NATIVE {};
template<> struct NATIVE<hal_bool_t> { typedef rtapi_bool type; };
template<> struct NATIVE<hal_si32_t> { typedef rtapi_s32 type; };
template<> struct NATIVE<hal_ui32_t> { typedef rtapi_u32 type; };
// FIXME: These need to be 64-bit. Can't set them now because the compiler sees
// the typedef mapped overlap.
//template<> struct NATIVE<hal_sint_t> { typedef rtapi_sint type; };
//template<> struct NATIVE<hal_uint_t> { typedef rtapi_uint type; };
template<> struct NATIVE<hal_real_t> { typedef rtapi_real type; };
struct VALUE {
    template<class T> struct field { typedef typename NATIVE<T>::type type; };
};

template<class T>
struct inihal_base
{
#define FIELD(t,f) typename T::template field<hal_##t##_t>::type f;
#define ARRAY(t,f,n) typename T::template field<hal_##t##_t>::type f[n];
HAL_FIELDS
#undef FIELD
#undef ARRAY
};

typedef inihal_base<PTR> ptr_inihal_data;
typedef inihal_base<VALUE> value_inihal_data;
#pragma GCC diagnostic pop

#endif
