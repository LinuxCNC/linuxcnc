/********************************************************************
* Description: emcsvr.cc
*   Network server for EMC NML
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
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "nml_intf/emc.hh"		// EMC NML
#include "nml_intf/emc_nml.hh"		// EMC NML
#include "nml_intf/emcglb.h"		// emcGetArgs(), EMC_NMLFILE
#include <fmt/format.h>
#include <inifile.hh>
#include "libnml/nml/nml_oi.hh"
#include "timeutil.hh"
#include "libnml/nml/nml_srv.hh"           // run_nml_servers()
#include <rtapi_string.h>

using namespace std::chrono_literals;
using namespace linuxcnc;

static int iniLoad(const char *filename)
{
    IniFile inifile(filename);

    if (!inifile) {
	return -1;
    }

    // EMC debugging flags
    emc_debug = inifile.findUIntV("DEBUG", "EMC", 0);

    if (emc_debug & EMC_DEBUG_CONFIG) {
        std::string version = inifile.findStringV("VERSION", "EMC", "<unknown>");
        std::string machine = inifile.findStringV("MACHINE", "EMC", "<unknown>");
        extern char *program_invocation_short_name;
        fmt::print(
            "{} ({}) emcsvr: machine '{}'  version '{}'\n",
            program_invocation_short_name, getpid(), machine, version
        );
    }

    if (auto inistring = inifile.findString("NML_FILE", "EMC")) {
	// copy to global
	rtapi_strxcpy(emc_nmlfile, inistring->c_str());
    } // else not found, use default

    if(emc_debug & EMC_DEBUG_CONFIG)
        fmt::print("config file \"{}\" loaded successfully.\n",
                   filename ? filename : "(null)");

    return 0;
}

// based on code from
// http://www.microhowto.info/howto/cause_a_process_to_become_a_daemon_in_c.html
static void daemonize()
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("daemonize: fork()");
    } else if (pid) {
        _exit(0);
    }

    if(setsid() < 0)
        perror("daemonize: setsid()");

    // otherwise the parent may deliver a SIGHUP to this process when it
    // terminates
    signal(SIGHUP,SIG_IGN);

    pid=fork();
    if (pid < 0) {
        perror("daemonize: fork() 2");
    } else if (pid) {
        _exit(0);
    }
}

static RCS_CMD_CHANNEL *emcCommandChannel = NULL;
static RCS_STAT_CHANNEL *emcStatusChannel = NULL;
static NML *emcErrorChannel = NULL;

int main(int argc, char *argv[])
{
    double start_time;

    // process command line args
    if (0 != emcGetArgs(argc, argv)) {
	fmt::print(stderr, "Error in argument list\n");
	exit(1);
    }
    // get configuration information
    iniLoad(emc_inifile);

    start_time = etime();

    while (fabs(etime() - start_time) < 10.0 &&
	   (emcCommandChannel == NULL || emcStatusChannel == NULL
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
	esleep(200ms);
    }


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
    daemonize();
    run_nml_servers();

    return 0;
}

// vim:sw=4:sts=4:et
