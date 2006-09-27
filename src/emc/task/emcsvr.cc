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
* $Revision$
* $Author$
* $Date$
********************************************************************/

#include <stdio.h>		// sscanf()
#include <math.h>		// fabs()
#include <stdlib.h>		// exit()
#include <string.h>		// strncpy()

#include <signal.h>

#include "rcs.hh"		// EMC NML
#include "emc.hh"		// EMC NML
#include "emcglb.h"		// emcGetArgs(), EMC_NMLFILE

static int iniLoad(const char *filename)
{
    Inifile inifile;
    const char *inistring;

    // open it
    if (inifile.open(filename) == false) {
	return -1;
    }

    if (NULL != (inistring = inifile.find("DEBUG", "EMC"))) {
	// copy to global
	if (1 != sscanf(inistring, "%x", &EMC_DEBUG)) {
	    EMC_DEBUG = 0;
	}
    } else {
	// not found, use default
	EMC_DEBUG = 0;
    }
    if (EMC_DEBUG & EMC_DEBUG_RCS) {
	set_rcs_print_flag(PRINT_EVERYTHING);
	max_rcs_errors_to_print = -1;
    }

    if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
	// copy to global
	strcpy(EMC_NMLFILE, inistring);
    } else {
	// not found, use default
    }

    // close it
    inifile.close();

    return 0;
}

static RCS_CMD_CHANNEL *emcCommandChannel = NULL;
static RCS_STAT_CHANNEL *emcStatusChannel = NULL;
static NML *emcErrorChannel = NULL;
static RCS_CMD_CHANNEL *toolCommandChannel = NULL;
static RCS_STAT_CHANNEL *toolStatusChannel = NULL;

void nice_exit(int ignore) {
    _exit(0);
}

int main(int argc, char *argv[])
{
    double start_time;

    // process command line args
    if (0 != emcGetArgs(argc, argv)) {
	rcs_print_error("Error in argument list\n");
	exit(1);
    }
    // get configuration information
    iniLoad(EMC_INIFILE);

    set_rcs_print_destination(RCS_PRINT_TO_NULL);

    rcs_print("after iniLoad()\n");


    start_time = etime();

    while (fabs(etime() - start_time) < 10.0 &&
	   (emcCommandChannel == NULL || emcStatusChannel == NULL
	    || toolCommandChannel == NULL || toolStatusChannel == NULL
	    || emcErrorChannel == NULL)
	) {
	if (NULL == emcCommandChannel) {
	    rcs_print("emcCommandChannel==NULL, attempt to create\n");
	    emcCommandChannel =
		new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr",
				    EMC_NMLFILE);
	}
	if (NULL == emcStatusChannel) {
	    rcs_print("emcStatusChannel==NULL, attempt to create\n");
	    emcStatusChannel =
		new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr",
				     EMC_NMLFILE);
	}
	if (NULL == emcErrorChannel) {
	    emcErrorChannel =
		new NML(nmlErrorFormat, "emcError", "emcsvr", EMC_NMLFILE);
	}
	if (NULL == toolCommandChannel) {
	    toolCommandChannel =
		new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr",
				    EMC_NMLFILE);
	}
	if (NULL == toolStatusChannel) {
	    toolStatusChannel =
		new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr",
				     EMC_NMLFILE);
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
	if (!toolCommandChannel->valid()) {
	    delete toolCommandChannel;
	    toolCommandChannel = NULL;
	}
	if (!toolStatusChannel->valid()) {
	    delete toolStatusChannel;
	    toolStatusChannel = NULL;
	}

	esleep(0.200);
    }

    set_rcs_print_destination(RCS_PRINT_TO_STDERR);

    if (NULL == emcCommandChannel) {
	emcCommandChannel =
	    new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr",
				EMC_NMLFILE);
    }
    if (NULL == emcStatusChannel) {
	emcStatusChannel =
	    new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr",
				 EMC_NMLFILE);
    }
    if (NULL == emcErrorChannel) {
	emcErrorChannel =
	    new NML(nmlErrorFormat, "emcError", "emcsvr", EMC_NMLFILE);
    }
    if (NULL == toolCommandChannel) {
	toolCommandChannel =
	    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr",
				EMC_NMLFILE);
    }
    if (NULL == toolStatusChannel) {
	toolStatusChannel =
	    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr",
				 EMC_NMLFILE);
    }

    signal(SIGTERM, nice_exit);

    run_nml_servers();

    return 0;
}
