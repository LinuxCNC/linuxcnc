/*
   wrapper for rtapi_math kernel module exported functions

   Copyright (C) 2013 Michael Haberler <license@mah.priv.at>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

int libm_errno;

MODULE_LICENSE("GPL");

int __rtapi_math_init(void)
{
    printk(KERN_INFO "Xenomai math [rtapi_math] loaded\n");
    return 0;
}

void __rtapi_math_exit(void)
{
    printk(KERN_INFO "Xenomai math [rtapi_math]: unloaded.\n");
}


// I have no idea why this is needed on precise

#if 0
double __ieee754_pow(double x, double y);
double rtapi_pow(double x, double y)
{
    return  __ieee754_pow(x,y);
}
#endif


module_init(__rtapi_math_init);
module_exit(__rtapi_math_exit);

EXPORT_SYMBOL(rtapi_sin);
EXPORT_SYMBOL(rtapi_cos);
EXPORT_SYMBOL(rtapi_tan);
EXPORT_SYMBOL(rtapi_sqrt);
EXPORT_SYMBOL(rtapi_fabs);
EXPORT_SYMBOL(rtapi_atan);
EXPORT_SYMBOL(rtapi_atan2);
EXPORT_SYMBOL(rtapi_asin);
EXPORT_SYMBOL(rtapi_acos);
EXPORT_SYMBOL(rtapi_pow);
EXPORT_SYMBOL(rtapi_fmin);
EXPORT_SYMBOL(rtapi_fmax);
EXPORT_SYMBOL(rtapi_fmod);
EXPORT_SYMBOL(rtapi_hypot);
EXPORT_SYMBOL(rtapi_rint);
EXPORT_SYMBOL(rtapi_scalbn);
EXPORT_SYMBOL(rtapi_finite);
EXPORT_SYMBOL(rtapi_copysign);

EXPORT_SYMBOL(rtapi_ceil);
EXPORT_SYMBOL(rtapi_floor);
EXPORT_SYMBOL(rtapi_cbrt);


EXPORT_SYMBOL(__powidf2);
