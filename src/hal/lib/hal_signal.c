// HAL signal API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_group.h"
#include "hal_internal.h"

static hal_sig_t *alloc_sig_struct(void);
static void free_sig_struct(hal_sig_t * sig);

/***********************************************************************
*                      "SIGNAL" FUNCTIONS                              *
************************************************************************/

int hal_signal_new(const char *name, hal_type_t type)
{

    int *prev, next, cmp;
    hal_sig_t *new, *ptr;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STRLEN(name, HAL_NAME_LEN);

    {
	void *data_addr  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	HALDBG("creating signal '%s'", name);

	/* check for an existing signal with the same name */
	if (halpr_find_sig_by_name(name) != 0) {
	    HALERR("duplicate signal '%s'", name);
	    return -EINVAL;
	}
	/* allocate memory for the signal value */
	switch (type) {
	case HAL_BIT:
	    data_addr = shmalloc_up(sizeof(hal_bit_t));
	    break;
	case HAL_S32:
	    data_addr = shmalloc_up(sizeof(hal_u32_t));
	    break;
	case HAL_U32:
	    data_addr = shmalloc_up(sizeof(hal_s32_t));
	    break;
	case HAL_FLOAT:
	    data_addr = shmalloc_up(sizeof(hal_float_t));
	    break;
	default:
	    HALERR("signal '%s': illegal signal type %d'", name, type);
	    return -EINVAL;
	    break;
	}
	/* allocate a new signal structure */
	new = alloc_sig_struct();
	if ((new == 0) || (data_addr == 0)) {
	    /* alloc failed */
	    HALERR("insufficient memory for signal '%s'", name);
	    return -ENOMEM;
	}
	/* initialize the signal value */
	switch (type) {
	case HAL_BIT:
	    *((hal_bit_t *) data_addr) = 0;
	    break;
	case HAL_S32:
	    *((hal_s32_t *) data_addr) = 0;
	    break;
	case HAL_U32:
	    *((hal_u32_t *) data_addr) = 0;
	    break;
	case HAL_FLOAT:
	    *((hal_float_t *) data_addr) = 0.0;
	    break;
	default:
	    break;
	}
	/* initialize the structure */
	new->data_ptr = SHMOFF(data_addr);
	new->type = type;
	new->readers = 0;
	new->writers = 0;
	new->bidirs = 0;
	new->handle = rtapi_next_handle();
	rtapi_snprintf(new->name, sizeof(new->name), "%s", name);

	/* search list for 'name' and insert new structure */
	prev = &(hal_data->sig_list_ptr);
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
	    /* didn't find it yet, look at next one */
	    prev = &(ptr->next_ptr);
	    next = *prev;
	}
    }
}

int hal_signal_delete(const char *name)
{

    int *prev, next;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STRLEN(name, HAL_NAME_LEN);

    HALDBG("deleting signal '%s'", name);

    {
	hal_sig_t *sig  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));
	/* search for the signal */
	prev = &(hal_data->sig_list_ptr);
	next = *prev;
	while (next != 0) {
	    sig = SHMPTR(next);
	    if (strcmp(sig->name, name) == 0) {
		hal_group_t *grp = halpr_find_group_of_member(name);
		if (grp) {
		    HALERR("cannot delete signal '%s'"
			   " since it is member of group '%s'",
			   name, grp->name);
		    return -EINVAL;
		}
		/* this is the right signal, unlink from list */
		*prev = sig->next_ptr;
		/* and delete it */
		free_sig_struct(sig);
		/* done */
		return 0;
	    }
	    /* no match, try the next one */
	    prev = &(sig->next_ptr);
	    next = *prev;
	}
    }
    /* if we get here, we didn't find a match */
    HALERR("signal '%s' not found",  name);
    return -EINVAL;
}


int hal_link(const char *pin_name, const char *sig_name)
{
    hal_sig_t *sig;
    hal_comp_t *comp;
    void **data_ptr_addr, *data_addr;


    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STRLEN(pin_name, HAL_NAME_LEN);
    CHECK_STRLEN(sig_name, HAL_NAME_LEN);

    HALDBG("linking pin '%s' to '%s'", pin_name, sig_name);

    {
	hal_pin_t *pin  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing data structures */
	rtapi_mutex_get(&(hal_data->mutex));

	/* locate the pin */
	pin = halpr_find_pin_by_name(pin_name);
	if (pin == 0) {
	    /* not found */
	    HALERR("pin '%s' not found", pin_name);
	    return -EINVAL;
	}
	/* locate the signal */
	sig = halpr_find_sig_by_name(sig_name);
	if (sig == 0) {
	    /* not found */
	    HALERR("signal '%s' not found", sig_name);
	    return -EINVAL;
	}
	/* found both pin and signal, are they already connected? */
	if (SHMPTR(pin->signal) == sig) {
	    HALWARN("pin '%s' already linked to '%s'", pin_name, sig_name);
	    return 0;
	}
	/* is the pin connected to something else? */
	if(pin->signal) {
	    sig = SHMPTR(pin->signal);
	    HALERR("pin '%s' is linked to '%s', cannot link to '%s'",
		   pin_name, sig->name, sig_name);
	    return -EINVAL;
	}
	/* check types */
	if (pin->type != sig->type) {
	    HALERR("type mismatch '%s' <- '%s'", pin_name, sig_name);
	    return -EINVAL;
	}
	/* linking output pin to sig that already has output or I/O pins? */
	if ((pin->dir == HAL_OUT) && ((sig->writers > 0) || (sig->bidirs > 0 ))) {
	    /* yes, can't do that */
	    HALERR("signal '%s' already has output or I/O pin(s)", sig_name);
	    return -EINVAL;
	}
	/* linking bidir pin to sig that already has output pin? */
	if ((pin->dir == HAL_IO) && (sig->writers > 0)) {
	    /* yes, can't do that */
	    HALERR("signal '%s' already has output pin", sig_name);
	    return -EINVAL;
	}
        /* everything is OK, make the new link */
        data_ptr_addr = SHMPTR(pin->data_ptr_addr);
	comp = halpr_find_owning_comp(pin->owner_id);
	data_addr = comp->shmem_base + sig->data_ptr;
	*data_ptr_addr = data_addr;

	if (( sig->readers == 0 ) && ( sig->writers == 0 ) && ( sig->bidirs == 0 )) {
	    /* this is the first pin for this signal, copy value from pin's "dummy" field */
	    data_addr = hal_shmem_base + sig->data_ptr;

	    // assure proper typing on assignment, assigning a hal_data_u is
	    // a surefire cause for memory corrupion as hal_data_u is larger
	    // than hal_bit_t, hal_s32_t, and hal_u32_t - this works only for 
	    // hal_float_t (!)
	    // my old, buggy code:
	    //*((hal_data_u *)data_addr) = pin->dummysig;

	    switch (pin->type) {
	    case HAL_BIT:
		*((hal_bit_t *) data_addr) = pin->dummysig.b;
		break;
	    case HAL_S32:
		*((hal_s32_t *) data_addr) = pin->dummysig.s;
		break;
	    case HAL_U32:
		*((hal_u32_t *) data_addr) = pin->dummysig.u;
		break;
	    case HAL_FLOAT:
		*((hal_float_t *) data_addr) = pin->dummysig.f;
		break;
	    default:
		hal_print_msg(RTAPI_MSG_ERR,
			      "HAL: BUG: pin '%s' has invalid type %d !!\n",
			      pin->name, pin->type);
		return -EINVAL;
	    }
	}

	/* update the signal's reader/writer/bidir counts */
	if ((pin->dir & HAL_IN) != 0) {
	    sig->readers++;
	}
	if (pin->dir == HAL_OUT) {
	    sig->writers++;
	}
	if (pin->dir == HAL_IO) {
	    sig->bidirs++;
	}
	/* and update the pin */
	pin->signal = SHMOFF(sig);
    }
    return 0;
}

int hal_unlink(const char *pin_name)
{

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STRLEN(pin_name, HAL_NAME_LEN);
    HALDBG("unlinking pin '%s'", pin_name);

    {
	hal_pin_t *pin __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing data structures */
	rtapi_mutex_get(&(hal_data->mutex));

	/* locate the pin */
	pin = halpr_find_pin_by_name(pin_name);

	if (pin == 0) {
	    /* not found */
	    HALERR("pin '%s' not found", pin_name);
	    return -EINVAL;
	}

	/* found pin, unlink it */
	unlink_pin(pin);

	// done, mutex will be released on scope exit automagically
	return 0;
    }
}


hal_sig_t *halpr_find_sig_by_name(const char *name)
{
    int next;
    hal_sig_t *sig;

    /* search signal list for 'name' */
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if (strcmp(sig->name, name) == 0) {
	    /* found a match */
	    return sig;
	}
	/* didn't find it yet, look at next one */
	next = sig->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

static hal_sig_t *alloc_sig_struct(void)
{
    hal_sig_t *p;

    /* check the free list */
    if (hal_data->sig_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->sig_free_ptr);
	/* unlink it from the free list */
	hal_data->sig_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_sig_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr = 0;
	p->type = 0;
	p->readers = 0;
	p->writers = 0;
	p->bidirs = 0;
	p->name[0] = '\0';
    }
    return p;
}

static void free_sig_struct(hal_sig_t * sig)
{
    hal_pin_t *pin;

    /* look for pins linked to this signal */
    pin = halpr_find_pin_by_sig(sig, 0);
    while (pin != 0) {
	/* found one, unlink it */
	unlink_pin(pin);
	/* check for another pin linked to the signal */
	pin = halpr_find_pin_by_sig(sig, pin);
    }
    /* clear contents of struct */
    sig->data_ptr = 0;
    sig->type = 0;
    sig->readers = 0;
    sig->writers = 0;
    sig->bidirs = 0;
    sig->handle = -1;
    sig->name[0] = '\0';
    /* add it to free list */
    sig->next_ptr = hal_data->sig_free_ptr;
    hal_data->sig_free_ptr = SHMOFF(sig);
}
