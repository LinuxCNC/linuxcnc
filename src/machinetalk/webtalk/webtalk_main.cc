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

#include "webtalk.hh"
#include <setup_signals.h>
#include <inihelp.hh>

// configuration defaults
static wtconf_t conf;

static void lwsl_emit_wtlog(int filter, const char *line);
static int lwsl_logopts(char *logopts);
static struct timeval tv_start;

static int
handle_signal(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    wtself_t *self = (wtself_t *)arg;
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(self->signal_fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
	perror("read");
    }
    switch (fdsi.ssi_signo) {
    default:
	syslog_async(LOG_ERR, "%s: signal %d - '%s' received\n",
		     self->cfg->progname,
		     fdsi.ssi_signo,
		     strsignal(fdsi.ssi_signo));
    }
    self->interrupted = true;
    return -1; // exit reactor with -1
}

static int
mainloop( wtself_t *self)
{
    int retval;
    zloop_set_verbose (self->netopts.z_loop, self->cfg->debug & LLL_LOOP);

    zmq_pollitem_t signal_poller = { 0, self->signal_fd, ZMQ_POLLIN };
    if (self->cfg->trap_signals)
	zloop_poller(self->netopts.z_loop, &signal_poller, handle_signal, self);

    if ((self->wsctx = libwebsocket_create_context(&self->cfg->info)) == NULL) {
	lwsl_err("libwebsocket_create_context failed\n");
	return -1;
    }
    if (self->cfg->service_timer > 0)
	self->service_timer = zloop_timer(self->netopts.z_loop,
					  self->cfg->service_timer,
					  0,
					  service_timer_callback,
					  self->wsctx);

    // announce only after websocket successfulled created
    if (mk_announce(&self->netopts, &self->mksock,
		    "Machinekit", self->cfg->index_html))
	return -1;
    syslog_async(LOG_DEBUG, "%s: talking Machinekit %s on '%s'",
		 self->cfg->progname, self->mksock.tag,
		 self->mksock.announced_uri);


    do {
	retval = zloop_start(self->netopts.z_loop);
    } while  (!(retval || self->interrupted));

    if (self->service_timer)
	zloop_timer_end (self->netopts.z_loop, self->service_timer);

    libwebsocket_context_destroy(self->wsctx);

    syslog_async(LOG_INFO,
		 "%s: exiting mainloop (%s)\n",
		 self->cfg->progname,
		 self->interrupted ? "interrupted": "reactor exited");

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

static int
zmq_init(wtself_t *self)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // ease debugging
    if (self->cfg->trap_signals) {
	self->signal_fd = setup_signals(sigaction_handler,
					SIGINT, SIGQUIT, SIGKILL, SIGTERM, -1);
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
    np->rtapi_instance = self->cfg->rtapi_instance;

    np->av_loop = avahi_czmq_poll_new(np->z_loop);
    assert(np->av_loop);

    mk_socket_t *ms = &self->mksock;
    ms->dnssd_type = self->cfg->use_ssl ? "_https._tcp" : "_http._tcp";
    ms->dnssd_subtype = NULL;
    ms->tag =  self->cfg->use_ssl ? "https" : "http";
    ms->port =  self->cfg->info.port;

    return 0;
}

static void
wt_hello(wtconf_t *cfg)
{
    int major, minor, patch;
    zmq_version (&major, &minor, &patch);
    syslog_async(LOG_DEBUG,
		 "%s: startup ØMQ=%d.%d.%d czmq=%d.%d.%d protobuf=%d.%d.%d libwebsockets='%s'\n",
		 cfg->progname, major, minor, patch,
		 CZMQ_VERSION_MAJOR, CZMQ_VERSION_MINOR,CZMQ_VERSION_PATCH,
		 GOOGLE_PROTOBUF_VERSION / 1000000,
		 (GOOGLE_PROTOBUF_VERSION / 1000) % 1000,
		 GOOGLE_PROTOBUF_VERSION % 1000,
		 lws_get_library_version());
}

// this is pathetic. We need a decent library solution for config/arg/environmnt handling.
static int
read_config(wtconf_t *conf)
{
    FILE *inifp = NULL;
    int flag;

    if (conf->inifile) {
	if ((inifp = fopen(conf->inifile,"r")) == NULL) {
	    syslog_async(LOG_ERR, "%s: cant open inifile '%s'\n",
			 conf->progname, conf->inifile);
	    return -1;
	}
    } else
	return 0;

    iniFindInt(inifp, "DEBUG", conf->section, &conf->debug);
    iniFindInt(inifp, "IPV6", conf->section, &conf->ipv6);
    iniFindInt(inifp, "PORT", conf->section, &conf->info.port);
    flag = 0;
    iniFindInt(inifp, "REQUIRE_VALID_OPENSSL_CLIENT_CERT", conf->section, &flag);
    if (flag) conf->info.options |= LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT;
    flag = 0;
    iniFindInt(inifp, "SKIP_SERVER_CANONICAL_NAME", conf->section, &flag);
    if (flag) conf->info.options |= LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME;
    flag = 0;
    iniFindInt(inifp, "ALLOW_NON_SSL_ON_SSL_PORT", conf->section, &flag);
    if (flag) conf->info.options |= LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT;
    flag = 0;
    iniFindInt(inifp, "DISABLE_OS_CA_CERTS", conf->section, &flag);
    if (flag) conf->info.options |= LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS;

    iniFindInt(inifp, "EXTENSIONS",  conf->section, &flag);
    if (flag) conf->info.extensions = libwebsocket_get_internal_extensions();
    iniFindInt(inifp, "TIMER", conf->section, &conf->service_timer);

    str_inidefault(&conf->index_html, inifp, "INDEX_HTML", conf->section);
    str_inidefault(&conf->www_dir, inifp, "WWW_DIR", conf->section);
    str_inidefault((char **)&conf->info.ssl_cert_filepath, inifp, "CERT_PATH", conf->section);
    str_inidefault((char **)&conf->info.ssl_private_key_filepath, inifp, "KEY_PATH", conf->section);

    //keyword-parse LOG= line for any of
    // ERR WARN NOTICE INFO DEBUG PARSER HEADER EXTENSION CLIENT LATENCY URI TOWS FROMWS LOOP CONFIG ZWS
    char *logopts = NULL;
    str_inidefault(&logopts, inifp, "LOG", conf->section);
    if (logopts) {
	conf->debug |= lwsl_logopts(logopts);
	free(logopts);
    }
    fclose(inifp);
    int cnt =  (conf->info.ssl_cert_filepath != NULL) ? 1 : 0;
    if  (conf->info.ssl_private_key_filepath != NULL) cnt++;
    if (cnt == 1) {
	lwsl_err("%s %s: SSL mode needs both CERT_PATH and KEY_PATH",
		 conf->progname, conf->inifile);
	return -1;
    }
    if (cnt == 2)
	conf->use_ssl = true;
    return 0;
}

static void
config_overrides(wtconf_t *conf)
{
    if (conf->index_html == NULL)
	conf->index_html = (char *) "/";

    // debug defaults -  emit those always regardless of flags
    conf->debug |= (LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_CONFIG);
}

static void
usage(void)
{
    printf("Usage:  webtalk [options]\n");
    printf("This is a normal userspace program and need not run on the same host as RT.\n"
	   "Options are:\n"
	   "-I or --ini <inifile>\n"
	   "    Use <inifile> (default: take ini filename from environment"
	   " variable INI_FILE_NAME)\n"
	   "-S or --section <section-name> (default 8)\n"
	   "    Read parameters from <section_name> (default 'HALTALK')\n"
	   "-R or --mkuuid <uuid>\n"
	   "    use as service uuid\n"
	   "-X or --index <announce>\n"
	   "    as path to toplevel html file>\n"
	   "-p or --port <portnumber>\n"
	   "    to use for http/https\n"
	   "-w or --wwwdir <directory>\n"
	   "    to serve html files from\n"
	   "-C or --certpath <path>\n"
	   "    to to SSL cert\n"
	   "-K or --keypath <path>\n"
	   "    to to SSL key\n"
	   "-s or --stderr\n"
	   "    log to stderr besides syslog\n"
	   "-F or --foreground\n"
	   "    stay in foreground - dont fork\n"
	   "-d or --debug <mask>\n"
	   "    Turn on event debugging messages.\n");
}


static const char *option_string = "hI:S:d:R:Fsw:X:p:C:K:P:";
static struct option long_options[] = {
    { "help", no_argument, 0, 'h'},
    { "ini", required_argument, 0, 'I'},
    { "section", required_argument, 0, 'S'},
    { "debug", required_argument, 0, 'd'},
    { "timer", required_argument, 0, 't'},
    { "mkuuid", required_argument, 0, 'R'},
    { "index", required_argument, 0, 'X'},
    { "port", required_argument, 0, 'p'},
    { "wwwdir", required_argument, 0, 'w'},
    { "certpath", required_argument, 0, 'C'},
    { "keypath", required_argument, 0, 'K'},
    { "stderr",  no_argument,        0, 's'},
    { "foreground",  no_argument,    0, 'F'},
    { "plugin", required_argument, 0, 'P'},
    {0,0,0,0}
};

int main (int argc, char *argv[])
{
    conf.progname = argv[0];
    conf.inifile  = getenv("INI_FILE_NAME");
    conf.section  = "WEBTALK";
    conf.info.gid = -1;
    conf.info.uid = -1;
    conf.index_html = NULL;
    conf.info.port = PROXY_PORT;
    // ease debugging with gdb - disable all signal handling
    conf.trap_signals = (getenv("NOSIGHDLR") == NULL);

    wtself_t self = {0};
    self.cfg = &conf;
    self.pid = getpid();

    int logopt = LOG_NDELAY;
    int opt, retval;
    zlist_t *plugins = zlist_new(); // list of .so pathnames

    // first pass - only read opts relevant for logging and inifile
    while ((opt = getopt_long(argc, argv, option_string,
			      long_options, NULL)) != -1) {
	switch(opt) {
	case 'S':
	    conf.section = optarg;
	case 'I':
	    conf.inifile = optarg;
	    break;
	case 'F':
	    conf.foreground = true;
	    break;
	case 's':
	    conf.log_stderr = true;
	    logopt |= LOG_PERROR;
	    break;
	break;
	default:
	    ;
	}
    }

    openlog_async(conf.progname, logopt , SYSLOG_FACILITY);

    // generic binding & announcement parameters
    // from $MACHINEKIT_INI
    self.netopts.rundir = RUNDIR;
    if (mk_getnetopts(&self.netopts))
	exit(1);

    if (read_config(&conf))
	exit(1);

    config_overrides(&conf);

    // second pass: override ini opts by command line
    optind = 0;
    while ((opt = getopt_long(argc, argv, option_string,
			      long_options, NULL)) != -1) {
	switch(opt) {
	case 'd':
	    conf.debug = atoi(optarg);
	    break;
	case 't':
	    conf.service_timer = atoi(optarg);
	    break;
	case 'p':
	    conf.info.port = atoi(optarg);
	    break;
	case 'C':
	    conf.info.ssl_cert_filepath  = optarg;
	    break;
	case 'K':
	    conf.info.ssl_private_key_filepath  = optarg;
	    break;
	case 'w':
	    conf.www_dir = optarg;
	    break;
	case 'X':
	    conf.index_html = optarg;
	    break;
	case 'R':
	    self.netopts.service_uuid = optarg;
	    break;
	case 'P':
	    zlist_append(plugins, strdup(optarg));
	    break;

	case 'S': // already covered on first pass
	case 'I':
	case 'F':
	case 's':
	    break;
	case 'h':
	default:
	    usage();
	    exit(0);
	}
    }

    // good to go
    if (!conf.foreground) {
        pid_t pid = fork();
        if (pid < 0) {
	    exit(EXIT_FAILURE);
        }
        if (pid > 0) {
	    exit(EXIT_SUCCESS);
        }
        umask(0);
        int sid = setsid();
        if (sid < 0) {
	    exit(EXIT_FAILURE);
        }
        // if ((chdir("/")) < 0) {
	//     exit(EXIT_FAILURE);
        // }
    }
    wt_hello(&conf);


    gettimeofday(&tv_start, NULL);

    lws_set_log_level(self.cfg->debug, lwsl_emit_wtlog);

    retval = zmq_init(&self);
    if (retval) exit(retval);

    init_protocols();
    retval = wt_proxy_new(&self);
    if (retval) exit(retval);

    // custom relay policy
    for (char *p = (char *) zlist_first(plugins);
	 p != NULL;
	 p = (char *) zlist_next(plugins)) {
	if (wt_add_plugin(&self, p)) {
	    exit(1);
	}
    }
    wt_proxy_add_policy(&self, "json", json_policy);

    mainloop(&self);

    mk_withdraw(&self.mksock);
    // probably should run zloop here until deregister complete

    // shutdown zmq context
    zctx_destroy(&self.netopts.z_context);

    exit(0);
}

static const char * const wt_log_level_names[] = {
    "ERR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG",
    "PARSER",
    "HEADER",
    "EXTENSION",
    "CLIENT",
    "LATENCY",
    "URI",
    "TOWS",
    "FROMWS",
    "LOOP",
    "CONFIG",
    "ZWS",
    NULL
};

static void lwsl_emit_wtlog(int filter, const char *line)
{
    int syslog_level = LOG_DEBUG;
    int n;
    char buf[300];
    struct timeval tv_now, tv_delta;

    gettimeofday(&tv_now, NULL);
    timersub(&tv_now,&tv_start, &tv_delta);

    if (filter & LLL_DEBUG)  syslog_level = LOG_DEBUG;
    if (filter & LLL_INFO)   syslog_level = LOG_INFO;
    if (filter & LLL_NOTICE) syslog_level = LOG_NOTICE;
    if (filter & LLL_WARN)   syslog_level = LOG_WARNING;
    if (filter & LLL_ERR)    syslog_level = LOG_ERR;
    buf[0] = '\0';
    for (n = 0; n < LLL_LAST; n++)
	if (filter == (1 << n)) {
	    snprintf(buf, sizeof(buf),
		     "[%lu.%06ld] %s: ", tv_delta.tv_sec, tv_delta.tv_usec,
		     wt_log_level_names[n]);
	    break;
	}
    syslog_async(syslog_level, "%s%s", buf, line);
}

static int lwsl_logopts(char *logopts)
{
    int mask = 0;
    char *s = logopts, *token, *save;
    while ((token = strtok_r(s, "\t ,", &save)) != NULL) {
	char *t = token;
	int flip = 0;
	if (*t == '-') {
	    t++;
	    flip = 1;
	}
	if (strcasecmp(t,"ALL") == 0) {
	    mask = -1;
	    goto next;
	}
	for (int n = 0; wt_log_level_names[n] != NULL; n++) {
	    if (strcasecmp(t, wt_log_level_names[n]) == 0) {
		if (flip)
		    mask &= ~(1 << n);
		else
		    mask |= (1 << n);
		goto next;
	    }
	}
	syslog_async(LOG_ERR, "inifile item LOG: no such keyword '%s'", token);
    next:
	s = NULL;
    }
    return mask;
}
