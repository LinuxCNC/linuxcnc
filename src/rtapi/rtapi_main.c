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
#include "rtapi.h"
#include "rtapi_support.h"
#include "rtapi/shmdrv/shmdrv.h"

#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <stdlib.h>		/* rand_r() */
#include <unistd.h>		/* getuid(), getgid(), sysconf(),
				   ssize_t, _SC_PAGESIZE */

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "rtapi_common.h"       /* global_data_t */
#include "rtapi_kdetect.h"      /* environment autodetection */

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("RTAPI module support - userland threads");
MODULE_LICENSE("GPL");

int rtapi_instance;
static char *instance_name = "";
static int hal_size = HAL_SIZE;                 // default size of HAL data segment
static int rt_msg_level = RTAPI_MSG_INFO;	/* initial RT message printing level */ 
static int user_msg_level = RTAPI_MSG_INFO;	/* initial User message printing level */ 

RTAPI_MP_STRING(instance_name,"name of this instance")
RTAPI_MP_INT(rtapi_instance, "instance ID");
RTAPI_MP_INT(hal_size, "size of the HAL data segment");
RTAPI_MP_INT(rt_msg_level, "RT debug message level (default=3)");
RTAPI_MP_INT(user_msg_level, "user debug message level (default=3)");
EXPORT_SYMBOL(rtapi_instance);
EXPORT_SYMBOL(global_data);

global_data_t *global_data = NULL;
ringbuffer_t rtapi_message_buffer;   // error ring access strcuture

static int flavor_id = THREAD_FLAVOR_ID;
static int check_compatible();

global_data_t *get_global_handle(void)
{
    return global_data;
}
EXPORT_SYMBOL(get_global_handle);

int rtapi_app_main(void)
{
    struct shm_status sm;
    int retval, compatible;

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d %s %s init\n",
		    rtapi_instance,
		    rtapi_get_handle()->thread_flavor_name,
		    GIT_VERSION);

    sm.driver_fd = shmdrv_driver_fd();
    sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
    sm.size = sizeof(global_data_t);
    sm.flags = 0;
    if ((retval = shmdrv_create(&sm)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: can create global segment: %d\n",
			rtapi_instance, retval);
	return -EINVAL;
    }
    if ((retval = shmdrv_attach(&sm, (void **)&global_data)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: can attach global segment: %d\n",
			rtapi_instance, retval);
	return -EINVAL;
    }

    sm.driver_fd = shmdrv_driver_fd();
    sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
    sm.size = sizeof(rtapi_data_t);
    sm.flags = 0;
    if ((retval = shmdrv_create(&sm)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: can create rtapi segment: %d\n",
			rtapi_instance, retval);
	return -EINVAL;
    }
    if ((retval = shmdrv_attach(&sm, (void **)&rtapi_data)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: cant attach rtapi segment: %d\n",
			rtapi_instance, retval);
	return -EINVAL;
    }

    /* perform a global init */
    init_global_data(global_data, rtapi_instance,
		     hal_size, rt_msg_level, user_msg_level, 
		     instance_name);
    init_rtapi_data(rtapi_data);

    compatible = check_compatible();
    if (!compatible) {
	// detach and fail
	sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
	sm.size = sizeof(global_data_t);
	sm.flags = 0;
	if ((retval = shmdrv_detach(&sm, rtapi_data)) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: shmdrv_detach(rtapi_data) returns %d\n",
			    rtapi_instance, retval);
	}
	rtapi_data = NULL;
	sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
	sm.size = sizeof(global_data_t);
	sm.flags = 0;
	if ((retval = shmdrv_detach(&sm, global_data)) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "INSTANCE:%d ERROR: shmdrv_detach() returns %d\n",
			    rtapi_instance, retval);
	}
	global_data = NULL;
	return -EINVAL;
    }
}

void rtapi_app_exit(void)
{
    struct shm_status sm;
    int retval;

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d exit\n",
		    rtapi_instance);

    sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
    sm.size = sizeof(global_data_t);
    sm.flags = 0;
    if ((retval = shmdrv_detach(&sm, rtapi_data)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: shmdrv_detach(rtapi_data) returns %d\n",
			rtapi_instance, retval);
    }
    rtapi_data = NULL;
    sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
    sm.size = sizeof(global_data_t);
    sm.flags = 0;
    if ((retval = shmdrv_detach(&sm, global_data)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"INSTANCE:%d ERROR: shmdrv_detach(global_data) returns %d\n",
			rtapi_instance, retval);
    }
    global_data = NULL;
}

EXPORT_SYMBOL(real_rtapi_app_main);
EXPORT_SYMBOL(real_rtapi_app_exit);

static int check_compatible()
{
    int retval = 0;
    const char *flavor_name = rtapi_get_handle()->thread_flavor_name;

    if (flavor_id == RTAPI_POSIX_ID)
	return 1; // no prerequisites

    if (kernel_is_xenomai() &&
	(flavor_id != RTAPI_XENOMAI_USER_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on a Xenomai kernel\n",
			flavor_name,
			rtapi_instance);
	return 0;
    }

    if (kernel_is_rtai() &&
	(flavor_id != RTAPI_RTAI_KERNEL_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on an RTAI kernel\n",
			flavor_name,
			rtapi_instance);
	retval--;
    }
    if (kernel_is_rtpreempt() &&
	(flavor_id != RTAPI_RT_PREEMPT_USER_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on an RT PREEMPT kernel\n",
			flavor_name,
			rtapi_instance);
	retval--;
    }
    return retval;
}

