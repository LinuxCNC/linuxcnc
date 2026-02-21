/********************************************************************
* Description: shcom.cc
*   Common functions for NML calls
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Further derived from work by jmkasunich, Alex Joni
*
* Author: Eric H. Johnson
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
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

#include "emc/linuxcnc.h"
#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "nml_oi.hh"            // nmlErrorFormat, NML_ERROR, etc
#include "rcs_print.hh"
#include "timer.hh"             // esleep
#include "shcom.hh"             // Common NML communications functions
#include <rtapi_string.h>

LINEAR_UNIT_CONVERSION linearUnitConversion = LINEAR_UNITS_AUTO;
ANGULAR_UNIT_CONVERSION angularUnitConversion = ANGULAR_UNITS_AUTO;

static int num_joints = EMCMOT_MAX_JOINTS;

int emcCommandSerialNumber;

// the NML channels to the EMC task
RCS_CMD_CHANNEL *emcCommandBuffer;
RCS_STAT_CHANNEL *emcStatusBuffer;
EMC_STAT *emcStatus;

// the NML channel for errors
NML *emcErrorBuffer;
char error_string[NML_ERROR_LEN];
char operator_text_string[NML_TEXT_LEN];
char operator_display_string[NML_DISPLAY_LEN];
char defaultPath[80] = DEFAULT_PATH;
// default value for timeout, 0 means wait forever
double emcTimeout;
int programStartLine;

EMC_UPDATE_TYPE emcUpdateType;
EMC_WAIT_TYPE emcWaitType;

void strupr(char *s)
{
    while(*s) {
        *s = toupper((unsigned char)*s);
         s++;
    }
}

int emcTaskNmlGet()
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
	if (!emcStatusBuffer->valid()
	    || EMC_STAT_TYPE != emcStatusBuffer->peek()) {
	    delete emcStatusBuffer;
	    emcStatusBuffer = 0;
	    emcStatus = 0;
	    retval = -1;
	} else {
	    emcStatus = static_cast<EMC_STAT *>(emcStatusBuffer->get_address());
	}
    }

    return retval;
}

int emcErrorNmlGet()
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

int tryNml(double retry_time, double retry_interval)
{
    double end;
    int good;

    end = retry_time;
    good = 0;
    do {
        if (0 == emcTaskNmlGet()) {
            good = 1;
            break;
        }
        esleep(retry_interval);
        end -= retry_interval;
    } while (end > 0.0);

    if (!good) {
	    return -1;
    }

    end = retry_time;
    good = 0;
    do {
        if (0 == emcErrorNmlGet()) {
            good = 1;
            break;
        }
        esleep(retry_interval);
        end -= retry_interval;
    } while (end > 0.0);

    if (!good) {
	    return -1;
    }

    return 0;
}

int updateStatus()
{
    if (!emcStatus || !emcStatusBuffer || !emcStatusBuffer->valid()) {
        return -1;
    }

    switch (emcStatusBuffer->peek()) {
    case -1: // error on CMS channel
    default:
        return -1;

    case 0:             // no new data
    case EMC_STAT_TYPE: // new data
        return 0;
    }

}

/*
  updateError() updates "errors," which are true errors and also
  operator display and text messages.
*/
int updateError()
{
    NMLTYPE type;

    if (!emcErrorBuffer || !emcErrorBuffer->valid()) {
	return -1;
    }

    switch (type = emcErrorBuffer->read()) {
    case -1:
	// error reading channel
	return -1;

    default:
	// if not recognized, set the error string
	snprintf(error_string, sizeof(error_string), "shcom:updateError(): unrecognized error %d", (int)type);
	return 0;

    case 0:
	// nothing new
	break;

    case EMC_OPERATOR_ERROR_TYPE:
	strncpy(error_string,
		(static_cast<EMC_OPERATOR_ERROR *>((emcErrorBuffer->get_address()))->error),
                LINELEN - 1);
	error_string[NML_ERROR_LEN - 1] = 0;
	break;

    case EMC_OPERATOR_TEXT_TYPE:
	strncpy(operator_text_string,
		(static_cast<EMC_OPERATOR_TEXT *>((emcErrorBuffer->get_address()))->text),
                LINELEN - 1);
	operator_text_string[NML_TEXT_LEN - 1] = 0;
	break;

    case EMC_OPERATOR_DISPLAY_TYPE:
	strncpy(operator_display_string,
		(static_cast<EMC_OPERATOR_DISPLAY *>((emcErrorBuffer->get_address()))->display),
		LINELEN - 1);
	operator_display_string[NML_DISPLAY_LEN - 1] = 0;
	break;

    case NML_ERROR_TYPE:
	strncpy(error_string,
		(static_cast<NML_ERROR *>((emcErrorBuffer->get_address()))->error),
		NML_ERROR_LEN - 1);
	error_string[NML_ERROR_LEN - 1] = 0;
	break;

    case NML_TEXT_TYPE:
	strncpy(operator_text_string,
		(static_cast<NML_TEXT *>((emcErrorBuffer->get_address()))->text),
		NML_TEXT_LEN - 1);
	operator_text_string[NML_TEXT_LEN - 1] = 0;
	break;

    case NML_DISPLAY_TYPE:
	strncpy(operator_display_string,
		(static_cast<NML_DISPLAY *>((emcErrorBuffer->get_address()))->display),
		NML_DISPLAY_LEN - 1);
	operator_display_string[NML_DISPLAY_LEN - 1] = 0;
	break;
    }

    return 0;
}

// How long to start to sleep between checks.
// It uses a progressive back-off strategy when it takes longer. This makes
// fast commands faster and slow commands use fewer system resources.
#define EMC_COMMAND_DELAY      0.001
#define EMC_COMMAND_FACTOR_MAX 100

int emcCommandWaitDone()
{
    double end;
    int factor = 1;
    for (end = 0.0; emcTimeout <= 0.0 || end < emcTimeout; end += EMC_COMMAND_DELAY * factor) {
        updateStatus();
        int serial_diff = emcStatus->echo_serial_number - emcCommandSerialNumber;

        if (serial_diff > 0) { // We've past beyond our command
            return 0;
        }

        if (!serial_diff) { // We're at our command
            switch (emcStatus->status) {
            case RCS_STATUS::EXEC: // Still busy executing command
                break;

            case RCS_STATUS::DONE:
                return 0;

            case RCS_STATUS::ERROR: // The command failed
                return -1;

            default: // Default should never happen...
                fprintf(stderr, "shcom.cc: emcCommandWaitDone(): unknown emcStatus->status=%d\n", (int)emcStatus->status);
                return -1;
            }
        }
        esleep(EMC_COMMAND_DELAY * factor);

        // Progressive backoff until max
        factor *= 2;
        if (factor > EMC_COMMAND_FACTOR_MAX) {
            factor = EMC_COMMAND_FACTOR_MAX;
        }
    }

    return -1;
}

int emcCommandWaitReceived()
{
    double end;
    int factor = 1;
    for (end = 0.0; emcTimeout <= 0.0 || end < emcTimeout; end += EMC_COMMAND_DELAY * factor) {
	updateStatus();

	int serial_diff = emcStatus->echo_serial_number - emcCommandSerialNumber;
	if (serial_diff >= 0) {
	    return 0;
	}

	esleep(EMC_COMMAND_DELAY * factor);

        // Progressive backoff until max
        factor *= 2;
        if (factor > EMC_COMMAND_FACTOR_MAX) {
            factor = EMC_COMMAND_FACTOR_MAX;
        }
    }

    return -1;
}

int emcCommandSend(RCS_CMD_MSG & cmd)
{
    // write command
    if (emcCommandBuffer->write(&cmd)) {
        return -1;
    }
    emcCommandSerialNumber = cmd.serial_number;
    return 0;
}

static inline int emcSendCommandAndWait(RCS_CMD_MSG& cmd)
{
    if (emcCommandSend(cmd))
        return -1;
    if (emcWaitType == EMC_WAIT_RECEIVED) {
        return emcCommandWaitReceived();
    } else if (emcWaitType == EMC_WAIT_DONE) {
        return emcCommandWaitDone();
    }
    return 0;
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


/*
  to convert linear units, values are converted to mm, then to desired
  units
*/
double convertLinearUnits(double u)
{
    double in_mm;

    /* convert u to mm */
    in_mm = u / emcStatus->motion.traj.linearUnits;

    /* convert u to display units */
    switch (linearUnitConversion) {
    case LINEAR_UNITS_MM:
	return in_mm;
	break;
    case LINEAR_UNITS_INCH:
	return in_mm * INCH_PER_MM;
	break;
    case LINEAR_UNITS_CM:
	return in_mm * CM_PER_MM;
	break;
    case LINEAR_UNITS_AUTO:
	switch (emcStatus->task.programUnits) {
	case CANON_UNITS_MM:
	    return in_mm;
	    break;
	case CANON_UNITS_INCHES:
	    return in_mm * INCH_PER_MM;
	    break;
	case CANON_UNITS_CM:
	    return in_mm * CM_PER_MM;
	    break;
	}
	break;

    case LINEAR_UNITS_CUSTOM:
	return u;
	break;
    }

    // If it ever gets here we have an error.

    return u;
}

double convertAngularUnits(double u)
{
    // Angular units are always degrees
    return u;
}

// polarities for joint jogging, from INI file
static int jogPol[EMCMOT_MAX_JOINTS];

int sendDebug(int level)
{
    EMC_SET_DEBUG debug_msg;

    debug_msg.debug = level;
    return emcSendCommandAndWait(debug_msg);
}

int sendEstop()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::ESTOP;
    return emcSendCommandAndWait(state_msg);
}

int sendEstopReset()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::ESTOP_RESET;
    return emcSendCommandAndWait(state_msg);
}

int sendMachineOn()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::ON;
    return emcSendCommandAndWait(state_msg);
}

int sendMachineOff()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::OFF;
    return emcSendCommandAndWait(state_msg);
}

int sendManual()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE::MANUAL;
    return emcSendCommandAndWait(mode_msg);
}

int sendAuto()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE::AUTO;
    return emcSendCommandAndWait(mode_msg);
}

int sendMdi()
{
    EMC_TASK_SET_MODE mode_msg;

    mode_msg.mode = EMC_TASK_MODE::MDI;
    return emcSendCommandAndWait(mode_msg);
}

int sendOverrideLimits(int joint)
{
    EMC_JOINT_OVERRIDE_LIMITS lim_msg;

    lim_msg.joint = joint;	// neg means off, else on for all
    return emcSendCommandAndWait(lim_msg);
}

int sendJogStop(int ja, int jjogmode)
{
    EMC_JOG_STOP emc_jog_stop_msg;

    if (   (   (jjogmode == JOGJOINT)
            && (emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP) )
        || (   (jjogmode == JOGTELEOP )
            && (emcStatus->motion.traj.mode != EMC_TRAJ_MODE::TELEOP) )
       ) {
       return -1;
    }

    // FIXME: these checks should use the emcStatus values
    if (  jjogmode &&  (ja < 0 || ja >= num_joints)) {
      fprintf(stderr,"shcom.cc: unexpected_1 %d\n",ja); return -1;
    }
    if ( !jjogmode &&  (ja < 0))                     {
      fprintf(stderr,"shcom.cc: unexpected_2 %d\n",ja); return -1;
    }

    emc_jog_stop_msg.jjogmode = jjogmode;
    emc_jog_stop_msg.joint_or_axis = ja;
    return emcSendCommandAndWait(emc_jog_stop_msg);
}

int sendJogCont(int ja, int jjogmode, double speed)
{
    EMC_JOG_CONT emc_jog_cont_msg;

    if (emcStatus->task.state != EMC_TASK_STATE::ON) { return -1; }
    if (   (  (jjogmode == JOGJOINT)
            && (emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP) )
        || (   (jjogmode == JOGTELEOP )
            && (emcStatus->motion.traj.mode != EMC_TRAJ_MODE::TELEOP) )
       ) {
       return -1;
    }

    // FIXME: these checks should use the emcStatus values
    if (  jjogmode &&  (ja < 0 || ja >= num_joints)) {
       fprintf(stderr,"shcom.cc: unexpected_3 %d\n",ja); return -1;
    }
    if ( !jjogmode &&  (ja < 0))                     {
       fprintf(stderr,"shcom.cc: unexpected_4 %d\n",ja); return -1;
    }

    emc_jog_cont_msg.jjogmode = jjogmode;
    emc_jog_cont_msg.joint_or_axis = ja;
    emc_jog_cont_msg.vel = speed / 60.0;

    return emcSendCommandAndWait(emc_jog_cont_msg);
}

int sendJogIncr(int ja, int jjogmode, double speed, double incr)
{
    EMC_JOG_INCR emc_jog_incr_msg;

    if (emcStatus->task.state != EMC_TASK_STATE::ON) { return -1; }
    if (   ( (jjogmode == JOGJOINT)
        && (  emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP) )
        || ( (jjogmode == JOGTELEOP )
        && (  emcStatus->motion.traj.mode != EMC_TRAJ_MODE::TELEOP) )
       ) {
       return -1;
    }

    // FIXME: these checks should use the emcStatus values
    if (  jjogmode &&  (ja < 0 || ja >= num_joints)) {
        fprintf(stderr,"shcom.cc: unexpected_5 %d\n",ja); return -1;
    }
    if ( !jjogmode &&  (ja < 0))                     {
        fprintf(stderr,"shcom.cc: unexpected_6 %d\n",ja); return -1;
    }

    emc_jog_incr_msg.jjogmode = jjogmode;
    emc_jog_incr_msg.joint_or_axis = ja;
    emc_jog_incr_msg.vel = speed / 60.0;
    emc_jog_incr_msg.incr = incr;

    return emcSendCommandAndWait(emc_jog_incr_msg);
}

int sendMistOn()
{
    EMC_COOLANT_MIST_ON emc_coolant_mist_on_msg;

    return emcSendCommandAndWait(emc_coolant_mist_on_msg);
}

int sendMistOff()
{
    EMC_COOLANT_MIST_OFF emc_coolant_mist_off_msg;

    return emcSendCommandAndWait(emc_coolant_mist_off_msg);
}

int sendFloodOn()
{
    EMC_COOLANT_FLOOD_ON emc_coolant_flood_on_msg;

    return emcSendCommandAndWait(emc_coolant_flood_on_msg);
}

int sendFloodOff()
{
    EMC_COOLANT_FLOOD_OFF emc_coolant_flood_off_msg;

    return emcSendCommandAndWait(emc_coolant_flood_off_msg);
}

int sendSpindleForward(int spindle)
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    emc_spindle_on_msg.spindle = spindle;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed = fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = +500;
    }
    return emcSendCommandAndWait(emc_spindle_on_msg);
}

int sendSpindleReverse(int spindle)
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    emc_spindle_on_msg.spindle = spindle;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed =
	    -1 * fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = -500;
    }
    return emcSendCommandAndWait(emc_spindle_on_msg);
}

int sendSpindleOff(int spindle)
{
    EMC_SPINDLE_OFF emc_spindle_off_msg;
    emc_spindle_off_msg.spindle = spindle;
    return emcSendCommandAndWait(emc_spindle_off_msg);
}

int sendSpindleIncrease(int spindle)
{
    EMC_SPINDLE_INCREASE emc_spindle_increase_msg;
    emc_spindle_increase_msg.spindle = spindle;
    return emcSendCommandAndWait(emc_spindle_increase_msg);
}

int sendSpindleDecrease(int spindle)
{
    EMC_SPINDLE_DECREASE emc_spindle_decrease_msg;
    emc_spindle_decrease_msg.spindle = spindle;
    return emcSendCommandAndWait(emc_spindle_decrease_msg);
}

int sendSpindleConstant(int spindle)
{
    EMC_SPINDLE_CONSTANT emc_spindle_constant_msg;
    emc_spindle_constant_msg.spindle = spindle;
    return emcSendCommandAndWait(emc_spindle_constant_msg);
}

int sendBrakeEngage(int spindle)
{
    EMC_SPINDLE_BRAKE_ENGAGE emc_spindle_brake_engage_msg;

    emc_spindle_brake_engage_msg.spindle = spindle;
    return emcSendCommandAndWait(emc_spindle_brake_engage_msg);
}

int sendBrakeRelease(int spindle)
{
    EMC_SPINDLE_BRAKE_RELEASE emc_spindle_brake_release_msg;

    emc_spindle_brake_release_msg.spindle = spindle;
    return emcSendCommandAndWait(emc_spindle_brake_release_msg);
}

int sendAbort()
{
    EMC_TASK_ABORT task_abort_msg;

    return emcSendCommandAndWait(task_abort_msg);
}

int sendHome(int joint)
{
    EMC_JOINT_HOME emc_joint_home_msg;

    emc_joint_home_msg.joint = joint;
    return emcSendCommandAndWait(emc_joint_home_msg);
}

int sendUnHome(int joint)
{
    EMC_JOINT_UNHOME emc_joint_home_msg;

    emc_joint_home_msg.joint = joint;
    return emcSendCommandAndWait(emc_joint_home_msg);
}

int sendFeedOverride(double _override)
{
    EMC_TRAJ_SET_SCALE emc_traj_set_scale_msg;

    if (_override < 0.0) {
	_override = 0.0;
    }

    emc_traj_set_scale_msg.scale = _override;
    return emcSendCommandAndWait(emc_traj_set_scale_msg);
}

int sendRapidOverride(double _override)
{
    EMC_TRAJ_SET_RAPID_SCALE emc_traj_set_scale_msg;

    if (_override < 0.0) {
	_override = 0.0;
    }

    if (_override > 1.0) {
	_override = 1.0;
    }

    emc_traj_set_scale_msg.scale = _override;
    return emcSendCommandAndWait(emc_traj_set_scale_msg);
}


int sendSpindleOverride(int spindle, double _override)
{
    EMC_TRAJ_SET_SPINDLE_SCALE emc_traj_set_spindle_scale_msg;

    if (_override < 0.0) {
	_override = 0.0;
    }

    emc_traj_set_spindle_scale_msg.spindle = spindle;
    emc_traj_set_spindle_scale_msg.scale = _override;
    return emcSendCommandAndWait(emc_traj_set_spindle_scale_msg);
}

int sendTaskPlanInit()
{
    EMC_TASK_PLAN_INIT task_plan_init_msg;

    return emcSendCommandAndWait(task_plan_init_msg);
}

// saved value of last program opened
static std::string lastProgramFile;

int sendProgramOpen(const char *program)
{
    int res = 0;
    EMC_TASK_PLAN_OPEN msg;

    /* save this to run again */
    lastProgramFile = program;
    /* store filename in message */
    rtapi_strxcpy(msg.file, program);
    /* clear optional fields */
    msg.remote_buffersize = 0;
    msg.remote_filesize = 0;
    /* if we are a remote process, we send file in chunks to linuxcnc via remote_buffer */
    if(emcCommandBuffer->cms->ProcessType == CMS_REMOTE_TYPE && strcmp(emcCommandBuffer->cms->ProcessName, "emc") != 0) {
        /* open file */
        FILE *fd;
        if(!(fd = fopen(program, "r"))) {
            rcs_print_error("fopen(%s) error: %s\n", program, strerror(errno));
            return -1;
        }
        /* get filesize */
        if(fseek(fd, 0L, SEEK_END) != 0) {
            fclose(fd);
            rcs_print_error("fseek(%s) error: %s\n", program, strerror(errno));
            return -1;
        }
        long ftpos = ftell(fd);
        msg.remote_filesize = ftpos;
        if(ftpos < 0) {
            fclose(fd);
            rcs_print_error("ftell(%s) error: %s\n", program, strerror(errno));
            return -1;
        }
        if(fseek(fd, 0L, SEEK_SET) != 0) {
            fclose(fd);
            rcs_print_error("fseek(%s) error: %s\n", program, strerror(errno));
            return -1;
        }

        /* send complete file content in chunks of sizeof(msg.remote_buffer) */
        while(!(feof(fd))) {
            size_t bytes_read = fread(&msg.remote_buffer, 1, sizeof(msg.remote_buffer), fd);
            /* read error? */
            if(bytes_read <= 0 && ferror(fd)) {
                rcs_print_error("fread(%s) error: %s\n", program, strerror(errno));
                res = -1;
                break;
            }
            /* save amount of bytes written to buffer */
            msg.remote_buffersize = bytes_read;
            /* send chunk */
            emcCommandSend(msg);
            /* error happened? */
            if(emcCommandWaitDone() != 0) {
                rcs_print_error("emcCommandSend() error\n");
                res = -1;
                break;
            }
        }
        fclose(fd);
        return res;
    }

    /* local process, just send filename */
    return emcSendCommandAndWait(msg);
}

int sendProgramRun(int line)
{
    EMC_TASK_PLAN_RUN emc_task_plan_run_msg;

    if (emcUpdateType == EMC_UPDATE_AUTO) {
	updateStatus();
    }
    // first reopen program if it's not open
    if (0 == emcStatus->task.file[0]) {
	// send a request to open last one
	sendProgramOpen(lastProgramFile.c_str());
    }
    // save the start line, to compare against active line later
    programStartLine = line;

    emc_task_plan_run_msg.line = line;
    return emcSendCommandAndWait(emc_task_plan_run_msg);
}

int sendProgramPause()
{
    EMC_TASK_PLAN_PAUSE emc_task_plan_pause_msg;

    return emcSendCommandAndWait(emc_task_plan_pause_msg);
}

int sendProgramResume()
{
    EMC_TASK_PLAN_RESUME emc_task_plan_resume_msg;

    return emcSendCommandAndWait(emc_task_plan_resume_msg);
}

int sendSetOptionalStop(bool state)
{
    EMC_TASK_PLAN_SET_OPTIONAL_STOP emc_task_plan_set_optional_stop_msg;

    emc_task_plan_set_optional_stop_msg.state = state;
    return emcSendCommandAndWait(emc_task_plan_set_optional_stop_msg);
}


int sendProgramStep()
{
    EMC_TASK_PLAN_STEP emc_task_plan_step_msg;

    // clear out start line, if we had a verify before it would be -1
    programStartLine = 0;

    return emcSendCommandAndWait(emc_task_plan_step_msg);
}

int sendMdiCmd(const char *mdi)
{
    EMC_TASK_PLAN_EXECUTE emc_task_plan_execute_msg;

    rtapi_strxcpy(emc_task_plan_execute_msg.command, mdi);
    return emcSendCommandAndWait(emc_task_plan_execute_msg);
}

int sendLoadToolTable(const char *file)
{
    EMC_TOOL_LOAD_TOOL_TABLE emc_tool_load_tool_table_msg;

    rtapi_strxcpy(emc_tool_load_tool_table_msg.file, file);
    return emcSendCommandAndWait(emc_tool_load_tool_table_msg);
}

int sendToolSetOffset(int toolno, double zoffset, double diameter)
{
    EMC_TOOL_SET_OFFSET emc_tool_set_offset_msg;

    emc_tool_set_offset_msg.toolno = toolno;
    emc_tool_set_offset_msg.offset.tran.z = zoffset;
    emc_tool_set_offset_msg.diameter = diameter;
    emc_tool_set_offset_msg.orientation = 0; // mill style tool table

    return emcSendCommandAndWait(emc_tool_set_offset_msg);
}

int sendToolSetOffset(int toolno, double zoffset, double xoffset,
                      double diameter, double frontangle, double backangle,
                      int orientation)
{
    EMC_TOOL_SET_OFFSET emc_tool_set_offset_msg;

    emc_tool_set_offset_msg.toolno = toolno;
    emc_tool_set_offset_msg.offset.tran.z = zoffset;
    emc_tool_set_offset_msg.offset.tran.x = xoffset;
    emc_tool_set_offset_msg.diameter = diameter;
    emc_tool_set_offset_msg.frontangle = frontangle;
    emc_tool_set_offset_msg.backangle = backangle;
    emc_tool_set_offset_msg.orientation = orientation;

    return emcSendCommandAndWait(emc_tool_set_offset_msg);
}

int sendJointSetBacklash(int joint, double backlash)
{
    EMC_JOINT_SET_BACKLASH emc_joint_set_backlash_msg;

    emc_joint_set_backlash_msg.joint = joint;
    emc_joint_set_backlash_msg.backlash = backlash;
    return emcSendCommandAndWait(emc_joint_set_backlash_msg);
}

int sendJointLoadComp(int /*joint*/, const char *file, int type)
{
    EMC_JOINT_LOAD_COMP emc_joint_load_comp_msg;

    rtapi_strxcpy(emc_joint_load_comp_msg.file, file);
    emc_joint_load_comp_msg.type = type;
    return emcSendCommandAndWait(emc_joint_load_comp_msg);
}

int sendSetTeleopEnable(int enable)
{
    EMC_TRAJ_SET_TELEOP_ENABLE emc_set_teleop_enable_msg;

    emc_set_teleop_enable_msg.enable = enable;
    return emcSendCommandAndWait(emc_set_teleop_enable_msg);
}

int sendClearProbeTrippedFlag()
{
    EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG emc_clear_probe_tripped_flag_msg;

    return emcSendCommandAndWait(emc_clear_probe_tripped_flag_msg);
}

int sendProbe(double x, double y, double z)
{
    EMC_TRAJ_PROBE emc_probe_msg;

    emc_probe_msg.pos.tran.x = x;
    emc_probe_msg.pos.tran.y = y;
    emc_probe_msg.pos.tran.z = z;

    return emcSendCommandAndWait(emc_probe_msg);
}

int iniLoad(const char *filename)
{
    IniFile inifile;
    char displayString[LINELEN] = "";
    int t;
    int i;

    // open it
    if (inifile.Open(filename) == false) {
	return -1;
    }

    // EMC debugging flags
	emc_debug = 0;  // disabled by default
    if (auto inistring = inifile.Find("DEBUG", "EMC")) {
        // parse to global
        if (sscanf(inistring->c_str(), "%x", &emc_debug) < 1) {
            perror("failed to parse [EMC] DEBUG");
        }
    }

    // set output for RCS messages
    set_rcs_print_destination(RCS_PRINT_TO_STDOUT);   // use stdout by default
    if (auto inistring = inifile.Find("RCS_DEBUG_DEST", "EMC")) {
        static RCS_PRINT_DESTINATION_TYPE type;
        if (*inistring == "STDOUT") {
            type = RCS_PRINT_TO_STDOUT;
        } else if (*inistring == "STDERR") {
            type = RCS_PRINT_TO_STDERR;
        } else if (*inistring == "FILE") {
            type = RCS_PRINT_TO_FILE;
        } else if (*inistring == "LOGGER") {
            type = RCS_PRINT_TO_LOGGER;
        } else if (*inistring == "MSGBOX") {
            type = RCS_PRINT_TO_MESSAGE_BOX;
        } else if (*inistring == "NULL") {
            type = RCS_PRINT_TO_NULL;
        } else {
             type = RCS_PRINT_TO_STDOUT;
        }
        set_rcs_print_destination(type);
    }

    // NML/RCS debugging flags
    set_rcs_print_flag(PRINT_RCS_ERRORS);  // only print errors by default
    // enable all debug messages by default if RCS or NML debugging is enabled
    if ((emc_debug & EMC_DEBUG_RCS) || (emc_debug & EMC_DEBUG_NML)) {
        // output all RCS debug messages
        set_rcs_print_flag(PRINT_EVERYTHING);
    }

    // set flags if RCS_DEBUG in ini file
    if (auto inistring = inifile.Find("RCS_DEBUG", "EMC")) {
        long unsigned int flags;
        if (sscanf(inistring->c_str(), "%lx", &flags) < 1) {
            perror("failed to parse [EMC] RCS_DEBUG");
        }
        // clear all flags
        clear_rcs_print_flag(PRINT_EVERYTHING);
        // set parsed flags
        set_rcs_print_flag((long)flags);
    }
    // output infinite RCS errors by default
    max_rcs_errors_to_print = -1;
    if (auto inistring = inifile.Find("RCS_MAX_ERR", "EMC")) {
        if (sscanf(inistring->c_str(), "%d", &max_rcs_errors_to_print) < 1) {
            perror("failed to parse [EMC] RCS_MAX_ERR");
        }
    }

    if (emc_debug & EMC_DEBUG_CONFIG) {
        std::string version = "<unknown>";
        std::string machine = "<unknown>";
        if (auto inistring = inifile.Find("VERSION", "EMC")) {
            version = *inistring;
        }

        if (auto inistring = inifile.Find("MACHINE", "EMC")) {
            machine = *inistring;
        }

        extern char *program_invocation_short_name;
        rcs_print(
            "%s (%d) shcom: machine '%s'  version '%s'\n",
            program_invocation_short_name, getpid(), machine.c_str(), version.c_str()
        );
    }

    if (auto inistring = inifile.Find("NML_FILE", "EMC")) {
	// copy to global
	rtapi_strxcpy(emc_nmlfile, inistring->c_str());
    } else {
	// not found, use default
    }

    for (t = 0; t < EMCMOT_MAX_JOINTS; t++) {
	jogPol[t] = 1;		// set to default
	snprintf(displayString, sizeof(displayString), "JOINT_%d", t);
	auto inistring = inifile.Find("JOGGING_POLARITY", displayString);
	if (inistring && 1 == sscanf(inistring->c_str(), "%d", &i) && i == 0) {
	    // it read as 0, so override default
	    jogPol[t] = 0;
	}
    }

    if (auto inistring = inifile.Find("LINEAR_UNITS", "DISPLAY")) {
	if (*inistring == "AUTO") {
	    linearUnitConversion = LINEAR_UNITS_AUTO;
	} else if (*inistring == "INCH") {
	    linearUnitConversion = LINEAR_UNITS_INCH;
	} else if (*inistring == "MM") {
	    linearUnitConversion = LINEAR_UNITS_MM;
	} else if (*inistring == "CM") {
	    linearUnitConversion = LINEAR_UNITS_CM;
	}
    } else {
	// not found, leave default alone
    }

    if (auto inistring = inifile.Find("ANGULAR_UNITS", "DISPLAY")) {
	if (*inistring == "AUTO") {
	    angularUnitConversion = ANGULAR_UNITS_AUTO;
	} else if (*inistring == "DEG") {
	    angularUnitConversion = ANGULAR_UNITS_DEG;
	} else if (*inistring == "RAD") {
	    angularUnitConversion = ANGULAR_UNITS_RAD;
	} else if (*inistring == "GRAD") {
	    angularUnitConversion = ANGULAR_UNITS_GRAD;
	}
    } else {
	// not found, leave default alone
    }

    // close it
    inifile.Close();

    return 0;
}

int checkStatus ()
{
    return emcStatus ? 1 : 0;
}

