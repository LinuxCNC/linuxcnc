/********************************************************************
* Description:  rtapi_support.c
*               This file, 'rtapi_support.c', implements the messaging
*               functions for both kernel and userland thread
*               systems.  See rtapi.h for more info.
*
*               Other than the rest of RTAPI, these functions are linked
*               into the instance module which is loaded before rtapi.so/ko
*               so they are available and message level set before
*               RTAPI starts up
********************************************************************/


#include "config.h"
#include "rtapi.h"
#include "rtapi/shmdrv/shmdrv.h"

#define RTPRINTBUFFERLEN 1024

#ifdef MODULE
#include "rtapi_app.h"

#include <stdarg.h>		/* va_* */
#include <linux/kernel.h>	/* kernel's vsnprintf */

static int rt_msg_level = RTAPI_MSG_INFO; // RT space
#define MSG_ORIGIN MSG_KERNEL

#else  /* user land */

#include <stdio.h>		/* libc's vsnprintf() */
#include <sys/types.h>
#include <unistd.h>

#ifdef RTAPI
#define MSG_ORIGIN MSG_RTUSER
#else
#define MSG_ORIGIN MSG_ULAPI
#endif

static int pp_msg_level = RTAPI_MSG_INFO; // per process only
#endif

#ifdef ULAPI
ringbuffer_t rtapi_message_buffer;   // rtapi_message ring access strcuture
# else
extern ringbuffer_t rtapi_message_buffer; // instance.c
#endif

static char logtag[TAGSIZE];
// most RT systems use printk()
#ifndef RTAPI_PRINTK
#define RTAPI_PRINTK printk
#endif

// candidate for rtapi_ring.h
void vs_ring_write(msg_level_t level, const char *format, va_list ap)
{
    int n;
    rtapi_msgheader_t *msg;

    if (global_data) {
	// one-time initialisation
	if (!rtapi_message_buffer.header) {
	    rtapi_ringbuffer_init(&global_data->rtapi_messages, &rtapi_message_buffer);

	}
	if (rtapi_mutex_try(&rtapi_message_buffer.header->wmutex)) {
	    global_data->error_ring_locked++;
	    return;
	}
	// zero-copy write
	// reserve space in ring:
	if (rtapi_record_write_begin(&rtapi_message_buffer, 
				     (void **) &msg, 
				     sizeof(rtapi_msgheader_t) + RTPRINTBUFFERLEN)) {
	    global_data->error_ring_full++;
	    rtapi_mutex_give(&rtapi_message_buffer.header->wmutex);
	    return;
	}
	msg->origin = MSG_ORIGIN;
#if defined(RTAPI) && defined(BUILD_SYS_KBUILD)
	msg->pid = 0;
#else
	msg->pid  = getpid(); // FIXME is this rt-safe?
#endif
	msg->level = level;
	msg->encoding = MSG_ASCII;
	strncpy(msg->tag, logtag, sizeof(msg->tag));

	n = vsnprintf(msg->buf, RTPRINTBUFFERLEN, format, ap);
	// commit write
	rtapi_record_write_end(&rtapi_message_buffer, (void *) msg,
			       sizeof(rtapi_msgheader_t) + n + 1); // trailing zero
	rtapi_mutex_give(&rtapi_message_buffer.header->wmutex);
    }
}

#ifdef MODULE
void default_rtapi_msg_handler(msg_level_t level, const char *fmt,
			      va_list ap) {
    char buf[RTPRINTBUFFERLEN];
    vsnprintf(buf, RTPRINTBUFFERLEN, fmt, ap);
    RTAPI_PRINTK(buf);
#ifdef USE_MESSAGE_RING
    vs_ring_write(level, buf, ap);
#endif
}

#else /* user land */
void default_rtapi_msg_handler(msg_level_t level, const char *fmt,
			       va_list ap) {
    // during startup the global segment might not be
    // available yet, so use stderr until then
    if (MMAP_OK(global_data)) {
	vs_ring_write(level, fmt, ap);
    } else {
	vfprintf(stderr, fmt, ap);
    }
}
#endif

static rtapi_msg_handler_t rtapi_msg_handler = default_rtapi_msg_handler;

rtapi_msg_handler_t rtapi_get_msg_handler(void) {
    return rtapi_msg_handler;
}

void rtapi_set_msg_handler(rtapi_msg_handler_t handler) {
    if (handler == NULL)
	rtapi_msg_handler = default_rtapi_msg_handler;
    else
	rtapi_msg_handler = handler;
}


// rtapi_get_msg_level and rtapi_set_msg_level moved here
// since they access the global segment 
// which might not exist during first use
// assure we can use message levels before global_data is set up

static int get_msg_level(void)
{
#if MODULE
    if (global_data == 0)
	return rt_msg_level;
    else
	return global_data->rt_msg_level;
#else
    return pp_msg_level;
#endif
}

static int set_msg_level(int new_level)
{
    int old_level;

#if MODULE
    if (global_data) {
	old_level = global_data->rt_msg_level;
	global_data->rt_msg_level = new_level;
    } else {
	old_level = rt_msg_level;
	rt_msg_level = new_level;
    }
    return old_level;
#else
    old_level = pp_msg_level;
    pp_msg_level = new_level;
    return old_level;
#endif
}

int rtapi_set_msg_level(int level) {
    int oldlevel;
    if ((level < RTAPI_MSG_NONE) || (level > RTAPI_MSG_ALL)) {
	return -EINVAL;
    }
    oldlevel = set_msg_level(level);
    return oldlevel;
}

int rtapi_get_msg_level() {
    return get_msg_level();
}

void rtapi_print(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    rtapi_msg_handler(RTAPI_MSG_ALL, fmt, args);
    va_end(args);
}

void rtapi_print_msg(int level, const char *fmt, ...) {
    va_list args;

    if ((level <= rtapi_get_msg_level()) && 
	(rtapi_get_msg_level() != RTAPI_MSG_NONE)) {
	va_start(args, fmt);
	rtapi_msg_handler(level, fmt, args);
	va_end(args);
    }
}

int rtapi_snprintf(char *buf, unsigned long int size,
		   const char *fmt, ...) {
    va_list args;
    int result;

    va_start(args, fmt);
    result = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return result;
}

int rtapi_vsnprintf(char *buf, unsigned long int size, const char *fmt,
		    va_list ap) {
    return vsnprintf(buf, size, fmt, ap);
}

int rtapi_set_logtag(const char *fmt, ...) {
    va_list args;
    int result;

    va_start(args, fmt);
    result = vsnprintf(logtag, sizeof(logtag), fmt, args);
    va_end(args);
    return result;
}

#ifdef RTAPI
EXPORT_SYMBOL(rtapi_get_msg_handler);
EXPORT_SYMBOL(rtapi_set_msg_handler);
EXPORT_SYMBOL(rtapi_print_msg);
EXPORT_SYMBOL(rtapi_print);
EXPORT_SYMBOL(rtapi_snprintf);
EXPORT_SYMBOL(rtapi_vsnprintf);
EXPORT_SYMBOL(rtapi_set_msg_level);
EXPORT_SYMBOL(rtapi_get_msg_level);
EXPORT_SYMBOL(rtapi_set_logtag);
#endif
