/********************************************************************
* Description: mot_priv.h
*   Macros and declarations local to the realtime sources.
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
*
********************************************************************/
#ifndef MOT_PRIV_H
#define MOT_PRIV_H

/***********************************************************************
*                       TYPEDEFS, ENUMS, ETC.                          *
************************************************************************/

/* First we define structures for data shared with the HAL */

/* HAL visible data notations:
   RPA:  read only parameter
   WPA:  write only parameter
   WRPA: read/write parameter
   RPI:  read only pin
   WPI:  write only pin
   WRPI: read/write pin
*/

/* axis data */

typedef struct {
    hal_float_t coarse_pos_cmd;	/* RPA: commanded position, w/o comp */
    hal_float_t joint_pos_cmd;	/* RPA: commanded position, w/o comp */
    hal_float_t joint_vel_cmd;	/* RPA: commanded velocity, w/o comp */
    hal_float_t backlash_corr;	/* RPA: correction for backlash */
    hal_float_t backlash_filt;	/* RPA: filtered backlash correction */
    hal_float_t *motor_pos_cmd;	/* WPI: commanded position, with comp */
    hal_float_t *motor_pos_fb;	/* RPI: position feedback, with comp */
    hal_float_t joint_pos_fb;	/* RPA: position feedback, w/o comp */
    hal_float_t f_error;	/* RPA: following error */
    hal_float_t f_error_lim;	/* RPA: following error limit */

/* FIXME - these might not be HAL params forever, but they are usefull now */
    hal_float_t free_pos_cmd;	/* RPA: free traj planner pos cmd */
    hal_float_t free_vel_lim;	/* RPA: free traj planner vel limit */
    hal_bit_t free_tp_enable;	/* RPA: free traj planner is running */

    hal_bit_t active;		/* RPA: axis is active, whatever that means */
    hal_bit_t in_position;	/* RPA: axis is in position */
    hal_bit_t error;		/* RPA: axis has an error */
    hal_bit_t psl;		/* RPA: axis is at positive soft limit */
    hal_bit_t nsl;		/* RPA: axis is at negative soft limit */
    hal_bit_t phl;		/* RPA: axis is at positive hard limit */
    hal_bit_t nhl;		/* RPA: axis is at negative hard limit */
    hal_bit_t homing;		/* RPA: axis is homing */
    hal_bit_t homed;		/* RPA: axis was homed */
    hal_bit_t f_errored;	/* RPA: axis had too much following error */
    hal_bit_t faulted;		/* RPA: axis amp faulted */
    hal_bit_t *pos_lim_sw;	/* RPI: positive limit switch input */
    hal_bit_t *neg_lim_sw;	/* RPI: negative limit switch input */
    hal_bit_t *home_sw;		/* RPI: home switch input */
    hal_bit_t *index_pulse;	/* RPI: index pulse input */
    hal_bit_t *amp_fault;	/* RPI: amp fault input */
    hal_bit_t *amp_enable;	/* WPI: amp enable output */
    hal_s8_t home_state;	/* RPA: homing state machine state */

/* FIXME - these have been temporarily? deleted */
#if 0
    /* for now we control the index model through the mode and model pins on
       axis 0, later this may be done on a per axis basis */
    hal_u32_t *mode;		/* index model output */
    hal_u32_t *model;		/* index model input */
    hal_bit_t *reset;		/* index latch reset output */
    hal_bit_t *latch;		/* index latch input */
    hal_bit_t *index;		/* index input */
#endif
} axis_hal_t;

/* machine data */

typedef struct {
    hal_bit_t *probe_input;	/* RPI: probe switch input */
    hal_bit_t motion_enable;	/* RPA: motion enable for all axis */
    hal_bit_t in_position;	/* RPA: all axis are in position */
    hal_bit_t coord_mode;	/* RPA: TRUE if coord, FALSE if free */
    hal_bit_t teleop_mode;	/* RPA: TRUE if teleop mode */
    hal_bit_t coord_error;	/* RPA: TRUE if coord mode error */

    hal_bit_t debug_bit_0;	/* RPA: generic param, for debugging */
    hal_bit_t debug_bit_1;	/* RPA: generic param, for debugging */
    hal_float_t debug_float_0;	/* RPA: generic param, for debugging */
    hal_float_t debug_float_1;	/* RPA: generic param, for debugging */

    axis_hal_t axis[EMCMOT_MAX_AXIS];	/* data for each axis */

} machine_hal_t;

/***********************************************************************
*                   GLOBAL VARIABLE DECLARATIONS                       *
************************************************************************/

/* HAL component ID for motion module */
extern int mot_comp_id;

/* pointer to machine_hal_t struct in HAL shmem, with all HAL data */
extern machine_hal_t *machine_hal_data;

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
extern emcmot_internal_t *emcmotInternal;
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

/* joint flags */

#define GET_JOINT_ENABLE_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0)

#define SET_JOINT_ENABLE_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_ENABLE_BIT; else (joint)->flag &= ~EMCMOT_AXIS_ENABLE_BIT;

#define GET_JOINT_ACTIVE_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_ACTIVE_BIT ? 1 : 0)

#define SET_JOINT_ACTIVE_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_ACTIVE_BIT; else (joint)->flag &= ~EMCMOT_AXIS_ACTIVE_BIT;

#define GET_JOINT_INPOS_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_INPOS_BIT ? 1 : 0)

#define SET_JOINT_INPOS_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_INPOS_BIT; else (joint)->flag &= ~EMCMOT_AXIS_INPOS_BIT;

#define GET_JOINT_ERROR_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_ERROR_BIT ? 1 : 0)

#define SET_JOINT_ERROR_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_ERROR_BIT; else (joint)->flag &= ~EMCMOT_AXIS_ERROR_BIT;

#define GET_JOINT_PSL_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT ? 1 : 0)

#define SET_JOINT_PSL_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT; else (joint)->flag &= ~EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT;

#define GET_JOINT_NSL_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT ? 1 : 0)

#define SET_JOINT_NSL_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT; else (joint)->flag &= ~EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT;

#define GET_JOINT_PHL_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT ? 1 : 0)

#define SET_JOINT_PHL_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_MAX_HARD_LIMIT_BIT; else (joint)->flag &= ~EMCMOT_AXIS_MAX_HARD_LIMIT_BIT;

#define GET_JOINT_NHL_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT ? 1 : 0)

#define SET_JOINT_NHL_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_MIN_HARD_LIMIT_BIT; else (joint)->flag &= ~EMCMOT_AXIS_MIN_HARD_LIMIT_BIT;

#define GET_JOINT_HOME_SWITCH_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_HOME_SWITCH_BIT ? 1 : 0)

#define SET_JOINT_HOME_SWITCH_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_HOME_SWITCH_BIT; else (joint)->flag &= ~EMCMOT_AXIS_HOME_SWITCH_BIT;

#define GET_JOINT_HOMING_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_HOMING_BIT ? 1 : 0)

#define SET_JOINT_HOMING_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_HOMING_BIT; else (joint)->flag &= ~EMCMOT_AXIS_HOMING_BIT;

#define GET_JOINT_HOMED_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_HOMED_BIT ? 1 : 0)

#define SET_JOINT_HOMED_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_HOMED_BIT; else (joint)->flag &= ~EMCMOT_AXIS_HOMED_BIT;

#define GET_JOINT_FERROR_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_FERROR_BIT ? 1 : 0)

#define SET_JOINT_FERROR_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_FERROR_BIT; else (joint)->flag &= ~EMCMOT_AXIS_FERROR_BIT;

#define GET_JOINT_FAULT_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_FAULT_BIT ? 1 : 0)

#define SET_JOINT_FAULT_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_FAULT_BIT; else (joint)->flag &= ~EMCMOT_AXIS_FAULT_BIT;

#endif /* MOT_PRIV_H */
