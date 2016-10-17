// HAL param API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

/***********************************************************************
*                       "PARAM" FUNCTIONS                              *
************************************************************************/

static int hal_param_newfv(hal_type_t type,
			   hal_param_dir_t dir,
			   volatile void *data_addr,
			   int owner_id,
			   const char *fmt,
			   va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        HALFAIL_RC(ENOMEM, "length %d invalid too long for name starting '%s'\n",
	       sz, name);
    }
    return hal_param_new(name, type, dir, (void *) data_addr, owner_id);
}

int hal_param_bit_newf(hal_param_dir_t dir, hal_bit_t * data_addr,
		       int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_BIT, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_float_newf(hal_param_dir_t dir, hal_float_t * data_addr,
			 int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_FLOAT, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_u32_newf(hal_param_dir_t dir, hal_u32_t * data_addr,
		       int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_U32, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_s32_newf(hal_param_dir_t dir, hal_s32_t * data_addr,
		       int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_S32, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

// printf-style version of hal_param_new()
int hal_param_newf(hal_type_t type,
		   hal_param_dir_t dir,
		   volatile void * data_addr,
		   int owner_id,
		   const char *fmt, ...)
{
    va_list ap;
    void *p;
    va_start(ap, fmt);
    p = halg_param_newfv(1, type, dir, data_addr, owner_id, fmt, ap);
    va_end(ap);
    return p == NULL ? _halerrno : 0;
}

int halg_param_newf(const int use_hal_mutex,
		    hal_type_t type,
		    hal_param_dir_t dir,
		    volatile void * data_addr,
		    int owner_id,
		    const char *fmt, ...)
{
    va_list ap;
    void *p;
    va_start(ap, fmt);
    p = halg_param_newfv(use_hal_mutex, type, dir, data_addr, owner_id, fmt, ap);
    va_end(ap);
    return p == NULL ? _halerrno : 0;
}


/* this is a generic function that does the majority of the work. */
// v2
hal_param_t *halg_param_newfv(const int use_hal_mutex,
			      hal_type_t type,
			      hal_param_dir_t dir,
			      volatile void *data_addr,
			      int owner_id,
			      const char *fmt, va_list ap)
{
    PCHECK_HALDATA();
    PCHECK_LOCK(HAL_LOCK_LOAD);
    PCHECK_NULL(fmt);

    char buf[HAL_MAX_NAME_LEN + 1];
    char *name = fmt_ap(buf, sizeof(buf), fmt, ap);

    PCHECK_NULL(name);

    hal_param_t *new;

    if (type != HAL_BIT && type != HAL_FLOAT && type != HAL_S32 && type != HAL_U32) {
	HALFAIL_NULL(EINVAL,
		     "param '%s': param type not one of HAL_BIT,"
		     " HAL_FLOAT, HAL_S32 or HAL_U32",
		     name);
    }

    if (dir != HAL_RO && dir != HAL_RW) {
	HALFAIL_NULL(EINVAL,
		     "param '%s': param direction not one of HAL_RO, or HAL_RW",
		     name);
    }

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_comp_t *comp;
	bool is_legacy = false;

	HALDBG("creating parameter '%s'\n", name);

	/* validate comp_id */
	comp = halpr_find_owning_comp(owner_id);
	if (comp == 0) {
	    /* bad comp_id */
	    HALFAIL_NULL(EINVAL, "param '%s': owning component %d not found\n",
			 name, owner_id);
	}

	if (data_addr != NULL) {
	    // the v2 params dont use data_ptr but refer to param->value
	    is_legacy = true;

	    /* validate passed in pointer - must point to HAL shmem */
	    if (! SHMCHK(data_addr)) {
		/* bad pointer */
		HALFAIL_NULL(EINVAL, "param '%s': data_addr not in shared memory\n", name);
	    }
	} // else a v2 param whose value is contained in the descriptor

	// this will be 0 for legacy comps which use comp_id
	hal_inst_t *inst = halpr_find_inst_by_id(owner_id);
	int inst_id = (inst ? hh_get_id(&inst->hdr) : 0);

	// instances may create params post hal_ready
	// never understood the restriction in the first place
	if ((inst_id == 0) && (comp->state > COMP_INITIALIZING)) {
	    HALFAIL_NULL(EINVAL, "component '%s': %s called after hal_ready",
			 ho_name(comp), __FUNCTION__);
	}

	// allocate parameter descriptor
	if ((new = halg_create_objectf(0, sizeof(hal_param_t),
				       HAL_PARAM, owner_id, name)) == NULL)
	    return NULL; //  _halerrno, error msg set

	if (is_legacy) {
	    // v1 semantics
	    new->data_ptr = SHMOFF(data_addr);
	    hh_set_legacy(&new->hdr);
	} else {
	    // v2 param. Bend data_ptr to point to value.
	    // param accessors rely on it for v1/v2 compatibility
	    new->data_ptr = SHMOFF(&new->value);
	}
	new->type = type;
	new->dir = dir;

	// make it visible
	halg_add_object(false, (hal_object_ptr)new);
    }
    return 0;
}


#if 0

/* wrapper functs for typed params - these call the generic funct below */

int hal_param_bit_set(const char *name, int value)
{
    return hal_param_set(name, HAL_BIT, &value);
}

int hal_param_float_set(const char *name, double value)
{
    return hal_param_set(name, HAL_FLOAT, &value);
}

int hal_param_u32_set(const char *name, unsigned long value)
{
    return hal_param_set(name, HAL_U32, &value);
}

int hal_param_s32_set(const char *name, signed long value)
{
    return hal_param_set(name, HAL_S32, &value);
}

/* this is a generic function that does the majority of the work */

int hal_param_set(const char *name, hal_type_t type, void *value_addr)
{

    void *d_ptr;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_PARAMS);
    CHECK_STRLEN(name, HAL_NAME_LEN);

    HALDBG("setting parameter '%s'\n", name);

    {
	WITH_HAL_MUTEX();

	/* search param list for name */
	hal_param_t *param = halpr_find_param_by_name(name);
	if (param == 0) {
	    /* parameter not found */
	    HALFAIL_RC(EINVAL, ("parameter '%s' not found\n", name);
	}
	/* found it, is type compatible? */
	if (param->type != type) {
	    HALFAIL_RC(EINVAL, "parameter '%s': type mismatch %d != %d\n",
		   name, param->type, type);
	}
	/* is it read only? */
	if (param->dir == HAL_RO) {
	    HALFAIL_RC(EINVAL, "parameter '%s': param is not writable\n", name);
	}
	/* everything is OK, set the value */
	d_ptr = SHMPTR(param->data_ptr);
	switch (param->type) {
	case HAL_BIT:
	    if (*((int *) value_addr) == 0) {
		*(hal_bit_t *) (d_ptr) = 0;
	    } else {
		*(hal_bit_t *) (d_ptr) = 1;
	    }
	    break;
	case HAL_FLOAT:
	    *((hal_float_t *) (d_ptr)) = *((double *) (value_addr));
	    break;
	case HAL_S32:
	    *((hal_s32_t *) (d_ptr)) = *((signed long *) (value_addr));
	    break;
	case HAL_U32:
	    *((hal_u32_t *) (d_ptr)) = *((unsigned long *) (value_addr));
	    break;
	default:
	    /* Shouldn't get here, but just in case... */
	    HALFAIL_RC(EINVAL, "parameter '%s': bad type %d setting param\n",
		   name, param->type);
	}
    }
    return 0;
}
#endif // unused
