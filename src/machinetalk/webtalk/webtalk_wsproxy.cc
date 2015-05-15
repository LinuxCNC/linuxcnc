// embeddable zeroMQ/websockets proxy server
// see zwsmain.c and zwsproxy.h for usage information
//
// the server runs in the context of a czmq poll loop
// messages to the websocket are funneled through a zmq PAIR pipe
// (similar to the ringbuffer in test-server.c but using zeroMQ means).

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "webtalk.hh"

static int libws_socket_readable(zloop_t *loop, zmq_pollitem_t *item, void *context);
static int zmq_socket_readable(zloop_t *loop, zmq_pollitem_t *item, void *context);
static int wsqin_socket_readable(zloop_t *loop, zmq_pollitem_t *item, void *context);
const char *zwsmimetype(const char *ext);

// map poll(2) revents to ZMQ event masks and back
static inline int poll2zmq(int mask)
{
    int result = 0;
    if (mask & POLLIN) result |= ZMQ_POLLIN;
    if (mask & POLLOUT) result |= ZMQ_POLLOUT;
    if (mask & POLLERR) result |= ZMQ_POLLERR;
    return result;
}

static inline int zmq2poll(int mask)
{
    int result = 0;
    if (mask & ZMQ_POLLIN) result |= POLLIN;
    if (mask & ZMQ_POLLOUT) result |= POLLOUT;
    if (mask & ZMQ_POLLERR) result |= POLLERR;
    return result;
}

int wt_proxy_new(wtself_t *self)
{
    self->policies = zlist_new();
    self->cfg->info.user = self; // pass instance pointer
    // protocols is a zero delimited array of struct libwebsocket_protocols
    self->cfg->info.protocols = protocols;
    return 0;
}

int wt_proxy_add_policy(wtself_t *self, const char *name, zwscvt_cb cb)
{
    zwspolicy_t *policy = (zwspolicy_t *) zmalloc (sizeof (zwspolicy_t));
    if (policy == NULL) {
	perror("malloc");
	return -1;
    }
    policy->name = strdup(name);
    policy->callback = cb;
    zlist_append (self->policies, policy);
    lwsl_debug("%s: add policy '%s' at %p\n",
	       __func__, name, cb);
    return 0;
}

void wt_proxy_exit(wtself_t *self)
{
    zlist_destroy (&self->policies);
}

int register_zmq_poller(zws_session_t *wss)
{
    if (wss->socket == NULL)
	return -1;

    // start watching the zmq socket
    wtself_t *self = (wtself_t *) libwebsocket_context_user(wss->ctxref);

    wss->pollitem.socket =  wss->socket;
    wss->pollitem.fd = 0;
    wss->pollitem.events =  ZMQ_POLLIN;
    int retval = zloop_poller (self->netopts.z_loop, &wss->pollitem,
			       zmq_socket_readable, wss);
    assert(retval == 0);
    return 0;
}

int
service_timer_callback(zloop_t *loop, int  timer_id, void *context)
{
    libwebsocket_service ((struct libwebsocket_context *) context, 0);
    return 0;
}

//---- internals ----

static const UriQueryListA *find_query(zws_session_t *wss, const char *name)
{
    const UriQueryListA *q = wss->queryList;
    while (q != NULL) {
	if (!strcmp(q->key, name))
	    return q;
	q = q->next;
    }
    return NULL;
}

static int set_policy(zws_session_t *wss, wtself_t *self)
{
    const UriQueryListA *q;

    if ((q = find_query(wss, "policy")) == NULL) {
	wss->policy = default_policy;
	return 0;
    } else {
	zwspolicy_t *p;
	for (p = (zwspolicy_t *) zlist_first(self->policies);
	     p != NULL;
	     p = (zwspolicy_t *) zlist_next(self->policies)) {
	    if (!strcmp(q->value, p->name)) {
		lwsl_uri("%s: setting policy '%s' callback=%p\n",
			 __func__, p->name, p->callback);
		wss->policy = p->callback;
		break;
	    }
	}
	if (p == NULL) {
	    lwsl_err("%s: no such policy '%s'\n",
		     __func__, q->value);
	    return -1; // close connection
	}
    }
    return 0;
}

// serves files via HTTP from www_dir if not NULL
static int serve_http(struct libwebsocket_context *context,
		      wtself_t *self, struct libwebsocket *wsi,
		      void *in, size_t len)
{
    const char *ext, *mt = NULL;
    char buf[PATH_MAX];

    if (self->cfg->www_dir == NULL) {
	lwsl_err( "closing HTTP connection - www_dir not configured\n");
	return -1;
    }
    snprintf(buf, sizeof(buf), "%s/", self->cfg->www_dir);
    if (uriUriStringToUnixFilenameA((const char *)in,
				    &buf[strlen(self->cfg->www_dir)])) {
	lwsl_err("HTTP: cant normalize '%s'\n", (const char *)in);
	return -1;
    }
    if (strstr((const char *)buf + strlen(self->cfg->www_dir), "..")) {
	lwsl_err("closing HTTP connection: not serving files with '..': '%s'\n",
		 (char *) in);
	return -1;
    }
    ext = strrchr((const char *)in, '.');
    if (ext)
	mt = zwsmimetype(ext);
    if (mt == NULL)
	mt = "text/html";

    if (access(buf, R_OK)) {
	lwsl_err("HTTP: 404 on '%s'\n",buf);
	char m404[PATH_MAX];
	size_t len = snprintf(m404, sizeof(m404),
			      "HTTP/1.0 404 Not Found\r\n"
			      "Content-type: text/html\r\n\r\n"
			      "<html><head><title>404 Not Found</title></head>"
			      "<body><h1>Not Found</h1>"
			      "<p>The requested URL %s "
			      "was not found on this server."
			      "path was: '%s'</p>"
			      "<address>webtalk Port %d</address>"
			      "</body></html>",
			      (char *)in,buf, self->cfg->info.port);
	libwebsocket_write (wsi, (unsigned char *)m404, len, LWS_WRITE_HTTP);
	return -1;
    }
    lwsl_debug("serving '%s' mime type='%s'\n", buf, mt);
#ifdef LWS_FEATURE_SERVE_HTTP_FILE_HAS_OTHER_HEADERS_LEN
    if (libwebsockets_serve_http_file(context, wsi, buf, mt, NULL, 0))
        return -1;
#else
    if (libwebsockets_serve_http_file(context, wsi, buf, mt, NULL))
        return -1;
#endif
    return 0;
}

// HTTP + Websockets server
int
callback_http(struct libwebsocket_context *context,
	      struct libwebsocket *wsi,
	      enum libwebsocket_callback_reasons reason, void *user,
	      void *in, size_t len)
{


    zws_session_t *wss = (zws_session_t *)user;
    wtself_t *self = (wtself_t *) libwebsocket_context_user(context);
    struct libwebsocket_pollargs *pa = (struct libwebsocket_pollargs *)in;

    switch (reason) {

	// any type of connection
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
	{
	    char client_name[128];
	    char client_ip[128];
	    libwebsockets_get_peer_addresses(context, wsi, (int)(long)in, client_name,
					     sizeof(client_name),
					     client_ip,
					     sizeof(client_ip));
	    lwsl_info("%s connect from %s (%s)\n",
		      __func__, client_name, client_ip);
	    // access control: insert any filter here and return -1 to close connection
	}
	break;

	// basic http serving
    case LWS_CALLBACK_HTTP:
	return serve_http(context, self, wsi, in, len);
	break;

	// websockets support
    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
	{
	    int fd = libwebsocket_get_socket_fd(wsi);

	    // parse URI and query
	    // set policy if contained in query
	    // call ZWS_CONNECTING callback

	    UriParserStateA state;
	    int itemCount;
	    const UriQueryListA *q;
	    char geturi[MAX_HEADER_LEN];
	    char uriargs[MAX_HEADER_LEN];
	    int retval = 0;

	    wss->txbuffer = (unsigned char *) zmalloc(LWS_INITIAL_TXBUFFER);
	    assert(wss->txbuffer);
	    wss->txbufsize = LWS_INITIAL_TXBUFFER;

	    // extract and parse uri and args
	    lws_hdr_copy(wsi, geturi, sizeof geturi, WSI_TOKEN_GET_URI);
	    int arglen = lws_hdr_copy(wsi, uriargs, sizeof uriargs, WSI_TOKEN_HTTP_URI_ARGS);

	    const struct libwebsocket_protocols *proto = libwebsockets_get_protocol(wsi);

	    lwsl_uri("%s %d: proto='%s' uri='%s' args='%s'\n",
		     __func__, fd, proto->name, geturi, uriargs);

	    state.uri = &wss->u;
	    if (uriParseUriA(&state, geturi) != URI_SUCCESS) {
		lwsl_err("Websocket %d: cant parse URI: '%s' near '%s' rc=%d\n",
			 fd, geturi, state.errorPos, state.errorCode);
		return -1;
	    }
	    if ((retval = uriDissectQueryMallocA(&wss->queryList, &itemCount, uriargs,
						 uriargs + arglen)) != URI_SUCCESS) {
		lwsl_err("Websocket %d: cant dissect query: '%s' rc=%d\n",
			 fd, geturi, retval);
		return -1;
	    }
	    if ((q = find_query(wss, "debug")) != NULL)
		lws_set_log_level(atoi(q->value), NULL);

	    if (set_policy(wss, self))
		return -1; // invalid policy - close connection

	    wss->wsiref = wsi;
	    wss->ctxref = context;

	    retval = wss->policy(self, wss, ZWS_CONNECTING);
	    if (retval > 0) // user policy indicated to use default policy function
		return default_policy(self, wss, ZWS_CONNECTING);
	    else
		return retval;
	}
	break;

    case LWS_CALLBACK_ESTABLISHED:
	{
	    int retval;

	    // the two/from WS pair pipe
	    wss->wsq_out = zsocket_new (self->netopts.z_context, ZMQ_PAIR);
	    assert (wss->wsq_out);
	    zsocket_bind (wss->wsq_out, "inproc://wsq-%p", wss);

	    wss->wsq_in = zsocket_new (self->netopts.z_context, ZMQ_PAIR);
	    assert (wss->wsq_in);
	    zsocket_connect (wss->wsq_in, "inproc://wsq-%p", wss);

	    // start watching the to-websocket pipe
	    wss->wsqin_pollitem.socket =  wss->wsq_in;
	    wss->wsqin_pollitem.fd = 0;
	    wss->wsqin_pollitem.events =  ZMQ_POLLIN;
	    assert(zloop_poller (self->netopts.z_loop, &wss->wsqin_pollitem,
				 wsqin_socket_readable, wss) == 0);
	    wss->wsqin_poller_active = true;

	    retval = wss->policy(self, wss, ZWS_ESTABLISHED);
	    if (retval > 0) // user policy indicated to use default policy function
		default_policy(self, wss, ZWS_ESTABLISHED);
	    // register_zmq_poller(wss) now deferred to ZWS_ESTABLISHED handler
	    // so a socket can be created during negotiation, not on connect only
	}
	break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
	{
	    int m,n;
	    zframe_t *f;

	    // let library handle partial writes
	    // deal with ws send flow control only

	    if (!lws_send_pipe_choked(wsi) &&
		(wss->wsqin_poller_active == false)) {

		// send pipe just unchoked, meaning we were called due to
		// libwebsocket_callback_on_writable(context, wsi)
		// the  wsq_in poller was disabled, so good to reenable now.
		// since we're ready to take more

		assert(zloop_poller (self->netopts.z_loop, &wss->wsqin_pollitem,
				     wsqin_socket_readable, wss) == 0);
		wss->wsqin_poller_active = true;
	    }

	    // now stuff down what we have, and as fast as we can
	    do {
		if ((f = zframe_recv_nowait (wss->wsq_in)) == NULL)
		    break; // done for now

		n = zframe_size(f);
		wss->wsout_msgs++;
		size_t needed  = n + LWS_SEND_BUFFER_PRE_PADDING +
		    LWS_SEND_BUFFER_POST_PADDING;
		if (needed > wss->txbufsize) {
		    // enlarge as needed
		    needed += LWS_TXBUFFER_EXTRA;
		    lwsl_info("Websocket %d: enlarge txbuf %d to %d\n",
			     libwebsocket_get_socket_fd(wsi), wss->txbufsize, needed);
		    free(wss->txbuffer);
		    wss->txbuffer = (unsigned char *) zmalloc(needed);
		    assert(wss->txbuffer);
		    wss->txbufsize = needed;
		}
		memcpy(&wss->txbuffer[LWS_SEND_BUFFER_PRE_PADDING], zframe_data(f), n);
		m = libwebsocket_write(wsi, &wss->txbuffer[LWS_SEND_BUFFER_PRE_PADDING],
				       n, wss->txmode);
		assert(m == n);  // library must handle this case
		zframe_destroy(&f);
		wss->completed++;
		wss->wsout_bytes += m;

		if (lws_send_pipe_choked(wsi)) {

		    // this write choked.
		    // disable wsq_in poller while stuck - write cb will take care of this
		    // otherwise this is hammered by zmq readable callbacks because
		    // of pending frames in the wsq_in pipe

		    zloop_poller_end (self->netopts.z_loop, &wss->wsqin_pollitem);
		    wss->wsqin_poller_active = false;
		    libwebsocket_callback_on_writable(context, wsi);
		    break;
		}
	    } while (1);
	}
	break;

    case LWS_CALLBACK_CLOSED:
	{
	    // any wss->user_data is to be deallocated in this callback
	    int retval = wss->policy(self, wss, ZWS_CLOSE);
	    if (retval > 0) // user policy indicated to use default policy function
		default_policy(self, wss, ZWS_CLOSE);

	    lwsl_info("Websocket %d stats: in %d/%d out"
		      " %d/%d zmq %d/%d partial=%d retry=%d complete=%d txbuf=%d\n",
		      libwebsocket_get_socket_fd(wsi), wss->wsin_msgs, wss->wsin_bytes,
		      wss->wsout_msgs, wss->wsout_bytes,
		      wss->zmq_msgs, wss->zmq_bytes,
		      wss->partial, wss->partial_retry, wss->completed,
		      wss->txbufsize);
	    // stop watching and destroy the zmq sockets

	    if (wss->pollitem.socket != NULL)
		zloop_poller_end (self->netopts.z_loop, &wss->pollitem);
	    zloop_poller_end (self->netopts.z_loop, &wss->wsqin_pollitem);

	    if (wss->socket != NULL)
		zsocket_destroy (self->netopts.z_context, wss->socket);
	    zsocket_destroy (self->netopts.z_context, wss->wsq_in);
	    zsocket_destroy (self->netopts.z_context, wss->wsq_out);

	    uriFreeQueryListA(wss->queryList);
	    uriFreeUriMembersA(&wss->u);
	    free(wss->txbuffer);
	}
	break;

    case LWS_CALLBACK_RECEIVE:
	{
	    wss->buffer = in;
	    wss->length = len;
	    wss->wsin_bytes += len;
	    wss->wsin_msgs++;
	    int retval = wss->policy(self, wss, ZWS_FROM_WS);
	    if (retval > 0) // user policy indicated to use default policy function
		return default_policy(self, wss, ZWS_FROM_WS);
	    else
		return retval;
	}
	break;

	// external poll support for zloop
    case LWS_CALLBACK_ADD_POLL_FD:
	{
	    short zevents = poll2zmq(pa->events);
	    zmq_pollitem_t additem = { 0, pa->fd, zevents };
	    assert(zloop_poller (self->netopts.z_loop, &additem,
				 libws_socket_readable, context) == 0);

	    if (zevents & ZMQ_POLLERR) // dont remove poller on POLLERR
		zloop_set_tolerant (self->netopts.z_loop, &additem);
	}
	break;

    case LWS_CALLBACK_DEL_POLL_FD:
	{
	    zmq_pollitem_t delitem = { 0, pa->fd, 0 };
	    zloop_poller_end (self->netopts.z_loop, &delitem);
	}
	break;

    case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
	{
	    if (pa->prev_events == pa->events) // nothing to do
		break;

	    // remove existing poller
	    zmq_pollitem_t item = { 0, pa->fd, 0 };
	    zloop_poller_end (self->netopts.z_loop, &item);

	    // insert new poller with current event mask
	    item.events = poll2zmq(pa->events);

	    assert(zloop_poller (self->netopts.z_loop, &item, libws_socket_readable, context) == 0);
	    if (item.events & ZMQ_POLLERR) // dont remove poller on POLLERR
		zloop_set_tolerant (self->netopts.z_loop, &item);
	}
	break;

    default:
	break;
    }
    return 0;
}

static int
libws_socket_readable(zloop_t *loop, zmq_pollitem_t *item, void *context)
{
    struct pollfd pollstruct;
    int pevents = zmq2poll(item->revents);

    pollstruct.fd = item->fd;
    pollstruct.revents = pollstruct.events = pevents;
    libwebsocket_service_fd((struct libwebsocket_context *)context, &pollstruct);
    return 0;
}

static int
zmq_socket_readable(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
    zws_session_t *wss = (zws_session_t *)arg;
    wtself_t *self = (wtself_t *) libwebsocket_context_user(wss->ctxref);
    return wss->policy(self, wss, ZWS_TO_WS);
}

static int
wsqin_socket_readable(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
    zws_session_t *wss = (zws_session_t *)arg;
    libwebsocket_callback_on_writable(wss->ctxref, wss->wsiref);
    return 0;
}

static const struct {
    const char *extension;
    const char *mime_type;
} builtin_mime_types[] = {

    { ".html", "text/html" },
    { ".htm",  "text/html" },
    { ".shtm", "text/html" },
    { ".shtml","text/html" },
    { ".css",  "text/css" },
    { ".js",   "application/x-javascript" },
    { ".ico",  "image/x-icon" },
    { ".gif",  "image/gif" },
    { ".jpg",  "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".png",  "image/png" },
    { ".svg",  "image/svg+xml" },
    { ".torrent",  "application/x-bittorrent" },
    { ".wav",  "audio/x-wav" },
    { ".mp3",  "audio/x-mp3" },
    { ".mid",  "audio/mid" },
    { ".m3u",  "audio/x-mpegurl" },
    { ".ram",  "audio/x-pn-realaudio" },
    { ".xml",  "text/xml" },
    { ".xslt", "application/xml" },
    { ".ra",   "audio/x-pn-realaudio" },
    { ".doc",  "application/msword" },
    { ".exe",  "application/octet-stream" },
    { ".zip",  "application/x-zip-compressed" },
    { ".xls",  "application/excel" },
    { ".tgz",  "application/x-tar-gz" },
    { ".tar",  "application/x-tar" },
    { ".gz",   "application/x-gunzip" },
    { ".arj",  "application/x-arj-compressed" },
    { ".rar",  "application/x-arj-compressed" },
    { ".rtf",  "application/rtf" },
    { ".pdf",  "application/pdf" },
    { ".swf",  "application/x-shockwave-flash" },
    { ".mpg",  "video/mpeg" },
    { ".mpeg", "video/mpeg" },
    { ".asf",  "video/x-ms-asf" },
    { ".avi",  "video/x-msvideo" },
    { ".bmp",  "image/bmp" },
    { NULL,  NULL }
};

const char *
zwsmimetype(const char *ext)
{
    int i;
    for ( i = 0; builtin_mime_types[i].extension != NULL; i++)
	if (strcasecmp(ext, builtin_mime_types[i].extension) == 0)
	    return  builtin_mime_types[i].mime_type;
    return NULL;
}
