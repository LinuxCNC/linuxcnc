/********************************************************************
* Description:  rtapi_support.c
*               This file, 'rtapi_support.c', implements the messaging
*               functions for both kernel and userland thread
*               systems.  See rtapi_support.h for more info.
*
*               Other than the rest of RTAPI, these functions are linked
*               into the instance module which is loaded before rtapi.so/ko
*               so they are available and message level set before
*               RTAPI starts up
********************************************************************/

#define USE_SYSLOG // FIXME
#define USE_MESSAGE_RING

#include "config.h"
#include "rtapi.h"
#include "rtapi_support.h"

#define RTPRINTBUFFERLEN 1024

#ifdef MODULE
#include "rtapi_app.h"

#    include <stdarg.h>		/* va_* */
#    include <linux/kernel.h>	/* kernel's vsnprintf */

static int rt_msg_level = RTAPI_MSG_INFO; // RT space
#define MSG_ORIGIN MSG_KERNEL
#else  /* user land */
#include <stdio.h>		/* libc's vsnprintf() */
#include <sys/types.h>
#include <unistd.h>

#ifdef USE_SYSLOG
#include <syslog.h>
#endif

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

// most RT systems use printk()
#ifndef RTAPI_PRINTK
#define RTAPI_PRINTK printk
#endif

#if defined(USE_MESSAGE_RING) 


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
	strcpy(msg->tag, "notyet");

	n = vsnprintf(msg->buf, RTPRINTBUFFERLEN, format, ap);
	// commit write
	rtapi_record_write_end(&rtapi_message_buffer, (void *) msg,
			       sizeof(rtapi_msgheader_t) + n);
	rtapi_mutex_give(&rtapi_message_buffer.header->wmutex);
    }
}
#endif

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
#ifdef USE_MESSAGE_RING
    vs_ring_write(level, fmt, ap);
#endif
#ifdef USE_SYSLOG
    vsyslog(rtapi2syslog(level), fmt, ap);
#else
    if (rtapi_get_msg_level() == RTAPI_MSG_ALL)
	vfprintf(stdout, fmt, ap);
    else
	vfprintf(stderr, fmt, ap);
#endif

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
    // FIXME cleanup logging 
#ifdef USE_SYSLOG
#ifndef MODULE    
    setlogmask(LOG_UPTO (rtapi2syslog(level)));
#endif
#endif
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

    if ((level <= rtapi_get_msg_level()) && (rtapi_get_msg_level() != RTAPI_MSG_NONE)) {
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

#ifdef MODULE

int rtapi_openlog(const char *tag, int level) 
{
    return 0;
}

int rtapi_closelog(void)
{    
    return 0;
}

#else

int rtapi_closelog(void)
{
#ifdef USE_SYSLOG
    closelog();
#endif
    return 0;
}

int rtapi_openlog(const char *tag, int level) 
{
#ifdef USE_SYSLOG
    int option = LOG_PID | LOG_NDELAY;
    if (level > RTAPI_MSG_INFO)
	option |= LOG_PERROR;
    openlog (tag, option , LOG_LOCAL1);
#endif
    return 0;
}
#endif

EXPORT_SYMBOL(rtapi_get_msg_handler);
EXPORT_SYMBOL(rtapi_set_msg_handler);
EXPORT_SYMBOL(rtapi_print_msg);
EXPORT_SYMBOL(rtapi_print);
EXPORT_SYMBOL(rtapi_snprintf);
EXPORT_SYMBOL(rtapi_vsnprintf);
EXPORT_SYMBOL(rtapi_openlog);
EXPORT_SYMBOL(rtapi_closelog);
EXPORT_SYMBOL(rtapi_set_msg_level);
EXPORT_SYMBOL(rtapi_get_msg_level);
