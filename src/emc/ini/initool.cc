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
* $Revision$
* $Author$
* $Date$
********************************************************************/

extern "C" {
#include <stdio.h>		// NULL
#include <stdlib.h>		// atol()
#include <string.h>		// strcpy()
}
#include "emc.hh"
#include "inifile.hh"
#include "initool.hh"		// these decls
#include "emcglb.h"		// TOOL_TABLE_FILE
// inifile ref'ed by iniTool(), loadTool() 

static INIFILE *toolInifile = 0;

/*
  loadTool()

  Loads ini file params for spindle from [EMCIO] section

  TOOL_TABLE <file name>  name of tool table file

  calls:

  emcToolSetToolTableFile(const char filename);
  */

static int loadTool(void)
{
  int retval = 0;
  const char *inistring;

  if (NULL != (inistring = toolInifile->find("TOOL_TABLE", "EMCIO"))) {
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
static int readToolChange(void)
{
  int retval = 0;
  const char *inistring;

  if (NULL != (inistring = toolInifile->find("TOOL_CHANGE_POSITION", "EMCIO"))) {
    /* found an entry */
    if (3 == sscanf(inistring, "%lf %lf %lf",
		    &TOOL_CHANGE_POSITION.tran.x,
		    &TOOL_CHANGE_POSITION.tran.y,
		    &TOOL_CHANGE_POSITION.tran.z)) {
      /* read them OK */
      TOOL_CHANGE_POSITION.a = 0.0; // not supporting ABC for now
      TOOL_CHANGE_POSITION.b = 0.0;
      TOOL_CHANGE_POSITION.c = 0.0;
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

  if (NULL != (inistring = toolInifile->find("TOOL_HOLDER_CLEAR", "EMCIO"))) {
    /* found an entry */
    if (3 == sscanf(inistring, "%lf %lf %lf",
		    &TOOL_HOLDER_CLEAR.tran.x,
		    &TOOL_HOLDER_CLEAR.tran.y,
		    &TOOL_HOLDER_CLEAR.tran.z)) {
      /* read them OK */
      TOOL_HOLDER_CLEAR.a = 0.0; // not supporting ABC for now
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

  toolInifile = new INIFILE;

  if (-1 == toolInifile->open(filename)) {
    return -1;
  }
  // load tool values
  if (0 != loadTool()) {
    retval = -1;
  }
  // read the tool change positions
  if (0 != readToolChange()) {
    retval = -1;
  }
  // close the inifile
  toolInifile->close();
  delete toolInifile;

  return retval;
}

// functions to set global variables

int emcToolSetToolTableFile(const char *filename)
{
  strcpy(TOOL_TABLE_FILE, filename);

  return 0;
}
