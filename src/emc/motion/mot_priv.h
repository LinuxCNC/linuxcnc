#ifndef MOT_PRIV_H
#define MOT_PRIV_H

#include "emcmot.h"

/* function definitions */
extern void emcmot_config_change(void);
extern int emcmotCommandHandler(void);
extern void emcmotController(void *arg);
extern void setTrajCycleTime(double secs);

 /* rtapi_get_time() returns a nanosecond value. In time, we should use a u64 
    value for all calcs and only do the conversion to seconds when it is
    really needed. */
#define etime() (((double) rtapi_get_time()) / 1.0e9)

extern void reportError(const char *fmt, ...);	/* Use the rtapi_print call */

/* Variable defs */
extern int kinType;
extern int rehomeAll;
extern int DEBUG_MOTION;
extern int logSkip;
extern int loggingAxis;
extern int logStartTime;
extern EmcPose worldHome;
extern int EMCMOT_NO_FORWARD_KINEMATICS;
extern KINEMATICS_FORWARD_FLAGS fflags;
extern KINEMATICS_INVERSE_FLAGS iflags;

/* Struct pointers */
extern EMCMOT_STRUCT *emcmotStruct;
extern EMCMOT_COMMAND *emcmotCommand;
extern EMCMOT_STATUS *emcmotStatus;
extern EMCMOT_CONFIG *emcmotConfig;
extern EMCMOT_DEBUG *emcmotDebug;
extern EMCMOT_ERROR *emcmotError;
extern EMCMOT_LOG *emcmotLog;
extern EMCMOT_IO *emcmotIo;
extern EMCMOT_COMP *emcmotComp[EMCMOT_MAX_AXIS];
extern EMCMOT_LOG_STRUCT ls;

/* macros for reading, writing bit flags */

/* motion flags */

#define GET_MOTION_ERROR_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_ERROR_BIT ? 1 : 0)

#define SET_MOTION_ERROR_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_ERROR_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_ERROR_BIT;

#define GET_MOTION_COORD_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_COORD_BIT ? 1 : 0)

#define SET_MOTION_COORD_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_COORD_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_COORD_BIT;

#define GET_MOTION_TELEOP_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_TELEOP_BIT ? 1 : 0)

#define SET_MOTION_TELEOP_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_TELEOP_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_TELEOP_BIT;

#define GET_MOTION_INPOS_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_INPOS_BIT ? 1 : 0)

#define SET_MOTION_INPOS_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_INPOS_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_INPOS_BIT;

#define GET_MOTION_ENABLE_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_ENABLE_BIT ? 1 : 0)

#define SET_MOTION_ENABLE_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_ENABLE_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_ENABLE_BIT;

/* axis flags */

#define GET_AXIS_ENABLE_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0)

#define SET_AXIS_ENABLE_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_ENABLE_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_ENABLE_BIT;

#define GET_AXIS_ACTIVE_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_ACTIVE_BIT ? 1 : 0)

#define SET_AXIS_ACTIVE_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_ACTIVE_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_ACTIVE_BIT;

#define GET_AXIS_INPOS_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_INPOS_BIT ? 1 : 0)

#define SET_AXIS_INPOS_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_INPOS_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_INPOS_BIT;

#define GET_AXIS_ERROR_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_ERROR_BIT ? 1 : 0)

#define SET_AXIS_ERROR_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_ERROR_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_ERROR_BIT;

#define GET_AXIS_PSL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_PSL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT;

#define GET_AXIS_NSL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_NSL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT;

#define GET_AXIS_PHL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_PHL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MAX_HARD_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MAX_HARD_LIMIT_BIT;

#define GET_AXIS_NHL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_NHL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MIN_HARD_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MIN_HARD_LIMIT_BIT;

#define GET_AXIS_HOME_SWITCH_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_HOME_SWITCH_BIT ? 1 : 0)

#define SET_AXIS_HOME_SWITCH_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_HOME_SWITCH_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_HOME_SWITCH_BIT;

#define GET_AXIS_HOMING_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_HOMING_BIT ? 1 : 0)

#define SET_AXIS_HOMING_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_HOMING_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_HOMING_BIT;

#define GET_AXIS_HOMED_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_HOMED_BIT ? 1 : 0)

#define SET_AXIS_HOMED_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_HOMED_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_HOMED_BIT;

#define GET_AXIS_FERROR_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_FERROR_BIT ? 1 : 0)

#define SET_AXIS_FERROR_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_FERROR_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_FERROR_BIT;

#define GET_AXIS_FAULT_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_FAULT_BIT ? 1 : 0)

#define SET_AXIS_FAULT_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_FAULT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_FAULT_BIT;

/* polarity flags */

#define GET_AXIS_ENABLE_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0)

#define SET_AXIS_ENABLE_POLARITY(ax,fl) MARK_EMCMOT_CONFIG_CHANGE(); if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_ENABLE_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_ENABLE_BIT;

#define GET_AXIS_PHL_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_PHL_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_MAX_HARD_LIMIT_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_MAX_HARD_LIMIT_BIT;

#define GET_AXIS_NHL_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_NHL_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_MIN_HARD_LIMIT_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_MIN_HARD_LIMIT_BIT;

#define GET_AXIS_HOME_SWITCH_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_HOME_SWITCH_BIT ? 1 : 0)

#define SET_AXIS_HOME_SWITCH_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_HOME_SWITCH_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_HOME_SWITCH_BIT;

#define GET_AXIS_HOMING_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_HOMING_BIT ? 1 : 0)

#define SET_AXIS_HOMING_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_HOMING_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_HOMING_BIT;

#define GET_AXIS_FAULT_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_FAULT_BIT ? 1 : 0)

#define SET_AXIS_FAULT_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_FAULT_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_FAULT_BIT;

/* axis lengths */
#define AXRANGE(axis) ((emcmotConfig->maxLimit[axis] - emcmotConfig->minLimit[axis]))

#endif /* MOT_PRIV_H */
