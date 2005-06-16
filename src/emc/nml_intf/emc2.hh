/********************************************************************
* Description: emc2.hh
*   Declarations for EMC NML vocabulary
*
*   Most of the NML message defines have been taken from the original
*   emc.hh source file - Commentry on message types and functions are
*   all new.
*
* Author: Paul Corner
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
#ifndef EMC2_HH
#define EMC2_HH

#include "global_defs.h"
#include "rcs.hh"
#include "posemath.h"		// PM_POSE, etc.
#include "canon.hh"		// CANON_TOOL_TABLE, CANON_UNITS
#include "emcglb.h"		// EMC_AXIS_MAX
#include "emcpos.h"

/*! \page NML message types
  OPERATOR_ERROR_MSG                      ((NMLTYPE) 10)
  OPERATOR_TEXT_MSG                       ((NMLTYPE) 11)
  OPERATOR_DISPLAY_MSG                    ((NMLTYPE) 12)
  NULL_MSG                                ((NMLTYPE) 20)

  These pass fixed length strings to the HMI from lower levels. Primary
   uses would be to alert the user to a critical system error or prompt
   an action.
*/
#define OPERATOR_ERROR_MSG                      ((NMLTYPE) 10)
#define OPERATOR_TEXT_MSG                       ((NMLTYPE) 11)
#define OPERATOR_DISPLAY_MSG                    ((NMLTYPE) 12)
#define NULL_MSG                                ((NMLTYPE) 20)
/*! \page NML message types
  SET_DEBUG_LEVEL                         ((NMLTYPE) 21)

  Sets the debug level in subservient processes
*/
#define SET_DEBUG_LEVEL                         ((NMLTYPE) 21)
/*! \page NML message types
  SYSTEM_CMD_MSG                          ((NMLTYPE) 22)

  Part of the hot command system - Used to execute commands
  specified in the interpreter's input file.
*/
#define SYSTEM_CMD_MSG                          ((NMLTYPE) 22)

/*! \page NML message types
  SET_AXIS_PARAM                          ((NMLTYPE) 100)

  SET_AXIS_PARAM is used to configure the low level subsytem.
  Each command will include a field specifying the axis, a
  parameter, and it's value.
*/
#define SET_AXIS_PARAM                          ((NMLTYPE) 100)
/*! \page NML message types
  UNITS GAINS CYCLE_TIME INPUT_SCALE OUTPUT_SCALE MIN_POSITION_LIMIT
  MAX_POSITION_LIMIT MIN_OUTPUT_LIMIT MAX_OUTPUT_LIMIT MIN_FERROR
  SET_FERROR SET_HOMING SET_HOME HOME_OFFSET MAX_ACCELERATION
  MAX_VELOCITY

  In reality, these would generally be set from a local configuration
  file rather than a remote process.
*/
#define UNITS
#define GAINS
#define CYCLE_TIME
#define INPUT_SCALE
#define OUTPUT_SCALE
#define MIN_POSITION_LIMIT
#define MAX_POSITION_LIMIT
#define MIN_OUTPUT_LIMIT
#define MAX_OUTPUT_LIMIT
#define MIN_FERROR
#define SET_FERROR
#define SET_HOMING
#define SET_HOME
#define HOME_OFFSET
#define MAX_ACCELERATION
#define MAX_VELOCITY

/*! \page NML message types
  AXIS_COMMAND                            ((NMLTYPE) 120)

  NML commands to directly control an axis. One mandatory paramater
  will be required (axis number), and an optional value for velocity
  or distance.
*/
#define AXIS_COMMAND                            ((NMLTYPE) 120)
#define AXIS_INIT
#define AXIS_HALT
#define AXIS_ABORT
#define AXIS_ENABLE
#define AXIS_DISABLE
#define AXIS_HOME
#define AXIS_JOG
#define AXIS_INCR_JOG
#define AXIS_ABS_JOG
#define AXIS_ACTIVATE
#define AXIS_DEACTIVATE
#define AXIS_OVERRIDE_LIMITS
/*! \page NML message types

    AXIS_SET_OUTPUT & AXIS_SET_STEP_PARAMS are candidates for moving
    to SET_AXIS_PARAM
*/
#define AXIS_SET_OUTPUT
#define AXIS_LOAD_COMP
#define AXIS_ALTER
#define AXIS_SET_STEP_PARAMS

/*! \page NML message types
  AXIS_STATUS                             ((NMLTYPE) 199)

  This returns the status of each axis, typically, active state, in
  position, and current position. It does NOT report the state of
  it's interpolator or any other internal data.
*/
#define AXIS_STATUS                             ((NMLTYPE) 199)

/*! \page NML message types
  SET_TRAJECTORY_PARAM                    ((NMLTYPE) 200)

  SET_TRAJECTORY_PARAM is used to configure the low level subsytem
  or set preconditions for a move appended to the queue.
*/
#define SET_TRAJECTORY_PARAM                    ((NMLTYPE) 200)
#define TRAJ_SET_AXES
#define TRAJ_SET_UNITS
#define TRAJ_SET_CYCLE
#define TRAJ_SET_MODE
#define TRAJ_SET_VELOCITY
#define TRAJ_SET_ACCELERATION
#define TRAJ_SET_MAX_VELOCITY
#define TRAJ_SET_MAX_ACCELERATION
#define TRAJ_SET_SCALE
#define TRAJ_SET_MOTION_ID

/*! \page NML message types
  TRAJECTORY_COMMAND                      ((NMLTYPE) 220)

  WARNING - most of the folowing map to canonical commands. Need
  to look very closely at task to see what is safe to refactor,
  and which should be left for another day.
*/
#define TRAJECTORY_COMMAND                      ((NMLTYPE) 220)
#define TRAJ_INIT
#define TRAJ_HALT
#define TRAJ_ENABLE
#define TRAJ_DISABLE
#define TRAJ_ABORT
#define TRAJ_PAUSE
#define TRAJ_STEP
#define TRAJ_RESUME
#define TRAJ_DELAY
#define TRAJ_LINEAR_MOVE
#define TRAJ_CIRCULAR_MOVE
#define TRAJ_SET_TERM_COND
#define TRAJ_SET_OFFSET
#define TRAJ_SET_ORIGIN
#define TRAJ_SET_HOME
#define TRAJ_SET_PROBE_INDEX
#define TRAJ_SET_PROBE_POLARITY
#define TRAJ_CLEAR_PROBE_TRIPPED_FLAG
#define TRAJ_PROBE
#define TRAJ_SET_TELEOP_ENABLE
#define TRAJ_SET_TELEOP_VECTOR

/*! \page NML message types
  TRAJ_STATUS                             ((NMLTYPE) 299)

  This returns essential status data of the trajectory subsystem.
  It does NOT report the state of the motion queue.
*/
#define TRAJ_STATUS                             ((NMLTYPE) 299)

/*! \page NML message types
  MOTION_COMMAND                          ((NMLTYPE) 300)

  Commands to coordinate IO with motion - May be better to
  include these in the trajectory group...
*/
#define MOTION_COMMAND                          ((NMLTYPE) 300)
#define MOTION_INIT
#define MOTION_HALT
#define MOTION_ABORT
#define MOTION_SET_AOUT
#define MOTION_SET_DOUT

/*! \page NML message types
  MOTION_STATUS                           ((NMLTYPE) 399)

  Status message for motion commands.
*/
#define MOTION_STATUS                           ((NMLTYPE) 399)

/*! \page NML message types
  EMC_TASK_INIT_TYPE                           ((NMLTYPE) 501)
  to
  EMC_TASK_PLAN_SYNCH_TYPE                     ((NMLTYPE) 516)

  Most of these _do_ map to functions within task - Need to do
  a full analysis of task before trashing this section.
*/
#define EMC_TASK_INIT_TYPE                           ((NMLTYPE) 501)
#define EMC_TASK_HALT_TYPE                           ((NMLTYPE) 502)
#define EMC_TASK_ABORT_TYPE                          ((NMLTYPE) 503)
#define EMC_TASK_SET_MODE_TYPE                       ((NMLTYPE) 504)
#define EMC_TASK_SET_STATE_TYPE                      ((NMLTYPE) 505)
#define EMC_TASK_PLAN_OPEN_TYPE                      ((NMLTYPE) 506)
#define EMC_TASK_PLAN_RUN_TYPE                       ((NMLTYPE) 507)
#define EMC_TASK_PLAN_READ_TYPE                      ((NMLTYPE) 508)
#define EMC_TASK_PLAN_EXECUTE_TYPE                   ((NMLTYPE) 509)
#define EMC_TASK_PLAN_PAUSE_TYPE                     ((NMLTYPE) 510)
#define EMC_TASK_PLAN_STEP_TYPE                      ((NMLTYPE) 511)
#define EMC_TASK_PLAN_RESUME_TYPE                    ((NMLTYPE) 512)
#define EMC_TASK_PLAN_END_TYPE                       ((NMLTYPE) 513)
#define EMC_TASK_PLAN_CLOSE_TYPE                     ((NMLTYPE) 514)
#define EMC_TASK_PLAN_INIT_TYPE                      ((NMLTYPE) 515)
#define EMC_TASK_PLAN_SYNCH_TYPE                     ((NMLTYPE) 516)

/*! \page NML message types
  EMC_TASK_STAT_TYPE                           ((NMLTYPE) 599)

  More status data... How much do we need to report on ?
*/
#define EMC_TASK_STAT_TYPE                           ((NMLTYPE) 599)

/*! \page NML message types
  TOOL_COMMAND                            ((NMLTYPE) 600)

  NML commands for the tool subsystem
*/
#define TOOL_COMMAND                            ((NMLTYPE) 600)
#define TOOL_INIT
#define TOOL_HALT
#define TOOL_ABORT
#define TOOL_PREPARE
#define TOOL_LOAD
#define TOOL_UNLOAD
#define TOOL_LOAD_TOOL_TABLE
#define TOOL_SET_OFFSET

/*! \page NML message types
  TOOL_STATUS                             ((NMLTYPE) 699)

  Status for the tool module - Probably just need to know which
  tool number is loaded. Anything more is just noise.
*/
#define TOOL_STATUS                             ((NMLTYPE) 699)

/*! \page NML message types
  AUX_IO_COMMAND                          ((NMLTYPE) 700)

  Apart from E-Stop and Abort, do we need the rest of these
  commands ?
*/
#define AUX_IO_COMMAND                          ((NMLTYPE) 700)
#define AUX_INIT
#define AUX_HALT
#define AUX_ABORT
#define AUX_DIO_WRITE
#define AUX_AIO_WRITE
#define AUX_ESTOP_ON
#define AUX_ESTOP_OFF

/*! \page NML message types
  AUX_IO_STATUS                           ((NMLTYPE) 799)

  Main purpose of this status message is to return the current
  E-Stop state.
*/
#define AUX_IO_STATUS                           ((NMLTYPE) 799)

/*! \page NML message types
  SPINDLE_COMMAND                         ((NMLTYPE) 800)

  Spindle commands - On. Off, and speed should be all that needs
  to be passed to the spindle module. Brake interlocks are a
  function of a PLC rather than the high level system.
*/
#define SPINDLE_COMMAND                         ((NMLTYPE) 800)
#define SPINDLE_INIT
#define SPINDLE_HALT
#define SPINDLE_ABORT
#define SPINDLE_ON
#define SPINDLE_OFF
#define SPINDLE_FORWARD
#define SPINDLE_REVERSE
#define SPINDLE_STOP
#define SPINDLE_INCREASE
#define SPINDLE_DECREASE
#define SPINDLE_CONSTANT
#define SPINDLE_BRAKE_RELEASE
#define SPINDLE_BRAKE_ENGAGE
#define SPINDLE_ENABLE
#define SPINDLE_DISABLE

/*! \page NML message types
  SPINDLE_STATUS                          ((NMLTYPE) 899)

  Spindle status. Current speed, brake condition, & direction..
*/
#define SPINDLE_STATUS                          ((NMLTYPE) 899)

/*! \page NML message types
  COOLANT_COMMAND                         ((NMLTYPE) 900)

  Coolant subsystem commands - Should these be aggregated with
  Spindle & Lube commands and passed to a PLC module ?
*/
#define COOLANT_COMMAND                         ((NMLTYPE) 900)
#define COOLANT_INIT
#define COOLANT_HALT
#define COOLANT_ABORT
#define COOLANT_MIST_ON
#define COOLANT_MIST_OFF
#define COOLANT_FLOOD_ON
#define COOLANT_FLOOD_OFF

/*! \page NML message types
  COOLANT_STATUS                          ((NMLTYPE) 999)

  Coolant level, mist, or flood. what other status is there ?
*/
#define COOLANT_STATUS                          ((NMLTYPE) 999)

/*! \page NML message types
  LUBE_COMMAND                            ((NMLTYPE) 1000)

  Certainly Lube should be part of the main IO group !
*/
#define LUBE_COMMAND                            ((NMLTYPE) 1000)
#define LUBE_INIT
#define LUBE_HALT
#define LUBE_ABORT
#define LUBE_ON
#define LUBE_OFF

/*! \page NML message types
  LUBE_STATUS                             ((NMLTYPE) 1099)
  The only "status" required is level OK
*/
#define LUBE_STATUS                             ((NMLTYPE) 1099)

/*! \page NML message types
  IO_COMMAND                              ((NMLTYPE) 1100)

  Appears to be duplicate messages here... 
*/
#define IO_COMMAND                              ((NMLTYPE) 1100)
#define IO_INIT
#define IO_HALT
#define IO_ABORT
#define IO_SET_CYCLE_TIME

/*! \page NML message types
  IO_STATUS                             ((NMLTYPE) 1199)

  What would the HMI need to know about IO when there is so few
  commands to issue ?
*/
#define IO_STATUS                             ((NMLTYPE) 1199)

// EMC aggregate class type declaration
/*! \page NML message types
  EMC_COMMAND                             ((NMLTYPE) 1200)

  IF halscope lives up to expectations, logging would disappear
  from the motion level. That on it's own would free up considerable
  chunks of memory.
*/
#define EMC_COMMAND                             ((NMLTYPE) 1200)
#define EMC_INIT
#define EMC_HALT
#define EMC_ABORT
#define EMC_LOG_OPEN
#define EMC_LOG_START
#define EMC_LOG_STOP
#define EMC_LOG_CLOSE

/*! \page NML message types
  EMC_STATUS                              ((NMLTYPE) 1299)

  Aggregate status message containing all the status messages from
  lower levels. How much is needed by the HMI ?
  Probably very little if you are to take the RMA model to it's
  limits.
*/
#define EMC_STATUS                              ((NMLTYPE) 1299)

// NML formatting function
extern int emcFormat(NMLTYPE type, void * buffer, CMS * cms);

// NML Symbol Lookup Function
extern const char * emc_symbol_lookup(long type);
#define emcSymbolLookup(a) emc_symbol_lookup(a)

#endif
