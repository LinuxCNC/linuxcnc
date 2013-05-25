#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_shm.h>
#include <rtai_fifos.h>

#ifdef MODULE
#include <linux/delay.h>  // udelay()
#endif 

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#ifdef RTAPI
extern rtapi_exception_handler_t rt_exception_handler;
#endif

/***********************************************************************
*                           rtapi_time.c                               *
************************************************************************/

#ifdef RTAPI
void _rtapi_module_timer_stop(void) {
    stop_rt_timer();
    rt_free_timer();
}


void _rtapi_clock_set_period_hook(long int nsecs, RTIME *counts, 
				 RTIME *got_counts) {
    rt_set_periodic_mode();
    *counts = nano2count((RTIME) nsecs);
    if(count2nano(*counts) > nsecs) (*counts)--;
    *got_counts = start_rt_timer(*counts);
    rtapi_data->timer_period = count2nano(*got_counts);
}


void _rtapi_delay_hook(long int nsec)
{
     udelay(nsec / 1000);
}
#endif  /* RTAPI */

long long int _rtapi_get_time_hook(void) {
    //struct timeval tv;

    //AJ: commenting the following code out, as it seems on some systems it
    // really breaks
    
    /* call the kernel's internal implementation of gettimeofday() */
    /* unfortunately timeval has only usec, struct timespec would be
       better, it has nsec resolution.  Doing this right probably
       involves a number of ifdefs based on kernel version and such */
    /*do_gettimeofday(&tv);*/
    /* convert to nanoseconds */
    /*return (tv.tv_sec * 1000000000LL) + (tv.tv_usec * 1000L);*/
    
    //reverted to old code for now
    /* this is a monstrosity that seems to take several MICROSECONDS!!!
       on some boxes.  Why the RTAI folks even bothered I have no idea!
       If you have any need for speed at all use _rtapi_get_clocks()!!
    */
    return rt_get_cpu_time_ns();
}



/***********************************************************************
*                          rtapi_module.c                              *
************************************************************************/

#ifdef RTAPI
int _rtapi_module_master_shared_memory_init(rtapi_data_t **rtapi_data) {
    /* get master shared memory block from OS and save its address */
    *rtapi_data = rtai_kmalloc(OS_KEY(RTAPI_KEY,rtapi_instance), sizeof(rtapi_data_t));
    if (*rtapi_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: could not open rtapi_data shared memory\n");
	return -ENOMEM;
    }
    return 0;
}

void _rtapi_module_master_shared_memory_free(void) {
    rtai_kfree(OS_KEY(RTAPI_KEY,rtapi_instance));
}

void _rtapi_module_cleanup_hook(void) {
    rtai_kfree(OS_KEY(RTAPI_KEY,rtapi_instance));
}


#else /* ULAPI */
rtapi_data_t *_rtapi_init_hook() {
    rtapi_data_t *result;
    result = rtai_malloc(OS_KEY(RTAPI_KEY,rtapi_instance), sizeof(rtapi_data_t));

    // the check for -1 here is because rtai_malloc (in at least
    // rtai 3.6.1, and probably others) has a bug where it
    // sometimes returns -1 on error
    if (result == (rtapi_data_t*)-1)
	result = NULL;

    return result;
}

#endif  /* ULAPI */




/***********************************************************************
*                           rtapi_task.c                               *
************************************************************************/

#ifdef RTAPI

# if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)
#  define IP(x) ((x)->ip)
# elif defined(__i386__)
#  define IP(x) ((x)->eip)
# else
#  define IP(x) ((x)->rip)
# endif

static int _rtapi_trap_handler(int vec, int signo, struct pt_regs *regs,
			      void *task) {
    int self = _rtapi_task_self();
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "RTAPI: Task %d[%p]: Fault with vec=%d, signo=%d "
		    "ip=%08lx.\nRTAPI: This fault may not be recoverable "
		    "without rebooting.\n",
		    self, task, vec, signo, IP(regs));
    _rtapi_task_pause(self);
    return 0;
}


static void _rtapi_task_wrapper(long task_id)  {
    task_data *task;

    /* point to the task data */
    task = &task_array[task_id];
    /* call the task function with the task argument */
    (task->taskcode) (task->arg);
    /* if the task ever returns, we record that fact */
    task->state = ENDED;
    /* and return to end the thread */
    return;
}

int _rtapi_task_new_hook(task_data *task, int task_id) {
    int retval, v;
    retval = rt_task_init_cpuid(ostask_array[task_id], _rtapi_task_wrapper,
				task_id, task->stacksize, task->prio,
				task->uses_fp, 0 /* signal */, task->cpu);
    if (retval) return retval;

    /* request to handle traps in the new task */
    for(v=0; v<HAL_NR_FAULTS; v++)
        rt_set_task_trap_handler(ostask_array[task_id], v, _rtapi_trap_handler);

    return 0;
}


int _rtapi_task_start_hook(task_data *task, int task_id,
			  unsigned long int period_nsec) {
    int retval;
    unsigned long int quo, period_counts;

    period_counts = nano2count((RTIME)period_nsec);  
    quo = (period_counts + timer_counts / 2) / timer_counts;
    period_counts = quo * timer_counts;
    period_nsec = count2nano(period_counts);

    /* start the task */
    retval = rt_task_make_periodic(ostask_array[task_id],
	rt_get_time() + period_counts, period_counts);
    if (retval != 0) {
	return -EINVAL;
    }

    return 0;
}

void _rtapi_wait_hook(void) {
    char buf[LINELEN];
    int result = rt_task_wait_period();

    if(result != 0) {
#ifdef RTE_TMROVRN
	if (result == RTE_TMROVRN) {
	    rtapi_snprintf(buf, sizeof(buf),
			   "Unexpected realtime delay on task %d",
			   _rtapi_task_self());
	    if (rt_exception_handler)
		rt_exception_handler(RTAI_RTE_TMROVRN, _rtapi_task_self(), buf);
	} else
#endif
#ifdef RTE_UNBLKD
	    if (result == RTE_UNBLKD) {
		rtapi_snprintf(buf, sizeof(buf),
			       "rt_task_wait_period() returned RTE_UNBLKD (%d).",
			       result);
		if (rt_exception_handler)
		    rt_exception_handler(RTAI_RTE_UNBLKD, _rtapi_task_self(), buf);
	    } else
#endif
		{
		    rtapi_snprintf(buf, sizeof(buf),
				   "rt_task_wait_period() returned %d.\n", result);
		    if (rt_exception_handler)
			rt_exception_handler(RTAI_RTE_UNCLASSIFIED, _rtapi_task_self(), buf);
		}
    }
}

int _rtapi_task_self_hook(void) {
    RT_TASK *ptr;
    int n;

    /* ask OS for pointer to its data for the current task */
    ptr = rt_whoami();
    if (ptr == NULL) {
	/* called from outside a task? */
	return -EINVAL;
    }
    /* find matching entry in task array */
    n = 1;
    while (n <= RTAPI_MAX_TASKS) {
	if (ostask_array[n] == ptr) {
	    /* found a match */
	    return n;
	}
	n++;
    }
    return -EINVAL;
}
#endif /* RTAPI */


/***********************************************************************
*                           rtapi_shmem.c                              *
************************************************************************/
/* needed for both RTAPI and ULAPI */

void *_rtapi_shmem_new_realloc_hook(int shmem_id, int key,
				    unsigned long int size, int instance) {
    rtapi_data_t * result;
    result = rtai_kmalloc(key, shmem_array[shmem_id].size);
    // the check for -1 here is because rtai_malloc (in at least
    // rtai 3.6.1, and probably others) has a bug where it
    // sometimes returns -1 on error
    if (result == (void*)-1)
	return NULL;
    else
	return result;
}

void *_rtapi_shmem_new_malloc_hook(int shmem_id, int key,
				  unsigned long int size, int instance) {
    void * result;
    result = rtai_kmalloc(key, size);

    // the check for -1 here is because rtai_malloc (in at least
    // rtai 3.6.1, and probably others) has a bug where it
    // sometimes returns -1 on error
    if (result == (void*)-1)
	return NULL;
    else
	return result;
}

void _rtapi_shmem_delete_hook(shmem_data *shmem,int shmem_id) {
    rtai_kfree(shmem->key);
}
