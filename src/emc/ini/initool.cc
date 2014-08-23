/********************************************************************
* Description: initool.cc
*   INI file initialization for tool controller
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

#include <stdio.h>		// NULL
#include <stdlib.h>		// atol()
#include <string.h>		// strcpy()

#include "emc.hh"
#include "emcpos.h"             // EmcPose
#include "rcs_print.hh"
#include "inifile.hh"
#include "initool.hh"		// these decls
#include "emcglb.h"		// TOOL_TABLE_FILE

/*
  loadTool()

  Loads ini file params for spindle from [EMCIO] section

  TOOL_TABLE <file name>  name of tool table file

  calls:

  emcToolSetToolTableFile(const char filename);
  */

static int loadTool(IniFile *toolInifile)
{
    int retval = 0;
    const char *inistring;

    if (NULL != (inistring = toolInifile->Find("TOOL_TABLE", "EMCIO"))) {
	if (0 != emcToolSetToolTableFile(inistring)) {
	    rcs_print("bad return value from emcToolSetToolTableFile\n");
	    retval = -1;
	}
    }
    // else ignore omission

    return retval;
}

/*
  readToolChange() reads the values of [EMCIO] TOOL_CHANGE_POSITION and
  TOOL_HOLDER_CLEAR, and loads them into their associated globals 
*/
static int readToolChange(IniFile *toolInifile)
{
    int retval = 0;
    const char *inistring;

    if (NULL !=
	(inistring = toolInifile->Find("TOOL_CHANGE_POSITION", "EMCIO"))) {
	/* found an entry */
        if (9 == sscanf(inistring, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
                        &TOOL_CHANGE_POSITION.tran.x,
                        &TOOL_CHANGE_POSITION.tran.y,
                        &TOOL_CHANGE_POSITION.tran.z,
                        &TOOL_CHANGE_POSITION.a,
                        &TOOL_CHANGE_POSITION.b,
                        &TOOL_CHANGE_POSITION.c,
                        &TOOL_CHANGE_POSITION.u,
                        &TOOL_CHANGE_POSITION.v,
                        &TOOL_CHANGE_POSITION.w)) {
            HAVE_TOOL_CHANGE_POSITION=1;
            retval=0;
        } else if (6 == sscanf(inistring, "%lf %lf %lf %lf %lf %lf",
                        &TOOL_CHANGE_POSITION.tran.x,
                        &TOOL_CHANGE_POSITION.tran.y,
                        &TOOL_CHANGE_POSITION.tran.z,
                        &TOOL_CHANGE_POSITION.a,
                        &TOOL_CHANGE_POSITION.b,
                        &TOOL_CHANGE_POSITION.c)) {
	    TOOL_CHANGE_POSITION.u = 0.0;
	    TOOL_CHANGE_POSITION.v = 0.0;
	    TOOL_CHANGE_POSITION.w = 0.0;
            HAVE_TOOL_CHANGE_POSITION = 1;
            retval = 0;
        } else if (3 == sscanf(inistring, "%lf %lf %lf",
                               &TOOL_CHANGE_POSITION.tran.x,
                               &TOOL_CHANGE_POSITION.tran.y,
                               &TOOL_CHANGE_POSITION.tran.z)) {
	    /* read them OK */
	    TOOL_CHANGE_POSITION.a = 0.0;
	    TOOL_CHANGE_POSITION.b = 0.0;
	    TOOL_CHANGE_POSITION.c = 0.0;
	    TOOL_CHANGE_POSITION.u = 0.0;
	    TOOL_CHANGE_POSITION.v = 0.0;
	    TOOL_CHANGE_POSITION.w = 0.0;
	    HAVE_TOOL_CHANGE_POSITION = 1;
	    retval = 0;
	} else {
	    /* bad format */
	    rcs_print("bad format for TOOL_CHANGE_POSITION\n");
	    HAVE_TOOL_CHANGE_POSITION = 0;
	    retval = -1;
	}
    } else {
	/* didn't find an entry */
	HAVE_TOOL_CHANGE_POSITION = 0;
    }

    if (NULL !=
	(inistring = toolInifile->Find("TOOL_HOLDER_CLEAR", "EMCIO"))) {
	/* found an entry */
	if (3 == sscanf(inistring, "%lf %lf %lf",
			&TOOL_HOLDER_CLEAR.tran.x,
			&TOOL_HOLDER_CLEAR.tran.y,
			&TOOL_HOLDER_CLEAR.tran.z)) {
	    /* read them OK */
	    TOOL_HOLDER_CLEAR.a = 0.0;	// not supporting ABC for now
	    TOOL_HOLDER_CLEAR.b = 0.0;
	    TOOL_HOLDER_CLEAR.c = 0.0;
	    HAVE_TOOL_HOLDER_CLEAR = 1;
	    retval = 0;
	} else {
	    /* bad format */
	    rcs_print("bad format for TOOL_HOLDER_CLEAR\n");
	    HAVE_TOOL_HOLDER_CLEAR = 0;
	    retval = -1;
	}
    } else {
	/* didn't find an entry */
	HAVE_TOOL_HOLDER_CLEAR = 0;
    }

    return retval;
}

/*
  iniTool(const char *filename)

  Loads ini file parameters for tool controller, from [EMCIO] section
 */
int iniTool(const char *filename)
{
    int retval = 0;
    IniFile toolInifile;

    if (toolInifile.Open(filename) == false) {
	return -1;
    }
    // load tool values
    if (0 != loadTool(&toolInifile)) {
	retval = -1;
    }
    // read the tool change positions
    if (0 != readToolChange(&toolInifile)) {
	retval = -1;
    }
    // close the inifile
    toolInifile.Close();

    return retval;
}

// functions to set global variables

int emcToolSetToolTableFile(const char *filename)
{
    strncpy(TOOL_TABLE_FILE, filename, sizeof(TOOL_TABLE_FILE));
    if (TOOL_TABLE_FILE[sizeof(TOOL_TABLE_FILE)-1] != '\0') {
        rcs_print("Tool Table File name too long, max %zu characters.\n", sizeof(TOOL_TABLE_FILE)-1);
        rcs_print("Requested Tool Table File name was: %s\n", filename);
        return -1;
    }
    return 0;
}
