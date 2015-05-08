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

// msgbusd acts as a switching fabric between RT and non-RT actors.
// see https://github.com/mhaberler/messagebus for an overview.

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <sys/signalfd.h>
#include <errno.h>
#include <getopt.h>
#include <syslog.h>
#include <uuid/uuid.h>
#include <czmq.h>
#include <syslog_async.h>

#include <string>
#include <unordered_set>

#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

#include <rtapi.h>
#include <hal.h>
#include <hal_priv.h>
#include <hal_ring.h>
#include <setup_signals.h>
#include <mk-zeroconf.hh>
#include <select_interface.h>
#include <inifile.h>
#include <inihelp.hh>

#include <machinetalk/generated/message.pb.h>
using namespace google::protobuf;

#include "messagebus.hh"
#include "rtproxy.hh"

#define MESSAGEBUS_VERSION 1 // protocol version

typedef std::unordered_set<std::string> actormap_t;
typedef actormap_t::iterator actormap_iterator;

static const char *option_string = "hI:S:dr:c:tp:";
static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"ini", required_argument, 0, 'I'},     // default: getenv(INI_FILE_NAME)
    {"section", required_argument, 0, 'S'},
    {"debug", no_argument, 0, 'd'},
    {"textreply", no_argument, 0, 't'},
    {"response", required_argument, 0, 'r'},
    {"cmd", required_argument, 0, 'c'},
    {"rtproxy", required_argument, 0, 'p'},
    {0,0,0,0}
};


static const char *inifile;
static const char *section = "MSGBUS";

// inproc variant for comms with RT proxy threads (not announced)
const char *proxy_cmd_uri = "inproc://messagebus.cmd";
const char *proxy_response_uri = "inproc://messagebus.response";

const char *progname = "";

static int debug;
int comp_id;
int instance_id; // HAL instance

static int textreplies; // return error messages in strings instead of protobuf Containers
static int signal_fd;

typedef struct {
    void *response;
    void *cmd;
    uuid_t process_uuid;
    char process_uuid_str[40];
    char *service_uuid;
    const char *ipaddr;
    const char *interface;
    int remote;
    unsigned ifIndex;
    const char *cmd_uri,*response_uri;
    const char *command_dsn, *response_dsn;
    int command_port, response_port;
    actormap_t *cmd_subscribers;
    actormap_t *response_subscribers;
    int comp_id;
    zctx_t *context;
    zloop_t *loop;
    register_context_t *command_publisher;
    register_context_t *response_publisher;
    bool interrupted;
    FILE *inifp;
    AvahiCzmqPoll *av_loop;
} msgbusd_self_t;


static int handle_xpub_in(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    msgbusd_self_t *self = (msgbusd_self_t *) arg;
    actormap_t *map;
    const char *rail;
    char *data, *topic;
    int retval;

    if (poller->socket == self->cmd) {
	map = self->cmd_subscribers;
	rail = "cmd";
    } else {
	map = self->response_subscribers;
	rail = "response";
    }

    zmsg_t *msg = zmsg_recv(poller->socket);
    size_t nframes = zmsg_size( msg);

    if (nframes == 1) {
	// likely a subscribe/unsubscribe message
	// a proper message needs at least three parts: src, dest, contents
	    zframe_t *f = zmsg_pop(msg);
	data = (char *) zframe_data(f);
	assert(data);
	topic = data + 1;

	switch (*data) {
	case '\001':
	    map->insert(topic);
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s: rail %s: %s subscribed\n",
			    progname, rail, topic);
	    break;

	case '\000':
	    map->erase(topic);
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s: rail %s: %s unsubscribed\n",
			    progname, rail, topic);
	    break;

	default:
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: rail %s: invalid frame (tag=%d topic=%s)",
			    progname, rail, *data, topic);
	}
	zframe_destroy(&f);
	return 0;
    }
    if (nframes > 2) {
	// forward
	char *from  = zmsg_popstr(msg);
	char *to  = zmsg_popstr(msg);

	if (map->find(to) == map->end()) {
	    char errmsg[100];
	    snprintf(errmsg, sizeof(errmsg), "rail %s: no such destination: %s", rail, to);
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: %s\n", progname,errmsg);

	    if (poller->socket == self->cmd) {
		// command was directed to non-existent actor
		// we wont get a reply from a non-existent actor
		// so send error message on response rail instead:

		retval = zstr_sendm(self->response, from);  // originator
		assert(retval == 0);
		retval = zstr_sendm(self->response, to);    // destination
		assert(retval == 0);

		if (textreplies) {
		    assert(zstr_send(self->response, errmsg) == 0);
		    assert(retval == 0);
		} else {
		    pb::Container c;
		    c.set_type(pb::MT_MESSAGEBUS_NO_DESTINATION);
		    c.set_name(to);
		    c.add_note(errmsg);
		    zframe_t *errorframe = zframe_new(NULL, c.ByteSize());
		    c.SerializeWithCachedSizesToArray(zframe_data(errorframe));
		    retval = zframe_send(&errorframe, self->response, 0);
		    assert(retval == 0);
		}
		zmsg_destroy(&msg);
	    } // else: response to non-existent actor is dropped
	} else {
	    // forward
	    if (debug)
		rtapi_print_msg(RTAPI_MSG_ERR, "forward: %s->%s:\n", from,to);

	    zstr_sendm(poller->socket, to);          // topic
	    zstr_sendm(poller->socket, from);        // destination
	    zmsg_send(&msg, poller->socket);
	}
	free(from);
	free(to);

    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: rail %s: short message (%zu frames)",
			progname, rail, nframes);
	zmsg_dump_to_stream(msg, stderr);
	zmsg_destroy(&msg);
    }
    return 0;
}


static int handle_signal(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    msgbusd_self_t *self = (msgbusd_self_t *)arg;
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(poller->fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
	perror("read");
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: signal %d - '%s' received\n",
			progname, fdsi.ssi_signo, strsignal(fdsi.ssi_signo));
    self->interrupted = true;
    return -1; // exit reactor with -1
}



static int mainloop(msgbusd_self_t *self)
{
    int retval;

    zmq_pollitem_t signal_poller =        { 0, signal_fd, ZMQ_POLLIN };
    zmq_pollitem_t cmd_poller =           { self->cmd, 0, ZMQ_POLLIN };
    zmq_pollitem_t response_poller =      { self->response, 0, ZMQ_POLLIN };

    zloop_poller(self->loop, &signal_poller,   handle_signal,  self);
    zloop_poller(self->loop, &cmd_poller,      handle_xpub_in, self);
    zloop_poller(self->loop, &response_poller, handle_xpub_in, self);

    do {
	retval = zloop_start(self->loop);
    } while  (!(retval || self->interrupted));

    rtapi_print_msg(RTAPI_MSG_INFO,
		    "%s: exiting mainloop (%s)\n",
		    progname,
		    self->interrupted ? "interrupted": "reactor exited");

    return 0;
}

static int zmq_setup(msgbusd_self_t *self)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // suppress default handling of signals in zctx_new()
    // since we're using signalfd()
    zsys_handler_set(NULL);

    self->context = zctx_new ();
    assert(self->context);

    zctx_set_linger (self->context, 0);

    self->cmd = zsocket_new (self->context, ZMQ_XPUB);
    assert(self->cmd);
    zsocket_set_xpub_verbose (self->cmd, 1);
    self->command_port = zsocket_bind(self->cmd, self->cmd_uri);
    assert(self->command_port > -1);

    self->command_dsn = zsocket_last_endpoint (self->cmd);

    assert(zsocket_bind(self->cmd, proxy_cmd_uri) > -1);

    self->response = zsocket_new (self->context, ZMQ_XPUB);
    assert(self->response);
    zsocket_set_xpub_verbose (self->response, 1);
    self->response_port = zsocket_bind(self->response, self->response_uri);
    assert(self->response_port >  -1);
    self->response_dsn = zsocket_last_endpoint (self->response);

    assert(zsocket_bind(self->response, proxy_response_uri) > -1);

    usleep(200 *1000); // avoid slow joiner syndrome

    self->cmd_subscribers = new actormap_t();
    self->response_subscribers = new actormap_t();

    self->loop = zloop_new();
    assert(self->loop);
    zloop_set_verbose (self->loop, debug);

    // register Avahi poll adapter
    if (self->remote && !(self->av_loop = avahi_czmq_poll_new(self->loop))) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: zeroconf: Failed to create avahi event loop object.",
			progname);
	return -1;
    }
    return 0;
}

static int mb_zeroconf_announce(msgbusd_self_t *self)
{
    char name[100];
    char puuid[40];
    char hostname[PATH_MAX];
    char uri[PATH_MAX];

    uuid_unparse(self->process_uuid, puuid);
    if (gethostname(hostname, sizeof(hostname)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: gethostname() failed ?! %s\n",
			progname, strerror(errno));
	return -1;
    }
    strtok(hostname, "."); // get rid of the domain name

    if (self->remote)
	snprintf(uri,sizeof(uri), "tcp://%s.local.:%d",hostname, self->command_port);

    snprintf(name,sizeof(name), "Messagebus command service on %s.local pid %d",
	     hostname, getpid());
    self->command_publisher = zeroconf_service_announce(name,
							MACHINEKIT_DNSSD_SERVICE_TYPE,
							MSGBUSCMD_DNSSD_SUBTYPE,
							self->command_port,
							self->remote ? uri :
							(char *)self->command_dsn,
							self->service_uuid,
							self->process_uuid_str,
							"mbcmd",
							NULL,
							self->av_loop);
    if (self->command_publisher == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: failed to start zeroconf Messagebus command publisher\n",
			progname);
	return -1;
    }

    if (self->remote)
	snprintf(uri,sizeof(uri), "tcp://%s.local.:%d",hostname, self->response_port);
    snprintf(name,sizeof(name), "Messagebus response service on %s.local pid %d",
	     hostname, getpid());

    self->response_publisher = zeroconf_service_announce(name,
							 MACHINEKIT_DNSSD_SERVICE_TYPE,
							 MSGBUSRESP_DNSSD_SUBTYPE,
							 self->response_port,
							 self->remote ? uri :
							 (char *)self->response_dsn,
							 self->service_uuid,
							 self->process_uuid_str,
							 "mbresp",
							 NULL,
							 self->av_loop);
    if (self->response_publisher == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: failed to start zeroconf Messagebus response publisher\n",
			progname);
	return -1;
    }
    return 0;
}

int
mb_zeroconf_withdraw(msgbusd_self_t *self)
{
    if (self->command_publisher)
	zeroconf_service_withdraw(self->command_publisher);
    if (self->response_publisher)
	zeroconf_service_withdraw(self->response_publisher);

    // deregister poll adapter
    if (self->av_loop)
        avahi_czmq_poll_free(self->av_loop);
    return 0;
}


static rtproxy_t echo, demo; //, too;


static int rtproxy_setup(msgbusd_self_t *self)
{
    echo.flags = ACTOR_ECHO|TRACE_TO_RT;
    echo.name = "echo";
    echo.pipe = zthread_fork (self->context, rtproxy_thread, &echo);
    assert (echo.pipe);

    demo.flags = ACTOR_RESPONDER|TRACE_FROM_RT|TRACE_TO_RT|DESERIALIZE_TO_RT|SERIALIZE_FROM_RT;
    demo.state = IDLE;
    demo.min_delay = 2;   // msec
    demo.max_delay = 200; // msec

    demo.name = "demo";
    demo.to_rt_name = "mptx.0.in";
    demo.from_rt_name = "mptx.0.out";
    demo.min_delay = 2;   // msec
    demo.max_delay = 200; // msec
    demo.pipe = zthread_fork (self->context, rtproxy_thread, &demo);
    assert (demo.pipe);

    // too.flags = ACTOR_RESPONDER|ACTOR_TRACE;
    // too.state = IDLE;
    // too.min_delay = 2;   // msec
    // too.max_delay = 200; // msec
    // too.name = "mptx";
    // too.to_rt_name = "mptx.0.in";
    // too.from_rt_name = "mptx.0.out";
    // too.pipe = zthread_fork (self->context, rtproxy_thread, &too);
    // assert (too.pipe);

    return 0;
}

static int hal_setup(msgbusd_self_t *self)
{
    if ((comp_id = hal_init(progname)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init(%s) failed: HAL error code=%d\n",
			progname, progname, comp_id);
	return -1;
    }
    hal_ready(comp_id);

    {
	int zmajor, zminor, zpatch, pbmajor, pbminor, pbpatch;
	zmq_version (&zmajor, &zminor, &zpatch);
	pbmajor = GOOGLE_PROTOBUF_VERSION / 1000000;
	pbminor = (GOOGLE_PROTOBUF_VERSION / 1000) % 1000;
	pbpatch = GOOGLE_PROTOBUF_VERSION % 1000;
	rtapi_print_msg(RTAPI_MSG_DBG,
	       "%s: startup ØMQ=%d.%d.%d protobuf=%d.%d.%d",
	       progname, zmajor, zminor, zpatch, pbmajor, pbminor, pbpatch);

	rtapi_print_msg(RTAPI_MSG_DBG, "%s: talking Messagebus on cmd='%s' response='%s'",
			progname, self->command_dsn, self->response_dsn);

    }
    return 0;
}

static void sigaction_handler(int sig, siginfo_t *si, void *uctx)
{
    syslog_async(LOG_ERR,"signal %d - '%s' received, dumping core (current dir=%s)",
		    sig, strsignal(sig), get_current_dir_name());
    closelog_async(); // let syslog_async drain
    sleep(1);
    // reset handler for current signal to default
    signal(sig, SIG_DFL);
    // and re-raise so we get a proper core dump and stacktrace
    kill(getpid(), sig);
    sleep(1);
}

// pull global values from MACHINEKIT_INI
static int
read_global_config(msgbusd_self_t *self)
{
    const char *s, *mkinifile;
    const char *mkini = "MACHINEKIT_INI";
    FILE *inifp;
    uuid_t uutmp;

    if ((mkinifile = getenv(mkini)) == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: FATAL - '%s' missing in environment\n",
		     progname, mkini);
	return -1;
    }
    if ((inifp = fopen(mkinifile,"r")) == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: cant open inifile '%s'\n",
		     progname,mkinifile);
    }

    str_inidefault(&self->service_uuid, inifp, "MKUUID", "MACHINEKIT");

    if (self->service_uuid == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		     "%s: no service UUID (-R <uuid> or MACHINEKIT_INI [MACHINEKIT]MKUUID) present\n",
		     progname);
	    return -1;
    }
    if (uuid_parse(self->service_uuid, uutmp)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		     "%s: service UUID: syntax error: '%s'",
		     progname,self->service_uuid);
	return -1;
    }
    iniFindInt(inifp, "REMOTE", "MACHINEKIT", &self->remote);
    if (self->remote) {
	if ((s = iniFind(inifp, "INTERFACES", "MACHINEKIT"))) {

	    char ifname[LINELEN], ip[LINELEN];

	    // pick a preferred interface
	    if (parse_interface_prefs(s,  ifname, ip, &self->ifIndex) == 0) {
		self->interface = strdup(ifname);
		self->ipaddr = strdup(ip);
		syslog_async(LOG_INFO, "%s %s: using preferred interface %s/%s\n",
			     progname, mkini,
			     self->interface, self->ipaddr);
	    } else {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s %s: INTERFACES='%s'"
			     " - cant determine preferred interface, using %s/%s\n",
			     progname, mkini, s,
			     self->interface, self->ipaddr);
	    }
	}
    }
    fclose(inifp);
    return 0;
}

static int read_config(msgbusd_self_t *self, const char *inifile)
{
    const char *s;

    if (inifile && (self->inifp = fopen(inifile,"r")) == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: cant open inifile '%s'\n",
			progname, inifile);
    } else {
	iniFindInt(self->inifp, "DEBUG", section, &debug);
	iniFindInt(self->inifp, "TEXTREPLIES", section, &textreplies);

	if ((self->cmd_uri == NULL) && (s = iniFind(self->inifp, "CMD", section)))
	    self->cmd_uri = strdup(s);

	if ((self->response_uri == NULL) && (s = iniFind(self->inifp, "RESPONSE", section)))
	    self->response_uri = strdup(s);
    }

    // finalize the URI's if ephemeral to be used
    char uri[100];
    if (self->cmd_uri == NULL) {

	if (self->remote)
	    snprintf(uri, sizeof(uri), "tcp://%s:*",self->ipaddr);
	else
	    snprintf(uri, sizeof(uri), ZMQIPC_FORMAT,
		     RUNDIR, instance_id, "mbcmd", self->service_uuid);
	self->cmd_uri = strdup(uri);
    }
    if (self->response_uri == NULL) {

	if (self->remote)
	    snprintf(uri, sizeof(uri), "tcp://%s:*",self->ipaddr);
	else
	    snprintf(uri, sizeof(uri), ZMQIPC_FORMAT,
		     RUNDIR, instance_id, "mbresp", self->service_uuid);
	self->response_uri = strdup(uri);
    }
    return 0;
}

static int parse_proxy(const char *s)
{
    return 0;
}


static void usage(void) {
    printf("Usage:  messagebus [options]\n");
    printf("This is a userspace HAL program, typically loaded "
	   "using the halcmd \"loadusr\" command:\n"
	   "    loadusr messagebus [options]\n"
	   "Options are:\n"
	   "-I or --ini <inifile>\n"
	   "    Use <inifile> (default: take ini filename from environment"
	   " variable INI_FILE_NAME)\n"
	   "-S or --section <section-name> (default 8)\n"
	   "    Read parameters from <section_name> (default 'VFS11')\n"
	   "-d --debug\n"
	   "    increase debug level.\n"
	   "-t or --textreply\n"
	   "    send error messages on response-out as strings (default protobuf)\n"
	   "-d or --debug\n"
	   "    Turn on event debugging messages.\n");
}


int main (int argc, char *argv[])
{
    int opt, retval;
    int logopt = LOG_LOCAL1;
    msgbusd_self_t self = {0};

    self.ipaddr = "127.0.0.1";
    self.interface = "";
    progname = argv[0];

    inifile = getenv("INI_FILE_NAME");

    while ((opt = getopt_long(argc, argv, option_string,
			      long_options, NULL)) != -1) {
	switch(opt) {
	case 'd':
	    debug++;
	    break;
	case 't':
	    textreplies++;
	    break;
	case 'S':
	    section = optarg;
	    break;
	case 'I':
	    inifile = optarg;
	    break;
	case 'r':
	    self.response_uri = optarg;
	    break;
	case 'c':
	    self.cmd_uri = optarg;
	    break;
	case 'p':
	    parse_proxy(optarg);
	    break;

	case 'h':
	default:
	    usage();
	    exit(0);
	}
    }
    openlog("", LOG_NDELAY , logopt);

    if (read_global_config(&self))
	exit(1);

    if (read_config(&self, inifile))
	exit(1);

    uuid_generate_time(self.process_uuid);
    uuid_unparse(self.process_uuid, self.process_uuid_str);

    retval = zmq_setup(&self);
    if (retval) exit(retval);

    retval = hal_setup(&self);
    if (retval) exit(retval);

    signal_fd = setup_signals(sigaction_handler, SIGINT, SIGQUIT, SIGKILL, SIGTERM, -1);
    if (signal_fd < 0)
	exit(1);

    if (self.remote) {
	retval = mb_zeroconf_announce(&self);
	if (retval) exit(retval);
    }
    retval = rtproxy_setup(&self);
    if (retval) exit(retval);

    mainloop(&self);

    if (self.remote)
	mb_zeroconf_withdraw(&self);

    // shutdown zmq context
    zctx_destroy (&self.context);

    if (comp_id)
	hal_exit(comp_id);
    exit(0);
}
