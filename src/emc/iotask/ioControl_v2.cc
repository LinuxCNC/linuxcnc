/********************************************************************
 * Description: IoControl_v2.cc
 *           Simply accepts NML messages sent to the IO controller
 *           outputs those to a HAL pin,
 *           and sends back a "Done" message.
 *
 *   Built as a C plugin (.so) loaded by the linuxcnc-launcher via:
 *
 *       load iov2
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
 * Last change: Michael Haberler 1/2011 - rework & add v2 protocol support
 *              Ported to cmod plugin API
 ********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <atomic>

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

#include "launcher/pkg/cmodule/gomc_env.h"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

typedef enum {
    V1 = 1,
    V2 = 2
} version_t;

// extend EMC_IO_ABORT_REASON_ENUM from emc.hh
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

struct iocontrol_str {
    gomc_hal_bit_t *user_enable_out;        /* output, TRUE when EMC wants stop */
    gomc_hal_bit_t *emc_enable_in;        /* input, TRUE on any external stop */
    gomc_hal_bit_t *user_request_enable;        /* output, used to reset ENABLE latch */
    gomc_hal_bit_t *coolant_mist;        /* coolant mist output pin */
    gomc_hal_bit_t *coolant_flood;        /* coolant flood output pin */
    gomc_hal_bit_t *lube;                /* lube output pin */
    gomc_hal_bit_t *lube_level;        /* lube level input pin */

    // the following pins are needed for toolchanging
    //tool-prepare
    gomc_hal_bit_t *tool_prepare;        /* output, pin that notifies HAL it needs to prepare a tool */
    gomc_hal_s32_t *tool_prep_pocket;/* output, pin that holds the P word from the tool table entry matching the tool to be prepared,
                                   only valid when tool-prepare=TRUE */
    gomc_hal_s32_t *tool_prep_index; /* output, internal index (idx) of prepped tool above */
    gomc_hal_s32_t *tool_prep_number;/* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    gomc_hal_s32_t *tool_number;     /* output, pin that holds the tool number currently in the spindle */
    gomc_hal_bit_t *tool_prepared;        /* input, pin that notifies that the tool has been prepared */
    //tool-change
    gomc_hal_bit_t *tool_change;        /* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    gomc_hal_bit_t *tool_changed;        /* input, notifies tool has been changed */

    // v2 protocol
    // iocontrolv2 -> toolchanger
    gomc_hal_bit_t *emc_abort;         /* output, signals emc-originated abort to toolchanger */
    gomc_hal_bit_t *emc_abort_ack;         /* input, handshake line to acknowledge abort_tool_change */
    gomc_hal_s32_t *emc_reason;             /* output, convey cause for EMC-originated abort to toolchanger.
                                 * UI informational. Valid during emc-abort True.
                                 */
    // toolchanger -> iocontrolv2
    gomc_hal_bit_t *toolchanger_fault;        /* input, toolchanger signals fault . Always monitored.
                                         * A fault is recorded in the toolchange_faulted pin
                                         */
    gomc_hal_bit_t *toolchanger_fault_ack;        /* handshake line for above signal. will be set by iocontrol
                                         * after above fault line is recognized and deasserted when
                                         * toolchanger-fault drops. Toolchanger is free to interpret
                                         * the ack; reading the -ack lines assures fault has been
                                         * received and acted upon.
                                         */
    gomc_hal_s32_t *toolchanger_reason;         /* input, convey reason code for toolchanger-originated
                                            * fault to iocontrol. read during toolchanger-fault True.
                                            * on a toolchange abort, the reason is passed to EMC in the
                                            * emcioStat message.
                                            * a positive value causes an OperatorDisplay text
                                            * a negative value causes an OperatorError text
                                            * a zero value does not cause any display
                                            */

    gomc_hal_bit_t *start_change;              /* signal begin of M6 cycle even before move to toolchange
                                           * position starts
                                           */
    gomc_hal_bit_t *start_change_ack;          /* acknowledge line for start_change */

    // other:
    gomc_hal_bit_t *toolchanger_faulted;         /* output. signals toolchanger-fault line has toggled
                                         * The next M6 will abort if True.
                                         */
    gomc_hal_bit_t *toolchanger_clear_fault;        /* input. resets TC fault condition.
                                         * Deasserts toolchanger-faulted if toolchanger-fault is line False.
                                         * Usage: UI - e.g. 'clear TC fault' button
                                         */
    gomc_hal_s32_t *state;                         /* output. Internal state for debugging */
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
    int proto;
    int support_start_change;

    // Tool table comments
    char *ttcomments[CANON_POCKETS_MAX];

    // Runtime state
    int toolchanger_reason;  // last fault reason read from toolchanger

    // Main loop thread
    pthread_t loop_thread;
    std::atomic<int> done;
    bool thread_started;
};

// predicates for testing toolchanger fault conditions
#define TC_FAULT  (*(m->hal_data->toolchanger_faulted))
#define TC_HARDFAULT (TC_FAULT && (m->toolchanger_reason <= 0))
#define TC_SOFTFAULT (TC_FAULT && (m->toolchanger_reason > 0))

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

    // make it verbose if debugging RCS
    if (emc_debug & EMC_DEBUG_IOCONTROL) {
        rtapi_set_msg_level(RTAPI_MSG_DBG);
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
            rtapi_print("invalid [EMCIO] CYCLE_TIME (%s); using default %f\n",
                        val, emc_io_cycle_time);
        }
    }

    val = env->ini->get(env->ini->ctx, "EMCIO", "PROTOCOL_VERSION");
    if (val) {
        sscanf(val, "%i", &m->proto);
    }
    rtapi_print_msg(RTAPI_MSG_DBG,"%s: [EMCIO] using v%d protocol\n", m->name, m->proto);

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
* Called By: New()
********************************************************************/
static int iocontrol_hal_init(iocontrol_module *m)
{
    int retval;

    /* STEP 1: initialise the hal component */
    m->comp_id = m->env->hal->init(m->env->hal->ctx, m->name, m->env->dl_handle, GOMC_HAL_COMP_USER);
    if (m->comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: hal_init() failed\n");
        return -1;
    }

    /* STEP 2: allocate shared memory for iocontrol data */
    m->hal_data = (iocontrol_str *) m->env->hal->malloc(m->env->hal->ctx, sizeof(iocontrol_str));
    if (m->hal_data == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: hal_malloc() failed\n");
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    /* STEP 3a: export the out-pin(s) */

    // user-enable-out
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->user_enable_out), m->comp_id,
                              "%s.user-enable-out", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin user-enable-out export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // user-request-enable
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->user_request_enable), m->comp_id,
                             "%s.user-request-enable", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin user-request-enable export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // coolant-flood
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->coolant_flood), m->comp_id,
                         "%s.coolant-flood", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin coolant-flood export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // coolant-mist
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->coolant_mist), m->comp_id,
                              "%s.coolant-mist", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin coolant-mist export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // lube
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->lube), m->comp_id,
                              "%s.lube", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin lube export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-number
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_number), m->comp_id,
                              "%s.tool-number", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-number export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prep-number
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prep_number), m->comp_id,
                              "%s.tool-prep-number", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-prep-number export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prep-pocket
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prep_pocket), m->comp_id,
                              "%s.tool-prep-pocket", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-prep-pocket export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prep-index (idx)
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prep_index), m->comp_id,
                              "%s.tool-prep-index", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-prep-index export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prepare
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_prepare), m->comp_id,
                              "%s.tool-prepare", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-prepare export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-prepared
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->tool_prepared), m->comp_id,
                              "%s.tool-prepared", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-prepared export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-change
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->tool_change), m->comp_id,
                              "%s.tool-change", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-change export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // tool-changed
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->tool_changed), m->comp_id,
                        "%s.tool-changed", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin tool-changed export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    /* STEP 3b: export the in-pin(s) */

    // emc-enable-in
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->emc_enable_in), m->comp_id,
                             "%s.emc-enable-in", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin emc-enable-in export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }
    // lube_level
    retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->lube_level), m->comp_id,
                             "%s.lube_level", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin lube_level export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    // state pin (present in all protocol versions)
    retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->state), m->comp_id,
                              "%s.state", m->name);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOV2: ERROR: %s pin state export failed with err=%i\n",
                        m->name, retval);
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        return -1;
    }

    // v2 protocol pins
    if (m->proto > V1) {
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->emc_abort), m->comp_id,
                                  "%s.emc-abort", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin emc-abort export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->emc_abort_ack), m->comp_id,
                                  "%s.emc-abort-ack", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin emc-abort-ack export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->emc_reason), m->comp_id,
                                  "%s.emc-reason", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin emc-reason export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->toolchanger_fault), m->comp_id,
                                  "%s.toolchanger-fault", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin toolchanger-fault export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->toolchanger_fault_ack), m->comp_id,
                                  "%s.toolchanger-fault-ack", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin toolchanger-fault-ack export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_s32_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->toolchanger_reason), m->comp_id,
                                  "%s.toolchanger-reason", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin toolchanger-reason export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->toolchanger_faulted), m->comp_id,
                                  "%s.toolchanger-faulted", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin toolchanger-faulted export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->toolchanger_clear_fault), m->comp_id,
                                  "%s.toolchanger-clear-fault", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin toolchanger-clear-fault export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_OUT, &(m->hal_data->start_change), m->comp_id,
                                  "%s.start-change", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin start-change export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
        retval = gomc_hal_pin_bit_newf(m->env->hal, GOMC_HAL_IN, &(m->hal_data->start_change_ack), m->comp_id,
                                  "%s.start-change-ack", m->name);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "IOV2: ERROR: %s pin start-change-ack export failed with err=%i\n",
                            m->name, retval);
            m->env->hal->exit(m->env->hal->ctx, m->comp_id);
            return -1;
        }
    }

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: iocontrol_hal_init() complete\n", m->name);
    m->env->hal->ready(m->env->hal->ctx, m->comp_id);
    return 0;
}


/********************************************************************
 *
 * Description: hal_init_pins(void)
 *
 * Side Effects: Sets HAL pins default values.
 *
 * Called By: New, EMC_IO_INIT handler
 ********************************************************************/
static void hal_init_pins(iocontrol_module *m)
{
    iocontrol_str *d = m->hal_data;
    *(d->user_enable_out) = 0;
    *(d->user_request_enable) = 0;
    *(d->coolant_mist) = 0;
    *(d->coolant_flood) = 0;
    *(d->lube) = 0;
    *(d->tool_prepare) = 0;
    *(d->tool_prep_number) = 0;
    *(d->tool_prep_pocket) = 0;
    *(d->tool_prep_index) = 0;
    *(d->tool_change) = 0;

    *(d->state) = ST_IDLE;

    if (m->proto > V1) {
        // v2 protocol output pins
        *(d->emc_abort) = 0;
        *(d->emc_reason) = 0;
        *(d->toolchanger_fault_ack) = 0;
        *(d->toolchanger_faulted) = 0;
        *(d->start_change) = 0;
    }
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
        if (tooldata_put(tzero, idx) == IDX_FAIL) {
            UNEXPECTED_MSG;
        }

        // pocket-->spindle (idx==0)
        tooldata_db_notify(SPINDLE_LOAD,tpocket.toolno,0,tpocket);
        tpocket.pocketno = 0;
        if (tooldata_put(tpocket,0) == IDX_FAIL) {
            UNEXPECTED_MSG;
        }

        comment_temp = m->ttcomments[0];
        m->ttcomments[0] = m->ttcomments[idx];
        m->ttcomments[idx] = comment_temp;

        if (0 != tooldata_save(m->io_tool_table_file, m->ttcomments)) {
            m->emcioStatus.status = RCS_ERROR;
        }
    } else if(idx == 0) {
            // magic T0 = pocket 0 = no tool
        tdata = tooldata_entry_init();
        tdata.toolno   = 0; // nonrandom unload tool from spindle
        tdata.pocketno = 0; // nonrandom unload tool from spindle
        if (tooldata_put(tdata,0) == IDX_FAIL) {
            UNEXPECTED_MSG; return;
        }
        if (tooldata_db_notify(SPINDLE_UNLOAD,0,0,tdata)) { UNEXPECTED_MSG; }
    } else {
        // just copy the desired tool to the spindle
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if (tooldata_put(tdata,0) == IDX_FAIL) {
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
    if(m->random_toolchanger) return; // doesn't need special handling here
    for(int idx = 1; idx <= tooldata_last_index_get(); idx++) { //note <=
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if(tdata.toolno == toolno) {
            load_tool(m, idx);
            break;
        }
    }
}

static char *str_input(int status)
{
    static char seen[200];

    seen[0] = 0;
    if (status & TI_PREPARING) rtapi_strxcat(seen," TI_PREPARING");
    if (status & TI_PREPARE_COMPLETE) rtapi_strxcat(seen," TI_PREPARE_COMPLETE");
    if (status & TI_CHANGING) rtapi_strxcat(seen," TI_CHANGING");
    if (status & TI_CHANGE_COMPLETE) rtapi_strxcat(seen," TI_CHANGE_COMPLETE");
    if (status & TI_TC_FAULT) rtapi_strxcat(seen," TI_TC_FAULT");
    if (status & TI_TC_ABORT) rtapi_strxcat(seen," TI_TC_ABORT");
    if (status & TI_EMC_ABORT_SIGNALED) rtapi_strxcat(seen," TI_EMC_ABORT_SIGNALED");
    if (status & TI_EMC_ABORT_ACKED) rtapi_strxcat(seen," TI_EMC_ABORT_ACKED");
    if (status & TI_ESTOP_CHANGED) rtapi_strxcat(seen," TI_ESTOP_CHANGED");
    if (status & TI_LUBELEVEL_CHANGED) rtapi_strxcat(seen," TI_LUBELEVEL_CHANGED");
    if (status & TI_START_CHANGE) rtapi_strxcat(seen," TI_START_CHANGE");
    if (status & TI_START_CHANGE_ACKED) rtapi_strxcat(seen," TI_START_CHANGE_ACKED");
    return seen;
}

/********************************************************************
 *
 * Description: read_inputs()
 *                        Reads pin values from HAL
 *                      Handle pin protocol
 *                        this function gets called once per cycle
 *                        It sets the values for the emcioStatus.aux.*
 *                      also handle estop and lube_level
 *
 * Returns:        returns a bitmask of relevant status
 *
 * Side Effects: updates toolchanger_reason,
 *               emcioStatus.tool.*
 *               emcioStatus.aux.estop
 *               emcioStatus.lube.level
 *
 * Called By: iocontrol_loop every CYCLE
 ********************************************************************/
static int read_inputs(iocontrol_module *m)
{
    int oldval, retval = 0;
    iocontrol_str *d = m->hal_data;

    oldval = m->emcioStatus.aux.estop;

    if ( *(d->emc_enable_in)==0) //check for estop from HW
        m->emcioStatus.aux.estop = 1;
    else
        m->emcioStatus.aux.estop = 0;

    if (oldval != m->emcioStatus.aux.estop) {
        retval |= TI_ESTOP_CHANGED;
    }

    oldval = m->emcioStatus.lube.level;
    m->emcioStatus.lube.level = *(d->lube_level); // check for lube_level from HW
    if (oldval != m->emcioStatus.lube.level) {
        retval |=  TI_LUBELEVEL_CHANGED;
    }

    if (m->proto > V1) {
        // record toolchanger fault
        if (*d->toolchanger_fault) {
            // reason code now valid
            m->toolchanger_reason = *d->toolchanger_reason;
            rtapi_print_msg(RTAPI_MSG_DBG, "%s:read_input: toolchanger fault signaled, reason: %d \n",
                            m->name, m->toolchanger_reason);

            // raise ack line
            *(d->toolchanger_fault_ack) = 1;
            // record the fact that a fault or abort was received
            *(d->toolchanger_faulted) = 1;
            retval |= TI_TC_FAULT;
        } else {
            // clear the ack line
            *(d->toolchanger_fault_ack) = 0;
        }

        // clear toolchanger fault condition if so signaled,
        if (*d->toolchanger_clear_fault) {
            *(d->toolchanger_faulted) = 0;
            // clear last tc fault reason
            m->toolchanger_reason = 0;
            retval &= ~TI_TC_FAULT;
        }

        // an EMC-side abort is in progress.
        if (*d->emc_abort) {
            if (*d->emc_abort_ack) {
                // the toolchanger acknowledged the fact so deassert
                *(d->emc_abort) = 0;
                // a completed EMC-side abort always forces IDLE state
                *(d->state) = ST_IDLE;
                retval |= TI_EMC_ABORT_ACKED;
            } else {
                // waiting for abort being acked by toolchanger.
                retval |= TI_EMC_ABORT_SIGNALED;
            }
        }
        // the very start of an M6 operation was signaled
        if (*d->start_change) {
            if (*d->start_change_ack) {
                // the toolchanger acknowledged the start-change
                *(d->start_change) = 0;
                retval |= TI_START_CHANGE_ACKED;
            } else {
                // wait for ack by toolchanger
                retval |= TI_START_CHANGE;
            }
        }
    }

    if (*d->tool_prepare) {
        if (*d->tool_prepared) {
            m->emcioStatus.tool.pocketPrepped = *(d->tool_prep_index); //check if tool has been prepared
            *(d->tool_prepare) = 0;
            *(d->state) = ST_IDLE; // normal prepare completion
            retval |= TI_PREPARE_COMPLETE;
        } else {
            *(d->state) = ST_PREPARING;
            retval |= TI_PREPARING;
        }
    }

    if (*d->tool_change) {

        // check whether a toolchanger fault will force an abort of this change
        if ((m->proto > V1) && (*d->toolchanger_faulted))  {

            m->toolchanger_reason = *d->toolchanger_reason;
            *(d->emc_reason) = EMC_ABORT_BY_TOOLCHANGER_FAULT;
            *(d->emc_abort) = 1;
            retval |= TI_TC_ABORT;
            // give up on request
            *(d->tool_change) = 0;
            *(d->state) = ST_WAIT_FOR_ABORT_ACK;
        }

        if (*d->tool_changed) {
            // good to commit this change
            if(!m->random_toolchanger && m->emcioStatus.tool.pocketPrepped == 0) {
                m->emcioStatus.tool.toolInSpindle = 0;
            } else {
                // the tool now in the spindle is the one that was prepared
                CANON_TOOL_TABLE tdata;
                if (tooldata_get(&tdata,m->emcioStatus.tool.pocketPrepped) != IDX_OK) {
                    UNEXPECTED_MSG; return -1;
                }
                m->emcioStatus.tool.toolInSpindle = tdata.toolno;
            }
            *(d->tool_number) = m->emcioStatus.tool.toolInSpindle;
            load_tool(m, m->emcioStatus.tool.pocketPrepped);
            m->emcioStatus.tool.pocketPrepped = -1;
            *(d->tool_prep_number) = 0;
            *(d->tool_prep_pocket) = 0;
            *(d->tool_prep_index)  = 0;
            *(d->tool_change) = 0;
            *(d->state) = ST_IDLE;
            retval |= TI_CHANGE_COMPLETE;
        } else {
            retval |= TI_CHANGING;
        }
    }
    return retval;
}


static void update_status(iocontrol_module *m, int status, int serial)
{
    static int status_reported = -1;
    iocontrol_str *d = m->hal_data;

    m->emcioStatus.status = status;
    if (status_reported != status) {
        rtapi_print_msg(RTAPI_MSG_DBG, "%s: updating status=%s state=%s fault=%d reason=%d\n",
                        m->name, strcs[m->emcioStatus.status], strstate[*(d->state)],
                        m->emcioStatus.fault, m->emcioStatus.reason
                        );
        status_reported = m->emcioStatus.status;
    }
    m->emcioStatus.command_type = EMC_IO_STAT_TYPE;
    m->emcioStatus.echo_serial_number = serial;
    m->emcioStatus.heartbeat++;
    m->emcioStatusBuffer->write(&m->emcioStatus);
}


/********************************************************************
* iocontrol_loop — main NML processing loop, runs in a dedicated thread.
* Replaces the old while(!done) loop from main().
********************************************************************/
static void *iocontrol_loop(void *arg)
{
    iocontrol_module *m = (iocontrol_module *)arg;
    iocontrol_str *d = m->hal_data;
    int input_status;
    NMLTYPE type;

    while (!m->done) {

        input_status = read_inputs(m);

        // always piggyback fault and reason
        m->emcioStatus.fault =  *(d->toolchanger_faulted);
        m->emcioStatus.reason = m->toolchanger_reason;

        if (input_status & (TI_ESTOP_CHANGED|TI_LUBELEVEL_CHANGED)) {
            if (input_status & TI_ESTOP_CHANGED) {
                rtapi_print_msg(RTAPI_MSG_DBG, "%s:ESTOP changed to %d\n", m->name, m->emcioStatus.aux.estop);
            }
            if (input_status & TI_LUBELEVEL_CHANGED)
                rtapi_print_msg(RTAPI_MSG_DBG, "%s:lube_level changed to %d\n", m->name, m->emcioStatus.lube.level);

            update_status(m, RCS_DONE, m->emcioCommand->serial_number + 1);
        }

        if (input_status & (TI_PREPARING)) {
            update_status(m, RCS_EXEC, m->emcioCommand->serial_number);
        }

        if (input_status & (TI_START_CHANGE|TI_CHANGING)) {
            if (TC_FAULT) {
                rtapi_print_msg(RTAPI_MSG_DBG, "%s: signaling fault during change, reason=%d state=%s input_status=%s\n",
                                m->name,
                                m->toolchanger_reason,
                                strstate[*(d->state)],
                                str_input(input_status)
                                );
                update_status(m, RCS_ERROR, m->emcioCommand->serial_number);
            } else {
                update_status(m, RCS_EXEC, m->emcioCommand->serial_number);
            }
        }

        if (input_status & (TI_PREPARE_COMPLETE|TI_CHANGE_COMPLETE|TI_START_CHANGE_ACKED)) {
            update_status(m, RCS_DONE, m->emcioCommand->serial_number);
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

        m->emcioStatus.status = RCS_DONE;
        type = m->emcioCommand->type;

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
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT reason=%d\n",
                            ((EMC_TOOL_ABORT *) m->emcioCommand)->reason);

            m->emcioStatus.coolant.mist = 0;
            m->emcioStatus.coolant.flood = 0;
            *(d->coolant_mist) = 0;
            *(d->coolant_flood) = 0;

            if (m->proto > V1) {
                *(d->emc_reason) = ((EMC_TOOL_ABORT *) m->emcioCommand)->reason;
                *(d->emc_abort) = 1;
            }
            *(d->tool_change) = 0;
            *(d->tool_prepare) = 0;
            *(d->start_change) = 0;

            *(d->state) = (m->proto > V1) ? ST_WAIT_FOR_ABORT_ACK : ST_IDLE;
            break;

        case EMC_TOOL_PREPARE_TYPE:
        {
            int idx = 0;
            int toolno = ((EMC_TOOL_PREPARE*)m->emcioCommand)->tool;
            CANON_TOOL_TABLE tdata;
            idx = tooldata_find_index_for_tool(toolno);
#ifdef TOOL_NML
            if (!m->random_toolchanger && toolno == 0) { idx = 0; }
#endif
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE tool=%d pocket=%d\n", toolno, idx);

            if (m->random_toolchanger && idx == 0) {
                break;
            }
            *(d->tool_prep_index) = idx;
            if (idx == -1) {
                m->emcioStatus.tool.pocketPrepped = 0;
            } else {
                if (tooldata_get(&tdata,idx) != IDX_OK) {
                     UNEXPECTED_MSG;
                }
                *(d->tool_prep_pocket) = m->random_toolchanger ? idx : tdata.pocketno;
                if (!m->random_toolchanger && idx == 0) {
                    *(d->tool_prep_number) = 0;
                    *(d->tool_prep_pocket) = 0;
                } else {
                    *(d->tool_prep_number) = tdata.toolno;
                    if (tdata.toolno != toolno)
                        rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE: mismatch: tooltable[%d]=%d, got %d\n",
                                idx, tdata.toolno, toolno);
                }
                if ((m->proto > V1) && *(d->toolchanger_faulted)) {
                    rtapi_print_msg(RTAPI_MSG_DBG, "%s: prepare: toolchanger faulted (reason=%d), next M6 will %s\n",
                        m->name, m->toolchanger_reason,
                        m->toolchanger_reason > 0 ? "set fault code and reason" : "abort program");
                }
                *(d->tool_prepare) = 1;
                *(d->state) = ST_PREPARING;

                if (!(input_status & TI_PREPARE_COMPLETE)) {
                    m->emcioStatus.status = RCS_EXEC;
                }
            }
        }
        break;

        case EMC_TOOL_LOAD_TYPE:
        {
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_LOAD loaded=%d prepped=%d\n",
                            m->emcioStatus.tool.toolInSpindle, m->emcioStatus.tool.pocketPrepped);

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
                *(d->state) = ST_CHANGING;

                if (! (input_status & TI_CHANGE_COMPLETE)) {
                    m->emcioStatus.status = RCS_EXEC;
                }
            }
            break;
        }
        case EMC_TOOL_START_CHANGE_TYPE:
            rtapi_print_msg(RTAPI_MSG_DBG, "EMC_START_CHANGE\n");
            if ((m->proto > V1) && m->support_start_change) {
                *(d->start_change) = 1;
                *(d->state) = ST_START_CHANGE;
                if (! (input_status & TI_START_CHANGE_ACKED)) {
                    m->emcioStatus.status = RCS_EXEC;
                }
            }
            break;

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
                if (tooldata_put(tdata,idx) == IDX_FAIL) {
                    UNEXPECTED_MSG;
                }
                if (0 != tooldata_save(m->io_tool_table_file, m->ttcomments)) {
                    m->emcioStatus.status = RCS_ERROR;
                }
                if (m->io_db_mode == DB_ACTIVE) {
                    int pno = idx;
                    if (!m->random_toolchanger) { pno = tdata.pocketno; }
                    if (tooldata_db_notify(TOOL_OFFSET,toolno,pno,tdata)) { UNEXPECTED_MSG; }
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
                 "EMC_TOOL_SET_NUMBER idx=%d old_loaded=%d new_number=%d\n",
                 idx, m->emcioStatus.tool.toolInSpindle, tdata.toolno);
            *(d->tool_number) = m->emcioStatus.tool.toolInSpindle;
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

        default:
            rtapi_print("IO: unknown command %s\n", emcSymbolLookup(type));
            break;
        }                        /* switch (type) */

        // ack for the received command
        m->emcioStatus.command_type = type;
        m->emcioStatus.echo_serial_number = m->emcioCommand->serial_number;
        m->emcioStatus.heartbeat++;
        m->emcioStatus.reason = m->toolchanger_reason;
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
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "can't connect to NML buffers in %s\n",
                        emc_nmlfile);
        return -1;
    }

    m->done = 0;
    m->thread_started = true;
    if (pthread_create(&m->loop_thread, NULL, iocontrol_loop, m) != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "IOV2: ERROR: pthread_create failed\n");
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

    rtapi_print("%s: exiting\n", m->name);
    delete m;
}

/********************************************************************
* New — cmod factory function (extern "C").
* The launcher calls dlsym(handle, "New") to find this.
********************************************************************/
extern "C" int New(const cmod_env_t *env, const char *name,
                   int argc, const char **argv, cmod_t **out)
{
#ifdef TOOL_NML //{
  #define TOOL_DATA "nml"
#else //}{
  #define TOOL_DATA "mmap"
#endif //}

    iocontrol_module *m = new iocontrol_module();

    m->env = env;
    strncpy(m->name, name, sizeof(m->name) - 1);

    // Default tool table
    strncpy(m->io_tool_table_file, "tool.tbl", sizeof(m->io_tool_table_file) - 1);

    // Default protocol version
    m->proto = V2;
    m->support_start_change = 0;

    // Debug flag from environment (legacy)
    if (getenv("IO_DEBUG")) {
        m->io_debug = true;
        fprintf(stderr,"%8d IOV2: %s\n",getpid(),TOOL_DATA);
        fprintf(stderr,"         EMC_STAT      size=%8ld\n",sizeof(EMC_STAT));
        fprintf(stderr,"         EMC_IO_STAT   size=%8ld\n",sizeof(EMC_IO_STAT));
        fprintf(stderr,"         EMC_TOOL_STAT size=%8ld\n",sizeof(EMC_TOOL_STAT));
        fprintf(stderr,"         CANON_POCKETS_MAX =%8d\n",CANON_POCKETS_MAX);
    }

    // Parse arguments (replace old argv parsing from main)
    for (int t = 0; t < argc; t++) {
        if (!strcmp(argv[t], "-support-start-change") || !strcmp(argv[t], "support-start-change")) {
            m->support_start_change = 1;
        }
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
    if (m->io_debug) {
        fprintf(stderr,"IOV2: REGISTER %p\n",
                (CANON_TOOL_TABLE*)&(m->emcioStatus.tool.toolTable));
    }
#else //}{
    tool_mmap_creator((EMC_TOOL_STAT*)&(m->emcioStatus.tool), m->random_toolchanger);
    if (m->io_debug) {
        fprintf(stderr,"IOV2: CREATOR  random_toolchanger=%d\n", m->random_toolchanger);
    }
#endif //}

    if (m->io_db_mode == DB_ACTIVE) {
        if (tooldata_db_init(m->db_program, m->random_toolchanger)) {
            fprintf(stderr,"\n%5d IOV2::tooldata_db_init() FAIL\n\n",getpid());
            m->io_db_mode = DB_NOTUSED;
        }
    }

    if(!m->random_toolchanger) {
        m->ttcomments[0][0] = '\0';
        CANON_TOOL_TABLE tdata = tooldata_entry_init();
        tdata.pocketno =  0;
        tdata.toolno   = -1;
        if (tooldata_put(tdata,0) == IDX_FAIL) {
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
