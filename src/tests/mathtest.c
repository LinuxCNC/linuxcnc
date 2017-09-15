/********************************************************************
* Description: mathtest.c - A small file to test for unresolved
*                        symbols in loadable kernel modules.
*
* Author: Paul_C - Derived from a file by Fred Proctor.
* Created at: Mon Feb 16 20:39:42 GMT 2004
* Computer: Morphix 
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#define MODULE
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <math.h>		/* sin(), cos(), isnan() etc. */
#include <float.h>		/* DBL_MAX */
#include <errno.h>		/* errno, EDOM */

/*
  math functions are:

  extern double sin(double);            used in posemath, siggen, & noncartesian kins
  extern double cos(double);            used in posemath, siggen, & noncartesian kins
  extern double tan(double);            not used in RT
  extern double asin(double);           not used in RT
  extern double acos(double);           used in posemath & noncartesian kins
  extern double atan2(double, double);  used in posemath & noncartesian kins
  extern double sinh(double);           not used in RT
  extern double cosh(double);           not used in RT
  extern double tanh(double);           not used in RT
  extern double exp(double);            not used in RT
  extern double log(double);            not used in RT
  extern double log10(double);          not used in RT
  extern double pow(double, double);    not used in RT
  extern double sqrt(double);           used in tc, segmot, & noncartesean kins.
  extern double ceil(double);           used in segmot & emcpid
  extern double floor(double);          used by siggen & segmot
  extern double fabs(double);           used a lot in RT
  extern double ldexp(double, int);     not used in RT

  extern double sincos(double, double *, double *); Is called at four places in
                                                    posemath - None of the resulting
                                                    functions are used in EMC.
  Extras:

  extern int isnan(double);             Not used directly in RT - But is called
                                        by several (all ?) of the floating point
					math functions.
*/

/* Declare as volatile and gcc *shouldn't* optimize too much... */
volatile double a = 0;
volatile double b = 0;
volatile double c = 0;
volatile double x;
double d;
int n;

/*
  isnan() - C99 extensions to the math library functions

  IEEE double-precision floating point:

  N = (-1)^S * 2^(E-1023) * 1.F

  where S, E and F are composed of bits as labeled in these 8 bytes:

  [7]      [6]      [5]          [0]
  SEEEEEEE EEEEFFFF FFFFFFFF ... FFFFFFFF

  If E = 2047 and F is nonzero, N = Not a Number (NaN)
  If E = 2047 and F = 0 and S = 1, N = -Inf
  If E = 2047 and F = 0 and S = 0, N = Inf
  If 0 < E < 2047, N = (-1)^S * 2^(E-1023) * 1.F
  If E = 0 and F is nonzero, N = (-1)^S * 2^(-1022) * 0.F ("unnormalized")
  If E = 0 and F = 0 and S = 1, N = -0
  If E = 0 and F = 0 and S = 0, N = 0

  Example: for N = -1, S = 1, E = 1023, F = 0, and
  byte[0] = 10111111 = BF
  byte[1] = 11110000 = F0
  byte[2-7] = 0
*/

int isnan(double d)
{
  int e;
  unsigned char * c = (unsigned char *) &d;

  e = (int) (c[7] & 0x7F);
  e <<= 4;
  e += (int) ((c[6] & 0xF0) >> 4);

  return (e == 2047) && (c[0] || c[1] || c[2] || c[3] || 
			 c[4] || c[5] || (c[6] & 0x0F));
}

int math_test(void)
{
  double v, u;
  a = 1.00039276;
  b = 1.9999 * a;
  c = a / 2;

  /* force a domain error, EDOM, and check that we got it */
  x = acos(b);
  if (isnan(x)) x = 0.0;

  /* force a range error, ERANGE, and check that we got it */
  x = pow(DBL_MAX, b);
  if (isnan(x)) x = 0.0;

  /* do some legit math */
  x = sin(a) + cos(a) + tan(a) + 
      asin(c) + acos(c) + atan2(a, b) +
      sinh(a) + cosh(a) + tanh(a) + 
      exp(a) + log(a) + log10(a) + 
      pow(a, b) + sqrt(a) + ceil(a) + 
      floor(a) + fabs(a) + ldexp(a, b);

  /* Test the sincos func */
  sincos(x, &v, &u);

  return 0;
}
