
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
/***********************************************************************
*                     Public HAL vtable functions                       *
************************************************************************/
int halg_export_vtable(const int use_hal_mutex,
		       const char *name,
		       int version,
		       void *vtref,
		       int comp_id)
{
    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_NULL(vtref);
    CHECK_LOCK(HAL_LOCK_LOAD);

    HALDBG("exporting vtable '%s' version=%d owner=%d at %p",
	   name, version, comp_id, vtref);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_vtable_t *vt;

	// make sure no such vtable name already exists
	if ((vt = halg_find_vtable_by_name(0, name, version)) != 0) {
	    HALFAIL_RC(EEXIST, "vtable '%s' already exists", name);
	}

	// allocate vtable descriptor
	if ((vt = halg_create_objectf(0, sizeof(hal_vtable_t),
				      HAL_VTABLE, comp_id, name)) == NULL)
	    return _halerrno;

	vt->vtable  =  vtref;
	vt->version =  version;
#ifdef RTAPI
	vt->context = 0;  // this is in RTAPI, and can be shared
#else
	vt->context = getpid(); // in per-process memory, no shareable code
#endif
	// make it visible
	halg_add_object(false, (hal_object_ptr)vt);

	HALDBG("created vtable '%s' vtable=%p version=%d",
	       hh_get_name(&vt->hdr), vt->vtable, vt->version);

	// automatic unlock by scope exit
	return hh_get_id(&vt->hdr);
    }
}


int halg_remove_vtable(const int use_hal_mutex, const int vtable_id)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_vtable_t *vt;

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_id(vtable_id)) == NULL) {
	    HALFAIL_RC(ENOENT, "vtable %d not found", vtable_id);
	}
	// still referenced?
	if (ho_referenced(vt)) {
	    HALFAIL_RC(ENOENT, "vtable %d busy (refcount=%d)",
		   vtable_id, ho_refcnt(vt));
	}
	HALDBG("vtable %s/%d version %d removed",
	       hh_get_name(&vt->hdr), vtable_id,  vt->version);

	return halg_free_object(false, (hal_object_ptr)vt);
    }
}

// returns vtable_id (handle) or error code
// increases refcount
int halg_reference_vtable(const int use_hal_mutex,
			 const char *name,
			 int version,
			 void **vtableref)
{
    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_NULL(vtableref);
    CHECK_LOCK(HAL_LOCK_LOAD);

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_vtable_t *vt;

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_name(name, version)) == NULL) {
	    HALFAIL_RC(ENOENT, "vtable '%s' version %d not found", name, version);
	}

	// make sure it's in the proper context
#ifdef RTAPI
	int context = 0;  // this is in RTAPI, and can be shared
#else
	int context = getpid(); // in per-process memory, no shareable code
#endif
	if (vt->context != context) {
	    HALFAIL_RC(ENOENT, "vtable %s version %d: "
		   "context mismatch - found context %d",
		   name, version, vt->context);
	}

	ho_incref(vt);

	*vtableref = vt->vtable;
	HALDBG("vtable %s,%d found vtable=%p context=%d",
	       hh_get_name(&vt->hdr), vt->version, vt->vtable, vt->context);

	// automatic unlock by scope exit
	return hh_get_id(&vt->hdr);
    }
}

// drops refcount
int halg_unreference_vtable(const int use_hal_mutex, int vtable_id)
{
    CHECK_HALDATA();
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_vtable_t *vt;

	// make sure no such vtable name already exists
	if ((vt = halpr_find_vtable_by_id(vtable_id)) == NULL) {
	    HALFAIL_RC(ENOENT, "vtable %d not found", vtable_id);
	}

	// make sure it's in the proper context
#ifdef RTAPI
	int context = 0;  // this is in RTAPI, and can be shared
#else
	int context = getpid(); // in per-process memory, no shareable code
#endif
	if (vt->context != context) {
	    HALFAIL_RC(ENOENT, "vtable %s/%d: "
		   "context mismatch - calling context %d vtable context %d",
		   hh_get_name(&vt->hdr), vtable_id, context, vt->context);
	}
	ho_decref(vt);
	HALDBG("vtable %s/%d refcount=%d",
	       hh_get_name(&vt->hdr),
	       vtable_id, ho_refcnt(vt));

	// automatic unlock by scope exit
	return 0;
    }
}

hal_vtable_t *halg_find_vtable_by_name(const int use_hal_mutex,
				       const char *name,
				       const int version)
{
    foreach_args_t args =  {
	.type = HAL_VTABLE,
	.name = (char *)name,
	.user_arg1 = version,
	.user_ptr1 = NULL
    };
    if (halg_foreach(use_hal_mutex, &args, yield_versioned_vtable_object))
	return args.user_ptr1;
    return NULL;
}

// user_arg2 returns the number of vtables exported by the
// comp with id passed in in user_arg1
static int count_exported_vtables_cb(hal_object_ptr o,
				     foreach_args_t *args)
{
    if (hh_get_owner_id(o.hdr) == args->user_arg1) {
	args->user_arg2++;
    }
    return 0;
}

int halg_count_exported_vtables(const int use_hal_mutex,
				const int comp_id)
{
    foreach_args_t args =  {
	.type = HAL_VTABLE,
	.user_arg1 = comp_id,
	.user_arg2 = 0, // returned count of exported vtables
    };
    halg_foreach(use_hal_mutex, &args, count_exported_vtables_cb);
    return args.user_arg2;
}

#ifdef RTAPI
EXPORT_SYMBOL(halg_export_vtable);
EXPORT_SYMBOL(halg_remove_vtable);
EXPORT_SYMBOL(halg_reference_vtable);
EXPORT_SYMBOL(halg_unreference_vtable);
EXPORT_SYMBOL(halg_find_vtable_by_name);
EXPORT_SYMBOL(halg_count_exported_vtables);
#endif
