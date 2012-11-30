/********************************************************************
* Description:  rtapi_task.c
*               This file, 'rtapi_task.c', implements the task-
*               related functions for realtime modules.  See rtapi.h
*               for more info.
*
*		The functions here can be customized by defining
*		'hook' functions in a separate source file for the
*		thread system, and define a macro indicating that the
*		definition exists.  The functions below that accept
*		hooks are preceded by a prototype for the hook
*		function.
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// RTAPI macros and decls

#ifdef MODULE
#include <linux/module.h>	/* EXPORT_SYMBOL */
#endif

/*
  These functions are completely different between each thread system,
  so they should be defined in their respective $THREADS.c files:

  int rtapi_init(const char *modname)
  int rtapi_exit(int id)
*/


/* priority functions */

/* Some RT systems (well, just RTAI) define lower values as higher
   priority */
#ifdef INVERSE_PRIO
#    define PRIO_INCR --
#    define PRIO_DECR ++
#    define PRIO_GT(a,b) (a<b)
#    define PRIO_LT(a,b) (a>b)
#else // normal priorities
#    define PRIO_INCR ++
#    define PRIO_DECR --
#    define PRIO_GT(a,b) (a>b)
#    define PRIO_LT(a,b) (a<b)
#endif

int rtapi_prio_highest(void)
{
    return PRIO_HIGHEST;
}

int rtapi_prio_lowest(void)
{
    return PRIO_LOWEST;
}

int rtapi_prio_next_higher(int prio)
{
    /* next higher priority for arg */
    prio PRIO_INCR;

    /* return a valid priority for out of range arg */
    if (PRIO_GT(prio,rtapi_prio_highest()))
	return rtapi_prio_highest();
    if (PRIO_GT(rtapi_prio_lowest(),prio))
	return rtapi_prio_lowest();

    return prio;
}

int rtapi_prio_next_lower(int prio)
{
    /* next lower priority for arg */
    prio PRIO_DECR;

    /* return a valid priority for out of range arg */
    if (PRIO_GT(prio,rtapi_prio_highest()))
	return rtapi_prio_highest();
    if (PRIO_GT(rtapi_prio_lowest(),prio))
	return rtapi_prio_lowest();

    return prio;
}


#ifdef RTAPI  /* below functions not available to user programs */

/* task setup and teardown functions */
#ifdef HAVE_RTAPI_TASK_NEW_HOOK
int rtapi_task_new_hook(task_data *task, int task_id);
#endif

int rtapi_task_new(void (*taskcode) (void*), void *arg,
		   int prio, int owner, unsigned long int stacksize, 
		   int uses_fp, char *name, int cpu_id) 
{
    int n;
    int task_id;
    task_data *task;

    /* find an empty entry in the task array */
    /*! \todo  FIXME - this is not 100% thread safe.  If another thread
      calls this function after the first thread breaks out of
      the loop but before it sets the magic number, two tasks
      might wind up assigned to the same structure.  Need an
      atomic test and set for the magic number.  Not tonight! */
    rtapi_mutex_get(&(rtapi_data->mutex));

    n = 1; // tasks start at one!
    // go through task_array until an empty task slot is found
    while ((n < MAX_TASKS) && (task_array[n].magic == TASK_MAGIC))
	n++;
    // if task_array is full, release lock and return error
    if (n == MAX_TASKS) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -ENOMEM;
    }
    task_id = n;
    task = &(task_array[task_id]);

    // if requested priority is invalid, release lock and return error
    if (PRIO_LT(prio,rtapi_prio_lowest()) ||
	PRIO_GT(prio,rtapi_prio_highest())) {
	
	rtapi_print_msg(RTAPI_MSG_ERR,
			"New task  %d  '%s': invalid priority %d "
			"(highest=%d lowest=%d)\n",
			task_id, name, prio,
			rtapi_prio_highest(),
			rtapi_prio_lowest());
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }

    // task slot found; reserve it and release lock
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "Creating new task %d  '%s': "
		    "req prio %d (highest=%d lowest=%d)\n",
		    task_id, name, prio,
		    rtapi_prio_highest(),
		    rtapi_prio_lowest());
    task->magic = TASK_MAGIC;

    rtapi_mutex_give(&(rtapi_data->mutex));
    /*! \todo FIXME - end of non-threadsafe window */

    /* fill out task structure */
    if(stacksize < 16384) stacksize = 16384;
    task->owner = owner;
    task->arg = arg;
    task->stacksize = stacksize;
    task->taskcode = taskcode;
    task->prio = prio;
    task->uses_fp = uses_fp;
    task->cpu = cpu_id;
    strncpy(task->name, name, sizeof(task->name));
    task->name[sizeof(task->name) - 1] = '\0';

    /* rtapi_task_new_hook() should perform any thread system-specific
       tasks, and return task_id or an error code back to the
       caller */
#ifdef HAVE_RTAPI_TASK_NEW_HOOK
    return rtapi_task_new_hook(task,task_id);
#else
    return task_id;
#endif

}


#ifdef HAVE_RTAPI_TASK_DELETE_HOOK
int rtapi_task_delete_hook(task_data *task, int task_id);
#endif

int rtapi_task_delete(int task_id) {
    task_data *task;
    int retval = 0;

    if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;

    task = &(task_array[task_id]);
    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

    rtapi_print_msg(RTAPI_MSG_DBG, "rt_task_delete %d \"%s\"\n", task_id, 
		    task->name );

#ifdef HAVE_RTAPI_TASK_DELETE_HOOK
    retval = rtapi_task_delete_hook(task,task_id);
#endif

    rtapi_mutex_get(&(rtapi_data->mutex));
    task->magic = 0;
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}


#ifdef HAVE_RTAPI_TASK_WRAPPER_HOOK
void rtapi_task_wrapper_hook(task_data *task, int task_id);
#endif

#ifndef NO_RTAPI_TASK_WRAPPER
void task_wrapper(void *arg) {
    long task_id_long = (long) arg;  // silence gcc warnings on 64-bit arch
    int task_id = (int) task_id_long;
    task_data *task = &task_array[task_id];

    /* use the argument to point to the task data */
    if (task->period < period) task->period = period;
    task->ratio = task->period / period;
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_task_wrapper: task %p '%s' period=%d "
		    "prio=%d ratio=%d\n",
		    task, task->name, task->ratio * period,
		    task->prio, task->ratio);
    
#ifdef HAVE_RTAPI_TASK_WRAPPER_HOOK
    rtapi_task_wrapper_hook(task, task_id);
#endif

    /* call the task function with the task argument */
    (task->taskcode) (task->arg);
    
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "ERROR: reached end of wrapper for task %d '%s'\n", 
		    task_id, task->name);
}
wrapper_t rtapi_task_wrapper = &task_wrapper;
#endif


#ifdef HAVE_RTAPI_TASK_START_HOOK
int rtapi_task_start_hook(task_data *task, int task_id);
#endif

int rtapi_task_start(int task_id, unsigned long int period_nsec)
{
    task_data *task;

    if (task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
    task = &task_array[task_id];

    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

    if (period_nsec < period) period_nsec = period;
    task->period = period_nsec;
    task->ratio = period_nsec / period;

    /* create the thread - use the wrapper function, pass it a pointer
     * to the task structure so it can call the actual task function 
     */

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_task_start:  starting task %d '%s'\n",
		    task_id, task->name);

#ifdef HAVE_RTAPI_TASK_START_HOOK
    return rtapi_task_start_hook(task,task_id);
#else
    return 0;
#endif
}


#ifdef HAVE_RTAPI_TASK_STOP_HOOK
int rtapi_task_stop_hook(task_data *task, int task_id);
#endif

int rtapi_task_stop(int task_id)
{
    task_data *task;

    if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
    task = &task_array[task_id];

    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

#ifdef HAVE_RTAPI_TASK_STOP_HOOK
    rtapi_task_stop_hook(task,task_id);
#endif

    return 0;
}

#ifdef HAVE_RTAPI_TASK_PAUSE_HOOK
int rtapi_task_pause_hook(task_data *task, int task_id);
#endif

int rtapi_task_pause(int task_id)
{
    task_data *task;
    if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
    task = &task_array[task_id];
  
    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

#ifdef HAVE_RTAPI_TASK_PAUSE_HOOK
    return rtapi_task_pause_hook(task,task_id);
#else
    return -ENOSYS;
#endif

}

#ifdef HAVE_RTAPI_WAIT_HOOK
void rtapi_wait_hook();
#endif

void rtapi_wait(void)
{
#ifdef HAVE_RTAPI_WAIT_HOOK
    rtapi_wait_hook();
#endif
    return;
}

#ifdef HAVE_RTAPI_TASK_RESUME_HOOK
int rtapi_task_resume_hook(task_data *task, int task_id);
#endif

int rtapi_task_resume(int task_id)
{
    task_data *task;
    if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
    task = &task_array[task_id];
  
    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

#ifdef HAVE_RTAPI_TASK_RESUME_HOOK
    return rtapi_task_resume_hook(task,task_id);
#else
    return -ENOSYS;
#endif
}


/* not defined in rt-preempt */
#ifdef HAVE_RTAPI_TASK_SELF_HOOK
int rtapi_task_self_hook();
#endif

int rtapi_task_self(void) {
#ifdef HAVE_RTAPI_TASK_SELF_HOOK
    return rtapi_task_self_hook();
#else
    /* not implemented */
    return -EINVAL;
#endif
}

#endif  /* RTAPI */

#ifdef MODULE
EXPORT_SYMBOL(rtapi_prio_highest);
EXPORT_SYMBOL(rtapi_prio_lowest);
EXPORT_SYMBOL(rtapi_prio_next_higher);
EXPORT_SYMBOL(rtapi_prio_next_lower);
EXPORT_SYMBOL(rtapi_task_new);
EXPORT_SYMBOL(rtapi_task_delete);
EXPORT_SYMBOL(rtapi_task_start);
EXPORT_SYMBOL(rtapi_wait);
EXPORT_SYMBOL(rtapi_task_resume);
EXPORT_SYMBOL(rtapi_task_pause);
#endif
