
#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

#if defined(ULAPI)
#include <stdio.h>
#include <sys/types.h>		/* pid_t */
#include <unistd.h>		/* getpid() */
#endif

static hal_vtable_t *alloc_vtable_struct(void);
static void free_vtable_struct(hal_vtable_t *c);


/***********************************************************************
*                     Public HAL vtable functions                       *
************************************************************************/
int hal_export_vtable(const char *name, int version, void *vtref, int comp_id)
{
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: hal_export_vtable called before init\n");
	return -EINVAL;
    }
    if (vtref == NULL) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "HAL: ERROR: hal_export_vtable called with NULL vtable\n");
	return -EINVAL;
    }
    if (!name) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: hal_export_vtable: NULL name\n");
	return -EINVAL;
    }
    if (strlen(name) > HAL_NAME_LEN) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: ERROR: vtable name '%s' is too long\n", name);
	return -EINVAL;
    }
    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));

	if (hal_data->lock & HAL_LOCK_LOAD)  {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: hal_export_vtable called while HAL locked\n");
	    return -EPERM;
	}

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_name(name, version)) != 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: vtable '%s' already exists\n", name);
	    return -EEXIST;
	}

	// allocate a new vtable descriptor in the HAL shm segment
	if ((vt = alloc_vtable_struct()) == NULL) {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: insufficient memory for vtable '%s'\n",
			  name);
	    return -ENOMEM;
	}

	vt->refcount = 0;
	vt->vtable  =  vtref;
	vt->version =  version;
	vt->handle = rtapi_next_handle();
	vt->comp_id = comp_id;
#ifdef RTAPI
	vt->context = 0;  // this is in RTAPI, and can be shared
#else
	vt->context = getpid(); // in per-process memory, no shareable code
#endif
	rtapi_snprintf(vt->name, sizeof(vt->name), "%s", name);

	// insert new structure at head of vtable list
	vt->next_ptr = hal_data->vtable_list_ptr;
	hal_data->vtable_list_ptr = SHMOFF(vt);

	hal_print_msg(RTAPI_MSG_DBG, "HAL: created vtable '%s' vtable=%p version=%d\n",
		      vt->name, vt->vtable, vt->version);

	// automatic unlock by scope exit
	return vt->handle;
    }
}


int hal_remove_vtable(int vtable_id)
{
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: hal_remove_vtable called before init\n");
	return -EINVAL;
    }
    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));
	if (hal_data->lock & HAL_LOCK_LOAD)  {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: hal_remove_vtable called while HAL locked\n");
	    return -EPERM;
	}

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_id(vtable_id)) == NULL) {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: hal_remove_vtable(%d): vtable not found\n", vtable_id);
	    return -ENOENT;
	}
	// still referenced?
	if (vt->refcount > 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: hal_remove_vtable(%d): busy (refcount=%d)\n",
			  vtable_id, vt->refcount);
	    return -ENOENT;

	}
	int *prev, next;
	hal_vtable_t *c;

	prev = &(hal_data->vtable_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		hal_print_msg(RTAPI_MSG_ERR,
			      "HAL: ERROR: vtable %d not found\n", vtable_id);
		return -EINVAL;
	    }
	    c = SHMPTR(next);
	    if ( c == vt ) {
		*prev = c->next_ptr; // unlink
		break;
	    }
	    prev = &(c->next_ptr);
	    next = *prev;
	}
	free_vtable_struct(vt);
	hal_print_msg(RTAPI_MSG_DBG,
		      "HAL: hal_remove_vtable(%d): vtable %s/%d removed\n",
		      vtable_id, vt->name, vt->version);
	return 0;
    }
}

// returns vtable_id (handle) or error code
// increases refcount
int hal_reference_vtable(const char *name, int version, void **vtableref)
{
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "HAL: ERROR: hal_reference_vtable called before init\n");
	return -EINVAL;
    }
    if (vtableref == NULL) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "HAL: ERROR: hal_reference_vtable called with NULL vtable\n");
	return -EINVAL;
    }
    if (!name) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "HAL: ERROR: hal_export_vtable: NULL name\n");
	return -EINVAL;
    }
    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));

	if (hal_data->lock & HAL_LOCK_LOAD)  {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: hal_reference_vtable called while HAL locked\n");
	    return -EPERM;
	}

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_name(name, version)) == NULL) {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: vtable '%s' version %d not found\n",
			  name, version);
	    return -ENOENT;
	}

	// make sure it's in the proper context
#ifdef RTAPI
	int context = 0;  // this is in RTAPI, and can be shared
#else
	int context = getpid(); // in per-process memory, no shareable code
#endif
	if (vt->context != context) {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: hal_reference_vtable(%s,%d): "
			  "context mismatch - found context %d\n",
			  name, version, vt->context);
	    return -ENOENT;
	}

	vt->refcount += 1;
	*vtableref = vt->vtable;
	hal_print_msg(RTAPI_MSG_DBG,
		      "HAL: hal_reference_vtable(%s,%d) found vtable=%p context=%d\n",
		      vt->name, vt->version, vt->vtable, vt->context);

	// automatic unlock by scope exit
	return vt->handle;
    }
}

// drops refcount
int hal_unreference_vtable(int vtable_id)
{
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "HAL: ERROR: hal_unreference_vtable called before init\n");
	return -EINVAL;
    }
    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_id(vtable_id)) == NULL) {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: hal_unreference_vtable(id=%d) not found\n",
			  vtable_id);
	    return -ENOENT;
	}

	// make sure it's in the proper context
#ifdef RTAPI
	int context = 0;  // this is in RTAPI, and can be shared
#else
	int context = getpid(); // in per-process memory, no shareable code
#endif
	if (vt->context != context) {
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: ERROR: hal_unreference_vtable(%d) (name=%s): "
			  "context mismatch - calling context %d vtable context %d\n",
			  vtable_id, vt->name, context, vt->context);
	    return -ENOENT;
	}

	vt->refcount -= 1;
	hal_print_msg(RTAPI_MSG_DBG,
		      "HAL: hal_unreference_vtable(%d) (name=%s) refcount=%d\n",
		      vtable_id, vt->name, vt->refcount);

	// automatic unlock by scope exit
	return 0;
    }
}

// private HAL API

hal_vtable_t *halpr_find_vtable_by_name(const char *name, int version)
{
    int next;
    hal_vtable_t *vt;

    next = hal_data->vtable_list_ptr;
    while (next != 0) {
	vt = SHMPTR(next);
	if (((strcmp(vt->name, name) == 0) &&
	     vt->version == version)) {
	    return vt;
	}
	next = vt->next_ptr;
    }
    return 0;
}

hal_vtable_t *halpr_find_vtable_by_id(int vtable_id)
{
    int next;
    hal_vtable_t *vt;

    next = hal_data->vtable_list_ptr;
    while (next != 0) {
	vt = SHMPTR(next);
	if (vt->handle == vtable_id) {
	    return vt;
	}
	next = vt->next_ptr;
    }
    return 0;
}


static hal_vtable_t *alloc_vtable_struct(void)
{
    hal_vtable_t *p;

    if (hal_data->vtable_free_ptr != 0) {
	p = SHMPTR(hal_data->vtable_free_ptr);
	hal_data->vtable_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	p = shmalloc_dn(sizeof(hal_vtable_t));
    }
    return p;
}

static void free_vtable_struct(hal_vtable_t * p)
{
    p->next_ptr = hal_data->vtable_free_ptr;
    hal_data->vtable_free_ptr = SHMOFF(p);
}


#ifdef RTAPI

EXPORT_SYMBOL(hal_export_vtable);
EXPORT_SYMBOL(hal_remove_vtable);
EXPORT_SYMBOL(hal_reference_vtable);
EXPORT_SYMBOL(hal_unreference_vtable);
EXPORT_SYMBOL(halpr_find_vtable_by_name);
EXPORT_SYMBOL(halpr_find_vtable_by_id);
#endif
