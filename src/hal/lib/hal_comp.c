// HAL components API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

#if defined(ULAPI)
#include <sys/types.h>		/* pid_t */
#include <unistd.h>		/* getpid() */
#endif

hal_comp_t *halpr_alloc_comp_struct(void);
static void free_comp_struct(hal_comp_t * comp);

#ifdef RTAPI
static int init_hal_data(void);
static int create_instance(const hal_funct_args_t *fa);
static int delete_instance(const hal_funct_args_t *fa);
#endif

int hal_xinitf(const int type,
	       const int userarg1,
	       const int userarg2,
	       const hal_constructor_t ctor,
	       const hal_destructor_t dtor,
	       const char *fmt, ...)
{
    va_list ap;
    char hal_name[HAL_NAME_LEN + 1];

    CHECK_NULL(fmt);
    va_start(ap, fmt);
    int sz = rtapi_vsnprintf(hal_name, sizeof(hal_name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        HALERR("invalid length %d for name starting with '%s'",
	       sz, hal_name);
        return -EINVAL;
    }
    return hal_xinit(type, userarg1, userarg2, ctor, dtor, hal_name);
}

int hal_xinit(const int type,
	      const int userarg1,
	      const int userarg2,
	      const hal_constructor_t ctor,
	      const hal_destructor_t dtor,
	      const char *name)
{
    int comp_id, retval;

    rtapi_set_logtag("hal_lib");
    CHECK_STRLEN(name, HAL_NAME_LEN);

    // sanity: these must have been inited before by the
    // respective rtapi.so/.ko module
    CHECK_NULL(rtapi_switch);

    if ((dtor != NULL) && (ctor == NULL)) {
	HALERR("component '%s': NULL constructor doesnt make"
	       " sense with non-NULL destructor", name);
	return -EINVAL;
    }

    // RTAPI initialisation already done
    HALDBG("initializing component '%s' type=%d arg1=%d arg2=%d/0x%x",
	   name, type, userarg1, userarg2, userarg2);

    if ((lib_module_id < 0) && (type != TYPE_HALLIB)) {
	// if hal_lib not inited yet, do so now - recurse
#ifdef RTAPI
	retval = hal_xinit(TYPE_HALLIB, 0, 0, NULL, NULL, "hal_lib");
#else
	retval = hal_xinitf(TYPE_HALLIB, 0, 0, NULL, NULL, "hal_lib%ld",
			    (long) getpid());
#endif
	if (retval < 0)
	    return retval;
    }

    // tag message origin field since ulapi autoload re-tagged them temporarily
    rtapi_set_logtag("hal_lib");

    /* copy name to local vars, truncating if needed */
    char rtapi_name[RTAPI_NAME_LEN + 1];
    char hal_name[HAL_NAME_LEN + 1];

    rtapi_snprintf(hal_name, sizeof(hal_name), "%s", name);
    rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_%s", hal_name);

    /* do RTAPI init */
    comp_id = rtapi_init(rtapi_name);
    if (comp_id < 0) {
	HALERR("rtapi init(%s) failed", rtapi_name);
	return -EINVAL;
    }

    // recursing? init HAL shm
    if ((lib_module_id < 0) && (type == TYPE_HALLIB)) {
	// recursion case, we're initing hal_lib

	// get HAL shared memory from RTAPI
	int shm_id = rtapi_shmem_new(HAL_KEY,
				     comp_id,
				     global_data->hal_size);
	if (shm_id < 0) {
	    HALERR("hal_lib:%d failed to allocate HAL shm %x, rc=%d",
		   comp_id, HAL_KEY, shm_id);
	    rtapi_exit(comp_id);
	    return -EINVAL;
	}
	// retrieve address of HAL shared memory segment
	void *mem;
	retval = rtapi_shmem_getptr(shm_id, &mem, 0);
	if (retval < 0) {
	    HALERR("hal_lib:%d failed to acquire HAL shm %x, id=%d rc=%d",
		   comp_id, HAL_KEY, shm_id, retval);
	    rtapi_exit(comp_id);
	    return -EINVAL;
	}
	// set up internal pointers to shared mem and data structure
	hal_shmem_base = (char *) mem;
	hal_data = (hal_data_t *) mem;

#ifdef RTAPI
	// only on RTAPI hal_lib initialization:
	// initialize up the HAL shm segment
	retval = init_hal_data();
	if (retval) {
	    HALERR("could not init HAL shared memory rc=%d", retval);
	    rtapi_exit(lib_module_id);
	    lib_module_id = -1;
	    return -EINVAL;
	}
	retval = hal_proc_init();
	if (retval) {
	    HALERR("could not init /proc files");
	    rtapi_exit(lib_module_id);
	    lib_module_id = -1;
	    return -EINVAL;
	}
#endif
	// record hal_lib comp_id
	lib_module_id = comp_id;
	// and the HAL shm segmed id
	lib_mem_id = shm_id;

    }
    // global_data MUST be at hand now:
    HAL_ASSERT(global_data != NULL);

    // paranoia
    HAL_ASSERT(hal_shmem_base != NULL);
    HAL_ASSERT(hal_data != NULL);
    HAL_ASSERT(lib_module_id > -1);
    HAL_ASSERT(lib_mem_id > -1);
    if (lib_module_id < 0) {
	HALERR("giving up");
	return -EINVAL;
    }

    {
	hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before manipulating the shared data */
	rtapi_mutex_get(&(hal_data->mutex));
	/* make sure name is unique in the system */
	if (halpr_find_comp_by_name(hal_name) != 0) {
	    /* a component with this name already exists */
	    HALERR("duplicate component name '%s'", hal_name);
	    rtapi_exit(comp_id);
	    return -EINVAL;
	}
	/* allocate a new component structure */
	comp = halpr_alloc_comp_struct();
	if (comp == 0) {
	    HALERR("insufficient memory for component '%s'", hal_name);
	    rtapi_exit(comp_id);
	    return -ENOMEM;
	}

	/* initialize the comp structure */
	comp->userarg1 = userarg1;
	comp->userarg2 = userarg2;
	comp->comp_id = comp_id;
	comp->type = type;
	comp->ctor = ctor;
	comp->dtor = dtor;
#ifdef RTAPI
	comp->pid = 0;
#else /* ULAPI */
	// a remote component starts out disowned
	comp->pid = comp->type == TYPE_REMOTE ? 0 : getpid();
#endif
	comp->state = COMP_INITIALIZING;
	comp->last_update = 0;
	comp->last_bound = 0;
	comp->last_unbound = 0;
	comp->shmem_base = hal_shmem_base;
	comp->insmod_args = 0;
	rtapi_snprintf(comp->name, sizeof(comp->name), "%s", hal_name);
	/* insert new structure at head of list */
	comp->next_ptr = hal_data->comp_list_ptr;
	hal_data->comp_list_ptr = SHMOFF(comp);

    }
    // scope exited - mutex released

    // finish hal_lib initialisation
    // in ULAPI this will happen after the recursion on hal_lib%d unwinds

    if (type == TYPE_HALLIB) {
#ifdef RTAPI
	// only on RTAPI hal_lib initialization:
	// export the instantiation support userfuncts
	hal_export_xfunct_args_t ni = {
	    .type = FS_USERLAND,
	    .funct.u = create_instance,
	    .arg = NULL,
	    .owner_id = lib_module_id
	};
	if ((retval = hal_export_xfunctf( &ni, "newinst")) < 0)
	    return retval;

	hal_export_xfunct_args_t di = {
	    .type = FS_USERLAND,
	    .funct.u = delete_instance,
	    .arg = NULL,
	    .owner_id = lib_module_id
	};
	if ((retval = hal_export_xfunctf( &di, "delinst")) < 0)
	    return retval;
#endif
	retval = hal_ready(lib_module_id);
	if (retval)
	    HALERR("hal_ready(%d) failed rc=%d", lib_module_id, retval);
	else
	    HALDBG("%s initialization complete", hal_name);
	return retval;
    }

    HALDBG("%s component '%s' id=%d initialized%s",
	   (ctor != NULL) ? "instantiable" : "legacy",
	   hal_name, comp_id,
	   (dtor != NULL) ? ", has destructor" : "");
    return comp_id;
}


int hal_exit(int comp_id)
{
    int *prev, next, comptype;
    char name[HAL_NAME_LEN + 1];

    CHECK_HALDATA();

    HALDBG("removing component %d", comp_id);

    {
	hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* grab mutex before manipulating list */
	rtapi_mutex_get(&(hal_data->mutex));
	/* search component list for 'comp_id' */
	prev = &(hal_data->comp_list_ptr);
	next = *prev;
	if (next == 0) {
	    /* list is empty - should never happen, but... */
	    HALERR("no components defined");
	    return -EINVAL;
	}
	comp = SHMPTR(next);
	while (comp->comp_id != comp_id) {
	    /* not a match, try the next one */
	    prev = &(comp->next_ptr);
	    next = *prev;
	    if (next == 0) {
		/* reached end of list without finding component */
		HALERR("no such component with id %d", comp_id);
		return -EINVAL;
	    }
	    comp = SHMPTR(next);
	}

	// record type, since we're about to zap the comp in free_comp_struct()
	comptype = comp->type;

	/* save component name for later */
	rtapi_snprintf(name, sizeof(name), "%s", comp->name);
	/* get rid of the component */
	free_comp_struct(comp);

	// unlink the comp only now as free_comp_struct() must
	// determine ownership of pins/params/functs and this
	// requires access to the current comp, too
	// since this is all under lock it should not matter
	*prev = comp->next_ptr;

	// add it to free list
	comp->next_ptr = hal_data->comp_free_ptr;
	hal_data->comp_free_ptr = SHMOFF(comp);

	// scope exit - mutex released
    }

    // if unloading the hal_lib component, destroy HAL shm
    if (comptype == TYPE_HALLIB) {
	int retval;

	/* release RTAPI resources */
	retval = rtapi_shmem_delete(lib_mem_id, comp_id);
	if (retval) {
	    HALERR("rtapi_shmem_delete(%d,%d) failed: %d",
		   lib_mem_id, comp_id, retval);
	}
	// HAL shm is history, take note ASAP
	lib_mem_id = -1;
	hal_shmem_base = NULL;
	hal_data = NULL;;

	retval = rtapi_exit(comp_id);
	if (retval) {
	    HALERR("rtapi_exit(%d) failed: %d",
		   lib_module_id, retval);
	}
	// the hal_lib RTAPI module is history, too
	// in theory we'd be back to square 1
	lib_module_id = -1;

    } else {
	// the standard case
	rtapi_exit(comp_id);
    }

    //HALDBG("component '%s' id=%d removed", name, comp_id);
    return 0;
}


int hal_ready(int comp_id) {
    int next;
    hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

    rtapi_mutex_get(&(hal_data->mutex));

    /* search component list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	HALERR("BUG: no components defined - %d", comp_id);
	return -EINVAL;
    }

    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	next = comp->next_ptr;
	if (next == 0) {
	    /* reached end of list without finding component */
	    HALERR("component %d not found", comp_id);
	    return -EINVAL;
	}
	comp = SHMPTR(next);
    }
    if(comp->state > COMP_INITIALIZING) {
	HALERR("component '%s' id %d already ready (state %d)",
	       comp->name, comp->comp_id, comp->state);
        return -EINVAL;
    }
    comp->state = (comp->type == TYPE_REMOTE ?  COMP_UNBOUND : COMP_READY);
    return 0;
}

char *hal_comp_name(int comp_id)
{
    hal_comp_t *comp;
    char *result = NULL;
    rtapi_mutex_get(&(hal_data->mutex));
    comp = halpr_find_comp_by_id(comp_id);
    if(comp) result = comp->name;
    rtapi_mutex_give(&(hal_data->mutex));
    return result;
}

hal_comp_t *halpr_find_comp_by_name(const char *name)
{
    int next;
    hal_comp_t *comp;

    /* search component list for 'name' */
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if (strcmp(comp->name, name) == 0) {
	    /* found a match */
	    return comp;
	}
	/* didn't find it yet, look at next one */
	next = comp->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_comp_t *halpr_find_comp_by_id(int id)
{
    int next;
    hal_comp_t *comp;

    /* search list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if (comp->comp_id == id) {
	    /* found a match */
	    return comp;
	}
	/* didn't find it yet, look at next one */
	next = comp->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

// use only for owner_ids of pins, params or functs
// may return NULL if buggy using code
hal_comp_t *halpr_find_owning_comp(const int owner_id)
{
    hal_comp_t *comp = halpr_find_comp_by_id(owner_id);
    if (comp != NULL)
	return comp;  // legacy case: the owner_id refers to a comp

    // nope, so it better be an instance
    hal_inst_t *inst = halpr_find_inst_by_id(owner_id);
    if (inst == NULL) {
	HALERR("BUG: owner_id %d refers neither to a hal_comp_t nor an hal_inst_t",
	       owner_id);
	return NULL;
    }

    // found the instance. Retrieve its owning comp:
    comp =  halpr_find_comp_by_id(inst->comp_id);
    if (comp == NULL) {
	// really bad. an instance which has no owning comp?
	HALERR("BUG: instance %s/%d's comp_id %d refers to a non-existant comp",
	       inst->name, inst->inst_id, inst->comp_id);
    }
    return comp;
}


hal_comp_t *halpr_alloc_comp_struct(void)
{
    hal_comp_t *p;

    /* check the free list */
    if (hal_data->comp_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->comp_free_ptr);
	/* unlink it from the free list */
	hal_data->comp_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_comp_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->comp_id = 0;
	p->type = TYPE_INVALID;
	p->state = COMP_INVALID;
	p->shmem_base = 0;
	p->name[0] = '\0';
    }
    return p;
}

static void free_comp_struct(hal_comp_t * comp)
{
    int *prev, next;
#ifdef RTAPI
    hal_funct_t *funct;
#endif /* RTAPI */
    hal_pin_t *pin;
    hal_param_t *param;

    /* can't delete the component until we delete its "stuff" */
    /* need to check for functs only if a realtime component */
#ifdef RTAPI
    // first unlink and destroy all functs, so an RT thread
    // cant trample on the comp while it's being destroyed

    /* search the function list for this component's functs */
    prev = &(hal_data->funct_list_ptr);
    next = *prev;
    while (next != 0) {
	funct = SHMPTR(next);
	hal_comp_t *owner = halpr_find_owning_comp(funct->owner_id);
	if (owner == comp) {
	    /* this function belongs to our component, unlink from list */
	    *prev = funct->next_ptr;
	    /* and delete it */
	    free_funct_struct(funct);
	} else {
	    /* no match, try the next one */
	    prev = &(funct->next_ptr);
	}
	next = *prev;
    }
    // here, technically all the comp's functs are
    // delf'd and not visible anymore

    // now that the funct is gone,
    // exit all the comp's instances
    next = hal_data->inst_list_ptr;
    while (next != 0) {
	hal_inst_t *inst = SHMPTR(next);
	next = inst->next_ptr;
	if (inst->comp_id == comp->comp_id) {
	    // this instance is owned by this comp
	    free_inst_struct(inst);
	}
    }
    // here all insts, their pins, params and functs are gone.

#endif /* RTAPI */

    // now work the legacy pins and params which are
    // directly owned by the comp.

    /* search the pin list for this component's pins */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->owner_id == comp->comp_id) {
	    /* this pin belongs to our component, unlink from list */
	    *prev = pin->next_ptr;
	    /* and delete it */
	    free_pin_struct(pin);
	} else {
	    /* no match, try the next one */
	    prev = &(pin->next_ptr);
	}
	next = *prev;
    }
    /* search the parameter list for this component's parameters */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (next != 0) {
	param = SHMPTR(next);
	if (param->owner_id == comp->comp_id) {
	    /* this param belongs to our component, unlink from list */
	    *prev = param->next_ptr;
	    /* and delete it */
	    free_param_struct(param);
	} else {
	    /* no match, try the next one */
	    prev = &(param->next_ptr);
	}
	next = *prev;
    }

    /* now we can delete the component itself */
    /* clear contents of struct */
    comp->comp_id = 0;
    comp->type = TYPE_INVALID;
    comp->state = COMP_INVALID;
    comp->last_bound = 0;
    comp->last_unbound = 0;
    comp->last_update = 0;
    comp->shmem_base = 0;
    comp->name[0] = '\0';

}

#ifdef RTAPI

// instantiation handlers
static int create_instance(const hal_funct_args_t *fa)
{
    const int argc = fa_argc(fa);
    const char **argv = fa_argv(fa);

#if 0
    HALDBG("'%s' called, arg=%p argc=%d",
	   fa_funct_name(fa), fa_arg(fa), argc);
    int i;
    for (i = 0; i < argc; i++)
	HALDBG("    argv[%d] = \"%s\"", i,argv[i]);
#endif

    if (argc < 2) {
	HALERR("need component name and instance name");
	return -EINVAL;
    }
    const char *cname = argv[0];
    const char *iname = argv[1];

    hal_comp_t *comp = halpr_find_comp_by_name(cname);
    if (!comp) {
	HALERR("no such component '%s'", cname);
	return -EINVAL;
    }
    if (!comp->ctor) {
	HALERR("component '%s' not instantiable", cname);
	return -EINVAL;
    }
    hal_inst_t *inst = halpr_find_inst_by_name(iname);
    if (inst) {
	HALERR("instance '%s' already exists", iname);
	return -EBUSY;
    }
    return comp->ctor(iname, argc - 2, &argv[2]);
}

static int delete_instance(const hal_funct_args_t *fa)
{
    const int argc = fa_argc(fa);
    const char **argv = fa_argv(fa);

    HALDBG("'%s' called, arg=%p argc=%d",
	   fa_funct_name(fa), fa_arg(fa), argc);
    int i;
    for (i = 0; i < argc; i++)
	HALDBG("    argv[%d] = \"%s\"", i, argv[i]);
    if (argc < 1) {
	HALERR("no instance name given");
	return -EINVAL;
    }
    return hal_inst_delete(argv[0]);
}


/** init_hal_data() initializes the entire HAL data structure,
    by the RT hal_lib component
*/
int init_hal_data(void)
{

    /* has the block already been initialized? */
    if (hal_data->version != 0) {
	/* yes, verify version code */
	if (hal_data->version == HAL_VER) {
	    return 0;
	} else {
	    HALERR("version code mismatch");
	    return -1;
	}
    }
    /* no, we need to init it, grab the mutex unconditionally */
    rtapi_mutex_try(&(hal_data->mutex));

    // some heaps contain garbage, like xenomai
    memset(hal_data, 0, global_data->hal_size);

    /* set version code so nobody else init's the block */
    hal_data->version = HAL_VER;

    /* initialize everything */
    hal_data->comp_list_ptr = 0;
    hal_data->pin_list_ptr = 0;
    hal_data->sig_list_ptr = 0;
    hal_data->param_list_ptr = 0;
    hal_data->funct_list_ptr = 0;
    hal_data->thread_list_ptr = 0;
    hal_data->vtable_list_ptr = 0;
    hal_data->base_period = 0;
    hal_data->threads_running = 0;
    hal_data->oldname_free_ptr = 0;
    hal_data->comp_free_ptr = 0;
    hal_data->pin_free_ptr = 0;
    hal_data->sig_free_ptr = 0;
    hal_data->param_free_ptr = 0;
    hal_data->funct_free_ptr = 0;
    hal_data->vtable_free_ptr = 0;

    list_init_entry(&(hal_data->funct_entry_free));
    hal_data->thread_free_ptr = 0;
    hal_data->exact_base_period = 0;

    hal_data->group_list_ptr = 0;
    hal_data->member_list_ptr = 0;
    hal_data->ring_list_ptr = 0;
    hal_data->inst_list_ptr = 0;

    hal_data->group_free_ptr = 0;
    hal_data->member_free_ptr = 0;
    hal_data->ring_free_ptr = 0;
    hal_data->inst_free_ptr = 0;

    RTAPI_ZERO_BITMAP(&hal_data->rings, HAL_MAX_RINGS);
    // silly 1-based shm segment id allocation FIXED
    // yeah, 'user friendly', how could one possibly think zero might be a valid id
    RTAPI_BIT_SET(hal_data->rings,0);

    /* set up for shmalloc_xx() */
    hal_data->shmem_bot = sizeof(hal_data_t);
    hal_data->shmem_top = global_data->hal_size;
    hal_data->lock = HAL_LOCK_NONE;

    int i;
    for (i = 0; i < MAX_EPSILON; i++)
	hal_data->epsilon[i] = 0.0;
    hal_data->epsilon[0] = DEFAULT_EPSILON;

    /* done, release mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}
#endif
