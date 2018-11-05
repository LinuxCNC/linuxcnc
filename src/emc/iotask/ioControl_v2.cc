/********************************************************************
 * Description: IoControl.cc
 *           Simply accepts NML messages sent to the IO controller
 *           outputs those to a HAL pin,
 *           and sends back a "Done" message.
 *
 *
 *  ENABLE logic:  this module exports three HAL pins related to ENABLE.
 *  The first is emc-enable-in.  It is an input from the HAL, when FALSE,
 *  EMC will go into the STOPPED state (regardless of the state of
 *  the other two pins).  When it goes TRUE, EMC will go into the
 *  ESTOP_RESET state (also known as READY).
 *
 *  The second HAL pin is an output to the HAL.  It is controlled by
 *  the NML messages ESTOP_ON and ESTOP_OFF, which normally result from
 *  user actions at the GUI.  For the simplest system, loop user-enable-out
 *  back to emc-enable-in in the HAL.  The GUI controls user-enable-out, and EMC
 *  responds to that once it is looped back.
 *
 *  If external ESTOP inputs are desired, they can be
 *  used in a classicladder rung, in series with user-enable-out.
 *  It will look like this:
 *
 *  -----|UEO|-----|EEST|--+--|EEI|--+--(EEI)----
 *                         |         |
 *                         +--|URE|--+
 *  UEO=user-enable-out
 *  EEST=external ESTOP circuitry
 *  EEI=machine is enabled
 *  URE=user request enable
 *
 *  This will work like this: EMC will be enabled (by EEI, emc-enabled-in),
 *  only if UEO, EEST are closed when URE gets strobed.
 *  If any of UEO (user requested stop) or EEST (external estop) have been
 *  opened, then EEI will open as well.
 *  After restoring normal condition (UEO and EEST closed), an aditional
 *  URE (user-request-enable) is needed, this is either sent by the GUI
 *  (using the EMC_AUX_ESTOP_RESET NML message), or by a hardware button
 *  connected to the ladder driving URE.
 *
 *  NML messages are sent usually from the user hitting F1 on the GUI.
 *
 *   Derived from a work by Fred Proctor & Will Shackleford
 *
 * Author:
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2004 All rights reserved.
 *
 * Last change: Michael Haberler 1/2011 - rework & add v2 protocol support
 ********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <getopt.h>

#include "hal.h"		/* access to HAL functions/definitions */
#include "rtapi.h"		/* rtapi_print_msg */
#include "rcs.hh"		/* RCS_CMD_CHANNEL */
#include "emc.hh"		/* EMC NML */
#include "emc_nml.hh"
#include "emcglb.h"		/* EMC_NMLFILE, EMC_INIFILE, TOOL_TABLE_FILE */
#include "inifile.hh"		/* INIFILE */
#include "initool.hh"		/* iniTool() */
#include "nml_oi.hh"
#include "timer.hh"
#include "rcs_print.hh"
#include "tool_parse.h"

static RCS_CMD_CHANNEL *emcioCommandBuffer = 0;
static RCS_CMD_MSG *emcioCommand = 0;
static RCS_STAT_CHANNEL *emcioStatusBuffer = 0;
static EMC_IO_STAT emcioStatus;
static NML *emcErrorBuffer = 0;

static char *ttcomments[CANON_POCKETS_MAX];
static int random_toolchanger = 0;
static int support_start_change = 0;
static const char *progname;

typedef enum {
    V1 = 1,
    V2 = 2
} version_t;
static int proto = V2;

// extend  EMC_IO_ABORT_REASON_ENUM from emc.hh
enum {
    EMC_ABORT_BY_TOOLCHANGER_FAULT = EMC_ABORT_USER + 1
};

static const char *strcs[] = { "invalid","RCS_DONE","RCS_EXEC","RCS_ERROR"};

// read_inputs() returns a mask of changes observed
enum {
    TI_PREPARING = 1,
    TI_PREPARE_COMPLETE = 2,
    TI_CHANGING = 4,
    TI_CHANGE_COMPLETE = 8,
    TI_TC_FAULT = 16,
    TI_TC_ABORT = 32,
    TI_EMC_ABORT_SIGNALED = 64,
    TI_EMC_ABORT_ACKED = 128,
    TI_ESTOP_CHANGED = 256,
    TI_LUBELEVEL_CHANGED = 512,
    TI_START_CHANGE = 1024,
    TI_START_CHANGE_ACKED = 2048
};

// iocontrol states. Reflected in state pin
typedef  enum {
    ST_IDLE = 0,
    ST_PREPARING = 1,
    ST_START_CHANGE = 2, // V2 only
    ST_CHANGING = 3,
    ST_WAIT_FOR_ABORT_ACK = 4, // V2 only
} iostate_t;
static const char *strstate[] = { "IDLE","PREPARING","START_CHANGE", "CHANGING","WAIT_FOR_ABORT_ACK"};

static int toolchanger_reason;  // last fault reason read from toolchanger

// predicates for testing toolchanger fault conditions
#define TC_FAULT  (*(iocontrol_data->toolchanger_faulted))
#define TC_HARDFAULT (TC_FAULT && (toolchanger_reason <= 0))
#define TC_SOFTFAULT (TC_FAULT && (toolchanger_reason > 0))

struct iocontrol_str {
    hal_bit_t *user_enable_out;	/* output, TRUE when EMC wants stop */
    hal_bit_t *emc_enable_in;	/* input, TRUE on any external stop */
    hal_bit_t *user_request_enable;	/* output, used to reset ENABLE latch */
    hal_bit_t *coolant_mist;	/* coolant mist output pin */
    hal_bit_t *coolant_flood;	/* coolant flood output pin */
    hal_bit_t *lube;		/* lube output pin */
    hal_bit_t *lube_level;	/* lube level input pin */

    // the following pins are needed for toolchanging
    //tool-prepare
    hal_bit_t *tool_prepare;	/* output, pin that notifies HAL it needs to prepare a tool */
    hal_s32_t *tool_prep_pocket;/* output, pin that holds the P word from the tool table entry matching the tool to be prepared,
                                   only valid when tool-prepare=TRUE */
    hal_s32_t tool_prep_index; /* internal array index of prepped tool above */
    hal_s32_t *tool_prep_number;/* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    hal_s32_t *tool_number;     /* output, pin that holds the tool number currently in the spindle */
    hal_bit_t *tool_prepared;	/* input, pin that notifies that the tool has been prepared */
    //tool-change
    hal_bit_t *tool_change;	/* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    hal_bit_t *tool_changed;	/* input, notifies tool has been changed */

    // v2 protocol
    // iocontrolv2 -> toolchanger
    hal_bit_t *emc_abort; 	/* output, signals emc-originated abort to toolchanger */
    hal_bit_t *emc_abort_ack; 	/* input, handshake line to acknowledge abort_tool_change */
    hal_s32_t *emc_reason;     	/* output, convey cause for EMC-originated abort to toolchanger.
				 * UI informational. Valid during emc-abort True.
				 */
    // toolchanger -> iocontrolv2
    hal_bit_t *toolchanger_fault;	/* input, toolchanger signals fault . Always monitored.
					 * A fault is recorded in the toolchange_faulted pin
					 */
    hal_bit_t *toolchanger_fault_ack;	/* handshake line for above signal. will be set by iocontrol
					 * after above fault line is recognized and deasserted when
					 * toolchanger-fault drops. Toolchanger is free to interpret
					 * the ack; reading the -ack lines assures fault has been
					 * received and acted upon.
					 */
    hal_s32_t *toolchanger_reason;         /* input, convey reason code for toolchanger-originated
					    * fault to iocontrol. read during toolchanger-fault True.
					    * on a toolchange abort, the reason is passed to EMC in the
					    * emcioStat message.
					    * a positive value causes an OperatorDisplay text
					    * a negative value causes an OperatorError text
					    * a zero value does not cause any display
					    */

    hal_bit_t *start_change;              /* signal begin of M6 cycle even before move to toolchange
					   * position starts
					   */
    hal_bit_t *start_change_ack;          /* acknowledge line for start_change */

    // other:
    hal_bit_t *toolchanger_faulted; 	/* output. signals toolchanger-faul line has toggled
					 * The next M6 will abort if True.
					 */
    hal_bit_t *toolchanger_clear_fault;	/* input. resets TC fault condition.
					 * Deasserts toolchanger-faulted if toolchanger-fault is line False.
					 * Usage: UI - e.g. 'clear TC fault' button
					 */
    hal_s32_t *state; 			/* output. Internal state for debugging */

    // note: spindle control has been moved to motion
} *iocontrol_data;			//pointer to the HAL-struct

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
	    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "tool", emc_nmlfile);
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
				 emc_nmlfile);
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
	    new NML(nmlErrorFormat, "emcError", "tool", emc_nmlfile);
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
    IniFile inifile;
    const char *inistring;
    char version[LINELEN], machine[LINELEN];

    /* Open the ini file */
    if (inifile.Open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = inifile.Find("DEBUG", "EMC"))) {
	/* copy to global */
	if (1 != sscanf(inistring, "%i", &emc_debug)) {
	    emc_debug = 0;
	}
    } else {
	/* not found, use default */
	emc_debug = 0;
    }

    // make it verbose if debugging RCS
    if (emc_debug & EMC_DEBUG_IOCONTROL) {
	rtapi_set_msg_level(RTAPI_MSG_DBG);
    }

    if (emc_debug & EMC_DEBUG_VERSIONS) {
	if (NULL != (inistring = inifile.Find("VERSION", "EMC"))) {
	    if(sscanf(inistring, "$Revision: %s", version) != 1) {
		strncpy(version, "unknown", LINELEN-1);
	    }
	} else {
	    strncpy(version, "unknown", LINELEN-1);
	}

	if (NULL != (inistring = inifile.Find("MACHINE", "EMC"))) {
	    strncpy(machine, inistring, LINELEN-1);
	} else {
	    strncpy(machine, "unknown", LINELEN-1);
	}
	rtapi_print_msg(RTAPI_MSG_DBG,"%s: machine: '%s'  version '%s'\n", progname,machine, version);
    }

    if (NULL != (inistring = inifile.Find("NML_FILE", "EMC"))) {
	strcpy(emc_nmlfile, inistring);
    } else {
	// not found, use default
    }

    double temp;
    temp = emc_io_cycle_time;
    if (NULL != (inistring = inifile.Find("CYCLE_TIME", "EMCIO"))) {
	if (1 == sscanf(inistring, "%lf", &emc_io_cycle_time)) {
	    // found it
	} else {
	    // found, but invalid
	    emc_io_cycle_time = temp;
	    rtapi_print
		("invalid [EMCIO] CYCLE_TIME in %s (%s); using default %f\n",
		 filename, inistring, emc_io_cycle_time);
	}
    } else {
	// not found, using default
	rtapi_print
	    ("[EMCIO] CYCLE_TIME not found in %s; using default %f\n",
	     filename, emc_io_cycle_time);
    }

    inifile.Find(&proto, "PROTOCOL_VERSION", "EMCIO");
    rtapi_print_msg(RTAPI_MSG_DBG,"%s: [EMCIO] using v%d protocol\n",progname,proto);

    inifile.Find(&random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");

    // close it
    inifile.Close();

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

#define BITPIN(dir,fmt,ptr)						\
    if ((retval = hal_pin_bit_newf(dir,ptr,comp_id, fmt,n)) < 0) {	\
	sprintf(buf,fmt,n);						\
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: %s - export failed error=%d\n",progname,buf,retval); \
	goto HAL_EXIT;							\
    }
#define S32PIN(dir,fmt,ptr)						\
    if ((retval = hal_pin_s32_newf(dir,ptr,comp_id, fmt,n)) < 0) {	\
	sprintf(buf,fmt,n);						\
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: %s - export failed error=%d\n",progname,buf,retval); \
	goto HAL_EXIT;							\
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
    int n = 0;		//n - number of the hal component (only one for iocotrol)
    int retval = -1;
    char buf[100];

    // initialise the hal component
    if ((comp_id = hal_init("iocontrol")) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"%s: ERROR: hal_init() failed - error=%d\n",progname,comp_id);
	goto FAIL;
    }

    // allocate shared memory for iocontrol data
    if ((iocontrol_data = (iocontrol_str *) hal_malloc(sizeof(iocontrol_str))) == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,"%s: ERROR: hal_malloc() failed\n",progname);
	goto HAL_EXIT;
    }

    BITPIN( HAL_OUT, "iocontrol.%d.user-enable-out", &(iocontrol_data->user_enable_out));
    BITPIN( HAL_OUT, "iocontrol.%d.user-request-enable", &(iocontrol_data->user_request_enable));
    BITPIN( HAL_OUT, "iocontrol.%d.coolant-flood", &(iocontrol_data->coolant_flood));
    BITPIN( HAL_OUT, "iocontrol.%d.coolant-mist", &(iocontrol_data->coolant_mist));
    BITPIN( HAL_OUT, "iocontrol.%d.lube", &(iocontrol_data->lube));
    S32PIN( HAL_OUT, "iocontrol.%d.tool-number", &(iocontrol_data->tool_number));
    S32PIN( HAL_OUT, "iocontrol.%d.tool-prep-number", &(iocontrol_data->tool_prep_number));
    S32PIN( HAL_OUT, "iocontrol.%d.tool-prep-pocket", &(iocontrol_data->tool_prep_pocket));

    // tool-prep-index
    retval = hal_param_s32_newf(HAL_RO, &(iocontrol_data->tool_prep_index), comp_id,
                                "iocontrol.%d.tool-prep-index", n);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"IOCONTROL: ERROR: iocontrol %d param tool-prep-index export failed with err=%i\n",
			n, retval);
	hal_exit(comp_id);
	return -1;
    }

    BITPIN( HAL_OUT, "iocontrol.%d.tool-prepare", &(iocontrol_data->tool_prepare));
    BITPIN( HAL_IN , "iocontrol.%d.tool-prepared", &(iocontrol_data->tool_prepared));
    BITPIN( HAL_OUT, "iocontrol.%d.tool-change", &(iocontrol_data->tool_change));
    BITPIN( HAL_IN , "iocontrol.%d.tool-changed", &(iocontrol_data->tool_changed));
    BITPIN( HAL_IN , "iocontrol.%d.emc-enable-in", &(iocontrol_data->emc_enable_in));
    BITPIN( HAL_IN , "iocontrol.%d.lube_level", &(iocontrol_data->lube_level));

    S32PIN( HAL_OUT, "iocontrol.%d.state", &(iocontrol_data->state));

    // v2 pins
    if (proto > V1) {
	BITPIN( HAL_OUT, "iocontrol.%d.emc-abort", &(iocontrol_data->emc_abort));
	BITPIN( HAL_IN , "iocontrol.%d.emc-abort-ack", &(iocontrol_data->emc_abort_ack));
	S32PIN( HAL_OUT, "iocontrol.%d.emc-reason", &(iocontrol_data->emc_reason));
	BITPIN( HAL_IN , "iocontrol.%d.toolchanger-fault", &(iocontrol_data->toolchanger_fault));
	BITPIN( HAL_OUT, "iocontrol.%d.toolchanger-fault-ack", &(iocontrol_data->toolchanger_fault_ack));
	S32PIN( HAL_IN , "iocontrol.%d.toolchanger-reason", &(iocontrol_data->toolchanger_reason));
	BITPIN( HAL_OUT, "iocontrol.%d.toolchanger-faulted", &(iocontrol_data->toolchanger_faulted));
	BITPIN( HAL_IN , "iocontrol.%d.toolchanger-clear-fault", &(iocontrol_data->toolchanger_clear_fault));
	BITPIN( HAL_OUT, "iocontrol.%d.start-change", &(iocontrol_data->start_change));
	BITPIN( HAL_IN , "iocontrol.%d.start-change-ack", &(iocontrol_data->start_change_ack));
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "%s: iocontrol_hal_init() complete\n",progname);
    hal_ready(comp_id);
    return(0);

HAL_EXIT:
    hal_exit(comp_id);
FAIL:
    return retval;
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
    *(iocontrol_data->user_enable_out) = 0;	/* output, FALSE when EMC wants stop */
    *(iocontrol_data->user_request_enable) = 0;	/* output, used to reset HAL latch */
    *(iocontrol_data->coolant_mist) = 0;		/* coolant mist output pin */
    *(iocontrol_data->coolant_flood) = 0;		/* coolant flood output pin */
    *(iocontrol_data->lube) = 0;			/* lube output pin */
    *(iocontrol_data->tool_prepare) = 0;		/* output, pin that notifies HAL it needs to prepare a tool */
    *(iocontrol_data->tool_prep_number) = 0;	/* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    *(iocontrol_data->tool_prep_pocket) = 0;	/* output, pin that holds the P word from the tool to be prepared, only valid when tool-prepare=TRUE */
    iocontrol_data->tool_prep_index = 0;        /* output, param that holds the internal index of the tool to be prepared, for debug */
    *(iocontrol_data->tool_change) = 0;		/* output, notifies a tool-change should happen (emc should be in the tool-change position) */

    *(iocontrol_data->state) = ST_IDLE;           // new pin in v1 mode, too

    if (proto > V1) {
	// v2 protocol output pins
	*(iocontrol_data->emc_abort) = 0;
	*(iocontrol_data->emc_reason) = 0;
	*(iocontrol_data->toolchanger_fault_ack) = 0;
	*(iocontrol_data->toolchanger_faulted) = 0;
	*(iocontrol_data->start_change) = 0;
    }
}

void load_tool(int pocket) {
    if(random_toolchanger) {
	// swap the tools between the desired pocket and the spindle pocket
	CANON_TOOL_TABLE temp;
	char *comment_temp;

	temp = emcioStatus.tool.toolTable[0];
	emcioStatus.tool.toolTable[0] = emcioStatus.tool.toolTable[pocket];
	emcioStatus.tool.toolTable[pocket] = temp;

	comment_temp = ttcomments[0];
	ttcomments[0] = ttcomments[pocket];
	ttcomments[pocket] = comment_temp;

	if (0 != saveToolTable(tool_table_file, emcioStatus.tool.toolTable, ttcomments, random_toolchanger))
	    emcioStatus.status = RCS_ERROR;
    } else if (pocket == 0) {
	// magic T0 = pocket 0 = no tool
	emcioStatus.tool.toolTable[0].toolno = -1;
	ZERO_EMC_POSE(emcioStatus.tool.toolTable[0].offset);
	emcioStatus.tool.toolTable[0].diameter = 0.0;
	emcioStatus.tool.toolTable[0].frontangle = 0.0;
	emcioStatus.tool.toolTable[0].backangle = 0.0;
	emcioStatus.tool.toolTable[0].orientation = 0;
    } else {
	// just copy the desired tool to the spindle
	emcioStatus.tool.toolTable[0] = emcioStatus.tool.toolTable[pocket];
    }
}

void reload_tool_number(int toolno) {
    if(random_toolchanger) return; // doesn't need special handling here
    for(int i=1; i<CANON_POCKETS_MAX; i++) {
	if(emcioStatus.tool.toolTable[i].toolno == toolno) {
	    load_tool(i);
	    break;
	}
    }
}

static char *str_input(int status)
{
    static char seen[200];

    seen[0] = 0;
    if (status & TI_PREPARING) strcat(seen," TI_PREPARING");
    if (status & TI_PREPARE_COMPLETE) strcat(seen," TI_PREPARE_COMPLETE");
    if (status & TI_CHANGING) strcat(seen," TI_CHANGING");
    if (status & TI_CHANGE_COMPLETE) strcat(seen," TI_CHANGE_COMPLETE");
    if (status & TI_TC_FAULT) strcat(seen," TI_TC_FAULT");
    if (status & TI_TC_ABORT) strcat(seen," TI_TC_ABORT");
    if (status & TI_EMC_ABORT_SIGNALED) strcat(seen," TI_EMC_ABORT_SIGNALED");
    if (status & TI_EMC_ABORT_ACKED) strcat(seen," TI_EMC_ABORT_ACKED");
    if (status & TI_ESTOP_CHANGED) strcat(seen," TI_ESTOP_CHANGED");
    if (status & TI_LUBELEVEL_CHANGED) strcat(seen," TI_LUBELEVEL_CHANGED");
    if (status & TI_START_CHANGE) strcat(seen," TI_START_CHANGE");
    if (status & TI_START_CHANGE_ACKED) strcat(seen," TI_START_CHANGE_ACKED");
    return seen;
}
/********************************************************************
 *
 * Description: read_inputs(void)
 *			Reads pin values from HAL
 *                      Handle pin protocol
 *			this function gets called once per cycle
 *			It sets the values for the emcioStatus.aux.*
 *                      also handle estop and lube_level
 *
 * Returns:	returns a bitmask of relevant status
 *
 * Side Effects: updates toolchanger_reason,
 *               emcioStatus.tool.*
 *               emcioStatus.aux.estop
 *               emcioStatus.lube.level
 *
 * Called By: main every CYCLE
 ********************************************************************/

int read_inputs(void)
{
    int oldval, retval = 0;

    oldval = emcioStatus.aux.estop;

    if ( *(iocontrol_data->emc_enable_in)==0) //check for estop from HW
	emcioStatus.aux.estop = 1;
    else
	emcioStatus.aux.estop = 0;

    if (oldval != emcioStatus.aux.estop) {
	retval |= TI_ESTOP_CHANGED;
    }

    oldval = emcioStatus.lube.level;
    emcioStatus.lube.level = *(iocontrol_data->lube_level); // check for lube_level from HW
    if (oldval != emcioStatus.lube.level) {
	retval |=  TI_LUBELEVEL_CHANGED;
    }

    if (proto > V1) {
	// FIXME test with predicate in main
	// record toolchanger fault
	if (*iocontrol_data->toolchanger_fault) {
	    // reason code now valid
	    toolchanger_reason = *iocontrol_data->toolchanger_reason;
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s:read_input: toolchanger fault signaled, reason: %d \n",progname, toolchanger_reason);

	    // raise ack line
	    *(iocontrol_data->toolchanger_fault_ack) = 1;
	    // record the fact that a fault or abort was received
	    *(iocontrol_data->toolchanger_faulted) = 1;
	    retval |= TI_TC_FAULT;
	} else {
	    // clear the ack line
	    *(iocontrol_data->toolchanger_fault_ack) = 0;
	}

	// clear toolchanger fault condition if so signaled,
	if (*iocontrol_data->toolchanger_clear_fault) {
	    *(iocontrol_data->toolchanger_faulted) = 0;
	    // clear last tc fault reason
	    toolchanger_reason = 0;
	    retval &= ~TI_TC_FAULT;
	}

	// an EMC-side abort is in progress.
	if (*iocontrol_data->emc_abort) {
	    if (*iocontrol_data->emc_abort_ack) {
		// the toolchanger acknowledged the fact so deassert
		*(iocontrol_data->emc_abort) = 0;
		// a completed EMC-side abort always forces IDLE state
		*(iocontrol_data->state) = ST_IDLE;
		retval |= TI_EMC_ABORT_ACKED;
	    } else {
		// waiting for abort being acked by toolchanger.
		retval |= TI_EMC_ABORT_SIGNALED;
	    }
	}
	// the very start of an M6 operation was signaled
	if (*iocontrol_data->start_change) {
	    if (*iocontrol_data->start_change_ack) {
		// the toolchanger acknowledged the start-change
		*(iocontrol_data->start_change) = 0;
		retval |= TI_START_CHANGE_ACKED;
	    } else {
		// wait for ack by toolchanger
		retval |= TI_START_CHANGE;
	    }
	}
    }

    if (*iocontrol_data->tool_prepare) {
	if (*iocontrol_data->tool_prepared) {
	    emcioStatus.tool.pocketPrepped = iocontrol_data->tool_prep_index; //check if tool has been prepared
	    *(iocontrol_data->tool_prepare) = 0;
	    *(iocontrol_data->state) = ST_IDLE; // normal prepare completion
	    retval |= TI_PREPARE_COMPLETE;
	} else {
	    *(iocontrol_data->state) = ST_PREPARING;
	    retval |= TI_PREPARING;
	}
    }

    if (*iocontrol_data->tool_change) {

	// check wether a toolchanger fault will force an abort of this change
	if ((proto > V1) && (*iocontrol_data->toolchanger_faulted))  {

	    /* unlikely but possible: toolchanger_faulted was asserted since
	     * we received the EMC_TOOL_LOAD
	     * read new reason code
	     * retract change request
	     */
	    toolchanger_reason = *iocontrol_data->toolchanger_reason;
	    /* signal an abort in response, abort-ack line is handled
	     * at beginning of function in next iteration(s)
	     * this assures toolchanger is aware of an aborted change
	     * even if it was caused by the toolchanger faulting it in
	     * the first place
	     */
	    *(iocontrol_data->emc_reason) = EMC_ABORT_BY_TOOLCHANGER_FAULT;
	    *(iocontrol_data->emc_abort) = 1;
	    retval |= TI_TC_ABORT;
	    // give up on request
	    *(iocontrol_data->tool_change) = 0;
	    *(iocontrol_data->state) = ST_WAIT_FOR_ABORT_ACK;
	}

	if (*iocontrol_data->tool_changed) {
	    // good to commit this change
	    if(!random_toolchanger && emcioStatus.tool.pocketPrepped == 0) {
		emcioStatus.tool.toolInSpindle = 0;
	    } else {
		// the tool now in the spindle is the one that was prepared
		emcioStatus.tool.toolInSpindle = emcioStatus.tool.toolTable[emcioStatus.tool.pocketPrepped].toolno;
	    }
	    *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle; // likewise in HAL
	    load_tool(emcioStatus.tool.pocketPrepped);
	    emcioStatus.tool.pocketPrepped = -1; // reset the tool prepped number, -1 to permit tool 0 to be loaded
	    *(iocontrol_data->tool_prep_number) = 0; // likewise in HAL
	    *(iocontrol_data->tool_prep_pocket) = 0; // likewise in HAL
	    iocontrol_data->tool_prep_index = 0; // likewise in HAL
	    *(iocontrol_data->tool_change) = 0; // also reset the tool change signal
	    *(iocontrol_data->state) = ST_IDLE;
	    retval |= TI_CHANGE_COMPLETE;
	} else {
	    retval |= TI_CHANGING;
	}
    }
    return retval;
}

static void do_hal_exit(void) {
    hal_exit(comp_id);
}


void update_status(int status, int serial)
{
    static int status_reported = -1;

    emcioStatus.status = status;
    if (status_reported != status) {
	rtapi_print_msg(RTAPI_MSG_DBG, "%s: updating status=%s state=%s fault=%d reason=%d\n",
			progname,strcs[emcioStatus.status],strstate[*(iocontrol_data->state)],
			emcioStatus.fault, emcioStatus.reason
			);
	status_reported = emcioStatus.status;  // just print this once
    }
    emcioStatus.command_type = EMC_IO_STAT_TYPE;
    emcioStatus.echo_serial_number = serial;
    emcioStatus.heartbeat++;
    emcioStatusBuffer->write(&emcioStatus);
}

/********************************************************************
 *
 * Description: main(int argc, char * argv[])
 *		Connects to NML buffers and enters an endless loop
 *		processing NML IO commands. Print statements are
 *		sent to the console indicating which IO command was
 *		executed if debug level is set to RTAPI_MSG_DBG.
 *
 * Exit Value:  Zero or -1 if ini file not found or failure to connect
 *		to NML buffers.
 *
 * Side Effects: None.
 *
 * Called By:
 *
 ********************************************************************/

int main(int argc, char *argv[])
{
    int t, input_status;
    NMLTYPE type;

    progname = argv[0];
    for (t = 1; t < argc; t++) {
	if (!strcmp(argv[t], "-support-start-change")) {
	    support_start_change = 1;
	    continue;
	}
	if (!strcmp(argv[t], "-ini")) {
	    if (t == argc - 1) {
		return -1;
	    } else {
		if (strlen(argv[t+1]) >= LINELEN) {
		    rtapi_print_msg(RTAPI_MSG_ERR, "ini file name too long (max %d)\n", LINELEN);
		    rtapi_print_msg(RTAPI_MSG_ERR, "    %s\n", argv[t+1]);
		    return -1;
		}
		strcpy(emc_inifile, argv[t + 1]);
		t++;
	    }
	    continue;
	}
	/* do other args similarly here */
    }

    /* Register the routine that catches the SIGINT signal */
    signal(SIGINT, quit);
    /* catch SIGTERM too - the run script uses it to shut things down */
    signal(SIGTERM, quit);

    if (0 != iniLoad(emc_inifile)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: can't open ini file %s\n",progname,emc_inifile);
	exit(-1);
    }

    if (iocontrol_hal_init() != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: can't initialize the HAL\n",progname);
	exit(-1);
    }

    atexit(do_hal_exit);

    if (0 != emcIoNmlGet()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s:can't connect to NML buffers in %s\n",progname,emc_inifile);
	exit(-1);
    }
    // used only for getting TOOL_TABLE_FILE out of the ini file
    if (0 != iniTool(emc_inifile)) {
	rcs_print_error("%s: iniTool failed.\n",progname);
	exit(-1);
    }

    for(int i = 0; i < CANON_POCKETS_MAX; i++) {
	ttcomments[i] = (char *)malloc(CANON_TOOL_ENTRY_LEN);
    }

    // on nonrandom machines, always start by assuming the spindle is empty
    if(!random_toolchanger) {
	emcioStatus.tool.toolTable[0].toolno = -1;
	ZERO_EMC_POSE(emcioStatus.tool.toolTable[0].offset);
	emcioStatus.tool.toolTable[0].diameter = 0.0;
	emcioStatus.tool.toolTable[0].frontangle = 0.0;
	emcioStatus.tool.toolTable[0].backangle = 0.0;
	emcioStatus.tool.toolTable[0].orientation = 0;
	ttcomments[0][0] = '\0';
    }

    if (0 != loadToolTable(tool_table_file, emcioStatus.tool.toolTable,
			   ttcomments, random_toolchanger)) {
	rcs_print_error("%s: can't load tool table.\n",progname);
    }

    done = 0;

    /* set status values to 'normal' */
    emcioStatus.aux.estop = 1; // estop=1 means to emc that ESTOP condition is met
    emcioStatus.tool.pocketPrepped = -1;
    emcioStatus.tool.toolInSpindle = 0;
    emcioStatus.coolant.mist = 0;
    emcioStatus.coolant.flood = 0;
    emcioStatus.lube.on = 0;
    emcioStatus.lube.level = 1;

    while (!done) {

	/* check for inputs from HAL (updates emcioStatus)
	 * read_inputs() returns a bit mask of observed state changes
	 * if an external ESTOP is activated or lube_level changes,
	 * an NML message has to be pushed to EMC.
	 * the way it was done status was only checked at the end of a command
	 */
	input_status = read_inputs();

	// always piggyback fault and reason
	emcioStatus.fault =  *(iocontrol_data->toolchanger_faulted);
	emcioStatus.reason = toolchanger_reason;

	if (input_status & (TI_ESTOP_CHANGED|TI_LUBELEVEL_CHANGED)) {
	    if (input_status & TI_ESTOP_CHANGED) {
		rtapi_print_msg(RTAPI_MSG_DBG, "%s:ESTOP changed to %d\n",progname,emcioStatus.aux.estop);
	    }
	    if (input_status & TI_LUBELEVEL_CHANGED)
		rtapi_print_msg(RTAPI_MSG_DBG, "%s:lube_level changed to %d\n",progname,emcioStatus.lube.level);

	    // need for different serial number, because we are pushing a new message
	    update_status(RCS_DONE, emcioCommand->serial_number + 1);
	}

	if (input_status & (TI_PREPARING)) {
	    update_status(RCS_EXEC, emcioCommand->serial_number);
	}

	if (input_status & (TI_START_CHANGE|TI_CHANGING)) {
	    if (TC_FAULT) {
		rtapi_print_msg(RTAPI_MSG_DBG, "%s: signaling fault during change, reason=%d state=%s input_status=%s\n",
				progname,
				toolchanger_reason,
				strstate[*(iocontrol_data->state)],
				str_input(input_status)
				);
		update_status(RCS_ERROR, emcioCommand->serial_number);
	    } else {
		update_status(RCS_EXEC, emcioCommand->serial_number);
	    }
	}

	//	if (input_status & (TI_PREPARE_COMPLETE|TI_CHANGE_COMPLETE|TI_START_CHANGE_ACKED|TI_EMC_ABORT_ACKED)) {
	if (input_status & (TI_PREPARE_COMPLETE|TI_CHANGE_COMPLETE|TI_START_CHANGE_ACKED)) {
	    update_status(RCS_DONE, emcioCommand->serial_number);
	}

	/* read NML, run commands */
	if (-1 == emcioCommandBuffer->read()) {
	    /* bad command, wait until next cycle */
	    esleep(emc_io_cycle_time);
	    /* and repeat */
	    continue;
	}

	if (0 == emcioCommand ||	// bad command pointer
	    0 == emcioCommand->type ||	// bad command type
	    emcioCommand->serial_number == emcioStatus.echo_serial_number) {	// command already finished
	    /* wait until next cycle */
	    esleep(emc_io_cycle_time);
	    /* and repeat */
	    continue;
	}

	/* emcioStatus.status is set to RCS_DONE here which would cause
	 * the next message to be read and processed on the next iteration.
	 * those command types which need to block processing the next message
	 * until the step is completed need to set emcioStatus.status to RCS_EXEC
	 * (EMC_TOOL_PREPARE_TYPE, EMC_TOOL_LOAD_TYPE)
	 * to cause an abort, set emcioStatus.reason and
	 * emcioStatus.status to RCS_ERROR.
	 */
	emcioStatus.status = RCS_DONE;
	type = emcioCommand->type;

	switch (type) {
	case 0:
	    break;

	case EMC_IO_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_IO_INIT\n");
	    hal_init_pins();
	    break;

	case EMC_TOOL_INIT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_INIT\n");
	    loadToolTable(tool_table_file, emcioStatus.tool.toolTable,
			  ttcomments, random_toolchanger);
	    reload_tool_number(emcioStatus.tool.toolInSpindle);
	    break;

	case EMC_TOOL_HALT_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_HALT\n");
	    break;

	case EMC_TOOL_ABORT_TYPE:
	    // this gets sent on any Task Abort, so it might be safer to stop
	    // mist and coolant
	    // spindle should be turned off through motion
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT reason=%d\n",((EMC_TOOL_ABORT *) emcioCommand)->reason);

	    emcioStatus.coolant.mist = 0;
	    emcioStatus.coolant.flood = 0;
	    *(iocontrol_data->coolant_mist) = 0;	// coolant mist output pin
	    *(iocontrol_data->coolant_flood) = 0;	// coolant flood output pin

	    if (proto > V1) {
		// fix race condition by asserting abort-tool-change before
		// deasserting tool-change and tool-prepare
		*(iocontrol_data->emc_reason) = ((EMC_TOOL_ABORT *) emcioCommand)->reason;
		*(iocontrol_data->emc_abort) = 1;      // notify TC of abort conditon
	    }
	    *(iocontrol_data->tool_change) = 0;      // abort tool change if in progress
	    *(iocontrol_data->tool_prepare) = 0;     // abort tool prepare if in progress
	    *(iocontrol_data->start_change) = 0;

	    // indicate state change - waiting for ack line in V2
	    // wait-for-ack intermediate state meaningful in V2 mode only
	    *(iocontrol_data->state) = (proto > V1) ? ST_WAIT_FOR_ABORT_ACK : ST_IDLE;
	    break;

	case EMC_TOOL_PREPARE_TYPE:
	{
	    int p = ((EMC_TOOL_PREPARE*)emcioCommand)->pocket;
	    int t = ((EMC_TOOL_PREPARE*)emcioCommand)->tool;
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE tool=%d pocket=%d\n", t, p);

	    // it doesn't make sense to prep the spindle pocket
	    if (random_toolchanger && p == 0)
		break;

	    /* set tool number first */
            iocontrol_data->tool_prep_index = p;
            *(iocontrol_data->tool_prep_pocket) = random_toolchanger? p: emcioStatus.tool.toolTable[p].pocketno;
	    if (!random_toolchanger && p == 0) {
			*(iocontrol_data->tool_prep_number) = 0;
			*(iocontrol_data->tool_prep_pocket) = 0;
	    } else {
		*(iocontrol_data->tool_prep_number) = emcioStatus.tool.toolTable[p].toolno;
		if (emcioStatus.tool.toolTable[p].toolno != t) // sanity check
		    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE: mismatch: tooltable[%d]=%d, got %d\n", 
				    p, emcioStatus.tool.toolTable[p].toolno, t);
	    }

	    if ((proto > V1) && *(iocontrol_data->toolchanger_faulted)) { // informational
		rtapi_print_msg(RTAPI_MSG_DBG, "%s: prepare: toolchanger faulted (reason=%d), next M6 will %s\n",
				progname,toolchanger_reason,
				toolchanger_reason > 0 ? "set fault code and reason" : "abort program");
	    }
	    // then set the prepare pin to tell external logic to get started
	    *(iocontrol_data->tool_prepare) = 1;
	    *(iocontrol_data->state) = ST_PREPARING;

	    // delay fetching the next message until prepare done
	    if (!(input_status & TI_PREPARE_COMPLETE)) {
		emcioStatus.status = RCS_EXEC;
	    }
	}
	break;

	case EMC_TOOL_LOAD_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD loaded=%d prepped=%d\n", emcioStatus.tool.toolInSpindle, emcioStatus.tool.pocketPrepped);

	    // it doesn't make sense to load a tool from the spindle pocket
	    if (random_toolchanger && emcioStatus.tool.pocketPrepped == 0) {
		break;
	    }

	    // it's not necessary to load the tool already in the spindle
	    if (!random_toolchanger && emcioStatus.tool.pocketPrepped > 0 &&
		emcioStatus.tool.toolInSpindle == emcioStatus.tool.toolTable[emcioStatus.tool.pocketPrepped].toolno) {
		break;
	    }

	    if (emcioStatus.tool.pocketPrepped != -1) {
		//notify HW for toolchange
		*(iocontrol_data->tool_change) = 1;
		*(iocontrol_data->state) = ST_CHANGING;

		// delay fetching the next message until change done
		if (! (input_status & TI_CHANGE_COMPLETE)) {
		    emcioStatus.status = RCS_EXEC;
		}
	    }
	    break;

	case EMC_TOOL_START_CHANGE_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_START_CHANGE\n");
	    // only handle this if explicitly enabled - less backwards config breakage
	    if ((proto > V1) && support_start_change) {
		*(iocontrol_data->start_change) = 1;
		*(iocontrol_data->state) = ST_START_CHANGE;
		// delay fetching the next message until ack line seen
		if (! (input_status & TI_START_CHANGE_ACKED)) {
		    emcioStatus.status = RCS_EXEC;
		}
	    }
	    break;

	case EMC_TOOL_UNLOAD_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_UNLOAD\n");
	    emcioStatus.tool.toolInSpindle = 0;
	    break;

	case EMC_TOOL_LOAD_TOOL_TABLE_TYPE:
	{
	    const char *filename =
		((EMC_TOOL_LOAD_TOOL_TABLE *) emcioCommand)->file;
	    if (!strlen(filename)) filename = tool_table_file;
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD_TOOL_TABLE\n");
	    if (0 != loadToolTable(filename, emcioStatus.tool.toolTable,
				   ttcomments, random_toolchanger))
		emcioStatus.status = RCS_ERROR;
	    else
		reload_tool_number(emcioStatus.tool.toolInSpindle);
	}
	break;

	case EMC_TOOL_SET_OFFSET_TYPE:
	{
	    int p, t, o;
	    double d, f, b;
	    EmcPose offs;

	    p = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->pocket;
	    t = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->toolno;
	    offs = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->offset;
	    d = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->diameter;
	    f = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->frontangle;
	    b = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->backangle;
	    o = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->orientation;

	    rtapi_print_msg(RTAPI_MSG_DBG,
			    "EMC_TOOL_SET_OFFSET pocket=%d toolno=%d zoffset=%lf, xoffset=%lf, diameter=%lf,"
			    " frontangle=%lf, backangle=%lf, orientation=%d\n",
			    p, t, offs.tran.z, offs.tran.x, d, f, b, o);

	    emcioStatus.tool.toolTable[p].toolno = t;
	    emcioStatus.tool.toolTable[p].offset = offs;
	    emcioStatus.tool.toolTable[p].diameter = d;
	    emcioStatus.tool.toolTable[p].frontangle = f;
	    emcioStatus.tool.toolTable[p].backangle = b;
	    emcioStatus.tool.toolTable[p].orientation = o;

	    if (emcioStatus.tool.toolInSpindle == t) {
		emcioStatus.tool.toolTable[0] = emcioStatus.tool.toolTable[p];
	    }
	}
	if (0 != saveToolTable(tool_table_file, emcioStatus.tool.toolTable, ttcomments, random_toolchanger))
	    emcioStatus.status = RCS_ERROR;
	break;

	case EMC_TOOL_SET_NUMBER_TYPE:
	{
	    // changed als in interp_convert.cc to convey the pocket number, not the tool number
	    // needed so toolTable[0] can be properly set including offsets
	    int number;

	    number = ((EMC_TOOL_SET_NUMBER *) emcioCommand)->tool;
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_SET_NUMBER pocket=%d old_loaded=%d new_number=%d\n",
			    number, emcioStatus.tool.toolInSpindle,
			    emcioStatus.tool.toolTable[number].toolno);
	    emcioStatus.tool.toolInSpindle = emcioStatus.tool.toolTable[number].toolno;
	    load_tool(number);
	    *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle; //likewise in HAL
	}
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

	case EMC_AUX_ESTOP_ON_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_ON\n");
	    /* assert an ESTOP to the outside world (thru HAL) */
	    *(iocontrol_data->user_enable_out) = 0; //disable on ESTOP_ON
	    hal_init_pins(); //resets all HAL pins to safe value
	    break;

	case EMC_AUX_ESTOP_OFF_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_OFF\n");
	    /* remove ESTOP */
	    *(iocontrol_data->user_enable_out) = 1; // we're good to enable on ESTOP_OFF
	    /* generate a rising edge to reset optional HAL latch */
	    *(iocontrol_data->user_request_enable) = 1;
	    break;

	case EMC_AUX_ESTOP_RESET_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_RESET\n");
	    // doesn't do anything right now, this will need to come from GUI
	    // but that means task needs to be rewritten/rethinked
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

	case EMC_SET_DEBUG_TYPE:
	    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SET_DEBUG\n");
	    emc_debug = ((EMC_SET_DEBUG *) emcioCommand)->debug;
	    break;

	default:
	    rtapi_print("IO: unknown command %s\n", emcSymbolLookup(type));
	    break;
	}			/* switch (type) */

	// ack for the received command
	emcioStatus.command_type = type;
	emcioStatus.echo_serial_number = emcioCommand->serial_number;
	emcioStatus.heartbeat++;
	emcioStatus.reason = toolchanger_reason;  // always piggyback current fault code
	emcioStatusBuffer->write(&emcioStatus);

	esleep(emc_io_cycle_time);
	/* clear reset line to allow for a later rising edge */
	*(iocontrol_data->user_request_enable) = 0;

    }	// end of "while (! done)" loop

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

    for(int i=0; i<CANON_POCKETS_MAX; i++) {
	free(ttcomments[i]);
    }
    rtapi_print("%s: exiting\n",progname);
    exit(0);
}
