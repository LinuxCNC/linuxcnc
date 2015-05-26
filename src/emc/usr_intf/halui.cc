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
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "rtapi_math.h"

#include "hal.h"		/* access to HAL functions/definitions */
#include "rtapi.h"		/* rtapi_print_msg */
#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
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
   halui.mode.teleop                   bit  //pin for requesting teleop mode
   halui.mode.is_teleop                bit  //pin for teleop mode is on
   halui.mode.joint                    bit  //pin for requesting joint mode
   halui.mode.is_joint                 bit  //pin for joint mode is on

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
   halui.joint.0.unhome                bit  // pin for unhoming the specific joint
   halui.joint.0.is-homed              bit  // status pin telling that the joint is homed
   ..
   halui.joint.8.home                  bit 
   halui.joint.8.is-homed              bit 

   halui.joint.selected.home           bit  // pin for homing the selected joint
   halui.joint.selected.unhome         bit  // pin for unhoming the selected joint
   halui.joint.selected.is-homed       bit  // status pin telling that the selected joint is homed

   halui.joint.x.on-soft-min-limit     bit
   halui.joint.x.on-soft-max-limit     bit
   halui.joint.x.on-hard-min-limit     bit
   halui.joint.x.on-hard-max-limit     bit
     (x = 0..8, selected)
   
   halui.joint.x.has-fault             bit   
     (x = 0..8, selected)

   halui.joint.select                  u8   // select joint (0..7)           - internal halui
   halui.joint.selected                u8   // selected joint (0..7)         - internal halui
   halui.joint.x.select                bit  // pins for selecting a joint    - internal halui
   halui.joint.x.is-selected           bit  // status pin                    - internal halui

WIP:
   halui.axis.0.pos-commanded          float //cartesian position, commanded
   halui.axis.0.pos-feedback           float //cartesian position, actual
   halui.axis.0.pos-relative           float //cartesian position, relative
   ...

DONE - jogging:
   halui.jog-speed                     float //set jog speed
   halui.jog-deadband                  float //pin for setting the jog analog deadband (where not to move)

   halui.jog.0.minus                   bit
   halui.jog.0.plus                    bit
   halui.jog.0.analog                  float //pin for jogging the axis 0
   halui.jog.0.increment               float
   halui.jog.0.increment-plus          bit
   halui.jog.0.increment-minus         bit
   ..
   halui.jog.7.minus                   bit
   halui.jog.7.plus                    bit
   halui.jog.7.analog                  float //pin for jogging the axis 7
   halui.jog.7.increment               float
   halui.jog.7.increment-plus          bit
   halui.jog.7.increment-minus         bit
   halui.jog.selected.minus            bit
   halui.jog.selected.plus             bit
   halui.jog.selected.increment        float
   halui.jog.selected.increment-plus   bit
   halui.jog.selected.increment-minus  bit

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
   halui.program.stop                  bit

DONE: - general:
   halui.home-all                      bit // pin to send a sequenced home all joints message
   halui.abort                         bit // pin to send an abort message (clears out most errors, stops running programs, etc)

DONE: - max-velocity-override
   halui.max-velocity-override.value           float //current MV value
   halui.max-velocity-override.scale           float // pin for setting the scale on changing the MV
   halui.max-velocity-override.counts          s32   //counts from an encoder for example to change MV
   halui.max-velocity-override.count-enable    bit   // TRUE to modify MV based on counts
   halui.max-velocity-override.increase        bit   // pin for increasing the MV (+=scale)
   halui.max-velocity-override.direct-value    bit   // TRUE to make override based as a direct (scaled) value rather then counts of increments
   halui.max-velocity-override.decrease        bit   // pin for decreasing the MV (-=scale)

DONE: - feed-override
   halui.feed-override.value           float //current FO value
   halui.feed-override.scale           float // pin for setting the scale on changing the FO
   halui.feed-override.counts          s32   //counts from an encoder for example to change FO
   halui.feed-override.count-enable    bit   // TRUE to modify FO based on counts
   halui.feed-override.direct-value    bit   // TRUE to make override based as a direct (scaled) value rather then counts of increments
   halui.feed-override.increase        bit   // pin for increasing the FO (+=scale)
   halui.feed-override.decrease        bit   // pin for decreasing the FO (-=scale)

DONE: - spindle-override
   halui.spindle-override.value           float //current FO value
   halui.spindle-override.scale           float // pin for setting the scale on changing the SO
   halui.spindle-override.counts          s32   //counts from an encoder for example to change SO
   halui.spindle-override.count-enable    bit   // TRUE to modify SO based on counts
   halui.spindle-override.direct-value    bit   // TRUE to make override based as a direct (scaled) value rather then counts of increments
   halui.spindle-override.increase        bit   // pin for increasing the SO (+=scale)
   halui.spindle-override.decrease        bit   // pin for decreasing the SO (-=scale)

*/

#define MDI_MAX 64

#define HAL_FIELDS \
    FIELD(hal_bit_t,machine_on) /* pin for setting machine On */ \
    FIELD(hal_bit_t,machine_off) /* pin for setting machine Off */ \
    FIELD(hal_bit_t,machine_is_on) /* pin for machine is On/Off */ \
    FIELD(hal_bit_t,estop_activate) /* pin for activating EMC ESTOP  */ \
    FIELD(hal_bit_t,estop_reset) /* pin for resetting ESTOP */ \
    FIELD(hal_bit_t,estop_is_activated) /* pin for status ESTOP is activated */ \
\
    FIELD(hal_bit_t,mode_manual) /* pin for requesting manual mode */ \
    FIELD(hal_bit_t,mode_is_manual) /* pin for manual mode is on */ \
    FIELD(hal_bit_t,mode_auto) /* pin for requesting auto mode */ \
    FIELD(hal_bit_t,mode_is_auto) /* pin for auto mode is on */ \
    FIELD(hal_bit_t,mode_mdi) /* pin for requesting mdi mode */ \
    FIELD(hal_bit_t,mode_is_mdi) /* pin for mdi mode is on */ \
    FIELD(hal_bit_t,mode_teleop) /* pin for requesting teleop mode */ \
    FIELD(hal_bit_t,mode_is_teleop) /* pin for teleop mode is on */ \
    FIELD(hal_bit_t,mode_joint) /* pin for requesting joint mode */ \
    FIELD(hal_bit_t,mode_is_joint) /* pin for joint mode is on */ \
\
    FIELD(hal_bit_t,mist_on) /* pin for starting mist */ \
    FIELD(hal_bit_t,mist_off) /* pin for stoping mist */ \
    FIELD(hal_bit_t,mist_is_on) /* pin for mist is on */ \
    FIELD(hal_bit_t,flood_on) /* pin for starting flood */ \
    FIELD(hal_bit_t,flood_off) /* pin for stoping flood */ \
    FIELD(hal_bit_t,flood_is_on) /* pin for flood is on */ \
    FIELD(hal_bit_t,lube_on) /* pin for starting lube */ \
    FIELD(hal_bit_t,lube_off) /* pin for stoping lube */ \
    FIELD(hal_bit_t,lube_is_on) /* pin for lube is on */ \
\
    FIELD(hal_bit_t,program_is_idle) /* pin for notifying user that program is idle */ \
    FIELD(hal_bit_t,program_is_running) /* pin for notifying user that program is running */ \
    FIELD(hal_bit_t,program_is_paused) /* pin for notifying user that program is paused */ \
    FIELD(hal_bit_t,program_run) /* pin for running program */ \
    FIELD(hal_bit_t,program_pause) /* pin for pausing program */ \
    FIELD(hal_bit_t,program_resume) /* pin for resuming program */ \
    FIELD(hal_bit_t,program_step) /* pin for running one line of the program */ \
    FIELD(hal_bit_t,program_stop) /* pin for stopping the program */ \
    FIELD(hal_bit_t,program_os_on) /* pin for setting optional stop on */ \
    FIELD(hal_bit_t,program_os_off) /* pin for setting optional stop off */ \
    FIELD(hal_bit_t,program_os_is_on) /* status pin that optional stop is on */ \
    FIELD(hal_bit_t,program_bd_on) /* pin for setting block delete on */ \
    FIELD(hal_bit_t,program_bd_off) /* pin for setting block delete off */ \
    FIELD(hal_bit_t,program_bd_is_on) /* status pin that block delete is on */ \
\
    FIELD(hal_u32_t,tool_number) /* pin for current selected tool */ \
    FIELD(hal_float_t,tool_length_offset_x) /* current applied x tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_y) /* current applied y tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_z) /* current applied z tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_a) /* current applied a tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_b) /* current applied b tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_c) /* current applied c tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_u) /* current applied u tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_v) /* current applied v tool-length-offset */ \
    FIELD(hal_float_t,tool_length_offset_w) /* current applied w tool-length-offset */ \
\
    FIELD(hal_bit_t,spindle_start) /* pin for starting the spindle */ \
    FIELD(hal_bit_t,spindle_stop) /* pin for stoping the spindle */ \
    FIELD(hal_bit_t,spindle_is_on) /* status pin for spindle is on */ \
    FIELD(hal_bit_t,spindle_forward) /* pin for making the spindle go forward */ \
    FIELD(hal_bit_t,spindle_runs_forward) /* status pin for spindle running forward */ \
    FIELD(hal_bit_t,spindle_reverse) /* pin for making the spindle go reverse */ \
    FIELD(hal_bit_t,spindle_runs_backward) /* status pin for spindle running backward */ \
    FIELD(hal_bit_t,spindle_increase) /* pin for making the spindle go faster */ \
    FIELD(hal_bit_t,spindle_decrease) /* pin for making the spindle go slower */ \
\
    FIELD(hal_bit_t,spindle_brake_on) /* pin for activating spindle-brake */ \
    FIELD(hal_bit_t,spindle_brake_off) /* pin for deactivating spindle/brake */ \
    FIELD(hal_bit_t,spindle_brake_is_on) /* status pin that tells us if brake is on */ \
\
    ARRAY(hal_bit_t,joint_home,EMCMOT_MAX_JOINTS+1) /* pin for homing one joint */ \
    ARRAY(hal_bit_t,joint_unhome,EMCMOT_MAX_JOINTS+1) /* pin for unhoming one joint */ \
    ARRAY(hal_bit_t,joint_is_homed,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is homed */ \
    ARRAY(hal_bit_t,joint_on_soft_min_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the software min limit */ \
    ARRAY(hal_bit_t,joint_on_soft_max_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the software max limit */ \
    ARRAY(hal_bit_t,joint_on_hard_min_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the hardware min limit */ \
    ARRAY(hal_bit_t,joint_on_hard_max_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the hardware max limit */ \
    ARRAY(hal_bit_t,joint_has_fault,EMCMOT_MAX_JOINTS+1) /* status pin that the joint has a fault */ \
    FIELD(hal_u32_t,joint_selected) /* status pin for the joint selected */ \
    ARRAY(hal_bit_t,joint_nr_select,EMCMOT_MAX_JOINTS) /* nr. of pins to select a joint */ \
    ARRAY(hal_bit_t,joint_is_selected,EMCMOT_MAX_JOINTS) /* nr. of status pins for joint selected */ \
\
    ARRAY(hal_float_t,axis_pos_commanded,EMCMOT_MAX_AXIS+1) /* status pin for commanded cartesian position */ \
    ARRAY(hal_float_t,axis_pos_feedback,EMCMOT_MAX_AXIS+1) /* status pin for actual cartesian position */ \
    ARRAY(hal_float_t,axis_pos_relative,EMCMOT_MAX_AXIS+1) /* status pin for relative cartesian position */ \
\
    FIELD(hal_float_t,jog_speed) /* pin for setting the jog speed (halui internal) */ \
    ARRAY(hal_bit_t,jog_minus,EMCMOT_MAX_JOINTS+1) /* pin to jog in positive direction */ \
    ARRAY(hal_bit_t,jog_plus,EMCMOT_MAX_JOINTS+1) /* pin to jog in negative direction */ \
    ARRAY(hal_float_t,jog_analog,EMCMOT_MAX_JOINTS+1) /* pin for analog jogging (-1..0..1) */ \
    ARRAY(hal_float_t,jog_increment,EMCMOT_MAX_JOINTS+1) /* Incremental jogging */ \
    ARRAY(hal_bit_t,jog_increment_plus,EMCMOT_MAX_JOINTS+1) /* Incremental jogging, positive direction */ \
    ARRAY(hal_bit_t,jog_increment_minus,EMCMOT_MAX_JOINTS+1) /* Incremental jogging, negative direction */ \
    FIELD(hal_float_t,jog_deadband) /* pin for setting the jog analog deadband (where not to move) */ \
\
    FIELD(hal_s32_t,mv_counts) /* pin for the Max Velocity counting */ \
    FIELD(hal_bit_t,mv_count_enable) /* pin for the Max Velocity counting enable */ \
    FIELD(hal_bit_t,mv_direct_value) /* pin for enabling direct value option instead of counts */ \
    FIELD(hal_float_t,mv_scale) /* scale for the Max Velocity counting */ \
    FIELD(hal_float_t,mv_value) /* current Max Velocity value */ \
    FIELD(hal_bit_t,mv_increase) /* pin for increasing the MV (+=scale) */ \
    FIELD(hal_bit_t,mv_decrease) /* pin for decreasing the MV (-=scale) */ \
\
    FIELD(hal_s32_t,fo_counts) /* pin for the Feed Override counting */ \
    FIELD(hal_bit_t,fo_count_enable) /* pin for the Feed Override counting enable */ \
    FIELD(hal_bit_t,fo_direct_value) /* pin for enabling direct value option instead of counts  */ \
    FIELD(hal_float_t,fo_scale) /* scale for the Feed Override counting */ \
    FIELD(hal_float_t,fo_value) /* current Feed Override value */ \
    FIELD(hal_bit_t,fo_increase) /* pin for increasing the FO (+=scale) */ \
    FIELD(hal_bit_t,fo_decrease) /* pin for decreasing the FO (-=scale) */ \
\
    FIELD(hal_s32_t,so_counts) /* pin for the Spindle Speed Override counting */ \
    FIELD(hal_bit_t,so_count_enable) /* pin for the Spindle Speed Override counting enable */ \
    FIELD(hal_bit_t,so_direct_value) /* pin for enabling direct value option instead of counts */ \
    FIELD(hal_float_t,so_scale) /* scale for the Spindle Speed Override counting */ \
    FIELD(hal_float_t,so_value) /* current Spindle speed Override value */ \
    FIELD(hal_bit_t,so_increase) /* pin for increasing the SO (+=scale) */ \
    FIELD(hal_bit_t,so_decrease) /* pin for decreasing the SO (-=scale) */ \
\
    FIELD(hal_bit_t,home_all) /* pin for homing all joints in sequence */ \
    FIELD(hal_bit_t,abort) /* pin for aborting */ \
    ARRAY(hal_bit_t,mdi_commands,MDI_MAX)

struct PTR {
    template<class T>
    struct field { typedef T *type; };
};

template<class T> struct NATIVE {};
template<> struct NATIVE<hal_bit_t> { typedef bool type; };
template<> struct NATIVE<hal_s32_t> { typedef __s32 type; };
template<> struct NATIVE<hal_u32_t> { typedef __u32 type; };
template<> struct NATIVE<hal_float_t> { typedef double type; };
struct VALUE {
    template<class T> struct field { typedef typename NATIVE<T>::type type; };
};

template<class T>
struct halui_str_base
{
#define FIELD(t,f) typename T::template field<t>::type f;
#define ARRAY(t,f,n) typename T::template field<t>::type f[n];
HAL_FIELDS
#undef FIELD
#undef ARRAY
};

typedef halui_str_base<PTR> halui_str;
typedef halui_str_base<VALUE> local_halui_str;

static halui_str *halui_data;
static local_halui_str old_halui_data;

static char *mdi_commands[MDI_MAX];
static int num_mdi_commands=0;
static int have_home_all = 0;

static int comp_id, done;				/* component ID, main while loop */

static int num_axes = 3; //number of axes, taken from the ini [TRAJ] section

static double maxFeedOverride=1;
static double maxMaxVelocity=1;
static double minSpindleOverride=0.0;
static double maxSpindleOverride=1.0;
static EMC_TASK_MODE_ENUM halui_old_mode = EMC_TASK_MODE_MANUAL;
static int halui_sent_mdi = 0;

// the NML channels to the EMC task
static RCS_CMD_CHANNEL *emcCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcStatusBuffer = 0;
EMC_STAT *emcStatus = 0;

// the NML channel for errors
static NML *emcErrorBuffer = 0;

// the serial number to use.  By starting high we'll not clash with other guis so easily.
// XXX it would be nice to have a real fix here. XXX
static int emcCommandSerialNumber = 100000;

// default value for timeout, 0 means wait forever
// use same timeout value as in tkemc & mini
static double receiveTimeout = 1.;
static double doneTimeout = 60.;

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
				emc_nmlfile);
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
				 emc_nmlfile);
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
	    new NML(nmlErrorFormat, "emcError", "xemc", emc_nmlfile);
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

    if ((emc_debug & EMC_DEBUG_NML) == 0) {
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
    if ((emc_debug & EMC_DEBUG_NML) == 0) {
	set_rcs_print_destination(RCS_PRINT_TO_STDOUT);	// inhibit diag
	// messages
    }
    if (!good) {
	return -1;
    }

    if ((emc_debug & EMC_DEBUG_NML) == 0) {
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
    if ((emc_debug & EMC_DEBUG_NML) == 0) {
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

    while (end < receiveTimeout) {
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
    while (end < doneTimeout) {
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
    
    if(emcCommandBuffer) { delete emcCommandBuffer;  emcCommandBuffer = 0; }
    if(emcStatusBuffer) { delete emcStatusBuffer;  emcStatusBuffer = 0; }
    if(emcErrorBuffer) { delete emcErrorBuffer;  emcErrorBuffer = 0; }
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

int halui_export_pin_IN_bit(hal_bit_t **pin, const char *name)
{
    int retval;
    retval = hal_pin_bit_new(name, HAL_IN, pin, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}

int halui_export_pin_IN_s32(hal_s32_t **pin, const char *name)
{
    int retval;
    retval = hal_pin_s32_new(name, HAL_IN, pin, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}

int halui_export_pin_IN_float(hal_float_t **pin, const char *name)
{
    int retval;
    retval = hal_pin_float_new(name, HAL_IN, pin, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}


int halui_export_pin_OUT_bit(hal_bit_t **pin, const char *name)
{
    int retval;
    retval = hal_pin_bit_new(name, HAL_OUT, pin, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
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
    int axis;

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
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->estop_is_activated), "halui.estop.is-activated"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_manual), "halui.mode.is-manual"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_auto), "halui.mode.is-auto"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_mdi), "halui.mode.is-mdi"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_teleop), "halui.mode.is-teleop"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mode_is_joint), "halui.mode.is-joint"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->mist_is_on), "halui.mist.is-on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->flood_is_on), "halui.flood.is-on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->lube_is_on), "halui.lube.is-on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_is_idle), "halui.program.is-idle"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_is_running), "halui.program.is-running"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_is_paused), "halui.program.is-paused"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_os_is_on), "halui.program.optional-stop.is-on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->program_bd_is_on), "halui.program.block-delete.is-on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_is_on), "halui.spindle.is-on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_runs_forward), "halui.spindle.runs-forward"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_runs_backward), "halui.spindle.runs-backward"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_OUT_bit(&(halui_data->spindle_brake_is_on), "halui.spindle.brake-is-on"); 
    if (retval < 0) return retval;

    for (joint=0; joint < num_axes ; joint++) {
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_is_homed[joint]), comp_id, "halui.joint.%d.is-homed", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_is_selected[joint]), comp_id, "halui.joint.%d.is-selected", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_min_limit[joint]), comp_id, "halui.joint.%d.on-soft-min-limit", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_max_limit[joint]), comp_id, "halui.joint.%d.on-soft-max-limit", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_min_limit[joint]), comp_id, "halui.joint.%d.on-hard-min-limit", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_max_limit[joint]), comp_id, "halui.joint.%d.on-hard-max-limit", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_has_fault[joint]), comp_id, "halui.joint.%d.has-fault", joint); 
	if (retval < 0) return retval;
    }

    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_min_limit[num_axes]), comp_id, "halui.joint.selected.on-soft-min-limit"); 
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_soft_max_limit[num_axes]), comp_id, "halui.joint.selected.on-soft-limit"); 
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_min_limit[num_axes]), comp_id, "halui.joint.selected.on-hard-min-limit"); 
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_on_hard_max_limit[num_axes]), comp_id, "halui.joint.selected.on-hard-max-limit"); 
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_has_fault[num_axes]), comp_id, "halui.joint.selected.has-fault"); 
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_OUT, &(halui_data->joint_is_homed[num_axes]), comp_id, "halui.joint.selected.is_homed"); 
    if (retval < 0) return retval;

    for (axis=0; axis < EMCMOT_MAX_AXIS ; axis++) {
	retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->axis_pos_commanded[axis]), comp_id, "halui.axis.%d.pos-commanded", axis);
    if (retval < 0) return retval;
	retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->axis_pos_feedback[axis]), comp_id, "halui.axis.%d.pos-feedback", axis);
    if (retval < 0) return retval;
	retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->axis_pos_relative[axis]), comp_id, "halui.axis.%d.pos-relative", axis);
    if (retval < 0) return retval;
    }

    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->mv_value), comp_id, "halui.max-velocity.value"); 
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->fo_value), comp_id, "halui.feed-override.value"); 
    if (retval < 0) return retval;
    retval = hal_pin_u32_newf(HAL_OUT, &(halui_data->joint_selected), comp_id, "halui.joint.selected"); 
    if (retval < 0) return retval;
    retval = hal_pin_u32_newf(HAL_OUT, &(halui_data->tool_number), comp_id, "halui.tool.number"); 
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_x), comp_id, "halui.tool.length_offset.x");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_y), comp_id, "halui.tool.length_offset.y");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_z), comp_id, "halui.tool.length_offset.z");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_a), comp_id, "halui.tool.length_offset.a");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_b), comp_id, "halui.tool.length_offset.b");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_c), comp_id, "halui.tool.length_offset.c");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_u), comp_id, "halui.tool.length_offset.u");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_v), comp_id, "halui.tool.length_offset.v");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->tool_length_offset_w), comp_id, "halui.tool.length_offset.w");
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_OUT, &(halui_data->so_value), comp_id, "halui.spindle-override.value"); 
    if (retval < 0) return retval;

    /* STEP 3b: export the in-pin(s) */

    retval = halui_export_pin_IN_bit(&(halui_data->machine_on), "halui.machine.on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->machine_off), "halui.machine.off"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->estop_activate), "halui.estop.activate"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->estop_reset), "halui.estop.reset"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_manual), "halui.mode.manual"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_auto), "halui.mode.auto"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_mdi), "halui.mode.mdi"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_teleop), "halui.mode.teleop"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mode_joint), "halui.mode.joint"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mist_on), "halui.mist.on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mist_off), "halui.mist.off"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->flood_on), "halui.flood.on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->flood_off), "halui.flood.off"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->lube_on), "halui.lube.on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->lube_off), "halui.lube.off"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_run), "halui.program.run"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_pause), "halui.program.pause"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_resume), "halui.program.resume"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_step), "halui.program.step"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_stop), "halui.program.stop"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_os_on), "halui.program.optional-stop.on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_os_off), "halui.program.optional-stop.off"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_bd_on), "halui.program.block-delete.on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->program_bd_off), "halui.program.block-delete.off"); 
    if (retval < 0) return retval;

    retval = halui_export_pin_IN_bit(&(halui_data->spindle_start), "halui.spindle.start");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_stop), "halui.spindle.stop");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_forward), "halui.spindle.forward");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_reverse), "halui.spindle.reverse");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_increase), "halui.spindle.increase");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_decrease), "halui.spindle.decrease");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_brake_on), "halui.spindle.brake-on"); 
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->spindle_brake_off), "halui.spindle.brake-off"); 
    if (retval < 0) return retval;

    retval = halui_export_pin_IN_s32(&(halui_data->mv_counts), "halui.max-velocity.counts");
    if (retval < 0) return retval;
    *halui_data->mv_counts = 0;
    retval = halui_export_pin_IN_bit(&(halui_data->mv_count_enable), "halui.max-velocity.count-enable");
    if (retval < 0) return retval;
    *halui_data->mv_count_enable = 1;
    retval = halui_export_pin_IN_bit(&(halui_data->mv_direct_value), "halui.max-velocity.direct-value");
    if (retval < 0) return retval;
    *halui_data->mv_direct_value = 0;
    retval = halui_export_pin_IN_float(&(halui_data->mv_scale), "halui.max-velocity.scale");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mv_increase), "halui.max-velocity.increase");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->mv_decrease), "halui.max-velocity.decrease");
    if (retval < 0) return retval;

    retval = halui_export_pin_IN_s32(&(halui_data->fo_counts), "halui.feed-override.counts");
    if (retval < 0) return retval;
    *halui_data->fo_counts = 0;
    retval = halui_export_pin_IN_bit(&(halui_data->fo_count_enable), "halui.feed-override.count-enable");
    if (retval < 0) return retval;
    *halui_data->fo_count_enable = 1;
    retval = halui_export_pin_IN_bit(&(halui_data->fo_direct_value), "halui.feed-override.direct-value");
    if (retval < 0) return retval;
    *halui_data->fo_direct_value = 0;
    retval = halui_export_pin_IN_float(&(halui_data->fo_scale), "halui.feed-override.scale");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->fo_increase), "halui.feed-override.increase");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->fo_decrease), "halui.feed-override.decrease");
    if (retval < 0) return retval;

    retval = halui_export_pin_IN_s32(&(halui_data->so_counts), "halui.spindle-override.counts");
    if (retval < 0) return retval;
    *halui_data->so_counts = 0;
    retval = halui_export_pin_IN_bit(&(halui_data->so_count_enable), "halui.spindle-override.count-enable");
    if (retval < 0) return retval;
    *halui_data->so_count_enable = 1;
    retval = halui_export_pin_IN_bit(&(halui_data->so_direct_value), "halui.spindle-override.direct-value");
    if (retval < 0) return retval;
    *halui_data->so_direct_value = 0;
    retval = halui_export_pin_IN_float(&(halui_data->so_scale), "halui.spindle-override.scale");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->so_increase), "halui.spindle-override.increase");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_bit(&(halui_data->so_decrease), "halui.spindle-override.decrease");
    if (retval < 0) return retval;

    if (have_home_all) {
        retval = halui_export_pin_IN_bit(&(halui_data->home_all), "halui.home-all");
        if (retval < 0) return retval;
    }

    retval = halui_export_pin_IN_bit(&(halui_data->abort), "halui.abort"); 
    if (retval < 0) return retval;

    for (joint=0; joint < num_axes ; joint++) {
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_home[joint]), comp_id, "halui.joint.%d.home", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_unhome[joint]), comp_id, "halui.joint.%d.unhome", joint);
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_nr_select[joint]), comp_id, "halui.joint.%d.select", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_plus[joint]), comp_id, "halui.jog.%d.plus", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_minus[joint]), comp_id, "halui.jog.%d.minus", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_float_newf(HAL_IN, &(halui_data->jog_analog[joint]), comp_id, "halui.jog.%d.analog", joint); 
	if (retval < 0) return retval;
	retval =  hal_pin_float_newf(HAL_IN, &(halui_data->jog_increment[joint]), comp_id, "halui.jog.%d.increment", joint);
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_increment_plus[joint]), comp_id, "halui.jog.%d.increment-plus", joint);
	if (retval < 0) return retval;
	retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_increment_minus[joint]), comp_id, "halui.jog.%d.increment-minus", joint);
	if (retval < 0) return retval;
    }

    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_home[num_axes]), comp_id, "halui.joint.selected.home"); 
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->joint_unhome[num_axes]), comp_id, "halui.joint.selected.unhome");
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_plus[num_axes]), comp_id, "halui.jog.selected.plus"); 
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_minus[num_axes]), comp_id, "halui.jog.selected.minus"); 
    if (retval < 0) return retval;
    retval =  hal_pin_float_newf(HAL_IN, &(halui_data->jog_increment[num_axes]), comp_id, "halui.jog.selected.increment");
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_increment_plus[num_axes]), comp_id, "halui.jog.selected.increment-plus");
    if (retval < 0) return retval;
    retval =  hal_pin_bit_newf(HAL_IN, &(halui_data->jog_increment_minus[num_axes]), comp_id, "halui.jog.selected.increment-minus");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_float(&(halui_data->jog_speed), "halui.jog-speed");
    if (retval < 0) return retval;
    retval = halui_export_pin_IN_float(&(halui_data->jog_deadband), "halui.jog-deadband");
    if (retval < 0) return retval;


    for (int n=0; n<num_mdi_commands; n++) {
        retval = hal_pin_bit_newf(HAL_IN, &(halui_data->mdi_commands[n]), comp_id, "halui.mdi-command-%02d", n);
        if (retval < 0) return retval;
    }

    hal_ready(comp_id);
    return 0;
}

static int sendMachineOn()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ON;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendMachineOff()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_OFF;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendEstop()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ESTOP;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendEstopReset()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE_ESTOP_RESET;
    state_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(state_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendManual()
{
    EMC_TASK_SET_MODE mode_msg;

    if (emcStatus->task.mode == EMC_TASK_MODE_MANUAL) {
        return 0;
    }

    mode_msg.mode = EMC_TASK_MODE_MANUAL;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendAuto()
{
    EMC_TASK_SET_MODE mode_msg;

    if (emcStatus->task.mode == EMC_TASK_MODE_AUTO) {
        return 0;
    }

    mode_msg.mode = EMC_TASK_MODE_AUTO;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendMdi()
{
    EMC_TASK_SET_MODE mode_msg;

    if (emcStatus->task.mode == EMC_TASK_MODE_MDI) {
        return 0;
    }

    mode_msg.mode = EMC_TASK_MODE_MDI;
    mode_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(mode_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

int sendMdiCmd(char *mdi)
{
    EMC_TASK_PLAN_EXECUTE emc_task_plan_execute_msg;

    if (emcStatus->task.mode != EMC_TASK_MODE_MDI) {
	halui_old_mode = emcStatus->task.mode;
	sendMdi();
    }
    strcpy(emc_task_plan_execute_msg.command, mdi);
    emc_task_plan_execute_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_execute_msg);
    halui_sent_mdi = 1;
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendMdiCommand(int n)
{
    int r1,r2;
    halui_old_mode = emcStatus->task.mode;
    r1 = sendMdi();
    r2 = sendMdiCmd(mdi_commands[n]);
    return r1 || r2;
}


static int sendTeleop()
{
    EMC_TRAJ_SET_TELEOP_ENABLE emc_set_teleop_enable_msg;

    emc_set_teleop_enable_msg.enable = 1;
    emc_set_teleop_enable_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_set_teleop_enable_msg);
    return emcCommandWaitDone(emcCommandSerialNumber);
}

static int sendJoint()
{
    EMC_TRAJ_SET_TELEOP_ENABLE emc_set_teleop_enable_msg;

    emc_set_teleop_enable_msg.enable = 0;
    emc_set_teleop_enable_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_set_teleop_enable_msg);
    return emcCommandWaitDone(emcCommandSerialNumber);
}

static int sendMistOn()
{
    EMC_COOLANT_MIST_ON emc_coolant_mist_on_msg;

    emc_coolant_mist_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_mist_on_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendMistOff()
{
    EMC_COOLANT_MIST_OFF emc_coolant_mist_off_msg;

    emc_coolant_mist_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_mist_off_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendFloodOn()
{
    EMC_COOLANT_FLOOD_ON emc_coolant_flood_on_msg;

    emc_coolant_flood_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_flood_on_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendFloodOff()
{
    EMC_COOLANT_FLOOD_OFF emc_coolant_flood_off_msg;

    emc_coolant_flood_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_coolant_flood_off_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendLubeOn()
{
    EMC_LUBE_ON emc_lube_on_msg;

    emc_lube_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_lube_on_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendLubeOff()
{
    EMC_LUBE_OFF emc_lube_off_msg;

    emc_lube_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_lube_off_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
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
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendProgramPause()
{
    EMC_TASK_PLAN_PAUSE emc_task_plan_pause_msg;

    emc_task_plan_pause_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_pause_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSetOptionalStop(bool state)
{
    EMC_TASK_PLAN_SET_OPTIONAL_STOP emc_task_plan_set_optional_stop_msg;

    emc_task_plan_set_optional_stop_msg.state = state;
    emc_task_plan_set_optional_stop_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_set_optional_stop_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSetBlockDelete(bool state)
{
    EMC_TASK_PLAN_SET_BLOCK_DELETE emc_task_plan_set_block_delete_msg;

    emc_task_plan_set_block_delete_msg.state = state;
    emc_task_plan_set_block_delete_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_set_block_delete_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}


static int sendProgramResume()
{
    EMC_TASK_PLAN_RESUME emc_task_plan_resume_msg;

    emc_task_plan_resume_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_resume_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendProgramStep()
{
    EMC_TASK_PLAN_STEP emc_task_plan_step_msg;

    emc_task_plan_step_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_task_plan_step_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSpindleForward()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed = rtapi_fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = +1;
    }
    emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_on_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSpindleReverse()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed =
	    -1 * rtapi_fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = -1;
    }
    emc_spindle_on_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_on_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSpindleOff()
{
    EMC_SPINDLE_OFF emc_spindle_off_msg;

    emc_spindle_off_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_off_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSpindleIncrease()
{
    EMC_SPINDLE_INCREASE emc_spindle_increase_msg;

    emc_spindle_increase_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_increase_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSpindleDecrease()
{
    EMC_SPINDLE_DECREASE emc_spindle_decrease_msg;

    emc_spindle_decrease_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_decrease_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendSpindleConstant()
{
    EMC_SPINDLE_CONSTANT emc_spindle_constant_msg;

    emc_spindle_constant_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_constant_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendBrakeEngage()
{
    EMC_SPINDLE_BRAKE_ENGAGE emc_spindle_brake_engage_msg;

    emc_spindle_brake_engage_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_brake_engage_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendBrakeRelease()
{
    EMC_SPINDLE_BRAKE_RELEASE emc_spindle_brake_release_msg;

    emc_spindle_brake_release_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(emc_spindle_brake_release_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendHome(int axis)
{
    EMC_AXIS_HOME emc_axis_home_msg;

    emc_axis_home_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_home_msg.axis = axis;
    emcCommandBuffer->write(emc_axis_home_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendUnhome(int axis)
{
    EMC_AXIS_UNHOME emc_axis_unhome_msg;

    emc_axis_unhome_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_unhome_msg.axis = axis;
    emcCommandBuffer->write(emc_axis_unhome_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendAbort()
{
    EMC_TASK_ABORT task_abort_msg;

    task_abort_msg.serial_number = ++emcCommandSerialNumber;
    emcCommandBuffer->write(task_abort_msg);
    return emcCommandWaitReceived(emcCommandSerialNumber);
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

        return emcCommandWaitReceived(emcCommandSerialNumber);
    } else {
	emc_set_teleop_vector.serial_number = ++emcCommandSerialNumber;
        ZERO_EMC_POSE(emc_set_teleop_vector.vector);
	emcCommandBuffer->write(emc_set_teleop_vector);

        return emcCommandWaitReceived(emcCommandSerialNumber);
    }
}

static int sendJogCont(int axis, double speed)
{
    EMC_AXIS_JOG emc_axis_jog_msg;
    EMC_TRAJ_SET_TELEOP_VECTOR emc_set_teleop_vector;

    if (emcStatus->task.state != EMC_TASK_STATE_ON) {
	return -1;
    }

    if (axis < 0 || axis >= EMC_AXIS_MAX) {
	return -1;
    }

    sendManual();

    if (emcStatus->motion.traj.mode != EMC_TRAJ_MODE_TELEOP) {
	emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
	emc_axis_jog_msg.axis = axis;
	emc_axis_jog_msg.vel = speed / 60.0;
	emcCommandBuffer->write(emc_axis_jog_msg);
    } else {
	emc_set_teleop_vector.serial_number = ++emcCommandSerialNumber;
        ZERO_EMC_POSE(emc_set_teleop_vector.vector);

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


static int sendJogInc(int axis, double speed, double inc)
{
    EMC_AXIS_INCR_JOG emc_axis_jog_msg;

    if (emcStatus->task.state != EMC_TASK_STATE_ON) {
	return -1;
    }

    if (axis < 0 || axis >= EMC_AXIS_MAX)
	return -1;

    sendManual();

    if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE_TELEOP)
    	return -1;

    emc_axis_jog_msg.serial_number = ++emcCommandSerialNumber;
    emc_axis_jog_msg.axis = axis;
    emc_axis_jog_msg.vel = speed / 60.0;
    emc_axis_jog_msg.incr = inc;
    emcCommandBuffer->write(emc_axis_jog_msg);

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
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

static int sendMaxVelocity(double velocity)
{
    EMC_TRAJ_SET_MAX_VELOCITY mv;

    if (velocity < 0.0) {
        velocity = 0.0;
    }

    if (velocity > maxMaxVelocity) {
        velocity = maxMaxVelocity;
    }

    mv.serial_number = ++emcCommandSerialNumber;
    mv.velocity = velocity;
    emcCommandBuffer->write(mv);
    return emcCommandWaitReceived(emcCommandSerialNumber);
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
    return emcCommandWaitReceived(emcCommandSerialNumber);
}

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
	if (1 != sscanf(inistring, "%i", &emc_debug)) {
	    emc_debug = 0;
	}
    } else {
	// not found, use default
	emc_debug = 0;
    }

    if (NULL != (inistring = inifile.Find("NML_FILE", "EMC"))) {
	// copy to global
	strcpy(emc_nmlfile, inistring);
    } else {
	// not found, use default
    }

    if (NULL != (inistring = inifile.Find("MAX_FEED_OVERRIDE", "DISPLAY"))) {
	if (1 == sscanf(inistring, "%lf", &d) && d > 0.0) {
	    maxFeedOverride =  d;
	}
    }

    if(inifile.Find(&maxMaxVelocity, "MAX_VELOCITY", "TRAJ") &&
       inifile.Find(&maxMaxVelocity, "MAX_VELOCITY", "AXIS_0"))
        maxMaxVelocity = 1.0;

    if (NULL != (inistring = inifile.Find("MIN_SPINDLE_OVERRIDE", "DISPLAY"))) {
	if (1 == sscanf(inistring, "%lf", &d) && d > 0.0) {
	    minSpindleOverride =  d;
	}
    }

    if (NULL != (inistring = inifile.Find("MAX_SPINDLE_OVERRIDE", "DISPLAY"))) {
	if (1 == sscanf(inistring, "%lf", &d) && d > 0.0) {
	    maxSpindleOverride =  d;
	}
    }
    
    if (NULL != (inistring = inifile.Find("AXES", "TRAJ"))) {
	if (1 == sscanf(inistring, "%d", &i) && i > 0) {
	    num_axes =  i;
	}
    }

    if (NULL != inifile.Find("HOME_SEQUENCE", "AXIS_0")) {
        have_home_all = 1;
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
    }

    const char *mc;
    while(num_mdi_commands < MDI_MAX && (mc = inifile.Find("MDI_COMMAND", "HALUI", num_mdi_commands+1))) {
        mdi_commands[num_mdi_commands++] = strdup(mc);
    }

    // close it
    inifile.Close();

    return 0;
}

static void hal_init_pins()
{
    int joint;

    *(halui_data->machine_on) = old_halui_data.machine_on = 0;
    *(halui_data->machine_off) = old_halui_data.machine_off = 0;

    *(halui_data->estop_activate) = old_halui_data.estop_activate = 0;
    *(halui_data->estop_reset) = old_halui_data.estop_reset = 0;

    
    for (joint=0; joint < num_axes; joint++) {
	*(halui_data->joint_home[joint]) = old_halui_data.joint_home[joint] = 0;
	*(halui_data->joint_unhome[joint]) = old_halui_data.joint_unhome[joint] = 0;
	*(halui_data->joint_nr_select[joint]) = old_halui_data.joint_nr_select[joint] = 0;
	*(halui_data->jog_minus[joint]) = old_halui_data.jog_minus[joint] = 0;
	*(halui_data->jog_plus[joint]) = old_halui_data.jog_plus[joint] = 0;
	*(halui_data->jog_analog[joint]) = old_halui_data.jog_analog[joint] = 0;
	*(halui_data->jog_increment[joint]) = old_halui_data.jog_increment[joint] = 0.0;
	*(halui_data->jog_increment_plus[joint]) = old_halui_data.jog_increment_plus[joint] = 0;
	*(halui_data->jog_increment_minus[joint]) = old_halui_data.jog_increment_minus[joint] = 0;
    }

    *(halui_data->joint_home[num_axes]) = old_halui_data.joint_home[num_axes] = 0;
    *(halui_data->jog_minus[num_axes]) = old_halui_data.jog_minus[num_axes] = 0;
    *(halui_data->jog_plus[num_axes]) = old_halui_data.jog_plus[num_axes] = 0;
    *(halui_data->jog_increment[num_axes]) = old_halui_data.jog_increment[num_axes] = 0.0;
    *(halui_data->jog_increment_plus[num_axes]) = old_halui_data.jog_increment_plus[num_axes] = 0;
    *(halui_data->jog_increment_minus[num_axes]) = old_halui_data.jog_increment_minus[num_axes] = 0;
    *(halui_data->jog_deadband) = 0.2;
    *(halui_data->jog_speed) = 0;

    *(halui_data->joint_selected) = 0; // select joint 0 by default
    
    *(halui_data->fo_scale) = old_halui_data.fo_scale = 0.1; //sane default
    *(halui_data->so_scale) = old_halui_data.so_scale = 0.1; //sane default
}

static int check_bit_changed(bool halpin, bool &newpin)
{
    if (halpin != newpin) {
	newpin = halpin;
	return halpin;
    }
    return 0;
}

static void copy_hal_data(const halui_str &i, local_halui_str &j)
{
    int x;
#define FIELD(t,f) j.f = (i.f)?*i.f:0;
#define ARRAY(t,f,n) do { for (x = 0; x < n; x++) j.f[x] = (i.f[x])?*i.f[x]:0; } while (0);
    HAL_FIELDS
#undef FIELD
#undef ARRAY
}

// this function looks if any of the hal pins has changed
// and sends appropiate messages if so
static void check_hal_changes()
{
    hal_s32_t counts;
    int select_changed, joint;
    hal_bit_t bit;
    int js;
    hal_float_t floatt;
    int jog_speed_changed;

    local_halui_str new_halui_data_mutable;
    copy_hal_data(*halui_data, new_halui_data_mutable);
    const local_halui_str &new_halui_data = new_halui_data_mutable;


    //check if machine_on pin has changed (the rest work exactly the same)
    if (check_bit_changed(new_halui_data.machine_on, old_halui_data.machine_on) != 0)
	sendMachineOn();                //send MachineOn NML command
    
    if (check_bit_changed(new_halui_data.machine_off, old_halui_data.machine_off) != 0)
	sendMachineOff();

    if (check_bit_changed(new_halui_data.estop_activate, old_halui_data.estop_activate) != 0)
	sendEstop();

    if (check_bit_changed(new_halui_data.estop_reset, old_halui_data.estop_reset) != 0)
	sendEstopReset();

    if (check_bit_changed(new_halui_data.mode_manual, old_halui_data.mode_manual) != 0)
	sendManual();

    if (check_bit_changed(new_halui_data.mode_auto, old_halui_data.mode_auto) != 0)
	sendAuto();

    if (check_bit_changed(new_halui_data.mode_mdi, old_halui_data.mode_mdi) != 0)
	sendMdi();

    if (check_bit_changed(new_halui_data.mode_teleop, old_halui_data.mode_teleop) != 0)
	sendTeleop();

    if (check_bit_changed(new_halui_data.mode_joint, old_halui_data.mode_joint) != 0)
	sendJoint();

    if (check_bit_changed(new_halui_data.mist_on, old_halui_data.mist_on) != 0)
	sendMistOn();

    if (check_bit_changed(new_halui_data.mist_off, old_halui_data.mist_off) != 0)
	sendMistOff();

    if (check_bit_changed(new_halui_data.flood_on, old_halui_data.flood_on) != 0)
	sendFloodOn();

    if (check_bit_changed(new_halui_data.flood_off, old_halui_data.flood_off) != 0)
	sendFloodOff();

    if (check_bit_changed(new_halui_data.lube_on, old_halui_data.lube_on) != 0)
	sendLubeOn();

    if (check_bit_changed(new_halui_data.lube_off, old_halui_data.lube_off) != 0)
	sendLubeOff();

    if (check_bit_changed(new_halui_data.program_run, old_halui_data.program_run) != 0)
	sendProgramRun(0);

    if (check_bit_changed(new_halui_data.program_pause, old_halui_data.program_pause) != 0)
	sendProgramPause();

    if (check_bit_changed(new_halui_data.program_os_on, old_halui_data.program_os_on) != 0)
	sendSetOptionalStop(ON);

    if (check_bit_changed(new_halui_data.program_os_off, old_halui_data.program_os_off) != 0)
	sendSetOptionalStop(OFF);

    if (check_bit_changed(new_halui_data.program_bd_on, old_halui_data.program_bd_on) != 0)
	sendSetBlockDelete(ON);

    if (check_bit_changed(new_halui_data.program_bd_off, old_halui_data.program_bd_off) != 0)
	sendSetBlockDelete(OFF);

    if (check_bit_changed(new_halui_data.program_resume, old_halui_data.program_resume) != 0)
	sendProgramResume();

    if (check_bit_changed(new_halui_data.program_step, old_halui_data.program_step) != 0)
	sendProgramStep();

    if (check_bit_changed(new_halui_data.program_stop, old_halui_data.program_stop) != 0)
	sendAbort();

    //max-velocity stuff
    counts = new_halui_data.mv_counts;
    if (counts != old_halui_data.mv_counts) {
        if (new_halui_data.mv_count_enable) {
            if (new_halui_data.mv_direct_value) {
                sendMaxVelocity(counts * new_halui_data.mv_scale);
            } else {
                sendMaxVelocity( new_halui_data.mv_value + (counts - old_halui_data.mv_counts) *
                    new_halui_data.mv_scale);
            }
        }
        old_halui_data.mv_counts = counts;
    }

    //feed-override stuff
    counts = new_halui_data.fo_counts;
    if (counts != old_halui_data.fo_counts) {
        if (new_halui_data.fo_count_enable) {
            if (new_halui_data.fo_direct_value) {
                sendFeedOverride(counts * new_halui_data.fo_scale);
            } else {
                sendFeedOverride( new_halui_data.fo_value + (counts - old_halui_data.fo_counts) *
                    new_halui_data.fo_scale);
            }
        }
        old_halui_data.fo_counts = counts;
    }

    //spindle-override stuff
    counts = new_halui_data.so_counts;
    if (counts != old_halui_data.so_counts) {
        if (new_halui_data.so_count_enable) {
            if (new_halui_data.so_direct_value) {
                sendSpindleOverride(counts * new_halui_data.so_scale);
            } else {
                sendSpindleOverride( new_halui_data.so_value + (counts - old_halui_data.so_counts) *
                    new_halui_data.so_scale);
            }
        }
        old_halui_data.so_counts = counts;
    }

    if (check_bit_changed(new_halui_data.mv_increase, old_halui_data.mv_increase) != 0)
        sendMaxVelocity(new_halui_data.mv_value + new_halui_data.mv_scale);
    if (check_bit_changed(new_halui_data.mv_decrease, old_halui_data.mv_decrease) != 0)
        sendMaxVelocity(new_halui_data.mv_value - new_halui_data.mv_scale);

    if (check_bit_changed(new_halui_data.fo_increase, old_halui_data.fo_increase) != 0)
        sendFeedOverride(new_halui_data.fo_value + new_halui_data.fo_scale);
    if (check_bit_changed(new_halui_data.fo_decrease, old_halui_data.fo_decrease) != 0)
        sendFeedOverride(new_halui_data.fo_value - new_halui_data.fo_scale);

    if (check_bit_changed(new_halui_data.so_increase, old_halui_data.so_increase) != 0)
        sendSpindleOverride(new_halui_data.so_value + new_halui_data.so_scale);
    if (check_bit_changed(new_halui_data.so_decrease, old_halui_data.so_decrease) != 0)
        sendSpindleOverride(new_halui_data.so_value - new_halui_data.so_scale);

//spindle stuff
    if (check_bit_changed(new_halui_data.spindle_start, old_halui_data.spindle_start) != 0)
	sendSpindleForward();

    if (check_bit_changed(new_halui_data.spindle_stop, old_halui_data.spindle_stop) != 0)
	sendSpindleOff();

    if (check_bit_changed(new_halui_data.spindle_forward, old_halui_data.spindle_forward) != 0)
	sendSpindleForward();

    if (check_bit_changed(new_halui_data.spindle_reverse, old_halui_data.spindle_reverse) != 0)
	sendSpindleReverse();

    bit = new_halui_data.spindle_increase;
    if (bit != old_halui_data.spindle_increase) {
	if (bit != 0)
	    sendSpindleIncrease();
	if (bit == 0)
	    sendSpindleConstant();
	old_halui_data.spindle_increase = bit;
    }

    bit = new_halui_data.spindle_decrease;
    if (bit != old_halui_data.spindle_decrease) {
	if (bit != 0)
	    sendSpindleDecrease();
	if (bit == 0)
	    sendSpindleConstant();
	old_halui_data.spindle_decrease = bit;
    }

    if (check_bit_changed(new_halui_data.spindle_brake_on, old_halui_data.spindle_brake_on) != 0)
	sendBrakeEngage();

    if (check_bit_changed(new_halui_data.spindle_brake_off, old_halui_data.spindle_brake_off) != 0)
	sendBrakeRelease();
    
    if (check_bit_changed(new_halui_data.abort, old_halui_data.abort) != 0)
	sendAbort();
    
    if (check_bit_changed(new_halui_data.home_all, old_halui_data.home_all) != 0)
	sendHome(-1);

// joint stuff (selection, homing..)
    select_changed = -1; // flag to see if the selected joint changed

    // if the jog-speed changes while in a continuous jog, we want to
    // re-start the jog with the new speed
    if (rtapi_fabs(old_halui_data.jog_speed - new_halui_data.jog_speed) > 0.00001) {
        old_halui_data.jog_speed = new_halui_data.jog_speed;
        jog_speed_changed = 1;
    } else {
        jog_speed_changed = 0;
    }

    
    for (joint=0; joint < num_axes; joint++) {
	if (check_bit_changed(new_halui_data.joint_home[joint], old_halui_data.joint_home[joint]) != 0)
	    sendHome(joint);

	if (check_bit_changed(new_halui_data.joint_unhome[joint], old_halui_data.joint_unhome[joint]) != 0)
	    sendUnhome(joint);

	bit = new_halui_data.jog_minus[joint];
	if ((bit != old_halui_data.jog_minus[joint]) || (bit && jog_speed_changed)) {
	    if (bit != 0)
		sendJogCont(joint,-new_halui_data.jog_speed);
	    else
		sendJogStop(joint);
	    old_halui_data.jog_minus[joint] = bit;
	}

	bit = new_halui_data.jog_plus[joint];
	if ((bit != old_halui_data.jog_plus[joint]) || (bit && jog_speed_changed)) {
	    if (bit != 0)
		sendJogCont(joint,new_halui_data.jog_speed);
	    else
		sendJogStop(joint);
	    old_halui_data.jog_plus[joint] = bit;
	}

	floatt = new_halui_data.jog_analog[joint];
	bit = (rtapi_fabs(floatt) > new_halui_data.jog_deadband);
	if ((floatt != old_halui_data.jog_analog[joint]) || (bit && jog_speed_changed)) {
	    if (bit)
		sendJogCont(joint,(new_halui_data.jog_speed) * (new_halui_data.jog_analog[joint]));
	    else
		sendJogStop(joint);
	    old_halui_data.jog_analog[joint] = floatt;
	}

	bit = new_halui_data.jog_increment_plus[joint];
	if (bit != old_halui_data.jog_increment_plus[joint]) {
	    if (bit)
		sendJogInc(joint, new_halui_data.jog_speed, new_halui_data.jog_increment[joint]);
	    old_halui_data.jog_increment_plus[joint] = bit;
	}

	bit = new_halui_data.jog_increment_minus[joint];
	if (bit != old_halui_data.jog_increment_minus[joint]) {
	    if (bit)
		sendJogInc(joint, new_halui_data.jog_speed, -(new_halui_data.jog_increment[joint]));
	    old_halui_data.jog_increment_minus[joint] = bit;
	}

	// check to see if another joint has been selected
	bit = new_halui_data.joint_nr_select[joint];
	if (bit != old_halui_data.joint_nr_select[joint]) {
	    if (bit != 0) {
		*halui_data->joint_selected = joint;
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

    if (check_bit_changed(new_halui_data.joint_home[num_axes], old_halui_data.joint_home[num_axes]) != 0)
	sendHome(new_halui_data.joint_selected);

    if (check_bit_changed(new_halui_data.joint_unhome[num_axes], old_halui_data.joint_unhome[num_axes]) != 0)
	sendUnhome(new_halui_data.joint_selected);

    bit = new_halui_data.jog_minus[num_axes];
    js = new_halui_data.joint_selected;
    if ((bit != old_halui_data.jog_minus[num_axes]) || (bit && jog_speed_changed)) {
        if (bit != 0)
	    sendJogCont(js, -new_halui_data.jog_speed);
	else
	    sendJogStop(js);
	old_halui_data.jog_minus[num_axes] = bit;
    }

    bit = new_halui_data.jog_plus[num_axes];
    js = new_halui_data.joint_selected;
    if ((bit != old_halui_data.jog_plus[num_axes]) || (bit && jog_speed_changed)) {
        if (bit != 0)
	    sendJogCont(js,new_halui_data.jog_speed);
	else
	    sendJogStop(js);
	old_halui_data.jog_plus[num_axes] = bit;
    }

    bit = new_halui_data.jog_increment_plus[num_axes];
    js = new_halui_data.joint_selected;
    if (bit != old_halui_data.jog_increment_plus[num_axes]) {
	if (bit)
	    sendJogInc(js, new_halui_data.jog_speed, new_halui_data.jog_increment[num_axes]);
	old_halui_data.jog_increment_plus[num_axes] = bit;
    }

    bit = new_halui_data.jog_increment_minus[num_axes];
    js = new_halui_data.joint_selected;
    if (bit != old_halui_data.jog_increment_minus[num_axes]) {
	if (bit)
	    sendJogInc(js, new_halui_data.jog_speed, -(new_halui_data.jog_increment[num_axes]));
	old_halui_data.jog_increment_minus[num_axes] = bit;
    }

    for(int n = 0; n < num_mdi_commands; n++) {
        if (check_bit_changed(new_halui_data.mdi_commands[n], old_halui_data.mdi_commands[n]) != 0)
            sendMdiCommand(n);
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

    if (halui_sent_mdi) { // we have an ongoing MDI command
	if (emcStatus->status == 1) { //which seems to have finished
	    halui_sent_mdi = 0;
	    switch (halui_old_mode) {
		case EMC_TASK_MODE_MANUAL: sendManual();break;
		case EMC_TASK_MODE_MDI: break;
		case EMC_TASK_MODE_AUTO: sendAuto();break;
		default: sendManual();break;
	    }
	}
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

    if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE_TELEOP) {
	*(halui_data->mode_is_teleop)=1;
    } else {
	*(halui_data->mode_is_teleop)=0;
    }

    if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE_TELEOP) {
	*(halui_data->mode_is_joint)=0;
    } else {
	*(halui_data->mode_is_joint)=1;
    }

    *(halui_data->program_is_paused) = emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED;
    *(halui_data->program_is_running) = emcStatus->task.interpState == EMC_TASK_INTERP_READING || 
                                        emcStatus->task.interpState == EMC_TASK_INTERP_WAITING;
    *(halui_data->program_is_idle) = emcStatus->task.interpState == EMC_TASK_INTERP_IDLE;
    *(halui_data->program_os_is_on) = emcStatus->task.optional_stop_state;
    *(halui_data->program_bd_is_on) = emcStatus->task.block_delete_state;

    *(halui_data->mv_value) = emcStatus->motion.traj.maxVelocity;
    *(halui_data->fo_value) = emcStatus->motion.traj.scale; //feedoverride from 0 to 1 for 100%
    *(halui_data->so_value) = emcStatus->motion.traj.spindle_scale; //spindle-speed-override from 0 to 1 for 100%

    *(halui_data->mist_is_on) = emcStatus->io.coolant.mist;
    *(halui_data->flood_is_on) = emcStatus->io.coolant.flood;
    *(halui_data->lube_is_on) = emcStatus->io.lube.on;

    *(halui_data->tool_number) = emcStatus->io.tool.toolInSpindle;
    *(halui_data->tool_length_offset_x) = emcStatus->task.toolOffset.tran.x;
    *(halui_data->tool_length_offset_y) = emcStatus->task.toolOffset.tran.y;
    *(halui_data->tool_length_offset_z) = emcStatus->task.toolOffset.tran.z;
    *(halui_data->tool_length_offset_a) = emcStatus->task.toolOffset.a;
    *(halui_data->tool_length_offset_b) = emcStatus->task.toolOffset.b;
    *(halui_data->tool_length_offset_c) = emcStatus->task.toolOffset.c;
    *(halui_data->tool_length_offset_u) = emcStatus->task.toolOffset.u;
    *(halui_data->tool_length_offset_v) = emcStatus->task.toolOffset.v;
    *(halui_data->tool_length_offset_w) = emcStatus->task.toolOffset.w;

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

    *(halui_data->axis_pos_commanded[0]) = emcStatus->motion.traj.position.tran.x;	
    *(halui_data->axis_pos_commanded[1]) = emcStatus->motion.traj.position.tran.y;	
    *(halui_data->axis_pos_commanded[2]) = emcStatus->motion.traj.position.tran.z;
    *(halui_data->axis_pos_commanded[3]) = emcStatus->motion.traj.position.a;
    *(halui_data->axis_pos_commanded[4]) = emcStatus->motion.traj.position.b;
    *(halui_data->axis_pos_commanded[5]) = emcStatus->motion.traj.position.c;
    *(halui_data->axis_pos_commanded[6]) = emcStatus->motion.traj.position.u;
    *(halui_data->axis_pos_commanded[7]) = emcStatus->motion.traj.position.v;
    *(halui_data->axis_pos_commanded[8]) = emcStatus->motion.traj.position.w;
    *(halui_data->axis_pos_feedback[0]) = emcStatus->motion.traj.actualPosition.tran.x;	
    *(halui_data->axis_pos_feedback[1]) = emcStatus->motion.traj.actualPosition.tran.y;	
    *(halui_data->axis_pos_feedback[2]) = emcStatus->motion.traj.actualPosition.tran.z;
    *(halui_data->axis_pos_feedback[3]) = emcStatus->motion.traj.actualPosition.a;
    *(halui_data->axis_pos_feedback[4]) = emcStatus->motion.traj.actualPosition.b;
    *(halui_data->axis_pos_feedback[5]) = emcStatus->motion.traj.actualPosition.c;
    *(halui_data->axis_pos_feedback[6]) = emcStatus->motion.traj.actualPosition.u;
    *(halui_data->axis_pos_feedback[7]) = emcStatus->motion.traj.actualPosition.v;
    *(halui_data->axis_pos_feedback[8]) = emcStatus->motion.traj.actualPosition.w;
    *(halui_data->axis_pos_relative[0]) = emcStatus->motion.traj.actualPosition.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.g92_offset.tran.x - emcStatus->task.toolOffset.tran.x;
    *(halui_data->axis_pos_relative[1]) = emcStatus->motion.traj.actualPosition.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.g92_offset.tran.y - emcStatus->task.toolOffset.tran.y;
    *(halui_data->axis_pos_relative[2]) = emcStatus->motion.traj.actualPosition.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z - emcStatus->task.toolOffset.tran.z;
    *(halui_data->axis_pos_relative[3]) = emcStatus->motion.traj.actualPosition.a - emcStatus->task.g5x_offset.a - emcStatus->task.g92_offset.a - emcStatus->task.toolOffset.a;
    *(halui_data->axis_pos_relative[4]) = emcStatus->motion.traj.actualPosition.b - emcStatus->task.g5x_offset.b - emcStatus->task.g92_offset.b - emcStatus->task.toolOffset.b;
    *(halui_data->axis_pos_relative[5]) = emcStatus->motion.traj.actualPosition.c - emcStatus->task.g5x_offset.c - emcStatus->task.g92_offset.c - emcStatus->task.toolOffset.c;
    *(halui_data->axis_pos_relative[6]) = emcStatus->motion.traj.actualPosition.u - emcStatus->task.g5x_offset.u - emcStatus->task.g92_offset.u - emcStatus->task.toolOffset.u;
    *(halui_data->axis_pos_relative[7]) = emcStatus->motion.traj.actualPosition.v - emcStatus->task.g5x_offset.v - emcStatus->task.g92_offset.v - emcStatus->task.toolOffset.v;
    *(halui_data->axis_pos_relative[8]) = emcStatus->motion.traj.actualPosition.w - emcStatus->task.g5x_offset.w - emcStatus->task.g92_offset.w - emcStatus->task.toolOffset.w;

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
    if (0 != iniLoad(emc_inifile)) {
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
    }
    thisQuit();
    return 0;
}
