
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

static RT_HEAP shmem_heap_array[RTAPI_MAX_SHMEMS + 1];        

#ifdef RTAPI
static RT_HEAP master_heap;
static rthal_trap_handler_t old_trap_handler;
static int rtapi_trap_handler(unsigned event, unsigned domid, void *data);

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
*                          rtapi_module.c                              *
************************************************************************/

#ifdef RTAPI
int rtapi_module_master_shared_memory_init(rtapi_data_t **rtapi_data) {
    int n;

    /* get master shared memory block from OS and save its address */
    if ((n = rt_heap_create(&master_heap, MASTER_HEAP, 
			    sizeof(rtapi_data_t), H_SHARED)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: rt_heap_create() returns %d\n", n);
	return -EINVAL;
    }
    if ((n = rt_heap_alloc(&master_heap, 0, TM_INFINITE,
			   (void **)rtapi_data)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: rt_heap_alloc() returns %d\n", n);
	return -EINVAL;
    }
    return 0;
}

void rtapi_module_master_shared_memory_free(void) {
    rt_heap_delete(&master_heap);
}

void rtapi_module_init_hook(void) {
    old_trap_handler = \
	rthal_trap_catch((rthal_trap_handler_t) rtapi_trap_handler);
}

void rtapi_module_cleanup_hook(void) {
    /* release master shared memory block */
    rt_heap_delete(&master_heap);
    rthal_trap_catch(old_trap_handler);
}
#endif /* RTAPI */


/***********************************************************************
*                           rtapi_time.c                               *
************************************************************************/

#ifdef RTAPI
/*  RTAPI time functions */
long long int rtapi_get_time_hook(void) {
    /* The value returned will represent a count of jiffies if the
       native skin is bound to a periodic time base (see
       CONFIG_XENO_OPT_NATIVE_PERIOD), or nanoseconds otherwise.  */
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
    rtapi_data->timer_period = *got_counts = (RTIME) nsecs; 
}


void rtapi_delay_hook(long int nsec) 
{
    long long int release = rt_timer_tsc() + nsec;
    while (rt_timer_tsc() < release);
}
#endif /* RTAPI */


/***********************************************************************
*                            rtapi_task.c                              *
************************************************************************/

#ifdef RTAPI
// not better than the builtin Xenomai handler, but at least
// hook into to rtapi_print
int rtapi_trap_handler(unsigned event, unsigned domid, void *data) {
    struct pt_regs *regs = data;
    xnthread_t *thread = xnpod_current_thread(); ;

    rtapi_print_msg(RTAPI_MSG_ERR, 
		    "RTAPI: trap event=%d thread=%s ip:%lx sp:%lx "
		    "userpid=%d errcode=%d\n",
		    event, thread->name,
		    regs->ip, regs->sp, 
		    xnthread_user_pid(thread), thread->errcode);
    // forward to default Xenomai trap handler
    return ((rthal_trap_handler_t) old_trap_handler)(event, domid, data);
}

int rtapi_task_self_hook(void) {
    RT_TASK *ptr;
    int n;

    /* ask OS for pointer to its data for the current task */
    ptr = rt_task_self();

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


void rtapi_wait_hook(void) {
    unsigned long overruns;
    static int error_printed = 0;
    int task_id;
    task_data *task;

    int result =  rt_task_wait_period(&overruns);
    switch (result) {
    case 0: // ok - no overruns;
	break;

    case -ETIMEDOUT: // release point was missed
	rtapi_data->rt_wait_error++;
	rtapi_data->rt_last_overrun = overruns;
	rtapi_data->rt_total_overruns += overruns;

	if (error_printed < MAX_ERRORS) {
	    task_id = rtapi_task_self();
	    task = &(task_array[task_id]);

	    rtapi_print_msg
		(RTAPI_MSG_ERR,
		 "RTAPI: ERROR: Unexpected realtime delay on task %d - "
		 "'%s' (%lu overruns)\n" 
		 "This Message will only display once per session.\n"
		 "Run the Latency Test and resolve before continuing.\n", 
		 task_id, task->name, overruns);
	}
	error_printed++;
	if(error_printed == MAX_ERRORS) 
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI: (further messages will be suppressed)\n");
	break;

    case -EWOULDBLOCK:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: rt_task_wait_period() without "
			"previous rt_task_set_periodic()\n");
	error_printed++;
	break;

    case -EINTR:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: rt_task_unblock() called before "
			"release point\n");
	error_printed++;
	break;

    case -EPERM:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: cannot rt_task_wait_period() from "
			"this context\n");
	error_printed++;
	break;
    default:
	rtapi_print_msg(error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
			"RTAPI: ERROR: unknown error code %d\n", result);
	error_printed++;
	break;
    }
}


int rtapi_task_new_hook(task_data *task, int task_id) {
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rt_task_create %d \"%s\" cpu=%d fpu=%d prio=%d\n", 
		    task_id, task->name, task->cpu, task->uses_fp,
		    task->prio );

    return rt_task_create(ostask_array[task_id], task->name, task->stacksize,
			  task->prio,
			  (task->uses_fp ? T_FPU : 0) | T_CPU(task->cpu));
}


int rtapi_task_start_hook(task_data *task, int task_id,
			  unsigned long int period_nsec) {
    int retval;

    if ((retval = rt_task_set_periodic(ostask_array[task_id], TM_NOW,
				       period_nsec)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: rt_task_set_periodic() task_id %d "
			"periodns=%ld returns %d\n", 
			task_id, period_nsec, retval);
	return -EINVAL;
    }
    if ((retval = rt_task_start(ostask_array[task_id], task->taskcode,
				(void*)task->arg )) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: rt_task_start() task_id %d returns %d\n", 
			task_id, retval);
	return -EINVAL;
    }

    return 0;
}


#else /* ULAPI */
rtapi_data_t *rtapi_init_hook() {
    int retval;
    rtapi_data_t *rtapi_data;

    if ((retval = rt_heap_bind(&ul_heap_desc, MASTER_HEAP, TM_NONBLOCK))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: rtapi_init: rt_heap_bind() "
			"returns %d - %s\n", 
			retval, strerror(-retval));
	return NULL;
    }
    if ((retval = rt_heap_alloc(&ul_heap_desc, 0,
				TM_NONBLOCK, (void **)&rtapi_data)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: rt_heap_alloc() returns %d - %s\n", 
			retval, strerror(retval));
	return NULL;
    }

    //rtapi_printall();
    return rtapi_data;
}
#endif /* ULAPI */

/***********************************************************************
*                           rtapi_shmem.c                              *
************************************************************************/

#ifdef RTAPI
void *rtapi_shmem_new_realloc_hook(int shmem_id, int key,
				   unsigned long int size) {
    rtapi_print_msg(RTAPI_MSG_ERR, 
		    "RTAPI: UNSUPPORTED OPERATION - cannot map "
		    "user segment %d into kernel\n",shmem_id);
    return NULL;
}

#else  /* ULAPI */
void *rtapi_shmem_new_realloc_hook(int shmem_id, int key,
				   unsigned long int size) {
    char shm_name[20];
    int retval;
    void *shmem_addr;

    snprintf(shm_name, sizeof(shm_name), "shm-%d", shmem_id);

    if (shmem_addr_array[shmem_id] == NULL) {
	if ((retval = rt_heap_bind(&shmem_heap_array[shmem_id], shm_name,
				   TM_NONBLOCK))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "ULAPI: ERROR: rtapi_shmem_new: "
			    "rt_heap_bind(%s) returns %d\n", 
			    shm_name, retval);
	    return NULL;
	}
	if ((retval = rt_heap_alloc(&shmem_heap_array[shmem_id], 0,
				    TM_NONBLOCK, (void**)&shmem_addr)) != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI: ERROR: rt_heap_alloc() returns %d\n",
			    retval);
	    return NULL;
	}
    } else {
	rtapi_print_msg(RTAPI_MSG_DBG,
			"ulapi %s already mapped \n",shm_name);
	return shmem_addr_array[shmem_id];
    }
    return shmem_addr;
}
#endif  /* ULAPI */


#ifdef RTAPI
void * rtapi_shmem_new_malloc_hook(int shmem_id, int key,
				   unsigned long int size) {
    char shm_name[20];
    void *shmem_addr;
    int retval;

    snprintf(shm_name, sizeof(shm_name), "shm-%d", shmem_id);
    if ((retval = rt_heap_create(&shmem_heap_array[shmem_id], shm_name, 
			    size, H_SHARED)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: rt_heap_create() returns %d\n", retval);
	return NULL;
    }
    if ((retval = rt_heap_alloc(&shmem_heap_array[shmem_id], 0, TM_INFINITE , 
				(void **)&shmem_addr)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: rt_heap_alloc() returns %d\n", retval);
	return NULL;
    }
    return shmem_addr;
}


#else  /* ULAPI */
void * rtapi_shmem_new_malloc_hook(int shmem_id, int key,
				   unsigned long int size) {
    char shm_name[20];
    void *shmem_addr;
    int retval;

    snprintf(shm_name, sizeof(shm_name), "shm-%d", shmem_id);

    if (shmem_addr_array[shmem_id] == NULL) {
	snprintf(shm_name, sizeof(shm_name), "shm-%d", shmem_id);
	if ((retval = rt_heap_create(&shmem_heap_array[shmem_id], shm_name,
				     size, H_SHARED))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "RTAPI: ERROR: rtapi_shmem_new: "
			    "rt_heap_create(%s,%ld) returns %d\n", 
			    shm_name, size, retval);
	    return NULL;
	}
	if ((retval = rt_heap_alloc(&shmem_heap_array[shmem_id], 0,
				    TM_NONBLOCK, (void **)&shmem_addr)) != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "RTAPI: ERROR: rt_heap_alloc() returns %d - %s\n", 
			    retval, strerror(retval));
	    return NULL;
	}
    } else {
	rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI: %s already mapped\n", shm_name);
    }

    return shmem_addr;
}
#endif /* ULAPI */


void rtapi_shmem_delete_hook(shmem_data *shmem,int shmem_id) {
    rt_heap_delete(&shmem_heap_array[shmem_id]);
}
