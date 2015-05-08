/*
 * Copyright (C) 2013-2014 Michael Haberler <license@mah.priv.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webtalk.hh"

// zeroconf-register webtalk service (http/https)
int
wt_zeroconf_announce(wtself_t *self)
{
    char name[255];
    char dsn[255];
    char puuid[40];
    char hostname[PATH_MAX];

    uuid_unparse(self->process_uuid, puuid);

    if (gethostname(hostname, sizeof(hostname)) < 0) {
	syslog_async(LOG_ERR, "%s: gethostname() failed ?! %s\n",
		     self->cfg->progname, strerror(errno));
	return -1;
    }
    strtok(hostname, "."); // get rid of the domain name
    
    snprintf(name,sizeof(name), "Machinekit on %s.local", hostname);
    snprintf(dsn, sizeof(dsn), "http%s://%s.local.:%d%s",
	     self->cfg->use_ssl ? "s" : "",
	     hostname,
	     self->cfg->info.port,
	     self->cfg->index_html);

    self->www_publisher = zeroconf_service_announce(name,
						    self->cfg->use_ssl ?
						    "_https._tcp" :
						    "_http._tcp",
						    NULL,
						    self->cfg->info.port,
						    dsn,
						    self->cfg->service_uuid,
						    puuid,
						    self->cfg->use_ssl ?
						    "https" :
						    "http",
						    self->cfg->index_html,
						    self->av_loop);
    if (self->www_publisher == NULL) {
	syslog_async(LOG_ERR, "%s: failed to start HTTP zeroconf publisher\n",
		     self->cfg->progname);
	return -1;
    }
    return 0;
}

int
wt_zeroconf_withdraw(wtself_t *self)
{
    if (self->www_publisher)
	zeroconf_service_withdraw(self->www_publisher);

    // deregister poll adapter
    if (self->av_loop)
        avahi_czmq_poll_free(self->av_loop);
    return 0;
}
