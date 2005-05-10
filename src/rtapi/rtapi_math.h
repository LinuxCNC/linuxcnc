#ifndef RTAPI_MATH_H
#define RTAPI_MATH_H

/** RTAPI is a library providing a uniform API for several real time
  operating systems.  As of ver 2.0, RTLinux and RTAI are supported.
*/

/** This file, 'rtapi_math.h', contains code that allows RT modules
    to include <math.h> without any warnings during compile.
    The warnings come from a double defined symbol, which gets undefined
    here to remove the warning.
*/

/** Copyright (C) 2005 Alex Joni
                       <alex_joni AT users DOT sourceforge DOT net>

    Copyright (C) 2005 Paul Corner
                       <paul_c AT users DOT sourceforge DOT net>

    This library is based on version 1.0, which was released into
    the public domain by its author, Fred Proctor.  Thanks Fred!
*/

/* This library is free software; you can redistribute it and/or
  modify it under the terms of version 2.1 of the GNU Lesser General
  Public License as published by the Free Software Foundation.
  This library is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
*/

/** THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.
*/

/** This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include <linux/types.h>
#ifdef __attribute_used__
#undef __attribute_used__
#endif
#include <sys/cdefs.h>
#include <float.h>
#include <math.h>

#endif
