/********************************************************************
* Description: iniaxis_gomc.cc
*   INI file initialization routines for axis NML — gomc variant.
*   Uses gomc_ini_t instead of EmcIniFile.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* License: GPL Version 2
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "emc.hh"
#include "rcs_print.hh"
#include "iniaxis_gomc.hh"
#include "emcglb.h"
#include "emccfg.h"

#include "inihal_gomc.hh"

extern value_inihal_data old_inihal_data;
double ext_offset_a_or_v_ratio[EMCMOT_MAX_AXIS]; // all zero

#define DEFAULT_A_OR_V_RATIO 0
#define REPLACE_AV_RATIO 0.1
#define MAX_AV_RATIO     0.9

static const char *axisName(int axis)
{
    static const char * const names[] = {
        "AXIS_X","AXIS_Y","AXIS_Z",
        "AXIS_A","AXIS_B","AXIS_C",
        "AXIS_U","AXIS_V","AXIS_W",
    };
    if (axis >= 0 && axis < 9) return names[axis];
    return "AXIS_X";
}

static int loadAxis(int axis, const gomc_ini_t *ini)
{
    const char *section = axisName(axis);

    // min position limit
    double limit = gomc_ini_get_double(ini, section, "MIN_LIMIT", -1e99);
    if (0 != emcAxisSetMinPositionLimit(axis, limit)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print_error("bad return from emcAxisSetMinPositionLimit\n");
        }
        return -1;
    }
    old_inihal_data.axis_min_limit[axis] = limit;

    // max position limit
    limit = gomc_ini_get_double(ini, section, "MAX_LIMIT", 1e99);
    if (0 != emcAxisSetMaxPositionLimit(axis, limit)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print_error("bad return from emcAxisSetMaxPositionLimit\n");
        }
        return -1;
    }
    old_inihal_data.axis_max_limit[axis] = limit;

    // ext offset ratio
    ext_offset_a_or_v_ratio[axis] = DEFAULT_A_OR_V_RATIO;
    ext_offset_a_or_v_ratio[axis] = gomc_ini_get_double(ini, section,
                                        "OFFSET_AV_RATIO",
                                        (double)DEFAULT_A_OR_V_RATIO);
    if (ext_offset_a_or_v_ratio[axis] < 0
     || ext_offset_a_or_v_ratio[axis] > MAX_AV_RATIO) {
        rcs_print_error("\n   Invalid:[%s]OFFSET_AV_RATIO= %8.5f\n"
                          "   Using:  [%s]OFFSET_AV_RATIO= %8.5f\n",
                        section, ext_offset_a_or_v_ratio[axis],
                        section, REPLACE_AV_RATIO);
        ext_offset_a_or_v_ratio[axis] = REPLACE_AV_RATIO;
    }

    // max velocity
    double maxVelocity = gomc_ini_get_double(ini, section, "MAX_VELOCITY",
                                             DEFAULT_AXIS_MAX_VELOCITY);
    if (0 != emcAxisSetMaxVelocity(axis,
              (1 - ext_offset_a_or_v_ratio[axis]) * maxVelocity,
              (    ext_offset_a_or_v_ratio[axis]) * maxVelocity)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print_error("bad return from emcAxisSetMaxVelocity\n");
        }
        return -1;
    }
    old_inihal_data.axis_max_velocity[axis] = maxVelocity;

    // max acceleration
    double maxAcceleration = gomc_ini_get_double(ini, section, "MAX_ACCELERATION",
                                                 DEFAULT_AXIS_MAX_ACCELERATION);
    if (0 != emcAxisSetMaxAcceleration(axis,
              (1 - ext_offset_a_or_v_ratio[axis]) * maxAcceleration,
              (    ext_offset_a_or_v_ratio[axis]) * maxAcceleration)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print_error("bad return from emcAxisSetMaxAcceleration\n");
        }
        return -1;
    }
    old_inihal_data.axis_max_acceleration[axis] = maxAcceleration;

    // locking indexer joint
    int lockingjnum = gomc_ini_get_int(ini, section, "LOCKING_INDEXER_JOINT", -1);
    if (0 != emcAxisSetLockingJoint(axis, lockingjnum)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print_error("bad return from emcAxisSetLockingJoint\n");
        }
        return -1;
    }

    return 0;
}

int iniAxis(int axis, const gomc_ini_t *ini)
{
    if (0 != loadAxis(axis, ini)) {
        return -1;
    }
    return 0;
}

// vim: sts=4 sw=4 et
