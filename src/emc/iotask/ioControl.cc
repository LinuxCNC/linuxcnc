/********************************************************************
* Description: IoControl.cc
*           Simply accepts NML messages sent to the IO controller
*           outputs those to a HAL pin,
*           and sends back a "Done" message.
*
*  ESTOP logic:  this module exports three HAL pins related to ESTOP.
*  The first is estop-in.  It is an input from the HAL, when TRUE,
*  EMC will go into the STOPPED state (regardless of the state of
*  the other two pins).  When it goes FALSE, EMC will go into the
*  ESTOP_RESET state (also known as READY).
*
*  The second HAL pin is an output to the HAL.  It is controlled by
*  the NML messages ESTOP_ON and ESTOP_OFF, which normally result from
*  user actions at the GUI.  For the simplest system, loop estop-out 
*  back to estop-in in the HAL.  The GUI controls estop-out, and EMC
*  responds to that once it is looped back.
*
*  If external _mainteined_ ESTOP inputs are desired, they can be
*  ORed with estop-out and looped back to estop-in.  Finally, if
*  external momentary ESTOP inputs are desired, the HAL estop latch
*  component can be inserted in the loop.  In that case, the final
*  HAL pin, estop-reset, is used to reset the latch.  The estop-reset
*  pin generates a pulse (one io-period long) whenever the ESTOP-OFF
*  NML message is sent (usually from the user hitting F1 on the GUI).
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

#include "hal.h"		/* access to HAL functions/definitions */
#include "rtapi.h"		/* rtapi_print_msg */
#include "rcs.hh"		/* RCS_CMD_CHANNEL */
#include "emc.hh"		/* EMC NML */
#include "emcglb.h"		/* EMC_NMLFILE, EMC_INIFILE, TOOL_TABLE_FILE */
#include "inifile.hh"		/* INIFILE */
#include "initool.hh"		/* iniTool() */

static RCS_CMD_CHANNEL *emcioCommandBuffer = 0;
static RCS_CMD_MSG *emcioCommand = 0;
static RCS_STAT_CHANNEL *emcioStatusBuffer = 0;
static EMC_IO_STAT emcioStatus;
static NML *emcErrorBuffer = 0;

struct iocontrol_str {
    hal_bit_t *estop_out;	/* output, TRUE when EMC wants stop */
    hal_bit_t *estop_in;	/* input, TRUE on any stop */
    hal_bit_t *estop_reset;	/* output, used to reset HAL latch */
    hal_bit_t *coolant_mist;	/* coolant mist output pin */
    hal_bit_t *coolant_flood;	/* coolant flood output pin */
    hal_bit_t *lube;		/* lube output pin */
    hal_bit_t *lube_level;	/* lube level input pin */


    // the following pins are needed for toolchanging
    //tool-prepare
    hal_bit_t *tool_prepare;	/* output, pin that notifies HAL it needs to prepare a tool */
    hal_u8_t  *tool_prep_number;/* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    hal_bit_t *tool_prepared;	/* input, pin that notifies that the tool has been prepared */
    //tool-change
    hal_bit_t *tool_change;	/* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    hal_bit_t *tool_changed;	/* input, notifies tool has been changed */

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

} * iocontrol_data;			//pointer to the HAL-struct

//static iocontrol_struct *iocontrol_data;	
static int comp_id;				/* component ID */

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
	emcioCommandBuffer =
	    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "tool", EMC_NMLFILE);
	if (!emcioCommandBuffer->valid()) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "emcToolCmd buffer not available\n");
	    delete emcioCommandBuffer;
	    emcioCommandBuffer = 0;
	    retval = -1;
	} else {
	    /* Get our command data structure */
	    emcioCommand = emcioCommandBuffer->get_address();
	}
    }

    /* try to connect to EMC IO status buffer */
    if (emcioStatusBuffer == 0) {
	emcioStatusBuffer =
	    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "tool",
				 EMC_NMLFILE);
	if (!emcioStatusBuffer->valid()) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "toolSts buffer not available\n");
	    delete emcioStatusBuffer;
	    emcioStatusBuffer = 0;
	    retval = -1;
	} else {
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
	emcErrorBuffer =
	    new NML(nmlErrorFormat, "emcError", "tool", EMC_NMLFILE);
	if (!emcErrorBuffer->valid()) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "emcError buffer not available\n");
	    delete emcErrorBuffer;
	    emcErrorBuffer = 0;
	    retval = -1;
	}
    }

    return retval;
}

static int iniLoad(const char *filename)
{
    Inifile inifile;
    const char *inistring;
    char version[LINELEN];

    /* Open the ini file */
    if (inifile.open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = inifile.find("DEBUG", "EMC"))) {
	/* copy to global */
	if (1 != sscanf(inistring, "%i", &EMC_DEBUG)) {
	    EMC_DEBUG = 0;
	}
    } else {
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
    } else {
	// not found, use default
    }

    double temp;
    temp = EMC_IO_CYCLE_TIME;
    if (NULL != (inistring = inifile.find("CYCLE_TIME", "EMCIO"))) {
	if (1 == sscanf(inistring, "%lf", &EMC_IO_CYCLE_TIME)) {
	    // found it
	} else {
	    // found, but invalid
	    EMC_IO_CYCLE_TIME = temp;
	    rtapi_print
		("invalid [EMCIO] CYCLE_TIME in %s (%s); using default %f\n",
		 filename, inistring, EMC_IO_CYCLE_TIME);
	}
    } else {
	// not found, using default
	rtapi_print
	    ("[EMCIO] CYCLE_TIME not found in %s; using default %f\n",
	     filename, EMC_IO_CYCLE_TIME);
    }

    // close it
    inifile.close();

    return 0;
}

/********************************************************************
*
* Description: loadToolTable(const char *filename, CANON_TOOL_TABLE toolTable[])
*		Loads the tool table from file filename into toolTable[] array.
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
static int loadToolTable(const char *filename,
			 CANON_TOOL_TABLE toolTable[])
{
    int t;
    FILE *fp;
    char buffer[CANON_TOOL_ENTRY_LEN];
    const char *name;

    // check filename
    if (filename[0] == 0) {
	name = TOOL_TABLE_FILE;
    } else {
	// point to name provided
	name = filename;
    }

    //AJ: for debug reasons
    //rtapi_print("loadToolTable called with %s\n", filename);

    // open tool table file
    if (NULL == (fp = fopen(name, "r"))) {
	// can't open file
	fclose(fp);
	return -1;
    }
    // clear out tool table
    for (t = 0; t <= CANON_TOOL_MAX; t++) {
	// unused tools are 0, 0.0, 0.0
	toolTable[t].id = 0;
	toolTable[t].length = 0.0;
	toolTable[t].diameter = 0.0;
    }

    /*
       Override 0's with codes from tool file
       File format is:

       <header>
       <pocket # 0..CANON_TOOL_MAX> <FMS id> <length> <diameter>
       ...

     */

    // read and discard header
    if (NULL == fgets(buffer, 256, fp)) {
	// nothing in file at all
	rtapi_print("IO: toolfile exists, but is empty\n");
	fclose(fp);
	return -1;
    }

    while (!feof(fp)) {
	int pocket;
	int id;
	double length;
	double diameter;

	// just read pocket, ID, and length offset
	if (NULL == fgets(buffer, CANON_TOOL_ENTRY_LEN, fp)) {
	    break;
	}

	if (4 !=
	    sscanf(buffer, "%d %d %lf %lf", &pocket, &id, &length,
		   &diameter)) {
	    // bad entry-- skip
	    continue;
	} else {
	    if (pocket < 0 || pocket > CANON_TOOL_MAX) {
		continue;
	    } else {
		toolTable[pocket].id = id;
		toolTable[pocket].length = length;
		toolTable[pocket].diameter = diameter;
	    }
	}
    }

    // close the file
    fclose(fp);

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
    // write header
    fprintf(fp, "POC\tFMS\tLEN\t\tDIAM\n");

    for (pocket = 1; pocket <= CANON_TOOL_MAX; pocket++) {
	fprintf(fp, "%d\t%d\t%f\t%f\n",
		pocket,
		toolTable[pocket].id,
		toolTable[pocket].length, toolTable[pocket].diameter);
    }

    // close the file
    fclose(fp);

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
* Description: iocontrol_hal_init(void)
*
* Side Effects: Exports HAL pins.
*
* Called By: main
********************************************************************/
int iocontrol_hal_init(void)
{
    char name[HAL_NAME_LEN + 2];	//name of the pin to be registered
    int n = 0, retval;		//n - number of the hal component (only one for iocotrol)

    /* STEP 1: initialise the hal component */
    comp_id = hal_init("iocontrol");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: hal_init() failed\n");
	return -1;
    }

    /* STEP 2: allocate shared memory for iocontrol data */
    iocontrol_data = (iocontrol_str *) hal_malloc(sizeof(iocontrol_str));
    if (iocontrol_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 3a: export the out-pin(s) */

    // estop-out
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.estop-out", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->estop_out), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin estop-out export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // estop-reset
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.estop-reset", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->estop_reset), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin estop-reset export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // coolant-flood
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.coolant-flood", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->coolant_flood),	comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin coolant-flood export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // coolant-mist
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.coolant-mist", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->coolant_mist),
			comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin coolant-mist export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // lube
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.lube", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->lube), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin lube export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // tool-prepare
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.tool-prepare", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->tool_prepare), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin tool-prepare export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // tool-number
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.tool-prep-number", n);
    retval =
	hal_pin_u8_new(name, HAL_WR, &(iocontrol_data->tool_prep_number), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin tool-prep-number export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // tool-prepared
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.tool-prepared", n);
    retval =
	hal_pin_bit_new(name, HAL_RD, &(iocontrol_data->tool_prepared), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin tool-prepared export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // tool-change
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.tool-change", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->tool_change), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin tool-change export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // tool-changed
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.tool-changed", n);
    retval =
	hal_pin_bit_new(name, HAL_RD, &(iocontrol_data->tool_changed), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin tool-changed export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-on
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-on", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->spindle_on), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-on export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-forward
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-forward", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->spindle_forward), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-forward export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-reverse
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-reverse", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->spindle_reverse), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-reverse export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-incr-speed
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-incr-speed",
		   n);
    retval =
	hal_pin_bit_new(name, HAL_WR,
			&(iocontrol_data->spindle_incr_speed), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-incr-speed export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-decr-speed
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-decr-speed",
		   n);
    retval =
	hal_pin_bit_new(name, HAL_WR,
			&(iocontrol_data->spindle_decr_speed), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-decr-speed export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-brake
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-brake", n);
    retval =
	hal_pin_bit_new(name, HAL_WR, &(iocontrol_data->spindle_brake),	comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-brake export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-speed-out
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-speed-out",
		   n);
    retval =
	hal_pin_float_new(name, HAL_WR,
			  &(iocontrol_data->spindle_speed_out), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-speed-out export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    /* STEP 3b: export the in-pin(s) */

    // estop-in
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.estop-in", n);
    retval =
	hal_pin_bit_new(name, HAL_RD, &(iocontrol_data->estop_in), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin estop-in export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // lube_level
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.lube_level", n);
    retval =
	hal_pin_bit_new(name, HAL_RD, &(iocontrol_data->lube_level), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin lube_level export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }
    // spindle-speed-in
    rtapi_snprintf(name, HAL_NAME_LEN, "iocontrol.%d.spindle-speed-in", n);
    retval =
	hal_pin_float_new(name, HAL_RD,
			  &(iocontrol_data->spindle_speed_in), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d pin spindle-speed-in export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }

    return 0;
}

/********************************************************************
*
* Description: hal_init_pins(void)
*
* Side Effects: Sets HAL pins default values.
*
* Called By: main
********************************************************************/
void hal_init_pins(void)
{
    *(iocontrol_data->estop_out)=1;		/* output, TRUE when EMC wants stop */
    *(iocontrol_data->estop_reset)=0;		/* output, used to reset HAL latch */
    *(iocontrol_data->coolant_mist)=0;		/* coolant mist output pin */
    *(iocontrol_data->coolant_flood)=0;		/* coolant flood output pin */
    *(iocontrol_data->lube)=0;			/* lube output pin */
    *(iocontrol_data->tool_prepare)=0;		/* output, pin that notifies HAL it needs to prepare a tool */
    *(iocontrol_data->tool_prep_number)=0;	/* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    *(iocontrol_data->tool_change)=0;		/* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    *(iocontrol_data->spindle_on)=0;		/* spindle spin output */
    *(iocontrol_data->spindle_forward)=0;	/* spindle spin-forward output */
    *(iocontrol_data->spindle_reverse)=0;	/* spindle spin-reverse output */
    *(iocontrol_data->spindle_incr_speed)=0;	/* spindle spin-increase output */
    *(iocontrol_data->spindle_decr_speed)=0;	/* spindle spin-decrease output */
    *(iocontrol_data->spindle_brake)=0;		/* spindle brake output */
    *(iocontrol_data->spindle_speed_out)=0.0;	/* spindle speed output */
}


/********************************************************************
*
* Description: read_hal_inputs(void)
*			Reads the pin values from HAL 
*			this function gets called once per cycle
*			It sets the values for the emcioStatus.aux.*
*
* Returns:	returns > 0 if any of the status has changed
*		we then need to update through NML
*
* Side Effects: updates values
*
* Called By: main every CYCLE
********************************************************************/
int read_hal_inputs(void)
{
    int oldval, retval = 0;

    oldval = emcioStatus.aux.estop;
    emcioStatus.aux.estop = *(iocontrol_data->estop_in); //check for estop from HW
    if (oldval != emcioStatus.aux.estop) {
	retval = 1;
    }
    
    
    oldval = emcioStatus.lube.level;
    emcioStatus.lube.level = *(iocontrol_data->lube_level);	//check for lube_level from HW
    if (oldval != emcioStatus.lube.level) {
	retval = 1;
    }
    return retval;
}


/********************************************************************
*
* Description: read_tool_inputs(void)
*			Reads the tool-pin values from HAL 
*			this function gets called once per cycle
*			It sets the values for the emcioStatus.aux.*
*
* Returns:	returns which of the status has changed
*		we then need to update through NML (a bit different as read_hal_inputs)
*
* Side Effects: updates values
*
* Called By: main every CYCLE
********************************************************************/
int read_tool_inputs(void)
{
    int oldval, retval = 0;

    oldval = emcioStatus.tool.toolPrepped;
    if ((*(iocontrol_data->tool_prepare) == 1) && (*(iocontrol_data->tool_prepared) == 1)) {
	emcioStatus.tool.toolPrepped = *(iocontrol_data->tool_prep_number); //check if tool has been prepared
	*(iocontrol_data->tool_prepare) = 0;
	emcioStatus.status = RCS_DONE;  // we finally finished to do tool-changing, signal task with RCS_DONE
	return 10; //prepped finished
    }
    
    oldval = emcioStatus.tool.toolInSpindle;
    if ((*(iocontrol_data->tool_change) != 0) && (*(iocontrol_data->tool_changed)==1)) {
	emcioStatus.tool.toolInSpindle = emcioStatus.tool.toolPrepped; //check if tool has been prepared
	emcioStatus.tool.toolPrepped = -1; //reset the tool preped number, -1 to permit tool 0 to be loaded
	*(iocontrol_data->tool_prep_number) = 0; //likewise in HAL
	*(iocontrol_data->tool_change) = 0; //also reset the tool change signal
	emcioStatus.status = RCS_DONE;	// we finally finished to do tool-changing, signal task with RCS_DONE
	return 11; //change finished
    }
    return retval;
}

/********************************************************************
*
* Description: main(int argc, char * argv[])
*		Connects to NML buffers and enters an endless loop
*		processing NML IO commands. Print statements are
*		sent to the console indicating which IO command was
*		executed if debug level is set to RTAPI_MSG_DBG.
*
* Return Value: Zero or -1 if ini file not found or failure to connect
*		to NML buffers.
*
* Side Effects: None.
*
* Called By:
*
********************************************************************/
int main(int argc, char *argv[])
{
    int t, tool_status;
    NMLTYPE type;

    for (t = 1; t < argc; t++) {
	if (!strcmp(argv[t], "-ini")) {
	    if (t == argc - 1) {
		return -1;
	    } else {
		strcpy(EMC_INIFILE, argv[t + 1]);
		t++;
	    }
	    continue;
	}
	/* do other args similarly here */
    }

    if (0 != iniLoad(EMC_INIFILE)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "can't open ini file %s\n",
			EMC_INIFILE);
	return -1;
    }

    if (0 != emcIoNmlGet()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"can't connect to NML buffers in %s\n",
			EMC_NMLFILE);
	return -1;
    }
    // used only for getting TOOL_TABLE_FILE out of the ini file
    if (0 != iniTool(EMC_INIFILE)) {
	rcs_print_error("iniTool failed.\n");
	return -1;
    }

    if (0 != loadToolTable(TOOL_TABLE_FILE, emcioStatus.tool.toolTable)) {
	rcs_print_error("can't load tool table.\n");
    }

    done = 0;
    /* Register the routine that catches the SIGINT signal */
    signal(SIGINT, quit);
    /* catch SIGTERM too - the run script uses it to shut things down */
    signal(SIGTERM, quit);

    if (iocontrol_hal_init() != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "can't initialize the HAL\n");
	return -1;
    }

    /* set status values to 'normal' */
    emcioStatus.aux.estop = 1;
    emcioStatus.tool.toolPrepped = -1;
    emcioStatus.tool.toolInSpindle = 0;
    emcioStatus.spindle.speed = 0.0;
    emcioStatus.spindle.direction = 0;
    emcioStatus.spindle.brake = 1;
    emcioStatus.spindle.increasing = 0;
    emcioStatus.spindle.enabled = 1;
    emcioStatus.coolant.mist = 0;
    emcioStatus.coolant.flood = 0;
    emcioStatus.lube.on = 0;
    emcioStatus.lube.level = 1;

    while (!done) {
	// check for inputs from HAL (updates emcioStatus)
	// returns 1 if any of the HAL pins changed from the last time we checked
	/* if an external ESTOP is activated (or another hal-pin has changed)
	   a NML message has to be pushed to EMC.
	   the way it was done status was only checked at the end of a command */
	if (read_hal_inputs() > 0) {
	    emcioStatus.command_type = EMC_IO_STAT_TYPE;
	    emcioStatus.echo_serial_number =
		emcioCommand->serial_number+1; //need for different serial number, because we are pushing a new message
	    emcioStatus.heartbeat++;
	    emcioStatusBuffer->write(&emcioStatus);
	}
	;
	if ( (tool_status = read_tool_inputs() ) > 0) { // in case of tool prep (or change) update, we only need to change the state (from RCS_EXEC
	    emcioStatus.command_type = EMC_IO_STAT_TYPE; // to RCS_DONE, no need for different serial_number
	    emcioStatus.echo_serial_number =
		emcioCommand->serial_number;
	    emcioStatus.heartbeat++;
	    emcioStatusBuffer->write(&emcioStatus);
	}

	/* read NML, run commands */
	if (-1 == emcioCommandBuffer->read()) {
	    /* bad command, wait until next cycle */
	    esleep(EMC_IO_CYCLE_TIME);
	    /* and repeat */
	    continue;
	}

	if (0 == emcioCommand ||	// bad command pointer
	    0 == emcioCommand->type ||	// bad command type
	    emcioCommand->serial_number == emcioStatus.echo_serial_number) {	// command already finished
	    /* wait until next cycle */
	    esleep(EMC_IO_CYCLE_TIME);
	    /* and repeat */
	    continue;
	}

	type = emcioCommand->type;

	emcioStatus.status = RCS_DONE;

	switch (type) {
	case 0:
	    break;

	case EMC_IO_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_IO_INIT\n");
	    hal_init_pins();
	    break;

	case EMC_TOOL_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_INIT\n");
	    loadToolTable(TOOL_TABLE_FILE, emcioStatus.tool.toolTable);
	    break;

	case EMC_TOOL_HALT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_HALT\n");
	    break;

	case EMC_TOOL_ABORT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT\n");
	    break;

	case EMC_TOOL_PREPARE_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE\n");
	    *(iocontrol_data->tool_prepare) = 1;
	    *(iocontrol_data->tool_prep_number) = ((EMC_TOOL_PREPARE *) emcioCommand)->tool;
	    // the feedback logic is done inside read_hal_inputs()
	    // we only need to set RCS_EXEC if RCS_DONE is not already set by the above logic
	    if (tool_status != 10) //set above to 10 in case PREP already finished (HAL loopback machine)
		emcioStatus.status = RCS_EXEC;
	    break;

	case EMC_TOOL_LOAD_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD\n");
	    if (emcioStatus.tool.toolPrepped != -1) {
		*(iocontrol_data->tool_change) = 1; //notify HW for toolchange
	    }
	    // the feedback logic is done inside read_hal_inputs()
	    // we only need to set RCS_EXEC if RCS_DONE is not already set by the above logic
	    if (tool_status != 11) //set above to 11 in case LOAD already finished (HAL loopback machine)
		emcioStatus.status = RCS_EXEC;
	    break;

	case EMC_TOOL_UNLOAD_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_UNLOAD\n");
	    emcioStatus.tool.toolInSpindle = 0;
	/*! \todo FIXME  - not sure about this NML message, does it get sent? when and why? */
	    break;

	case EMC_TOOL_LOAD_TOOL_TABLE_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD_TOOL_TABLE\n");
	    if (0 != loadToolTable(((EMC_TOOL_LOAD_TOOL_TABLE *) emcioCommand)->
			      file, emcioStatus.tool.toolTable))
		emcioStatus.status = RCS_ERROR;
	    break;

	case EMC_TOOL_SET_OFFSET_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG,
			    "EMC_TOOL_SET_OFFSET length=%lf diameter=%lf\n",
			    ((EMC_TOOL_SET_OFFSET *) emcioCommand)->length,
			    ((EMC_TOOL_SET_OFFSET *) emcioCommand)->diameter);
	    emcioStatus.tool.
		toolTable[((EMC_TOOL_SET_OFFSET *) emcioCommand)->tool].
		length = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->length;
	    emcioStatus.tool.
		toolTable[((EMC_TOOL_SET_OFFSET *) emcioCommand)->tool].
		diameter = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->diameter;
	    if (0 != saveToolTable(TOOL_TABLE_FILE, emcioStatus.tool.toolTable))
		emcioStatus.status = RCS_ERROR;
	    break;

	case EMC_SPINDLE_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_INIT\n");
	    emcioStatus.spindle.speed = 0.0;
	    emcioStatus.spindle.direction = 0;
	    emcioStatus.spindle.brake = 1;
	    emcioStatus.spindle.increasing = 0;
	    *(iocontrol_data->spindle_speed_out) = 0;
	    *(iocontrol_data->spindle_on) = 0;
	    *(iocontrol_data->spindle_forward) = 0;
	    *(iocontrol_data->spindle_reverse) = 0;
	    *(iocontrol_data->spindle_brake) = 1;
	    break;

	case EMC_SPINDLE_HALT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_HALT\n");
	    emcioStatus.spindle.speed = 0.0;
	    emcioStatus.spindle.direction = 0;
	    emcioStatus.spindle.brake = 1;
	    emcioStatus.spindle.increasing = 0;
	    *(iocontrol_data->spindle_speed_out) = 0;
	    *(iocontrol_data->spindle_on) = 0;
	    *(iocontrol_data->spindle_forward) = 0;
	    *(iocontrol_data->spindle_reverse) = 0;
	    *(iocontrol_data->spindle_brake) = 1;
	    break;

	case EMC_SPINDLE_ABORT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_ABORT\n");
	    emcioStatus.spindle.speed = 0.0;
	    emcioStatus.spindle.direction = 0;
	    emcioStatus.spindle.brake = 1;
	    emcioStatus.spindle.increasing = 0;
	    *(iocontrol_data->spindle_speed_out) = 0;
	    *(iocontrol_data->spindle_on) = 0;
	    *(iocontrol_data->spindle_forward) = 0;
	    *(iocontrol_data->spindle_reverse) = 0;
	    *(iocontrol_data->spindle_brake) = 1;
	    break;

	case EMC_SPINDLE_ON_TYPE:
	    emcioStatus.spindle.speed =
		((EMC_SPINDLE_ON *) emcioCommand)->speed;
	    emcioStatus.spindle.brake = 0;
	    emcioStatus.spindle.increasing = 0;
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_ON %f RPM\n",
			    emcioStatus.spindle.speed);
	    if (((EMC_SPINDLE_ON *) emcioCommand)->speed > 0) {
		emcioStatus.spindle.direction = 1;
		*(iocontrol_data->spindle_on) = 1;
		*(iocontrol_data->spindle_forward) = 1;
		*(iocontrol_data->spindle_reverse) = 0;
	    } else {
		emcioStatus.spindle.direction = -1;
		*(iocontrol_data->spindle_on) = 1;
		*(iocontrol_data->spindle_forward) = 0;
		*(iocontrol_data->spindle_reverse) = 1;
	    }
	    *(iocontrol_data->spindle_speed_out) =
		    ((EMC_SPINDLE_ON *) emcioCommand)->speed;
	    *(iocontrol_data->spindle_brake) = 0;
	    break;

	case EMC_SPINDLE_OFF_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_OFF\n");
	    emcioStatus.spindle.speed = 0.0;
	    emcioStatus.spindle.direction = 0;
	    emcioStatus.spindle.brake = 1;
	    emcioStatus.spindle.increasing = 0;
	    *(iocontrol_data->spindle_speed_out) = 0;
	    *(iocontrol_data->spindle_on) = 0;
	    *(iocontrol_data->spindle_forward) = 0;
	    *(iocontrol_data->spindle_reverse) = 0;
	    *(iocontrol_data->spindle_brake) = 1;
	    break;

	case EMC_SPINDLE_FORWARD_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_FORWARD %f RPM\n",
			    emcioStatus.spindle.speed);
	    emcioStatus.spindle.direction = 1;
	    emcioStatus.spindle.brake = 0;
	    *(iocontrol_data->spindle_forward) = 1;
	    break;

	case EMC_SPINDLE_REVERSE_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_REVERSE %f RPM\n",
			    emcioStatus.spindle.speed);
	    emcioStatus.spindle.direction = -1;
	    emcioStatus.spindle.brake = 0;
	    *(iocontrol_data->spindle_reverse) = 1;
	    break;

	case EMC_SPINDLE_STOP_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_STOP\n");
	    emcioStatus.spindle.direction = 0;
	    emcioStatus.spindle.brake = 1;
	    *(iocontrol_data->spindle_speed_out) = 0;
	    *(iocontrol_data->spindle_on) = 0;
	    *(iocontrol_data->spindle_forward) = 0;
	    *(iocontrol_data->spindle_reverse) = 0;
	    *(iocontrol_data->spindle_brake) = 1;
	    break;

	case EMC_SPINDLE_INCREASE_TYPE:
	    emcioStatus.spindle.speed++;
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_INCREASE %f RPM\n",
			    emcioStatus.spindle.speed);
	    emcioStatus.spindle.increasing = 1;
	    emcioStatus.spindle.brake = 0;
	    if ( *(iocontrol_data->spindle_speed_out) > 0 )
		*(iocontrol_data->spindle_speed_out) += 1.0;
	    else
		*(iocontrol_data->spindle_speed_out) -= 1.0;
	    *(iocontrol_data->spindle_incr_speed) = 1;
	    break;

	case EMC_SPINDLE_DECREASE_TYPE:
	    emcioStatus.spindle.speed--;
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_DECREASE %f RPM\n",
			    emcioStatus.spindle.speed);
	    emcioStatus.spindle.increasing = -1;
	    emcioStatus.spindle.brake = 0;
	    if ( *(iocontrol_data->spindle_speed_out) > 0 )
		*(iocontrol_data->spindle_speed_out) -= 1.0;
	    else
		*(iocontrol_data->spindle_speed_out) += 1.0;
	    *(iocontrol_data->spindle_decr_speed) = 1;
	    break;

	case EMC_SPINDLE_CONSTANT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_CONSTANT %f RPM\n",
			    emcioStatus.spindle.speed);
	    emcioStatus.spindle.increasing = 0;
	    emcioStatus.spindle.brake = 0;
	    *(iocontrol_data->spindle_decr_speed) = 0;
	    *(iocontrol_data->spindle_incr_speed) = 0;
	    break;

	case EMC_SPINDLE_BRAKE_RELEASE_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_BRAKE_RELEASE\n");
	    emcioStatus.spindle.brake = 0;
	    *(iocontrol_data->spindle_brake) = 0;
	    break;

	case EMC_SPINDLE_BRAKE_ENGAGE_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SPINDLE_BRAKE_ENGAGE\n");
	    emcioStatus.spindle.brake = 1;
	    *(iocontrol_data->spindle_brake) = 1;
	    break;

	case EMC_COOLANT_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_INIT\n");
	    emcioStatus.coolant.mist = 0;
	    emcioStatus.coolant.flood = 0;
	    *(iocontrol_data->coolant_mist) = 0;
	    *(iocontrol_data->coolant_flood) = 0;
	    break;

	case EMC_COOLANT_HALT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_HALT\n");
	    emcioStatus.coolant.mist = 0;
	    emcioStatus.coolant.flood = 0;
	    *(iocontrol_data->coolant_mist) = 0;
	    *(iocontrol_data->coolant_flood) = 0;
	    break;

	case EMC_COOLANT_ABORT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_ABORT\n");
	    emcioStatus.coolant.mist = 0;
	    emcioStatus.coolant.flood = 0;
	    *(iocontrol_data->coolant_mist) = 0;
	    *(iocontrol_data->coolant_flood) = 0;
	    break;

	case EMC_COOLANT_MIST_ON_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_MIST_ON\n");
	    emcioStatus.coolant.mist = 1;
	    *(iocontrol_data->coolant_mist) = 1;
	    break;

	case EMC_COOLANT_MIST_OFF_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_MIST_OFF\n");
	    emcioStatus.coolant.mist = 0;
	    *(iocontrol_data->coolant_mist) = 0;
	    break;

	case EMC_COOLANT_FLOOD_ON_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_FLOOD_ON\n");
	    emcioStatus.coolant.flood = 1;
	    *(iocontrol_data->coolant_flood) = 1;
	    break;

	case EMC_COOLANT_FLOOD_OFF_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_FLOOD_OFF\n");
	    emcioStatus.coolant.flood = 0;
	    *(iocontrol_data->coolant_flood) = 0;
	    break;

	case EMC_AUX_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_INIT\n");
	    hal_init_pins(); //init default (safe) pin values
	    emcioStatus.aux.estop = 1;
	    *(iocontrol_data->estop_out) = 1;
	    break;

	case EMC_AUX_HALT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_HALT\n");
	    emcioStatus.aux.estop = 1;
	    *(iocontrol_data->estop_out) = 1;
	    break;

	case EMC_AUX_ABORT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ABORT\n");
	    emcioStatus.aux.estop = 1;
	    *(iocontrol_data->estop_out) = 1;
	    break;

	case EMC_AUX_ESTOP_ON_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_ON\n");
	    /* assert an ESTOP to the outside world (thru HAL) */
	    *(iocontrol_data->estop_out) = 1;
	    hal_init_pins(); //resets all HAL pins to safe value
	    break;

	case EMC_AUX_ESTOP_OFF_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_OFF\n");
	    /* remove ESTOP */
	    *(iocontrol_data->estop_out) = 0;
	    /* generate a rising edge to reset optional HAL latch */
	    *(iocontrol_data->estop_reset) = 1;
	    break;

	case EMC_LUBE_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_INIT\n");
	    emcioStatus.lube.on = 0;
	    //get the lube-level from hal
	    emcioStatus.lube.level = *(iocontrol_data->lube_level);
	    *(iocontrol_data->lube) = 0;
	    break;

	case EMC_LUBE_HALT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_HALT\n");
	    emcioStatus.lube.on = 0;
	    //get the lube-level from hal
	    emcioStatus.lube.level = *(iocontrol_data->lube_level);
	    *(iocontrol_data->lube) = 0;
	    break;

	case EMC_LUBE_ABORT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_ABORT\n");
	    emcioStatus.lube.on = 0;
	    emcioStatus.lube.level = 1;
	    //get the lube-level from hal
	    emcioStatus.lube.level = *(iocontrol_data->lube_level);
	    *(iocontrol_data->lube) = 0;
	    break;

	case EMC_LUBE_ON_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_ON\n");
	    emcioStatus.lube.on = 1;
	    *(iocontrol_data->lube) = 1;
	    break;

	case EMC_LUBE_OFF_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_OFF\n");
	    emcioStatus.lube.on = 0;
	    *(iocontrol_data->lube) = 0;
	    break;

	    /* FIXME - look if it's used, DEBUG level for the iocontroller */
	case EMC_SET_DEBUG_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SET_DEBUG\n");
	    EMC_DEBUG = ((EMC_SET_DEBUG *) emcioCommand)->debug;
	    break;

	default:
	    rtapi_print("IO: unknown command %s\n", emcSymbolLookup(type));
	    break;
	}			/* switch (type) */

	// ack for the received command
	emcioStatus.command_type = type;
	emcioStatus.echo_serial_number = emcioCommand->serial_number;
	//set above, to allow some commands to fail this
	//emcioStatus.status = RCS_DONE;
	emcioStatus.heartbeat++;
	emcioStatusBuffer->write(&emcioStatus);

	esleep(EMC_IO_CYCLE_TIME);
	/* clear reset line to allow for a later rising edge */
	*(iocontrol_data->estop_reset) = 0;
	
    }	// end of "while (! done)" loop

    // disconnect from the HAL
    hal_exit(comp_id);

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
