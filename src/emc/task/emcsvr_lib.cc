/********************************************************************
 * Description: emcsvr_lib.cc
 *   Library entry points for embedding the NML server in-process.
 *   Called from Go via cgo (no main(), no fork(), no signal handlers).
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

#include "rcs.hh"
#include "emc.hh"
#include "emc_nml.hh"
#include "emcglb.h"
#include "inifile.hh"
#include "rcs_print.hh"
#include "nml_oi.hh"
#include "timer.hh"
#include "nml_srv.hh"
#include "nml.hh"
#include <rtapi_string.h>

// nml_control_C_caught is a non-static global in nml_srv.cc (line 479).
extern int nml_control_C_caught;

static int tool_channels = 1;

// Polling interval for the NML server shutdown loop (seconds).
static const double EMCSVR_POLL_INTERVAL = 0.2;

static RCS_CMD_CHANNEL  *emcCommandChannel  = NULL;
static RCS_STAT_CHANNEL *emcStatusChannel   = NULL;
static NML              *emcErrorChannel    = NULL;
static RCS_CMD_CHANNEL  *toolCommandChannel = NULL;
static RCS_STAT_CHANNEL *toolStatusChannel  = NULL;

static int iniLoad(const char *filename)
{
    IniFile inifile;
    const char *inistring;

    if (inifile.Open(filename) == false) {
        return -1;
    }

    if (NULL != (inistring = inifile.Find("DEBUG", "EMC"))) {
        if (1 != sscanf(inistring, "%x", &emc_debug)) {
            emc_debug = 0;
        }
    } else {
        emc_debug = 0;
    }
    if (emc_debug & EMC_DEBUG_RCS) {
        set_rcs_print_flag(PRINT_EVERYTHING);
        max_rcs_errors_to_print = -1;
    }

    if (NULL != (inistring = inifile.Find("NML_FILE", "EMC"))) {
        rtapi_strxcpy(emc_nmlfile, inistring);
    }
    inifile.Find(&tool_channels, "TOOL_CHANNELS", "EMC");
    inifile.Close();

    return 0;
}

extern "C" {

int emcsvr_init(const char *ini_file)
{
    double start_time;

    rtapi_strxcpy(emc_inifile, ini_file);

    if (iniLoad(emc_inifile) != 0) {
        rcs_print_error("emcsvr_init: iniLoad(%s) failed\n", emc_inifile);
        return -1;
    }

    set_rcs_print_destination(RCS_PRINT_TO_NULL);

    start_time = etime();

    while (fabs(etime() - start_time) < 10.0 &&
           (emcCommandChannel == NULL || emcStatusChannel == NULL
            || (tool_channels && (toolCommandChannel == NULL || toolStatusChannel == NULL))
            || emcErrorChannel == NULL)
          ) {
        if (NULL == emcCommandChannel) {
            emcCommandChannel =
                new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr",
                                    emc_nmlfile);
        }
        if (NULL == emcStatusChannel) {
            emcStatusChannel =
                new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr",
                                     emc_nmlfile);
        }
        if (NULL == emcErrorChannel) {
            emcErrorChannel =
                new NML(nmlErrorFormat, "emcError", "emcsvr", emc_nmlfile);
        }
        if (tool_channels) {
            if (NULL == toolCommandChannel) {
                toolCommandChannel =
                    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr",
                                        emc_nmlfile);
            }
            if (NULL == toolStatusChannel) {
                toolStatusChannel =
                    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr",
                                         emc_nmlfile);
            }
        }

        if (!emcCommandChannel->valid()) {
            delete emcCommandChannel;
            emcCommandChannel = NULL;
        }
        if (!emcStatusChannel->valid()) {
            delete emcStatusChannel;
            emcStatusChannel = NULL;
        }
        if (!emcErrorChannel->valid()) {
            delete emcErrorChannel;
            emcErrorChannel = NULL;
        }
        if (tool_channels) {
            if (!toolCommandChannel->valid()) {
                delete toolCommandChannel;
                toolCommandChannel = NULL;
            }
            if (!toolStatusChannel->valid()) {
                delete toolStatusChannel;
                toolStatusChannel = NULL;
            }
        }
        esleep(0.200);
    }

    set_rcs_print_destination(RCS_PRINT_TO_STDERR);

    if (NULL == emcCommandChannel) {
        emcCommandChannel =
            new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr",
                                emc_nmlfile);
    }
    if (NULL == emcStatusChannel) {
        emcStatusChannel =
            new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr",
                                 emc_nmlfile);
    }
    if (NULL == emcErrorChannel) {
        emcErrorChannel =
            new NML(nmlErrorFormat, "emcError", "emcsvr", emc_nmlfile);
    }
    if (tool_channels) {
        if (NULL == toolCommandChannel) {
            toolCommandChannel =
                new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr",
                                    emc_nmlfile);
        }
        if (NULL == toolStatusChannel) {
            toolStatusChannel =
                new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr",
                                     emc_nmlfile);
        }
    }

    return 0;
}

int emcsvr_run(void)
{
    if (NULL == NML_Default_Super_Server) {
        rcs_print_error("emcsvr_run: NML_Default_Super_Server is NULL\n");
        return -1;
    }

    // Always use the multi-server spawn path — never the single-server
    // sole_server->run() path which blocks and installs signal handlers.
    NML_Default_Super_Server->spawn_all_servers();

    // Do NOT install any signal handlers — the Go runtime owns signals.
    nml_control_C_caught = 0;

    // Poll until emcsvr_stop() sets the flag.
    while (!nml_control_C_caught) {
        esleep(EMCSVR_POLL_INTERVAL);
    }

    // Only kill spawned server threads — do NOT call nml_cleanup() here.
    // Channel deletion happens in emcsvr_cleanup() which is called separately.
    if (NML_Default_Super_Server != NULL) {
        NML_Default_Super_Server->kill_all_servers();
    }

    return 0;
}

void emcsvr_stop(void)
{
    nml_control_C_caught = 1;
}

void emcsvr_cleanup(void)
{
    if (emcCommandChannel != NULL) {
        delete emcCommandChannel;
        emcCommandChannel = NULL;
    }
    if (emcStatusChannel != NULL) {
        delete emcStatusChannel;
        emcStatusChannel = NULL;
    }
    if (emcErrorChannel != NULL) {
        delete emcErrorChannel;
        emcErrorChannel = NULL;
    }
    if (toolCommandChannel != NULL) {
        delete toolCommandChannel;
        toolCommandChannel = NULL;
    }
    if (toolStatusChannel != NULL) {
        delete toolStatusChannel;
        toolStatusChannel = NULL;
    }
    nml_server_cleanup();
}

} // extern "C"

// vim:sw=4:sts=4:et
