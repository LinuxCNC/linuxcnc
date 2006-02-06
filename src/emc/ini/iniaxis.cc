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
* $Revision$
* $Author$
* $Date$
********************************************************************/

extern "C" {
#include <unistd.h>
#include <stdio.h>		// NULL
#include <stdlib.h>		// atol(), _itoa()
#include <string.h>		// strcmp()
#include <ctype.h>		// isdigit()
#include <sys/types.h>
#include <sys/stat.h>
}
#include "emc.hh"
#include "inifile.hh"
#include "iniaxis.hh"		// these decls
#include "emcglb.h"		// EMC_DEBUG
#include "emccfg.h"		// default values for globals

// inifile ref'ed by iniAxes(), loadAxis() 
static Inifile *axisInifile = 0;

/* a function that checks a string to see if it is one of:
   "TRUE", "FALSE", "YES", "NO", "1", or "0" (case insensitive)
   if it is, sets '*result' accordingly, and returns 1, else
   returns 0 and leaves '*result' unchanged */
static int strbool(const char *str, int *result)
{
    if (strcasecmp(str, "TRUE") == 0) {
	*result = 1;
	return 1;
    }
    if (strcasecmp(str, "YES") == 0) {
	*result = 1;
	return 1;
    }
    if (strcasecmp(str, "1") == 0) {
	*result = 1;
	return 1;
    }
    if (strcasecmp(str, "FALSE") == 0) {
	*result = 0;
	return 1;
    }
    if (strcasecmp(str, "NO") == 0) {
	*result = 0;
	return 1;
    }
    if (strcasecmp(str, "0") == 0) {
	*result = 0;
	return 1;
    }
    return 0;
}

/*
  loadAxis(int axis)

  Loads ini file params for axis, axis = 0, ...

  TYPE <LINEAR ANGULAR>        type of axis
  UNITS <float>                units per mm or deg
  MAX_VELOCITY <float>         max vel for axis
  MAX_ACCELERATION <float>     max accel for axis
  BACKLASH <float>             backlash
  CYCLE_TIME <float>           cycle time
  INPUT_SCALE <float> <float>  scale, offset
  OUTPUT_SCALE <float> <float> scale, offset
  MIN_LIMIT <float>            minimum soft position limit
  MAX_LIMIT <float>            maximum soft position limit
  FERROR <float>               maximum following error, scaled to max vel
  MIN_FERROR <float>           minimum following error
  HOME <float>                 home position (where to go after home)
  HOME_OFFSET <float>          home switch/index pulse location
  HOME_SEARCH_VEL <float>      homing speed, search phase
  HOME_LATCH_VEL <float>       homing speed, latch phase
  HOME_USE_INDEX <bool>        use index pulse when homing?
  HOME_IGNORE_LIMITS <bool>    ignore limit switches when homing?
  COMP_FILE <filename>         file of axis compensation points

  calls:

  emcAxisSetAxis(int axis, unsigned char axisType);
  emcAxisSetUnits(int axis, double units); */
/*! \todo FIXME - these gains are no longer used */
/*  emcAxisSetGains(int axis, double p, double i, double d, double ff0, double ff1, double ff2, double bias, double maxError, double deadband);
  emcAxisSetBacklash(int axis, double backlash);
  emcAxisSetCycleTime(int axis, double cycleTime);
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

static int loadAxis(int axis)
{
#define AXIS_STRING_LEN 16
    char axisString[AXIS_STRING_LEN];
    const char *inistring;
    unsigned char axisType;
    double units;
    double backlash;
    double offset;
    double limit;
    double home;
    double search_vel;
    double latch_vel;
    int use_index;
    int ignore_limits;
    double maxVelocity;
    double maxAcceleration;
    double maxFerror;

    // compose string to match, axis = 0 -> AXIS_1, etc.
    sprintf(axisString, "AXIS_%d", axis);

    // set axis type

    if (NULL != (inistring = axisInifile->find("TYPE", axisString))) {
	if (!strcmp(inistring, "LINEAR")) {
	    // found, and valid
	    axisType = EMC_AXIS_LINEAR;
	} else if (!strcmp(inistring, "ANGULAR")) {
	    // found, and valid
	    axisType = EMC_AXIS_ANGULAR;
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] TYPE: %s\n",
		     axisString, inistring);
	    }
	    axisType = EMC_AXIS_LINEAR;	// default is linear
	}
    } else {
	// not found at all
	axisType = EMC_AXIS_LINEAR;	// default is linear
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] TYPE, using default\n",
			    axisString);
	}
    }
    if (0 != emcAxisSetAxis(axis, axisType)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetAxis\n");
	}
	return -1;
    }
    // set units

    if (NULL != (inistring = axisInifile->find("UNITS", axisString))) {
	if (1 == sscanf(inistring, "%lf", &units)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] UNITS: %s\n",
		     axisString, inistring);
	    }
	    units = 1.0;	// default
	}
    } else {
	// not found at all
	units = 1.0;		// default
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] UNITS, using default\n",
			    axisString);
	}
    }
    if (0 != emcAxisSetUnits(axis, units)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetUnits\n");
	}
	return -1;
    }
    // set backlash

    if (NULL != (inistring = axisInifile->find("BACKLASH", axisString))) {
	if (1 == sscanf(inistring, "%lf", &backlash)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] BACKLASH: %s\n",
		     axisString, inistring);
	    }
	    backlash = 0;	// default
	}
    } else {
	// not found at all
	backlash = 0;		// default
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] BACKLASH, using default\n",
			    axisString);
	}
    }
    if (0 != emcAxisSetBacklash(axis, backlash)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetBacklash\n");
	}
	return -1;
    }

    // set min position limit

    if (NULL != (inistring = axisInifile->find("MIN_LIMIT", axisString))) {
	if (1 == sscanf(inistring, "%lf", &limit)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] MIN_LIMIT: %s\n",
		     axisString, inistring);
	    }
	    limit = -1;		// default for min limit
	}
    } else {
	// not found at all
	limit = -1;		// default for min limit
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] MIN_LIMIT, using default\n",
			    axisString);
	}
    }

    if (0 != emcAxisSetMinPositionLimit(axis, limit)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error
		("bad return from emcAxisSetMinPositionLimit\n");
	}
	return -1;
    }
    // set max position limit

    if (NULL != (inistring = axisInifile->find("MAX_LIMIT", axisString))) {
	if (1 == sscanf(inistring, "%lf", &limit)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] MAX_LIMIT: %s\n",
		     axisString, inistring);
	    }
	    limit = 1;		// default for max limit
	}
    } else {
	// not found at all
	limit = 1;		// default for max limit
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] MAX_LIMIT, using default\n",
			    axisString);
	}
    }
    if (0 != emcAxisSetMaxPositionLimit(axis, limit)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error
		("bad return from emcAxisSetMaxPositionLimit\n");
	}
	return -1;
    }
    // set following error limit (at max speed)

    if (NULL != (inistring = axisInifile->find("FERROR", axisString))) {
	if (1 == sscanf(inistring, "%lf", &maxFerror)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] FERROR: %s\n",
		     axisString, inistring);
	    }
	    maxFerror = 1;	// default for max ferror
	}
    } else {
	// not found at all
	maxFerror = 1;		// default for max ferror
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] FERROR, using default\n",
			    axisString);
	}
    }
    if (0 != emcAxisSetFerror(axis, maxFerror)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetFerror\n");
	}
	return -1;
    }
    // do MIN_FERROR, if it's there. If not, use value of maxFerror above

    if (NULL != (inistring = axisInifile->find("MIN_FERROR", axisString))) {
	if (1 == sscanf(inistring, "%lf", &limit)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    limit = maxFerror;	// use prev value of max ferror
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] MIN_FERROR: %s, using default %f\n",
		     axisString, inistring, limit);
	    }
	}
    } else {
	// not found at all
	limit = maxFerror;	// use prev value of max ferror
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error
		("can't find [%s] MIN_FERROR, using default %f\n",
		 axisString, limit);
	}
    }
    if (0 != emcAxisSetMinFerror(axis, limit)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetMinFerror\n");
	}
	return -1;
    }
    // set homing paramsters (total of 6)

    if (NULL != (inistring = axisInifile->find("HOME", axisString))) {
	if (1 == sscanf(inistring, "%lf", &home)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] HOME: %s\n",
		     axisString, inistring);
	    }
	    home = 0.0;		// default
	}
    } else {
	// not found at all
	home = 0.0;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] HOME, using default\n",
			    axisString);
	}
    }

    if (NULL != (inistring = axisInifile->find("HOME_OFFSET", axisString))) {
	if (1 == sscanf(inistring, "%lf", &offset)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] HOME_OFFSET: %s\n",
		     axisString, inistring);
	    }
	    offset = 0.0;	// default
	}
    } else {
	// not found at all
	offset = 0.0;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [%s] HOME_OFFSET, using default\n",
			    axisString);
	}
    }

    if (NULL !=
	(inistring = axisInifile->find("HOME_SEARCH_VEL", axisString))) {
	if (1 == sscanf(inistring, "%lf", &search_vel)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] HOME_SEARCH_VEL: %s\n",
		     axisString, inistring);
	    }
	    search_vel = 0.0;	// default - skips entire homing process
	}
    } else {
	// not found at all
	search_vel = 0.0;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error
		("can't find [%s] HOME_SEARCH_VEL, using default\n",
		 axisString);
	}
    }

    if (NULL !=
	(inistring = axisInifile->find("HOME_LATCH_VEL", axisString))) {
	if (1 == sscanf(inistring, "%lf", &latch_vel)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] HOME_LATCH_VEL: %s\n",
		     axisString, inistring);
	    }
	    latch_vel = 0.0;	// default
	}
    } else {
	// not found at all
	latch_vel = 0.0;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error
		("can't find [%s] HOME_LATCH_VEL, using default\n",
		 axisString);
	}
    }

    if (NULL !=
	(inistring = axisInifile->find("HOME_USE_INDEX", axisString))) {
	if (1 == strbool(inistring, &use_index)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] HOME_USE_INDEX: %s\n",
		     axisString, inistring);
	    }
	    use_index = 0;	// default
	}
    } else {
	// not found at all
	use_index = 0;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error
		("can't find [%s] HOME_USE_INDEX, using default\n",
		 axisString);
	}
    }

    if (NULL !=
	(inistring =
	 axisInifile->find("HOME_IGNORE_LIMITS", axisString))) {
	if (1 == strbool(inistring, &ignore_limits)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] HOME_IGNORE_LIMITS: %s\n",
		     axisString, inistring);
	    }
	    ignore_limits = 0;	// default
	}
    } else {
	// not found at all
	ignore_limits = 0;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error
		("can't find [%s] HOME_IGNORE_LIMITS, using default\n",
		 axisString);
	}
    }
    // issue NML message to set all params
    if (0 != emcAxisSetHomingParams(axis, home, offset, search_vel,
				    latch_vel, use_index, ignore_limits)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetHomingParams\n");
	}
	return -1;
    }

    // set maximum velocity

    if (NULL !=
	(inistring = axisInifile->find("MAX_VELOCITY", axisString))) {
	if (1 == sscanf(inistring, "%lf", &maxVelocity)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] MAX_VELOCITY: %s\n",
		     axisString, inistring);
	    }
	    maxVelocity = DEFAULT_AXIS_MAX_VELOCITY;	// default
	}
    } else {
	// not found at all
	maxVelocity = DEFAULT_AXIS_MAX_VELOCITY;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error
		("can't find [%s] MAX_VELOCITY, using default\n",
		 axisString);
	}
    }
    if (0 != emcAxisSetMaxVelocity(axis, maxVelocity)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetMaxVelocity\n");
	}
	return -1;
    }

    if (NULL !=
	(inistring = axisInifile->find("MAX_ACCELERATION", axisString))) {
	if (1 == sscanf(inistring, "%lf", &maxAcceleration)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [%s] MAX_ACCELERATION: %s\n",
		     axisString, inistring);
	    }
	    maxAcceleration = DEFAULT_AXIS_MAX_ACCELERATION;	// default
	}
    } else {
	// not found at all
	maxAcceleration = DEFAULT_AXIS_MAX_ACCELERATION;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error
		("can't find [%s] MAX_ACCELERATION, using default\n",
		 axisString);
	}
    }
    if (0 != emcAxisSetMaxAcceleration(axis, maxAcceleration)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print_error("bad return from emcAxisSetMaxAcceleration\n");
	}
	return -1;
    }

    if (NULL != (inistring = axisInifile->find("COMP_FILE", axisString))) {
	if (0 != emcAxisLoadComp(axis, inistring)) {
	    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
		rcs_print_error("bad return from emcAxisLoadComp\n");
	    }
	    return -1;
	}
    }
    // else not found, so ignore

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
    int retval = 0;
    const char *inistring;
    int axes;

    axisInifile = new Inifile;
    if (axisInifile->open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = axisInifile->find("AXES", "TRAJ"))) {
	if (1 == sscanf(inistring, "%d", &axes)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print_error
		    ("invalid inifile value for [TRAJ] AXES: %s\n",
		     inistring);
	    }
	    axes = 0;
	}
    } else {
	// not found at all
	axes = 1;
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print_error("can't find [TRAJ] AXES, using default %d\n",
			    axes);
	}
    }

    if (axis < 0 || axis >= axes) {
	// requested axis exceeds machine axes
	axisInifile->close();
	delete axisInifile;
	return -1;
    }
    // load its values
    if (0 != loadAxis(axis)) {
	retval = -1;
    }
    // close the inifile
    axisInifile->close();
    delete axisInifile;

    return retval;
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

int iniFormatFloat(char *fmt, const char *var, const char *val)
{
    sprintf(fmt, "%s = %%.%df\n", var, iniGetFloatPrec(val));

    return 0;
}

// 'val' in this case is a string with a pair of floats, the first
// which sets the precision
int iniFormatFloat2(char *fmt, const char *var, const char *val)
{
    int prec;

    /*! \todo FIXME-- should capture each one's float precision; right
       now we're using the first as the precision for both */
    prec = iniGetFloatPrec(val);
    sprintf(fmt, "%s = %%.%df %%.%df\n", var, prec, prec);

    return 0;
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
