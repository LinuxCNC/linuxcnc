// example translation plugin for webtalk
// adds a named translation policy
//
// use like so:
// webtalk --plugin example
//
// when a webtalk connect URI contains the parameter 'policy=example',
// all gateway interactions are routed through the example_policy handler
// this allows for custom translations

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libwebsockets.h>

#include "webtalk.hh"

static  int example_policy(wtself_t *self,         // server instance
			   zws_session_t *s,       // session
			   zwscb_type type);       // which callback

// plugin descriptor structure
// exported symbol: proxy_policy
// see webtalk_plugin.cc
zwspolicy_t proxy_policy {
    "example",
    example_policy
};


// relay policy handler
static int
example_policy(wtself_t *self,
	       zws_session_t *wss,
	       zwscb_type type)
{
    zmsg_t *m;
    zframe_t *f;

    switch (type) {

    case ZWS_CONNECTING:
	lwsl_notice("%s: ZWS_CONNECTING\n", __func__);
	// > 0 indicates: run the default policy ZWS_CONNECTING code
	return 1;
	break;

    case ZWS_ESTABLISHED:
	lwsl_notice("%s: ZWS_ESTABLISHED\n", __func__);
	return register_zmq_poller(wss);
	break;

    case ZWS_CLOSE:
	lwsl_notice("%s: ZWS_CLOSE\n", __func__);
	break;

    case ZWS_FROM_WS:
	// a frame was received from the websocket client
	// apply any custom translations here
	f = zframe_new (wss->buffer, wss->length);
	lwsl_notice("%s: '%.*s'\n", __func__, wss->length, wss->buffer);
	return zframe_send(&f, wss->socket, 0);

    case ZWS_TO_WS:
	{
	    // a frame was received from the zeroMQ socket
	    // apply any custom translations here, then send on to websocket

	    // zmq->ws: unwrap all frames and send individually by stuffing into wsq_out
	    // this might not make sense on subscribe sockets which send the topic frame
	    // first
	    m = zmsg_recv(wss->socket);
	    while ((f = zmsg_pop (m)) != NULL) {
		wss->zmq_bytes += zframe_size(f);
		wss->zmq_msgs++;
		lwsl_tows("%s: %d:'%.*s'\n", __func__, zframe_size(f),zframe_size(f),zframe_data(f));
		zframe_send(&f, wss->wsq_out, 0);
	    }
	    zmsg_destroy(&m);
	}
	break;

    default:
	lwsl_err("%s: unhandled type: %d\n", __func__, type);
	break;
    }
    return 0;
}
