/********************************************************************
* Description: initraj_gomc.cc
*   INI file initialization for trajectory level — gomc variant.
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
#include <math.h>

#include "emc.hh"
#include "emcpos.h"
#include "rcs_print.hh"
#include "posemath.h"
#include "initraj_gomc.hh"
#include "emcglb.h"

#include "inihal_gomc.hh"

extern value_inihal_data old_inihal_data;

// --- unit-string lookup tables (replaces EmcIniFile::FindLinearUnits etc.) ---

static double parseLinearUnits(const char *s)
{
    if (!s) return 0;
    if (!strcasecmp(s, "mm")       || !strcasecmp(s, "metric"))   return 1.0;
    if (!strcasecmp(s, "in")       || !strcasecmp(s, "inch")
     || !strcasecmp(s, "imperial"))                                return 1.0/25.4;
    // try numeric
    char *end;
    double v = strtod(s, &end);
    if (end != s) return v;
    return 0;
}

static double parseAngularUnits(const char *s)
{
    if (!s) return 0;
    if (!strcasecmp(s, "deg")    || !strcasecmp(s, "degree"))  return 1.0;
    if (!strcasecmp(s, "grad")   || !strcasecmp(s, "gon"))     return 0.9;
    if (!strcasecmp(s, "rad")    || !strcasecmp(s, "radian"))  return M_PI / 180;
    char *end;
    double v = strtod(s, &end);
    if (end != s) return v;
    return 0;
}

/*
  loadKins()
*/
static int loadKins(const gomc_ini_t *ini)
{
    int joints = gomc_ini_get_int(ini, "KINS", "JOINTS", 0);
    if (joints == 0) {
        rcs_print("initraj_gomc: [KINS]JOINTS not found or zero\n");
        return -1;
    }
    if (0 != emcTrajSetJoints(joints)) {
        return -1;
    }
    return 0;
}

/*
  loadTraj()
*/
static int loadTraj(const gomc_ini_t *ini)
{
    // spindles
    int spindles = gomc_ini_get_int(ini, "TRAJ", "SPINDLES", 1);
    if (0 != emcTrajSetSpindles(spindles)) {
        return -1;
    }

    // coordinates / axismask
    int axismask = 0;
    const char *coord = ini->get(ini->ctx, "TRAJ", "COORDINATES");
    if (coord) {
        if (strchr(coord, 'x') || strchr(coord, 'X')) axismask |= 1;
        if (strchr(coord, 'y') || strchr(coord, 'Y')) axismask |= 2;
        if (strchr(coord, 'z') || strchr(coord, 'Z')) axismask |= 4;
        if (strchr(coord, 'a') || strchr(coord, 'A')) axismask |= 8;
        if (strchr(coord, 'b') || strchr(coord, 'B')) axismask |= 16;
        if (strchr(coord, 'c') || strchr(coord, 'C')) axismask |= 32;
        if (strchr(coord, 'u') || strchr(coord, 'U')) axismask |= 64;
        if (strchr(coord, 'v') || strchr(coord, 'V')) axismask |= 128;
        if (strchr(coord, 'w') || strchr(coord, 'W')) axismask |= 256;
    } else {
        axismask = 1 | 2 | 4;  // default: XYZ
    }
    if (0 != emcTrajSetAxes(axismask)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcTrajSetAxes\n");
        }
        return -1;
    }

    // units
    const char *lu_str = ini->get(ini->ctx, "TRAJ", "LINEAR_UNITS");
    double linearUnits = parseLinearUnits(lu_str);
    const char *au_str = ini->get(ini->ctx, "TRAJ", "ANGULAR_UNITS");
    double angularUnits = parseAngularUnits(au_str);
    if (0 != emcTrajSetUnits(linearUnits, angularUnits)) {
        rcs_print("emcTrajSetUnits failed to set "
                  "[TRAJ]LINEAR_UNITS or [TRAJ]ANGULAR_UNITS\n");
        return -1;
    }

    // velocities / accelerations
    double vel = gomc_ini_get_double(ini, "TRAJ", "DEFAULT_LINEAR_VELOCITY", 1.0);
    old_inihal_data.traj_default_velocity = vel;
    if (0 != emcTrajSetVelocity(0, vel)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcTrajSetVelocity\n");
        }
        return -1;
    }

    vel = gomc_ini_get_double(ini, "TRAJ", "MAX_LINEAR_VELOCITY", 1e99);
    old_inihal_data.traj_max_velocity = vel;
    if (0 != emcTrajSetMaxVelocity(vel)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcTrajSetMaxVelocity\n");
        }
        return -1;
    }

    double acc = gomc_ini_get_double(ini, "TRAJ", "DEFAULT_LINEAR_ACCELERATION", 1e99);
    if (0 != emcTrajSetAcceleration(acc)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcTrajSetAcceleration\n");
        }
        return -1;
    }
    old_inihal_data.traj_default_acceleration = acc;

    acc = gomc_ini_get_double(ini, "TRAJ", "MAX_LINEAR_ACCELERATION", 1e99);
    if (0 != emcTrajSetMaxAcceleration(acc)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcTrajSetMaxAcceleration\n");
        }
        return -1;
    }
    old_inihal_data.traj_max_acceleration = acc;

    // arc blend
    int arcBlendEnable              = gomc_ini_get_int(ini, "TRAJ", "ARC_BLEND_ENABLE", 1);
    int arcBlendFallbackEnable      = gomc_ini_get_int(ini, "TRAJ", "ARC_BLEND_FALLBACK_ENABLE", 0);
    int arcBlendOptDepth            = gomc_ini_get_int(ini, "TRAJ", "ARC_BLEND_OPTIMIZATION_DEPTH", 50);
    int arcBlendGapCycles           = gomc_ini_get_int(ini, "TRAJ", "ARC_BLEND_GAP_CYCLES", 4);
    double arcBlendRampFreq         = gomc_ini_get_double(ini, "TRAJ", "ARC_BLEND_RAMP_FREQ", 100.0);
    double arcBlendTangentKinkRatio = gomc_ini_get_double(ini, "TRAJ", "ARC_BLEND_KINK_RATIO", 0.1);

    if (0 != emcSetupArcBlends(arcBlendEnable, arcBlendFallbackEnable,
                arcBlendOptDepth, arcBlendGapCycles, arcBlendRampFreq,
                arcBlendTangentKinkRatio)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcSetupArcBlends\n");
        }
        return -1;
    }

    old_inihal_data.traj_arc_blend_enable              = arcBlendEnable;
    old_inihal_data.traj_arc_blend_fallback_enable      = arcBlendFallbackEnable;
    old_inihal_data.traj_arc_blend_optimization_depth    = arcBlendOptDepth;
    old_inihal_data.traj_arc_blend_gap_cycles            = arcBlendGapCycles;
    old_inihal_data.traj_arc_blend_ramp_freq             = arcBlendRampFreq;
    old_inihal_data.traj_arc_blend_tangent_kink_ratio    = arcBlendTangentKinkRatio;

    // max feed override
    double maxFeedScale = gomc_ini_get_double(ini, "DISPLAY", "MAX_FEED_OVERRIDE", 1.0);
    if (0 != emcSetMaxFeedOverride(maxFeedScale)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcSetMaxFeedOverride\n");
        }
        return -1;
    }

    // probe error inhibit
    int j_inhibit = gomc_ini_get_int(ini, "TRAJ", "NO_PROBE_JOG_ERROR", 0);
    int h_inhibit = gomc_ini_get_int(ini, "TRAJ", "NO_PROBE_HOME_ERROR", 0);
    if (0 != emcSetProbeErrorInhibit(j_inhibit, h_inhibit)) {
        if (emc_debug & EMC_DEBUG_CONFIG) {
            rcs_print("bad return value from emcSetProbeErrorInhibit\n");
        }
        return -1;
    }

    // [TRAJ]HOME  (for non-identity kins forward solve seed)
    {
        unsigned char coordinateMark[6] = { 1, 1, 1, 0, 0, 0 };
        EmcPose homePose = { {0.0, 0.0, 0.0}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

        const char *inistring = ini->get(ini->ctx, "TRAJ", "HOME");
        if (inistring) {
            char homes[LINELEN];
            char home[LINELEN];
            double d;
            int len;

            snprintf(homes, sizeof(homes), "%s", inistring);
            len = 0;
            for (int t = 0; t < 6; t++) {
                if (!coordinateMark[t]) {
                    continue;
                }
                if (1 == sscanf(&homes[len], "%s", home) &&
                    1 == sscanf(home, "%lf", &d)) {
                    if (t == 0)      homePose.tran.x = d;
                    else if (t == 1) homePose.tran.y = d;
                    else if (t == 2) homePose.tran.z = d;
                    else if (t == 3) homePose.a = d;
                    else if (t == 4) homePose.b = d;
                    else if (t == 5) homePose.c = d;
                    else if (t == 6) homePose.u = d;
                    else if (t == 7) homePose.v = d;
                    else             homePose.w = d;

                    len += strlen(home);
                    while ((len < LINELEN) && (homes[len] == ' ' || homes[len] == '\t')) {
                        len++;
                    }
                    if (len >= LINELEN) {
                        break;
                    }
                } else {
                    rcs_print("invalid INI file value for [TRAJ] HOME: %s\n",
                              inistring);
                    return -1;
                }
            }
        }
        if (0 != emcTrajSetHome(homePose)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetHome\n");
            }
            return -1;
        }
    }

    return 0;
}

/*
  iniTraj(const gomc_ini_t *ini)
*/
int iniTraj(const gomc_ini_t *ini)
{
    if (0 != loadKins(ini)) {
        return -1;
    }
    if (0 != loadTraj(ini)) {
        return -1;
    }
    return 0;
}

// vim: sts=4 sw=4 et
