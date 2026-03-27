/********************************************************************
* Description: inijoint_gomc.cc
*   INI file initialization routines for joint NML — gomc variant.
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
#include "inijoint_gomc.hh"
#include "emcglb.h"
#include "emccfg.h"

#include "inihal_gomc.hh"

extern value_inihal_data old_inihal_data;

static int loadJoint(int joint, const gomc_ini_t *ini)
{
    char section[16];
    snprintf(section, sizeof(section), "JOINT_%d", joint);

    // joint type
    EmcJointType jointType = EMC_LINEAR;
    const char *typeStr = ini->get(ini->ctx, section, "TYPE");
    if (typeStr) {
        if (!strcasecmp(typeStr, "ANGULAR"))
            jointType = EMC_ANGULAR;
    }
    if (0 != emcJointSetType(joint, jointType)) {
        return -1;
    }

    // units
    double units;
    if (jointType == EMC_LINEAR) {
        units = emcTrajGetLinearUnits();
    } else {
        units = emcTrajGetAngularUnits();
    }
    if (0 != emcJointSetUnits(joint, units)) {
        return -1;
    }

    // backlash
    double backlash = gomc_ini_get_double(ini, section, "BACKLASH", 0);
    if (0 != emcJointSetBacklash(joint, backlash)) {
        return -1;
    }
    old_inihal_data.joint_backlash[joint] = backlash;

    // min position limit
    double limit = gomc_ini_get_double(ini, section, "MIN_LIMIT", -1e99);
    if (0 != emcJointSetMinPositionLimit(joint, limit)) {
        return -1;
    }
    old_inihal_data.joint_min_limit[joint] = limit;

    // max position limit
    limit = gomc_ini_get_double(ini, section, "MAX_LIMIT", 1e99);
    if (0 != emcJointSetMaxPositionLimit(joint, limit)) {
        return -1;
    }
    old_inihal_data.joint_max_limit[joint] = limit;

    // following error limit (at max speed)
    double ferror = gomc_ini_get_double(ini, section, "FERROR", 1);
    if (0 != emcJointSetFerror(joint, ferror)) {
        return -1;
    }
    old_inihal_data.joint_ferror[joint] = ferror;

    // min following error
    ferror = gomc_ini_get_double(ini, section, "MIN_FERROR", ferror);
    if (0 != emcJointSetMinFerror(joint, ferror)) {
        return -1;
    }
    old_inihal_data.joint_min_ferror[joint] = ferror;

    // homing parameters
    double home       = gomc_ini_get_double(ini, section, "HOME", 0);
    old_inihal_data.joint_home[joint] = home;

    double offset     = gomc_ini_get_double(ini, section, "HOME_OFFSET", 0);
    old_inihal_data.joint_home_offset[joint] = offset;

    double search_vel = gomc_ini_get_double(ini, section, "HOME_SEARCH_VEL", 0);
    double latch_vel  = gomc_ini_get_double(ini, section, "HOME_LATCH_VEL", 0);
    double final_vel  = gomc_ini_get_double(ini, section, "HOME_FINAL_VEL", -1);
    int is_shared     = gomc_ini_get_int(ini, section, "HOME_IS_SHARED", 0);
    int use_index     = gomc_ini_get_int(ini, section, "HOME_USE_INDEX", 0);
    int encoder_does_not_reset = gomc_ini_get_int(ini, section, "HOME_INDEX_NO_ENCODER_RESET", 0);
    int ignore_limits = gomc_ini_get_int(ini, section, "HOME_IGNORE_LIMITS", 0);

    int sequence = gomc_ini_get_int(ini, section, "HOME_SEQUENCE", 999);
    old_inihal_data.joint_home_sequence[joint] = sequence;

    int volatile_home     = gomc_ini_get_int(ini, section, "VOLATILE_HOME", 0);
    int locking_indexer   = gomc_ini_get_int(ini, section, "LOCKING_INDEXER", 0);
    int absolute_encoder  = gomc_ini_get_int(ini, section, "HOME_ABSOLUTE_ENCODER", 0);

    if (0 != emcJointSetHomingParams(joint, home, offset,
                                     final_vel, search_vel, latch_vel,
                                     use_index,
                                     encoder_does_not_reset,
                                     ignore_limits,
                                     is_shared,
                                     sequence,
                                     volatile_home,
                                     locking_indexer,
                                     absolute_encoder)) {
        return -1;
    }

    // max velocity
    double maxVelocity = gomc_ini_get_double(ini, section, "MAX_VELOCITY",
                                             DEFAULT_JOINT_MAX_VELOCITY);
    if (0 != emcJointSetMaxVelocity(joint, maxVelocity)) {
        return -1;
    }
    old_inihal_data.joint_max_velocity[joint] = maxVelocity;

    // max acceleration
    double maxAcceleration = gomc_ini_get_double(ini, section, "MAX_ACCELERATION",
                                                 DEFAULT_JOINT_MAX_ACCELERATION);
    if (0 != emcJointSetMaxAcceleration(joint, maxAcceleration)) {
        return -1;
    }
    old_inihal_data.joint_max_acceleration[joint] = maxAcceleration;

    // compensation file
    int comp_file_type = gomc_ini_get_int(ini, section, "COMP_FILE_TYPE", 0);
    const char *comp_file = ini->get(ini->ctx, section, "COMP_FILE");
    if (comp_file) {
        if (0 != emcJointLoadComp(joint, comp_file, comp_file_type)) {
            return -1;
        }
    }

    // lastly, activate joint
    if (0 != emcJointActivate(joint)) {
        return -1;
    }

    return 0;
}

int iniJoint(int joint, const gomc_ini_t *ini)
{
    if (0 != loadJoint(joint, ini)) {
        return -1;
    }
    return 0;
}

// vim: sts=4 sw=4 et
