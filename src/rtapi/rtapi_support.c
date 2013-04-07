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

// TODO: ringbuffer in global

#include "config.h"
#include "rtapi.h"
#include "rtapi_support.h"

#ifdef MODULE
#include "rtapi_app.h"

#    include <stdarg.h>		/* va_* */
#    include <linux/kernel.h>	/* kernel's vsnprintf */
#    define RTPRINTBUFFERLEN 1024
static int rt_msg_level = RTAPI_MSG_INFO; // RT space
#else  /* user land */
#    include <stdio.h>		/* libc's vsnprintf() */

#ifdef USE_SYSLOG
#include <syslog.h>
#endif

#ifdef RTAPI // for now, fixme
#define USE_ERROR_RING
#endif


static int pp_msg_level = RTAPI_MSG_INFO; // per process only
#endif

// most RT systems use printk()
#ifndef RTAPI_PRINTK
#define RTAPI_PRINTK printk
#endif

#if defined(USE_ERROR_RING) 

extern ringbuffer_t error_buffer; 
void error_ring_write(const char *buf)
{
   if (global_data) {
	if (rtapi_mutex_try(&error_buffer.header->wmutex)) {
	    global_data->error_ring_locked++;
	    return;
	}
	if (rtapi_record_write(&error_buffer, (void *) buf, strlen(buf)))
	    global_data->error_ring_full++;
	rtapi_mutex_give(&error_buffer.header->wmutex);
    }
}
#endif

#ifdef MODULE
void default_rtapi_msg_handler(msg_level_t level, const char *fmt,
			      va_list ap) {
    char buf[RTPRINTBUFFERLEN];
    vsnprintf(buf, RTPRINTBUFFERLEN, fmt, ap);
    RTAPI_PRINTK(buf);
#ifdef USE_ERROR_RING
    error_ring_write(buf);
#endif
}

#else /* user land */
void default_rtapi_msg_handler(msg_level_t level, const char *fmt,
			       va_list ap) {
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

int rtapi_closelog()
{    
    return 0;
}

#else

int rtapi_closelog()
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
