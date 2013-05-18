
#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#ifdef BUILD_SYS_USER_DSO
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#endif

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
ring_data *ring_array = local_rtapi_data.ring_array;
#else
rtapi_data_t *rtapi_data = NULL;
task_data *task_array = NULL;
shmem_data *shmem_array = NULL;
module_data *module_array = NULL;
ring_data *ring_array = NULL;
#endif

// in the RTAPI scenario,  
// global_data is exported by instance.ko and referenced
// by rtapi.ko and hal_lib.ko

// in ULAPI, we have only hal_lib which calls 'down'
// onto ulapi.so to init, so in this case global_data
// is exported by hal_lib and referenced by ulapi.so

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
#if THREAD_FLAVOR_ID == RTAPI_XENOMAI_ID
    .thread_flavor_name = "xenomai",
#endif
#if THREAD_FLAVOR_ID ==  RTAPI_RT_PREEMPT_ID
    .thread_flavor_name = "rt-preempt",
#endif
#if THREAD_FLAVOR_ID == RTAPI_POSIX_ID
    .thread_flavor_name = "posix",
#endif
#if THREAD_FLAVOR_ID == RTAPI_RTAI_KERNEL_ID
    .thread_flavor_name = "rtai",
#endif
#if THREAD_FLAVOR_ID == RTAPI_XENOMAI_KERNEL_ID
    .thread_flavor_name = "xenomai-kernel",
#endif
    .thread_flavor_id = THREAD_FLAVOR_ID,
    // init & exit functions
    .rtapi_init = &_rtapi_init,
    .rtapi_exit = &_rtapi_exit,
    .rtapi_next_module_id = &_rtapi_next_module_id,
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

    // ringbuffer functions
    .rtapi_ring_new = &_rtapi_ring_new,
    .rtapi_ring_attach = &_rtapi_ring_attach,
    .rtapi_ring_detach = &_rtapi_ring_detach,
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
#ifdef HAVE_INIT_RTAPI_DATA_HOOK  // declare a prototype
void init_rtapi_data_hook(rtapi_data_t * data);
#endif

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
	for (m = 0; m < _BITS_TO_LONGS(RTAPI_MAX_SHMEMS +1); m++) {
	    data->shmem_array[n].bitmap[m] = 0;
	}
    }
    for (n = 0; n <= RTAPI_MAX_RINGS; n++) {
	data->ring_array[n].magic = 0;
	data->ring_array[n].key = 0;
	data->ring_array[n].owner = 0;
    }
#ifdef HAVE_INIT_RTAPI_DATA_HOOK
    init_rtapi_data_hook(data);
#endif

    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return;
}

// rtapi_module.c, rtapi_main.c
extern ringbuffer_t rtapi_message_buffer;   // error ring access strcuture

void init_global_data(global_data_t * data, 
		      int instance_id, int hal_size, 
		      int rt_level, int user_level,
		      const char *name)
{
    /* has the block already been initialized? */
    if (data->magic == GLOBAL_MAGIC) {
	/* yes, nothing to do */
	return;
    }
    /* no, we need to init it, grab mutex unconditionally */
    // 'locked' should never happen since this is the first module loaded
    // but you never know what people come up with
    rtapi_mutex_try(&(data->mutex));
    /* set magic number so nobody else init's the block */
    data->magic = GLOBAL_MAGIC;
    /* set version code so other modules can check it */
    data->layout_version = GLOBAL_LAYOUT_VERSION;

    data->instance_id = instance_id;

    // passed in from rtap.so/ko args
    if (strlen(name) == 0) {
	snprintf(data->instance_name,sizeof(data->instance_name), 
		 "inst%d",rtapi_instance);
    } else {
	strncpy(data->instance_name,name, sizeof(data->instance_name));
    }

    // separate message levels for RT and userland
    data->rt_msg_level = rt_level;
    data->user_msg_level = user_level;

    // next value returned by rtapi_init (userland threads)
    // those dont use fixed sized arrays 
    data->next_module_id = 0;

    // tell the others what thread flavor this RTAPI was compiled for
    data->rtapi_thread_flavor = THREAD_FLAVOR_ID;

    // HAL segment size - module param to rtapi 'hal_size=n'
    data->hal_size = hal_size;

    // init the error ring
    rtapi_ringheader_init(&data->rtapi_messages, 0, SIZE_ALIGN(MESSAGE_RING_SIZE), 0);
    memset(&data->rtapi_messages.buf[0], 0, SIZE_ALIGN(MESSAGE_RING_SIZE));

    // prime it
    data->rtapi_messages.refcount = 1;   // rtapi is 'attached'
    data->rtapi_messages.use_wmutex = 1; // hint only

    // demon pids
    data->rtapi_app_pid = 0;
    data->rtapi_msgd_pid = 0;

    // make it accessible
    rtapi_ringbuffer_init(&data->rtapi_messages, &rtapi_message_buffer);

    rtapi_set_logtag("rt");

    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return;
}

int  _rtapi_next_module_id(void) 
{
    int next_id;

    // TODO: replace by atomic inrement-and-get
    // once rtapi_atomic.h has been merged
    rtapi_mutex_try(&(global_data->mutex));
    next_id = global_data->next_module_id++;
    rtapi_mutex_give(&(global_data->mutex));
    return next_id;
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
		if (test_bit(m, shmems[n].bitmap)) {
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
