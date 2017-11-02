// HAL thread API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_atomics.h"
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

#ifdef RTAPI

/** 'thread_task()' is a function that is invoked as a realtime task.
    It implements a thread, by running down the thread's function list
    and calling each function in turn.
*/
static void thread_task(void *arg)
{
    hal_thread_t *thread = arg;
    hal_funct_entry_t *funct_root, *funct_entry;
    long long int end_time;
    hal_s32_t delta, act_period;

    thread->cycles = 0;
    thread->mean = 0.0;
    thread->m2 = 0.0;

    // thread execution times collected here, doubles as
    // param struct for xthread functs
    hal_funct_args_t fa = {
	.thread = thread,
	.argc = 0,
	.argv = NULL,
    };

    while (1) {
	if (hal_data->threads_running > 0) {

	    /* point at first function on function list */
	    funct_root = (hal_funct_entry_t *) & (thread->funct_list);
	    funct_entry = SHMPTR(funct_root->links.next);

	    // the thread release point
	    fa.start_time = rtapi_get_time();

	    // expose current invocation period as pin (includes jitter)
	    act_period = fa.start_time - fa.last_start_time;
	    set_s32_pin(thread->curr_period, act_period);

	    fa.last_start_time = fa.thread_start_time = fa.start_time;

	    /* run thru function list */
	    while (funct_entry != funct_root) {
		/* point to function structure */
		fa.funct = SHMPTR(funct_entry->funct_ptr);

		// issue a read barrier if set in funct_entry or
		// funct object header
		if (funct_entry->rmb || ho_rmb(fa.funct)) {
		    rtapi_smp_rmb();
		}

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
		// capture execution time of this funct
		end_time = rtapi_get_time();

		/* update execution time data */
		delta = end_time - fa.start_time;
		set_s32_pin(fa.funct->f_runtime, delta);
		if ( delta > get_s32_pin(fa.funct->f_maxtime)) {
		    set_s32_pin(fa.funct->f_maxtime, delta);
#ifdef ENABLE_TMAX_INC
		    set_bit_pin(fa.funct->f_maxtime_increased, 1);
		} else {
		    set_bit_pin(fa.funct->f_maxtime_increased, 0);
#endif
		}

		// issue a write barrier if set in funct_entry or
		// funct object header
		if (funct_entry->wmb || ho_wmb(fa.funct)) {
		    rtapi_smp_wmb();
		}

		/* point to next next entry in list */
		funct_entry = SHMPTR(funct_entry->links.next);
		/* prepare to measure time for next funct */
		fa.start_time = end_time;
	    }
	    // update thread execution time in this period
	    hal_s32_t rt = (end_time - fa.thread_start_time);
	    set_s32_pin(thread->runtime, rt);
	    if (rt > get_s32_pin(thread->maxtime)) {
		set_s32_pin(thread->maxtime, rt);
	    }
	} else {
	    // threads_running flag false:

	    // nothing to do, so just update actual period

	    if (fa.last_start_time > 0) // avoids spike on first iteration
		act_period = rtapi_get_time() - fa.last_start_time;
	    else
		act_period = thread->period;
	    set_s32_pin(thread->curr_period, act_period);

	    // support actual period measurement (get the starting value right)
	    fa.last_start_time = rtapi_get_time();

            // If a nowait thread is idle, this becomes a tight loop that
            // effectively spinlocks a single core processor. Allow the thread
            // to sleep and give other threads some cpu time.
	    rtapi_wait(thread->flags & ~TF_NOWAIT);
	    continue;
	}

	// update variance to derive a jitter ballpark figure
	// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm
	thread->cycles++;
	double x = (double)act_period;
	double tdelta = x - thread->mean;
	thread->mean += tdelta/thread->cycles;
	thread->m2 += tdelta * (x - thread->mean);

	/* wait until next period */
	rtapi_wait(thread->flags);
    }
}

// HAL threads - public API

int hal_create_xthread(const hal_threadargs_t *args)
{
    int prev_priority;
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
	HALFAIL_RC(EINVAL,"create_thread called "
		   "with period of zero");
    }
    {
	WITH_HAL_MUTEX();

	if (halg_find_object_by_name(0, HAL_THREAD, args->name).thread) {
	    HALFAIL_RC(EINVAL, "duplicate thread name %s", args->name);
	}

	// allocate thread descriptor
	if ((new = halg_create_objectf(0, sizeof(hal_thread_t),
				       HAL_THREAD, 0, args->name)) == NULL)
	    return _halerrno;

	dlist_init_entry(&(new->funct_list));

	/* initialize the structure */
	new->uses_fp = args->uses_fp;
	new->cpu_id = args->cpu_id;
	new->flags = args->flags;

	/* have to create and start a task to run the thread */
	if (dlist_empty(&hal_data->threads)) {

	    /* this is the first thread created */
	    /* is timer started? if so, what period? */
	    curr_period = rtapi_clock_set_period(0);
	    if (curr_period == 0) {
		/* not running, start it */
		curr_period = rtapi_clock_set_period(args->period_nsec);
		if (curr_period < 0) {
		    HALFAIL_RC(EINVAL, "clock_set_period returned %ld",
				    curr_period);
		}
	    }
	    /* make sure period <= desired period (allow 1% roundoff error) */
	    if (curr_period > (args->period_nsec + (args->period_nsec / 100))) {
		HALFAIL_RC(EINVAL, "clock period too long: %ld", curr_period);
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

	    tptr = dlist_first_entry(&hal_data->threads, hal_thread_t, thread);
	    // tptr = SHMPTR(hal_data->thread_list_ptr);
	    prev_period = tptr->period;
	    prev_priority = tptr->priority;
	}
	if ( args->period_nsec < hal_data->base_period) {
	    HALFAIL_RC(EINVAL, "new thread period %ld is less than clock period %ld",
		   args->period_nsec, hal_data->base_period);
	}
	/* make period an integer multiple of the timer period */
	n = (args->period_nsec + hal_data->base_period / 2) / hal_data->base_period;
	new->period = hal_data->base_period * n;
	if ( new->period < prev_period ) {
	    HALFAIL_RC(EINVAL, "new thread period %ld is less than existing thread period %ld",
		   args->period_nsec, prev_period);
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
	    .name = (char *)ho_name(new),
	    .flags = new->flags,
	};
	retval = rtapi_task_new(&rargs);
	if (retval < 0) {
	    HALFAIL_RC(EINVAL, "could not create task for thread %s", args->name);
	}
	new->task_id = retval;
	new->runtime._sp = hal_off_safe(halg_pin_newf(0, HAL_S32, HAL_OUT,
						      NULL, lib_module_id,
						      "%s.time", args->name));
	new->maxtime._sp = hal_off_safe(halg_pin_newf(0, HAL_S32, HAL_IO, NULL,
						      lib_module_id,
						      "%s.tmax", args->name));
	new->curr_period._sp = hal_off_safe(halg_pin_newf(0, HAL_S32, HAL_OUT, NULL,
							 lib_module_id,
							 "%s.curr-period", args->name));

	// expose nominal period for a start
	set_s32_pin(new->curr_period, new->period);

	/* start task */
	retval = rtapi_task_start(new->task_id, new->period);
	if (retval < 0) {
	    HALFAIL_RC(EINVAL, "could not start task for thread %s: %d", args->name, retval);
	}
	/* insert new structure at head of list */
	dlist_add_before(&new->thread, &hal_data->threads);

	// make it visible
	halg_add_object(false, (hal_object_ptr)new);

    } // exit block protected by scoped lock


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

static int delete_thread_cb(hal_object_ptr o, foreach_args_t *args)
{
    free_pin_struct(hal_ptr(o.thread->runtime._sp));
    free_pin_struct(hal_ptr(o.thread->maxtime._sp));
    free_pin_struct(hal_ptr(o.thread->curr_period._sp));
    free_thread_struct(o.thread);
    return 0;
}

// delete a named thread, or all threads if name == NULL
int halg_exit_thread(const int use_hal_mutex, const char *name)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_RUN);

    hal_data->threads_running = 0;
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	foreach_args_t args =  {
	    .type = HAL_THREAD,
	    .name = (char *)name
	};
	int ret = halg_foreach(0, &args, delete_thread_cb);
	if (name && (ret == 0)) {
	    HALFAIL_RC(EINVAL, "thread '%s' not found",   name);
	}
	HALDBG("%d thread%s exited", ret, ret == 1 ? "":"s");
	// all threads stopped & deleted
    }
    return 0;
}

extern int hal_thread_delete(const char *name)
{
    CHECK_STR(name);
    HALDBG("deleting thread '%s'", name);
    return halg_exit_thread(1, name);
}

int hal_exit_threads(void)
{
    return halg_exit_thread(1, NULL);
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

#ifdef RTAPI

void free_thread_struct(hal_thread_t * thread)
{
    hal_funct_entry_t *funct_entry;
    hal_list_t *list_root, *list_entry;

    /* if we're deleting a thread, we need to stop all threads */
    hal_data->threads_running = 0;

    /* and stop the task associated with this thread */
    rtapi_task_pause(thread->task_id);
    rtapi_task_delete(thread->task_id);

    /* clear the function entry list */
    list_root = &(thread->funct_list);
    list_entry = dlist_next(list_root);
    while (list_entry != list_root) {
	/* entry found, save pointer to it */
	funct_entry = (hal_funct_entry_t *) list_entry;
	/* unlink it, point to the next one */
	list_entry = dlist_remove_entry(list_entry);
	/* free the removed entry */
	free_funct_entry_struct(funct_entry);
    }
    // remove from priority list
    dlist_remove_entry(&thread->thread);
    halg_free_object(false, (hal_object_ptr) thread);
}
#endif /* RTAPI */
