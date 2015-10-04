/*
 * zeroconf register interface / low level register/unregister
 * czmq reactor compatible
 *
 * Copyright Michael Haberler 2014-2015
 * License: Mozilla Public License Version 2.0
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>


#include "config.h"
#include "ll-zeroconf.hh"
#include "syslog_async.h"
#include "czmq.h"
#include "czmq-watch.h"

static void publish_reply(AvahiEntryGroup *g,
                          AvahiEntryGroupState state,
                          void *userdata);

static void register_stuff(register_context_t *rctx)
{

    const char *name = avahi_client_get_host_name_fqdn(rctx->client);
    syslog_async(LOG_DEBUG, "%s: actual hostname as announced by avahi='%s'", __FUNCTION__, name);

    if (!rctx->group) {
        if (!(rctx->group = avahi_entry_group_new(rctx->client,
						  publish_reply,
						  rctx))) {
            syslog_async(LOG_ERR,
			 "zeroconf: Failed to create avahi entry group: %s\n",
			 avahi_strerror(avahi_client_errno(rctx->client)));
            goto fail;
        }
    }
    if (avahi_entry_group_is_empty(rctx->group)) {
	// fill in the dsn= txt record interpolated with actual hostname
	// condtional with uri_fmt set since not needed for http/https
	if (rctx->service->uri_fmt)
	    rctx->service->txt = avahi_string_list_add_printf(rctx->service->txt,
							      rctx->service->uri_fmt, name);

	// Register our service
        if (avahi_entry_group_add_service_strlst(rctx->group,
						 rctx->service->interface,
						 rctx->service->proto,
						 (AvahiPublishFlags) 0,
						 rctx->name,
						 rctx->service->type,
						 NULL,
						 NULL,
						 rctx->service->port,
						 rctx->service->txt) < 0) {
            syslog_async(LOG_ERR,
			 "zeroconf: Failed to add service: %s\n",
			 avahi_strerror(avahi_client_errno(rctx->client)));
            goto fail;
        }
	AvahiStringList *i;
	for (i = rctx->service->subtypes; i; i = i->next) {
            if (avahi_entry_group_add_service_subtype(rctx->group,
						      rctx->service->interface,
						      rctx->service->proto,
						      (AvahiPublishFlags) 0,
						      rctx->name,
						      rctx->service->type,
						      NULL,
						      (char*) i->text) < 0) {
		syslog_async(LOG_ERR,
			     "zeroconf: Failed to add subtype '%s': %s\n",
			     i->text, avahi_strerror(avahi_client_errno(rctx->client)));
		goto fail;
            }
	}
        if (avahi_entry_group_commit(rctx->group) < 0) {
            syslog_async(LOG_ERR,
			 "zeroconf: Failed to commit entry group: %s\n",
			 avahi_strerror(avahi_client_errno(rctx->client)));
            goto fail;
        }
    }
    return;

 fail:
    // Stop the avahi client, if it's running.
    if (rctx->czmq_poll)
	avahi_czmq_poll_quit(rctx->czmq_poll);
    rctx->czmq_poll = NULL;
    avahi_free(rctx->name);
}

// Called when publishing of service data completes
static void publish_reply(AvahiEntryGroup *g,
                          AvahiEntryGroupState state,
			  void *userdata)
{
    register_context_t *rctx = (register_context_t *)userdata;
    switch (state) {
    case AVAHI_ENTRY_GROUP_COLLISION: {
	// Pick a new name for our service
	char *n = avahi_alternative_service_name(rctx->name);
	assert(n);
	avahi_free(rctx->name);
	rctx->name = n;
	syslog_async(LOG_INFO,
		     "zeroconf: collision - register alternative"
		     " service name: '%s'\n",
		     rctx->name);
	register_stuff(rctx);
	break;
    }
    case AVAHI_ENTRY_GROUP_FAILURE:
	syslog_async(LOG_ERR,
		     "zeroconf: Failed to register service '%s': %s (avahi running?)\n",
		     rctx->name,
		     avahi_strerror(avahi_client_errno(rctx->client)));
	if (rctx->czmq_poll)
	    avahi_czmq_poll_quit(rctx->czmq_poll);
	break;
    case AVAHI_ENTRY_GROUP_UNCOMMITED:
	break;
    case AVAHI_ENTRY_GROUP_REGISTERING:
	syslog_async(LOG_DEBUG, "zeroconf: registering: '%s'\n", rctx->name);
	break;
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
	{
	    char *txt = avahi_string_list_to_string(rctx->service->txt);
	    syslog_async(LOG_INFO,
			 "zeroconf: registered '%s' %s %d TXT %s\n",
			 rctx->name, rctx->service->type, rctx->service->port, txt);
	    avahi_free(txt);
	}
    }
}

static void client_callback(AvahiClient *client,
                            AvahiClientState state,
                            void *userdata)
{
    register_context_t *rctx = (register_context_t *)userdata;
    rctx->client = client;

    switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
	register_stuff(rctx);
	break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
	if (rctx->group)
	    avahi_entry_group_reset(rctx->group);
	break;

    case AVAHI_CLIENT_FAILURE:
	if (avahi_client_errno(client) == AVAHI_ERR_DISCONNECTED) {
	    int error;
	    avahi_client_free(rctx->client);
	    rctx->client = NULL;
	    rctx->group = NULL;
	    // Reconnect to the server
	    if (!(rctx->client = avahi_client_new(avahi_czmq_poll_get(rctx->czmq_poll),
						  AVAHI_CLIENT_NO_FAIL,
						  client_callback,
						  rctx,
						  &error))) {
		syslog_async(LOG_ERR,
			     "zeroconf: Failed to contact mDNS server: %s\n",
			     avahi_strerror(error));
		avahi_czmq_poll_quit(rctx->czmq_poll);
	    }
	} else {
	    syslog_async(LOG_ERR,"zeroconf: Client failure: %s\n",
			 avahi_strerror(avahi_client_errno(client)));
	    avahi_czmq_poll_quit(rctx->czmq_poll);
	}
	break;

    case AVAHI_CLIENT_CONNECTING:
	syslog_async(LOG_ERR, "zeroconf: connecting - mDNS server not yet available"
		     " (avahi-daemon not installed or not runnung?)\n");

	;
    }
}

// register a service in DNS-SD/mDNS
register_context_t *ll_zeroconf_register(zservice_t *s, AvahiCzmqPoll *av_loop)
{
    register_context_t *rctx = NULL;
    int error;

    assert(av_loop);
    rctx = (register_context_t *)malloc(sizeof(register_context_t));
    assert(rctx);
    rctx->client = NULL;
    rctx->group = NULL;
    rctx->service = s;
    assert(rctx->service->name);
    rctx->name = strdup(rctx->service->name); // might be renamed
    assert(rctx->name);
    rctx->czmq_poll = av_loop;

    if (!(rctx->client = avahi_client_new(avahi_czmq_poll_get(rctx->czmq_poll),
					  AVAHI_CLIENT_NO_FAIL,
					  client_callback,
					  rctx,
					  &error))) {
        syslog_async(LOG_ERR,
		     "zeroconf: Failed to create avahi client object: %s\n",
		     avahi_strerror(error));
        goto fail;
    }

    return rctx;

 fail:
    if (rctx)
        ll_zeroconf_unregister(rctx);
    return NULL;
}

// Unregister this server from DNS-SD/mDNS
int ll_zeroconf_unregister(register_context_t *rctx)
{
    if (rctx == NULL)
	return 0;

    syslog_async(LOG_INFO, "zeroconf: unregistering '%s'\n", rctx->name);
    if (rctx->client)
        avahi_client_free(rctx->client);

    return 0;
}
