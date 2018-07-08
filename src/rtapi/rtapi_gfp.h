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
#ifndef RTAPI_GFP_H
#define RTAPI_GFP_H

#ifdef __KERNEL__
#include <linux/gfp.h>

// types
#define rtapi_gpf_e gpf_e
#define rtapi_gpf_t gpf_t

// enumerated values
#define RTAPI_GFP_BUFFER GFP_BUFFER
#define RTAPI_GFP_ATOMIC GFP_ATOMIC
#define RTAPI_GFP_KERNEL GFP_KERNEL
#define RTAPI_GFP_USER GFP_USER
#define RTAPI_GFP_NOBUFFER GFP_NOBUFFER
#define RTAPI_GFP_NFS GFP_NFS
#define RTAPI_GFP_DMA GFP_DMA

#else

enum rtapi_gfp_e {
    RTAPI_GFP_BUFFER,
    RTAPI_GFP_ATOMIC,
    RTAPI_GFP_KERNEL,
    RTAPI_GFP_USER,
    RTAPI_GFP_NOBUFFER,
    RTAPI_GFP_NFS,
    RTAPI_GFP_DMA
};

typedef unsigned long rtapi_gfp_t;



#endif
#endif
