/********************************************************************
* Description: iniaxis.cc
*   INI file initialization routines for axis NML
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
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
#include "iniaxis.hh"		// these decls
#include "emcglb.h"		// EMC_DEBUG
#include "emccfg.h"		// default values for globals

#include "inihal.hh"

extern value_inihal_data old_inihal_data;

/*
  loadAxis(int axis)

  Loads ini file params for axis, axis = X, Y, Z, A, B, C, U, V, W 

  TYPE <LINEAR ANGULAR>        type of axis (hardcoded: X,Y,Z,U,V,W: LINEAR, A,B,C: ANGULAR)
  MAX_VELOCITY <float>         max vel for axis
  MAX_ACCELERATION <float>     max accel for axis
  MIN_LIMIT <float>            minimum soft position limit
  MAX_LIMIT <float>            maximum soft position limit

  calls:

  emcAxisSetMinPositionLimit(int axis, double limit);
  emcAxisSetMaxPositionLimit(int axis, double limit);
  emcAxisSetMaxVelocity(int axis, double vel);
  emcAxisSetMaxAcceleration(int axis, double acc);
  */

static int loadAxis(int axis, EmcIniFile *axisIniFile)
{
    char axisString[16];
    double limit;
    double maxVelocity;
    double maxAcceleration;
    int    lockingjnum = -1; // -1 ==> locking joint not used

    // compose string to match, axis = 0 -> AXIS_X etc.
    switch (axis) {
	case 0: sprintf(axisString, "AXIS_X"); break;
	case 1: sprintf(axisString, "AXIS_Y"); break;
	case 2: sprintf(axisString, "AXIS_Z"); break;
	case 3: sprintf(axisString, "AXIS_A"); break;
	case 4: sprintf(axisString, "AXIS_B"); break;
	case 5: sprintf(axisString, "AXIS_C"); break;
	case 6: sprintf(axisString, "AXIS_U"); break;
	case 7: sprintf(axisString, "AXIS_V"); break;
	case 8: sprintf(axisString, "AXIS_W"); break;
    }

    axisIniFile->EnableExceptions(EmcIniFile::ERR_CONVERSION);
    
    try {
        // set min position limit
        limit = -1e99;	                // default
        axisIniFile->Find(&limit, "MIN_LIMIT", axisString);
        if (0 != emcAxisSetMinPositionLimit(axis, limit)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMinPositionLimit\n");
            }
            return -1;
        }
        old_inihal_data.axis_min_limit[axis] = limit;

        // set max position limit
        limit = 1e99;	                // default
        axisIniFile->Find(&limit, "MAX_LIMIT", axisString);
        if (0 != emcAxisSetMaxPositionLimit(axis, limit)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMaxPositionLimit\n");
            }
            return -1;
        }
        old_inihal_data.axis_max_limit[axis] = limit;

        // set maximum velocity
        maxVelocity = DEFAULT_AXIS_MAX_VELOCITY;
        axisIniFile->Find(&maxVelocity, "MAX_VELOCITY", axisString);
        if (0 != emcAxisSetMaxVelocity(axis, maxVelocity)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMaxVelocity\n");
            }
            return -1;
        }
        old_inihal_data.axis_max_velocity[axis] = maxVelocity;

        maxAcceleration = DEFAULT_AXIS_MAX_ACCELERATION;
        axisIniFile->Find(&maxAcceleration, "MAX_ACCELERATION", axisString);
        if (0 != emcAxisSetMaxAcceleration(axis, maxAcceleration)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMaxAcceleration\n");
            }
            return -1;
        }
        old_inihal_data.axis_max_acceleration[axis] = maxAcceleration;

        axisIniFile->Find(&lockingjnum, "LOCKING_INDEXER_JOINT", axisString);
        if (0 != emcAxisSetLockingJoint(axis, lockingjnum)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetLockingJoint\n");
            }
            return -1;
        }
    }


    catch(EmcIniFile::Exception &e){
        e.Print();
        return -1;
    }

    return 0;
}

/*
  iniAxis(int axis, const char *filename)

  Loads ini file parameters for specified axis, [0 .. AXES - 1]

 */
int iniAxis(int axis, const char *filename)
{
    EmcIniFile axisIniFile(EmcIniFile::ERR_TAG_NOT_FOUND |
                           EmcIniFile::ERR_SECTION_NOT_FOUND |
                           EmcIniFile::ERR_CONVERSION);

    if (axisIniFile.Open(filename) == false) {
	return -1;
    }

    // load its values
    if (0 != loadAxis(axis, &axisIniFile)) {
        return -1;
    }
    return 0;
}
