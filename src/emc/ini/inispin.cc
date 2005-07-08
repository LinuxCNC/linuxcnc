/********************************************************************
* Description: inispin.cc
*   INI file initialization for spindle controller
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
#include "inispin.hh"		// these decls
#include "emcglb.h"		// SPINDLE_FORWARD_INDEX, etc
// inifile ref'ed by iniSpindle(), loadSpindle()
    static Inifile *spindleInifile = 0;

/*
  loadSpindle()

  Loads ini file params for spindle from [EMCIO] section

  SPINDLE_OFF_WAIT <float>      seconds to brake on, after spindle off
  SPINDLE_ON_WAIT <float>       seconds to spindle on, after brake off

  SPINDLE_FORWARD_INDEX <int>   dio point for spindle forward
  SPINDLE_REVERSE_INDEX <int>   dio point for spindle reverse
  SPINDLE_DECREASE_INDEX <int>  dio point for spindle speed decrease
  SPINDLE_INCREASE_INDEX <int>  dio point for spindle speed increase
  SPINDLE_BRAKE_INDEX <int>     dio point for spindle brake on/off
  SPINDLE_ENABLE_INDEX <int>    dio point for spindle enable

  SPINDLE_ON_INDEX <int>        aio point for spindle speed reference

  SPINDLE_FORWARD_POLARITY <0,1>  polarity for spindle forward
  SPINDLE_REVERSE_POLARITY <0,1>  polarity for spindle reverse
  SPINDLE_DECREASE_POLARITY <0,1> polarity for spindle speed decrease
  SPINDLE_INCREASE_POLARITY <0,1> polarity for spindle speed increase
  SPINDLE_BRAKE_POLARITY <0,1>    polarity for spindle brake on/off
  SPINDLE_ENABLE_POLARITY <0,1>   polarity for spindle enable

  calls:

  emcSpindleSetOffWait(double wait);
  emcSpindleSetOnWait(double wait);
  emcSpindleSetForwardIndex(int index);
  emcSpindleSetReverseIndex(int index);
  emcSpindleSetDecreaseIndex(int index);
  emcSpindleSetIncreaseIndex(int index);
  emcSpindleSetBrakeIndex(int index);
  emcSpindleSetEnableIndex(int index);

  emcSpindleSetOnIndex(int index);

  emcSpindleSetForwardPolarity(int polarity);
  emcSpindleSetReversePolarity(int polarity);
  emcSpindleSetDecreasePolarity(int polarity);
  emcSpindleSetIncreasePolarity(int polarity);
  emcSpindleSetBrakePolarity(int polarity);
  emcSpindleSetEnablePolarity(int polarity);
  */

static int loadSpindle()
{
    int retval = 0;
    const char *inistring;
    int i;
    double d;

    if (NULL !=
	(inistring = spindleInifile->find("SPINDLE_OFF_WAIT", "EMCIO"))) {
	if (1 == sscanf(inistring, "%lf", &d)) {
	    // found, and valid
	    if (0 != emcSpindleSetOffWait(d)) {
		printf("bad return value from emcSpindleSetOffWait\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for SPINDLE_OFF_WAIT: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring = spindleInifile->find("SPINDLE_ON_WAIT", "EMCIO"))) {
	if (1 == sscanf(inistring, "%lf", &d)) {
	    // found, and valid
	    if (0 != emcSpindleSetOnWait(d)) {
		printf("bad return value from emcSpindleSetOnWait\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for SPINDLE_ON_WAIT: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_FORWARD_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetForwardIndex(i)) {
		printf
		    ("bad return value from emcSpindleSetForwardIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for SPINDLE_FORWARD_INDEX: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_REVERSE_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetReverseIndex(i)) {
		printf
		    ("bad return value from emcSpindleSetReverseIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for SPINDLE_REVERSE_INDEX: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_DECREASE_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetDecreaseIndex(i)) {
		printf
		    ("bad return value from emcSpindleSetDecreaseIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_DECREASE_INDEX: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_INCREASE_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetIncreaseIndex(i)) {
		printf
		    ("bad return value from emcSpindleSetIncreaseIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_INCREASE_INDEX: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_BRAKE_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetBrakeIndex(i)) {
		printf("bad return value from emcSpindleSetBrakeIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for SPINDLE_BRAKE_INDEX: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_ENABLE_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetEnableIndex(i)) {
		printf("bad return value from emcSpindleSetEnableIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for SPINDLE_ENABLE_INDEX: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring = spindleInifile->find("MIN_VOLTS_PER_RPM", "EMCIO"))) {
	if (1 == sscanf(inistring, "%lf", &d)) {
	    // found, and valid
	    if (0 != emcMinVoltsPerRpm(d)) {
		printf("bad return value from emcMinVoltsPerRpm\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for MIN_VOLTS_PER_RPM: %s\n",
		   inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring = spindleInifile->find("MAX_VOLTS_PER_RPM", "EMCIO"))) {
	if (1 == sscanf(inistring, "%lf", &d)) {
	    // found, and valid
	    if (0 != emcMaxVoltsPerRpm(d)) {
		printf("bad return value from emcMaxVoltsPerRpm\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for MAX_VOLTS_PER_RPM: %s\n",
		   inistring);
	}
    }
    // else ignore omission
    if (NULL !=
	(inistring = spindleInifile->find("SPINDLE_ON_INDEX", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetOnIndex(i)) {
		printf("bad return value from emcSpindleSetOnIndex\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf("invalid inifile value for SPINDLE_ON_INDEX: %s\n",
		   inistring);
	}
    }
    // else ignore omission
    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_FORWARD_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetForwardPolarity(i)) {
		printf
		    ("bad return value from emcSpindleSetSpindleForwardPolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_FORWARD_POLARITY: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_REVERSE_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetReversePolarity(i)) {
		printf
		    ("bad return value from emcSpindleSetReversePolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_REVERSE_POLARITY: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_DECREASE_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetDecreasePolarity(i)) {
		printf
		    ("bad return value from emcSpindleSetDecreasePolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_DECREASE_POLARITY: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_INCREASE_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetIncreasePolarity(i)) {
		printf
		    ("bad return value from emcSpindleSetIncreasePolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_INCREASE_POLARITY: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_BRAKE_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetBrakePolarity(i)) {
		printf
		    ("bad return value from emcSpindleSetBrakePolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_BRAKE_POLARITY: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    if (NULL !=
	(inistring =
	 spindleInifile->find("SPINDLE_ENABLE_POLARITY", "EMCIO"))) {
	if (1 == sscanf(inistring, "%d", &i)) {
	    // found, and valid
	    if (0 != emcSpindleSetEnablePolarity(i)) {
		printf
		    ("bad return value from emcSpindleSetEnablePolarity\n");
		retval = -1;
	    }
	} else {
	    // found, but invalid, so warn
	    printf
		("invalid inifile value for SPINDLE_ENABLE_POLARITY: %s\n",
		 inistring);
	}
    }
    // else ignore omission

    return retval;
}

/*
  iniSpindle(const char *filename)

  Loads ini file parameters for spindle, from [EMCIO] section
 */
int iniSpindle(const char *filename)
{
    int retval = 0;

    spindleInifile = new Inifile;

    if (spindleInifile->open(filename) == false) {
	return -1;
    }
    // load spindle values
    if (0 != loadSpindle()) {
	retval = -1;
    }
    // close the inifile
    spindleInifile->close();
    delete spindleInifile;

    return retval;
}

// functions to set global variables

int emcSpindleSetForwardIndex(int index)
{
    SPINDLE_FORWARD_INDEX = index;

    return 0;
}

int emcSpindleSetReverseIndex(int index)
{
    SPINDLE_REVERSE_INDEX = index;

    return 0;
}

int emcSpindleSetDecreaseIndex(int index)
{
    SPINDLE_DECREASE_INDEX = index;

    return 0;
}

int emcSpindleSetIncreaseIndex(int index)
{
    SPINDLE_INCREASE_INDEX = index;

    return 0;
}

int emcSpindleSetBrakeIndex(int index)
{
    SPINDLE_BRAKE_INDEX = index;

    return 0;
}

int emcSpindleSetEnableIndex(int index)
{
    SPINDLE_ENABLE_INDEX = index;

    return 0;
}

int emcSpindleSetOnIndex(int index)
{
    SPINDLE_ON_INDEX = index;

    return 0;
}

int emcMinVoltsPerRpm(double volts)
{
    MIN_VOLTS_PER_RPM = volts;
    return 0;
}

int emcMaxVoltsPerRpm(double volts)
{
    MAX_VOLTS_PER_RPM = volts;
    return 0;
}

int emcSpindleSetForwardPolarity(int polarity)
{
    SPINDLE_FORWARD_POLARITY = polarity;

    return 0;
}

int emcSpindleSetReversePolarity(int polarity)
{
    SPINDLE_REVERSE_POLARITY = polarity;

    return 0;
}

int emcSpindleSetDecreasePolarity(int polarity)
{
    SPINDLE_DECREASE_POLARITY = polarity;

    return 0;
}

int emcSpindleSetIncreasePolarity(int polarity)
{
    SPINDLE_INCREASE_POLARITY = polarity;

    return 0;
}

int emcSpindleSetBrakePolarity(int polarity)
{
    SPINDLE_BRAKE_POLARITY = polarity;

    return 0;
}

int emcSpindleSetEnablePolarity(int polarity)
{
    SPINDLE_ENABLE_POLARITY = polarity;

    return 0;
}

int emcSpindleSetOffWait(double wait)
{
    SPINDLE_OFF_WAIT = wait;

    return 0;
}

int emcSpindleSetOnWait(double wait)
{
    SPINDLE_ON_WAIT = wait;

    return 0;
}
