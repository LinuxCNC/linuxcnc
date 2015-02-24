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


int hal_init_mode(const char *name, int type, int userarg1, int userarg2)
{
    int comp_id;
    char rtapi_name[RTAPI_NAME_LEN + 1];
    char hal_name[HAL_NAME_LEN + 1];

    // tag message origin field
    rtapi_set_logtag("hal_lib");

    if (name == 0) {
	hal_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: no component name\n");
	return -EINVAL;
    }
    if (strlen(name) > HAL_NAME_LEN) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: component name '%s' is too long\n", name);
	return -EINVAL;
    }
    // rtapi initialisation already done
    // since this happens through the constructor
    hal_print_msg(RTAPI_MSG_DBG,
		    "HAL: initializing component '%s' type=%d arg1=%d arg2=%d/0x%x\n",
		    name, type, userarg1, userarg2, userarg2);
    /* copy name to local vars, truncating if needed */
    rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_%s", name);
    rtapi_snprintf(hal_name, sizeof(hal_name), "%s", name);

    /* do RTAPI init */
    comp_id = rtapi_init(rtapi_name);
    if (comp_id < 0) {
	hal_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: rtapi init failed\n");
	return -EINVAL;
    }
    // tag message origin field since ulapi autoload re-tagged them
    rtapi_set_logtag("hal_lib");
#ifdef ULAPI
    hal_rtapi_attach();
#endif
    {
	hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before manipulating the shared data */
	rtapi_mutex_get(&(hal_data->mutex));
	/* make sure name is unique in the system */
	if (halpr_find_comp_by_name(hal_name) != 0) {
	    /* a component with this name already exists */
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: duplicate component name '%s'\n", hal_name);
	    rtapi_exit(comp_id);
	    return -EINVAL;
	}
	/* allocate a new component structure */
	comp = halpr_alloc_comp_struct();
	if (comp == 0) {
	    /* couldn't allocate structure */
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: insufficient memory for component '%s'\n", hal_name);
	    rtapi_exit(comp_id);
	    return -ENOMEM;
	}

	/* initialize the comp structure */
	comp->userarg1 = userarg1;
	comp->userarg2 = userarg2;
	comp->comp_id = comp_id;
	comp->type = type;
#ifdef RTAPI
	comp->pid = 0;   //FIXME revisit this
#else /* ULAPI */
	// a remote component starts out disowned
	comp->pid = comp->type == TYPE_REMOTE ? 0 : getpid(); //FIXME revisit this
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
    /* done */
    hal_print_msg(RTAPI_MSG_DBG,
		    "HAL: component '%s' initialized, ID = %02d\n", hal_name, comp_id);
    return comp_id;
}


int hal_exit(int comp_id)
{
    int *prev, next;
    char name[HAL_NAME_LEN + 1];

    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: exit called before init\n");
	return -EINVAL;
    }
    hal_print_msg(RTAPI_MSG_DBG, "HAL: removing component %02d\n", comp_id);

    {
	hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* grab mutex before manipulating list */
	rtapi_mutex_get(&(hal_data->mutex));
	/* search component list for 'comp_id' */
	prev = &(hal_data->comp_list_ptr);
	next = *prev;
	if (next == 0) {
	    /* list is empty - should never happen, but... */
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: component %d not found\n", comp_id);
	    return -EINVAL;
	}
	comp = SHMPTR(next);
	while (comp->comp_id != comp_id) {
	    /* not a match, try the next one */
	    prev = &(comp->next_ptr);
	    next = *prev;
	    if (next == 0) {
		/* reached end of list without finding component */
		hal_print_msg(RTAPI_MSG_ERR,
				"HAL: ERROR: component %d not found\n", comp_id);
		return -EINVAL;
	    }
	    comp = SHMPTR(next);
	}
	/* found our component, unlink it from the list */
	*prev = comp->next_ptr;
	/* save component name for later */
	rtapi_snprintf(name, sizeof(name), "%s", comp->name);
	/* get rid of the component */
	free_comp_struct(comp);
	/*! \todo Another #if 0 */
#if 0
	/*! \todo FIXME - this is the beginning of a two pronged approach to managing
	  shared memory.  Prong 1 - re-init the shared memory allocator whenever
	  it is known to be safe.  Prong 2 - make a better allocator that can
	  reclaim memory allocated by components when those components are
	  removed. To be finished later. */
	/* was that the last component? */
	if (hal_data->comp_list_ptr == 0) {
	    /* yes, are there any signals or threads defined? */
	    if ((hal_data->sig_list_ptr == 0) && (hal_data->thread_list_ptr == 0)) {
		/* no, invalidate "magic" number so shmem will be re-inited when
		   a new component is loaded */
		hal_data->magic = 0;
	    }
	}
#endif
	// scope exit - mutex released
    }
    // the RTAPI resources are now released
    // on hal_lib shared library unload
    rtapi_exit(comp_id);
    /* done */
    hal_print_msg(RTAPI_MSG_DBG,
	"HAL: component %02d removed, name = '%s'\n", comp_id, name);

    return 0;
}


#ifdef RTAPI
int hal_set_constructor(int comp_id, constructor make) {
    int next;
    hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

    rtapi_mutex_get(&(hal_data->mutex));

    /* search component list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }

    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	next = comp->next_ptr;
	if (next == 0) {
	    /* reached end of list without finding component */
	    hal_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: component %d not found\n", comp_id);
	    return -EINVAL;
	}
	comp = SHMPTR(next);
    }
    comp->make = make;
    return 0;
}
#endif

int hal_ready(int comp_id) {
    int next;
    hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

    rtapi_mutex_get(&(hal_data->mutex));

    /* search component list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }

    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	next = comp->next_ptr;
	if (next == 0) {
	    /* reached end of list without finding component */
	    hal_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: component %d not found\n", comp_id);
	    return -EINVAL;
	}
	comp = SHMPTR(next);
    }
    if(comp->state > COMP_INITIALIZING) {
        hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: Component '%s' already ready (%d)\n",
			comp->name, comp->state);
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
    /* search the function list for this component's functs */
    prev = &(hal_data->funct_list_ptr);
    next = *prev;
    while (next != 0) {
	funct = SHMPTR(next);
	if (SHMPTR(funct->owner_ptr) == comp) {
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
#endif /* RTAPI */
    /* search the pin list for this component's pins */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (next != 0) {
	pin = SHMPTR(next);
	if (SHMPTR(pin->owner_ptr) == comp) {
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
	if (SHMPTR(param->owner_ptr) == comp) {
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
    comp->comp_id = -1;
    comp->type = TYPE_INVALID;
    comp->state = COMP_INVALID;
    comp->last_bound = 0;
    comp->last_unbound = 0;
    comp->last_update = 0;
    comp->shmem_base = 0;
    comp->name[0] = '\0';
    /* add it to free list */
    comp->next_ptr = hal_data->comp_free_ptr;
    hal_data->comp_free_ptr = SHMOFF(comp);
}
