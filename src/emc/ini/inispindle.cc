/********************************************************************
* Description: inispindle.cc
*   INI file initialization routines for spindle NML
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Andy Pugh
* License: GPL Version 2+
* System: Linux
*
* Copyright (c) 2021 All rights reserved.
*
* Last change: created 30/12/21
********************************************************************/

#include <unistd.h>
#include <stdio.h>		// NULL
#include <stdlib.h>		// atol(), _itoa()
#include <string.h>		// strcmp()
#include <ctype.h>		// isdigit()
#include <sys/types.h>
#include <sys/stat.h>

#include "emc.hh"
#include "rcs_print.hh"
#include "emcIniFile.hh"
#include "inispindle.hh"		// these decls
#include "emcglb.h"		// EMC_DEBUG
#include "emccfg.h"		// default values for globals

#include "inihal.hh"

extern value_inihal_data old_inihal_data;

/*
  loadSpindls(int spindle)

  Loads ini file params for the specified spindle
  spindle max and min velocities
  */

static int loadSpindle(int spindle, EmcIniFile *spindleIniFile)
{
    int num_spindles = 1;
    char spindleString[11];
    double max_pos = 1e99;
    double max_neg = 0;
    double min_pos = 0;
    double min_neg = -1e99;
    int home_sequence = 0;
    double search_vel = 0;
    double home_angle = 0;
    double increment = 100;
    double limit;

    spindleIniFile->EnableExceptions(EmcIniFile::ERR_CONVERSION);

    if (spindleIniFile->Find(&num_spindles, "SPINDLES", "TRAJ") < 0){
        num_spindles = 1; }
    if (spindle > num_spindles) return -1;

    snprintf(spindleString, sizeof(spindleString), "SPINDLE_%i", spindle);

    // set max positive speed limit
    if (spindleIniFile->Find(&limit, "MAX_VELOCITY", spindleString) == 0){
        max_pos = limit;
        min_neg = limit;
    }
    // set min positive speed limit
    if (spindleIniFile->Find(&limit, "MIN_VELOCITY", spindleString) == 0){
        min_pos = limit;
        max_neg = limit;
    }
    // set min negative speed limit
    if (spindleIniFile->Find(&limit, "MIN_REVERSE_VELOCITY", spindleString) == 0){
        max_neg = -1.0 * fabs(limit);
    }
    // set max negative speed limit
    if (spindleIniFile->Find(&limit, "MAX_REVERSE_VELOCITY", spindleString) == 0){
        min_neg = -1.0 * fabs(limit);
    }
    // set home sequence
    if (spindleIniFile->Find(&limit, "HOME_SEQUENCE", spindleString) == 0){
        home_sequence = (int)limit;
    }
    // set home velocity
    if (spindleIniFile->Find(&limit, "HOME_SEARCH_VELOCITY", spindleString) == 0){
        search_vel = (int)limit;
    }
    /* set home angle - I believe this is a bad idea - andypugh 30/12/21
    if (spindleIniFile->Find(&limit, "HOME", spindleString) >= 0){
        home_angle = (int)limit;
    }*/
    home_angle = 0;
    // set spindle increment
    if (spindleIniFile->Find(&limit, "INCREMENT", spindleString) == 0){
        increment = limit;
    }

    if (0 != emcSpindleSetParams(spindle, max_pos, min_pos, max_neg,
        min_neg, search_vel, home_angle, home_sequence, increment)) {
        return -1;
    }
    return 0;
}

/*
  iniAxis(int axis, const char *filename)

  Loads ini file parameters for specified axis, [0 .. AXES - 1]

 */
int iniSpindle(int spindle, const char *filename)
{
    EmcIniFile spindleIniFile(EmcIniFile::ERR_TAG_NOT_FOUND |
                           EmcIniFile::ERR_SECTION_NOT_FOUND |
                           EmcIniFile::ERR_CONVERSION);

    if (spindleIniFile.Open(filename) == false) {
	return -1;
    }

    // load its values
    if (0 != loadSpindle(spindle, &spindleIniFile)) {
        return -1;
    }
    return 0;
}
