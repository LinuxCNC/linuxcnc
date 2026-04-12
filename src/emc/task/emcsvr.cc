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

#include <signal.h>

#include "libnml/rcs/rcs.hh"		// EMC NML
#include "nml_intf/emc.hh"		// EMC NML
#include "nml_intf/emc_nml.hh"		// EMC NML
#include "nml_intf/emcglb.h"		// emcGetArgs(), EMC_NMLFILE
#include <inifile.hh>
#include "libnml/rcs/rcs_print.hh"
#include "libnml/nml/nml_oi.hh"
#include "libnml/os_intf/timer.hh"
#include "libnml/nml/nml_srv.hh"           // run_nml_servers()
#include <rtapi_string.h>
#include "usr_intf/mapini.hh"

using namespace linuxcnc;

static int iniLoad(const char *filename)
{
    IniFile inifile(filename);

    if (!inifile) {
	return -1;
    }

    // EMC debugging flags
    emc_debug = inifile.findUIntV("DEBUG", "EMC", 0);

    // set output for RCS messages
    if (auto inival = mapRcsDestination(inifile, "RCS_DEBUG_DEST", "EMC")) {
        set_rcs_print_destination(*inival);
    } else {
        set_rcs_print_destination(RCS_PRINT_TO_STDOUT);
    }

    // NML/RCS debugging flags
    set_rcs_print_flag(PRINT_RCS_ERRORS);  // only print errors by default
    // enable all debug messages by default if RCS or NML debugging is enabled
    if ((emc_debug & EMC_DEBUG_RCS) || (emc_debug & EMC_DEBUG_NML)) {
        // output all RCS debug messages
        set_rcs_print_flag(PRINT_EVERYTHING);
    }

    // set flags if RCS_DEBUG in ini file
    if (auto inival = inifile.findUInt("RCS_DEBUG", "EMC")) {
        // clear all flags
        clear_rcs_print_flag(PRINT_EVERYTHING);
        // set parsed flags
        set_rcs_print_flag((long)*inival);
    }
    // output infinite RCS errors by default
    max_rcs_errors_to_print = inifile.findIntV("RCS_MAX_ERR", "EMC", -1);

    if (emc_debug & EMC_DEBUG_CONFIG) {
        std::string version = inifile.findStringV("VERSION", "EMC", "<unknown>");
        std::string machine = inifile.findStringV("MACHINE", "EMC", "<unknown>");
        extern char *program_invocation_short_name;
        rcs_print(
            "%s (%d) emcsvr: machine '%s'  version '%s'\n",
            program_invocation_short_name, getpid(), machine.c_str(), version.c_str()
        );
    }

    if (auto inistring = inifile.findString("NML_FILE", "EMC")) {
	// copy to global
	rtapi_strxcpy(emc_nmlfile, inistring->c_str());
    } // else not found, use default

    if(emc_debug & EMC_DEBUG_CONFIG)
        rcs_print("config file \"%s\" loaded successfully.\n", filename);

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
	rcs_print_error("Error in argument list\n");
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
	    rcs_print_debug(PRINT_NML_CONSTRUCTORS, "emcCommandChannel==NULL, attempt to create\n");
	    emcCommandChannel =
		new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr",
				    emc_nmlfile);
	}
	if (NULL == emcStatusChannel) {
	    rcs_print_debug(PRINT_NML_CONSTRUCTORS, "emcStatusChannel==NULL, attempt to create\n");
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
	esleep(0.200);
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
