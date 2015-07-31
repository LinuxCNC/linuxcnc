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
    hal_thread_t *thread = arg;
    hal_funct_entry_t *funct_root, *funct_entry;
    long long int end_time;

    // thread execution times collected here, doubles as
    // param struct for xthread functs
    hal_funct_args_t fa = {
	.thread = thread,
	.argc = 0,
	.argv = NULL,
    };
    bool do_wait = ((thread->flags & TF_NOWAIT) == 0);

    while (1) {
	if (hal_data->threads_running > 0) {
	    /* point at first function on function list */
	    funct_root = (hal_funct_entry_t *) & (thread->funct_list);
	    funct_entry = SHMPTR(funct_root->links.next);
	    /* execution time logging */
	    fa.start_time = rtapi_get_clocks();
	    end_time = fa.start_time;
	    fa.thread_start_time = fa.start_time;

	    /* run thru function list */
	    while (funct_entry != funct_root) {
		/* point to function structure */
		fa.funct = SHMPTR(funct_entry->funct_ptr);

		/* call the function */
		switch (funct_entry->type) {
		case FS_LEGACY_THREADFUNC:
		    funct_entry->funct.l(funct_entry->arg, thread->period);
		    break;
		case FS_XTHREADFUNC:
		    funct_entry->funct.x(funct_entry->arg, &fa);
		    break;
		default:
		    // bad - a mistyped funct
		    ;
		}
		/* capture execution time */
		end_time = rtapi_get_clocks();
		/* update execution time data */
		*(fa.funct->runtime) = (hal_s32_t)(end_time - fa.start_time);
		if ( *(fa.funct->runtime) > fa.funct->maxtime) {
		    fa.funct->maxtime = *(fa.funct->runtime);
		    fa.funct->maxtime_increased = 1;
		} else {
		    fa.funct->maxtime_increased = 0;
		}
		/* point to next next entry in list */
		funct_entry = SHMPTR(funct_entry->links.next);
		/* prepare to measure time for next funct */
		fa.start_time = end_time;
	    }
	    /* update thread execution time */
	    thread->runtime = (hal_s32_t)(end_time - fa.thread_start_time);
	    if (thread->runtime > thread->maxtime) {
		thread->maxtime = thread->runtime;
	    }
	}
	/* wait until next period */
	if (do_wait)
	    rtapi_wait();
    }
}

// HAL threads - public API

int hal_create_xthread(const hal_threadargs_t *args)
{
    int next, prev_priority;
    int retval, n;
    hal_thread_t *new, *tptr;
    long prev_period, curr_period;

    CHECK_NULL(args);
    CHECK_STRLEN(args->name, HAL_NAME_LEN);
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    HALDBG("creating thread %s, %ld nsec fp=%d\n",
	   args->name,
	   args->period_nsec,
	   args->uses_fp);

    if (args->period_nsec == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: create_thread called "
			"with period of zero");
	return -EINVAL;
    }
    {
	int  cmp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* make sure name is unique on thread list */
	next = hal_data->thread_list_ptr;
	while (next != 0) {
	    tptr = SHMPTR(next);
	    cmp = strcmp(tptr->name, args->name);
	    if (cmp == 0) {
		/* name already in list, can't insert */
		HALERR("duplicate thread name %s", args->name);
		return -EINVAL;
	    }
	    /* didn't find it yet, look at next one */
	    next = tptr->next_ptr;
	}
	/* allocate a new thread structure */
	new = alloc_thread_struct();
	if (new == 0) {
	    /* alloc failed */
	    HALERR("insufficient memory to create thread");
	    return -ENOMEM;
	}
	/* initialize the structure */
	new->uses_fp = args->uses_fp;
	new->cpu_id = args->cpu_id;
	new->handle = rtapi_next_handle();
	new->flags = args->flags;
	rtapi_snprintf(new->name, sizeof(new->name), "%s", args->name);
	/* have to create and start a task to run the thread */
	if (hal_data->thread_list_ptr == 0) {
	    /* this is the first thread created */
	    /* is timer started? if so, what period? */
	    curr_period = rtapi_clock_set_period(0);
	    if (curr_period == 0) {
		/* not running, start it */
		curr_period = rtapi_clock_set_period(args->period_nsec);
		if (curr_period < 0) {
		    HALERR("clock_set_period returned %ld",
				    curr_period);
		    return -EINVAL;
		}
	    }
	    /* make sure period <= desired period (allow 1% roundoff error) */
	    if (curr_period > (args->period_nsec + (args->period_nsec / 100))) {
		HALERR("clock period too long: %ld", curr_period);
		return -EINVAL;
	    }
	    if(hal_data->exact_base_period) {
		hal_data->base_period = args->period_nsec;
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
	if ( args->period_nsec < hal_data->base_period) {
	    HALERR("new thread period %ld is less than clock period %ld",
		   args->period_nsec, hal_data->base_period);
	    return -EINVAL;
	}
	/* make period an integer multiple of the timer period */
	n = (args->period_nsec + hal_data->base_period / 2) / hal_data->base_period;
	new->period = hal_data->base_period * n;
	if ( new->period < prev_period ) {
	    HALERR("new thread period %ld is less than existing thread period %ld",
		   args->period_nsec, prev_period);
	    return -EINVAL;
	}
	/* make priority one lower than previous */
	new->priority = rtapi_prio_next_lower(prev_priority);

	/* create task - owned by library module, not caller */

	rtapi_task_args_t rargs = {
	    .taskcode = thread_task,
	    .arg = new,
	    .prio = new->priority,
	    .owner = lib_module_id,
	    .stacksize = global_data->hal_thread_stack_size,
	    .uses_fp = new->uses_fp,
	    .cpu_id =new->cpu_id,
	    .name = new->name,
	    .flags = new->flags,
	};
	retval = rtapi_task_new(&rargs);
	if (retval < 0) {
	    HALERR("could not create task for thread %s", args->name);
	    return -EINVAL;
	}
	new->task_id = retval;
	/* start task */
	retval = rtapi_task_start(new->task_id, new->period);
	if (retval < 0) {
	    HALERR("could not start task for thread %s: %d", args->name, retval);
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

    HALDBG("thread %s id %d created prio=%d",
	   args->name, new->task_id, new->priority);
    return 0;
}

// HAL threads - legacy API
int hal_create_thread(const char *name, unsigned long period_nsec,
		      int uses_fp, int cpu_id) {
    hal_threadargs_t args = {
	.name = name,
	.period_nsec = period_nsec,
	.uses_fp = uses_fp,
	.cpu_id = cpu_id,
	.flags = 0,
    };
    return hal_create_xthread(&args);
}

extern int hal_thread_delete(const char *name)
{
    int *prev, next;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STR(name);

    HALDBG("deleting thread '%s'", name);
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
    HALERR("thread '%s' not found",   name);
    return -EINVAL;
}

// internal use from hal_lib.c:rtapi_app_exit() only
int hal_exit_threads(void)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_RUN);

    hal_data->threads_running = 0;
    {
	hal_thread_t *thread __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));

	while (hal_data->thread_list_ptr != 0) {
	    /* point to a thread */
	    thread = SHMPTR(hal_data->thread_list_ptr);
	    /* unlink from list */
	    hal_data->thread_list_ptr = thread->next_ptr;
	    /* and delete it */
	    free_thread_struct(thread);
	}
	// all threads stopped & deleted
    }
    HALDBG("all threads exited");
    return 0;
}
#endif /* RTAPI */


int hal_start_threads(void)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_RUN);

    HALDBG("starting threads");
    hal_data->threads_running = 1;
    return 0;
}

int hal_stop_threads(void)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_RUN);

    hal_data->threads_running = 0;
    HALDBG("threads stopped");
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

    thread->name[0] = '\0';
    /* add thread to free list */
    thread->next_ptr = hal_data->thread_free_ptr;
    hal_data->thread_free_ptr = SHMOFF(thread);
}
#endif /* RTAPI */
