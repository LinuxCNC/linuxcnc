/*
 * Declarations for math functions.
 * Copyright (C) 1991,92,93,95,96,97,98,99,2001 Free Software Foundation, Inc.
 * This file is part of the GNU C Library.
 *
 * The GNU C Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * The GNU C Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the GNU C Library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 */

/*
 * ISO C99 Standard: 7.12 Mathematics <math.h>
 */

#ifndef	_XENO_MATH_H
#define	_XENO_MATH_H	1
#define _MATH_H         1

//LIBMSRCS= s_floor.c w_pow.c s_frexp.c s_fabs.c      	e_pow.c e_sqrt.c s_scalbn.c s_copysign.c
extern 	double fabs(double x);
extern 	double frexp(double x, int *eptr);
extern 	double floor(double x);
extern 	double pow(double x, double y);
extern 	double __ieee754_sqrt(double x);

#endif /* !_XENO_MATH_H  */
