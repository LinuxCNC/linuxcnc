/********************************************************************
* Description: IoControl.cc
*           Simply accepts NML messages sent to the IO controller
*           outputs those to a HAL pin,
*           and sends back a "Done" message.
*
*   Built as a C plugin (.so) loaded by gomc-server via:
*
*       load iocontrol
*
*   The launcher calls New (constructor + HAL init), Start
*   (NML + main loop thread), Stop (signal shutdown), and Destroy
*   (release resources) in that order during its lifecycle.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* Copyright (c) 2004 All rights reserved.
* Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
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
#include <stdatomic.h>
#include "iocontrol_stat.h"

#include "gomc/pkg/cmodule/gomc_env.h"
#include "gomc/generated/gmi/emcio/emcio_api.h"
#include "gomc/generated/gmi/tooltable/tooltable_api.h"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

typedef struct iocontrol_str {
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
} iocontrol_str;

// iocontrol_module holds all per-instance state.  Allocated in New(),
// freed in Destroy().
typedef struct iocontrol_module {
    cmod_t base;                        // must be first — contains lifecycle vtable
    const cmod_env_t *env;              // launcher-provided environment
    char name[64];                      // instance name (HAL component name)

    // HAL
    int comp_id;
    iocontrol_str *hal_data;

    // Cached IO status (used by GMI callbacks)
    iocontrol_stat_t emcioStatus;

    // Configuration (read from INI via env->get_ini in New)
    int debug;
    double io_cycle_time;
    int random_toolchanger;

    // tooltable GMI instance name (resolved at New time)
    char tooltable_instance[64];

    // tooltable API pointer (looked up once in Start)
    const tooltable_callbacks_t *tt;

    // Shutdown flag (checked by blocking GMI callbacks)
    atomic_int done;

    // GMI callback table (persists for lifetime of module)
    emcio_callbacks_t emcio_cb;
} iocontrol_module;

// iniLoad reads configuration from the launcher's parsed INI via env callbacks.
static int iniLoad(iocontrol_module *m)
{
    const cmod_env_t *env = m->env;
    const char *val;

    val = env->ini->get(env->ini->ctx, "EMC", "DEBUG");
    if (val) {
        if (1 != sscanf(val, "%i", &m->debug)) {
            m->debug = 0;
        }
    } else {
        m->debug = 0;
    }

    val = env->ini->get(env->ini->ctx, "EMCIO", "CYCLE_TIME");
    if (val) {
        double temp = m->io_cycle_time;
        if (1 != sscanf(val, "%lf", &m->io_cycle_time)) {
            m->io_cycle_time = temp;
            gomc_log_warnf(m->env->log, m->name, "invalid [EMCIO] CYCLE_TIME (%s); using default %f",
                        val, m->io_cycle_time);
        }
    }

    val = env->ini->get(env->ini->ctx, "EMCIO", "RANDOM_TOOLCHANGER");
    if (val) {
        m->random_toolchanger = atoi(val);
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


static void load_tool(iocontrol_module *m, int toolno) {
    const tooltable_callbacks_t *tt = m->tt;

    if(m->random_toolchanger) {
        // For random toolchanger: swap tool in spindle with requested tool.
        // The spindle tool is tracked as toolno=0 in the tooltable.
        tooltable_tool_entry_t spindle = tt->get_tool(tt->ctx, 0);
        tooltable_tool_entry_t target = tt->get_tool(tt->ctx, toolno);

        if (target.toolno == 0 && toolno != 0) {
            // tool not found
            UNEXPECTED_MSG; return;
        }

        // Move spindle tool to target's pocket
        int target_pocket = target.pocketno;
        spindle.pocketno = target_pocket;
        tt->put_tool(tt->ctx, spindle.toolno, &spindle);

        // Move target to spindle (pocket 0)
        target.pocketno = 0;
        tt->put_tool(tt->ctx, target.toolno, &target);
    } else if(toolno == 0) {
        // on non-random tool-changers, asking for tool 0 is the secret
        // handshake for "unload the tool from the spindle"
        tooltable_tool_entry_t empty;
        memset(&empty, 0, sizeof(empty));
        tt->put_tool(tt->ctx, 0, &empty);
    } else {
        // just copy the desired tool's offsets to spindle (pocket 0)
        tooltable_tool_entry_t tdata = tt->get_tool(tt->ctx, toolno);
        if (tdata.toolno == 0 && toolno != 0) {
            UNEXPECTED_MSG; return;
        }
        // Store as the spindle tool (toolno 0 represents "what's in the spindle")
        tooltable_tool_entry_t spindle = tdata;
        spindle.pocketno = 0;
        tt->put_tool(tt->ctx, 0, &spindle);
    }
} // load_tool()

static void reload_tool_number(iocontrol_module *m, int toolno) {
    if(m->random_toolchanger) return;
    if(toolno > 0) {
        load_tool(m, toolno);
    }
}

/********************************************************************
* GMI emcio callbacks — called by milltask via function pointers.
********************************************************************/

static int32_t gmi_io_abort(void *ctx, int32_t reason)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    iocontrol_str *d = m->hal_data;

    gomc_log_debugf(m->env->log, m->name, "gmi_io_abort reason=%d", reason);
    m->emcioStatus.coolant.mist = 0;
    m->emcioStatus.coolant.flood = 0;
    *(d->coolant_mist) = 0;
    *(d->coolant_flood) = 0;
    *(d->tool_change) = 0;
    *(d->tool_prepare) = 0;
    return 0;
}

static int32_t gmi_set_debug(void *ctx, int32_t debug)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    gomc_log_debugf(m->env->log, m->name, "gmi_set_debug debug=%d", debug);
    m->debug = debug;
    return 0;
}

static int32_t gmi_estop_on(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    iocontrol_str *d = m->hal_data;

    gomc_log_debugf(m->env->log, m->name, "gmi_estop_on");
    *(d->user_enable_out) = 0;
    hal_init_pins(m);
    return 0;
}

static int32_t gmi_estop_off(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    iocontrol_str *d = m->hal_data;

    gomc_log_debugf(m->env->log, m->name, "gmi_estop_off");
    *(d->user_enable_out) = 1;
    *(d->user_request_enable) = 1;
    return 0;
}

static int32_t gmi_coolant_mist_on(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    m->emcioStatus.coolant.mist = 1;
    *(m->hal_data->coolant_mist) = 1;
    return 0;
}

static int32_t gmi_coolant_mist_off(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    m->emcioStatus.coolant.mist = 0;
    *(m->hal_data->coolant_mist) = 0;
    return 0;
}

static int32_t gmi_coolant_flood_on(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    m->emcioStatus.coolant.flood = 1;
    *(m->hal_data->coolant_flood) = 1;
    return 0;
}

static int32_t gmi_coolant_flood_off(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    m->emcioStatus.coolant.flood = 0;
    *(m->hal_data->coolant_flood) = 0;
    return 0;
}

static int32_t gmi_lube_on(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    m->emcioStatus.lube.on = 1;
    *(m->hal_data->lube) = 1;
    return 0;
}

static int32_t gmi_lube_off(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    m->emcioStatus.lube.on = 0;
    *(m->hal_data->lube) = 0;
    return 0;
}

static int32_t gmi_tool_prepare(void *ctx, int32_t toolno)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    iocontrol_str *d = m->hal_data;
    const tooltable_callbacks_t *tt = m->tt;

    tooltable_tool_entry_t tdata = tt->get_tool(tt->ctx, toolno);

    // toolno==0 means "unload" — always valid
    if (toolno != 0 && tdata.toolno == 0) {
        // tool not found in database
        m->emcioStatus.tool.pocketPrepped = -1;
        return -1;
    }

    gomc_log_debugf(m->env->log, m->name, "gmi_tool_prepare tool=%d pocket=%d", toolno, tdata.pocketno);

    *(d->tool_prep_index) = tdata.pocketno;  // pocket serves as index

    if (toolno == 0) {
        m->emcioStatus.tool.pocketPrepped = 0;
        *(d->tool_prep_number) = 0;
        *(d->tool_prep_pocket) = 0;
        return 0;
    }

    *(d->tool_prep_number) = tdata.toolno;
    *(d->tool_prep_pocket) = tdata.pocketno;

    // Signal HAL and wait for tool-prepared
    *(d->tool_prepare) = 1;
    while (!m->done) {
        if (*(d->tool_prepare) && *(d->tool_prepared)) {
            m->emcioStatus.tool.pocketPrepped = toolno;
            *(d->tool_prepare) = 0;
            return 0;
        }
        usleep((useconds_t)(m->io_cycle_time * 1e6));
    }
    return -1;  // shutdown
}

static int32_t gmi_tool_start_change(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    gomc_log_debugf(m->env->log, m->name, "gmi_tool_start_change");
    return 0;
}

static int32_t gmi_tool_load(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    iocontrol_str *d = m->hal_data;

    gomc_log_debugf(m->env->log, m->name, "gmi_tool_load loaded=%d prepped=%d",
                    m->emcioStatus.tool.toolInSpindle,
                    m->emcioStatus.tool.pocketPrepped);

    if (m->random_toolchanger && m->emcioStatus.tool.pocketPrepped == 0)
        return 0;

    int prepped_toolno = m->emcioStatus.tool.pocketPrepped;

    if (prepped_toolno == -1)
        return 0;

    if (!m->random_toolchanger && prepped_toolno > 0 &&
        m->emcioStatus.tool.toolInSpindle == prepped_toolno)
        return 0;

    // Signal HAL and wait for tool-changed
    *(d->tool_change) = 1;
    while (!m->done) {
        if (*(d->tool_change) && *(d->tool_changed)) {
            // Update the tool table DB BEFORE publishing toolInSpindle.
            // The stat watch goroutine uses toolInSpindle as a cache key;
            // if it observes the new value before the DB is updated, the
            // UI fetches stale data and caches it permanently (race).
            load_tool(m, prepped_toolno);
            if (!m->random_toolchanger && prepped_toolno == 0) {
                m->emcioStatus.tool.toolInSpindle = 0;
                m->emcioStatus.tool.toolFromPocket = *(d->tool_from_pocket) = 0;
            } else {
                const tooltable_callbacks_t *tt = m->tt;
                tooltable_tool_entry_t td2 = tt->get_tool(tt->ctx, prepped_toolno);
                m->emcioStatus.tool.toolInSpindle = td2.toolno;
                m->emcioStatus.tool.toolFromPocket = *(d->tool_from_pocket) = td2.pocketno;
            }
            if (m->emcioStatus.tool.toolInSpindle == 0) {
                m->emcioStatus.tool.toolFromPocket = *(d->tool_from_pocket) = 0;
            }
            *(d->tool_number) = m->emcioStatus.tool.toolInSpindle;
            m->emcioStatus.tool.pocketPrepped = -1;
            *(d->tool_prep_number) = 0;
            *(d->tool_prep_pocket) = 0;
            *(d->tool_prep_index) = 0;
            *(d->tool_change) = 0;
            return 0;
        }
        // Abort detected: gmi_io_abort cleared tool_change
        if (!*(d->tool_change)) {
            gomc_log_debugf(m->env->log, m->name, "gmi_tool_load aborted");
            m->emcioStatus.tool.pocketPrepped = -1;
            *(d->tool_prep_number) = 0;
            *(d->tool_prep_pocket) = 0;
            *(d->tool_prep_index) = 0;
            return -1;
        }
        usleep((useconds_t)(m->io_cycle_time * 1e6));
    }
    return -1;  // shutdown
}

static int32_t gmi_tool_unload(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    gomc_log_debugf(m->env->log, m->name, "gmi_tool_unload");
    m->emcioStatus.tool.toolInSpindle = 0;
    return 0;
}

static int32_t gmi_tool_load_table(void *ctx, const char *file)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    (void)file;
    // With tooltable, reloading is a no-op — the SQLite DB is always current.
    // Just refresh the spindle state.
    gomc_log_debugf(m->env->log, m->name, "gmi_tool_load_table (no-op with tooltable)");
    reload_tool_number(m, m->emcioStatus.tool.toolInSpindle);
    return 0;
}

static int32_t gmi_tool_set_offset(void *ctx,
    int32_t pocket, int32_t toolno,
    double x, double y, double z,
    double a, double b, double c,
    double u, double v, double w,
    double diameter, double frontangle, double backangle,
    int32_t orientation)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    const tooltable_callbacks_t *tt = m->tt;

    gomc_log_debugf(m->env->log, m->name,
        "gmi_tool_set_offset pocket=%d toolno=%d z=%lf x=%lf dia=%lf",
        pocket, toolno, z, x, diameter);

    tooltable_tool_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.toolno = toolno;
    entry.pocketno = pocket;
    entry.x_offset = x;
    entry.y_offset = y;
    entry.z_offset = z;
    entry.a_offset = a;
    entry.b_offset = b;
    entry.c_offset = c;
    entry.u_offset = u;
    entry.v_offset = v;
    entry.w_offset = w;
    entry.diameter = diameter;
    entry.frontangle = frontangle;
    entry.backangle = backangle;
    entry.orientation = orientation;

    tooltable_put_tool_result_t res = tt->put_tool(tt->ctx, toolno, &entry);
    if (!res.ok) {
        UNEXPECTED_MSG;
        return -1;
    }
    return 0;
}

static int32_t gmi_tool_set_number(void *ctx, int32_t toolno)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    iocontrol_str *d = m->hal_data;

    load_tool(m, toolno);

    m->emcioStatus.tool.toolInSpindle = toolno;
    gomc_log_debugf(m->env->log, m->name,
        "gmi_tool_set_number new_tool=%d", toolno);
    *(d->tool_number) = m->emcioStatus.tool.toolInSpindle;
    if (m->emcioStatus.tool.toolInSpindle == 0) {
        m->emcioStatus.tool.toolFromPocket = *(d->tool_from_pocket) = 0;
    }
    return 0;
}

static emcio_io_status_t gmi_get_status(void *ctx)
{
    iocontrol_module *m = (iocontrol_module *)ctx;
    iocontrol_str *d = m->hal_data;
    emcio_io_status_t s;
    memset(&s, 0, sizeof(s));

    // Read live HAL inputs
    s.estop = (*(d->emc_enable_in) == 0);
    s.lube_level = *(d->lube_level);

    // Copy cached state
    s.heartbeat = m->emcioStatus.heartbeat++;
    s.status = EMCIO_DONE;
    s.reason = m->emcioStatus.reason;
    s.fault = m->emcioStatus.fault;
    s.tool.pocket_prepped = m->emcioStatus.tool.pocketPrepped;
    s.tool.tool_in_spindle = m->emcioStatus.tool.toolInSpindle;
    s.tool.tool_from_pocket = m->emcioStatus.tool.toolFromPocket;
    s.coolant.mist = m->emcioStatus.coolant.mist;
    s.coolant.flood = m->emcioStatus.coolant.flood;
    s.lube_on = m->emcioStatus.lube.on;
    s.debug = m->debug;

    return s;
}

static const emcio_callbacks_t emcio_table = {
    .ctx              = NULL,  // set at registration time
    .io_abort         = gmi_io_abort,
    .set_debug        = gmi_set_debug,
    .estop_on         = gmi_estop_on,
    .estop_off        = gmi_estop_off,
    .coolant_mist_on  = gmi_coolant_mist_on,
    .coolant_mist_off = gmi_coolant_mist_off,
    .coolant_flood_on = gmi_coolant_flood_on,
    .coolant_flood_off= gmi_coolant_flood_off,
    .lube_on          = gmi_lube_on,
    .lube_off         = gmi_lube_off,
    .tool_prepare     = gmi_tool_prepare,
    .tool_start_change= gmi_tool_start_change,
    .tool_load        = gmi_tool_load,
    .tool_unload      = gmi_tool_unload,
    .tool_load_table  = gmi_tool_load_table,
    .tool_set_offset  = gmi_tool_set_offset,
    .tool_set_number  = gmi_tool_set_number,
    .get_status       = gmi_get_status,
};

/********************************************************************
* cmod lifecycle functions
********************************************************************/

static int iocontrol_start(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;

    // Look up the tooltable API (must have been loaded before us)
    m->tt = tooltable_api_get(m->env->api, m->tooltable_instance);
    if (!m->tt) {
        gomc_log_errorf(m->env->log, m->name,
            "IOCONTROL: tooltable instance '%s' not found — "
            "ensure 'load tooltable' appears before 'load iocontrol' in HAL",
            m->tooltable_instance);
        return -1;
    }

    m->done = 0;
    return 0;
}

static void iocontrol_stop(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;
    m->done = 1;
}

static void iocontrol_destroy(cmod_t *self)
{
    iocontrol_module *m = (iocontrol_module *)self->priv;

    if (m->comp_id > 0) {
        m->env->hal->exit(m->env->hal->ctx, m->comp_id);
        m->comp_id = 0;
    }

    free(m);
}

/********************************************************************
* New — cmod factory function (extern "C").
* The launcher calls dlsym(handle, "New") to find this.
********************************************************************/
int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    iocontrol_module *m = calloc(1, sizeof(*m));
    if (!m) return -1;

    m->env = env;
    strncpy(m->name, name, sizeof(m->name) - 1);

    // Defaults
    m->io_cycle_time = 0.100;
    strncpy(m->tooltable_instance, "tooltable", sizeof(m->tooltable_instance) - 1);

    // Parse arguments: tooltable_instance=<name>
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "tooltable_instance=", 19) == 0) {
            strncpy(m->tooltable_instance, argv[i] + 19, sizeof(m->tooltable_instance) - 1);
        }
    }

    // Read configuration from INI via launcher callbacks
    if (0 != iniLoad(m)) {
        free(m);
        return -1;
    }

    // Initialise HAL component and pins.
    if (iocontrol_hal_init(m) != 0) {
        free(m);
        return -1;
    }

    hal_init_pins(m);

    m->emcioStatus.aux.estop = 1;
    m->emcioStatus.tool.pocketPrepped = -1;
    m->emcioStatus.tool.toolInSpindle = 0;
    m->emcioStatus.coolant.mist = 0;
    m->emcioStatus.coolant.flood = 0;
    m->emcioStatus.lube.on = 0;
    m->emcioStatus.lube.level = 1;

    // Register GMI emcio API so milltask can call us via function pointers.
    m->emcio_cb = emcio_table;
    m->emcio_cb.ctx = m;
    emcio_api_register(m->env->api, m->name, &m->emcio_cb);

    // Wire up the cmod vtable
    m->base.Start   = iocontrol_start;
    m->base.Stop    = iocontrol_stop;
    m->base.Destroy = iocontrol_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
