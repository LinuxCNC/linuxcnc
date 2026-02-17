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
#include <rtapi_string.h>
#include "motion.h"       // For emcmotConfig, emcmot_config_t

// Userspace kinematics for trajectory planning
#include "userspace_kinematics.hh"

// Predictive handoff configuration for feed override handling
#include "motion_planning_9d.hh"

using namespace linuxcnc;

extern value_inihal_data old_inihal_data;

/* Access to emcmotConfig from usrmotintf.cc (initialized by usrmotInit()) */
extern emcmot_config_t *emcmotConfig;

static void inline print_dbg_config(const std::string &s)
{
    if (emc_debug & EMC_DEBUG_CONFIG) {
        rcs_print_error("%s", (fmt::format("{}: failed\n", s)).c_str());
    }
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


    double linearUnits  = ini.findLinearUnits("LINEAR_UNITS", "TRAJ", 0.0);
    double angularUnits = ini.findAngularUnits("ANGULAR_UNITS", "TRAJ", 0.0);
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

    // Planner: 0 = trapezoidal, 1 = S-curve, 2 = 9D (EXPERIMENTAL)
    // Default = 0
    int planner_type = ini.findIntV("PLANNER_TYPE", "TRAJ", 0, 0, 2);
    // Force planner type 0 if max_jerk < 1 (S-curve needs valid jerk)
    if (planner_type == 1 && jerk < 1.0) {
        planner_type = 0;
    }
    if (planner_type == 2) {
        rcs_print("PLANNER_TYPE 2 (9D) is EXPERIMENTAL. Use with caution.\n");

        // Planner type 2 requires userspace kinematics - enable automatically
        {
            // Read kinematics module name from emcmotConfig (set by motion module)
            const char *kins_module = emcmotConfig->kins_module_name;

            if (!kins_module || kins_module[0] == '\0') {
                rcs_print_error("ERROR: No kinematics module name in emcmotConfig\n");
                rcs_print_error("PLANNER_TYPE 2 requires a kinematics module to be loaded.\n");
                rcs_print_error("Ensure your HAL file loads a kinematics module (e.g., loadrt trivkins)\n");
                return -1;
            }

            rcs_print("Detected kinematics module: %s\n", kins_module);

            // Get configuration from INI file
            auto coord = ini.findString("COORDINATES", "TRAJ");
            int joints = ini.findIntV("JOINTS", "KINS", 3);
            if (joints <= 0) joints = 3;

            if (motion_planning::userspace_kins_init(kins_module,
                                                    joints,
                                                    coord ? coord->c_str() : "XYZ") != 0) {
                rcs_print_error("WARNING: Failed to initialize userspace kinematics for '%s'\n",
                                kins_module);
                rcs_print_error("  Falling back to PLANNER_TYPE 0 (trapezoidal)\n");
                planner_type = 0;
            } else {
                rcs_print("Userspace kinematics initialized (module=%s, joints=%d, coords=%s)\n",
                          kins_module, joints, coord ? coord->c_str() : "XYZ");
            }
        }

        // Parse 9D-specific parameters
        int optimization_depth = ini.findIntV("OPTIMIZATION_DEPTH", "TRAJ", 8);
        double ramp_frequency = ini.findRealV("RAMP_FREQUENCY", "TRAJ", 10.0);
        int smoothing_passes = ini.findIntV("SMOOTHING_PASSES", "TRAJ", 2);
        int tc_queue_size = ini.findIntV("TC_QUEUE_SIZE", "TRAJ", 50);

        // Validate 9D parameters
        if (optimization_depth < 4 || optimization_depth > 200) {
            rcs_print("Warning: OPTIMIZATION_DEPTH %d out of range [4,200], using default 8\n", optimization_depth);
            optimization_depth = 8;
        }
        if (ramp_frequency < 1.0 || ramp_frequency > 1000.0) {
            rcs_print("Warning: RAMP_FREQUENCY %.1f out of range [1.0,1000.0], using default 10.0\n", ramp_frequency);
            ramp_frequency = 10.0;
        }
        if (smoothing_passes < 1 || smoothing_passes > 10) {
            rcs_print("Warning: SMOOTHING_PASSES %d out of range [1,10], using default 2\n", smoothing_passes);
            smoothing_passes = 2;
        }
        if (tc_queue_size < 32 || tc_queue_size > 400) {
            rcs_print("Warning: TC_QUEUE_SIZE %d out of range [32,400], using default 50\n", tc_queue_size);
            tc_queue_size = 50;
        }

        rcs_print("9D Planner Configuration:\n");
        rcs_print("  OPTIMIZATION_DEPTH = %d\n", optimization_depth);
        rcs_print("  RAMP_FREQUENCY = %.1f Hz\n", ramp_frequency);
        rcs_print("  SMOOTHING_PASSES = %d\n", smoothing_passes);
        rcs_print("  TC_QUEUE_SIZE = %d\n", tc_queue_size);

        // TODO: Pass these parameters to motion controller
        // For now, they will be hardcoded in motion_planning_9d.cc

        // Predictive handoff configuration for feed override handling
        // All timing values in milliseconds
        double handoff_horizon_ms = ini.findRealV("HANDOFF_HORIZON_MS", "TRAJ", 100.0);
        double branch_window_ms = ini.findRealV("BRANCH_WINDOW_MS", "TRAJ", 50.0);
        double min_buffer_time_ms = ini.findRealV("MIN_BUFFER_TIME_MS", "TRAJ", 100.0);
        double target_buffer_time_ms = ini.findRealV("TARGET_BUFFER_TIME_MS", "TRAJ", 200.0);
        double max_buffer_time_ms = ini.findRealV("MAX_BUFFER_TIME_MS", "TRAJ", 500.0);
        double feed_override_debounce_ms = ini.findRealV("FEED_OVERRIDE_DEBOUNCE_MS", "TRAJ", 50.0);

        // Validate predictive handoff timing parameters
        if (handoff_horizon_ms < 1.0 || handoff_horizon_ms > 1000.0) {
            rcs_print("Warning: HANDOFF_HORIZON_MS %.1f out of range [1,1000], using default 100\n", handoff_horizon_ms);
            handoff_horizon_ms = 100.0;
        }
        if (branch_window_ms < 10.0 || branch_window_ms > 500.0) {
            rcs_print("Warning: BRANCH_WINDOW_MS %.1f out of range [10,500], using default 50\n", branch_window_ms);
            branch_window_ms = 50.0;
        }
        if (min_buffer_time_ms < 10.0 || min_buffer_time_ms > 1000.0) {
            rcs_print("Warning: MIN_BUFFER_TIME_MS %.1f out of range [10,1000], using default 100\n", min_buffer_time_ms);
            min_buffer_time_ms = 100.0;
        }
        if (target_buffer_time_ms < min_buffer_time_ms || target_buffer_time_ms > 2000.0) {
            rcs_print("Warning: TARGET_BUFFER_TIME_MS %.1f out of range [%.1f,2000], using default 200\n",
                      target_buffer_time_ms, min_buffer_time_ms);
            target_buffer_time_ms = 200.0;
        }
        if (max_buffer_time_ms < target_buffer_time_ms || max_buffer_time_ms > 5000.0) {
            rcs_print("Warning: MAX_BUFFER_TIME_MS %.1f out of range [%.1f,5000], using default 500\n",
                      max_buffer_time_ms, target_buffer_time_ms);
            max_buffer_time_ms = 500.0;
        }
        if (feed_override_debounce_ms < 1.0 || feed_override_debounce_ms > 500.0) {
            rcs_print("Warning: FEED_OVERRIDE_DEBOUNCE_MS %.1f out of range [1,500], using default 50\n", feed_override_debounce_ms);
            feed_override_debounce_ms = 50.0;
        }

        // Get servo cycle time from motion config (already initialized)
        double servo_cycle_time_sec = 0.001;
        if (emcmotConfig && emcmotConfig->trajCycleTime > 0) {
            servo_cycle_time_sec = emcmotConfig->trajCycleTime;
        }

        // Use max jerk from INI (already parsed above)
        double default_max_jerk = jerk;
        if (default_max_jerk < 1.0) {
            default_max_jerk = 1e9;
        }

        // Apply predictive handoff configuration
        setHandoffConfig(handoff_horizon_ms,
                         branch_window_ms,
                         min_buffer_time_ms,
                         target_buffer_time_ms,
                         max_buffer_time_ms,
                         feed_override_debounce_ms,
                         servo_cycle_time_sec,
                         default_max_jerk);

        rcs_print("Predictive Handoff Configuration:\n");
        rcs_print("  HANDOFF_HORIZON_MS = %.1f\n", handoff_horizon_ms);
        rcs_print("  BRANCH_WINDOW_MS = %.1f\n", branch_window_ms);
        rcs_print("  MIN_BUFFER_TIME_MS = %.1f\n", min_buffer_time_ms);
        rcs_print("  TARGET_BUFFER_TIME_MS = %.1f\n", target_buffer_time_ms);
        rcs_print("  MAX_BUFFER_TIME_MS = %.1f\n", max_buffer_time_ms);
        rcs_print("  FEED_OVERRIDE_DEBOUNCE_MS = %.1f\n", feed_override_debounce_ms);
        rcs_print("  servo_cycle_time = %.3f ms\n", servo_cycle_time_sec * 1000.0);
        rcs_print("  default_max_jerk = %.0f\n", default_max_jerk);

        // Initialize predictive handoff system
        initPredictiveHandoff();
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

    // Dual-layer architecture: 1 = send NML after userspace planning (backward compat)
    // 0 = userspace only mode (new dual-layer architecture)
    int emulate_legacy = ini.findIntV("EMULATE_LEGACY_MOVE_COMMANDS", "TRAJ", 1);
    if (0 != emcSetEmulateLegacyMoveCommands(emulate_legacy)) {
        print_dbg_config("emcSetEmulateLegacyMoveCommands");
        return -1;
    }
    if (!emulate_legacy) {
        rcs_print("Dual-layer architecture enabled: NML motion commands disabled\n");
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
