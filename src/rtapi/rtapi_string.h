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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef RTAPI_STRING_H
#define RTAPI_STRING_H

#ifdef MODULE
/* Suspect only very early kernels are missing the basic string functions.
   To be sure, see what has been implemented by looking in linux/string.h
   and {linux_src_dir}/lib/string.c */
#include <linux/string.h>
#include <linux/version.h>
#define rtapi_argv_split argv_split
#define rtapi_argv_free argv_free
#define rtapi_kstrdup(a,b) kstrdup(a,b)
#else
#include <string.h>
#include <rtapi.h>
#include <rtapi_gfp.h>
RTAPI_BEGIN_DECLS
extern char **rtapi_argv_split(rtapi_gfp_t, const char *argstr, int *argc);
extern void rtapi_argv_free(char **argv);
#define rtapi_kstrdup(a,b) strdup(a)
RTAPI_END_DECLS
#endif

#endif
