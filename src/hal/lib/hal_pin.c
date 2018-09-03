// HAL pin API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_group.h"
#include "hal_internal.h"

/***********************************************************************
*                        "PIN" FUNCTIONS                               *
************************************************************************/

int hal_pin_bit_newf(hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    void *p;
    hal_data_u defval = {._b = 0};

    va_start(ap, fmt);
    p = halg_pin_newfv(1, HAL_BIT, dir, (void**)data_ptr_addr,
		       owner_id, defval, fmt, ap);
    va_end(ap);
    return p ? 0 : _halerrno;
}

int hal_pin_float_newf(hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    void *p;
    hal_data_u defval = {._f = 0.0};
    va_start(ap, fmt);
    p = halg_pin_newfv(1, HAL_FLOAT, dir, (void**)data_ptr_addr,
		       owner_id, defval,  fmt, ap);
    va_end(ap);
    return p ? 0 : _halerrno;
}

int hal_pin_u32_newf(hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    void *p;
    hal_data_u defval = {._u = 0};
    va_start(ap, fmt);
    p = halg_pin_newfv(1, HAL_U32, dir, (void**)data_ptr_addr, owner_id,
		       defval,  fmt, ap);
    va_end(ap);
    return p ? 0 : _halerrno;
}

int hal_pin_s32_newf(hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    void *p;
    hal_data_u defval = {._s = 0};
    va_start(ap, fmt);
    p = halg_pin_newfv(1,HAL_S32, dir, (void**)data_ptr_addr,
		       owner_id, defval,  fmt, ap);
    va_end(ap);
    return p ? 0 : _halerrno;
}

int hal_pin_u64_newf(hal_pin_dir_t dir,
    hal_u64_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    void *p;
    hal_data_u defval = {._u = 0};
    va_start(ap, fmt);
    p = halg_pin_newfv(1, HAL_U64, dir, (void**)data_ptr_addr, owner_id,
		       defval,  fmt, ap);
    va_end(ap);
    return p ? 0 : _halerrno;
}

int hal_pin_s64_newf(hal_pin_dir_t dir,
    hal_s64_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
{
    va_list ap;
    void *p;
    hal_data_u defval = {._s = 0};
    va_start(ap, fmt);
    p = halg_pin_newfv(1,HAL_S64, dir, (void**)data_ptr_addr,
		       owner_id, defval,  fmt, ap);
    va_end(ap);
    return p ? 0 : _halerrno;
}

int zero_hal_data_u(const int type, hal_data_u *u)
{
    switch (type) {
    case HAL_BIT:
	set_bit_value(u, 0);
	break;
    case HAL_FLOAT:
	set_float_value(u, 0.0);
	break;
    case HAL_S32:
	set_s32_value(u, 0);
	break;
    case HAL_U32:
	set_u32_value(u, 0);
	break;
    case HAL_S64:
	set_s64_value(u, 0);
	break;
    case HAL_U64:
	set_u64_value(u, 0);
	break;
    default:
	HALFAIL_RC(EINVAL,"invalid hal_data_u type %d", type);
    }
    return 0;
}

// printf-style version of hal_pin_new()
int hal_pin_newf(hal_type_t type,
		 hal_pin_dir_t dir,
		 void ** data_ptr_addr,
		 int owner_id,
		 const char *fmt, ...)
{
    va_list ap;
    void *p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));

    va_start(ap, fmt);
    p = halg_pin_newfv(1, type, dir, data_ptr_addr,
		       owner_id, defval, fmt, ap);
    va_end(ap);
    return p ? 0 : _halerrno;
}

// generic printf-style version of hal_pin_new()
hal_pin_t *halg_pin_newf(const int use_hal_mutex,
			 hal_type_t type,
			 hal_pin_dir_t dir,
			 void ** data_ptr_addr,
			 int owner_id,
			 const char *fmt, ...)
{
    va_list ap;
    hal_pin_t *p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));

    va_start(ap, fmt);
    p = halg_pin_newfv(use_hal_mutex, type, dir, data_ptr_addr,
		       owner_id, defval, fmt, ap);
    va_end(ap);
    return p;
}

/* this is a generic function that does the majority of the work. */
hal_pin_t *halg_pin_newfv(const int use_hal_mutex,
			  const hal_type_t type,
			  const hal_pin_dir_t dir,
			  void **data_ptr_addr,
			  const int owner_id,
			  const hal_data_u defval,
			  const char *fmt, va_list ap)
{
    PCHECK_HALDATA();
    PCHECK_LOCK(HAL_LOCK_LOAD);
    PCHECK_NULL(fmt);

    char buf[HAL_MAX_NAME_LEN + 1];
    char *name = fmt_ap(buf, sizeof(buf), fmt, ap);
    PCHECK_NULL(name);

    hal_pin_t *new;
    bool is_legacy = false;

    if (!hal_valid_type(type)) {
	HALFAIL_NULL(EINVAL,
		     "pin '%s': pin type not a legit HAL type (%d)",
		     name, type);
    }
    if (!hal_valid_dir(dir)) {
	HALFAIL_NULL(EINVAL,"pin '%s': pin direction not one of HAL_IN, HAL_OUT, or HAL_IO (%d)",
		     name, dir);
    }
    {
	char value[100];
	hals_value(value, sizeof(value), type, &defval);
	HALDBG("creating pin '%s' %s %s %s",
	       name, hals_type(type), hals_pindir(dir), value);
    }
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_comp_t *comp;

	if (halpr_find_pin_by_name(name) != NULL) {
	    HALFAIL_NULL(EEXIST, "duplicate pin '%s'", name);
	}

	/* validate comp_id */
	comp = halpr_find_owning_comp(owner_id);
	if (comp == 0) {
	    /* bad comp_id */
	    HALFAIL_NULL(EINVAL,
			 "pin '%s': owning component %d not found",
			 name, owner_id);
	}

	/* validate passed in pointer - must point to HAL shmem */
	if (data_ptr_addr != 0) {
	    if(*data_ptr_addr) {
		HALERR("pin '%s': called with already-initialized memory", name);
	    }
	    // the v2 pins dont use data_ptr_addr
	    is_legacy = true;
	    if (! SHMCHK(data_ptr_addr)) {
		/* bad pointer */
		HALFAIL_NULL(EINVAL, "pin '%s': data_ptr_addr not in shared memory", name);
	    }
	}

#ifdef UNNECESSARY
	// this will be 0 for legacy comps which use comp_id to
	// refer to a comp - pins owned by an instance will refer
	// to an instance:
	hal_inst_t *inst = halpr_find_inst_by_id(owner_id);

	int inst_id = (inst ? ho_id(inst) : 0);

	// instances may create pins post hal_ready
	if ((inst_id == 0) && (comp->state > COMP_INITIALIZING)) {
	    // legacy error message. Never made sense.. why?
	    HALFAIL_NULL(EINVAL, "pin '%s': hal_pin_new called after hal_ready (%d)",
		   name, comp->state);
	}
#endif

	// allocate pin descriptor
	if ((new = halg_create_objectf(0, sizeof(hal_pin_t),
				       HAL_PIN, owner_id, name)) == NULL) {
	    return NULL;
	}

	/* initialize the structure */
	new->type = type;
	new->dir = dir;
	new->_signal = 0;
	memcpy(&new->dummysig, (void *)&defval, sizeof(hal_data_u));
	if (is_legacy) {
	    hh_set_legacy(&new->hdr);
	    new->_data_ptr_addr = SHMOFF(data_ptr_addr);
	    /* make 'data_ptr' point to dummy signal */
	    *data_ptr_addr = comp->shmem_base + SHMOFF(&(new->dummysig));
	} else {
	    // poison the old value** to ease debugging
	    new->_data_ptr_addr =  SHMOFF(&(hal_data->dead_beef));
	}
	// since in v2 this is just a value *, not a value**
	// just make it point to the dummy signal
	new->data_ptr = SHMOFF(&new->dummysig);

	// make object visible
	halg_add_object(false, (hal_object_ptr)new);
	return new;
    }
}

void unlink_pin(hal_pin_t * pin)
{
    hal_sig_t *sig;
    hal_comp_t *comp;
    void **data_ptr_addr;
    hal_data_u  *sig_data_addr;

    /* is this pin linked to a signal? */
    if (pin_is_linked(pin)) {
	hal_data_u *dummy_addr;

	/* yes, need to unlink it */
	sig = signal_of(pin);

	if (hh_get_legacy(&pin->hdr)) {

	    /* make pin's 'data_ptr' point to its dummy signal */
	    data_ptr_addr = SHMPTR(pin->_data_ptr_addr);
	    comp = halpr_find_owning_comp(ho_owner_id(pin));
	    dummy_addr = comp->shmem_base + SHMOFF(&(pin->dummysig));
	    *data_ptr_addr = dummy_addr;

	    dummy_addr = (hal_data_u *)(hal_shmem_base + SHMOFF(&(pin->dummysig))); // XXX use SHMPTR
	} else {
	    dummy_addr = (hal_data_u *) &pin->dummysig;
	}
	pin->data_ptr = SHMOFF(&(pin->dummysig));

	/* copy current signal value to dummy */
	//XXX use SHMPTR
	sig_data_addr = (hal_data_u *)(hal_shmem_base + SHMOFF(&sig->value));


	switch (pin->type) {
	case HAL_BIT:
	    set_bit_value(dummy_addr, get_bit_value(sig_data_addr));
	    break;
	case HAL_S32:
	    set_s32_value(dummy_addr, get_s32_value(sig_data_addr));
	    break;
	case HAL_U32:
	    set_u32_value(dummy_addr, get_u32_value(sig_data_addr));
	    break;
	case HAL_FLOAT:
	    set_float_value(dummy_addr, get_float_value(sig_data_addr));
	    break;
	default:
	    hal_print_msg(RTAPI_MSG_ERR,
			  "HAL: BUG: pin '%s' has invalid type %d !!\n",
			  ho_name(pin), pin->type);
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
	pin_set_unlinked(pin);

	// propagate the news
	rtapi_smp_mb();
    }
}

void free_pin_struct(hal_pin_t * pin)
{
    if (pin == NULL)
	return;
    unlink_pin(pin);
    halg_free_object(false, (hal_object_ptr) pin);
}
