/********************************************************************
* Description: rcsversion.h
*   Library version number.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/
#ifndef LIBVERSION_H
#define LIBVERSION_H

#define LIB_VERSION "5.1"
#define LIB_MAJOR_VERSION (5)
#define LIB_MINOR_VERSION (1)

static const int lib_major_version = LIB_MAJOR_VERSION;
static const int lib_minor_version = LIB_MINOR_VERSION;

static const char __attribute__ ((unused)) * rcs_version_info_string =
    "@(#)" " $Info: NML Library version " LIB_VERSION " Compiled on  "
    __DATE__ " at " __TIME__ " for Linux. $ \n";

#endif
