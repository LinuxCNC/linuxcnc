/********************************************************************
 * Description:  rtapi_main.c
 *
 *               This file, 'rtapi_main.c', implements the RTAPI
 *               rtapi_app_main() and rtapi_app_exit() functions
 *               for userspace thread systems.
 *
 *               It should not be used for kernel thread systems.
 *
 * Copyright (C) 2012, 2013 John Morris <john AT zultron DOT com>
 *                          Michael Haberler <license AT mah DOT priv DOT at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ********************************************************************/


#include <unistd.h>		/* ssize_t, _SC_PAGESIZE */
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "rtapi_common.h"       /* global_data_t */
#include "rtapi_compat.h"       /* global_data_t */
#include "rtapi/shmdrv/shmdrv.h"  /* common shm driver API */

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("RTAPI module support - userland threads");
MODULE_LICENSE("GPL");

// mostly for argument compatibility with kernel thread flavors
int rtapi_instance;                             // instance id, visible throughout RTAPI
RTAPI_MP_INT(rtapi_instance, "instance ID");
EXPORT_SYMBOL(rtapi_instance);

global_data_t *global_data = NULL;              // visible to all RTAPI modules
EXPORT_SYMBOL(global_data);

rtapi_switch_t *rtapi_switch  = NULL;
EXPORT_SYMBOL(rtapi_switch);

#ifdef HAVE_RTAPI_MODULE_INIT_HOOK
void _rtapi_module_init_hook(void);
#endif

#ifdef HAVE_RTAPI_MODULE_EXIT_HOOK
void _rtapi_module_exit_hook(void);
#endif

ringbuffer_t rtapi_message_buffer;   // error ring access strcuture

int rtapi_app_main(void)
{
    int retval;
    int globalkey = OS_KEY(GLOBAL_KEY, rtapi_instance);
    int rtapikey = OS_KEY(RTAPI_KEY, rtapi_instance);
    int size  = 0;

    rtapi_switch = rtapi_get_handle();
    shm_common_init();

    // tag messages originating from RT proper
    rtapi_set_logtag("rtapi");

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d  %s %s init\n",
		    rtapi_instance,
		    rtapi_switch->thread_flavor_name,
		    GIT_VERSION);

    // attach to global segment which rtapi_msgd owns and already
    // has set up:
    retval = shm_common_new(globalkey, &size,
			    rtapi_instance, (void **) &global_data, 0);

    if (retval ==  -ENOENT) {
	// the global_data segment does not exist.
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: global segment 0x%x does not exist"
			" (rtapi_msgd not started?)\n",
			rtapi_instance, globalkey);
	return -EBUSY;
    }
    if (retval < 0) {
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "RTAPI:%d ERROR: shm_common_new() failed key=0x%x %s\n",
			 rtapi_instance, globalkey, strerror(-retval));
	 return retval;
    }
    if (size != sizeof(global_data_t)) {
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "RTAPI:%d ERROR: unexpected global shm size:"
			 " expected: %zd actual:%d\n",
			 rtapi_instance, sizeof(global_data_t), size);
	 return -EINVAL;
    }

    // consistency check
    if (global_data->rtapi_thread_flavor != THREAD_FLAVOR_ID) {
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "RTAPI:%d BUG: thread flavors dont match:"
			 " global %d rtapi %d\n",
			 rtapi_instance, global_data->rtapi_thread_flavor,
			 THREAD_FLAVOR_ID);
	 return -EINVAL;
    }

    // good to use global_data from here on

    // make the message ringbuffer accessible
    ringbuffer_init(&global_data->rtapi_messages, &rtapi_message_buffer);
    rtapi_message_buffer.header->refcount++;


    // some flavors might use a shared memory segment for rtapi data. That
    // fact is recorded in rtapi_switch->flavor_flags

    if (rtapi_switch->thread_flavor_flags & FLAVOR_RTAPI_DATA_IN_SHM) {
	size = sizeof(rtapi_data_t);
	retval = shm_common_new(rtapikey, &size,
				rtapi_instance, (void **) &rtapi_data, 1);
	if (retval ==  0) {
	    // the rtapi_data segment already existed.
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: rtapi segment 0x%x already exists!\n",
			    rtapi_instance, rtapikey);
	    return -EBUSY;
	}
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: shm_common_new() failed key=0x%x %s\n",
			    rtapi_instance, rtapikey, strerror(-retval));
	    return retval;
	}
	if (size != sizeof(rtapi_data_t)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: unexpected rtapi shm size:"
			 " expected: %zd actual:%d\n",
			    rtapi_instance, sizeof(rtapi_data_t), size);
	    return -EINVAL;
	}
    }

    init_rtapi_data(rtapi_data);

#ifdef HAVE_RTAPI_MODULE_INIT_HOOK
    _rtapi_module_init_hook();
#endif
    return 0;
}

void rtapi_app_exit(void)
{
    int retval;

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d exit\n", rtapi_instance);

#ifdef HAVE_RTAPI_MODULE_EXIT_HOOK
    _rtapi_module_exit_hook();
#endif

    rtapi_message_buffer.header->refcount--;

    if (rtapi_switch->thread_flavor_flags & FLAVOR_RTAPI_DATA_IN_SHM) {

	if ((retval = shm_common_detach(sizeof(rtapi_data_t), rtapi_data))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: shm_common_detach(rtapi_data)"
			    " failed: %s\n",
			    rtapi_instance,  strerror(-retval));
	}
	shm_common_unlink(OS_KEY(RTAPI_KEY, rtapi_instance));
    }
    rtapi_data = NULL;
}
