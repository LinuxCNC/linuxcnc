
#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

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


/* define the rtapi_switch struct, with pointers to all rtapi_*
   functions
*/

rtapi_switch_t rtapi_switch = {
    // init & exit functions
    .rtapi_init = &_rtapi_init,
    .rtapi_exit = &_rtapi_exit,
    // messaging functions
    .rtapi_snprintf = &_rtapi_snprintf,
    .rtapi_vsnprintf = &_rtapi_vsnprintf,
    .rtapi_print = &_rtapi_print,
    .rtapi_print_msg = &_rtapi_print_msg,
    .rtapi_set_msg_level = &_rtapi_set_msg_level,
    .rtapi_get_msg_level = &_rtapi_get_msg_level,
#ifdef RTAPI
    .rtapi_set_msg_handler = &_rtapi_set_msg_handler,
    .rtapi_get_msg_handler = &_rtapi_get_msg_handler,
#endif
    // time functions
#ifdef RTAPI
    .rtapi_clock_set_period = &_rtapi_clock_set_period,
    .rtapi_delay = &_rtapi_delay,
    .rtapi_delay_max = &_rtapi_delay_max,
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
#endif
    // shared memory functions
    .rtapi_shmem_new = &_rtapi_shmem_new,
    .rtapi_shmem_delete = &_rtapi_shmem_delete,
    .rtapi_shmem_getptr = &_rtapi_shmem_getptr,
    // i/o related functions
    .rtapi_outb = &_rtapi_outb,
    .rtapi_inb = &_rtapi_inb,
    .rtapi_outw = &_rtapi_outw,
    .rtapi_inw = &_rtapi_inw,
};


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
    /* set version code so other modules can check it */
    data->rev_code = REV_CODE;
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
	for (m = 0; m < (RTAPI_MAX_SHMEMS / 8) + 1; m++) {
	    data->shmem_array[n].bitmap[m] = 0;
	}
    }
#ifdef HAVE_INIT_RTAPI_DATA_HOOK
    init_rtapi_data_hook(data);
#endif

    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return;
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
void rtapi_printall(void) {
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
    rtapi_print_msg(RTAPI_MSG_DBG, "  rev_code = %08x\n",
		    rtapi_data->rev_code);
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
