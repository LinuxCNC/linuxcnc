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
#include <linux/slab.h>		/* kmalloc() */
#endif

/*
  These functions are completely different between each userland
  thread system, so these are defined in rtapi_module.c for kernel
  threads systems and $THREADS.c for the userland thread systems

  int rtapi_init(const char *modname)
  int rtapi_exit(int id)
*/


#ifdef MODULE
/* resource data unique to kernel space */
RT_TASK *ostask_array[RTAPI_MAX_TASKS + 1];
#endif


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

int _rtapi_prio_highest(void) {
    return PRIO_HIGHEST;
}

int _rtapi_prio_lowest(void) {
    return PRIO_LOWEST;
}

int _rtapi_prio_next_higher(int prio) {
    /* next higher priority for arg */
    prio PRIO_INCR;

    /* return a valid priority for out of range arg */
    if (PRIO_GT(prio,rtapi_prio_highest()))
	return rtapi_prio_highest();
    if (PRIO_GT(rtapi_prio_lowest(),prio))
	return rtapi_prio_lowest();

    return prio;
}

int _rtapi_prio_next_lower(int prio) {
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

int _rtapi_task_new(void (*taskcode) (void*), void *arg,
		   int prio, int owner, unsigned long int stacksize, 
		   int uses_fp, char *name, int cpu_id) {
    int task_id;
    int retval = 0;
    task_data *task;

    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));

#ifdef MODULE
    /* validate owner */
    if ((owner < 1) || (owner > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    if (module_array[owner].state != REALTIME) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
#endif

    /* find an empty entry in the task array */
    task_id = 1; // tasks start at one!
    // go through task_array until an empty task slot is found
    while ((task_id < RTAPI_MAX_TASKS) &&
	   (task_array[task_id].magic == TASK_MAGIC))
	task_id++;
    // if task_array is full, release lock and return error
    if (task_id == RTAPI_MAX_TASKS) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -ENOMEM;
    }
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

    /* fill out task structure */
    if(stacksize < MIN_STACKSIZE) stacksize = MIN_STACKSIZE;
    task->owner = owner;
    task->arg = arg;
    task->stacksize = stacksize;
    task->taskcode = taskcode;
    task->prio = prio;
    task->uses_fp = uses_fp;
    /*  hopefully this works for userland thread systems too  */
    task->cpu = cpu_id > -1 ? cpu_id : rtapi_data->rt_cpu;
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "Task CPU:  %d\n", task->cpu);
    /*    task->cpu = cpu_id;  */
    strncpy(task->name, name, sizeof(task->name));
    task->name[sizeof(task->name) - 1] = '\0';

#ifdef MODULE
    /* get space for the OS's task data - this is around 900 bytes, */
    /* so we don't want to statically allocate it for unused tasks. */
    ostask_array[task_id] = kmalloc(sizeof(RT_TASK), GFP_USER);
    if (ostask_array[task_id] == NULL) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -ENOMEM;
    }

#ifdef HAVE_RTAPI_TASK_NEW_HOOK
    /* kernel threads: rtapi_task_new_hook() should call OS to
       initialize the task - use predetermined or explicitly assigned
       CPU */
    retval = rtapi_task_new_hook(task, task_id);

    if (retval) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rt_task_create failed, rc = %d\n", retval );

	/* couldn't create task, free task data memory */
	kfree(ostask_array[task_id]);
	rtapi_mutex_give(&(rtapi_data->mutex));
	if (retval == ENOMEM) {
	    /* not enough space for stack */
	    return -ENOMEM;
	}
	/* unknown error */
	return -EINVAL;
    }
#endif

    /* the task has been created, update data */
    task->state = PAUSED;
    retval = task_id;
#else  /* userland thread */
    /* userland threads: rtapi_task_new_hook() should perform any
       thread system-specific tasks, and return task_id or an error
       code back to the caller (how do we know the diff between an
       error and a task_id???).  */
    task->state = USERLAND;	// userland threads don't track this

#  ifdef HAVE_RTAPI_TASK_NEW_HOOK
    retval = rtapi_task_new_hook(task,task_id);
#  else
    retval = task_id;
#  endif
#endif  /* userland thread */

    rtapi_data->task_count++;

    rtapi_mutex_give(&(rtapi_data->mutex));

    /* announce the birth of a brand new baby task */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: task %02d installed by module %02d, priority %d, code: %p\n",
	task_id, task->owner, task->prio, taskcode);

    return task_id;
}


#ifdef HAVE_RTAPI_TASK_DELETE_HOOK
int rtapi_task_delete_hook(task_data *task, int task_id);
#endif

int _rtapi_task_delete(int task_id) {
    task_data *task;
    int retval = 0;

    if(task_id < 0 || task_id >= RTAPI_MAX_TASKS) return -EINVAL;

    task = &(task_array[task_id]);
    /* validate task handle */
    if (task->magic != TASK_MAGIC)	// nothing to delete
	return -EINVAL;

    if (task->state != DELETE_LOCKED)	// we don't already hold mutex
	rtapi_mutex_get(&(rtapi_data->mutex));

#ifdef MODULE
    if ((task->state == PERIODIC) || (task->state == FREERUN)) {
	/* task is running, need to stop it */
	rtapi_print_msg(RTAPI_MSG_WARN,
	    "RTAPI: WARNING: tried to delete task %02d while running\n",
	    task_id);
	rtapi_task_pause(task_id);
    }
    /* get rid of it */
    rt_task_delete(ostask_array[task_id]);
    /* free kernel memory */
    kfree(ostask_array[task_id]);
    /* update data */
    task->prio = 0;
    task->owner = 0;
    task->taskcode = NULL;
    ostask_array[task_id] = NULL;
    rtapi_data->task_count--;
    /* if no more tasks, stop the timer */
    if (rtapi_data->task_count == 0) {
	if (rtapi_data->timer_running != 0) {
#  ifdef HAVE_RTAPI_MODULE_TIMER_STOP
	    rtapi_module_timer_stop();
#  endif
	    rtapi_data->timer_period = 0;
	    max_delay = DEFAULT_MAX_DELAY;
	    rtapi_data->timer_running = 0;
	}
    }
#endif /* MODULE */

#ifdef HAVE_RTAPI_TASK_DELETE_HOOK
    retval = rtapi_task_delete_hook(task,task_id);
#endif

    if (task->state != DELETE_LOCKED)	// we don't already hold mutex
	rtapi_mutex_give(&(rtapi_data->mutex));
    task->state = EMPTY;
    task->magic = 0;

    rtapi_print_msg(RTAPI_MSG_DBG, "rt_task_delete %d \"%s\"\n", task_id, 
		    task->name );

    return retval;
}


/* all threads systems must define this hook */
int rtapi_task_start_hook(task_data *task, int task_id,
			   unsigned long int period_nsec);

#ifndef MODULE  /* userspace RTAPI */
int _rtapi_task_start(int task_id, unsigned long int period_nsec) {
    task_data *task;

    if (task_id < 0 || task_id >= RTAPI_MAX_TASKS) return -EINVAL;
    
    task = &task_array[task_id];

    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

    if (period_nsec < period) period_nsec = period;
    task->period = period_nsec;
    task->ratio = period_nsec / period;

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_task_start:  starting task %d '%s'\n",
		    task_id, task->name);
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: period_nsec: %ld\n", period_nsec);

    return rtapi_task_start_hook(task,task_id,0);
}
#else  /* kernel RTAPI */
int _rtapi_task_start(int task_id, unsigned long int period_nsec) {
    int retval;
    task_data *task;

    /* validate task ID */
    if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
	return -EINVAL;
    }
    /* point to the task's data */
    task = &(task_array[task_id]);
    /* is task ready to be started? */
    if (task->state != PAUSED) {
	return -EINVAL;
    }
    /* can't start periodic tasks if timer isn't running */
    if ((rtapi_data->timer_running == 0) || (rtapi_data->timer_period == 0)) {
        rtapi_print_msg(RTAPI_MSG_ERR, 
                "RTAPI: could not start task: timer isn't running\n");
	return -EINVAL;
    }

    if ((retval = rtapi_task_start_hook(task, task_id, period_nsec)))
	return retval;

    /* ok, task is started */
    task->state = PERIODIC;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: start_task id: %02d\n", task_id);
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: period_nsec: %ld\n", period_nsec);
    return retval;
}
#endif  /* kernel threads */


#ifdef HAVE_RTAPI_TASK_STOP_HOOK
int rtapi_task_stop_hook(task_data *task, int task_id);
#endif

int _rtapi_task_stop(int task_id) {
    task_data *task;

    if(task_id < 0 || task_id >= RTAPI_MAX_TASKS) return -EINVAL;
    
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

int _rtapi_task_pause(int task_id) {
    task_data *task;
#ifdef MODULE
    task_state_t oldstate;
    int retval;
#endif

    if(task_id < 0 || task_id >= RTAPI_MAX_TASKS) return -EINVAL;
    
    task = &task_array[task_id];
  
    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

#ifdef MODULE
    if ((task->state != PERIODIC) && (task->state != FREERUN)) {
	return -EINVAL;
    }
    /* pause the task */
    oldstate = task->state;
    task->state = PAUSED;
    // ok for both RTAI and Xenomai
    retval = rt_task_suspend(ostask_array[task_id]);
    if (retval != 0) {
        task->state = oldstate;
	return -EINVAL;
    }
    /* update task data */
    return 0;
#endif

#ifdef HAVE_RTAPI_TASK_PAUSE_HOOK
    return rtapi_task_pause_hook(task,task_id);
#else
    return -ENOSYS;
#endif

}

#ifdef HAVE_RTAPI_WAIT_HOOK
extern void rtapi_wait_hook(void);
#endif

void _rtapi_wait(void) {
#ifdef HAVE_RTAPI_WAIT_HOOK
    rtapi_wait_hook();
#endif
    return;
}

#ifdef HAVE_RTAPI_TASK_RESUME_HOOK
int rtapi_task_resume_hook(task_data *task, int task_id);
#endif

int _rtapi_task_resume(int task_id) {
    task_data *task;
#ifdef MODULE
    int retval;
#endif

    if(task_id < 0 || task_id >= RTAPI_MAX_TASKS) return -EINVAL;
    
    task = &task_array[task_id];
  
    /* validate task handle */
    if (task->magic != TASK_MAGIC)
	return -EINVAL;

#ifdef MODULE
    if (task->state != PAUSED) {
	return -EINVAL;
    }
    /* start the task */
    // ok for both RTAI and Xenomai
    retval = rt_task_resume(ostask_array[task_id]);
    if (retval != 0) {
	return -EINVAL;
    }
    /* update task data */
    task->state = FREERUN;

    return 0;
#endif

#ifdef HAVE_RTAPI_TASK_RESUME_HOOK
    return rtapi_task_resume_hook(task,task_id);
#else
    return -ENOSYS;
#endif
}


/* not defined in rt-preempt */
#ifdef HAVE_RTAPI_TASK_SELF_HOOK
int rtapi_task_self_hook(void);
#endif

int _rtapi_task_self(void) {
#ifdef HAVE_RTAPI_TASK_SELF_HOOK
    return rtapi_task_self_hook();
#else
    /* not implemented */
    return -EINVAL;
#endif
}

#endif  /* RTAPI */

#ifdef EXPORT_SYMS
EXPORT_SYMBOL(_rtapi_prio_highest);
EXPORT_SYMBOL(_rtapi_prio_lowest);
EXPORT_SYMBOL(_rtapi_prio_next_higher);
EXPORT_SYMBOL(_rtapi_prio_next_lower);
EXPORT_SYMBOL(_rtapi_task_new);
EXPORT_SYMBOL(_rtapi_task_delete);
EXPORT_SYMBOL(_rtapi_task_start);
EXPORT_SYMBOL(_rtapi_wait);
EXPORT_SYMBOL(_rtapi_task_resume);
EXPORT_SYMBOL(_rtapi_task_pause);
EXPORT_SYMBOL(_rtapi_task_self);
#endif
