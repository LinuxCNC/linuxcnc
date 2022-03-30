/********************************************************************
* Description: linuxcnc.hh
*	Common defines used in many emc2 source files.
*
*
* Author: Petter Reinholdtsen
* License: LGPL Version 2
* System: Any
*
* Copyright (c) 2021 All rights reserved.
********************************************************************/

#ifndef LINUXCNC_H
#define LINUXCNC_H

/* LINELEN is used throughout for buffer sizes, length of file name strings,
   etc. Let's just have one instead of a multitude of defines all the same. */
#define LINELEN 255

/* Used in a number of places for sprintf() buffers. */
#define BUFFERLEN 80

/* Imperial/Metric conversion */
#define MM_PER_INCH 25.4
#define INCH_PER_MM (1.0/MM_PER_INCH)

#endif /* LINUXCNC_H */
