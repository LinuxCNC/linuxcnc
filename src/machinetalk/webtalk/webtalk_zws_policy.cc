// implements ZWS
// see https://raw.githubusercontent.com/somdoron/rfc/master/spec_39.txt

// use like so:
// webtalk --plugin <path-to-plugin.so>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libwebsockets.h>

// DoS/dummy protection: close connection if a multiframe input message
// has more than MAX_ZWS_FRAMES frames
#define MAX_ZWS_FRAMES 1000

#include "webtalk.hh"

struct zwsproto_session {
    zmsg_t *partial;  // accumulate frames from ws with 'more' flag set, send on final
};

static  int zws_policy(wtself_t *self,         // server instance
		       zws_session_t *s,       // session
		       zwscb_type type);       // which callback

// plugin descriptor structure
// exported symbol: proxy_policy
// see webtalk_plugin.cc
zwspolicy_t proxy_policy {
    "zws",
    zws_policy
};

static int handle_websocket_frame(struct zwsproto_session *zwss, zws_session_t *wss);

// relay policy handler
static int
zws_policy(wtself_t *self,
	   zws_session_t *wss,
	   zwscb_type type)
{
    zmsg_t *m;
    zframe_t *f;
    struct zwsproto_session *zwss = (struct zwsproto_session *) wss->user_data;
    char *data;

    switch (type) {

    case ZWS_CONNECTING:
	{
	    const struct libwebsocket_protocols *proto = libwebsockets_get_protocol (wss->wsiref);
	    lwsl_notice("%s: ZWS_CONNECTING protocol='%s'\n",
			__func__, proto->name);

	    wss->user_data = zmalloc(sizeof(struct zwsproto_session));
	    assert(wss->user_data != NULL);

	    // > 0 indicates: run the default policy ZWS_CONNECTING code
	    return 1;
	}
	break;

    case ZWS_ESTABLISHED:
	lwsl_notice("%s: ZWS_ESTABLISHED\n", __func__);
	return register_zmq_poller(wss);
	break;

    case ZWS_CLOSE:
	lwsl_notice("%s: ZWS_CLOSE\n", __func__);

	if (zwss->partial != NULL) {
	    lwsl_zws("%s: FRAMES LOST:  partial message pending on close, %d frames, socket type=%d\n",
		     __func__, zmsg_size(zwss->partial), wss->socket_type);
	    zmsg_destroy(&zwss->partial);
	}
	free(wss->user_data);
	break;

    case ZWS_FROM_WS:
	{
	    // a frame was received from the websocket client
	    switch (wss->socket_type) {
	    case ZMQ_SUB:
		{
		    data = (char *) wss->buffer;
		    switch (*data) {
		    case '0': // a final frame
			data++;
			switch (*data) {
			case '1': // subscribe command:
			    {
				std::string s(data + 1, data + wss->length-2);
				zsocket_set_subscribe (wss->socket, s.c_str());
				lwsl_zws("%s: SUB subscribe '%s'\n", __func__, s.c_str());
			    }
			    break;
			case '0': // unsubscribe command:
			    {
				std::string s(data + 1, data + wss->length-2);
				zsocket_set_unsubscribe (wss->socket, s.c_str());
				lwsl_zws("%s: SUB unsubscribe '%s'\n", __func__, s.c_str());
			    }
			    break;
			default:
			    lwsl_err("%s: invalid code in SUB ws input frame, c='%c' (0x%x)\n",
				       __func__, *data, *data);
			    return -1;
			}
		    }
		    break;
		}
		break;

	    case ZMQ_XSUB:
		data = (char *) wss->buffer;

		switch (*data) {
		case '0': // a final frame
		    data++;
		    switch (*data) {

		    case '1': // subscribe command:
			lwsl_zws("%s: XSUB subscribe '%.*s'\n", __func__,
				 wss->length - 2, data + 1);
			*data = '\001';
			f = zframe_new (data, wss->length - 1);
			return zframe_send(&f, wss->socket, 0);
			break;

		    case '0': // unsubscribe command:
			*data = '\000';
			lwsl_zws("%s: XSUB unsubscribe '%.*s'\n", __func__,
				 wss->length - 2, data + 1);
			f = zframe_new (data, wss->length - 1);
			return zframe_send(&f, wss->socket, 0);
			break;

		    default:
			// XXX single frame in on xsub - exotic but possible
			// strip unknow char and send the rest
			lwsl_info("%s: stripping unknown code XSUB in ws frame, c='%c' (0x%x)\n",
				  __func__, *data, *data);
			f = zframe_new (data + 1, wss->length - 1);
			return zframe_send(&f, wss->socket, 0);
		    }
		    break;

		case '1': // XXX multipart in on xsub - exotic but possible ?
		    return -1;
		    break;

		default:
		    lwsl_err("%s: invalid code on XSUB in ws frame, c='%c' (0x%x), socket type=%d\n",
			     __func__, *data, *data, wss->socket_type);
		    return -1;
		}
		break;

	    default:
		// all other socket types - pass on single & multiframe messages
		// without inspection
		return handle_websocket_frame(zwss, wss);
	    }
	}
	break;

    case ZWS_TO_WS:
	{
	    // a message was received from the zeroMQ socket
	    // ZWS: prepend non-final frames with '1', final frame with '0'

	    m = zmsg_recv(wss->socket);
	    size_t nf = zmsg_size (m);

	    while ((f = zmsg_pop (m)) != NULL) {
		wss->zmq_bytes += zframe_size(f);
		wss->zmq_msgs++;
		nf--;
		size_t nsize = zframe_size(f) + 1;

		zframe_t *nframe = zframe_new (NULL, nsize);
		char *data = (char *) zframe_data(nframe);

		// prepend more/final flag
		*data++ = (nf > 0) ? '1' : '0';
		memcpy(data, zframe_data(f), zframe_size(f));
		lwsl_tows("%s: %c %d:\"%.*s\"\n", __func__,
			  (nf > 0) ? '1' : '0',
			  zframe_size(f), zframe_size(f), zframe_data(f));
		zframe_send(&nframe, wss->wsq_out, 0);
		zframe_destroy(&f);
	    }
	    zmsg_destroy(&m);
	}
	break;

    default:
	lwsl_err("%s: unhandled operation type: %d\n", __func__, type);
	break;
    }
    return 0;
}

static int handle_websocket_frame(struct zwsproto_session *zwss,
				  zws_session_t *wss)
{
    zframe_t *f;
    char *data = (char *) wss->buffer;
    int rc;

    lwsl_zws("%s: ws in '%.*s'\n", __func__, wss->length, wss->buffer);

    switch (*data) {

    case '0': // last frame
	if (zwss->partial == NULL) {
	    // trivial case - single frame message, final
	    f = zframe_new (data + 1, wss->length - 1);
	    return zframe_send(&f, wss->socket, 0);
	};

	// append this frame to the accumulator msg
	rc = zmsg_addmem (zwss->partial, data + 1, wss->length - 1);
	assert (rc == 0);
	// and send it off - this sets zwss->partial to NULL
	rc = zmsg_send(&zwss->partial,  wss->socket);
	assert (rc == 0);
	break;

    case '1': // more to come
	if (zwss->partial == NULL) {
	    lwsl_zws("%s: start multiframe message\n",  __func__);
	    zwss->partial = zmsg_new();
	}

	// protect against input overflow
	if (zmsg_size(zwss->partial) > MAX_ZWS_FRAMES) {
	    lwsl_err("%s: message sized exceeded (%d frames), closing\n",
		     __func__, zmsg_size(zwss->partial));
	    // the resulting ZWS_CLOSE will free zwss->partial for us
	    return -1;
	}

	// append current frame
	rc = zmsg_addmem (zwss->partial, data + 1, wss->length - 1);
	lwsl_zws("%s: adding frame, count=%d\n",
		 __func__, zmsg_size(zwss->partial));
	break;

    default:
	// XXX unspecified prefix character - just pass on as a single frame after stripping first char:
	lwsl_info("%s: stripping unknown code in ws frame, c='%c' (0x%x), socket type=%d\n",
		  __func__, *data, *data, wss->socket_type);
	f = zframe_new (data + 1, wss->length - 1);
	return zframe_send(&f, wss->socket, 0);
    }
    return 0;
}
