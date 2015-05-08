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

#include "haltalk.hh"

// zeroconf-register haltalk services
int
ht_zeroconf_announce(htself_t *self)
{
    char name[LINELEN];
    char hostname[PATH_MAX];
    char uri[PATH_MAX];
    char puuid[40];
    uuid_unparse(self->process_uuid, puuid);

    if (gethostname(hostname, sizeof(hostname)) < 0) {
	syslog_async(LOG_ERR, "%s: gethostname() failed ?! %s\n",
		     self->cfg->progname, strerror(errno));
	return -1;
    }

    // use mDNS addressing if running over TCP:
    // construct a URI of the form 'tcp://<hostname>.local.:<portnumber>'
    strtok(hostname, "."); // get rid of the domain name

    if (self->cfg->remote)
	snprintf(uri,sizeof(uri), "tcp://%s.local.:%d",hostname, self->z_group_port);

    snprintf(name,sizeof(name), "HAL Group service on %s.local pid %d", hostname, getpid());
    self->halgroup_publisher = zeroconf_service_announce(name,
							 MACHINEKIT_DNSSD_SERVICE_TYPE,
							 HALGROUP_DNSSD_SUBTYPE,
							 self->z_group_port,
							 self->cfg->remote ? uri :
							 (char *)self->z_halgroup_dsn,
							 self->cfg->service_uuid,
							 puuid,
							 "halgroup", NULL,
							 self->av_loop);
    if (self->halgroup_publisher == NULL) {
	syslog_async(LOG_ERR, "%s: failed to start zeroconf HAL Group publisher\n",
		     self->cfg->progname);
	return -1;
    }

    snprintf(name,sizeof(name), "HAL Rcomp service on %s.local pid %d", hostname, getpid());

    if (self->cfg->remote)
	snprintf(uri,sizeof(uri), "tcp://%s.local.:%d",hostname, self->z_rcomp_port);

    self->halrcomp_publisher = zeroconf_service_announce(name,
							 MACHINEKIT_DNSSD_SERVICE_TYPE,
							 HALRCOMP_DNSSD_SUBTYPE,
							 self->z_rcomp_port,
							 self->cfg->remote ? uri :
							 (char *)self->z_halrcomp_dsn,
							 self->cfg->service_uuid,
							 puuid,
							 "halrcomp", NULL,
							 self->av_loop);
    if (self->halrcomp_publisher == NULL) {
	syslog_async(LOG_ERR, "%s: failed to start zeroconf HAL Rcomp publisher\n",
		     self->cfg->progname);
	return -1;
    }

    snprintf(name,sizeof(name),  "HAL Rcommand service on %s.local pid %d", hostname, getpid());

    if (self->cfg->remote)
	snprintf(uri,sizeof(uri), "tcp://%s.local.:%d",hostname, self->z_halrcmd_port);

    self->halrcmd_publisher = zeroconf_service_announce(name,
							MACHINEKIT_DNSSD_SERVICE_TYPE,
							HALRCMD_DNSSD_SUBTYPE,
							self->z_halrcmd_port,
							self->cfg->remote ? uri :
							(char *)self->z_halrcmd_dsn,
							self->cfg->service_uuid,
							puuid,
							"halrcmd", NULL,
							self->av_loop);
    if (self->halrcmd_publisher == NULL) {
	syslog_async(LOG_ERR, "%s: failed to start zeroconf HAL Rcommand publisher\n",
		     self->cfg->progname);
	return -1;
    }

    return 0;
}

int
ht_zeroconf_withdraw(htself_t *self)
{
    if (self->halgroup_publisher)
	zeroconf_service_withdraw(self->halgroup_publisher);
    if (self->halrcomp_publisher)
	zeroconf_service_withdraw(self->halrcomp_publisher);
    if (self->halrcmd_publisher)
	zeroconf_service_withdraw(self->halrcmd_publisher);

    // deregister poll adapter
    if (self->av_loop)
        avahi_czmq_poll_free(self->av_loop);
    return 0;
}
