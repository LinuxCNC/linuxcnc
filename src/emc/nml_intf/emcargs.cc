/********************************************************************
* Description: emcargs.cc
*   Globals initialized to values in emccfg.h
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

#include <string.h>		/* strcpy() */
#include <stdio.h>		/* fgets() */
#include "nml.hh"               /* nmlSetHostAlias */
#include "emcglb.h"		/* these decls */
#include "emccfg.h"		/* their initial values */
#include "rcs_print.hh"

int emcGetArgs(int argc, char *argv[])
{
    int t;

    /* process command line args, indexing argv[] from [1] */
    for (t = 1; t < argc; t++) {
	if (!strcmp(argv[t], "-ini")) {
	    if (t == argc - 1) {
		return -1;
	    } else {
                if (strlen(argv[t+1]) >= LINELEN) {
                    fprintf(stderr, "ini file name too long (max %d):\n", LINELEN);
                    fprintf(stderr, "    %s\n", argv[t+1]);
                    return -1;
                }
		strcpy(emc_inifile, argv[t + 1]);
		t++;
	    }
	    continue;
	}
	if (!strcmp(argv[t], "-rcsdebug")) {
	    set_rcs_print_flag(PRINT_EVERYTHING);
	    max_rcs_errors_to_print = -1;
	    continue;
	}

	if (!strcmp(argv[t], "-queryhost")) {
	    char qhost[80];
	    printf("EMC Host?");
	    if(!fgets(qhost, 80, stdin)) return -1;
	    for (int i = 0; i < 80; i++) {
		if (qhost[i] == '\r' || qhost[i] == '\n'
		    || qhost[i] == ' ') {
		    qhost[i] = 0;
		    break;
		}
	    }
	    nmlSetHostAlias(qhost, "localhost");	/* If localhost
							   appears in .nml
							   file it will
							   overriden by this
							   argument. */
	    nmlForceRemoteConnection();
	    /* The only good reason for aliasing the host that I know of is
	       to connect to a remote server so we will ignore the
	       LOCAL/REMOTE field in the .nml file and always connect
	       remotely. */
	    continue;
	}
	if (!strcmp(argv[t], "-host")) {
	    if (t == argc - 1) {
		return -1;
	    } else {
		nmlSetHostAlias(argv[t + 1], "localhost");	/* If
								   localhost
								   appears in 
								   .nml file
								   it will
								   overriden
								   by this
								   argument. */
		nmlForceRemoteConnection();
		/* The only good reason for aliasing the host that I know of
		   is to connect to a remote server so we will ignore the
		   LOCAL/REMOTE field in the .nml file and always connect
		   remotely. */
		t++;
	    }
	    continue;
	}
    }
    /* else not recognized-- ignore */

    return 0;
}
