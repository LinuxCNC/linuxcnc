//    Copyright 2006-2010 Various Authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef __LINUXCNC_RTAPI_MATH_H
#define __LINUXCNC_RTAPI_MATH_H

#include "rtapi.h"  /* Because of all the rtapi refs */
#include <float.h>  /* DBL_MAX and other FP goodies */

/* The math constants are not in ISO C.  Userspace gets them from math.h,
   where glibc keeps the long double ones behind _GNU_SOURCE, and a kernel
   build includes no math.h at all, so define whatever is missing.  Values
   are the ones glibc uses. */
#ifndef M_PIl
#define M_PIl		3.1415926535897932384626433832795029L  /* pi */
#endif
#ifndef M_PI_2l
#define M_PI_2l        1.570796326794896619231321691639751442L /* pi/2 */
#endif
#ifndef M_E
#define M_E		2.7182818284590452354	/* e */
#endif
#ifndef M_LOG2E
#define M_LOG2E		1.4426950408889634074	/* log_2 e */
#endif
#ifndef M_LOG10E
#define M_LOG10E	0.43429448190325182765	/* log_10 e */
#endif
#ifndef M_LN2
#define M_LN2		0.69314718055994530942	/* log_e 2 */
#endif
#ifndef M_LN10
#define M_LN10		2.30258509299404568402	/* log_e 10 */
#endif
#ifndef M_PI
#define M_PI		3.14159265358979323846	/* pi */
#endif
#ifndef M_PI_2
#define M_PI_2		1.57079632679489661923	/* pi/2 */
#endif
#ifndef M_PI_4
#define M_PI_4		0.78539816339744830962	/* pi/4 */
#endif
#ifndef M_1_PI
#define M_1_PI		0.31830988618379067154	/* 1/pi */
#endif
#ifndef M_2_PI
#define M_2_PI		0.63661977236758134308	/* 2/pi */
#endif
#ifndef M_2_SQRTPI
#define M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#endif
#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */
#endif

#if defined(__KERNEL__)
extern double sin(double);
extern double cos(double);
extern double tan(double);
extern double sqrt(double);
extern double fabs(double);
extern double atan(double);
extern double atan2(double, double);
extern double asin(double);
extern double acos(double);
extern double exp(double);
extern double pow(double, double);
extern double fmin(double, double);
extern double fmax(double, double);
extern double fmod(double, double);

extern double round(double);
extern double ceil(double);
extern double floor(double);

#define frexp(p,q) __builtin_frexp((p),(q))
#define isnan(x) __builtin_isnan((x))
#define signbit(x) __builtin_signbit((x))
#define nan(x) __builtin_nan((x))

#define isinf(x) __builtin_isinf((x))
#define isfinite(x) __builtin_isfinite((x))

#ifdef __i386__
#include "rtapi_math_i386.h"
#endif

#else
#include <math.h>
#endif

#include "rtapi_byteorder.h"

// adapted from ieee754.h
union ieee754_double
  {
    double d;

    /* This is the IEEE 754 double-precision format.  */
    struct
      {
#if     RTAPI_BIG_ENDIAN
        unsigned int negative:1;
        unsigned int exponent:11;
        /* Together these comprise the mantissa.  */
        unsigned int mantissa0:20;
        unsigned int mantissa1:32;
#endif                          /* Big endian.  */
#if     RTAPI_LITTLE_ENDIAN
# if    RTAPI_FLOAT_BIG_ENDIAN
        unsigned int mantissa0:20;
        unsigned int exponent:11;
        unsigned int negative:1;
        unsigned int mantissa1:32;
# else
        /* Together these comprise the mantissa.  */
        unsigned int mantissa1:32;
        unsigned int mantissa0:20;
        unsigned int exponent:11;
        unsigned int negative:1;
# endif
#endif                          /* Little endian.  */
      } ieee;

    /* This format makes it easier to see if a NaN is a signalling NaN.  */
    struct
      {
#if     RTAPI_BIG_ENDIAN
        unsigned int negative:1;
        unsigned int exponent:11;
        unsigned int quiet_nan:1;
        /* Together these comprise the mantissa.  */
        unsigned int mantissa0:19;
        unsigned int mantissa1:32;
#else
# if    RTAPI_FLOAT_BIG_ENDIAN
        unsigned int mantissa0:19;
        unsigned int quiet_nan:1;
        unsigned int exponent:11;
        unsigned int negative:1;
        unsigned int mantissa1:32;
# else
        /* Together these comprise the mantissa.  */
        unsigned int mantissa1:32;
        unsigned int mantissa0:19;
        unsigned int quiet_nan:1;
        unsigned int exponent:11;
        unsigned int negative:1;
# endif
#endif
      } ieee_nan;
  };

#define IEEE754_DOUBLE_BIAS     0x3ff /* Added to exponent.  */

#endif
