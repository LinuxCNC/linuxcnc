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
#include "rtapi_common.h"
#include "rtapi_support.h"
#include "rtapi/shmdrv/shmdrv.h"

// #include <stdlib.h>
#include <unistd.h>		/* ssize_t, _SC_PAGESIZE */
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h> 

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
static int shmdrv_loaded;
static long page_size;

#define PAGESIZE_ALIGN(x)  ((x) + (-(x) & (page_size - 1)))

global_data_t *get_global_handle(void)
{
    return global_data;
}
EXPORT_SYMBOL(get_global_handle);

int rtapi_app_main(void)
{
    struct shm_status sm;
    int retval, compatible;

    page_size = sysconf(_SC_PAGESIZE);

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d %s %s init\n",
		    rtapi_instance,
		    rtapi_get_handle()->thread_flavor_name,
		    GIT_VERSION);

    shmdrv_loaded  = shmdrv_available();

    if (shmdrv_loaded) {
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
    } else {
	char segment_name[LINELEN];
	sprintf(segment_name, "0x%8.8x",OS_KEY(GLOBAL_KEY, rtapi_instance));
	int global_fd = shm_open(segment_name, 
				 (O_CREAT | O_EXCL | O_RDWR),
				 (S_IREAD | S_IWRITE));
	if (global_fd < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: cant shm_open(%s) - instance already started?: %s\n",
			    rtapi_instance, segment_name, strerror(-errno));
	    return -errno;
	}
	ftruncate(global_fd, sizeof(global_data_t));
	if((global_data = mmap(0, sizeof(global_data_t), (PROT_READ | PROT_WRITE),
			       MAP_SHARED, global_fd, 0)) == MAP_FAILED) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: mmap(%s) failed: %s\n",
			    rtapi_instance, segment_name, strerror(-errno));
	    return -errno;
	}
	sprintf(segment_name, "0x%8.8x",OS_KEY(RTAPI_KEY, rtapi_instance));
	int rtapi_fd = shm_open(segment_name,
				(O_CREAT | O_EXCL | O_RDWR),
				(S_IREAD | S_IWRITE));
	if (rtapi_fd < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: cant shm_open(%s) - instance already started?: %s\n",
			    rtapi_instance, segment_name, strerror(-errno));
	    return -errno;
	}
	ftruncate(rtapi_fd, sizeof(rtapi_data_t));
	if((rtapi_data = mmap(0, sizeof(rtapi_data_t), (PROT_READ | PROT_WRITE),
			      MAP_SHARED, rtapi_fd, 0)) == MAP_FAILED) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: mmap(%s) failed: %s\n",
			    rtapi_instance, segment_name, strerror(-errno));
	    return -errno;
	}
    }
    /* perform a global init */
    init_global_data(global_data, rtapi_instance,
		     hal_size, rt_msg_level, user_msg_level, 
		     instance_name);
    init_rtapi_data(rtapi_data);

    compatible = check_compatible();
    if (!compatible) {
	if (shmdrv_loaded) {
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
	} else {
	    retval = 0;
	    if (munmap(global_data, PAGESIZE_ALIGN(sizeof(global_data_t)))) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"RTAPI:%d ERROR: munmap(global_data) failed: %s\n",
				rtapi_instance, strerror(-errno));
		retval = -errno;
		global_data = NULL;
	    }
	    if (munmap(rtapi_data, PAGESIZE_ALIGN(sizeof(rtapi_data_t)))) {
		rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: munmap(rtapi_data) failed: %s\n",
				rtapi_instance, strerror(-errno));
		retval = -errno;
		rtapi_data = NULL;
	    }
	    return retval;
	}
    }
    return 0;
}

void rtapi_app_exit(void)
{
    struct shm_status sm;
    int retval;

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d exit\n",
		    rtapi_instance);

    if (shmdrv_loaded) {
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
    } else {
	if (munmap(global_data, PAGESIZE_ALIGN(sizeof(global_data_t)))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: munmap(global_data) failed: %s\n",
			    rtapi_instance, strerror(-errno));
	    global_data = NULL;
	}
	if (munmap(rtapi_data, PAGESIZE_ALIGN(sizeof(rtapi_data_t)))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: munmap(rtapi_data) failed: %s\n",
			    rtapi_instance, strerror(-errno));
	    rtapi_data = NULL;
	}
    }
}

EXPORT_SYMBOL(real_rtapi_app_main);
EXPORT_SYMBOL(real_rtapi_app_exit);

static int check_compatible()
{
    int retval = 1;
    const char *flavor_name = rtapi_get_handle()->thread_flavor_name;

    if (flavor_id == RTAPI_POSIX_ID)
	return 1; // no prerequisites

    if (kernel_is_xenomai() &&
	(flavor_id != RTAPI_XENOMAI_USER_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on a Xenomai kernel\n",
			rtapi_instance,flavor_name);
	return 0;
    }

    if (kernel_is_rtai() &&
	(flavor_id != RTAPI_RTAI_KERNEL_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on an RTAI kernel\n",
			rtapi_instance,flavor_name);
	return 0;
    }
    if (kernel_is_rtpreempt() &&
	(flavor_id != RTAPI_RT_PREEMPT_USER_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on an RT PREEMPT kernel\n",
			rtapi_instance,flavor_name);
	return 0;
    }
    return retval;
}

