/* avahi event loopt adapter for czmq
 *
 * Copyright Michael Haberler 2013-2015
 * License: Mozilla Public License Version 2.0
 */
#ifndef _czmq_watch_h
#define _czmq_watch_h

#include <czmq.h>

#include <avahi-common/cdecl.h>
#include <avahi-common/watch.h>

AVAHI_C_DECL_BEGIN

/** zloop adapter. */
typedef struct AvahiCzmqPoll AvahiCzmqPoll;

/** Create a new zloop main loop adapter attached to the specified
 context. */
AvahiCzmqPoll *avahi_czmq_poll_new(zloop_t *loop);

/** Free  zloop adapter */
void avahi_czmq_poll_free(AvahiCzmqPoll *g);

/** Return the abstract poll API structure for this object. This will
 * return the same pointer to a internally allocated structure on each
 * call */
const AvahiPoll *avahi_czmq_poll_get(AvahiCzmqPoll *g);

void avahi_czmq_poll_quit(AvahiCzmqPoll *g);

AVAHI_C_DECL_END

#endif
