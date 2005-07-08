/********************************************************************
* Description: inicool.cc
*   INI file initialization for coolant controller
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
}
#include "emc.hh"
#include "inifile.hh"
#include "inicool.hh"		// these decls
#include "emcglb.h"		// MIST_COOLANT_INDEX, etc
// inifile ref'ed by iniCoolant(), loadCoolant()
    static Inifile *coolantInifile = 0;

/*
  loadCoolant()

  Loads ini file params for coolant from [EMCIO] section

  MIST_COOLANT_INDEX <int>      dio point for mist coolant on/off
  FLOOD_COOLANT_INDEX <int>     dio point for flood coolant on/off

  MIST_COOLANT_POLARITY <0,1>   polarity for mist on
  FLOOD_COOLANT_POLARITY <0,1>  polarity for flood on

  calls:

  emcCoolantSetMistIndex(int index);
  emcCoolantSetFloodIndex(int index);

  emcCoolantSetMistPolarity(int polarity);
  emcCoolantSetFloodPolarity(int polarity);
  */

static int loadCoolant()
{
    int retval = 0;
    const char *inistring;
    int i;

    if (NULL !=
	(inistring =
	 coolantInifile->find("MIST_COOLANT_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcCoolantSetMistIndex(i)) {
		printf("bad return value from emcCoolantSetMistIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for MIST_COOLANT_INDEX: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 coolantInifile->find("FLOOD_COOLANT_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcCoolantSetFloodIndex(i)) {
		printf("bad return value from emcCoolantSetFloodIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for FLOOD_COOLANT_INDEX: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 coolantInifile->find("MIST_COOLANT_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcCoolantSetMistPolarity(i)) {
		printf
		    ("bad return value from emcCoolantSetMistPolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for MIST_COOLANT_POLARITY: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 coolantInifile->find("FLOOD_COOLANT_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcCoolantSetFloodPolarity(i)) {
		printf
		    ("bad return value from emcCoolantSetFloodPolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for FLOOD_COOLANT_POLARITY: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    return retval;
}

/*
  iniCoolant(const char *filename)

  Loads ini file parameters for coolant controller, from [EMCIO] section
 */
int iniCoolant(const char *filename)
{
    int retval = 0;

    coolantInifile = new Inifile;

    if (coolantInifile->open(filename) == false) {
	return -1;
    }
    // load coolant values
    if (0 != loadCoolant()) {
	retval = -1;
    }
    // close the inifile
    coolantInifile->close();
    delete coolantInifile;

    return retval;
}

// implementations of functions to set ini file global variables

int emcCoolantSetMistIndex(int index)
{
    MIST_COOLANT_INDEX = index;

    return 0;
}

int emcCoolantSetFloodIndex(int index)
{
    FLOOD_COOLANT_INDEX = index;

    return 0;
}

int emcCoolantSetMistPolarity(int polarity)
{
    MIST_COOLANT_POLARITY = polarity;

    return 0;
}

int emcCoolantSetFloodPolarity(int polarity)
{
    FLOOD_COOLANT_POLARITY = polarity;

    return 0;
}
