/********************************************************************
* Description:  rtapi_io.h
*
*               This file, 'rtapi_io.h', implements the i/o- related
*               functions for realtime modules as a series of static
*               inline functions.
*
*     Copyright 2006-2013 Various Authors
* 
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; either version 2 of the License, or
*     (at your option) any later version.
* 
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
* 
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
********************************************************************/
#ifndef RTAPI_IO_H
#define RTAPI_IO_H

#define RTAPI_IO		// for disabling parts of header files

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions

#ifdef MODULE
#  if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,17)
#    include <asm/io.h>		/* inb(), outb() */
#  else
#    include <sys/io.h>		/* inb(), outb() */
#  endif
#else
#  include <sys/io.h>		/* inb(), outb() */
#endif


#ifdef RTAPI_IO

/** 'rtapi_outb() writes 'byte' to 'port'.  May be called from
    init/cleanup code, and from within realtime tasks.
    Note: This function does nothing on the simulated RTOS.
*/
static inline void rtapi_outb(unsigned char byte, unsigned int port)
{
    outb(byte,port);
}

/** 'rtapi_inb() gets a byte from 'port'.  Returns the byte.  May
    be called from init/cleanup code, and from within realtime tasks.
    Note: This function always returns zero on the simulated RTOS.
    Note: Many platforms provide an inline inb() that is faster.
*/
static inline unsigned char rtapi_inb(unsigned int port)
{
    return inb(port);
}

/** 'rtapi_outw() writes 'word' to 'port'.  May be called from
    init/cleanup code, and from within realtime tasks.
    Note: This function does nothing on the simulated RTOS.
*/
static inline void rtapi_outw(unsigned short word, unsigned int port)
{
    outw(word,port);
};

/** 'rtapi_inw() gets a word from 'port'.  Returns the word.  May
    be called from init/cleanup code, and from within realtime tasks.
    Note: This function always returns zero on the simulated RTOS.
*/
static inline unsigned short rtapi_inw(unsigned int port)
{
    return inw(port);
}

#else

// noops
static inline void rtapi_outb(unsigned char byte, unsigned int port) { return; }
static inline unsigned char rtapi_inb_hook(unsigned int port)        { return 0; };
static inline void rtapi_outw(unsigned short word, unsigned int port){ return; }
static inline unsigned short rtapi_inw(unsigned int port)  { return 0; };

#endif


#endif // RTAPI_IO_H
