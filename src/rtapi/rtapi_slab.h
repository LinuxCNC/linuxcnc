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
#ifndef RTAPI_SLAB_H
#define RTAPI_SLAB_H

#include "rtapi_gfp.h"

#ifdef __KERNEL__
#include <linux/slab.h>

#define rtapi_kfree kfree
#define rtapi_kmalloc kmalloc
#define rtapi_krealloc krealloc
#define rtapi_kzalloc kzalloc

#else
#include <stdlib.h>

#define rtapi_kfree free
#define rtapi_kmalloc(sz, flags) malloc((sz))
#define rtapi_kzalloc(sz, flags) calloc(1,(sz))
#define rtapi_krealloc(p, sz, flags) realloc((p), (sz))

#endif
#endif
