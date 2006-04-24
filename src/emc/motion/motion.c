/********************************************************************
* Description: motion.c
*   Main module initialisation and cleanup routines.
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

#ifndef RTAPI
#error This is a realtime component only!
#endif
#include <stdarg.h>
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* decls for HAL implementation */
#include "emcmotglb.h"
#include "motion.h"
#include "mot_priv.h"

/***********************************************************************
*                    KERNEL MODULE PARAMETERS                          *
************************************************************************/

#ifdef MODULE
/* module information */
/* register symbols to be modified by insmod
   see "Linux Device Drivers", Alessandro Rubini, p. 385
   (p.42-44 in 2nd edition) */
MODULE_AUTHOR("Matt Shaver/John Kasunich");
MODULE_DESCRIPTION("Motion Controller for EMC");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */

/*! \todo FIXME - find a better way to do this */
int DEBUG_MOTION = 0;
MODULE_PARM(DEBUG_MOTION, "i");

/* RTAPI shmem key - for comms with higher level user space stuff */
static int key = 100;		/* the shared memory key, default value */
MODULE_PARM(key, "i");
MODULE_PARM_DESC(key, "shared memory key");

/*! \todo Another #if 0 */
#if 0
/*! \todo FIXME - currently HAL has a fixed stacksize of 16384...
   the upcoming HAL rewrite may make it a paramater of the
   create_thread call, in which case this will be restored */
static int EMCMOT_TASK_STACKSIZE = 8192;	/* default stacksize */
MODULE_PARM(EMCMOT_TASK_STACKSIZE, "i");
MODULE_PARM_DESC(EMCMOT_TASK_STACKSIZE, "motion stack size");
#endif

static long base_period_nsec = 0;	/* fastest thread period */
MODULE_PARM(base_period_nsec, "l");
MODULE_PARM_DESC(base_period_nsec, "fastest thread period (nsecs)");
static long servo_period_nsec = 0;	/* servo thread period */
MODULE_PARM(servo_period_nsec, "l");
MODULE_PARM_DESC(servo_period_nsec, "servo thread period (nsecs)");
static long traj_period_nsec = 0;	/* trajectory planner period */
MODULE_PARM(traj_period_nsec, "l");
MODULE_PARM_DESC(traj_period_nsec, "trajectory planner period (nsecs)");

#endif /* MODULE */

/***********************************************************************
*                  GLOBAL VARIABLE DEFINITIONS                         *
************************************************************************/

/* pointer to emcmot_hal_data_t struct in HAL shmem, with all HAL data */
emcmot_hal_data_t *emcmot_hal_data = 0;

/* pointer to joint data */
emcmot_joint_t *joints = 0;

#ifndef STRUCTS_IN_SHMEM
/* allocate array for joint data */
emcmot_joint_t joint_array[EMCMOT_MAX_AXIS];
#endif

int mot_comp_id;		/* component ID for motion module */

int kinType = 0;

/*
  Principles of communication:

  Data is copied in or out from the various types of comm mechanisms:
  mbuff mapped memory for Linux/RT-Linux, or OS shared memory for Unixes.

  emcmotStruct is ptr to this memory.

  emcmotCommand points to emcmotStruct->command,
  emcmotStatus points to emcmotStruct->status,
  emcmotError points to emcmotStruct->error, and
 */
emcmot_struct_t *emcmotStruct = 0;
/* ptrs to either buffered copies or direct memory for
   command and status */
emcmot_command_t *emcmotCommand = 0;
emcmot_status_t *emcmotStatus = 0;
emcmot_config_t *emcmotConfig = 0;
emcmot_debug_t *emcmotDebug = 0;
emcmot_internal_t *emcmotInternal = 0;
emcmot_error_t *emcmotError = 0;	/* unused for RT_FIFO */

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/* RTAPI shmem ID - for comms with higher level user space stuff */
static int emc_shmem_id;	/* the shared memory ID */

/***********************************************************************
*                   LOCAL FUNCTION PROTOTYPES                          *
************************************************************************/

/* init_hal_io() exports HAL pins and parameters making data from
   the realtime control module visible and usable by the world
*/
static int init_hal_io(void);

/* functions called by init_hal_io() */
static int export_axis(int num, axis_hal_t * addr);

/* init_comm_buffers() allocates and initializes the command,
   status, and error buffers used to communicate witht the user
   space parts of emc.
*/
static int init_comm_buffers(void);

/* functions called by init_comm_buffers() */

/* init_threads() creates realtime threads, exports functions to
   do the realtime control, and adds the functions to the threads.
*/
static int init_threads(void);

/* functions called by init_threads() */
static int setTrajCycleTime(double secs);
static int setServoCycleTime(double secs);

/***********************************************************************
*                     PUBLIC FUNCTION CODE                             *
************************************************************************/

void emcmot_config_change(void)
{
    if (emcmotConfig->head == emcmotConfig->tail) {
	emcmotConfig->config_num++;
	emcmotStatus->config_num = emcmotConfig->config_num;
	emcmotConfig->head++;
    }
}

void reportError(const char *fmt, ...)
{
    va_list args;
    char error[EMCMOT_ERROR_LEN + 2];

    va_start(args, fmt);
    /* Don't use the rtapi_snprintf... */
    vsnprintf(error, EMCMOT_ERROR_LEN, fmt, args);
    va_end(args);
/*! \todo FIXME - eventually should print _only_ to the RCS buffer, I think */
/* print to the kernel buffer... */
    rtapi_print("%d: ERROR: %s\n", emcmotStatus->heartbeat, error);
/* print to the RCS buffer... */
    emcmotErrorPut(emcmotError, error);
}

int init_module(void)
{
    int retval;

    /*! \todo FIXME - debug only */
//    rtapi_set_msg_level(RTAPI_MSG_ALL);

    rtapi_print_msg(RTAPI_MSG_INFO, "MOTION: init_module() starting...\n");

    /* connect to the HAL and RTAPI */
    mot_comp_id = hal_init("motmod");
    if (mot_comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: hal_init() failed\n");
	return -1;
    }

    /* initialize/export HAL pins and parameters */
    retval = init_hal_io();
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: init_hal_io() failed\n");
	hal_exit(mot_comp_id);
	return -1;
    }

    /* allocate/initialize user space comm buffers (cmd/status/err) */
    retval = init_comm_buffers();
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: init_comm_buffers() failed\n");
	hal_exit(mot_comp_id);
	return -1;
    }

    /* set up for realtime execution of code */
    retval = init_threads();
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: init_threads() failed\n");
	hal_exit(mot_comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO, "MOTION: init_module() complete\n");

    return 0;
}

void cleanup_module(void)
{
//    int axis;
    int retval;

    rtapi_print_msg(RTAPI_MSG_INFO, "MOTION: cleanup_module() started.\n");

    retval = hal_stop_threads();
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: hal_stop_threads() failed, returned %d\n", retval);
    }
/*! \todo Another #if 0 */
#if 0
    /* WPS these were moved from above to avoid a possible mutex problem. */
    /* There is no point in clearing the trajectory queue since the planner
       should be dead by now anyway. */
    if (emcmotStruct != 0 && emcmotDebug != 0 && emcmotConfig != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "motion: disabling amps\n");
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
	}
    }
#endif

    /* free shared memory */
    retval = rtapi_shmem_delete(emc_shmem_id, mot_comp_id);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: rtapi_shmem_delete() failed, returned %d\n", retval);
    }
    /* disconnect from HAL and RTAPI */
    retval = hal_exit(mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: hal_exit() failed, returned %d\n", retval);
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "MOTION: cleanup_module() finished.\n");
}

/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

/* init_hal_io() exports HAL pins and parameters making data from
   the realtime control module visible and usable by the world
*/
static int init_hal_io(void)
{
    int n, retval;
    axis_hal_t *axis_data;
    char buf[HAL_NAME_LEN + 2];

    rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: init_hal_io() starting...\n");

    /* allocate shared memory for machine data */
    emcmot_hal_data = hal_malloc(sizeof(emcmot_hal_data_t));
    if (emcmot_hal_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: emcmot_hal_data malloc failed\n");
	return -1;
    }

    /* export machine wide hal pins */
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.probe-input");
    retval =
	hal_pin_bit_new(buf, HAL_RD, &(emcmot_hal_data->probe_input),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.spindle-sync");
    retval =
	hal_pin_bit_new(buf, HAL_WR, &(emcmot_hal_data->spindle_sync),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.spindle-revs");
    retval =
	hal_pin_float_new(buf, HAL_RD, &(emcmot_hal_data->spindle_revs),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.enable");
    retval =
	hal_pin_bit_new(buf, HAL_RD, &(emcmot_hal_data->enable),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    /* export motion-synched digital output pins */
    for (n = 0; n < EMCMOT_MAX_DIO; n++) {
	rtapi_snprintf(buf, HAL_NAME_LEN, "motion.digital-out-%02d",n);
	retval =
	    hal_pin_bit_new(buf, HAL_WR, &(emcmot_hal_data->synch_do[n]),
	    mot_comp_id);
	if (retval != 0) {
	    return retval;
	}
    }

    /* export machine wide hal parameters */
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.motion-enabled");
    retval =
	hal_param_bit_new(buf, HAL_RD, &(emcmot_hal_data->motion_enabled),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.in-position");
    retval =
	hal_param_bit_new(buf, HAL_RD, &(emcmot_hal_data->in_position),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.coord-mode");
    retval =
	hal_param_bit_new(buf, HAL_RD, &(emcmot_hal_data->coord_mode),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.teleop-mode");
    retval =
	hal_param_bit_new(buf, HAL_RD, &(emcmot_hal_data->teleop_mode),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.coord-error");
    retval =
	hal_param_bit_new(buf, HAL_RD, &(emcmot_hal_data->coord_error),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export debug parameters */
    /* these can be used to view any internal variable, simply change a line
       in control.c:output_to_hal() and recompile */
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.debug-bit-0");
    retval =
	hal_param_bit_new(buf, HAL_RD, &(emcmot_hal_data->debug_bit_0),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.debug-bit-1");
    retval =
	hal_param_bit_new(buf, HAL_RD, &(emcmot_hal_data->debug_bit_1),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.debug-float-0");
    retval =
	hal_param_float_new(buf, HAL_RD, &(emcmot_hal_data->debug_float_0),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "motion.debug-float-1");
    retval =
	hal_param_float_new(buf, HAL_RD, &(emcmot_hal_data->debug_float_1),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    // FIXME - debug only, remove later
    // export HAL parameters for some trajectory planner internal variables
    // so they can be scoped
    rtapi_snprintf(buf, HAL_NAME_LEN, "traj.pos_out");
    retval =
	hal_param_float_new(buf, HAL_RD, &(emcmot_hal_data->traj_pos_out),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "traj.vel_out");
    retval =
	hal_param_float_new(buf, HAL_RD, &(emcmot_hal_data->traj_vel_out),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "traj.active_tc");
    retval =
	hal_param_u8_new(buf, HAL_RD, &(emcmot_hal_data->traj_active_tc),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    for ( n = 0 ; n < 4 ; n++ ) {
	rtapi_snprintf(buf, HAL_NAME_LEN, "tc.%d.pos", n);
	retval = hal_param_float_new(buf, HAL_RD, &(emcmot_hal_data->tc_pos[n]), mot_comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "tc.%d.vel", n);
	retval = hal_param_float_new(buf, HAL_RD, &(emcmot_hal_data->tc_vel[n]), mot_comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "tc.%d.acc", n);
	retval = hal_param_float_new(buf, HAL_RD, &(emcmot_hal_data->tc_acc[n]), mot_comp_id);
	if (retval != 0) {
	    return retval;
	}
    }
    // end of exporting trajectory planner internals

    /* initialize machine wide pins and parameters */
    *(emcmot_hal_data->probe_input) = 0;
    /* default value of enable is TRUE, so simple machines
       can leave it disconnected */
    *(emcmot_hal_data->enable) = 1;
    
    /* motion synched dio, init to not enabled */
    for (n = 0; n < EMCMOT_MAX_DIO; n++) {
	 *(emcmot_hal_data->synch_do[n]) = 0;
    }
    
    /*! \todo FIXME - these don't really need initialized, since they are written
       with data from the emcmotStatus struct */
    emcmot_hal_data->motion_enabled = 0;
    emcmot_hal_data->in_position = 0;
    emcmot_hal_data->coord_mode = 0;
    emcmot_hal_data->teleop_mode = 0;
    emcmot_hal_data->coord_error = 0;

    /* init debug parameters */
    emcmot_hal_data->debug_bit_0 = 0;
    emcmot_hal_data->debug_bit_1 = 0;
    emcmot_hal_data->debug_float_0 = 0.0;
    emcmot_hal_data->debug_float_1 = 0.0;

    /* export axis pins and parameters */
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
	/* point to axis data */
	axis_data = &(emcmot_hal_data->axis[n]);
	/* export all vars */
	retval = export_axis(n, axis_data);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"MOTION: axis %d pin/param export failed\n", n);
	    return -1;
	}
	/* init axis pins and parameters */
	/*! \todo FIXME - struct members are in a state of flux - make sure to
	   update this - most won't need initing anyway */
	*(axis_data->amp_enable) = 0;
	axis_data->home_state = 0;
	/* We'll init the index model to EXT_ENCODER_INDEX_MODEL_RAW for now,
	   because it is always supported. */
/*! \todo Another #if 0 */
#if 0
	*(axis_data->mode) = EXT_ENCODER_INDEX_MODEL_RAW;
	*(axis_data->reset) = 0;
#endif
    }
    /* Done! */
    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_hal_io() complete, %d axes.\n", EMCMOT_MAX_AXIS);
    return 0;
}

static int export_axis(int num, axis_hal_t * addr)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export axis pins */
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.motor-pos-cmd", num);
    retval =
	hal_pin_float_new(buf, HAL_WR, &(addr->motor_pos_cmd), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.motor-pos-fb", num);
    retval =
	hal_pin_float_new(buf, HAL_RD, &(addr->motor_pos_fb), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.pos-lim-sw-in", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->pos_lim_sw), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.neg-lim-sw-in", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->neg_lim_sw), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.home-sw-in", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->home_sw), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.index-pulse-in", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->index_pulse), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.amp-enable-out", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(addr->amp_enable), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.amp-fault-in", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->amp_fault), mot_comp_id);
    if (retval != 0) {
	return retval;
    }

    /* export axis parameters */
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.coarse-pos-cmd", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->coarse_pos_cmd),
	mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.joint-pos-cmd", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->joint_pos_cmd), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.joint-vel-cmd", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->joint_vel_cmd), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.backlash-corr", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->backlash_corr), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.backlash-filt", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->backlash_filt), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.joint-pos-fb", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->joint_pos_fb), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.f-error", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->f_error), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.f-error-lim", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->f_error_lim), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.free-pos-cmd", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->free_pos_cmd), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.free-vel-lim", num);
    retval =
	hal_param_float_new(buf, HAL_RD, &(addr->free_vel_lim), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.free-tp-enable", num);
    retval =
	hal_param_bit_new(buf, HAL_RD, &(addr->free_tp_enable), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.active", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->active), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.in-position", num);
    retval =
	hal_param_bit_new(buf, HAL_RD, &(addr->in_position), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.error", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->error), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.pos-soft-limit", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->psl), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.neg-soft-limit", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->nsl), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.pos-hard-limit", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->phl), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.neg-hard-limit", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->nhl), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.homing", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->homing), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.homed", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->homed), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.f-errored", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->f_errored), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.faulted", num);
    retval = hal_param_bit_new(buf, HAL_RD, &(addr->faulted), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.home-state", num);
    retval = hal_param_s8_new(buf, HAL_RD, &(addr->home_state), mot_comp_id);
    if (retval != 0) {
	return retval;
    }

/*! \todo FIXME - these have been temporarily? deleted */
/*! \todo Another #if 0 */
#if 0
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.mode", num);
    retval = hal_pin_u32_new(buf, HAL_WR, &(addr->mode), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.model", num);
    retval = hal_pin_u32_new(buf, HAL_RD, &(addr->model), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.reset", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(addr->reset), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.latch", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->latch), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "axis.%d.index", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->index), mot_comp_id);
    if (retval != 0) {
	return retval;
    }
#endif

    /* restore saved message level */
    rtapi_set_msg_level(msg);

    return 0;
}

/* init_comm_buffers() allocates and initializes the command,
   status, and error buffers used to communicate with the user
   space parts of emc.
*/
static int init_comm_buffers(void)
{
    int joint_num;
    emcmot_joint_t *joint;
    int retval;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: init_comm_buffers() starting...\n");

    emcmotStruct = 0;
    emcmotDebug = 0;
    emcmotStatus = 0;
    emcmotCommand = 0;
    emcmotConfig = 0;

    /* record the kinematics type of the machine */
    kinType = kinematicsType();

    /* allocate and initialize the shared memory structure */
    emc_shmem_id = rtapi_shmem_new(key, mot_comp_id, sizeof(emcmot_struct_t));
    if (emc_shmem_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: rtapi_shmem_new failed, returned %d\n", emc_shmem_id);
	return -1;
    }
    retval = rtapi_shmem_getptr(emc_shmem_id, (void **) &emcmotStruct);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: rtapi_shmem_getptr failed, returned %d\n", retval);
	return -1;
    }

    /* zero shared memory before doing anything else. */
    memset(emcmotStruct, 0, sizeof(emcmot_struct_t));

    /* we'll reference emcmotStruct directly */
    emcmotCommand = &emcmotStruct->command;
    emcmotStatus = &emcmotStruct->status;
    emcmotConfig = &emcmotStruct->config;
    emcmotDebug = &emcmotStruct->debug;
    emcmotInternal = &emcmotStruct->internal;
    emcmotError = &emcmotStruct->error;

    /* init error struct */
    emcmotErrorInit(emcmotError);

    /* init command struct */
    emcmotCommand->head = 0;
    emcmotCommand->command = 0;
    emcmotCommand->commandNum = 0;
    emcmotCommand->tail = 0;
    emcmotCommand->spindlesync = 0.0;

    /* init status struct */
    emcmotStatus->head = 0;
    emcmotStatus->commandEcho = 0;
    emcmotStatus->commandNumEcho = 0;
    emcmotStatus->commandStatus = 0;

    /* init more stuff */

    emcmotDebug->head = 0;
    emcmotConfig->head = 0;

    emcmotStatus->motionFlag = 0;
    SET_MOTION_ERROR_FLAG(0);
    SET_MOTION_COORD_FLAG(0);
    SET_MOTION_TELEOP_FLAG(0);
    emcmotDebug->split = 0;
    emcmotStatus->heartbeat = 0;
    emcmotStatus->computeTime = 0.0;
    emcmotConfig->numAxes = EMCMOT_MAX_AXIS;

    emcmotStatus->carte_pos_cmd.tran.x = 0.0;
    emcmotStatus->carte_pos_cmd.tran.y = 0.0;
    emcmotStatus->carte_pos_cmd.tran.z = 0.0;
    emcmotStatus->carte_pos_fb.tran.x = 0.0;
    emcmotStatus->carte_pos_fb.tran.y = 0.0;
    emcmotStatus->carte_pos_fb.tran.z = 0.0;
    emcmotStatus->vel = VELOCITY;
    emcmotConfig->limitVel = VELOCITY;
    emcmotStatus->acc = ACCELERATION;
    emcmotStatus->qVscale = 1.0;
    emcmotStatus->id = 0;
    emcmotStatus->depth = 0;
    emcmotStatus->activeDepth = 0;
    emcmotStatus->paused = 0;
    emcmotStatus->overrideLimits = 0;
    SET_MOTION_INPOS_FLAG(1);
    SET_MOTION_ENABLE_FLAG(0);
    emcmotConfig->kinematics_type = kinType;

    emcmotDebug->oldPos = emcmotStatus->carte_pos_cmd;
    emcmotDebug->oldVel.tran.x = 0.0;
    emcmotDebug->oldVel.tran.y = 0.0;
    emcmotDebug->oldVel.tran.z = 0.0;

    emcmot_config_change();

    /* init pointer to joint structs */
#ifdef STRUCTS_IN_SHMEM
    joints = &(emcmotDebug->joints[0]);
#else
    joints = &(joint_array[0]);
#endif

    /* init per-axis stuff */
    for (joint_num = 0; joint_num < EMCMOT_MAX_AXIS; joint_num++) {
	/* point to structure for this joint */
	joint = &joints[joint_num];

	/* init the config fields with some "reasonable" defaults" */

	joint->type = 0;
	joint->max_pos_limit = 1.0;
	joint->min_pos_limit = -1.0;
	joint->vel_limit = 1.0;
	joint->acc_limit = 1.0;
	joint->min_ferror = 0.01;
	joint->max_ferror = 1.0;
	joint->switch_flags = 0;
	joint->home_search_vel = 0.0;
	joint->home_latch_vel = 0.0;
	joint->home_offset = 0.0;
	joint->home = 0.0;
	joint->home_flags = 0;
	joint->backlash = 0.0;
	/* init compensation struct - leave out the avgint, nominal, forward,
	   and reverse, since these can't be zero and the total flag prevents
	   their use anyway */
	joint->comp.total = 0;
	joint->comp.alter = 0.0;

	/* init status info */
	joint->flag = 0;
	joint->coarse_pos = 0.0;
	joint->pos_cmd = 0.0;
	joint->vel_cmd = 0.0;
	joint->backlash_corr = 0.0;
	joint->backlash_filt = 0.0;
	joint->motor_pos_cmd = 0.0;
	joint->motor_pos_fb = 0.0;
	joint->pos_fb = 0.0;
	joint->ferror = 0.0;
	joint->ferror_limit = joint->min_ferror;
	joint->ferror_high_mark = 0.0;

	/* init internal info */
	cubicInit(&(joint->cubic));

	/* init misc other stuff in joint structure */
	joint->big_vel = 10.0 * joint->vel_limit;
	joint->home_state = 0;

	/* init joint flags (reduntant, since flag = 0 */

	SET_JOINT_ENABLE_FLAG(joint, 0);
	SET_JOINT_ACTIVE_FLAG(joint, 0);
	SET_JOINT_NSL_FLAG(joint, 0);
	SET_JOINT_PSL_FLAG(joint, 0);
	SET_JOINT_NHL_FLAG(joint, 0);
	SET_JOINT_PHL_FLAG(joint, 0);
	SET_JOINT_INPOS_FLAG(joint, 1);
	SET_JOINT_HOMING_FLAG(joint, 0);
	SET_JOINT_HOMED_FLAG(joint, 0);
	SET_JOINT_FERROR_FLAG(joint, 0);
	SET_JOINT_FAULT_FLAG(joint, 0);
	SET_JOINT_ERROR_FLAG(joint, 0);

    }

    /*! \todo FIXME-- add emcmotError */

    /* init min-max-avg stats */
    mmxavgInit(&emcmotDebug->tMmxavg, emcmotDebug->tMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->sMmxavg, emcmotDebug->sMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->nMmxavg, emcmotDebug->nMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->yMmxavg, emcmotDebug->yMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fMmxavg, emcmotDebug->fMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fyMmxavg, emcmotDebug->fyMmxavgSpace,
	MMXAVG_SIZE);
    emcmotDebug->tMin = 0.0;
    emcmotDebug->tMax = 0.0;
    emcmotDebug->tAvg = 0.0;
    emcmotDebug->sMin = 0.0;
    emcmotDebug->sMax = 0.0;
    emcmotDebug->sAvg = 0.0;
    emcmotDebug->nMin = 0.0;
    emcmotDebug->nMax = 0.0;
    emcmotDebug->nAvg = 0.0;
    emcmotDebug->yMin = 0.0;
    emcmotDebug->yMax = 0.0;
    emcmotDebug->yAvg = 0.0;
    emcmotDebug->fyMin = 0.0;
    emcmotDebug->fyMax = 0.0;
    emcmotDebug->fyAvg = 0.0;
    emcmotDebug->fMin = 0.0;
    emcmotDebug->fMax = 0.0;
    emcmotDebug->fAvg = 0.0;

    emcmotDebug->cur_time = emcmotDebug->last_time = 0.0;
    emcmotDebug->start_time = etime();
    emcmotDebug->running_time = 0.0;

    /* init motion emcmotDebug->queue */
    if (-1 == tpCreate(&emcmotDebug->queue, DEFAULT_TC_QUEUE_SIZE,
	    emcmotDebug->queueTcSpace)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to create motion emcmotDebug->queue\n");
	return -1;
    }
//    tpInit(&emcmotDebug->queue); // tpInit called from tpCreate
    tpSetCycleTime(&emcmotDebug->queue, emcmotConfig->trajCycleTime);
    tpSetPos(&emcmotDebug->queue, emcmotStatus->carte_pos_cmd);
    tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel, emcmotStatus->vel);
    tpSetAmax(&emcmotDebug->queue, emcmotStatus->acc);

    emcmotStatus->tail = 0;

    rtapi_print_msg(RTAPI_MSG_INFO, "MOTION: init_comm_buffers() complete\n");
    return 0;
}

/* init_threads() creates realtime threads, exports functions to
   do the realtime control, and adds the functions to the threads.
*/
static int init_threads(void)
{
    double base_period_sec, servo_period_sec;
    int servo_base_ratio;
    int retval;

    rtapi_print_msg(RTAPI_MSG_INFO, "MOTION: init_threads() starting...\n");

    /* if base_period not specified, assume same as servo_period */
    if (base_period_nsec == 0) {
	base_period_nsec = servo_period_nsec;
    }
    /* servo period must be greater or equal to base period */
    if (servo_period_nsec < base_period_nsec) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: bad servo period %d nsec\n", servo_period_nsec);
	return -1;
    }
    /* convert desired periods to floating point */
    base_period_sec = base_period_nsec * 0.000000001;
    servo_period_sec = servo_period_nsec * 0.000000001;
    /* calculate period ratios, round to nearest integer */
    servo_base_ratio = (servo_period_sec / base_period_sec) + 0.5;
    /* revise desired periods to be integer multiples of each other */
    servo_period_nsec = base_period_nsec * servo_base_ratio;
    /* create HAL threads for each period */
    /* only create base thread if it is faster than servo thread */
    if (servo_base_ratio > 1) {
	retval = hal_create_thread("base-thread", base_period_nsec, 0);
	if (retval != HAL_SUCCESS) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"MOTION: failed to create %d nsec base thread\n",
		base_period_nsec);
	    return -1;
	}
    }
    retval = hal_create_thread("servo-thread", servo_period_nsec, 1);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to create %d nsec servo thread\n",
	    servo_period_nsec);
	return -1;
    }
    /* export realtime functions that do the real work */
    retval = hal_export_funct("motion-controller", emcmotController, 0	/* arg 
	 */ , 1 /* uses_fp */ , 0 /* reentrant */ , mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to export controller function\n");
	return -1;
    }
    retval = hal_export_funct("motion-command-handler", emcmotCommandHandler, 0	/* arg 
	 */ , 1 /* uses_fp */ , 0 /* reentrant */ , mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to export command handler function\n");
	return -1;
    }
/*! \todo Another #if 0 */
#if 0
    /*! \todo FIXME - currently the traj planner is called from the controller */
    /* eventually it will be a separate function */
    retval = hal_export_funct("motion-traj-planner", emcmotTrajPlanner, 0	/* arg 
	 */ , 1 /* uses_fp */ ,
	0 /* reentrant */ , mot_comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "MOTION: failed to export traj planner function\n");
	return -1;
    }
#endif

    /* init the time and rate using functions to affect traj, and the cubics
       properly, since they're coupled */

    retval = setTrajCycleTime(traj_period_nsec * 0.000000001);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: setTrajCycleTime() failed\n");
	return -1;
    }

    retval = setServoCycleTime(servo_period_nsec * 0.000000001);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTION: setServoCycleTime() failed\n");
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO, "MOTION: init_threads() complete\n");
    return 0;
}

/* call this when setting the trajectory cycle time */
static int setTrajCycleTime(double secs)
{
    static int t;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: setting Traj cycle time to %d nsecs\n", (long) (secs * 1e9));

    /* make sure it's not zero */
    if (secs <= 0.0) {

	return -1;
    }

    emcmot_config_change();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (secs / emcmotConfig->servoCycleTime + 0.5);

    /* set traj planner */
    tpSetCycleTime(&emcmotDebug->queue, secs);

    /* set the free planners, cubic interpolation rate and segment time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	cubicSetInterpolationRate(&(joints[t].cubic),
	    emcmotConfig->interpolationRate);
    }

    /* copy into status out */
    emcmotConfig->trajCycleTime = secs;

    return 0;
}

/* call this when setting the servo cycle time */
static int setServoCycleTime(double secs)
{
    static int t;

    rtapi_print_msg(RTAPI_MSG_INFO,
	"MOTION: setting Servo cycle time to %d nsecs\n", (long) (secs * 1e9));

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return -1;
    }

    emcmot_config_change();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (emcmotConfig->trajCycleTime / secs + 0.5);

    /* set the cubic interpolation rate and PID cycle time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	cubicSetInterpolationRate(&(joints[t].cubic),
	    emcmotConfig->interpolationRate);
	cubicSetSegmentTime(&(joints[t].cubic), secs);
    }

    /* copy into status out */
    emcmotConfig->servoCycleTime = secs;

    return 0;
}
