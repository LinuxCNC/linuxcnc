#ifndef RTAPI_MATH_H
#define RTAPI_MATH_H

#if defined(RTAPI) && !defined(SIM)
extern double sin(double);
extern double cos(double);
extern double sqrt(double);
extern double fabs(double);
extern double atan2(double, double);
extern double acos(double);
extern double pow(double, double);

extern double round(double);
extern double ceil(double);
extern double floor(double);

#define M_PIl		3.1415926535897932384626433832795029L  /* pi */

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


#ifndef HUGE_VAL
/* IEEE positive infinity (-HUGE_VAL is negative infinity).  */
/*  taken from include/bits/huge_val.h */
#if __GNUC_PREREQ(3,3)
# define HUGE_VAL	(__builtin_huge_val())
#elif __GNUC_PREREQ(2,96)
# define HUGE_VAL	(__extension__ 0x1.0p2047)
#else
# define HUGE_VAL \
  (__extension__							      \
   ((union { unsigned __l __attribute__((__mode__(__DI__))); double __d; })   \
    { __l: 0x7ff0000000000000ULL }).__d)
#endif
#endif /* HUGE_VAL */


#ifdef __i386__
#include "rtapi_math_i386.h"
#endif

#else
#include <math.h>
#endif

#endif
