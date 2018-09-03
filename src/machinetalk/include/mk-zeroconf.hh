/* zeroconf announce/withdraw tailored for machinekit purposes
 *
 * Copyright Michael Haberler 2013-2015
 * License: Mozilla Public License Version 2.0
 */

#ifndef _MK_ZEROCONF_HH
#define _MK_ZEROCONF_HH

#include <uuid/uuid.h>
#include <czmq.h>

#include "czmq-watch.h"
#include "ll-zeroconf.hh"
#include "mk-zeroconf-types.h"

#ifdef __cplusplus
extern "C" {
#endif

    register_context_t * zeroconf_service_announce(const char *name,
						   const char *type,
						   const char *subtype,      // may be NULL
						   int port,
						   char *dsn,                // may be NULL
						   const char *service_uuid, // must be valid
						   const char *process_uuid,
						   const char *tag,          // may be NULL
						   const char *path,         // for _http._tcp, else NULL

						   // protocol:
						   // AVAHI_PROTO_INET    - IPv4 only
						   // AVAHI_PROTO_INET6   - IPv6 only
						   // AVAHI_PROTO_UNSPEC  - IPv4 and IPv6
						   const int  protocol,
						   AvahiCzmqPoll *av_loop);

    int zeroconf_service_withdraw( register_context_t *publisher);

#ifdef __cplusplus
}
#endif

#endif
