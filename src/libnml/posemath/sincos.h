/********************************************************************
* Description: sincos.h
*   support for native sincos functions
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
#ifndef SINCOS_H
#define SINCOS_H

/*
  for each platform that has built-in support for the sincos function,
  define SINCOS_SUPPORT here
*/

#if defined (__GLIBC__) && defined(__USE_GNU) && defined(__FAST_MATH__)
#define SINCOS_SUPPORT
#define sincos __sincos
#endif

/*
  all other platforms will not have SINCOS_SUPPORT defined, and will
  get the declaration for the explicit function
*/

#ifndef SINCOS_SUPPORT

extern void sincos(double x, double *sx, double *cx);

#endif

#endif /* #ifndef SINCOS_H */
