/********************************************************************
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


// ULAPI autoloading support
//
// this postpones binding to the flavor-specific ULAPI
// by intercepting the first call to rtapi_init()

// other than the previous code which assumed realtime is started
// and failed during loading of the HAL library, this code makes the
// HAL library load fine regardless of realtime started or stopped
// however, any calls to hal_init() (which cause a call to rtapi_init())
// will fail if realtime stopped; or succeed after loading the
// ulapi shared object for the current flavor.

// Michael Haberler 7/2013

#include <stdio.h>		// snprintf
#include <dlfcn.h>              // for dlopen/dlsym ulapi-$THREADSTYLE.so
#include <assert.h>
#include <limits.h>             // PATH_MAX
#include <stdlib.h>		// exit()

#include "rtapi.h"		// RTAPI realtime OS API
#include "rtapi/shmdrv/shmdrv.h" // common shared memory API
#include "rtapi_compat.h"	// flavor support

#ifdef ULAPI

static rtapi_switch_t dummy_ulapi_switch_struct;

// exported symbols
// since this is normal userland linking, not RT loading, no need to
// EXPORT_SYMBOL() any of those

rtapi_switch_t *rtapi_switch = &dummy_ulapi_switch_struct;
global_data_t *global_data;
int rtapi_instance;
flavor_ptr flavor;
// end exported symbols:

static void *ulapi_so; // dlopen handle for ULAPI .so
static char *ulapi_lib = "ulapi";

static ulapi_main_t ulapi_main_ref;
static ulapi_exit_t ulapi_exit_ref;

// use 'ULAPI_DEBUG=<level> <hal binary/Python>' to trace ulapi loading
static int ulapi_debug = RTAPI_MSG_NONE;

static int ulapi_load(rtapi_switch_t **ulapi_switch);

static int _ulapi_dummy(void) __attribute__((noreturn));

static int _ulapi_dummy(void) {
    // API abuse trap: rtapi_init() must be first, and succeed
    // for any other _subsequent_ rtapi_* calls to succeed

    rtapi_print_msg(RTAPI_MSG_ERR,
		    "Error:  _ulapi_dummy function called from rtapi_switch; "
		    "this should never happen!");
    assert("API Abuse - check your code" == NULL);
}

int _ulapi_init(const char *modname) {
    if (ulapi_load(&rtapi_switch) < 0) {
	return -ENOSYS;
    }
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "_ulapi_init(): ulapi %s %s loaded\n",
		    flavor->name, rtapi_switch->git_version);

    // switch logging level to what was set in global via msgd:
    rtapi_set_msg_level(global_data->user_msg_level);

    // and return what was intended to start with
    return rtapi_init(modname);
}

// NB: must be kept in sync with rtapi_switch_t in rtapi.h

static rtapi_switch_t dummy_ulapi_switch_struct = {
    .git_version = GIT_VERSION,
    .thread_flavor_name = RTAPI_NOTLOADED_NAME,
    .thread_flavor_id = RTAPI_NOTLOADED_ID,

    // the only "working" method is _ulapi_init()
    // which will detect and load the proper ulapi
    .rtapi_init = &_ulapi_init,

    // everything else is supposed to fail miserably
    // since it violates the API contract: rtapi_init()
    // must be first
    .rtapi_exit = (rtapi_exit_t) &_ulapi_dummy,
    .rtapi_next_handle = &_ulapi_dummy,

    .rtapi_clock_set_period = &_ulapi_dummy,
    .rtapi_delay = &_ulapi_dummy,
    .rtapi_delay_max = &_ulapi_dummy,

    .rtapi_get_time = (rtapi_get_time_t) &_ulapi_dummy,
    .rtapi_get_clocks = (rtapi_get_clocks_t) &_ulapi_dummy,

    .rtapi_prio_highest = &_ulapi_dummy,
    .rtapi_prio_lowest = &_ulapi_dummy,

    .rtapi_prio_next_higher =  (rtapi_prio_next_higher_lower_t) &_ulapi_dummy,
    .rtapi_prio_next_lower =  (rtapi_prio_next_higher_lower_t) &_ulapi_dummy,

    .rtapi_task_new = &_ulapi_dummy,
    .rtapi_task_delete = &_ulapi_dummy,
    .rtapi_task_start = &_ulapi_dummy,
    .rtapi_wait = &_ulapi_dummy,
    .rtapi_task_resume = &_ulapi_dummy,
    .rtapi_task_pause = &_ulapi_dummy,
    .rtapi_task_self = &_ulapi_dummy,

    .rtapi_shmem_new = (rtapi_shmem_new_t) &_ulapi_dummy,
    .rtapi_shmem_new_inst = (rtapi_shmem_new_inst_t) &_ulapi_dummy,

    .rtapi_shmem_delete = (rtapi_shmem_delete_t) &_ulapi_dummy,
    .rtapi_shmem_delete_inst =(rtapi_shmem_delete_inst_t)&_ulapi_dummy,

    .rtapi_shmem_getptr = (rtapi_shmem_getptr_t) &_ulapi_dummy,
    .rtapi_shmem_getptr_inst = (rtapi_shmem_getptr_inst_t) &_ulapi_dummy,

    .rtapi_set_exception = &_ulapi_dummy,
    .rtapi_task_update_stats = &_ulapi_dummy,
};


static int ulapi_load(rtapi_switch_t **ulapi_switch)
{
    int retval;
    const char *errmsg;
    rtapi_get_handle_t rtapi_get_handle;
    char ulapi_lib_fname[PATH_MAX];
    char *instance = getenv("INSTANCE");
    char *debug_env = getenv("ULAPI_DEBUG");
    int size = 0;
    int globalkey;

    // set the rtapi_instance global for this hal library instance
    if (instance != NULL)
	rtapi_instance = atoi(instance);

    if (debug_env)
	ulapi_debug = atoi(debug_env);

    rtapi_set_msg_level(ulapi_debug);

    // tag message origin field
    rtapi_set_logtag("ulapi");

    // first thing is to attach the global segment, based on
    // the RTAPI instance id. This will contain the flavor
    // this ULAPI HAL instance is to run with.

    // Also, it's the prerequisite for common error message
    // handling through the message ringbuffer; unless then
    // error messages will go to stderr.

    // the global segment is attached once here per ULAPI instance;
    // it's address is passed to the ulapi-<flavor>.so module once loaded.

    // init the common shared memory driver APU
    shm_common_init();

    globalkey = OS_KEY(GLOBAL_KEY, rtapi_instance);
    retval = shm_common_new(globalkey, &size,
			    rtapi_instance, (void **) &global_data, 0);

    if (retval == -ENOENT) {
	// the global_data segment does not exist. Happens if the realtime
	// script was not started
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ULAPI:%d ERROR: realtime not started\n",
			rtapi_instance);
	return retval;
    }

    if (retval < 0) {
	// some other error attaching global
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ULAPI:%d ERROR: shm_common_new() failed key=0x%x %s\n",
			rtapi_instance, globalkey, strerror(-retval));
	return retval;
    }

    if (size != sizeof(global_data_t)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ULAPI:%d ERROR: global segment size mismatch,"
			" expected: %zd, actual:%d\n",
		 rtapi_instance, sizeof(global_data_t), size);
	return -EINVAL;
    }

    if (global_data->magic != GLOBAL_READY) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ULAPI:%d ERROR: global segment invalid magic:"
			" expected: 0x%x, actual: 0x%x\n",
			rtapi_instance, GLOBAL_READY,
			global_data->magic);
	return -EINVAL;
    }

    // global data set up ok

    // obtain handle on flavor descriptor as detected by rtapi_msgd
    flavor = flavor_byid(global_data->rtapi_thread_flavor);
    if (flavor == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d BUG - invalid flavor id: %d\n",
			rtapi_instance, global_data->rtapi_thread_flavor);
	return -EINVAL;
    }

    snprintf(ulapi_lib_fname,PATH_MAX,"%s/%s-%s%s",
	     EMC2_RTLIB_DIR, ulapi_lib, flavor->name, flavor->so_ext);

    // dynload the proper ulapi.so:
    if ((ulapi_so = dlopen(ulapi_lib_fname, RTLD_GLOBAL|RTLD_NOW))  == NULL) {
	errmsg = dlerror();
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d FATAL - dlopen(%s) failed: %s\n",
			rtapi_instance, ulapi_lib_fname,
			errmsg ? errmsg : "NULL");
	return -ENOENT;
    }

    // resolve rtapi_switch getter function
    dlerror();
    if ((rtapi_get_handle = (rtapi_get_handle_t)
	 dlsym(ulapi_so, "rtapi_get_handle")) == NULL) {
	errmsg = dlerror();
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d FATAL - resolving %s: cant"
			" dlsym(rtapi_get_handle): %s\n",
			rtapi_instance, ulapi_lib, errmsg ? errmsg : "NULL");
	return -ENOENT;
    }

    assert(rtapi_get_handle != NULL);

    // this redirects calls to rtapi through the just-loaded ulapi
    *ulapi_switch = rtapi_get_handle();

    // from here on it is safe to call all RTAPI functions (i.e. including those
    // which go through rtapi_switch)

    // resolve main function
    dlerror();
    if ((ulapi_main_ref = (ulapi_main_t)
	 dlsym(ulapi_so, "ulapi_main")) == NULL) {
	errmsg = dlerror();
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d FATAL - resolving %s: "
			"cant dlsym(ulapi_main): %s\n",
			rtapi_instance, ulapi_lib, errmsg ? errmsg : "NULL");
	return -ENOENT;
    }
    // resolve exit function
    dlerror();
    if ((ulapi_exit_ref = (ulapi_exit_t)
	 dlsym(ulapi_so, "ulapi_exit")) == NULL) {
	errmsg = dlerror();
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB: FATAL - resolving %s:"
			" cant dlsym(ulapi_exit): %s\n",
		ulapi_lib, errmsg ? errmsg : "NULL");
	return -ENOENT;
    }

    assert(ulapi_main_ref != NULL);
    assert(ulapi_exit_ref != NULL);

    // call the ulapi init method, passing in the global segment
    if ((retval = ulapi_main_ref(rtapi_instance,
				 flavor->id, global_data)) < 0) {
	// check shmdrv, permissions
	rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL_LIB: FATAL - cannot attach to instance %d"
			" - realtime not started?\n",
		rtapi_instance);
	return -ENOENT;
    }

    // pretty bad - we loaded the wrong ulapi.so
    if (flavor->id != rtapi_switch->thread_flavor_id) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL_LIB: BUG: thread flavors disagree:"
			" hal_lib.c=%d rtapi=%d\n",
		flavor->id, rtapi_switch->thread_flavor_id);
    }

    // sanity check - may be harmless
    if (strcmp(GIT_VERSION, rtapi_switch->git_version)) {
	rtapi_print_msg(RTAPI_MSG_WARN,
			"HAL_LIB: UP API warning - git versions disagree:"
			" hal_lib.c=%s %s=%s\n",
			GIT_VERSION, ulapi_lib, rtapi_switch->git_version);
    }

    // declare victory
    return 0;
}

// determine if ulapi.so loaded
int ulapi_loaded(void) {
    return (rtapi_switch->thread_flavor_id != RTAPI_NOTLOADED_ID);
}

//  ULAPI cleanup. Call the exit handler and unload ulapi-<flavor>.so.
void ulapi_cleanup(void)
{
    // call the ulapi exit handler
    // detach the rtapi shm segment as needed
    // (some flavors do not employ an rtapi shm segment)
    if (ulapi_exit_ref) {
	ulapi_exit_ref(rtapi_instance);
	ulapi_exit_ref = NULL;
    }
    // NB: we do not detach the global segment

    // unload ulapi shared object.
    if (ulapi_so){
	dlclose(ulapi_so);
	ulapi_so = NULL;
    }
    // reset rtapi_switch to make the code
    // serially reusable
    rtapi_switch = &dummy_ulapi_switch_struct;
}

#endif // ULAPI
