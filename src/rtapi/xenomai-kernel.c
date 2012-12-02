
#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#include <native/heap.h>		// RT_HEAP, H_SHARED, rt_heap_*
#include <native/task.h>		// RT_TASK, rt_task_*()

#ifdef RTAPI	/* In kernel land, this is equiv. to MODULE */
#include <linux/slab.h>			// kfree
#include <native/types.h>		// TM_INFINITE
#include <native/timer.h>		// rt_timer_*()
// #include  <rtdk.h>

#else /* ULAPI */
#include <errno.h>		/* errno */
#endif


#define MASTER_HEAP "rtapi-heap"

#define MAX_ERRORS 3

/* resource data unique to kernel space */
RT_TASK *ostask_array[RTAPI_MAX_TASKS + 1];

static RT_HEAP shmem_heap_array[RTAPI_MAX_SHMEMS + 1];        

#ifdef RTAPI
static RT_HEAP master_heap;
static rthal_trap_handler_t old_trap_handler;
static int rtapi_trap_handler(unsigned event, rthal_pipeline_stage_t *stage,
			      void *data);

#else /* ULAPI */
RT_HEAP ul_heap_desc;
#endif /* ULAPI */



/***********************************************************************
*                          rtapi_common.h                              *
************************************************************************/
/* fill out Xenomai-specific fields in rtapi_data */
void init_rtapi_data_hook(rtapi_data_t * data) {
    data->rt_wait_error = 0;
    data->rt_last_overrun = 0;
    data->rt_total_overruns = 0;

#if 0
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	memset(&shmem_heap_array[n].heap, 0, sizeof(shmem_heap_array[n]));
    }
#endif

}

/***********************************************************************
*                           rtapi_time.c                               *
************************************************************************/

#ifdef RTAPI
/*  RTAPI time functions */
long long int rtapi_get_time_hook(void) {
    /* The value returned will represent a count of jiffies if the native  */
    /* skin is bound to a periodic time base (see CONFIG_XENO_OPT_NATIVE_PERIOD),  */
    /* or nanoseconds otherwise.  */
    return  rt_timer_read();
}

/* This returns a result in clocks instead of nS, and needs to be used
   with care around CPUs that change the clock speed to save power and
   other disgusting, non-realtime oriented behavior.  But at least it
   doesn't take a week every time you call it.
*/
long long int rtapi_get_clocks_hook(void) {
    // Gilles says: do this - it's portable
    return rt_timer_tsc();
}

void rtapi_clock_set_period_hook(long int nsecs, RTIME *counts, 
				 RTIME *got_counts) {

    *counts = rt_timer_ns2ticks((RTIME) nsecs);
    rt_timer_set_mode(*counts);
    rtapi_data->timer_period = *got_counts = rt_timer_ticks2ns(*counts);
    timer_counts = *got_counts;
}
#endif /* RTAPI */


