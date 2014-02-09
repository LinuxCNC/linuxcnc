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

/*
  loadAxis(int axis)

  Loads ini file params for axis, axis = 0, ...

  TYPE <LINEAR ANGULAR>        type of axis
  UNITS <float>                units per mm or deg
  MAX_VELOCITY <float>         max vel for axis
  MAX_ACCELERATION <float>     max accel for axis
  BACKLASH <float>             backlash
  INPUT_SCALE <float> <float>  scale, offset
  OUTPUT_SCALE <float> <float> scale, offset
  MIN_LIMIT <float>            minimum soft position limit
  MAX_LIMIT <float>            maximum soft position limit
  FERROR <float>               maximum following error, scaled to max vel
  MIN_FERROR <float>           minimum following error
  HOME <float>                 home position (where to go after home)
  HOME_FINAL_VEL <float>       speed to move from HOME_OFFSET to HOME location (at the end of homing)
  HOME_OFFSET <float>          home switch/index pulse location
  HOME_SEARCH_VEL <float>      homing speed, search phase
  HOME_LATCH_VEL <float>       homing speed, latch phase
  HOME_USE_INDEX <bool>        use index pulse when homing?
  HOME_IGNORE_LIMITS <bool>    ignore limit switches when homing?
  COMP_FILE <filename>         file of axis compensation points

  calls:

  emcAxisSetAxis(int axis, unsigned char axisType);
  emcAxisSetUnits(int axis, double units);
  emcAxisSetBacklash(int axis, double backlash);
  emcAxisSetInterpolationRate(int axis, int rate);
  emcAxisSetInputScale(int axis, double scale, double offset);
  emcAxisSetOutputScale(int axis, double scale, double offset);
  emcAxisSetMinPositionLimit(int axis, double limit);
  emcAxisSetMaxPositionLimit(int axis, double limit);
  emcAxisSetFerror(int axis, double ferror);
  emcAxisSetMinFerror(int axis, double ferror);
  emcAxisSetHomingParams(int axis, double home, double offset,
    double search_vel, double latch_vel, int use_index, int ignore_limits );
  emcAxisActivate(int axis);
  emcAxisDeactivate(int axis);
  emcAxisSetMaxVelocity(int axis, double vel);
  emcAxisSetMaxAcceleration(int axis, double acc);
  emcAxisLoadComp(int axis, const char * file);
  emcAxisLoadComp(int axis, const char * file);
  */

extern value_inihal_data old_inihal_data;

static int loadAxis(int axis, EmcIniFile *axisIniFile)
{
    char axisString[16];
    const char *inistring;
    EmcAxisType axisType;
    double units;
    double backlash;
    double offset;
    double limit;
    double home;
    double search_vel;
    double latch_vel;
    double home_final_vel; // moving from OFFSET to HOME
    bool use_index;
    bool ignore_limits;
    bool is_shared;
    int sequence;
    int volatile_home;
    int locking_indexer;
    int comp_file_type; //type for the compensation file. type==0 means nom, forw, rev. 
    double maxVelocity;
    double maxAcceleration;
    double ferror;

    // compose string to match, axis = 0 -> AXIS_0, etc.
    sprintf(axisString, "AXIS_%d", axis);

    axisIniFile->EnableExceptions(EmcIniFile::ERR_CONVERSION);
    
    try {
        // set axis type
        axisType = EMC_AXIS_LINEAR;	// default
        axisIniFile->Find(&axisType, "TYPE", axisString);

        if (0 != emcAxisSetAxis(axis, axisType)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetAxis\n");
            }
            return -1;
        }

        // set units
        if(axisType == EMC_AXIS_LINEAR){
            units = emcTrajGetLinearUnits();
            axisIniFile->FindLinearUnits(&units, "UNITS", axisString);
        }else{
            units = emcTrajGetAngularUnits();
            axisIniFile->FindAngularUnits(&units, "UNITS", axisString);
        }

        if (0 != emcAxisSetUnits(axis, units)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetUnits\n");
            }
            return -1;
        }

        // set backlash
        backlash = 0;	                // default
        axisIniFile->Find(&backlash, "BACKLASH", axisString);

        if (0 != emcAxisSetBacklash(axis, backlash)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetBacklash\n");
            }
            return -1;
        }
        old_inihal_data.backlash[axis] = backlash;

        // set min position limit
        limit = -1e99;	                // default
        axisIniFile->Find(&limit, "MIN_LIMIT", axisString);

        if (0 != emcAxisSetMinPositionLimit(axis, limit)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMinPositionLimit\n");
            }
            return -1;
        }
        old_inihal_data.min_limit[axis] = limit;

        // set max position limit
        limit = 1e99;	                // default
        axisIniFile->Find(&limit, "MAX_LIMIT", axisString);

        if (0 != emcAxisSetMaxPositionLimit(axis, limit)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMaxPositionLimit\n");
            }
            return -1;
        }
        old_inihal_data.max_limit[axis] = limit;

        // set following error limit (at max speed)
        ferror = 1;	                // default
        axisIniFile->Find(&ferror, "FERROR", axisString);

        if (0 != emcAxisSetFerror(axis, ferror)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetFerror\n");
            }
            return -1;
        }
        old_inihal_data.ferror[axis] = ferror;

        // do MIN_FERROR, if it's there. If not, use value of maxFerror above
        axisIniFile->Find(&ferror, "MIN_FERROR", axisString);

        if (0 != emcAxisSetMinFerror(axis, ferror)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMinFerror\n");
            }
            return -1;
        }
        old_inihal_data.min_ferror[axis] = ferror;

        // set homing paramsters (total of 6)
        home = 0;	                // default
        axisIniFile->Find(&home, "HOME", axisString);
        offset = 0;	                // default
        axisIniFile->Find(&offset, "HOME_OFFSET", axisString);
        search_vel = 0;	                // default
        axisIniFile->Find(&search_vel, "HOME_SEARCH_VEL", axisString);
        latch_vel = 0;	                // default
        axisIniFile->Find(&latch_vel, "HOME_LATCH_VEL", axisString);
        home_final_vel = -1;	                // default (rapid)
        axisIniFile->Find(&home_final_vel, "HOME_FINAL_VEL", axisString);
        is_shared = false;	        // default
        axisIniFile->Find(&is_shared, "HOME_IS_SHARED", axisString);
        use_index = false;	        // default
        axisIniFile->Find(&use_index, "HOME_USE_INDEX", axisString);
        ignore_limits = false;	        // default
        axisIniFile->Find(&ignore_limits, "HOME_IGNORE_LIMITS", axisString);
        sequence = -1;	                // default
        axisIniFile->Find(&sequence, "HOME_SEQUENCE", axisString);
        volatile_home = 0;	        // default
        axisIniFile->Find(&volatile_home, "VOLATILE_HOME", axisString);
        locking_indexer = false;
        axisIniFile->Find(&locking_indexer, "LOCKING_INDEXER", axisString);

        // issue NML message to set all params
        if (0 != emcAxisSetHomingParams(axis, home, offset, home_final_vel, search_vel,
                                        latch_vel, (int)use_index, (int)ignore_limits,
                                        (int)is_shared, sequence, volatile_home, locking_indexer)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetHomingParams\n");
            }
            return -1;
        }

        // set maximum velocity
        maxVelocity = DEFAULT_AXIS_MAX_VELOCITY;
        axisIniFile->Find(&maxVelocity, "MAX_VELOCITY", axisString);

        if (0 != emcAxisSetMaxVelocity(axis, maxVelocity)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMaxVelocity\n");
            }
            return -1;
        }

        old_inihal_data.max_velocity[axis] = maxVelocity;

        maxAcceleration = DEFAULT_AXIS_MAX_ACCELERATION;
        axisIniFile->Find(&maxAcceleration, "MAX_ACCELERATION", axisString);

        if (0 != emcAxisSetMaxAcceleration(axis, maxAcceleration)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print_error("bad return from emcAxisSetMaxAcceleration\n");
            }
            return -1;
        }

        old_inihal_data.max_acceleration[axis] = maxAcceleration;

        comp_file_type = 0;             // default
        axisIniFile->Find(&comp_file_type, "COMP_FILE_TYPE", axisString);

        if (NULL != (inistring = axisIniFile->Find("COMP_FILE", axisString))) {
            if (0 != emcAxisLoadComp(axis, inistring, comp_file_type)) {
                if (emc_debug & EMC_DEBUG_CONFIG) {
                    rcs_print_error("bad return from emcAxisLoadComp\n");
                }
                return -1;
            }
        }
    }


    catch(EmcIniFile::Exception &e){
        e.Print();
        return -1;
    }

    // lastly, activate axis. Do this last so that the motion controller
    // won't flag errors midway during configuration
    emcAxisActivate(axis);

    return 0;
}


/*
  iniAxis(int axis, const char *filename)

  Loads ini file parameters for specified axis, [0 .. AXES - 1]

  Looks for AXES in TRAJ section for how many to do, up to
  EMC_AXIS_MAX.
 */
int iniAxis(int axis, const char *filename)
{
    int axes;
    EmcIniFile axisIniFile(EmcIniFile::ERR_TAG_NOT_FOUND |
                           EmcIniFile::ERR_SECTION_NOT_FOUND |
                           EmcIniFile::ERR_CONVERSION);

    if (axisIniFile.Open(filename) == false) {
	return -1;
    }

    try {
        axisIniFile.Find(&axes, "AXES", "TRAJ");
    }

    catch(EmcIniFile::Exception &e){
        e.Print();
        return -1;
    }

    if (axis < 0 || axis >= axes) {
	// requested axis exceeds machine axes
	return -1;
    }

    // load its values
    if (0 != loadAxis(axis, &axisIniFile)) {
        return -1;
    }
    return 0;
}

/*! \todo FIXME-- begin temporary insert of ini file stuff */

#define INIFILE_MIN_FLOAT_PRECISION 3
#define INIFILE_BACKUP_SUFFIX ".bak"

int iniGetFloatPrec(const char *str)
{
    const char *ptr = str;
    int prec = 0;

    // find '.', return min precision if no decimal point
    while (1) {
	if (*ptr == 0) {
	    return INIFILE_MIN_FLOAT_PRECISION;
	}
	if (*ptr == '.') {
	    break;
	}
	ptr++;
    }

    // ptr is on '.', so step over
    ptr++;

    // count number of digits until whitespace or end or non-digit
    while (1) {
	if (*ptr == 0) {
	    break;
	}
	if (!isdigit(*ptr)) {
	    break;
	}
	// else it's a digit
	prec++;
	ptr++;
    }

    return prec >
	INIFILE_MIN_FLOAT_PRECISION ? prec : INIFILE_MIN_FLOAT_PRECISION;
}

// end temporary insert of ini file stuff

/*
  dumpAxis(int axis, const char *filename, EMC_AXIS_STAT *status)

  This used to rewrite an AXIS_n section of the ini file.  Everyone
  now seems to think this is a bad idea.  It's certainly incompatible
  with template/sample configurations that should not be changed by
  the user OR the program.
 */
int dumpAxis(int axis, const char *filename, EMC_AXIS_STAT * status)
{
    return 0;
}
