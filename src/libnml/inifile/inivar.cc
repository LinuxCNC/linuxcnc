/********************************************************************
* Description: inivar.cc
*   prints to stdout the INI file result of a variable-in-section
*   search, useful for scripts that want to pick things out of INI files.
*
*   syntax:  inivar -var <variable> {-sec <section>} {-ini <INI file>}
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
********************************************************************/

#include <cstdio>		/* printf(), fprintf(), FILE, fopen(),*/
#include <cstdlib>		/* exit() */
#include <cstring>		/* strcmp(), strcpy() */
#include <climits>

#include "inifile.hh"


int main(int argc, char *argv[])
{
    int num = 1;
    const char *variable = nullptr;
    const char *section = nullptr;
    const char *path = "emc.ini";
    int retval;
    bool tildeExpand = false;

    /* process command line args, indexing argv[] from [1] */
    for (int t = 1; t < argc; t++) {
	if (!strcmp(argv[t], "-ini")) {
	    if (t == argc - 1) {
		/* no arg following -ini, so abort */
		fprintf(stderr,
		    "%s: INI file not specified after -ini\n", argv[0]);
		exit(-1);
	    } else {
		path = argv[t+1];
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-var")) {
	    if (t == argc - 1) {
		/* no arg following -var, so abort */
		fprintf(stderr,
		    "%s: variable name not specified after -var\n", argv[0]);
		exit(-1);
	    } else {
		variable = argv[t+1];
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-sec")) {
	    if (t == argc - 1) {
		/* no arg following -sec, so abort */
		fprintf(stderr,
		    "%s: section name not specified after -sec\n", argv[0]);
		exit(-1);
	    } else {
		section = argv[t+1];
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-num")) {
	    if (t == argc - 1) {
		/* no arg following -num, so abort */
		fprintf(stderr,
		    "%s: occurrence number not specified after -num\n", argv[0]);
		exit(-1);
	    } else {
		char *endPtr;
		errno = 0;
		long result = strtol(argv[t + 1], &endPtr, 10);
		if (errno || *endPtr != '\0' || result < 0 || result > INT_MAX) {
		    fprintf(stderr,
			"%s: invalid number after -num\n", argv[0]);
		    exit(-1);
		}
		num = static_cast<int>(result);
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-tildeexpand")) {
	    tildeExpand = !tildeExpand;
	} else {
	    /* invalid argument */
	    fprintf(stderr,
		"%s: -var <variable> [-tildeexpand] [-sec <section>] [-num <occurrence_number>] [-ini <INI file>]\n",
		argv[0]);
	    exit(-1);
	}
    }

    /* check that variable was supplied */
    if (!variable) {
	fprintf(stderr, "%s: no variable supplied\n", argv[0]);
	exit(-1);
    }

    IniFile iniFile;
    /* open the INI file */
    iniFile.Open(path);
    if (!iniFile.IsOpen()) {
	fprintf(stderr, "%s: can't open %s\n", argv[0], path);
	exit(-1);
    }

    auto iniString = iniFile.Find(variable, section, num);
    if (iniString) {
	if (tildeExpand) {
	    char expanded[PATH_MAX];
	    iniFile.TildeExpansion(*iniString, expanded, sizeof(expanded));
	    printf("%s\n", expanded);
	} else {
	    printf("%s\n", *iniString);
	}
	retval = 0;
    } else {
	fprintf(stderr, "Can not find -sec %s -var %s -num %i \n", section, variable, num);
	retval = 1;
    }

    exit(retval);
}
