#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "rcs.hh"               // RCS_CMD_CHANNEL, rcs_print_error()
#include "emc.hh"               // EMC NML
#include "emcglb.h"             // EMC_NMLFILE, EMC_INIFILE
#include "inifile.h"            // INIFILE

static RCS_CMD_CHANNEL * emcioCommandBuffer = 0;
static RCS_CMD_MSG * emcioCommand = 0;
static RCS_STAT_CHANNEL * emcioStatusBuffer = 0;
static EMC_IO_STAT emcioStatus;
static NML * emcErrorBuffer = 0;

static int emcIoNmlGet()
{
  int retval = 0;

  // try to connect to EMC IO cmd
  if (emcioCommandBuffer == 0) {
    emcioCommandBuffer = new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "tool", EMC_NMLFILE);
    if (! emcioCommandBuffer->valid()) {
      rcs_print_error("emcToolCmd buffer not available\n");
      delete emcioCommandBuffer;
      emcioCommandBuffer = 0;
      retval = -1;
    }
    else {
      // get our command data structure
      emcioCommand = emcioCommandBuffer->get_address();
    }
  }

  // try to connect to EMC IO status
  if (emcioStatusBuffer == 0) {
    emcioStatusBuffer = new RCS_STAT_CHANNEL(emcFormat, "toolSts", "tool", EMC_NMLFILE);
    if (! emcioStatusBuffer->valid()) {
      rcs_print_error("toolSts buffer not available\n");
      delete emcioStatusBuffer;
      emcioStatusBuffer = 0;
      retval = -1;
    }
    else {
      // initialize and write status
      emcioStatus.heartbeat = 0;
      emcioStatus.command_type = 0;
      emcioStatus.echo_serial_number = 0;
      emcioStatus.status = RCS_DONE;
      emcioStatusBuffer->write(&emcioStatus);
    }
  }

  // try to connect to EMC error buffer
  if (emcErrorBuffer == 0) {
    emcErrorBuffer = new NML(nmlErrorFormat, "emcError", "tool", EMC_NMLFILE);
    if (! emcErrorBuffer->valid()) {
      rcs_print_error("emcError buffer not available\n");
      delete emcErrorBuffer;
      emcErrorBuffer = 0;
      retval = -1;
    }
  }

  return retval;
}

static int iniLoad(const char *filename)
{
  INIFILE inifile;
  const char * inistring;
  char version[INIFILE_MAX_LINELEN];
  double d1;

  // open it
  if (-1 == inifile.open(filename)) {
    return -1;
  }

  if (NULL != (inistring = inifile.find("DEBUG", "EMC"))) {
    // copy to global
    if (1 != sscanf(inistring, "%i", &EMC_DEBUG)) {
      EMC_DEBUG = 0;
    }
  }
  else {
    // not found, use default
    EMC_DEBUG = 0;
  }

  if (EMC_DEBUG & EMC_DEBUG_VERSIONS) {
    if (NULL != (inistring = inifile.find("VERSION", "EMC"))) {
      // print version
      sscanf(inistring, "$Revision: %s", version);
      rcs_print("Version:  %s\n", version);
    }
    else {
      // not found, not fatal
      rcs_print("Version:  (not found)\n");
    }

    if (NULL != (inistring = inifile.find("MACHINE", "EMC"))) {
      // print machine
      rcs_print("Machine:  %s\n", inistring);
    }
    else {
      // not found, not fatal
      rcs_print("Machine:  (not found)\n");
    }
  }

  if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
    // copy to global
    strcpy(EMC_NMLFILE, inistring);
  }
  else {
    // not found, use default
  }

  d1 = EMC_IO_CYCLE_TIME;
  if (NULL != (inistring = inifile.find("CYCLE_TIME", "EMCIO"))) {
    if (1 == sscanf(inistring, "%lf", &EMC_IO_CYCLE_TIME)) {
      // found it
    }
    else {
      // found, but invalid
      EMC_IO_CYCLE_TIME = d1;
      rcs_print("invalid [EMCIO] CYCLE_TIME in %s (%s); using default %f\n",
                filename, inistring, EMC_IO_CYCLE_TIME);
    }
  }
  else {
    // not found, using default
    rcs_print("[EMCIO] CYCLE_TIME not found in %s; using default %f\n",
              filename, EMC_IO_CYCLE_TIME);
  }

  // close it
  inifile.close();

  return 0;
}

static int done = 0;
static void quit(int sig)
{
  done = 1;
}

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
    rcs_print_error("can't open ini file %s\n", EMC_INIFILE);
    return 1;
  }

  if (0 != emcIoNmlGet()) {
    rcs_print_error("can't connect to NML buffers in %s\n", EMC_NMLFILE);
    return 1;
  }

  done = 0;
  signal(SIGINT, quit);

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
      printf("EMC_IO_INIT\n");
      break;

    case EMC_TOOL_INIT_TYPE:
      printf("EMC_TOOL_INIT\n");
      break;

    case EMC_TOOL_HALT_TYPE:
      printf("EMC_TOOL_HALT\n");
      break;

    case EMC_TOOL_ABORT_TYPE:
      printf("EMC_TOOL_ABORT\n");
      break;

    case EMC_TOOL_PREPARE_TYPE:
      printf("EMC_TOOL_PREPARE\n");
      emcioStatus.tool.toolPrepped = ((EMC_TOOL_PREPARE *) emcioCommand)->tool;
      break;

    case EMC_TOOL_LOAD_TYPE:
      printf("EMC_TOOL_LOAD\n");
      break;

    case EMC_TOOL_UNLOAD_TYPE:
      printf("EMC_TOOL_UNLOAD\n");
      break;

    case EMC_SPINDLE_INIT_TYPE:
      printf("EMC_SPINDLE_INIT\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_HALT_TYPE:
      printf("EMC_SPINDLE_HALT\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_ABORT_TYPE:
      printf("EMC_SPINDLE_ABORT\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_ON_TYPE:
      printf("EMC_SPINDLE_ON\n");
      emcioStatus.spindle.speed = ((EMC_SPINDLE_ON *) emcioCommand)->speed;
      emcioStatus.spindle.direction = 1;
      emcioStatus.spindle.brake = 0;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_OFF_TYPE:
      printf("EMC_SPINDLE_OFF\n");
      emcioStatus.spindle.speed = 0.0;
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      emcioStatus.spindle.increasing = 0;
      break;

    case EMC_SPINDLE_FORWARD_TYPE:
      printf("EMC_SPINDLE_FORWARD\n");
      emcioStatus.spindle.direction = 1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_REVERSE_TYPE:
      printf("EMC_SPINDLE_REVERSE\n");
      emcioStatus.spindle.direction = -1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_STOP_TYPE:
      printf("EMC_SPINDLE_STOP\n");
      emcioStatus.spindle.direction = 0;
      emcioStatus.spindle.brake = 1;
      break;

    case EMC_SPINDLE_INCREASE_TYPE:
      printf("EMC_SPINDLE_INCREASE\n");
      emcioStatus.spindle.increasing = 1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_DECREASE_TYPE:
      printf("EMC_SPINDLE_DECREASE\n");
      emcioStatus.spindle.increasing = -1;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_CONSTANT_TYPE:
      printf("EMC_SPINDLE_CONSTANT\n");
      emcioStatus.spindle.increasing = 0;
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_BRAKE_RELEASE_TYPE:
      printf("EMC_SPINDLE_BRAKE_RELEASE\n");
      emcioStatus.spindle.brake = 0;
      break;

    case EMC_SPINDLE_BRAKE_ENGAGE_TYPE:
      printf("EMC_SPINDLE_BRAKE_ENGAGE\n");
      emcioStatus.spindle.brake = 1;
      break;

    case EMC_COOLANT_INIT_TYPE:
      printf("EMC_COOLANT_INIT\n");
      emcioStatus.coolant.mist = 0;
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_COOLANT_HALT_TYPE:
      printf("EMC_COOLANT_HALT\n");
      emcioStatus.coolant.mist = 0;
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_COOLANT_ABORT_TYPE:
      printf("EMC_COOLANT_ABORT\n");
      emcioStatus.coolant.mist = 0;
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_COOLANT_MIST_ON_TYPE:
      printf("EMC_COOLANT_MIST_ON\n");
      emcioStatus.coolant.mist = 1;
      break;

    case EMC_COOLANT_MIST_OFF_TYPE:
      printf("EMC_COOLANT_MIST_OFF\n");
      emcioStatus.coolant.mist = 0;
      break;

    case EMC_COOLANT_FLOOD_ON_TYPE:
      printf("EMC_COOLANT_FLOOD_ON\n");
      emcioStatus.coolant.flood = 1;
      break;

    case EMC_COOLANT_FLOOD_OFF_TYPE:
      printf("EMC_COOLANT_FLOOD_OFF\n");
      emcioStatus.coolant.flood = 0;
      break;

    case EMC_AUX_INIT_TYPE:
      printf("EMC_AUX_INIT\n");
      emcioStatus.aux.estop = 1;
      emcioStatus.aux.estopIn = 0;
      break;

    case EMC_AUX_HALT_TYPE:
      printf("EMC_AUX_HALT\n");
      emcioStatus.aux.estop = 1;
      emcioStatus.aux.estopIn = 0;
      break;

    case EMC_AUX_ABORT_TYPE:
      printf("EMC_AUX_ABORT\n");
      emcioStatus.aux.estop = 1;
      emcioStatus.aux.estopIn = 0;
      break;

    case EMC_AUX_ESTOP_ON_TYPE:
      printf("EMC_AUX_ESTOP_ON\n");
      emcioStatus.aux.estop = 1;
      break;

    case EMC_AUX_ESTOP_OFF_TYPE:
      printf("EMC_AUX_ESTOP_OFF\n");
      emcioStatus.aux.estop = 0;
      break;

    case EMC_LUBE_INIT_TYPE:
      printf("EMC_LUBE_INIT\n");
      emcioStatus.lube.on = 0;
      emcioStatus.lube.level = 1;
      break;

    case EMC_LUBE_HALT_TYPE:
      printf("EMC_LUBE_HALT\n");
      emcioStatus.lube.on = 0;
      emcioStatus.lube.level = 1;
      break;

    case EMC_LUBE_ABORT_TYPE:
      printf("EMC_LUBE_ABORT\n");
      emcioStatus.lube.on = 0;
      emcioStatus.lube.level = 1;
      break;

    case EMC_LUBE_ON_TYPE:
      printf("EMC_LUBE_ON\n");
      emcioStatus.lube.on = 1;
      break;

    case EMC_LUBE_OFF_TYPE:
      printf("EMC_LUBE_OFF\n");
      emcioStatus.lube.on = 0;
      break;

    default:
      rcs_print("unknown command %s\n", emcSymbolLookup(type));
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
