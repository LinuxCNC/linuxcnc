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

extern "C" {
#include <stdio.h>		/* printf(), fprintf(), FILE, fopen(),*/
#include <stdlib.h>		/* exit() */
#include <string.h>		/* strcmp(), strcpy() */
}
#include "inifile.hh"
int main(int argc, char *argv[])
{
    int t;
    int num = 1;
    char _variable[LINELEN] = "";
    char *variable = 0;
    char _section[LINELEN] = "";
    char *section = 0;
    char path[LINELEN] = "emc.ini";
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
		strncpy(path, argv[t + 1], LINELEN);
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-var")) {
	    if (t == argc - 1) {
		/* no arg following -var, so abort */
		fprintf(stderr,
		    "%s: variable name not specified after -var\n", argv[0]);
		exit(1);
	    } else {
		strncpy(_variable, argv[t + 1], LINELEN);
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
		strncpy(_section, argv[t + 1], LINELEN);
		section = _section;
		t++;		/* step over following arg */
	    }
	} else if (!strcmp(argv[t], "-num")) {
	    if (t == argc - 1) {
		/* no arg following -num, so abort */
		fprintf(stderr,
		    "%s: line not specified after -num\n", argv[0]);
		exit(1);
	    } else {
		if (sscanf(argv[t + 1], "%i", &num) != 1) {
		    fprintf(stderr,
			"%s: invalid number after -num\n", argv[0]);
		    exit(1);
		}
		t++;		/* step over following arg */
	    }
	} else{
	    /* invalid argument */
	    fprintf(stderr,
		"%s: -var <variable> {-sec <section>} {<-ini inifile>} [-num <nth item>]\n",
		argv[0]);
	    exit(1);
	}
    }

    /* check that variable was supplied */
    if (0 == variable) {
	fprintf(stderr, "%s: no variable supplied\n", argv[0]);
	exit(1);
    }

    Inifile *inifile;
    /* open the inifile */
    inifile = new Inifile(path);
    if (inifile->valid() == false) {
	fprintf(stderr, "%s: can't open %s\n", argv[0], path);
	delete inifile;
	exit(-1);
    }

    inistring = inifile->find(variable, section, num);
    if (inistring != NULL) {
	printf("%s\n", inistring);
    }

    delete inifile;

    exit(0);
}
