/********************************************************************
* Description: iniaxis.hh
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
#ifndef INIAXIS_HH
#define INIAXIS_HH

#include "emc.hh"		// EMC_AXIS_STAT

/* initializes axis modules from ini file */
extern int iniAxis(int axis, const char *filename);

extern double ext_offset_a_or_v_ratio[];

#endif
