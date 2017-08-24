/********************************************************************
* Description: inijoint.cc
*   INI file initialization routines for joint/axis NML
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004-2009 All rights reserved.
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
#include "inijoint.hh"		// these decls
#include "emcglb.h"		// EMC_DEBUG
#include "emccfg.h"		// default values for globals

#include "inihal.hh"

extern value_inihal_data old_inihal_data;
/*
  loadJoint(int joint)

  Loads ini file params for joint, joint = 0, ...

  TYPE <LINEAR ANGULAR>        type of joint
  MAX_VELOCITY <float>         max vel for joint
  MAX_ACCELERATION <float>     max accel for joint
  BACKLASH <float>             backlash
  MIN_LIMIT <float>            minimum soft position limit
  MAX_LIMIT <float>            maximum soft position limit
  FERROR <float>               maximum following error, scaled to max vel
  MIN_FERROR <float>           minimum following error
  HOME <float>                 home position (where to go after home)
  HOME_FINAL_VEL <float>       speed to move from HOME_OFFSET to HOME location (at the end of homing)
  HOME_OFFSET <float>          home switch/index pulse location
  HOME_SEARCH_VEL <float>      homing speed, search phase
  HOME_LATCH_VEL <float>       homing speed, latch phase
  HOME_USE_INDEX <bool>        use index pulse when homing
  HOME_IGNORE_LIMITS <bool>    ignore limit switches when homing
  COMP_FILE <filename>         file of joint compensation points

  calls:

  emcJointSetType(int joint, unsigned char jointType);
  emcJointSetUnits(int joint, double units);
  emcJointSetBacklash(int joint, double backlash);
  emcJointSetMinPositionLimit(int joint, double limit);
  emcJointSetMaxPositionLimit(int joint, double limit);
  emcJointSetFerror(int joint, double ferror);
  emcJointSetMinFerror(int joint, double ferror);
  emcJointSetHomingParams(int joint, double home, double offset, double home_vel, 
                          double search_vel, double latch_vel, int use_index, 
                          int ignore_limits, int is_shared, int sequence, int volatile_home));
  emcJointActivate(int joint);
  emcJointSetMaxVelocity(int joint, double vel);
  emcJointSetMaxAcceleration(int joint, double acc);
  emcJointLoadComp(int joint, const char * file, int comp_file_type);
  */

static int loadJoint(int joint, EmcIniFile *jointIniFile)
{
    char jointString[16];
    const char *inistring;
    EmcJointType jointType;
    double units;
    double backlash;
    double offset;
    double limit;
    double home;
    double search_vel;
    double latch_vel;
    double final_vel; // moving from OFFSET to HOME
    bool use_index;
    bool ignore_limits;
    bool is_shared;
    int sequence;
    int volatile_home;
    int locking_indexer;
    int absolute_encoder;
    int comp_file_type; //type for the compensation file. type==0 means nom, forw, rev. 
    double maxVelocity;
    double maxAcceleration;
    double ferror;

    // compose string to match, joint = 0 -> JOINT_0, etc.
    sprintf(jointString, "JOINT_%d", joint);

    jointIniFile->EnableExceptions(EmcIniFile::ERR_CONVERSION);
    
    try {
        // set joint type
        jointType = EMC_LINEAR;	// default
        jointIniFile->Find(&jointType, "TYPE", jointString);
        if (0 != emcJointSetType(joint, jointType)) {
            return -1;
        }

        // set units
        if(jointType == EMC_LINEAR){
            units = emcTrajGetLinearUnits();
        }else{
            units = emcTrajGetAngularUnits();
        }
        if (0 != emcJointSetUnits(joint, units)) {
            return -1;
        }

        // set backlash
        backlash = 0;	                // default
        jointIniFile->Find(&backlash, "BACKLASH", jointString);
        if (0 != emcJointSetBacklash(joint, backlash)) {
            return -1;
        }
        old_inihal_data.joint_backlash[joint] = backlash;

        // set min position limit
        limit = -1e99;	                // default
        jointIniFile->Find(&limit, "MIN_LIMIT", jointString);
        if (0 != emcJointSetMinPositionLimit(joint, limit)) {
             return -1;
        }
        old_inihal_data.joint_min_limit[joint] = limit;

        // set max position limit
        limit = 1e99;	                // default
        jointIniFile->Find(&limit, "MAX_LIMIT", jointString);
        if (0 != emcJointSetMaxPositionLimit(joint, limit)) {
            return -1;
        }
        old_inihal_data.joint_max_limit[joint] = limit;

        // set following error limit (at max speed)
        ferror = 1;	                // default
        jointIniFile->Find(&ferror, "FERROR", jointString);
        if (0 != emcJointSetFerror(joint, ferror)) {
             return -1;
        }
        old_inihal_data.joint_ferror[joint] = ferror;

        // do MIN_FERROR, if it's there. If not, use value of maxFerror above
        jointIniFile->Find(&ferror, "MIN_FERROR", jointString);
        if (0 != emcJointSetMinFerror(joint, ferror)) {
            return -1;
        }
        old_inihal_data.joint_min_ferror[joint] = ferror;

        // set homing paramsters
        home = 0;	                // default
        jointIniFile->Find(&home, "HOME", jointString);
        old_inihal_data.joint_home[joint] = home;

        offset = 0;	                // default
        jointIniFile->Find(&offset, "HOME_OFFSET", jointString);
        old_inihal_data.joint_home_offset[joint] = offset;

        search_vel = 0;	                // default
        jointIniFile->Find(&search_vel, "HOME_SEARCH_VEL", jointString);
        latch_vel = 0;	                // default
        jointIniFile->Find(&latch_vel, "HOME_LATCH_VEL", jointString);
        final_vel = -1;	                // default (rapid)
        jointIniFile->Find(&final_vel, "HOME_FINAL_VEL", jointString);
        is_shared = false;	        // default
        jointIniFile->Find(&is_shared, "HOME_IS_SHARED", jointString);
        use_index = false;	        // default
        jointIniFile->Find(&use_index, "HOME_USE_INDEX", jointString);
        ignore_limits = false;	        // default
        jointIniFile->Find(&ignore_limits, "HOME_IGNORE_LIMITS", jointString);

        sequence = 999;// default: use unrealizable and postive sequence no.
                       // so that joints with unspecified HOME_SEQUENCE=
                       // will not be homed in home-all
        jointIniFile->Find(&sequence, "HOME_SEQUENCE", jointString);
        old_inihal_data.joint_home_sequence[joint] = sequence;

        volatile_home = 0;	        // default
        jointIniFile->Find(&volatile_home, "VOLATILE_HOME", jointString);
        locking_indexer = false;
        jointIniFile->Find(&locking_indexer, "LOCKING_INDEXER", jointString);
        absolute_encoder = false;
        jointIniFile->Find(&absolute_encoder, "HOME_ABSOLUTE_ENCODER", jointString);
        // issue NML message to set all params
        if (0 != emcJointSetHomingParams(joint, home, offset
                                        ,final_vel, search_vel, latch_vel
                                        ,(int)use_index
                                        ,(int)ignore_limits
                                        ,(int)is_shared
                                        ,sequence
                                        ,volatile_home
                                        ,locking_indexer
                                        ,absolute_encoder
                                        )) {
            return -1;
        }

        // set maximum velocity
        maxVelocity = DEFAULT_JOINT_MAX_VELOCITY;
        jointIniFile->Find(&maxVelocity, "MAX_VELOCITY", jointString);
        if (0 != emcJointSetMaxVelocity(joint, maxVelocity)) {
            return -1;
        }
        old_inihal_data.joint_max_velocity[joint] = maxVelocity;

        maxAcceleration = DEFAULT_JOINT_MAX_ACCELERATION;
        jointIniFile->Find(&maxAcceleration, "MAX_ACCELERATION", jointString);
        if (0 != emcJointSetMaxAcceleration(joint, maxAcceleration)) {
            return -1;
        }
        old_inihal_data.joint_max_acceleration[joint] = maxAcceleration;

        comp_file_type = 0;             // default
        jointIniFile->Find(&comp_file_type, "COMP_FILE_TYPE", jointString);
        if (NULL != (inistring = jointIniFile->Find("COMP_FILE", jointString))) {
            if (0 != emcJointLoadComp(joint, inistring, comp_file_type)) {
                return -1;
            }
        }
    }

    catch (EmcIniFile::Exception &e) {
        e.Print();
        return -1;
    }

    // lastly, activate joint. Do this last so that the motion controller
    // won't flag errors midway during configuration
    if (0 != emcJointActivate(joint)) {
        return -1;
    }

    return 0;
}

/*
  iniJoint(int joint, const char *filename)

  Loads ini file parameters for specified joint

  Looks for [KINS]JOINTS for how many to do, up to EMC_JOINT_MAX.
 */
int iniJoint(int joint, const char *filename)
{
    EmcIniFile jointIniFile(EmcIniFile::ERR_TAG_NOT_FOUND |
                           EmcIniFile::ERR_SECTION_NOT_FOUND |
                           EmcIniFile::ERR_CONVERSION);

    if (jointIniFile.Open(filename) == false) {
	return -1;
    }

    // load its values
    if (0 != loadJoint(joint, &jointIniFile)) {
        return -1;
    }

    return 0;
}

