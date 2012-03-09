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
                        &tool_change_position.tran.x,
                        &tool_change_position.tran.y,
                        &tool_change_position.tran.z,
                        &tool_change_position.a,
                        &tool_change_position.b,
                        &tool_change_position.c,
                        &tool_change_position.u,
                        &tool_change_position.v,
                        &tool_change_position.w)) {
            have_tool_change_position=1;
            retval=0;
        } else if (6 == sscanf(inistring, "%lf %lf %lf %lf %lf %lf",
                        &tool_change_position.tran.x,
                        &tool_change_position.tran.y,
                        &tool_change_position.tran.z,
                        &tool_change_position.a,
                        &tool_change_position.b,
                        &tool_change_position.c)) {
	    tool_change_position.u = 0.0;
	    tool_change_position.v = 0.0;
	    tool_change_position.w = 0.0;
            have_tool_change_position = 1;
            retval = 0;
        } else if (3 == sscanf(inistring, "%lf %lf %lf",
                               &tool_change_position.tran.x,
                               &tool_change_position.tran.y,
                               &tool_change_position.tran.z)) {
	    /* read them OK */
	    tool_change_position.a = 0.0;
	    tool_change_position.b = 0.0;
	    tool_change_position.c = 0.0;
	    tool_change_position.u = 0.0;
	    tool_change_position.v = 0.0;
	    tool_change_position.w = 0.0;
	    have_tool_change_position = 1;
	    retval = 0;
	} else {
	    /* bad format */
	    rcs_print("bad format for TOOL_CHANGE_POSITION\n");
	    have_tool_change_position = 0;
	    retval = -1;
	}
    } else {
	/* didn't find an entry */
	have_tool_change_position = 0;
    }

    if (NULL !=
	(inistring = toolInifile->Find("TOOL_HOLDER_CLEAR", "EMCIO"))) {
	/* found an entry */
	if (3 == sscanf(inistring, "%lf %lf %lf",
			&tool_holder_clear.tran.x,
			&tool_holder_clear.tran.y,
			&tool_holder_clear.tran.z)) {
	    /* read them OK */
	    tool_holder_clear.a = 0.0;	// not supporting ABC for now
	    tool_holder_clear.b = 0.0;
	    tool_holder_clear.c = 0.0;
	    have_tool_holder_clear = 1;
	    retval = 0;
	} else {
	    /* bad format */
	    rcs_print("bad format for TOOL_HOLDER_CLEAR\n");
	    have_tool_holder_clear = 0;
	    retval = -1;
	}
    } else {
	/* didn't find an entry */
	have_tool_holder_clear = 0;
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
    strncpy(tool_table_file, filename, sizeof(tool_table_file));
    if (tool_table_file[sizeof(tool_table_file)-1] != '\0') {
        rcs_print("Tool Table File name too long, max %zu characters.\n", sizeof(tool_table_file)-1);
        rcs_print("Requested Tool Table File name was: %s\n", filename);
        return -1;
    }
    return 0;
}
