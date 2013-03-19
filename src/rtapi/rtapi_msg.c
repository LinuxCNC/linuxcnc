/********************************************************************
* Description:  rtapi_msg.c
*               This file, 'rtapi_msg.c', implements the messaging
*               functions for both kernel and userland thread
*               systems.  See rtapi.h for more info.
********************************************************************/

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#ifdef MODULE
#    include <stdarg.h>		/* va_* */
#    include <linux/kernel.h>	/* kernel's vsnprintf */
#    define RTPRINTBUFFERLEN 1024
#else  /* user land */
#    include <stdio.h>		/* libc's vsnprintf() */
#endif

int msg_level = RTAPI_MSG_INFO;	/* message printing level */ //XXX
#ifdef MODULE
RTAPI_MP_INT(msg_level, "debug message level (default=1)");
#endif

// most RT systems use printk()
#ifndef RTAPI_PRINTK
#define RTAPI_PRINTK printk
#endif


#ifdef MODULE
void default_rtapi_msg_handler(msg_level_t level, const char *fmt,
			      va_list ap) {
    char buf[RTPRINTBUFFERLEN];
    vsnprintf(buf, RTPRINTBUFFERLEN, fmt, ap);
    RTAPI_PRINTK(buf);
}

#else /* user land */
void default_rtapi_msg_handler(msg_level_t level, const char *fmt,
			       va_list ap) {
    if(level == RTAPI_MSG_ALL)
	vfprintf(stdout, fmt, ap);
    else
	vfprintf(stderr, fmt, ap);
}
#endif

static rtapi_msg_handler_t rtapi_msg_handler = default_rtapi_msg_handler;

#ifdef RTAPI

rtapi_msg_handler_t _rtapi_get_msg_handler(void) {
    return rtapi_msg_handler;
}

void _rtapi_set_msg_handler(rtapi_msg_handler_t handler) {
    if (handler == NULL)
	rtapi_msg_handler = default_rtapi_msg_handler;
    else
	rtapi_msg_handler = handler;
}
#endif  /* RTAPI */

void _rtapi_print(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    rtapi_msg_handler(RTAPI_MSG_ALL, fmt, args);
    va_end(args);
}


void _rtapi_print_msg(int level, const char *fmt, ...) {
    va_list args;

    if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
	va_start(args, fmt);
	rtapi_msg_handler(level, fmt, args);
	va_end(args);
    }
}

int _rtapi_snprintf(char *buf, unsigned long int size,
		   const char *fmt, ...) {
    va_list args;
    int result;

    va_start(args, fmt);
    result = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return result;
}

int _rtapi_vsnprintf(char *buf, unsigned long int size, const char *fmt,
		    va_list ap) {
    return vsnprintf(buf, size, fmt, ap);
}

int _rtapi_set_msg_level(int level) {
    if ((level < RTAPI_MSG_NONE) || (level > RTAPI_MSG_ALL)) {
	return -EINVAL;
    }
    msg_level = level;
    return 0;
}

int _rtapi_get_msg_level() {
    return msg_level;
}

