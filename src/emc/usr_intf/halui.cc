/********************************************************************
* Description: halui.cc
*   HAL User-Interface component.
*   This file exports various UI related hal pins, and communicates 
*   with EMC through NML messages
*
*   Derived from a work by Fred Proctor & Will Shackleford (emcsh.cc)
*   some of the functions (sendFooBar() are adapted from there)
*
* Author: Alex Joni
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2006 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>

#include "hal.h"		/* access to HAL functions/definitions */
#include "rtapi.h"		/* rtapi_print_msg */
#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "emcmotcfg.h"          // EMCMOT_MAX_AXIS
#include "rcs_print.hh"
#include "nml_oi.hh"
#include "timer.hh"

/*
  Using halui:

  halui {-ini <ini file>}

  With -ini <inifile>, uses inifile instead of emc.ini. 

  Once executed, it connects to NML buffers, exports various HAL pins
  and communicates changes to EMC. It also sets certain HAL pins based 
  on status values.

  
  Naming:
  
  All pins will be named after the following scheme:
  
  halui.name.<number>.action
  
  name    refers to the name of the component,
             currently one of:
	     - machine
	     - estop
	     - mode
	     - mist
	     - flood
	     - lube
	     - jog
	     - program
	     - probe
	     ...

  <number>   if more than one component of the same type exists
	     
  action     usually on/off or is-on for the status (this uses the NIST way of
	     control, each action is done by momentary pushbuttons, and thus
	     more than one source of control is allowed: e.g. multiple UI's, 
	     GUI's )

  Exported pins:

DONE: - machine:
   halui.machine.on                    bit  //pin for setting machine On
   halui.machine.off                   bit  //pin for setting machine Off
   halui.machine.is-on                 bit  //pin for machine is On/Off

DONE: - estop:   
   halui.estop.activate                bit  //pin for setting Estop (emc internal) On
   halui.estop.reset                   bit  //pin for resetting Estop (emc internal) Off
   halui.estop.is-activated            bit  //pin for displaying Estop state (emc internal) On/Off
   
DONE: - mode:
   halui.mode.manual                   bit  //pin for requesting manual mode
   halui.mode.is_manual                bit  //pin for manual mode is on
   halui.mode.auto                     bit  //pin for requesting auto mode
   halui.mode.is_auto                  bit  //pin for auto mode is on
   halui.mode.mdi                      bit  //pin for requesting mdi mode
   halui.mode.is_mdi                   bit  //pin for mdi mode is on

DONE: - mist, flood, lube:
   halui.mist.on                       bit  //pin for starting mist
   halui.mist.off                      bit  //pin for stoping mist
   halui.mist.is-on                    bit  //pin for mist is on
   halui.flood.on                      bit  //pin for starting flood
   halui.flood.off                     bit  //pin for stoping flood
   halui.flood.is-on                   bit  //pin for flood is on
   halui.lube.on                       bit  //pin for starting lube
   halui.lube.off                      bit  //pin for stoping lube
   halui.lube.is-on                    bit  //pin for lube is on

DONE: - spindle:
   halui.spindle.start                 bit
   halui.spindle.stop                  bit
   halui.spindle.forward               bit
   halui.spindle.reverse               bit
   halui.spindle.increase              bit
   halui.spindle.decrease              bit

   halui.spindle.brake-on              bit  //pin for activating spindle-brake
   halui.spindle.brake-off             bit  //pin for deactivating spindle/brake
   halui.spindle.brake-is-on           bit  //status pin that tells us if brake is on

DONE: - joint:
   halui.joint.0.home                  bit  // pin for homing the specific joint
   halui.joint.0.is-homed              bit  // status pin telling that the joint is homed
   ..
   halui.joint.7.home                  bit 
   halui.joint.7.is-homed              bit 

   halui.joint.selected.home           bit  // pin for homing the selected joint
   halui.joint.selected.is-homed       bit  // status pin telling that the selected joint is homed

   halui.joint.x.on-soft-min-limit     bit
   halui.joint.x.on-soft-max-limit     bit
   halui.joint.x.on-hard-min-limit     bit
   halui.joint.x.on-hard-max-limit     bit
     (x = 0..7, selected)
   
   halui.joint.x.has-fault             bit   
     (x = 0..7, selected)

   halui.joint.select                  u8   // select joint (0..7)           - internal halui
   halui.joint.selected                u8   // selected joint (0..7)         - internal halui
   halui.joint.x.select                bit  // pins for selecting a joint    - internal halui
   halui.joint.x.is-selected           bit  // status pin                    - internal halui

DONE - jogging:
   halui.jog.speed                     float //set jog speed
   halui.jog-deadband                  float //pin for setting the jog analog deadband (where not to move)

   halui.jog.0.minus                   bit
   halui.jog.0.plus                    bit
   halui.jog.7.analog                  float //pin for jogging the axis 0
   ..
   halui.jog.7.minus                   bit
   halui.jog.7.plus                    bit
   halui.jog.7.analog                  float //pin for jogging the axis 7
   halui.jog.selected.minus            bit
   halui.jog.selected.plus             bit

DONE - tool:
   halui.tool.number                   u32  //current selected tool
   halui.tool.length-offset            float //current applied tool-length-offset

DONE: - program:
   halui.program.is-idle               bit
   halui.program.is-running            bit
   halui.program.is-paused             bit
   halui.program.run                   bit
   halui.program.pause                 bit
   halui.program.resume                bit
   halui.program.step                  bit

DONE: - general:
   halui.abort                         bit // pin to send an abort message (clears out most errors)

DONE: - feed-override
   halui.feed-override.value           float //current FO value
   halui.feed-override.scale           float // pin for setting the scale on changing the FO
   halui.feed-override.counts          s32   //counts from an encoder for example to change FO
   halui.feed-override.increase        bit   // pin for increasing the FO (+=scale)
   halui.feed-override.decrease        bit   // pin for decreasing the FO (-=scale)

DONE: - spindle-override
   halui.spindle-override.value           float //current FO value
   halui.spindle-override.scale           float // pin for setting the scale on changing the SO
   halui.spindle-override.counts          s43   //counts from an encoder for example to change SO
   halui.spindle-override.increase        bit   // pin for increasing the SO (+=scale)
   halui.spindle-override.decrease        bit   // pin for decreasing the SO (-=scale)

*/


struct halui_str {
    hal_bit_t *machine_on;         //pin for setting machine On
    hal_bit_t *machine_off;        //pin for setting machine Off
    hal_bit_t *machine_is_on;      //pin for machine is On/Off
    
                                   // (check iocontrol.cc for a proper description)
    hal_bit_t *estop_activate;     //pin for activating EMC ESTOP 
    hal_bit_t *estop_reset;        //pin for resetting ESTOP
    hal_bit_t *estop_is_activated; //pin for status ESTOP is activated

    hal_bit_t *mode_manual;        //pin for requesting manual mode
    hal_bit_t *mode_is_manual;     //pin for manual mode is on
    hal_bit_t *mode_auto;          //pin for requesting auto mode
    hal_bit_t *mode_is_auto;       //pin for auto mode is on
    hal_bit_t *mode_mdi;           //pin for requesting mdi mode
    hal_bit_t *mode_is_mdi;        //pin for mdi mode is on

    hal_bit_t *mist_on;            //pin for starting mist
    hal_bit_t *mist_off;           //pin for stoping mist
    hal_bit_t *mist_is_on;         //pin for mist is on
    hal_bit_t *flood_on;           //pin for starting flood
    hal_bit_t *flood_off;          //pin for stoping flood
    hal_bit_t *flood_is_on;        //pin for flood is on
    hal_bit_t *lube_on;            //pin for starting lube
    hal_bit_t *lube_off;           //pin for stoping lube
    hal_bit_t *lube_is_on;         //pin for lube is on

    hal_bit_t *program_is_idle;    //pin for notifying user that program is idle
    hal_bit_t *program_is_running; //pin for notifying user that program is running
    hal_bit_t *program_is_paused;  //pin for notifying user that program is paused
    hal_bit_t *program_run;        //pin for running program
    hal_bit_t *program_pause;      //pin for pausing program
    hal_bit_t *program_resume;     //pin for resuming program
    hal_bit_t *program_step;       //pin for running one line of the program
    hal_bit_t *program_os_on;      //pin for setting optional stop on
    hal_bit_t *program_os_off;     //pin for setting optional stop off
    hal_bit_t *program_os_is_on;   //status pin that optional stop is on
    hal_bit_t *program_bd_on;      //pin for setting block delete on
    hal_bit_t *program_bd_off;     //pin for setting block delete off
    hal_bit_t *program_bd_is_on;   //status pin that block delete is on

    hal_u32_t *tool_number;		//pin for current selected tool
    hal_float_t *tool_length_offset;	//current applied tool-length-offset

    hal_bit_t *spindle_start;		//pin for starting the spindle
    hal_bit_t *spindle_stop;		//pin for stoping the spindle
    hal_bit_t *spindle_is_on;		//status pin for spindle is on
    hal_bit_t *spindle_forward;		//pin for making the spindle go forward
    hal_bit_t *spindle_runs_forward;	//status pin for spindle running forward
    hal_bit_t *spindle_reverse;		//pin for making the spindle go reverse
    hal_bit_t *spindle_runs_backward;	//status pin for spindle running backward
    hal_bit_t *spindle_increase;	//pin for making the spindle go faster
    hal_bit_t *spindle_decrease;	//pin for making the spindle go slower

    hal_bit_t *spindle_brake_on;   //pin for activating spindle-brake
    hal_bit_t *spindle_brake_off;  //pin for deactivating spindle/brake
    hal_bit_t *spindle_brake_is_on;//status pin that tells us if brake is on

    hal_bit_t *joint_home[EMCMOT_MAX_AXIS+1];   //pin for homing one joint
    hal_bit_t *joint_is_homed[EMCMOT_MAX_AXIS+1];   //status pin that the joint is homed
    hal_bit_t *joint_on_soft_min_limit[EMCMOT_MAX_AXIS+1];   //status pin that the joint is on the software min limit
    hal_bit_t *joint_on_soft_max_limit[EMCMOT_MAX_AXIS+1];   //status pin that the joint is on the software max limit
    hal_bit_t *joint_on_hard_min_limit[EMCMOT_MAX_AXIS+1];   //status pin that the joint is on the hardware min limit
    hal_bit_t *joint_on_hard_max_limit[EMCMOT_MAX_AXIS+1];   //status pin that the joint is on the hardware max limit
    hal_bit_t *joint_has_fault[EMCMOT_MAX_AXIS+1];   //status pin that the joint has a fault
    hal_u32_t *joint_selected;                               // status pin for the joint selected
    hal_bit_t *joint_nr_select[EMCMOT_MAX_AXIS];             // nr. of pins to select a joint
    hal_bit_t *joint_is_selected[EMCMOT_MAX_AXIS];           // nr. of status pins for joint selected

    hal_float_t *jog_speed;	//pin for setting the jog speed (halui internal)
    hal_bit_t *jog_minus[EMCMOT_MAX_AXIS+1];	//pin to jog in positive direction
    hal_bit_t *jog_plus[EMCMOT_MAX_AXIS+1];	//pin to jog in negative direction
    hal_float_t *jog_analog[EMCMOT_MAX_AXIS+1];	//pin for analog jogging (-1..0..1)
    hal_float_t *jog_deadband;	//pin for setting the jog analog deadband (where not to move)

    hal_s32_t *fo_counts;	//pin for the Feed Override counting
    hal_float_t *fo_scale;	//scale for the Feed Override counting
    hal_float_t *fo_value;	//current Feed Override value
    hal_bit_t  *fo_increase;	// pin for increasing the FO (+=scale)
    hal_bit_t  *fo_decrease;	// pin for decreasing the FO (-=scale)

    hal_s32_t *so_counts;	//pin for the Spindle Speed Override counting
    hal_float_t *so_scale;	//scale for the Spindle Speed Override counting
    hal_float_t *so_value;	//current Spindle speed Override value
    hal_bit_t  *so_increase;	// pin for increasing the SO (+=scale)
    hal_bit_t  *so_decrease;	// pin for decreasing the SO (-=scale)

    hal_bit_t *abort;            //pin for aborting
} * halui_data; 

struct local_halui_str {
    hal_bit_t machine_on;         //pin for setting machine On
    hal_bit_t machine_off;        //pin for setting machine Off

    hal_bit_t estop_activate;     //pin for activating EMC ESTOP 
    hal_bit_t estop_reset;        //pin for resetting ESTOP

    hal_bit_t mode_manual;        //pin for requesting manual mode
    hal_bit_t mode_auto;          //pin for requesting auto mode
    hal_bit_t mode_mdi;           //pin for requesting mdi mode

    hal_bit_t mist_on;            //pin for starting mist
    hal_bit_t mist_off;           //pin for stoping mist
    hal_bit_t flood_on;           //pin for starting flood
    hal_bit_t flood_off;          //pin for stoping flood
    hal_bit_t lube_on;            //pin for starting lube
    hal_bit_t lube_off;           //pin for stoping lube

    hal_bit_t program_run;        //pin for running program
    hal_bit_t program_pause;      //pin for pausing program
    hal_bit_t program_resume;     //pin for resuming program
    hal_bit_t program_step;       //pin for running one line of the program
    hal_bit_t program_os_on;      //pin for setting optional stop on
    hal_bit_t program_os_off;     //pin for setting optional stop off
    hal_bit_t program_bd_on;      //pin for setting block delete on
    hal_bit_t program_bd_off;     //pin for setting block delete off

    hal_bit_t spindle_start;      //pin for starting the spindle
    hal_bit_t spindle_stop;       //pin for stoping the spindle
    hal_bit_t spindle_forward;    //pin for making the spindle go forward
    hal_bit_t spindle_reverse;    //pin for making the spindle go reverse
    hal_bit_t spindle_increase;   //pin for making the spindle go faster
    hal_bit_t spindle_decrease;   //pin for making the spindle go slower

    hal_bit_t spindle_brake_on;   //pin for activating spindle-brake
    hal_bit_t spindle_brake_off;  //pin for deactivating spindle/brake

    hal_bit_t joint_home[EMCMOT_MAX_AXIS+1];   //pin for homing one joint
    hal_u32_t joint_selected;
    hal_bit_t joint_nr_select[EMCMOT_MAX_AXIS];
    hal_bit_t joint_is_selected[EMCMOT_MAX_AXIS];

    hal_bit_t jog_minus[EMCMOT_MAX_AXIS+1];	//pin to jog in positive direction
    hal_bit_t jog_plus[EMCMOT_MAX_AXIS+1];	//pin to jog in negative direction
    hal_float_t jog_analog[EMCMOT_MAX_AXIS+1];	//pin for analog jogging (-1..0..1)
        
    hal_s32_t fo_counts;	//pin for the Feed Override counting
    hal_float_t fo_scale;	//scale for the Feed Override counting
    hal_bit_t  fo_increase;	// pin for increasing the FO (+=scale)
    hal_bit_t  fo_decrease;	// pin for decreasing the FO (-=scale)

    hal_s32_t so_counts;	//pin for the Spindle Speed Override counting
    hal_float_t so_scale;	//scale for the Spindle Speed Override counting
    hal_bit_t  so_increase;	// pin for increasing the SO (+=scale)
    hal_bit_t  so_decrease;	// pin for decreasing the SO (-=scale)

    hal_bit_t abort;            //pin for aborting
} old_halui_data; //pointer to the HAL-struct

static int comp_id, done;				/* component ID, main while loop */

static int num_axes = 3; //number of axes, taken from the ini [TRAJ] section

static double maxFeedOverride=1;
static double minSpindleOverride=1.0;// no variation allowed by default (old behaviour)
static double maxSpindleOverride=1.0;// the real values come from the ini

// the NML channels to the EMC task
static RCS_CMD_CHANNEL *emcCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcStatusBuffer = 0;
EMC_STAT *emcStatus = 0;

// the NML channel for errors
static NML *emcErrorBuffer = 0;

// the current command numbers, set up updateStatus(), used in main()
static int emcCommandSerialNumber = 0;
static int saveEmcCommandSerialNumber = 0;

// default value for timeout, 0 means wait forever
// use same timeout value as in tkemc & mini
static double emcTimeout = 1.0;

static enum {
    EMC_WAIT_NONE = 1,
    EMC_WAIT_RECEIVED,
    EMC_WAIT_DONE
} emcWaitType = EMC_WAIT_RECEIVED; 
//even if a command overlaps with another GUI 
// (probable on heavy use of both GUIS), we allow the waiting to timeout

static void quit(int sig)
{
    done = 1;
}

static int emcTaskNmlGet()
{
    int retval = 0;

    // try to connect to EMC cmd
    if (emcCommandBuffer == 0) {
	emcCommandBuffer =
	    new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc",
				EMC_NMLFILE);
	if (!emcCommandBuffer->valid()) {
	    delete emcCommandBuffer;
	    emcCommandBuffer = 0;
	    retval = -1;
	}
    }
    // try to connect to EMC status
    if (emcStatusBuffer == 0) {
	emcStatusBuffer =
	    new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc",
				 EMC_NMLFILE);
	if (!emcStatusBuffer->valid()) {
	    delete emcStatusBuffer;
	    emcStatusBuffer = 0;
	    emcStatus = 0;
	    retval = -1;
	} else {
	    emcStatus = (EMC_STAT *) emcStatusBuffer->get_address();
	}
    }

    return retval;
}

static int emcErrorNmlGet()
{
    int retval = 0;

    if (emcErrorBuffer == 0) {
	emcErrorBuffer =
	    new NML(nmlErrorFormat, "emcError", "xemc", EMC_NMLFILE);
	if (!emcErrorBuffer->valid()) {
	    delete emcErrorBuffer;
	    emcErrorBuffer = 0;
	    retval = -1;
	}
    }

    return retval;
}

static int tryNml()
{
    double end;
    int good;
#define RETRY_TIME 10.0		// seconds to wait for subsystems to come up
#define RETRY_INTERVAL 1.0	// seconds between wait tries for a subsystem

    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_NULL);	// inhibit diag
	// messages
    }
    end = RETRY_TIME;
    good = 0;
    do {
	if (0 == emcTaskNmlGet()) {
	    good = 1;
	    break;
	}
	esleep(RETRY_INTERVAL);
	end -= RETRY_INTERVAL;
    } while (end > 0.0);
    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// inhibit diag
	// messages
    }
    if (!good) {
	return -1;
    }

    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_NULL);	// inhibit diag
	// messages
    }
    end = RETRY_TIME;
    good = 0;
    do {
	if (0 == emcErrorNmlGet()) {
	    good = 1;
	    break;
	}
	esleep(RETRY_INTERVAL);
	end -= RETRY_INTERVAL;
    } while (end > 0.0);
    if (EMC_DEBUG & EMC_DEBUG_NML == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// inhibit diag
	// messages
    }
    if (!good) {
	return -1;
    }

    return 0;

#undef RETRY_TIME
#undef RETRY_INTERVAL
}

static int updateStatus()
{
    NMLTYPE type;

    if (0 == emcStatus || 0 == emcStatusBuffer
	|| !emcStatusBuffer->valid()) {
	return -1;
    }

    switch (type = emcStatusBuffer->peek()) {
    case -1:
	// error on CMS channel
	return -1;
	break;

    case 0:			// no new data
    case EMC_STAT_TYPE:	// new data
	break;

    default:
	return -1;
	break;
    }

    return 0;
}


#define EMC_COMMAND_DELAY   0.1	// how long to sleep between checks

/*
  emcCommandWaitReceived() waits until the EMC reports that it got
  the command with the indicated serial_number.
  emcCommandWaitDone() waits until the EMC reports that it got the
  command with the indicated serial_number, and it's done, or error.
*/

static int emcCommandWaitReceived(int serial_number)
{
    double end = 0.0;

    while (emcTimeout <= 0.0 || end < emcTimeout) {
	updateStatus();

	if (emcStatus->echo_serial_number == serial_number) {
	    return 0;
	}

	esleep(EMC_COMMAND_DELAY);
	end += EMC_COMMAND_DELAY;
    }

    return -1;
}

static int emcCommandWaitDone(int serial_number)
{
    double end = 0.0;

    // first get it there
    if (0 != emcCommandWaitReceived(serial_number)) {
	return -1;
    }
    // now wait until it, or subsequent command (e.g., abort) is done
    while (emcTimeout <= 0.0 || end < emcTimeout) {
	updateStatus();

	if (emcStatus->status == RCS_DONE) {
	    return 0;
	}

	if (emcStatus->status == RCS_ERROR) {
	    return -1;
	}

	esleep(EMC_COMMAND_DELAY);
	end += EMC_COMMAND_DELAY;
    }

    return -1;
}

static void thisQuit()
{
    //don't forget the big HAL sin ;)
    hal_exit(comp_id);
    
    exit(0);
}

static enum {
    LINEAR_UNITS_CUSTOM = 1,
    LINEAR_UNITS_AUTO,
    LINEAR_UNITS_MM,
    LINEAR_UNITS_INCH,
    LINEAR_UNITS_CM
} linearUnitConversion = LINEAR_UNITS_AUTO;

static enum {
    ANGULAR_UNITS_CUSTOM = 1,
    ANGULAR_UNITS_AUTO,
    ANGULAR_UNITS_DEG,
    ANGULAR_UNITS_RAD,
    ANGULAR_UNITS_GRAD
} angularUnitConversion = ANGULAR_UNITS_AUTO;

#define CLOSE(a,b,eps) ((a)-(b) < +(eps) && (a)-(b) > -(eps))
#define LINEAR_CLOSENESS 0.0001
#define ANGULAR_CLOSENESS 0.0001
#define INCH_PER_MM (1.0/25.4)
#define CM_PER_MM 0.1
#define GRAD_PER_DEG (100.0/90.0)
#define RAD_PER_DEG TO_RAD	// from posemath.h

int halui_export_pin_IN_bit(hal_bit_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_bit_new(name, HAL_IN, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}

int halui_export_pin_IN_s32(hal_s32_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_s32_new(name, HAL_IN, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}

int halui_export_pin_IN_float(hal_float_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_float_new(name, HAL_IN, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}


int halui_export_pin_OUT_bit(hal_bit_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_bit_new(name, HAL_OUT, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}


/********************************************************************
*
* Description: halui_hal_init(void)
*
* Side Effects: Exports HAL pins.
*
* Called By: main
********************************************************************/
int halui_hal_init(void)
{
    int retval;
    int joint;

    /* STEP 1: initialise the hal component */
    comp_id = hal_init("halui");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HALUI: ERROR: hal_init() failed\n");
	return -1;
    }

    /* STEP 2: allocate shared memory for halui data */
    halui_data = (halui_str *) hal_malloc(sizeof(halui_str));
    if (halui_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HALUI: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 3a: export the out-pin(s) */

    retval = halui_export_pin_OUT_bit(&(halui_data->machine_is_on), "halui.machine.is-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->estop_is_activated), "halui.estop.is-activated"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_manual), "halui.mode.is-manual"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_auto), "halui.mode.is-auto"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_mdi), "halui.mode.is-mdi"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mist_is_on), "halui.mist.is-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->flood_is_on), "halui.flood.is-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->lube_is_on), "halui.lube.is-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_is_idle), "halui.program.is-idle"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_is_running), "halui.program.is-running"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_is_paused), "halui.program.is-paused"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_os_is_on), "halui.program.optional-stop.is-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_bd_is_on), "halui.program.block-delete.is-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_is_on), "halui.spindle.is-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_runs_forward), "halui.spindle.runs-forward"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_runs_backward), "halui.spindle.runs-backward"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_brake_is_on), "halui.spindle.brake-is-on"); 
    if (retval != HAL_SUCCESS) return retval;

    for (joint=0; joint < num_axes ; joint++) {
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_is_homed[joint]), comp_id, "halui.joint.%d.is-homed", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_is_selected[joint]), comp_id, "halui.joint.%d.is-selected", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_min_limit[joint]), comp_id, "halui.joint.%d.on-soft-min-limit", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_max_limit[joint]), comp_id, "halui.joint.%d.on-soft-max-limit", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_min_limit[joint]), comp_id, "halui.joint.%d.on-hard-min-limit", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_max_limit[joint]), comp_id, "halui.joint.%d.on-hard-max-limit", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_has_fault[joint]), comp_id, "halui.joint.%d.has-fault", joint); 
	if (retval != HAL_SUCCESS) return retval;
    }

    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_min_limit[num_axes]), comp_id, "halui.joint.selected.on-soft-min-limit"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_max_limit[num_axes]), comp_id, "halui.joint.selected.on-soft-limit"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_min_limit[num_axes]), comp_id, "halui.joint.selected.on-hard-min-limit"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_max_limit[num_axes]), comp_id, "halui.joint.selected.on-hard-max-limit"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_has_fault[num_axes]), comp_id, "halui.joint.selected.has-fault"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_is_homed[num_axes]), comp_id, "halui.joint.selected.is_homed"); 
    if (retval != HAL_SUCCESS) return retval;

    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->fo_value), comp_id, "halui.feed-override.value"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = hal_pin_u32_newf(HAL_OUT, &(halui_data->joint_selected), comp_id, "halui.joint.selected"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = hal_pin_u32_newf(HAL_OUT, &(halui_data->tool_number), comp_id, "halui.tool.number"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset), comp_id, "halui.tool.length_offset"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->so_value), comp_id, "halui.spindle-override.value"); 
    if (retval != HAL_SUCCESS) return retval;

    /* STEP 3b: export the in-pin(s) */

    retval = halui_export_pin_IN_bit(&(halui_data->machine_on), "halui.machine.on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->machine_off), "halui.machine.off"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->estop_activate), "halui.estop.activate"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->estop_reset), "halui.estop.reset"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_manual), "halui.mode.manual"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_auto), "halui.mode.auto"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_mdi), "halui.mode.mdi"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mist_on), "halui.mist.on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mist_off), "halui.mist.off"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->flood_on), "halui.flood.on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->flood_off), "halui.flood.off"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->lube_on), "halui.lube.on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->lube_off), "halui.lube.off"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_run), "halui.program.run"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_pause), "halui.program.pause"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_resume), "halui.program.resume"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_step), "halui.program.step"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_os_on), "halui.program.optional-stop.on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_os_off), "halui.program.optional-stop.off"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_bd_on), "halui.program.block-delete.on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_bd_off), "halui.program.block-delete.off"); 
    if (retval != HAL_SUCCESS) return retval;

    retval = halui_export_pin_IN_bit(&(halui_data->spindle_start), "halui.spindle.start");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_stop), "halui.spindle.stop");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_forward), "halui.spindle.forward");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_reverse), "halui.spindle.reverse");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_increase), "halui.spindle.increase");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_decrease), "halui.spindle.decrease");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_brake_on), "halui.spindle.brake-on"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_brake_off), "halui.spindle.brake-off"); 
    if (retval != HAL_SUCCESS) return retval;

    retval = halui_export_pin_IN_s32(&(halui_data->fo_counts), "halui.feed-override.counts");
    if (retval != HAL_SUCCESS) return retval;
    *halui_data->fo_counts = 0;
    retval = halui_export_pin_IN_float(&(halui_data->fo_scale), "halui.feed-override.scale");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->fo_increase), "halui.feed-override.increase");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->fo_decrease), "halui.feed-override.decrease");
    if (retval != HAL_SUCCESS) return retval;

    retval = halui_export_pin_IN_s32(&(halui_data->so_counts), "halui.spindle-override.counts");
    if (retval != HAL_SUCCESS) return retval;
    *halui_data->so_counts = 0;
    retval = halui_export_pin_IN_float(&(halui_data->so_scale), "halui.spindle-override.scale");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->so_increase), "halui.spindle-override.increase");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->so_decrease), "halui.spindle-override.decrease");
    if (retval != HAL_SUCCESS) return retval;

    retval = halui_export_pin_IN_bit(&(halui_data->abort), "halui.abort"); 
    if (retval != HAL_SUCCESS) return retval;

    for (joint=0; joint < num_axes ; joint++) {
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_home[joint]), comp_id, "halui.joint.%d.home", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_nr_select[joint]), comp_id, "halui.joint.%d.select", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_plus[joint]), comp_id, "halui.jog.%d.plus", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_minus[joint]), comp_id, "halui.jog.%d.minus", joint); 
	if (retval != HAL_SUCCESS) return retval;
	retval =  hal_pin_float_newf(HAL_IN, &(halui_data->jog_analog[joint]), comp_id, "halui.jog.%d.analog", joint); 
	if (retval != HAL_SUCCESS) return retval;
    }

    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_home[num_axes]), comp_id, "halui.joint.selected.home"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_plus[num_axes]), comp_id, "halui.jog.selected.plus"); 
    if (retval != HAL_SUCCESS) return retval;
    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_minus[num_axes]), comp_id, "halui.jog.selected.minus"); 
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_float(&(halui_data->jog_speed), "halui.jog-speed");
    if (retval != HAL_SUCCESS) return retval;
    retval = halui_export_pin_IN_float(&(halui_data->jog_deadband), "halui.jog-deadband");
    if (retval != HAL_SUCCESS) return retval;

    hal_ready(comp_id);
    return 0;
}

static int sendMachineOn()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ON;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendMachineOff()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_OFF;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendEstop()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ESTOP;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendEstopReset()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ESTOP_RESET;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendManual()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE_MANUAL;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAuto()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE_AUTO;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendMdi()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE_MDI;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendMistOn()
{
    EMC_COOLANT_MIST_ON emc_coolant_mist_on_msg;

    emc_coolant_mist_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_mist_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendMistOff()
{
    EMC_COOLANT_MIST_OFF emc_coolant_mist_off_msg;

    emc_coolant_mist_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_mist_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendFloodOn()
{
    EMC_COOLANT_FLOOD_ON emc_coolant_flood_on_msg;

    emc_coolant_flood_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_flood_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendFloodOff()
{
    EMC_COOLANT_FLOOD_OFF emc_coolant_flood_off_msg;

    emc_coolant_flood_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_flood_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendLubeOn()
{
    EMC_LUBE_ON emc_lube_on_msg;

    emc_lube_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_lube_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendLubeOff()
{
    EMC_LUBE_OFF emc_lube_off_msg;

    emc_lube_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_lube_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

// programStartLine is the saved valued of the line that
// sendProgramRun(int line) sent
static int programStartLine = 0;

static int sendProgramRun(int line)
{
    EMC_TASK_PLAN_RUN emc_task_plan_run_msg;

    updateStatus();

    if (0 == emcStatus->task.file[0]) {
	return -1; // no program open
    }
    // save the start line, to compare against active line later
    programStartLine = line;

    emc_task_plan_run_msg.serial_number = ++emcCommandSerialNumber;
    emc_task_plan_run_msg.line = line;
    emcCommandBuffer->write(emc_task_plan_run_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendProgramPause()
{
    EMC_TASK_PLAN_PAUSE emc_task_plan_pause_msg;

    emc_task_plan_pause_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_pause_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSetOptionalStop(bool state)
{
    EMC_TASK_PLAN_SET_OPTIONAL_STOP emc_task_plan_set_optional_stop_msg;

    emc_task_plan_set_optional_stop_msg.state = state;
    emc_task_plan_set_optional_stop_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_set_optional_stop_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSetBlockDelete(bool state)
{
    EMC_TASK_PLAN_SET_BLOCK_DELETE emc_task_plan_set_block_delete_msg;

    emc_task_plan_set_block_delete_msg.state = state;
    emc_task_plan_set_block_delete_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_set_block_delete_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}


static int sendProgramResume()
{
    EMC_TASK_PLAN_RESUME emc_task_plan_resume_msg;

    emc_task_plan_resume_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_resume_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendProgramStep()
{
    EMC_TASK_PLAN_STEP emc_task_plan_step_msg;

    emc_task_plan_step_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_step_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSpindleForward()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed = fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = +500;
    }
    emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSpindleReverse()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed =
	    -1 * fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = -500;
    }
    emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_on_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSpindleOff()
{
    EMC_SPINDLE_OFF emc_spindle_off_msg;

    emc_spindle_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_off_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSpindleIncrease()
{
    EMC_SPINDLE_INCREASE emc_spindle_increase_msg;

    emc_spindle_increase_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_increase_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSpindleDecrease()
{
    EMC_SPINDLE_DECREASE emc_spindle_decrease_msg;

    emc_spindle_decrease_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_decrease_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSpindleConstant()
{
    EMC_SPINDLE_CONSTANT emc_spindle_constant_msg;

    emc_spindle_constant_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_constant_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendBrakeEngage()
{
    EMC_SPINDLE_BRAKE_ENGAGE emc_spindle_brake_engage_msg;

    emc_spindle_brake_engage_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_brake_engage_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendBrakeRelease()
{
    EMC_SPINDLE_BRAKE_RELEASE emc_spindle_brake_release_msg;

    emc_spindle_brake_release_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_brake_release_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendHome(int axis)
{
    EMC_AXIS_HOME emc_axis_home_msg;

    emc_axis_home_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_home_msg.axis = axis;
    emcCommandBuffer->write(emc_axis_home_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAbort()
{
    EMC_TASK_ABORT task_abort_msg;

    task_abort_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(task_abort_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}


static int sendJogStop(int axis)
{
    EMC_AXIS_ABORT emc_axis_abort_msg;
    
    // in case of TELEOP mode we really need to send an TELEOP_VECTOR message
    // not a simple AXIS_ABORT, as more than one axis would be moving
    // (hint TELEOP mode is for nontrivial kinematics)
    EMC_TRAJ_SET_TELEOP_VECTOR emc_set_teleop_vector;

    if ((emcStatus->task.state != EMC_TASK_STATE_ON) || (emcStatus->task.mode != EMC_TASK_MODE_MANUAL))
	return -1;

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    if (emcStatus->motion.traj.mode != EMC_TRAJ_MODE_TELEOP) {
	emc_axis_abort_msg.serial_number = ++emcCommandSerialNumber;
	emc_axis_abort_msg.axis = axis;
	emcCommandBuffer->write(emc_axis_abort_msg);

	if (emcWaitType == EMC_WAIT_RECEIVED) {
	    return emcCommandWaitReceived(emcCommandSerialNumber);
	} else if (emcWaitType == EMC_WAIT_DONE) {
	    return emcCommandWaitDone(emcCommandSerialNumber);
	}

    }
    else {
	emc_set_teleop_vector.serial_number = ++emcCommandSerialNumber;
	emc_set_teleop_vector.vector.tran.x = 0;
	emc_set_teleop_vector.vector.tran.y = 0;
	emc_set_teleop_vector.vector.tran.z = 0;
	emc_set_teleop_vector.vector.a = 0;
	emc_set_teleop_vector.vector.b = 0;
	emc_set_teleop_vector.vector.c = 0;
	emcCommandBuffer->write(emc_set_teleop_vector);

	if (emcWaitType == EMC_WAIT_RECEIVED) {
	    return emcCommandWaitReceived(emcCommandSerialNumber);
	} else if (emcWaitType == EMC_WAIT_DONE) {
	    return emcCommandWaitDone(emcCommandSerialNumber);
	}
	
    }
    return 0;
}

static int sendJogCont(int axis, double speed)
{
    EMC_AXIS_JOG emc_axis_jog_msg;
    EMC_TRAJ_SET_TELEOP_VECTOR emc_set_teleop_vector;

    if ((emcStatus->task.state != EMC_TASK_STATE_ON) || (emcStatus->task.mode != EMC_TASK_MODE_MANUAL))
	return -1;

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    if (emcStatus->motion.traj.mode != EMC_TRAJ_MODE_TELEOP) {
	emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
	emc_axis_jog_msg.axis = axis;
	emc_axis_jog_msg.vel = speed / 60.0;
	emcCommandBuffer->write(emc_axis_jog_msg);
    } else {
	emc_set_teleop_vector.serial_number = ++emcCommandSerialNumber;
	emc_set_teleop_vector.vector.tran.x = 0.0;
	emc_set_teleop_vector.vector.tran.y = 0.0;
	emc_set_teleop_vector.vector.tran.z = 0.0;

	switch (axis) {
	case 0:
	    emc_set_teleop_vector.vector.tran.x = speed / 60.0;
	    break;
	case 1:
	    emc_set_teleop_vector.vector.tran.y = speed / 60.0;
	    break;
	case 2:
	    emc_set_teleop_vector.vector.tran.z = speed / 60.0;
	    break;
	case 3:
	    emc_set_teleop_vector.vector.a = speed / 60.0;
	    break;
	case 4:
	    emc_set_teleop_vector.vector.b = speed / 60.0;
	    break;
	case 5:
	    emc_set_teleop_vector.vector.c = speed / 60.0;
	    break;
	}
	emcCommandBuffer->write(emc_set_teleop_vector);
    }

    return emcCommandWaitReceived(emcCommandSerialNumber);
}


static int sendFeedOverride(double override)
{
    EMC_TRAJ_SET_SCALE emc_traj_set_scale_msg;

    if (override < 0.0) {
	override = 0.0;
    }

    if (override > maxFeedOverride) {
	override = maxFeedOverride;
    }
    
    emc_traj_set_scale_msg.serial_number = ++emcCommandSerialNumber;
    emc_traj_set_scale_msg.scale = override;
    emcCommandBuffer->write(emc_traj_set_scale_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSpindleOverride(double override)
{
    EMC_TRAJ_SET_SPINDLE_SCALE emc_traj_set_spindle_scale_msg;

    if (override < minSpindleOverride) {
	override = minSpindleOverride;
    }

    if (override > maxSpindleOverride) {
	override = maxSpindleOverride;
    }
    
    emc_traj_set_spindle_scale_msg.serial_number = ++emcCommandSerialNumber;
    emc_traj_set_spindle_scale_msg.scale = override;
    emcCommandBuffer->write(emc_traj_set_spindle_scale_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}


//currently commented out to reduce warnings
/*
//sendFoo messages
//used for sending NML messages
static int sendOverrideLimits(int axis)
{
    EMC_AXIS_OVERRIDE_LIMITS lim_msg;

    lim_msg.axis = axis;	// neg means off, else on for all
    lim_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(lim_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendSetTeleopEnable(int enable)
{
    EMC_TRAJ_SET_TELEOP_ENABLE emc_set_teleop_enable_msg;

    emc_set_teleop_enable_msg.enable = enable;
    emc_set_teleop_enable_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_set_teleop_enable_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendClearProbeTrippedFlag()
{
    EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG emc_clear_probe_tripped_flag_msg;

    emc_clear_probe_tripped_flag_msg.serial_number =
	++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_clear_probe_tripped_flag_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendProbe(double x, double y, double z)
{
    EMC_TRAJ_PROBE emc_probe_msg;

    emc_probe_msg.pos.tran.x = x;
    emc_probe_msg.pos.tran.y = y;
    emc_probe_msg.pos.tran.z = z;

    emc_probe_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_probe_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}
//end of commenting out */

static int iniLoad(const char *filename)
{
    IniFile inifile;
    const char *inistring;
    double d;
    int i;

    // open it
    if (inifile.Open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = inifile.Find("DEBUG", "EMC"))) {
	// copy to global
	if (1 != sscanf(inistring, "%i", &EMC_DEBUG)) {
	    EMC_DEBUG = 0;
	}
    } else {
	// not found, use default
	EMC_DEBUG = 0;
    }

    if (NULL != (inistring = inifile.Find("NML_FILE", "EMC"))) {
	// copy to global
	strcpy(EMC_NMLFILE, inistring);
    } else {
	// not found, use default
    }

    if (NULL != (inistring = inifile.Find("MAX_FEED_OVERRIDE", "DISPLAY"))) {
	if (1 == sscanf(inistring, "%lf", &d) && d > 0.0) {
	    maxFeedOverride =  d;
	}
	else {
    	    // error-- no value provided, use 100% as max
	    maxFeedOverride =  1.0;
	}
    }
    else {
	// no line at all
        maxFeedOverride =  1.0;
    }

    if (NULL != (inistring = inifile.Find("MIN_SPINDLE_OVERRIDE", "DISPLAY"))) {
	if (1 == sscanf(inistring, "%lf", &d) && d > 0.0) {
	    minSpindleOverride =  d;
	}
	else {
    	    // error-- no value provided, use 100% as max
	    minSpindleOverride =  1.0;
	}
    }
    else {
	// no line at all
        minSpindleOverride =  1.0;
    }
    
    if (NULL != (inistring = inifile.Find("MAX_SPINDLE_OVERRIDE", "DISPLAY"))) {
	if (1 == sscanf(inistring, "%lf", &d) && d > 0.0) {
	    maxSpindleOverride =  d;
	}
	else {
    	    // error-- no value provided, use 100% as max
	    maxSpindleOverride =  1.0;
	}
    }
    else {
	// no line at all
        maxSpindleOverride =  1.0;
    }
    
    if (NULL != (inistring = inifile.Find("AXES", "TRAJ"))) {
	if (1 == sscanf(inistring, "%d", &i) && i > 0) {
	    num_axes =  i;
	}
	else {
    	    // error-- no value provided, use 100% as max
	    num_axes =  0;
	}
    }
    else {
	// no line at all
        num_axes =  0;
    }

    if (NULL != (inistring = inifile.Find("LINEAR_UNITS", "DISPLAY"))) {
	if (!strcmp(inistring, "AUTO")) {
	    linearUnitConversion = LINEAR_UNITS_AUTO;
	} else if (!strcmp(inistring, "INCH")) {
	    linearUnitConversion = LINEAR_UNITS_INCH;
	} else if (!strcmp(inistring, "MM")) {
	    linearUnitConversion = LINEAR_UNITS_MM;
	} else if (!strcmp(inistring, "CM")) {
	    linearUnitConversion = LINEAR_UNITS_CM;
	}
    } else {
	// not found, leave default alone
    }

    if (NULL != (inistring = inifile.Find("ANGULAR_UNITS", "DISPLAY"))) {
	if (!strcmp(inistring, "AUTO")) {
	    angularUnitConversion = ANGULAR_UNITS_AUTO;
	} else if (!strcmp(inistring, "DEG")) {
	    angularUnitConversion = ANGULAR_UNITS_DEG;
	} else if (!strcmp(inistring, "RAD")) {
	    angularUnitConversion = ANGULAR_UNITS_RAD;
	} else if (!strcmp(inistring, "GRAD")) {
	    angularUnitConversion = ANGULAR_UNITS_GRAD;
	}
    } else {
	// not found, leave default alone
    }

    // close it
    inifile.Close();

    return 0;
}

static void hal_init_pins()
{
    int joint;

    old_halui_data.machine_on = *(halui_data->machine_on) = 0;
    old_halui_data.machine_off = *(halui_data->machine_off) = 0;

    old_halui_data.estop_activate = *(halui_data->estop_activate) = 0;
    old_halui_data.estop_reset = *(halui_data->estop_reset) = 0;

    
    for (joint=0; joint < num_axes; joint++) {
	*(halui_data->joint_home[joint]) = old_halui_data.joint_home[joint] = 0;
	*(halui_data->joint_nr_select[joint]) = old_halui_data.joint_nr_select[joint] = 0;
	*(halui_data->jog_minus[joint]) = old_halui_data.jog_minus[joint] = 0;
	*(halui_data->jog_plus[joint]) = old_halui_data.jog_plus[joint] = 0;
	*(halui_data->jog_analog[joint]) = old_halui_data.jog_analog[joint] = 0;
    }

    *(halui_data->joint_home[num_axes]) = old_halui_data.joint_home[num_axes] = 0;
    *(halui_data->jog_minus[num_axes]) = old_halui_data.jog_minus[num_axes] = 0;
    *(halui_data->jog_plus[num_axes]) = old_halui_data.jog_plus[num_axes] = 0;
    *(halui_data->jog_deadband) = 0.2;
    *(halui_data->jog_speed) = 0;

    *(halui_data->joint_selected) = 0; // select joint 0 by default
    
    *(halui_data->fo_scale) = old_halui_data.fo_scale = 0.1; //sane default
    *(halui_data->so_scale) = old_halui_data.so_scale = 0.1; //sane default
}

static int check_bit_changed(hal_bit_t *halpin, hal_bit_t *oldpin) {
    hal_bit_t bit;
    
    bit = *(halpin);
    if (bit != *(oldpin)) {
	*(oldpin) = bit;
	if (bit != 0) //if transition to 1
	    return 1;
    }
    return 0;
}

// this function looks if any of the hal pins has changed
// and sends appropiate messages if so
static void check_hal_changes()
{
    hal_s32_t counts;
    int select_changed, joint;
    hal_bit_t bit, js;
    hal_float_t floatt;
    
    //check if machine_on pin has changed (the rest work exactly the same)
    if (check_bit_changed(halui_data->machine_on, &(old_halui_data.machine_on)) != 0)
	sendMachineOn();                //send MachineOn NML command
    
    if (check_bit_changed(halui_data->machine_off, &(old_halui_data.machine_off)) != 0)
	sendMachineOff();

    if (check_bit_changed(halui_data->estop_activate, &(old_halui_data.estop_activate)) != 0)
	sendEstop();

    if (check_bit_changed(halui_data->estop_reset, &(old_halui_data.estop_reset)) != 0)
	sendEstopReset();

    if (check_bit_changed(halui_data->mode_manual, &(old_halui_data.mode_manual)) != 0)
	sendManual();

    if (check_bit_changed(halui_data->mode_auto, &(old_halui_data.mode_auto)) != 0)
	sendAuto();

    if (check_bit_changed(halui_data->mode_mdi, &(old_halui_data.mode_mdi)) != 0)
	sendMdi();

    if (check_bit_changed(halui_data->mist_on, &(old_halui_data.mist_on)) != 0)
	sendMistOn();

    if (check_bit_changed(halui_data->mist_off, &(old_halui_data.mist_off)) != 0)
	sendMistOff();

    if (check_bit_changed(halui_data->flood_on, &(old_halui_data.flood_on)) != 0)
	sendFloodOn();

    if (check_bit_changed(halui_data->flood_off, &(old_halui_data.flood_off)) != 0)
	sendFloodOff();

    if (check_bit_changed(halui_data->lube_on, &(old_halui_data.lube_on)) != 0)
	sendLubeOn();

    if (check_bit_changed(halui_data->lube_off, &(old_halui_data.lube_off)) != 0)
	sendLubeOff();

    if (check_bit_changed(halui_data->program_run, &(old_halui_data.program_run)) != 0)
	sendProgramRun(0);

    if (check_bit_changed(halui_data->program_pause, &(old_halui_data.program_pause)) != 0)
	sendProgramPause();

    if (check_bit_changed(halui_data->program_os_on, &(old_halui_data.program_os_on)) != 0)
	sendSetOptionalStop(ON);

    if (check_bit_changed(halui_data->program_os_off, &(old_halui_data.program_os_off)) != 0)
	sendSetOptionalStop(OFF);

    if (check_bit_changed(halui_data->program_bd_on, &(old_halui_data.program_bd_on)) != 0)
	sendSetBlockDelete(ON);

    if (check_bit_changed(halui_data->program_bd_off, &(old_halui_data.program_bd_off)) != 0)
	sendSetBlockDelete(OFF);

    if (check_bit_changed(halui_data->program_resume, &(old_halui_data.program_resume)) != 0)
	sendProgramResume();

    if (check_bit_changed(halui_data->program_step, &(old_halui_data.program_step)) != 0)
	sendProgramStep();

    //feed-override stuff
    counts = *halui_data->fo_counts;
    if(counts != old_halui_data.fo_counts) {
        sendFeedOverride( *halui_data->fo_value + (counts - old_halui_data.fo_counts) *
                *halui_data->fo_scale);
        old_halui_data.fo_counts = counts;
    }

    //spindle-override stuff
    counts = *halui_data->so_counts;
    if(counts != old_halui_data.so_counts) {
        sendSpindleOverride( *halui_data->so_value + (counts - old_halui_data.so_counts) *
                *halui_data->so_scale);
        old_halui_data.so_counts = counts;
    }
    
    if (check_bit_changed(halui_data->fo_increase, &(old_halui_data.fo_increase)) != 0)
        sendFeedOverride(*halui_data->fo_value + *halui_data->fo_scale);
    if (check_bit_changed(halui_data->fo_decrease, &(old_halui_data.fo_decrease)) != 0)
        sendFeedOverride(*halui_data->fo_value - *halui_data->fo_scale);

    if (check_bit_changed(halui_data->so_increase, &(old_halui_data.so_increase)) != 0)
        sendSpindleOverride(*halui_data->so_value + *halui_data->so_scale);
    if (check_bit_changed(halui_data->so_decrease, &(old_halui_data.so_decrease)) != 0)
        sendSpindleOverride(*halui_data->so_value - *halui_data->so_scale);

//spindle stuff
    if (check_bit_changed(halui_data->spindle_start, &(old_halui_data.spindle_start)) != 0)
	sendSpindleForward();

    if (check_bit_changed(halui_data->spindle_stop, &(old_halui_data.spindle_stop)) != 0)
	sendSpindleOff();

    if (check_bit_changed(halui_data->spindle_forward, &(old_halui_data.spindle_forward)) != 0)
	sendSpindleForward();

    if (check_bit_changed(halui_data->spindle_reverse, &(old_halui_data.spindle_reverse)) != 0)
	sendSpindleReverse();

    bit = *(halui_data->spindle_increase);
    if (bit != old_halui_data.spindle_increase) {
	if (bit != 0)
	    sendSpindleIncrease();
	if (bit == 0)
	    sendSpindleConstant();
	old_halui_data.spindle_increase = bit;
    }

    bit = *(halui_data->spindle_decrease);
    if (bit != old_halui_data.spindle_decrease) {
	if (bit != 0)
	    sendSpindleDecrease();
	if (bit == 0)
	    sendSpindleConstant();
	old_halui_data.spindle_decrease = bit;
    }

    if (check_bit_changed(halui_data->spindle_brake_on, &(old_halui_data.spindle_brake_on)) != 0)
	sendBrakeEngage();

    if (check_bit_changed(halui_data->spindle_brake_off, &(old_halui_data.spindle_brake_off)) != 0)
	sendBrakeRelease();
    
    if (check_bit_changed(halui_data->abort, &(old_halui_data.abort)) != 0)
	sendAbort();
    
// joint stuff (selection, homing..)
    select_changed = -1; // flag to see if the selected joint changed
    
    for (joint=0; joint < num_axes; joint++) {
	if (check_bit_changed(halui_data->joint_home[joint], &(old_halui_data.joint_home[joint])) != 0)
	    sendHome(joint);

	bit = *(halui_data->jog_minus[joint]);
	if (bit != old_halui_data.jog_minus[joint]) {
	    if (bit != 0)
		sendJogCont(joint,-*(halui_data->jog_speed));
	    else
		sendJogStop(joint);
	    old_halui_data.jog_minus[joint] = bit;
	}

	bit = *(halui_data->jog_plus[joint]);
	if (bit != old_halui_data.jog_plus[joint]) {
	    if (bit != 0)
		sendJogCont(joint,*(halui_data->jog_speed));
	    else
		sendJogStop(joint);
	    old_halui_data.jog_plus[joint] = bit;
	}

	floatt = *(halui_data->jog_analog[joint]);
	if (floatt != old_halui_data.jog_analog[joint]) {
	    if (fabs(floatt) > *(halui_data->jog_deadband))
		sendJogCont(joint,*(halui_data->jog_speed) * *(halui_data->jog_analog[joint]));
	    else
		sendJogStop(joint);
	    old_halui_data.jog_analog[joint] = floatt;
	}
	
	// check to see if another joint has been selected
	bit = *(halui_data->joint_nr_select[joint]);
	if (bit != old_halui_data.joint_nr_select[joint]) {
	    if (bit != 0) {
		*(halui_data->joint_selected) = joint;
		select_changed = joint; // flag that we changed the selected joint
	    } 
	    old_halui_data.joint_home[joint] = bit;
	}
    }
    
    if (select_changed >= 0) {
	for (joint = 0; joint < num_axes; joint++) {
	    if (joint != select_changed) {
		*(halui_data->joint_is_selected[joint]) = 0;
    	    } else {
		*(halui_data->joint_is_selected[joint]) = 1;
	    }
	}
    }

    if (check_bit_changed(halui_data->joint_home[num_axes], &(old_halui_data.joint_home[num_axes])) != 0)
	sendHome(*(halui_data->joint_selected));

    bit = *(halui_data->jog_minus[num_axes]);
    js = *(halui_data->joint_selected);
    if (bit != old_halui_data.jog_minus[num_axes]) {
        if (bit != 0)
	    sendJogCont(js, -*(halui_data->jog_speed));
	else
	    sendJogStop(js);
	old_halui_data.jog_minus[num_axes] = bit;
    }

    bit = *(halui_data->jog_plus[num_axes]);
    js = *(halui_data->joint_selected);
    if (bit != old_halui_data.jog_plus[num_axes]) {
        if (bit != 0)
    	    sendJogCont(js,*(halui_data->jog_speed));
	else
	    sendJogStop(js);
	old_halui_data.jog_plus[num_axes] = bit;
    }

    
}

// this function looks at the received NML status message
// and modifies the appropiate HAL pins
static void modify_hal_pins()
{
    int joint;
    
    if (emcStatus->task.state == EMC_TASK_STATE_ON) {
	*(halui_data->machine_is_on)=1;
    } else {
	*(halui_data->machine_is_on)=0;
    }

    if (emcStatus->task.state == EMC_TASK_STATE_ESTOP) {
	*(halui_data->estop_is_activated)=1;
    } else {
	*(halui_data->estop_is_activated)=0;
    }

    if (emcStatus->task.mode == EMC_TASK_MODE_MANUAL) {
	*(halui_data->mode_is_manual)=1;
    } else {
	*(halui_data->mode_is_manual)=0;
    }

    if (emcStatus->task.mode == EMC_TASK_MODE_AUTO) {
	*(halui_data->mode_is_auto)=1;
    } else {
	*(halui_data->mode_is_auto)=0;
    }

    if (emcStatus->task.mode == EMC_TASK_MODE_MDI) {
	*(halui_data->mode_is_mdi)=1;
    } else {
	*(halui_data->mode_is_mdi)=0;
    }

    *(halui_data->program_is_paused) = emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED;
    *(halui_data->program_is_running) = emcStatus->task.interpState == EMC_TASK_INTERP_READING || 
                                        emcStatus->task.interpState == EMC_TASK_INTERP_WAITING;
    *(halui_data->program_is_idle) = emcStatus->task.interpState == EMC_TASK_INTERP_IDLE;
    *(halui_data->program_os_is_on) = emcStatus->task.optional_stop_state;
    *(halui_data->program_bd_is_on) = emcStatus->task.block_delete_state;

    *(halui_data->fo_value) = emcStatus->motion.traj.scale; //feedoverride from 0 to 1 for 100%
    *(halui_data->so_value) = emcStatus->motion.traj.spindle_scale; //spindle-speed-override from 0 to 1 for 100%

    *(halui_data->mist_is_on) = emcStatus->io.coolant.mist;
    *(halui_data->flood_is_on) = emcStatus->io.coolant.flood;
    *(halui_data->lube_is_on) = emcStatus->io.lube.on;

    *(halui_data->tool_number) = emcStatus->io.tool.toolInSpindle;
    *(halui_data->tool_length_offset) = emcStatus->task.toolOffset.tran.z;

    *(halui_data->spindle_is_on) = (emcStatus->motion.spindle.speed != 0);
    *(halui_data->spindle_runs_forward) = (emcStatus->motion.spindle.direction == 1);
    *(halui_data->spindle_runs_backward) = (emcStatus->motion.spindle.direction == -1);
    *(halui_data->spindle_brake_is_on) = emcStatus->motion.spindle.brake;
    
    for (joint=0; joint < num_axes; joint++) {
	*(halui_data->joint_is_homed[joint]) = emcStatus->motion.axis[joint].homed;
	*(halui_data->joint_on_soft_min_limit[joint]) = emcStatus->motion.axis[joint].minSoftLimit;
	*(halui_data->joint_on_soft_max_limit[joint]) = emcStatus->motion.axis[joint].maxSoftLimit; 
	*(halui_data->joint_on_hard_min_limit[joint]) = emcStatus->motion.axis[joint].minHardLimit; 
	*(halui_data->joint_on_hard_max_limit[joint]) = emcStatus->motion.axis[joint].maxHardLimit; 
	*(halui_data->joint_has_fault[joint]) = emcStatus->motion.axis[joint].fault;
    }
    
    *(halui_data->joint_is_homed[num_axes]) = emcStatus->motion.axis[*(halui_data->joint_selected)].homed;
    *(halui_data->joint_on_soft_min_limit[num_axes]) = emcStatus->motion.axis[*(halui_data->joint_selected)].minSoftLimit;
    *(halui_data->joint_on_soft_max_limit[num_axes]) = emcStatus->motion.axis[*(halui_data->joint_selected)].maxSoftLimit; 
    *(halui_data->joint_on_hard_min_limit[num_axes]) = emcStatus->motion.axis[*(halui_data->joint_selected)].minHardLimit; 
    *(halui_data->joint_on_hard_max_limit[num_axes]) = emcStatus->motion.axis[*(halui_data->joint_selected)].maxHardLimit; 
    *(halui_data->joint_has_fault[num_axes]) = emcStatus->motion.axis[*(halui_data->joint_selected)].fault;

}



int main(int argc, char *argv[])
{
    // process command line args
    if (0 != emcGetArgs(argc, argv)) {
	rcs_print_error("error in argument list\n");
	exit(1);
    }

    // get configuration information
    if (0 != iniLoad(EMC_INIFILE)) {
	rcs_print_error("iniLoad error\n");
	exit(2);
    }

    //init HAL and export pins
    if (0 != halui_hal_init()) {
	rcs_print_error("hal_init error\n");
	exit(1);
    }

    //initialize safe values
    hal_init_pins();

    // init NML
    if (0 != tryNml()) {
	rcs_print_error("can't connect to emc\n");
	thisQuit();
	exit(1);
    }
    
    // get current serial number, and save it for restoring when we quit
    // so as not to interfere with real operator interface
    updateStatus();

    done = 0;
    /* Register the routine that catches the SIGINT signal */
    signal(SIGINT, quit);
    /* catch SIGTERM too - the run script uses it to shut things down */
    signal(SIGTERM, quit);

    while (!done) {

	check_hal_changes(); //if anything changed send NML messages

	modify_hal_pins(); //if status changed modify HAL too
	
	esleep(0.02); //sleep for a while
	
	updateStatus();
	emcCommandSerialNumber = emcStatus->echo_serial_number;
	saveEmcCommandSerialNumber = emcStatus->echo_serial_number;
    }
    thisQuit();
    return 0;
}
