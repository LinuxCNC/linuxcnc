#ifdef NOTYET

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

// remote HAL instance bridging functions

#include "haltalk.hh"
#include <arpa/inet.h>
#include <sys/socket.h>
// #include "halpb.hh"
// #include "pbutil.hh"
// #include "rtapi_hexdump.h"

// #include <google/protobuf/text_format.h>

typedef enum {
    BSTATE_DISCOVER,
    BSTATE_RETRY_PROBE,
    BSTATE_CONNECT,
    BSTATE_UP,
    BSTATE_FAIL
} bridgestate_t;

typedef enum {
    BEVENT_STARTUP,
    BEVENT_DESCRIBE_SENT,
    BEVENT_DESCRIBE_RECEIVED,
    BEVENT_UP,
    BEVENT_FAIL
} bridgeevent_t;

static int bridge_fsm(htself_t *self,bridgeevent_t event);

int bridge_init(htself_t *self)
{
    htconf_t *cfg = self->cfg;
    //int retval;

    if (cfg->bridgecomp == NULL)
	return 0; // bridging not requested

    if (cfg->bridge_target_instance == rtapi_instance) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: cant bridge to myself - instance id %d",
			cfg->progname,  rtapi_instance);
	return -1;
    }

    htbridge_t *bridge = (htbridge_t *) calloc(1, sizeof(htbridge_t));
    if (bridge == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: bridge - cant calloc %d bytes",
			cfg->progname, sizeof(htbridge_t));
	return -1;
    }
    self->bridge = bridge;

    if ((cfg->bridgecomp_cmduri == NULL) ||
	(cfg->bridgecomp_updateuri == NULL)) {
	bridge->state = BSTATE_DISCOVER; // need to find target URI's first
    } else
	bridge->state = BSTATE_CONNECT; // all set

    bridge_fsm(self, BEVENT_STARTUP);
    return 0;
}

// -- end public functions ---

static int
handle_sd_input(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    htself_t *self = (htself_t *)arg;
    struct sockaddr_in remote_addr = {0};
    socklen_t addrlen = sizeof(remote_addr);
    unsigned char buffer[8192];

    // if (poller->revents & ZMQ_POLLIN) {
    // 	size_t len = recvfrom(sd_socket(self->bridge->sdiscover), buffer, sizeof(buffer), 0,
    // 			      (struct sockaddr *)&remote_addr, &addrlen);


    // }

    return 0;
}

static int
handle_sd_timer(zloop_t *loop, int timer_id, void *arg)
{
    htself_t *self = (htself_t *)arg;

    return 0;
}

static int
prepare_discovery(htself_t *self)
{
    htbridge_t *bridge = self->bridge;
    int retval;

    // bridge->sdiscover = sd_new(0, self->cfg->bridge_target_instance);
    // retval = sd_add(bridge->sdiscover,  pb::ST_HAL_RCOMMAND, 0, pb::SA_ZMQ_PROTOBUF);
    // assert(retval == 0);
    // retval = sd_add(bridge->sdiscover,  pb::ST_STP_HALRCOMP, 0, pb::SA_ZMQ_PROTOBUF);
    // assert(retval == 0);
    // sd_log(bridge->sdiscover, self->cfg->debug > 1);

    // zmq_pollitem_t sd_poller = { 0, sd_socket(bridge->sdiscover), ZMQ_POLLIN };
    // zloop_poller(self->z_loop, &sd_poller, handle_sd_input, self);
    // bridge->timer_id = zloop_timer(self->z_loop, 500, 1, handle_sd_timer, (void *)self); // one shot
    // assert(bridge->timer_id > -1);

    // return sd_send_probe(bridge->sdiscover);
}

static int bridge_fsm(htself_t *self, bridgeevent_t event)
{
    int retval;
    htbridge_t *bridge = self->bridge;


    switch (bridge->state) {

    case BSTATE_DISCOVER:
	prepare_discovery(self);
	break;
    case BSTATE_RETRY_PROBE:

    case BSTATE_CONNECT:
	bridge->z_bridge = zsocket_new (self->z_context, ZMQ_XSUB);
	retval = zsocket_connect(bridge->z_bridge, self->cfg->bridgecomp_updateuri);
	assert (retval == 0);
	bridge->z_bridge_cmd = zsocket_new (self->z_context, ZMQ_DEALER);
	retval = zsocket_connect(bridge->z_bridge_cmd, self->cfg->bridgecomp_cmduri);
	assert (retval == 0);
	break;

    // case BSTATE_WAIT_REPLY:
    // 	break;
    case BSTATE_UP:
	break;
    case BSTATE_FAIL:
	break;
    }
    return 0;
}
#endif
