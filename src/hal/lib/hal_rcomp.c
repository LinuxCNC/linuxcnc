
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
#include "rtapi/shmdrv/shmdrv.h"

int hal_bind(const char *comp_name)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	hal_comp_t *comp __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));
	comp = halpr_find_comp_by_name(comp_name);

	if (comp == NULL) {
	    HALERR("no such component '%s'", comp_name);
	    return -EINVAL;
	}
	if (comp->type != TYPE_REMOTE) {
	    HALERR("component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	    return -EINVAL;
	}
	if (comp->state != COMP_UNBOUND) {
	    HALERR("component '%s': state not unbound (%d)",
		   comp_name, comp->state);
	    return -EINVAL;
	}
	comp->state = COMP_BOUND;
	comp->last_bound = (long int) time(NULL);
    }
    return 0;
}

int hal_unbind(const char *comp_name)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	hal_comp_t *comp __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));

	comp = halpr_find_comp_by_name(comp_name);
	if (comp == NULL) {
	    HALERR("no such component '%s'", comp_name);
	    return -EINVAL;
	}
	if (comp->type != TYPE_REMOTE) {
	    HALERR("component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	    return -EINVAL;
	}
	if (comp->state != COMP_BOUND) {
	    HALERR("component '%s': state not bound (%d)",
		   comp_name, comp->state);
	    return -EINVAL;
	}
	comp->state = COMP_UNBOUND;
	comp->last_unbound = (long int) time(NULL);
    }
    return 0;
}

int hal_acquire(const char *comp_name, int pid)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	hal_comp_t *comp __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));
	comp = halpr_find_comp_by_name(comp_name);

	if (comp == NULL) {
	    HALERR("no such component '%s'", comp_name);
	    return -EINVAL;
	}
	if (comp->type != TYPE_REMOTE) {
	    HALERR("component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	    return -EINVAL;
	}
	if (comp->state == COMP_BOUND) {
	    HALERR("component '%s': cant reown a bound component (%d)",
		   comp_name, comp->state);
	    return -EINVAL;
	}
	// let a comp be 'adopted away' from the RT environment
	// this is a tad hacky, should separate owner pid from RT/user distinction
	if ((comp->pid !=0) &&
	    (comp->pid != global_data->rtapi_app_pid))

	    {
		HALERR("component '%s': already owned by pid %d",
		       comp_name, comp->pid);
		return -EINVAL;
	    }
	comp->pid = pid;
	return comp->comp_id;
    }
}

int hal_release(const char *comp_name)
{
    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);
    {
	hal_comp_t *comp __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));
	comp = halpr_find_comp_by_name(comp_name);

	if (comp == NULL) {
	    HALERR("no such component '%s'", comp_name);
	    return -EINVAL;
	}
	if (comp->type != TYPE_REMOTE) {
	    HALERR("component '%s' not a remote component (%d)",
		   comp_name, comp->type);
	    return -EINVAL;
	}
	if (comp->pid == 0) {
	    HALERR("component '%s': component already disowned",
			    comp_name);
	    return -EINVAL;
	}

	if (comp->pid != getpid()) {
	    HALERR("component '%s': component owned by pid %d",
			    comp_name, comp->pid);
	    // return -EINVAL;
	}
	comp->pid = 0;
    }
    return 0;
}

// introspection support for remote components.

int hal_retrieve_compstate(const char *comp_name,
			   hal_retrieve_compstate_callback_t callback,
			   void *cb_data)
{
    int next;
    int nvisited = 0;
    int result;
    hal_compstate_t state;

    CHECK_HALDATA();
    {
	hal_comp_t *comp  __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* search for the comp */
	next = hal_data->comp_list_ptr;
	while (next != 0) {
	    comp = SHMPTR(next);
	    if (!comp_name || (strcmp(comp->name, comp_name)) == 0) {
		nvisited++;
		/* this is the right comp */
		if (callback) {
		    // fill in the details:
		    state.type = comp->type;
		    state.state = comp->state;
		    state.last_update = comp->last_update;
		    state.last_bound = comp->last_bound;
		    state.last_unbound = comp->last_unbound;
		    state.pid = comp->pid;
		    state.insmod_args = comp->insmod_args;
		    strncpy(state.name, comp->name, sizeof(comp->name));

		    result = callback(&state, cb_data);
		    if (result < 0) {
			// callback signaled an error, pass that back up.
			return result;
		    } else if (result > 0) {
			// callback signaled 'stop iterating'.
			// pass back the number of visited comps so far.
			return nvisited;
		    } else {
			// callback signaled 'OK to continue'
			// fall through
		    }
		} else {
		    // null callback passed in,
		    // just count comps
		    // nvisited already bumped above.
		}
	    }
	    /* no match, try the next one */
	    next = comp->next_ptr;
	}
	// hal_print_msg(RTAPI_MSG_DBG,
	//		"HAL: hal_retrieve_compstate: visited %d comps", nvisited);
	/* if we get here, we ran through all the comps, so return count */
	return nvisited;
    }
}

int hal_retrieve_pinstate(const char *comp_name,
			  hal_retrieve_pins_callback_t callback,
			  void *cb_data)
{
    int next;
    int nvisited = 0;
    int result;
    hal_comp_t *comp = NULL;
    hal_comp_t *owner;
    hal_pinstate_t pinstate;

    CHECK_HALDATA();
    CHECK_STRLEN(comp_name, HAL_NAME_LEN);

    {
	hal_pin_t *pin __attribute__((cleanup(halpr_autorelease_mutex)));

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	if (comp_name != NULL) {
	    comp = halpr_find_comp_by_name(comp_name);
	    if (comp == NULL) {
		HALERR("no such component '%s'", comp_name);
		return -EINVAL;
	    }
	}
	// either comp == NULL, so visit all pins
	// or comp != NULL, in which case visit only this
	// component's pins

	// walk the pinlist
	next = hal_data->pin_list_ptr;
	while (next != 0) {
	    pin = SHMPTR(next);
	    owner = halpr_find_owning_comp(pin->owner_id);
	    if (!comp_name || (owner->comp_id == comp->comp_id)) {
		nvisited++;
		/* this is the right comp */
		if (callback) {
		    // fill in the details:
		    // NB: cover remote link case!
		    pinstate.value = SHMPTR(pin->data_ptr_addr);
		    pinstate.type = pin->type;
		    pinstate.dir = pin->dir;
		    pinstate.epsilon = hal_data->epsilon[pin->eps_index];
		    pinstate.flags = pin->flags;
		    strncpy(pinstate.name, pin->name, sizeof(pin->name));
		    strncpy(pinstate.owner_name, owner->name, sizeof(owner->name));

		    result = callback(&pinstate, cb_data);
		    if (result < 0) {
			// callback signaled an error, pass that back up.
			return result;
		    } else if (result > 0) {
			// callback signaled 'stop iterating'.
			// pass back the number of visited pins so far.
			return nvisited;
		    } else {
			// callback signaled 'OK to continue'
			// fall through
		    }
		} else {
		    // null callback passed in,
		    // just count pins
		    // nvisited already bumped above.
		}
	    }
	    /* no match, try the next one */
	    next = pin->next_ptr;
	}
	HALDBG("hal_retrieve_pinstate: visited %d pins", nvisited);
	/* if we get here, we ran through all the pins, so return count */
	return nvisited;
    }
}

// component reporting support

int hal_compile_comp(const char *name, hal_compiled_comp_t **ccomp)
{
   hal_compiled_comp_t *tc;
   int pincount = 0;

   CHECK_HALDATA();
   CHECK_STRLEN(name, HAL_NAME_LEN);
   {
       hal_comp_t *comp __attribute__((cleanup(halpr_autorelease_mutex)));
       int next, n;
       hal_comp_t *owner;
       hal_pin_t *pin;

       rtapi_mutex_get(&(hal_data->mutex));

       if ((comp = halpr_find_comp_by_name(name)) == NULL) {
	    HALERR("no such component '%s'", name);
	   return -EINVAL;
       }

       // array sizing: count pins owned by this component
       next = hal_data->pin_list_ptr;
       n = 0;
       while (next != 0) {
	    pin = SHMPTR(next);
	    owner = halpr_find_owning_comp(pin->owner_id);
	    if (owner->comp_id == comp->comp_id) {
		if (!(pin->flags & PIN_DO_NOT_TRACK))
		    n++;
		pincount++;
	    }
	    next = pin->next_ptr;
       }
       if (n == 0) {
	   HALERR("component %s has no pins to watch for changes",
		  name);
	   return -EINVAL;
       }
       // a compiled comp is a userland/per process memory object
       if ((tc = malloc(sizeof(hal_compiled_comp_t))) == NULL)
	   return -ENOMEM;

       memset(tc, 0, sizeof(hal_compiled_comp_t));
       tc->comp = comp;
       tc->n_pins = n;

       // alloc pin array
       if ((tc->pin = malloc(sizeof(hal_pin_t *) * tc->n_pins)) == NULL)
	   return -ENOMEM;
       // alloc tracking value array
       if ((tc->tracking =
	    malloc(sizeof(hal_data_u) * tc->n_pins )) == NULL)
	   return -ENOMEM;
       // alloc change bitmap
       if ((tc->changed =
	    malloc(RTAPI_BITMAP_BYTES(tc->n_pins))) == NULL)
	    return -ENOMEM;

       memset(tc->pin, 0, sizeof(hal_pin_t *) * tc->n_pins);
       memset(tc->tracking, 0, sizeof(hal_data_u) * tc->n_pins);
       RTAPI_ZERO_BITMAP(tc->changed,tc->n_pins);

       // fill in pin array
       n = 0;
       next = hal_data->pin_list_ptr;
       while (next != 0) {
	   pin = SHMPTR(next);
	   owner = halpr_find_owning_comp(pin->owner_id);
	   if ((owner->comp_id == comp->comp_id) &&
	       !(pin->flags & PIN_DO_NOT_TRACK))
	       tc->pin[n++] = pin;
	   next = pin->next_ptr;
       }
       assert(n == tc->n_pins);
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
    hal_pin_t *pin;
    hal_sig_t *sig;
    void *data_ptr;

    assert(cc->magic ==  CCOMP_MAGIC);
    RTAPI_ZERO_BITMAP(cc->changed, cc->n_pins);

    for (i = 0; i < cc->n_pins; i++) {
	pin = cc->pin[i];
	if (pin->signal != 0) {
	    sig = SHMPTR(pin->signal);
	    data_ptr = SHMPTR(sig->data_ptr);
	} else {
	    data_ptr = hal_shmem_base + SHMOFF(&(pin->dummysig));
	}

	switch (pin->type) {
	case HAL_BIT:
	    halbit = *((char *) data_ptr);
	    if (cc->tracking[i].b != halbit) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		cc->tracking[i].b = halbit;
	    }
	    break;
	case HAL_FLOAT:
	    halfloat = *((hal_float_t *) data_ptr);
	    delta = HAL_FABS(halfloat - cc->tracking[i].f);
	    if (delta > hal_data->epsilon[pin->eps_index]) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		cc->tracking[i].f = halfloat;
	    }
	    break;
	case HAL_S32:
	    hals32 =  *((hal_s32_t *) data_ptr);
	    if (cc->tracking[i].s != hals32) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		cc->tracking[i].s = hals32;
	    }
	    break;
	case HAL_U32:
	    halu32 =  *((hal_u32_t *) data_ptr);
	    if (cc->tracking[i].u != halu32) {
		nchanged++;
		RTAPI_BIT_SET(cc->changed, i);
		cc->tracking[i].u = halu32;
	    }
	    break;
	default:
	    HALERR("BUG: hal_ccomp_match(%s): invalid type for pin %s: %d",
		   cc->comp->name, pin->name, pin->type);
	    return -EINVAL;
	}
    }
    return nchanged;
}

int hal_ccomp_report(hal_compiled_comp_t *cc,
		     comp_report_callback_t report_cb,
		     void *cb_data, int report_all)
{
    int retval, i;
    hal_data_u *data_ptr;
    hal_pin_t *pin;
    hal_sig_t *sig;

    if (!report_cb)
	return 0;
    if ((retval = report_cb(REPORT_BEGIN, cc, NULL, NULL, cb_data)) < 0)
	return retval;

    for (i = 0; i < cc->n_pins; i++) {
	if (report_all || RTAPI_BIT_TEST(cc->changed, i)) {
	    pin = cc->pin[i];
	    if (pin->signal != 0) {
		sig = SHMPTR(pin->signal);
		data_ptr = (hal_data_u *)SHMPTR(sig->data_ptr);
	    } else {
		data_ptr = (hal_data_u *)(hal_shmem_base + SHMOFF(&(pin->dummysig)));
	    }
	    if ((retval = report_cb(REPORT_PIN, cc, pin,
				    data_ptr, cb_data)) < 0)
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
