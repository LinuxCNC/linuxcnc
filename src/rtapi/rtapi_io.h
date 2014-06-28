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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#ifndef RTAPI_IO_H
#define RTAPI_IO_H

#include <rtapi.h>

#ifdef __KERNEL__
#include <asm/io.h>
#else
#include <sys/io.h>
#endif

#define rtapi_inb inb
#define rtapi_inw inw
#define rtapi_inl inl

#define rtapi_outb outb
#define rtapi_outw outw
#define rtapi_outl outl

#endif
