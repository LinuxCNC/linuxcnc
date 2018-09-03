//   client resolve API for the rest of us
//   blocking API with timout, no czmq integration
//
//   Copyright Michael Haberler, 2014
//   License: Mozilla Public License Version 2.0
//

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <net/if.h>


#include "config.h"
#include "ll-zeroconf.hh"
#include "syslog_async.h"

#define DEFAUL_TIMEOUT 3000 // msec to resolution



static void resolve_callback(AvahiServiceResolver *r,
			     AvahiIfIndex interface,
			     AvahiProtocol protocol,
			     AvahiResolverEvent event,
			     const char *name,
			     const char *type,
			     const char *domain,
			     const char *host_name,
			     const AvahiAddress *address,
			     uint16_t port,
			     AvahiStringList *txt,
			     AvahiLookupResultFlags flags,
			     void* userdata)
{
    resolve_context_t *rctx = (resolve_context_t *)userdata;
    assert(r);

    // Called whenever a service has been resolved successfully or timed out
    switch (event) {

    case AVAHI_RESOLVER_FAILURE:
	syslog_async(LOG_ERR,
		     "%s: (Resolver) Failed to resolve service"
		     " '%s' of type '%s' in domain '%s': %s\n",
		     __func__, name, type, domain,
		     avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
	rctx->resolve->result = SD_RESOLVER_FAILURE;
	avahi_simple_poll_quit(rctx->simple_poll);
	break;

    case AVAHI_RESOLVER_FOUND:
	{
	    AvahiStringList *i;
	    for (i = txt; i; i = avahi_string_list_get_next(i)) {
		uint8_t *text = avahi_string_list_get_text(i);
		if ((rctx->resolve->match == NULL) || // match any
		    (strcasecmp((const char *)text,
				rctx->resolve->match) == 0)) {

		    rctx->resolve->name = strdup(name);
		    rctx->resolve->type = strdup(type);
		    rctx->resolve->domain = strdup(domain);
		    rctx->resolve->host_name = strdup(host_name);
		    rctx->resolve->address = *address;
		    rctx->resolve->interface = interface;
		    rctx->resolve->proto = protocol;
		    rctx->resolve->port = port;
		    rctx->resolve->flags = flags;
		    rctx->resolve->txt = avahi_string_list_copy(txt);
		    rctx->resolve->result = SD_OK;

		    char a[AVAHI_ADDRESS_STR_MAX], *t;
		    avahi_address_snprint(a, sizeof(a), address);
		    t = avahi_string_list_to_string(txt);
		    syslog_async(LOG_INFO,
				 "%s:  Service matched '%s' of type '%s' in domain '%s' %s:%u %s TXT=%s\n",
				 __func__, name, type, domain,  host_name, port, a,  t);
		    avahi_free(t);
		    avahi_simple_poll_quit(rctx->simple_poll);
		    return;
		}
	    }
	    syslog_async(LOG_INFO,
			 "%s: unmatched Service '%s' of type '%s' in domain '%s' %s:%u\n",
			 __func__, name, type, domain,  host_name, port);
	}
    }
}

static void browse_callback(AvahiServiceBrowser *b,
			    AvahiIfIndex interface,
			    AvahiProtocol protocol,
			    AvahiBrowserEvent event,
			    const char *name,
			    const char *type,
			    const char *domain,
			    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
			    void* userdata)
{
    resolve_context_t *rctx = (resolve_context_t *)userdata;
    AvahiClient *c = rctx->client;
    assert(b);
    char ifname[IF_NAMESIZE];

    // Called whenever a new services becomes available on the LAN or is removed from the LAN
    switch (event) {
    case AVAHI_BROWSER_FAILURE:
	syslog_async(LOG_ERR,"%s: (Browser) %s\n",
		     __func__,
		     avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
	avahi_simple_poll_quit(rctx->simple_poll);
	return;

    case AVAHI_BROWSER_NEW:
	syslog_async(LOG_DEBUG,
		     "%s: (Browser) NEW: service '%s' of type '%s' in domain '%s' ifindex=%d protocol=%d\n",
		     __func__,name, type, domain, interface, protocol);
	rctx->resolver = avahi_service_resolver_new(c,
						    interface,
						    protocol,
						    name,
						    type,
						    domain,
						    rctx->resolve->proto,
						    (AvahiLookupFlags)0,
						    resolve_callback, rctx);
	if (rctx->resolver == NULL)
	    syslog_async(LOG_ERR,"%s: Failed to start resolver for '%s': %s\n",
			 __func__, name, avahi_strerror(avahi_client_errno(c)));
	break;

    case AVAHI_BROWSER_REMOVE:
	memset(ifname, 0, sizeof(ifname));
	if_indextoname(interface, ifname);
	syslog_async(LOG_DEBUG,"%s: (Browser) REMOVE: service '%s' of type '%s' in domain '%s' if=%s\n",
		     __func__, name, type, domain, ifname);
	break;

    // case AVAHI_BROWSER_ALL_FOR_NOW:
    // case AVAHI_BROWSER_CACHE_EXHAUSTED:
    // 	syslog_async(LOG_DEBUG,"%s: (Browser) %s\n",
    // 		     __func__, event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
    //
    default:
	break;
    }
}

static void client_callback(AvahiClient *c, AvahiClientState state,
			    void * userdata)
{
    resolve_context_t *rctx = (resolve_context_t *)userdata;
    assert(c);

    // Called whenever the client or server state changes
    if (state == AVAHI_CLIENT_FAILURE) {
	syslog_async(LOG_ERR,"%s: Server connection failure: %s\n",
	       __func__, avahi_strerror(avahi_client_errno(c)));
	rctx->resolve->result = SD_CLIENT_FAILURE;
        avahi_simple_poll_quit(rctx->simple_poll);
    }
}

static void resolve_timeout(AvahiTimeout *timeout, void* userdata)
{
    resolve_context_t *rctx = (resolve_context_t *)userdata;
    syslog_async(LOG_ERR,"%s: Timeout resolving service type '%s'\n",
			 __func__, rctx->resolve->type);
    rctx->resolve->result = SD_TIMEOUT;
    avahi_simple_poll_quit(rctx->simple_poll);
}

resolve_context_t *ll_zeroconf_resolve(zresolve_t *res)
{
    int error;

    resolve_context_t *rctx = (resolve_context_t *)calloc(sizeof(resolve_context_t),1);
    assert(rctx);
    rctx->client = NULL;
    rctx->resolver = NULL;
    rctx->browser = NULL;
    rctx->resolve = res;
    rctx->resolve->result = SD_UNSET;

    rctx->simple_poll =  avahi_simple_poll_new();
    assert(rctx->simple_poll);

    const AvahiPoll *poll_api = avahi_simple_poll_get(rctx->simple_poll);
    struct timeval tv;
    avahi_elapse_time(&tv, res->timeout_ms ? res->timeout_ms : DEFAUL_TIMEOUT , 0);
    poll_api->timeout_new(poll_api, &tv, resolve_timeout, rctx);

    rctx->client = avahi_client_new(avahi_simple_poll_get(rctx->simple_poll),
				    (AvahiClientFlags)0,
				    client_callback,
				    rctx,
				    &error);
    if (!rctx->client) {
	syslog_async(LOG_ERR,"%s: Failed to create client: '%s'\n",
			 __func__, avahi_strerror(error));
        goto fail;
    }
    if (!(rctx->browser = avahi_service_browser_new(rctx->client,
						    rctx->resolve->interface,
						    rctx->resolve->proto,
						    rctx->resolve->type,
						    rctx->resolve->domain,
						    (AvahiLookupFlags)0,
						    browse_callback,
						    rctx))) {
	syslog_async(LOG_ERR,"%s: Failed to create service browser: %s\n",
		     __func__, avahi_strerror(avahi_client_errno(rctx->client)));
        goto fail;
    }
    avahi_simple_poll_loop(rctx->simple_poll);
    return rctx;

fail:
    if (rctx->client)
        avahi_client_free(rctx->client);

    if (rctx->simple_poll)
        avahi_simple_poll_free(rctx->simple_poll);

    return NULL;

}

int ll_zeroconf_resolve_free(resolve_context_t *rctx)
{
    if (rctx == NULL)
	return -1;

    if (rctx->resolve->txt)
	avahi_string_list_free(rctx->resolve->txt);

    if (rctx->resolve->name) free(rctx->resolve->name);
    if (rctx->resolve->type) free(rctx->resolve->type);
    if (rctx->resolve->domain) free(rctx->resolve->domain);
    if (rctx->resolve->host_name) free(rctx->resolve->host_name);


    // http://lists.freedesktop.org/archives/avahi/2007-December/001232.html
    // avahi_client_free() frees all attached objects (browser, resolver)
    if (rctx->client)
        avahi_client_free(rctx->client);

    if (rctx->simple_poll)
        avahi_simple_poll_free(rctx->simple_poll);

    return 0;
}
