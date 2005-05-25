/********************************************************************
* Description: global_defs.h
*
*	Common defines used in many emc2 source files.
*
* Author: Paul Corner
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef GLOBAL_DEFS_H
#define GLOBAL_DEFS_H

/* LINELEN is used throughout for buffer sizes, length of file name strings,
   etc. Let's just have one instead of a multitude of defines all the same. */
#define LINELEN 256

#define ACTIVE_G_CODES 12
#define ACTIVE_M_CODES 7
#define ACTIVE_SETTINGS 3

#define MM_PER_INCH 25.4
#define INCH_PER_MM (1.0/25.4)

#define LINEAR_TOLERANCE 0.0001
#define ANGULAR_TOLERANCE 0.0001


#endif
