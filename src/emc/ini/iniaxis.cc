/********************************************************************
* Description: iniaxis.cc
*   INI file initialization routines for axis NML
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004,2026 All rights reserved.
*
* Last change:
********************************************************************/

#include <fmt/format.h>

#include "nml_intf/emc.hh"
#include "nml_intf/emcglb.h"
#include "nml_intf/emccfg.h"
#include "libnml/rcs/rcs_print.hh"
#include "inifile.hh"

#include "inihal.hh"
#include "iniaxis.hh"

using namespace linuxcnc;

extern value_inihal_data old_inihal_data;
double ext_offset_a_or_v_ratio[EMCMOT_MAX_AXIS]; // all zero

// default ratio or external offset velocity,acceleration
#define DEFAULT_A_OR_V_RATIO 0.0

static void inline print_dbg_config(const std::string &s)
{
    if (emc_debug & EMC_DEBUG_CONFIG) {
        rcs_print_error("%s", (fmt::format("{}: failed\n", s)).c_str());
    }
}

//
// Load INI file params for axis [0,8]
//
// Section [AXIS_n] where 'n' is one of [XYZABCUVW].
//
// [AXIS_n]MIN_LIMIT <real>             Minimum soft position limit
// [AXIS_n]MAX_LIMIT <real>             Maximum soft position limit
// [AXIS_n]OFFSET_AV_RATIO <real>
// [AXIS_n]MAX_VELOCITY <real>          Maximum velocity for axis
// [AXIS_n]MAX_ACCELERATION <real>      Maximum acceleration for axis
// [AXIS_n]LOCKING_INDEXER_JOINT <int>
// [AXIS_n]MAX_JERK <real>
//
static int loadAxis(int axis, const IniFile &ini)
{
    if(axis < 0 || axis > 8) {
        rcs_print_error("Invalid axis index '%d' outside range [0,8]\n", axis);
        return -1;
    }

    std::string axisSection = fmt::format("AXIS_{}", "XYZABCUVW"[axis]);

    // set min position limit
    double limit = ini.findRealV("MIN_LIMIT", axisSection, -1e99);
    if (0 != emcAxisSetMinPositionLimit(axis, limit)) {
        print_dbg_config("emcAxisSetMinPositionLimit");
        return -1;
    }
    old_inihal_data.axis_min_limit[axis] = limit;

    // set max position limit
    limit = ini.findRealV("MAX_LIMIT", axisSection, 1e99);
    if (0 != emcAxisSetMaxPositionLimit(axis, limit)) {
        print_dbg_config("emcAxisSetMaxPositionLimit");
        return -1;
    }
    old_inihal_data.axis_max_limit[axis] = limit;

#define REPLACE_AV_RATIO 0.1
#define MAX_AV_RATIO     0.9
    double ratio = ini.findRealV("OFFSET_AV_RATIO", axisSection, DEFAULT_A_OR_V_RATIO);
    if (ratio < 0.0 || ratio > MAX_AV_RATIO) {
        rcs_print_error("Invalid: [%s]OFFSET_AV_RATIO=%8.5f (range [0.0,%f]); using: [%s]OFFSET_AV_RATIO=%8.5f\n",
                        axisSection.c_str(), ratio, MAX_AV_RATIO, axisSection.c_str(), REPLACE_AV_RATIO);
        ratio = REPLACE_AV_RATIO;
    }
    ext_offset_a_or_v_ratio[axis] = ratio;

    // set maximum velocities for axis: vel,ext_offset_vel
    double maxVelocity = ini.findRealV("MAX_VELOCITY", axisSection, DEFAULT_AXIS_MAX_VELOCITY);
    if (0 != emcAxisSetMaxVelocity(axis,
               (1 - ext_offset_a_or_v_ratio[axis]) * maxVelocity,
               (    ext_offset_a_or_v_ratio[axis]) * maxVelocity)) {
        print_dbg_config("emcAxisSetMaxVelocity");
        return -1;
    }
    old_inihal_data.axis_max_velocity[axis] = maxVelocity;

    // set maximum accels for axis: acc,ext_offset_acc
    double maxAcceleration = ini.findRealV("MAX_ACCELERATION", axisSection, DEFAULT_AXIS_MAX_ACCELERATION);
    if (0 != emcAxisSetMaxAcceleration(axis,
                (1 - ext_offset_a_or_v_ratio[axis]) * maxAcceleration,
                (    ext_offset_a_or_v_ratio[axis]) * maxAcceleration)) {
        print_dbg_config("emcAxisSetMaxAcceleration");
        return -1;
    }
    old_inihal_data.axis_max_acceleration[axis] = maxAcceleration;


    rtapi_s64 jnum = ini.findSIntV("LOCKING_INDEXER_JOINT", axisSection, (rtapi_s64)-1);
    if (0 != emcAxisSetLockingJoint(axis, jnum)) {
        print_dbg_config("emcAxisSetLockingJoint");
        return -1;
    }

    double maxJerk = ini.findRealV("MAX_JERK", axisSection, DEFAULT_AXIS_MAX_JERK);
    if (0 != emcAxisSetMaxJerk(axis, maxJerk)) {
        print_dbg_config("emcAxisSetMaxJerk");
        return -1;
    }
    old_inihal_data.axis_jerk[axis] = maxJerk;

    return 0;
}

//
// iniAxis(int axis, const char *filename)
// Loads INI file parameters for specified axis, [0 .. AXES - 1]
//
int iniAxis(int axis, const char *filename)
{
    IniFile ini(filename);
    if(!ini)
        return -1;

    return loadAxis(axis, ini);
}
