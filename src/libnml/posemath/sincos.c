/********************************************************************
* Description: sincos.c
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
* $Revision$
* $Author$
* $Date$
********************************************************************/
#if HAVE_CONFIG_H
#include "rcs_config.h"
#endif

/*
   sincos.c

   Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
*/

#ifndef HAVE_SINCOS

#include "sincos.h"

#ifndef SINCOS_SUPPORT

#include <linux/types.h>
#ifdef __attribute_used__
#undef __attribute_used__
#endif
#include <sys/cdefs.h>
#include <float.h>
#include <math.h>

void sincos(double x, double *sx, double *cx)
{
    *sx = sin(x);
    *cx = cos(x);
}

#endif

#endif
