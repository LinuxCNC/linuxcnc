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
#include <stdio.h>              // NULL
#include <stdlib.h>             // atol(), _itoa()
#include <string.h>             // strcmp()
#include <ctype.h>              // isdigit()
#include <sys/types.h>
#include <sys/stat.h>
}

#include "emc.hh"
#include "inifile.hh"
#include "iniaxis.hh"           // these decls
#include "emcglb.h"             // EMC_DEBUG
#include "emccfg.h"             // default values for globals

// inifile ref'ed by iniAxes(), loadAxis()
static INIFILE *axisInifile = 0;

/*
  loadAxis(int axis)

  Loads ini file params for axis, axis = 0, ...

  TYPE <LINEAR ANGULAR>        type of axis
  UNITS <float>                units per mm or deg
  HOME <float>                 home position
  MAX_VELOCITY <float>         max vel for axis
  MAX_ACCELERATION <float>     max accel for axis
  P <float>                    proportional gain
  I <float>                    integral gain
  D <float>                    derivative gain
  FF0 <float>                  0th order feedforward (position)
  FF1 <float>                  1st order feedforward (velocity)
  FF2 <float>                  2nd order feedforward (acceleration)
  MAX_ERROR <float>            max cumulative error
  BACKLASH <float>             backlash
  BIAS <float>                 constant bias
  DEADBAND <float>             error deadband
  CYCLE_TIME <float>           cycle time
  INPUT_SCALE <float> <float>  scale, offset
  OUTPUT_SCALE <float> <float> scale, offset
  MIN_LIMIT <float>            minimum soft position limit
  MAX_LIMIT <float>            maximum soft position limit
  MIN_OUTPUT <float>           minimum output value (voltage, typically)
  MAX_OUTPUT <float>           maximum output value (voltage, typically)
  FERROR <float>               maximum following error, scaled to max vel
  MIN_FERROR <float>           minimum following error
  HOMING_VEL <float>           homing speed, positive
  SETUP_TIME <float>             number of periods dir change preceeds step
  HOLD_TIME <float>              number of periods step line is held low/high after active edge
  ENABLE_POLARITY <0, 1>       polarity for amp enable output
  MIN_LIMIT_SWITCH_POLARITY <0, 1> polarity for min limit switch input
  MAX_LIMIT_SWITCH_POLARITY <0, 1> polarity for max limit switch input
  HOME_SWITCH_POLARITY <0, 1>  polarity for home switch input
  HOMING_POLARITY <0, 1>       direction for homing search
  HOME_OFFSET <float>          where to move axis after home
  FAULT_POLARITY <0, 1>        polarity for amp fault input
  COMP_FILE <filename>         file of axis compensation points

  calls:

  emcAxisSetAxis(int axis, unsigned char axisType);
  emcAxisSetUnits(int axis, double units);
  emcAxisSetGains(int axis, double p, double i, double d, double ff0, double ff1, double ff2, double backlash, double bias, double maxError, double deadband);
  emcAxisSetCycleTime(int axis, double cycleTime);
  emcAxisSetInterpolationRate(int axis, int rate);
  emcAxisSetInputScale(int axis, double scale, double offset);
  emcAxisSetOutputScale(int axis, double scale, double offset);
  emcAxisSetMinPositionLimit(int axis, double limit);
  emcAxisSetMaxPositionLimit(int axis, double limit);
  emcAxisSetMinOutputLimit(int axis, double limit);
  emcAxisSetMaxOutputLimit(int axis, double limit);
  emcAxisSetFerror(int axis, double ferror);
  emcAxisSetMinFerror(int axis, double ferror);
  emcAxisSetHomingVel(int axis, double homingVel);
  emcAxisSetStepParams(int axis, double setup_time, double hold_time);
  emcAxisSetEnablePolarity(int axis, int level);
  emcAxisSetMinLimitSwitchPolarity(int axis, int level);
  emcAxisSetMaxLimitSwitchPolarity(int axis, int level);
  emcAxisSetHomeSwitchPolarity(int axis, int level);
  emcAxisSetHomingPolarity(int axis, int level);
  emcAxisSetFaultPolarity(int axis, int level);
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
/* FIXME - variables no longer needed */
#if 0
  double p, i, d, ff0, ff1, ff2;
#endif
  double backlash;
#if 0
  double bias;
  double maxError;
  double deadband;
  double cycleTime;
  double scale;
#endif
  double offset;
  double limit;
  double homingVel;
#if 0
  double setup_time;
  double hold_time;
#endif
  double home;
  double maxVelocity;
  double maxAcceleration;
  int polarity;
  double maxFerror;

  // compose string to match, axis = 0 -> AXIS_1, etc.
  sprintf(axisString, "AXIS_%d", axis);

  // set axis type

  if (NULL != (inistring = axisInifile->find("TYPE", axisString))) {
    if (! strcmp(inistring, "LINEAR")) {
      // found, and valid
      axisType = EMC_AXIS_LINEAR;
    }
    else if (! strcmp(inistring, "ANGULAR")) {
      // found, and valid
      axisType = EMC_AXIS_ANGULAR;
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] TYPE: %s\n", axisString, inistring);
      }
      axisType = EMC_AXIS_LINEAR;       // default is linear
    }
  }
  else {
    // not found at all
    axisType = EMC_AXIS_LINEAR; // default is linear
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] TYPE, using default\n", axisString);
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
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] UNITS: %s\n", axisString, inistring);
      }
      units = 1.0;                      // default
    }
  }
  else {
    // not found at all
    units = 1.0;                        // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] UNITS, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetUnits(axis, units)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetUnits\n");
    }
    return -1;
  }

/* FIXME - PID gains no longer in ini file, this gets deleted */
/* maybe.... perhaps the gains should be in the ini, but they */
/* should be passed to the HAL pid block another way */
#if 0

  // set forward gains

  if (NULL != (inistring = axisInifile->find("P", axisString))) {
    if (1 == sscanf(inistring, "%lf", &p)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] P: %s\n", axisString, inistring);
      }
      p = 0;                    // default
    }
  }
  else {
    // not found at all
    p = 0;                      // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] P, using default\n", axisString);
    }
  }

  if (NULL != (inistring = axisInifile->find("I", axisString))) {
    if (1 == sscanf(inistring, "%lf", &i)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] I: %s\n", axisString, inistring);
      }
      i = 0;                    // default
    }
  }
  else {
    // not found at all
    i = 0;                      // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] I, using default\n", axisString);
    }
  }

  if (NULL != (inistring = axisInifile->find("D", axisString))) {
    if (1 == sscanf(inistring, "%lf", &d)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] D: %s\n", axisString, inistring);
      }
      d = 0;                    // default
    }
  }
  else {
    // not found at all
    d = 0;                      // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] D, using default\n", axisString);
    }
  }

  if (NULL != (inistring = axisInifile->find("FF0", axisString))) {
    if (1 == sscanf(inistring, "%lf", &ff0)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] FF0: %s\n", axisString, inistring);
      }
      ff0 = 0;                  // default
    }
  }
  else {
    // not found at all
    ff0 = 0;                    // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] FF0, using default\n", axisString);
    }
  }

  if (NULL != (inistring = axisInifile->find("FF1", axisString))) {
    if (1 == sscanf(inistring, "%lf", &ff1)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] FF1: %s\n", axisString, inistring);
      }
      ff1 = 0;                  // default
    }
  }
  else {
    // not found at all
    ff1 = 0;                    // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] FF1, using default\n", axisString);
    }
  }

  if (NULL != (inistring = axisInifile->find("FF2", axisString))) {
    if (1 == sscanf(inistring, "%lf", &ff2)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] FF2: %s\n", axisString, inistring);
      }
      ff2 = 0;                  // default
    }
  }
  else {
    // not found at all
    ff2 = 0;                    // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] FF2, using default\n", axisString);
    }
  }
#endif

/* FIXME - backlash is not a PID parameter, so we keep it */

  if (NULL != (inistring = axisInifile->find("BACKLASH", axisString))) {
    if (1 == sscanf(inistring, "%lf", &backlash)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] BACKLASH: %s\n", axisString, inistring);
      }
      backlash = 0;                     // default
    }
  }
  else {
    // not found at all
    backlash = 0;                       // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] BACKLASH, using default\n", axisString);
    }
  }

/* FIXME - more PID parameters, no longer needed */
#if 0

  if (NULL != (inistring = axisInifile->find("BIAS", axisString))) {
    if (1 == sscanf(inistring, "%lf", &bias)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] BIAS: %s\n", axisString, inistring);
      }
      bias = 0;                 // default
    }
  }
  else {
    // not found at all
    bias = 0;                   // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] BIAS, using default\n", axisString);
    }
  }

  // max cumulative error

  if (NULL != (inistring = axisInifile->find("MAX_ERROR", axisString))) {
    if (1 == sscanf(inistring, "%lf", &maxError)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MAX_ERROR: %s\n", axisString, inistring);
      }
      maxError = 0;                     // default
    }
  }
  else {
    // not found at all
    maxError = 0;                       // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MAX_ERROR, using default\n", axisString);
    }
  }

  // deadband

  if (NULL != (inistring = axisInifile->find("DEADBAND", axisString))) {
    if (1 == sscanf(inistring, "%lf", &deadband)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] DEADBAND: %s\n", axisString, inistring);
      }
      deadband = 0;                     // default
    }
  }
  else {
    // not found at all
    deadband = 0;                       // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] DEADBAND, using default\n", axisString);
    }
  }
#endif

  // now set them

/* FIXME - need to handle backlash separately from the rest */
#if 0

  if (0 != emcAxisSetGains(axis,
                           p, i, d, ff0, ff1, ff2,
                           backlash, bias, maxError, deadband)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetGains\n");
    }
    return -1;
  }
#endif

/* FIXME - cycle times and scaling no longer needed */
#if 0
  // set cycle time

  if (NULL != (inistring = axisInifile->find("CYCLE_TIME", axisString))) {
    if (1 == sscanf(inistring, "%lf", &cycleTime)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] CYCLE_TIME: %s\n", axisString, inistring);
      }
      cycleTime = 1.0;                  // default
    }
  }
  else {
    // not found at all
    cycleTime = 1.0;                    // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] CYCLE_TIME, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetCycleTime(axis, cycleTime)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetCycleTime\n");
    }
    return -1;
  }

  // set input scale

  if (NULL != (inistring = axisInifile->find("INPUT_SCALE", axisString))) {
    if (2 == sscanf(inistring, "%lf %lf", &scale, &offset)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] INPUT_SCALE: %s\n", axisString, inistring);
      }
      scale = 1.0;                      // default
      offset = 0.0;                     // default
    }
  }
  else {
    // not found at all
    scale = 1.0;                        // default
    offset = 0.0;                       // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] INPUT_SCALE, using default\n", axisString);
    }
  }

  if (0 != emcAxisSetInputScale(axis, scale, offset)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetInputScale\n");
    }
    return -1;
  }

  // set output scale

  if (NULL != (inistring = axisInifile->find("OUTPUT_SCALE", axisString))) {
    if (2 == sscanf(inistring, "%lf %lf", &scale, &offset)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] OUTPUT_SCALE: %s\n", axisString, inistring);
      }
      scale = 1.0;                      // default
      offset = 0.0;                     // default
    }
  }
  else {
    // not found at all
    scale = 1.0;                        // default
    offset = 0.0;                       // default
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] OUTPUT_SCALE, using default\n", axisString);
    }
  }

  if (0 != emcAxisSetOutputScale(axis, scale, offset)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetOutputScale\n");
    }
    return -1;
  }
#endif

  if (NULL != (inistring = axisInifile->find("MIN_LIMIT", axisString))) {
    if (1 == sscanf(inistring, "%lf", &limit)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MIN_LIMIT: %s\n", axisString, inistring);
      }
      limit = -1;                       // default for min limit
    }
  }
  else {
    // not found at all
    limit = -1;                 // default for min limit
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MIN_LIMIT, using default\n", axisString);
    }
  }

  if (0 != emcAxisSetMinPositionLimit(axis, limit)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMinPositionLimit\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("MAX_LIMIT", axisString))) {
    if (1 == sscanf(inistring, "%lf", &limit)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MAX_LIMIT: %s\n", axisString, inistring);
      }
      limit = 1;                        // default for max limit
    }
  }
  else {
    // not found at all
    limit = 1;                  // default for max limit
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MAX_LIMIT, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetMaxPositionLimit(axis, limit)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMaxPositionLimit\n");
    }
    return -1;
  }
#if 0  /* these commands no longer exist */
  if (NULL != (inistring = axisInifile->find("MIN_OUTPUT", axisString))) {
    if (1 == sscanf(inistring, "%lf", &limit)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MIN_OUTPUT: %s\n", axisString, inistring);
      }
      limit = -1;                       // default for min output
    }
  }
  else {
    // not found at all
    limit = -1;                 // default for min output
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MIN_OUTPUT, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetMinOutputLimit(axis, limit)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMinOutputLimit\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("MAX_OUTPUT", axisString))) {
    if (1 == sscanf(inistring, "%lf", &limit)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MAX_OUTPUT: %s\n", axisString, inistring);
      }
      limit = 1;                        // default for max output
    }
  }
  else {
    // not found at all
    limit = 1;                  // default for max output
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MAX_OUTPUT, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetMaxOutputLimit(axis, limit)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMaxOutputLimit\n");
    }
    return -1;
  }
#endif

  if (NULL != (inistring = axisInifile->find("FERROR", axisString))) {
    if (1 == sscanf(inistring, "%lf", &maxFerror)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] FERROR: %s\n", axisString, inistring);
      }
      maxFerror = 1;                    // default for max ferror
    }
  }
  else {
    // not found at all
    maxFerror = 1;                      // default for max ferror
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] FERROR, using default\n", axisString);
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
    }
    else {
      // found, but invalid
      limit = maxFerror;        // use prev value of max ferror
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MIN_FERROR: %s, using default %f\n", axisString, inistring, limit);
      }
    }
  }
  else {
    // not found at all
    limit = maxFerror;  // use prev value of max ferror
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MIN_FERROR, using default %f\n", axisString, limit);
    }
  }
  if (0 != emcAxisSetMinFerror(axis, limit)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMinFerror\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("HOMING_VEL", axisString))) {
    if (1 == sscanf(inistring, "%lf", &homingVel)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] HOMING_VEL: %s\n", axisString, inistring);
      }
      homingVel = 1;                    // default for homing vel
    }
  }
  else {
    // not found at all
    homingVel = 1;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] HOMING_VEL, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetHomingVel(axis, homingVel)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetHomingVel\n");
    }
    return -1;
  }

/* FIXME - step timing parameters no longer handled here */
#if 0

  if (NULL != (inistring = axisInifile->find("SETUP_TIME", axisString))) {
    if (1 == sscanf(inistring, "%lf", &setup_time)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] SETUP_TIME: %s\n", axisString, inistring);
      }
      setup_time = 1;                    // default for setup time
    }
  }
  else {
    // not found at all
    setup_time = 1;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] SETUP_TIME, using default\n", axisString);
    }
  }
  if (NULL != (inistring = axisInifile->find("HOLD_TIME", axisString))) {
    if (1 == sscanf(inistring, "%lf", &hold_time)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] HOLD_TIME: %s\n", axisString, inistring);
      }
      hold_time = 2;                    // default for setup time
    }
  }
  else {
    // not found at all
    hold_time = 2;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] HOLD_TIME, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetStepParams(axis, setup_time, hold_time)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetStepParams\n");
    }
    return -1;
  }
#endif

  if (NULL != (inistring = axisInifile->find("HOME", axisString))) {
    if (1 == sscanf(inistring, "%lf", &home)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] HOME: %s\n", axisString, inistring);
      }
      home = 0.0;                       // default
    }
  }
  else {
    // not found at all
    home = 0.0;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] HOME, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetHome(axis, home)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetHome\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("MAX_VELOCITY", axisString))) {
    if (1 == sscanf(inistring, "%lf", &maxVelocity)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MAX_VELOCITY: %s\n", axisString, inistring);
      }
      maxVelocity = DEFAULT_AXIS_MAX_VELOCITY;  // default
    }
  }
  else {
    // not found at all
    maxVelocity = DEFAULT_AXIS_MAX_VELOCITY;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MAX_VELOCITY, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetMaxVelocity(axis, maxVelocity)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMaxVelocity\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("MAX_ACCELERATION", axisString))) {
    if (1 == sscanf(inistring, "%lf", &maxAcceleration)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MAX_ACCELERATION: %s\n", axisString, inistring);
      }
      maxAcceleration = DEFAULT_AXIS_MAX_ACCELERATION;  // default
    }
  }
  else {
    // not found at all
    maxAcceleration = DEFAULT_AXIS_MAX_ACCELERATION;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MAX_ACCELERATION, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetMaxAcceleration(axis, maxAcceleration)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMaxAcceleration\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("ENABLE_POLARITY", axisString))) {
    if (1 == sscanf(inistring, "%d", &polarity)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] ENABLE_POLARITY: %s\n", axisString, inistring);
      }
      polarity = 1;                     // default for polarities
    }
  }
  else {
    // not found at all
    polarity = 1;                       // default for polarities
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] ENABLE_POLARITY, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetEnablePolarity(axis, polarity)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetEnablePolarity\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("MIN_LIMIT_SWITCH_POLARITY", axisString))) {
    if (1 == sscanf(inistring, "%d", &polarity)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MIN_LIMIT_SWITCH_POLARITY: %s\n", axisString, inistring);
      }
      polarity = 1;                     // default for polarities
    }
  }
  else {
    // not found at all
    polarity = 1;                       // default for polarities
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MIN_LIMIT_SWITCH_POLARITY, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetMinLimitSwitchPolarity(axis, polarity)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMinLimitSwitchPolarity\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("MAX_LIMIT_SWITCH_POLARITY", axisString))) {
    if (1 == sscanf(inistring, "%d", &polarity)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] MAX_LIMIT_SWITCH_POLARITY: %s\n", axisString, inistring);
      }
      polarity = 1;                     // default for polarities
    }
  }
  else {
    // not found at all
    polarity = 1;                       // default for polarities
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] MAX_LIMIT_SWITCH_POLARITY, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetMaxLimitSwitchPolarity(axis, polarity)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetMaxLimitSwitchPolarity\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("HOME_SWITCH_POLARITY", axisString))) {
    if (1 == sscanf(inistring, "%d", &polarity)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] HOME_SWITCH_POLARITY: %s\n", axisString, inistring);
      }
      polarity = 1;                     // default for polarities
    }
  }
  else {
    // not found at all
    polarity = 1;                       // default for polarities
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] HOME_SWITCH_POLARITY, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetHomeSwitchPolarity(axis, polarity)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetHomeSwitchPolarity\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("HOMING_POLARITY", axisString))) {
    if (1 == sscanf(inistring, "%d", &polarity)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] HOMING_POLARITY: %s\n", axisString, inistring);
      }
      polarity = 1;                     // default for polarities
    }
  }
  else {
    // not found at all
    polarity = 1;                       // default for polarities
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] HOMING_POLARITY, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetHomingPolarity(axis, polarity)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetHomingPolarity\n");
    }
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("HOME_OFFSET", axisString))) {
    if (1 == sscanf(inistring, "%lf", &offset)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] HOME_OFFSET: %s\n", axisString, inistring);
      }
      offset = 0.0;
    }
  }
  else {
    // not found at all
    offset = 0.0;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] HOME_OFFSET, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetHomeOffset(axis, offset)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetHomeOffset\n");
    }
    return -1;
  }

/* FIXME - more polarity stuff handled by HAL now */
#if 0

  if (NULL != (inistring = axisInifile->find("FAULT_POLARITY", axisString))) {
    if (1 == sscanf(inistring, "%d", &polarity)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [%s] FAULT_POLARITY: %s\n", axisString, inistring);
      }
      polarity = 1;                     // default for polarities
    }
  }
  else {
    // not found at all
    polarity = 1;                       // default for polarities
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [%s] FAULT_POLARITY, using default\n", axisString);
    }
  }
  if (0 != emcAxisSetFaultPolarity(axis, polarity)) {
    if (EMC_DEBUG & EMC_DEBUG_CONFIG) {
      rcs_print_error("bad return from emcAxisSetFaultPolarity\n");
    }
    return -1;
  }
#endif

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

  axisInifile = new INIFILE;
  if (-1 == axisInifile->open(filename)) {
    return -1;
  }

  if (NULL != (inistring = axisInifile->find("AXES", "TRAJ"))) {
    if (1 == sscanf(inistring, "%d", &axes)) {
      // found, and valid
    }
    else {
      // found, but invalid
      if (EMC_DEBUG & EMC_DEBUG_INVALID) {
        rcs_print_error("invalid inifile value for [TRAJ] AXES: %s\n", inistring);
      }
      axes = 0;
    }
  }
  else {
    // not found at all
    axes = 1;
    if (EMC_DEBUG & EMC_DEBUG_DEFAULTS) {
      rcs_print_error("can't find [TRAJ] AXES, using default %d\n", axes);
    }
  }

  if (axis < 0 ||
      axis >= axes) {
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


// FIXME-- begin temporary insert of ini file stuff

#define INIFILE_MIN_FLOAT_PRECISION 3
#define INIFILE_BACKUP_SUFFIX ".bak"

/* iniIsEntry(const char *line, char *var, char *val) returns 0 if the
   line is not interpretable as a variable-value ini file entry. If the
   line is interpretable as an ini file entry (i.e., it is of the form
   ``<some nonwhite> = {<value>}'' where <value> is optional), then the
   variable string is copied to 'var', and the value string is copied to
   'val'. Leading and trailing whitespace around 'var' and 'val' are
   omitted. */

static int iniIsEntry(const char *line, char *var, char *val)
{
  const char *ptr = line;
  char *varptr = var;
  char *valptr = val;
  char *lastwhite = NULL;
  int sawEqual = 0;

  // position ptr to first non-white on line
  while (isspace(*ptr)) {
    ptr++;
  }

  if (*ptr == 0 ||
      *ptr == '=') {
    // got to end or '=' before any non-white
    return 0;
  }

  // here's the first non-white, so begin copy to var
  while (*ptr != 0) {
    if (*ptr == '=') {
      sawEqual = 1;
      break;
    }

    // copy it, including any whitespace
    *varptr = *ptr;

    // mark the location of last whitespace, for nulling
    if (isspace(*ptr)) {
      if (lastwhite == NULL) {
        lastwhite = varptr;
      }
    }
    else {
      lastwhite = NULL;
    }

    varptr++;
    ptr++;
  }

  if (! sawEqual) {
    return 0;
  }

  // null-terminate at last whitespace, or end of string if none
  if (lastwhite != NULL) {
    *lastwhite = 0;
  }
  else {
    *varptr = 0;
  }

  // now move to value part

  lastwhite = NULL;
  ptr++;                        // step over '='

  // position ptr to first non-white on line
  while (isspace(*ptr)) {
    ptr++;
  }

  if (*ptr == 0) {
    // got to end before any non-white
    return 0;
  }

  // here's the first non-white, so begin copy to var
  while (*ptr != 0) {
    // copy it, including any whitespace
    *valptr = *ptr;

    // mark the location of last whitespace, for nulling
    if (isspace(*ptr)) {
      if (lastwhite == NULL) {
        lastwhite = valptr;
      }
    }
    else {
      lastwhite = NULL;
    }

    valptr++;
    ptr++;
  }

  // null-terminate at last whitespace, or end of string if none
  if (lastwhite != NULL) {
    *lastwhite = 0;
  }
  else {
    *valptr = 0;
  }

  return 1;
}

/* iniIsSection(const char *line, char *section) returns 0 if the line
   is not interpretable as a section. If the line is interpretable as a
   section (i.e., it has [ <string > ] as the only thing on it), then the
   stuff between the [], not including any leading or trailing
   whitespace, but including any intervening whitespace, is copied to
   'section'. */

static int iniIsSection(const char *line, char *section)
{
  const char *ptr = line;
  char *secptr = section;
  char *lastwhite = NULL;
  int sawClose = 0;

  // position ptr to first non-white on line
  while (isspace(*ptr)) {
    ptr++;
  }

  if (*ptr == 0) {
    // got to end before any non-white
    return 0;
  }

  if (*ptr != '[') {
    // not section format
    return 0;
  }

  // we're on [, so move to next non-white
  ptr++;
  while (isspace(*ptr)) {
    ptr++;
  }

  if (*ptr == 0) {
    // got to end before any non-white
    return 0;
  }

  // here's the first non-white after the [, so begin copy to string
  while (*ptr != 0) {
    if (*ptr == ']') {
      sawClose = 1;
      break;
    }

    // copy it, including any whitespace
    *secptr = *ptr;

    // mark the location of last whitespace, for nulling
    if (isspace(*ptr)) {
      if (lastwhite == NULL) {
        lastwhite = secptr;
      }
    }
    else {
      lastwhite = NULL;
    }

    secptr++;
    ptr++;
  }

  if (! sawClose) {
    // section had a close bracket
    return 0;
  }

  // null-terminate at last whitespace, or end of string if none
  if (lastwhite != NULL) {
    *lastwhite = 0;
  }
  else {
    *secptr = 0;
  }

  // section didn't have a close bracket
  return 1;
}

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
    if (! isdigit(*ptr)) {
      break;
    }
    // else it's a digit
    prec++;
    ptr++;
  }

  return prec > INIFILE_MIN_FLOAT_PRECISION ? prec : INIFILE_MIN_FLOAT_PRECISION;
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

  // FIXME-- should capture each one's float precision; right
  // now we're using the first as the precision for both
  prec = iniGetFloatPrec(val);
  sprintf(fmt, "%s = %%.%df %%.%df\n", var, prec, prec);

  return 0;
}

// end temporary insert of ini file stuff

/*
  dumpAxis(int axis, const char *filename, EMC_AXIS_STAT *status)

  Takes the file name for the ini file, makes a backup copy, opens the
  backup copy, then overwrites the original with a line-by-line version
  of the original, parsing the lines for ones it understands and
  replacing the values with the current ones.
 */
int dumpAxis(int axis, const char *filename, EMC_AXIS_STAT *status)
{
  char ourAxisSection[256];
  int ourAxis = 0;
  FILE *infp = NULL;
  FILE *outfp = NULL;
  char line[256];
  char section[256];
  char var[256], val[256];
  char fmt[256];
  struct stat ini_stat;

/* FIXME - stat() and chown() can disappear when we no longer need
   to run as root. */
printf ( "iniaxis: dumpAxis(%d, %s, %p)\n", axis, filename, status);

    stat(filename, &ini_stat);		// save the ownership details.

  // rename with backup suffix
  strcpy(line, filename);
  strcat(line, INIFILE_BACKUP_SUFFIX);
  if (0 != rename(filename, line)) {
    fprintf(stderr, "can't make backup copy of INI file %s\n", filename);
    return -1;
  }

  // open backup for reading
  if (NULL == (infp = fopen(line, "r"))) {
    fprintf(stderr, "can't open backup copy of INI file %s\n", line);
    return -1;
  }

  // open original for writing
  if (NULL == (outfp = fopen(filename, "w"))) {
    fprintf(stderr, "can't open original copy of INI file %s\n", line);
    return -1;
  }

  // set our axis string and flag that we're in that section
  sprintf(ourAxisSection, "AXIS_%d", axis);
  ourAxis = 0;

  while (!feof(infp)) {
    if (NULL == fgets(line, 256, infp)) {
      break;
    }

    if (iniIsSection(line, section)) {
      // if this is "AXIS_0,1,...", it's what we want
      if (!strcmp(section, ourAxisSection)) {
        ourAxis = 1;
      }
      else {
        ourAxis = 0;
      }
    }

    if (ourAxis) {
      if (iniIsEntry(line, var, val)) {
        if (!strcmp(var, "P")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->p);
          continue;             // avoid fputs() below, since we wrote it
        }
        else if (!strcmp(var, "I")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->i);
          continue;
        }
        else if (!strcmp(var, "D")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->d);
          continue;
        }
        else if (!strcmp(var, "FF0")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->ff0);
          continue;
        }
        else if (!strcmp(var, "FF1")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->ff1);
          continue;
        }
        else if (!strcmp(var, "FF2")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->ff2);
          continue;
        }
        else if (!strcmp(var, "BACKLASH")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->backlash);
          continue;
        }
        else if (!strcmp(var, "BIAS")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->bias);
          continue;
        }
        else if (!strcmp(var, "MAX_ERROR")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->maxError);
          continue;
        }
        else if (!strcmp(var, "DEADBAND")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->deadband);
          continue;
        }
        else if (!strcmp(var, "OUTPUT_SCALE")) {
          // val will be a string with two floats, e.g., "1.0 0.0",
          // and iniFormatFloat2() will set fmt to convert 2 floats
          // at the precision of the first in the string
          iniFormatFloat2(fmt, var, val);
          fprintf(outfp, fmt, status->outputScale, status->outputOffset);
          continue;
        }
        else if (!strcmp(var, "FERROR")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->maxFerror);
          continue;
        }
        else if (!strcmp(var, "MIN_FERROR")) {
          iniFormatFloat(fmt, var, val);
          fprintf(outfp, fmt, status->minFerror);
          continue;
        }
      }
    }

    // write it out
    fputs(line, outfp);
  }

  fclose(infp);
  fclose(outfp);

  /* Update the uid and gid of the new ini file - else it will end up
     being owned by root */
    chown(filename, ini_stat.st_uid, ini_stat.st_gid);

  return 0;
}
