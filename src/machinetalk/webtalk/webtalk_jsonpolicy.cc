
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libwebsockets.h>

#include <google/protobuf/text_format.h>
#include <machinetalk/generated/message.pb.h>
using namespace google::protobuf;

#include "webtalk.hh"

// relay policy to convert to/from JSON as needed
int
json_policy(wtself_t *self,
	    zws_session_t *wss,
	    zwscb_type type)
{
    lwsl_debug("%s op=%d\n",__func__,  type);
    zmsg_t *m;
    zframe_t *f;
    static pb::Container c; // fast; threadsafe???

    switch (type) {

    case ZWS_CONNECTING:
	// > 0 indicates: run the default policy ZWS_CONNECTING code
	return 1;
	break;

    case ZWS_ESTABLISHED:
	return register_zmq_poller(wss);
	break;

    case ZWS_CLOSE:
	break;

    case ZWS_FROM_WS:

	switch (wss->socket_type) {
	case ZMQ_DEALER:
	case ZMQ_SUB:
	case ZMQ_XSUB:
	    {
		// parse from JSON:
		lwsl_fromws("%s: '%.*s'\n", __func__, wss->length, wss->buffer);
		try{
		    json2pb(c, (const char *) wss->buffer, wss->length);

		    zframe_t *z_pbframe;

		    switch (wss->socket_type) {

		    case ZMQ_DEALER: // pass on
			z_pbframe = zframe_new(NULL, c.ByteSize());
			assert(z_pbframe != NULL);
			if (c.SerializeWithCachedSizesToArray(zframe_data(z_pbframe))) {
			    assert(zframe_send(&z_pbframe, wss->socket, 0) == 0);
			} else {
			    lwsl_err("from_ws: cant serialize: '%.*s'\n",
				     wss->length, wss->buffer);
			    zframe_destroy(&z_pbframe);
			}
			c.Clear();
			return 0;
			break;

		    case ZMQ_SUB:
			// inspect message for inband subscribe/unsubscribe
			for (int i = 0; i < c.note_size(); i++) {
				if (c.type() == pb::MT_ZMQ_SUBSCRIBE) {
				lwsl_fromws("%s: subscribe to '%s'\n", __func__, c.note(i).c_str());
				zsocket_set_subscribe (wss->socket, c.note(i).c_str());
			    }
			    if (c.type() == pb::MT_ZMQ_UNSUBSCRIBE) {
				lwsl_fromws("%s: unsubscribe from '%s'\n", __func__, c.note(i).c_str());
				zsocket_set_unsubscribe (wss->socket, c.note(i).c_str());
			    }
			}
			c.Clear();
			return 0;
			break;

		    case ZMQ_XSUB: // TBD
			lwsl_err("from_ws: XSUB subscribe/unsubscripe not yet implemented : '%.*s'\n",
				 wss->length, wss->buffer);
		    }
		} catch (std::exception &ex) {
		    lwsl_err("%s from_ws: json2pb exception: %s on '%.*s'\n",
			     __func__, ex.what(),wss->length, wss->buffer);
		}

	    }
	    break;

	// case ZMQ_SUB:
	// case ZMQ_XSUB:
	//     lwsl_err("dropping frame '%.*s'\n",wss->length, wss->buffer);
	//     break;
	default:
	    break;
	}
	    // a frame was received from the websocket client
	// just pass on as-is:
	f = zframe_new (wss->buffer, wss->length);
	lwsl_fromws("%s: '%.*s'\n", __func__, wss->length, wss->buffer);
	return zframe_send(&f, wss->socket, 0);

    case ZWS_TO_WS:
	{
	    m = zmsg_recv(wss->socket);
	    if ((wss->socket_type == ZMQ_SUB) ||
		(wss->socket_type == ZMQ_XSUB)) {

		// just drop the topic frame
		f = zmsg_pop (m);
		zframe_destroy(&f);
#if 0
		topic = zmsg_popstr (m);
		lwsl_tows("%s: { topic = \"%s\"}\n", __func__, topic);

		assert(zstr_sendf(wss->wsq_out, "{ topic = \"%s\"}\n", topic) == 0);
		free(topic);
#endif
	    }

	    while ((f = zmsg_pop (m)) != NULL) {
		if (!c.ParseFromArray(zframe_data(f), zframe_size(f))) {
		    char *hex = zframe_strhex(f);
		    lwsl_err("cant protobuf parse from %s",
			     hex);
		    free(hex);
		    zframe_destroy(&f);
		    break;
		}
		// this breaks - probably needs some MergeFrom* pb method
		// if (!c.has_topic() && (topic != NULL))
		//     c.set_topic(topic); // tack on

		try {
		    std::string json = pb2json(c);
		    zframe_t *z_jsonframe = zframe_new( json.c_str(), json.size());
		    lwsl_tows("%s: '%s'\n", __func__, json.c_str());
		    assert(zframe_send(&z_jsonframe, wss->wsq_out, 0) == 0);

		} catch (std::exception &ex) {
		    lwsl_err("%s: pb2json exception: %s\n", __func__, ex.what());
		    std::string text;
		    if (TextFormat::PrintToString(c, &text))
			lwsl_err("%s: pb2json exception: %s\n container: %s\n",
				 __func__, ex.what(), text.c_str());
		}
	    }
	    zmsg_destroy(&m);
	    c.Clear();
	}
	break;

    default:
	break;
    }
    return 0;
}


// to make this a proper plug, export plugin descriptor structure here:
// see webtalk_plugin.cc
// struct policy policy { "json", json_policy };
