/*
  minimilltaskintf.cc

  NIST minimill NML interface functions for motion and IO

  emcmot is not an NML_MODULE, so command and status info is
  manually manipulated. 'command_type' is only set when a command
  is sent to emcmot, likewise with 'echo_serial_number'.
  'status' is set to RCS_DONE, RCS_EXEC, and RCS_ERROR based on the
  corresponding emcmot status.

  Modification history:
  
  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

#include <string.h>             // strncpy()
#include <math.h>               // sqrt(), atan2()
#include <float.h>              // DBL_MAX
#include <stdlib.h>

#include "rcs.hh"               // RCS_CMD_CHANNEL, etc.
#include "nml_mod.hh"           // RCS_DONE
#include "emc.hh"               // EMC NML, Argc/Argv
#include "usrmotintf.h"         // usrmotInit(), usrmotReadEmcmotStatus(), etc.
#include "emcmot.h"             // EMCMOT_COMMAND,STATUS, etc.
#include "emcmotcfg.h"          // EMCMOT_ERROR_LEN
#include "canon.hh"             // CANON_PLANE
#include "posemath.h"           // PM_POSE
#include "initraj.hh"
#include "iniaxis.hh"
#include "inispin.hh"
#include "emcglb.h"             // EMC_INIFILE, SPINDLE_ENABLE_INDEX
#include "emcpos.h"		// EmcPose

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

// MOTION INTERFACE

/*
  Implementation notes:

  Initing:  the emcmot interface needs to be inited once, but nml_traj_init()
  and nml_servo_init() can be called in any order. Similarly, the emcmot
  interface needs to be exited once, but nml_traj_exit() and nml_servo_exit()
  can be called in any order. They can also be called multiple times. Flags
  are used to signify if initing has been done, or if the final exit has
  been called.
  */

// define this to catch isnan errors, for rtlinux FPU register problem testing
#include <stdio.h>              // printf()
#define ISNAN_TRAP

static EMCMOT_COMMAND emcmotCommand;

static int emcmotTrajInited = 0; // non-zero means traj called init
static int emcmotAxisInited = 0; // non-zero means axis called init
static int emcmotIoInited = 0;  // non-zero means io called init
static int emcmotion_initialized=0; // non-zero means both emcMotionInit called.


// saved value of velocity last sent out, so we don't send redundant requests
// used by emcTrajSetVelocity(), emcMotionAbort()
static double lastVel = -1.0;

// EMC_AXIS functions

// local status data, not provided by emcmot
static unsigned long localMotionHeartbeat = 0;
static int localMotionCommandType = 0;
static int localMotionEchoSerialNumber = 0;
static unsigned char localEmcAxisAxisType[EMCMOT_MAX_AXIS];
static double localEmcAxisUnits[EMCMOT_MAX_AXIS];
static double localEmcMaxAcceleration = DBL_MAX;

// axes are numbered 0..NUM-1

/*
  In emcmot, we need to set the cycle time for traj, and the interpolation
  rate, in any order, but both need to be done. The PID cycle time will
  be set by emcmot automatically.
 */

int emcAxisSetAxis(int axis, unsigned char axisType)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  localEmcAxisAxisType[axis] = axisType;

  return 0;
}

int emcAxisSetUnits(int axis, double units)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  localEmcAxisUnits[axis] = units;

  return 0;
}

int emcAxisSetGains(int axis, double p, double i, double d,
                    double ff0, double ff1, double ff2,
                    double backlash, double bias, double maxError,
                    double deadband)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_PID;
  emcmotCommand.axis = axis;

  emcmotCommand.pid.p = p;
  emcmotCommand.pid.i = i;
  emcmotCommand.pid.d = d;
  emcmotCommand.pid.ff0 = ff0;
  emcmotCommand.pid.ff1 = ff1;
  emcmotCommand.pid.ff2 = ff2;
  emcmotCommand.pid.backlash = backlash;
  emcmotCommand.pid.bias = bias;
  emcmotCommand.pid.maxError = maxError;
  emcmotCommand.pid.deadband = deadband;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.pid.p) ||
      isnan(emcmotCommand.pid.i) ||
      isnan(emcmotCommand.pid.d) ||
      isnan(emcmotCommand.pid.ff0) ||
      isnan(emcmotCommand.pid.ff1) ||
      isnan(emcmotCommand.pid.ff2) ||
      isnan(emcmotCommand.pid.backlash) ||
      isnan(emcmotCommand.pid.bias) ||
      isnan(emcmotCommand.pid.maxError) ||
      isnan(emcmotCommand.pid.deadband))
    {
      printf("isnan error in emcAxisSetGains\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetCycleTime(int axis, double cycleTime)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  if (cycleTime <= 0.0)
    {
      return -1;
    }

  emcmotCommand.command = EMCMOT_SET_SERVO_CYCLE_TIME;
  emcmotCommand.cycleTime = cycleTime;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetInputScale(int axis, double scale, double offset)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_INPUT_SCALE;
  emcmotCommand.axis = axis;
  emcmotCommand.scale = scale;
  emcmotCommand.offset = offset;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.scale) ||
      isnan(emcmotCommand.offset))
    {
      printf("isnan eror in emcAxisSetInputScale\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetOutputScale(int axis, double scale, double offset)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_OUTPUT_SCALE;
  emcmotCommand.axis = axis;
  emcmotCommand.scale = scale;
  emcmotCommand.offset = offset;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.scale) ||
      isnan(emcmotCommand.offset))
    {
      printf("isnan eror in emcAxisSetOutputScale\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

// saved values of limits, since emcmot expects them to be set in
// pairs and we set them individually.
static double saveMinLimit[EMCMOT_MAX_AXIS];
static double saveMaxLimit[EMCMOT_MAX_AXIS];

int emcAxisSetMinPositionLimit(int axis, double limit)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_POSITION_LIMITS;
  emcmotCommand.axis = axis;
  emcmotCommand.maxLimit = saveMaxLimit[axis];
  emcmotCommand.minLimit = limit;
  saveMinLimit[axis] = limit;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.maxLimit) ||
      isnan(emcmotCommand.minLimit))
    {
      printf("isnan error in emcAxisSetMinPosition\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMaxPositionLimit(int axis, double limit)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_POSITION_LIMITS;
  emcmotCommand.axis = axis;
  emcmotCommand.minLimit = saveMinLimit[axis];
  emcmotCommand.maxLimit = limit;
  saveMaxLimit[axis] = limit;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.maxLimit) ||
      isnan(emcmotCommand.minLimit))
    {
      printf("isnan error in emcAxisSetMaxPosition\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

// saved values of limits, since emcmot expects them to be set in
// pairs and we set them individually.
static double saveMinOutput[EMCMOT_MAX_AXIS];
static double saveMaxOutput[EMCMOT_MAX_AXIS];

int emcAxisSetMinOutputLimit(int axis, double limit)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_OUTPUT_LIMITS;
  emcmotCommand.axis = axis;
  emcmotCommand.maxLimit = saveMaxOutput[axis];
  emcmotCommand.minLimit = limit;
  saveMinOutput[axis] = limit;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.maxLimit) ||
      isnan(emcmotCommand.minLimit))
    {
      printf("isnan error in emcAxisSetMinOutputLimit\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMaxOutputLimit(int axis, double limit)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_OUTPUT_LIMITS;
  emcmotCommand.axis = axis;
  emcmotCommand.minLimit = saveMinOutput[axis];
  emcmotCommand.maxLimit = limit;
  saveMaxOutput[axis] = limit;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.maxLimit) ||
      isnan(emcmotCommand.minLimit))
    {
      printf("isnan error in emcAxisSetMaxOutputLimit\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetFerror(int axis, double ferror)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_MAX_FERROR;
  emcmotCommand.axis = axis;
  emcmotCommand.maxFerror = ferror;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.maxFerror))
    {
      printf("isnan error in emcAxisSetFerror\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMinFerror(int axis, double ferror)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_MIN_FERROR;
  emcmotCommand.axis = axis;
  emcmotCommand.minFerror = ferror;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.minFerror))
    {
      printf("isnan error in emcAxisSetMinFerror\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetHomingVel(int axis, double homingVel)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_HOMING_VEL;
  emcmotCommand.axis = axis;
  emcmotCommand.vel = homingVel;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.vel))
    {
      printf("isnan error in emcAxisSetHomingVel\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetStepParams(int axis, double setup_time, double hold_time)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
  {
    return 0;
  }

  emcmotCommand.command = EMCMOT_SET_STEP_PARAMS;
  emcmotCommand.axis = axis;
  emcmotCommand.setup_time = setup_time;
  emcmotCommand.hold_time = hold_time;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMaxVelocity(int axis, double vel)
{
  if (axis < 0 || axis >= EMC_AXIS_MAX)
    {
      return 0;
    }

  if (vel < 0.0)
    {
      vel = 0.0;
    }

  AXIS_MAX_VELOCITY[axis] = vel;

  emcmotCommand.command = EMCMOT_SET_AXIS_VEL_LIMIT;
  emcmotCommand.axis = axis;
  emcmotCommand.vel = vel;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetHomeOffset(int axis, double offset)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_HOME_OFFSET;
  emcmotCommand.axis = axis;
  emcmotCommand.offset = offset;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.offset))
    {
      printf("isnan error in emcAxisSetHomeOffset\n");
      return -1;
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetEnablePolarity(int axis, int level)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_POLARITY;
  emcmotCommand.axis = axis;
  emcmotCommand.level = level;
  emcmotCommand.axisFlag = EMCMOT_AXIS_ENABLE_BIT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMinLimitSwitchPolarity(int axis, int level)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_POLARITY;
  emcmotCommand.axis = axis;
  emcmotCommand.level = level;
  emcmotCommand.axisFlag = EMCMOT_AXIS_MIN_HARD_LIMIT_BIT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetMaxLimitSwitchPolarity(int axis, int level)
{
  if (axis < 0)
    {
      axis = 0;
    }
  else if (axis >= EMCMOT_MAX_AXIS)
    {
      axis = EMCMOT_MAX_AXIS - 1;
    }

  emcmotCommand.command = EMCMOT_SET_POLARITY;
  emcmotCommand.axis = axis;
  emcmotCommand.level = level;
  emcmotCommand.axisFlag = EMCMOT_AXIS_MAX_HARD_LIMIT_BIT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetHomeSwitchPolarity(int axis, int level)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_POLARITY;
  emcmotCommand.axis = axis;
  emcmotCommand.level = level;
  emcmotCommand.axisFlag = EMCMOT_AXIS_HOME_SWITCH_BIT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetHomingPolarity(int axis, int level)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_POLARITY;
  emcmotCommand.axis = axis;
  emcmotCommand.level = level;
  emcmotCommand.axisFlag = EMCMOT_AXIS_HOMING_BIT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetFaultPolarity(int axis, int level)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_SET_POLARITY;
  emcmotCommand.axis = axis;
  emcmotCommand.level = level;
  emcmotCommand.axisFlag = EMCMOT_AXIS_FAULT_BIT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisInit(int axis)
{
  int retval = 0;

  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  // init emcmot interface
  if (! emcmotAxisInited &&
      ! emcmotTrajInited &&
      ! emcmotIoInited)
    {
      usrmotIniLoad(EMC_INIFILE);

      if (0 != usrmotInit())
        {
          return -1;
        }
    }
  emcmotAxisInited = 1;

  if (0 != iniAxis(axis, EMC_INIFILE))
    {
      retval = -1;
    }

  return retval;
}

int emcAxisHalt(int axis)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  // FIXME-- refs global emcStatus; should make EMC_AXIS_STAT an arg here
  if(NULL != emcStatus && emcmotion_initialized && emcmotAxisInited)
    {
      dumpAxis(axis, EMC_INIFILE, &emcStatus->motion.axis[axis]);
    }

  if (! emcmotTrajInited &&     // traj is gone
      ! emcmotIoInited &&       // io is gone
      emcmotAxisInited)         // but we're still here
    {
      usrmotExit();             // but now we're gone
    }

  // mark us as not needing in any case
  emcmotAxisInited = 0;

  return 0;
}

int emcAxisAbort(int axis)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_ABORT;
  emcmotCommand.axis = axis;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisActivate(int axis)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_ACTIVATE_AXIS;
  emcmotCommand.axis = axis;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisDeactivate(int axis)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_DEACTIVATE_AXIS;
  emcmotCommand.axis = axis;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisOverrideLimits(int axis)
{
  // can have axis < 0, for resuming normal limit checking
  if (axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_OVERRIDE_LIMITS;
  emcmotCommand.axis = axis;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetOutput(int axis, double output)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_DAC_OUT;
  emcmotCommand.axis = axis;
  emcmotCommand.dacOut = output;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisEnable(int axis)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_ENABLE_AMPLIFIER;
  emcmotCommand.axis = axis;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisDisable(int axis)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_DISABLE_AMPLIFIER;
  emcmotCommand.axis = axis;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisHome(int axis)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_HOME;
  emcmotCommand.axis = axis;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisSetHome(int axis, double home)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  emcmotCommand.command = EMCMOT_SET_JOINT_HOME;
  emcmotCommand.axis = axis;
  emcmotCommand.offset = home;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisJog(int axis, double vel)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  if (vel > AXIS_MAX_VELOCITY[axis]) {
    vel = AXIS_MAX_VELOCITY[axis];
  }
  else if (vel < -AXIS_MAX_VELOCITY[axis]) {
    vel = -AXIS_MAX_VELOCITY[axis];
  }

  emcmotCommand.command = EMCMOT_JOG_CONT;
  emcmotCommand.axis = axis;
  emcmotCommand.vel = vel;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisIncrJog(int axis, double incr, double vel)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  if (vel > AXIS_MAX_VELOCITY[axis]) {
    vel = AXIS_MAX_VELOCITY[axis];
  }
  else if (vel < -AXIS_MAX_VELOCITY[axis]) {
    vel = -AXIS_MAX_VELOCITY[axis];
  }

  emcmotCommand.command = EMCMOT_JOG_INCR;
  emcmotCommand.axis = axis;
  emcmotCommand.vel = vel;
  emcmotCommand.offset = incr;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisAbsJog(int axis, double pos, double vel)
{
  if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
    return 0;
  }

  if (vel > AXIS_MAX_VELOCITY[axis]) {
    vel = AXIS_MAX_VELOCITY[axis];
  }
  else if (vel < -AXIS_MAX_VELOCITY[axis]) {
    vel = -AXIS_MAX_VELOCITY[axis];
  }

  emcmotCommand.command = EMCMOT_JOG_ABS;
  emcmotCommand.axis = axis;
  emcmotCommand.vel = vel;
  emcmotCommand.offset = pos;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcAxisLoadComp(int axis, const char * file)
{
  return usrmotLoadComp(axis, file);
}

int emcAxisAlter(int axis, double alter)
{
  return usrmotAlter(axis, alter);
}

static EMCMOT_CONFIG emcmotConfig;
int get_emcmot_debug_info = 0;

/*
  these globals are set in emcMotionUpdate(), then referenced in
  emcAxisUpdate(), emcTrajUpdate() to save calls to usrmotReadEmcmotStatus
 */
static EMCMOT_STATUS emcmotStatus;
static EMCMOT_DEBUG emcmotDebug;
static char errorString[EMCMOT_ERROR_LEN];
static int new_config = 0;

int emcAxisUpdate(EMC_AXIS_STAT stat[], int numAxes)
{
  int axis;

  // check for valid range
  if (numAxes <= 0 || numAxes > EMCMOT_MAX_AXIS) {
    return -1;
  }

  for (axis = 0; axis < numAxes; axis++) {
    stat[axis].axisType = localEmcAxisAxisType[axis];
    stat[axis].units = localEmcAxisUnits[axis];

    stat[axis].inputScale = emcmotStatus.inputScale[axis];
    stat[axis].inputOffset = emcmotStatus.inputOffset[axis];
    stat[axis].outputScale = emcmotStatus.outputScale[axis];
    stat[axis].outputOffset = emcmotStatus.outputOffset[axis];

    if (new_config) {
      stat[axis].p = emcmotConfig.pid[axis].p;
      stat[axis].i = emcmotConfig.pid[axis].i;
      stat[axis].d = emcmotConfig.pid[axis].d;
      stat[axis].ff0 = emcmotConfig.pid[axis].ff0;
      stat[axis].ff1 = emcmotConfig.pid[axis].ff1;
      stat[axis].ff2 = emcmotConfig.pid[axis].ff2;
      stat[axis].backlash = emcmotConfig.pid[axis].backlash;
      stat[axis].bias = emcmotConfig.pid[axis].bias;
      stat[axis].maxError = emcmotConfig.pid[axis].maxError;
      stat[axis].deadband = emcmotConfig.pid[axis].deadband;
      stat[axis].cycleTime = emcmotConfig.servoCycleTime;
      stat[axis].minPositionLimit = emcmotConfig.minLimit[axis];
      stat[axis].maxPositionLimit = emcmotConfig.maxLimit[axis];
      stat[axis].minOutputLimit = emcmotConfig.minOutput[axis];
      stat[axis].maxOutputLimit = emcmotConfig.maxOutput[axis];
      stat[axis].minFerror = emcmotConfig.minFerror[axis];
      stat[axis].maxFerror = emcmotConfig.maxFerror[axis];
      stat[axis].homeOffset = emcmotConfig.homeOffset[axis];
      stat[axis].enablePolarity = (emcmotConfig.axisPolarity[axis] &
				   EMCMOT_AXIS_ENABLE_BIT) ? 1 : 0;
      stat[axis].minLimitSwitchPolarity = (emcmotConfig.axisPolarity[axis] &
					   EMCMOT_AXIS_MIN_HARD_LIMIT_BIT) ? 1 : 0;
      stat[axis].maxLimitSwitchPolarity = (emcmotConfig.axisPolarity[axis] &
					   EMCMOT_AXIS_MAX_HARD_LIMIT_BIT) ? 1 : 0;
      stat[axis].homeSwitchPolarity = (emcmotConfig.axisPolarity[axis] &
				       EMCMOT_AXIS_HOME_SWITCH_BIT) ? 1 : 0;
      stat[axis].homingPolarity = (emcmotConfig.axisPolarity[axis] &
				   EMCMOT_AXIS_HOMING_BIT) ? 1 : 0;
      stat[axis].faultPolarity = (emcmotConfig.axisPolarity[axis] &
				  EMCMOT_AXIS_FAULT_BIT) ? 1 : 0;
    }

    stat[axis].setpoint = emcmotStatus.axisPos[axis];
     
    if (get_emcmot_debug_info) {
      stat[axis].ferrorCurrent = emcmotDebug.ferrorCurrent[axis];
      stat[axis].ferrorHighMark = emcmotDebug.ferrorHighMark[axis];
    }
      
    stat[axis].output = emcmotStatus.output[axis];
    stat[axis].input = emcmotStatus.input[axis];
    stat[axis].homing = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_HOMING_BIT ? 1 : 0);
    stat[axis].homed = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_HOMED_BIT ? 1 : 0);
    stat[axis].fault = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_FAULT_BIT ? 1 : 0);
    stat[axis].enabled = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0);

    stat[axis].minSoftLimit = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT ? 1 : 0);
    stat[axis].maxSoftLimit = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT ? 1 : 0);
    stat[axis].minHardLimit = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT ? 1 : 0);
    stat[axis].maxHardLimit = (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT ? 1 : 0);
    stat[axis].overrideLimits = (emcmotStatus.overrideLimits); // one for all

    stat[axis].scale = emcmotStatus.axVscale[axis];
    usrmotQueryAlter(axis, &stat[axis].alter);

    if (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_ERROR_BIT) {
      if (stat[axis].status != RCS_ERROR) {
	rcs_print_error("Error on axis %d.\n",axis);
	stat[axis].status = RCS_ERROR;
      }
    }
    else if (emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_INPOS_BIT) {
      stat[axis].status = RCS_DONE;
    }
    else {
      stat[axis].status = RCS_EXEC;
    }
  }

  return 0;
}

// EMC_TRAJ functions

// local status data, not provided by emcmot
static int localEmcTrajAxes = 0;
static double localEmcTrajLinearUnits = 1.0;
static double localEmcTrajAngularUnits = 1.0;
static int localEmcTrajMotionId = 0;

int emcTrajSetAxes(int axes)
{
  if (axes <= 0 || axes > EMCMOT_MAX_AXIS)
    {
      return -1;
    }

  localEmcTrajAxes = axes;
  emcmotCommand.command = EMCMOT_SET_NUM_AXES;
  emcmotCommand.axis = axes;
  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetUnits(double linearUnits, double angularUnits)
{
  if (linearUnits <= 0.0 ||
      angularUnits <= 0.0)
    {
      return -1;
    }

  localEmcTrajLinearUnits = linearUnits;
  localEmcTrajAngularUnits = angularUnits;

  return 0;
}

int emcTrajSetCycleTime(double cycleTime)
{
  if (cycleTime <= 0.0)
    {
      return -1;
    }

  emcmotCommand.command = EMCMOT_SET_TRAJ_CYCLE_TIME;
  emcmotCommand.cycleTime = cycleTime;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetMode(int mode)
{
  switch (mode)
    {
    case EMC_TRAJ_MODE_FREE:
      emcmotCommand.command = EMCMOT_FREE;
      return usrmotWriteEmcmotCommand(&emcmotCommand);

    case EMC_TRAJ_MODE_COORD:
      emcmotCommand.command = EMCMOT_COORD;
      return usrmotWriteEmcmotCommand(&emcmotCommand);

    case EMC_TRAJ_MODE_TELEOP:
      emcmotCommand.command = EMCMOT_TELEOP;
      return usrmotWriteEmcmotCommand(&emcmotCommand);

    default:
      return -1;
    }
}


int emcTrajSetTeleopVector(EmcPose vel)
{
  emcmotCommand.command = EMCMOT_SET_TELEOP_VECTOR;
  emcmotCommand.pos = vel;
  return usrmotWriteEmcmotCommand(&emcmotCommand);
}


int emcTrajSetVelocity(double vel)
{
  int retval;

  if (vel < 0.0)
    {
      vel = 0.0;
    }
  else if (vel > TRAJ_MAX_VELOCITY)
    {
      vel = TRAJ_MAX_VELOCITY;
    }

#if 0
  // FIXME-- this fixes rapid rate reset problem
if (vel == lastVel)
    {
      // suppress it
      return 0;
    }
#endif

  emcmotCommand.command = EMCMOT_SET_VEL;
  emcmotCommand.vel = vel;

  retval = usrmotWriteEmcmotCommand(&emcmotCommand);

  if (0 == retval)
    {
      lastVel = vel;
    }

  return retval;
}

int emcTrajSetAcceleration(double acc)
{
  if (acc < 0.0)
    {
      acc = 0.0;
    }
  else if (acc > localEmcMaxAcceleration)
    {
      acc = localEmcMaxAcceleration;
    }

  emcmotCommand.command = EMCMOT_SET_ACC;
  emcmotCommand.acc = acc;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

/*
  emcmot has no limits on max velocity, acceleration so we'll save them
  here and apply them in the functions above
  */
int emcTrajSetMaxVelocity(double vel)
{
  if (vel < 0.0)
    {
      vel = 0.0;
    }

  TRAJ_MAX_VELOCITY = vel;

  emcmotCommand.command = EMCMOT_SET_VEL_LIMIT;
  emcmotCommand.vel = vel;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetMaxAcceleration(double acc)
{
  if (acc < 0.0)
    {
      acc = 0.0;
    }

  localEmcMaxAcceleration = acc;

  return 0;
}

int emcTrajSetHome(EmcPose home)
{
  emcmotCommand.command = EMCMOT_SET_WORLD_HOME;

  emcmotCommand.pos = home;


#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.pos.tran.x) ||
      isnan(emcmotCommand.pos.tran.y) ||
      isnan(emcmotCommand.pos.tran.z))
    {
      printf("isnan error in emcTrajSetHome\n");
      return 0;                 // ignore it for now, just don't send it
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetScale(double scale)
{
  if (scale < 0.0)
    {
      scale = 0.0;
    }

  emcmotCommand.command = EMCMOT_SCALE;
  emcmotCommand.scale = scale;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetMotionId(int id)
{

  if(EMC_DEBUG_MOTION_TIME & EMC_DEBUG)
    {
      if(id != localEmcTrajMotionId)
	{
	  rcs_print("Outgoing motion id is %d.\n",id);
	}
    }

  localEmcTrajMotionId = id;

  return 0;
}

int emcTrajInit()
{
  int retval = 0;

  // init emcmot interface
  if (! emcmotAxisInited &&
      ! emcmotTrajInited &&
      ! emcmotIoInited)
    {
      usrmotIniLoad(EMC_INIFILE);

      if (0 != usrmotInit())
        {
          return -1;
        }
    }
  emcmotTrajInited = 1;

  // initialize parameters from ini file
  if (0 != iniTraj(EMC_INIFILE))
    {
      retval = -1;
    }

  return retval;
}

int emcTrajHalt()
{
  if (! emcmotAxisInited &&     // axes are gone
      ! emcmotIoInited &&       // io is gone
      emcmotTrajInited)         // but we're still here
    {
      usrmotExit();             // but now we're gone
    }

  // mark us as not needing in any case
  emcmotTrajInited = 0;

  return 0;
}

int emcTrajEnable()
{
  emcmotCommand.command = EMCMOT_ENABLE;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajDisable()
{
  emcmotCommand.command = EMCMOT_DISABLE;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajAbort()
{
  emcmotCommand.command = EMCMOT_ABORT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajPause()
{
  emcmotCommand.command = EMCMOT_PAUSE;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajStep()
{
  emcmotCommand.command = EMCMOT_STEP;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajResume()
{
  emcmotCommand.command = EMCMOT_RESUME;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajDelay(double delay)
{
  // nothing need be done here-- it's done in task controller

  return 0;
}

int emcTrajSetTermCond(int cond)
{
  emcmotCommand.command = EMCMOT_SET_TERM_COND;
  emcmotCommand.termCond = (cond == EMC_TRAJ_TERM_COND_STOP ? EMCMOT_TERM_COND_STOP : EMCMOT_TERM_COND_BLEND);

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajLinearMove(EmcPose end)
{
  emcmotCommand.command = EMCMOT_SET_LINE;

  emcmotCommand.pos = end;

  emcmotCommand.id = localEmcTrajMotionId;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.pos.tran.x) ||
      isnan(emcmotCommand.pos.tran.y) ||
      isnan(emcmotCommand.pos.tran.z))
    {
      printf("isnan error in emcTrajLinearMove\n");
      return 0;                 // ignore it for now, just don't send it
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajCircularMove(EmcPose end, PM_CARTESIAN center,
                        PM_CARTESIAN normal, int turn)
{
  emcmotCommand.command = EMCMOT_SET_CIRCLE;

  emcmotCommand.pos = end;

  emcmotCommand.center.x = center.x;
  emcmotCommand.center.y = center.y;
  emcmotCommand.center.z = center.z;

  emcmotCommand.normal.x = normal.x;
  emcmotCommand.normal.y = normal.y;
  emcmotCommand.normal.z = normal.z;

  emcmotCommand.turn = turn;
  emcmotCommand.id = localEmcTrajMotionId;

#ifdef ISNAN_TRAP
  if (isnan(emcmotCommand.pos.tran.x) ||
      isnan(emcmotCommand.pos.tran.y) ||
      isnan(emcmotCommand.pos.tran.z) ||
      isnan(emcmotCommand.pos.a) ||
      isnan(emcmotCommand.pos.b) ||
      isnan(emcmotCommand.pos.c) ||
      isnan(emcmotCommand.center.x) ||
      isnan(emcmotCommand.center.y) ||
      isnan(emcmotCommand.center.z) ||
      isnan(emcmotCommand.normal.x) ||
      isnan(emcmotCommand.normal.y) ||
      isnan(emcmotCommand.normal.z))
    {
      printf("isnan error in emcTrajCircularMove\n");
      return 0;                 // ignore it for now, just don't send it
    }
#endif

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetProbeIndex(int index)
{
  emcmotCommand.command = EMCMOT_SET_PROBE_INDEX;
  emcmotCommand.probeIndex = index;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajSetProbePolarity(int polarity)
{
  emcmotCommand.command = EMCMOT_SET_PROBE_POLARITY;
  emcmotCommand.level = polarity;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajClearProbeTrippedFlag()
{
  emcmotCommand.command = EMCMOT_CLEAR_PROBE_FLAGS;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcTrajProbe(EmcPose pos)
{
  emcmotCommand.command = EMCMOT_PROBE;

  emcmotCommand.pos.tran.x = pos.tran.x;
  emcmotCommand.pos.tran.y = pos.tran.y;
  emcmotCommand.pos.tran.z = pos.tran.z;
  emcmotCommand.id = localEmcTrajMotionId;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

static int last_id=0;
static int last_id_printed=0;
static int last_status=0;
static double last_id_time;

int emcTrajUpdate(EMC_TRAJ_STAT *stat)
{
  int axis;

  stat->axes = localEmcTrajAxes;
  stat->linearUnits = localEmcTrajLinearUnits;
  stat->angularUnits = localEmcTrajAngularUnits;

  stat->mode = 
    emcmotStatus.motionFlag & EMCMOT_MOTION_TELEOP_BIT ? EMC_TRAJ_MODE_TELEOP :
    ( emcmotStatus.motionFlag & EMCMOT_MOTION_COORD_BIT ? EMC_TRAJ_MODE_COORD : 
      EMC_TRAJ_MODE_FREE ) ;

  // enabled iff motion enabled and all axes enabled
  stat->enabled = 0;            // start at disabled
  if (emcmotStatus.motionFlag & EMCMOT_MOTION_ENABLE_BIT) {
    for (axis = 0; axis < localEmcTrajAxes; axis++) {
      if (! emcmotStatus.axisFlag[axis] & EMCMOT_AXIS_ENABLE_BIT) {
	break;
      }

      // got here, then all are enabled
      stat->enabled = 1;
    }
  }

  stat->inpos = emcmotStatus.motionFlag & EMCMOT_MOTION_INPOS_BIT ? 1 : 0;
  stat->queue = emcmotStatus.depth;
  stat->activeQueue = emcmotStatus.activeDepth;
  stat->queueFull = emcmotStatus.queueFull;
  stat->id = emcmotStatus.id;
  if (EMC_DEBUG_MOTION_TIME & EMC_DEBUG) {
    if (stat->id != last_id) {
      if (last_id != last_id_printed) {
	rcs_print("Motion id %d took %f seconds.\n",last_id, etime()-last_id_time);
	last_id_printed = last_id;
      }
      last_id = stat->id;
      last_id_time = etime();
    }
  }

  stat->paused = emcmotStatus.paused;
  stat->scale = emcmotStatus.qVscale;

  stat->position = emcmotStatus.pos;

  stat->actualPosition = emcmotStatus.actualPos;

  stat->velocity = emcmotStatus.vel;
  stat->acceleration = emcmotStatus.acc;
  stat->maxAcceleration = localEmcMaxAcceleration;

  if (emcmotStatus.motionFlag & EMCMOT_MOTION_ERROR_BIT) {
    stat->status = RCS_ERROR;
  }
  else if (stat->inpos &&
           (stat->queue == 0)) {
    stat->status = RCS_DONE;
  }
  else {
    stat->status = RCS_EXEC;
  }

  if (EMC_DEBUG_MOTION_TIME & EMC_DEBUG) {
    if(stat->status == RCS_DONE && last_status != RCS_DONE && stat->id != last_id_printed) {
      rcs_print("Motion id %d took %f seconds.\n",last_id, etime()-last_id_time);
      last_id_printed = last_id = stat->id;
      last_id_time = etime();
    }
  }

  // update logging
  if (emcmotStatus.logOpen) {
    // we're logging motion
    emcStatus->logType = emcmotStatus.logType;
    emcStatus->logSize = emcmotStatus.logSize;
    emcStatus->logSkip = emcmotStatus.logSkip;
    emcStatus->logOpen = emcmotStatus.logOpen;
    emcStatus->logStarted = emcmotStatus.logStarted;
    emcStatus->logPoints = emcmotStatus.logPoints;
  }
  // else if it's not open, don't update emcStatus-- someone else may
  // be logging

  stat->probedPosition.tran.x = emcmotStatus.probedPos.tran.x;
  stat->probedPosition.tran.y = emcmotStatus.probedPos.tran.y;
  stat->probedPosition.tran.z = emcmotStatus.probedPos.tran.z;
  stat->probeval = emcmotStatus.probeval;
  stat->probe_tripped = emcmotStatus.probeTripped;

  if (new_config) {
    stat->cycleTime = emcmotConfig.trajCycleTime;
    stat->probe_index = emcmotConfig.probeIndex;
    stat->probe_polarity = emcmotConfig.probePolarity;
    stat->kinematics_type = emcmotConfig.kinematics_type;
    stat->maxVelocity = emcmotConfig.limitVel;
  }

  return 0;
}

// EMC_MOTION functions
int emcMotionInit()
{
  int r1;
  int r2;
  int axis;

  r1 = -1;
  for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++)
    {
      if (0 == emcAxisInit(axis))
        {
          r1 = 0;               // at least one is okay
        }
    }

  r2 = emcTrajInit();

  if(r1 == 0 && r2 == 0)
    {
      emcmotion_initialized=1;
    }

  return (r1 == 0 &&
          r2 == 0) ? 0 : -1;
}

int emcMotionHalt()
{
  int r1;
  int r2;
  int t;

  r1 = -1;
  for (t = 0; t < EMCMOT_MAX_AXIS; t++)
    {
      if (0 == emcAxisHalt(t))
        {
          r1 = 0;               // at least one is okay
        }
    }

  r2 = emcTrajHalt();

  emcmotion_initialized=0;

  return (r1 == 0 &&
          r2 == 0) ? 0 : -1;
}

int emcMotionAbort()
{
  int r1;
  int r2;
  int t;

  r1 = -1;
  for (t = 0; t < EMCMOT_MAX_AXIS; t++)
    {
      if (0 == emcAxisAbort(t))
        {
          r1 = 0;               // at least one is okay
        }
    }

  r2 = emcTrajAbort();

  // reset optimization flag which suppresses duplicate speed requests
  lastVel = -1.0;

  return (r1 == 0 &&
          r2 == 0) ? 0 : -1;
}

int emcMotionSetDebug(int debug)
{
  emcmotCommand.command = EMCMOT_ABORT;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcMotionSetAout(unsigned char index, double start, double end)
{
  emcmotCommand.command = EMCMOT_SET_AOUT;
  /* FIXME-- if this works, set up some dedicated cmd fields instead of
     borrowing these */
  emcmotCommand.out = index;
  emcmotCommand.minLimit = start;
  emcmotCommand.maxLimit = end;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcMotionSetDout(unsigned char index, unsigned char start, unsigned char end)
{
  emcmotCommand.command = EMCMOT_SET_DOUT;
  emcmotCommand.out = index;
  emcmotCommand.start = start;
  emcmotCommand.end = end;

  return usrmotWriteEmcmotCommand(&emcmotCommand);
}

int emcMotionUpdate(EMC_MOTION_STAT *stat)
{
  int r1;
  int r2;
  int axis;
  int error;
  int exec;

  // read the emcmot status
  if (0 != usrmotReadEmcmotStatus(&emcmotStatus)) {
    return -1;
  }

  new_config = 0;
  if (emcmotStatus.config_num != emcmotConfig.config_num) {
    if (0 != usrmotReadEmcmotConfig(&emcmotConfig)) {
      return -1;
    }
    new_config = 1;
  }

  if (get_emcmot_debug_info) {
    if (0 != usrmotReadEmcmotDebug(&emcmotDebug)) {
      return -1;
    }
  }

  // read the emcmot error
  if (0 != usrmotReadEmcmotError(errorString)) {
    // no error, so ignore
  }
  else {
    // an error to report
    emcOperatorError(0, errorString);
  }

  // save the heartbeat and command number locally,
  // for use with emcMotionUpdate
  localMotionHeartbeat = emcmotStatus.heartbeat;
  localMotionCommandType = emcmotStatus.commandEcho; // FIXME-- not NML one!
  localMotionEchoSerialNumber = emcmotStatus.commandNumEcho;

  r1 = emcAxisUpdate(&stat->axis[0], EMCMOT_MAX_AXIS);
  r2 = emcTrajUpdate(&stat->traj);
  stat->heartbeat = localMotionHeartbeat;
  stat->command_type = localMotionCommandType;
  stat->echo_serial_number = localMotionEchoSerialNumber;
  stat->debug = emcmotConfig.debug;

  // set the status flag
  error = 0;
  exec = 0;

  for (axis = 0; axis < stat->traj.axes; axis++) {
    if (stat->axis[axis].status == RCS_ERROR) {
      error = 1;
      break;
    }
    if (stat->axis[axis].status == RCS_EXEC) {
      exec = 1;
      break;
    }
  }

  if (stat->traj.status == RCS_ERROR) {
    error = 1;
  }
  else if (stat->traj.status == RCS_EXEC) {
    exec = 1;
  }

  if (error) {
    stat->status = RCS_ERROR;
  }
  else if (exec) {
    stat->status = RCS_EXEC;
  }
  else {
    stat->status = RCS_DONE;
  }

  return (r1 == 0 &&
          r2 == 0) ? 0 : -1;
}

// IO INTERFACE

// the NML channels to the EMCIO controller
static RCS_CMD_CHANNEL *emcIoCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcIoStatusBuffer = 0;

// global status structure
EMC_IO_STAT *emcIoStatus = 0;

// serial number for communication
static int emcIoCommandSerialNumber = 0;
static double EMCIO_BUFFER_GET_TIMEOUT=5.0;

static int emcioNmlGet()
{
  int retval = 0;
  double start_time;
  RCS_PRINT_DESTINATION_TYPE orig_dest;
  if (emcIoCommandBuffer == 0)
    {
      orig_dest = get_rcs_print_destination();
      set_rcs_print_destination(RCS_PRINT_TO_NULL);
      start_time = etime();
      while(start_time-etime() < EMCIO_BUFFER_GET_TIMEOUT)
	{
	  emcIoCommandBuffer = new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc", EMC_NMLFILE);
	  if (! emcIoCommandBuffer->valid())
	    {
	      delete emcIoCommandBuffer;
	      emcIoCommandBuffer = 0;
	    }
	  else
	    {
	      break;
	    }
	  esleep(0.1);
	}
      set_rcs_print_destination(orig_dest);
    }

  if (emcIoCommandBuffer == 0)
    {
      emcIoCommandBuffer = new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc", EMC_NMLFILE);
      if (! emcIoCommandBuffer->valid())
	{
	  delete emcIoCommandBuffer;
	  emcIoCommandBuffer = 0;
	  retval=-1;
	}
    }
  
  if (emcIoStatusBuffer == 0)
    {
      orig_dest = get_rcs_print_destination();
      set_rcs_print_destination(RCS_PRINT_TO_NULL);
      start_time = etime();
      while(start_time-etime() < EMCIO_BUFFER_GET_TIMEOUT)
	{
	  emcIoStatusBuffer = new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emc", EMC_NMLFILE);
	  if (! emcIoStatusBuffer->valid())
	    {
	      delete emcIoStatusBuffer;
	      emcIoStatusBuffer = 0;
	    }
	  else
	    {
	      emcIoStatus = (EMC_IO_STAT *) emcIoStatusBuffer->get_address();
	      // capture serial number for next send
	      emcIoCommandSerialNumber = emcIoStatus->echo_serial_number;
	      break;
	    }
	  esleep(0.1);
	}
      set_rcs_print_destination(orig_dest);
    }

  if (emcIoStatusBuffer == 0)
    {
      emcIoStatusBuffer = new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emc", EMC_NMLFILE);
      if (! emcIoStatusBuffer->valid() ||
	  EMC_IO_STAT_TYPE != emcIoStatusBuffer->peek())
	{
	  delete emcIoStatusBuffer;
	  emcIoStatusBuffer = 0;
	  emcIoStatus = 0;
	  retval = -1;
	}
      else
	{
	  emcIoStatus = (EMC_IO_STAT *) emcIoStatusBuffer->get_address();
	  // capture serial number for next send
	  emcIoCommandSerialNumber = emcIoStatus->echo_serial_number;
	}
    }

  return retval;
}

static RCS_CMD_MSG *last_io_command =0;

/*
  sendCommand() waits until any currently executing command has finished,
  then writes the given command.

  FIXME: Not very RCS-like to wait for status done here. (wps)
*/
static int sendCommand(RCS_CMD_MSG *msg)
{
  // need command buffer to be there
  if (0 == emcIoCommandBuffer) {
    return -1;
  }
  
  // need status buffer also, to check for command received
  if (0 == emcIoStatusBuffer ||
      ! emcIoStatusBuffer->valid()) {
    return -1;
  }

  double send_command_timeout = etime() + 5.0;

  // check if we're executing, and wait until we're done
  while(etime() < send_command_timeout) {
    emcIoStatusBuffer->peek();
    if (emcIoStatus->echo_serial_number != emcIoCommandSerialNumber ||
        emcIoStatus->status == RCS_EXEC) {
      esleep(0.001);
      continue;
    }
    else {
      break;
    }
  }

  if (emcIoStatus->echo_serial_number != emcIoCommandSerialNumber ||
      emcIoStatus->status == RCS_EXEC) {
    // Still not done, must have timed out.
    rcs_print_error("Command to IO level (%s:%s) timed out. \n",
		    emcSymbolLookup(msg->type),
		    emcIoCommandBuffer->msg2str(msg));
    rcs_print_error("emcIoStatus->echo_serial_number=%d, emcIoCommandSerialNumber=%d, emcIoStatus->status=%d\n",
		    emcIoStatus->echo_serial_number, 
		    emcIoCommandSerialNumber,
		    emcIoStatus->status);
    if(0 != last_io_command)
      {
	rcs_print_error("Last command sent to IO level was (%s:%s)\n",
		    emcSymbolLookup(last_io_command->type),
		    emcIoCommandBuffer->msg2str(last_io_command));
      }
    return -1;
  }

  // now we can send
  msg->serial_number = ++emcIoCommandSerialNumber;
  if (0 != emcIoCommandBuffer->write(msg)) {
    rcs_print_error("Failed to send command to  IO level (%s:%s)\n",
		    emcSymbolLookup(msg->type),
		    emcIoCommandBuffer->msg2str(msg));
    return -1;
  }

  if (last_io_command == 0)
    {
      last_io_command = (RCS_CMD_MSG *) malloc(0x4000);
    }
  
  if(0 != last_io_command)
    {
      memcpy(last_io_command,msg,msg->size);
    }

  return 0;
}

/*
  forceCommand() writes the given command regardless of the executing
  status of any previous command.
*/
static int forceCommand(RCS_CMD_MSG *msg)
{
  // need command buffer to be there
  if (0 == emcIoCommandBuffer) {
    return -1;
  }

  // need status buffer also, to check for command received
  if (0 == emcIoStatusBuffer ||
      ! emcIoStatusBuffer->valid()) {
    return -1;
  }

  // send it immediately
  msg->serial_number = ++emcIoCommandSerialNumber;
  if (0 != emcIoCommandBuffer->write(msg)) {
    return -1;
  }

  if (last_io_command == 0)
    {
      last_io_command = (RCS_CMD_MSG *) malloc(0x4000);
    }
  
  if(0 != last_io_command)
    {
      memcpy(last_io_command,msg,msg->size);
    }

  return 0;
}

// NML commands

int emcIoInit()
{
  EMC_TOOL_INIT ioInitMsg;
  int retval;

  // get NML buffer to emcio
  if (0 != (retval = emcioNmlGet())) {
    rcs_print_error("emcioNmlGet() failed.\n");
    return -1;
  }

  // initialize parameters from ini file for spindle subsystem
  // uses local versions of set functions for global variables,
  // since spindle is done here via AIO, DIO messages to emcmot

  if (0 != iniSpindle(EMC_INIFILE))
    {
      rcs_print_error("iniSpindle failed.\n");
      return -1;
    }

  // deactivate spindle axis in emcmot

  if (! emcmotAxisInited &&
      ! emcmotTrajInited &&
      ! emcmotIoInited)
    {
      usrmotIniLoad(EMC_INIFILE);

      if (0 != usrmotInit())
        {
	  rcs_print_error("usrmotInit failed.\n");
          return -1;
        }
    }
  emcmotIoInited = 1;

  emcmotCommand.command = EMCMOT_DEACTIVATE_AXIS;
  emcmotCommand.axis = SPINDLE_ENABLE_INDEX;

  if(usrmotWriteEmcmotCommand(&emcmotCommand))
    {
      rcs_print_error("Can't send EMCMOT_DEACTIVATE_AXIS for the spindle axis.\n");
      return -1;
    }

  // send init command to emcio
  if(forceCommand(&ioInitMsg))
    {
      rcs_print_error("Can't forceCommand(ioInitMsg)\n");
      return -1;
    }

  return 0;
}

int emcIoHalt()
{
  EMC_TOOL_HALT ioHaltMsg;

  // send halt command to emcio
  if (emcIoCommandBuffer != 0)
    {
      forceCommand(&ioHaltMsg);
    }

  if (! emcmotAxisInited &&     // axes are gone
      ! emcmotTrajInited &&     // traj is gone
      emcmotIoInited)           // but we're still here
    {
      usrmotExit();             // but now we're gone
    }

  // mark us as not needing in any case
  emcmotIoInited = 0;

  // clear out the buffers

  if (emcIoStatusBuffer != 0)
    {
      delete emcIoStatusBuffer;
      emcIoStatusBuffer = 0;
      emcIoStatus = 0;
    }

  if (emcIoCommandBuffer != 0)
    {
      delete emcIoCommandBuffer;
      emcIoCommandBuffer = 0;
    }

  return 0;
}

int emcIoAbort()
{
  EMC_TOOL_ABORT ioAbortMsg;

  // send abort command to emcio
  sendCommand(&ioAbortMsg);

  return 0;
}

int emcIoSetDebug(int debug)
{
  EMC_SET_DEBUG ioDebugMsg;

  ioDebugMsg.debug = debug;

  return sendCommand(&ioDebugMsg);
}

int emcAuxEstopOn()
{
  EMC_AUX_ESTOP_ON estopOnMsg;

  return forceCommand(&estopOnMsg);
}

int emcAuxEstopOff()
{
  EMC_AUX_ESTOP_OFF estopOffMsg;

  return forceCommand(&estopOffMsg);
}

static double simSpindleSpeed = 0.0;    // spindle speed in RPMs
static int simSpindleDirection = 0;     // 0 stopped, 1 forward, -1 reverse
static int simSpindleBrake = 1; // 0 released, 1 engaged
static int simSpindleIncreasing = 0; // 1 increasing, -1 decreasing, 0 neither
static int simSpindleEnabled = 0;       // non-zero means enabled

int emcSpindleAbort()
{
  return emcSpindleOff();
}

// send to motion system

int emcSpindleOn(double speed)
{
  int r1, r2;

  r1 = emcSpindleEnable();

  emcmotCommand.command = EMCMOT_DAC_OUT;
  emcmotCommand.axis = SPINDLE_ON_INDEX;
  emcmotCommand.dacOut = (speed > 0.0 ? (fabs(speed) * MAX_VOLTS_PER_RPM) :
			  speed < 0.0 ? (fabs(speed) * MIN_VOLTS_PER_RPM) : 0);

  r2 = usrmotWriteEmcmotCommand(&emcmotCommand);

  if (0 == r1 && 0 == r2)
    {
      simSpindleSpeed = speed;
      simSpindleDirection = (speed > 0.0 ? 1 : speed < 0.0 ? -1 : 0);
      return 0;
    }

  return -1;
}

// send to motion system
int emcSpindleEnable()
{
  int r1;

  emcmotCommand.command = EMCMOT_ENABLE_AMPLIFIER;
  emcmotCommand.axis = SPINDLE_ENABLE_INDEX;
  emcmotCommand.dacOut = 0.0;

  r1 = usrmotWriteEmcmotCommand(&emcmotCommand);

  if (0 == r1)
    {
      simSpindleEnabled = 1;
      return 0;
    }

  return -1;
}

// send to motion system
int emcSpindleDisable()
{
  int r1;

  emcmotCommand.command = EMCMOT_DISABLE_AMPLIFIER;
  emcmotCommand.axis = SPINDLE_ENABLE_INDEX;

  r1 = usrmotWriteEmcmotCommand(&emcmotCommand);

  if (0 == r1)
    {
      simSpindleEnabled = 0;
      return 0;
    }

  return -1;
}

int emcSpindleOff()
{
  int r1, r2;

  r1 = emcSpindleOn(0.0);

  r2 = emcSpindleDisable();

  if (0 == r1 && 0 == r2)
    {
      return 0;
    }

  return -1;
}

int emcSpindleBrakeRelease()
{
  // simulate here
  simSpindleBrake = 0;

  return 0;
}

int emcSpindleBrakeEngage()
{
  // simulate here
  simSpindleBrake = 1;

  return 0;
}

int emcSpindleIncrease()
{
  // simulate here
  simSpindleIncreasing = 1;

  return 0;
}

int emcSpindleDecrease()
{
  // simulate here
  simSpindleIncreasing = -1;

  return 0;
}

int emcSpindleConstant()
{
  // simulate here
  simSpindleIncreasing = 0;

  return 0;
}

static int simCoolantMist = 0;
static int simCoolantFlood = 0;

int emcCoolantMistOn()
{
  // simulate here
  simCoolantMist = 1;

  return 0;
}

int emcCoolantMistOff()
{
  // simulate here
  simCoolantMist = 0;

  return 0;
}

int emcCoolantFloodOn()
{
  // simulate here
  simCoolantFlood = 1;

  return 0;
}

int emcCoolantFloodOff()
{
  // simulate here
  simCoolantFlood = 0;

  return 0;
}

static int simLubeOn = 0;
static int simLubeLevel = 1;

int emcLubeInit()
{
  return 0;
}

int emcLubeHalt()
{
  return 0;
}

int emcLubeAbort()
{
  simLubeOn = 0;

  return 0;
}

int emcLubeOn()
{
  simLubeOn = 1;

  return 0;
}

int emcLubeOff()
{
  simLubeOn = 0;

  return 0;
}

int emcToolPrepare(int tool)
{
  EMC_TOOL_PREPARE toolPrepareMsg;

  toolPrepareMsg.tool = tool;
  sendCommand(&toolPrepareMsg);

  return 0;
}

int emcToolLoad()
{
  EMC_TOOL_LOAD toolLoadMsg;

  sendCommand(&toolLoadMsg);

  return 0;
}

int emcToolUnload()
{
  EMC_TOOL_UNLOAD toolUnloadMsg;

  sendCommand(&toolUnloadMsg);

  return 0;
}

int emcToolLoadToolTable(const char *file)
{
  EMC_TOOL_LOAD_TOOL_TABLE toolLoadToolTableMsg;

  strcpy(toolLoadToolTableMsg.file, file);

  sendCommand(&toolLoadToolTableMsg);

  return 0;
}

int emcToolSetOffset(int tool, double length, double diameter)
{
  EMC_TOOL_SET_OFFSET toolSetOffsetMsg;

  toolSetOffsetMsg.tool = tool;
  toolSetOffsetMsg.length = length;
  toolSetOffsetMsg.diameter = diameter;

  sendCommand(&toolSetOffsetMsg);

  return 0;
}

int emcLogOpen(char *file, int type, int size, int skip, int which, int triggerType, int triggerVar, double triggerThreshold )
{
  int r1;

  emcmotCommand.command = EMCMOT_OPEN_LOG;
  emcmotCommand.logSize = size;
  emcmotCommand.logSkip = skip;
  emcmotCommand.axis = which;
  emcmotCommand.logTriggerType = triggerType;
  emcmotCommand.logTriggerVariable = triggerVar;
  emcmotCommand.logTriggerThreshold = triggerThreshold;

  // Note that EMC NML and emcmot will be different, in general--
  // need to map types
  switch (type)
    {
    case EMC_LOG_TYPE_AXIS_POS:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_AXIS_POS;
      break;

    case EMC_LOG_TYPE_AXES_INPOS:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_ALL_INPOS;
      break;

    case EMC_LOG_TYPE_AXES_OUTPOS:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_ALL_OUTPOS;
      break;

    case EMC_LOG_TYPE_AXIS_VEL:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_AXIS_VEL;
      break;

    case EMC_LOG_TYPE_AXES_FERROR:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_ALL_FERROR;
      break;

    case EMC_LOG_TYPE_TRAJ_POS:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_TRAJ_POS;
      break;

    case EMC_LOG_TYPE_TRAJ_VEL:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_TRAJ_VEL;
      break;

    case EMC_LOG_TYPE_TRAJ_ACC:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_TRAJ_ACC;
      break;

    case EMC_LOG_TYPE_POS_VOLTAGE:
      emcmotCommand.logType = EMCMOT_LOG_TYPE_POS_VOLTAGE;
      break;

    default:
      return -1;
    }

  r1 = usrmotWriteEmcmotCommand(&emcmotCommand);

  if (r1 == 0)
    {
      strncpy(emcStatus->logFile, file, EMC_LOG_FILENAME_LEN - 1);
      emcStatus->logFile[EMC_LOG_FILENAME_LEN - 1] = 0;
    }
  // type, size, skip, open, and started will be gotten out of
  // subsystem status, e.g., emcTrajUpdate()

  return r1;
}

int emcLogStart()
{
  int r1;

  emcmotCommand.command = EMCMOT_START_LOG;

  r1 = usrmotWriteEmcmotCommand(&emcmotCommand);
  return(0);
}

int emcLogStop()
{
  int r1;

  emcmotCommand.command = EMCMOT_STOP_LOG;

  r1 = usrmotWriteEmcmotCommand(&emcmotCommand);

  return r1;
}

int emcLogClose()
{
  int r1;
  int r2;

  // first check for active log, return nicely if not
  if (emcStatus->logFile[0] == 0 ||
      emcStatus->logSize == 0)
    {
      return 0;
    }

  emcmotCommand.command = EMCMOT_CLOSE_LOG;

  r1 = usrmotWriteEmcmotCommand(&emcmotCommand);
  r2 = usrmotDumpLog(emcStatus->logFile, EMCLOG_INCLUDE_HEADER);

  emcStatus->logFile[0] = 0;
  emcStatus->logType = 0;
  emcStatus->logSize = 0;
  emcStatus->logSkip = 0;
  emcStatus->logOpen = 0;
  emcStatus->logStarted = 0;

  return (r1 || r2 ? -1 : 0);
}

// Status functions

int emcIoUpdate(EMC_IO_STAT *stat)
{
  if (0 == emcIoStatusBuffer ||
      ! emcIoStatusBuffer->valid())
    {
      return -1;
    }

  switch (emcIoStatusBuffer->peek())
    {
    case -1:
      // error on CMS channel
      return -1;
      break;

    case 0:                     // nothing new
    case EMC_IO_STAT_TYPE:      // something new
      // drop out to copy
      break;

    default:
      // something else is in there
      return -1;
      break;
    }

  // copy status
  *stat = *emcIoStatus;

  /*
    We need to check that the RCS_DONE isn't left over from the previous
    command, by comparing the command number we sent with the command
    number that emcio echoes. If they're different, then the command hasn't
    been acknowledged yet and the state should be forced to be RCS_EXEC.
  */
  if (stat->echo_serial_number != emcIoCommandSerialNumber)
    {
      stat->status = RCS_EXEC;
    }

  // overwrite with simulated locals

  stat->spindle.speed = simSpindleSpeed;
  stat->spindle.direction = simSpindleDirection;
  stat->spindle.brake = simSpindleBrake;
  stat->spindle.increasing = simSpindleIncreasing;
  stat->spindle.enabled = simSpindleEnabled;
  stat->coolant.mist = simCoolantMist;
  stat->coolant.flood = simCoolantFlood;
  stat->lube.on = simLubeOn;
  stat->lube.level = simLubeLevel;

  return 0;
}
