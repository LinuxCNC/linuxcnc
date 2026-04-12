/********************************************************************
* Description: inijoint.cc
*   INI file initialization routines for joint/axis NML
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004-2009,2026 All rights reserved.
********************************************************************/

#include <fmt/format.h>

#include "nml_intf/emc.hh"
#include "libnml/rcs/rcs_print.hh"
#include "nml_intf/emcglb.h"
#include "nml_intf/emccfg.h"
#include "inifile.hh"

#include "inihal.hh"
#include "inijoint.hh"

using namespace linuxcnc;

extern value_inihal_data old_inihal_data;

static void inline print_dbg_config(const std::string &s)
{
    if (emc_debug & EMC_DEBUG_CONFIG) {
        rcs_print_error("%s", (fmt::format("{}: failed\n", s)).c_str());
    }
}

static EmcJointType getJointType(const IniFile &ini, const std::string &var, const std::string &sec, EmcJointType def)
{
	static const std::map<const std::string, const EmcJointType, IniFile::caseless> jointTypeMap = {
		{"LINEAR",  EMC_LINEAR },
		{"ANGULAR", EMC_ANGULAR}
	};
	if(auto c = ini.findMap(jointTypeMap, var, sec))
		return *c;
	return def;
}

//
// Load INI file params for joint
//
// Section [JOINT_n] where 'n' is a number in range [0,EMC_MAX_JOINT-1]
//
// [JOINT_n]TYPE <LINEAR,ANGULAR>              Type of joint
// [JOINT_n]BACKLASH <real>                    Backlash of joint
// [JOINT_n]MIN_LIMIT <real>                   Minimum soft position limit
// [JOINT_n]MAX_LIMIT <real>                   Maximum soft position limit
// [JOINT_n]FERROR <real>                      Maximum following error, scaled to maximum velocity
// [JOINT_n]MIN_FERROR <real>                  Minimum following error
// [JOINT_n]HOME <real>                        Home position (where to go after home)
// [JOINT_n]HOME_OFFSET <real>                 Home switch/index pulse location
// [JOINT_n]HOME_SEARCH_VEL <real>             Homing speed, search phase
// [JOINT_n]HOME_LATCH_VEL <real>              Homing speed, latch phase
// [JOINT_n]HOME_FINAL_VEL <real>              Speed to move from HOME_OFFSET to HOME location (at the end of homing)
// [JOINT_n]HOME_IS_SHARED <bool>              The home switch input is shared between joints
// [JOINT_n]HOME_USE_INDEX <bool>              Use index pulse when homing
// [JOINT_n]HOME_INDEX_NO_ENCODER_RESET <bool>
// [JOINT_n]HOME_IGNORE_LIMITS <bool>          Ignore limit switches when homing
// [JOINT_n]VOLATILE_HOME <bool>               Must re-home after estop or machine off
// [JOINT_n]LOCKING_INDEXER <bool>
// [JOINT_n]HOME_ABSOLUTE_ENCODER <int>
// [JOINT_n]HOME_SEQUENCE <int>
// [JOINT_n]MAX_VELOCITY <real>                Maximum velocity for joint (ds/dt)
// [JOINT_n]MAX_ACCELERATION <real>            Maximum acceleration for joint (dv/dt)
// [JOINT_n]MAX_JERK <real>                    Maximum jerk for joint (da/dt)
// [JOINT_n]COMP_FILE_TYPE <int>               Compensation file format [0,1]
// [JOINT_n]COMP_FILE <filename>               File of joint compensation points
//
static int loadJoint(int joint, const IniFile &ini)
{
    std::string jointSection = fmt::format("JOINT_{}", joint);

    EmcJointType jointType = getJointType(ini, "TYPE", jointSection, EMC_LINEAR);
    if (0 != emcJointSetType(joint, jointType)) {
        print_dbg_config("emcJointSetType");
        return -1;
    }

    double units;
    switch(jointType) {
    case EMC_LINEAR:  units = emcTrajGetLinearUnits(); break;
    case EMC_ANGULAR: units = emcTrajGetAngularUnits(); break;
    default: return -1;
    }
    if (0 != emcJointSetUnits(joint, units)) {
        print_dbg_config("emcJointSetUnits");
        return -1;
    }

    double backlash = ini.findRealV("BACKLASH", jointSection, 0.0);
    if (0 != emcJointSetBacklash(joint, backlash)) {
        print_dbg_config("emcJointSetBacklash");
        return -1;
    }
    old_inihal_data.joint_backlash[joint] = backlash;

    double limit = ini.findRealV("MIN_LIMIT", jointSection, -1e99);
    if (0 != emcJointSetMinPositionLimit(joint, limit)) {
        print_dbg_config("emcJointSetMinPositionLimit");
         return -1;
    }
    old_inihal_data.joint_min_limit[joint] = limit;

    limit = ini.findRealV("MAX_LIMIT", jointSection, 1e99);
    if (0 != emcJointSetMaxPositionLimit(joint, limit)) {
        print_dbg_config("emcJointSetMaxPositionLimit");
        return -1;
    }
    old_inihal_data.joint_max_limit[joint] = limit;

    double ferror = ini.findRealV("FERROR", jointSection, 1.0);
    if (0 != emcJointSetFerror(joint, ferror)) {
        print_dbg_config("emcJointSetFerror");
         return -1;
    }
    old_inihal_data.joint_ferror[joint] = ferror;

    // MIN_FERROR - uses default from FERROR above
    ferror = ini.findRealV("MIN_FERROR", jointSection, ferror);
    if (0 != emcJointSetMinFerror(joint, ferror)) {
        print_dbg_config("emcJointSetMinFerror");
        return -1;
    }
    old_inihal_data.joint_min_ferror[joint] = ferror;

    // Homing parameters
    double home   = ini.findRealV("HOME", jointSection, 0.0);
    old_inihal_data.joint_home[joint] = home;
    double offset = ini.findRealV("HOME_OFFSET", jointSection, 0.0);
    old_inihal_data.joint_home_offset[joint] = offset;

    double search_vel  = ini.findRealV("HOME_SEARCH_VEL", jointSection, 0.0);
    double latch_vel   = ini.findRealV("HOME_LATCH_VEL", jointSection, 0.0);
    double final_vel   = ini.findRealV("HOME_FINAL_VEL", jointSection, -1.0);
    bool is_shared     = ini.findBoolV("HOME_IS_SHARED", jointSection, false);
    bool use_index     = ini.findBoolV("HOME_USE_INDEX", jointSection, false);
    bool encoder_reset = ini.findBoolV("HOME_INDEX_NO_ENCODER_RESET", jointSection, false);
    bool ignore_limits = ini.findBoolV("HOME_IGNORE_LIMITS", jointSection, false);
    bool volatile_home = ini.findBoolV("VOLATILE_HOME", jointSection, false);
    bool locking_idxer = ini.findBoolV("LOCKING_INDEXER", jointSection, false);
    int abs_encoder    = ini.findIntV("HOME_ABSOLUTE_ENCODER", jointSection, 0, 0, 2);

    // Sequence defaults to an unrealizable and positive sequence so that
    // joints with unspecified HOME_SEQUENCE= will not be homed in home-all
    int sequence = ini.findIntV("HOME_SEQUENCE", jointSection, 999);
    old_inihal_data.joint_home_sequence[joint] = sequence;

    if (0 != emcJointSetHomingParams(joint, home, offset,
                                     final_vel, search_vel, latch_vel,
                                     use_index,
                                     encoder_reset,
                                     ignore_limits,
                                     is_shared,
                                     sequence,
                                     volatile_home,
                                     locking_idxer,
                                     abs_encoder)) {
        print_dbg_config("emcJointSetHomingParams");
        return -1;
    }

    // Velocity, acceleration and jerk
    double maxVelocity = ini.findRealV("MAX_VELOCITY", jointSection, DEFAULT_JOINT_MAX_VELOCITY);
    if (0 != emcJointSetMaxVelocity(joint, maxVelocity)) {
        print_dbg_config("emcJointSetMaxVelocity");
        return -1;
    }
    old_inihal_data.joint_max_velocity[joint] = maxVelocity;

    double maxAcceleration = ini.findRealV("MAX_ACCELERATION", jointSection, DEFAULT_JOINT_MAX_ACCELERATION);
    if (0 != emcJointSetMaxAcceleration(joint, maxAcceleration)) {
        print_dbg_config("emcJointSetMaxAcceleration");
        return -1;
    }
    old_inihal_data.joint_max_acceleration[joint] = maxAcceleration;

    double maxJerk = ini.findRealV("MAX_JERK", jointSection, DEFAULT_JOINT_MAX_JERK);
    if (0 != emcJointSetMaxJerk(joint, maxJerk)) {
        print_dbg_config("emcJointSetMaxJerk");
        return -1;
    }
    old_inihal_data.joint_jerk[joint] = maxJerk;

    // Compensation file (backlash alternative)
    int comp_file_type = ini.findIntV("COMP_FILE_TYPE", jointSection, 0, 0, 1);
    if(auto comp_file = ini.findString("COMP_FILE", jointSection)) {
        if (0 != emcJointLoadComp(joint, comp_file->c_str(), comp_file_type)) {
        print_dbg_config("emcJointLoadComp");
            return -1;
        }
    }

    // Activate joint.
    // Doing this last should prevent the motion controller to flag errors
    // during configuration
    if (0 != emcJointActivate(joint)) {
        print_dbg_config("emcJointActivate");
        return -1;
    }

    return 0;
}

//
// iniJoint(int joint, const char *filename)
// Loads INI file parameters for specified joint
//
int iniJoint(int joint, const char *filename)
{
    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
        rcs_print_error("iniJoint: Invalid joint '%d'", joint);
        return -1;
    }

    IniFile ini(filename);
    if (!ini)
        return -1;

    return loadJoint(joint, ini);
}
