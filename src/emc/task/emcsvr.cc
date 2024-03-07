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

#include <stdio.h>		// sscanf()
#include <math.h>		// fabs()
#include <stdlib.h>		// exit()
#include <string.h>		// strncpy()
#include <unistd.h>             // _exit()
#include <signal.h>

#include "rcs.hh"		// EMC NML
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"		// EMC NML
#include "emcglb.h"		// emcGetArgs(), EMC_NMLFILE
#include "inifile.hh"
#include "rcs_print.hh"
#include "nml_oi.hh"
#include "timer.hh"
#include "nml_srv.hh"           // run_nml_servers()
#include <rtapi_string.h>

static int iniLoad(const char *filename)
{
    IniFile inifile;
    std::optional<const char*> inistring;
    char version[LINELEN], machine[LINELEN];

    // open it
    if (inifile.Open(filename) == false) {
	return -1;
    }

    // EMC debugging flags
    emc_debug = 0;  // disabled by default
    if ((inistring = inifile.Find("DEBUG", "EMC"))) {
        // parse to global
        if (sscanf(*inistring, "%x", &emc_debug) < 1) {
            perror("failed to parse [EMC] DEBUG");
        }
    }

    // set output for RCS messages
    set_rcs_print_destination(RCS_PRINT_TO_STDOUT);   // use stdout by default
    if ((inistring = inifile.Find("RCS_DEBUG_DEST", "EMC"))) {
        static RCS_PRINT_DESTINATION_TYPE type;
        if (!strcmp(*inistring, "STDOUT")) {
            type = RCS_PRINT_TO_STDOUT;
        } else if (!strcmp(*inistring, "STDERR")) {
            type = RCS_PRINT_TO_STDERR;
        } else if (!strcmp(*inistring, "FILE")) {
            type = RCS_PRINT_TO_FILE;
        } else if (!strcmp(*inistring, "LOGGER")) {
            type = RCS_PRINT_TO_LOGGER;
        } else if (!strcmp(*inistring, "MSGBOX")) {
            type = RCS_PRINT_TO_MESSAGE_BOX;
        } else if (!strcmp(*inistring, "NULL")) {
            type = RCS_PRINT_TO_NULL;
        } else {
             type = RCS_PRINT_TO_STDOUT;
        }
        set_rcs_print_destination(type);
    }

    // NML/RCS debugging flags
    set_rcs_print_flag(PRINT_RCS_ERRORS);  // only print errors by default
    // enable all debug messages by default if RCS or NML debugging is enabled
    if ((emc_debug & EMC_DEBUG_RCS) || (emc_debug & EMC_DEBUG_NML)) {
        // output all RCS debug messages
        set_rcs_print_flag(PRINT_EVERYTHING);
    }

    // set flags if RCS_DEBUG in ini file
    if ((inistring = inifile.Find("RCS_DEBUG", "EMC"))) {
        static long int flags;
        if (sscanf(*inistring, "%lx", &flags) < 1) {
            perror("failed to parse [EMC] RCS_DEBUG");
        }
        // clear all flags
        clear_rcs_print_flag(PRINT_EVERYTHING);
        // set parsed flags
        set_rcs_print_flag(flags);
    }
    // output infinite RCS errors by default
    max_rcs_errors_to_print = -1;
    if ((inistring = inifile.Find("RCS_MAX_ERR", "EMC"))) {
        if (sscanf(*inistring, "%d", &max_rcs_errors_to_print) < 1) {
            perror("failed to parse [EMC] RCS_MAX_ERR");
        }
    }

    inistring = inifile.Find("VERSION", "EMC");
    strncpy(version, inistring.value_or("unknown"), LINELEN-1);

    inistring = inifile.Find("MACHINE", "EMC");
    strncpy(machine, inistring.value_or("unknown"), LINELEN-1);

    extern char *program_invocation_short_name;
    rcs_print(
        "%s (%d) emcsvr: machine '%s'  version '%s'\n",
        program_invocation_short_name, getpid(), machine, version
    );

    if ((inistring = inifile.Find("NML_FILE", "EMC"))) {
	// copy to global
	rtapi_strxcpy(emc_nmlfile, *inistring);
    } else {
	// not found, use default
    }
    // close it
    inifile.Close();

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
