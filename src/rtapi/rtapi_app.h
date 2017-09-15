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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef RTAPI_APP_H
#define RTAPI_APP_H

/*
  for Linux kernel modules, exactly one file needs to
  include <linux/module.h>. We put this in this header.
  If we ever support non-Linux platforms, this file will
  get full of ifdefs.
*/

#if !defined(__KERNEL__)
EXPORT_SYMBOL(rtapi_app_main);
EXPORT_SYMBOL(rtapi_app_exit);
#else
#include <linux/module.h>

#define rtapi_app_main(a) init_module(a)
#define rtapi_app_exit(a) cleanup_module(a)
#endif

#endif /* RTAPI_APP_H */
