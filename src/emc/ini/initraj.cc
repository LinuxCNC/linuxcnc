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

  Loads ini file params for traj

  COORDINATES <char[]>            axes in system
  LINEAR_UNITS <float>            units per mm
  ANGULAR_UNITS <float>           units per degree
  DEFAULT_LINEAR_VELOCITY <float> default linear velocity
  MAX_LINEAR_VELOCITY <float>     max linear velocity
  DEFAULT_LINEAR_ACCEL <float>    default linear acceleration
  MAX_LINEAR_ACCEL <float>        max linear acceleration

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

    trajInifile->EnableExceptions(EmcIniFile::ERR_CONVERSION);

    try{
	int axes = 0;
	int axismask = 0;
	const char *coord = trajInifile->Find("COORDINATES", "TRAJ");
	if(coord) {
	    if(strchr(coord, 'x') || strchr(coord, 'X')) {
	         axismask |= 1;
	         axes += 1;
            }
	    if(strchr(coord, 'y') || strchr(coord, 'Y')) {
	         axismask |= 2;
	         axes += 1;
            }
	    if(strchr(coord, 'z') || strchr(coord, 'Z')) {
	         axismask |= 4;
	         axes += 1;
            }
	    if(strchr(coord, 'a') || strchr(coord, 'A')) {
	         axismask |= 8;
	         axes += 1;
            }
	    if(strchr(coord, 'b') || strchr(coord, 'B')) {
	         axismask |= 16;
	         axes += 1;
            }
	    if(strchr(coord, 'c') || strchr(coord, 'C')) {
	         axismask |= 32;
	         axes += 1;
            }
	    if(strchr(coord, 'u') || strchr(coord, 'U')) {
	         axismask |= 64;
	         axes += 1;
            }
	    if(strchr(coord, 'v') || strchr(coord, 'V')) {
	         axismask |= 128;
	         axes += 1;
            }
	    if(strchr(coord, 'w') || strchr(coord, 'W')) {
	         axismask |= 256;
	         axes += 1;
            }
	} else {
	    axismask = 1 | 2 | 4;		// default: XYZ machine
	    axes = 3;
	}
        if (0 != emcTrajSetAxes(axes, axismask)) {
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
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetUnits\n");
            }
            return -1;
        }

        vel = 1.0;
        trajInifile->Find(&vel, "DEFAULT_LINEAR_VELOCITY", "TRAJ");

        // and set dynamic value
        if (0 != emcTrajSetVelocity(0, vel)) { //default velocity on startup 0
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetVelocity\n");
            }
            return -1;
        }

        vel = 1e99; // by default, use AXIS limit
        trajInifile->Find(&vel, "MAX_LINEAR_VELOCITY", "TRAJ");

        // and set dynamic value
        if (0 != emcTrajSetMaxVelocity(vel)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetMaxVelocity\n");
            }
            return -1;
        }

        acc = 1e99; // let the axis values apply
        trajInifile->Find(&acc, "DEFAULT_LINEAR_ACCEL", "TRAJ");
        if (0 != emcTrajSetAcceleration(acc)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetAcceleration\n");
            }
            return -1;
        }

        acc = 1e99; // let the axis values apply
        trajInifile->Find(&acc, "MAX_LINEAR_ACCEL", "TRAJ");
        if (0 != emcTrajSetMaxAcceleration(acc)) {
            if (emc_debug & EMC_DEBUG_CONFIG) {
                rcs_print("bad return value from emcTrajSetMaxAcceleration\n");
            }
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
  iniTraj(const char *filename)

  Loads ini file parameters for trajectory, from [TRAJ] section
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
