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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#ifndef RTAPI_MATH_H
#define RTAPI_MATH_H

#include "rtapi_byteorder.h"  /* Using the bytorder to specify the ieee types */
#include "rtapi_errno.h" /* Using the error codes */
#include <float.h>  /* DBL_MAX and other FP goodies */

#ifdef __cplusplus
extern "C"
{
#endif

extern int libm_errno;

extern double rtapi_sin(double);
extern double rtapi_cos(double);
extern double rtapi_tan(double);
extern double rtapi_sqrt(double);
extern double rtapi_fabs(double);
extern double rtapi_atan(double);
extern double rtapi_atan2(double, double);
extern double rtapi_asin(double);
extern double rtapi_acos(double);
extern double rtapi_pow(double, double);
extern double rtapi_fmin(double, double);
extern double rtapi_fmax(double, double);
extern double rtapi_fmod(double, double);
extern double rtapi_hypot(double, double);
extern double rtapi_rint(double);
extern double rtapi_scalbn(double, int);
extern int rtapi_finite(double);
extern double rtapi_copysign(double, double);

extern double rtapi_ceil(double);
extern double rtapi_floor(double);
extern double rtapi_cbrt(double);

#ifdef __cplusplus
}
#endif

#define rtapi_frexp(p,q) __builtin_frexp((p),(q))
#define rtapi_isnan(x) __builtin_isnan((x))
#define rtapi_signbit(x) __builtin_signbit((x))

#ifndef M_PIl
#define M_PIl		3.1415926535897932384626433832795029L  /* pi */
#endif
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

#if __GNUC_PREREQ(4,4)
#define rtapi_isinf(x) __builtin_isinf((x))
#else
#define rtapi_isinf(x) ({ double v=((x)); !isnan(v) && isnan(v-v); })
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
