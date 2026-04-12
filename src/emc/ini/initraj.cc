/********************************************************************
* Description: initraj.cc
*   INI file initialization for trajectory level
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2026 All rights reserved.
********************************************************************/

#include <stdlib.h>
#include <fmt/format.h>

#include "nml_intf/emc.hh"
#include <emcpos.h>
#include "libnml/rcs/rcs_print.hh"
#include "nml_intf/emcglb.h"
#include "inifile.hh"

#include "inihal.hh"
#include "initraj.hh"

using namespace linuxcnc;

extern value_inihal_data old_inihal_data;

static void inline print_dbg_config(const std::string &s)
{
    if (emc_debug & EMC_DEBUG_CONFIG) {
        rcs_print_error("%s", (fmt::format("{}: failed\n", s)).c_str());
    }
}

static double findLinearUnits(const IniFile &ini, const std::string &var, const std::string &sec, double def)
{
    // The const map holds pairs for linear units which are valid under the
    // [TRAJ] section. These are of the form {"name", value}.
    // If the name "name" is encountered in the INI, the value will be used.
    static const std::map<const std::string, const double> linearUnitsMap = {
        { "mm",         1.0 },
        { "metric",     1.0 },
        { "in",         1/25.4 },
        { "inch",       1/25.4 },
        { "imperial",   1/25.4 },
    };

    if(auto c = ini.findMap(linearUnitsMap, var, sec))
        return *c;
    return def;
}

static double findAngularUnits(const IniFile &ini, const std::string &var, const std::string &sec, double def)
{
    // The const map holds pairs for angular units which are valid under
    // the [TRAJ] section. These are of the form {"name", value}.
    // If the name "name" is encountered in the INI, the value will be used.
    static const std::map<const std::string, const double, IniFile::caseless> angularUnitsMap = {
        { "deg",        1.0 },
        { "degree",     1.0 },
        { "grad",       0.9 },
        { "gon",        0.9 },
        { "rad",        M_PI / 180.0 },
        { "radian",     M_PI / 180.0 },
    };

    if(auto c = ini.findMap(angularUnitsMap, var, sec))
        return *c;
    return def;
}

//
// loadKins()
//
// JOINTS <int>   number of joints (DOF) in system
//
static int loadKins(const IniFile &ini)
{
    int joints = ini.findIntV("JOINTS", "KINS", 0);
    if (0 != emcTrajSetJoints(joints)) {
        print_dbg_config("emcTrajSetJoints");
        return -1;
    }
    return 0;
}


//
// loadTraj()
//
// Load INI file params for traj
// Note: MAX_FEED_OVERRIDE comes from the [DISPLAY] section
//
// [TRAJ]SPINDLES <int>                      Number of spindles configured
// [TRAJ]COORDINATES <char[]>                Axes configured in system (sets axis mask)
// [TRAJ]LINEAR_UNITS <real>                 Units per mm
// [TRAJ]ANGULAR_UNITS <real>                Units per degree
// [TRAJ]DEFAULT_LINEAR_VELOCITY <real>      Default linear velocity (ds/dt)
// [TRAJ]MAX_LINEAR_VELOCITY <real>          Maximum linear velocity (ds/dt)
// [TRAJ]DEFAULT_LINEAR_ACCELERATION <real>  Default linear acceleration (dv/dt)
// [TRAJ]MAX_LINEAR_ACCELERATION <real>      Maximum linear acceleration (dv/dt)
// [TRAJ]MAX_LINEAR_JERK <real>              Maximum linear jerk (da/dt)
// [TRAJ]PLANNER_TYPE <int>                  Planner type (0=standard, 1=S-curve)
// [TRAJ]ARC_BLEND_ENABLE <bool>             S-curve planner settings
// [TRAJ]ARC_BLEND_FALLBACK_ENABLE <bool>    ...
// [TRAJ]ARC_BLEND_OPTIMIZATION_DEPTH <int>  ...
// [TRAJ]ARC_BLEND_GAP_CYCLES <int>          ...
// [TRAJ]ARC_BLEND_RAMP_FREQ <real>          ...
// [TRAJ]ARC_BLEND_KINK_RATIO <real>         ...
// [DISPLAY]MAX_FEED_OVERRIDE <real>
// [TRAJ]NO_PROBE_JOG_ERROR <bool>
// [TRAJ]NO_PROBE_HOME_ERROR <bool>
// [TRAJ]HOME <real[]>                       Home position (pose) one coordinate for each axis
//
static int loadTraj(const IniFile &ini)
{
    int spindles = ini.findIntV("SPINDLES", "TRAJ", 1);
    if (0 != emcTrajSetSpindles(spindles)) {
        print_dbg_config("emcTrajSetSpindles");
        return -1;
    }

    int axismask = 0;
    if (auto coord = ini.findString("COORDINATES", "TRAJ")) {
        static const std::string axes{"XYZABCUVW"};
        for (auto c : *coord) {
            size_t n = axes.find_first_of(std::toupper(c & 0xff));
            if (n != std::string::npos)
                axismask |= 1 << n;
        }
    } else {
        axismask = (1<<0) | (1<<1) | (1<<2); // X, Y and Z machine
    }
    if (0 != emcTrajSetAxes(axismask)) {
        print_dbg_config("emcTrajSetAxes");
        return -1;
    }


    double linearUnits  = findLinearUnits(ini, "LINEAR_UNITS", "TRAJ", 0.0);
    double angularUnits = findAngularUnits(ini, "ANGULAR_UNITS", "TRAJ", 0.0);
    if (0 != emcTrajSetUnits(linearUnits, angularUnits)) {
        rcs_print("emcTrajSetUnits failed to set [TRAJ]LINEAR_UNITS or [TRAJ]ANGULAR_UNITS\n");
        return -1;
    }

    double velocity = ini.findRealV("DEFAULT_LINEAR_VELOCITY", "TRAJ", 1.0);
    if (0 != emcTrajSetVelocity(0.0, velocity)) { // Default velocity=0.0 on startup
        print_dbg_config("emcTrajSetVelocity");
        return -1;
    }
    old_inihal_data.traj_default_velocity = velocity;


    velocity = ini.findRealV("MAX_LINEAR_VELOCITY", "TRAJ", 1e99);
    if (0 != emcTrajSetMaxVelocity(velocity)) {
        print_dbg_config("emcTrajSetMaxVelocity");
        return -1;
    }
    old_inihal_data.traj_max_velocity = velocity;


    double accel = ini.findRealV("DEFAULT_LINEAR_ACCELERATION", "TRAJ", 1e99);
    if (0 != emcTrajSetAcceleration(accel)) {
        print_dbg_config("emcTrajSetAcceleration");
        return -1;
    }
    old_inihal_data.traj_default_acceleration = accel;

    accel = ini.findRealV("MAX_LINEAR_ACCELERATION", "TRAJ", 1e99);
    if (0 != emcTrajSetMaxAcceleration(accel)) {
        print_dbg_config("emcTrajSetMaxAcceleration");
        return -1;
    }
    old_inihal_data.traj_max_acceleration = accel;

    // Set max jerk (default to 1e9 if not specified in INI)
    double jerk = ini.findRealV("MAX_LINEAR_JERK", "TRAJ", 1e9);
    // Set both current and max jerk
    if (0 != emcTrajSetMaxJerk(jerk)) {
        print_dbg_config("emcTrajSetMaxJerk");
        return -1;
    }
    old_inihal_data.traj_max_jerk = jerk;
    if (0 != emcTrajSetJerk(jerk)) {
        print_dbg_config("emcTrajSetJerk");
        return -1;
    }

    // Planner: 0 = trapezoidal, 1 = S-curve
    // Default = 0
    int planner_type = ini.findIntV("PLANNER_TYPE", "TRAJ", 0, 0, 1);
    // Also force planner type 0 if max_jerk < 1 (S-curve needs valid jerk)
    if (planner_type == 1 && jerk < 1.0) {
        // FIXME: Should write a warning message to the user
        planner_type = 0;
    }
    if (0 != emcTrajPlannerType(planner_type)) {
        print_dbg_config("emcTrajPlannerType");
        return -1;
    }
    old_inihal_data.traj_planner_type = planner_type;

    int arcBlendEnable = ini.findBoolV("ARC_BLEND_ENABLE", "TRAJ", true);
    int arcBlendFallbackEnable = ini.findBoolV("ARC_BLEND_FALLBACK_ENABLE", "TRAJ", false);
    int arcBlendOptDepth = ini.findIntV("ARC_BLEND_OPTIMIZATION_DEPTH", "TRAJ", 50);
    int arcBlendGapCycles = ini.findIntV("ARC_BLEND_GAP_CYCLES", "TRAJ", 4);
    double arcBlendRampFreq = ini.findRealV("ARC_BLEND_RAMP_FREQ", "TRAJ", 100.0);
    double arcBlendTangentKinkRatio = ini.findRealV("ARC_BLEND_KINK_RATIO", "TRAJ", 0.1);
    if (0 != emcSetupArcBlends(arcBlendEnable, arcBlendFallbackEnable,
                arcBlendOptDepth, arcBlendGapCycles, arcBlendRampFreq, arcBlendTangentKinkRatio)) {
        print_dbg_config("emcSetupArcBlends");
        return -1;
    }
    old_inihal_data.traj_arc_blend_enable = arcBlendEnable;
    old_inihal_data.traj_arc_blend_fallback_enable = arcBlendFallbackEnable;
    old_inihal_data.traj_arc_blend_optimization_depth = arcBlendOptDepth;
    old_inihal_data.traj_arc_blend_gap_cycles = arcBlendGapCycles;
    old_inihal_data.traj_arc_blend_ramp_freq = arcBlendRampFreq;
    old_inihal_data.traj_arc_blend_tangent_kink_ratio = arcBlendTangentKinkRatio;
    //TODO update inihal

    double maxFeedScale = ini.findRealV("MAX_FEED_OVERRIDE", "DISPLAY", 1.0);
    if (0 != emcSetMaxFeedOverride(maxFeedScale)) {
        print_dbg_config("emcSetMaxFeedOverride");
        return -1;
    }

    bool j_inhibit = ini.findBoolV("NO_PROBE_JOG_ERROR", "TRAJ", false);
    bool h_inhibit = ini.findBoolV("NO_PROBE_HOME_ERROR", "TRAJ", false);
    if (0 != emcSetProbeErrorInhibit(j_inhibit, h_inhibit)) {
        print_dbg_config("emcSetProbeErrorInhibit");
        return -1;
    }

    if (auto inistring = ini.findString("HOME", "TRAJ")) {
        std::vector<std::string> toks = IniFile::split(" \t", *inistring);
	// NOTE: originally, this code would only set axes X, Y and Z and
	// ignore everything else. Now all axes are set if provided in the
	// [TRAJ]HOME position.
	EmcPose homePose{};
        for (size_t i = 0; i < toks.size() && i <= EMCMOT_MAX_AXIS; i++) {
            char *eptr;
            errno = 0;
            double val = strtod(toks[i].c_str(), &eptr);
            if (errno || *eptr || eptr == toks[i].c_str()) {
                rcs_print_error("Invalid value '%s' for axis %zu in homePose\n", toks[i].c_str(), i);
                return -1;
            }
            switch(i) {
            case 0: homePose.tran.x = val; break;
            case 1: homePose.tran.y = val; break;
            case 2: homePose.tran.z = val; break;
            case 3: homePose.a = val; break;
            case 4: homePose.b = val; break;
            case 5: homePose.c = val; break;
            case 6: homePose.u = val; break;
            case 7: homePose.v = val; break;
            case 8: homePose.w = val; break;
            default:
                // Should never trigger because of EMCMOT_MAX_AXIS, but you never know
                rcs_print_error("Value for invalid axis number %zu cannot be part of homePose\n", i);
                return -1;
            }
        }
        if (0 != emcTrajSetHome(homePose)) {
            print_dbg_config("emcTrajSetHome");
            return -1;
        }
    }
    return 0;
}

/*
  iniTraj(const char *filename)

  Loads INI file parameters for trajectory, from [TRAJ] section
 */
int iniTraj(const char *filename)
{
    IniFile ini(filename);
    if (!ini)
        return -1;

    if (loadKins(ini)) {
	return -1;
    }

    return loadTraj(ini);
}
