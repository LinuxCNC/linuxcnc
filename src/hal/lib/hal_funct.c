// HAL funct API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

static hal_funct_entry_t *alloc_funct_entry_struct(void);

#ifdef RTAPI
hal_funct_t *alloc_funct_struct(void);
static int hal_export_xfunctfv(const hal_export_xfunct_args_t *xf, const char *fmt, va_list ap);

// printf-style version of hal_export_funct
int hal_export_functf(void (*funct) (void *, long),
		      void *arg,
		      int uses_fp,
		      int reentrant,
		      int owner_id,
		      const char *fmt, ... )
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    hal_export_xfunct_args_t xf = {
	.type = FS_LEGACY_THREADFUNC,
	.funct.l = funct,
	.arg = arg,
	.uses_fp = uses_fp,
	.reentrant = reentrant,
	.owner_id = owner_id,
    };
    ret = hal_export_xfunctfv(&xf, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_export_xfunctf( const hal_export_xfunct_args_t *xf, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_export_xfunctfv(xf, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_export_funct(const char *name, void (*funct) (void *, long),
		     void *arg, int uses_fp, int reentrant, int owner_id)
{
    return hal_export_functf(funct, arg, uses_fp, reentrant, owner_id, name);
}

static int hal_export_xfunctfv(const hal_export_xfunct_args_t *xf, const char *fmt, va_list ap)
{
    int *prev, next, cmp, sz;
    hal_funct_t *nf, *fptr;
    char name[HAL_NAME_LEN + 1];

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);

    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        HALERR("length %d invalid for name starting '%s'", sz, name);
        return -ENOMEM;
    }

    HALDBG("exporting function '%s' type %d", name, xf->type);
    {
	hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	comp = halpr_find_owning_comp(xf->owner_id);
	if (comp == 0) {
	    /* bad comp_id */
	    HALERR("funct '%s': owning component %d not found",
		   name, xf->owner_id);
	    return -EINVAL;
	}

	if (comp->type == TYPE_USER) {
	    /* not a realtime component */
	    HALERR("funct '%s': component %s/%d is not realtime (%d)",
		   name, comp->name, comp->comp_id, comp->type);
	    return -EINVAL;
	}

	bool legacy = (halpr_find_inst_by_id(xf->owner_id) == NULL);

	// instances may export functs post hal_ready
	if (legacy && (comp->state > COMP_INITIALIZING)) {
	    HALERR("funct '%s': called after hal_ready", name);
	    return -EINVAL;
	}
	/* allocate a new function structure */
	nf = alloc_funct_struct();
	if (nf == 0)
	    NOMEM("function '%s'", name);

	/* initialize the structure */
	nf->uses_fp = xf->uses_fp;
	nf->owner_id = xf->owner_id;
	nf->reentrant = xf->reentrant;
	nf->users = 0;
	nf->handle = rtapi_next_handle();
	nf->arg = xf->arg;
	nf->type = xf->type;
	nf->funct.l = xf->funct.l; // a bit of a cheat really
	rtapi_snprintf(nf->name, sizeof(nf->name), "%s", name);
	/* search list for 'name' and insert new structure */
	prev = &(hal_data->funct_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		/* reached end of list, insert here */
		nf->next_ptr = next;
		*prev = SHMOFF(nf);
		/* break out of loop and init the new function */
		break;
	    }
	    fptr = SHMPTR(next);
	    cmp = strcmp(fptr->name, nf->name);
	    if (cmp > 0) {
		/* found the right place for it, insert here */
		nf->next_ptr = next;
		*prev = SHMOFF(nf);
		/* break out of loop and init the new function */
		break;
	    }
	    if (cmp == 0) {
		/* name already in list, can't insert */
		free_funct_struct(nf);
		HALERR("duplicate function '%s'", name);
		return -EINVAL;
	    }
	    /* didn't find it yet, look at next one */
	    prev = &(fptr->next_ptr);
	    next = *prev;
	}
	// at this point we have a new function and can
	// yield the mutex by scope exit

    }
    /* init time logging variables */
    nf->runtime = 0;
    nf->maxtime = 0;
    nf->maxtime_increased = 0;

    /* at this point we have a new function and can yield the mutex */
    rtapi_mutex_give(&(hal_data->mutex));

    switch (xf->type) {
    case FS_LEGACY_THREADFUNC:
    case FS_XTHREADFUNC:
	/* create a pin with the function's runtime in it */
	if (hal_pin_s32_newf(HAL_OUT, &(nf->runtime), xf->owner_id, "%s.time",name)) {
	    HALERR("failed to create pin '%s.time'", name);
	    return -EINVAL;
	}
	*(nf->runtime) = 0;

	/* note that failure to successfully create the following params
	   does not cause the "export_funct()" call to fail - they are
	   for debugging and testing use only */
	/* create a parameter with the function's maximum runtime in it */
	nf->maxtime = 0;
	hal_param_s32_newf(HAL_RW,  &(nf->maxtime), xf->owner_id, "%s.tmax", name);

	/* create a parameter with the function's maximum runtime in it */
	nf->maxtime_increased = 0;
	hal_param_bit_newf(HAL_RO, &(nf->maxtime_increased), xf->owner_id,
			    "%s.tmax-inc", name);
	break;
    case FS_USERLAND: // no timing pins/params
	;
    }

    return 0;
}
#endif // RTAPI

int hal_call_usrfunct(const char *name, const int argc, const char **argv, int *ureturn)
{
    hal_funct_t *funct;

    CHECK_HALDATA();
    CHECK_STR(name);

    if (argc && (argv == NULL)) {
	HALERR("funct '%s': argc=%d but argv is NULL", name, argc);
	return -EINVAL;
    }

    {
	int i __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	funct = halpr_find_funct_by_name(name);
	if (funct == NULL) {
	    HALERR("funct '%s' not found", name);
	    return -ENOENT;
	}

	if (funct->type != FS_USERLAND) {
	    HALERR("funct '%s': invalid type %d", name, funct->type);
	    return -ENOENT;
	}

	// argv sanity check - we dont want to fail this, esp in kernel land
	for (i = 0; i < argc; i++) {
	    if (argv[i] == NULL) {
		HALERR("funct '%s': argc=%d but argv[%d] is NULL",
		       name, i, i);
		return -EINVAL;
	    }
	}
    }
    // call the function with rtapi_mutex unlocked
    long long int now = rtapi_get_clocks();

    hal_funct_args_t fa = {
	.thread_start_time = now,
	.start_time = now,
	.thread = NULL,
	.funct = funct,
	.argc = argc,
	.argv = argv,
    };
    int retval = funct->funct.u(&fa);
    if (ureturn)
	*ureturn = retval;
    return 0;
}

int hal_add_funct_to_thread(const char *funct_name,
			    const char *thread_name, int position)
{
    hal_funct_t *funct;
    hal_list_t *list_root, *list_entry;
    int n;
    hal_funct_entry_t *funct_entry;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STR(funct_name);
    CHECK_STR(thread_name);

    HALDBG("adding function '%s' to thread '%s'", funct_name, thread_name);
    {
	hal_thread_t *thread __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing data structures */
	rtapi_mutex_get(&(hal_data->mutex));

	/* make sure position is valid */
	if (position == 0) {
	    /* zero is not allowed */
	    HALERR("bad position: 0");
	    return -EINVAL;
	}

	/* search function list for the function */
	funct = halpr_find_funct_by_name(funct_name);
	if (funct == 0) {
	    HALERR("function '%s' not found", funct_name);
	    return -EINVAL;
	}
	// type-check the functions which go onto threads
	switch (funct->type) {
	case FS_LEGACY_THREADFUNC:
	case FS_XTHREADFUNC:
	    break;
	default:
	    HALERR("cant add type %d function '%s' "
		   "to a thread", funct->type, funct_name);
	    return -EINVAL;
	}
	/* found the function, is it available? */
	if ((funct->users > 0) && (funct->reentrant == 0)) {
	    HALERR("function '%s' may only be added "
		   "to one thread", funct_name);
	    return -EINVAL;
	}
	/* search thread list for thread_name */
	thread = halpr_find_thread_by_name(thread_name);
	if (thread == 0) {
	    /* thread not found */
	    HALERR("thread '%s' not found", thread_name);
	    return -EINVAL;
	}
	/* ok, we have thread and function, are they compatible? */
	if ((funct->uses_fp) && (!thread->uses_fp)) {
	    HALERR("function '%s' needs FP", funct_name);
	    return -EINVAL;
	}
	/* find insertion point */
	list_root = &(thread->funct_list);
	list_entry = list_root;
	n = 0;
	if (position > 0) {
	    /* insertion is relative to start of list */
	    while (++n < position) {
		/* move further into list */
		list_entry = list_next(list_entry);
		if (list_entry == list_root) {
		    /* reached end of list */
		    HALERR("position '%d' is too high", position);
		    return -EINVAL;
		}
	    }
	} else {
	    /* insertion is relative to end of list */
	    while (--n > position) {
		/* move further into list */
		list_entry = list_prev(list_entry);
		if (list_entry == list_root) {
		    /* reached end of list */
		    HALERR("position '%d' is too low", position);
		    return -EINVAL;
		}
	    }
	    /* want to insert before list_entry, so back up one more step */
	    list_entry = list_prev(list_entry);
	}
	/* allocate a funct entry structure */
	funct_entry = alloc_funct_entry_struct();
	if (funct_entry == 0)
	    NOMEM("thread->function link");

	/* init struct contents */
	funct_entry->funct_ptr = SHMOFF(funct);
	funct_entry->arg = funct->arg;
	funct_entry->funct.l = funct->funct.l;
	funct_entry->type = funct->type;
	/* add the entry to the list */
	list_add_after((hal_list_t *) funct_entry, list_entry);
	/* update the function usage count */
	funct->users++;
    }
    return 0;
}

int hal_del_funct_from_thread(const char *funct_name, const char *thread_name)
{
    hal_funct_t *funct;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *funct_entry;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STR(funct_name);
    CHECK_STR(thread_name);

    HALDBG("removing function '%s' from thread '%s'", funct_name, thread_name);
    {
	hal_thread_t *thread __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing data structures */
	rtapi_mutex_get(&(hal_data->mutex));

	/* search function list for the function */
	funct = halpr_find_funct_by_name(funct_name);
	if (funct == 0) {
	    HALERR("function '%s' not found", funct_name);
	    return -EINVAL;
	}
	/* found the function, is it in use? */
	if (funct->users == 0) {
	    HALERR("function '%s' is not in use", funct_name);
	    return -EINVAL;
	}
	/* search thread list for thread_name */
	thread = halpr_find_thread_by_name(thread_name);
	if (thread == 0) {
	    /* thread not found */
	    HALERR("thread '%s' not found", thread_name);
	    return -EINVAL;
	}
	/* ok, we have thread and function, does thread use funct? */
	list_root = &(thread->funct_list);
	list_entry = list_next(list_root);
	while (1) {
	    if (list_entry == list_root) {
		/* reached end of list, funct not found */
		HALERR("thread '%s' doesn't use %s",
		       thread_name, funct_name);
		return -EINVAL;
	    }
	    funct_entry = (hal_funct_entry_t *) list_entry;
	    if (SHMPTR(funct_entry->funct_ptr) == funct) {
		/* this funct entry points to our funct, unlink */
		list_remove_entry(list_entry);
		/* and delete it */
		free_funct_entry_struct(funct_entry);
		/* done */
		return 0;
	    }
	    /* try next one */
	    list_entry = list_next(list_entry);
	}
    }
}

static hal_funct_entry_t *alloc_funct_entry_struct(void)
{
    hal_list_t *freelist, *l;
    hal_funct_entry_t *p;

    /* check the free list */
    freelist = &(hal_data->funct_entry_free);
    l = list_next(freelist);
    if (l != freelist) {
	/* found a free structure, unlink from the free list */
	list_remove_entry(l);
	p = (hal_funct_entry_t *) l;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_funct_entry_t));
	l = (hal_list_t *) p;
	list_init_entry(l);
    }
    if (p) {
	/* make sure it's empty */
	p->funct_ptr = 0;
	p->arg = 0;
	p->funct.l = 0;
    }
    return p;
}

void free_funct_entry_struct(hal_funct_entry_t * funct_entry)
{
    hal_funct_t *funct;

    if (funct_entry->funct_ptr > 0) {
	/* entry points to a function, update the function struct */
	funct = SHMPTR(funct_entry->funct_ptr);
	funct->users--;
    }
    /* clear contents of struct */
    funct_entry->funct_ptr = 0;
    funct_entry->arg = 0;
    funct_entry->funct.l = 0;
    /* add it to free list */
    list_add_after((hal_list_t *) funct_entry, &(hal_data->funct_entry_free));
}


hal_funct_t *halpr_find_funct_by_name(const char *name)
{
    int next;
    hal_funct_t *funct;

    /* search function list for 'name' */
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	funct = SHMPTR(next);
	if (strcmp(funct->name, name) == 0) {
	    /* found a match */
	    return funct;
	}
	/* didn't find it yet, look at next one */
	next = funct->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

// find a funct by owner id, which may refer to a instance or a comp
hal_funct_t *halpr_find_funct_by_owner_id(const int owner_id,
					  hal_funct_t * start)
{
    int next;
    hal_funct_t *funct;

    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of function list */
	next = hal_data->funct_list_ptr;
    } else {
	/* no, start at next function */
	next = start->next_ptr;
    }
    while (next != 0) {
	funct = SHMPTR(next);
	if (funct->owner_id == owner_id) {
	    /* found a match */
	    return funct;
	}
	/* didn't find it yet, look at next one */
	next = funct->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

#ifdef RTAPI

hal_funct_t *alloc_funct_struct(void)
{
    hal_funct_t *p;

    /* check the free list */
    if (hal_data->funct_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->funct_free_ptr);
	/* unlink it from the free list */
	hal_data->funct_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_funct_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->uses_fp = 0;
	p->owner_id = 0;
	p->reentrant = 0;
	p->users = 0;
	p->arg = 0;
	p->funct.l = 0;
	p->name[0] = '\0';
    }
    return p;
}


void free_funct_struct(hal_funct_t * funct)
{
    int next_thread;
    hal_thread_t *thread;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *funct_entry;

    if (funct->users > 0) {
	/* We can't casually delete the function, there are thread(s) which
	   will call it.  So we must check all the threads and remove any
	   funct_entrys that call this function */
	/* start at root of thread list */
	next_thread = hal_data->thread_list_ptr;
	/* run through thread list */
	while (next_thread != 0) {
	    /* point to thread */
	    thread = SHMPTR(next_thread);
	    /* start at root of funct_entry list */
	    list_root = &(thread->funct_list);
	    list_entry = list_next(list_root);
	    /* run thru funct_entry list */
	    while (list_entry != list_root) {
		/* point to funct entry */
		funct_entry = (hal_funct_entry_t *) list_entry;
		/* test it */
		if (SHMPTR(funct_entry->funct_ptr) == funct) {
		    /* this funct entry points to our funct, unlink */
		    list_entry = list_remove_entry(list_entry);
		    /* and delete it */
		    free_funct_entry_struct(funct_entry);
		} else {
		    /* no match, try the next one */
		    list_entry = list_next(list_entry);
		}
	    }
	    /* move on to the next thread */
	    next_thread = thread->next_ptr;
	}
    }
    /* clear contents of struct */
    funct->uses_fp = 0;
    funct->owner_id = 0;
    funct->reentrant = 0;
    funct->users = 0;
    funct->arg = 0;
    funct->funct.l = 0;
    funct->runtime = 0;
    funct->name[0] = '\0';

    /* add it to free list */
    funct->next_ptr = hal_data->funct_free_ptr;
    hal_data->funct_free_ptr = SHMOFF(funct);
}
#endif
