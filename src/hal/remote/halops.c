
#include "config.h"

#include <rtapi.h>
#include <hal.h>
#include <hal_priv.h>
#include <czmq.h>

#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

int count_unbound_comps_cb(hal_compstate_t *cs,  void *cbdata)
{
    int *counter = cbdata;

    if ((cs->type == TYPE_REMOTE) &&
	(cs->state == COMP_UNBOUND)) {
	*counter += 1;
    }
    return 0;
}

int num_unbound_components()
{
    int counter = 0;

    hal_retrieve_compstate(NULL, count_unbound_comps_cb, (void *) &counter);
    return counter;
}

int num_pins(const char *comp_name)
{
    return hal_retrieve_pinstate(comp_name, NULL, NULL);
}
