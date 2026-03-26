//    Copyright 2014 Jeff Epler
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
#ifndef RTAPI_IO_H
#define RTAPI_IO_H

#include <rtapi.h>

#if defined(__i386) || defined(__x86_64)
#include <sys/io.h>
#endif

#if defined(__i386) || defined(__x86_64)
#define rtapi_inb inb
#define rtapi_inw inw
#define rtapi_inl inl

#define rtapi_outb outb
#define rtapi_outw outw
#define rtapi_outl outl
#define rtapi_ioperm ioperm
#else
#define rtapi_inb(x) ((void)(x), 0)
#define rtapi_inw(x) ((void)(x), 0)
#define rtapi_inl(x) ((void)(x), 0)
#define rtapi_outb(x,y) ((void)(x), (void)(y))
#define rtapi_outw(x,y) ((void)(x), (void)(y))
#define rtapi_outl(x,y) ((void)(x), (void)(y))
#define rtapi_ioperm(x,y,z) ((void)(x), (void)(y), (void)(z))
#endif

#endif
