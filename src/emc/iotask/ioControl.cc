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
*  After restoring normal condition (UEO and EEST closed), an additional
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
* Last change:
********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include "hal.h"                /* access to HAL functions/definitions */
#include "rtapi.h"                /* rtapi_print_msg */
#include "rcs.hh"                /* RCS_CMD_CHANNEL */
#include "emc.hh"                /* EMC NML */
#include "emc_nml.hh"
#include "emcglb.h"                /* EMC_NMLFILE, EMC_INIFILE, TOOL_TABLE_FILE */
#include "inifile.hh"                /* INIFILE */
#include "nml_oi.hh"
#include "timer.hh"
#include "rcs_print.hh"
#include <rtapi_string.h>
#include "tooldata.hh"

static bool io_debug = 0;
#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

static RCS_CMD_CHANNEL *emcioCommandBuffer = 0;
static RCS_CMD_MSG *emcioCommand = 0;
static RCS_STAT_CHANNEL *emcioStatusBuffer = 0;
static EMC_IO_STAT emcioStatus;
static NML *emcErrorBuffer = 0;

static char io_tool_table_file[LINELEN] = "tool.tbl"; // default
static char *ttcomments[CANON_POCKETS_MAX];

static int      random_toolchanger  = 0;
static tooldb_t io_db_mode          = DB_NOTUSED;
static char     db_program[LINELEN] = {0};

struct iocontrol_str {
    hal_bit_t *user_enable_out;        /* output, TRUE when EMC wants stop */
    hal_bit_t *emc_enable_in;        /* input, TRUE on any external stop */
    hal_bit_t *user_request_enable;        /* output, used to reset ENABLE latch */
    hal_bit_t *coolant_mist;        /* coolant mist output pin */
    hal_bit_t *coolant_flood;        /* coolant flood output pin */
    hal_bit_t *lube;                /* lube output pin */
    hal_bit_t *lube_level;        /* lube level input pin */


    // the following pins are needed for toolchanging
    //tool-prepare
    hal_bit_t *tool_prepare;        /* output, pin that notifies HAL it needs to prepare a tool */
    hal_s32_t *tool_prep_pocket;/* output, pin that holds the pocketno for the tool table entry matching the tool to be prepared,
                                   only valid when tool-prepare=TRUE */
    hal_s32_t *tool_prep_index; /* output, pin for internal index (idx) of prepped tool above */
    hal_s32_t *tool_prep_number;/* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    hal_s32_t *tool_number;     /* output, pin that holds the tool number currently in the spindle */
    hal_bit_t *tool_prepared;        /* input, pin that notifies that the tool has been prepared */
    //tool-change
    hal_bit_t *tool_change;        /* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    hal_bit_t *tool_changed;        /* input, notifies tool has been changed */

    // note: spindle control has been moved to motion
} * iocontrol_data;                        //pointer to the HAL-struct

//static iocontrol_struct *iocontrol_data;
static int comp_id;                                /* component ID */

/********************************************************************
*
* Description: emcIoNmlGet()
*                Attempts to connect to NML buffers and set the relevant
*                pointers.
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
    bool tooltable_specified = 0;

    /* Open the ini file */
    if (inifile.Open(filename) == false) {
        return -1;
    }

    if (NULL != (inistring = inifile.Find("TOOL_TABLE", "EMCIO"))) {
        strncpy(io_tool_table_file, inistring, sizeof(io_tool_table_file)-1);
        tooltable_specified = 1;
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

    if (NULL != (inistring = inifile.Find("NML_FILE", "EMC"))) {
        rtapi_strxcpy(emc_nmlfile, inistring);
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

    inifile.Find(&random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");

    io_db_mode = DB_NOTUSED;
    if(NULL != (inistring = inifile.Find("DB_PROGRAM", "EMCIO"))) {
        rtapi_strxcpy(db_program,inistring);
        io_db_mode = DB_ACTIVE;
        if (tooltable_specified) {
            fprintf(stderr,"DB_PROGRAM active: IGNORING tool table file %s\n",
                    io_tool_table_file);
        }
    }
    // close it
    inifile.Close();

    return 0;
}

static int done = 0;

/********************************************************************
*
* Description: quit(int sig)
*                Signal handler for SIGINT - Usually generated by a
*                Ctrl C sequence from the keyboard.
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
static int iocontrol_hal_init(void)
{
    int n = 0, retval;                //n - number of the hal component (only one for iocotrol)

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

    // user-enable-out
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->user_enable_out), comp_id,
                              "iocontrol.%d.user-enable-out", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin user-enable-out export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // user-request-enable
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->user_request_enable), comp_id,
                             "iocontrol.%d.user-request-enable", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin user-request-enable export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // coolant-flood
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->coolant_flood), comp_id,
                         "iocontrol.%d.coolant-flood", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin coolant-flood export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // coolant-mist
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->coolant_mist), comp_id,
                              "iocontrol.%d.coolant-mist", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin coolant-mist export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // lube
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->lube), comp_id,
                              "iocontrol.%d.lube", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin lube export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-prepare
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->tool_prepare), comp_id,
                              "iocontrol.%d.tool-prepare", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prepare export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-number
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_number), comp_id,
                              "iocontrol.%d.tool-number", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-number export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-prep-number
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_prep_number), comp_id,
                              "iocontrol.%d.tool-prep-number", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-number export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }

    // tool-prep-index (idx)
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_prep_index), comp_id,
                              "iocontrol.%d.tool-prep-index", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-index export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }

    // tool-prep-pocket
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_prep_pocket), comp_id,
                              "iocontrol.%d.tool-prep-pocket", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-pocket export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-prepared
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->tool_prepared), comp_id,
                              "iocontrol.%d.tool-prepared", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prepared export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-change
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->tool_change), comp_id,
                              "iocontrol.%d.tool-change", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-change export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-changed
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->tool_changed), comp_id,
                        "iocontrol.%d.tool-changed", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-changed export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    /* STEP 3b: export the in-pin(s) */

    // emc-enable-in
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->emc_enable_in), comp_id,
                             "iocontrol.%d.emc-enable-in", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin emc-enable-in export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // lube_level
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->lube_level), comp_id,
                             "iocontrol.%d.lube_level", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin lube_level export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);

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
static void hal_init_pins(void)
{
    *(iocontrol_data->user_enable_out)=0;    /* output, FALSE when EMC wants stop */
    *(iocontrol_data->user_request_enable)=0;/* output, used to reset HAL latch */
    *(iocontrol_data->coolant_mist)=0;       /* coolant mist output pin */
    *(iocontrol_data->coolant_flood)=0;      /* coolant flood output pin */
    *(iocontrol_data->lube)=0;               /* lube output pin */
    *(iocontrol_data->tool_prepare)=0;       /* output, pin that notifies HAL it needs to prepare a tool */
    *(iocontrol_data->tool_prep_number)=0;   /* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    *(iocontrol_data->tool_prep_pocket)=0;   /* output, pin that holds the pocketno for the tool to be prepared, only valid when tool-prepare=TRUE */
    *iocontrol_data->tool_prep_index=0;      /* output, pin that holds the internal index (idx) of the tool to be prepared, for debug */
    *(iocontrol_data->tool_change)=0;        /* output, notifies a tool-change should happen (emc should be in the tool-change position) */
}


/********************************************************************
*
* Description: read_hal_inputs(void)
*                        Reads the pin values from HAL
*                        this function gets called once per cycle
*                        It sets the values for the emcioStatus.aux.*
*
* Returns:        returns > 0 if any of the status has changed
*                we then need to update through NML
*
* Side Effects: updates values
*
* Called By: main every CYCLE
********************************************************************/
static int read_hal_inputs(void)
{
    int oldval, retval = 0;

    oldval = emcioStatus.aux.estop;

    if ( *(iocontrol_data->emc_enable_in)==0) //check for estop from HW
        emcioStatus.aux.estop = 1;
    else
        emcioStatus.aux.estop = 0;

    if (oldval != emcioStatus.aux.estop) {
        retval = 1;
    }


    oldval = emcioStatus.lube.level;
    emcioStatus.lube.level = *(iocontrol_data->lube_level);        //check for lube_level from HW
    if (oldval != emcioStatus.lube.level) {
        retval = 1;
    }
    return retval;
}

void load_tool(int idx) {
    CANON_TOOL_TABLE tdata;
    if(random_toolchanger) {
        char *comment_temp;
        // swap the tools between the desired pocket and the spindle pocket

        CANON_TOOL_TABLE tzero,tpocket;
        if (   tooldata_get(&tzero,0    ) != IDX_OK
            || tooldata_get(&tpocket,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        // spindle-->pocket (specified by idx)
        tooldata_db_notify(tzero.toolno,idx,tzero);
        tzero.pocketno = tpocket.pocketno;
        if (tooldata_put(tzero,idx) != IDX_OK) {
            UNEXPECTED_MSG;
        }

        // pocket-->spindle (idx==0)
        tooldata_db_notify(tpocket.toolno,0,tpocket);
        tpocket.pocketno = 0;
        if (tooldata_put(tpocket,0) != IDX_OK) {
            UNEXPECTED_MSG;
        }

        comment_temp = ttcomments[0];
        ttcomments[0] = ttcomments[idx];
        ttcomments[idx] = comment_temp;

        if (0 != tooldata_save(io_tool_table_file,ttcomments)) {
            emcioStatus.status = RCS_ERROR;
        }
    } else if(idx == 0) {
        // on non-random tool-changers, asking for pocket 0 is the secret
        // handshake for "unload the tool from the spindle"
        tdata = tooldata_entry_init();
        tdata.toolno   = 0; // nonrandom unload tool from spindle
        tdata.pocketno = 0; // nonrandom unload tool from spindle
        if (tooldata_put(tdata,0) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if (tooldata_db_notify(0,0,tdata)) { UNEXPECTED_MSG; }
    } else {
        // just copy the desired tool to the spindle
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if (tooldata_put(tdata,0) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        // notify idx==0 tool in spindle:
        CANON_TOOL_TABLE temp;
        if (tooldata_get(&temp,0) != IDX_OK) { UNEXPECTED_MSG; }
        if (tooldata_db_notify(temp.toolno,0,temp)) { UNEXPECTED_MSG; }
    }
} // load_tool()

void reload_tool_number(int toolno) {
    CANON_TOOL_TABLE tdata;
    if(random_toolchanger) return; // doesn't need special handling here
    for(int idx = 1; idx <= tooldata_last_index_get(); idx++) { //note <=
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if(tdata.toolno == toolno) {
            load_tool(idx);
            break;
        }
    }
}

/********************************************************************
*
* Description: read_tool_inputs(void)
*                        Reads the tool-pin values from HAL
*                        this function gets called once per cycle
*                        It sets the values for the emcioStatus.aux.*
*
* Returns:        returns which of the status has changed
*                we then need to update through NML (a bit different as read_hal_inputs)
*
* Side Effects: updates values
*
* Called By: main every CYCLE
********************************************************************/
static int read_tool_inputs(void)
{
    if (*iocontrol_data->tool_prepare && *iocontrol_data->tool_prepared) {
        emcioStatus.tool.pocketPrepped = *iocontrol_data->tool_prep_index; //check if tool has been (idx) prepared
        *(iocontrol_data->tool_prepare) = 0;
        emcioStatus.status = RCS_DONE;  // we finally finished to do tool-changing, signal task with RCS_DONE
        return 10; //prepped finished
    }

    if (*iocontrol_data->tool_change && *iocontrol_data->tool_changed) {
        if(!random_toolchanger && emcioStatus.tool.pocketPrepped == 0) {
            emcioStatus.tool.toolInSpindle = 0;
        } else {
            // the tool now in the spindle is the one that was prepared
            CANON_TOOL_TABLE tdata;
            if (tooldata_get(&tdata,emcioStatus.tool.pocketPrepped) != IDX_OK) {
                UNEXPECTED_MSG; return -1;
            }
            emcioStatus.tool.toolInSpindle = tdata.toolno;
        }
        *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle; //likewise in HAL
        load_tool(emcioStatus.tool.pocketPrepped);
        emcioStatus.tool.pocketPrepped = -1; //reset the tool preped number, -1 to permit tool 0 to be loaded
        *(iocontrol_data->tool_prep_number) = 0; //likewise in HAL
        *(iocontrol_data->tool_prep_pocket) = 0; //likewise in HAL
        *(iocontrol_data->tool_prep_index)  = 0; //likewise in HAL
        *(iocontrol_data->tool_change) = 0; //also reset the tool change signal
        emcioStatus.status = RCS_DONE;        // we finally finished to do tool-changing, signal task with RCS_DONE
        return 11; //change finished
    }
    return 0;
}

static void do_hal_exit(void) {
    hal_exit(comp_id);
}

/********************************************************************
*
* Description: main(int argc, char * argv[])
*                Connects to NML buffers and enters an endless loop
*                processing NML IO commands. Print statements are
*                sent to the console indicating which IO command was
*                executed if debug level is set to RTAPI_MSG_DBG.
*
* Return Value: Zero or -1 if ini file not found or failure to connect
*                to NML buffers.
*
* Side Effects: None.
*
* Called By:
*
********************************************************************/
int main(int argc, char *argv[])
{
#ifdef TOOL_NML //{
  #define TOOL_DATA "nml"
#else //}{
  #define TOOL_DATA "mmap"
#endif //}
    if (getenv( (char*)"IO_DEBUG" )) { getpid(),io_debug = 1; }
    if (io_debug) {
        fprintf(stderr,"%8d IO: %s\n",getpid(),TOOL_DATA);
        fprintf(stderr,"         EMC_STAT      size=%8ld\n",sizeof(EMC_STAT));
        fprintf(stderr,"         EMC_IO_STAT   size=%8ld\n",sizeof(EMC_IO_STAT));
        fprintf(stderr,"         EMC_TOOL_STAT size=%8ld\n",sizeof(EMC_TOOL_STAT));
        fprintf(stderr,"         CANON_POCKETS_MAX =%8d\n",CANON_POCKETS_MAX);
    }

    int tool_status;
    NMLTYPE type;
    for (int t = 1; t < argc; t++) {
        if (!strcmp(argv[t], "-ini")) {
            if (t == argc - 1) {
                return -1;
            } else {
                if (strlen(argv[t+1]) >= LINELEN) {
                    rtapi_print_msg(RTAPI_MSG_ERR, "ini file name too long (max %d)\n", LINELEN);
                    rtapi_print_msg(RTAPI_MSG_ERR, "    %s\n", argv[t+1]);
                    return -1;
                }
                rtapi_strxcpy(emc_inifile, argv[t + 1]);
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

    if (iocontrol_hal_init() != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "can't initialize the HAL\n");
        return -1;
    }

    atexit(do_hal_exit);
#ifdef TOOL_NML //{
#else //}{
    atexit(tool_mmap_close);
#endif //}

    if (0 != iniLoad(emc_inifile)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "can't open ini file %s\n",
                        emc_inifile);
        return -1;
    }

    if (0 != emcIoNmlGet()) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "can't connect to NML buffers in %s\n",
                        emc_nmlfile);
        return -1;
    }
    for(int i=0; i<CANON_POCKETS_MAX; i++) {
        ttcomments[i] = (char *)malloc(CANON_TOOL_ENTRY_LEN);
    }

    tooldata_init(random_toolchanger);
    tooldata_set_db(io_db_mode);
    // io process must be started before task
#ifdef TOOL_NML //{
    tool_nml_register( (CANON_TOOL_TABLE*)&(emcioStatus.tool.toolTable));
    if (io_debug) {
        fprintf(stderr,"IO: REGISTER %p\n",
                (CANON_TOOL_TABLE*)&(emcioStatus.tool.toolTable));
    }
#else //}{
    tool_mmap_creator((EMC_TOOL_STAT*)&(emcioStatus.tool), random_toolchanger);
    if (io_debug) {
        fprintf(stderr,"IO: CREATOR  random_toolchanger=%d\n",random_toolchanger);
    }
#endif //}

    if (io_db_mode == DB_ACTIVE) {
        if (tooldata_db_init(db_program,random_toolchanger)) {
            fprintf(stderr,"\n%5d IO::tooldata_db_init() FAIL\n\n",getpid());
            io_db_mode = DB_NOTUSED;
        }
    }

    // on nonrandom machines, always start by assuming the spindle is empty
    if(!random_toolchanger) {
        ttcomments[0][0] = '\0';
        CANON_TOOL_TABLE tdata = tooldata_entry_init();
        tdata.pocketno =  0; //nonrandom init
        tdata.toolno   = -1; //nonrandom init
        if (tooldata_put(tdata,0) != IDX_OK) {
            UNEXPECTED_MSG;
        }
    }
    if (0 != tooldata_load(io_tool_table_file, ttcomments)) {
        rcs_print_error("can't load tool table.\n");
    }
    done = 0;

    /* set status values to 'normal' */
    emcioStatus.aux.estop = 1; //estop=1 means to emc that ESTOP condition is met
    emcioStatus.tool.pocketPrepped = -1;
    if (random_toolchanger) {
        CANON_TOOL_TABLE tdata;
        if (tooldata_get(&tdata,0) != IDX_OK) {
            UNEXPECTED_MSG; return -1;
        }
        emcioStatus.tool.toolInSpindle = tdata.toolno;
    } else {
        emcioStatus.tool.toolInSpindle = 0;
    }
    emcioStatus.coolant.mist = 0;
    emcioStatus.coolant.flood = 0;
    emcioStatus.lube.on = 0;
    emcioStatus.lube.level = 1;
    *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle;

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
            esleep(emc_io_cycle_time);
            /* and repeat */
            continue;
        }

        if (0 == emcioCommand ||        // bad command pointer
            0 == emcioCommand->type ||        // bad command type
            emcioCommand->serial_number == emcioStatus.echo_serial_number) {        // command already finished
            /* wait until next cycle */
            esleep(emc_io_cycle_time);
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
            tooldata_load(io_tool_table_file, ttcomments);
            reload_tool_number(emcioStatus.tool.toolInSpindle);
            break;

        case EMC_TOOL_HALT_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_HALT\n");
            break;

        case EMC_TOOL_ABORT_TYPE:
            // this gets sent on any Task Abort, so it might be safer to stop
            // the spindle  and coolant
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT\n");
            emcioStatus.coolant.mist = 0;
            emcioStatus.coolant.flood = 0;
            *(iocontrol_data->coolant_mist)=0;                /* coolant mist output pin */
            *(iocontrol_data->coolant_flood)=0;                /* coolant flood output pin */
            *(iocontrol_data->tool_change)=0;                /* abort tool change if in progress */
            *(iocontrol_data->tool_prepare)=0;                /* abort tool prepare if in progress */
            break;

        case EMC_TOOL_PREPARE_TYPE:
            {
                int idx = 0;
                int toolno = ((EMC_TOOL_PREPARE*)emcioCommand)->tool;
                CANON_TOOL_TABLE tdata;
                idx   = tooldata_find_index_for_tool(toolno);
                if (idx == -1) {  // not found
                    emcioStatus.tool.pocketPrepped = -1;
                } else {
                    if (tooldata_get(&tdata,idx) != IDX_OK) {
                        UNEXPECTED_MSG;
                    }
                    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE tool=%d idx=%d\n", toolno, idx);

                    *(iocontrol_data->tool_prep_index)  = idx; // any type of changer

                    // Note: some of the following logic could be simplified
                    //       but is maintained to preserve runtests expectations
                    // Set HAL pins for tool number, pocket, and index.
                    if (random_toolchanger) {
                        // RANDOM_TOOLCHANGER
                        *(iocontrol_data->tool_prep_number) = tdata.toolno;
                        if (idx == 0) {
                            emcioStatus.tool.pocketPrepped      = 0; // pocketPrepped is an idx
                            *(iocontrol_data->tool_prep_pocket) = 0;
                            break;
                        }
                        *(iocontrol_data->tool_prep_pocket) = tdata.pocketno;
                    } else {
                        // NON_RANDOM_TOOLCHANGER
                        if (idx == 0) {
                            emcioStatus.tool.pocketPrepped      = 0; // pocketPrepped is an idx
                            *(iocontrol_data->tool_prep_number) = 0;
                            *(iocontrol_data->tool_prep_pocket) = 0;
                        } else {
                            *(iocontrol_data->tool_prep_number) = tdata.toolno;
                            *(iocontrol_data->tool_prep_pocket) = tdata.pocketno;
                        }
                    }
                    // it doesn't make sense to prep the spindle pocket
                    if (random_toolchanger && idx == 0) {
                        emcioStatus.tool.pocketPrepped = 0; // idx
                        break;
                    }
                }

                /* then set the prepare pin to tell external logic to get started */
                *(iocontrol_data->tool_prepare) = 1;
                // the feedback logic is done inside read_hal_inputs()
                // we only need to set RCS_EXEC if RCS_DONE is not already set by the above logic
                if (tool_status != 10) //set above to 10 in case PREP already finished (HAL loopback machine)
                    emcioStatus.status = RCS_EXEC;
            }
            break;
        case EMC_TOOL_LOAD_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD loaded=%d prepped=%d\n", emcioStatus.tool.toolInSpindle, emcioStatus.tool.pocketPrepped);

        {
            // it doesn't make sense to load a tool from the spindle pocket
            if (random_toolchanger && emcioStatus.tool.pocketPrepped == 0) {
                break;
            }

            // it's not necessary to load the tool already in the spindle
            CANON_TOOL_TABLE tdata;
            if (tooldata_get(&tdata, emcioStatus.tool.pocketPrepped) != IDX_OK) {
                UNEXPECTED_MSG;
            }
            if (!random_toolchanger && (emcioStatus.tool.pocketPrepped > 0) &&
                (emcioStatus.tool.toolInSpindle == tdata.toolno) ) {
                break;
            }

            if (emcioStatus.tool.pocketPrepped != -1) {
                //notify HW for toolchange
                *(iocontrol_data->tool_change) = 1;
                // the feedback logic is done inside read_hal_inputs() we only
                // need to set RCS_EXEC if RCS_DONE is not already set by the
                // above logic
                if (tool_status != 11)
                    // set above to 11 in case LOAD already finished (HAL
                    // loopback machine)
                    emcioStatus.status = RCS_EXEC;
            }
            break;
        }
        case EMC_TOOL_UNLOAD_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_UNLOAD\n");
            emcioStatus.tool.toolInSpindle = 0;
            break;

        case EMC_TOOL_LOAD_TOOL_TABLE_TYPE:
            {
                const char *filename =
                    ((EMC_TOOL_LOAD_TOOL_TABLE *) emcioCommand)->file;
                if(!strlen(filename)) filename = io_tool_table_file;
                rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD_TOOL_TABLE\n");
                if (0 != tooldata_load(filename, ttcomments)) {
                    emcioStatus.status = RCS_ERROR;
                } else {
                    reload_tool_number(emcioStatus.tool.toolInSpindle);
                }
            }
            break;
        case EMC_TOOL_SET_OFFSET_TYPE:
            {
                int idx, toolno, o;
                double d, f, b;
                EmcPose offs;

                idx    = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->pocket;
                toolno = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->toolno;
                offs   = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->offset;
                d = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->diameter;
                f = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->frontangle;
                b = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->backangle;
                o = ((EMC_TOOL_SET_OFFSET *) emcioCommand)->orientation;

                rtapi_print_msg(RTAPI_MSG_DBG,
                     "EMC_TOOL_SET_OFFSET idx=%d toolno=%d zoffset=%lf, "
                     "xoffset=%lf, diameter=%lf, "
                     "frontangle=%lf, backangle=%lf, orientation=%d\n",
                     idx, toolno, offs.tran.z, offs.tran.x, d, f, b, o);
                CANON_TOOL_TABLE tdata;
                if (tooldata_get(&tdata,idx) != IDX_OK) {
                    UNEXPECTED_MSG;
                }
                tdata.toolno = toolno;
                tdata.offset = offs;
                tdata.diameter = d;
                tdata.frontangle = f;
                tdata.backangle = b;
                tdata.orientation = o;
                if (tooldata_put(tdata,idx) != IDX_OK) {
                    UNEXPECTED_MSG;
                }
                if (0 != tooldata_save(io_tool_table_file, ttcomments)) {
                    emcioStatus.status = RCS_ERROR;
                }
                if (io_db_mode == DB_ACTIVE) {
                    int pno = idx; // for random_toolchanger
                    if (!random_toolchanger) { pno = tdata.pocketno; }
                    if (tooldata_db_notify(toolno,pno,tdata)) { UNEXPECTED_MSG; }
                }
            }
            break;

        case EMC_TOOL_SET_NUMBER_TYPE: //m61q-
            {
                int idx;

                idx = ((EMC_TOOL_SET_NUMBER *) emcioCommand)->tool;
                CANON_TOOL_TABLE tdata;
                if (tooldata_get(&tdata,idx) != IDX_OK) {
                    UNEXPECTED_MSG;
                }
                load_tool(idx);

                idx=0; // update spindle (fix legacy behavior)
                if (tooldata_get(&tdata,idx) != IDX_OK) {
                   UNEXPECTED_MSG;
                }
                emcioStatus.tool.toolInSpindle = tdata.toolno;
                rtapi_print_msg(RTAPI_MSG_DBG,
                     "EMC_TOOL_SET_NUMBER old_loaded_tool=%d new_idx_number=%d new_tool=%d\n"
                     , emcioStatus.tool.toolInSpindle, idx, tdata.toolno);
                //likewise in HAL
                *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle;
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
            *(iocontrol_data->user_enable_out) = 1; //we're good to enable on ESTOP_OFF
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

        case EMC_TOOL_START_CHANGE_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_START_CHANGE\n");
            break;

        default:
            rtapi_print("IO: unknown command %s\n", emcSymbolLookup(type));
            break;
        }                        /* switch (type) */

        // ack for the received command
        emcioStatus.command_type = type;
        emcioStatus.echo_serial_number = emcioCommand->serial_number;
        //set above, to allow some commands to fail this
        //emcioStatus.status = RCS_DONE;
        emcioStatus.heartbeat++;
        emcioStatusBuffer->write(&emcioStatus);

        esleep(emc_io_cycle_time);
        /* clear reset line to allow for a later rising edge */
        *(iocontrol_data->user_request_enable) = 0;

    }        // end of "while (! done)" loop

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

    return 0;
}
