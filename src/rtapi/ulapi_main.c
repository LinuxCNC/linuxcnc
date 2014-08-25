/********************************************************************
* Description:  ulapi_main.c
*
*               purpose:
*               - attach to the rtapi data segment within the ULAPI
*                 symbol namespace
*               - pass in the global_data shm pointer
*               - set the rtapi_instance variable
*
* Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
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

#if defined(ULAPI)

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_global.h"       /* global_data_t */
#include "rtapi_common.h"
#include "rtapi_compat.h"
#include "rtapi/shmdrv/shmdrv.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>          //shm_open
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int ulapi_main(int instance, int flavor, global_data_t *global)
{
    int retval = 0;
    int rtapikey;
    int size = 0;

    rtapi_instance = instance; // from here on global within ulapi.so

    // shm_common_init(); // common shared memory API needs this

    // the HAL library constructor already has the global
    // shm segment attached, so no need to do it again here
    // since we're not using the rtapi_app_init()/rtapi_app_exit()
    // calling conventions might as well pass it it

    // this sets global_data for use within ulapi.so which
    // has a disjoint symbol namespace from hal_lib
    global_data = global;

    rtapi_print_msg(RTAPI_MSG_DBG,"ULAPI:%d %s %s init\n",
		    rtapi_instance,
		    rtapi_get_handle()->thread_flavor_name,
		    GIT_VERSION);


    if (rtapi_switch->thread_flavor_flags & FLAVOR_RTAPI_DATA_IN_SHM) {

	rtapikey = OS_KEY(RTAPI_KEY, rtapi_instance);

	// attach to existing RTAPI segment
	// not all thread flavors actuall might use it
	if ((retval = shm_common_new(rtapikey, &size,
				     rtapi_instance, (void **) &rtapi_data, 0))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "ULAPI:%d ERROR: cannot attach rtapi"
			    " segment key=0x%x %s\n",
			    rtapi_instance, rtapikey, strerror(-retval));
	}
	if (size != sizeof(rtapi_data_t)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "ULAPI:%d ERROR: unexpected rtapi shm size:"
			    " expected: %zu actual: %d\n",
			    rtapi_instance, sizeof(rtapi_data_t), size);
	    return -EINVAL;
	}

	if (MMAP_OK(global_data) && MMAP_OK(rtapi_data)) {
	    rtapi_print_msg(RTAPI_MSG_DBG,
			    "ULAPI:%d msglevel=%d/%d halsize=%d"
			    " %s startup %s\n",
			    rtapi_instance,
			    global_data->rt_msg_level,
			    global_data->user_msg_level,
			    global_data->hal_size,
			    GIT_VERSION, retval ? "FAILED" : "OK");
	} else {
	    rtapi_print_msg(RTAPI_MSG_DBG,
			    "ULAPI:%d init failed, realtime not running?"
			    " global=%p rtapi=%p\n",
			    rtapi_instance, global_data, rtapi_data);
	}
    }
    return retval;
}

int ulapi_exit(int instance)
{
    rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI:%d %s exit\n",
		    instance,
		    GIT_VERSION);

    if (rtapi_switch->thread_flavor_flags & FLAVOR_RTAPI_DATA_IN_SHM) {
	// detach RTAPI segment
	int retval = shm_common_detach(sizeof(rtapi_data_t), rtapi_data);
	if (retval) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "ULAPI:%d ERROR: shm_common_detach(rtapi_data)"
			    " failed: %s\n",
			    rtapi_instance,  strerror(-retval));
	}
	rtapi_data = NULL;
    }
    return 0;
}

#endif // ULAPI
