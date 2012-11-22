/********************************************************************
* Description:  rtapi_io.c
*
*               This file, 'rtapi_io.c', implements the i/o- related
*               functions for realtime modules.  See rtapi.h for more
*               info.
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// RTAPI macros and decls

#ifdef MODULE
#include <linux/module.h>	// EXPORT_SYMBOL
#else
#include <sys/io.h>		/* inb(), outb(), inw(), outw() */
#include <stdlib.h>		/* strtol() */
#endif

void rtapi_outb(unsigned char byte, unsigned int port) {
#ifdef HAVE_RTAPI_OUTB_HOOK
    rtapi_outb_hook(byte,port);
#endif
}

unsigned char rtapi_inb(unsigned int port)
{
#ifdef HAVE_RTAPI_INB_HOOK
    return rtapi_inb_hook(port);
#else
    return 0;
#endif
}

void rtapi_outw(unsigned short word, unsigned int port)
{
#ifdef HAVE_RTAPI_OUTW_HOOK
    rtapi_outw_hook(word,port);
#endif
}

unsigned short rtapi_inw(unsigned int port)
{
#ifdef HAVE_RTAPI_INW_HOOK
    return rtapi_inw_hook(port);
#else
    return inw(port);
#endif
}

// this goes somewhere else
long int simple_strtol(const char *nptr, char **endptr, int base) {
#ifdef HAVE_RTAPI_SIMPLE_STRTOL_HOOK
    return rtapi_simple_strtol_hook(nptr,endptr,base);
#else
    return strtol(nptr, endptr, base);
#endif
}
