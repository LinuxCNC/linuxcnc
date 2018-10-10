// HAL funct API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

static hal_funct_entry_t *alloc_funct_entry_struct(void);

#ifdef RTAPI
hal_funct_t *alloc_funct_struct(void);
static int halg_export_xfunctfv(const int use_halmutex,
				const hal_export_xfunct_args_t *xf,
				const char *fmt, va_list ap);

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
    ret = halg_export_xfunctfv(1, &xf, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_export_xfunctf( const hal_export_xfunct_args_t *xf, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = halg_export_xfunctfv(1, xf, fmt, ap);
    va_end(ap);
    return ret;
}
int halg_export_xfunctf(const int use_halmutex,
			const hal_export_xfunct_args_t *xf,
			const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = halg_export_xfunctfv(use_halmutex, xf, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_export_funct(const char *name, void (*funct) (void *, long),
		     void *arg, int uses_fp, int reentrant, int owner_id)
{
    return hal_export_functf(funct, arg, uses_fp, reentrant, owner_id, "%s", name);
}

static int halg_export_xfunctfv(const int use_hal_mutex,
			       const hal_export_xfunct_args_t *xf,
			       const char *fmt,
			       va_list ap)
{
    int sz;
    hal_funct_t *nf;
    char name[HAL_NAME_LEN + 1];

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);

    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        HALFAIL_RC(ENOMEM,"length %d invalid for name starting '%s'", sz, name);
    }
    HALDBG("exporting function '%s' type %d fp=%d owner=%d",
	   name, xf->type, xf->uses_fp, xf->owner_id);

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	if (halpr_find_funct_by_name(name)) {
	    HALFAIL_RC(EINVAL, "funct '%s' already exists", name);
	}

	hal_comp_t *comp = halpr_find_owning_comp(xf->owner_id);
	if (comp == NULL) {
	    HALFAIL_RC(EINVAL, "funct '%s': owning component %d not found",
		   name, xf->owner_id);
	}

	if (comp->type == TYPE_USER) {
	    /* not a realtime component */
	    HALFAIL_RC(EINVAL, "funct '%s': component %s/%d is not realtime (%d)",
		   name, ho_name(comp), ho_id(comp), comp->type);
	}

	bool legacy = (halpr_find_inst_by_id(xf->owner_id) == NULL);

	// instances may export functs post hal_ready
	if (legacy && (comp->state > COMP_INITIALIZING)) {
	    HALFAIL_RC(EINVAL,"funct '%s': called after hal_ready", name);
	}

	// allocate a new function structure
	nf = halg_create_objectf(false, sizeof(hal_funct_t),
				 HAL_FUNCT, xf->owner_id, name);
	if (nf == NULL)
	    return _halerrno;

	/* initialize the structure */
	nf->uses_fp = xf->uses_fp;
	nf->reentrant = xf->reentrant;
	nf->users = 0;
	nf->arg = xf->arg;
	nf->type = xf->type;
	nf->funct.l = xf->funct.l; // a bit of a cheat really

	halg_add_object(false, (hal_object_ptr)nf);
    }
    // unlocked here

    switch (xf->type) {
    case FS_LEGACY_THREADFUNC:
    case FS_XTHREADFUNC:
	nf->f_runtime = halx_pin_s32_newf(HAL_OUT, xf->owner_id,"%s.time",name);
	nf->f_maxtime = halx_pin_s32_newf(HAL_IO, xf->owner_id,"%s.tmax",name);
	nf->f_maxtime_increased =
	    halx_pin_bit_newf(HAL_OUT,xf->owner_id,"%s.tmax-inc",name);
	// TBD: check success of above
	break;
    case FS_USERLAND: // no timing pins/params
	;
    }

    return 0;
}
#endif // RTAPI

int hal_call_usrfunct(const char *name, const int argc, char * const *argv,
                      int *ureturn)
{
    hal_funct_t *funct;
    int i;

    CHECK_HALDATA();
    CHECK_STR(name);

    if (argc && (argv == NULL)) {
	HALFAIL_RC(EINVAL,"funct '%s': argc=%d but argv is NULL", name, argc);
    }

    {
	WITH_HAL_MUTEX();

	funct = halpr_find_funct_by_name(name);
	if (funct == NULL) {
	    HALFAIL_RC(ENOENT,"funct '%s' not found", name);
	}

	if (funct->type != FS_USERLAND) {
	    HALFAIL_RC(ENOENT,"funct '%s': invalid type %d", name, funct->type);
	}

	// argv sanity check - we dont want to fail this, esp in kernel land
	for (i = 0; i < argc; i++) {
	    if (argv[i] == NULL) {
		HALFAIL_RC(EINVAL,"funct '%s': argc=%d but argv[%d] is NULL",
		       name, i, i);
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
			    const char *thread_name,
			    const int position,
			    const int read_barrier,
			    const int write_barrier)
{
    hal_funct_t *funct;
    hal_list_t *list_root, *list_entry;
    int n;
    hal_funct_entry_t *funct_entry;
    char buff[HAL_NAME_LEN + 1];
    rtapi_snprintf(buff, HAL_NAME_LEN, "%s.funct", funct_name);

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STR(funct_name);
    CHECK_STR(thread_name);

    HALDBG("adding function '%s' to thread '%s'", funct_name, thread_name);
    {
	WITH_HAL_MUTEX();

	hal_thread_t *thread;

	/* make sure position is valid */
	if (position == 0) {
	    /* zero is not allowed */
	    HALFAIL_RC(EINVAL, "bad position: 0");
	}

	/* search function list for the function */
	funct = halpr_find_funct_by_name(funct_name);
	if (funct == NULL) {
	    funct = halpr_find_funct_by_name((const char*)buff);
	    if (funct == NULL) {
		HALFAIL_RC(EINVAL,"function '%s' not found", funct_name);
	    } else
		HALWARN("'%s' should be added to thread as '%s' ", funct_name, buff);
	}
	// type-check the functions which go onto threads
	switch (funct->type) {
	case FS_LEGACY_THREADFUNC:
	case FS_XTHREADFUNC:
	    break;
	default:
	    HALFAIL_RC(EINVAL, "cant add type %d function '%s' "
		   "to a thread", funct->type, funct_name);
	}
	/* found the function, is it available? */
	if ((funct->users > 0) && (funct->reentrant == 0)) {
	    HALFAIL_RC(EINVAL, "function '%s' may only be added "
		       "to one thread", funct_name);
	}
	/* search thread list for thread_name */
	thread = halpr_find_thread_by_name(thread_name);
	if (thread == 0) {
	    /* thread not found */
	    HALFAIL_RC(EINVAL, "thread '%s' not found", thread_name);
	}
	/* ok, we have thread and function, are they compatible? */
	if ((funct->uses_fp) && (!thread->uses_fp)) {
	    HALFAIL_RC(EINVAL, "function '%s' needs FP", funct_name);
	}
	/* find insertion point */
	list_root = &(thread->funct_list);
	list_entry = list_root;
	n = 0;
	if (position > 0) {
	    /* insertion is relative to start of list */
	    while (++n < position) {
		/* move further into list */
		list_entry = dlist_next(list_entry);
		if (list_entry == list_root) {
		    /* reached end of list */
		    HALFAIL_RC(EINVAL, "position '%d' is too high", position);
		}
	    }
	} else {
	    /* insertion is relative to end of list */
	    while (--n > position) {
		/* move further into list */
		list_entry = dlist_prev(list_entry);
		if (list_entry == list_root) {
		    /* reached end of list */
		    HALFAIL_RC(EINVAL, "position '%d' is too low", position);
		}
	    }
	    /* want to insert before list_entry, so back up one more step */
	    list_entry = dlist_prev(list_entry);
	}
	/* allocate a funct entry structure */
	funct_entry = alloc_funct_entry_struct();
	if (funct_entry == 0)
	    NOMEM("thread->function link");

	/* init struct contents */
	funct_entry->funct_ptr = SHMOFF(funct);
	funct_entry->arg = funct->arg;
	funct_entry->funct.l = funct->funct.l;
	funct_entry->rmb = read_barrier;
	funct_entry->wmb = write_barrier;
	funct_entry->type = funct->type;

	/* add the entry to the list */
	dlist_add_after((hal_list_t *) funct_entry, list_entry);
	/* update the function usage count */
	funct->users++;
    }
    return 0;
}

int hal_del_funct_from_thread(const char *funct_name, const char *thread_name)
{

    char buff[HAL_NAME_LEN + 1];
    rtapi_snprintf(buff, HAL_NAME_LEN, "%s.funct", funct_name);

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STR(funct_name);
    CHECK_STR(thread_name);

    HALDBG("removing function '%s' from thread '%s'", funct_name, thread_name);
    {
	WITH_HAL_MUTEX();

	hal_thread_t *thread;

	/* search function list for the function */
	hal_funct_t *funct = halpr_find_funct_by_name(funct_name);
	if (funct == NULL) {
	    funct = halpr_find_funct_by_name((const char*)buff);
	    if (funct == NULL) {
		HALFAIL_RC(EINVAL, "function '%s' not found", funct_name);
	    }
	}
	/* found the function, is it in use? */
	if (funct->users == 0) {
	    HALFAIL_RC(EINVAL, "function '%s' is not in use", funct_name);
	}
	/* search thread list for thread_name */
	thread = halpr_find_thread_by_name(thread_name);
	if (thread == 0) {
	    /* thread not found */
	    HALFAIL_RC(EINVAL, "thread '%s' not found", thread_name);
	}
	/* ok, we have thread and function, does thread use funct? */
	hal_list_t *list_root = &(thread->funct_list);
	hal_list_t *list_entry = dlist_next(list_root);
	while (1) {
	    if (list_entry == list_root) {
		/* reached end of list, funct not found */
		HALFAIL_RC(EINVAL, "thread '%s' doesn't use %s",
			   thread_name, funct_name);
	    }
	    hal_funct_entry_t *funct_entry = (hal_funct_entry_t *) list_entry;
	    if (SHMPTR(funct_entry->funct_ptr) == funct) {
		/* this funct entry points to our funct, unlink */
		dlist_remove_entry(list_entry);
		/* and delete it */
		free_funct_entry_struct(funct_entry);
		/* done */
		return 0;
	    }
	    /* try next one */
	    list_entry = dlist_next(list_entry);
	}
    }
}

static hal_funct_entry_t *alloc_funct_entry_struct(void)
{
    hal_list_t *freelist, *l;
    hal_funct_entry_t *p;

    /* check the free list */
    freelist = &(hal_data->funct_entry_free);
    l = dlist_next(freelist);
    if (l != freelist) {
	/* found a free structure, unlink from the free list */
	dlist_remove_entry(l);
	p = (hal_funct_entry_t *) l;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_rt(sizeof(hal_funct_entry_t));
	l = (hal_list_t *) p;
	dlist_init_entry(l);
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
    dlist_add_after((hal_list_t *) funct_entry, &(hal_data->funct_entry_free));
}


#ifdef RTAPI

static int thread_cb(hal_object_ptr o, foreach_args_t *args)
{
    hal_thread_t *thread = o.thread;
    hal_funct_t *funct = args->user_ptr1;

    /* start at root of funct_entry list */
    hal_list_t *list_root = &(thread->funct_list);
    hal_list_t *list_entry = dlist_next(list_root);

    /* run thru funct_entry list */
    while (list_entry != list_root) {
	/* point to funct entry */
	hal_funct_entry_t *funct_entry = (hal_funct_entry_t *) list_entry;
	/* test it */
	if (SHMPTR(funct_entry->funct_ptr) == funct) {
	    /* this funct entry points to our funct, unlink */
	    list_entry = dlist_remove_entry(list_entry);
	    /* and delete it */
	    free_funct_entry_struct(funct_entry);
	} else {
	    /* no match, try the next one */
	    list_entry = dlist_next(list_entry);
	}
    }
    return 0;
}

void free_funct_struct(hal_funct_t * funct)
{
    if (funct->users > 0) {
	/* We can't casually delete the function, there are thread(s) which
	   will call it.  So we must check all the threads and remove any
	   funct_entrys that call this function */

	foreach_args_t args =  {
	    .type = HAL_THREAD,
	    .user_ptr1 = funct,
	};
	halg_foreach(0, &args, thread_cb);
    }
    halg_free_object(false, (hal_object_ptr) funct);
}
#endif
