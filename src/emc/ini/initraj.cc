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
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include <stdio.h>		// NULL
#include <stdlib.h>		// atol()
#include <string.h>		// strlen()
#include <ctype.h>		// isspace()

#include "emc.hh"
#include "emcpos.h"             // EmcPose
#include "rcs_print.hh"
#include "posemath.h"		// PM_POSE, PM_RPY
#include "emcIniFile.hh"
#include "initraj.hh"		// these decls
#include "emcglb.h"		/*! \todo TRAVERSE_RATE (FIXME) */
#include "inihal.hh"
#include <rtapi_string.h>
#include "motion.h"       // For emcmotConfig, emcmot_config_t

// Userspace kinematics for trajectory planning
#include "userspace_kinematics.hh"

// Predictive handoff configuration for feed override handling
#include "motion_planning_9d.hh"

extern value_inihal_data old_inihal_data;

/* Access to emcmotConfig from usrmotintf.cc (initialized by usrmotInit()) */
extern emcmot_config_t *emcmotConfig;

/*
  loadKins()

  JOINTS <int>                  number of joints (DOF) in system

  calls:

  emcTrajSetJoints(int joints);
*/

static int loadKins(EmcIniFile *trajInifile)
{
    trajInifile->EnableExceptions(EmcIniFile::ERR_CONVERSION);

    try {
	int joints = 0;
	trajInifile->Find(&joints, "JOINTS", "KINS");

        if (0 != emcTrajSetJoints(joints)) {
            return -1;
        }
    }
   
    catch (EmcIniFile::Exception &e) {
        e.Print();
        return -1;
    }
    
    return 0;
}


/*
  loadTraj()

  Loads INI file params for traj

  COORDINATES <char[]>            axes in system
  LINEAR_UNITS <float>            units per mm
  ANGULAR_UNITS <float>           units per degree
  DEFAULT_LINEAR_VELOCITY <float> default linear velocity
  MAX_LINEAR_VELOCITY <float>     max linear velocity
  DEFAULT_LINEAR_ACCELERATION <float> default linear acceleration
  MAX_LINEAR_ACCELERATION <float>     max linear acceleration

  calls:

  emcTrajSetAxes(int axes, int axismask);
  emcTrajSetUnits(double linearUnits, double angularUnits);
  emcTrajSetVelocity(double vel, double ini_maxvel);
  emcTrajSetAcceleration(double acc);
  emcTrajSetMaxVelocity(double vel);
  emcTrajSetMaxAcceleration(double acc);
  */

static int loadTraj(EmcIniFile *trajInifile)
{
    EmcLinearUnits linearUnits;
    EmcAngularUnits angularUnits;
    double vel;
    double acc;
    double jerk;
    int planner_type;

    trajInifile->EnableExceptions(EmcIniFile::ERR_CONVERSION);

    try{
		int spindles = 1;
		trajInifile->Find(&spindles, "SPINDLES", "TRAJ");
		if (0 != emcTrajSetSpindles(spindles)) {
			return -1;
		}
    }

    catch (EmcIniFile::Exception &e) {
        e.Print();
        return -1;
    }

    try{
	int axismask = 0;
	auto coord = trajInifile->Find("COORDINATES", "TRAJ");
	if(coord) {
	    if(coord->find_first_of("xX") != std::string::npos) {
	         axismask |= 1;
            }
	    if(coord->find_first_of("yY") != std::string::npos) {
	         axismask |= 2;
            }
	    if(coord->find_first_of("zZ") != std::string::npos) {
	         axismask |= 4;
            }
	    if(coord->find_first_of("aA") != std::string::npos) {
	         axismask |= 8;
            }
	    if(coord->find_first_of("bB") != std::string::npos) {
	         axismask |= 16;
            }
	    if(coord->find_first_of("cC") != std::string::npos) {
	         axismask |= 32;
            }
	    if(coord->find_first_of("uU") != std::string::npos) {
	         axismask |= 64;
            }
	    if(coord->find_first_of("vV") != std::string::npos) {
	         axismask |= 128;
            }
	    if(coord->find_first_of("wW") != std::string::npos) {
	         axismask |= 256;
            }
	} else {
	    axismask = 1 | 2 | 4;		// default: XYZ machine
	}
        if (0 != emcTrajSetAxes(axismask)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetAxes\n");
            }
            return -1;
        }

        linearUnits = 0;
        trajInifile->FindLinearUnits(&linearUnits, "LINEAR_UNITS", "TRAJ");
        angularUnits = 0;
        trajInifile->FindAngularUnits(&angularUnits, "ANGULAR_UNITS", "TRAJ");
        if (0 != emcTrajSetUnits(linearUnits, angularUnits)) {
            rcs_print("emcTrajSetUnits failed to set "
                      "[TRAJ]LINEAR_UNITS or [TRAJ]ANGULAR_UNITS\n");
            return -1;
        }

        vel = 1.0;
        trajInifile->Find(&vel, "DEFAULT_LINEAR_VELOCITY", "TRAJ");
        old_inihal_data.traj_default_velocity = vel;

        // and set dynamic value
        if (0 != emcTrajSetVelocity(0, vel)) { //default velocity on startup 0
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetVelocity\n");
            }
            return -1;
        }

        vel = 1e99; // by default, use AXIS limit
        trajInifile->Find(&vel, "MAX_LINEAR_VELOCITY", "TRAJ");
        // XXX CJR merge question: Set something in TrajConfig here?
        old_inihal_data.traj_max_velocity = vel;

        // and set dynamic value
        if (0 != emcTrajSetMaxVelocity(vel)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetMaxVelocity\n");
            }
            return -1;
        }

        acc = 1e99; // let the axis values apply
        trajInifile->Find(&acc, "DEFAULT_LINEAR_ACCELERATION", "TRAJ");
        if (0 != emcTrajSetAcceleration(acc)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetAcceleration\n");
            }
            return -1;
        }
        old_inihal_data.traj_default_acceleration = acc;

        acc = 1e99; // let the axis values apply
        trajInifile->Find(&acc, "MAX_LINEAR_ACCELERATION", "TRAJ");
        if (0 != emcTrajSetMaxAcceleration(acc)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetMaxAcceleration\n");
            }
            return -1;
        }
        old_inihal_data.traj_max_acceleration = acc;

        // Set max jerk (default to 1e9 if not specified in INI)
        jerk = 1e9;
        trajInifile->Find(&jerk, "MAX_LINEAR_JERK", "TRAJ");
        if (0 != emcTrajSetMaxJerk(jerk)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetMaxJerk\n");
            }
            return -1;
        }
        old_inihal_data.traj_max_jerk = jerk;
        // Also set current jerk to max_jerk
        if (0 != emcTrajSetJerk(jerk)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetJerk\n");
            }
            return -1;
        }
        planner_type = 0;  // Default: 0 = trapezoidal, 1 = S-curve, 2 = 9D (EXPERIMENTAL)
        trajInifile->Find(&planner_type, "PLANNER_TYPE", "TRAJ");
        // Support types 0, 1, and 2 (9D experimental)
        // Force planner type 0 if max_jerk < 1 (S-curve needs valid jerk)
        if (planner_type < 0 || planner_type > 2) {
            rcs_print("Invalid PLANNER_TYPE %d, must be 0, 1, or 2. Defaulting to 0.\n", planner_type);
            planner_type = 0;
        }
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
                auto coord = trajInifile->Find("COORDINATES", "TRAJ");
                int joints = 0;
                trajInifile->Find(&joints, "JOINTS", "KINS");
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
            int optimization_depth = 8;
            double ramp_frequency = 10.0;
            int smoothing_passes = 2;
            int tc_queue_size = 50;

            trajInifile->Find(&optimization_depth, "OPTIMIZATION_DEPTH", "TRAJ");
            trajInifile->Find(&ramp_frequency, "RAMP_FREQUENCY", "TRAJ");
            trajInifile->Find(&smoothing_passes, "SMOOTHING_PASSES", "TRAJ");
            trajInifile->Find(&tc_queue_size, "TC_QUEUE_SIZE", "TRAJ");

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

            // Predictive handoff configuration for feed override handling
            // All timing values in milliseconds
            double handoff_horizon_ms = 100.0;
            double branch_window_ms = 50.0;
            double min_buffer_time_ms = 100.0;
            double target_buffer_time_ms = 200.0;
            double max_buffer_time_ms = 500.0;
            double feed_override_debounce_ms = 50.0;

            trajInifile->Find(&handoff_horizon_ms, "HANDOFF_HORIZON_MS", "TRAJ");
            trajInifile->Find(&branch_window_ms, "BRANCH_WINDOW_MS", "TRAJ");
            trajInifile->Find(&min_buffer_time_ms, "MIN_BUFFER_TIME_MS", "TRAJ");
            trajInifile->Find(&target_buffer_time_ms, "TARGET_BUFFER_TIME_MS", "TRAJ");
            trajInifile->Find(&max_buffer_time_ms, "MAX_BUFFER_TIME_MS", "TRAJ");
            trajInifile->Find(&feed_override_debounce_ms, "FEED_OVERRIDE_DEBOUNCE_MS", "TRAJ");

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
            // Default to 1ms if not available
            double servo_cycle_time_sec = 0.001;
            if (emcmotConfig && emcmotConfig->trajCycleTime > 0) {
                servo_cycle_time_sec = emcmotConfig->trajCycleTime;
            }

            // Use max jerk from INI (already parsed above)
            double default_max_jerk = jerk;
            if (default_max_jerk < 1.0) {
                default_max_jerk = 1e9;  // Match initraj.cc default
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
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajPlannerType\n");
            }
            return -1;
        }
        old_inihal_data.traj_planner_type = planner_type;

        int arcBlendEnable = 1;
        int arcBlendFallbackEnable = 0;
        int arcBlendOptDepth = 50;
        int arcBlendGapCycles = 4;
        double arcBlendRampFreq = 100.0;
        double arcBlendTangentKinkRatio = 0.1;

        trajInifile->Find(&arcBlendEnable, "ARC_BLEND_ENABLE", "TRAJ");
        trajInifile->Find(&arcBlendFallbackEnable, "ARC_BLEND_FALLBACK_ENABLE", "TRAJ");
        trajInifile->Find(&arcBlendOptDepth, "ARC_BLEND_OPTIMIZATION_DEPTH", "TRAJ");
        trajInifile->Find(&arcBlendGapCycles, "ARC_BLEND_GAP_CYCLES", "TRAJ");
        trajInifile->Find(&arcBlendRampFreq, "ARC_BLEND_RAMP_FREQ", "TRAJ");
        trajInifile->Find(&arcBlendTangentKinkRatio, "ARC_BLEND_KINK_RATIO", "TRAJ");

        if (0 != emcSetupArcBlends(arcBlendEnable, arcBlendFallbackEnable,
                    arcBlendOptDepth, arcBlendGapCycles, arcBlendRampFreq, arcBlendTangentKinkRatio)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcSetupArcBlends\n");
            }
            return -1;
        } 

        old_inihal_data.traj_arc_blend_enable = arcBlendEnable;
        old_inihal_data.traj_arc_blend_fallback_enable = arcBlendFallbackEnable;
        old_inihal_data.traj_arc_blend_optimization_depth = arcBlendOptDepth;
        old_inihal_data.traj_arc_blend_gap_cycles = arcBlendGapCycles;
        old_inihal_data.traj_arc_blend_ramp_freq = arcBlendRampFreq;
        old_inihal_data.traj_arc_blend_tangent_kink_ratio = arcBlendTangentKinkRatio;
        //TODO update inihal

        double maxFeedScale = 1.0;
        trajInifile->Find(&maxFeedScale, "MAX_FEED_OVERRIDE", "DISPLAY");

        if (0 != emcSetMaxFeedOverride(maxFeedScale)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcSetMaxFeedOverride\n");
            }
            return -1;
        } 
        
        int j_inhibit = 0;
        int h_inhibit = 0;
        trajInifile->Find(&j_inhibit, "NO_PROBE_JOG_ERROR", "TRAJ");
        trajInifile->Find(&h_inhibit, "NO_PROBE_HOME_ERROR", "TRAJ");
        if (0 != emcSetProbeErrorInhibit(j_inhibit, h_inhibit)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcSetProbeErrorInhibit\n");
            }
            return -1;
        }

        // Dual-layer architecture: 1 = send NML after userspace planning (backward compat)
        // 0 = userspace only mode (new dual-layer architecture)
        int emulate_legacy = 1;  // Default: backward compatible
        trajInifile->Find(&emulate_legacy, "EMULATE_LEGACY_MOVE_COMMANDS", "TRAJ");
        if (0 != emcSetEmulateLegacyMoveCommands(emulate_legacy)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcSetEmulateLegacyMoveCommands\n");
            }
            return -1;
        }
        if (!emulate_legacy) {
            rcs_print("Dual-layer architecture enabled: NML motion commands disabled\n");
        }
    }

    catch (EmcIniFile::Exception &e) {
        e.Print();
        return -1;
    }
    try{
        unsigned char coordinateMark[6] = { 1, 1, 1, 0, 0, 0 };
        int t;
        int len;
        char homes[LINELEN];
        char home[LINELEN];
        EmcPose homePose = { {0.0, 0.0, 0.0}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        double d;
        auto inistring = trajInifile->Find("HOME", "TRAJ");
        if (inistring) {
            // [TRAJ]HOME is important for genhexkins.c kinetmaticsForward()
            // and probably other non-identity kins that solve the forward
            // kinematics with an iterative algorithm when the homePose
            // is not all zeros

            // found it, now interpret it according to coordinateMark[]
            rtapi_strxcpy(homes, inistring->c_str());
            len = 0;
            for (t = 0; t < 6; t++) {
                if (!coordinateMark[t]) {
                    continue;    // position t at index of next non-zero mark
                }
                // there is a mark, so read the string for a value
                if (1 == sscanf(&homes[len], "%254s", home) &&
                    1 == sscanf(home, "%lf", &d)) {
                    // got an entry, index into coordinateMark[] is 't'
                    if (t == 0)
                        homePose.tran.x = d;
                    else if (t == 1)
                        homePose.tran.y = d;
                    else if (t == 2)
                        homePose.tran.z = d;
                    else if (t == 3)
                        homePose.a = d;
                    else if (t == 4)
                        homePose.b = d;
                    else if (t == 5)
                        homePose.c = d;
/*
 * The following have no effect. The loop only counts [0..5].
                    else if (t == 6)
                        homePose.u = d;
                    else if (t == 7)
                        homePose.v = d;
                    else
                        homePose.w = d;
*/

                    // position string ptr past this value
                    len += strlen(home);
                    // and at start of next value
                    while ((len < LINELEN) && (homes[len] == ' ' || homes[len] == '\t')) {
                        len++;
                    }
                    if (len >= LINELEN) {
                        break;    // out of for loop
                    }
                } else {
                    // badly formatted entry
                    rcs_print("invalid INI file value for [TRAJ] HOME: %s\n",
                          inistring->c_str());
                        return -1;
                }
            }  // end of for-loop on coordinateMark[]
        }
        if (0 != emcTrajSetHome(homePose)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetHome\n");
            }
            return -1;
        }
     } //try

    catch (EmcIniFile::Exception &e) {
        e.Print();
        return -1;
    }
    return 0;
}

/*
  iniTraj(const char *filename)

  Loads INI file parameters for trajectory, from [TRAJ] section
 */
int iniTraj(const char *filename)
{
    EmcIniFile trajInifile;

    if (trajInifile.Open(filename) == false) {
	return -1;
    }
    // load trajectory values
    if (0 != loadKins(&trajInifile)) {
	return -1;
    }
    // load trajectory values
    if (0 != loadTraj(&trajInifile)) {
	return -1;
    }

    return 0;
}
