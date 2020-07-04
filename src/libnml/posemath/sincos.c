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
********************************************************************/
/*
   sincos.c

   Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
*/

#include "config.h"

#include "rtapi_math.h"
#include "sincos.h"

void pm_sincos(double x, double *sx, double *cx)
{
    *sx = sin(x);
    *cx = cos(x);
}
