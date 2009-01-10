/********************************************************************
* Description: inijoint.hh
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
#ifndef INIJOINT_HH
#define INIJOINT_HH

#include "emc.hh"		// EMC_JOINT_STAT

/* initializes joint modules from ini file */
extern int iniJoint(int joint, const char *filename);

#endif
