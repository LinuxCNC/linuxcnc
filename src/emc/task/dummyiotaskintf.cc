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
********************************************************************/

#include <math.h>		// fabs()
#include <float.h>		// DBL_MAX
#include <string.h>		// memcpy() strncpy()
#include <stdlib.h>		// malloc()

#include "rcs.hh"		// RCS_CMD_CHANNEL, etc.
#include "rcs_print.hh"
#include "timer.hh"             // esleep, etc.
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "inifile.hh"
//#include "initool.hh"		// these decls
#include "tool_parse.h"
#include "emcglb.h"		// EMC_INIFILE

#include "initool.hh"

// IO INTERFACE

// the NML channels to the EMCIO controller
static RCS_CMD_CHANNEL *emcIoCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcIoStatusBuffer = 0;

// global status structure
EMC_IO_STAT *emcIoStatus = 0;

// serial number for communication
static int emcIoCommandSerialNumber = 0;
static double EMCIO_BUFFER_GET_TIMEOUT = 5.0;

static int forceCommand(RCS_CMD_MSG *msg);

static int random_toolchanger;
static char *ttcomments[CANON_POCKETS_MAX];
static int fms[CANON_POCKETS_MAX];
static char *progname = "task-dummy";

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
		new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc",
				    EMC_NMLFILE);
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
static long largest_io_command_size = 0;

/*
  sendCommand() waits until any currently executing command has finished,
  then writes the given command.*/
/*! \todo
  FIXME: Not very RCS-like to wait for status done here. (wps)
*/
static int sendCommand(RCS_CMD_MSG * msg)
{
    return 0;

    // need command buffer to be there
    if (0 == emcIoCommandBuffer) {
	return -1;
    }
    // need status buffer also, to check for command received
    if (0 == emcIoStatusBuffer || !emcIoStatusBuffer->valid()) {
	return -1;
    }

    // always force-queue an abort
    if (msg->type == EMC_TOOL_ABORT_TYPE) {
	// just queue the abort and call it a day
	int rc = forceCommand(msg);
	if (rc) {
	    rcs_print_error("forceCommand(EMC_TOOL_ABORT) returned %d\n", rc);
	}
	return 0;
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
			emcSymbolLookup(msg->type),
			emcIoCommandBuffer->msg2str(msg));
	return -1;
    }

    if (largest_io_command_size < msg->size) {
	largest_io_command_size = std::max<long>(msg->size, 4096);
	last_io_command = (RCS_CMD_MSG *) realloc(last_io_command, largest_io_command_size);
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
    return 0;

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
			emcSymbolLookup(msg->type),
			emcIoCommandBuffer->msg2str(msg));
	return -1;
    }

    if (largest_io_command_size < msg->size) {
	largest_io_command_size = std::max<long>(msg->size, 4096);
	last_io_command = (RCS_CMD_MSG *) realloc(last_io_command, largest_io_command_size);
    }

    if (0 != last_io_command) {
	memcpy(last_io_command, msg, msg->size);
    }

    return 0;
}
/*
  readToolChange() reads the values of [EMCIO] TOOL_CHANGE_POSITION and
  TOOL_HOLDER_CLEAR, and loads them into their associated globals
*/
static int readToolChange(IniFile *toolInifile)
{
    int retval = 0;
    const char *inistring;

    if (NULL !=
	(inistring = toolInifile->Find("TOOL_CHANGE_POSITION", "EMCIO"))) {
	/* found an entry */
        if (9 == sscanf(inistring, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
                        &TOOL_CHANGE_POSITION.tran.x,
                        &TOOL_CHANGE_POSITION.tran.y,
                        &TOOL_CHANGE_POSITION.tran.z,
                        &TOOL_CHANGE_POSITION.a,
                        &TOOL_CHANGE_POSITION.b,
                        &TOOL_CHANGE_POSITION.c,
                        &TOOL_CHANGE_POSITION.u,
                        &TOOL_CHANGE_POSITION.v,
                        &TOOL_CHANGE_POSITION.w)) {
            HAVE_TOOL_CHANGE_POSITION=1;
            retval=0;
        } else if (6 == sscanf(inistring, "%lf %lf %lf %lf %lf %lf",
                        &TOOL_CHANGE_POSITION.tran.x,
                        &TOOL_CHANGE_POSITION.tran.y,
                        &TOOL_CHANGE_POSITION.tran.z,
                        &TOOL_CHANGE_POSITION.a,
                        &TOOL_CHANGE_POSITION.b,
                        &TOOL_CHANGE_POSITION.c)) {
	    TOOL_CHANGE_POSITION.u = 0.0;
	    TOOL_CHANGE_POSITION.v = 0.0;
	    TOOL_CHANGE_POSITION.w = 0.0;
            HAVE_TOOL_CHANGE_POSITION = 1;
            retval = 0;
        } else if (3 == sscanf(inistring, "%lf %lf %lf",
                               &TOOL_CHANGE_POSITION.tran.x,
                               &TOOL_CHANGE_POSITION.tran.y,
                               &TOOL_CHANGE_POSITION.tran.z)) {
	    /* read them OK */
	    TOOL_CHANGE_POSITION.a = 0.0;
	    TOOL_CHANGE_POSITION.b = 0.0;
	    TOOL_CHANGE_POSITION.c = 0.0;
	    TOOL_CHANGE_POSITION.u = 0.0;
	    TOOL_CHANGE_POSITION.v = 0.0;
	    TOOL_CHANGE_POSITION.w = 0.0;
	    HAVE_TOOL_CHANGE_POSITION = 1;
	    retval = 0;
	} else {
	    /* bad format */
	    rcs_print("bad format for TOOL_CHANGE_POSITION\n");
	    HAVE_TOOL_CHANGE_POSITION = 0;
	    retval = -1;
	}
    } else {
	/* didn't find an entry */
	HAVE_TOOL_CHANGE_POSITION = 0;
    }

    if (NULL !=
	(inistring = toolInifile->Find("TOOL_HOLDER_CLEAR", "EMCIO"))) {
	/* found an entry */
	if (3 == sscanf(inistring, "%lf %lf %lf",
			&TOOL_HOLDER_CLEAR.tran.x,
			&TOOL_HOLDER_CLEAR.tran.y,
			&TOOL_HOLDER_CLEAR.tran.z)) {
	    /* read them OK */
	    TOOL_HOLDER_CLEAR.a = 0.0;	// not supporting ABC for now
	    TOOL_HOLDER_CLEAR.b = 0.0;
	    TOOL_HOLDER_CLEAR.c = 0.0;
	    HAVE_TOOL_HOLDER_CLEAR = 1;
	    retval = 0;
	} else {
	    /* bad format */
	    rcs_print("bad format for TOOL_HOLDER_CLEAR\n");
	    HAVE_TOOL_HOLDER_CLEAR = 0;
	    retval = -1;
	}
    } else {
	/* didn't find an entry */
	HAVE_TOOL_HOLDER_CLEAR = 0;
    }

    return retval;
}

/*
  loadTool()

  Loads ini file params for spindle from [EMCIO] section

  TOOL_TABLE <file name>  name of tool table file

  calls:

  emcToolSetToolTableFile(const char filename);
  */

static int loadTool(IniFile *toolInifile)
{
    int retval = 0;
    const char *inistring;

    if (NULL != (inistring = toolInifile->Find("TOOL_TABLE", "EMCIO"))) {
	if (0 != emcToolSetToolTableFile(inistring)) {
	    rcs_print("bad return value from emcToolSetToolTableFile\n");
	    retval = -1;
	}
    }
    // else ignore omission

    return retval;
}

/*
  iniTool(const char *filename)

  Loads ini file parameters for tool controller, from [EMCIO] section
 */
int DummyiniTool(const char *filename)
{
    int retval = 0;
    IniFile toolInifile;
    const char *s;

    if (toolInifile.Open(filename) == false) {
	return -1;
    }
    toolInifile.Find(&random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");
    if ((s =toolInifile.Find("TOOL_TABLE", "EMCIO")) != NULL)
	strcpy(TOOL_TABLE_FILE,s);

    // load tool values
    if (0 != loadTool(&toolInifile)) {
	retval = -1;
    }
    // read the tool change positions
    if (0 != readToolChange(&toolInifile)) {
	retval = -1;
    }
    // close the inifile
    toolInifile.Close();

    return retval;
}

// NML commands
int emcIoInit()
{


    EMC_TOOL_INIT ioInitMsg;
    fprintf(stderr,"emcIOInit() start\n");
    // get NML buffer to emcio
    if (0 != emcioNmlGet()) {
	rcs_print_error("emcioNmlGet() failed.\n");
	return -1;
    }

    if (0 != DummyiniTool(EMC_INIFILE)) {
	return -1;
    }
   for(int i = 0; i < CANON_POCKETS_MAX; i++) {
	ttcomments[i] = (char *)malloc(CANON_TOOL_ENTRY_LEN);
    }

    // on nonrandom machines, always start by assuming the spindle is empty
    if(!random_toolchanger) {
	emcIoStatus->tool.toolTable[0].toolno = -1;
	ZERO_EMC_POSE(emcIoStatus->tool.toolTable[0].offset);
	emcIoStatus->tool.toolTable[0].diameter = 0.0;
	emcIoStatus->tool.toolTable[0].frontangle = 0.0;
	emcIoStatus->tool.toolTable[0].backangle = 0.0;
	emcIoStatus->tool.toolTable[0].orientation = 0;
	fms[0] = 0;
	ttcomments[0][0] = '\0';
    }

    if (0 != loadToolTable(TOOL_TABLE_FILE, emcIoStatus->tool.toolTable,
			   fms, ttcomments, random_toolchanger)) {
	rcs_print_error("%s: can't load tool table.\n",progname);
    }
    /* set status values to 'normal' */
    emcIoStatus->aux.estop = 1; // estop=1 means to emc that ESTOP condition is met
    emcIoStatus->tool.pocketPrepped = -1;
    emcIoStatus->tool.toolInSpindle = 0;
    emcIoStatus->coolant.mist = 0;
    emcIoStatus->coolant.flood = 0;
    emcIoStatus->lube.on = 0;
    emcIoStatus->lube.level = 1;



    // send init command to emcio
    if (forceCommand(&ioInitMsg)) {
	rcs_print_error("Can't forceCommand(ioInitMsg)\n");
	return -1;
    }


    fprintf(stderr,"emcIOInit() done\n");
    return 0;
}

int emcIoHalt()
{
    EMC_TOOL_HALT ioHaltMsg;
    fprintf(stderr,"emcIOHalt() start\n");
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

    if (last_io_command) {
        free(last_io_command);
        last_io_command = 0;
    }
    fprintf(stderr,"emcIOHalt() done\n");
    return 0;
}

int emcIoAbort(int reason)
{
    EMC_TOOL_ABORT ioAbortMsg;

    ioAbortMsg.reason = reason;
    // send abort command to emcio
    sendCommand(&ioAbortMsg);

    // this is too early - must execute AFTER interplist has been cleared
    // call abort o-word sub handler if defined
    // emcAbortCleanup(reason);

    fprintf(stderr,"emcIOAbort()\n");
    return 0;
}

int emcIoSetDebug(int debug)
{
    EMC_SET_DEBUG ioDebugMsg;

    ioDebugMsg.debug = debug;
    fprintf(stderr,"emcIoSetDebig(%d)\n",debug);
    return sendCommand(&ioDebugMsg);
}

int emcAuxEstopOn()
{
    EMC_AUX_ESTOP_ON estopOnMsg;
    fprintf(stderr,"emcAuxEstopOn()\n");
    return forceCommand(&estopOnMsg);
}

int emcAuxEstopOff()
{
    EMC_AUX_ESTOP_OFF estopOffMsg;
    fprintf(stderr,"emcAuxEstopOff()\n");
    emcIoStatus->aux.estop = 0;
    return forceCommand(&estopOffMsg); //force the EstopOff message
}

int emcCoolantMistOn()
{
    EMC_COOLANT_MIST_ON mistOnMsg;
    fprintf(stderr,"emcCoolantMistOn()\n");
    sendCommand(&mistOnMsg);
    emcIoStatus->coolant.mist = 1;

    return 0;
}

int emcCoolantMistOff()
{
    EMC_COOLANT_MIST_OFF mistOffMsg;

    sendCommand(&mistOffMsg);
    fprintf(stderr,"emcCoolantMistOff()\n");
    emcIoStatus->coolant.mist = 1;

    return 0;
}

int emcCoolantFloodOn()
{
    EMC_COOLANT_FLOOD_ON floodOnMsg;

    sendCommand(&floodOnMsg);
    fprintf(stderr,"emcCoolantFloodOn()\n");
    emcIoStatus->coolant.flood = 1;

    return 0;
}

int emcCoolantFloodOff()
{
    EMC_COOLANT_FLOOD_OFF floodOffMsg;

    sendCommand(&floodOffMsg);
    emcIoStatus->coolant.flood = 0;

    return 0;
}

int emcLubeOn()
{
    EMC_LUBE_ON lubeOnMsg;

    sendCommand(&lubeOnMsg);
    fprintf(stderr,"emcLubeOn()\n");
    emcIoStatus->lube.on = 1;
    emcIoStatus->lube.level = 1;
    return 0;
}

int emcLubeOff()
{
    EMC_LUBE_OFF lubeOffMsg;

    sendCommand(&lubeOffMsg);
    fprintf(stderr,"emcLubeOff()\n");
    emcIoStatus->lube.on = 0;

    return 0;
}

/********************************************************************
 *
 * Description: saveToolTable(const char *filename, CANON_TOOL_TABLE toolTable[])
 *		Saves the tool table from toolTable[] array into file filename.
 *		  Array is CANON_TOOL_MAX + 1 entries, since 0 is included.
 *
 * Return Value: Zero on success or -1 if file not found.
 *
 * Side Effects: Default setting used if the parameter not found in
 *		the ini file.
 *
 * Called By: main()
 *
 ********************************************************************/
static int saveToolTable(const char *filename,
			 CANON_TOOL_TABLE toolTable[])
{
    int pocket;
    FILE *fp;
    const char *name;
    int start_pocket;

    // check filename
    if (filename[0] == 0) {
	name = TOOL_TABLE_FILE;
    } else {
	// point to name provided
	name = filename;
    }

    // open tool table file
    if (NULL == (fp = fopen(name, "w"))) {
	// can't open file
	return -1;
    }

    if(random_toolchanger) {
	start_pocket = 0;
    } else {
	start_pocket = 1;
    }
    for (pocket = start_pocket; pocket < CANON_POCKETS_MAX; pocket++) {
	if (toolTable[pocket].toolno != -1) {
	    fprintf(fp, "T%d P%d", toolTable[pocket].toolno, random_toolchanger? pocket: fms[pocket]);
	    if (toolTable[pocket].diameter) fprintf(fp, " D%f", toolTable[pocket].diameter);
	    if (toolTable[pocket].offset.tran.x) fprintf(fp, " X%+f", toolTable[pocket].offset.tran.x);
	    if (toolTable[pocket].offset.tran.y) fprintf(fp, " Y%+f", toolTable[pocket].offset.tran.y);
	    if (toolTable[pocket].offset.tran.z) fprintf(fp, " Z%+f", toolTable[pocket].offset.tran.z);
	    if (toolTable[pocket].offset.a) fprintf(fp, " A%+f", toolTable[pocket].offset.a);
	    if (toolTable[pocket].offset.b) fprintf(fp, " B%+f", toolTable[pocket].offset.b);
	    if (toolTable[pocket].offset.c) fprintf(fp, " C%+f", toolTable[pocket].offset.c);
	    if (toolTable[pocket].offset.u) fprintf(fp, " U%+f", toolTable[pocket].offset.u);
	    if (toolTable[pocket].offset.v) fprintf(fp, " V%+f", toolTable[pocket].offset.v);
	    if (toolTable[pocket].offset.w) fprintf(fp, " W%+f", toolTable[pocket].offset.w);
	    if (toolTable[pocket].frontangle) fprintf(fp, " I%+f", toolTable[pocket].frontangle);
	    if (toolTable[pocket].backangle) fprintf(fp, " J%+f", toolTable[pocket].backangle);
	    if (toolTable[pocket].orientation) fprintf(fp, " Q%d", toolTable[pocket].orientation);
	    fprintf(fp, " ;%s\n", ttcomments[pocket]);
	}
    }

    fclose(fp);
    return 0;
}
void load_tool(int pocket) {
    if(random_toolchanger) {
	// swap the tools between the desired pocket and the spindle pocket
	CANON_TOOL_TABLE temp;
	char *comment_temp;

	temp = emcIoStatus->tool.toolTable[0];
	emcIoStatus->tool.toolTable[0] = emcIoStatus->tool.toolTable[pocket];
	emcIoStatus->tool.toolTable[pocket] = temp;

	comment_temp = ttcomments[0];
	ttcomments[0] = ttcomments[pocket];
	ttcomments[pocket] = comment_temp;

	if (0 != saveToolTable(TOOL_TABLE_FILE, emcIoStatus->tool.toolTable))
	    emcIoStatus->status = RCS_ERROR;
    } else if (pocket == 0) {
	// magic T0 = pocket 0 = no tool
	emcIoStatus->tool.toolTable[0].toolno = -1;
	ZERO_EMC_POSE(emcIoStatus->tool.toolTable[0].offset);
	emcIoStatus->tool.toolTable[0].diameter = 0.0;
	emcIoStatus->tool.toolTable[0].frontangle = 0.0;
	emcIoStatus->tool.toolTable[0].backangle = 0.0;
	emcIoStatus->tool.toolTable[0].orientation = 0;
    } else {
	// just copy the desired tool to the spindle
	emcIoStatus->tool.toolTable[0] = emcIoStatus->tool.toolTable[pocket];
    }
}
int emcToolPrepare(int tool)
{
    EMC_TOOL_PREPARE toolPrepareMsg;

    // toolPrepareMsg.tool = tool;
    // sendCommand(&toolPrepareMsg);
    fprintf(stderr,"emcToolPrepare(%d)\n",tool);

    int p = tool; // ((EMC_TOOL_PREPARE*)emcioCommand)->tool;

    // it doesn't make sense to prep the spindle pocket
    if (random_toolchanger && p == 0)
	return 0;


	    /* set tool number first */
    int prep_number;
    //*(iocontrol_data->tool_prep_pocket) = p;
    if (!random_toolchanger && p == 0) {
	//	*(iocontrol_data->tool_prep_number) = 0;
	prep_number = 0;
    } else {
	// *(iocontrol_data->tool_prep_number) = emcIoStatus->tool.toolTable[p].toolno;
	prep_number = emcIoStatus->tool.toolTable[p].toolno;
    }
    fprintf(stderr,"emcToolPrepare: raise prepare, prep_number=%d, wait for prepared\n",prep_number);

    emcIoStatus->tool.pocketPrepped = p; // *(iocontrol_data->tool_prep_pocket); //check if tool has been prepared
	emcIoStatus->status = RCS_DONE;

    // *(iocontrol_data->tool_prepare) = 0;

    // if ((proto > V1) && *(iocontrol_data->toolchanger_faulted)) { // informational
    // 	rtapi_print_msg(RTAPI_MSG_DBG, "%s: prepare: toolchanger faulted (reason=%d), next M6 will %s\n",
    // 			progname,toolchanger_reason,
    // 			toolchanger_reason > 0 ? "set fault code and reason" : "abort program");
    // }
    // // then set the prepare pin to tell external logic to get started
    // *(iocontrol_data->tool_prepare) = 1;
    // *(iocontrol_data->state) = ST_PREPARING;

    // // delay fetching the next message until prepare done
    // if (!(input_status & TI_PREPARE_COMPLETE)) {
    // 	emcIoStatus->status = RCS_EXEC;
    // }



    return 0;
}


int emcToolStartChange()
{
    EMC_TOOL_START_CHANGE toolStartChangeMsg;

    sendCommand(&toolStartChangeMsg);
    fprintf(stderr,"emcToolStartChange()\n");

    return 0;
}


int emcToolLoad()
{
    EMC_TOOL_LOAD toolLoadMsg;

    sendCommand(&toolLoadMsg);
    fprintf(stderr,"emcToolLoad()\n");
    // it doesn't make sense to load a tool from the spindle pocket
    if (random_toolchanger && emcIoStatus->tool.pocketPrepped == 0) {
	return 0;
    }

    // it's not necessary to load the tool already in the spindle
    if (!random_toolchanger && emcIoStatus->tool.pocketPrepped > 0 &&
	emcIoStatus->tool.toolInSpindle == emcIoStatus->tool.toolTable[emcIoStatus->tool.pocketPrepped].toolno) {
		return 0;
    }

    if (emcIoStatus->tool.pocketPrepped != -1) {
	fprintf(stderr,"emcToolLoad() raise change, wait for changed\n");

	// Assume changed=true:
        if(!random_toolchanger && emcIoStatus->tool.pocketPrepped == 0) {
            emcIoStatus->tool.toolInSpindle = 0;
        } else {
            // the tool now in the spindle is the one that was prepared
            emcIoStatus->tool.toolInSpindle = emcIoStatus->tool.toolTable[emcIoStatus->tool.pocketPrepped].toolno;
        }
	// *(iocontrol_data->tool_number) = emcIoStatus->tool.toolInSpindle; //likewise in HAL
	load_tool(emcIoStatus->tool.pocketPrepped);
	emcIoStatus->tool.pocketPrepped = -1; //reset the tool preped number, -1 to permit tool 0 to be loaded
	// *(iocontrol_data->tool_prep_number) = 0; //likewise in HAL
	// *(iocontrol_data->tool_prep_pocket) = 0; //likewise in HAL
	// *(iocontrol_data->tool_change) = 0; //also reset the tool change signal
	emcIoStatus->status = RCS_DONE;	// we finally finished to do tool-changing, signal task with RCS_DONE
	// return 11; //change finished

		// //notify HW for toolchange
		// *(iocontrol_data->tool_change) = 1;
		// *(iocontrol_data->state) = ST_CHANGING;

		// // delay fetching the next message until change done
		// if (! (input_status & TI_CHANGE_COMPLETE)) {
		//     emcIoStatus->status = RCS_EXEC;
		// }
	emcIoStatus->tool.pocketPrepped = -1; // reset the tool prepped number, -1 to permit tool 0 to be loaded

    }
    return 0;
}

int emcToolUnload()
{
    EMC_TOOL_UNLOAD toolUnloadMsg;

    sendCommand(&toolUnloadMsg);
    fprintf(stderr,"emcToolUnLoad()\n");
    emcIoStatus->tool.toolInSpindle = 0;

    return 0;
}

void reload_tool_number(int toolno) {
    if(random_toolchanger) return; // doesn't need special handling here
    for(int i=1; i<CANON_POCKETS_MAX; i++) {
	if(emcIoStatus->tool.toolTable[i].toolno == toolno) {
	    load_tool(i);
	    break;
	}
    }
}

int emcToolLoadToolTable(const char *file)
{
    EMC_TOOL_LOAD_TOOL_TABLE toolLoadToolTableMsg;

    strcpy(toolLoadToolTableMsg.file, file);

    sendCommand(&toolLoadToolTableMsg);

    const char *filename = file;

    fprintf(stderr,"emcLoadToolTable()\n");
    if (!strlen(filename)) filename = TOOL_TABLE_FILE;
    if (0 != loadToolTable(filename, emcIoStatus->tool.toolTable,
			   fms, ttcomments, random_toolchanger))
	emcIoStatus->status = RCS_ERROR;
    else
	reload_tool_number(emcIoStatus->tool.toolInSpindle);
    return 0;
}

int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
                     double frontangle, double backangle, int orientation)
{


    fprintf(stderr,"emcToolSetOffset(): pocket=%d toolno=%d zoffset=%lf, xoffset=%lf, diameter=%lf,"
	    " frontangle=%lf, backangle=%lf, orientation=%d\n",
	    pocket, toolno, offset.tran.z, offset.tran.x, diameter, frontangle,
	    backangle, orientation);

    emcIoStatus->tool.toolTable[pocket].toolno = toolno;
    emcIoStatus->tool.toolTable[pocket].offset = offset;
    emcIoStatus->tool.toolTable[pocket].diameter = diameter;
    emcIoStatus->tool.toolTable[pocket].frontangle = frontangle;
    emcIoStatus->tool.toolTable[pocket].backangle = backangle;
    emcIoStatus->tool.toolTable[pocket].orientation = orientation;

    if (emcIoStatus->tool.toolInSpindle == toolno) {
	emcIoStatus->tool.toolTable[0] = emcIoStatus->tool.toolTable[pocket];
    }

    if (0 != saveToolTable(TOOL_TABLE_FILE, emcIoStatus->tool.toolTable))
	emcIoStatus->status = RCS_ERROR;
    return 0;
}

int emcToolSetNumber(int number)
{
    EMC_TOOL_SET_NUMBER toolSetNumberMsg;

    toolSetNumberMsg.tool = number;

    sendCommand(&toolSetNumberMsg);
    fprintf(stderr,"emcToolSetNumber(%d)\n",number);
    emcIoStatus->tool.toolInSpindle = emcIoStatus->tool.toolTable[number].toolno;
    load_tool(number);
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

    // force done -mah
    stat->status = RCS_DONE;
    return 0;

    if (stat->echo_serial_number != emcIoCommandSerialNumber) {
	stat->status = RCS_EXEC;
    }
    //commented out because it keeps resetting the spindle speed to some odd value
    //the speed gets set by the IO controller, no need to override it here (io takes care of increase/decrease speed too)
    // stat->spindle.speed = spindleSpeed;

    return 0;
}
