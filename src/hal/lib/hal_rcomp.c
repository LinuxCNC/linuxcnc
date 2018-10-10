
#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_rcomp.h"		/* HAL remote component decls */
#include "hal_group.h"		/* common defs - REPORT_* */



#if defined(ULAPI)
#include <stdio.h>
#include <sys/types.h>		/* pid_t */
#include <unistd.h>		/* getpid() */
#include <assert.h>
#include <time.h>               /* remote comp bind/unbind/update timestamps */
#include <limits.h>             /* PATH_MAX */
#include <stdlib.h>		/* exit() */
#include "shmdrv.h"

int halg_bind(const int use_hal_mutex, const char *comp_name)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_comp_t *comp;
	comp = halpr_find_comp_by_name(comp_name);

	if (comp == NULL) {
	    HALFAIL_RC(EINVAL, "no such component '%s'", comp_name);
	}
	if (comp->type != TYPE_REMOTE) {
	    HALFAIL_RC(EINVAL, "component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	}
	if (comp->state != COMP_UNBOUND) {
	    HALFAIL_RC(EINVAL, "component '%s': state not unbound (%d)",
		   comp_name, comp->state);
	}
	comp->state = COMP_BOUND;
	comp->last_bound = (long int) time(NULL);
    }
    return 0;
}

int halg_unbind(const int use_hal_mutex, const char *comp_name)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_comp_t *comp;

	comp = halpr_find_comp_by_name(comp_name);
	if (comp == NULL) {
	    HALFAIL_RC(EINVAL, "no such component '%s'", comp_name);
	}
	if (comp->type != TYPE_REMOTE) {
	    HALFAIL_RC(EINVAL, "component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	}
	if (comp->state != COMP_BOUND) {
	    HALFAIL_RC(EINVAL, "component '%s': state not bound (%d)",
		   comp_name, comp->state);
	}
	comp->state = COMP_UNBOUND;
	comp->last_unbound = (long int) time(NULL);
    }
    return 0;
}

int halg_acquire(const int use_hal_mutex, const char *comp_name, int pid)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_comp_t *comp;

	comp = halpr_find_comp_by_name(comp_name);
	if (comp == NULL) {
	    HALFAIL_RC(EINVAL, "no such component '%s'", comp_name);
	}
	if (comp->type != TYPE_REMOTE) {
	    HALFAIL_RC(EINVAL, "component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	}
	if (comp->state == COMP_BOUND) {
	    HALFAIL_RC(EINVAL, "component '%s': cant reown a bound component (%d)",
		   comp_name, comp->state);
	}
	// let a comp be 'adopted away' from the RT environment
	// this is a tad hacky, should separate owner pid from RT/user distinction
	if ((comp->pid !=0) &&
	    (comp->pid != global_data->rtapi_app_pid))

	    {
		HALFAIL_RC(EINVAL, "component '%s': already owned by pid %d",
		       comp_name, comp->pid);
	    }
	comp->pid = pid;
	ho_incref(comp); // prevent deletion
	return ho_id(comp);
    }
}

int halg_release(const int use_hal_mutex, const char *comp_name)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	hal_comp_t *comp;

	comp = halpr_find_comp_by_name(comp_name);
	if (comp == NULL) {
	    HALFAIL_RC(EINVAL, "no such component '%s'", comp_name);
	}
	if (comp->type != TYPE_REMOTE) {
	    HALFAIL_RC(EINVAL, "component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	}
	if (comp->pid == 0) {
	    HALFAIL_RC(EINVAL, "component '%s': component already disowned",
			    comp_name);
	}

	if (comp->pid != getpid()) {
	    HALFAIL_RC(EINVAL, "component '%s': component owned by pid %d",
			    comp_name, comp->pid);
	}
	comp->pid = 0;
	ho_decref(comp); // enable deletion if last ref
    }
    return 0;
}

static int count_pins_and_tracked_pins(hal_object_ptr o, foreach_args_t *args)
{
    args->user_arg1++;     // pin count
    if (!(o.pin->flags & PIN_DO_NOT_TRACK))
	args->user_arg2++; // pins tracked
    return 0;
}

static int fill_pin_array(hal_object_ptr o, foreach_args_t *args)
{
    hal_compiled_comp_t *tc = args->user_ptr1;
    if (!(o.pin->flags & PIN_DO_NOT_TRACK))
	tc->pin[args->user_arg1++] = o.pin;
    return 0;
}

// component reporting support
int halg_compile_comp(const int use_hal_mutex,
		      const char *name,
		      hal_compiled_comp_t **ccomp)
{
   hal_compiled_comp_t *tc;
   int pincount = 0;

   CHECK_HALDATA();
   CHECK_STRLEN(name, HAL_NAME_LEN);
   {
       WITH_HAL_MUTEX_IF(use_hal_mutex);

       hal_comp_t *comp;
       int n;

       if ((comp = halpr_find_comp_by_name(name)) == NULL) {
	   HALFAIL_RC(EINVAL, "no such component '%s'", name);
       }

       // array sizing: count pins owned by this component
       // and how many of them are to be tracked
       foreach_args_t args =  {
	   .type = HAL_PIN,
	   // technically rcomps are legacy (not instantiable)
	   // so match on owner_id instead of owning_comp
	   // since all pins directly owned by comp, not an inst
	   .owner_id = ho_id(comp),
       };
       halg_foreach(0, &args, count_pins_and_tracked_pins);
       pincount = args.user_arg1;
       n = args.user_arg2;

       if (n == 0) {
	   HALFAIL_RC(EINVAL, "component %s has no pins to watch for changes",
		  name);
       }
       // a compiled comp is a userland/per process memory object
       if ((tc = malloc(sizeof(hal_compiled_comp_t))) == NULL)
	   NOMEM("allocating a compiled comp struct");

       memset(tc, 0, sizeof(hal_compiled_comp_t));
       tc->comp = comp;
       tc->n_pins = n;

       // alloc pin array
       if ((tc->pin = malloc(sizeof(hal_pin_t *) * tc->n_pins)) == NULL)
	   NOMEM("allocating a pin reference array");
       // alloc tracking value array
       if ((tc->tracking =
	    malloc(sizeof(hal_data_u) * tc->n_pins )) == NULL)
	   NOMEM("allocating a array of tracking values");
       // alloc change bitmap
       if ((tc->changed =
	    malloc(RTAPI_BITMAP_BYTES(tc->n_pins))) == NULL)
	   NOMEM("allocating change bitmap");

       memset(tc->pin, 0, sizeof(hal_pin_t *) * tc->n_pins);
       memset(tc->tracking, 0, sizeof(hal_data_u) * tc->n_pins);
       RTAPI_ZERO_BITMAP(tc->changed,tc->n_pins);

       // fill in pin array

       // reuse args from above and pass tc
       args.user_ptr1 = tc;
       args.user_arg1 = 0; // pin index in cc
       halg_foreach(0, &args, fill_pin_array);

       assert(args.user_arg1 == tc->n_pins);
       tc->magic = CCOMP_MAGIC;
       *ccomp = tc;
   }
   HALDBG("ccomp '%s': %d pins, %d tracked", name, pincount, tc->n_pins);
   return 0;
}

int hal_ccomp_match(hal_compiled_comp_t *cc)
{
    int i, nchanged = 0;
    hal_bit_t halbit;
    hal_s32_t hals32;
    hal_u32_t halu32;
    hal_float_t halfloat,delta;
    const hal_pin_t *hp;

    assert(cc->magic ==  CCOMP_MAGIC);
    RTAPI_ZERO_BITMAP(cc->changed, cc->n_pins);

    for (i = 0; i < cc->n_pins; i++) {
	hp = cc->pin[i];

	switch (pin_type(hp)) {
	case HAL_BIT:
	    halbit = _get_bit_pin(hp);
	    if (get_bit_value(&cc->tracking[i]) != halbit) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		set_bit_value(&cc->tracking[i], halbit);
	    }
	    break;

	case HAL_FLOAT:
	    halfloat = _get_float_pin(hp);
	    delta = HAL_FABS(halfloat - get_float_value(&cc->tracking[i]));
	    if (delta > hal_data->epsilon[hp->eps_index]) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		set_float_value(&cc->tracking[i], halfloat);
	    }
	    break;
	case HAL_S32:
	    hals32 = _get_s32_pin(hp);
	    if (get_s32_value(&cc->tracking[i]) != hals32) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		set_s32_value(&cc->tracking[i],hals32);
	    }
	    break;
	case HAL_U32:
	    halu32 = _get_u32_pin(hp);
	    if (get_u32_value(&cc->tracking[i]) != halu32) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		set_u32_value(&cc->tracking[i],halu32);
	    }
	    break;

	default:
	    HALFAIL_RC(EINVAL, "BUG: hal_ccomp_match(%s): invalid type for pin %s: %d",
		   ho_name(cc->comp), ho_name(hp), pin_type(hp));
	}
    }
    return nchanged;
}

int hal_ccomp_report(hal_compiled_comp_t *cc,
		     comp_report_callback_t report_cb,
		     void *cb_data, int report_all)
{
    int retval, i;

    if (!report_cb)
	return 0;
    if ((retval = report_cb(REPORT_BEGIN, cc, NULL, NULL, cb_data)) < 0)
	return retval;

    for (i = 0; i < cc->n_pins; i++) {
	if (report_all || RTAPI_BIT_TEST(cc->changed, i)) {
	    hal_pin_t *pin = cc->pin[i];
	    // XXX this is not a good API
	    // drop the fourth argument and pass only the pin
	    // to force accessor use in the report callback
	    if ((retval = report_cb(REPORT_PIN, cc, pin,
				    pin_value(pin), cb_data)) < 0)
		return retval;
	}
    }
    return report_cb(REPORT_END, cc, NULL, NULL, cb_data);
}

int hal_ccomp_free(hal_compiled_comp_t *cc)
{
    if (cc == NULL)
	return 0;
    assert(cc->magic ==  CCOMP_MAGIC);
    if (cc->tracking)
	free(cc->tracking);
    if (cc->changed)
	free(cc->changed);
    if (cc->pin)
	free(cc->pin);
    free(cc);
    return 0;
}

int hal_ccomp_args(hal_compiled_comp_t *cc, int *arg1, int *arg2)
{
    if (cc == NULL)
	return 0;
    assert(cc->magic ==  CCOMP_MAGIC);
    assert(cc->comp != NULL);
    if (arg1) *arg1 = cc->comp->userarg1;
    if (arg2) *arg2 = cc->comp->userarg2;
    return 0;
}
#endif
