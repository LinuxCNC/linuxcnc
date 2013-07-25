/********************************************************************
* Description:  rtapi_io.h
*
*               This file, 'rtapi_io.h', implements the i/o- related
*               functions for realtime modules as a series of static
*               inline functions.
********************************************************************/
#ifndef RTAPI_IO_H
#define RTAPI_IO_H

#define RTAPI_IO		// for disabling parts of header files

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
// #include "rtapi_common.h"	// RTAPI macros and decls

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
    Note: Many platforms provide an inline outb() that is faster.
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
    Note: Many platforms provide an inline outw() that is faster.
*/
static inline void rtapi_outw(unsigned short word, unsigned int port)
{
    outw(word,port);
};

/** 'rtapi_inw() gets a word from 'port'.  Returns the word.  May
    be called from init/cleanup code, and from within realtime tasks.
    Note: This function always returns zero on the simulated RTOS.
    Note: Many platforms provide an inline inw() that is faster.
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
