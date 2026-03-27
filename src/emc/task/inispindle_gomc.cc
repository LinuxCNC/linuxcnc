/********************************************************************
* Description: inispindle_gomc.cc
*   INI file initialization routines for spindle NML — gomc variant.
*   Uses gomc_ini_t instead of EmcIniFile.
*
*   Derived from a work by Andy Pugh
*
* License: GPL Version 2+
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "emc.hh"
#include "rcs_print.hh"
#include "inispindle_gomc.hh"
#include "emcglb.h"
#include "emccfg.h"

static int loadSpindle(int spindle, const gomc_ini_t *ini)
{
    int num_spindles = gomc_ini_get_int(ini, "TRAJ", "SPINDLES", 1);
    if (spindle > num_spindles) return -1;

    char section[16];
    snprintf(section, sizeof(section), "SPINDLE_%d", spindle);

    double fastest_pos = 1e99;
    double slowest_neg = 0;
    double slowest_pos = 0;
    double fastest_neg = -1e99;
    double limit;

    // max positive speed
    const char *s;
    s = ini->get(ini->ctx, section, "MAX_FORWARD_VELOCITY");
    if (s) { limit = atof(s); fastest_pos = limit; fastest_neg = -limit; }

    // min positive speed
    s = ini->get(ini->ctx, section, "MIN_FORWARD_VELOCITY");
    if (s) { limit = atof(s); slowest_pos = limit; slowest_neg = -limit; }

    // min negative speed (absolute)
    s = ini->get(ini->ctx, section, "MIN_REVERSE_VELOCITY");
    if (s) { limit = atof(s); slowest_neg = -fabs(limit); }

    // max negative speed (absolute)
    s = ini->get(ini->ctx, section, "MAX_REVERSE_VELOCITY");
    if (s) { limit = atof(s); fastest_neg = -fabs(limit); }

    // homing
    int home_sequence = gomc_ini_get_int(ini, section, "HOME_SEQUENCE", 0);
    double search_vel = gomc_ini_get_double(ini, section, "HOME_SEARCH_VELOCITY", 0);
    double home_angle = 0;  // intentionally hardcoded to 0 (see original)
    double increment  = gomc_ini_get_double(ini, section, "INCREMENT", 100);

    if (0 != emcSpindleSetParams(spindle, fastest_pos, slowest_pos,
                                 slowest_neg, fastest_neg,
                                 search_vel, home_angle, home_sequence,
                                 increment)) {
        return -1;
    }
    return 0;
}

int iniSpindle(int spindle, const gomc_ini_t *ini)
{
    if (0 != loadSpindle(spindle, ini)) {
        return -1;
    }
    return 0;
}

// vim: sts=4 sw=4 et
