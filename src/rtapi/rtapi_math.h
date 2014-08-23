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

#include <float.h>  /* DBL_MAX and other FP goodies */

#if defined(RTAPI) && !defined(SIM)
extern double sin(double);
extern double cos(double);
extern double tan(double);
extern double sqrt(double);
extern double fabs(double);
extern double atan(double);
extern double atan2(double, double);
extern double asin(double);
extern double acos(double);
extern double pow(double, double);

extern double round(double);
extern double ceil(double);
extern double floor(double);

#define frexp(p,q) __builtin_frexp((p),(q))
#define isnan(x) __builtin_isnan((x))
#define signbit(x) __builtin_signbit((x))

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

#if __GNUC_PREREQ(4,4)
#define isinf(x) __builtin_isinf((x))
#else
#define isinf(x) ({ double v=((x)); !isnan(v) && isnan(v-v); })
#endif

#ifdef __i386__
#include "rtapi_math_i386.h"
#endif

#else
#include <math.h>
#endif

#endif
