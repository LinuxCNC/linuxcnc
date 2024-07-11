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
********************************************************************/
#ifndef EMC_HH
#define EMC_HH

#include "emcmotcfg.h"		// EMC_JOINT_MAX, EMC_AXIS_MAX
#include "nml_type.hh"
#include "motion_types.h"
#include <stdint.h>
#include "modal_state.hh"

// Forward class declarations
class EMC_JOINT_STAT;
class EMC_AXIS_STAT;
class EMC_TRAJ_STAT;
class EMC_MOTION_STAT;
class EMC_TASK_STAT;
class EMC_TOOL_STAT;
class EMC_AUX_STAT;
class EMC_SPINDLE_STAT;
class EMC_COOLANT_STAT;
class EMC_IO_STAT;
class EMC_STAT;
class CMS;
class RCS_CMD_CHANNEL;
class RCS_STAT_CHANNEL;
class NML;
struct EmcPose;
struct PM_CARTESIAN;

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

// NML for EMC_JOINT

#define EMC_JOINT_SET_MIN_POSITION_LIMIT_TYPE         ((NMLTYPE) 107)
#define EMC_JOINT_SET_MAX_POSITION_LIMIT_TYPE         ((NMLTYPE) 108)
#define EMC_JOINT_SET_FERROR_TYPE                     ((NMLTYPE) 111)
#define EMC_JOINT_SET_HOMING_PARAMS_TYPE              ((NMLTYPE) 112)
#define EMC_JOINT_SET_MIN_FERROR_TYPE                 ((NMLTYPE) 115)
#define EMC_JOINT_HALT_TYPE                           ((NMLTYPE) 119)
#define EMC_JOINT_HOME_TYPE                           ((NMLTYPE) 123)
#define EMC_JOG_CONT_TYPE                             ((NMLTYPE) 124)
#define EMC_JOG_INCR_TYPE                             ((NMLTYPE) 125)
#define EMC_JOG_ABS_TYPE                              ((NMLTYPE) 126)
#define EMC_JOINT_OVERRIDE_LIMITS_TYPE                ((NMLTYPE) 129)
#define EMC_JOINT_LOAD_COMP_TYPE                      ((NMLTYPE) 131)
#define EMC_JOINT_SET_BACKLASH_TYPE                   ((NMLTYPE) 134)
#define EMC_JOINT_UNHOME_TYPE                         ((NMLTYPE) 135)
#define EMC_JOG_STOP_TYPE                             ((NMLTYPE) 136)

#define EMC_JOINT_STAT_TYPE                          ((NMLTYPE) 198)
#define EMC_AXIS_STAT_TYPE                           ((NMLTYPE) 199)

// NML for EMC_TRAJ

// defs for termination conditions
#define EMC_TRAJ_TERM_COND_STOP  0
#define EMC_TRAJ_TERM_COND_EXACT 1
#define EMC_TRAJ_TERM_COND_BLEND 2

#define EMC_TRAJ_SET_MODE_TYPE                       ((NMLTYPE) 204)
#define EMC_TRAJ_SET_VELOCITY_TYPE                   ((NMLTYPE) 205)
#define EMC_TRAJ_SET_ACCELERATION_TYPE               ((NMLTYPE) 206)
#define EMC_TRAJ_SET_MAX_VELOCITY_TYPE               ((NMLTYPE) 207)
#define EMC_TRAJ_SET_SCALE_TYPE                      ((NMLTYPE) 209)
#define EMC_TRAJ_SET_RAPID_SCALE_TYPE                ((NMLTYPE) 238)

#define EMC_TRAJ_ABORT_TYPE                          ((NMLTYPE) 215)
#define EMC_TRAJ_PAUSE_TYPE                          ((NMLTYPE) 216)
#define EMC_TRAJ_RESUME_TYPE                         ((NMLTYPE) 218)
#define EMC_TRAJ_DELAY_TYPE                          ((NMLTYPE) 219)
#define EMC_TRAJ_LINEAR_MOVE_TYPE                    ((NMLTYPE) 220)
#define EMC_TRAJ_CIRCULAR_MOVE_TYPE                  ((NMLTYPE) 221)
#define EMC_TRAJ_SET_TERM_COND_TYPE                  ((NMLTYPE) 222)
#define EMC_TRAJ_SET_OFFSET_TYPE                     ((NMLTYPE) 223)
#define EMC_TRAJ_SET_G5X_TYPE                        ((NMLTYPE) 224)
#define EMC_TRAJ_SET_ROTATION_TYPE                   ((NMLTYPE) 226)
#define EMC_TRAJ_SET_G92_TYPE                        ((NMLTYPE) 227)
#define EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG_TYPE       ((NMLTYPE) 228)
#define EMC_TRAJ_PROBE_TYPE                          ((NMLTYPE) 229)
#define EMC_TRAJ_SET_TELEOP_ENABLE_TYPE              ((NMLTYPE) 230)
#define EMC_TRAJ_SET_SPINDLESYNC_TYPE                ((NMLTYPE) 232)
#define EMC_TRAJ_SET_SPINDLE_SCALE_TYPE              ((NMLTYPE) 233)
#define EMC_TRAJ_SET_FO_ENABLE_TYPE                  ((NMLTYPE) 234)
#define EMC_TRAJ_SET_SO_ENABLE_TYPE                  ((NMLTYPE) 235)
#define EMC_TRAJ_SET_FH_ENABLE_TYPE                  ((NMLTYPE) 236)
#define EMC_TRAJ_RIGID_TAP_TYPE                      ((NMLTYPE) 237)

#define EMC_TRAJ_STAT_TYPE                           ((NMLTYPE) 299)

// EMC_MOTION aggregate class type declaration

#define EMC_MOTION_SET_AOUT_TYPE                     ((NMLTYPE) 304)
#define EMC_MOTION_SET_DOUT_TYPE                     ((NMLTYPE) 305)
#define EMC_MOTION_ADAPTIVE_TYPE                     ((NMLTYPE) 306)

#define EMC_MOTION_STAT_TYPE                         ((NMLTYPE) 399)

// NML for EMC_TASK

#define EMC_TASK_ABORT_TYPE                          ((NMLTYPE) 503)
#define EMC_TASK_SET_MODE_TYPE                       ((NMLTYPE) 504)
#define EMC_TASK_SET_STATE_TYPE                      ((NMLTYPE) 505)
#define EMC_TASK_PLAN_OPEN_TYPE                      ((NMLTYPE) 506)
#define EMC_TASK_PLAN_RUN_TYPE                       ((NMLTYPE) 507)
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
#define EMC_TASK_PLAN_REVERSE_TYPE                   ((NMLTYPE) 520)
#define EMC_TASK_PLAN_FORWARD_TYPE                   ((NMLTYPE) 521)

#define EMC_TASK_STAT_TYPE                           ((NMLTYPE) 599)

// EMC_TOOL type declarations

#define EMC_TOOL_HALT_TYPE                           ((NMLTYPE) 1102)
#define EMC_TOOL_ABORT_TYPE                          ((NMLTYPE) 1103)
#define EMC_TOOL_PREPARE_TYPE                        ((NMLTYPE) 1104)
#define EMC_TOOL_LOAD_TYPE                           ((NMLTYPE) 1105)
#define EMC_TOOL_UNLOAD_TYPE                         ((NMLTYPE) 1106)
#define EMC_TOOL_LOAD_TOOL_TABLE_TYPE                ((NMLTYPE) 1107)
#define EMC_TOOL_SET_OFFSET_TYPE                     ((NMLTYPE) 1108)
#define EMC_TOOL_SET_NUMBER_TYPE                     ((NMLTYPE) 1109)

#define EMC_TOOL_STAT_TYPE                           ((NMLTYPE) 1199)

// EMC_AUX type declarations
#define EMC_AUX_INPUT_WAIT_TYPE                       ((NMLTYPE) 1209)

#define EMC_AUX_STAT_TYPE                             ((NMLTYPE) 1299)

// EMC_SPINDLE type declarations
#define EMC_SPINDLE_ON_TYPE                          ((NMLTYPE) 1304)
#define EMC_SPINDLE_OFF_TYPE                         ((NMLTYPE) 1305)
#define EMC_SPINDLE_INCREASE_TYPE                    ((NMLTYPE) 1309)
#define EMC_SPINDLE_DECREASE_TYPE                    ((NMLTYPE) 1310)
#define EMC_SPINDLE_CONSTANT_TYPE                    ((NMLTYPE) 1311)
#define EMC_SPINDLE_BRAKE_RELEASE_TYPE               ((NMLTYPE) 1312)
#define EMC_SPINDLE_BRAKE_ENGAGE_TYPE                ((NMLTYPE) 1313)
#define EMC_SPINDLE_SPEED_TYPE                       ((NMLTYPE) 1316)
#define EMC_SPINDLE_ORIENT_TYPE                      ((NMLTYPE) 1317)
#define EMC_SPINDLE_WAIT_ORIENT_COMPLETE_TYPE        ((NMLTYPE) 1318)

#define EMC_SPINDLE_STAT_TYPE                        ((NMLTYPE) 1399)

// EMC_COOLANT type declarations
#define EMC_COOLANT_MIST_ON_TYPE                     ((NMLTYPE) 1404)
#define EMC_COOLANT_MIST_OFF_TYPE                    ((NMLTYPE) 1405)
#define EMC_COOLANT_FLOOD_ON_TYPE                    ((NMLTYPE) 1406)
#define EMC_COOLANT_FLOOD_OFF_TYPE                   ((NMLTYPE) 1407)

#define EMC_COOLANT_STAT_TYPE                        ((NMLTYPE) 1499)

#define EMC_IO_STAT_TYPE                             ((NMLTYPE) 1699)

#define EMC_STAT_TYPE                                ((NMLTYPE) 1999)

// types for EMC_TASK mode
enum class EMC_TASK_MODE {
    MANUAL = 1,
    AUTO = 2,
    MDI = 3
};

// types for EMC_TASK state
enum class EMC_TASK_STATE {
    ESTOP = 1,
    ESTOP_RESET = 2,
    OFF = 3,
    ON = 4
};

// types for EMC_TASK execState
enum class EMC_TASK_EXEC {
    ERROR = 1,
    DONE = 2,
    WAITING_FOR_MOTION = 3,
    WAITING_FOR_MOTION_QUEUE = 4,
    WAITING_FOR_IO = 5,
    WAITING_FOR_MOTION_AND_IO = 7,
    WAITING_FOR_DELAY = 8,
    WAITING_FOR_SYSTEM_CMD = 9,
    WAITING_FOR_SPINDLE_ORIENTED = 10
};

// types for EMC_TASK interpState
enum class EMC_TASK_INTERP {
    IDLE = 1,
    READING = 2,
    PAUSED = 3,
    WAITING = 4
};

// types for motion control
enum class EMC_TRAJ_MODE {
    FREE = 1,	// independent-axis motion,
    COORD = 2,	// coordinated-axis motion,
    TELEOP = 3	// velocity based world coordinates motion,
};

// types for emcIoAbort() reasons
enum class EMC_ABORT {
	TASK_EXEC_ERROR = 1,
	AUX_ESTOP = 2,
	MOTION_OR_IO_RCS_ERROR = 3,
	TASK_STATE_OFF = 4,
	TASK_STATE_ESTOP_RESET = 5,
	TASK_STATE_ESTOP = 6,
	TASK_STATE_NOT_ON = 7,
	TASK_ABORT = 8,
	INTERPRETER_ERROR = 9,	// interpreter failed during readahead
	INTERPRETER_ERROR_MDI = 10,	// interpreter failed during MDI execution
	USER = 100  // user-defined abort codes start here
};
// --------------
// EMC VOCABULARY
// --------------

// NML formatting function
extern int emcFormat(NMLTYPE type, void *buffer, CMS * cms);

// NML Symbol Lookup Function
extern const char *emc_symbol_lookup(uint32_t type);
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
extern int emcOperatorError(const char *fmt, ...) __attribute__((format(printf,1,2)));

// print general text
extern int emcOperatorText(const char *fmt, ...) __attribute__((format(printf,1,2)));

// print note to operator
extern int emcOperatorDisplay(const char *fmt, ...) __attribute__((format(printf,1,2)));

// implementation functions for EMC_AXIS types

extern int emcAxisSetMinPositionLimit(int axis, double limit);
extern int emcAxisSetMaxPositionLimit(int axis, double limit);
extern int emcAxisSetMaxVelocity(int axis, double vel, double ext_offset_vel);
extern int emcAxisSetMaxAcceleration(int axis, double acc, double ext_offset_acc);
extern double emcAxisGetMaxVelocity(int axis);
extern double emcAxisGetMaxAcceleration(int axis);
extern int emcAxisSetLockingJoint(int axis,int joint);

extern int emcAxisUpdate(EMC_AXIS_STAT stat[], int numAxes);

// implementation functions for EMC_JOINT types

extern int emcJointSetType(int joint, unsigned char jointType);
extern int emcJointSetUnits(int joint, double units);
extern int emcJointSetBacklash(int joint, double backlash);
extern int emcJointSetMinPositionLimit(int joint, double limit);
extern int emcJointSetMaxPositionLimit(int joint, double limit);
extern int emcJointSetMotorOffset(int joint, double offset);
extern int emcJointSetFerror(int joint, double ferror);
extern int emcJointSetMinFerror(int joint, double ferror);
extern int emcJointSetHomingParams(int joint, double home, double offset, double home_vel,
				  double search_vel, double latch_vel,
				  int use_index, int encoder_does_not_reset, int ignore_limits,
				  int is_shared, int home_sequence, int volatile_home, int locking_indexer,
                  int absolute_encoder);
extern int emcJointUpdateHomingParams(int joint, double home, double offset, int sequence);
extern int emcJointSetMaxVelocity(int joint, double vel);
extern int emcJointSetMaxAcceleration(int joint, double acc);

extern int emcJointInit(int joint);
extern int emcJointHalt(int joint);
extern int emcJointHome(int joint);
extern int emcJointUnhome(int joint);
extern int emcJointActivate(int joint);
extern int emcJointDeactivate(int joint);
extern int emcJointOverrideLimits(int joint);
extern int emcJointLoadComp(int joint, const char *file, int type);
extern int emcJogStop(int nr, int jjogmode);
extern int emcJogCont(int nr, double vel, int jjogmode);
extern int emcJogIncr(int nr, double incr, double vel, int jjogmode);
extern int emcJogAbs(int nr, double pos, double vel, int jjogmode);


extern int emcJointUpdate(EMC_JOINT_STAT stat[], int numJoints);


// implementation functions for EMC_SPINDLE types

extern int emcSpindleSetParams(int spindle, double max_pos, double min_pos, double max_neg,
            double min_neg, double search_vel, double home_angle, int sequence, double increment);

// implementation functions for EMC_TRAJ types

extern int emcTrajSetJoints(int joints);
extern int emcTrajUpdateTag(StateTag const &tag);
extern int emcTrajSetAxes(int axismask);
extern int emcTrajSetSpindles(int spindles);
extern int emcTrajSetUnits(double linearUnits, double angularUnits);
extern int emcTrajSetMode(EMC_TRAJ_MODE traj_mode);
extern int emcTrajSetVelocity(double vel, double ini_maxvel);
extern int emcTrajSetAcceleration(double acc);
extern int emcTrajSetMaxVelocity(double vel);
extern int emcTrajSetMaxAcceleration(double acc);
extern int emcTrajSetScale(double scale);
extern int emcTrajSetRapidScale(double scale);
extern int emcTrajSetFOEnable(unsigned char mode);   //feed override enable
extern int emcTrajSetFHEnable(unsigned char mode);   //feed hold enable
extern int emcTrajSetSpindleScale(int spindle, double scale);
extern int emcTrajSetSOEnable(unsigned char mode);   //spindle speed override enable
extern int emcTrajSetAFEnable(unsigned char enable); //adaptive feed enable
extern int emcTrajSetMotionId(int id);
extern double emcTrajGetLinearUnits();
extern double emcTrajGetAngularUnits();

extern int emcTrajInit();
extern int emcTrajHalt();
extern int emcTrajEnable();
extern int emcTrajDisable();
extern int emcTrajAbort();
extern int emcTrajPause();
extern int emcTrajReverse();
extern int emcTrajForward();
extern int emcTrajStep();
extern int emcTrajResume();
extern int emcTrajDelay(double delay);
extern int emcTrajLinearMove(EmcPose end, int type, double vel,
                             double ini_maxvel, double acc, int indexer_jnum);
extern int emcTrajCircularMove(EmcPose end, PM_CARTESIAN center, PM_CARTESIAN
        normal, int turn, int type, double vel, double ini_maxvel, double acc);
extern int emcTrajSetTermCond(int cond, double tolerance);
extern int emcTrajSetSpindleSync(int spindle, double feed_per_revolution, bool wait_for_index);
extern int emcTrajSetOffset(EmcPose tool_offset);
extern int emcTrajSetHome(EmcPose home);
extern int emcTrajClearProbeTrippedFlag();
extern int emcTrajProbe(EmcPose pos, int type, double vel, 
                        double ini_maxvel, double acc, unsigned char probe_type);
extern int emcTrajRigidTap(EmcPose pos, double vel, double ini_maxvel, double acc, double scale);

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

extern int emcMotionUpdate(EMC_MOTION_STAT * stat);

extern int emcAbortCleanup(EMC_ABORT reason,const char *message = "");

// implementation functions for EMC_TOOL types

extern int emcToolPrepare(int tool);
extern int emcToolLoad();
extern int emcToolUnload();
extern int emcToolLoadToolTable(const char *file);
extern int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
                            double frontangle, double backangle, int orientation);
extern int emcToolSetNumber(int number);

// implementation functions for EMC_AUX types

extern int emcAuxEstopOn();
extern int emcAuxEstopOff();

// implementation functions for EMC_SPINDLE types

extern int emcSpindleAbort(int spindle);
extern int emcSpindleSpeed(int spindle, double speed, double factor, double xoffset);
extern int emcSpindleOn(int spindle, double speed, double factor, double xoffset,int wait_for_atspeed = 1);
extern int emcSpindleOrient(int spindle, double orientation, int direction);
extern int emcSpindleOff(int spindle);
extern int emcSpindleIncrease(int spindle);
extern int emcSpindleDecrease(int spindle);
extern int emcSpindleConstant(int spindle);
extern int emcSpindleBrakeRelease(int spindle);
extern int emcSpindleBrakeEngage(int spindle);

extern int emcSpindleUpdate(EMC_SPINDLE_STAT stat[], int num_spindles);

// implementation functions for EMC_COOLANT types

extern int emcCoolantMistOn();
extern int emcCoolantMistOff();
extern int emcCoolantFloodOn();
extern int emcCoolantFloodOff();

// implementation functions for EMC_IO types

extern int emcIoInit();
extern int emcIoAbort(EMC_ABORT reason);

// implementation functions for EMC aggregate types

int emcSetMaxFeedOverride(double maxFeedScale);
int emcSetupArcBlends(int arcBlendEnable,
        int arcBlendFallbackEnable,
        int arcBlendOptDepth,
        int arcBlendGapCycles,
        double arcBlendRampFreq,
        double arcBlendTangentKinkRatio);
int emcSetProbeErrorInhibit(int j_inhibit, int h_inhibit);
int emcGetExternalOffsetApplied(void);
EmcPose emcGetExternalOffsets(void);

extern int emcUpdate(EMC_STAT * stat);
// full EMC status
extern EMC_STAT *emcStatus;

// EMC IO status
extern EMC_IO_STAT *emcIoStatus;

// EMC MOTION status
extern EMC_MOTION_STAT *emcMotionStatus;

// values for EMC_JOINT_SET_JOINT, jointType
enum EmcJointType {
    EMC_LINEAR             = 1,
    EMC_ANGULAR            = 2,
};

/**
 * Set the units conversion factor.
 * @see EMC_JOINT_SET_INPUT_SCALE
 */
using EmcLinearUnits = double;
using EmcAngularUnits = double;

#endif				// #ifndef EMC_HH
