// HAL miscellaneous functions which dont clearly fit elsewhere

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

/***********************************************************************
*                     utility functions, mostly used by haltalk        *
*                                                                      *
************************************************************************/

// return number of pins in a component
int halpr_pin_count(const char *name)
{
    hal_comp_t *comp;
    hal_comp_t *owner;
    hal_pin_t *pin;
    int count = 0;

    comp = halpr_find_comp_by_name(name);
    if (comp == 0)
	return -ENOENT;

    int next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = (hal_pin_t *)SHMPTR(next);
	owner = halpr_find_owning_comp(pin->owner_id);
	if (owner->comp_id == comp->comp_id)
	    count++;
	next = pin->next_ptr;
    }
    return count;
}

// return number of params in a component
int
halpr_param_count(const char *name)
{
    hal_comp_t *comp;
    hal_comp_t *owner;
    int count = 0;

    comp = halpr_find_comp_by_name(name);
    if (comp == 0)
	return -ENOENT;

    int next = hal_data->param_list_ptr;
    while (next != 0) {
	hal_param_t *param = (hal_param_t *)SHMPTR(next);
	owner = halpr_find_owning_comp(param->owner_id);
	if (owner->comp_id == comp->comp_id)
	    count++;
	next = param->next_ptr;
    }
    return count;
}

// hal mutex scope-locked version of halpr_find_pin_by_name()
hal_pin_t *
hal_find_pin_by_name(const char *name)
{
    hal_pin_t *p __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));
    p = halpr_find_pin_by_name(name);
    return p;
}

int
hal_comp_state_by_name(const char *name)
{
    hal_comp_t *comp __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));

    comp = halpr_find_comp_by_name(name);
    if (comp == NULL)
	return -ENOENT;
    return comp->state;
}
