/********************************************************************
* Description: sincos.h
*   support for native sincos functions
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
#ifndef SINCOS_H
#define SINCOS_H

#include "config.h"
/*
  for each platform that has built-in support for the sincos function,
  that should be discovered by ./configure
*/

/* testing for sincos now supported by ./configure */
#ifdef HAVE___SINCOS
#define sincos __sincos
#endif


/*
  all other platforms will not have __sincos, and will
  get the declaration for the explicit function
*/

#ifndef HAVE___SINCOS

extern void sincos(double x, double *sx, double *cx);

#endif

#endif /* #ifndef SINCOS_H */
