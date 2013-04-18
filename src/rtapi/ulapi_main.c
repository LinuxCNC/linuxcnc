/********************************************************************
* Description:  instance.c
*
*               the purpose is to allocate and init the
*               instance data segment, or attach to it if it exists
*               the latter is possible only with userland styles
*               because kernel styles do not allow attaching a shm
*               which was created in userland
*
*               In RTAPI
*               a module to be loaded before rtapi.so/ko
*
*               Unload of the module unmaps the shm segment. It is
*               the last action of a RTAPI session shutdown.
*
*               In ULAPI:
*               Linked into ulapi.so without module headers, it
*               just provides functions for instance initialisation
*               to be used by hal_lib
*
********************************************************************/

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_global.h"       /* global_data_t */
#include "rtapi_common.h"
#include "rtapi/shmdrv/shmdrv.h"

#if defined(ULAPI)
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>          //shm_open
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

global_data_t *get_global_handle(void)
{
    return global_data;
}

int ulapi_main(int instance, int flavor, global_data_t **global)
{
    int retval = 0;
    int globalkey = OS_KEY(GLOBAL_KEY, rtapi_instance);
    int rtapikey = OS_KEY(RTAPI_KEY, rtapi_instance);

    page_size = sysconf(_SC_PAGESIZE);
    shmdrv_loaded  = shmdrv_available();

    rtapi_print_msg(RTAPI_MSG_DBG,"ULAPI:%d %s %s init\n",
		    rtapi_instance,
		    rtapi_get_handle()->thread_flavor_name,
		    GIT_VERSION);

    retval = shm_common_new(globalkey, sizeof(global_data_t), 
			    rtapi_instance, (void **) &global_data, 0);
     if (retval < 0) {
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "ULAPI:%d ERROR: cannot attach global segment key=0x%x %s\n",
			 rtapi_instance, globalkey, strerror(-retval));
	 goto done;
    }
    retval = shm_common_new(rtapikey, sizeof(rtapi_data_t), 
			    rtapi_instance, (void **) &rtapi_data, 0);

    if (retval < 0) {
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "ULAPI:%d ERROR: cannot attach rtapi segment key=0x%x %s\n",
			 rtapi_instance, rtapikey, strerror(-retval));
    }

 done:
    if (MMAP_OK(global_data) && MMAP_OK(rtapi_data)) {
	rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI:%d msglevel=%d/%d halsize=%d %s startup %s\n", 
			rtapi_instance,
			global_data->rt_msg_level,
			global_data->user_msg_level,
			global_data->hal_size,
			GIT_VERSION, retval ? "FAILED" : "OK");
    } else {
	rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI:%d init failed, realtime not running? global=%p rtapi=%p\n", 
			rtapi_instance, global_data, rtapi_data);
    }
    return retval;
}

int ulapi_exit(int instance)
{
    rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI:%d %s exit\n",
		    instance,
		    GIT_VERSION);
    return 0;
}

#endif // ULAPI
