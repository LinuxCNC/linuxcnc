//    Copyright 2006-2013 Various Authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"
#include "rtapi_compat.h"

#ifndef MODULE
#include <stdlib.h>		/* strtol() */
#endif

#if defined(BUILD_SYS_KBUILD) && defined(ULAPI)
#include <stdio.h>		/* putchar */
#endif


/* these pointers are initialized at startup to point
   to resource data in the master data structure above
   all access to the data structure should uses these
   pointers, they take into account the mapping of
   shared memory into either kernel or user space.
   (the RTAPI kernel module and each ULAPI user process
   has its own set of these vars, initialized to match
   that process's memory mapping.)
*/

#ifdef BUILD_SYS_USER_DSO
// in the userland threads scenario, there is no point in having this 
// in shared memory, so keep it here
static rtapi_data_t local_rtapi_data;
rtapi_data_t *rtapi_data = &local_rtapi_data;
task_data *task_array =  local_rtapi_data.task_array;
shmem_data *shmem_array = local_rtapi_data.shmem_array;
module_data *module_array = local_rtapi_data.module_array;
#else
rtapi_data_t *rtapi_data = NULL;
task_data *task_array = NULL;
shmem_data *shmem_array = NULL;
module_data *module_array = NULL;
#endif

// RTAPI:
// global_data is exported by rtapi_module.c (kthreads)
// or rtapi_main.c (uthreads)
// ULAPI: exported in ulapi_autoload.c
extern global_data_t *global_data;

/* 
   define the rtapi_switch struct, with pointers to all rtapi_*
   functions

   ULAPI doesn't define all functions, so for missing functions point
   to the dummy function _rtapi_dummy() in hopes of more graceful
   failure
*/

int _rtapi_dummy(void) {
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "Error:  _rtapi_dummy function called from rtapi_switch; "
		    "this should never happen!");
    return -EINVAL;
}

static rtapi_switch_t rtapi_switch_struct = {
    .git_version = GIT_VERSION,
    .thread_flavor_name = THREAD_FLAVOR_NAME,
    .thread_flavor_id = THREAD_FLAVOR_ID,
    .thread_flavor_flags = FLAVOR_FLAGS,

    // init & exit functions
    .rtapi_init = &_rtapi_init,
    .rtapi_exit = &_rtapi_exit,
    .rtapi_next_handle = &_rtapi_next_handle,
    // messaging functions moved to instance,
    // implemented in rtapi_support.c
    // time functions
#ifdef RTAPI
    .rtapi_clock_set_period = &_rtapi_clock_set_period,
    .rtapi_delay = &_rtapi_delay,
    .rtapi_delay_max = &_rtapi_delay_max,
#else
    .rtapi_clock_set_period = &_rtapi_dummy,
    .rtapi_delay = &_rtapi_dummy,
    .rtapi_delay_max = &_rtapi_dummy,
#endif
    .rtapi_get_time = &_rtapi_get_time,
    .rtapi_get_clocks = &_rtapi_get_clocks,
    // task functions
    .rtapi_prio_highest = &_rtapi_prio_highest,
    .rtapi_prio_lowest = &_rtapi_prio_lowest,
    .rtapi_prio_next_higher = &_rtapi_prio_next_higher,
    .rtapi_prio_next_lower = &_rtapi_prio_next_lower,
#ifdef RTAPI
    .rtapi_task_new = &_rtapi_task_new,
    .rtapi_task_delete = &_rtapi_task_delete,
    .rtapi_task_start = &_rtapi_task_start,
    .rtapi_wait = &_rtapi_wait,
    .rtapi_task_resume = &_rtapi_task_resume,
    .rtapi_task_pause = &_rtapi_task_pause,
    .rtapi_task_self = &_rtapi_task_self,
#else
    .rtapi_task_new = &_rtapi_dummy,
    .rtapi_task_delete = &_rtapi_dummy,
    .rtapi_task_start = &_rtapi_dummy,
    .rtapi_wait = &_rtapi_dummy,
    .rtapi_task_resume = &_rtapi_dummy,
    .rtapi_task_pause = &_rtapi_dummy,
    .rtapi_task_self = &_rtapi_dummy,
#endif
    // shared memory functions
    .rtapi_shmem_new = &_rtapi_shmem_new,
    .rtapi_shmem_new_inst = &_rtapi_shmem_new_inst,

    .rtapi_shmem_delete = &_rtapi_shmem_delete,
    .rtapi_shmem_delete_inst = &_rtapi_shmem_delete_inst,

    .rtapi_shmem_getptr = &_rtapi_shmem_getptr,
    .rtapi_shmem_getptr_inst = &_rtapi_shmem_getptr_inst,
    .rtapi_shmem_exists = &_rtapi_shmem_exists,

#ifdef RTAPI
    .rtapi_set_exception = &_rtapi_set_exception,
#else
    .rtapi_set_exception = &_rtapi_dummy,
#endif
#ifdef RTAPI
    .rtapi_task_update_stats = &_rtapi_task_update_stats,
#else
    .rtapi_task_update_stats = &_rtapi_dummy,
#endif
};

// any API, any style:
rtapi_switch_t *rtapi_get_handle(void) {
    return &rtapi_switch_struct;
}
#ifdef RTAPI
EXPORT_SYMBOL(rtapi_get_handle);
#endif

#if defined(BUILD_SYS_KBUILD)
int shmdrv_loaded = 1;  // implicit
#else
int shmdrv_loaded;  // set in rtapi_app_main, and ulapi_main
#endif
long page_size;     // set in rtapi_app_main

void rtapi_autorelease_mutex(void *variable)
{
    if (rtapi_data != NULL)
	rtapi_mutex_give(&(rtapi_data->mutex));
    else 
	// programming error
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_autorelease_mutex: rtapi_data == NULL!\n");
}

// in the RTAPI scenario,
// global_data is exported by instance.ko and referenced
// by rtapi.ko and hal_lib.ko

// in ULAPI, we have only hal_lib which calls 'down'
// onto ulapi.so to init, so in this case global_data
// is exported by hal_lib and referenced by ulapi.so

extern global_data_t *global_data;


/* global init code */

void init_rtapi_data(rtapi_data_t * data)
{
    int n, m;

    /* has the block already been initialized? */
    if (data->magic == RTAPI_MAGIC) {
	/* yes, nothing to do */
	return;
    }
    /* no, we need to init it, grab mutex unconditionally */
    rtapi_mutex_try(&(data->mutex));
    /* set magic number so nobody else init's the block */
    data->magic = RTAPI_MAGIC;
    /* set version code and flavor ID so other modules can check it */
    data->serial = RTAPI_SERIAL;
    data->thread_flavor_id = THREAD_FLAVOR_ID;
    data->ring_mutex = 0;
    /* and get busy */
    data->rt_module_count = 0;
    data->ul_module_count = 0;
    data->task_count = 0;
    data->shmem_count = 0;
    data->timer_running = 0;
    data->timer_period = 0;
    /* init the arrays */
    for (n = 0; n <= RTAPI_MAX_MODULES; n++) {
	data->module_array[n].state = EMPTY;
	data->module_array[n].name[0] = '\0';
    }
    for (n = 0; n <= RTAPI_MAX_TASKS; n++) {
	data->task_array[n].state = EMPTY;
	data->task_array[n].prio = 0;
	data->task_array[n].owner = 0;
	data->task_array[n].taskcode = NULL;
	data->task_array[n].cpu = -1;   // use default
    }
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	data->shmem_array[n].key = 0;
	data->shmem_array[n].rtusers = 0;
	data->shmem_array[n].ulusers = 0;
	data->shmem_array[n].size = 0;
	for (m = 0; m < RTAPI_BITMAP_SIZE(RTAPI_MAX_SHMEMS +1); m++) {
	    data->shmem_array[n].bitmap[m] = 0;
	}
    }

    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return;
}
/***********************************************************************
*                           RT Thread statistics collection            *
*
* Thread statistics are recorded in the global_data_t thread_status.
* array. Values therein are updated when:
*
* - an exception happens, and it is safe to do so
* - by an explicit call to rtapi_task_update_stats() from an RT thread
*
* This avoids the overhead of permanently updating thread status, while
* giving the user the option to track thread status from a HAL component
* thread function if so desired.
*
* Updating thread status is necessarily a flavor-dependent
* operation and hence goes through a hook.
*
* Inspecting thread status (e.g. in halcmd) needs to evaluate
* the thread status information based on the current flavor.
************************************************************************/

#ifdef RTAPI
int _rtapi_task_update_stats(void)
{
#ifdef HAVE_RTAPI_TASK_UPDATE_STATS_HOOK
    extern int _rtapi_task_update_stats_hook(void);

    return _rtapi_task_update_stats_hook();
#else
    return -ENOSYS;  // not implemented in this flavor
#endif
}
#endif
/***********************************************************************
*                           RT exception handling                      *
*
*  all exceptions are funneled through a common exception handler
*  the default exception handler is defined in rtapi_exception.c but
*  may be redefined by a user-defined handler.
*
*  NB: the exception handler executes in a restricted context like
*  an in-kernel trap, or a signal handler. Limit processing in the
*  handler to an absolute minimum, and watch the stack size.
************************************************************************/

#ifdef RTAPI
// not available in ULAPI
extern rtapi_exception_handler_t rt_exception_handler;

// may override default exception handler
// returns the current handler so it might be restored
// does NOT go through rtapi_switch (!)
rtapi_exception_handler_t _rtapi_set_exception(rtapi_exception_handler_t h)
{
    rtapi_exception_handler_t previous = rt_exception_handler;
    rt_exception_handler = h;
    return previous;
}
#endif

// defined and initialized in rtapi_module.c (kthreads), rtapi_main.c (userthreads)
extern ringbuffer_t rtapi_message_buffer;   // error ring access strcuture

int  _rtapi_next_handle(void)
{
    return rtapi_add_and_fetch(1, &global_data->next_handle);
}

/* simple_strtol defined in
   /usr/src/kernels/<kversion>/include/linux/kernel.h */
#ifndef MODULE
long int simple_strtol(const char *nptr, char **endptr, int base) {
# ifdef HAVE_RTAPI_SIMPLE_STRTOL_HOOK
    return rtapi_simple_strtol_hook(nptr,endptr,base);
# else
    return strtol(nptr, endptr, base);
# endif
}
#ifdef RTAPI
EXPORT_SYMBOL(simple_strtol);
#endif
#endif

#if defined(BUILD_SYS_KBUILD) && defined(ULAPI)
/*  This function is disabled everywhere...  */
void _rtapi_printall(void) {
    module_data *modules;
    task_data *tasks;
    shmem_data *shmems;
    int n, m;

    if (rtapi_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_DBG, "rtapi_data = NULL, not initialized\n");
	return;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "rtapi_data = %p\n",
		    rtapi_data);
    rtapi_print_msg(RTAPI_MSG_DBG, "  magic = %d\n",
		    rtapi_data->magic);
    rtapi_print_msg(RTAPI_MSG_DBG, "  serial = %d\n",
		    rtapi_data->serial);
    rtapi_print_msg(RTAPI_MSG_DBG, "  thread_flavor id = %d\n",
		    rtapi_data->thread_flavor_id);
    rtapi_print_msg(RTAPI_MSG_DBG, "  mutex = %lu\n",
		    rtapi_data->mutex);
    rtapi_print_msg(RTAPI_MSG_DBG, "  rt_module_count = %d\n",
		    rtapi_data->rt_module_count);
    rtapi_print_msg(RTAPI_MSG_DBG, "  ul_module_count = %d\n",
		    rtapi_data->ul_module_count);
    rtapi_print_msg(RTAPI_MSG_DBG, "  task_count  = %d\n",
		    rtapi_data->task_count);
    rtapi_print_msg(RTAPI_MSG_DBG, "  shmem_count = %d\n",
		    rtapi_data->shmem_count);
    rtapi_print_msg(RTAPI_MSG_DBG, "  timer_running = %d\n",
		    rtapi_data->timer_running);
    rtapi_print_msg(RTAPI_MSG_DBG, "  timer_period  = %ld\n",
		    rtapi_data->timer_period);
    modules = &(rtapi_data->module_array[0]);
    tasks = &(rtapi_data->task_array[0]);
    shmems = &(rtapi_data->shmem_array[0]);
    rtapi_print_msg(RTAPI_MSG_DBG, "  module array = %p\n",modules);
    rtapi_print_msg(RTAPI_MSG_DBG, "  task array   = %p\n", tasks);
    rtapi_print_msg(RTAPI_MSG_DBG, "  shmem array  = %p\n", shmems);
    for (n = 0; n <= RTAPI_MAX_MODULES; n++) {
	if (modules[n].state != NO_MODULE) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "  module %02d\n", n);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    state = %d\n",
			    modules[n].state);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    name = %p\n",
			    modules[n].name);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    name = '%s'\n",
			    modules[n].name);
	}
    }
    for (n = 0; n <= RTAPI_MAX_TASKS; n++) {
	if (tasks[n].state != EMPTY) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "  task %02d\n", n);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    state = %d\n",
			    tasks[n].state);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    prio  = %d\n",
			    tasks[n].prio);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    owner = %d\n",
			    tasks[n].owner);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    code  = %p\n",
			    tasks[n].taskcode);
	}
    }
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmems[n].key != 0) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "  shmem %02d\n", n);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    key     = %d\n",
			    shmems[n].key);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    rtusers = %d\n",
			    shmems[n].rtusers);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    ulusers = %d\n",
			    shmems[n].ulusers);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    size    = %ld\n",
			    shmems[n].size);
	    rtapi_print_msg(RTAPI_MSG_DBG, "    bitmap  = ");
	    for (m = 0; m <= RTAPI_MAX_MODULES; m++) {
		if (rtapi_test_bit(m, shmems[n].bitmap)) {
		    putchar('1');
		} else {
		    putchar('0');
		}
	    }
	    putchar('\n');
	}
    }
}
#endif
