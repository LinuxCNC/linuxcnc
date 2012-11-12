
#include "config.h"
#include "rtapi.h"
#ifndef NULL
#define NULL 0
#endif

#include "rtapi_common.h"


#if defined(RTAPI_RTAI)
unsigned int rev_code = 1;  // increment this whenever you change the data structures
#endif

#if defined(RTAPI_XENOMAI_KERNEL)
unsigned int rev_code = 2; 
#endif

#if defined(RTAPI_XENOMAI_USER)
unsigned int rev_code = 3; 
#endif

#if defined(RTAPI_POSIX)
unsigned int rev_code = 4; 
#endif

#if defined(RTAPI_RTPREEMPT_USER)
unsigned int rev_code = 5; 
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

#if defined(RTAPI_XENOMAI_USER) || defined(RTAPI_POSIX) || defined(RTAPI_RTPREEMPT_USER)

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
    /* set version code so other modules can check it */
    data->rev_code = rev_code;
    /* and get busy */
    data->rt_module_count = 0;
    data->ul_module_count = 0;
    data->task_count = 0;
    data->shmem_count = 0;
    data->timer_running = 0;
    data->timer_period = 0;
#if defined(RTAPI_XENOMAI_KERNEL)
    data->rt_wait_error = 0;
    data->rt_last_overrun = 0;
    data->rt_total_overruns = 0;
#endif
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
    }
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	data->shmem_array[n].key = 0;
	data->shmem_array[n].rtusers = 0;
	data->shmem_array[n].ulusers = 0;
	data->shmem_array[n].size = 0;
#if 0 // defined(RTAPI_XENOMAI_KERNEL) 
	memset(&shmem_heap_array[n].heap, 0, sizeof(shmem_heap_array[n]));
#endif
	for (m = 0; m < (RTAPI_MAX_SHMEMS / 8) + 1; m++) {
	    data->shmem_array[n].bitmap[m] = 0;
	}
    }
    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return;
}
