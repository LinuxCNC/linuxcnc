/********************************************************************
* Description: inivar.c
*   prints to stdout the INI file result of a variable-in-section
*   search, useful for scripts that want to pick things out of INI files.
*
*   syntax:  inivar -var <variable> {-sec <section>} {<-ini inifile>}
*
*   Uses emc.ini as default. <variable> needs to be supplied. If <section>
*   is omitted, first instance of <variable> will be looked for in any
*   section. Otherwise only a match of the variable in <section> will
*   be returned.
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

#include <stdio.h>		/* printf(), fprintf(), FILE, fopen(),
				   fclose() */
#include <stdlib.h>		/* exit() */
#include <string.h>		/* strcmp(), strcpy() */
#include "inifile.h"		/* iniFind() */

int main(int argc, char *argv[])
{
    int t;
    char _variable[LINELEN] = "";
    char *variable = 0;
    char _section[LINELEN] = "";
    char *section = 0;
    char INIFILE[256] = "generic.ini";
    FILE *fp;
    const char *inistring;

    /* process command line args, indexing argv[] from [1] */
    for (t = 1; t < argc; t++) {
	if (!strcmp(argv[t], "-ini")) {
	    if (t == argc - 1) {
		/* no arg following -ini, so abort */
		fprintf(stderr,
		    "%s: ini file not specified after -ini\n", argv[0]);
		exit(1);
	    } else {
		strcpy(INIFILE, argv[t + 1]);
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-var")) {
	    if (t == argc - 1) {
		/* no arg following -var, so abort */
		fprintf(stderr,
		    "%s: variable name not specified after -var\n", argv[0]);
		exit(1);
	    } else {
		strcpy(_variable, argv[t + 1]);
		variable = _variable;
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-sec")) {
	    if (t == argc - 1) {
		/* no arg following -sec, so abort */
		fprintf(stderr,
		    "%s: section name not specified after -sec\n", argv[0]);
		exit(1);
	    } else {
		strcpy(_section, argv[t + 1]);
		section = _section;
		t++;		/* step over following arg */
	    }
	} else {
	    /* invalid argument */
	    fprintf(stderr,
		"%s: -var <variable> {-sec <section>} {<-ini inifile>}\n",
		argv[0]);
	    exit(1);
	}
    }

    /* check that variable was supplied */
    if (0 == variable) {
	fprintf(stderr, "%s: no variable supplied\n", argv[0]);
	exit(1);
    }

    /* open the inifile */
    if (NULL == (fp = fopen(INIFILE, "r"))) {
	fprintf(stderr, "%s: can't open %s\n", argv[0], INIFILE);
	exit(1);
    }

    inistring = iniFind(fp, variable, section);
    if (NULL != inistring) {
	printf("%s\n", inistring);
    }

    fclose(fp);

    exit(0);
}
