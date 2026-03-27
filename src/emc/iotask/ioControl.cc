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
*   The launcher calls New (constructor + HAL init), Start
*   (NML + main loop thread), Stop (signal shutdown), and Destroy
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
#include <atomic>
#include "rcs.hh"                /* RCS_CMD_CHANNEL */
#include "emc.hh"                /* EMC NML */
#include "emc_nml.hh"
#include "emcglb.h"                /* EMC_NMLFILE, EMC_INIFILE, TOOL_TABLE_FILE */
#include "nml_oi.hh"
#include "timer.hh"
#include "rcs_print.hh"
#include <rtapi_string.h>
#include "tooldata.hh"

#include "launcher/pkg/cmodule/gomc_env.h"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d",__FILE__,__LINE__);

struct iocontrol_str {
    gomc_hal_bit_t *user_enable_out;        /* output, TRUE when EMC wants stop */
    gomc_hal_bit_t *emc_enable_in;        /* input, TRUE on any external stop */
    gomc_hal_bit_t *user_request_enable;        /* output, used to reset ENABLE latch */
    gomc_hal_bit_t *coolant_mist;        /* coolant mist output pin */
    gomc_hal_bit_t *coolant_flood;        /* coolant flood output pin */
    gomc_hal_bit_t *lube;                /* lube output pin */
    gomc_hal_bit_t *lube_level;        /* lube level input pin */

    // the following pins are needed for toolchanging
    gomc_hal_bit_t *tool_prepare;        /* output, pin that notifies HAL it needs to prepare a tool */
    gomc_hal_s32_t *tool_prep_pocket;/* output, pin that holds the pocketno for the tool table entry matching the tool to be prepared */
    gomc_hal_s32_t *tool_from_pocket;/* output, pin indicating pocket current load tool retrieved from*/
    gomc_hal_s32_t *tool_prep_index; /* output, pin for internal index (idx) of prepped tool above */
    gomc_hal_s32_t *tool_prep_number;/* output, pin that holds the tool number to be prepared */
    gomc_hal_s32_t *tool_number;     /* output, pin that holds the tool number currently in the spindle */
    gomc_hal_bit_t *tool_prepared;        /* input, pin that notifies that the tool has been prepared */
    gomc_hal_bit_t *tool_change;        /* output, notifies a tool-change should happen */
    gomc_hal_bit_t *tool_changed;        /* input, notifies tool has been changed */
};

// iocontrol_module holds all per-instance state.  Allocated in New(),
// freed in Destroy().
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
    std::atomic<int> done;
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
            gomc_log_errorf(m->env->log, m->name, "emcToolCmd buffer not available");
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
            gomc_log_errorf(m->env->log, m->name, "toolSts buffer not available");
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
            gomc_log_errorf(m->env->log, m->name, "emcError buffer not available");
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

    val = env->ini->get(env->ini->ctx, "EMCIO", "TOOL_TABLE");
    if (val) {
        strncpy(m->io_tool_table_file, val, sizeof(m->io_tool_table_file) - 1);
        tooltable_specified = true;
    }

    val = env->ini->get(env->ini->ctx, "EMC", "DEBUG");
    if (val) {
        if (1 != sscanf(val, "%i", &emc_debug)) {
            emc_debug = 0;
        }
    } else {
        emc_debug = 0;
    }

    val = env->ini->get(env->ini->ctx, "EMC", "NML_FILE");
    if (val) {
        rtapi_strxcpy(emc_nmlfile, val);
    }

    val = env->ini->get(env->ini->ctx, "EMCIO", "CYCLE_TIME");
    if (val) {
        double temp = emc_io_cycle_time;
        if (1 != sscanf(val, "%lf", &emc_io_cycle_time)) {
            emc_io_cycle_time = temp;
            gomc_log_warnf(m->env->log, m->name, "invalid [EMCIO] CYCLE_TIME (%s); using default %f",
                        val, emc_io_cycle_time);
        }
    }

    val = env->ini->get(env->ini->ctx, "EMCIO", "RANDOM_TOOLCHANGER");
    if (val) {
        m->random_toolchanger = atoi(val);
    }

    m->io_db_mode = DB_NOTUSED;
    val = env->ini->get(env->ini->ctx, "EMCIO", "DB_PROGRAM");
    if (val) {
        rtapi_strxcpy(m->db_program, val);
        m->io_db_mode = DB_ACTIVE;
        if (tooltable_specified) {
            fprintf(stderr, "DB_PROGRAM active: IGNORING tool table file %s",
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
* Called By: New()
********************************************************************/
static int iocontrol_hal_init(iocontrol_module *m)
{
    int retval;

    /* STEP 1: initialise the hal component */
    m->comp_id = m->env->hal->init(m->env->hal->ctx, m->name, m->env->dl_handle, GOMC_HAL_COMP_USER);
    if (m->comp_id < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: hal_init() failed");
        return -1;
    }

    /* STEP 2: allocate shared memory for iocontrol data */
    m->hal_data = (iocontrol_str *) m->env->hal->malloc(m->env->hal->ctx, sizeof(iocontrol_str));
    if (m->hal_data == 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: hal_malloc() failed");
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    /* STEP 3a: export the out-pin(s) */

    // user-enable-out
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->user_enable_out), m->comp_id,
                              "%s.user-enable-out", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin user-enable-out export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // user-request-enable
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->user_request_enable), m->comp_id,
                             "%s.user-request-enable", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin user-request-enable export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // coolant-flood
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->coolant_flood), m->comp_id,
                         "%s.coolant-flood", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin coolant-flood export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // coolant-mist
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->coolant_mist), m->comp_id,
                              "%s.coolant-mist", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin coolant-mist export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // lube
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->lube), m->comp_id,
                              "%s.lube", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin lube export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prepare
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prepare), m->comp_id,
                              "%s.tool-prepare", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-prepare export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-number
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_number), m->comp_id,
                              "%s.tool-number", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-number export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prep-number
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prep_number), m->comp_id,
                              "%s.tool-prep-number", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-prep-number export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    // tool-prep-index (idx)
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prep_index), m->comp_id,
                              "%s.tool-prep-index", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-prep-index export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    // tool-prep-pocket
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prep_pocket), m->comp_id,
                              "%s.tool-prep-pocket", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-prep-pocket export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-from-pocket
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_from_pocket), m->comp_id,
                              "%s.tool-from-pocket", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-from-pocket export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prepared
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->tool_prepared), m->comp_id,
                              "%s.tool-prepared", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-prepared export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-change
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_change), m->comp_id,
                              "%s.tool-change", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-change export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-changed
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->tool_changed), m->comp_id,
                        "%s.tool-changed", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin tool-changed export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    /* STEP 3b: export the in-pin(s) */

    // emc-enable-in
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->emc_enable_in), m->comp_id,
                             "%s.emc-enable-in", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin emc-enable-in export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // lube_level
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->lube_level), m->comp_id,
                             "%s.lube_level", m->name);
    if (retval < 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: %s pin lube_level export failed with err=%i",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    m->env->hal->ready(m->env->hal->ctx, m->comp_id);

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
            gomc_log_debugf(m->env->log, m->name, "EMC_IO_INIT");
            hal_init_pins(m);
            break;

        case EMC_TOOL_INIT_TYPE:
            tooldata_load(m->io_tool_table_file, m->ttcomments);
            reload_tool_number(m, m->emcioStatus.tool.toolInSpindle);
            break;

        case EMC_TOOL_HALT_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_HALT");
            break;

        case EMC_TOOL_ABORT_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_ABORT");
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
                    gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_PREPARE tool=%d idx=%d", toolno, idx);

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
            gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_LOAD loaded=%d prepped=%d", m->emcioStatus.tool.toolInSpindle, m->emcioStatus.tool.pocketPrepped);

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
            gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_UNLOAD");
            m->emcioStatus.tool.toolInSpindle = 0;
            break;

        case EMC_TOOL_LOAD_TOOL_TABLE_TYPE:
            {
                const char *filename =
                    ((EMC_TOOL_LOAD_TOOL_TABLE *) m->emcioCommand)->file;
                if(!strlen(filename)) filename = m->io_tool_table_file;
                gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_LOAD_TOOL_TABLE");
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

                gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_SET_OFFSET idx=%d toolno=%d zoffset=%lf, "
                     "xoffset=%lf, diameter=%lf, "
                     "frontangle=%lf, backangle=%lf, orientation=%d",
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
                gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_SET_NUMBER old_loaded_tool=%d new_idx_number=%d new_tool=%d"
                     , m->emcioStatus.tool.toolInSpindle, idx, tdata.toolno);
                *(d->tool_number) = m->emcioStatus.tool.toolInSpindle;
                if (m->emcioStatus.tool.toolInSpindle == 0) {
                    m->emcioStatus.tool.toolFromPocket =  *(d->tool_from_pocket) = 0;
                }
            }
            break;

        case EMC_COOLANT_MIST_ON_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_COOLANT_MIST_ON");
            m->emcioStatus.coolant.mist = 1;
            *(d->coolant_mist) = 1;
            break;

        case EMC_COOLANT_MIST_OFF_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_COOLANT_MIST_OFF");
            m->emcioStatus.coolant.mist = 0;
            *(d->coolant_mist) = 0;
            break;

        case EMC_COOLANT_FLOOD_ON_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_COOLANT_FLOOD_ON");
            m->emcioStatus.coolant.flood = 1;
            *(d->coolant_flood) = 1;
            break;

        case EMC_COOLANT_FLOOD_OFF_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_COOLANT_FLOOD_OFF");
            m->emcioStatus.coolant.flood = 0;
            *(d->coolant_flood) = 0;
            break;

        case EMC_AUX_ESTOP_ON_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_AUX_ESTOP_ON");
            *(d->user_enable_out) = 0;
            hal_init_pins(m);
            break;

        case EMC_AUX_ESTOP_OFF_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_AUX_ESTOP_OFF");
            *(d->user_enable_out) = 1;
            *(d->user_request_enable) = 1;
            break;

        case EMC_AUX_ESTOP_RESET_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_AUX_ESTOP_RESET");
            break;

        case EMC_LUBE_ON_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_LUBE_ON");
            m->emcioStatus.lube.on = 1;
            *(d->lube) = 1;
            break;

        case EMC_LUBE_OFF_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_LUBE_OFF");
            m->emcioStatus.lube.on = 0;
            *(d->lube) = 0;
            break;

        case EMC_SET_DEBUG_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_SET_DEBUG");
            emc_debug = ((EMC_SET_DEBUG *) m->emcioCommand)->debug;
            break;

        case EMC_TOOL_START_CHANGE_TYPE:
            gomc_log_debugf(m->env->log, m->name, "EMC_TOOL_START_CHANGE");
            break;

        default:
            gomc_log_warnf(m->env->log, m->name, "IO: unknown command %s", emcSymbolLookup(type));
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

static int iocontrol_start(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;

    if (0 != emcIoNmlGet(m)) {
        gomc_log_errorf(m->env->log, m->name, "can't connect to NML buffers in %s",
                        emc_nmlfile);
        return -1;
    }

    m->done = 0;
    m->thread_started = true;
    if (pthread_create(&m->loop_thread, NULL, iocontrol_loop, m) != 0) {
        gomc_log_errorf(m->env->log, m->name, "IOCONTROL: ERROR: pthread_create failed");
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

static void iocontrol_destroy(cmod_t *self)
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
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
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

    // Initialise HAL component and pins.
    if (iocontrol_hal_init(m) != 0) {
        delete m;
        return -1;
    }

    // Initialise tool data and create the mmap file early so that milltask,
    // halui, and the display (which all start before Start()) can access
    // the shared tool table.
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
            fprintf(stderr,"\n%5d IO::tooldata_db_init() FAIL\n",getpid());
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
        rcs_print_error("can't load tool table.");
    }

    m->emcioStatus.aux.estop = 1;
    m->emcioStatus.tool.pocketPrepped = -1;
    if (m->random_toolchanger) {
        CANON_TOOL_TABLE tdata;
        if (tooldata_get(&tdata,0) != IDX_OK) {
            UNEXPECTED_MSG;
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            for(int i=0; i<CANON_POCKETS_MAX; i++) free(m->ttcomments[i]);
            delete m;
            return -1;
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

    // Wire up the cmod vtable
    m->base.Start   = iocontrol_start;
    m->base.Stop    = iocontrol_stop;
    m->base.Destroy = iocontrol_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
