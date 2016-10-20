// HAL instance API

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

int halg_inst_create(const int use_hal_mutex,
		     const char *name,
		     const int comp_id,
		     const int size,
		     void **inst_data)
{
    CHECK_HALDATA();
    CHECK_STR(name);

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_inst_t *inst;
	hal_comp_t *comp;
	void *m = NULL;

	// comp must exist
	if ((comp = halg_find_object_by_id(0, HAL_COMPONENT, comp_id).comp) == 0) {
	    HALFAIL_RC(ENOENT, "comp %d not found", comp_id);
	}

	// inst may not exist
	if ((inst = halg_find_object_by_name(0, HAL_INST, name).inst) != NULL) {
	    HALFAIL_RC(EEXIST, "instance '%s' already exists", name);
	}

	// allocate instance descriptor
	if ((inst = halg_create_objectf(0, sizeof(hal_inst_t),
					HAL_INST, ho_id(comp), name)) == NULL)
	    return _halerrno;

	if (size > 0) {
	    // the instance data is likely to contain pins so
	    // allocate in 'rt' memory for cache friendliness
	    m = shmalloc_rt(size);
	    if (m == NULL)
		HALFAIL_RC(ENOMEM, " instance %s: cant allocate %d bytes", name, size);
	    memset(m, 0, size);
	}

	inst->inst_data_ptr = SHMOFF(m);
	inst->inst_size = size;
	inst->frozen_argv = NULL; // set in create_instance post-call

	HALDBG("%s: creating instance '%s' size %d",
#ifdef RTAPI
	       "rtapi",
#else
	       "ulapi",
#endif
	       hh_get_name(&inst->hdr), size);

	// if not NULL, pass pointer to blob
	if (inst_data)
	    *(inst_data) = m;

	// make it visible
	halg_add_object(0, (hal_object_ptr)inst);
	return ho_id(inst);
    }
}

int halg_inst_delete(const int use_hal_mutex, const char *name)
{
    CHECK_HALDATA();
    CHECK_STR(name);

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_inst_t *inst;

	// inst must exist
	if ((inst = halg_find_object_by_name(0, HAL_INST, name).inst) == NULL) {
	    HALFAIL_RC(ENOENT, "instance '%s' does not exist", name);
	}
	// this does most of the heavy lifting
	free_inst_struct(inst);
    }
    return 0;
}

// lookup instance by instance ID
hal_inst_t *halg_find_inst_by_id(const int use_hal_mutex,
				 const int id)
{
    foreach_args_t args =  {
	.type = HAL_INST,
	.id = id,
	.user_ptr1 = NULL
    };
    if (halg_foreach(use_hal_mutex, &args, yield_match))
	return args.user_ptr1;
    return NULL;
}

// assumed to be called with HAL mutex held
void free_inst_struct(hal_inst_t * inst)
{
    // can't delete the instance until we delete its "stuff"
    // need to check for functs only if a realtime component

    foreach_args_t args =  {
	// search for objects owned by this instance
	.owner_id  = ho_id(inst),
    };

#ifdef RTAPI

    args.type = HAL_FUNCT;
    halg_foreach(0, &args, yield_free);

    // now that the funct is gone, call the dtor for this instance
    // get owning comp

    hal_comp_t *comp = halpr_find_owning_comp(ho_id(inst));

    // NB: a custom dtor will be called with the HAL mutex held
    if (comp->dtor) {
	// NB - pins, params etc still intact
	// this instance is owned by this comp, call destructor
	HALDBG("calling custom destructor(%s,%s)",
	       ho_name(comp),
	       ho_name(inst));
	// temporarily unlock HAL mutex - for now, until all halg_* methods
	rtapi_mutex_give(&(hal_data->mutex));
	comp->dtor(ho_name(inst),
		   SHMPTR(inst->inst_data_ptr),
		   inst->inst_size);
	rtapi_mutex_get(&(hal_data->mutex));

    }
#endif // RTAPI

    args.type = HAL_PIN;
    halg_foreach(0, &args, yield_free);  // free pins

    args.type = HAL_PARAM;
    halg_foreach(0, &args, yield_free);  // free params

//    args.type = HAL_PLUG;
//    halg_foreach(0, &args, yield_free);  // free plugs

    // free frozen argv if any
    if (inst->frozen_argv) {
	halg_free_argv(0, inst->frozen_argv);
    }
    // now we can delete the instance itself
    halg_free_object(false, (hal_object_ptr) inst);
}
