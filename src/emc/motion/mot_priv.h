#ifndef MOT_PRIV_H
#define MOT_PRIV_H
/********************************************************************
* Description: mot_priv.h
*   Macros and declarations local to the realtime sources.
*
* Author: 
* License: GPL Version 2
* Created on:
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
*
********************************************************************/


/***********************************************************************
*                       TYPEDEFS, ENUMS, ETC.                          *
************************************************************************/

/* HAL visible data notations:
   RPA:  read only parameter
   WPA:  write only parameter
   PA:   read/write parameter
   RP:   read only pin
   WP:   write only pin
   P:    read/write pin
*/

/* HAL-visible runtime data for a single axis */
typedef struct {
    hal_float_t joint_pos_cmd;	/* RPA: commanded position, w/o comp */
    hal_float_t joint_vel_cmd;  /* RPA: commanded velocity, w/o comp */
    hal_float_t backlash_corr;	/* RPA: correction for backlash */
    hal_float_t backlash_filt;	/* RPA: filtered backlash correction */
    hal_float_t *motor_pos_cmd;	/* WP:  commanded position, with comp */
    hal_float_t *motor_pos_fb;	/* RP:  position feedback, with comp */
    hal_float_t joint_pos_fb;	/* RPA: position feedback, w/o comp */

/* OLD PINS */

    hal_float_t *volts;		/* pin: voltage output */
    hal_float_t *position;	/* pin: position input */
    hal_bit_t *max;		/* max limit switch input */
    hal_bit_t *min;		/* min limit switch input */
    hal_bit_t *home;		/* home switch input */
    hal_float_t *probe;		/* probe input */
    hal_bit_t *enable;		/* amp enable output */
    hal_bit_t *fault;		/* amp fault input */
    /* for now we control the index model through the mode and model pins on
       axis 0, later this may be done on a per axis basis */
    hal_u32_t *mode;		/* index model output */
    hal_u32_t *model;		/* index model input */
    hal_bit_t *reset;		/* index latch reset output */
    hal_bit_t *latch;		/* index latch input */
    hal_bit_t *index;		/* index input */
} axis_hal_t;


/***********************************************************************
*                   GLOBAL VARIABLE DECLARATIONS                       *
************************************************************************/

/* HAL component ID for motion module */
extern int mot_comp_id;

/* pointer to array of axis_hal_t structs in HAL shmem, 1 per axis */
extern axis_hal_t *axis_hal_array;

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
extern emcmot_struct_t *emcmotStruct;
extern emcmot_command_t *emcmotCommand;
extern emcmot_status_t *emcmotStatus;
extern emcmot_config_t *emcmotConfig;
extern emcmot_debug_t *emcmotDebug;
extern emcmot_error_t *emcmotError;
extern emcmot_log_t *emcmotLog;
extern emcmot_comp_t *emcmotComp[EMCMOT_MAX_AXIS];
extern emcmot_log_struct_t ls;

/***********************************************************************
*                    PUBLIC FUNCTION PROTOTYPES                        *
************************************************************************/



/* function definitions */
extern void emcmotCommandHandler(void *arg, long period);
extern void emcmotController(void *arg, long period);

extern void emcmot_config_change(void);
extern void reportError(const char *fmt, ...);	/* Use the rtapi_print call */

 /* rtapi_get_time() returns a nanosecond value. In time, we should use a u64
    value for all calcs and only do the conversion to seconds when it is
    really needed. */
#define etime() (((double) rtapi_get_time()) / 1.0e9)


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
