/********************************************************************
* Description: iotaskintf.cc
*   NML interface functions for IO
*
*   Based on a work by Fred Proctor & Will Shackleford
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

#include <math.h>		// fabs()
#include <float.h>		// DBL_MAX
#include <string.h>		// memcpy() strncpy()
#include <stdlib.h>		// malloc()

#include "rcs.hh"		// RCS_CMD_CHANNEL, etc.

#include "emc.hh"		// EMC NML
#include "emcglb.h"		// EMC_INIFILE

#include "inispin.hh"
#include "initool.hh"

/* FIXME - defining LASER to test motion/IO synch
   Laser currently uses M07 for ON and M09 for OFF - This is incorrect, we
   should be using M03/M05 or a dedicated M code. */
// #define LASER

// IO INTERFACE

// the NML channels to the EMCIO controller
static RCS_CMD_CHANNEL *emcIoCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcIoStatusBuffer = 0;

// global status structure
EMC_IO_STAT *emcIoStatus = 0;

// serial number for communication
static int emcIoCommandSerialNumber = 0;
static double EMCIO_BUFFER_GET_TIMEOUT = 5.0;

static int emcioNmlGet()
{
    int retval = 0;
    double start_time;
    RCS_PRINT_DESTINATION_TYPE orig_dest;
    if (emcIoCommandBuffer == 0) {
	orig_dest = get_rcs_print_destination();
	set_rcs_print_destination(RCS_PRINT_TO_NULL);
	start_time = etime();
	while (start_time - etime() < EMCIO_BUFFER_GET_TIMEOUT) {
	    emcIoCommandBuffer =
		new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc", EMC_NMLFILE);
	    if (!emcIoCommandBuffer->valid()) {
		delete emcIoCommandBuffer;
		emcIoCommandBuffer = 0;
	    } else {
		break;
	    }
	    esleep(0.1);
	}
	set_rcs_print_destination(orig_dest);
    }

    if (emcIoCommandBuffer == 0) {
	emcIoCommandBuffer =
	    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc", EMC_NMLFILE);
	if (!emcIoCommandBuffer->valid()) {
	    delete emcIoCommandBuffer;
	    emcIoCommandBuffer = 0;
	    retval = -1;
	}
    }

    if (emcIoStatusBuffer == 0) {
	orig_dest = get_rcs_print_destination();
	set_rcs_print_destination(RCS_PRINT_TO_NULL);
	start_time = etime();
	while (start_time - etime() < EMCIO_BUFFER_GET_TIMEOUT) {
	    emcIoStatusBuffer =
		new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emc",
		EMC_NMLFILE);
	    if (!emcIoStatusBuffer->valid()) {
		delete emcIoStatusBuffer;
		emcIoStatusBuffer = 0;
	    } else {
		emcIoStatus =
		    (EMC_IO_STAT *) emcIoStatusBuffer->get_address();
		// capture serial number for next send
		emcIoCommandSerialNumber = emcIoStatus->echo_serial_number;
		break;
	    }
	    esleep(0.1);
	}
	set_rcs_print_destination(orig_dest);
    }

    if (emcIoStatusBuffer == 0) {
	emcIoStatusBuffer =
	    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emc", EMC_NMLFILE);
	if (!emcIoStatusBuffer->valid()
	    || EMC_IO_STAT_TYPE != emcIoStatusBuffer->peek()) {
	    delete emcIoStatusBuffer;
	    emcIoStatusBuffer = 0;
	    emcIoStatus = 0;
	    retval = -1;
	} else {
	    emcIoStatus = (EMC_IO_STAT *) emcIoStatusBuffer->get_address();
	    // capture serial number for next send
	    emcIoCommandSerialNumber = emcIoStatus->echo_serial_number;
	}
    }

    return retval;
}

static RCS_CMD_MSG *last_io_command = 0;

/*
  sendCommand() waits until any currently executing command has finished,
  then writes the given command.

  FIXME: Not very RCS-like to wait for status done here. (wps)
*/
static int sendCommand(RCS_CMD_MSG * msg)
{
    // need command buffer to be there
    if (0 == emcIoCommandBuffer) {
	return -1;
    }
    // need status buffer also, to check for command received
    if (0 == emcIoStatusBuffer || !emcIoStatusBuffer->valid()) {
	return -1;
    }

    double send_command_timeout = etime() + 5.0;

    // check if we're executing, and wait until we're done
    while (etime() < send_command_timeout) {
	emcIoStatusBuffer->peek();
	if (emcIoStatus->echo_serial_number != emcIoCommandSerialNumber ||
	    emcIoStatus->status == RCS_EXEC) {
	    esleep(0.001);
	    continue;
	} else {
	    break;
	}
    }

    if (emcIoStatus->echo_serial_number != emcIoCommandSerialNumber ||
	emcIoStatus->status == RCS_EXEC) {
	// Still not done, must have timed out.
	rcs_print_error
	    ("Command to IO level (%s:%s) timed out waiting for last command done. \n",
	    emcSymbolLookup(msg->type), emcIoCommandBuffer->msg2str(msg));
	rcs_print_error
	    ("emcIoStatus->echo_serial_number=%d, emcIoCommandSerialNumber=%d, emcIoStatus->status=%d\n",
	    emcIoStatus->echo_serial_number, emcIoCommandSerialNumber,
	    emcIoStatus->status);
	if (0 != last_io_command) {
	    rcs_print_error("Last command sent to IO level was (%s:%s)\n",
		emcSymbolLookup(last_io_command->type),
		emcIoCommandBuffer->msg2str(last_io_command));
	}
	return -1;
    }
    // now we can send
    msg->serial_number = ++emcIoCommandSerialNumber;
    if (0 != emcIoCommandBuffer->write(msg)) {
	rcs_print_error("Failed to send command to  IO level (%s:%s)\n",
	    emcSymbolLookup(msg->type), emcIoCommandBuffer->msg2str(msg));
	return -1;
    }

    if (last_io_command == 0) {
	last_io_command = (RCS_CMD_MSG *) malloc(0x4000);
    }

    if (0 != last_io_command) {
	memcpy(last_io_command, msg, msg->size);
    }

    return 0;
}

/*
  forceCommand() writes the given command regardless of the executing
  status of any previous command.
*/
static int forceCommand(RCS_CMD_MSG * msg)
{
    // need command buffer to be there
    if (0 == emcIoCommandBuffer) {
	return -1;
    }
    // need status buffer also, to check for command received
    if (0 == emcIoStatusBuffer || !emcIoStatusBuffer->valid()) {
	return -1;
    }
    // send it immediately
    msg->serial_number = ++emcIoCommandSerialNumber;
    if (0 != emcIoCommandBuffer->write(msg)) {
	rcs_print_error("Failed to send command to  IO level (%s:%s)\n",
	    emcSymbolLookup(msg->type), emcIoCommandBuffer->msg2str(msg));
	return -1;
    }

    if (last_io_command == 0) {
	last_io_command = (RCS_CMD_MSG *) malloc(0x4000);
    }

    if (0 != last_io_command) {
	memcpy(last_io_command, msg, msg->size);
    }

    return 0;
}

// NML commands

int emcIoInit()
{
    EMC_TOOL_INIT ioInitMsg;

    // get NML buffer to emcio
    if (0 != emcioNmlGet()) {
	rcs_print_error("emcioNmlGet() failed.\n");
	return -1;
    }

    if (0 != iniSpindle(EMC_INIFILE)) {
	rcs_print_error("iniSpindle failed.\n");
	return -1;
    }

    if (0 != iniTool(EMC_INIFILE)) {
	return -1;
    }
    // send init command to emcio
    if (forceCommand(&ioInitMsg)) {
	rcs_print_error("Can't forceCommand(ioInitMsg)\n");
	return -1;
    }

    return 0;
}

int emcIoHalt()
{
    EMC_TOOL_HALT ioHaltMsg;

    // send halt command to emcio
    if (emcIoCommandBuffer != 0) {
	forceCommand(&ioHaltMsg);
    }
    // clear out the buffers

    if (emcIoStatusBuffer != 0) {
	delete emcIoStatusBuffer;
	emcIoStatusBuffer = 0;
	emcIoStatus = 0;
    }

    if (emcIoCommandBuffer != 0) {
	delete emcIoCommandBuffer;
	emcIoCommandBuffer = 0;
    }

    return 0;
}

int emcIoAbort()
{
    EMC_TOOL_ABORT ioAbortMsg;

    // send abort command to emcio
    forceCommand(&ioAbortMsg);

    return 0;
}

int emcIoSetDebug(int debug)
{
    EMC_SET_DEBUG ioDebugMsg;

    ioDebugMsg.debug = debug;

    return sendCommand(&ioDebugMsg);
}

int emcAuxEstopOn()
{
    EMC_AUX_ESTOP_ON estopOnMsg;

    return forceCommand(&estopOnMsg);
}

int emcAuxEstopOff()
{
    EMC_AUX_ESTOP_OFF estopOffMsg;

    return forceCommand(&estopOffMsg);
}

static double spindleSpeed = 0;

int emcSpindleAbort()
{
    return emcSpindleOff();
}

int emcSpindleOn(double speed)
{
    double dacout = 0;

    spindleSpeed = speed;
    // do digital IO stuff to enable spindle
    spindleSpeed = speed;
    EMC_SPINDLE_ON spindleOnMsg;
    spindleOnMsg.speed = speed;
    sendCommand(&spindleOnMsg);
/* Convert speed to an analogue voltage */
    dacout = (speed > 0.0 ? (fabs(speed) * MAX_VOLTS_PER_RPM) :
	speed < 0.0 ? (fabs(speed) * MIN_VOLTS_PER_RPM) : 0);
/* Use an existing function to set the DAC output */
    emcAxisSetOutput(SPINDLE_ON_INDEX, dacout);
/* and enable the DAC */
    emcAxisEnable(SPINDLE_ON_INDEX);

    return 0;
}

int emcSpindleOff()
{
    EMC_SPINDLE_OFF spindleOffMsg;

    sendCommand(&spindleOffMsg);

/* Disable the DAC and set the output to zero 
   (just in case it is enabled else where !) */
    emcAxisDisable(SPINDLE_ON_INDEX);
    emcAxisSetOutput(SPINDLE_ON_INDEX, 0);

    return 0;
}

int emcSpindleBrakeRelease()
{
    EMC_SPINDLE_BRAKE_RELEASE spindleBrakeReleaseMsg;

    sendCommand(&spindleBrakeReleaseMsg);

    return 0;
}

int emcSpindleBrakeEngage()
{
    EMC_SPINDLE_BRAKE_ENGAGE spindleBrakeEngageMsg;

    sendCommand(&spindleBrakeEngageMsg);

    return 0;
}

int emcSpindleIncrease()
{
    EMC_SPINDLE_INCREASE spindleIncreaseMsg;

    sendCommand(&spindleIncreaseMsg);

    return 0;
}

int emcSpindleDecrease()
{
    EMC_SPINDLE_DECREASE spindleDecreaseMsg;

    sendCommand(&spindleDecreaseMsg);

    return 0;
}

int emcSpindleConstant()
{
    EMC_SPINDLE_CONSTANT spindleConstantMsg;

    sendCommand(&spindleConstantMsg);

    return 0;
}

#ifdef LASER

int emcCoolantMistOn()
{
    return emcMotionSetDout(0, 1, 1);
}

int emcCoolantMistOff()
{
    return emcMotionSetDout(0, 1, 0);
}

int emcCoolantFloodOn()
{
    return 0;
}

int emcCoolantFloodOff()
{
    return 0;
}

#else

int emcCoolantMistOn()
{
    EMC_COOLANT_MIST_ON mistOnMsg;

    sendCommand(&mistOnMsg);

    return 0;
}

int emcCoolantMistOff()
{
    EMC_COOLANT_MIST_OFF mistOffMsg;

    sendCommand(&mistOffMsg);

    return 0;
}

int emcCoolantFloodOn()
{
    EMC_COOLANT_FLOOD_ON floodOnMsg;

    sendCommand(&floodOnMsg);

    return 0;
}

int emcCoolantFloodOff()
{
    EMC_COOLANT_FLOOD_OFF floodOffMsg;

    sendCommand(&floodOffMsg);

    return 0;
}

#endif // LASER

int emcLubeInit()
{
    EMC_LUBE_INIT lubeInitMsg;

    sendCommand(&lubeInitMsg);

    return 0;
}

int emcLubeHalt()
{
    EMC_LUBE_HALT lubeHaltMsg;

    sendCommand(&lubeHaltMsg);

    return 0;
}

int emcLubeAbort()
{
    EMC_LUBE_ABORT lubeAbortMsg;

    sendCommand(&lubeAbortMsg);

    return 0;
}

int emcLubeOn()
{
    EMC_LUBE_ON lubeOnMsg;

    sendCommand(&lubeOnMsg);

    return 0;
}

int emcLubeOff()
{
    EMC_LUBE_OFF lubeOffMsg;

    sendCommand(&lubeOffMsg);

    return 0;
}

int emcToolPrepare(int tool)
{
    EMC_TOOL_PREPARE toolPrepareMsg;

    toolPrepareMsg.tool = tool;
    sendCommand(&toolPrepareMsg);

    return 0;
}

int emcToolLoad()
{
    EMC_TOOL_LOAD toolLoadMsg;

    sendCommand(&toolLoadMsg);

    return 0;
}

int emcToolUnload()
{
    EMC_TOOL_UNLOAD toolUnloadMsg;

    sendCommand(&toolUnloadMsg);

    return 0;
}

int emcToolLoadToolTable(const char *file)
{
    EMC_TOOL_LOAD_TOOL_TABLE toolLoadToolTableMsg;

    strcpy(toolLoadToolTableMsg.file, file);

    sendCommand(&toolLoadToolTableMsg);

    return 0;
}

int emcToolSetOffset(int tool, double length, double diameter)
{
    EMC_TOOL_SET_OFFSET toolSetOffsetMsg;

    toolSetOffsetMsg.tool = tool;
    toolSetOffsetMsg.length = length;
    toolSetOffsetMsg.diameter = diameter;

    sendCommand(&toolSetOffsetMsg);

    return 0;
}

// Status functions

int emcIoUpdate(EMC_IO_STAT * stat)
{
    if (0 == emcIoStatusBuffer || !emcIoStatusBuffer->valid()) {
	return -1;
    }

    switch (emcIoStatusBuffer->peek()) {
    case -1:
	// error on CMS channel
	return -1;
	break;

    case 0:			// nothing new
    case EMC_IO_STAT_TYPE:	// something new
	// drop out to copy
	break;

    default:
	// something else is in there
	return -1;
	break;
    }

    // copy status
    *stat = *emcIoStatus;

    /* 
       We need to check that the RCS_DONE isn't left over from the previous
       command, by comparing the command number we sent with the command
       number that emcio echoes. If they're different, then the command
       hasn't been acknowledged yet and the state should be forced to be
       RCS_EXEC. */
    if (stat->echo_serial_number != emcIoCommandSerialNumber) {
	stat->status = RCS_EXEC;
    }
    stat->spindle.speed = spindleSpeed;

    return 0;
}
