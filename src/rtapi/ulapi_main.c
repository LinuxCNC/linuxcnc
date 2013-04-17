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
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

global_data_t *get_global_handle(void)
{
    return global_data;
}

int ulapi_main(int instance, int flavor, global_data_t **global)
{
    int retval = 0;
    struct shm_status sm;

    page_size = sysconf(_SC_PAGESIZE);
    shmdrv_loaded  = shmdrv_available();

    if (global_data == NULL) {
	if (shmdrv_available()) {
	    sm.size = sizeof(global_data_t);
	    sm.flags = 0;
	    sm.key = OS_KEY(GLOBAL_KEY, instance);
	    sm.driver_fd = shmdrv_driver_fd();
	    retval = shmdrv_attach(&sm, (void **)global);
	    if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"global shmdrv attach failed %d\n", retval);
		return retval;
	    }
	} else {
	    //TBD
	    rtapi_print_msg(RTAPI_MSG_ERR,"global shmdrv attachNIY\n");
	    return -EINVAL;
	}
    }
    if (rtapi_data == NULL) {
	if (shmdrv_available()) {
	    sm.driver_fd = shmdrv_driver_fd();
	    sm.key = OS_KEY(RTAPI_KEY, instance);
	    sm.size = sizeof(rtapi_data_t);
	    sm.flags = 0;
	    retval = shmdrv_attach(&sm, (void **) &rtapi_data);
	    if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"rtapi shmdrv attach failed\n");
		return retval;
	    }
	} else {
	    //TBD
	    rtapi_print_msg(RTAPI_MSG_ERR,"global shmdrv attach NIY\n");
	    return -EINVAL;
	}
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI:%d startup RT msglevel=%d halsize=%d %s\n", 
		    rtapi_instance,
		    global_data->rt_msg_level,
		    global_data->hal_size,
		    GIT_VERSION);
    return 0;
}

int ulapi_exit(int instance)
{
    rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI:%d %s exit\n",
		    instance,
		    GIT_VERSION);
    return 0;
}

#endif // ULAPI
