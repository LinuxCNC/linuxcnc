/********************************************************************
* Description: simIoControl.cc
*           Simply accepts NML messages sent to the IO controller
*           and sends back a "Done" message.
*           Use this as a template for real IO controllers once a
*           HAL interface has been implimented.
*
*   Derived from a work by Fred Proctor & Will Shackleford
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "rtapi.h"              /* rtapi_print_msg */
#include "rcs.hh"               /* RCS_CMD_CHANNEL */
#include "emc.hh"               /* EMC NML */
#include "emcglb.h"             /* EMC_NMLFILE, EMC_INIFILE */
#include "inifile.hh"            /* INIFILE */

static RCS_CMD_CHANNEL * emcioCommandBuffer = 0;
static RCS_CMD_MSG * emcioCommand = 0;
static RCS_STAT_CHANNEL * emcioStatusBuffer = 0;
static EMC_IO_STAT emcioStatus;
static NML * emcErrorBuffer = 0;

/********************************************************************
*
* Description: emcIoNmlGet()
*		Attempts to connect to NML buffers and set the relevant
*		pointers.
*
* Return Value: Zero on success or -1 if can not connect to a buffer.
*
* Side Effects: None.
*
* Called By: main()
*
********************************************************************/
static int emcIoNmlGet()
{
  int retval = 0;

  /* Try to connect to EMC IO command buffer */
  if (emcioCommandBuffer == 0) {
    emcioCommandBuffer = new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "tool", EMC_NMLFILE);
    if (! emcioCommandBuffer->valid()) {
      rtapi_print_msg(RTAPI_MSG_ERR, "emcToolCmd buffer not available\n");
      delete emcioCommandBuffer;
      emcioCommandBuffer = 0;
      retval = -1;
    }
    else {
      /* Get our command data structure */
      emcioCommand = emcioCommandBuffer->get_address();
    }
  }

  /* try to connect to EMC IO status buffer */
  if (emcioStatusBuffer == 0) {
    emcioStatusBuffer = new RCS_STAT_CHANNEL(emcFormat, "toolSts", "tool", EMC_NMLFILE);
    if (! emcioStatusBuffer->valid()) {
      rtapi_print_msg(RTAPI_MSG_ERR, "toolSts buffer not available\n");
      delete emcioStatusBuffer;
      emcioStatusBuffer = 0;
      retval = -1;
    }
    else {
      /* initialize and write status */
      emcioStatus.heartbeat = 0;
      emcioStatus.command_type = 0;
      emcioStatus.echo_serial_number = 0;
      emcioStatus.status = RCS_DONE;
      emcioStatusBuffer->write(&emcioStatus);
    }
  }

  /* try to connect to EMC error buffer */
  if (emcErrorBuffer == 0) {
    emcErrorBuffer = new NML(nmlErrorFormat, "emcError", "tool", EMC_NMLFILE);
    if (! emcErrorBuffer->valid()) {
      rtapi_print_msg(RTAPI_MSG_ERR, "emcError buffer not available\n");
      delete emcErrorBuffer;
      emcErrorBuffer = 0;
      retval = -1;
    }
  }

  return retval;
}

/********************************************************************
*
* Description: iniLoad(const char *filename)
*		Extracts the settings from the specified ini file.
*
* Return Value: Zero on success or -1 if file not found.
*
* Side Effects: Default setting used if the parameter not found in
*		the ini file.
*
* Called By: main()
*
********************************************************************/
static int iniLoad(const char *filename)
{
  Inifile inifile;
  const char * inistring;
  char version[LINELEN];

  /* Open the ini file */
  if (-1 == inifile.open(filename)) {
    return -1;
  }

  if (NULL != (inistring = inifile.find("DEBUG", "EMC"))) {
    /* copy to global */
    if (1 != sscanf(inistring, "%i", &EMC_DEBUG)) {
      EMC_DEBUG = 0;
    }
  }
  else {
    /* not found, use default */
    EMC_DEBUG = 0;
  }

  if (EMC_DEBUG & EMC_DEBUG_VERSIONS) {
    if (NULL != (inistring = inifile.find("VERSION", "EMC"))) {
      sscanf(inistring, "$Revision: %s", version);
      rtapi_print("Version:  %s\n", version);
    }

    if (NULL != (inistring = inifile.find("MACHINE", "EMC"))) {
      rtapi_print("Machine:  %s\n", inistring);
    }
  }

  if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
    strcpy(EMC_NMLFILE, inistring);
  }
  else {
    // not found, use default
  }

  double temp;
  temp = EMC_IO_CYCLE_TIME;
  if (NULL != (inistring = inifile.find("CYCLE_TIME", "EMCIO"))) {
    if (1 == sscanf(inistring, "%lf", &EMC_IO_CYCLE_TIME)) {
      // found it
    }
    else {
      // found, but invalid
      EMC_IO_CYCLE_TIME = temp;
      rtapi_print("invalid [EMCIO] CYCLE_TIME in %s (%s); using default %f\n",
                filename, inistring, EMC_IO_CYCLE_TIME);
    }
  }
  else {
    // not found, using default
    rtapi_print("[EMCIO] CYCLE_TIME not found in %s; using default %f\n",
              filename, EMC_IO_CYCLE_TIME);
  }

  // close it
  inifile.close();

  return 0;
}

static int done = 0;

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

/********************************************************************
*
* Description: main(int argc, char * argv[])
*		Connects to NML buffers and enters an endless loop
*		processing NML IO commands. Print statements are
*		sent to the console indicating which IO command was
*		executed if debug level is set to RTAPI_MSG_DBG.
*		No hardware IO is used - This is a simulation only.
*
* Return Value: Zero or -1 if ini file not found or failure to connect
*		to NML buffers.
*
* Side Effects: None.
*
* Called By: 
*
********************************************************************/
int main(int argc, char * argv[])
{
  int t;
  NMLTYPE type;

  for (t = 1; t < argc; t++) {
    if (!strcmp(argv[t], "-ini")) {
      if (t == argc - 1) {
        return -1;
      }
      else {
        strcpy(EMC_INIFILE, argv[t+1]);
        t++;
      }
      continue;
    }
    /* do other args similarly here */
  }

  if (0 != iniLoad(EMC_INIFILE)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "can't open ini file %s\n", EMC_INIFILE);
    return -1;
  }

  if (0 != emcIoNmlGet()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "can't connect to NML buffers in %s\n", EMC_NMLFILE);
    return -1;
  }

  done = 0;
  /* Register the routine that catches the SIGINT signal */
  signal(SIGINT, quit);
  /* catch SIGTERM too - the run script uses it to shut things down */
  signal(SIGTERM, quit);

  /* set status values to 'normal' */
  emcioStatus.aux.estop = 1;
  emcioStatus.aux.estopIn = 0;
  emcioStatus.spindle.speed = 0.0;
  emcioStatus.spindle.direction = 0;
  emcioStatus.spindle.brake = 1;
  emcioStatus.spindle.increasing = 0;
  emcioStatus.spindle.enabled = 1;
  emcioStatus.coolant.mist = 0;
  emcioStatus.coolant.flood = 0;
  emcioStatus.lube.on = 0;
  emcioStatus.lube.level = 1;
 
  while (! done) {
    /* read NML, run commands */
    if (-1 == emcioCommandBuffer->read()) {
      /* bad command */
      continue;
    }

    if (0 == emcioCommand ||
	0 == emcioCommand->type || 
	emcioCommand->serial_number == emcioStatus.echo_serial_number) {
      continue;
    }

    type = emcioCommand->type;
    switch (type) {
    case 0:
      break;

    case EMC_IO_INIT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_IO_INIT\n");
      break;

    case EMC_TOOL_INIT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_INIT\n");
      break;

    case EMC_TOOL_HALT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_HALT\n");
      break;

    case EMC_TOOL_ABORT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT\n");
      break;

    case EMC_TOOL_PREPARE_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE\n");
      emcioStatus.tool.toolPrepped = ((EMC_TOOL_PREPARE *) emcioCommand)->tool;
      break;

    case EMC_TOOL_LOAD_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD\n");
      break;

    case EMC_TOOL_UNLOAD_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_UNLOAD\n");
      break;

    case EMC_SPINDLE_INIT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_INIT\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_HALT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_HALT\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_ABORT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_ABORT\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_ON_TYPE:
      emcioStatus.spindle.speed = ((EMC_SPINDLE_ON *) emcioCommand)->speed;
      emcioStatus.spindle.direction = 1;
      emcioStatus.spindle.brake = 0;
      emcioStatus.spindle.increasing = 0;
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_ON %f RPM\n", emcioStatus.spindle.speed);
      break;

    case EMC_SPINDLE_OFF_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_OFF\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_FORWARD_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_FORWARD %f RPM\n", emcioStatus.spindle.speed);
      emcioStatus.spindle.direction = 1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_REVERSE_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_REVERSE %f RPM\n", emcioStatus.spindle.speed);
      emcioStatus.spindle.direction = -1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_STOP_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_STOP\n");
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      break;

    case EMC_SPINDLE_INCREASE_TYPE:
      emcioStatus.spindle.speed++;
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_INCREASE %f RPM\n", emcioStatus.spindle.speed);
      emcioStatus.spindle.increasing = 1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_DECREASE_TYPE:
      emcioStatus.spindle.speed--;
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_DECREASE %f RPM\n", emcioStatus.spindle.speed);
      emcioStatus.spindle.increasing = -1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_CONSTANT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_CONSTANT %f RPM\n", emcioStatus.spindle.speed);
      emcioStatus.spindle.increasing = 0;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_BRAKE_RELEASE_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_BRAKE_RELEASE\n");
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_BRAKE_ENGAGE_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_BRAKE_ENGAGE\n");
      emcioStatus.spindle.brake = 1;
      break;

    case EMC_COOLANT_INIT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_INIT\n");
      emcioStatus.coolant.mist = 0;
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_COOLANT_HALT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_HALT\n");
      emcioStatus.coolant.mist = 0;
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_COOLANT_ABORT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_ABORT\n");
      emcioStatus.coolant.mist = 0;
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_COOLANT_MIST_ON_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_MIST_ON\n");
      emcioStatus.coolant.mist = 1;
      break;

    case EMC_COOLANT_MIST_OFF_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_MIST_OFF\n");
      emcioStatus.coolant.mist = 0;
      break;

    case EMC_COOLANT_FLOOD_ON_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_FLOOD_ON\n");
      emcioStatus.coolant.flood = 1;
      break;

    case EMC_COOLANT_FLOOD_OFF_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_FLOOD_OFF\n");
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_AUX_INIT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_INIT\n");
      emcioStatus.aux.estop = 1;
      emcioStatus.aux.estopIn = 0;
      break;

    case EMC_AUX_HALT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_HALT\n");
      emcioStatus.aux.estop = 1;
      emcioStatus.aux.estopIn = 0;
      break;

    case EMC_AUX_ABORT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ABORT\n");
      emcioStatus.aux.estop = 1;
      emcioStatus.aux.estopIn = 0;
      break;

    case EMC_AUX_ESTOP_ON_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_ON\n");
      emcioStatus.aux.estop = 1;
      break;

    case EMC_AUX_ESTOP_OFF_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_OFF\n");
      emcioStatus.aux.estop = 0;
      break;

    case EMC_LUBE_INIT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_INIT\n");
      emcioStatus.lube.on = 0;
      emcioStatus.lube.level = 1;
      break;

    case EMC_LUBE_HALT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_HALT\n");
      emcioStatus.lube.on = 0;
      emcioStatus.lube.level = 1;
      break;

    case EMC_LUBE_ABORT_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_ABORT\n");
      emcioStatus.lube.on = 0;
      emcioStatus.lube.level = 1;
      break;

    case EMC_LUBE_ON_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_ON\n");
      emcioStatus.lube.on = 1;
      break;

    case EMC_LUBE_OFF_TYPE:
      rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_OFF\n");
      emcioStatus.lube.on = 0;
      break;

    default:
      rtapi_print("unknown command %s\n", emcSymbolLookup(type));
      break;
    } /* switch (type) */

    emcioStatus.command_type = type;
    emcioStatus.echo_serial_number = emcioCommand->serial_number;
    emcioStatus.status = RCS_DONE;
    emcioStatus.heartbeat++;
    emcioStatusBuffer->write(&emcioStatus);
    
    esleep(EMC_IO_CYCLE_TIME);
  }

  if (emcErrorBuffer != 0) {
    delete emcErrorBuffer;
    emcErrorBuffer = 0;
  }

  if (emcioStatusBuffer != 0) {
    delete emcioStatusBuffer;
    emcioStatusBuffer = 0;
  }

  if (emcioCommandBuffer != 0) {
    delete emcioCommandBuffer;
    emcioCommandBuffer = 0;
  }

  return 0;
}
