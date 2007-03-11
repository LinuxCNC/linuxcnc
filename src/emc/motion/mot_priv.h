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
    hal_float_t joint_vel_cmd;	/* RPA: commanded velocity, w/o comp */
    hal_float_t backlash_corr;	/* RPA: correction for backlash */
    hal_float_t backlash_filt;	/* RPA: filtered backlash correction */
    hal_float_t backlash_vel;	/* RPA: backlash speed variable */
    hal_float_t *motor_pos_cmd;	/* WPI: commanded position, with comp */
    hal_float_t *motor_pos_fb;	/* RPI: position feedback, with comp */
    hal_float_t *joint_pos_cmd;	/* WPI: commanded position w/o comp, mot ofs */
    hal_float_t *joint_pos_fb;	/* RPA: position feedback, w/o comp */
    hal_float_t f_error;	/* RPA: following error */
    hal_float_t f_error_lim;	/* RPA: following error limit */

/*! \todo FIXME - these might not be HAL params forever, but they are usefull now */
    hal_float_t free_pos_cmd;	/* RPA: free traj planner pos cmd */
    hal_float_t free_vel_lim;	/* RPA: free traj planner vel limit */
    hal_bit_t free_tp_enable;	/* RPA: free traj planner is running */
    hal_s32_t free_tp_source;   /* RPA: free traj planner source of command */

    hal_bit_t active;		/* RPA: axis is active, whatever that means */
    hal_bit_t in_position;	/* RPA: axis is in position */
    hal_bit_t error;		/* RPA: axis has an error */
    hal_bit_t psl;		/* RPA: axis is at positive soft limit */
    hal_bit_t nsl;		/* RPA: axis is at negative soft limit */
    hal_bit_t phl;		/* RPA: axis is at positive hard limit */
    hal_bit_t nhl;		/* RPA: axis is at negative hard limit */
    hal_bit_t *homing;		/* RPI: axis is homing */
    hal_bit_t homed;		/* RPA: axis was homed */
    hal_bit_t f_errored;	/* RPA: axis had too much following error */
    hal_bit_t faulted;		/* RPA: axis amp faulted */
    hal_bit_t *pos_lim_sw;	/* RPI: positive limit switch input */
    hal_bit_t *neg_lim_sw;	/* RPI: negative limit switch input */
    hal_bit_t *home_sw;		/* RPI: home switch input */
    hal_bit_t *index_enable;	/* RPIO: motmod sets: request reset on index
				         encoder clears: index arrived */
    hal_bit_t *amp_fault;	/* RPI: amp fault input */
    hal_bit_t *amp_enable;	/* WPI: amp enable output */
    hal_s32_t home_state;	/* RPA: homing state machine state */

    hal_s32_t *jog_counts;	/* RPI: jogwheel position input */
    hal_bit_t *jog_enable;	/* RPI: enable jogwheel */
    hal_float_t *jog_scale;	/* RPI: distance to jog on each count */

} axis_hal_t;

/* machine data */

typedef struct {
    hal_bit_t *probe_input;	/* RPI: probe switch input */
    hal_bit_t *enable;		/* RPI: motion inhibit input */
    hal_bit_t *spindle_index_enable;
    hal_float_t *spindle_revs;
    hal_float_t *adaptive_feed;	/* RPI: adaptive feedrate, 0.0 to 1.0 */
    hal_bit_t *feed_hold;	/* RPI: set TRUE to stop motion */
    hal_bit_t motion_enabled;	/* RPA: motion enable for all axis */
    hal_bit_t in_position;	/* RPA: all axis are in position */
    hal_bit_t *inpos_output;	/* WPI: all axes are in position (used to power down steppers for example) */
    hal_bit_t coord_mode;	/* RPA: TRUE if coord, FALSE if free */
    hal_bit_t teleop_mode;	/* RPA: TRUE if teleop mode */
    hal_bit_t coord_error;	/* RPA: TRUE if coord mode error */

    hal_bit_t debug_bit_0;	/* RPA: generic param, for debugging */
    hal_bit_t debug_bit_1;	/* RPA: generic param, for debugging */
    hal_float_t debug_float_0;	/* RPA: generic param, for debugging */
    hal_float_t debug_float_1;	/* RPA: generic param, for debugging */
    hal_s32_t debug_s32_0;	/* RPA: generic param, for debugging */
    hal_s32_t debug_s32_1;	/* RPA: generic param, for debugging */
    
    hal_bit_t *synch_do[EMCMOT_MAX_DIO]; /* WPI array: output pins for motion synched IO */


    // creating a lot of pins for spindle control to be very flexible
    // the user needs only a subset of these

    // simplest way of spindle control (output start/stop)
    hal_bit_t *spindle_on;	/* spindle spin output */

    // same thing for 2 directions
    hal_bit_t *spindle_forward;	/* spindle spin-forward output */
    hal_bit_t *spindle_reverse;	/* spindle spin-reverse output */

    // simple velocity control (as long as the output is active the spindle
    //                          should accelerate/decelerate
    hal_bit_t *spindle_incr_speed;	/* spindle spin-increase output */
    hal_bit_t *spindle_decr_speed;	/* spindle spin-decrease output */

    // simple output for brake
    hal_bit_t *spindle_brake;	/* spindle brake output */

    // output of a prescribed speed (to hook-up to a velocity controller)
    hal_float_t *spindle_speed_out;	/* spindle speed output */
    hal_float_t *spindle_speed_in;	/* spindle speed measured */
    
    // FIXME - debug only, remove later
    hal_float_t traj_pos_out;	/* RPA: traj internals, for debugging */
    hal_float_t traj_vel_out;	/* RPA: traj internals, for debugging */
    hal_u32_t traj_active_tc;	/* RPA: traj internals, for debugging */
    hal_float_t tc_pos[4];	/* RPA: traj internals, for debugging */
    hal_float_t tc_vel[4];	/* RPA: traj internals, for debugging */
    hal_float_t tc_acc[4];	/* RPA: traj internals, for debugging */

    // realtime overrun detection
    hal_u32_t last_period;	/* param: last period in clocks */
    hal_float_t last_period_ns;	/* param: last period in clocks */
    hal_u32_t overruns;		/* param: count of RT overruns */

    axis_hal_t axis[EMCMOT_MAX_AXIS];	/* data for each axis */

} emcmot_hal_data_t;

/***********************************************************************
*                   GLOBAL VARIABLE DECLARATIONS                       *
************************************************************************/

/* HAL component ID for motion module */
extern int mot_comp_id;

/* userdefined number of max joints. default is EMCMOT_MAX_AXIS(=8), 
   but can be altered at motmod insmod time */
extern int EMCMOT_MAX_JOINTS;

/* pointer to emcmot_hal_data_t struct in HAL shmem, with all HAL data */
extern emcmot_hal_data_t *emcmot_hal_data;

/* pointer to array of joint structs with all joint data */
/* the actual array may be in shared memory or in kernel space, as
   determined by the init code in motion.c
*/
extern emcmot_joint_t *joints;

/* flag used to indicate that this is the very first pass thru the
   code.  Various places in the code use this to set initial conditions
   and avoid startup glitches.
*/
extern int first_pass;

/* Variable defs */
extern int kinType;
extern int rehomeAll;
extern int DEBUG_MOTION;
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

/***********************************************************************
*                    PUBLIC FUNCTION PROTOTYPES                        *
************************************************************************/

/* function definitions */
extern void emcmotCommandHandler(void *arg, long period);
extern void emcmotController(void *arg, long period);

/* these are related to synchronized I/O */
extern void emcmotDioWrite(int index, char value);
extern void emcmotAioWrite(int index, double value);

/* loops through the active joints and checks if any are not homed */
extern int checkAllHomed(void);
/* recalculates jog limits */
extern void refresh_jog_limits(emcmot_joint_t *joint);
/* handles 'homed' flags, see command.c for details */
extern void clearHomes(int joint_num);

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

#define GET_JOINT_AT_HOME_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_AT_HOME_BIT ? 1 : 0)

#define SET_JOINT_AT_HOME_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_AT_HOME_BIT; else (joint)->flag &= ~EMCMOT_AXIS_AT_HOME_BIT;

#define GET_JOINT_FERROR_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_FERROR_BIT ? 1 : 0)

#define SET_JOINT_FERROR_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_FERROR_BIT; else (joint)->flag &= ~EMCMOT_AXIS_FERROR_BIT;

#define GET_JOINT_FAULT_FLAG(joint) ((joint)->flag & EMCMOT_AXIS_FAULT_BIT ? 1 : 0)

#define SET_JOINT_FAULT_FLAG(joint,fl) if (fl) (joint)->flag |= EMCMOT_AXIS_FAULT_BIT; else (joint)->flag &= ~EMCMOT_AXIS_FAULT_BIT;

#ifdef LINUX_VERSION_CODE
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#define HAVE_CPU_KHZ
#endif
#endif
#endif /* MOT_PRIV_H */
