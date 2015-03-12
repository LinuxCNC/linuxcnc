// HAL pin API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_group.h"
#include "hal_internal.h"

static hal_pin_t *alloc_pin_struct(void);

/***********************************************************************
*                        "PIN" FUNCTIONS                               *
************************************************************************/

int hal_pin_newfv(hal_type_t type,
		  hal_pin_dir_t dir,
		  void ** data_ptr_addr,
		  int owner_id,
		  const char *fmt,
		  va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        HALERR("length %d invalid for name starting '%s'",
	       sz, name);
        return -ENOMEM;
    }
    return hal_pin_new(name, type, dir, data_ptr_addr, owner_id);
}

int hal_pin_bit_newf(hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_BIT, dir, (void**)data_ptr_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_float_newf(hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_FLOAT, dir, (void**)data_ptr_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_u32_newf(hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_U32, dir, (void**)data_ptr_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_s32_newf(hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_S32, dir, (void**)data_ptr_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

// printf-style version of hal_pin_new()
int hal_pin_newf(hal_type_t type,
		 hal_pin_dir_t dir,
		 void ** data_ptr_addr,
		 int owner_id,
		 const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(type, dir, data_ptr_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

/* this is a generic function that does the majority of the work. */

int hal_pin_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
		    void **data_ptr_addr, int owner_id)
{
    int *prev, next, cmp;
    hal_pin_t *new, *ptr;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);
    CHECK_STRLEN(name, HAL_NAME_LEN);

    if(*data_ptr_addr)
    {
	HALERR("pin '%s': called with already-initialized memory", name);
    }
    if (type != HAL_BIT && type != HAL_FLOAT && type != HAL_S32 && type != HAL_U32) {
	HALERR("pin '%s': pin type not one of HAL_BIT, HAL_FLOAT, HAL_S32 or HAL_U32 (%d)",
	       name, type);
	return -EINVAL;
    }

    if(dir != HAL_IN && dir != HAL_OUT && dir != HAL_IO) {
	HALERR("pin '%s': pin direction not one of HAL_IN, HAL_OUT, or HAL_IO (%d)",
	       name, dir);
	return -EINVAL;
    }

    HALDBG("creating pin '%s'", name);

    {
	hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* validate comp_id */
	comp = halpr_find_owning_comp(owner_id);
	if (comp == 0) {
	    /* bad comp_id */
	    HALERR("pin '%s': owning component %d not found",
		   name, owner_id);
	    return -EINVAL;
	}

	/* validate passed in pointer - must point to HAL shmem */
	if (! SHMCHK(data_ptr_addr)) {
	    /* bad pointer */
	    HALERR("pin '%s': data_ptr_addr not in shared memory", name);
	    return -EINVAL;
	}

	// this will be 0 for legacy comps which use comp_id
	hal_inst_t *inst = halpr_find_inst_by_id(owner_id);
	int inst_id = (inst ? inst->inst_id : 0);

	// instances may create pins post hal_ready
	if ((inst_id == 0) && (comp->state > COMP_INITIALIZING)) {
	    // legacy error message. Never made sense.. why?
	    HALERR("pin '%s': hal_pin_new called after hal_ready (%d)",
		   name, comp->state);
	    return -EINVAL;
	}

	/* allocate a new variable structure */
	new = alloc_pin_struct();
	if (new == 0) {
	    /* alloc failed */
	    HALERR("insufficient memory for pin '%s'", name);
	    return -ENOMEM;
	}
	/* initialize the structure */
	new->data_ptr_addr = SHMOFF(data_ptr_addr);
	new->owner_id = owner_id;
	new->type = type;
	new->dir = dir;
	new->signal = 0;
	new->handle = rtapi_next_handle();
	memset(&new->dummysig, 0, sizeof(hal_data_u));
	rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
	/* make 'data_ptr' point to dummy signal */
	*data_ptr_addr = comp->shmem_base + SHMOFF(&(new->dummysig));
	/* search list for 'name' and insert new structure */
	prev = &(hal_data->pin_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		/* reached end of list, insert here */
		new->next_ptr = next;
		*prev = SHMOFF(new);
		return 0;
	    }
	    ptr = SHMPTR(next);
	    cmp = strcmp(ptr->name, new->name);
	    if (cmp > 0) {
		/* found the right place for it, insert here */
		new->next_ptr = next;
		*prev = SHMOFF(new);
		return 0;
	    }
	    if (cmp == 0) {
		/* name already in list, can't insert */
		free_pin_struct(new);
		HALERR("duplicate pin '%s'", name);
		return -EINVAL;
	    }
	    /* didn't find it yet, look at next one */
	    prev = &(ptr->next_ptr);
	    next = *prev;
	}
    }
}

void unlink_pin(hal_pin_t * pin)
{
    hal_sig_t *sig;
    hal_comp_t *comp;
    void **data_ptr_addr;
    hal_data_u *dummy_addr, *sig_data_addr;

    /* is this pin linked to a signal? */
    if (pin->signal != 0) {
	/* yes, need to unlink it */
	sig = SHMPTR(pin->signal);
	/* make pin's 'data_ptr' point to its dummy signal */
	data_ptr_addr = SHMPTR(pin->data_ptr_addr);
	comp = halpr_find_owning_comp(pin->owner_id);
	dummy_addr = comp->shmem_base + SHMOFF(&(pin->dummysig));
	*data_ptr_addr = dummy_addr;

	/* copy current signal value to dummy */
	sig_data_addr = (hal_data_u *)(hal_shmem_base + sig->data_ptr);
	dummy_addr = (hal_data_u *)(hal_shmem_base + SHMOFF(&(pin->dummysig)));

	switch (pin->type) {
	case HAL_BIT:
	    dummy_addr->b = sig_data_addr->b;
	    break;
	case HAL_S32:
	    dummy_addr->s = sig_data_addr->s;
	    break;
	case HAL_U32:
	    dummy_addr->u = sig_data_addr->u;
	    break;
	case HAL_FLOAT:
	    dummy_addr->f = sig_data_addr->f;
	    break;
	default:
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: BUG: pin '%s' has invalid type %d !!\n",
			  pin->name, pin->type);
	}

	/* update the signal's reader/writer counts */
	if ((pin->dir & HAL_IN) != 0) {
	    sig->readers--;
	}
	if (pin->dir == HAL_OUT) {
	    sig->writers--;
	}
	if (pin->dir == HAL_IO) {
	    sig->bidirs--;
	}
	/* mark pin as unlinked */
	pin->signal = 0;
    }
}

int hal_pin_alias(const char *pin_name, const char *alias)
{
    int *prev, next, cmp;
    hal_pin_t *pin, *ptr;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STRLEN(pin_name, HAL_NAME_LEN);

    if (alias != NULL ) {
	if (strlen(alias) > HAL_NAME_LEN) {
	    HALERR("alias name '%s' is too long", alias);
	    return -EINVAL;
	}
    }
    {
	hal_oldname_t *oldname  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));
	if (alias != NULL ) {
	    pin = halpr_find_pin_by_name(alias);
	    if ( pin != NULL ) {
		HALERR("duplicate pin/alias name '%s'", alias);
		return -EINVAL;
	    }
	}
	/* once we unlink the pin from the list, we don't want to have to
	   abort the change and repair things.  So we allocate an oldname
	   struct here, then free it (which puts it on the free list).  This
	   allocation might fail, in which case we abort the command.  But
	   if we actually need the struct later, the next alloc is guaranteed
	   to succeed since at least one struct is on the free list. */
	oldname = halpr_alloc_oldname_struct();
	if ( oldname == NULL ) {
	    HALERR("alias '%s': insufficient memory for pin_alias", pin_name);
	    return -EINVAL;
	}
	free_oldname_struct(oldname);

	/* find the pin and unlink it from pin list */
	prev = &(hal_data->pin_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		/* reached end of list, not found */
		HALERR("pin '%s' not found", pin_name);
		return -EINVAL;
	    }
	    pin = SHMPTR(next);
	    if ( strcmp(pin->name, pin_name) == 0 ) {
		/* found it, unlink from list */
		*prev = pin->next_ptr;
		break;
	    }
	    if (pin->oldname != 0 ) {
		oldname = SHMPTR(pin->oldname);
		if (strcmp(oldname->name, pin_name) == 0) {
		    /* found it, unlink from list */
		    *prev = pin->next_ptr;
		    break;
		}
	    }
	    /* didn't find it yet, look at next one */
	    prev = &(pin->next_ptr);
	    next = *prev;
	}
	if ( alias != NULL ) {
	/* adding a new alias */
	    if ( pin->oldname == 0 ) {
		/* save old name (only if not already saved) */
		oldname = halpr_alloc_oldname_struct();
		pin->oldname = SHMOFF(oldname);
		rtapi_snprintf(oldname->name, sizeof(oldname->name),
			       "%s", pin->name);
	    }
	    /* change pin's name to 'alias' */
	    rtapi_snprintf(pin->name, sizeof(pin->name), "%s", alias);
	} else {
	    /* removing an alias */
	    if ( pin->oldname != 0 ) {
		/* restore old name (only if pin is aliased) */
	    oldname = SHMPTR(pin->oldname);
	    rtapi_snprintf(pin->name, sizeof(pin->name), "%s", oldname->name);
	    pin->oldname = 0;
	    free_oldname_struct(oldname);
	    }
	}
	/* insert pin back into list in proper place */
	prev = &(hal_data->pin_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		/* reached end of list, insert here */
		pin->next_ptr = next;
		*prev = SHMOFF(pin);
		return 0;
	    }
	    ptr = SHMPTR(next);
	    cmp = strcmp(ptr->name, pin->name);
	    if (cmp > 0) {
		/* found the right place for it, insert here */
		pin->next_ptr = next;
		*prev = SHMOFF(pin);
		return 0;
	    }
	    /* didn't find it yet, look at next one */
	    prev = &(ptr->next_ptr);
	    next = *prev;
	}
    }
}

hal_pin_t *halpr_find_pin_by_name(const char *name)
{
    int next;
    hal_pin_t *pin;
    hal_oldname_t *oldname;

    /* search pin list for 'name' */
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if (strcmp(pin->name, name) == 0) {
	    /* found a match */
	    return pin;
	}
	if (pin->oldname != 0 ) {
	    oldname = SHMPTR(pin->oldname);
	    if (strcmp(oldname->name, name) == 0) {
		/* found a match */
		return pin;
	    }
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

// find a pin by owner id, which may refer to a instance or a comp
hal_pin_t *halpr_find_pin_by_owner_id(const int owner_id, hal_pin_t * start)
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
	if (pin->owner_id == owner_id) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

hal_pin_t *halpr_find_pin_by_sig(hal_sig_t * sig, hal_pin_t * start)
{
    int sig_ptr, next;
    hal_pin_t *pin;

    /* get offset of 'sig' component */
    sig_ptr = SHMOFF(sig);
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
	if (pin->signal == sig_ptr) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

static hal_pin_t *alloc_pin_struct(void)
{
    hal_pin_t *p;

    /* check the free list */
    if (hal_data->pin_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->pin_free_ptr);
	/* unlink it from the free list */
	hal_data->pin_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_pin_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr_addr = 0;
	p->owner_id = 0;
	p->type = 0;
	p->dir = 0;
	p->signal = 0;
	memset(&p->dummysig, 0, sizeof(hal_data_u));
	p->name[0] = '\0';
	p->eps_index = 0;
	p->flags = 0;
    }
    return p;
}

void free_pin_struct(hal_pin_t * pin)
{

    unlink_pin(pin);
    /* clear contents of struct */
    if ( pin->oldname != 0 ) free_oldname_struct(SHMPTR(pin->oldname));
    pin->data_ptr_addr = 0;
    pin->owner_id = 0;
    //    pin->instance_ptr = 0;
    pin->type = 0;
    pin->dir = 0;
    pin->signal = 0;
    pin->handle = -1;
    memset(&pin->dummysig, 0, sizeof(hal_data_u));
    pin->name[0] = '\0';
    pin->eps_index = 0;
    pin->flags = 0;
    /* add it to free list */
    pin->next_ptr = hal_data->pin_free_ptr;
    hal_data->pin_free_ptr = SHMOFF(pin);
}
