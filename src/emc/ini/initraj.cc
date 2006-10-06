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
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/

extern "C" {
#include <stdio.h>		// NULL
#include <stdlib.h>		// atol()
#include <string.h>		// strlen()
#include <ctype.h>		// isspace()
}
#include "emc.hh"
#include "posemath.h"		// PM_POSE, PM_RPY
#include "inifile.hh"
#include "initraj.hh"		// these decls
#include "emcglb.h"		/*! \todo TRAVERSE_RATE (FIXME) */

// inifile ref'ed by iniTraj(), loadTraj() 
static Inifile *trajInifile = 0;

/*
  loadTraj()

  Loads ini file params for traj

  AXES <int>                    number of axes in system
  LINEAR_UNITS <float>          units per mm
  ANGULAR_UNITS <float>         units per degree
  CYCLE_TIME <float>            cycle time for traj calculations
  DEFAULT_VELOCITY <float>      default velocity
  MAX_VELOCITY <float>          max velocity
  MAX_ACCELERATION <float>      max acceleration
  DEFAULT_ACCELERATION <float>  default acceleration
  HOME <float> ...              world coords of home, in X Y Z R P W

  calls:

  emcTrajSetAxes(int axes);
  emcTrajSetUnits(double linearUnits, double angularUnits);
  emcTrajSetCycleTime(double cycleTime);
  emcTrajSetVelocity(double vel);
  emcTrajSetAcceleration(double acc);
  emcTrajSetMaxVelocity(double vel);
  emcTrajSetMaxAcceleration(double acc);
  emcTrajSetHome(EmcPose home);
  */

static int getValueFromTable(double *value, const char *inistring, const struct nameval *table) {
    if (1 == sscanf(inistring, "%lf", value)) { return 1; }
    for(; table->name; table++) {
        if(strcmp(inistring, table->name) == 0) {
            *value = table->value;
            return 1;
        }
    }
    return 0;
}

static int loadTraj()
{
    const char *inistring;
    int axes;
    double linearUnits;
    double angularUnits;
    double vel;
    double acc;
    unsigned char coordinateMark[6] = { 1, 1, 1, 0, 0, 0 };
    int t;
    int len;
    char homes[LINELEN];
    char home[LINELEN];
    EmcPose homePose = { {0.0, 0.0, 0.0}, 0.0, 0.0, 0.0 };
    double d;

    if (NULL != (inistring = trajInifile->find("AXES", "TRAJ"))) {
	if (1 == sscanf(inistring, "%d", &axes)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print("invalid inifile value for [TRAJ] AXES: %s\n",
			  inistring);
	    }
	    axes = 0;		// default
	}
    } else {
	// not found at all
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print("can't find [TRAJ] AXES, using default\n");
	}
	axes = 0;		// default
    }
    if (0 != emcTrajSetAxes(axes)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print("bad return value from emcTrajSetAxes\n");
	}
	return -1;
    }

    if (NULL != (inistring = trajInifile->find("LINEAR_UNITS", "TRAJ"))) {
        if (1 == getValueFromTable(&linearUnits, inistring, linear_nv_pairs)) {
            if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
                rcs_print("got LINEAR_UNITS '%s' : %lf\n", inistring, linearUnits);
            }
        } else {
            rcs_print
                ("invalid inifile value for [TRAJ] LINEAR_UNITS: %s\n",
                 inistring);
             linearUnits = 0;
        }
    }
    if (NULL != (inistring = trajInifile->find("ANGULAR_UNITS", "TRAJ"))) {
        if (1 == getValueFromTable(&angularUnits, inistring, angular_nv_pairs)) {
            if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
                rcs_print("got ANGULAR_UNITS '%s' : %lf\n", inistring, angularUnits);
            }
        } else {
            // found, but invalid
            rcs_print
                ("invalid inifile value for [TRAJ] ANGULAR_UNITS: %s\n",
                 inistring);
             angularUnits = 0;
        }
    }

    if (0 != emcTrajSetUnits(linearUnits, angularUnits)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print("bad return value from emcTrajSetUnits\n");
	}
	return -1;
    }

    if (NULL !=
	(inistring = trajInifile->find("DEFAULT_VELOCITY", "TRAJ"))) {
	if (1 == sscanf(inistring, "%lf", &vel)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print
		    ("invalid inifile value for [TRAJ] DEFAULT_VELOCITY: %s\n",
		     inistring);
	    }
	    vel = 1.0;		// default
	}
    } else {
	// not found at all
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print
		("can't find [TRAJ] DEFAULT_VELOCITY, using default\n");
	}
	vel = 1.0;		// default
    }

    // set the corresponding global
    TRAJ_DEFAULT_VELOCITY = vel;

    // and set dynamic value
    if (0 != emcTrajSetVelocity(0, vel)) { //default velocity on startup 0
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print("bad return value from emcTrajSetVelocity\n");
	}
	return -1;
    }

    if (NULL != (inistring = trajInifile->find("MAX_VELOCITY", "TRAJ"))) {
	if (1 == sscanf(inistring, "%lf", &vel)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print
		    ("invalid inifile value for [TRAJ] MAX_VELOCITY: %s\n",
		     inistring);
	    }
	    vel = 1.0;		// default
	}
    } else {
	// not found at all
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print("can't find [TRAJ] MAX_VELOCITY, using default\n");
	}
	vel = 1.0;		// default
    }

    // set the corresponding global
    TRAJ_MAX_VELOCITY = vel;

    // and set dynamic value
    if (0 != emcTrajSetMaxVelocity(vel)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print("bad return value from emcTrajSetMaxVelocity\n");
	}
	return -1;
    }
    // also set the global-- it's likely to be done in emcTrajSetMaxVelocity(),
    // but it can't hurt to make sure
    TRAJ_MAX_VELOCITY = vel;

    if (NULL !=
	(inistring = trajInifile->find("MAX_ACCELERATION", "TRAJ"))) {
	if (1 == sscanf(inistring, "%lf", &acc)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print
		    ("invalid inifile value for [TRAJ] MAX_ACCELERATION: %s\n",
		     inistring);
	    }
	    acc = 1.0;		// default
	}
    } else {
	// not found at all
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print
		("can't find [TRAJ] MAX_ACCELERATION, using default\n");
	}
	acc = 1.0;		// default
    }
    if (0 != emcTrajSetMaxAcceleration(acc)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print("bad return value from emcTrajSetMaxAcceleration\n");
	}
	return -1;
    }

    if (NULL !=
	(inistring = trajInifile->find("DEFAULT_ACCELERATION", "TRAJ"))) {
	if (1 == sscanf(inistring, "%lf", &acc)) {
	    // found, and valid
	} else {
	    // found, but invalid
	    if (EMC_DEBUG & EMC_DEBUG_INVALID) {
		rcs_print
		    ("invalid inifile value for [TRAJ] DEFAULT_ACCELERATION: %s\n",
		     inistring);
	    }
	    acc = 1.0;		// default
	}
    } else {
	// not found at all
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print
		("can't find [TRAJ] DEFAULT_ACCELERATION, using default\n");
	}
	acc = 1.0;		// default
    }
    if (0 != emcTrajSetAcceleration(acc)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print("bad return value from emcTrajSetAcceleration\n");
	}
	return -1;
    }

    // set coordinateMark[] to hold 1's for each coordinate present,
    // so that home position can be interpreted properly
    if (NULL != (inistring = trajInifile->find("COORDINATES", "TRAJ"))) {
	len = strlen(inistring);
	// there's an entry in ini file, so clear all the marks out first
	// so that defaults don't apply at all
	for (t = 0; t < 6; t++) {
	    coordinateMark[t] = 0;
	}
	// now set actual marks
	for (t = 0; t < len; t++) {
	    if (inistring[t] == 'X')
		coordinateMark[0] = 1;
	    else if (inistring[t] == 'Y')
		coordinateMark[1] = 1;
	    else if (inistring[t] == 'Z')
		coordinateMark[2] = 1;
	    else if (inistring[t] == 'R')
		coordinateMark[3] = 1;
	    else if (inistring[t] == 'P')
		coordinateMark[4] = 1;
	    else if (inistring[t] == 'W')
		coordinateMark[5] = 1;
	}
    } else {
	// not there, use default
	// by leaving coordinateMark[] alone, default is X Y Z
    }

    if (NULL != (inistring = trajInifile->find("HOME", "TRAJ"))) {
	// found it, now interpret it according to coordinateMark[]
	strcpy(homes, inistring);
	len = 0;
	for (t = 0; t < 6; t++) {
	    if (!coordinateMark[t]) {
		continue;	// position t at index of next non-zero mark
	    }
	    // there is a mark, so read the string for a value
	    if (1 == sscanf(&homes[len], "%s", home) &&
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
		else
		    homePose.c = d;

		// position string ptr past this value
		len += strlen(home);
		// and at start of next value
		while ((homes[len] == ' ' || home[len] == '\t') &&
		       len < LINELEN) {
		    len++;
		}
		if (len >= LINELEN) {
		    break;	// out of for loop
		}
	    } else {
		// badly formatted entry
		rcs_print("invalid inifile value for [TRAJ] HOME: %s\n",
			  inistring);
		break;
	    }
	}			// end of for-loop on coordinateMark[]
    } else {
	// not found at all
	if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
	    rcs_print("can't find [TRAJ] HOME, using default\n");
	}
    }

    if (0 != emcTrajSetHome(homePose)) {
	if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
	    rcs_print("bad return value from emcTrajSetHome\n");
	}
	return -1;
    }

    return 0;
}

/*
  iniTraj(const char *filename)

  Loads ini file parameters for trajectory, from [TRAJ] section
 */
int iniTraj(const char *filename)
{
    int retval = 0;

    trajInifile = new Inifile;

    if (trajInifile->open(filename) == false) {
	return -1;
    }
    // load trajectory values
    if (0 != loadTraj()) {
	retval = -1;
    }
    // close the inifile
    trajInifile->close();
    delete trajInifile;

    return retval;
}
