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
#ifndef RTAPI_MATH_H
#define RTAPI_MATH_H

#include "rtapi.h"  /* Because of all the rtapi refs */
#include <float.h>  /* DBL_MAX and other FP goodies */

#ifndef M_PIl
#define M_PIl		3.1415926535897932384626433832795029L  /* pi */
#endif
#ifndef M_PI_2l
#define M_PI_2l        1.570796326794896619231321691639751442L /* pi/2 */
#endif
#ifndef M_PI
#define M_PI		3.1415926535897932384626433832795029   /* pi */
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

extern __inline double atan (double __y) {
    return atan2(__y, 1.);
}

extern __inline double asin (double __x) {
    return atan2(__x, sqrt (1.0 - __x * __x));
}

extern __inline double acos (double __x) {
    return atan2(sqrt(1.0 - __x * __x), __x);
}

extern __inline double fmax(double __y, double __x) {
    return __y > __x || __builtin_isnan(__x) ? __y : __x;
}
extern __inline double fmin(double __y, double __x) {
    return __y < __x || __builtin_isnan(__x) ? __y : __x;
}

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
