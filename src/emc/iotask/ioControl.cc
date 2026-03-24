/********************************************************************
* Description: IoControl.cc
*           Simply accepts NML messages sent to the IO controller
*           outputs those to a HAL pin,
*           and sends back a "Done" message.
*
*   Built as a C plugin (.so) loaded by the linuxcnc-launcher via:
*
*       load iocontrol
*
*   The launcher calls New (constructor), Init (HAL pins), Start
*   (NML + main loop thread), Stop (signal shutdown), and DeInit
*   (release resources) in that order during its lifecycle.
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
#include <ctype.h>
#include <pthread.h>
#include "hal.h"                /* access to HAL functions/definitions */
#include "rtapi.h"                /* rtapi_print_msg */
#include "rcs.hh"                /* RCS_CMD_CHANNEL */
#include "emc.hh"                /* EMC NML */
#include "emc_nml.hh"
#include "emcglb.h"                /* EMC_NMLFILE, EMC_INIFILE, TOOL_TABLE_FILE */
#include "nml_oi.hh"
#include "timer.hh"
#include "rcs_print.hh"
#include <rtapi_string.h>
#include "tooldata.hh"
#include "launcher/pkg/cmodule/cmodule.h"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

struct iocontrol_str {
    hal_bit_t *user_enable_out;        /* output, TRUE when EMC wants stop */
    hal_bit_t *emc_enable_in;        /* input, TRUE on any external stop */
    hal_bit_t *user_request_enable;        /* output, used to reset ENABLE latch */
    hal_bit_t *coolant_mist;        /* coolant mist output pin */
    hal_bit_t *coolant_flood;        /* coolant flood output pin */
    hal_bit_t *lube;                /* lube output pin */
    hal_bit_t *lube_level;        /* lube level input pin */

    // the following pins are needed for toolchanging
    hal_bit_t *tool_prepare;        /* output, pin that notifies HAL it needs to prepare a tool */
    hal_s32_t *tool_prep_pocket;/* output, pin that holds the pocketno for the tool table entry matching the tool to be prepared */
    hal_s32_t *tool_from_pocket;/* output, pin indicating pocket current load tool retrieved from*/
    hal_s32_t *tool_prep_index; /* output, pin for internal index (idx) of prepped tool above */
    hal_s32_t *tool_prep_number;/* output, pin that holds the tool number to be prepared */
    hal_s32_t *tool_number;     /* output, pin that holds the tool number currently in the spindle */
    hal_bit_t *tool_prepared;        /* input, pin that notifies that the tool has been prepared */
    hal_bit_t *tool_change;        /* output, notifies a tool-change should happen */
    hal_bit_t *tool_changed;        /* input, notifies tool has been changed */
};

// iocontrol_module holds all per-instance state.  Allocated in New(),
// freed in DeInit().
struct iocontrol_module {
    cmod_t base;                        // must be first — contains lifecycle vtable
    const cmod_env_t *env;              // launcher-provided environment
    char name[64];                      // instance name (HAL component name)

    // HAL
    int comp_id;
    iocontrol_str *hal_data;

    // NML
    RCS_CMD_CHANNEL *emcioCommandBuffer;
    RCS_CMD_MSG *emcioCommand;
    RCS_STAT_CHANNEL *emcioStatusBuffer;
    EMC_IO_STAT emcioStatus;
    NML *emcErrorBuffer;

    // Configuration (read from INI via env->get_ini in New)
    bool io_debug;
    char io_tool_table_file[LINELEN];
    int random_toolchanger;
    tooldb_t io_db_mode;
    char db_program[LINELEN];

    // Tool table comments
    char *ttcomments[CANON_POCKETS_MAX];

    // Main loop thread
    pthread_t loop_thread;
    volatile int done;
    bool thread_started;
};

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
* Called By: iocontrol_start()
*
********************************************************************/
static int emcIoNmlGet(iocontrol_module *m)
{
    int retval = 0;

    /* Try to connect to EMC IO command buffer */
    if (m->emcioCommandBuffer == 0) {
        m->emcioCommandBuffer =
            new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "tool", emc_nmlfile);
        if (!m->emcioCommandBuffer->valid()) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "emcToolCmd buffer not available\n");
            delete m->emcioCommandBuffer;
            m->emcioCommandBuffer = 0;
            retval = -1;
        } else {
            /* Get our command data structure */
            m->emcioCommand = m->emcioCommandBuffer->get_address();
        }
    }

    /* try to connect to EMC IO status buffer */
    if (m->emcioStatusBuffer == 0) {
        m->emcioStatusBuffer =
            new RCS_STAT_CHANNEL(emcFormat, "toolSts", "tool",
                                 emc_nmlfile);
        if (!m->emcioStatusBuffer->valid()) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "toolSts buffer not available\n");
            delete m->emcioStatusBuffer;
            m->emcioStatusBuffer = 0;
            retval = -1;
        } else {
            /* initialize and write status */
            m->emcioStatus.heartbeat = 0;
            m->emcioStatus.command_type = 0;
            m->emcioStatus.echo_serial_number = 0;
            m->emcioStatus.status = RCS_DONE;
            m->emcioStatusBuffer->write(&m->emcioStatus);
        }
    }

    /* try to connect to EMC error buffer */
    if (m->emcErrorBuffer == 0) {
        m->emcErrorBuffer =
            new NML(nmlErrorFormat, "emcError", "tool", emc_nmlfile);
        if (!m->emcErrorBuffer->valid()) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "emcError buffer not available\n");
            delete m->emcErrorBuffer;
            m->emcErrorBuffer = 0;
            retval = -1;
        }
    }

    return retval;
}

// iniLoad reads configuration from the launcher's parsed INI via env callbacks.
// Replaces the old iniLoad() that opened and parsed the INI file directly.
static int iniLoad(iocontrol_module *m)
{
    const cmod_env_t *env = m->env;
    const char *val;
    bool tooltable_specified = false;

    val = env->get_ini(env->ctx, "EMCIO", "TOOL_TABLE");
    if (val) {
        strncpy(m->io_tool_table_file, val, sizeof(m->io_tool_table_file) - 1);
        tooltable_specified = true;
    }

    val = env->get_ini(env->ctx, "EMC", "DEBUG");
    if (val) {
        if (1 != sscanf(val, "%i", &emc_debug)) {
            emc_debug = 0;
        }
    } else {
        emc_debug = 0;
    }

    val = env->get_ini(env->ctx, "EMC", "NML_FILE");
    if (val) {
        rtapi_strxcpy(emc_nmlfile, val);
    }

    val = env->get_ini(env->ctx, "EMCIO", "CYCLE_TIME");
    if (val) {
        double temp = emc_io_cycle_time;
        if (1 != sscanf(val, "%lf", &emc_io_cycle_time)) {
            emc_io_cycle_time = temp;
            rtapi_print("invalid [EMCIO] CYCLE_TIME (%s); using default %f\n",
                        val, emc_io_cycle_time);
        }
    }

    val = env->get_ini(env->ctx, "EMCIO", "RANDOM_TOOLCHANGER");
    if (val) {
        m->random_toolchanger = atoi(val);
    }

    m->io_db_mode = DB_NOTUSED;
    val = env->get_ini(env->ctx, "EMCIO", "DB_PROGRAM");
    if (val) {
        rtapi_strxcpy(m->db_program, val);
        m->io_db_mode = DB_ACTIVE;
        if (tooltable_specified) {
            fprintf(stderr, "DB_PROGRAM active: IGNORING tool table file %s\n",
                    m->io_tool_table_file);
        }
    }

    return 0;
}

/********************************************************************
*
* Description: iocontrol_hal_init()
*
* Side Effects: Exports HAL pins.
*
* Called By: iocontrol_init()
********************************************************************/
static int iocontrol_hal_init(iocontrol_module *m)
{
    int n = 0, retval;

    /* STEP 1: initialise the hal component */
    m->comp_id = hal_init(m->name);
    if (m->comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: hal_init() failed\n");
        return -1;
    }

    /* STEP 2: allocate shared memory for iocontrol data */
    m->hal_data = (iocontrol_str *) hal_malloc(sizeof(iocontrol_str));
    if (m->hal_data == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: hal_malloc() failed\n");
        hal_exit(m->comp_id);
        return -1;
    }

    /* STEP 3a: export the out-pin(s) */

    // user-enable-out
    retval = hal_pin_bit_newf(HAL_OUT, &(m->hal_data->user_enable_out), m->comp_id,
                              "iocontrol.%d.user-enable-out", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin user-enable-out export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // user-request-enable
    retval = hal_pin_bit_newf(HAL_OUT, &(m->hal_data->user_request_enable), m->comp_id,
                             "iocontrol.%d.user-request-enable", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin user-request-enable export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // coolant-flood
    retval = hal_pin_bit_newf(HAL_OUT, &(m->hal_data->coolant_flood), m->comp_id,
                         "iocontrol.%d.coolant-flood", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin coolant-flood export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // coolant-mist
    retval = hal_pin_bit_newf(HAL_OUT, &(m->hal_data->coolant_mist), m->comp_id,
                              "iocontrol.%d.coolant-mist", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin coolant-mist export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // lube
    retval = hal_pin_bit_newf(HAL_OUT, &(m->hal_data->lube), m->comp_id,
                              "iocontrol.%d.lube", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin lube export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // tool-prepare
    retval = hal_pin_bit_newf(HAL_OUT, &(m->hal_data->tool_prepare), m->comp_id,
                              "iocontrol.%d.tool-prepare", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prepare export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // tool-number
    retval = hal_pin_s32_newf(HAL_OUT, &(m->hal_data->tool_number), m->comp_id,
                              "iocontrol.%d.tool-number", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-number export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // tool-prep-number
    retval = hal_pin_s32_newf(HAL_OUT, &(m->hal_data->tool_prep_number), m->comp_id,
                              "iocontrol.%d.tool-prep-number", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-number export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }

    // tool-prep-index (idx)
    retval = hal_pin_s32_newf(HAL_OUT, &(m->hal_data->tool_prep_index), m->comp_id,
                              "iocontrol.%d.tool-prep-index", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-index export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }

    // tool-prep-pocket
    retval = hal_pin_s32_newf(HAL_OUT, &(m->hal_data->tool_prep_pocket), m->comp_id,
                              "iocontrol.%d.tool-prep-pocket", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-pocket export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // tool-from-pocket
    retval = hal_pin_s32_newf(HAL_OUT, &(m->hal_data->tool_from_pocket), m->comp_id,
                              "iocontrol.%d.tool-from-pocket", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-from-pocket export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // tool-prepared
    retval = hal_pin_bit_newf(HAL_IN, &(m->hal_data->tool_prepared), m->comp_id,
                              "iocontrol.%d.tool-prepared", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prepared export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // tool-change
    retval = hal_pin_bit_newf(HAL_OUT, &(m->hal_data->tool_change), m->comp_id,
                              "iocontrol.%d.tool-change", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-change export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // tool-changed
    retval = hal_pin_bit_newf(HAL_IN, &(m->hal_data->tool_changed), m->comp_id,
                        "iocontrol.%d.tool-changed", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-changed export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    /* STEP 3b: export the in-pin(s) */

    // emc-enable-in
    retval = hal_pin_bit_newf(HAL_IN, &(m->hal_data->emc_enable_in), m->comp_id,
                             "iocontrol.%d.emc-enable-in", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin emc-enable-in export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }
    // lube_level
    retval = hal_pin_bit_newf(HAL_IN, &(m->hal_data->lube_level), m->comp_id,
                             "iocontrol.%d.lube_level", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin lube_level export failed with err=%i\n",
                        n, retval);
        hal_exit(m->comp_id);
        return -1;
    }

    hal_ready(m->comp_id);

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
static void hal_init_pins(iocontrol_module *m)
{
    iocontrol_str *d = m->hal_data;
    *(d->user_enable_out)=0;
    *(d->user_request_enable)=0;
    *(d->coolant_mist)=0;
    *(d->coolant_flood)=0;
    *(d->lube)=0;
    *(d->tool_prepare)=0;
    *(d->tool_prep_number)=0;
    *(d->tool_prep_pocket)=0;
    *(d->tool_from_pocket)=0;
    *(d->tool_prep_index)=0;
    *(d->tool_change)=0;
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
static int read_hal_inputs(iocontrol_module *m)
{
    int oldval, retval = 0;
    iocontrol_str *d = m->hal_data;

    oldval = m->emcioStatus.aux.estop;

    if ( *(d->emc_enable_in)==0)
        m->emcioStatus.aux.estop = 1;
    else
        m->emcioStatus.aux.estop = 0;

    if (oldval != m->emcioStatus.aux.estop) {
        retval = 1;
    }

    oldval = m->emcioStatus.lube.level;
    m->emcioStatus.lube.level = *(d->lube_level);
    if (oldval != m->emcioStatus.lube.level) {
        retval = 1;
    }
    return retval;
}

static void load_tool(iocontrol_module *m, int idx) {
    CANON_TOOL_TABLE tdata;
    if(m->random_toolchanger) {
        char *comment_temp;
        // swap the tools between the desired pocket and the spindle pocket

        CANON_TOOL_TABLE tzero,tpocket;
        if (   tooldata_get(&tzero,0    ) != IDX_OK
            || tooldata_get(&tpocket,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        // spindle-->pocket (specified by idx)
        tooldata_db_notify(SPINDLE_UNLOAD,tzero.toolno,idx,tzero);
        tzero.pocketno = tpocket.pocketno;
        if (tooldata_put(tzero,idx) != IDX_OK) {
            UNEXPECTED_MSG;
        }

        // pocket-->spindle (idx==0)
        tooldata_db_notify(SPINDLE_LOAD,tpocket.toolno,0,tpocket);
        tpocket.pocketno = 0;
        if (tooldata_put(tpocket,0) != IDX_OK) {
            UNEXPECTED_MSG;
        }

        comment_temp = m->ttcomments[0];
        m->ttcomments[0] = m->ttcomments[idx];
        m->ttcomments[idx] = comment_temp;

        if (0 != tooldata_save(m->io_tool_table_file, m->ttcomments)) {
            m->emcioStatus.status = RCS_ERROR;
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
        if (tooldata_db_notify(SPINDLE_UNLOAD,0,0,tdata)) { UNEXPECTED_MSG; }
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
        if (tooldata_db_notify(SPINDLE_LOAD,temp.toolno,0,temp)) { UNEXPECTED_MSG; }
    }
} // load_tool()

static void reload_tool_number(iocontrol_module *m, int toolno) {
    CANON_TOOL_TABLE tdata;
    if(m->random_toolchanger) return;
    for(int idx = 1; idx <= tooldata_last_index_get(); idx++) {
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if(tdata.toolno == toolno) {
            load_tool(m, idx);
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
static int read_tool_inputs(iocontrol_module *m)
{
    iocontrol_str *d = m->hal_data;
    if (*d->tool_prepare && *d->tool_prepared) {
        m->emcioStatus.tool.pocketPrepped = *d->tool_prep_index;
        *(d->tool_prepare) = 0;
        m->emcioStatus.status = RCS_DONE;
        return 10;
    }

    if (*d->tool_change && *d->tool_changed) {
        if(!m->random_toolchanger && m->emcioStatus.tool.pocketPrepped == 0) {
            m->emcioStatus.tool.toolInSpindle = 0;
            m->emcioStatus.tool.toolFromPocket  =  *(d->tool_from_pocket) = 0;
        } else {
            CANON_TOOL_TABLE tdata;
            if (tooldata_get(&tdata,m->emcioStatus.tool.pocketPrepped) != IDX_OK) {
                UNEXPECTED_MSG; return -1;
            }
            m->emcioStatus.tool.toolInSpindle = tdata.toolno;
            m->emcioStatus.tool.toolFromPocket = *(d->tool_from_pocket) = tdata.pocketno;
        }
        if (m->emcioStatus.tool.toolInSpindle == 0) {
             m->emcioStatus.tool.toolFromPocket =  *(d->tool_from_pocket) = 0;
        }
        *(d->tool_number) = m->emcioStatus.tool.toolInSpindle;
        load_tool(m, m->emcioStatus.tool.pocketPrepped);
        m->emcioStatus.tool.pocketPrepped = -1;
        *(d->tool_prep_number) = 0;
        *(d->tool_prep_pocket) = 0;
        *(d->tool_prep_index)  = 0;
        *(d->tool_change) = 0;
        m->emcioStatus.status = RCS_DONE;
        return 11;
    }
    return 0;
}

/********************************************************************
* iocontrol_loop — main NML processing loop, runs in a dedicated thread.
* Replaces the old while(!done) loop from main().
********************************************************************/
static void *iocontrol_loop(void *arg)
{
    iocontrol_module *m = (iocontrol_module *)arg;
    iocontrol_str *d = m->hal_data;
    int tool_status;
    NMLTYPE type;

    while (!m->done) {
        if (read_hal_inputs(m) > 0) {
            m->emcioStatus.command_type = EMC_IO_STAT_TYPE;
            m->emcioStatus.echo_serial_number =
                m->emcioCommand->serial_number+1;
            m->emcioStatus.heartbeat++;
            m->emcioStatusBuffer->write(&m->emcioStatus);
        }

        if ( (tool_status = read_tool_inputs(m) ) > 0) {
            m->emcioStatus.command_type = EMC_IO_STAT_TYPE;
            m->emcioStatus.echo_serial_number =
                m->emcioCommand->serial_number;
            m->emcioStatus.heartbeat++;
            m->emcioStatusBuffer->write(&m->emcioStatus);
        }

        /* read NML, run commands */
        if (-1 == m->emcioCommandBuffer->read()) {
            esleep(emc_io_cycle_time);
            continue;
        }

        if (0 == m->emcioCommand ||
            0 == m->emcioCommand->type ||
            m->emcioCommand->serial_number == m->emcioStatus.echo_serial_number) {
            esleep(emc_io_cycle_time);
            continue;
        }

        type = m->emcioCommand->type;
        m->emcioStatus.status = RCS_DONE;

        switch (type) {
        case 0:
            break;

        case EMC_IO_INIT_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_IO_INIT\n");
            hal_init_pins(m);
            break;

        case EMC_TOOL_INIT_TYPE:
            tooldata_load(m->io_tool_table_file, m->ttcomments);
            reload_tool_number(m, m->emcioStatus.tool.toolInSpindle);
            break;

        case EMC_TOOL_HALT_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_HALT\n");
            break;

        case EMC_TOOL_ABORT_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT\n");
            m->emcioStatus.coolant.mist = 0;
            m->emcioStatus.coolant.flood = 0;
            *(d->coolant_mist)=0;
            *(d->coolant_flood)=0;
            *(d->tool_change)=0;
            *(d->tool_prepare)=0;
            break;

        case EMC_TOOL_PREPARE_TYPE:
            {
                int idx = 0;
                int toolno = ((EMC_TOOL_PREPARE*)m->emcioCommand)->tool;
                CANON_TOOL_TABLE tdata;
                idx  = tooldata_find_index_for_tool(toolno);
#ifdef TOOL_NML
                if (!m->random_toolchanger && toolno == 0) { idx = 0; }
#endif
                if (idx == -1) {
                    m->emcioStatus.tool.pocketPrepped = -1;
                } else {
                    if (tooldata_get(&tdata,idx) != IDX_OK) {
                        UNEXPECTED_MSG;
                    }
                    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE tool=%d idx=%d\n", toolno, idx);

                    *(d->tool_prep_index)  = idx;

                    if (m->random_toolchanger) {
                        *(d->tool_prep_number) = tdata.toolno;
                        if (idx == 0) {
                            m->emcioStatus.tool.pocketPrepped      = 0;
                            *(d->tool_prep_pocket) = 0;
                            break;
                        }
                        *(d->tool_prep_pocket) = tdata.pocketno;
                    } else {
                        if (idx == 0) {
                            m->emcioStatus.tool.pocketPrepped      = 0;
                            *(d->tool_prep_number) = 0;
                            *(d->tool_prep_pocket) = 0;
                        } else {
                            *(d->tool_prep_number) = tdata.toolno;
                            *(d->tool_prep_pocket) = tdata.pocketno;
                        }
                    }
                    if (m->random_toolchanger && idx == 0) {
                        m->emcioStatus.tool.pocketPrepped = 0;
                        break;
                    }
                }

                *(d->tool_prepare) = 1;
                if (tool_status != 10)
                    m->emcioStatus.status = RCS_EXEC;
            }
            break;
        case EMC_TOOL_LOAD_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD loaded=%d prepped=%d\n", m->emcioStatus.tool.toolInSpindle, m->emcioStatus.tool.pocketPrepped);

        {
            if (m->random_toolchanger && m->emcioStatus.tool.pocketPrepped == 0) {
                break;
            }

            CANON_TOOL_TABLE tdata;
            if (tooldata_get(&tdata, m->emcioStatus.tool.pocketPrepped) != IDX_OK) {
                UNEXPECTED_MSG;
            }
            if (!m->random_toolchanger && (m->emcioStatus.tool.pocketPrepped > 0) &&
                (m->emcioStatus.tool.toolInSpindle == tdata.toolno) ) {
                break;
            }

            if (m->emcioStatus.tool.pocketPrepped != -1) {
                *(d->tool_change) = 1;
                if (tool_status != 11)
                    m->emcioStatus.status = RCS_EXEC;
            }
            break;
        }
        case EMC_TOOL_UNLOAD_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_UNLOAD\n");
            m->emcioStatus.tool.toolInSpindle = 0;
            break;

        case EMC_TOOL_LOAD_TOOL_TABLE_TYPE:
            {
                const char *filename =
                    ((EMC_TOOL_LOAD_TOOL_TABLE *) m->emcioCommand)->file;
                if(!strlen(filename)) filename = m->io_tool_table_file;
                rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD_TOOL_TABLE\n");
                if (0 != tooldata_load(filename, m->ttcomments)) {
                    m->emcioStatus.status = RCS_ERROR;
                } else {
                    reload_tool_number(m, m->emcioStatus.tool.toolInSpindle);
                }
            }
            break;
        case EMC_TOOL_SET_OFFSET_TYPE:
            {
                int idx, toolno, o;
                double dd, f, b;
                EmcPose offs;

                idx    = ((EMC_TOOL_SET_OFFSET *) m->emcioCommand)->pocket;
                toolno = ((EMC_TOOL_SET_OFFSET *) m->emcioCommand)->toolno;
                offs   = ((EMC_TOOL_SET_OFFSET *) m->emcioCommand)->offset;
                dd = ((EMC_TOOL_SET_OFFSET *) m->emcioCommand)->diameter;
                f = ((EMC_TOOL_SET_OFFSET *) m->emcioCommand)->frontangle;
                b = ((EMC_TOOL_SET_OFFSET *) m->emcioCommand)->backangle;
                o = ((EMC_TOOL_SET_OFFSET *) m->emcioCommand)->orientation;

                rtapi_print_msg(RTAPI_MSG_DBG,
                     "EMC_TOOL_SET_OFFSET idx=%d toolno=%d zoffset=%lf, "
                     "xoffset=%lf, diameter=%lf, "
                     "frontangle=%lf, backangle=%lf, orientation=%d\n",
                     idx, toolno, offs.tran.z, offs.tran.x, dd, f, b, o);
                CANON_TOOL_TABLE tdata;
                if (tooldata_get(&tdata,idx) != IDX_OK) {
                    UNEXPECTED_MSG;
                }
                tdata.toolno = toolno;
                tdata.offset = offs;
                tdata.diameter = dd;
                tdata.frontangle = f;
                tdata.backangle = b;
                tdata.orientation = o;
                if (tooldata_put(tdata,idx) != IDX_OK) {
                    UNEXPECTED_MSG;
                }
                if (0 != tooldata_save(m->io_tool_table_file, m->ttcomments)) {
                    m->emcioStatus.status = RCS_ERROR;
                }
                if (m->io_db_mode == DB_ACTIVE) {
                    int pno = idx;
                    if (!m->random_toolchanger) { pno = tdata.pocketno; }
                    if (tooldata_db_notify(TOOL_OFFSET,toolno,pno,tdata)) {
                        UNEXPECTED_MSG;
                    }
                }
            }
            break;

        case EMC_TOOL_SET_NUMBER_TYPE:
            {
                int idx;

                idx = ((EMC_TOOL_SET_NUMBER *) m->emcioCommand)->tool;
                CANON_TOOL_TABLE tdata;
                if (tooldata_get(&tdata,idx) != IDX_OK) {
                    UNEXPECTED_MSG;
                }
                load_tool(m, idx);

                idx=0;
                if (tooldata_get(&tdata,idx) != IDX_OK) {
                   UNEXPECTED_MSG;
                }
                m->emcioStatus.tool.toolInSpindle = tdata.toolno;
                rtapi_print_msg(RTAPI_MSG_DBG,
                     "EMC_TOOL_SET_NUMBER old_loaded_tool=%d new_idx_number=%d new_tool=%d\n"
                     , m->emcioStatus.tool.toolInSpindle, idx, tdata.toolno);
                *(d->tool_number) = m->emcioStatus.tool.toolInSpindle;
                if (m->emcioStatus.tool.toolInSpindle == 0) {
                    m->emcioStatus.tool.toolFromPocket =  *(d->tool_from_pocket) = 0;
                }
            }
            break;

        case EMC_COOLANT_MIST_ON_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_MIST_ON\n");
            m->emcioStatus.coolant.mist = 1;
            *(d->coolant_mist) = 1;
            break;

        case EMC_COOLANT_MIST_OFF_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_MIST_OFF\n");
            m->emcioStatus.coolant.mist = 0;
            *(d->coolant_mist) = 0;
            break;

        case EMC_COOLANT_FLOOD_ON_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_FLOOD_ON\n");
            m->emcioStatus.coolant.flood = 1;
            *(d->coolant_flood) = 1;
            break;

        case EMC_COOLANT_FLOOD_OFF_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_COOLANT_FLOOD_OFF\n");
            m->emcioStatus.coolant.flood = 0;
            *(d->coolant_flood) = 0;
            break;

        case EMC_AUX_ESTOP_ON_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_ON\n");
            *(d->user_enable_out) = 0;
            hal_init_pins(m);
            break;

        case EMC_AUX_ESTOP_OFF_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_OFF\n");
            *(d->user_enable_out) = 1;
            *(d->user_request_enable) = 1;
            break;

        case EMC_AUX_ESTOP_RESET_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_AUX_ESTOP_RESET\n");
            break;

        case EMC_LUBE_ON_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_ON\n");
            m->emcioStatus.lube.on = 1;
            *(d->lube) = 1;
            break;

        case EMC_LUBE_OFF_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_LUBE_OFF\n");
            m->emcioStatus.lube.on = 0;
            *(d->lube) = 0;
            break;

        case EMC_SET_DEBUG_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_SET_DEBUG\n");
            emc_debug = ((EMC_SET_DEBUG *) m->emcioCommand)->debug;
            break;

        case EMC_TOOL_START_CHANGE_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_START_CHANGE\n");
            break;

        default:
            rtapi_print("IO: unknown command %s\n", emcSymbolLookup(type));
            break;
        }

        m->emcioStatus.command_type = type;
        m->emcioStatus.echo_serial_number = m->emcioCommand->serial_number;
        m->emcioStatus.heartbeat++;
        m->emcioStatusBuffer->write(&m->emcioStatus);

        esleep(emc_io_cycle_time);
        *(d->user_request_enable) = 0;

    }        // end of "while (!m->done)" loop

    return NULL;
}

/********************************************************************
* cmod lifecycle functions
********************************************************************/

static int iocontrol_init(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;
    return iocontrol_hal_init(m);
}

static int iocontrol_start(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;

    if (0 != emcIoNmlGet(m)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "can't connect to NML buffers in %s\n",
                        emc_nmlfile);
        return -1;
    }

    for(int i=0; i<CANON_POCKETS_MAX; i++) {
        m->ttcomments[i] = (char *)malloc(CANON_TOOL_ENTRY_LEN);
    }

    tooldata_init(m->random_toolchanger);
    tooldata_set_db(m->io_db_mode);

#ifdef TOOL_NML //{
    tool_nml_register( (CANON_TOOL_TABLE*)&(m->emcioStatus.tool.toolTable));
#else //}{
    tool_mmap_creator((EMC_TOOL_STAT*)&(m->emcioStatus.tool), m->random_toolchanger);
#endif //}

    if (m->io_db_mode == DB_ACTIVE) {
        if (tooldata_db_init(m->db_program, m->random_toolchanger)) {
            fprintf(stderr,"\n%5d IO::tooldata_db_init() FAIL\n\n",getpid());
            m->io_db_mode = DB_NOTUSED;
        }
    }

    if(!m->random_toolchanger) {
        m->ttcomments[0][0] = '\0';
        CANON_TOOL_TABLE tdata = tooldata_entry_init();
        tdata.pocketno =  0;
        tdata.toolno   = -1;
        if (tooldata_put(tdata,0) != IDX_OK) {
            UNEXPECTED_MSG;
        }
    }
    if (0 != tooldata_load(m->io_tool_table_file, m->ttcomments)) {
        rcs_print_error("can't load tool table.\n");
    }

    m->emcioStatus.aux.estop = 1;
    m->emcioStatus.tool.pocketPrepped = -1;
    if (m->random_toolchanger) {
        CANON_TOOL_TABLE tdata;
        if (tooldata_get(&tdata,0) != IDX_OK) {
            UNEXPECTED_MSG; return -1;
        }
        m->emcioStatus.tool.toolInSpindle = tdata.toolno;
    } else {
        m->emcioStatus.tool.toolInSpindle = 0;
    }
    m->emcioStatus.coolant.mist = 0;
    m->emcioStatus.coolant.flood = 0;
    m->emcioStatus.lube.on = 0;
    m->emcioStatus.lube.level = 1;
    *(m->hal_data->tool_number) = m->emcioStatus.tool.toolInSpindle;

    m->done = 0;
    m->thread_started = true;
    if (pthread_create(&m->loop_thread, NULL, iocontrol_loop, m) != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "IOCONTROL: ERROR: pthread_create failed\n");
        m->thread_started = false;
        return -1;
    }

    return 0;
}

static void iocontrol_stop(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;

    m->done = 1;
    if (m->thread_started) {
        pthread_join(m->loop_thread, NULL);
        m->thread_started = false;
    }
}

static void iocontrol_deinit(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;

    if (m->emcErrorBuffer != 0) {
        delete m->emcErrorBuffer;
        m->emcErrorBuffer = 0;
    }
    if (m->emcioStatusBuffer != 0) {
        delete m->emcioStatusBuffer;
        m->emcioStatusBuffer = 0;
    }
    if (m->emcioCommandBuffer != 0) {
        delete m->emcioCommandBuffer;
        m->emcioCommandBuffer = 0;
    }

    for(int i=0; i<CANON_POCKETS_MAX; i++) {
        free(m->ttcomments[i]);
        m->ttcomments[i] = NULL;
    }

#ifndef TOOL_NML
    tool_mmap_close();
#endif

    if (m->comp_id > 0) {
        hal_exit(m->comp_id);
        m->comp_id = 0;
    }

    delete m;
}

/********************************************************************
* New — cmod factory function (extern "C").
* The launcher calls dlsym(handle, "New") to find this.
********************************************************************/
extern "C" int New(const cmod_env_t *env, const char *name,
                   int argc, const char **argv, cmod_t **out)
{
    iocontrol_module *m = new iocontrol_module();

    m->env = env;
    strncpy(m->name, name, sizeof(m->name) - 1);

    // Default tool table
    strncpy(m->io_tool_table_file, "tool.tbl", sizeof(m->io_tool_table_file) - 1);

    // Debug flag from environment (legacy)
    if (getenv("IO_DEBUG")) {
        m->io_debug = true;
    }

    // Read configuration from INI via launcher callbacks
    if (0 != iniLoad(m)) {
        delete m;
        return -1;
    }

    // Wire up the cmod vtable
    m->base.Init   = iocontrol_init;
    m->base.Start  = iocontrol_start;
    m->base.Stop   = iocontrol_stop;
    m->base.DeInit = iocontrol_deinit;
    m->base.priv   = m;

    *out = &m->base;
    return 0;
}
