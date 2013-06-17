//    Copyright 2006-2009, Jeff Epler
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
#ifdef MODULE
/* Suspect only very early kernels are missing the basic string functions.
   To be sure, see what has been implemented by looking in linux/string.h
   and {linux_src_dir}/lib/string.c */
#include <linux/string.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#ifndef __HAVE_ARCH_STRCMP	/* This flag will be defined if we do */
#define __HAVE_ARCH_STRCMP	/* have strcmp */
/* some kernels don't have strcmp */
static int strcmp(const char *cs, const char *ct)
{
    signed char __res;
    while (1) {
	if ((__res = *cs - *ct++) != 0 || !*cs++) {
	    break;
	}
    }
    return __res;
}
#endif /* __HAVE_ARCH_STRCMP */
#endif /* linux 2.4 */
#else
#include <string.h>
#endif
