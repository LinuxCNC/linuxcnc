/********************************************************************
* Description: inispindle.hh
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2021 All rights reserved.
*
* Last change: file created by andypugh 30/12/21
********************************************************************/
#ifndef INISPINDLE_HH
#define INISPINDLE_HH

#include "emc.hh"		// EMC_AXIS_STAT

/* initializes spindle modules from ini file */
extern int iniSpindle(int spindle, const char *filename);

#endif
