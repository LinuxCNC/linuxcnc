/********************************************************************
* Description:  mathstubs.c
*               Stubs for 'errno' and some error printing functions
*               in the math library that don't exist in the kernel.
*
* Author: Paul_C - Derived from a file by Fred Proctor.
* Created at: Mon Feb 16 21:08:44 GMT 2004
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

#include <linux/types.h>	/* u_int16_t */
#include "rtapi_math.h"

int stderr;

int fputs(const char *str)
{
    return 0;
}

unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nobj,
    void *stream)
{
    return nobj;
}

static int errno;

int *__errno_location(void)
{
    return &errno;
}

void __assert_fail(const char *s, const char *file, unsigned int line,
    const char *function)
{
    return;
}

#ifndef isnan
int isnan(double x)
{
/* Return zero if x is a real number. */
    int a;
    /* According to notes, a floating point number consists of 8 bytes.
       Expressed as a 64 bit No. the sign will be B63. If bits 52-62 equal
       0x7FF and bits 0-61 are non-zero, the number is a NaN. If bits 52-62
       equal 0x7FF and bits 0-61 are zero, the number is infinite. An infinite
       number will still cause errors so it should be safe to flag it as a
       NaN. */
    u_int16_t *c = (u_int16_t *) & x;
    a = c[3] & 0x7FF0;
    return (a == 0x7FF0);
}
#endif

#ifndef __isnan
int __isnan(double x)
{				/* There must be a better way of doing this ! 
				 */
    int a;
    u_int16_t *c = (u_int16_t *) & x;
    a = c[3] & 0x7FF0;
    return (a == 0x7FF0);
}
#endif

int RTdummy(void)
{
    return stderr;
}
