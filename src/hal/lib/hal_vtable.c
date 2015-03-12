
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
    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_NULL(vtref);
    CHECK_LOCK(HAL_LOCK_LOAD);

    HALDBG("exporting vtable '%s' version=%d owner=%d at %p",
	   name, version, comp_id, vtref);

    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_name(name, version)) != 0) {
	    HALERR("vtable '%s' already exists", name);
	    return -EEXIST;
	}

	// allocate a new vtable descriptor in the HAL shm segment
	if ((vt = alloc_vtable_struct()) == NULL)
	    NOMEM("vtable '%s'",  name);

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

	HALDBG("created vtable '%s' vtable=%p version=%d",
	       vt->name, vt->vtable, vt->version);

	// automatic unlock by scope exit
	return vt->handle;
    }
}


int hal_remove_vtable(int vtable_id)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);

    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_id(vtable_id)) == NULL) {
	    HALERR("vtable %d not found", vtable_id);
	    return -ENOENT;
	}
	// still referenced?
	if (vt->refcount > 0) {
	    HALERR("vtable %d busy (refcount=%d)",
		   vtable_id, vt->refcount);
	    return -ENOENT;

	}
	int *prev, next;
	hal_vtable_t *c;

	prev = &(hal_data->vtable_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		HALERR("vtable %d not found", vtable_id);
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
	HALDBG("vtable %s/%d version %d removed",
	       vt->name, vtable_id,  vt->version);
	return 0;
    }
}

// returns vtable_id (handle) or error code
// increases refcount
int hal_reference_vtable(const char *name, int version, void **vtableref)
{
    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_NULL(vtableref);
    CHECK_LOCK(HAL_LOCK_LOAD);

    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_name(name, version)) == NULL) {
	    HALERR("vtable '%s' version %d not found", name, version);
	    return -ENOENT;
	}

	// make sure it's in the proper context
#ifdef RTAPI
	int context = 0;  // this is in RTAPI, and can be shared
#else
	int context = getpid(); // in per-process memory, no shareable code
#endif
	if (vt->context != context) {
	    HALERR("vtable %s version %d: "
		   "context mismatch - found context %d",
		   name, version, vt->context);
	    return -ENOENT;
	}

	vt->refcount += 1;
	*vtableref = vt->vtable;
	HALDBG("vtable %s,%d found vtable=%p context=%d",
	       vt->name, vt->version, vt->vtable, vt->context);

	// automatic unlock by scope exit
	return vt->handle;
    }
}

// drops refcount
int hal_unreference_vtable(int vtable_id)
{
    CHECK_HALDATA();
    {
	hal_vtable_t *vt __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_id(vtable_id)) == NULL) {
	    HALERR("vtable %d not found", vtable_id);
	    return -ENOENT;
	}

	// make sure it's in the proper context
#ifdef RTAPI
	int context = 0;  // this is in RTAPI, and can be shared
#else
	int context = getpid(); // in per-process memory, no shareable code
#endif
	if (vt->context != context) {
	    HALERR("vtable %s/%d: "
		   "context mismatch - calling context %d vtable context %d",
		   vt->name, vtable_id, context, vt->context);
	    return -ENOENT;
	}

	vt->refcount -= 1;
	HALDBG("vtable %s/%d refcount=%d", vt->name, vtable_id, vt->refcount);

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
