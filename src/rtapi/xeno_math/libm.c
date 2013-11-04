/*
rtai/libm/libm.c - module wrapper for SunSoft/FreeBSD/MacOX/uclibc libm
RTAI - Real-Time Application Interface
Copyright (C) 2001   David A. Schleef <ds@schleef.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "rtapi_math.h"
#include "mathP.h"

_LIB_VERSION_TYPE _LIB_VERSION = _IEEE_;


MODULE_LICENSE("GPL");

int libm_errno;

int __xeno_math_init(void)
{
    printk(KERN_INFO "Xenomai math [xeno_math] loaded\n");
    return 0;
}

void __xeno_math_exit(void)
{
    printk(KERN_INFO "Xenomai math [xeno_math]: unloaded.\n");
}


// I have no idea why this is needed on precise

#if 0
double __ieee754_pow(double x, double y);
double pow(double x, double y)
{
    return  __ieee754_pow(x,y);
}
#endif


module_init(__xeno_math_init);
module_exit(__xeno_math_exit);

EXPORT_SYMBOL(sin);
EXPORT_SYMBOL(cos);
EXPORT_SYMBOL(tan);
EXPORT_SYMBOL(sqrt);
EXPORT_SYMBOL(fabs);
EXPORT_SYMBOL(atan);
EXPORT_SYMBOL(atan2);
EXPORT_SYMBOL(asin);
EXPORT_SYMBOL(acos);
EXPORT_SYMBOL(pow);

EXPORT_SYMBOL(ceil);
EXPORT_SYMBOL(floor);
EXPORT_SYMBOL(cbrt);
