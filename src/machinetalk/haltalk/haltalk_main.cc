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

// haltalk:
//   1. reports the status of HAL signals aggregated into
//      HAL groups via the Status Tracking protocol.
//
//   2. implements the HALRcomp protocol for remote HAL components.
//
//   3. Implements remote remote set/get operations for pins and signals.
//
//   4. Reports a HAL instance through the DESCRIBE operation.
//
//   5. Announce services via zeroconf.
//
//   6. [notyet] optional may bridge to a remote HAL instance through a remote component.

#include "haltalk.hh"
#include <setup_signals.h>
#include <mk-service.hh>
#include <mk-backtrace.h>

int print_container; // see pbutil.cc

// configuration defaults
static htconf_t conf = {
    "",
    NULL,
    "HALTALK",
    "haltalk",
#ifdef NOTYET
    NULL,
    NULL,
    NULL,
    -1,
#endif
    0,    // debug
    100,  // default_group_timer
    100,  // odefault_rcomp_timer
    2000, // keepalive
    true, // trap_signals
};


static int
handle_signal(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    htself_t *self = (htself_t *)arg;
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(self->signal_fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
	perror("read");
    }
    switch (fdsi.ssi_signo) {
    default:
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: signal %d - '%s' received\n",
			self->cfg->progname,
			fdsi.ssi_signo,
			strsignal(fdsi.ssi_signo));
    }
    self->interrupted = true;
    return -1; // exit reactor with -1
}

static int
handle_keepalive_timer(zloop_t *loop, int timer_id, void *arg)
{
    htself_t *self = (htself_t *) arg;
    ping_comps(self);
    ping_groups(self);
    return 0;
}

static int
mainloop( htself_t *self)
{
    int retval;
    zloop_t *loop = self->netopts.z_loop;

    zmq_pollitem_t signal_poller = { 0, self->signal_fd, ZMQ_POLLIN };
    zmq_pollitem_t group_poller =  { self->mksock[SVC_HALGROUP].socket, 0, ZMQ_POLLIN };
    zmq_pollitem_t rcomp_poller =  { self->mksock[SVC_HALRCOMP].socket, 0, ZMQ_POLLIN };
    zmq_pollitem_t cmd_poller =  { self->mksock[SVC_HALRCMD].socket, 0, ZMQ_POLLIN };

    zloop_set_verbose (loop, self->cfg->debug > 8);

    if (self->cfg->trap_signals)
	zloop_poller(loop, &signal_poller, handle_signal, self);
    zloop_poller(loop, &group_poller,  handle_group_input, self);
    zloop_poller(loop, &rcomp_poller,  handle_rcomp_input, self);
    zloop_poller(loop, &cmd_poller,    handle_command_input, self);
    if (self->cfg->keepalive_timer)
	zloop_timer(loop, self->cfg->keepalive_timer, 0,
		    handle_keepalive_timer, (void *) self);
    do {
	retval = zloop_start(loop);
    } while  (!(retval || self->interrupted));

    rtapi_print_msg(RTAPI_MSG_INFO,
		    "%s: exiting mainloop (%s)\n",
		    self->cfg->progname,
		    self->interrupted ? "interrupted": "reactor exited");
    return 0;
}

static void btprint(const char *prefix, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsyslog_async(LOG_ERR, fmt, args);
}

static void sigaction_handler(int sig, siginfo_t *si, void *uctx)
{
    syslog_async(LOG_ERR,"signal %d - '%s' received, dumping core (current dir=%s)",
		    sig, strsignal(sig), get_current_dir_name());

    backtrace("", "haltalk", btprint, 3);

    closelog_async(); // let syslog_async drain
    sleep(1);
    // reset handler for current signal to default
    signal(sig, SIG_DFL);
    // and re-raise so we get a proper core dump and stacktrace
    kill(getpid(), sig);
    sleep(1);
}

static int
zmq_init(htself_t *self)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (conf.trap_signals) {
	self->signal_fd = setup_signals(sigaction_handler, SIGINT, SIGQUIT, SIGKILL, SIGTERM, -1);
	assert(self->signal_fd > -1);
    }

    // suppress default handling of signals in zctx_new()
    // since we're using signalfd()
    // must happen before zctx_new()
    zsys_handler_set(NULL);

    mk_netopts_t *np = &self->netopts;

    np->z_context = zctx_new ();
    assert(np->z_context);

    np->z_loop = zloop_new();
    assert (np->z_loop);

    np->rundir = RUNDIR;
    np->rtapi_instance = rtapi_instance;

    np->av_loop = avahi_czmq_poll_new(np->z_loop);
    assert(np->av_loop);

    mk_socket_t *ms = &self->mksock[SVC_HALGROUP];
    ms->dnssd_subtype = HALGROUP_DNSSD_SUBTYPE;
    ms->tag = "halgroup";
    ms->socket = zsocket_new (self->netopts.z_context, ZMQ_XPUB);
    assert(ms->socket);
    zsocket_set_linger(ms->socket, 0);
    zsocket_set_xpub_verbose(ms->socket, 1);
    if (mk_bindsocket(np, ms))
	return -1;
    assert(ms->port > -1);
    if (mk_announce(np, ms, "HAL Group service", NULL))
	return -1;
    rtapi_print_msg(RTAPI_MSG_DBG, "%s: talking HALGroup on '%s'",
		    conf.progname, ms->announced_uri);


    ms = &self->mksock[SVC_HALRCOMP];
    ms->dnssd_subtype = HALRCOMP_DNSSD_SUBTYPE;
    ms->tag = "halrcomp";
    ms->socket = zsocket_new (self->netopts.z_context, ZMQ_XPUB);
    assert(ms->socket);
    zsocket_set_linger(ms->socket, 0);
    zsocket_set_xpub_verbose(ms->socket, 1);
    if (mk_bindsocket(np, ms))
	return -1;
    assert(ms->port > -1);
    if (mk_announce(np, ms, "HAL Rcomp service", NULL))
	return -1;
    rtapi_print_msg(RTAPI_MSG_DBG, "%s: talking HALRcomp on '%s'",
		    conf.progname, ms->announced_uri);


    ms = &self->mksock[SVC_HALRCMD];
    ms->dnssd_subtype = HALRCMD_DNSSD_SUBTYPE;
    ms->tag = "halrcmd";
    ms->socket = zsocket_new (self->netopts.z_context, ZMQ_ROUTER);
    assert(ms->socket);
    zsocket_set_linger(ms->socket, 0);
    zsocket_set_identity (ms->socket, self->cfg->modname);
    if (mk_bindsocket(np, ms))
	return -1;
    assert(ms->port > -1);
    if (mk_announce(np, ms, "HAL Rcommand service", NULL))
	return -1;
    rtapi_print_msg(RTAPI_MSG_DBG, "%s: talking HALComand on '%s'",
		    conf.progname, ms->announced_uri);

    usleep(200 *1000); // avoid slow joiner syndrome
    return 0;
}

int
ht_zeroconf_withdraw(htself_t *self)
{
    for (size_t i = 0; i < NSVCS; i++)
	mk_withdraw(&self->mksock[i]);

    // deregister poll adapter
    if (self->netopts.av_loop)
        avahi_czmq_poll_free(self->netopts.av_loop);
    return 0;
}

static int
hal_setup(htself_t *self)
{
    int retval;

    if ((self->comp_id = hal_init(self->cfg->modname)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init(%s) failed: HAL error code=%d\n",
			self->cfg->progname, self->cfg->modname, self->comp_id);
	return self->comp_id;
    }
    hal_ready(self->comp_id);

    int major, minor, patch;
    zmq_version (&major, &minor, &patch);

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "%s: startup ØMQ=%d.%d.%d czmq=%d.%d.%d protobuf=%d.%d.%d uuid=%s\n",
		    self->cfg->progname, major, minor, patch,
		    CZMQ_VERSION_MAJOR, CZMQ_VERSION_MINOR,CZMQ_VERSION_PATCH,
		    GOOGLE_PROTOBUF_VERSION / 1000000,
		    (GOOGLE_PROTOBUF_VERSION / 1000) % 1000,
		    GOOGLE_PROTOBUF_VERSION % 1000,
		    self->netopts.service_uuid);

    retval = scan_groups(self);
    if (retval < 0) return retval;
    retval = scan_comps(self);
    if (retval < 0) return retval;
    return 0;
}

static int
hal_cleanup(htself_t *self)
{
    int retval;
    retval = release_comps(self);
    retval = release_groups(self);

    if (self->comp_id)
	hal_exit(self->comp_id);
    return retval;
}

// pull haltalk-specific port values from MACHINEKIT_INI
// the other network-related values are read by mk_getnetopts()
// and mk_bindsocket()
static int
read_machinekit_ini(htself_t *s)
{
    iniFindInt(s->netopts.mkinifp, "GROUP_STATUS_PORT",
	       "MACHINEKIT", &s->mksock[SVC_HALGROUP].port);
    iniFindInt(s->netopts.mkinifp, "RCOMP_STATUS_PORT",
	       "MACHINEKIT", &s->mksock[SVC_HALRCOMP].port);
    iniFindInt(s->netopts.mkinifp, "COMMAND_PORT",
	       "MACHINEKIT", &s->mksock[SVC_HALRCMD].port);
    return 0;
}

// getenv("INI_FILE_NAME") or commandline - per config
static int
read_config(htself_t *self)
{
    FILE *inifp = NULL;

    if (self->cfg->inifile && ((inifp = fopen(self->cfg->inifile,"r")) == NULL)) {
	syslog_async(LOG_ERR, "%s: cant open inifile '%s'\n",
		     self->cfg->progname, self->cfg->inifile);
    }

#ifdef NOTYET
    // bridge: TBD
    const char *s;

    if (inifp) {
	if ((s = iniFind(inifp, "BRIDGE_COMP", conf->section)))
	    conf->bridgecomp = strdup(s);
	if ((s = iniFind(inifp, "BRIDGE_COMMAND_URI", conf->section)))
	    conf->bridgecomp_cmduri = strdup(s);
	if ((s = iniFind(inifp, "BRIDGE_STATUS_URI", conf->section)))
	    conf->bridgecomp_updateuri = strdup(s);
	iniFindInt(inifp, "BRIDGE_TARGET_INSTANCE", conf->section, &conf->bridge_target_instance);
	iniFindInt(inifp, "GROUPTIMER", conf->section, &conf->default_group_timer);
	iniFindInt(inifp, "RCOMPTIMER", conf->section, &conf->default_rcomp_timer);
	iniFindInt(inifp, "KEEPALIVETIMER", conf->section, &conf->keepalive_timer);
	if (!conf->debug)
	    iniFindInt(inifp, "DEBUG", conf->section, &conf->debug);
    }
#endif
    if (inifp)
	fclose(inifp);
    return 0;
}

static void
usage(void)
{
    printf("Usage:  haltalk [options]\n");
    printf("This is a userspace HAL program, typically loaded "
	   "using the halcmd \"loadusr\" command:\n"
	   "    loadusr haltalk [options]\n"
	   "Options are:\n"
	   "-I or --ini <inifile>\n"
	   "    Use <inifile> (default: take ini filename from environment"
	   " variable INI_FILE_NAME)\n"
	   "-S or --section <section-name> (default 8)\n"
	   "    Read parameters from <section_name> (default 'HALTALK')\n"
	   "-u or --uri <uri>\n"
	   "    zeroMQ URI for status reporting socket\n"
	   "-m or --rtapi-msg-level <level>\n"
	   "    set the RTAPI message level.\n"
	   "-t or --timer <msec>\n"
	   "    set the default group scan timer (100mS).\n"
	   "-d or --debug\n"
	   "    Turn on event debugging messages.\n");
}

static const char *option_string = "hI:S:d:t:T:R:sK:G";
static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"ini", required_argument, 0, 'I'},     // default: getenv(INI_FILE_NAME)
    {"section", required_argument, 0, 'S'},
    {"debug", required_argument, 0, 'd'},
    {"gtimer", required_argument, 0, 't'},
    {"ctimer", required_argument, 0, 'T'},
    {"keepalive", required_argument, 0, 'K'},
    {"svcuuid", required_argument, 0, 'R'},
    {"stderr",  no_argument,        0, 's'},
    {"nosighdlr",   no_argument,    0, 'G'},
    {0,0,0,0}
};

int main (int argc, char *argv[])
{
    int opt, retval;

    conf.progname = argv[0];
    conf.inifile = getenv("INI_FILE_NAME");
    int logopt = LOG_NDELAY;

    htself_t self = {0};
    self.cfg = &conf;
    for (size_t i = 0; i < NSVCS; i++)
	self.mksock[i].port = -1;

    while ((opt = getopt_long(argc, argv, option_string,
			      long_options, NULL)) != -1) {
	switch(opt) {
	case 'd':
	    conf.debug = atoi(optarg);
	    break;
	case 'S':
	    conf.section = optarg;
	    break;
	case 'I':
	    conf.inifile = optarg;
	    break;
	case 't':
	    conf.default_group_timer = atoi(optarg);
	    break;
	case 'T':
	    conf.default_rcomp_timer = atoi(optarg);
	    break;
	case 'K':
	    conf.keepalive_timer = atoi(optarg);
	    break;
#ifdef NOTYET
	case 'b':
	    conf.bridgecomp = optarg;
	    break;
	case 'C':
	    conf.bridgecomp_cmduri = optarg;
	    break;
	case 'i':
	    conf.bridge_target_instance = atoi(optarg);
	    break;
	case 'U':
	    conf.bridgecomp_updateuri = optarg;
	    break;
#endif
	case 'R':
	    self.netopts.service_uuid = optarg;
	    break;
	case 'G':
	    conf.trap_signals = false;
	    break;
	case 's':
	    logopt |= LOG_PERROR;
	    break;
	case 'h':
	default:
	    usage();
	    exit(0);
	}
    }
    openlog_async(conf.progname, logopt , SYSLOG_FACILITY);
    backtrace_init(argv[0]);

    // ease debugging with gdb - disable all signal handling
    if (getenv("NOSIGHDLR") != NULL)
	conf.trap_signals = false;

    self.pid = getpid();

    // generic binding & announcement parameters
    // from $MACHINEKIT_INI
    self.netopts.rundir = RUNDIR;
    if (mk_getnetopts(&self.netopts))
	exit(1);

    if (read_config(&self))
	exit(1);

    // webtalk-specific port number from $MACHINEKIT_INI
    if (read_machinekit_ini(&self))
	exit(1);

    print_container = self.cfg->debug & 1; // log sent protobuf messages to stderr if debug & 1

    retval = hal_setup(&self);
    if (retval) exit(retval);

    retval = zmq_init(&self);
    if (retval) exit(retval);

#ifdef NOTYET
    retval = bridge_init(&self);
    if (retval) exit(retval);
#endif

    mainloop(&self);

    ht_zeroconf_withdraw(&self);
    // probably should run zloop here until deregister complete

    // shutdown zmq context
    zctx_destroy(&self.netopts.z_context);

    hal_cleanup(&self);

    exit(0);
}
