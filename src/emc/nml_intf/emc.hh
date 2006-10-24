/********************************************************************
* Description: emc.hh
*   Declarations for EMC NML vocabulary
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
#ifndef EMC_HH
#define EMC_HH

#include "config.h"
#include "rcs.hh"
#include "posemath.h"		// PM_POSE, etc.
#include "canon.hh"		// CANON_TOOL_TABLE, CANON_UNITS
#include "emcglb.h"		// EMC_AXIS_MAX
#include "emcpos.h"

// ---------------------
// EMC TYPE DECLARATIONS
// ---------------------

// NML for base EMC

#define EMC_OPERATOR_ERROR_TYPE                      ((NMLTYPE) 11)
#define EMC_OPERATOR_TEXT_TYPE                       ((NMLTYPE) 12)
#define EMC_OPERATOR_DISPLAY_TYPE                    ((NMLTYPE) 13)

#define EMC_NULL_TYPE                                ((NMLTYPE) 21)

#define EMC_SET_DEBUG_TYPE                           ((NMLTYPE) 22)

#define EMC_SYSTEM_CMD_TYPE                          ((NMLTYPE) 30)

// NML for EMC_AXIS

#define EMC_AXIS_SET_AXIS_TYPE                       ((NMLTYPE) 101)
#define EMC_AXIS_SET_UNITS_TYPE                      ((NMLTYPE) 102)
/* gap because of deleted message types */




#define EMC_AXIS_SET_MIN_POSITION_LIMIT_TYPE         ((NMLTYPE) 107)
#define EMC_AXIS_SET_MAX_POSITION_LIMIT_TYPE         ((NMLTYPE) 108)
#define EMC_AXIS_SET_MIN_OUTPUT_LIMIT_TYPE           ((NMLTYPE) 109)
#define EMC_AXIS_SET_MAX_OUTPUT_LIMIT_TYPE           ((NMLTYPE) 110)
#define EMC_AXIS_SET_FERROR_TYPE                     ((NMLTYPE) 111)
#define EMC_AXIS_SET_HOMING_PARAMS_TYPE              ((NMLTYPE) 112)
// gap because of deleted message types

#define EMC_AXIS_SET_MIN_FERROR_TYPE                 ((NMLTYPE) 115)
#define EMC_AXIS_SET_MAX_VELOCITY_TYPE               ((NMLTYPE) 116)
// gap because of deleted message types

#define EMC_AXIS_INIT_TYPE                           ((NMLTYPE) 118)
#define EMC_AXIS_HALT_TYPE                           ((NMLTYPE) 119)
#define EMC_AXIS_ABORT_TYPE                          ((NMLTYPE) 120)
#define EMC_AXIS_ENABLE_TYPE                         ((NMLTYPE) 121)
#define EMC_AXIS_DISABLE_TYPE                        ((NMLTYPE) 122)
#define EMC_AXIS_HOME_TYPE                           ((NMLTYPE) 123)
#define EMC_AXIS_JOG_TYPE                            ((NMLTYPE) 124)
#define EMC_AXIS_INCR_JOG_TYPE                       ((NMLTYPE) 125)
#define EMC_AXIS_ABS_JOG_TYPE                        ((NMLTYPE) 126)
#define EMC_AXIS_ACTIVATE_TYPE                       ((NMLTYPE) 127)
#define EMC_AXIS_DEACTIVATE_TYPE                     ((NMLTYPE) 128)
#define EMC_AXIS_OVERRIDE_LIMITS_TYPE                ((NMLTYPE) 129)
#define EMC_AXIS_SET_OUTPUT_TYPE                     ((NMLTYPE) 130)
#define EMC_AXIS_LOAD_COMP_TYPE                      ((NMLTYPE) 131)
// gap because of deleted message type (EMC_AXIS_ALTER_TYPE)
#define EMC_AXIS_SET_STEP_PARAMS_TYPE                ((NMLTYPE) 133)
#define EMC_AXIS_SET_BACKLASH_TYPE                   ((NMLTYPE) 134)

#define EMC_AXIS_STAT_TYPE                           ((NMLTYPE) 199)

// NML for EMC_TRAJ

// defs for termination conditions

#define EMC_TRAJ_TERM_COND_STOP  1
#define EMC_TRAJ_TERM_COND_BLEND 2

#define EMC_TRAJ_SET_AXES_TYPE                       ((NMLTYPE) 201)
#define EMC_TRAJ_SET_UNITS_TYPE                      ((NMLTYPE) 202)
#define EMC_TRAJ_SET_CYCLE_TIME_TYPE                 ((NMLTYPE) 203)
#define EMC_TRAJ_SET_MODE_TYPE                       ((NMLTYPE) 204)
#define EMC_TRAJ_SET_VELOCITY_TYPE                   ((NMLTYPE) 205)
#define EMC_TRAJ_SET_ACCELERATION_TYPE               ((NMLTYPE) 206)
#define EMC_TRAJ_SET_MAX_VELOCITY_TYPE               ((NMLTYPE) 207)
#define EMC_TRAJ_SET_MAX_ACCELERATION_TYPE           ((NMLTYPE) 208)
#define EMC_TRAJ_SET_SCALE_TYPE                      ((NMLTYPE) 209)
#define EMC_TRAJ_SET_MOTION_ID_TYPE                  ((NMLTYPE) 210)

#define EMC_TRAJ_INIT_TYPE                           ((NMLTYPE) 211)
#define EMC_TRAJ_HALT_TYPE                           ((NMLTYPE) 212)
#define EMC_TRAJ_ENABLE_TYPE                         ((NMLTYPE) 213)
#define EMC_TRAJ_DISABLE_TYPE                        ((NMLTYPE) 214)
#define EMC_TRAJ_ABORT_TYPE                          ((NMLTYPE) 215)
#define EMC_TRAJ_PAUSE_TYPE                          ((NMLTYPE) 216)
#define EMC_TRAJ_STEP_TYPE                           ((NMLTYPE) 217)
#define EMC_TRAJ_RESUME_TYPE                         ((NMLTYPE) 218)
#define EMC_TRAJ_DELAY_TYPE                          ((NMLTYPE) 219)
#define EMC_TRAJ_LINEAR_MOVE_TYPE                    ((NMLTYPE) 220)
#define EMC_TRAJ_CIRCULAR_MOVE_TYPE                  ((NMLTYPE) 221)
#define EMC_TRAJ_SET_TERM_COND_TYPE                  ((NMLTYPE) 222)
#define EMC_TRAJ_SET_OFFSET_TYPE                     ((NMLTYPE) 223)
#define EMC_TRAJ_SET_ORIGIN_TYPE                     ((NMLTYPE) 224)
#define EMC_TRAJ_SET_HOME_TYPE                       ((NMLTYPE) 225)
/* gap because of removed messages */

#define EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG_TYPE       ((NMLTYPE) 228)
#define EMC_TRAJ_PROBE_TYPE                          ((NMLTYPE) 229)
#define EMC_TRAJ_SET_TELEOP_ENABLE_TYPE              ((NMLTYPE) 230)
#define EMC_TRAJ_SET_TELEOP_VECTOR_TYPE              ((NMLTYPE) 231)
#define EMC_TRAJ_SET_SPINDLESYNC_TYPE                ((NMLTYPE) 232)
#define EMC_TRAJ_SET_SPINDLE_SCALE_TYPE              ((NMLTYPE) 233)
#define EMC_TRAJ_SET_FEED_OVERRIDE_TYPE              ((NMLTYPE) 234)
#define EMC_TRAJ_SET_SPINDLE_OVERRIDE_TYPE           ((NMLTYPE) 235)

#define EMC_TRAJ_STAT_TYPE                           ((NMLTYPE) 299)

// EMC_MOTION aggregate class type declaration

#define EMC_MOTION_INIT_TYPE                         ((NMLTYPE) 301)
#define EMC_MOTION_HALT_TYPE                         ((NMLTYPE) 302)
#define EMC_MOTION_ABORT_TYPE                        ((NMLTYPE) 303)
#define EMC_MOTION_SET_AOUT_TYPE                     ((NMLTYPE) 304)
#define EMC_MOTION_SET_DOUT_TYPE                     ((NMLTYPE) 305)
#define EMC_MOTION_ADAPTIVE_TYPE                     ((NMLTYPE) 306)

#define EMC_MOTION_STAT_TYPE                         ((NMLTYPE) 399)

// NML for EMC_TASK

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
#define EMC_TASK_PLAN_SET_OPTIONAL_STOP_TYPE         ((NMLTYPE) 517)
#define EMC_TASK_PLAN_SET_BLOCK_DELETE_TYPE          ((NMLTYPE) 518)
#define EMC_TASK_PLAN_OPTIONAL_STOP_TYPE             ((NMLTYPE) 519)

#define EMC_TASK_STAT_TYPE                           ((NMLTYPE) 599)

// EMC_TOOL type declarations

#define EMC_TOOL_INIT_TYPE                           ((NMLTYPE) 1101)
#define EMC_TOOL_HALT_TYPE                           ((NMLTYPE) 1102)
#define EMC_TOOL_ABORT_TYPE                          ((NMLTYPE) 1103)
#define EMC_TOOL_PREPARE_TYPE                        ((NMLTYPE) 1104)
#define EMC_TOOL_LOAD_TYPE                           ((NMLTYPE) 1105)
#define EMC_TOOL_UNLOAD_TYPE                         ((NMLTYPE) 1106)
#define EMC_TOOL_LOAD_TOOL_TABLE_TYPE                ((NMLTYPE) 1107)
#define EMC_TOOL_SET_OFFSET_TYPE                     ((NMLTYPE) 1108)

#define EMC_TOOL_STAT_TYPE                           ((NMLTYPE) 1199)

// EMC_AUX type declarations

#define EMC_AUX_INIT_TYPE                             ((NMLTYPE) 1201)
#define EMC_AUX_HALT_TYPE                             ((NMLTYPE) 1202)
#define EMC_AUX_ABORT_TYPE                            ((NMLTYPE) 1203)
#define EMC_AUX_DIO_WRITE_TYPE                        ((NMLTYPE) 1204)
#define EMC_AUX_AIO_WRITE_TYPE                        ((NMLTYPE) 1205)
#define EMC_AUX_ESTOP_ON_TYPE                         ((NMLTYPE) 1206)
#define EMC_AUX_ESTOP_OFF_TYPE                        ((NMLTYPE) 1207)
#define EMC_AUX_ESTOP_RESET_TYPE                      ((NMLTYPE) 1208)

#define EMC_AUX_STAT_TYPE                             ((NMLTYPE) 1299)

// EMC_SPINDLE type declarations

#define EMC_SPINDLE_INIT_TYPE                        ((NMLTYPE) 1301)
#define EMC_SPINDLE_HALT_TYPE                        ((NMLTYPE) 1302)
#define EMC_SPINDLE_ABORT_TYPE                       ((NMLTYPE) 1303)
#define EMC_SPINDLE_ON_TYPE                          ((NMLTYPE) 1304)
#define EMC_SPINDLE_OFF_TYPE                         ((NMLTYPE) 1305)
#define EMC_SPINDLE_FORWARD_TYPE                     ((NMLTYPE) 1306)
#define EMC_SPINDLE_REVERSE_TYPE                     ((NMLTYPE) 1307)
#define EMC_SPINDLE_STOP_TYPE                        ((NMLTYPE) 1308)
#define EMC_SPINDLE_INCREASE_TYPE                    ((NMLTYPE) 1309)
#define EMC_SPINDLE_DECREASE_TYPE                    ((NMLTYPE) 1310)
#define EMC_SPINDLE_CONSTANT_TYPE                    ((NMLTYPE) 1311)
#define EMC_SPINDLE_BRAKE_RELEASE_TYPE               ((NMLTYPE) 1312)
#define EMC_SPINDLE_BRAKE_ENGAGE_TYPE                ((NMLTYPE) 1313)
#define EMC_SPINDLE_ENABLE_TYPE                      ((NMLTYPE) 1314)
#define EMC_SPINDLE_DISABLE_TYPE                     ((NMLTYPE) 1315)

#define EMC_SPINDLE_STAT_TYPE                        ((NMLTYPE) 1399)

// EMC_COOLANT type declarations

#define EMC_COOLANT_INIT_TYPE                        ((NMLTYPE) 1401)
#define EMC_COOLANT_HALT_TYPE                        ((NMLTYPE) 1402)
#define EMC_COOLANT_ABORT_TYPE                       ((NMLTYPE) 1403)
#define EMC_COOLANT_MIST_ON_TYPE                     ((NMLTYPE) 1404)
#define EMC_COOLANT_MIST_OFF_TYPE                    ((NMLTYPE) 1405)
#define EMC_COOLANT_FLOOD_ON_TYPE                    ((NMLTYPE) 1406)
#define EMC_COOLANT_FLOOD_OFF_TYPE                   ((NMLTYPE) 1407)

#define EMC_COOLANT_STAT_TYPE                        ((NMLTYPE) 1499)

// EMC_LUBE type declarations

#define EMC_LUBE_INIT_TYPE                           ((NMLTYPE) 1501)
#define EMC_LUBE_HALT_TYPE                           ((NMLTYPE) 1502)
#define EMC_LUBE_ABORT_TYPE                          ((NMLTYPE) 1503)
#define EMC_LUBE_ON_TYPE                             ((NMLTYPE) 1504)
#define EMC_LUBE_OFF_TYPE                            ((NMLTYPE) 1505)

#define EMC_LUBE_STAT_TYPE                           ((NMLTYPE) 1599)

// EMC IO point configuration declarations

#define EMC_SET_DIO_INDEX_TYPE                       ((NMLTYPE) 5001)
#define EMC_SET_AIO_INDEX_TYPE                       ((NMLTYPE) 5002)


// digital IO point indices
enum {
    // spindle
    EMC_SET_DIO_INDEX_SPINDLE_FORWARD = 5101,
    EMC_SET_DIO_INDEX_SPINDLE_REVERSE,
    EMC_SET_DIO_INDEX_SPINDLE_DECREASE,
    EMC_SET_DIO_INDEX_SPINDLE_INCREASE,
    EMC_SET_DIO_INDEX_SPINDLE_BRAKE,
    EMC_SET_DIO_INDEX_SPINDLE_ENABLE,
    // coolant
    EMC_SET_DIO_INDEX_COOLANT_MIST,
    EMC_SET_DIO_INDEX_COOLANT_FLOOD,
    // lube
    EMC_SET_DIO_INDEX_LUBE_SENSE,
    // aux
    EMC_SET_DIO_INDEX_ESTOP_SENSE,
    EMC_SET_DIO_INDEX_ESTOP_WRITE
};

// analog IO point indices
enum {
    // spindle
    EMC_SET_AIO_INDEX_SPINDLE_ON = 5201
};

// EMC_IO aggregate class type declaration

#define EMC_IO_INIT_TYPE                             ((NMLTYPE) 1601)
#define EMC_IO_HALT_TYPE                             ((NMLTYPE) 1602)
#define EMC_IO_ABORT_TYPE                            ((NMLTYPE) 1603)
#define EMC_IO_SET_CYCLE_TIME_TYPE                   ((NMLTYPE) 1604)

#define EMC_IO_STAT_TYPE                             ((NMLTYPE) 1699)

// EMC aggregate class type declaration

// these are placeholders
#define EMC_LOG_TYPE_IO_CMD      21	// command into EMC IO controller
#define EMC_LOG_TYPE_TASK_CMD    51	// command into EMC Task controller

#define EMC_INIT_TYPE                                ((NMLTYPE) 1901)
#define EMC_HALT_TYPE                                ((NMLTYPE) 1902)
#define EMC_ABORT_TYPE                               ((NMLTYPE) 1903)

#define EMC_STAT_TYPE                                ((NMLTYPE) 1999)

// types for EMC_TASK mode
enum EMC_TASK_MODE_ENUM {
    EMC_TASK_MODE_MANUAL = 1,
    EMC_TASK_MODE_AUTO = 2,
    EMC_TASK_MODE_MDI = 3
};

// types for EMC_TASK state
enum EMC_TASK_STATE_ENUM {
    EMC_TASK_STATE_ESTOP = 1,
    EMC_TASK_STATE_ESTOP_RESET = 2,
    EMC_TASK_STATE_OFF = 3,
    EMC_TASK_STATE_ON = 4
};

// types for EMC_TASK execState
enum EMC_TASK_EXEC_ENUM {
    EMC_TASK_EXEC_ERROR = 1,
    EMC_TASK_EXEC_DONE = 2,
    EMC_TASK_EXEC_WAITING_FOR_MOTION = 3,
    EMC_TASK_EXEC_WAITING_FOR_MOTION_QUEUE = 4,
    EMC_TASK_EXEC_WAITING_FOR_IO = 5,
    EMC_TASK_EXEC_WAITING_FOR_PAUSE = 6,
    EMC_TASK_EXEC_WAITING_FOR_MOTION_AND_IO = 7,
    EMC_TASK_EXEC_WAITING_FOR_DELAY = 8,
    EMC_TASK_EXEC_WAITING_FOR_SYSTEM_CMD = 9
};

// types for EMC_TASK interpState
enum EMC_TASK_INTERP_ENUM {
    EMC_TASK_INTERP_IDLE = 1,
    EMC_TASK_INTERP_READING = 2,
    EMC_TASK_INTERP_PAUSED = 3,
    EMC_TASK_INTERP_WAITING = 4
};

// types for motion control
enum EMC_TRAJ_MODE_ENUM {
    EMC_TRAJ_MODE_FREE = 1,	// independent-axis motion,
    EMC_TRAJ_MODE_COORD = 2,	// coordinated-axis motion,
    EMC_TRAJ_MODE_TELEOP = 3	// velocity based world coordinates motion,
};

#define EMC_MOTION_TYPE_TRAVERSE 1
#define EMC_MOTION_TYPE_FEED 2
#define EMC_MOTION_TYPE_ARC 3
#define EMC_MOTION_TYPE_TOOLCHANGE 4
#define EMC_MOTION_TYPE_PROBING 5

// --------------
// EMC VOCABULARY
// --------------

// NML formatting function
extern int emcFormat(NMLTYPE type, void *buffer, CMS * cms);

// NML Symbol Lookup Function
extern const char *emc_symbol_lookup(long type);
#define emcSymbolLookup(a) emc_symbol_lookup(a)

// decls for command line args-- mains are responsible for setting these
// so that other modules can get cmd line args for ad hoc processing
extern int Argc;
extern char **Argv;

// ------------------------
// IMPLEMENTATION FUNCTIONS
// ------------------------

// implementation functions for EMC error, message types
// intended to be implemented in main() file, by writing to NML buffer

// print an error
extern int emcOperatorError(int id, const char *fmt, ...);

// print general text
extern int emcOperatorText(int id, const char *fmt, ...);

// print note to operator
extern int emcOperatorDisplay(int id, const char *fmt, ...);

// implementation functions for EMC_AXIS types

extern int emcAxisSetAxis(int axis, unsigned char axisType);
extern int emcAxisSetUnits(int axis, double units);
extern int emcAxisSetBacklash(int axis, double backlash);
extern int emcAxisSetInterpolationRate(int axis, int rate);
extern int emcAxisSetMinPositionLimit(int axis, double limit);
extern int emcAxisSetMaxPositionLimit(int axis, double limit);
extern int emcAxisSetMotorOffset(int axis, double offset);
extern int emcAxisSetMinOutputLimit(int axis, double limit);
extern int emcAxisSetMaxOutputLimit(int axis, double limit);
extern int emcAxisSetFerror(int axis, double ferror);
extern int emcAxisSetMinFerror(int axis, double ferror);
extern int emcAxisSetHomingParams(int axis, double home, double offset,
				  double search_vel, double latch_vel,
				  int use_index, int ignore_limits,
				  int is_shared, int home_sequence);
extern int emcAxisSetMaxVelocity(int axis, double vel);
extern int emcAxisSetMaxAcceleration(int axis, double acc);

extern int emcAxisInit(int axis);
extern int emcAxisHalt(int axis);
extern int emcAxisAbort(int axis);
extern int emcAxisEnable(int axis);
extern int emcAxisDisable(int axis);
extern int emcAxisHome(int axis);
extern int emcAxisJog(int axis, double vel);
extern int emcAxisIncrJog(int axis, double incr, double vel);
extern int emcAxisAbsJog(int axis, double pos, double vel);
extern int emcAxisActivate(int axis);
extern int emcAxisDeactivate(int axis);
extern int emcAxisOverrideLimits(int axis);
extern int emcAxisSetOutput(int axis, double output);
extern int emcAxisLoadComp(int axis, const char *file, int type);


class EMC_AXIS_STAT;		// forward decl
extern int emcAxisUpdate(EMC_AXIS_STAT stat[], int numAxes);

// implementation functions for EMC_TRAJ types

extern int emcTrajSetAxes(int axes);
extern int emcTrajSetUnits(double linearUnits, double angularUnits);
extern int emcTrajSetCycleTime(double cycleTime);
extern int emcTrajSetMode(int axes);
extern int emcTrajSetTeleopVector(EmcPose vel);
extern int emcTrajSetVelocity(double vel, double ini_maxvel);
extern int emcTrajSetAcceleration(double acc);
extern int emcTrajSetMaxVelocity(double vel);
extern int emcTrajSetMaxAcceleration(double acc);
extern int emcTrajSetScale(double scale);
extern int emcTrajSetFeedOverride(unsigned char mode);
extern int emcTrajSetSpindleScale(double scale);
extern int emcTrajSetSpindleOverride(unsigned char mode);
extern int emcTrajSetAdaptiveFeed(unsigned char enable);
extern int emcTrajSetMotionId(int id);
extern double emcTrajGetLinearUnits();
extern double emcTrajGetAngularUnits();

extern int emcTrajInit();
extern int emcTrajHalt();
extern int emcTrajEnable();
extern int emcTrajDisable();
extern int emcTrajAbort();
extern int emcTrajPause();
extern int emcTrajStep();
extern int emcTrajResume();
extern int emcTrajDelay(double delay);
extern int emcTrajLinearMove(EmcPose end, int type, double vel, double
        ini_maxvel, double acc);
extern int emcTrajCircularMove(EmcPose end, PM_CARTESIAN center, PM_CARTESIAN
        normal, int turn, int type, double vel, double ini_maxvel, double acc);
extern int emcTrajSetTermCond(int cond, double tolerance);
extern int emcTrajSetSpindleSync(double spindlesync);
extern int emcTrajSetOffset(EmcPose offset);
extern int emcTrajSetOrigin(EmcPose origin);
extern int emcTrajSetHome(EmcPose home);
extern int emcTrajClearProbeTrippedFlag();
extern int emcTrajProbe(EmcPose pos, int type, double vel, 
	double ini_maxvel, double acc);

class EMC_TRAJ_STAT;		// forward decl
extern int emcTrajUpdate(EMC_TRAJ_STAT * stat);

// implementation functions for EMC_MOTION aggregate types

extern int emcMotionInit();
extern int emcMotionHalt();
extern int emcMotionAbort();
extern int emcMotionSetDebug(int debug);
extern int emcMotionSetAout(unsigned char index, double start, double end,
                            unsigned char now);
extern int emcMotionSetDout(unsigned char index, unsigned char start,
			    unsigned char end, unsigned char now);

class EMC_MOTION_STAT;		// forward decl
extern int emcMotionUpdate(EMC_MOTION_STAT * stat);

// implementation functions for EMC_TASK types

extern int emcTaskInit();
extern int emcTaskHalt();
extern int emcTaskAbort();
extern int emcTaskSetMode(int mode);
extern int emcTaskSetState(int state);
extern int emcTaskPlanInit();
extern int emcTaskPlanSetWait();
extern int emcTaskPlanIsWait();
extern int emcTaskPlanClearWait();
extern int emcTaskPlanSynch();
extern int emcTaskPlanSetOptionalStop(bool state);
extern int emcTaskPlanSetBlockDelete(bool state);
extern int emcTaskPlanExit();
extern int emcTaskPlanOpen(const char *file);
extern int emcTaskPlanRead();
extern int emcTaskPlanRun(int line);
extern int emcTaskPlanExecute(const char *command);
extern int emcTaskPlanPause();
extern int emcTaskPlanResume();
extern int emcTaskPlanClose();

extern int emcTaskPlanLine();
extern int emcTaskPlanCommand(char *cmd);

class EMC_TASK_STAT;		// forward decl
extern int emcTaskUpdate(EMC_TASK_STAT * stat);

// implementation functions for EMC_TOOL types

extern int emcToolInit();
extern int emcToolHalt();
extern int emcToolAbort();
extern int emcToolPrepare(int tool);
extern int emcToolLoad();
extern int emcToolUnload();
extern int emcToolLoadToolTable(const char *file);
extern int emcToolSetOffset(int tool, double length, double diameter);

extern int emcToolSetToolTableFile(const char *file);

class EMC_TOOL_STAT;		// forward decl
extern int emcToolUpdate(EMC_TOOL_STAT * stat);

// implementation functions for EMC_AUX types

extern int emcAuxInit();
extern int emcAuxHalt();
extern int emcAuxAbort();
extern int emcAuxDioWrite(int index, int value);
extern int emcAuxAioWrite(int index, double value);
extern int emcAuxEstopOn();
extern int emcAuxEstopOff();
extern int emcAuxEstopReset();

class EMC_AUX_STAT;		// forward decl
extern int emcAuxUpdate(EMC_AUX_STAT * stat);

// implementation functions for EMC_SPINDLE types

extern int emcSpindleInit();
extern int emcSpindleHalt();
extern int emcSpindleAbort();
extern int emcSpindleOn(double speed);
extern int emcSpindleOff();
extern int emcSpindleForward();
extern int emcSpindleReverse();
extern int emcSpindleStop();
extern int emcSpindleIncrease();
extern int emcSpindleDecrease();
extern int emcSpindleConstant();
extern int emcSpindleBrakeRelease();
extern int emcSpindleBrakeEngage();
extern int emcSpindleEnable();
extern int emcSpindleDisable();

extern int emcSpindleSetForwardIndex(int index);
extern int emcSpindleSetReverseIndex(int index);
extern int emcSpindleSetDecreaseIndex(int index);
extern int emcSpindleSetIncreaseIndex(int index);
extern int emcSpindleSetBrakeIndex(int index);
extern int emcSpindleSetEnableIndex(int index);

extern int emcSpindleSetOnIndex(int index);
extern int emcMinVoltsPerRpm(double volts);
extern int emcMaxVoltsPerRpm(double volts);

/*! \todo - FIXME - not used remove */
extern int emcSpindleSetOffWait(double wait);
extern int emcSpindleSetOnWait(double wait);

class EMC_SPINDLE_STAT;		// forward decl
extern int emcSpindleUpdate(EMC_SPINDLE_STAT * stat);

// implementation functions for EMC_COOLANT types

extern int emcCoolantInit();
extern int emcCoolantHalt();
extern int emcCoolantAbort();
extern int emcCoolantMistOn();
extern int emcCoolantMistOff();
extern int emcCoolantFloodOn();
extern int emcCoolantFloodOff();

class EMC_COOLANT_STAT;		// forward decl
extern int emcCoolantUpdate(EMC_COOLANT_STAT * stat);

// implementation functions for EMC_LUBE types

extern int emcLubeInit();
extern int emcLubeHalt();
extern int emcLubeAbort();
extern int emcLubeOn();
extern int emcLubeOff();

class EMC_LUBE_STAT;		// forward decl
extern int emcLubeUpdate(EMC_LUBE_STAT * stat);

// implementation functions for EMC_IO types

extern int emcIoInit();
extern int emcIoHalt();
extern int emcIoAbort();
extern int emcIoSetCycleTime(double cycleTime);
extern int emcIoSetDebug(int debug);

class EMC_IO_STAT;		// forward decl
class EMC_STAT;			// forward decl
extern int emcIoUpdate(EMC_IO_STAT * stat);

// implementation functions for EMC aggregate types

extern int emcInit();
extern int emcHalt();
extern int emcAbort();

extern int emcUpdate(EMC_STAT * stat);

// ------------------
// CLASS DECLARATIONS
// ------------------

// declarations for EMC general classes

/**
 * Send a textual error message to the operator.
 * The message is put in the errlog buffer to be read by the GUI.
 * This allows the controller a generic way to send error messages to
 * the operator.
 */
class EMC_OPERATOR_ERROR:public RCS_CMD_MSG {
  public:
    EMC_OPERATOR_ERROR():RCS_CMD_MSG(EMC_OPERATOR_ERROR_TYPE,
				     sizeof(EMC_OPERATOR_ERROR)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int id;
    char error[LINELEN];
};

/**
 * Send a textual information message to the operator.
 * This is similiar to EMC_OPERATOR_ERROR message except that the messages are
 * sent in situations not necessarily considered to be errors.
 */
class EMC_OPERATOR_TEXT:public RCS_CMD_MSG {
  public:
    EMC_OPERATOR_TEXT():RCS_CMD_MSG(EMC_OPERATOR_TEXT_TYPE,
				    sizeof(EMC_OPERATOR_TEXT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int id;
    char text[LINELEN];
};

/**
 * Send the URL or filename of a document to display.
 * This message is placed in the errlog buffer  to be read by the GUI.
 * If the GUI is capable of doing so it will show the operator a
 * previously created document, using the URL or filename provided.
 * This message is placed in the errlog channel to be read by the GUI.
 * This provides a general means of reporting an error from within the
 * controller without having to program the GUI to recognize each error type.
 */
class EMC_OPERATOR_DISPLAY:public RCS_CMD_MSG {
  public:
    EMC_OPERATOR_DISPLAY():RCS_CMD_MSG(EMC_OPERATOR_DISPLAY_TYPE,
				       sizeof(EMC_OPERATOR_DISPLAY)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int id;
    char display[LINELEN];
};

#define EMC_SYSTEM_CMD_LEN 256
/*
  execute a system command
*/
class EMC_SYSTEM_CMD:public RCS_CMD_MSG {
  public:
    EMC_SYSTEM_CMD():RCS_CMD_MSG(EMC_SYSTEM_CMD_TYPE,
				 sizeof(EMC_SYSTEM_CMD)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    char string[EMC_SYSTEM_CMD_LEN];
};

class EMC_NULL:public RCS_CMD_MSG {
  public:
    EMC_NULL():RCS_CMD_MSG(EMC_NULL_TYPE, sizeof(EMC_NULL)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SET_DEBUG:public RCS_CMD_MSG {
  public:
    EMC_SET_DEBUG():RCS_CMD_MSG(EMC_SET_DEBUG_TYPE, sizeof(EMC_SET_DEBUG)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int debug;
};

// declarations for EMC_AXIS classes

/*
 * AXIS command base class.
 * This is the base class for all commands that operate on a single axis.
 * The axis parameter specifies which axis the command affects.
 * These commands are sent to the emcCommand buffer to be read by the
 * TASK program that will then pass along corresponding messages to the
 * motion system.
 */
class EMC_AXIS_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_AXIS_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    // 0 = X, 1 = Y, 2 = Z, etc.
    int axis;
};

// values for EMC_AXIS_SET_AXIS, axisType
#define EMC_AXIS_LINEAR 1
#define EMC_AXIS_ANGULAR 2

/**
 * Set the axis type to linear or angular.
 * Similiar to the AXIS_TYPE field in the ".ini" file.
 */
class EMC_AXIS_SET_AXIS:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_AXIS():EMC_AXIS_CMD_MSG(EMC_AXIS_SET_AXIS_TYPE,
					 sizeof(EMC_AXIS_SET_AXIS)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    // EMC_AXIS_LINEAR, EMC_AXIS_ANGULAR
    unsigned char axisType;
};

/**
 * Set the units conversion factor.
 * @see EMC_AXIS_SET_INPUT_SCALE
 */
class EMC_AXIS_SET_UNITS:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_UNITS():EMC_AXIS_CMD_MSG(EMC_AXIS_SET_UNITS_TYPE,
					  sizeof(EMC_AXIS_SET_UNITS)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    // units per mm, deg for linear, angular
    double units;
};


/**
 * Set the Axis backlash.
 * This command sets the backlash value.
 */
class EMC_AXIS_SET_BACKLASH:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_BACKLASH():EMC_AXIS_CMD_MSG(EMC_AXIS_SET_BACKLASH_TYPE,
					     sizeof(EMC_AXIS_SET_BACKLASH))
    {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double backlash;
};

class EMC_AXIS_SET_MIN_POSITION_LIMIT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_MIN_POSITION_LIMIT():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_MIN_POSITION_LIMIT_TYPE,
	 sizeof(EMC_AXIS_SET_MIN_POSITION_LIMIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double limit;
};

class EMC_AXIS_SET_MAX_POSITION_LIMIT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_MAX_POSITION_LIMIT():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_MAX_POSITION_LIMIT_TYPE,
	 sizeof(EMC_AXIS_SET_MAX_POSITION_LIMIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double limit;
};

class EMC_AXIS_SET_MIN_OUTPUT_LIMIT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_MIN_OUTPUT_LIMIT():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_MIN_OUTPUT_LIMIT_TYPE,
	 sizeof(EMC_AXIS_SET_MIN_OUTPUT_LIMIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double limit;
};

class EMC_AXIS_SET_MAX_OUTPUT_LIMIT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_MAX_OUTPUT_LIMIT():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_MAX_OUTPUT_LIMIT_TYPE,
	 sizeof(EMC_AXIS_SET_MAX_OUTPUT_LIMIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double limit;
};

class EMC_AXIS_SET_FERROR:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_FERROR():EMC_AXIS_CMD_MSG(EMC_AXIS_SET_FERROR_TYPE,
					   sizeof(EMC_AXIS_SET_FERROR)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double ferror;
};

class EMC_AXIS_SET_MIN_FERROR:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_MIN_FERROR():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_MIN_FERROR_TYPE, sizeof(EMC_AXIS_SET_MIN_FERROR)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double ferror;
};

class EMC_AXIS_SET_HOMING_PARAMS:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_HOMING_PARAMS():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_HOMING_PARAMS_TYPE,
	 sizeof(EMC_AXIS_SET_HOMING_PARAMS)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double home;
    double offset;
    double search_vel;
    double latch_vel;
    int use_index;
    int ignore_limits;
    int is_shared;
    int home_sequence;
};

class EMC_AXIS_SET_MAX_VELOCITY:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_MAX_VELOCITY():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_MAX_VELOCITY_TYPE,
	 sizeof(EMC_AXIS_SET_MAX_VELOCITY)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double vel;
};

class EMC_AXIS_INIT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_INIT():EMC_AXIS_CMD_MSG(EMC_AXIS_INIT_TYPE,
				     sizeof(EMC_AXIS_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_HALT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_HALT():EMC_AXIS_CMD_MSG(EMC_AXIS_HALT_TYPE,
				     sizeof(EMC_AXIS_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_ABORT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_ABORT():EMC_AXIS_CMD_MSG(EMC_AXIS_ABORT_TYPE,
				      sizeof(EMC_AXIS_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_ENABLE:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_ENABLE():EMC_AXIS_CMD_MSG(EMC_AXIS_ENABLE_TYPE,
				       sizeof(EMC_AXIS_ENABLE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_DISABLE:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_DISABLE():EMC_AXIS_CMD_MSG(EMC_AXIS_DISABLE_TYPE,
					sizeof(EMC_AXIS_DISABLE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_HOME:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_HOME():EMC_AXIS_CMD_MSG(EMC_AXIS_HOME_TYPE,
				     sizeof(EMC_AXIS_HOME)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_JOG:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_JOG():EMC_AXIS_CMD_MSG(EMC_AXIS_JOG_TYPE,
				    sizeof(EMC_AXIS_JOG)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double vel;
};

class EMC_AXIS_INCR_JOG:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_INCR_JOG():EMC_AXIS_CMD_MSG(EMC_AXIS_INCR_JOG_TYPE,
					 sizeof(EMC_AXIS_INCR_JOG)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double incr;
    double vel;
};

class EMC_AXIS_ABS_JOG:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_ABS_JOG():EMC_AXIS_CMD_MSG(EMC_AXIS_ABS_JOG_TYPE,
					sizeof(EMC_AXIS_ABS_JOG)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double pos;
    double vel;
};

class EMC_AXIS_ACTIVATE:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_ACTIVATE():EMC_AXIS_CMD_MSG(EMC_AXIS_ACTIVATE_TYPE,
					 sizeof(EMC_AXIS_ACTIVATE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_DEACTIVATE:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_DEACTIVATE():EMC_AXIS_CMD_MSG(EMC_AXIS_DEACTIVATE_TYPE,
					   sizeof(EMC_AXIS_DEACTIVATE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_OVERRIDE_LIMITS:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_OVERRIDE_LIMITS():EMC_AXIS_CMD_MSG
	(EMC_AXIS_OVERRIDE_LIMITS_TYPE, sizeof(EMC_AXIS_OVERRIDE_LIMITS)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AXIS_SET_OUTPUT:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_OUTPUT():EMC_AXIS_CMD_MSG(EMC_AXIS_SET_OUTPUT_TYPE,
					   sizeof(EMC_AXIS_SET_OUTPUT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double output;		// value for output, in physical units
    // (volts)
};

class EMC_AXIS_LOAD_COMP:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_LOAD_COMP():EMC_AXIS_CMD_MSG(EMC_AXIS_LOAD_COMP_TYPE,
					  sizeof(EMC_AXIS_LOAD_COMP)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    char file[LINELEN];
    int type; // type of the comp file. type==0 means nom, forw, rev triplets
              // type != 0 means nom, forw_trim, rev_trim triplets
};


/**
 * Set the step parameters.
 * This command sets the setup time of the direction signal,
 * and the hold time of the step signal.
 */
class EMC_AXIS_SET_STEP_PARAMS:public EMC_AXIS_CMD_MSG {
  public:
    EMC_AXIS_SET_STEP_PARAMS():EMC_AXIS_CMD_MSG
	(EMC_AXIS_SET_STEP_PARAMS_TYPE, sizeof(EMC_AXIS_SET_STEP_PARAMS)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double setup_time;
    double hold_time;
};

// AXIS status base class
class EMC_AXIS_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_AXIS_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int axis;
};

class EMC_AXIS_STAT:public EMC_AXIS_STAT_MSG {
  public:
    EMC_AXIS_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);

    // configuration parameters
    unsigned char axisType;	// EMC_AXIS_LINEAR, EMC_AXIS_ANGULAR
    double units;		// units per mm, deg for linear, angular
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
    double cycleTime;
    double inputScale;
    double inputOffset;
    double outputScale;
    double outputOffset;
    double minPositionLimit;
    double maxPositionLimit;
    double minOutputLimit;
    double maxOutputLimit;
    double maxFerror;
    double minFerror;
    /*! \todo FIXME - homingVel has been superceded */
    double homingVel;
    double setup_time;
    double hold_time;
    double homeOffset;

    // dynamic status
    /*! \todo FIXME - is this the position cmd from control to PID, or
       something else? */
    double setpoint;		// input to axis controller
    double ferrorCurrent;	// current following error
    double ferrorHighMark;	// magnitude of max following error
    /*! \todo FIXME - is this really position, or the DAC output? */
    double output;		// commanded output position
    double input;		// current input position
    unsigned char inpos;	// non-zero means in position
    unsigned char homing;	// non-zero means homing
    unsigned char homed;	// non-zero means has been homed
    unsigned char fault;	// non-zero means axis amp fault
    unsigned char enabled;	// non-zero means enabled
    unsigned char minSoftLimit;	// non-zero means min soft limit exceeded
    unsigned char maxSoftLimit;	// non-zero means max soft limit exceeded
    unsigned char minHardLimit;	// non-zero means min hard limit exceeded
    unsigned char maxHardLimit;	// non-zero means max hard limit exceeded
    unsigned char overrideLimits;	// non-zero means limits are
    // overridden
    double scale;		// velocity scale
};

// declarations for EMC_TRAJ classes

// EMC_TRAJ command base class
class EMC_TRAJ_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_TRAJ_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_SET_UNITS:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_UNITS():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_UNITS_TYPE,
					  sizeof(EMC_TRAJ_SET_UNITS)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double linearUnits;		// units per mm
    double angularUnits;	// units per degree
};

class EMC_TRAJ_SET_AXES:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_AXES():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_AXES_TYPE,
					 sizeof(EMC_TRAJ_SET_AXES)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int axes;
};

class EMC_TRAJ_SET_CYCLE_TIME:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_CYCLE_TIME():EMC_TRAJ_CMD_MSG
	(EMC_TRAJ_SET_CYCLE_TIME_TYPE, sizeof(EMC_TRAJ_SET_CYCLE_TIME)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double cycleTime;
};

class EMC_TRAJ_SET_MODE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_MODE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_MODE_TYPE,
					 sizeof(EMC_TRAJ_SET_MODE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    enum EMC_TRAJ_MODE_ENUM mode;
};

class EMC_TRAJ_SET_VELOCITY:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_VELOCITY():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_VELOCITY_TYPE,
					     sizeof(EMC_TRAJ_SET_VELOCITY))
    {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double velocity;
    double ini_maxvel;
};

class EMC_TRAJ_SET_ACCELERATION:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_ACCELERATION():EMC_TRAJ_CMD_MSG
	(EMC_TRAJ_SET_ACCELERATION_TYPE,
	 sizeof(EMC_TRAJ_SET_ACCELERATION)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double acceleration;
};

class EMC_TRAJ_SET_MAX_VELOCITY:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_MAX_VELOCITY():EMC_TRAJ_CMD_MSG
	(EMC_TRAJ_SET_MAX_VELOCITY_TYPE,
	 sizeof(EMC_TRAJ_SET_MAX_VELOCITY)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double velocity;
};

class EMC_TRAJ_SET_MAX_ACCELERATION:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_MAX_ACCELERATION():EMC_TRAJ_CMD_MSG
	(EMC_TRAJ_SET_MAX_ACCELERATION_TYPE,
	 sizeof(EMC_TRAJ_SET_MAX_ACCELERATION)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double acceleration;
};

class EMC_TRAJ_SET_SCALE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_SCALE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_SCALE_TYPE,
					  sizeof(EMC_TRAJ_SET_SCALE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double scale;
};

class EMC_TRAJ_SET_SPINDLE_SCALE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_SPINDLE_SCALE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_SPINDLE_SCALE_TYPE,
					  sizeof(EMC_TRAJ_SET_SPINDLE_SCALE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double scale;
};

class EMC_TRAJ_SET_FEED_OVERRIDE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_FEED_OVERRIDE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_FEED_OVERRIDE_TYPE,
					  sizeof(EMC_TRAJ_SET_FEED_OVERRIDE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned char mode; //mode=0, override off (will work with 100% FO), mode != 0, override on, user can change FO
};

class EMC_TRAJ_SET_SPINDLE_OVERRIDE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_SPINDLE_OVERRIDE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_SPINDLE_OVERRIDE_TYPE,
					  sizeof(EMC_TRAJ_SET_SPINDLE_OVERRIDE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned char mode; //mode=0, override off (will work with 100% SO), mode != 0, override on, user can change SO
};


class EMC_TRAJ_SET_MOTION_ID:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_MOTION_ID():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_MOTION_ID_TYPE,
					      sizeof
					      (EMC_TRAJ_SET_MOTION_ID)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int id;
};

class EMC_TRAJ_INIT:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_INIT():EMC_TRAJ_CMD_MSG(EMC_TRAJ_INIT_TYPE,
				     sizeof(EMC_TRAJ_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_HALT:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_HALT():EMC_TRAJ_CMD_MSG(EMC_TRAJ_HALT_TYPE,
				     sizeof(EMC_TRAJ_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_ENABLE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_ENABLE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_ENABLE_TYPE,
				       sizeof(EMC_TRAJ_ENABLE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_DISABLE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_DISABLE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_DISABLE_TYPE,
					sizeof(EMC_TRAJ_DISABLE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_ABORT:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_ABORT():EMC_TRAJ_CMD_MSG(EMC_TRAJ_ABORT_TYPE,
				      sizeof(EMC_TRAJ_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_PAUSE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_PAUSE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_PAUSE_TYPE,
				      sizeof(EMC_TRAJ_PAUSE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_STEP:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_STEP():EMC_TRAJ_CMD_MSG(EMC_TRAJ_STEP_TYPE,
				     sizeof(EMC_TRAJ_STEP)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_RESUME:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_RESUME():EMC_TRAJ_CMD_MSG(EMC_TRAJ_RESUME_TYPE,
				       sizeof(EMC_TRAJ_RESUME)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_DELAY:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_DELAY():EMC_TRAJ_CMD_MSG(EMC_TRAJ_DELAY_TYPE,
				      sizeof(EMC_TRAJ_DELAY)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double delay;		// delay in seconds
};

class EMC_TRAJ_LINEAR_MOVE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_LINEAR_MOVE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_LINEAR_MOVE_TYPE,
					    sizeof(EMC_TRAJ_LINEAR_MOVE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int type;
    EmcPose end;		// end point
    double vel, ini_maxvel, acc;
};

class EMC_TRAJ_CIRCULAR_MOVE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_CIRCULAR_MOVE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_CIRCULAR_MOVE_TYPE,
					      sizeof
					      (EMC_TRAJ_CIRCULAR_MOVE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    EmcPose end;
    PM_CARTESIAN center;
    PM_CARTESIAN normal;
    int turn;
    int type;
    double vel, ini_maxvel, acc;
};

class EMC_TRAJ_SET_TERM_COND:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_TERM_COND():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_TERM_COND_TYPE,
					      sizeof
					      (EMC_TRAJ_SET_TERM_COND)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int cond;
    double tolerance; // used to set the precision/tolerance of path deviation 
		      // during CONTINUOUS motion mode. 
};

class EMC_TRAJ_SET_SPINDLESYNC:public EMC_TRAJ_CMD_MSG {
    public:
        EMC_TRAJ_SET_SPINDLESYNC():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_SPINDLESYNC_TYPE,
                sizeof(EMC_TRAJ_SET_SPINDLESYNC)) {
        };

        void update(CMS * cms);
        double spindlesync;
};

class EMC_TRAJ_SET_OFFSET:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_OFFSET():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_OFFSET_TYPE,
					   sizeof(EMC_TRAJ_SET_OFFSET)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    EmcPose offset;
};

class EMC_TRAJ_SET_ORIGIN:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_ORIGIN():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_ORIGIN_TYPE,
					   sizeof(EMC_TRAJ_SET_ORIGIN)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    EmcPose origin;
};

class EMC_TRAJ_SET_HOME:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_HOME():EMC_TRAJ_CMD_MSG(EMC_TRAJ_SET_HOME_TYPE,
					 sizeof(EMC_TRAJ_SET_HOME)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    EmcPose home;
};

class EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG():EMC_TRAJ_CMD_MSG
	(EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG_TYPE,
	 sizeof(EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_SET_TELEOP_ENABLE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_TELEOP_ENABLE():EMC_TRAJ_CMD_MSG
	(EMC_TRAJ_SET_TELEOP_ENABLE_TYPE,
	 sizeof(EMC_TRAJ_SET_TELEOP_ENABLE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int enable;
};

class EMC_TRAJ_SET_TELEOP_VECTOR:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_SET_TELEOP_VECTOR():EMC_TRAJ_CMD_MSG
	(EMC_TRAJ_SET_TELEOP_VECTOR_TYPE,
	 sizeof(EMC_TRAJ_SET_TELEOP_VECTOR)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    EmcPose vector;
};

class EMC_TRAJ_PROBE:public EMC_TRAJ_CMD_MSG {
  public:
    EMC_TRAJ_PROBE():EMC_TRAJ_CMD_MSG(EMC_TRAJ_PROBE_TYPE,
				      sizeof(EMC_TRAJ_PROBE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    EmcPose pos;
    int type;
    double vel, ini_maxvel, acc;
};

// EMC_TRAJ status base class
class EMC_TRAJ_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_TRAJ_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TRAJ_STAT:public EMC_TRAJ_STAT_MSG {
  public:
    EMC_TRAJ_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double linearUnits;		// units per mm
    double angularUnits;	// units per degree
    double cycleTime;		// cycle time, in seconds
    int axes;			// number of axes in group
    enum EMC_TRAJ_MODE_ENUM mode;	// EMC_TRAJ_MODE_FREE,
    // EMC_TRAJ_MODE_COORD
    int enabled;		// non-zero means enabled

    int inpos;			// non-zero means in position
    int queue;			// number of pending motions, counting
    // current
    int activeQueue;		// number of motions blending
    int queueFull;		// non-zero means can't accept another motion
    int id;			// id of the currently executing motion
    int paused;			// non-zero means motion paused
    double scale;		// velocity scale factor
    double spindle_scale;	// spindle velocity scale factor

    EmcPose position;		// current commanded position
    EmcPose actualPosition;	// current actual position, from forward kins
    double velocity;		// system velocity, for subsequent motions
    double acceleration;	// system acceleration, for subsequent
    // motions
    double maxVelocity;		// max system velocity
    double maxAcceleration;	// system acceleration

    EmcPose probedPosition;	// last position where probe was tripped.
    int probe_index;		// which wire or digital input is the probe
    // on.
    int probe_polarity;		// which value should the probe look for to
    // trip.
    int probe_tripped;		// Has the probe been tripped since the last
    // clear.
    int probing;		// Are we currently looking for a probe
    // signal.
    int probeval;		// Current value of probe input.
    int kinematics_type;	// identity=1,serial=2,parallel=3,custom=4
    int motion_type;
    double distance_to_go;         // in current move
};

// emc_MOTION is aggregate of all EMC motion-related status classes

// EMC_MOTION command base class
class EMC_MOTION_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_MOTION_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_MOTION_INIT:public EMC_MOTION_CMD_MSG {
  public:
    EMC_MOTION_INIT():EMC_MOTION_CMD_MSG(EMC_MOTION_INIT_TYPE,
					 sizeof(EMC_MOTION_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_MOTION_HALT:public EMC_MOTION_CMD_MSG {
  public:
    EMC_MOTION_HALT():EMC_MOTION_CMD_MSG(EMC_MOTION_HALT_TYPE,
					 sizeof(EMC_MOTION_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_MOTION_ABORT:public EMC_MOTION_CMD_MSG {
  public:
    EMC_MOTION_ABORT():EMC_MOTION_CMD_MSG(EMC_MOTION_ABORT_TYPE,
					  sizeof(EMC_MOTION_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_MOTION_SET_AOUT:public EMC_MOTION_CMD_MSG {
  public:
    EMC_MOTION_SET_AOUT():EMC_MOTION_CMD_MSG(EMC_MOTION_SET_AOUT_TYPE,
					     sizeof(EMC_MOTION_SET_AOUT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned char index;	// which to set
    double start;		// value at start
    double end;			// value at end
    unsigned char now;		// wether command is imediate or synched with motion
};

class EMC_MOTION_SET_DOUT:public EMC_MOTION_CMD_MSG {
  public:
    EMC_MOTION_SET_DOUT():EMC_MOTION_CMD_MSG(EMC_MOTION_SET_DOUT_TYPE,
					     sizeof(EMC_MOTION_SET_DOUT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned char index;	// which to set
    unsigned char start;	// binary value at start
    unsigned char end;		// binary value at end
    unsigned char now;		// wether command is imediate or synched with motion
};

class EMC_MOTION_ADAPTIVE:public EMC_MOTION_CMD_MSG {
  public:
    EMC_MOTION_ADAPTIVE():EMC_MOTION_CMD_MSG(EMC_MOTION_ADAPTIVE_TYPE,
					     sizeof(EMC_MOTION_ADAPTIVE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned char status;		// status=0 stop; status=1 start.
};

// EMC_MOTION status base class
class EMC_MOTION_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_MOTION_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
	heartbeat = 0;
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned long int heartbeat;
};


// EMC_SPINDLE status base class
class EMC_SPINDLE_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_SPINDLE_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_STAT:public EMC_SPINDLE_STAT_MSG {
  public:
    EMC_SPINDLE_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double speed;		// spindle speed in RPMs
    int direction;		// 0 stopped, 1 forward, -1 reverse
    int brake;			// 0 released, 1 engaged
    int increasing;		// 1 increasing, -1 decreasing, 0 neither
    int enabled;		// non-zero means enabled
};


// number of axes in EMC_MOTION_STAT class
#define EMC_AXIS_MAX 8

class EMC_MOTION_STAT:public EMC_MOTION_STAT_MSG {
  public:
    EMC_MOTION_STAT():EMC_MOTION_STAT_MSG(EMC_MOTION_STAT_TYPE,
					  sizeof(EMC_MOTION_STAT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    // aggregate of motion-related status classes
    EMC_TRAJ_STAT traj;
    EMC_AXIS_STAT axis[EMC_AXIS_MAX];
    EMC_SPINDLE_STAT spindle;

    int debug;			// copy of EMC_DEBUG global
    
};

// declarations for EMC_TASK classes

// EMC_TASK command base class
class EMC_TASK_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_TASK_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_INIT:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_INIT():EMC_TASK_CMD_MSG(EMC_TASK_INIT_TYPE,
				     sizeof(EMC_TASK_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_HALT:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_HALT():EMC_TASK_CMD_MSG(EMC_TASK_HALT_TYPE,
				     sizeof(EMC_TASK_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_ABORT:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_ABORT():EMC_TASK_CMD_MSG(EMC_TASK_ABORT_TYPE,
				      sizeof(EMC_TASK_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_SET_MODE:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_SET_MODE():EMC_TASK_CMD_MSG(EMC_TASK_SET_MODE_TYPE,
					 sizeof(EMC_TASK_SET_MODE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    enum EMC_TASK_MODE_ENUM mode;
};

class EMC_TASK_SET_STATE:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_SET_STATE():EMC_TASK_CMD_MSG(EMC_TASK_SET_STATE_TYPE,
					  sizeof(EMC_TASK_SET_STATE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    enum EMC_TASK_STATE_ENUM state;
};

class EMC_TASK_PLAN_OPEN:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_OPEN():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_OPEN_TYPE,
					  sizeof(EMC_TASK_PLAN_OPEN)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    char file[LINELEN];
};

class EMC_TASK_PLAN_RUN:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_RUN():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_RUN_TYPE,
					 sizeof(EMC_TASK_PLAN_RUN)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int line;			// line to run from; 0 or 1 means from start,
    // negative means run through to verify
};

class EMC_TASK_PLAN_READ:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_READ():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_READ_TYPE,
					  sizeof(EMC_TASK_PLAN_READ)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_EXECUTE:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_EXECUTE():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_EXECUTE_TYPE,
					     sizeof(EMC_TASK_PLAN_EXECUTE))
    {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    char command[LINELEN];
};

class EMC_TASK_PLAN_PAUSE:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_PAUSE():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_PAUSE_TYPE,
					   sizeof(EMC_TASK_PLAN_PAUSE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_STEP:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_STEP():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_STEP_TYPE,
					  sizeof(EMC_TASK_PLAN_STEP)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_RESUME:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_RESUME():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_RESUME_TYPE,
					    sizeof(EMC_TASK_PLAN_RESUME)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_END:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_END():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_END_TYPE,
					 sizeof(EMC_TASK_PLAN_END)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_CLOSE:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_CLOSE():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_CLOSE_TYPE,
					   sizeof(EMC_TASK_PLAN_CLOSE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_INIT:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_INIT():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_INIT_TYPE,
					  sizeof(EMC_TASK_PLAN_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_SYNCH:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_SYNCH():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_SYNCH_TYPE,
					   sizeof(EMC_TASK_PLAN_SYNCH)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TASK_PLAN_SET_OPTIONAL_STOP:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_SET_OPTIONAL_STOP():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_SET_OPTIONAL_STOP_TYPE,
					   sizeof(EMC_TASK_PLAN_SET_OPTIONAL_STOP)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
    
    bool state; //state == ON, optional stop is on (e.g. we stop on any stops)
};

class EMC_TASK_PLAN_SET_BLOCK_DELETE:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_SET_BLOCK_DELETE():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_SET_BLOCK_DELETE_TYPE,
					   sizeof(EMC_TASK_PLAN_SET_BLOCK_DELETE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
    
    bool state; //state == ON, block delete is on, we ignore lines starting with "/"
};

class EMC_TASK_PLAN_OPTIONAL_STOP:public EMC_TASK_CMD_MSG {
  public:
    EMC_TASK_PLAN_OPTIONAL_STOP():EMC_TASK_CMD_MSG(EMC_TASK_PLAN_OPTIONAL_STOP_TYPE,
					   sizeof(EMC_TASK_PLAN_OPTIONAL_STOP)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
    
};


// EMC_TASK status base class
class EMC_TASK_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_TASK_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
	heartbeat = 0;
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned long int heartbeat;
};

class EMC_TASK_STAT:public EMC_TASK_STAT_MSG {
  public:
    EMC_TASK_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);

    enum EMC_TASK_MODE_ENUM mode;	// EMC_TASK_MODE_MANUAL, etc.
    enum EMC_TASK_STATE_ENUM state;	// EMC_TASK_STATE_ESTOP, etc.

    enum EMC_TASK_EXEC_ENUM execState;	// EMC_DONE,WAITING_FOR_MOTION, etc.
    enum EMC_TASK_INTERP_ENUM interpState;	// EMC_IDLE,READING,PAUSED,WAITING
    int motionLine;		// line motion is executing-- may lag
    int currentLine;		// line currently executing
    int readLine;		// line interpreter has read to
    bool optional_stop_state;	// state of optional stop (== ON means we stop on M1)
    bool block_delete_state;	// state of block delete (== ON means we ignore lines starting with "/")
    char file[LINELEN];
    char command[LINELEN];
    EmcPose origin;		// origin, in user units, currently active
    EmcPose toolOffset;		// tool offset, in general pose form
    int activeGCodes[ACTIVE_G_CODES];
    int activeMCodes[ACTIVE_M_CODES];
    double activeSettings[ACTIVE_SETTINGS];
    CANON_UNITS programUnits;	// CANON_UNITS_INCHES,MM,CM

    int interpreter_errcode;	// return value from rs274ngc function 
    // (only useful for new interpreter.)
};

// declarations for EMC_TOOL classes

// EMC_TOOL command base class
class EMC_TOOL_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_TOOL_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TOOL_INIT:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_INIT():EMC_TOOL_CMD_MSG(EMC_TOOL_INIT_TYPE,
				     sizeof(EMC_TOOL_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TOOL_HALT:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_HALT():EMC_TOOL_CMD_MSG(EMC_TOOL_HALT_TYPE,
				     sizeof(EMC_TOOL_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TOOL_ABORT:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_ABORT():EMC_TOOL_CMD_MSG(EMC_TOOL_ABORT_TYPE,
				      sizeof(EMC_TOOL_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TOOL_PREPARE:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_PREPARE():EMC_TOOL_CMD_MSG(EMC_TOOL_PREPARE_TYPE,
					sizeof(EMC_TOOL_PREPARE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int tool;
};

class EMC_TOOL_LOAD:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_LOAD():EMC_TOOL_CMD_MSG(EMC_TOOL_LOAD_TYPE,
				     sizeof(EMC_TOOL_LOAD)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TOOL_UNLOAD:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_UNLOAD():EMC_TOOL_CMD_MSG(EMC_TOOL_UNLOAD_TYPE,
				       sizeof(EMC_TOOL_UNLOAD)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TOOL_LOAD_TOOL_TABLE:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_LOAD_TOOL_TABLE():EMC_TOOL_CMD_MSG
	(EMC_TOOL_LOAD_TOOL_TABLE_TYPE, sizeof(EMC_TOOL_LOAD_TOOL_TABLE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    char file[LINELEN];		// name of tool table, empty means default
};

class EMC_TOOL_SET_OFFSET:public EMC_TOOL_CMD_MSG {
  public:
    EMC_TOOL_SET_OFFSET():EMC_TOOL_CMD_MSG(EMC_TOOL_SET_OFFSET_TYPE,
					   sizeof(EMC_TOOL_SET_OFFSET)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int tool;
    double length;
    double diameter;
};

// EMC_TOOL status base class
class EMC_TOOL_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_TOOL_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_TOOL_STAT:public EMC_TOOL_STAT_MSG {
  public:
    EMC_TOOL_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);
    EMC_TOOL_STAT operator =(EMC_TOOL_STAT s);	// need this for [] members

    int toolPrepped;		// tool ready for loading, 0 is no tool
    int toolInSpindle;		// tool loaded, 0 is no tool
    CANON_TOOL_TABLE toolTable[CANON_TOOL_MAX + 1];
};

// EMC_AUX type declarations

// EMC_AUX command base class
class EMC_AUX_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_AUX_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AUX_INIT:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_INIT():EMC_AUX_CMD_MSG(EMC_AUX_INIT_TYPE, sizeof(EMC_AUX_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AUX_HALT:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_HALT():EMC_AUX_CMD_MSG(EMC_AUX_HALT_TYPE, sizeof(EMC_AUX_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AUX_ABORT:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_ABORT():EMC_AUX_CMD_MSG(EMC_AUX_ABORT_TYPE,
				    sizeof(EMC_AUX_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AUX_DIO_WRITE:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_DIO_WRITE():EMC_AUX_CMD_MSG(EMC_AUX_DIO_WRITE_TYPE,
					sizeof(EMC_AUX_DIO_WRITE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int index;
    int value;
};

class EMC_AUX_AIO_WRITE:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_AIO_WRITE():EMC_AUX_CMD_MSG(EMC_AUX_AIO_WRITE_TYPE,
					sizeof(EMC_AUX_AIO_WRITE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int index;
    double value;
};

class EMC_AUX_ESTOP_ON:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_ESTOP_ON():EMC_AUX_CMD_MSG(EMC_AUX_ESTOP_ON_TYPE,
				       sizeof(EMC_AUX_ESTOP_ON)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AUX_ESTOP_OFF:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_ESTOP_OFF():EMC_AUX_CMD_MSG(EMC_AUX_ESTOP_OFF_TYPE,
					sizeof(EMC_AUX_ESTOP_OFF)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_AUX_ESTOP_RESET:public EMC_AUX_CMD_MSG {
  public:
    EMC_AUX_ESTOP_RESET():EMC_AUX_CMD_MSG(EMC_AUX_ESTOP_RESET_TYPE,
					sizeof(EMC_AUX_ESTOP_RESET)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

// EMC_AUX status base class
class EMC_AUX_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_AUX_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

#define EMC_AUX_MAX_DOUT 4	// digital out bytes
#define EMC_AUX_MAX_DIN  4	// digital in bytes
#define EMC_AUX_MAX_AOUT 32	// analog out points
#define EMC_AUX_MAX_AIN  32	// analog in points

class EMC_AUX_STAT:public EMC_AUX_STAT_MSG {
  public:
    EMC_AUX_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);
    EMC_AUX_STAT operator =(EMC_AUX_STAT s);	// need this for [] members

    int estop;			// non-zero means estopped

    unsigned char dout[EMC_AUX_MAX_DOUT];	// digital output readings
    unsigned char din[EMC_AUX_MAX_DIN];	// digital input readings

    double aout[EMC_AUX_MAX_AOUT];	// digital output readings
    double ain[EMC_AUX_MAX_AIN];	// digital input readings
};

// EMC_SPINDLE type declarations

// EMC_SPINDLE command base class
class EMC_SPINDLE_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_SPINDLE_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_INIT:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_INIT():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_INIT_TYPE,
					   sizeof(EMC_SPINDLE_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_HALT:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_HALT():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_HALT_TYPE,
					   sizeof(EMC_SPINDLE_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_ABORT:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_ABORT():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_ABORT_TYPE,
					    sizeof(EMC_SPINDLE_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_ON:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_ON():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_ON_TYPE,
					 sizeof(EMC_SPINDLE_ON)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double speed;		// commanded speed in RPMs
};

class EMC_SPINDLE_OFF:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_OFF():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_OFF_TYPE,
					  sizeof(EMC_SPINDLE_OFF)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_FORWARD:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_FORWARD():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_FORWARD_TYPE,
					      sizeof(EMC_SPINDLE_FORWARD))
    {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double speed;		// commanded speed in RPMs
};

class EMC_SPINDLE_REVERSE:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_REVERSE():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_REVERSE_TYPE,
					      sizeof(EMC_SPINDLE_REVERSE))
    {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double speed;		// commanded speed in RPMs
};

class EMC_SPINDLE_STOP:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_STOP():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_STOP_TYPE,
					   sizeof(EMC_SPINDLE_STOP)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_INCREASE:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_INCREASE():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_INCREASE_TYPE,
					       sizeof
					       (EMC_SPINDLE_INCREASE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double speed;		// commanded speed in RPMs
};

class EMC_SPINDLE_DECREASE:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_DECREASE():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_DECREASE_TYPE,
					       sizeof
					       (EMC_SPINDLE_DECREASE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double speed;		// commanded speed in RPMs
};

class EMC_SPINDLE_CONSTANT:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_CONSTANT():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_CONSTANT_TYPE,
					       sizeof
					       (EMC_SPINDLE_CONSTANT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double speed;		// commanded speed in RPMs
};

class EMC_SPINDLE_BRAKE_RELEASE:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_BRAKE_RELEASE():EMC_SPINDLE_CMD_MSG
	(EMC_SPINDLE_BRAKE_RELEASE_TYPE,
	 sizeof(EMC_SPINDLE_BRAKE_RELEASE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_BRAKE_ENGAGE:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_BRAKE_ENGAGE():EMC_SPINDLE_CMD_MSG
	(EMC_SPINDLE_BRAKE_ENGAGE_TYPE, sizeof(EMC_SPINDLE_BRAKE_ENGAGE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_ENABLE:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_ENABLE():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_ENABLE_TYPE,
					     sizeof(EMC_SPINDLE_ENABLE)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_SPINDLE_DISABLE:public EMC_SPINDLE_CMD_MSG {
  public:
    EMC_SPINDLE_DISABLE():EMC_SPINDLE_CMD_MSG(EMC_SPINDLE_DISABLE_TYPE,
					      sizeof(EMC_SPINDLE_DISABLE))
    {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};


// EMC_COOLANT type declarations

// EMC_COOLANT command base class
class EMC_COOLANT_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_COOLANT_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_INIT:public EMC_COOLANT_CMD_MSG {
  public:
    EMC_COOLANT_INIT():EMC_COOLANT_CMD_MSG(EMC_COOLANT_INIT_TYPE,
					   sizeof(EMC_COOLANT_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_HALT:public EMC_COOLANT_CMD_MSG {
  public:
    EMC_COOLANT_HALT():EMC_COOLANT_CMD_MSG(EMC_COOLANT_HALT_TYPE,
					   sizeof(EMC_COOLANT_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_ABORT:public EMC_COOLANT_CMD_MSG {
  public:
    EMC_COOLANT_ABORT():EMC_COOLANT_CMD_MSG(EMC_COOLANT_ABORT_TYPE,
					    sizeof(EMC_COOLANT_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_MIST_ON:public EMC_COOLANT_CMD_MSG {
  public:
    EMC_COOLANT_MIST_ON():EMC_COOLANT_CMD_MSG(EMC_COOLANT_MIST_ON_TYPE,
					      sizeof(EMC_COOLANT_MIST_ON))
    {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_MIST_OFF:public EMC_COOLANT_CMD_MSG {
  public:
    EMC_COOLANT_MIST_OFF():EMC_COOLANT_CMD_MSG(EMC_COOLANT_MIST_OFF_TYPE,
					       sizeof
					       (EMC_COOLANT_MIST_OFF)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_FLOOD_ON:public EMC_COOLANT_CMD_MSG {
  public:
    EMC_COOLANT_FLOOD_ON():EMC_COOLANT_CMD_MSG(EMC_COOLANT_FLOOD_ON_TYPE,
					       sizeof
					       (EMC_COOLANT_FLOOD_ON)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_FLOOD_OFF:public EMC_COOLANT_CMD_MSG {
  public:
    EMC_COOLANT_FLOOD_OFF():EMC_COOLANT_CMD_MSG(EMC_COOLANT_FLOOD_OFF_TYPE,
						sizeof
						(EMC_COOLANT_FLOOD_OFF)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

// EMC_COOLANT status base class
class EMC_COOLANT_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_COOLANT_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_COOLANT_STAT:public EMC_COOLANT_STAT_MSG {
  public:
    EMC_COOLANT_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int mist;			// 0 off, 1 on
    int flood;			// 0 off, 1 on
};

// EMC_LUBE type declarations

// EMC_LUBE command base class
class EMC_LUBE_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_LUBE_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_LUBE_INIT:public EMC_LUBE_CMD_MSG {
  public:
    EMC_LUBE_INIT():EMC_LUBE_CMD_MSG(EMC_LUBE_INIT_TYPE,
				     sizeof(EMC_LUBE_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_LUBE_HALT:public EMC_LUBE_CMD_MSG {
  public:
    EMC_LUBE_HALT():EMC_LUBE_CMD_MSG(EMC_LUBE_HALT_TYPE,
				     sizeof(EMC_LUBE_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_LUBE_ABORT:public EMC_LUBE_CMD_MSG {
  public:
    EMC_LUBE_ABORT():EMC_LUBE_CMD_MSG(EMC_LUBE_ABORT_TYPE,
				      sizeof(EMC_LUBE_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_LUBE_ON:public EMC_LUBE_CMD_MSG {
  public:
    EMC_LUBE_ON():EMC_LUBE_CMD_MSG(EMC_LUBE_ON_TYPE, sizeof(EMC_LUBE_ON)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_LUBE_OFF:public EMC_LUBE_CMD_MSG {
  public:
    EMC_LUBE_OFF():EMC_LUBE_CMD_MSG(EMC_LUBE_OFF_TYPE,
				    sizeof(EMC_LUBE_OFF)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

// EMC_LUBE status base class
class EMC_LUBE_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_LUBE_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_LUBE_STAT:public EMC_LUBE_STAT_MSG {
  public:
    EMC_LUBE_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int on;			// 0 off, 1 on
    int level;			// 0 low, 1 okay
};

// EMC IO configuration classes

class EMC_SET_DIO_INDEX:public RCS_CMD_MSG {
  public:
    EMC_SET_DIO_INDEX():RCS_CMD_MSG(EMC_SET_DIO_INDEX_TYPE,
				    sizeof(EMC_SET_DIO_INDEX)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int value;			// one of enum EMC_SET_DIO_INDEX_XXX
    int index;			// index, 0..max
};

class EMC_SET_AIO_INDEX:public RCS_CMD_MSG {
  public:
    EMC_SET_AIO_INDEX():RCS_CMD_MSG(EMC_SET_AIO_INDEX_TYPE,
				    sizeof(EMC_SET_AIO_INDEX)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    int value;			// one of enum EMC_SET_AIO_INDEX_XXX
    int index;			// index, 0..max
};


// EMC_IO is aggregate of all EMC IO-related status classes

// EMC_IO command base class
class EMC_IO_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_IO_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_IO_INIT:public EMC_IO_CMD_MSG {
  public:
    EMC_IO_INIT():EMC_IO_CMD_MSG(EMC_IO_INIT_TYPE, sizeof(EMC_IO_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_IO_HALT:public EMC_IO_CMD_MSG {
  public:
    EMC_IO_HALT():EMC_IO_CMD_MSG(EMC_IO_HALT_TYPE, sizeof(EMC_IO_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_IO_ABORT:public EMC_IO_CMD_MSG {
  public:
    EMC_IO_ABORT():EMC_IO_CMD_MSG(EMC_IO_ABORT_TYPE, sizeof(EMC_IO_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_IO_SET_CYCLE_TIME:public EMC_IO_CMD_MSG {
  public:
    EMC_IO_SET_CYCLE_TIME():EMC_IO_CMD_MSG(EMC_IO_SET_CYCLE_TIME_TYPE,
					   sizeof(EMC_IO_SET_CYCLE_TIME)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    double cycleTime;
};

// EMC_IO status base class
class EMC_IO_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_IO_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
	heartbeat = 0;
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    unsigned long int heartbeat;
};

class EMC_IO_STAT:public EMC_IO_STAT_MSG {
  public:
    EMC_IO_STAT():EMC_IO_STAT_MSG(EMC_IO_STAT_TYPE, sizeof(EMC_IO_STAT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);

    // top-level stuff
    double cycleTime;
    int debug;			// copy of EMC_DEBUG global

    // aggregate of IO-related status classes
    EMC_TOOL_STAT tool;
    EMC_COOLANT_STAT coolant;
    EMC_AUX_STAT aux;
    EMC_LUBE_STAT lube;
};

// EMC is aggregate of EMC_TASK, EMC_TRAJ, EMC_IO, etc.

// EMC command base class
class EMC_CMD_MSG:public RCS_CMD_MSG {
  public:
    EMC_CMD_MSG(NMLTYPE t, size_t s):RCS_CMD_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_INIT:public EMC_CMD_MSG {
  public:
    EMC_INIT():EMC_CMD_MSG(EMC_INIT_TYPE, sizeof(EMC_INIT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_HALT:public EMC_CMD_MSG {
  public:
    EMC_HALT():EMC_CMD_MSG(EMC_HALT_TYPE, sizeof(EMC_HALT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_ABORT:public EMC_CMD_MSG {
  public:
    EMC_ABORT():EMC_CMD_MSG(EMC_ABORT_TYPE, sizeof(EMC_ABORT)) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

// EMC status base class

class EMC_STAT_MSG:public RCS_STAT_MSG {
  public:
    EMC_STAT_MSG(NMLTYPE t, size_t s):RCS_STAT_MSG(t, s) {
    };

    // For internal NML/CMS use only.
    void update(CMS * cms);
};

class EMC_STAT:public EMC_STAT_MSG {
  public:
    EMC_STAT();

    // For internal NML/CMS use only.
    void update(CMS * cms);

    // the top-level EMC_TASK status class
    EMC_TASK_STAT task;

    // subordinate status classes
    EMC_MOTION_STAT motion;
    EMC_IO_STAT io;

    int debug;			// copy of EMC_DEBUG global
};

/*
   Declarations of EMC status class implementations, for major subsystems.
   These are defined in the appropriate main() files, and referenced
   by code in other files to get EMC status.
   */

// full EMC status
extern EMC_STAT *emcStatus;

// EMC IO status
extern EMC_IO_STAT *emcIoStatus;

// EMC MOTION status
extern EMC_MOTION_STAT *emcMotionStatus;

#define EMC_VERSION_MAJOR 2
#define EMC_VERSION_MINOR 1
#define EMC_VERSION_MICRO 0

#define EMC_VERSION_CHECK(major,minor,micro) \
    (EMC_VERSION_MAJOR > (major) || \
     (EMC_VERSION_MAJOR == (major) && EMC_VERSION_MINOR > (minor)) || \
     (EMC_VERSION_MAJOR == (major) && EMC_VERSION_MINOR == (minor) && \
      EMC_VERSION_MICRO >= (micro)))

#endif				// #ifndef EMC_HH
