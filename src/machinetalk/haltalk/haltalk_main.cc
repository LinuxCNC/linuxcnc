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
#include <select_interface.h>
#include <mk-backtrace.h>

int print_container; // see pbutil.cc

// configuration defaults
static htconf_t conf = {
    "",
    NULL,
    "HALTALK",
    "haltalk",
    "localhost",
    "127.0.0.1",
    "tcp://%s:*", // as per preferred interface, use ephemeral port
    "tcp://%s:*",
    "tcp://%s:*",
    NULL,
    NULL,
    NULL,
    NULL,
    -1,
    0,
    0,
    100,
    100,
    2000, // keepalive
    0,
    NULL,
    0,
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

    zmq_pollitem_t signal_poller = { 0, self->signal_fd, ZMQ_POLLIN };
    zmq_pollitem_t group_poller =  { self->z_halgroup, 0, ZMQ_POLLIN };
    zmq_pollitem_t rcomp_poller =  { self->z_halrcomp, 0, ZMQ_POLLIN };
    zmq_pollitem_t cmd_poller   =  { self->z_halrcmd,      0, ZMQ_POLLIN };

    zloop_set_verbose (self->z_loop, self->cfg->debug > 8);

    if (self->cfg->trap_signals)
	zloop_poller(self->z_loop, &signal_poller, handle_signal, self);
    zloop_poller(self->z_loop, &group_poller,  handle_group_input, self);
    zloop_poller(self->z_loop, &rcomp_poller,  handle_rcomp_input, self);
    zloop_poller(self->z_loop, &cmd_poller,    handle_command_input, self);
    if (self->cfg->keepalive_timer)
	zloop_timer(self->z_loop, self->cfg->keepalive_timer, 0,
		    handle_keepalive_timer, (void *) self);
    do {
	retval = zloop_start(self->z_loop);
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

    self->z_context = zctx_new ();
    assert(self->z_context);

    self->z_loop = zloop_new();
    assert (self->z_loop);

    self->z_halgroup = zsocket_new (self->z_context, ZMQ_XPUB);
    assert(self->z_halgroup);
    zsocket_set_linger (self->z_halgroup, 0);
    zsocket_set_xpub_verbose (self->z_halgroup, 1);
    self->z_group_port = zsocket_bind(self->z_halgroup, self->cfg->halgroup);
    assert (self->z_group_port > -1);
    self->z_halgroup_dsn = zsocket_last_endpoint (self->z_halgroup);

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: talking HALGroup on '%s'",
		    conf.progname, conf.remote ? self->z_halgroup_dsn : self->cfg->halgroup);

    self->z_halrcomp = zsocket_new (self->z_context, ZMQ_XPUB);
    assert(self->z_halrcomp);
    zsocket_set_linger (self->z_halrcomp, 0);
    zsocket_set_xpub_verbose (self->z_halrcomp, 1);
    self->z_rcomp_port = zsocket_bind(self->z_halrcomp, self->cfg->halrcomp);
    assert (self->z_rcomp_port > -1);
    self->z_halrcomp_dsn = zsocket_last_endpoint (self->z_halrcomp);

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: talking HALRcomp on '%s'",
		    conf.progname, conf.remote ? self->z_halrcomp_dsn:self->cfg->halrcomp);


    self->z_halrcmd = zsocket_new (self->z_context, ZMQ_ROUTER);
    assert(self->z_halrcmd);
    zsocket_set_linger (self->z_halrcmd, 0);
    zsocket_set_identity (self->z_halrcmd, self->cfg->modname);

    self->z_halrcmd_port = zsocket_bind(self->z_halrcmd, self->cfg->command);
    assert (self->z_halrcmd_port > -1);
    self->z_halrcmd_dsn = zsocket_last_endpoint (self->z_halrcmd);

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: talking HALComannd on '%s'",
		    conf.progname, conf.remote ? self->z_halrcmd_dsn : self->cfg->command);

    // register Avahi poll adapter
    if (!(self->av_loop = avahi_czmq_poll_new(self->z_loop))) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: zeroconf: Failed to create avahi event loop object.",
			conf.progname);
	return -1;
    }

    usleep(200 *1000); // avoid slow joiner syndrome
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
		    conf.service_uuid);

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

// pull global values from MACHINEKIT_INI
static int
read_global_config(htconf_t *conf)
{
    const char *s, *inifile;
    const char *mkini = "MACHINEKIT_INI";
    FILE *inifp;
    uuid_t uutmp;

    if ((inifile = getenv(mkini)) == NULL) {
	syslog_async(LOG_ERR, "%s: FATAL - '%s' missing in environment\n",
		     conf->progname, mkini);
	return -1;
    }
    if ((inifp = fopen(inifile,"r")) == NULL) {
	syslog_async(LOG_ERR, "%s: cant open inifile '%s'\n",
		     conf->progname,inifile);
    }
    if (conf->service_uuid == NULL) {
	if ((s = iniFind(inifp, "MKUUID", "MACHINEKIT"))) {
	    conf->service_uuid = strdup(s);
	} else {
	    syslog_async(LOG_ERR,
			 "%s: no service UUID (-R <uuid> or MACHINEKIT_INI [MACHINEKIT]MKUUID) present\n",
			 conf->progname);
	    return -1;
	}
    }
    if (uuid_parse(conf->service_uuid, uutmp)) {
	syslog_async(LOG_ERR,
		     "%s: service UUID: syntax error: '%s'",
		     conf->progname,conf->service_uuid);
	return -1;
    }
    iniFindInt(inifp, "REMOTE", "MACHINEKIT", &conf->remote);
    if (conf->remote && (conf->interfaces == NULL)) {
	if ((s = iniFind(inifp, "INTERFACES", "MACHINEKIT"))) {

	    char ifname[LINELEN], ip[LINELEN];

	    // pick a preferred interface
	    if (parse_interface_prefs(s,  ifname, ip, &conf->ifIndex) == 0) {
		conf->interface = strdup(ifname);
		conf->ipaddr = strdup(ip);
		syslog_async(LOG_INFO, "%s %s: using preferred interface %s/%s\n",
			     conf->progname, inifile,
			     conf->interface, conf->ipaddr);
	    } else {
		syslog_async(LOG_ERR, "%s %s: INTERFACES='%s'"
			     " - cant determine preferred interface, using %s/%s\n",
			     conf->progname, inifile, s,
			     conf->interface, conf->ipaddr);
	    }
	}
    }
    fclose(inifp);
    return 0;
}

// getenv("INI_FILE_NAME") or commandline - per config
static int
read_config(htconf_t *conf)
{
    const char *s;
    FILE *inifp = NULL;

    if (conf->inifile && ((inifp = fopen(conf->inifile,"r")) == NULL)) {
	syslog_async(LOG_ERR, "%s: cant open inifile '%s'\n",
		     conf->progname, conf->inifile);
    }

    char uri[LINELEN];
    if (!conf->remote) {
	// use IPC sockets
	snprintf(uri, sizeof(uri), ZMQIPC_FORMAT,
		 RUNDIR, rtapi_instance,"halgroup", conf->service_uuid);
	conf->halgroup = strdup(uri);

	snprintf(uri, sizeof(uri), ZMQIPC_FORMAT,
		 RUNDIR, rtapi_instance,"halrcomp", conf->service_uuid);
	conf->halrcomp = strdup(uri);

	snprintf(uri, sizeof(uri), ZMQIPC_FORMAT,
		 RUNDIR, rtapi_instance,"halrcmd", conf->service_uuid);
	conf->command = strdup(uri);

    } else {
	// finalize the URI's; config values have precedence, else use
	// ephemeral URI's on preferred interface

	if (inifp && (s = iniFind(inifp, "HALGROUP_STATUS_URI", conf->section)))
	    conf->halgroup = strdup(s);
	else {
	    snprintf(uri, sizeof(uri), conf->halgroup, conf->ipaddr);
	    conf->halgroup = strdup(uri);
	}

	if (inifp && (s = iniFind(inifp, "HALRCOMP_STATUS_URI", conf->section)))
	    conf->halrcomp = strdup(s);
	else {
	    snprintf(uri, sizeof(uri), conf->halrcomp, conf->ipaddr);
	    conf->halrcomp = strdup(uri);
	}
	if (inifp && (s = iniFind(inifp, "COMMAND_URI", conf->section)))
	    conf->command = strdup(s);
	else {
	    snprintf(uri, sizeof(uri), conf->command, conf->ipaddr);
	    conf->command = strdup(uri);
	}
    }
    // ease debugging
    if (conf->trap_signals && (getenv("NOSIGHDLR") != NULL))
	conf->trap_signals = false;

    // bridge: TBD
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
	iniFindInt(inifp, "PARANOID", conf->section, &conf->paranoid);
	fclose(inifp);
    }
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
	   "-p or --paranoid <msec>\n"
	   "    turn on extensive runtime checks (may be costly).\n"
	   "-d or --debug\n"
	   "    Turn on event debugging messages.\n");
}

static const char *option_string = "hI:S:d:t:u:r:T:c:pb:C:U:i:N:R:sK:G";
static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"paranoid", no_argument, 0, 'p'},
    {"ini", required_argument, 0, 'I'},     // default: getenv(INI_FILE_NAME)
    {"section", required_argument, 0, 'S'},
    {"debug", required_argument, 0, 'd'},
    {"gtimer", required_argument, 0, 't'},
    {"ctimer", required_argument, 0, 'T'},
    {"keepalive", required_argument, 0, 'K'},
    {"stpuri", required_argument, 0, 'u'},
    {"rcompuri", required_argument, 0, 'r'},
    {"cmduri", required_argument, 0, 'c'},
    {"bridge", required_argument, 0, 'b'},
    {"bridgecmduri", required_argument, 0, 'C'},
    {"bridgeupdateuri", required_argument, 0, 'U'},
    {"bridgeinstance", required_argument, 0, 'i'},
    {"interfaces", required_argument, 0, 'N'},
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
	case 'u':
	    conf.halgroup = optarg;
	    break;
	case 'r':
	    conf.halrcomp = optarg;
	    break;
	case 'c':
	    conf.command = optarg;
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
	case 'p':
	    conf.paranoid = 1;
	    break;
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
	case 'N':
	    conf.interfaces = optarg;
	    break;
	case 'R':
	    conf.service_uuid = optarg;
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

    if (read_global_config(&conf))
	exit(1);

    if (read_config(&conf))
	exit(1);

    htself_t self = {0};
    self.cfg = &conf;
    self.pid = getpid();
    uuid_generate_time(self.process_uuid);

    print_container = self.cfg->debug & 1; // log sent protobuf messages to stderr if debug & 1

    retval = hal_setup(&self);
    if (retval) exit(retval);

    retval = zmq_init(&self);
    if (retval) exit(retval);

#ifdef NOTYET
    retval = bridge_init(&self);
    if (retval) exit(retval);
#endif

    retval = ht_zeroconf_announce(&self);
    if (retval) exit(retval);

    mainloop(&self);

    ht_zeroconf_withdraw(&self);
    // probably should run zloop here until deregister complete

    // shutdown zmq context
    zctx_destroy(&self.z_context);

    hal_cleanup(&self);

    exit(0);
}
