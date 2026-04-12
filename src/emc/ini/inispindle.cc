/********************************************************************
* Description: inispindle.cc
*   INI file initialization routines for spindle NML
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Andy Pugh
* Author: B.Stultiens
* License: GPL Version 2+
* System: Linux
*
* Copyright (c) 2021,2026 All rights reserved.
*
* Last change: created 30/12/21
********************************************************************/

#include <fmt/format.h>

#include "nml_intf/emc.hh"
#include "libnml/rcs/rcs_print.hh"
#include "nml_intf/emcglb.h"
#include "nml_intf/emccfg.h"
#include "inifile.hh"

#include "inihal.hh"
#include "inispindle.hh"

using namespace linuxcnc;

extern value_inihal_data old_inihal_data;

//
// loadSpindle(int spindle)
//
// Loads INI file params for the specified spindle spindle max and min
// velocities.
// The spindle argument is checked against the [TRAJ]SPINDLES setting and must
// be within bounds (otherwise it defaults to 1).
// If the section [SPINDLE_n], with 'n' the spindle being processed, then it
// will be configured using defaults as set in here.
//
// [TRAJ]SPINDLES <int>                    Number of spindles in system (defaults to 1)
// [SPINDLE_n]MAX_FORWARD_VELOCITY <real>
// [SPINDLE_n]MIN_FORWARD_VELOCITY <real>
// [SPINDLE_n]MIN_REVERSE_VELOCITY <real>
// [SPINDLE_n]MAX_REVERSE_VELOCITY <real>
// [SPINDLE_n]HOME_SEQUENCE <int>
// [SPINDLE_n]HOME_SEARCH_VELOCITY <real>
// -- disabled -- [SPINDLE_n]HOME <real>   set home angle - I believe this is a bad idea - andypugh 30/12/21
// [SPINDLE_n]INCREMENT <real>
//
static int loadSpindle(int spindle, const IniFile &ini)
{
    std::string spindleSection = fmt::format("SPINDLE_{}", spindle);

    int num_spindles = ini.findIntV("SPINDLES", "TRAJ", 1, 1, EMCMOT_MAX_SPINDLES-1);
    if (spindle >= num_spindles) { // Cannot configure spindles not present
        rcs_print_error("loadSpindle: spindle %d >= ini [SPINDLES]TRAJ %d\n", spindle, num_spindles);
	return -1;
    }

    double fastest_pos = 1e99;
    double slowest_neg = 0;
    double slowest_pos = 0;
    double fastest_neg = -1e99;

    // set max positive speed limit
    if (auto val = ini.findReal("MAX_FORWARD_VELOCITY", spindleSection)) {
        fastest_pos = *val;
        fastest_neg = -(*val);
    }
    // set min positive speed limit
    if (auto val = ini.findReal("MIN_FORWARD_VELOCITY", spindleSection)) {
        slowest_pos = *val;
        slowest_neg = -(*val);
    }

    // set min negative speed limit
    if (auto val = ini.findReal("MIN_REVERSE_VELOCITY", spindleSection)) {
        slowest_neg = -fabs(*val);
    }
    // set max negative speed limit
    if (auto val = ini.findReal("MAX_REVERSE_VELOCITY", spindleSection)) {
        fastest_neg = -fabs(*val);
    }

    int home_sequence = ini.findIntV("HOME_SEQUENCE", spindleSection, 0);
    double search_vel = ini.findRealV("HOME_SEARCH_VELOCITY", spindleSection, 0.0);
    // set home angle - I believe this is a bad idea - andypugh 30/12/21
    //double home_angle = ini.findRealV("HOME", spindleSection, 0.0);
    double home_angle = 0.0;
    double increment = ini.findRealV("INCREMENT", spindleSection, 100.0);

    if (0 != emcSpindleSetParams(spindle, fastest_pos, slowest_pos,
                                 slowest_neg, fastest_neg, search_vel,
                                 home_angle, home_sequence, increment)) {
        rcs_print_error("emcSpindleSetParams: failed\n");
        return -1;
    }
    return 0;
}

int iniSpindle(int spindle, const char *filename)
{
    if (spindle < 0 || spindle >= EMCMOT_MAX_SPINDLES) {
        rcs_print_error("iniJoint: Invalid spindle '%d'\n", spindle);
        return -1;
    }

    IniFile ini(filename);
    if (!ini)
        return -1;

    return loadSpindle(spindle, ini);
}
