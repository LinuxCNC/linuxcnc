/********************************************************************
* Description:  rtapi_main.c
*
*               This file, 'rtapi_main.c', implements the RTAPI
*               rtapi_app_main() and rtapi_app_exit() functions
*               for userspace thread systems.
*
*               It should not be used for kernel thread systems.
*
********************************************************************/

#include "config.h"

#include "rtapi_support.h"

#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <stdlib.h>		/* rand_r() */
#include <unistd.h>		/* getuid(), getgid(), sysconf(),
				   ssize_t, _SC_PAGESIZE */

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "rtapi_common.h"       /* global_data_t */
#include "rtapi_kdetect.h"      /* environment autodetection */

// set in instance
extern int rtapi_instance;

#ifdef RTAPI
/* MODULE_AUTHOR("Michael Haberler"); */
/* MODULE_DESCRIPTION("RTAPI stubs for userland threadstyles"); */
/* MODULE_LICENSE("GPL2 or later"); */

/* // XXX FIXME: move to global sizing include */
/* static int hal_size = 262000; // HAL_SIZE */
/* RTAPI_MP_INT(hal_size, "size of the HAL data segment");
o */
#endif


static int check_compatible();

// int rtapi_app_main(void)
int real_rtapi_app_main(void)
{
    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d %s %s init\n",
		    rtapi_instance, 
		    rtapi_get_handle()->thread_flavor_name, 
		    GIT_VERSION);

    return check_compatible();
}

//void rtapi_app_exit(void)
void real_rtapi_app_exit(void)
{
    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d exit\n", 
	      rtapi_instance);
}

EXPORT_SYMBOL(real_rtapi_app_main);
EXPORT_SYMBOL(real_rtapi_app_exit);

#if !defined(THREAD_FLAVOR_ID)
#error "THREAD_FLAVOR_ID is not defined!"
#endif

#if THREAD_FLAVOR_ID == RTAPI_XENOMAI_USER_ID
static int check_compatible()
{
    int retval = 0;

    if (!kernel_is_xenomai()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		  "RTAPI:%d started Xenomai RTAPI on a non-Xenomai kernel\n",
		  rtapi_instance);
	retval--;
    }

    if (kernel_is_rtai()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started Xenomai RTAPI on an RTAI kernel\n",
			rtapi_instance);
	//retval--;
    }
    if (kernel_is_rtpreempt()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started Xenomai RTAPI on an RT PREEMPT kernel\n",
			rtapi_instance);
	retval--;
    }
    return retval;
}

#elif THREAD_FLAVOR_ID ==  RTAPI_RT_PREEMPT_USER_ID

static int check_compatible()
{
   int retval = 0;

    if (!kernel_is_rtpreempt()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started RT_PREEMPT RTAPI on a non-RT PREEMPT kernel\n",
			rtapi_instance);
	// retval--;
    }

    if (kernel_is_rtai()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		  "RTAPI:%d started RT_PREEMPT RTAPI on an RTAI kernel\n",
		  rtapi_instance);
	// retval--;
    }
    if (kernel_is_xenomai()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		  "RTAPI:%d started RT_PREEMPT RTAPI on a Xenomai kernel\n",
		  rtapi_instance);
	// retval--;
    }
    return retval;
}

#elif THREAD_FLAVOR_ID == RTAPI_POSIX_ID

static int check_compatible()
{
    return 0; // no prerequisites
}

#else

#error "THREAD_FLAVOR_ID not set"
#endif
