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

/* Reorder the message types based on priority. The urgent messages will be at
   the top of the format function and get filtered first. */

/*! \page NML message types
  EMC_NULL_TYPE                                ((NMLTYPE) 21)

  Null message - Redundant perhaps ?
*/
#define EMC_NULL                                ((NMLTYPE) 21) // Keep

class EmcNull : public RCS_CMD_MSG
{
public:EmcNull(): RCS_CMD_MSG(EMC_NULL, sizeof(EmcNull)) {};

  void update(CMS *cms);
};

/*! \page NML message types
  And these remain unused. WHY ??
*/
#define EMC_INIT                                ((NMLTYPE) 1901) // fff... Either a global, or not at all.

class EmcInit : public RCS_CMD_MSG
{
public: EmcInit(): RCS_CMD_MSG(EMC_INIT, sizeof(EmcInit)) {};

  void update(CMS *cms);
};

#define EMC_HALT                                ((NMLTYPE) 1902) // AJ: these should be the only one used

class EmcHalt : public RCS_CMD_MSG
{
public: EmcHalt(): RCS_CMD_MSG(EMC_HALT, sizeof(EmcInit)) {};

  void update(CMS *cms);
};

#define EMC_ABORT                               ((NMLTYPE) 1903)

class EmcAbort : public RCS_CMD_MSG
{
public: EmcAbort(): RCS_CMD_MSG(EMC_ABORT, sizeof(EmcInit)) {};

  void update(CMS *cms);
};

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

/*! \page NML message types
  Send a textual error message to the operator.
  The message is put in the errlog buffer to be read by the GUI.
  This allows the controller a generic way to send error messages to
  the operator. */
#define EMC_OPERATOR_ERROR                      ((NMLTYPE) 11) // Keep

class EmcOperatorError : public RCS_CMD_MSG
{
public: EmcOperatorError() : RCS_CMD_MSG(EMC_OPERATOR_ERROR, sizeof(EmcOperatorError)) {};

  void update(CMS *cms);
  int id;
  char error[LINELEN];
};

/*! \page NML message types
  Send a textual information message to the operator.
  This is similiar to EMC_OPERATOR_ERROR message except that the messages are
  sent in situations not necessarily considered to be errors. */
#define EMC_OPERATOR_TEXT                       ((NMLTYPE) 12) // Keep

class EmcOperatorText : public RCS_CMD_MSG
{
public: EmcOperatorText() : RCS_CMD_MSG(EMC_OPERATOR_TEXT, sizeof(EmcOperatorText)) {};

  void update(CMS *cms);
  int id;
  char text[LINELEN];
};

/*! \page NML message types
  Send the URL or filename of a document to display.
  This message is placed in the errlog buffer  to be read by the GUI.
  If the GUI is capable of doing so it will show the operator a
  previously created document, using the URL or filename provided.
  This message is placed in the errlog channel to be read by the GUI.
  This provides a general means of reporting an error from within the
  controller without having to program the GUI to recognize each error type. */
#define EMC_OPERATOR_DISPLAY                    ((NMLTYPE) 13) // Keep

class EmcOperatorDisplay : public RCS_CMD_MSG
{
public: EmcOperatorDisplay(): RCS_CMD_MSG(EMC_OPERATOR_DISPLAY, sizeof(EmcOperatorDisplay)) {};

  void update(CMS *cms);
  int id;
  char display[LINELEN];
};

/*! \page NML message types
  EMC_SET_DEBUG_TYPE                           ((NMLTYPE) 22)

  Sets the debug level in all processes
*/
#define EMC_SET_DEBUG                           ((NMLTYPE) 22) // Keep

class EmcSetDebug : public RCS_CMD_MSG
{
public: EmcSetDebug(): RCS_CMD_MSG(EMC_SET_DEBUG, sizeof(EmcSetDebug)) {};

  void update(CMS *cms);
  int debug;
};

/*! \page NML message types
  EMC_SYSTEM_CMD_TYPE                          ((NMLTYPE) 30)

  Part of the hot command system - Used to execute commands
  specified in the interpreter's input file.
*/
#define EMC_SYSTEM_COMMAND                          ((NMLTYPE) 30) // Keep

class EmcSystemCommand : public RCS_CMD_MSG
{
public: EmcSystemCommand(): RCS_CMD_MSG(EMC_SYSTEM_COMMAND, sizeof(EmcSystemCommand)) {};

  void update(CMS *cms);
  char string[LINELEN];
};



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
/*! \page NML message types
  More infernal INIT, HALT, ABORT messages.. !
*/
#define EMC_MOTION_INIT_TYPE                         ((NMLTYPE) 301)  // AJ remove these, use the general EMC_INIT
#define EMC_MOTION_HALT_TYPE                         ((NMLTYPE) 302)  // AJ remove these, use the general EMC_HALT
#define EMC_MOTION_ABORT_TYPE                        ((NMLTYPE) 303)  // AJ remove these, use the general EMC_ABORT
#define EMC_MOTION_SET_AOUT_TYPE                     ((NMLTYPE) 304)  // Keep - Unused, but could be used for (e.g.) coordinated control of laser power.
#define EMC_MOTION_SET_DOUT_TYPE                     ((NMLTYPE) 305)  // Keep - Coordinated On/Off signals.
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
/*! \page NML message types
*/
#define EMC_LUBE_INIT_TYPE                           ((NMLTYPE) 1501)  // AJ: only one LUBE message should be enough. Maybe even part of an IO message, thus it would be easy for another IO controller without spindle to be used instead.
#define EMC_LUBE_HALT_TYPE                           ((NMLTYPE) 1502)  // PC: Do away with LUBE messages all together. Let the PLC process handle it.
#define EMC_LUBE_ABORT_TYPE                          ((NMLTYPE) 1503)  // PC: The only thing we need to know is if oil level is low - A simple error on STATUS is enough.
#define EMC_LUBE_ON_TYPE                             ((NMLTYPE) 1504)  // Aggregate with machine STATE and let PLC decide.
#define EMC_LUBE_OFF_TYPE                            ((NMLTYPE) 1505)
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

/*! \page NML message types
  With HAL and the scope tool, logging is almost redundant...
*/
#define EMC_LOG_OPEN_TYPE                            ((NMLTYPE) 1904) // Do we need these ?
#define EMC_LOG_START_TYPE                           ((NMLTYPE) 1905) // Can we get away with an aggregate message ?
#define EMC_LOG_STOP_TYPE                            ((NMLTYPE) 1906) // Maybe.
#define EMC_LOG_CLOSE_TYPE                           ((NMLTYPE) 1907) // AJ: not sure if LOGGING is needed, from what I've seen only logging of the MOTION part is done, and that can be done by halscope & the like.

#define EMC_AXIS_STAT                           ((NMLTYPE) 2000)  // Keep

class EmcAxisStatusMessage : public RCS_STAT_MSG
{
public: EmcAxisStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
  int axis;
};

class EmcAxisStatus : public EmcAxisStatusMessage
{
public: EmcAxisStatus();

  void update(CMS *cms);
/* P.C.: These variables *must* have a one to one mapping with emcmotStatus - Makes it much easier to
         track the flow of data across the system. */

  // configuration parameters // P.C.: Split in to a separate config message that is sent *only* when asked for or when config changes.
  unsigned char axisType;       // EMC_AXIS_LINEAR, EMC_AXIS_ANGULAR
  double units;                 // P.C.: Remove - Unit type defined in axisType. Makes no sense having one axis using mm and another using inches.
  double p;
  double i;
  double d;
  double ff0;
  double ff1;
  double ff2;
  double backlash;
  double bias;
  double maxError;
  double deadband;
  double cycleTime;  // P.C.: Remove - Can only have the axis loop running at one speed for all. Move to Traj.
  double inputScale; // P.C.: Steps (or counts) per millimetre.
  double inputOffset;
  double outputScale; // P.C.: Steps (or counts) per millimetre.
  double outputOffset;
  double minPositionLimit; // P.C.: Convert to mm internally..
  double maxPositionLimit; // P.C.: ditto.
  double minOutputLimit;
  double maxOutputLimit;
  double maxFerror;
  double minFerror;
  double homingVel;
  double setup_time;
  double hold_time;
  double homeOffset;
  unsigned char enablePolarity; // P.C.: Keep polarity, but convert to a bool data type
  unsigned char minLimitSwitchPolarity; // P.C.: Would need to add a new data type to libnml..
  unsigned char maxLimitSwitchPolarity;
  unsigned char homeSwitchPolarity;
  unsigned char homingPolarity;
  unsigned char faultPolarity;

  // dynamic status // P.C.: Some of this is required for the HMI.. But not in conjunction with config !
  double setpoint;              // input to axis controller // P.C.: Remove - Internal to PID.
  double ferrorCurrent;         // current following error  // P.C.: Keep
  double ferrorHighMark;        // magnitude of max following error // P.C.: Remove - This records the max ferror and _never_ gets knocked back.
  double output;                // commanded output position  // P.C.: In millimetres
  double input;                 // current input position // P.C.: In millimetres
  unsigned char inpos;          // non-zero means in position // P.C.: Also repeated in traj status - Decide which one is required.
  unsigned char homing;         // non-zero means homing
  unsigned char homed;          // non-zero means has been homed // P.C.: Keep - Convert to bool type.
  unsigned char fault;          // non-zero means axis amp fault // P.C.: Keep - Convert to bool type.
  unsigned char enabled;        // non-zero means enabled // P.C.: Keep - Convert to bool type.
  unsigned char minSoftLimit;   // non-zero means min soft limit exceeded // P.C.: Keep - Convert to bool type.
  unsigned char maxSoftLimit;   // non-zero means max soft limit exceeded // P.C.: Keep - Convert to bool type.
  unsigned char minHardLimit;   // non-zero means min hard limit exceeded // P.C.: Keep - Convert to bool type.
  unsigned char maxHardLimit;   // non-zero means max hard limit exceeded // P.C.: Keep - Convert to bool type.
  unsigned char overrideLimits; // non-zero means limits are overridden // P.C.: Keep ? - Convert to bool type.
  double scale;                 // velocity scale  // P.C.: per axis ? I think not - Remove.
  double alter;                 // external position alter value // P.C.: Remove
};

#define EMC_TRAJ_STAT                           ((NMLTYPE) 2001)  // Keep

class EmcTrajStatusMessage : public RCS_STAT_MSG
{
public: EmcTrajStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
};


class EmcTrajStatus : public EmcTrajStatusMessage
{
public: EmacTrajStatus();

  void update(CMS *cms);

/* P.C.: These variables *must* have a one to one mapping with emcmotStatus - Makes it much easier to
         track the flow of data across the system. */

  double linearUnits;           // units per mm // P.C.: enum metric or imperial.
  double angularUnits;          // units per degree // P.C.: enum degrees or radians
  double cycleTime;             // cycle time, in seconds
  int axes;                     // number of axes in group
  enum EMC_TRAJ_MODE_ENUM mode;	// EMC_TRAJ_MODE_FREE, EMC_TRAJ_MODE_COORD
  int enabled;                  // non-zero means enabled // P.C.: 'spose this has some value... But what ?

  int inpos;                    // non-zero means in position // P.C.: Also repeated in axis status - Decide which one is required.
  int queue;                    // number of pending motions, counting current // P.C.: Remove
  int activeQueue;              // number of motions blending // P.C.: Remove
  int queueFull;                // non-zero means can't accept another motion // P.C.: KEEP - Task waits on this.
  int id;                       // id of the currently executing motion // P.C.: Keep - HMI & task tracks ngc line numbers with this.
  int paused;                   // non-zero means motion paused // P.C.: keep - Convert to bool
  double scale;                 // velocity scale factor // Keep, but remove axis version.

  EmcPose position;             // current commanded position // P.C.: ALL distances are in millimetres or radians. Convert to usr units at input/output only.
  EmcPose actualPosition;       // current actual position, from forward kins
  double velocity;              // system velocity, for subsequent motions
  double acceleration;          // system acceleration, for subsequent motions
  double maxVelocity;           // max system velocity // P.C.: Config - Move to separate message.
  double maxAcceleration;       // system acceleration // P.C.: Config - Move to separate message.

  EmcPose probedPosition;       // last position where probe was tripped.
  int probe_index;              // which wire or digital input is the probe on. // P.C.: Config - Move to separate message.
  int probe_polarity;           // which value should the probe look for to trip. // P.C.: Config - Move to separate message. (bool type)
  int probe_tripped;            // Has the probe been tripped since the last clear. // P.C.: bool type
  int probing;                  // Are we currently looking for a probe signal. // P.C.: bool type
  int probeval;                 // Current value of probe input. // P.C.: bool type - Is this required ?
  int kinematics_type;		// identity=1,serial=2,parallel=3,custom=4 // P.C.: Remove.
};


#define EMC_MOTION_STAT                         ((NMLTYPE) 2002)  // keep

class EmcMotionStatusMessage : public RCS_STAT_MSG
{
public: EmcMotionStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s)
  {
    heartbeat = 0;
  };

  void update(CMS *cms);
  unsigned long int heartbeat;
};

class EmcMotionStatus : public EmcMotionStatusMessage
{
public: EmcMotionStatus() : EmcMotionStatusMessage(EMC_MOTION_STAT, sizeof(EmcMotionStatus)) {};

  void update(CMS *cms);

  EmcTrajStatus traj;
  EmcAxisStatus axis[EMC_AXIS_MAX];
  int debug;			// copy of EMC_DEBUG global // P.C.: Remove - In aggregate status message
};

#define EMC_TASK_STAT                           ((NMLTYPE) 2003)  // keep

class EmcTaskStatusMessage : public RCS_STAT_MSG
{
public: EmcTaskStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s)
  {
    heartbeat = 0;
  };

  void update(CMS *cms);
  unsigned long int heartbeat;
};

class EmcTaskStatus : public EmcTaskStatusMessage
{
public: EmcTaskStatus();

  void update(CMS *cms);

  enum EMC_TASK_MODE_ENUM mode;	// EMC_TASK_MODE_MANUAL, etc.
  enum EMC_TASK_STATE_ENUM state; // EMC_TASK_STATE_ESTOP, etc.
  enum EMC_TASK_EXEC_ENUM  execState; // EMC_DONE,WAITING_FOR_MOTION, etc.
  enum EMC_TASK_INTERP_ENUM interpState; // EMC_IDLE,READING,PAUSED,WAITING
  int motionLine;               // line motion is executing -- may lag  // P.C.: Most of the time, this would be motion.traj.id
  int currentLine;              // line currently executing // P.C.: exectuting in task, interp, or motion ??
  int readLine;                 // line interpreter has read to // P.C.: Keep
  char file[LINELEN]; // P.C.: Keep
  char command[LINELEN];  // P.C.: Keep - This is required in task !
  EmcPose origin;               // origin, in user units, currently active // P.C.: Keep, if only to display work offsets.
  EmcPose toolOffset;           // tool offset, in general pose form // P.C.: Is this needed ?
  int activeGCodes[ACTIVE_G_CODES]; // dialect-specific // P.C.: Keep - May be have the interp sprintf ?
  int activeMCodes[ACTIVE_M_CODES]; // dialect-specific // P.C.: Keep - May be have the interp sprintf ?
  double activeSettings[ACTIVE_SETTINGS]; // dialect-specific // P.C.: Keep - May be have the interp sprintf feedrate & speed ?
  CANON_UNITS programUnits;	// CANON_UNITS_INCHES,MM,CM // P.C.: Keep - but only as inch/metric
  int interpreter_errcode;	// return value from rs274ngc function // P.C.: Keep
};

#define EMC_TOOL_STAT                           ((NMLTYPE) 2004)  // Keep

class EmcToolStatusMessage : public RCS_STAT_MSG
{
public: EmcToolStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
};

class EmcToolStatus : public EmcToolStatusMessage
{
public: EmcToolStatus();
/* P.C.: Aggregate ALL IO status in to a single message. Coolant & lube are trivial enough to be
         handled by the Spindle/Tool process. */

  void update(CMS *cms);
  EmcToolStatus operator = (EMC_TOOL_STAT s); // need this for [] members // P.C.: If tool table is in task status, we don't need this here.
  int toolPrepped;              // tool ready for loading, 0 is no tool // P.C.: Tool ID - Needs to be an int
  int toolInSpindle;            // tool loaded, 0 is no tool// P.C.: ditto
  CANON_TOOL_TABLE toolTable[CANON_TOOL_MAX + 1];// P.C.: task needs to know about the tool table, as does the interp & may be the HMI. Tool process does not. (Should be in task status)
};

#define EMC_AUX_STAT                             ((NMLTYPE) 2005)  // Apart from ESTOP (which is duplicated elsewhwere), this is redundant.

class EmcAuxStatusMessage : public RCS_STAT_MSG
{
public: EmcAuxStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
};

class EmcAuxStatus : public EmcAuxStatusMessage
{
public: EmcAuxStatus();

  void update(CMS *cms);

  int estop;                    // non-zero means estopped
  int estopIn;                  // non-zero means estop button pressed // P.C.: Why does the state of the estop button need to be passed around ?
                                                                       // Surely estop on it's own would be enough..?
};

#define EMC_SPINDLE_STAT                        ((NMLTYPE) 2006)  // Keep - Aggregate with TOOL_STAT

class EmcSpindleStatusMessage : public RCS_STAT_MSG
{
public: EmcSpindleStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
};

class EmcSpindleStatus : public EmcSpindleStatusMessage
{
public:  EmcSpindleStatus();

  void update(CMS *cms);

  double speed;                 // spindle speed in RPMs
  int direction;                // 0 stopped, 1 forward, -1 reverse
  int brake;                    // 0 released, 1 engaged
  int increasing;               // 1 increasing, -1 decreasing, 0 neither
  int enabled;                  // non-zero means enabled
};

#define EMC_COOLANT_STAT                        ((NMLTYPE) 2007)  // Keep - Aggregate with TOOL_STAT ?

class EmcCoolantStatusMessage : public RCS_STAT_MSG
{
public: EmcCoolantStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
};

class EmcCoolantStatus : public EmcCoolantStatusMessage
{
public: EmcCoolantStatus();

  void update(CMS *cms);

  int mist;                     // 0 off, 1 on // P.C.: bool type
  int flood;                    // 0 off, 1 on // P.C.: bool type
};

#define EMC_LUBE_STAT                           ((NMLTYPE) 2008)  // Keep - Aggregate with TOOL_STAT

class EmcLubeStatusMessage : public RCS_STAT_MSG
{
public: EmcLubeStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
};

class EmcLubeStatus : public EmcLubeStatusMessage
{
public: EmcLubeStatus();

  void update(CMS *cms);

  int on;                       // 0 off, 1 on // P.C.: bool type
  int level;                    // 0 low, 1 okay // P.C.: bool type
};


#define EMC_IO_STAT                             ((NMLTYPE) 2009) // Redundant.

class EmcIoStatusMessage : public RCS_STAT_MSG
{
public: EmcIoStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s)
  {
    heartbeat = 0;
  };

  void update(CMS *cms);

  unsigned long int heartbeat;
};

class EmcIoStatus : public EmcIoStatusMessage
{
public: EmcIoStatus() : EmcIoStatusMessage(EMC_IO_STAT, sizeof(EmcIoStatus)) {};

  void update(CMS *cms);

  // top-level stuff
  double cycleTime;
  int debug;			// copy of EMC_DEBUG global // P.C.: Remove - In aggregate status message

  // aggregate of IO-related status classes // P.C.: Most of the data from the child messages could be aggregated in to a single status - We wouldn't need this lot then..
  EmcToolStatus tool;
  EmcSpindleStatus spindle;
  EmcCoolantStatus coolant;
  EmcAuxStatus aux;
  EmcLubeStatus lube;
};


/*! \page NML message types
  EMC_STAT_TYPE                              ((NMLTYPE) 1999)

  Aggregate status message containing all the status messages from
  lower levels. How much is needed by the HMI ?
  Probably very little if you are to take the RMA model to it's
  limits. Certainly plenty can be hacked out without detriment.
*/
#define EMC_STATUS                                ((NMLTYPE) 1999) // Keep

class EmcStatusMessage : public RCS_STAT_MSG
{
public: EmcStatusMessage(NMLTYPE t, size_t s): RCS_STAT_MSG(t, s) {};

  void update(CMS *cms);
};

class EmcStatus : public EmcStatusMessage
{
public: EmcStatus();

  void update(CMS *cms);

  // the top-level EMC_TASK status class
  EmcTaskStatus task;

  // subordinate status classes
  EmcMotionStatus motion;
  EmcIoStatus io;

  // logging status // P.C.: IF halscope is used, these log messages would never get updated.
  char logFile[LINELEN]; // name of file to log to upon close
  int logType;                  // type being logged
  int logSize;                  // size in entries, not bytes
  int logSkip;                  // how many are being skipped
  int logOpen;                  // non-zero means a log is open
  int logStarted;               // non-zero means logging is happening
  int logPoints;                // how many points currently in log

  int debug;			// copy of EMC_DEBUG global
};


// NML formatting function
extern int emcFormat(NMLTYPE type, void * buffer, CMS * cms);

// NML Symbol Lookup Function
extern const char * emc_symbol_lookup(long type);
#define emcSymbolLookup(a) emc_symbol_lookup(a)

#endif
