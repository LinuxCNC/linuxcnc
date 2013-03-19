/********************************************************************
* Description:  rtapi_io.c
*
*               This file, 'rtapi_io.c', implements the i/o- related
*               functions for realtime modules.  See rtapi.h for more
*               info.
********************************************************************/

#define RTAPI_IO		// for disabling parts of header files

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// RTAPI macros and decls

#ifdef MODULE
#  if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,17)
#    include <asm/io.h>		/* inb(), outb() */
#  else
#    include <sys/io.h>		/* inb(), outb() */
#  endif
#else
#  include <sys/io.h>		/* inb(), outb() */
#endif


#ifdef HAVE_RTAPI_OUTB_HOOK
extern inline void rtapi_outb_hook(unsigned char byte, unsigned int port);

void _rtapi_outb(unsigned char byte, unsigned int port) {
    rtapi_outb_hook(byte,port);
}

#else  /* default:  use outb() */
void _rtapi_outb(unsigned char byte, unsigned int port) {
    outb(byte,port);
}
#endif


#ifdef HAVE_RTAPI_INB_HOOK
extern inline unsigned char rtapi_inb_hook(unsigned int port);

unsigned char _rtapi_inb(unsigned int port) {
    return rtapi_inb_hook(port);
}

#else  /* default:  use inb() */
unsigned char _rtapi_inb(unsigned int port) {
    return inb(port);
}
#endif


#ifdef HAVE_RTAPI_OUTW_HOOK
extern inline void rtapi_outw_hook(unsigned short word, unsigned int port);

void _rtapi_outw(unsigned short word, unsigned int port) {
    rtapi_outw_hook(word,port);
}

#else  /* default:  use outw() */
void _rtapi_outw(unsigned short word, unsigned int port) {
    outw(word,port);
}
#endif


#ifdef HAVE_RTAPI_INW_HOOK
extern inline unsigned short rtapi_inw_hook(unsigned int port);

unsigned short _rtapi_inw(unsigned int port) {
    return rtapi_inw_hook(port);
}

#else /* default:  use inw() */
unsigned short _rtapi_inw(unsigned int port) {
    return inw(port);
}
#endif

