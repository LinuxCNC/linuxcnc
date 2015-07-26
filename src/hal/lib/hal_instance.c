// HAL instance API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

// interal: alloc/free
static hal_inst_t *alloc_inst_struct(void);

int hal_inst_create(const char *name, const int comp_id, const int size,
		    void **inst_data)
{
    CHECK_HALDATA();
    CHECK_STR(name);

    {

	WITH_HAL_MUTEX();

	hal_inst_t *inst;
	hal_comp_t *comp;
	void *m = NULL;

	// comp must exist
	if ((comp = halpr_find_comp_by_id(comp_id)) == 0) {
	    HALERR("comp %d not found", comp_id);
	    return -ENOENT;
	}

	// inst may not exist
	if ((inst = halpr_find_inst_by_name(name)) != NULL) {
	    HALERR("instance '%s' already exists", name);
	    return -EEXIST;
	}

	if (size > 0) {
	    m = shmalloc_up(size);
	    if (m == NULL)
		NOMEM(" instance %s: cant allocate %d bytes", name, size);
	}
	memset(m, 0, size);

	// allocate instance descriptor
	if ((inst = alloc_inst_struct()) == NULL)
	    NOMEM("instance '%s'", name);

	inst->comp_id = comp->comp_id;
	inst->inst_id = rtapi_next_handle();
	inst->inst_data_ptr = SHMOFF(m);

	inst->inst_size = size;
	rtapi_snprintf(inst->name, sizeof(inst->name), "%s", name);


	HALDBG("%s: creating instance '%s' size %d at ^ %d/%p base=%p",
#ifdef RTAPI
	       "rtapi",
#else
	       "ulapi",
#endif
	       name, size, inst->inst_data_ptr, m,  hal_shmem_base);

	// if not NULL, pass pointer to blob
	if (inst_data)
	    *(inst_data) = m;

	// make it visible
	inst->next_ptr = hal_data->inst_list_ptr;
	hal_data->inst_list_ptr = SHMOFF(inst);

	return inst->inst_id;
  }
}

int hal_inst_delete(const char *name)
{
    CHECK_HALDATA();
    CHECK_STR(name);

    {
	WITH_HAL_MUTEX();

	hal_inst_t *inst;

	// inst must exist
	if ((inst = halpr_find_inst_by_name(name)) == NULL) {
	    HALERR("instance '%s' does not exist", name);
	    return -ENOENT;
	}
	// this does most of the heavy lifting
	free_inst_struct(inst);
    }
    return 0;
}

hal_inst_t *halpr_find_inst_by_name(const char *name)
{
    int next;
    hal_inst_t *inst;

    /* search inst list for 'name' */
    next = hal_data->inst_list_ptr;
    while (next != 0) {
	inst = SHMPTR(next);
	if (strcmp(inst->name, name) == 0) {
	    /* found a match */
	    return inst;
	}
	if (strcmp(inst->name, name) == 0) {
	    /* found a match */
	    return inst;
	}
	/* didn't find it yet, look at next one */
	next = inst->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

// lookup instance by instance ID
hal_inst_t *halpr_find_inst_by_id(const int id)
{
    int next;
    hal_inst_t *inst;

    next = hal_data->inst_list_ptr;
    while (next != 0) {
	inst = SHMPTR(next);
	if (inst->inst_id == id) {
	    return inst;
	}
	next = inst->next_ptr;
    }
    return 0;
}

/** The 'halpr_find_pin_by_instance_id()' function find pins owned by a specific
    instance.  If 'start' is NULL, they start at the beginning of the
    appropriate list, and return the first item owned by 'instance'.
    Otherwise they assume that 'start' is the value returned by a prior
    call, and return the next matching item.  If no match is found, they
    return NULL.
*/
hal_pin_t *halpr_find_pin_by_instance_id(const int inst_id,
					 const hal_pin_t *start)
{
    int next;
    hal_pin_t *pin;

    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of pin list */
	next = hal_data->pin_list_ptr;
    } else {
	/* no, start at next pin */
	next = start->next_ptr;
    }
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->owner_id == inst_id) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

/** The 'halpr_find_param_by_instance_id()' function find params owned by a specific
    instance.  If 'start' is NULL, they start at the beginning of the
    appropriate list, and return the first item owned by 'instance'.
    Otherwise they assume that 'start' is the value returned by a prior
    call, and return the next matching item.  If no match is found, they
    return NULL.
*/
hal_param_t *halpr_find_param_by_instance_id(const int inst_id,
					     const hal_param_t *start)
{
    int next;
    hal_param_t *param;

    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of param list */
	next = hal_data->param_list_ptr;
    } else {
	/* no, start at next param */
	next = start->next_ptr;
    }
    while (next != 0) {
	param = SHMPTR(next);
	if (param->owner_id == inst_id) {
	    /* found a match */
	    return param;
	}
	/* didn't find it yet, look at next one */
	next = param->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

/** The 'halpr_find_funct_by_instance_id()' function find functs owned by a specific
    instance id.  If 'start' is NULL, they start at the beginning of the
    appropriate list, and return the first item owned by 'instance'.
    Otherwise they assume that 'start' is the value returned by a prior
    call, and return the next matching item.  If no match is found, they
    return NULL.
*/
hal_funct_t *halpr_find_funct_by_instance_id(const int inst_id,
					     const hal_funct_t * start)
{
    int next;
    hal_funct_t *funct;

     /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of funct list */
	next = hal_data->funct_list_ptr;
    } else {
	/* no, start at next funct */
	next = start->next_ptr;
    }
    while (next != 0) {
	funct = SHMPTR(next);
	if (funct->owner_id == inst_id) {
	    /* found a match */
	    return funct;
	}
	/* didn't find it yet, look at next one */
	next = funct->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

// iterate over insts owned by a particular comp.
// if comp_id < 0, return ALL instances, regardless which comp owns them.
hal_inst_t *halpr_find_inst_by_owning_comp(const int comp_id, hal_inst_t *start)
{
    int next;
    hal_inst_t *inst;

    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of inst list */
	next = hal_data->inst_list_ptr;
    } else {
	/* no, start at next inst */
	next = start->next_ptr;
    }
    while (next != 0) {
	inst = SHMPTR(next);
	if (comp_id < 0) // all insts
	    return inst;
	if (inst->comp_id == comp_id) {
	    return inst;
	}
	/* didn't find it yet, look at next one */
	next = inst->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}


static hal_inst_t *alloc_inst_struct(void)
{
    hal_inst_t *hi;

    /* check the free list */
    if (hal_data->inst_free_ptr != 0) {
	/* found a free structure, point to it */
	hi = SHMPTR(hal_data->inst_free_ptr);
	/* unlink it from the free list */
	hal_data->inst_free_ptr = hi->next_ptr;
	hi->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	hi = shmalloc_dn(sizeof(hal_inst_t));
    }
    if (hi) {
	/* make sure it's empty */
	hi->next_ptr = 0;
	hi->comp_id = 0;
	hi->inst_id = 0;
	hi->inst_data_ptr = 0;
	hi->inst_size = 0;
	hi->name[0] = '\0';
    }
    return hi;
}

// almost a copy of free_comp_struct(), but based on instance
//
// unlinks and frees functs exported by this instance
// then calls any custom destructor
// then unlinks & deletes all pins owned by this instance
// then deletes params owned by this instance
void free_inst_struct(hal_inst_t * inst)
{
    int *prev, next;
#ifdef RTAPI
    hal_funct_t *funct;
#endif /* RTAPI */
    hal_pin_t *pin;
    hal_param_t *param;

    /* can't delete the instance until we delete its "stuff" */
    /* need to check for functs only if a realtime component */
#ifdef RTAPI
    /* search the function list for this instance's functs */
    prev = &(hal_data->funct_list_ptr);
    next = *prev;
    while (next != 0) {
	funct = SHMPTR(next);
	if (funct->owner_id == inst->inst_id) {
	    /* this function belongs to this instance, unlink from list */
	    *prev = funct->next_ptr;
	    /* and delete it */
	    free_funct_struct(funct);
	} else {
	    /* no match, try the next one */
	    prev = &(funct->next_ptr);
	}
	next = *prev;
    }

    // now that the funct is gone, call the dtor for this instance
    // get owning comp
    hal_comp_t *comp = halpr_find_owning_comp(inst->comp_id);
    if (comp->dtor) {
	//NB - pins, params etc still intact
	// this instance is owned by this comp, call destructor
	HALDBG("calling custom destructor(%s,%s)", comp->name, inst->name);

	// for the time being (until the halg_ API is fully merged), unlock HAL
	// while calling the dtor
	rtapi_mutex_give(&(hal_data->mutex));
	comp->dtor(inst->name, SHMPTR(inst->inst_data_ptr), inst->inst_size);
	rtapi_mutex_get(&(hal_data->mutex));
    }
#endif /* RTAPI */

    /* search the pin list for this instance's pins */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->owner_id == inst->inst_id) {
	    /* this pin belongs to our instance, unlink from list */
	    *prev = pin->next_ptr;
	    /* and delete it */
	    free_pin_struct(pin);
	} else {
	    /* no match, try the next one */
	    prev = &(pin->next_ptr);
	}
	next = *prev;
    }
    /* search the parameter list for this instance's parameters */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (next != 0) {
	param = SHMPTR(next);
	if (param->owner_id == inst->inst_id) {
	    /* this param belongs to our instance, unlink from list */
	    *prev = param->next_ptr;
	    /* and delete it */
	    free_param_struct(param);
	} else {
	    /* no match, try the next one */
	    prev = &(param->next_ptr);
	}
	next = *prev;
    }
    /* now we can delete the instance itself */

    // search the instance list and unlink instances owned by this comp
    hal_inst_t *ip;
    prev = &(hal_data->inst_list_ptr);
    next = *prev;
    while (next != 0) {
	ip = SHMPTR(next);
	if (ip == inst) {
	    // this instance is owned by this comp
	    *prev = ip->next_ptr;
	    // zap the instance structure
	    ip->comp_id = 0;
	    ip->inst_id = 0;
	    ip->inst_data_ptr = 0; // NB - loosing HAL memory here
	    ip->inst_size = 0;
	    ip->name[0] = '\0';
	    // add it to free list
	    ip->next_ptr = hal_data->inst_free_ptr;
	    hal_data->inst_free_ptr = SHMOFF(ip);
	} else {
	    prev = &(ip->next_ptr);
	}
	next = *prev;
    }
}
