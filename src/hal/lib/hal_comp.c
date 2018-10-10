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
#if defined(BUILD_SYS_USER_DSO) || (defined(RTAPI) && !defined(BUILD_SYS_KBUILD))
#include <signal.h>
#ifndef abs
int abs(int x) { if(x < 0) return -x; else return x; }
#endif
#endif


hal_comp_t *halpr_alloc_comp_struct(void);

#ifdef RTAPI
static int init_hal_data(void);
static int create_instance(const hal_funct_args_t *fa);
static int delete_instance(const hal_funct_args_t *fa);
#endif

int hal_xinit(const int type,
	      const int userarg1,
	      const int userarg2,
	      const hal_constructor_t ctor,
	      const hal_destructor_t dtor,
	      const char *name) {
    hal_comp_t *c = halg_xinitf(1, type, userarg1, userarg2, ctor, dtor, "%s", name);
    return c == NULL ? _halerrno : hh_get_id(&c->hdr);
}

hal_comp_t *halg_xinitf(const int use_halmutex,
			const int type,
			const int userarg1,
			const int userarg2,
			const hal_constructor_t ctor,
			const hal_destructor_t dtor,
			const char *fmt, ...)
{
    va_list ap;
    PCHECK_NULL(fmt);
    va_start(ap, fmt);
    hal_comp_t *comp =  halg_xinitfv(use_halmutex,
				     type,
				     userarg1,
				     userarg2,
				     ctor,
				     dtor,
				     fmt, ap);
    va_end(ap);
    return comp;
}

hal_comp_t *halg_xinitfv(const int use_hal_mutex,
			 const int type,
			 const int userarg1,
			 const int userarg2,
			 const hal_constructor_t ctor,
			 const hal_destructor_t dtor,
			 const char *fmt,
			 va_list ap)
{
    PCHECK_NULL(fmt);
    PCHECK_STRLEN(fmt, HAL_MAX_NAME_LEN);
    char buf[HAL_MAX_NAME_LEN + 1];
    char *name = fmt_ap(buf, sizeof(buf), fmt, ap);
    PCHECK_NULL(name);

    rtapi_set_logtag("hal_lib");
    int comp_id, retval;

    // sanity: these must have been inited before by the
    // respective rtapi.so/.ko module
    PCHECK_NULL(rtapi_switch);

    if ((dtor != NULL) && (ctor == NULL)) {
	HALFAIL_NULL(EINVAL,"component '%s': NULL constructor doesnt make"
		     " sense with non-NULL destructor", name);
    }

    // RTAPI initialisation already done
    HALDBG("initializing component '%s' type=%d arg1=%d arg2=%d/0x%x",
	   name, type, userarg1, userarg2, userarg2);

    {
	// in this phase it is not guaranteed that the hal library is loaded,
	// and the hal shm segment is available; therefore, whatever the
	// argument says, we cannot lock the hal mutex since it resides
	// in the very shm segment the hal lib initialisation will attach.
	WITH_HAL_MUTEX_IF(use_hal_mutex && (hal_data != NULL));

	if ((lib_module_id < 0) && (type != TYPE_HALLIB)) {
	    // if hal_lib not inited yet, do so now - recurse
	    hal_comp_t *hallib;
#ifdef RTAPI
	    hallib = halg_xinitf(0, TYPE_HALLIB, 0, 0, NULL, NULL, "hal_lib");
#else
	    hallib = halg_xinitf(0, TYPE_HALLIB, 0, 0, NULL, NULL, "hal_lib%ld",
				(long) getpid());
#endif
	    if (hallib == NULL)
		return NULL;
	}

	// tag message origin field since ulapi autoload re-tagged them temporarily
	rtapi_set_logtag("hal_lib");

	/* copy name to local vars, truncating if needed */
	char rtapi_name[RTAPI_NAME_LEN + 1];

	rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_%s", name);

	/* do RTAPI init */
	comp_id = rtapi_init(rtapi_name);
	if (comp_id < 0) {
	    HALFAIL_NULL(comp_id, "rtapi init(%s) failed", rtapi_name);
	}

	// recursing? init HAL shm
	if ((lib_module_id < 0) && (type == TYPE_HALLIB)) {
	    // recursion case, we're initing hal_lib

	    // get HAL shared memory from RTAPI
	    int shm_id = rtapi_shmem_new(HAL_KEY,
					 comp_id,
					 global_data->hal_size);
	    if (shm_id < 0) {
		rtapi_exit(comp_id);
		HALFAIL_NULL(shm_id,
			     "hal_lib:%d failed to allocate HAL shm %x, rc=%d",
			     comp_id, HAL_KEY, shm_id);
	    }
	    // retrieve address of HAL shared memory segment
	    void *mem;
	    retval = rtapi_shmem_getptr(shm_id, &mem, 0);
	    if (retval < 0) {
		rtapi_exit(comp_id);
		HALFAIL_NULL(retval,
			     "hal_lib:%d failed to acquire HAL shm %x, id=%d rc=%d",
			     comp_id, HAL_KEY, shm_id, retval);
	    }
	    // set up internal pointers to shared mem and data structure
	    hal_shmem_base = (char *) mem;
	    hal_data = (hal_data_t *) mem;

#ifdef RTAPI
	    // only on RTAPI hal_lib initialization:
	    // initialize up the HAL shm segment
	    retval = init_hal_data();
	    if (retval) {
		rtapi_exit(lib_module_id);
		lib_module_id = -1;
		HALFAIL_NULL(retval,
			     "could not init HAL shared memory rc=%d", retval);
	    }
	    retval = hal_proc_init();
	    if (retval) {
		rtapi_exit(lib_module_id);
		lib_module_id = -1;
		HALFAIL_NULL(retval, "could not init /proc files");
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
	    HALFAIL_NULL(lib_module_id, "giving up");
	}
    }
    // from here on, the hal and global data segments are
    // guaranteed to be mapped, so it is safe to lock as
    // directed by use_hal_mutex
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_comp_t *comp;

	/* make sure name is unique in the system */
	if (halpr_find_comp_by_name(name) != 0) {
	    /* a component with this name already exists */
	    rtapi_exit(comp_id);
	    HALFAIL_NULL(EBUSY, "duplicate component name '%s'", name);
	}

	comp = halg_create_objectf(false, sizeof(hal_comp_t),
				   HAL_COMPONENT, 0, name);
	if (comp == NULL) {
	    rtapi_exit(comp_id);
	    return NULL;
	}

	// fixup comp_id which comes from
	// rtapi_init(), not rtapi_next_handle()
	hh_set_id(&comp->hdr, comp_id);

	/* initialize the comp structure */
	comp->userarg1 = userarg1;
	comp->userarg2 = userarg2;
	comp->type = type;  // subtype (RT, USER, REMOTE)
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

	// make it visible
	halg_add_object(false, (hal_object_ptr)comp);

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
	    if ((retval = halg_export_xfunctf(0,  &ni, "newinst")) < 0)
		return NULL;

	    hal_export_xfunct_args_t di = {
		.type = FS_USERLAND,
		.funct.u = delete_instance,
		.arg = NULL,
		.owner_id = lib_module_id
	    };
	    if ((retval = halg_export_xfunctf(0, &di, "delinst")) < 0)
		return NULL;
#endif
	    retval = halg_ready(0, lib_module_id);
	    if (retval)
		HALFAIL_NULL(retval,
			     "hal_ready(%d) failed rc=%d", lib_module_id, retval);

	    HALDBG("%s component '%s' id=%d initialized%s",
		   (ctor != NULL) ? "instantiable" : "singleton",
		   name, comp_id,
		   (dtor != NULL) ? ", has destructor" : "");

	}
	return comp;
    }
    return NULL;
}

int halg_exit(const int use_hal_mutex, int comp_id)
{
    int comptype;

    CHECK_HALDATA();

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_comp_t *comp = halpr_find_comp_by_id(comp_id);

	if (comp == NULL) {
	    HALFAIL_RC(EINVAL, "no such component with id %d", comp_id);
	}

	HALDBG("removing component %d '%s'", comp_id, ho_name(comp));

	// record type, since we're about to zap the comp in free_comp_struct()
	// which would be a dangling reference
	comptype = comp->type;

	// get rid of the component
	// this frees all dependent objects before releasing the comp
	// descriptor per se: functs, pins, params, instances, and in
	// turn, dependent objects of instances
	free_comp_struct(comp);

    } // scope exit - HAL mutex released

    // if unloading the hal_lib component, destroy HAL shm.
    // this must happen without the HAL mutex, because the
    // HAL mutex lives in the very shm segment which we're
    // about to release, which will it make impossible to
    // release the mutex (and leave the HAL shm locked
    // for other processes
    if (comptype == TYPE_HALLIB) {
	int retval;
	HALDBG("hal_errorcount()=%d", hal_errorcount(0));
	HALDBG("_halerrno=%d", _halerrno);
#ifdef RTAPI
	report_memory_usage();
	// just assure everything still intact
	HALDBG("hal_sweep: %d objects freed", hal_sweep());
#endif
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
    return 0;
}

int halg_ready(const int use_hal_mutex, int comp_id)
{
    WITH_HAL_MUTEX_IF(use_hal_mutex);

    hal_comp_t *comp = halpr_find_comp_by_id(comp_id);
    if (comp == NULL) {
	HALFAIL_RC(EINVAL, "component %d not found", comp_id);
    }

    if(comp->state > COMP_INITIALIZING) {
	HALFAIL_RC(EINVAL, "component '%s' id %d already ready (state %d)",
	       ho_name(comp), ho_id(comp), comp->state);
    }
    comp->state = (comp->type == TYPE_REMOTE ?  COMP_UNBOUND : COMP_READY);
    return 0;
}

const char *hal_comp_name(int comp_id)
{
    WITH_HAL_MUTEX();
    hal_comp_t *comp = halpr_find_comp_by_id(comp_id);
    return (comp == NULL) ? NULL : ho_name(comp);
}

// use only for owner_ids of pins, params or functs
// may return NULL if buggy using code
// This was a bottleneck with rcomps validating pins
// Move checks so that instances get checked first

hal_comp_t *halpr_find_owning_comp(const int owner_id)
{
hal_comp_t *comp;

    // more likely to be instcomp, so test that first
    hal_inst_t *inst = halpr_find_inst_by_id(owner_id);

    if (inst == NULL) { // is it a legacy comp?
        comp = halpr_find_comp_by_id(owner_id);
        if (comp != NULL)
            return comp;
        else
            return NULL;
        }
    // is it valid?
    HAL_ASSERT(ho_object_type(inst) == HAL_INST);

    // found the instance. Retrieve its owning comp:
    comp =  halpr_find_comp_by_id(ho_owner_id(inst));
    if (comp == NULL) {
        // really bad. an instance which has no owning comp?
        HALERR("BUG: instance %s/%d's comp_id %d refers to a non-existant comp",
           ho_name(inst), ho_id(inst), ho_owner_id(inst));
        }
    // is it valid?
    HAL_ASSERT(ho_object_type(comp) == HAL_COMPONENT);

    return comp;
}

int free_comp_struct(hal_comp_t * comp)
{

    // dont exit if the comp is still reference, eg a remote comp
    // served by haltalk:
    if (ho_referenced(comp)) {
	HALFAIL_RC(EBUSY, "not exiting comp %s - still referenced (refcnt=%d)",
	       ho_name(comp), ho_refcnt(comp));
    }

    /* can't delete the component until we delete its "stuff" */
    /* need to check for functs only if a realtime component */
#ifdef RTAPI
    // first unlink and destroy all functs, so an RT thread
    // cant trample on the comp while it's being destroyed

    foreach_args_t args =  {
	// search for functs owned by this comp
	.type = HAL_FUNCT,
	.owner_id  = ho_id(comp),
    };
    halg_foreach(0, &args, yield_free);

    // here, technically all the comp's functs are
    // delf'd and not visible anymore

    // now that the funct is gone,
    // exit all the comp's instances
    foreach_args_t iargs =  {
	// search for insts owned by this comp
	.type = HAL_INST,
	.owner_id  = ho_id(comp),
    };
    halg_foreach(0, &iargs, yield_free);

    // here all insts, their pins, params and functs are gone.

#endif /* RTAPI */

    // now work the legacy pins and params which are
    // directly owned by the comp.
    foreach_args_t pinargs =  {
	// wipe pins owned by this comp
	.type = HAL_PIN,
	.owner_id  = ho_id(comp),
    };
    halg_foreach(0, &pinargs, yield_free);

    foreach_args_t paramargs =  {
	// wipe params owned by this comp
	.type = HAL_PARAM,
	.owner_id  = ho_id(comp),
    };
    halg_foreach(0, &paramargs, yield_free);

    foreach_args_t plugargs =  {
	.type = HAL_PLUG,
	.owner_id  = ho_id(comp),
    };
    halg_foreach(0, &plugargs, yield_free);  // free plugs

    //  now we can delete the component itself.
    halg_free_object(false, (hal_object_ptr)comp);
    return 0;
}

#ifdef RTAPI

// instantiation handlers
static int create_instance(const hal_funct_args_t *fa)
{
    const int argc = fa_argc(fa);
    char * const *argv = fa_argv(fa);

#if 0
    HALDBG("'%s' called, arg=%p argc=%d",
	   fa_funct_name(fa), fa_arg(fa), argc);
    int i;
    for (i = 0; i < argc; i++)
	HALDBG("    argv[%d] = \"%s\"", i,argv[i]);
#endif

    if (argc < 2) {
	HALFAIL_RC(EINVAL, "need component name and instance name");
    }
    const char *cname = argv[0];
    const char *iname = argv[1];

    hal_comp_t *comp = halpr_find_comp_by_name(cname);
    if (!comp) {
	HALFAIL_RC(EINVAL,"no such component '%s'", cname);
    }
    if (!comp->ctor) {
	HALFAIL_RC(EINVAL,"component '%s' not instantiable", cname);
    }
    hal_inst_t *inst = halpr_find_inst_by_name(iname);
    if (inst) {
	HALFAIL_RC(EBUSY,"instance '%s' already exists", iname);
    }
    return comp->ctor(argc, argv);
}

static int delete_instance(const hal_funct_args_t *fa)
{
    const int argc = fa_argc(fa);
    char * const *argv = fa_argv(fa);

    HALDBG("'%s' called, arg=%p argc=%d",
	   fa_funct_name(fa), fa_arg(fa), argc);
    int i;
    for (i = 0; i < argc; i++)
	HALDBG("    argv[%d] = \"%s\"", i, argv[i]);
    if (argc < 1) {
	HALFAIL_RC(EINVAL,"no instance name given");
    }
    return halg_inst_delete(1, argv[0]);
}


/** init_hal_data() initializes the entire HAL data structure,
    by the RT hal_lib component
    must be called with hal mutex held
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

    // some heaps contain garbage, like xenomai
    memset(hal_data, 0, global_data->hal_size);

    /* set version code so nobody else init's the block */
    hal_data->version = HAL_VER;

    /* initialize everything */
    dlist_init_entry(&(hal_data->halobjects));
    dlist_init_entry(&(hal_data->funct_entry_free));
    dlist_init_entry(&(hal_data->threads));

    hal_data->base_period = 0;
    hal_data->exact_base_period = 0;

    hal_data->threads_running = 0;
    hal_data->default_ringsize = HAL_DEFAULT_RINGSIZE;

    hal_data->dead_beef = HAL_VALUE_POISON;
    hal_data->str_alloc = 0;
    hal_data->str_freed = 0;
    hal_data->rt_alignment_loss = 0;

    RTAPI_ZERO_BITMAP(&hal_data->rings, HAL_MAX_RINGS);
    RTAPI_BIT_SET(hal_data->rings,0);

    /* set up for shmalloc_xx() */
    hal_data->shmem_bot = SHMOFF(&hal_data->arena);
    hal_data->shmem_top = global_data->hal_size;
    hal_data->lock = HAL_LOCK_NONE;

    int i;
    for (i = 0; i < MAX_EPSILON; i++)
	hal_data->epsilon[i] = 0.0;
    hal_data->epsilon[0] = DEFAULT_EPSILON;

    // initialize the HAL heap
    rtapi_heap_init(&hal_data->heap, "hal heap");
    rtapi_heap_setflags(&hal_data->heap, global_data->hal_heap_flags);
    hal_heap_addmem((size_t) (global_data->hal_size / HAL_HEAP_INITIAL));

    return 0;
}
#endif


// part of shutdown by rtapi_app:
// send SIGTERM to all remaining usercomps
static int unload_usr_cb(hal_object_ptr o, foreach_args_t *args)
{
    hal_comp_t *comp = o.comp;
    if ((comp->type == TYPE_REMOTE)
	&& comp->pid == 0) {
	// found a disowned remote component
	halg_exit(0, ho_id(comp));
	return 0;
    }
    // an owned remote component, or a user component
    // owned by somebody other than us receives a signal
    if (((comp->type == TYPE_REMOTE) && (comp->pid != 0)) ||
	((comp->type == TYPE_USER))) {
	HALDBG("comp %s: sending SIGTERM to pid %d", ho_name(comp), comp->pid);

	// found a userspace or remote component
	// send SIGTERM to unload this component
	// this will also exit haltalk if unloadusr of a remote
	// comp which is being served by haltalk
	kill(abs(comp->pid), SIGTERM);
    }
    return 0;
}

int hal_exit_usercomps(char *name)
{
    foreach_args_t args =  {
	.type = HAL_COMPONENT,
	.name = name,
    };
    halg_foreach(1, &args, unload_usr_cb);
    return 0;
}
