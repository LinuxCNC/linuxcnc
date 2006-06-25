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

  Exported pins (list not complete, names up for debate):

- machine:
   halui.machine.on                    bit  //pin for setting machine On
   halui.machine.off                   bit  //pin for setting machine Off
   halui.machine.is-on                 bit  //pin for machine is On/Off

- estop:   
   halui.estop.activate                bit  //pin for resetting Estop (emc internal) On/Off
   halui.estop.reset                   bit  //pin for resetting Estop (emc internal) On/Off
   halui.estop.is-reset                bit  //pin for resetting Estop (emc internal) On/Off
   
- mode:
   halui.mode.manual                   bit  //pin for requesting manual mode
   halui.mode.is_manual                bit  //pin for manual mode is on
   halui.mode.auto                     bit  //pin for requesting auto mode
   halui.mode.is_auto                  bit  //pin for auto mode is on
   halui.mode.mdi                      bit  //pin for requesting mdi mode
   halui.mode.is_mdi                   bit  //pin for mdi mode is on

- mist, flood, lube:
   halui.mist.on                       bit  //pin for starting mist
   halui.mist.off                      bit  //pin for stoping mist
   halui.mist.is-on                    bit  //pin for mist is on
   halui.flood.on                      bit  //pin for starting flood
   halui.flood.off                     bit  //pin for stoping flood
   halui.flood.is-on                   bit  //pin for flood is on
   halui.lube.on                       bit  //pin for starting lube
   halui.lube.off                      bit  //pin for stoping lube
   halui.lube.is-on                    bit  //pin for lube is on

- spindle:
   halui.spindle.start                 bit
   halui.spindle.stop                  bit
   halui.spindle.forward               bit
   halui.spindle.reverse               bit
   halui.spindle.increase              bit
   halui.spindle.decrease              bit

   halui.spindle.brake-on              bit  //pin for activating spindle-brake
   halui.spindle.brake-off             bit  //pin for deactivating spindle/brake
   halui.spindle.brake-is-on           bit  //status pin that tells us if brake is on

- joint:
   halui.joint.0.home                  bit //works both ways
   ..
   halui.joint.7.home                  bit //works both ways

   halui.joint.x.on-min-limit-soft     bit
   halui.joint.x.on-max-limit-soft     bit
   halui.joint.x.on-min-limit-hard     bit
   halui.joint.x.on-max-limit-hard     bit
   
   halui.joint.x.fault                 bit   
   halui.joint.x.homed                 bit

- jogging:
   halui.jog.speed                     float //set jog speed

   halui.jog.0.minus                   bit
   halui.jog.0.plus                    bit
   ..
   halui.jog.7.jog-minus               bit
   halui.jog.7.jog-plus                bit

   halui.feed_override                 float

- tool:
   halui.tool.number                   u16  //current selected tool
   halui.tool.length-offset            float //current applied tool-length-offset

- program:
   halui.program.is-idle               bit
   halui.program.is-running            bit
   halui.program.is-paused             bit
   halui.program.run                   bit
   halui.program.pause                 bit
   halui.program.resume                bit
   halui.program.step                  bit

- probe:
   halui.probe.start                   bit
   halui.probe.clear                   bit
   halui.probe.is-tripped              bit
   halui.probe.has-value               float

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

    hal_s32_t *jog_wheel_counts;
    hal_u8_t *jog_wheel_axis;
    hal_float_t *jog_wheel_scale;
    hal_float_t *jog_wheel_speed;

} * halui_data; 

struct local_halui_str {
    hal_bit_t machine_on;         //pin for setting machine On
    hal_bit_t machine_off;        //pin for setting machine Off
    hal_bit_t machine_is_on;      //pin for machine is On/Off

    hal_bit_t estop_activate;     //pin for activating EMC ESTOP 
    hal_bit_t estop_reset;        //pin for resetting ESTOP
    hal_bit_t estop_is_reset;     //pin for status ESTOp is resetted
    hal_bit_t estop_is_activated; //pin for status ESTOP is activated

    hal_bit_t mode_manual;        //pin for requesting manual mode
    hal_bit_t mode_is_manual;     //pin for manual mode is on
    hal_bit_t mode_auto;          //pin for requesting auto mode
    hal_bit_t mode_is_auto;       //pin for auto mode is on
    hal_bit_t mode_mdi;           //pin for requesting mdi mode
    hal_bit_t mode_is_mdi;        //pin for mdi mode is on

    hal_bit_t mist_on;            //pin for starting mist
    hal_bit_t mist_off;           //pin for stoping mist
    hal_bit_t mist_is_on;         //pin for mist is on
    hal_bit_t flood_on;           //pin for starting flood
    hal_bit_t flood_off;          //pin for stoping flood
    hal_bit_t flood_is_on;        //pin for flood is on
    hal_bit_t lube_on;            //pin for starting lube
    hal_bit_t lube_off;           //pin for stoping lube
    hal_bit_t lube_is_on;         //pin for lube is on

    hal_bit_t program_is_idle;    //pin for notifying user that program is idle
    hal_bit_t program_is_running; //pin for notifying user that program is running
    hal_bit_t program_is_paused;  //pin for notifying user that program is paused
    hal_bit_t program_run;        //pin for running program
    hal_bit_t program_pause;      //pin for pausing program
    hal_bit_t program_resume;     //pin for resuming program
    hal_bit_t program_step;       //pin for running one line of the program

    hal_s32_t jog_wheel_counts;
    hal_u8_t jog_wheel_axis;
    hal_float_t jog_wheel_scale;
    hal_float_t jog_wheel_speed;

} old_halui_data; //pointer to the HAL-struct

static int comp_id, done;				/* component ID, main while loop */


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
static double emcTimeout = 0.0;

static enum {
    EMC_WAIT_NONE = 1,
    EMC_WAIT_RECEIVED,
    EMC_WAIT_DONE
} emcWaitType = EMC_WAIT_RECEIVED;

/* clean out for now, causes warnings
static enum {
    EMC_UPDATE_NONE = 1,
    EMC_UPDATE_AUTO
} emcUpdateType = EMC_UPDATE_AUTO; */

/********************************************************************
*
* Description: quit(int sig)
*		Signal handler for SIGINT - Usually generated by a
*		Ctrl C sequence from the keyboard.
*
* Return Value: None.
*
* Side Effects: Sets the termination condition of the main while loop.
*
* Called By: Operating system.
*
********************************************************************/
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
	// new data
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
    EMC_NULL emc_null_msg;

    if (0 != emcStatusBuffer) {
	// wait until current message has been received
	emcCommandWaitReceived(emcCommandSerialNumber);
    }

    if (0 != emcCommandBuffer) {
	// send null message to reset serial number to original
	emc_null_msg.serial_number = saveEmcCommandSerialNumber;
	emcCommandBuffer->write(emc_null_msg);
    }
    // clean up NML buffers

    if (emcErrorBuffer != 0) {
	delete emcErrorBuffer;
	emcErrorBuffer = 0;
    }

    if (emcStatusBuffer != 0) {
	delete emcStatusBuffer;
	emcStatusBuffer = 0;
	emcStatus = 0;
    }

    if (emcCommandBuffer != 0) {
	delete emcCommandBuffer;
	emcCommandBuffer = 0;
    }

    //don't forget the big HAL sin ;)
    hal_exit(comp_id);
    
    exit(0);
}

/*
  Unit conversion

  Length and angle units in the EMC status buffer are in user units, as
  defined in the INI file in [TRAJ] LINEAR,ANGULAR_UNITS. These may differ
  from the program units, and when they are the display is confusing.

  It may be desirable to synchronize the display units with the program
  units automatically, and also to break this sync and allow independent
  display of position values.

  The global variable "linearUnitConversion" is set by the Tcl commands
  emc_linear_unit_conversion to correspond to either "inch",
  "mm", "cm", "auto", or "custom". This forces numbers to be returned in the
  units specified, in program units when "auto" is set, or not converted
  at all if "custom" is specified.

  Ditto for "angularUnitConversion", set by emc_angular_unit_conversion
  to "deg", "rad", "grad", "auto", or "custom".

  With no args, emc_linear/angular_unit_conversion return the setting.

  The functions convertLinearUnits and convertAngularUnits take a length
  or angle value, typically from the emcStatus structure, and convert it
  as indicated by linearUnitConversion and angularUnitConversion, resp.
*/

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

/*
  to convert linear units, values are converted to mm, then to desired
  units
*/

// comment out for now, not used, causes a warning
//static double convertLinearUnits(double u)
//{
//    double in_mm;
//
//    /* convert u to mm */
//    in_mm = u / emcStatus->motion.traj.linearUnits;
//
//    /* convert u to display units */
//    switch (linearUnitConversion) {
//    case LINEAR_UNITS_MM:
//	return in_mm;
//	break;
//    case LINEAR_UNITS_INCH:
//	return in_mm * INCH_PER_MM;
//	break;
//    case LINEAR_UNITS_CM:
//	return in_mm * CM_PER_MM;
//	break;
//    case LINEAR_UNITS_AUTO:
//	switch (emcStatus->task.programUnits) {
//	case CANON_UNITS_MM:
//	    return in_mm;
//	    break;
//	case CANON_UNITS_INCHES:
//	    return in_mm * INCH_PER_MM;
//	    break;
//	case CANON_UNITS_CM:
//	    return in_mm * CM_PER_MM;
//	    break;
//	}
//	break;
//
//    case LINEAR_UNITS_CUSTOM:
//	return u;
//	break;
//    }
//
//    // If it ever gets here we have an error.
//
//    return u;
//}


int halui_export_pin_RD_bit(hal_bit_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_bit_new(name, HAL_RD, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}

int halui_export_pin_RD_s32(hal_s32_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_s32_new(name, HAL_RD, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}

int halui_export_pin_RD_u8(hal_u8_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_u8_new(name, HAL_RD, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}

int halui_export_pin_RD_float(hal_float_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_float_new(name, HAL_RD, pin, comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return HAL_SUCCESS;
}


int halui_export_pin_WR_bit(hal_bit_t **pin, char name[HAL_NAME_LEN+2]) 
{
    int retval;
    retval = hal_pin_bit_new(name, HAL_WR, pin, comp_id);
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

    //halui.machine.is-on              //pin for machine is On/Off
    retval = halui_export_pin_WR_bit(&(halui_data->machine_is_on), "halui.machine.is-on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.estop.is-activated         //pin for machine is On/Off
    retval = halui_export_pin_WR_bit(&(halui_data->estop_is_activated), "halui.estop.is-activated"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mode.is-manual             //pin for mode in manual mode
    retval = halui_export_pin_WR_bit(&(halui_data->mode_is_manual), "halui.mode.is-manual"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mode.is-auto               //pin for mode in auto mode
    retval = halui_export_pin_WR_bit(&(halui_data->mode_is_auto), "halui.mode.is-auto"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mode.is-mdi                //pin for mode in MDI mode
    retval = halui_export_pin_WR_bit(&(halui_data->mode_is_mdi), "halui.mode.is-mdi"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mist.is-on                 //pin signifiying mist is turned on
    retval = halui_export_pin_WR_bit(&(halui_data->mist_is_on), "halui.mist.is-on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.flood.is-on                 //pin signifiying flood is turned on
    retval = halui_export_pin_WR_bit(&(halui_data->flood_is_on), "halui.flood.is-on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.lube.is-on                 //pin signifiying lube is turned on
    retval = halui_export_pin_WR_bit(&(halui_data->lube_is_on), "halui.lube.is-on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.program.is-idle            //pin for notifying user that program is idle
    retval = halui_export_pin_WR_bit(&(halui_data->program_is_idle), "halui.program.is-idle"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.program.is-running         //pin for notifying user that program is running
    retval = halui_export_pin_WR_bit(&(halui_data->program_is_running), "halui.program.is-running"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.program.is-paused          //pin for notifying user that program is paused
    retval = halui_export_pin_WR_bit(&(halui_data->program_is_paused), "halui.program.is-paused"); 
    if (retval != HAL_SUCCESS) return -1;


    /* STEP 3b: export the in-pin(s) */

    //halui.machine.on           //pin for setting machine On
    retval = halui_export_pin_RD_bit(&(halui_data->machine_on), "halui.machine.on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.machine.off          //pin for setting machine Off
    retval = halui_export_pin_RD_bit(&(halui_data->machine_off), "halui.machine.off"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.estop.activate       //pin for activating ESTOP
    retval = halui_export_pin_RD_bit(&(halui_data->estop_activate), "halui.estop.activate"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.estop.reset          //pin for resetting ESTOP
    retval = halui_export_pin_RD_bit(&(halui_data->estop_reset), "halui.estop.reset"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mode.manual          //pin for activating manual mode
    retval = halui_export_pin_RD_bit(&(halui_data->mode_manual), "halui.mode.manual"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mode.auto            //pin for activating auto mode
    retval = halui_export_pin_RD_bit(&(halui_data->mode_auto), "halui.mode.auto"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mode.mdi             //pin for activating mdi mode
    retval = halui_export_pin_RD_bit(&(halui_data->mode_mdi), "halui.mode.mdi"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mist.on              //pin for activating mist
    retval = halui_export_pin_RD_bit(&(halui_data->mist_on), "halui.mist.on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.mist.off             //pin for deactivating mist
    retval = halui_export_pin_RD_bit(&(halui_data->mist_off), "halui.mist.off"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.flood.on             //pin for activating flood
    retval = halui_export_pin_RD_bit(&(halui_data->flood_on), "halui.flood.on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.flood.off            //pin for deactivating flood
    retval = halui_export_pin_RD_bit(&(halui_data->flood_off), "halui.flood.off"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.lube.on              //pin for activating lube
    retval = halui_export_pin_RD_bit(&(halui_data->lube_on), "halui.lube.on"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.lube.off             //pin for deactivating lube
    retval = halui_export_pin_RD_bit(&(halui_data->lube_off), "halui.lube.off"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.program.run          //pin for running program
    retval = halui_export_pin_RD_bit(&(halui_data->program_run), "halui.program.run"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.program.pause        //pin for pausing program
    retval = halui_export_pin_RD_bit(&(halui_data->program_pause), "halui.program.pause"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.program.resume       //pin for resuming program
    retval = halui_export_pin_RD_bit(&(halui_data->program_resume), "halui.program.resume"); 
    if (retval != HAL_SUCCESS) return -1;
    //halui.program.step         //pin for running one line of the program
    retval = halui_export_pin_RD_bit(&(halui_data->program_step), "halui.program.step"); 
    if (retval != HAL_SUCCESS) return -1;

    //halui.jog-wheel.counts
    retval = halui_export_pin_RD_s32(&(halui_data->jog_wheel_counts), "halui.jog-wheel.counts");
    if (retval != HAL_SUCCESS) return -1;
    *halui_data->jog_wheel_counts = 0;

    //halui.jog-wheel.axis
    retval = halui_export_pin_RD_u8(&(halui_data->jog_wheel_axis), "halui.jog-wheel.axis");
    if (retval != HAL_SUCCESS) return -1;
    //halui.jog-wheel.scale
    retval = halui_export_pin_RD_float(&(halui_data->jog_wheel_scale), "halui.jog-wheel.scale");
    if (retval != HAL_SUCCESS) return -1;
    //halui.jog-wheel.speed
    retval = halui_export_pin_RD_float(&(halui_data->jog_wheel_speed), "halui.jog-wheel.speed");
    if (retval != HAL_SUCCESS) return -1;


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

static int sendJogIncr(int axis, double speed, double incr)
{
    EMC_AXIS_INCR_JOG emc_axis_incr_jog_msg;

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    emc_axis_incr_jog_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_incr_jog_msg.axis = axis;
    emc_axis_incr_jog_msg.vel = speed;
    emc_axis_incr_jog_msg.incr = incr;
    emcCommandBuffer->write(emc_axis_incr_jog_msg);

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
static int sendDebug(int level)
{
    EMC_SET_DEBUG debug_msg;

    debug_msg.debug = level;
    debug_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(debug_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}




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
static int axisJogging = -1;

static int sendJogStop(int axis)
{
    EMC_AXIS_ABORT emc_axis_abort_msg;
    
    // in case of TELEOP mode we really need to send an TELEOP_VECTOR message
    // not a simple AXIS_ABORT, as more than one axis would be moving
    // (hint TELEOP mode is for nontrivial kinematics)
    EMC_TRAJ_SET_TELEOP_VECTOR emc_set_teleop_vector;

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

	axisJogging = -1;
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
	// \todo FIXME - should remember a list of jogging axes, and remove the last one
	axisJogging = -1;
	
    }
    return 0;
}

static int sendJogCont(int axis, double speed)
{
    EMC_AXIS_JOG emc_axis_jog_msg;
    EMC_TRAJ_SET_TELEOP_VECTOR emc_set_teleop_vector;

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    if (emcStatus->motion.traj.mode != EMC_TRAJ_MODE_TELEOP) {
	if (0 == jogPol[axis]) {
	    speed = -speed;
	}

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

    axisJogging = axis;
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

static int sendFeedOverride(double override)
{
    EMC_TRAJ_SET_SCALE emc_traj_set_scale_msg;

    if (override < 0.0) {
	override = 0.0;
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

static int sendTaskPlanInit()
{
    EMC_TASK_PLAN_INIT task_plan_init_msg;

    task_plan_init_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(task_plan_init_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}


static int sendMdiCmd(char *mdi)
{
    EMC_TASK_PLAN_EXECUTE emc_task_plan_execute_msg;

    strcpy(emc_task_plan_execute_msg.command, mdi);
    emc_task_plan_execute_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_execute_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendToolSetOffset(int tool, double length, double diameter)
{
    EMC_TOOL_SET_OFFSET emc_tool_set_offset_msg;

    emc_tool_set_offset_msg.tool = tool;
    emc_tool_set_offset_msg.length = length;
    emc_tool_set_offset_msg.diameter = diameter;
    emc_tool_set_offset_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_tool_set_offset_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAxisEnable(int axis, int val)
{
    EMC_AXIS_ENABLE emc_axis_enable_msg;
    EMC_AXIS_DISABLE emc_axis_disable_msg;

    if (val) {
	emc_axis_enable_msg.axis = axis;
	emc_axis_enable_msg.serial_number = ++emcCommandSerialNumber;
	emcCommandBuffer->write(emc_axis_enable_msg);
    } else {
	emc_axis_disable_msg.axis = axis;
	emc_axis_disable_msg.serial_number = ++emcCommandSerialNumber;
	emcCommandBuffer->write(emc_axis_disable_msg);
    }
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAxisLoadComp(int axis, const char *file)
{
    EMC_AXIS_LOAD_COMP emc_axis_load_comp_msg;

    strcpy(emc_axis_load_comp_msg.file, file);
    emc_axis_load_comp_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_load_comp_msg);
    if (emcWaitType == EMC_WAIT_RECEIVED) {
	return emcCommandWaitReceived(emcCommandSerialNumber);
    } else if (emcWaitType == EMC_WAIT_DONE) {
	return emcCommandWaitDone(emcCommandSerialNumber);
    }

    return 0;
}

static int sendAxisAlter(int axis, double alter)
{
    EMC_AXIS_ALTER emc_axis_alter_msg;

    emc_axis_alter_msg.alter = alter;
    emc_axis_alter_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_axis_alter_msg);
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
    Inifile inifile;
    const char *inistring;

    // open it
    if (inifile.open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = inifile.find("DEBUG", "EMC"))) {
	// copy to global
	if (1 != sscanf(inistring, "%i", &EMC_DEBUG)) {
	    EMC_DEBUG = 0;
	}
    } else {
	// not found, use default
	EMC_DEBUG = 0;
    }

    if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
	// copy to global
	strcpy(EMC_NMLFILE, inistring);
    } else {
	// not found, use default
    }

    if (NULL != (inistring = inifile.find("LINEAR_UNITS", "DISPLAY"))) {
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

    if (NULL != (inistring = inifile.find("ANGULAR_UNITS", "DISPLAY"))) {
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
    inifile.close();

    return 0;
}

static void hal_init_pins()
{
    old_halui_data.machine_on = *(halui_data->machine_on) = 0;
    old_halui_data.machine_off = *(halui_data->machine_off) = 0;

    old_halui_data.estop_activate = *(halui_data->estop_activate) = 0;
    old_halui_data.estop_reset = *(halui_data->estop_reset) = 0;

}

// this function looks if any of the hal pins has changed
// and sends appropiate messages if so
static void check_hal_changes()
{
    hal_s32_t counts;
    //check if machine_on pin has changed (the rest work exactly the same)
    if (*(halui_data->machine_on) != old_halui_data.machine_on) {
	if (*(halui_data->machine_on) != 0) //if transition to 1
	    sendMachineOn();                //send MachineOn NML command
	old_halui_data.machine_on = *(halui_data->machine_on);
    }

    if (*(halui_data->machine_off) != old_halui_data.machine_off) {
	if (*(halui_data->machine_off) != 0)
	    sendMachineOff();
	old_halui_data.machine_off = *(halui_data->machine_off);
    }

    if (*(halui_data->estop_activate) != old_halui_data.estop_activate) {
	if (*(halui_data->estop_activate) != 0)
	    sendEstop();
	old_halui_data.estop_activate = *(halui_data->estop_activate);
    }

    if (*(halui_data->estop_reset) != old_halui_data.estop_reset) {
	if (*(halui_data->estop_reset) != 0)
	    sendEstopReset();
	old_halui_data.estop_reset = *(halui_data->estop_reset);
    }

    if (*(halui_data->mode_manual) != old_halui_data.mode_manual) {
	if (*(halui_data->mode_manual) != 0)
	    sendManual();
	old_halui_data.mode_manual = *(halui_data->mode_manual);
    }

    if (*(halui_data->mode_auto) != old_halui_data.mode_auto) {
	if (*(halui_data->mode_auto) != 0)
	    sendAuto();
	old_halui_data.mode_auto = *(halui_data->mode_auto);
    }

    if (*(halui_data->mode_mdi) != old_halui_data.mode_mdi) {
	if (*(halui_data->mode_mdi) != 0)
	    sendMdi();
	old_halui_data.mode_mdi = *(halui_data->mode_mdi);
    }

    if (*(halui_data->mist_on) != old_halui_data.mist_on) {
	if (*(halui_data->mist_on) != 0)
	    sendMistOn();
	old_halui_data.mist_on = *(halui_data->mist_on);
    }

    if (*(halui_data->mist_off) != old_halui_data.mist_off) {
	if (*(halui_data->mist_off) != 0)
	    sendMistOff();
	old_halui_data.mist_off = *(halui_data->mist_off);
    }

    if (*(halui_data->flood_on) != old_halui_data.flood_on) {
	if (*(halui_data->flood_on) != 0)
	    sendFloodOn();
	old_halui_data.flood_on = *(halui_data->flood_on);
    }

    if (*(halui_data->flood_off) != old_halui_data.flood_off) {
	if (*(halui_data->flood_off) != 0)
	    sendFloodOff();
	old_halui_data.flood_off = *(halui_data->flood_off);
    }

    if (*(halui_data->lube_on) != old_halui_data.lube_on) {
	if (*(halui_data->lube_on) != 0)
	    sendLubeOn();
	old_halui_data.lube_on = *(halui_data->lube_on);
    }

    if (*(halui_data->lube_off) != old_halui_data.lube_off) {
	if (*(halui_data->lube_off) != 0)
	    sendLubeOff();
	old_halui_data.lube_off = *(halui_data->lube_off);
    }

    if (*(halui_data->program_run) != old_halui_data.program_run) {
	if (*(halui_data->program_run) != 0)
	    sendProgramRun(0);
	old_halui_data.program_run = *(halui_data->program_run);
    }

    if (*(halui_data->program_pause) != old_halui_data.program_pause) {
	if (*(halui_data->program_pause) != 0)
	    sendProgramPause();
	old_halui_data.program_pause = *(halui_data->program_pause);
    }

    if (*(halui_data->program_resume) != old_halui_data.program_resume) {
	if (*(halui_data->program_resume) != 0)
	    sendProgramResume();
	old_halui_data.program_resume = *(halui_data->program_resume);
    }

    if (*(halui_data->program_step) != old_halui_data.program_step) {
	if (*(halui_data->program_step) != 0)
	    sendProgramStep();
	old_halui_data.program_step = *(halui_data->program_step);
    }

    counts = *halui_data->jog_wheel_counts;
    if(counts != old_halui_data.jog_wheel_counts) {
        sendJogIncr(*halui_data->jog_wheel_axis, 
                *halui_data->jog_wheel_speed,
                (counts - old_halui_data.jog_wheel_counts) *
                *halui_data->jog_wheel_scale);
        old_halui_data.jog_wheel_counts = counts;
    }
}


// this function looks at the received NML status message
// and modifies the appropiate HAL pins
static void modify_hal_pins()
{
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

    *(halui_data->mist_is_on)=emcStatus->io.coolant.mist;
    *(halui_data->flood_is_on)=emcStatus->io.coolant.flood;
    *(halui_data->lube_is_on)=emcStatus->io.lube.on;

}



int main(int argc, char *argv[])
{
    // process command line args
    if (0 != emcGetArgs(argc, argv)) {
	rcs_print_error("error in argument list\n");
	exit(1);
    }

    //init HAL and export pins
    if (0 != halui_hal_init()) {
	rcs_print_error("hal_init error\n");
	exit(1);
    }
    //initialize safe values
    hal_init_pins();


    // get configuration information
    iniLoad(EMC_INIFILE);

    // init NML
    if (0 != tryNml()) {
	rcs_print_error("can't connect to emc\n");
	thisQuit();
	exit(1);
    }
    
    // get current serial number, and save it for restoring when we quit
    // so as not to interfere with real operator interface
    updateStatus();
    emcCommandSerialNumber = emcStatus->echo_serial_number;
    saveEmcCommandSerialNumber = emcStatus->echo_serial_number;

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
    }
    thisQuit();
    return 0;
}
