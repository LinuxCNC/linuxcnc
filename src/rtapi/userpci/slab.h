/********************************************************************
 *  Description: slab.h
 *  Provides functions for running linux kernel modules in user space,
 *  emulating part of
 *  #include <linux/slab.h>
 *
 *  Author(s): Charles Steinkuehler
 *  License: GNU LGPL Version 2.1 or (at your option) any later version.
 *
 *  Last change: 
 *  2012-Dec-17 Charles Steinkuehler
 *              Initial version
 ********************************************************************/

/********************************************************************
 *  This file is part of LinuxCNC RTAPI / HAL
 *
 *  Copyright (C) 2012  Charles Steinkuehler
 *                      <charles AT steinkuehler DOT net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 *  ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 *  TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 *  harming persons must have provisions for completely removing power
 *  from all motors, etc, before persons enter any danger area.  All
 *  machinery must be designed to comply with local and national safety
 *  codes, and the authors of this software can not, and do not, take
 *  any responsibility for such compliance.
 *
 *  This code was written as part of the LinuxCNC RTAPI project.  For 
 *  more information, go to www.linuxcnc.org.
 ********************************************************************/

#ifndef _LINUX_SLAB_H
#define	_LINUX_SLAB_H

#include <stdlib.h>         // malloc/free
#include "gfp.h"

static inline void *kmalloc(size_t size, gfp_t flags)
{
    return malloc(size);
}

static inline void *krealloc(void *ptr, size_t size, gfp_t flags)
{
    return realloc(ptr, size);
}

static inline void kfree(void *obj)
{
    free(obj);
}

#endif	/* _LINUX_SLAB_H */
