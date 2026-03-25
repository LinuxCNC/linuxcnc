/********************************************************************
 * Description: emcsvr_lib.cc
 *   NML server as a cmod plugin for the LinuxCNC launcher.
 *   Loaded via dlopen, lifecycle managed through cmod_t callbacks.
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
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <atomic>

#include "rcs.hh"
#include "emc.hh"
#include "emc_nml.hh"
#include "emcglb.h"
#include "rcs_print.hh"
#include "nml_oi.hh"
#include "timer.hh"
#include "nml_srv.hh"
#include "nml.hh"
#include <rtapi_string.h>

#include "launcher/pkg/cmodule/cmodule.h"

// nml_control_C_caught is a non-static global in nml_srv.cc.
extern int nml_control_C_caught;

// Polling interval for the NML server shutdown loop (seconds).
static const double EMCSVR_POLL_INTERVAL = 0.2;

// emcsvr_module holds all per-instance state.
struct emcsvr_module {
    cmod_t base;                        // must be first — contains lifecycle vtable
    const cmod_env_t *env;              // launcher-provided environment
    char name[64];                      // instance name

    // NML channels
    RCS_CMD_CHANNEL  *emcCommandChannel;
    RCS_STAT_CHANNEL *emcStatusChannel;
    NML              *emcErrorChannel;
    RCS_CMD_CHANNEL  *toolCommandChannel;
    RCS_STAT_CHANNEL *toolStatusChannel;

    // Configuration
    int tool_channels;

    // Main loop thread
    pthread_t loop_thread;
    std::atomic<int> done;
    bool thread_started;
};

// iniLoad reads configuration from the launcher's parsed INI via env callbacks.
static int iniLoad(emcsvr_module *m)
{
    const cmod_env_t *env = m->env;
    const char *val;

    val = env->get_ini(env->ctx, "EMC", "DEBUG");
    if (val) {
        if (1 != sscanf(val, "%x", &emc_debug)) {
            emc_debug = 0;
        }
    } else {
        emc_debug = 0;
    }
    if (emc_debug & EMC_DEBUG_RCS) {
        set_rcs_print_flag(PRINT_EVERYTHING);
        max_rcs_errors_to_print = -1;
    }

    val = env->get_ini(env->ctx, "EMC", "NML_FILE");
    if (val) {
        rtapi_strxcpy(emc_nmlfile, val);
    }

    val = env->get_ini(env->ctx, "EMC", "TOOL_CHANNELS");
    if (val) {
        m->tool_channels = atoi(val);
    }

    return 0;
}

// initChannels creates NML channels, retrying for up to 10 seconds.
static int initChannels(emcsvr_module *m)
{
    double start_time;

    // Store INI source file path for NML config resolution.
    const char *ini_path = m->env->ini_source_file(m->env->ctx);
    if (ini_path) {
        rtapi_strxcpy(emc_inifile, ini_path);
    }

    set_rcs_print_destination(RCS_PRINT_TO_NULL);

    start_time = etime();

    while (fabs(etime() - start_time) < 10.0 &&
           (m->emcCommandChannel == NULL || m->emcStatusChannel == NULL
            || (m->tool_channels && (m->toolCommandChannel == NULL || m->toolStatusChannel == NULL))
            || m->emcErrorChannel == NULL)
          ) {
        if (NULL == m->emcCommandChannel) {
            m->emcCommandChannel =
                new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr",
                                    emc_nmlfile);
        }
        if (NULL == m->emcStatusChannel) {
            m->emcStatusChannel =
                new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr",
                                     emc_nmlfile);
        }
        if (NULL == m->emcErrorChannel) {
            m->emcErrorChannel =
                new NML(nmlErrorFormat, "emcError", "emcsvr", emc_nmlfile);
        }
        if (m->tool_channels) {
            if (NULL == m->toolCommandChannel) {
                m->toolCommandChannel =
                    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr",
                                        emc_nmlfile);
            }
            if (NULL == m->toolStatusChannel) {
                m->toolStatusChannel =
                    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr",
                                         emc_nmlfile);
            }
        }

        if (!m->emcCommandChannel->valid()) {
            delete m->emcCommandChannel;
            m->emcCommandChannel = NULL;
        }
        if (!m->emcStatusChannel->valid()) {
            delete m->emcStatusChannel;
            m->emcStatusChannel = NULL;
        }
        if (!m->emcErrorChannel->valid()) {
            delete m->emcErrorChannel;
            m->emcErrorChannel = NULL;
        }
        if (m->tool_channels) {
            if (!m->toolCommandChannel->valid()) {
                delete m->toolCommandChannel;
                m->toolCommandChannel = NULL;
            }
            if (!m->toolStatusChannel->valid()) {
                delete m->toolStatusChannel;
                m->toolStatusChannel = NULL;
            }
        }
        esleep(0.200);
    }

    set_rcs_print_destination(RCS_PRINT_TO_STDERR);

    if (NULL == m->emcCommandChannel) {
        m->emcCommandChannel =
            new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr",
                                emc_nmlfile);
    }
    if (NULL == m->emcStatusChannel) {
        m->emcStatusChannel =
            new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr",
                                 emc_nmlfile);
    }
    if (NULL == m->emcErrorChannel) {
        m->emcErrorChannel =
            new NML(nmlErrorFormat, "emcError", "emcsvr", emc_nmlfile);
    }
    if (m->tool_channels) {
        if (NULL == m->toolCommandChannel) {
            m->toolCommandChannel =
                new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr",
                                    emc_nmlfile);
        }
        if (NULL == m->toolStatusChannel) {
            m->toolStatusChannel =
                new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr",
                                     emc_nmlfile);
        }
    }

    return 0;
}

// emcsvr_loop is the main server loop, run in a dedicated thread.
static void *emcsvr_loop(void *arg)
{
    emcsvr_module *m = (emcsvr_module *)arg;

    if (NULL == NML_Default_Super_Server) {
        rcs_print_error("emcsvr: NML_Default_Super_Server is NULL\n");
        return NULL;
    }

    // Always use the multi-server spawn path — never the single-server
    // sole_server->run() path which blocks and installs signal handlers.
    NML_Default_Super_Server->spawn_all_servers();

    // Do NOT install any signal handlers — the Go runtime owns signals.
    nml_control_C_caught = 0;

    // Poll until Stop() sets the done flag.
    while (!m->done) {
        esleep(EMCSVR_POLL_INTERVAL);
    }

    // Only kill spawned server threads — do NOT call nml_cleanup() here.
    // Channel deletion happens in Destroy() which is called separately.
    if (NML_Default_Super_Server != NULL) {
        NML_Default_Super_Server->kill_all_servers();
    }

    return NULL;
}

/********************************************************************
* cmod lifecycle functions
********************************************************************/

static int emcsvr_start(cmod_t *self)
{
    emcsvr_module *m = (emcsvr_module *)self->priv;

    m->done = 0;
    m->thread_started = true;
    if (pthread_create(&m->loop_thread, NULL, emcsvr_loop, m) != 0) {
        rcs_print_error("emcsvr: pthread_create failed\n");
        m->thread_started = false;
        return -1;
    }

    return 0;
}

static void emcsvr_stop(cmod_t *self)
{
    emcsvr_module *m = (emcsvr_module *)self->priv;

    m->done = 1;
    if (m->thread_started) {
        pthread_join(m->loop_thread, NULL);
        m->thread_started = false;
    }
}

static void emcsvr_destroy(cmod_t *self)
{
    emcsvr_module *m = (emcsvr_module *)self->priv;

    if (m->emcCommandChannel != NULL) {
        delete m->emcCommandChannel;
        m->emcCommandChannel = NULL;
    }
    if (m->emcStatusChannel != NULL) {
        delete m->emcStatusChannel;
        m->emcStatusChannel = NULL;
    }
    if (m->emcErrorChannel != NULL) {
        delete m->emcErrorChannel;
        m->emcErrorChannel = NULL;
    }
    if (m->toolCommandChannel != NULL) {
        delete m->toolCommandChannel;
        m->toolCommandChannel = NULL;
    }
    if (m->toolStatusChannel != NULL) {
        delete m->toolStatusChannel;
        m->toolStatusChannel = NULL;
    }
    nml_server_cleanup();

    delete m;
}

/********************************************************************
* New — cmod factory function (extern "C").
* The launcher calls dlsym(handle, "New") to find this.
********************************************************************/
extern "C" int New(const cmod_env_t *env, const char *name,
                   int argc, const char **argv, cmod_t **out)
{
    emcsvr_module *m = new emcsvr_module();

    m->env = env;
    strncpy(m->name, name, sizeof(m->name) - 1);

    // Defaults
    m->tool_channels = 1;
    m->emcCommandChannel = NULL;
    m->emcStatusChannel = NULL;
    m->emcErrorChannel = NULL;
    m->toolCommandChannel = NULL;
    m->toolStatusChannel = NULL;
    m->thread_started = false;
    m->done = 0;

    // Read configuration from INI via launcher callbacks
    if (0 != iniLoad(m)) {
        delete m;
        return -1;
    }

    // Initialize NML channels (creates shared memory segments)
    if (0 != initChannels(m)) {
        delete m;
        return -1;
    }

    // Wire up lifecycle vtable
    m->base.Start = emcsvr_start;
    m->base.Stop = emcsvr_stop;
    m->base.Destroy = emcsvr_destroy;
    m->base.priv = m;

    *out = &m->base;
    return 0;
}

// vim:sw=4:sts=4:et
