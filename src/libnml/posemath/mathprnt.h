/* Prototypes for math printing functions */

/*
   Modification history:

    9-Jan-1998 WPS forces VxWorks to use C function calls io streams are
    not available.
  16-May-1997 WPS added #ifdef __cplusplus #define __CPLUSPLUS__
  all ANSI C++ compilers define __cplusplus automatically so it
  makes more sense to use than __CPLUSPLUS__ which needs to be
  defined separately.
  14-Apr-1997  FMP took out legacy math_print stuff
   10-Feb-1997  FMP added C++ stuff
   4-Nov-1996  Fred Proctor added math_printError()
*/

#ifndef MATHPRNT_H
#define MATHPRNT_H

#include "posemath.h"

#ifdef __cplusplus
#ifndef __CPLUSPLUS__
#define __CPLUSPLUS__
#endif
#endif

#if defined(__CPLUSPLUS__) && !defined(VXWORKS) && !defined(gnuwin32) && !defined(NO_IOSTREAM)

// If we have generated config.h all .cc files that include this header must
// first include either <iostream.h> or <iostream>
#ifndef HAVE_CONFIG_H
#include <iostream.h>
#endif

extern ostream & operator <<(ostream & stream, PM_CARTESIAN v);
extern ostream & operator <<(ostream & stream, PM_SPHERICAL s);
extern ostream & operator <<(ostream & stream, PM_CYLINDRICAL c);
extern ostream & operator <<(ostream & stream, PM_QUATERNION q);
extern ostream & operator <<(ostream & stream, PM_ROTATION_VECTOR r);
extern ostream & operator <<(ostream & stream, PM_ROTATION_MATRIX m);
extern ostream & operator <<(ostream & stream, PM_EULER_ZYZ zyz);
extern ostream & operator <<(ostream & stream, PM_EULER_ZYX zyx);
extern ostream & operator <<(ostream & stream, PM_RPY rpy);
extern ostream & operator <<(ostream & stream, PM_POSE pose);
extern ostream & operator <<(ostream & stream, PM_HOMOGENEOUS hom);

#else /* end of C++ */

#ifdef __cplusplus
#ifdef __cplusplus
extern "C" {
#endif
#endif

#include <stdio.h>

#ifdef __cplusplus
#ifdef __cplusplus
}
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif

    extern void pmSprintf(char *string, const char *format, ...);
    extern void pmPrintf(const char *format, ...);
    extern void pmFprintf(FILE * file, const char *format, ...);

#ifdef __cplusplus
}
#endif
#endif				/* no C++ */
#endif				/* #ifndef MATHPRNT_H */
