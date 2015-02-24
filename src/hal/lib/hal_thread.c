// HAL thread API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

#ifdef RTAPI
static hal_thread_t *alloc_thread_struct(void);


/** 'thread_task()' is a function that is invoked as a realtime task.
    It implements a thread, by running down the thread's function list
    and calling each function in turn.
*/
static void thread_task(void *arg)
{
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_funct_entry_t *funct_root, *funct_entry;
    long long int start_time, end_time;
    long long int thread_start_time;

    thread = arg;
    while (1) {
	if (hal_data->threads_running > 0) {
	    /* point at first function on function list */
	    funct_root = (hal_funct_entry_t *) & (thread->funct_list);
	    funct_entry = SHMPTR(funct_root->links.next);
	    /* execution time logging */
	    start_time = rtapi_get_clocks();
	    end_time = start_time;
	    thread_start_time = start_time;
	    /* run thru function list */
	    while (funct_entry != funct_root) {
		/* call the function */
		funct_entry->funct(funct_entry->arg, thread->period);
		/* capture execution time */
		end_time = rtapi_get_clocks();
		/* point to function structure */
		funct = SHMPTR(funct_entry->funct_ptr);
		/* update execution time data */
		*(funct->runtime) = (hal_s32_t)(end_time - start_time);
		if ( *(funct->runtime) > funct->maxtime) {
		    funct->maxtime = *(funct->runtime);
		    funct->maxtime_increased = 1;
		} else {
		    funct->maxtime_increased = 0;
		}
		/* point to next next entry in list */
		funct_entry = SHMPTR(funct_entry->links.next);
		/* prepare to measure time for next funct */
		start_time = end_time;
	    }
	    /* update thread execution time */
	    thread->runtime = (hal_s32_t)(end_time - thread_start_time);
	    if (thread->runtime > thread->maxtime) {
		thread->maxtime = thread->runtime;
	    }
	}
	/* wait until next period */
	rtapi_wait();
    }
}

// HAL threads - public API


int hal_create_thread(const char *name, unsigned long period_nsec,
		      int uses_fp, int cpu_id)
{
    int next, prev_priority;
    int retval, n;
    hal_thread_t *new, *tptr;
    long prev_period, curr_period;
    /*! \todo Another #if 0 */
#if 0
    char buf[HAL_NAME_LEN + 1];
#endif

    hal_print_msg(RTAPI_MSG_DBG,
		    "HAL: creating thread %s, %ld nsec\n", name, period_nsec);
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: create_thread called before init\n");
	return -EINVAL;
    }
    if (period_nsec == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: create_thread called "
			"with period of zero\n");
	return -EINVAL;
    }

    if (strlen(name) > HAL_NAME_LEN) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: thread name '%s' is too long\n", name);
	return -EINVAL;
    }
    if (hal_data->lock & HAL_LOCK_CONFIG) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: create_thread called while"
			" HAL is locked\n");
	return -EPERM;
    }
    {
	int  cmp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* make sure name is unique on thread list */
	next = hal_data->thread_list_ptr;
	while (next != 0) {
	    tptr = SHMPTR(next);
	    cmp = strcmp(tptr->name, name);
	    if (cmp == 0) {
		/* name already in list, can't insert */
		hal_print_msg(RTAPI_MSG_ERR,
				"HAL: ERROR: duplicate thread name %s\n", name);
		return -EINVAL;
	    }
	    /* didn't find it yet, look at next one */
	    next = tptr->next_ptr;
	}
	/* allocate a new thread structure */
	new = alloc_thread_struct();
	if (new == 0) {
	    /* alloc failed */
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: insufficient memory to create thread\n");
	    return -ENOMEM;
	}
	/* initialize the structure */
	new->uses_fp = uses_fp;
	new->cpu_id = cpu_id;
	new->handle = rtapi_next_handle();
	rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
	/* have to create and start a task to run the thread */
	if (hal_data->thread_list_ptr == 0) {
	    /* this is the first thread created */
	    /* is timer started? if so, what period? */
	    curr_period = rtapi_clock_set_period(0);
	    if (curr_period == 0) {
		/* not running, start it */
		curr_period = rtapi_clock_set_period(period_nsec);
		if (curr_period < 0) {
		    hal_print_msg(RTAPI_MSG_ERR,
				    "HAL_LIB: ERROR: clock_set_period returned %ld\n",
				    curr_period);
		    return -EINVAL;
		}
	    }
	    /* make sure period <= desired period (allow 1% roundoff error) */
	    if (curr_period > (period_nsec + (period_nsec / 100))) {
		hal_print_msg(RTAPI_MSG_ERR,
				"HAL_LIB: ERROR: clock period too long: %ld\n", curr_period);
		return -EINVAL;
	    }
	    if(hal_data->exact_base_period) {
		hal_data->base_period = period_nsec;
	    } else {
		hal_data->base_period = curr_period;
	    }
	    /* reserve the highest priority (maybe for a watchdog?) */
	    prev_priority = rtapi_prio_highest();
	    /* no previous period to worry about */
	    prev_period = 0;
	} else {
	    /* there are other threads, slowest (and lowest
	       priority) is at head of list */
	    tptr = SHMPTR(hal_data->thread_list_ptr);
	    prev_period = tptr->period;
	    prev_priority = tptr->priority;
	}
	if ( period_nsec < hal_data->base_period) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL_LIB: ERROR: new thread period %ld is less than clock period %ld\n",
			    period_nsec, hal_data->base_period);
	    return -EINVAL;
	}
	/* make period an integer multiple of the timer period */
	n = (period_nsec + hal_data->base_period / 2) / hal_data->base_period;
	new->period = hal_data->base_period * n;
	if ( new->period < prev_period ) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL_LIB: ERROR: new thread period %ld is less than existing thread period %ld\n",
			    period_nsec, prev_period);
	    return -EINVAL;
	}
	/* make priority one lower than previous */
	new->priority = rtapi_prio_next_lower(prev_priority);
	/* create task - owned by library module, not caller */
	retval = rtapi_task_new(thread_task,
				new,
				new->priority,
				lib_module_id,
				global_data->hal_thread_stack_size,
				uses_fp,
				new->name, new->cpu_id);
	if (retval < 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL_LIB: could not create task for thread %s\n", name);
	    return -EINVAL;
	}
	new->task_id = retval;
	/* start task */
	retval = rtapi_task_start(new->task_id, new->period);
	if (retval < 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL_LIB: could not start task for thread %s: %d\n", name, retval);
	    return -EINVAL;
	}
	/* insert new structure at head of list */
	new->next_ptr = hal_data->thread_list_ptr;
	hal_data->thread_list_ptr = SHMOFF(new);

	// exit block protected by scoped lock
    }

    /* init time logging variables */
    new->runtime = 0;
    new->maxtime = 0;
    /*! \todo Another #if 0 */
#if 0
    /* These params need to be re-visited when I refactor HAL.  Right
       now they cause problems - they can no longer be owned by the calling
       component, and they can't be owned by the hal_lib because it isn't
       actually a component.
    */
    /* create a parameter with the thread's runtime in it */
    rtapi_snprintf(buf, sizeof(buf), "%s.time", name);
    hal_param_s32_new(buf, HAL_RO, &(new->runtime), lib_module_id);
    /* create a parameter with the thread's maximum runtime in it */
    rtapi_snprintf(buf, sizeof(buf), "%s.tmax", name);
    hal_param_s32_new(buf, HAL_RW, &(new->maxtime), lib_module_id);
#endif
    hal_print_msg(RTAPI_MSG_DBG, "HAL: thread %s id %d created prio=%d\n",
		    name, new->task_id, new->priority);
    return 0;
}

extern int hal_thread_delete(const char *name)
{
    int *prev, next;

    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: thread_delete called before init\n");
	return -EINVAL;
    }

    if (hal_data->lock & HAL_LOCK_CONFIG) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: thread_delete called while HAL is locked\n");
	return -EPERM;
    }
    hal_print_msg(RTAPI_MSG_DBG, "HAL: deleting thread '%s'\n", name);
    {
	hal_thread_t *thread __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));
	/* search for the signal */
	prev = &(hal_data->thread_list_ptr);
	next = *prev;
	while (next != 0) {
	    thread = SHMPTR(next);
	    if (strcmp(thread->name, name) == 0) {
		/* this is the right thread, unlink from list */
		*prev = thread->next_ptr;
		/* and delete it */
		free_thread_struct(thread);
		/* done */
		return 0;
	    }
	    /* no match, try the next one */
	    prev = &(thread->next_ptr);
	    next = *prev;
	}
    }
    /* if we get here, we didn't find a match */
    hal_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: thread '%s' not found\n",
		    name);
    return -EINVAL;
}

#endif /* RTAPI */


int hal_start_threads(void)
{
    /* a trivial function for a change! */
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: start_threads called before init\n");
	return -EINVAL;
    }

    if (hal_data->lock & HAL_LOCK_RUN) {
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: start_threads called while HAL is locked\n");
	return -EPERM;
    }


    hal_print_msg(RTAPI_MSG_DBG, "HAL: starting threads\n");
    hal_data->threads_running = 1;
    return 0;
}

int hal_stop_threads(void)
{
    /* wow, two in a row! */
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: stop_threads called before init\n");
	return -EINVAL;
    }

    if (hal_data->lock & HAL_LOCK_RUN) {
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: stop_threads called while HAL is locked\n");
	return -EPERM;
    }

    hal_data->threads_running = 0;
    hal_print_msg(RTAPI_MSG_DBG, "HAL: threads stopped\n");
    return 0;
}

hal_thread_t *halpr_find_thread_by_name(const char *name)
{
    int next;
    hal_thread_t *thread;

    /* search thread list for 'name' */
    next = hal_data->thread_list_ptr;
    while (next != 0) {
	thread = SHMPTR(next);
	if (strcmp(thread->name, name) == 0) {
	    /* found a match */
	    return thread;
	}
	/* didn't find it yet, look at next one */
	next = thread->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

#ifdef RTAPI
static hal_thread_t *alloc_thread_struct(void)
{
    hal_thread_t *p;

    /* check the free list */
    if (hal_data->thread_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->thread_free_ptr);
	/* unlink it from the free list */
	hal_data->thread_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_thread_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->uses_fp = 0;
	p->period = 0;
	p->priority = 0;
	p->task_id = 0;
	list_init_entry(&(p->funct_list));
	p->name[0] = '\0';
    }
    return p;
}

void free_thread_struct(hal_thread_t * thread)
{
    hal_funct_entry_t *funct_entry;
    hal_list_t *list_root, *list_entry;
/*! \todo Another #if 0 */
#if 0
    int *prev, next;
    char time[HAL_NAME_LEN + 1], tmax[HAL_NAME_LEN + 1];
    hal_param_t *param;
#endif

    /* if we're deleting a thread, we need to stop all threads */
    hal_data->threads_running = 0;
    /* and stop the task associated with this thread */
    rtapi_task_pause(thread->task_id);
    rtapi_task_delete(thread->task_id);
    /* clear contents of struct */
    thread->uses_fp = 0;
    thread->period = 0;
    thread->priority = 0;
    thread->task_id = 0;
    /* clear the function entry list */
    list_root = &(thread->funct_list);
    list_entry = list_next(list_root);
    while (list_entry != list_root) {
	/* entry found, save pointer to it */
	funct_entry = (hal_funct_entry_t *) list_entry;
	/* unlink it, point to the next one */
	list_entry = list_remove_entry(list_entry);
	/* free the removed entry */
	free_funct_entry_struct(funct_entry);
    }
/*! \todo Another #if 0 */
#if 0
/* Currently these don't get created, so we don't have to worry
   about deleting them.  They will come back when the HAL refactor
   is done, at that time this code or something like it will be
   needed.
*/
    /* need to delete <thread>.time and <thread>.tmax params */
    rtapi_snprintf(time, sizeof(time), "%s.time", thread->name);
    rtapi_snprintf(tmax, sizeof(tmax), "%s.tmax", thread->name);
    /* search the parameter list for those parameters */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (next != 0) {
	param = SHMPTR(next);
	/* does this param match either name? */
	if ((strcmp(param->name, time) == 0)
	    || (strcmp(param->name, tmax) == 0)) {
	    /* yes, unlink from list */
	    *prev = param->next_ptr;
	    /* and delete it */
	    free_param_struct(param);
	} else {
	    /* no match, try the next one */
	    prev = &(param->next_ptr);
	}
	next = *prev;
    }
#endif
    thread->name[0] = '\0';
    /* add thread to free list */
    thread->next_ptr = hal_data->thread_free_ptr;
    hal_data->thread_free_ptr = SHMOFF(thread);
}
#endif /* RTAPI */
