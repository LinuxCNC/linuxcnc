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

#include "config.h"

#define PROXY_PORT 7681  // serves both http and ws

#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_LOCAL1  // where all rtapi/ulapi logging goes
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <uuid/uuid.h>
#include <czmq.h>

#include <libwebsockets.h>

#define LWS_INITIAL_TXBUFFER 4096  // transmit buffer grows as needed
#define LWS_TXBUFFER_EXTRA 256   // add to current required size if growing tx buffer

#ifdef LWS_MAX_HEADER_LEN
#define MAX_HEADER_LEN LWS_MAX_HEADER_LEN
#else
#define MAX_HEADER_LEN 1024
#endif

// this extends enum lws_log_levels in libwebsockets.h
enum wt_log_levels {
    LLL_URI    = 1 << 10,
    LLL_TOWS   = 1 << 11,
    LLL_FROMWS = 1 << 12,
    LLL_LOOP   = 1 << 13,
    LLL_CONFIG = 1 << 14,
    LLL_ZWS    = 1 << 15,
    LLL_LAST = 15
};

#define lwsl_uri(...)    _lws_log(LLL_URI, __VA_ARGS__)
#define lwsl_tows(...)   _lws_log(LLL_TOWS, __VA_ARGS__)
#define lwsl_fromws(...) _lws_log(LLL_FROMWS, __VA_ARGS__)
#define lwsl_loop(...)   _lws_log(LLL_LOOP, __VA_ARGS__)
#define lwsl_cfg(...)    _lws_log(LLL_CONFIG, __VA_ARGS__)
#define lwsl_zws(...)    _lws_log(LLL_ZWS, __VA_ARGS__)


#include <inifile.h>
#include <syslog_async.h>
#include "mk-service.hh"
#include "mk-zeroconf.hh"

#include <machinetalk/generated/message.pb.h>
namespace gpb = google::protobuf;

#include <json2pb.hh>
#include <jansson.h>
#include <uriparser/Uri.h>

typedef struct wtself wtself_t;
typedef struct zws_session_data zws_session_t;

// policy callback phases
typedef enum zwscvt_type {
    ZWS_CONNECTING,
    ZWS_ESTABLISHED,
    ZWS_CLOSE,
    ZWS_FROM_WS,
    ZWS_TO_WS,
} zwscb_type;

// return values:
// 0: success, -1: error; closes connection
// >0: invoke default policy callback
typedef  int (*zwscvt_cb)(wtself_t *self,         // server instance
			  zws_session_t *s,       // session
			  zwscb_type type);       // which callback


// protocol flags
typedef enum protocol_flags {
    PROTO_FRAMING_NONE        =  (1 << 0),
    PROTO_FRAMING_ZWS         =  (1 << 1),
    PROTO_FRAMING_MACHINEKIT  =  (1 << 2),
    PROTO_FRAMING_ZWSPROTO    =  (1 << 3),

    PROTO_ENCODING_NONE       =  (1 << 6),
    PROTO_ENCODING_PROTOBUF   =  (1 << 7),
    PROTO_ENCODING_JSON       =  (1 << 8),

    PROTO_WRAP_BASE64         =  (1 << 10),

    // add flags as needed - up to << 23
} protocol_flags;


// http://stackoverflow.com/questions/8936639/bit-shifting-masks-still-elude-me ;)
#define PROTO_VERSION_BITS    8
#define PROTO_VERSION_OFFSET  24
#define PROTO_VERSION_MASK   ~( ~0 << PROTO_VERSION_BITS)
#define PROTO_VERSION(x)     (( (x)  & PROTO_VERSION_MASK) <<  PROTO_VERSION_OFFSET)

// per-session data
typedef struct zws_session_data {
    void *socket; // zmq destination
    zmq_pollitem_t pollitem;
    int socket_type;
    libwebsocket_write_protocol txmode;

    void *wsq_in;
    void *wsq_out;
    zmq_pollitem_t wsqin_pollitem;
    bool wsqin_poller_active; // false - disabled while send pipe choked

    // adapt to largest frame as we go
    unsigned char  *txbuffer;
    size_t txbufsize;

    void *user_data; // if any: allocate in ZWS_CONNECT, freed in ZWS_CLOSE

    // the current frame received from WS, for the ZWS_FROM_WS callback
    void *buffer;
    size_t length;

    zframe_t *current;   // partially sent frame (to ws)
    size_t already_sent; // how much of current was sent already

    // needed for websocket writable callback
    struct libwebsocket *wsiref;
    struct libwebsocket_context *ctxref;

    // URI/args state
    UriUriA u;
    UriQueryListA *queryList;

    // the policy applied to this session
    zwscvt_cb  policy;

    // stats counters:
    int wsin_bytes, wsin_msgs;
    int wsout_bytes, wsout_msgs;
    int zmq_bytes, zmq_msgs;

    int partial;
    int partial_retry;
    int completed;
} zws_session_t;

typedef struct zwspolicy {
    const char *name;
    zwscvt_cb  callback;
} zwspolicy_t;

typedef struct wtconf {
    const char *progname;
    const char *inifile;
    const char *section;
    int debug;


#if 0
    char *service_uuid;
    int remote;
    unsigned ifIndex;
#endif
    bool foreground;
    bool log_stderr;
    bool use_ssl;
    int service_timer;
    struct lws_context_creation_info info;
    char *index_html; // path to announce
    char *www_dir;
    bool trap_signals;
    int ipv6;
    int rtapi_instance;
} wtconf_t;

typedef struct wtself {
    wtconf_t *cfg;
    int signal_fd;
    bool interrupted;
    pid_t pid;

    pb::Container rx; // any ParseFrom.. function does a Clear() first
    pb::Container tx; // tx must be Clear()'d after or before use

    zlist_t *policies;
    struct libwebsocket_context *wsctx;
    int service_timer;

    mk_netopts_t netopts;
    // the zeromq socket in mksock is not used in webtalk,
    // just the related parameters in mk_socket for
    // announcing the http/https service:
    mk_socket_t mksock;
    zservice_t zswww;

} wtself_t;



// wt_zeroconf.cc:
int wt_zeroconf_announce(wtself_t *self);
int wt_zeroconf_withdraw(wtself_t *self);

// webtalk_echo.cc:
void echo_thread(void *args, zctx_t *ctx, void *pipe);

// webtalk_proxy.cc:
int callback_http(struct libwebsocket_context *context,
		  struct libwebsocket *wsi,
		  enum libwebsocket_callback_reasons reason, void *user,
		  void *in, size_t len);
int wt_proxy_new(wtself_t *self);
int wt_proxy_add_policy(wtself_t *self, const char *name, zwscvt_cb cb);
int service_timer_callback(zloop_t *loop, int  timer_id, void *context);
int register_zmq_poller(zws_session_t *wss);
const char *zwsmimetype(const char *ext);

// webtalk_jsonpolicy.cc:
int json_policy(wtself_t *self, zws_session_t *wss, zwscb_type type);

// webtalk_defaultpolicy.cc:
int default_policy(wtself_t *self, zws_session_t *wss, zwscb_type type);


// webtalk_plugin.cc:
int wt_add_plugin(wtself_t *self, const char *sopath);

// webtalk_initproto.cc
extern struct libwebsocket_protocols *protocols;
void init_protocols(void);
