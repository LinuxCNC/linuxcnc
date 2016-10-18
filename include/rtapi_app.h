//    Copyright 2003-2007, various authors
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

#ifndef RTAPI_APP_H
#define RTAPI_APP_H

#include "config.h"
/*
  for Linux kernel modules, exactly one file needs to
  include <linux/module.h>. We put this in this header.
  If we ever support non-Linux platforms, this file will
  get full of ifdefs.
*/

#if defined(BUILD_SYS_USER_DSO)

#if defined(USERMODE_PCI)
#include "userpci/module.h"
#endif
/*  For kernel modules (hm2_pci, hostmot2) to compile in usermode without lots
    of changes, the EXPORT_SYMBOL lines, below, need to be defined *ONLY* if
    there is actually an rtapi_app_* function in the code.  This is handeled 
    in the kernel module case by the simple define that renames the module 
    init/cleanup functions.
    For the usermode case, turn the first instance of rtapi_app_* into a 
    function declaration, then export the symbol, then re-create the function 
    definition.  This way the symbol is only exported if the function exists 
    in the code, and we don't have to have the ability to 'rewind' the C 
    preprocessor.
 */
#define rtapi_app_main(a)           \
    rtapi_app_main(a);              \
    EXPORT_SYMBOL(rtapi_app_main);  \
    int rtapi_app_main(a)
#define rtapi_app_exit(a)           \
    rtapi_app_exit(a);              \
    EXPORT_SYMBOL(rtapi_app_exit);  \
    void rtapi_app_exit(a)
#else
#include <linux/module.h>
#define rtapi_app_main(a) init_module(a)
#define rtapi_app_exit(a) cleanup_module(a)
#endif

#endif /* RTAPI_APP_H */
