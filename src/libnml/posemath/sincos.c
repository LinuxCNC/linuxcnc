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

#include <math.h>

void sincos(double x, double *sx, double *cx)
{
    *sx = sin(x);
    *cx = cos(x);
}

#endif

#endif
