#ifndef RTAPI_MATH_H
#define RTAPI_MATH_H

#include <float.h>  /* DBL_MAX and other FP goodies */

#if defined(RTAPI) && !defined(SIM)
extern double sin(double);
extern double cos(double);
extern double sqrt(double);
extern double fabs(double);
extern double atan2(double, double);
extern double asin(double);
extern double acos(double);
extern double pow(double, double);

extern double round(double);
extern double ceil(double);
extern double floor(double);

#define M_PIl		3.1415926535897932384626433832795029L  /* pi */
#ifndef M_PI
#define M_PI		3.1415926535897932384626433832795029   /* pi */
#endif

#ifndef __GNUC_PREREQ
/* Convenience macro to test the versions of glibc and gcc. */
/*  taken from include/features.h */
#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define __GNUC_PREREQ(maj, min) 0
#endif
#endif /* __GNUC_PREREQ */


#ifdef __i386__
#include "rtapi_math_i386.h"
#endif

#else
#include <math.h>
#endif

#endif
