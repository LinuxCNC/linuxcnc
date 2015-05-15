// implements a flavor of ZWS over protobuf framing, see zws.proto
//
// this assumes client side either protobuf support is available, or
// messages are parsed manually
//
// the websockets protocols "ZWS1.0-proto" and 	"ZWS1.0-proto-base64" activate this
// policy; the base64 variant is textmode-safe
//
// use like so:
// webtalk --foreground --debug -1 --plugin ../../../../lib/webtalk/plugins/pbzws.o

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libwebsockets.h>
#include <cdecode.h>
#include <cencode.h>

#include "webtalk.hh"
#include <google/protobuf/text_format.h>
#include "zws.pb.h"
namespace gpb = google::protobuf;

#define INITIAL_BUFSIZE 1024
#define MAX_BUFSIZE (1024*1024)  // fail if more needed - DoS
#define B64SIZE(x) ((((x)+2)/3)*4)  // size increase of b64 encoding a buffer
#define VERSION 1
#define MAX_ERRMSG_SIZE 1024

struct pbzws_session {
    unsigned char *buf;  // base64 encoding/decoding buffer & other temp use
    size_t bufsize;      // enlarged as needed
    zws::Frame *pzf;
    int client_version;
    const struct libwebsocket_protocols *proto;
    zws_session_t *wss; // technically the superclass instance
};

static inline bool b64wrapped(const struct pbzws_session *self)
{ return !!(self->proto->id & PROTO_WRAP_BASE64); }

static int buf_resize(struct pbzws_session *self, size_t required);
static int payload_fromws(zws::Frame *f, void *socket);
static int frame_tows(struct pbzws_session *self);
static int frame_fromws(struct pbzws_session *self);
static int error_tows(struct pbzws_session *self, const char *fmt, ...);
static int handle_subscription(struct pbzws_session *self, zws::frameType type);

// relay policy handler
static int
pbzws_policy(wtself_t *server,
	     zws_session_t *wss,
	     zwscb_type cbtype)
{
    zmsg_t *m;
    zframe_t *f;
    int rc;
    zws::frameType type;
    struct pbzws_session *self = (struct pbzws_session *) wss->user_data;

    switch (cbtype) {

    case ZWS_CONNECTING:
	// allocate per-session data
	wss->user_data = calloc(1, sizeof(struct pbzws_session));
	assert(wss->user_data != NULL);
	self = (struct pbzws_session *) wss->user_data;
	self->wss = wss;
	self->proto = libwebsockets_get_protocol (wss->wsiref);
	self->client_version = -1;

	lwsl_notice("%s: ZWS_CONNECTING protocol='%s'\n", __func__, self->proto->name);

	if (buf_resize(self, INITIAL_BUFSIZE)) {
	    lwsl_err("%s: cant malloc %d: %zu\n",
		     __func__, INITIAL_BUFSIZE);
	    return -1;
	}
	self->pzf = new zws::Frame();
	return 0; // no default policy

    case ZWS_ESTABLISHED:
	lwsl_notice("%s: ZWS_ESTABLISHED\n", __func__);

	// send hello frame
	self->pzf->Clear();
	self->pzf->set_type(zws::MT_HELLO);
	self->pzf->set_version(VERSION);
	self->pzf->set_sec(zws::SM_ZMQ_NONE); // or PLAIN - make this configurable?
	return frame_tows(self);
	break;

    case ZWS_CLOSE:
	lwsl_notice("%s: ZWS_CLOSE\n", __func__);
	if (self->pzf)
	    delete self->pzf;
	if (self->bufsize)
	    free(self->buf);
	free(wss->user_data);
	break;

    case ZWS_FROM_WS:
	rc = frame_fromws(self);
	if (rc) return rc;

	if (server->cfg->debug & LLL_DEBUG) {
	    std::string s;
	    gpb::TextFormat::PrintToString(*self->pzf, &s);
	    lwsl_debug("%s: fromws=%s\n",__func__,s.c_str());
	}
	type = self->pzf->type();

	switch (type) {
	case zws::MT_SOCKET:
	    wss->socket_type = self->pzf->stype();
	    wss->socket = zsocket_new (server->ctx, wss->socket_type);
	    if (self->cfg->ipv6) {
		zsocket_set_ipv6 (wss->socket, 1);
		assert (zsocket_ipv6 (wss->socket) == 1);
	    }
	    switch(self->pzf->sec()) {
	    case zws::SM_ZMQ_PLAIN:
		zsocket_set_plain_username (wss->socket, self->pzf->user().c_str());
		zsocket_set_plain_password (wss->socket, self->pzf->passwd().c_str());
		break;
	    default:
		break;
	    }
	    if (self->pzf->has_identity())
		zsocket_set_identity (wss->socket, self->pzf->identity().c_str());

	    register_zmq_poller(wss);
	    // fall through

	case zws::MT_CONNECT:
	    for (int i = 0; i < self->pzf->uri_size(); i++) {
		const char *uri = self->pzf->uri(i).c_str();
		if (zsocket_connect (wss->socket, uri) < 0) {
		    error_tows(self, "connect: endpoint '%s' invalid (%s socket)",
			       uri, zsocket_type_str (self->wss->socket));
		    return -1;
		}
	    }
	    break;

	case zws::MT_DISCONNECT:
	    for (int i = 0; i < self->pzf->uri_size(); i++) {
		const char *uri = self->pzf->uri(i).c_str();
		if (zsocket_connect (wss->socket, uri) < 0) {
		    error_tows(self,
			       "disconnect: endpoint '%s' invalid or operation not supported (%s socket)",
			       uri, zsocket_type_str (self->wss->socket));
		    return -1;
		}
	    }
	    break;

	case zws::MT_SUBSCRIBE:
	case zws::MT_UNSUBSCRIBE:
	    return handle_subscription(self, self->pzf->type());

	case zws::MT_PAYLOAD:
	    if (payload_fromws(self->pzf, wss->socket)) {
		error_tows(self, "sending payload to type %s socket failed",
			   zsocket_type_str (self->wss->socket));
		return -1;
	    }
	    break;

	case zws::MT_ERROR:
	    lwsl_err("client MT_ERROR: '%s'\n", self->pzf->errormsg().c_str());
	    break;

	default:
	    error_tows(self, "invalid frametype %d received on websocket", type);
	    return -1;
	};
	break;

    case ZWS_TO_WS:
	// a message was received from the zeroMQ socket
	m = zmsg_recv(wss->socket);
	self->pzf->Clear();
	self->pzf->set_type(zws::MT_PAYLOAD);

	while ((f = zmsg_pop (m)) != NULL) {
	    self->pzf->add_payload(zframe_data(f), zframe_size(f));
	    zframe_destroy(&f);
	}
	rc = frame_tows(self);
	zmsg_destroy(&m);
	return rc;
	break;

    default:
	lwsl_err("%s: unhandled operation type: %d\n", __func__, cbtype);
	break;
    }
    return 0;
}

// compute the next highest power of 2 of 32-bit v
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline size_t  next_power_of_two(size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static int buf_resize(struct pbzws_session *self, size_t required)
{
    if (required > MAX_BUFSIZE) return -1;
    if (self->bufsize > required) return 0;
    if (self->buf != NULL)
	free(self->buf);

    size_t size = required > INITIAL_BUFSIZE ? next_power_of_two(required) : INITIAL_BUFSIZE;
    self->buf = (unsigned char *) malloc(size);
    assert(self->buf != NULL);
    self->bufsize = size;
    return 0;
}

static int error_tows(struct pbzws_session *self, const char *fmt, ...)
{
    va_list ap;
    char buf[MAX_ERRMSG_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, MAX_ERRMSG_SIZE, fmt, ap);
    va_end(ap);

    lwsl_err("%s: %s\n", __func__, buf);

    self->pzf->Clear();
    self->pzf->set_type(zws::MT_ERROR);
    self->pzf->set_errormsg(buf);
    return frame_tows(self);
}

// serialize and send to ws client. Optionally base64-encode.
static int frame_tows(struct pbzws_session *self)
{
    zframe_t *f;

    size_t pbsize =  self->pzf->ByteSize();
    buf_resize(self,  b64wrapped(self) ? B64SIZE(pbsize) + pbsize : pbsize);

    unsigned char *end = self->pzf->SerializeWithCachedSizesToArray(self->buf);

    if (end == self->buf) {
	lwsl_err("%s: SerializeWithCachedSizesToArray failed\n",  __func__);
	return 1;
    }
    if (b64wrapped(self)) {
	size_t cnt;
	base64_encodestate state;
	base64_init_encodestate(&state);
	cnt = base64_encode_block((char *)self->buf, pbsize, (char *)end, &state);
	cnt += base64_encode_blockend((char *)end + cnt, &state);
	f = zframe_new (end, cnt);
    } else {
	f = zframe_new (self->buf, pbsize);
    }
    return zframe_send(&f, self->wss->wsq_out, 0);
}

static int frame_fromws(struct pbzws_session *self)
{
    unsigned char *data;
    size_t data_size;

    if (b64wrapped(self)) {
	if (buf_resize(self, self->wss->length)) {
	    lwsl_err("%s: size exceeded - malloc failed: %zu\n",
		     __func__, self->wss->length);
	    return -1;
	}
	base64_decodestate state;
	base64_init_decodestate(&state);
	data_size = base64_decode_block((const char*)self->wss->buffer,  self->wss->length,
					(char *)self->buf, &state);
	data = (unsigned char *) self->buf;
    } else {
	data_size = self->wss->length;
	data = (unsigned char *) self->wss->buffer;
    }

    if (!self->pzf->ParseFromArray(data, data_size)) {
	error_tows(self, "couldnt parse websocket input as protobuf (length %zu)", data_size);
	return -1;
    }
    return 0;
}

// unwrap the payload array and send as multipart msg
static int payload_fromws(zws::Frame *f, void *socket)
{
    int rc;
    zmsg_t *m = zmsg_new();

    for (int i = 0; i < f->payload_size(); i++) {
	const std::string &s = f->payload(i);
	rc = zmsg_addmem (m, s.data(), s.size());
	if (rc) {
	    lwsl_err("%s: zmsg_addmem failed %d, size=%d\n",
		     __func__, rc, s.size());
	    zmsg_destroy(&m);
	    return -1;
	}
    }
    rc = zmsg_send(&m, socket);
    if (rc) {
	lwsl_err("%s: zmsg_send failed %d %s\n",
		 __func__, rc, strerror(errno));
	zmsg_destroy(&m);
	return -2;
    }
    return f->payload_size();
}

static int handle_subscription(struct pbzws_session *self, zws::frameType type)
{
    int rc = 0;
    zframe_t *f;

    for (int i = 0; i < self->pzf->topic_size(); i++) {
	const char *topic = self->pzf->topic(i).c_str();
	std::string s;
	switch (self->wss->socket_type) {
	case ZMQ_SUB:
	    if (type == zws::MT_SUBSCRIBE)
		zsocket_set_subscribe (self->wss->socket, topic);
	    else
		zsocket_set_unsubscribe (self->wss->socket, topic);
	    break;
	case ZMQ_XSUB:
	    if (type == zws::MT_SUBSCRIBE)
		s = std::string("\001" + self->pzf->topic(i));
	    else
		s = std::string("\00" + self->pzf->topic(i));
	    f = zframe_new (s.data(), s.size());
	    rc = zframe_send(&f, self->wss->socket, 0);
	    if (rc)
		return rc;
	    break;
	default:
	    error_tows(self, "cant %ssubscribe on a type %s socket",
		       type == zws::MT_SUBSCRIBE ? "":"un",
		       zsocket_type_str (self->wss->socket));
	    return -1;
	}
    }
    return 0;
}
// plugin descriptor structure
// exported symbol: proxy_policy
// see webtalk_plugin.cc
zwspolicy_t proxy_policy {
    "pbzws",
     pbzws_policy,
};
