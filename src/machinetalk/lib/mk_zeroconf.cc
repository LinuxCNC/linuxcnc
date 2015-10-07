/* zeroconf announce/withdraw tailored for machinekit purposes
 *
 * Copyright Michael Haberler 2014-2015
 * License: Mozilla Public License Version 2.0
 */
#include "mk-zeroconf.hh"



register_context_t *zeroconf_service_announce(const char *name,
					      const char *type,
					      const char *subtype,
					      int port,
					      char *dsn,
					      const char *service_uuid,
					      const char *process_uuid,
					      const char *tag,
					      const char *path,
					      const int protocol,
					      AvahiCzmqPoll *av_loop)
{
    zservice_t *zs = (zservice_t *) calloc(sizeof(zservice_t), 1);
    zs->name = name;
    zs->proto = protocol;
    zs->interface = AVAHI_IF_UNSPEC;
    zs->type = type;
    zs->port = port;
    zs->uri_fmt = dsn;
    zs->txt = avahi_string_list_add_printf(zs->txt, "uuid=%s", service_uuid);
    if (process_uuid)
	zs->txt = avahi_string_list_add_printf(zs->txt, "instance=%s", process_uuid);
    if (tag)
	zs->txt = avahi_string_list_add_printf(zs->txt, "service=%s", tag);
    if (subtype)
	zs->subtypes = avahi_string_list_add_printf(zs->subtypes,"%s%s", subtype, type);
    if (path)
	zs->txt = avahi_string_list_add_printf(zs->txt, "path=%s", path);
    return ll_zeroconf_register(zs, av_loop);
}

int zeroconf_service_withdraw(register_context_t *publisher)
{
    if (publisher == NULL)
	return -1;
    ll_zeroconf_unregister(publisher);

    zservice_t *s = publisher->service;
    if (s) {
	if (s->subtypes)
	    avahi_string_list_free(s->subtypes);
	if (s->txt)
	    avahi_string_list_free(s->txt);
	free(publisher->service);
    }

    free(publisher->name);
    free(publisher);
    return 0;
}
