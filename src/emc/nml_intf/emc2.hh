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
  EMC_OPERATOR_ERROR_TYPE                      ((NMLTYPE) 11)
  EMC_OPERATOR_TEXT_TYPE                       ((NMLTYPE) 12)
  EMC_OPERATOR_DISPLAY_TYPE                    ((NMLTYPE) 13)

  These pass fixed length strings to the HMI from lower levels. Primary
   uses would be to alert the user to a critical system error or prompt
   an action.

  AJ says: merge these into a single message, and check an aditional flag for message/error. 
           The last one isn't used anyways.
*/
#define EMC_OPERATOR_ERROR_TYPE                      ((NMLTYPE) 11) // Keep
#define EMC_OPERATOR_TEXT_TYPE                       ((NMLTYPE) 12) // Keep
#define EMC_OPERATOR_DISPLAY_TYPE                    ((NMLTYPE) 13) // Keep
/*! \page NML message types
  EMC_NULL_TYPE                                ((NMLTYPE) 21)

  Null message - Redundant perhaps ?
*/
#define EMC_NULL_TYPE                                ((NMLTYPE) 21) // Keep
/*! \page NML message types
  EMC_SET_DEBUG_TYPE                           ((NMLTYPE) 22)

  Sets the debug level in subservient processes
*/
#define EMC_SET_DEBUG_TYPE                           ((NMLTYPE) 22) // Keep
/*! \page NML message types
  EMC_SYSTEM_CMD_TYPE                          ((NMLTYPE) 30)

  Part of the hot command system - Used to execute commands
  specified in the interpreter's input file.
*/
#define EMC_SYSTEM_CMD_TYPE                          ((NMLTYPE) 30) // Keep
/*! \page NML message types
  Proposal: Replace the multitude of EMC_AXIS_SET_* with a single
  SET_PARAM message. Each parameter would be enumerated and one
  value (along with an axis identifier) sent individually.
  AJ: agreed, have one 
#define EMC_AXIS_SET_PARAM_TYPE                       ((NMLTYPE) 100)
  with subtypes 
#define SET_AXIS_TYPE                       ((NMLSUBTYPE) 101)
#define SET_UNITS_TYPE                      ((NMLSUBTYPE) 102) - Linear (in mm), or Angular (in rads). Works for me.
#define SET_P_TYPE                          ((NMLSUBTYPE) 104)
#define SET_I_TYPE                          ((NMLSUBTYPE) 105)
#define SET_D_TYPE                          ((NMLSUBTYPE) 106)
#define SET_FF0_TYPE                        ((NMLSUBTYPE) 107)
<snip>
  and remove all other EMC_AXIS_SET_*

#define EMC_AXIS_SET_PARAM_TYPE                       ((NMLTYPE) 100)
  Class members: AXIS
                 enum VARIABLE {p, i, d, ff, etc}
                 double data
                 int data

Fortunately, only have two data types to pass - We could use a double and cast it
as an int, but the payload of an additional int is small..
*/
#define EMC_AXIS_SET_AXIS_TYPE                       ((NMLTYPE) 101)
#define EMC_AXIS_SET_UNITS_TYPE                      ((NMLTYPE) 102)
#define EMC_AXIS_SET_GAINS_TYPE                      ((NMLTYPE) 103)
#define EMC_AXIS_SET_CYCLE_TIME_TYPE                 ((NMLTYPE) 104)
#define EMC_AXIS_SET_INPUT_SCALE_TYPE                ((NMLTYPE) 105)
#define EMC_AXIS_SET_OUTPUT_SCALE_TYPE               ((NMLTYPE) 106)
#define EMC_AXIS_SET_MIN_POSITION_LIMIT_TYPE         ((NMLTYPE) 107)
#define EMC_AXIS_SET_MAX_POSITION_LIMIT_TYPE         ((NMLTYPE) 108)
#define EMC_AXIS_SET_MIN_OUTPUT_LIMIT_TYPE           ((NMLTYPE) 109)
#define EMC_AXIS_SET_MAX_OUTPUT_LIMIT_TYPE           ((NMLTYPE) 110)
#define EMC_AXIS_SET_FERROR_TYPE                     ((NMLTYPE) 111)
#define EMC_AXIS_SET_HOMING_VEL_TYPE                 ((NMLTYPE) 112)
#define EMC_AXIS_SET_HOME_TYPE                       ((NMLTYPE) 113)
#define EMC_AXIS_SET_HOME_OFFSET_TYPE                ((NMLTYPE) 114)
#define EMC_AXIS_SET_MIN_FERROR_TYPE                 ((NMLTYPE) 115)
#define EMC_AXIS_SET_MAX_VELOCITY_TYPE               ((NMLTYPE) 116)

/*! \page NML message types
  These infernal EMC_*_INIT_, HALT, and ABORT _TYPE messages should
  be reduced to a single group.
  
  AJ: agreed, remove them all, keep the generic 
  EMC_INIT_TYPE, EMC_HALT_TYPE, EMC_ABORT_TYPE
*/
#define EMC_AXIS_INIT_TYPE                           ((NMLTYPE) 118)  // AJ: remove
#define EMC_AXIS_HALT_TYPE                           ((NMLTYPE) 119)  // AJ: remove
#define EMC_AXIS_ABORT_TYPE                          ((NMLTYPE) 120)  // AJ: used currently to stop a JOG, not the cleanest way
#define EMC_AXIS_ENABLE_TYPE                         ((NMLTYPE) 121)
#define EMC_AXIS_DISABLE_TYPE                        ((NMLTYPE) 122)  // AJ: merge this with EMC_AXIS_ENABLE
#define EMC_AXIS_HOME_TYPE                           ((NMLTYPE) 123)  // Keep - Triggers a homing sequence.
#define EMC_AXIS_JOG_TYPE                            ((NMLTYPE) 124)  // Need a JOG message, but three variants ?
#define EMC_AXIS_INCR_JOG_TYPE                       ((NMLTYPE) 125)
#define EMC_AXIS_ABS_JOG_TYPE                        ((NMLTYPE) 126)
#define EMC_AXIS_ACTIVATE_TYPE                       ((NMLTYPE) 127)  // Unused, AJ: remove (use EMC_AXIS_ENABLE if needed)
#define EMC_AXIS_DEACTIVATE_TYPE                     ((NMLTYPE) 128)  // Unused, AJ: remove (use EMC_AXIS_ENABLE if needed)
#define EMC_AXIS_OVERRIDE_LIMITS_TYPE                ((NMLTYPE) 129)  // keep
#define EMC_AXIS_SET_OUTPUT_TYPE                     ((NMLTYPE) 130)  // This may have a purpose, or it can be replaced with a SET_AIO AJ: SET_AIO
#define EMC_AXIS_LOAD_COMP_TYPE                      ((NMLTYPE) 131)  // Should probably keep
#define EMC_AXIS_ALTER_TYPE                          ((NMLTYPE) 132)  // EMC_AXIS_ALTER_TYPE Obscure name - Used to load a compensation value for current position.
#define EMC_AXIS_SET_STEP_PARAMS_TYPE                ((NMLTYPE) 133)  // AJ: Make part of SET_PARAM above, remove here. should probably end up in the Hardware driver (HAL). 
		//AJ: but it's an ugly hack to have this in NML, but not other generic Hardware config (why not similar messages for servo's...)
		/* PC: Comment about AXIS & TRAJ parameters. Just because *some* of them might be HAL parameters, to arbitrarily remove
                       them destroys the possibility of using a "remote" tuning tool. It also wrecks a few ideas on config handling
                       (amongst other things) - Condemn the code to HAL at your peril. */
		/* AJ: ok, then maybe generalize it to some extent so that it can be used for "remote" tuning. 
		       But definately it can be included in the SET_* aggregated message above. */
#define EMC_AXIS_STAT_TYPE                           ((NMLTYPE) 199)  // Keep
/*! \page NML message types
  MOTION_ID, MODE, SCALE, and VELOCITY are core parts of task. Need to retain.
*/
#define EMC_TRAJ_SET_AXES_TYPE                       ((NMLTYPE) 201)  // AJ: Unused, remove
#define EMC_TRAJ_SET_UNITS_TYPE                      ((NMLTYPE) 202)  // AJ: Unused, remove - Use the metre and radians throughout. Would save having to mess with converting legacy units that very few people use in the real world.
#define EMC_TRAJ_SET_CYCLE_TIME_TYPE                 ((NMLTYPE) 203)  // AJ: Unused, remove
#define EMC_TRAJ_SET_MODE_TYPE                       ((NMLTYPE) 204)  // AJ: Unused, remove
#define EMC_TRAJ_SET_VELOCITY_TYPE                   ((NMLTYPE) 205)  // Keep
#define EMC_TRAJ_SET_ACCELERATION_TYPE               ((NMLTYPE) 206)  // AJ: Unused, remove
#define EMC_TRAJ_SET_MAX_VELOCITY_TYPE               ((NMLTYPE) 207)  // AJ: Unused, remove
#define EMC_TRAJ_SET_MAX_ACCELERATION_TYPE           ((NMLTYPE) 208)  // AJ: Unused, remove
#define EMC_TRAJ_SET_SCALE_TYPE                      ((NMLTYPE) 209)  // AJ: might want to change name to better reflect feed override
#define EMC_TRAJ_SET_MOTION_ID_TYPE                  ((NMLTYPE) 210)  // AJ: Unused, remove
/*! \page NML message types
  More INIT, HALT, ABORT types.
*/
#define EMC_TRAJ_INIT_TYPE                           ((NMLTYPE) 211)  // AJ: Unused, remove and use EMC_INIT instead
#define EMC_TRAJ_HALT_TYPE                           ((NMLTYPE) 212)  // AJ: Unused, remove and use EMC_HALT instead
#define EMC_TRAJ_ENABLE_TYPE                         ((NMLTYPE) 213)  // AJ: Unused, remove
#define EMC_TRAJ_DISABLE_TYPE                        ((NMLTYPE) 214)  // AJ: Unused, remove
#define EMC_TRAJ_ABORT_TYPE                          ((NMLTYPE) 215)  // AJ: maybe use EMC_ABORT instead?
#define EMC_TRAJ_PAUSE_TYPE                          ((NMLTYPE) 216)  // Used for single stepping
#define EMC_TRAJ_STEP_TYPE                           ((NMLTYPE) 217)  // Used for single stepping
#define EMC_TRAJ_RESUME_TYPE                         ((NMLTYPE) 218)  // Used for single stepping
#define EMC_TRAJ_DELAY_TYPE                          ((NMLTYPE) 219)  // Keep - G4
#define EMC_TRAJ_LINEAR_MOVE_TYPE                    ((NMLTYPE) 220)  // Keep
#define EMC_TRAJ_CIRCULAR_MOVE_TYPE                  ((NMLTYPE) 221)  // Keep
#define EMC_TRAJ_SET_TERM_COND_TYPE                  ((NMLTYPE) 222)  // Keep, AJ: used to switch from blend to exact path
#define EMC_TRAJ_SET_OFFSET_TYPE                     ((NMLTYPE) 223)  // Keep
#define EMC_TRAJ_SET_ORIGIN_TYPE                     ((NMLTYPE) 224)  // Keep
#define EMC_TRAJ_SET_HOME_TYPE                       ((NMLTYPE) 225)  // Unused, AJ: remove
#define EMC_TRAJ_SET_PROBE_INDEX_TYPE                ((NMLTYPE) 226)  // This is config AJ: both of these should reach HAL eventually
#define EMC_TRAJ_SET_PROBE_POLARITY_TYPE             ((NMLTYPE) 227)  // as is this - Flag for rethink. AJ: ditto
#define EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG_TYPE       ((NMLTYPE) 228)  // Keep
#define EMC_TRAJ_PROBE_TYPE                          ((NMLTYPE) 229)  // Keep - Used for probing (wow)
#define EMC_TRAJ_SET_TELEOP_ENABLE_TYPE              ((NMLTYPE) 230)  // Oh wth... Can we not use a MODE message here ?
#define EMC_TRAJ_SET_TELEOP_VECTOR_TYPE              ((NMLTYPE) 231)  // Jogging in teleop mode - fff....
#define EMC_TRAJ_STAT_TYPE                           ((NMLTYPE) 299)  // Keep
/*! \page NML message types
  More infernal INIT, HALT, ABORT messages.. !
*/
#define EMC_MOTION_INIT_TYPE                         ((NMLTYPE) 301)  // AJ remove these, use the general EMC_INIT
#define EMC_MOTION_HALT_TYPE                         ((NMLTYPE) 302)  // AJ remove these, use the general EMC_HALT
#define EMC_MOTION_ABORT_TYPE                        ((NMLTYPE) 303)  // AJ remove these, use the general EMC_ABORT
#define EMC_MOTION_SET_AOUT_TYPE                     ((NMLTYPE) 304)  // Keep - Unused, but could be used for (e.g.) coordinated control of laser power.
#define EMC_MOTION_SET_DOUT_TYPE                     ((NMLTYPE) 305)  // Keep - Coordinated On/Off signals.
#define EMC_MOTION_STAT_TYPE                         ((NMLTYPE) 399)  // keep
/*! \page NML message types
  And yet more....
*/
#define EMC_TASK_INIT_TYPE                           ((NMLTYPE) 501)  // AJ remove these, use the general EMC_INIT
#define EMC_TASK_HALT_TYPE                           ((NMLTYPE) 502)  // AJ remove these, use the general EMC_HALT
#define EMC_TASK_ABORT_TYPE                          ((NMLTYPE) 503)  // AJ remove these, use the general EMC_ABORT
#define EMC_TASK_SET_MODE_TYPE                       ((NMLTYPE) 504)  // Should mode & state be aggregated with TELEOP & STATE ?
#define EMC_TASK_SET_STATE_TYPE                      ((NMLTYPE) 505)  // keep
#define EMC_TASK_PLAN_OPEN_TYPE                      ((NMLTYPE) 506)  // keep
#define EMC_TASK_PLAN_RUN_TYPE                       ((NMLTYPE) 507)  // keep
#define EMC_TASK_PLAN_READ_TYPE                      ((NMLTYPE) 508)  // keep
#define EMC_TASK_PLAN_EXECUTE_TYPE                   ((NMLTYPE) 509)  // keep
#define EMC_TASK_PLAN_PAUSE_TYPE                     ((NMLTYPE) 510)  // keep
#define EMC_TASK_PLAN_STEP_TYPE                      ((NMLTYPE) 511)  // keep
#define EMC_TASK_PLAN_RESUME_TYPE                    ((NMLTYPE) 512)  // keep
#define EMC_TASK_PLAN_END_TYPE                       ((NMLTYPE) 513)  // keep
#define EMC_TASK_PLAN_CLOSE_TYPE                     ((NMLTYPE) 514)  // keep
#define EMC_TASK_PLAN_INIT_TYPE                      ((NMLTYPE) 515)  // Gnnnn...
#define EMC_TASK_PLAN_SYNCH_TYPE                     ((NMLTYPE) 516)  // keep
#define EMC_TASK_STAT_TYPE                           ((NMLTYPE) 599)  // keep
/*! \page NML message types
  These are not the last...
*/
#define EMC_TOOL_INIT_TYPE                           ((NMLTYPE) 1101)  // AJ remove these, use the general EMC_INIT
#define EMC_TOOL_HALT_TYPE                           ((NMLTYPE) 1102)  // AJ remove these, use the general EMC_HALT
#define EMC_TOOL_ABORT_TYPE                          ((NMLTYPE) 1103)  // AJ remove these, use the general EMC_ABORT
#define EMC_TOOL_PREPARE_TYPE                        ((NMLTYPE) 1104)  // keep AJ: extend the toolchanger to properly handle this
#define EMC_TOOL_LOAD_TYPE                           ((NMLTYPE) 1105)  // keep
#define EMC_TOOL_UNLOAD_TYPE                         ((NMLTYPE) 1106)  // LOAD implies an UNLOAD preceeds it...
#define EMC_TOOL_LOAD_TOOL_TABLE_TYPE                ((NMLTYPE) 1107)  // keep - But get rid of the lunacy.
#define EMC_TOOL_SET_OFFSET_TYPE                     ((NMLTYPE) 1108)  // keep
#define EMC_TOOL_STAT_TYPE                           ((NMLTYPE) 1199)  // Keep
/*! \page NML message types
  Nor these the last...
*/
#define EMC_AUX_INIT_TYPE                             ((NMLTYPE) 1201)  // AJ remove these, use the general EMC_INIT
#define EMC_AUX_HALT_TYPE                             ((NMLTYPE) 1202)  // AJ remove these, use the general EMC_HALT
#define EMC_AUX_ABORT_TYPE                            ((NMLTYPE) 1203)  // AJ remove these, use the general EMC_ABORT
#define EMC_AUX_DIO_WRITE_TYPE                        ((NMLTYPE) 1204)  // keep and/or modify
#define EMC_AUX_AIO_WRITE_TYPE                        ((NMLTYPE) 1205)  // keep and/or modify
#define EMC_AUX_ESTOP_ON_TYPE                         ((NMLTYPE) 1206)  // Why does task need to twiddle ESTOP ? AJ: Task doesn't, but GUI does. This should be part of IO IMO.
#define EMC_AUX_ESTOP_OFF_TYPE                        ((NMLTYPE) 1207)  // This should be a failsafe system.
#define EMC_AUX_STAT_TYPE                             ((NMLTYPE) 1299)  // Apart from ESTOP (which is duplicated elsewhwere), this is redundant.
/*! \page NML message types
  Some more INIT, HALT, ABORT....
*/
#define EMC_SPINDLE_INIT_TYPE                        ((NMLTYPE) 1301)  // AJ: only one SPINDLE message should be enough. Maybe even part of an IO message, thus it would be easy for another IO controller without spindle to be used instead.
#define EMC_SPINDLE_HALT_TYPE                        ((NMLTYPE) 1302)  // AJ: remove all others
#define EMC_SPINDLE_ABORT_TYPE                       ((NMLTYPE) 1303)
#define EMC_SPINDLE_ON_TYPE                          ((NMLTYPE) 1304)  // Keep - In a modified form
#define EMC_SPINDLE_OFF_TYPE                         ((NMLTYPE) 1305)  // Virtually all SPINDLE messages can be aggregated in to one.
#define EMC_SPINDLE_FORWARD_TYPE                     ((NMLTYPE) 1306)
#define EMC_SPINDLE_REVERSE_TYPE                     ((NMLTYPE) 1307)
#define EMC_SPINDLE_STOP_TYPE                        ((NMLTYPE) 1308)
#define EMC_SPINDLE_INCREASE_TYPE                    ((NMLTYPE) 1309)
#define EMC_SPINDLE_DECREASE_TYPE                    ((NMLTYPE) 1310)
#define EMC_SPINDLE_CONSTANT_TYPE                    ((NMLTYPE) 1311)
#define EMC_SPINDLE_BRAKE_RELEASE_TYPE               ((NMLTYPE) 1312)  // Brake interlocks is for PLC to handle.
#define EMC_SPINDLE_BRAKE_ENGAGE_TYPE                ((NMLTYPE) 1313)
#define EMC_SPINDLE_ENABLE_TYPE                      ((NMLTYPE) 1314)
#define EMC_SPINDLE_DISABLE_TYPE                     ((NMLTYPE) 1315)
#define EMC_SPINDLE_STAT_TYPE                        ((NMLTYPE) 1399)  // Keep - Aggregate with TOOL_STAT
/*! \page NML message types
  Starting to get tiresome, these INIT/HALT types..
*/
#define EMC_COOLANT_INIT_TYPE                        ((NMLTYPE) 1401)  // AJ: only one COOLANT message should be enough. Maybe even part of an IO message, thus it would be easy for another IO controller without spindle to be used instead.
#define EMC_COOLANT_HALT_TYPE                        ((NMLTYPE) 1402)
#define EMC_COOLANT_ABORT_TYPE                       ((NMLTYPE) 1403)
#define EMC_COOLANT_MIST_ON_TYPE                     ((NMLTYPE) 1404) // Keep - Aggregate STATE ?
#define EMC_COOLANT_MIST_OFF_TYPE                    ((NMLTYPE) 1405)
#define EMC_COOLANT_FLOOD_ON_TYPE                    ((NMLTYPE) 1406) // Keep - Aggregate STATE ?
#define EMC_COOLANT_FLOOD_OFF_TYPE                   ((NMLTYPE) 1407)
#define EMC_COOLANT_STAT_TYPE                        ((NMLTYPE) 1499)  // Keep - Aggregate with TOOL_STAT ?
/*! \page NML message types
*/
#define EMC_LUBE_INIT_TYPE                           ((NMLTYPE) 1501)  // AJ: only one LUBE message should be enough. Maybe even part of an IO message, thus it would be easy for another IO controller without spindle to be used instead.
#define EMC_LUBE_HALT_TYPE                           ((NMLTYPE) 1502)  // PC: Do away with LUBE messages all together. Let the PLC process handle it.
#define EMC_LUBE_ABORT_TYPE                          ((NMLTYPE) 1503)  // PC: The only thing we need to know is if oil level is low - A simple error on STATUS is enough.
#define EMC_LUBE_ON_TYPE                             ((NMLTYPE) 1504)  // Aggregate with machine STATE and let PLC decide.
#define EMC_LUBE_OFF_TYPE                            ((NMLTYPE) 1505)
#define EMC_LUBE_STAT_TYPE                           ((NMLTYPE) 1599)  // Keep - Aggregate with TOOL_STAT
/*! \page NML message types
  FPS... Keeping SET_DIO & SET_AIO paves the way for the possibility of limited PLC type functionality (or custom macros)
  within the interpreter. n.b. Whilst HAL offers a solution for IO configuration, it is NOT a panacea for everything.
*/
#define EMC_SET_DIO_INDEX_TYPE                       ((NMLTYPE) 5001) // Unused AJ: should go away, or a more general message to announce HAL about settings
#define EMC_SET_AIO_INDEX_TYPE                       ((NMLTYPE) 5002) // Unused AJ:    -"-
#define EMC_SET_POLARITY_TYPE                        ((NMLTYPE) 5003) // Unused AJ:    -"-
/*! \page NML message types
  Arrrgggg
*/
#define EMC_IO_INIT_TYPE                             ((NMLTYPE) 1601) // Unused AJ: should really go away, use EMC_INIT instead
#define EMC_IO_HALT_TYPE                             ((NMLTYPE) 1602) // Unused AJ: should really go away, use EMC_HALT instead
#define EMC_IO_ABORT_TYPE                            ((NMLTYPE) 1603) // Unused AJ: should really go away, use EMC_ABORT instead
#define EMC_IO_SET_CYCLE_TIME_TYPE                   ((NMLTYPE) 1604) // Unused AJ: should really go away
#define EMC_IO_STAT_TYPE                             ((NMLTYPE) 1699) // Redundant.
/*! \page NML message types
  And these remain unused. WHY ??
*/
#define EMC_INIT_TYPE                                ((NMLTYPE) 1901) // fff... Either a global, or not at all.
#define EMC_HALT_TYPE                                ((NMLTYPE) 1902) // AJ: these should be the only one used
#define EMC_ABORT_TYPE                               ((NMLTYPE) 1903)
/*! \page NML message types
  With HAL and the scope tool, logging is almost redundant...
*/
#define EMC_LOG_OPEN_TYPE                            ((NMLTYPE) 1904) // Do we need these ?
#define EMC_LOG_START_TYPE                           ((NMLTYPE) 1905) // Can we get away with an aggregate message ?
#define EMC_LOG_STOP_TYPE                            ((NMLTYPE) 1906) // Maybe.
#define EMC_LOG_CLOSE_TYPE                           ((NMLTYPE) 1907) // AJ: not sure if LOGGING is needed, from what I've seen only logging of the MOTION part is done, and that can be done by halscope & the like.

/*! \page NML message types
  EMC_STAT_TYPE                              ((NMLTYPE) 1999)

  Aggregate status message containing all the status messages from
  lower levels. How much is needed by the HMI ?
  Probably very little if you are to take the RMA model to it's
  limits. Certainly plenty can be hacked out without detriment.
*/
#define EMC_STAT_TYPE                                ((NMLTYPE) 1999) // Keep

// NML formatting function
extern int emcFormat(NMLTYPE type, void * buffer, CMS * cms);

// NML Symbol Lookup Function
extern const char * emc_symbol_lookup(long type);
#define emcSymbolLookup(a) emc_symbol_lookup(a)

#endif
