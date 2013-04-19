#ifndef RTAPI_SUPPORT_H
#define RTAPI_SUPPORT_H

#include "rtapi.h"

RTAPI_BEGIN_DECLS

/***********************************************************************
*                      MESSAGING FUNCTIONS                             *
************************************************************************/
/* implemented in rtapi_support.c */

#include <stdarg.h>		/* va_start and va_end macros */

/** 'rtapi_snprintf()' works like 'snprintf()' from the normal
    C library, except that it may not handle long longs.
    It is provided here because some RTOS kernels don't provide
    a realtime safe version of the function, and those that do don't provide
    support for printing doubles.  On systems with a
    good kernel snprintf(), or in user space, this function
    simply calls the normal snprintf().  May be called from user,
    init/cleanup, and realtime code.
*/
extern int rtapi_snprintf(char *buf, unsigned long int size,
			   const char *fmt, ...)
    __attribute__((format(printf,3,4)));

/** 'rtapi_vsnprintf()' works like 'vsnprintf()' from the normal
    C library, except that it doesn't handle long longs.
    It is provided here because some RTOS kernels don't provide
    a realtime safe version of the function, and those that do don't provide
    support for printing doubles.  On systems with a
    good kernel vsnprintf(), or in user space, this function
    simply calls the normal vsnrintf().  May be called from user,
    init/cleanup, and realtime code.
*/
extern int rtapi_vsnprintf(char *buf, unsigned long size,
			    const char *fmt, va_list ap);

/** 'rtapi_print()' prints a printf style message.  Depending on the
    RTOS and whether the program is being compiled for user space
    or realtime, the message may be printed to stdout, stderr, or
    to a kernel message log, etc.  The calling syntax and format
    string is similar to printf except that floating point and
    longlongs are NOT supported in realtime and may not be supported
    in user space.  For some RTOS's, a 80 byte buffer is used, so the
    format line and arguments should not produce a line more than
    80 bytes long.  (The buffer is protected against overflow.)
    Does not block, but  can take a fairly long time, depending on
    the format string and OS.  May be called from user, init/cleanup,
    and realtime code.
*/
extern void rtapi_print(const char *fmt, ...)
    __attribute__((format(printf,1,2)));

/** 'rtapi_print_msg()' prints a printf-style message when the level
    is less than or equal to the current message level set by
    rtapi_set_msg_level().  May be called from user, init/cleanup,
    and realtime code.
*/
    typedef enum {
	RTAPI_MSG_NONE = 0,
	RTAPI_MSG_ERR,
	RTAPI_MSG_WARN,
	RTAPI_MSG_INFO,
	RTAPI_MSG_DBG,
	RTAPI_MSG_ALL
    } msg_level_t;

extern void rtapi_print_msg(int level, const char *fmt, ...)
    __attribute__((format(printf,2,3)));

/** Set the maximum level of message to print.  In userspace code,
    each component has its own independent message level.  In realtime
    code, all components share a single message level.  Returns 0 for
    success or -EINVAL if the level is out of range. */
extern int rtapi_set_msg_level(int level);

/** Retrieve the message level set by the last call to rtapi_set_msg_level */
extern int rtapi_get_msg_level(void);

/** 'rtapi_get_msg_handler' and 'rtapi_set_msg_handler' access the function
    pointer used by rtapi_print and rtapi_print_msg.  By default, messages
    appear in the kernel log, but by replacing the handler a user of the rtapi
    library can send the messages to another destination.  Calling
    rtapi_set_msg_handler with NULL restores the default handler. Call from
    real-time init/cleanup code only.  When called from rtapi_print(),
    'level' is RTAPI_MSG_ALL, a level which should not normally be used
    with rtapi_print_msg().
*/
typedef void(*rtapi_msg_handler_t)(msg_level_t level, const char *fmt,
				   va_list ap);

// message handler which writes to ringbuffer if global is available
extern void vs_ring_write(msg_level_t level, const char *format, va_list ap);

#ifdef RTAPI
#endif // RTAPI

typedef void (*rtapi_set_msg_handler_t)(rtapi_msg_handler_t);

extern void rtapi_set_msg_handler(rtapi_msg_handler_t handler);

typedef rtapi_msg_handler_t (*rtapi_get_msg_handler_t)(void);

extern rtapi_msg_handler_t rtapi_get_msg_handler(void);

extern int rtapi_set_logtag(const char *fmt, ...);

typedef enum {
	MSG_KERNEL = 0,
	MSG_RTUSER = 1,
	MSG_ULAPI = 2,
} msg_origin_t;

typedef enum {
    MSG_ASCII    = 0,  // printf conversion already applied
    MSG_STASHF   = 1,  // Jeff's stashf.c argument encoding
    MSG_PROTOBUF = 2,  // encoded as protobuf RTAPI_Message
} msg_encoding_t;

#define TAGSIZE 16

typedef struct {
    msg_origin_t   origin;   // where is this coming from
    int pid;                 // if User RT or ULAPI; 0 for kernel
    int level;               // as passed in to rtapi_print_msg()
    char tag[TAGSIZE];       // eg program or module name
    msg_encoding_t encoding; // how to interpret buf
    char buf[0];             // actual message
} rtapi_msgheader_t;

#define rtapi2syslog(level) (level+2)

#if defined(ULAPI) || defined(BUILD_SYS_USER_DSO)

/* make sure a given kernel module is loaded.
   might be needed for some usermode PCI drivers
*/
int rtapi_assure_module_loaded(const char *module);

extern long int simple_strtol(const char *nptr, char **endptr, int base);

// kernel tests in rtapi_compat.c
extern int kernel_is_xenomai();
extern int kernel_is_rtai();
extern int kernel_is_rtpreempt();

#endif 

enum flavor_flags {
    FLAVOR_DOES_IO=1,
    FLAVOR_KERNEL_BUILD=2,
};

//#ifdef ULAPI
typedef struct {
    const char *name;
    const char *mod_ext;
    const char *so_ext;
    int id;
    unsigned long flags;
    
} flavor_t, *flavor_ptr;

extern flavor_t flavors[];

extern flavor_ptr flavor_byname(const char *flavorname);
extern flavor_ptr flavor_byid(int flavor_id);
extern flavor_ptr default_flavor(void);

/*
 * given a flavor reference, result buffer of PATH__MAX size,
 * a basename and extension, find the path to a module/shared library
 * as follows:
 * if LIBPATH/FLAVORNAME/KERNELRELEASE/BASENAME.EXTENSION exists, succeed
 * else
 * if LIBPATH/FLAVORNAME/BASENAME.EXTENSION exists, succeed
 * else
 * if LIBPATH/BASENAME.EXTENSION exists, succeed
 * else fail
 *
 * With this scheme it is possible to support any number of flavors nad
 * kernel versions, and specifically override default choices in case of
 * problems:
 *
 * assume you have a xenomai-user build which has mostly flavor-dependent but
 * release-independent modules;
 * however, there is a HAL driver for kernel version foo which is not needed on
 * other kernel versions.
 * In this case, place modules like so:
 * 
 * LIBPATH/xenomai-user/    # standard modules go here
 * LIBPATH/xenomai-user/foo/hal_driver.so   # more specific driver goes here
 *
 */
extern int module_path(flavor_ptr f, 
		       char *result,
		       const char *libpath, 
		       const char *basename, 
		       const char *ext);
//#endif // ULAPI

RTAPI_END_DECLS

#endif /* RTAPI_SUPPORT_H */
