
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <sys/un.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <alloca.h>
#include <limits.h>		/* for CHAR_BIT */
#include <signal.h>

#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

#include <rtapi.h>
#include <rtapi_bitops.h>
#include <hal.h>
#include <hal_priv.h>
#include <inifile.h>
#include <czmq.h>

#include "halops.h"

#include <protobuf/generated/haltype.pb-c.h>
#include <protobuf/generated/halvalue.pb-c.h>
#include <protobuf/generated/command.pb-c.h>
#include <protobuf/generated/report.pb-c.h>

#define SAFE(x) (((x) ? x : "<NULL>"))

static const char *data_value(int type, void *valptr);
static const char *data_type(int type);


void *ctx, *rpcserver, *publisher;
int signal_fd, timer_fd;
static zloop_t *loop;
static time_t startup_time; // 'session ID'

static char *option_string = "nr:hi:S:zd";
static struct option long_options[] = {
    {"rtapi-msg-level", no_argument, 0, 'r'},
    {"help", no_argument, 0, 'h'},
    {"ini", required_argument, 0, 'i'},     // default: getenv(INI_FILE_NAME)
    {"section", required_argument, 0, 'S'},
    {"zmq-monitor", required_argument, 0, 'z'},
    {"debug", required_argument, 0, 'd'},
    {"no-encode", required_argument, 0, 'n'},
    {0,0,0,0}
};

struct conf {
    char *progname;
    char *inifile;
    char *section;
    char *modname;
    char *publish;
    char *rpc;
    int msglevel;
    int debug;
    int highwater_mark;
    int zmq_monitor;
    int do_encode;
} conf = {
    .section = "HALBIND",
    .modname = "halbind",
    .publish = "tcp://localhost:5556",
    .rpc = "tcp://localhost:5557",
    .msglevel = -1,
    .do_encode = 1,
};

int read_ini()
{
    const char *s;
    FILE *inifp;

    if (!conf.inifile) {
	fprintf(stderr, "%s: need an inifile - either -i <inifile> or env INI_FILE_NAME=<inifile>\n",
		conf.progname);
	return -1;
    }
    if ((inifp = fopen(conf.inifile,"r")) == NULL) {
	fprintf(stderr, "%s: cant open inifile '%s'\n",
		conf.progname, conf.inifile);
	return -1;
    }
    if ((s = iniFind(inifp, "PUBLISH", conf.section)))
	conf.publish = strdup(s);
    if ((s = iniFind(inifp, "RPC", conf.section)))
	conf.rpc = strdup(s);
    fclose(inifp);
    return 0;
}


static int nreports;
static int n_components;
int comp_id;


typedef struct {
    hal_compstate_t cstate;
    int n_monitored; // count of objects to monitor for change
    int n_members;
    hal_data_u *shadow_signals; // tracking values of monitored members
    unsigned   *member_attributes;
    //hal_sig_t  **member_signals;
    char *changed; // BITMASK
} comp_t;

static comp_t *comps;

int setup_hal()
{
    int retval, i;
#if 0

    n_components = num_unbound_components();
    if (n_components < 1)
	return -1;

    comps = malloc(1, sizeof(comp_t *) * n_components);
    assert(comps != NULL);

    for (i = 0; i < n_components; i++) {

    }
	    rep->member_attributes = malloc(sizeof(unsigned) * MAX_MEMBERS);
	    rep->shadow_signals = malloc(sizeof(hal_data_u) * MAX_MEMBERS);
	    assert(rep->member_signals != NULL);
	    assert(rep->member_attributes != NULL);
	    assert(rep->shadow_signals != NULL);

    if ((comp_id = hal_init(conf.modname)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init(%s) failed: HAL error code=%d\n",
			conf.progname, conf.modname, comp_id);
	return -1;
    }
    // run through all groups, execute a callback for each group found.
    retval = halpr_foreach_group(NULL, group_cb, NULL);
    rtapi_print_msg(RTAPI_MSG_DBG,"found %d group(s)\n", retval);

    for (i = 0; i <  HAL_NGROUPS; i++) {
	if (reports[i] != NULL) {
	    retval = halpr_foreach_member(reports[i]->group->name, NULL, member_cb,
					  "group=%s id=%d arg1=%d arg2=%d serial=%d\n");
	}
    }
#endif
    hal_ready(comp_id);
    return 0;
}

int setup_io()
{
    int major, minor, patch, retval;

    ctx = zctx_new ();
    if (!ctx) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: zctx_new(): cant create ØMQ context: %s \n",
			conf.progname, zmq_strerror (errno));
	return -1;
    }
    zmq_version (&major, &minor, &patch);
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "%s: Current ØMQ version is %d.%d.%d, czmq version %d\n",
		    conf.progname, major, minor, patch, CZMQ_VERSION);


    rpcserver = zsocket_new(ctx, ZMQ_XREP);
    //    rpcserver = zsocket_new(ctx, ZMQ_PULL);
    if (!rpcserver) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: zsocket_new() cant create PULL socket: %s\n",
			conf.progname, zmq_strerror (errno));
	return -1;
    }

    if (zsocket_bind(rpcserver, conf.rpc) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: zsocket_bind(%s) failed: %s\n",
			conf.progname, conf.rpc,
			strerror (errno));
	return -1;
    }

    publisher = zsocket_new(ctx, ZMQ_PUB);
    if (!publisher) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: zsocket_new(): cant create PUB socket: %s\n",
			conf.progname, zmq_strerror (errno));
	return -1;
    }

    if (zsocket_bind (publisher, conf.publish) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: zsocket_bind(%s) failed: %s\n",
			conf.progname, conf.publish,
			zmq_strerror (errno));
	return -1;
    }

    if (conf.highwater_mark)
	zsocket_set_hwm (publisher, conf.highwater_mark);

    return 0;
}

int detect_changes(comp_t *rep)
{
    int i;
    hal_sig_t *sig;
    int nchanged = 0, nshadow = 0;
    hal_data_u *track;
    void *current;

    memset(rep->changed, 0, BITNSLOTS(rep->n_monitored));
    for (i = 0; i < rep->n_members; i++) {
	/* if (!(rep->member_attributes[i] & MEMBER_CHANGE_DETECT)) */
	/*     continue; */
	/* sig = rep->member_signals[i]; */
	current = SHMPTR(sig->data_ptr);
	track = &rep->shadow_signals[nshadow];
	switch (sig->type) {
	case HAL_BIT:
	    if (track->b != *((char *) current)) {
		nchanged++;
		BITSET(rep->changed, i);
	    }
	    track->b = *((char *) current);
	    break;
	case HAL_FLOAT:
	    // TODO: we need the epsilon parameter here
	    if (track->f != *((hal_float_t *) current)) {
		nchanged++;
		BITSET(rep->changed, i);
	    }
	    track->f = *((hal_float_t *) current);
	    break;
	case HAL_S32:
	    if (track->s != *((hal_s32_t *) current)) {
		nchanged++;
		BITSET(rep->changed, i);
	    }
	    track->s = *((hal_s32_t *) current);
	    break;
	case HAL_U32:
	    if (track->u != *((hal_u32_t *) current))  {
		nchanged++;
		BITSET(rep->changed, i);
	    }
	    track->u = *((hal_u32_t *) current);
	    break;
	default:
	    /* rtapi_print_msg(RTAPI_MSG_ERR,  */
	    /* 		    "%s: ERROR: detect_changes(%s): invalid type for signal %s: %d\n", */
	    /* 		    conf.progname, rep->group->name, sig->name, sig->type); */
	    return -1;
	}
	nshadow++;
    }
    return nchanged;
}

int assemble_cb(hal_group_t *group, hal_member_t *member, void  *cb_data)
{
#if 0
    report_t *report = cb_data;
    hal_sig_t *sig;
    sig = SHMPTR(member->member_ptr);

    if (member->userarg1 & MEMBER_CHANGE_DETECT) {
	assert(report->n_monitored < MAX_MEMBERS);
	report->n_monitored++;
    }
    report->member_signals[report->n_members] = sig;
    report->member_attributes[report->n_members] = member->userarg1;
    report->n_members++;
#endif
    return 0; // run to end.
}

int setup_reports()
{
#if 0
    int i, retval;
    report_t *rep;

    for (i = 0; i < HAL_NGROUPS; i++) {
	rep = reports[i];
	if (rep != NULL) {
	    rep->member_signals = malloc(sizeof(hal_sig_t *) * MAX_MEMBERS);
	    rep->member_attributes = malloc(sizeof(unsigned) * MAX_MEMBERS);
	    rep->shadow_signals = malloc(sizeof(hal_data_u) * MAX_MEMBERS);
	    assert(rep->member_signals != NULL);
	    assert(rep->member_attributes != NULL);
	    assert(rep->shadow_signals != NULL);

	    retval = halpr_foreach_member(rep->group->name, NULL, assemble_cb,
					  (void *) rep);
	    rep->member_signals = realloc(rep->member_signals,
					  sizeof(hal_data_u) * rep->n_members);
	    rep->member_attributes = realloc(rep->member_attributes,
					     sizeof(unsigned) * rep->n_members);
	    rep->changed = malloc(BITNSLOTS(rep->n_monitored));
	    if (rep->n_monitored) {
		rep->shadow_signals = realloc(rep->shadow_signals,
					      sizeof(hal_data_u) * rep->n_monitored);
			if (!rep->shadow_signals) {
		    rtapi_print_msg(RTAPI_MSG_ERR,
				    "%s: ERROR: out of memory allocating %d tracking variables\n",
				    conf.progname, rep->n_monitored);
		    return -1;
		}
	    }
	    rtapi_print_msg(RTAPI_MSG_DBG, "group %s (%d) has %d member(s)\n",
			    rep->group->name, rep->group->id, rep->n_members);

	    if (rep->n_monitored && !rep->group->userarg2) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"ERROR:  monitoring change of %d signal(s) but timer period zero\n",
				rep->n_monitored);
		return -1;
	    }
	    if (rep->n_monitored && !(rep->group->userarg1 & GROUP_TIMER)) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"ERROR:  monitoring change of %d signal(s) but timer not set\n",
				rep->n_monitored);
		return -1;
	    }
	    if (rep->group->userarg1 & GROUP_TIMER)
		rtapi_print_msg(RTAPI_MSG_DBG, "group %s timer %d mS",
				rep->group->name, rep->group->userarg2);
	    if (rep->n_monitored)
		if (!rep->group->userarg2)
		    rtapi_print_msg(RTAPI_MSG_ERR, ", monitoring change of %d signal(s)",
				rep->n_monitored);
	    rtapi_print_msg(RTAPI_MSG_DBG,"\n");

	}
    }
#endif
    return 0;
}

int pb_encode_report(comp_t *rep, void *socket, int cause, int n_changes)
{
    Report msg = REPORT__INIT;
    void *buf;
    unsigned len;
    static int serial;
    int i;
    void *valptr;
    hal_sig_t *sig;
    Value default_value = VALUE__INIT;
    zframe_t *frame;
    Value *submsg, *vp;

    // report header
    msg.type = REPORT_TYPE__UPDATE_FULL;
    // msg.group_id = rep->group->id;
    msg.serial = serial++;
    msg.cause = cause;
    msg.session = startup_time;
    if (rep->n_monitored) {
	msg.has_changes = 1;
	msg.changes = n_changes;
    }

    // repeated Value submessage
    msg.n_value = rep->n_members;
    msg.value = alloca(sizeof(Value *) *rep->n_members);
    submsg = alloca(sizeof(Value) *rep->n_members);
    for (i = 0; i < rep->n_members; i++) {

	submsg[i] = default_value;
	msg.value[i] = &submsg[i];
	vp = msg.value[i];
	// sig = rep->member_signals[i];
	valptr = SHMPTR(sig->data_ptr);

	vp->type = sig->type;
	vp->key = (uint32_t) sig; // address = opaque key
	vp->name = sig->name;
	switch (sig->type) {
	case HAL_BIT:
	    vp->has_halbit = 1;
	    vp->halbit = *((char *) valptr);
	    break;
	case HAL_FLOAT:
	    vp->has_halfloat = 1;
	    vp->halfloat = *((hal_float_t *) valptr);
	    break;
	case HAL_S32:
	    vp->has_hals32 = 1;
	    vp->hals32 = *((hal_s32_t *) valptr);
	    break;
	case HAL_U32:
	    vp->has_halu32 = 1;
	    vp->halu32 = *((hal_u32_t *) valptr);
	    break;
	default:
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "pb_encode_report(): value %d bad type %d (sig@0x%p)\n",
			    i, sig->type, (void *)sig);
	}
	if (rep->n_monitored) {
	    vp->has_changed = 1;
	    vp->changed = BITTEST(rep->changed, i);
	}
    }
    len = report__get_packed_size(&msg);
    buf = alloca(len);
    report__pack(&msg,buf);
    frame = zframe_new(buf, len);

    return zframe_send(&frame, socket,0);
}

int send_report(int cause, unsigned long events)
{
    int i;
    int nmsgs = 0, n_changes = 0;
    comp_t *rep;

    for (i = 0; i < HAL_NGROUPS; i++) {
	if (events & (1 << i)) {
	    //rep = reports[i];
	    if (rep) {
#if 0
		// the PUB topic - use the HAL group name
		if (zstr_sendm (publisher, rep->group->name) < 0) {
		    rtapi_print_msg(RTAPI_MSG_ERR,"zstr_sendm(%s) failed: %s\n",
				    rep->group->name, zmq_strerror (errno));
		}
#endif
		if (rep->n_monitored) {
		    n_changes = detect_changes(rep);
		}
		rtapi_print_msg(RTAPI_MSG_DBG, "send_report cause=%d group=%d n_changes=%d\n",
				cause, i, n_changes);
		if (conf.do_encode) {
		    if (pb_encode_report(rep, publisher, cause, n_changes) < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"zframe_send() failed: %s\n",
					zmq_strerror (errno));
		    }
		}
		nmsgs++;
	    }
	}
    }
    return nmsgs;
}

void set_signal(HalValue *value)
{

    hal_sig_t *sig;
    void *data_ptr;

    // mutex?
    if ((sig = halpr_find_sig_by_name(value->name)) == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "set_signal: no such signal '%s'\n",
			value->name);
	return;
    }
    if (sig->type != value->type) {
	rtapi_print_msg(RTAPI_MSG_ERR, "set_signal(%s): data type mismatch %d/%d\n",
			value->name, sig->type, value->type);
	return;
    }

    data_ptr = SHMPTR(sig->data_ptr);

    switch (value->type) {
    case HAL_BIT:
	*((char *) data_ptr) = value->halbit;
	break;
    case HAL_FLOAT:
	*((hal_float_t *) data_ptr) = value->halfloat;
	break;
    case HAL_S32:
	*((hal_s32_t *) data_ptr) = value->hals32;
	break;
    case HAL_U32:
	*((hal_u32_t *) data_ptr) = value->halu32;

	break;
    default:
	rtapi_print_msg(RTAPI_MSG_ERR, "set_signal: unknown type %d\n",
			value->type);
    }
}

void rpcreply(void *rpcserver, zframe_t *route,
	      int type, int req_type, int req_serial, char *errmsg)
{
    Reply reply = REPLY__INIT;
    int len, retval;
    unsigned char *buf;
    zframe_t *frame;

    reply.type = type;
    reply.request_type = req_type;
    reply.request_serial = req_serial;
    reply.timestamp = 123456789;
    reply.errormsg = errmsg;

    len = reply__get_packed_size(&reply);
    buf = alloca(len);
    reply__pack(&reply,buf);
    frame = zframe_new(buf, len);

    retval = zframe_send (&route, rpcserver, ZFRAME_MORE);
    assert (retval == 0);

    if ((retval = zframe_send(&frame, rpcserver, 0))) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: zframe_send: %d %s\n",
			conf.progname, retval, zmq_strerror (errno));
    }
}

void rpcrequest(void *rpcserver, zmsg_t *cmd_msg)
{
    Command *cmd;
    zframe_t *cmd_route;
    zframe_t *cmd_frame;
    int i;
    HalValue *value;

    cmd_route = zmsg_pop(cmd_msg);
    cmd_frame = zmsg_pop(cmd_msg);
    if ((cmd = command__unpack(NULL, zframe_size(cmd_frame),
			       zframe_data(cmd_frame))) == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: error unpacking incoming message\n",
			conf.progname);
	return;
    }
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "request: type=%d serial=%d rsvp=%d n_val=%d\n",
		    cmd->type, cmd->serial, cmd->rsvp, cmd->n_value);

    switch (cmd->type) {
    case COMMAND_TYPE__SET_HAL_SIGNAL:
	for (i = 0; i < cmd->n_value; i++) {
	    set_signal(cmd->value[i]);
	}
	break;
    case COMMAND_TYPE__LINK_PINS:
	printf("link_pins %d component='%s'\n",
	       cmd->n_value, SAFE(cmd->component));

	for (i = 0; i < cmd->n_value; i++) {
	    value = cmd->value[i];
	    switch (value->type) {
	    case HAL_BIT:
		rtapi_print_msg(RTAPI_MSG_ERR, "commit latent '%s.%s' type=%d dir=%d value=%d\n",
				SAFE(cmd->component), value->name, value->type,value->dir, value->halbit);
		break;
	    case HAL_FLOAT:
		rtapi_print_msg(RTAPI_MSG_ERR, "commit latent '%s.%s' type=%d dir=%d value=%f\n",
				SAFE(cmd->component), value->name, value->type,value->dir,value->halfloat);
		break;
	    case HAL_S32:
		rtapi_print_msg(RTAPI_MSG_ERR, "commit latent '%s.%s' type=%d dir=%d value=%d\n",
				SAFE(cmd->component), value->name,
				value->type,value->dir,value->hals32);
		break;
	    case HAL_U32:
		rtapi_print_msg(RTAPI_MSG_ERR, "commit latent '%s.%s' type=%d dir=%d value=%u\n",
				SAFE(cmd->component), value->name, value->type,value->dir,value->halu32);
		break;
	    default:
		rtapi_print_msg(RTAPI_MSG_ERR, "commit latent: unknown type %d\n",
				value->type);
	    }
	}
	break;
    default:
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: rpc: unkown request type %d\n",
			conf.progname, cmd->type);

	// send error report here
	command__free_unpacked(cmd, NULL);
    }
    if (cmd->rsvp != REPLY_REQUIRED__NONE) {
	// printf("reply cmd type=%d\n", cmd->type);
	rpcreply(rpcserver, cmd_route, REPLY_TYPE__DONE, cmd->type, cmd->serial, NULL);
    }
    command__free_unpacked(cmd, NULL);
}


int s_handle_timer(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    comp_t *rep = arg;
    // send_report(REPORT_CAUSE__PERIODIC, 1 << rep->group->id);
    return 0;
}

int s_handle_rpcserver(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    zmsg_t *msg ;

    msg = zmsg_recv(rpcserver);
    rpcrequest(rpcserver, msg);
    zmsg_destroy (&msg);
    return 0;
}

int s_handle_signal(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(signal_fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
	perror("read");
    }
    switch (fdsi.ssi_signo) {
    case SIGINT:
    case SIGQUIT:
	hal_exit(comp_id);
	exit(0);
    default:
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: signal %d - '%s' received\n",
			conf.progname, fdsi.ssi_signo, strsignal(fdsi.ssi_signo));
    }
    return 0;
}

void mainloop()
{
    int i, retval;
    comp_t *rep;
    zmq_pollitem_t rpcserver_poller = { rpcserver, 0, ZMQ_POLLIN };
    zmq_pollitem_t signal_poller =    { 0, signal_fd,   ZMQ_POLLIN };

    startup_time = time(NULL);
    loop = zloop_new();
    assert (loop);
    zloop_set_verbose (loop, conf.debug);

    zloop_poller (loop, &rpcserver_poller, s_handle_rpcserver, NULL);
    zloop_poller (loop, &signal_poller,    s_handle_signal,    NULL);

#if 0

    for (i = 0; i < HAL_NGROUPS; i++) {
	rep = reports[i];
	if (rep && (rep->group->userarg1 & (GROUP_TIMER)))
	    zloop_timer(loop, rep->group->userarg2, 0, s_handle_timer, rep);
    }

#endif

    // handle signals && profiling properly
    do {
	retval = zloop_start(loop);
    } while (!(retval || zctx_interrupted));

    rtapi_print_msg(RTAPI_MSG_INFO,"%s: shutting down\n",
		    conf.progname);
    hal_exit(comp_id);
    exit(0);
}

void usage(int argc, char **argv) {
    printf("Usage:  %s [options]\n", argv[0]);
    printf("This is a userspace HAL program, typically loaded using the halcmd \"loadusr\" command:\n"
	   "    loadusr halreport [options]\n"
	   "Options are:\n"
	   "-I or --ini <inifile>\n"
	   "    Use <inifile> (default: take ini filename from environment variable INI_FILE_NAME)\n"
	   "-S or --section <section-name> (default 8)\n"
	   "    Read parameters from <section_name> (default 'VFS11')\n"
	   "-r or --rtapi-msg-level <level>\n"
	   "    set the RTAPI message level.\n"
	   "-d or --debug\n"
	   "    Turn on event debugging messages.\n");
}
int main (int argc, char *argv[ ])
{
    int opt;
    sigset_t mask;

    conf.progname = argv[0];
    conf.inifile = getenv("INI_FILE_NAME");
    while ((opt = getopt_long(argc, argv, option_string,
			      long_options, NULL)) != -1) {
	switch(opt) {
	case 'n':
	    conf.do_encode  = 0;
	    break;
	case 'd':
	    conf.debug = 1;
	    break;
	case 'S':
	    conf.section = optarg;
	    break;
	case 'i':
	    conf.inifile = optarg;
	    break;
	case 'r':
	    conf.msglevel = atoi(optarg);
	    break;
	case 'h':
	default:
	    usage(argc, argv);
	    exit(0);
	}
    }
    if (conf.msglevel > -1)
	rtapi_set_msg_level(conf.msglevel);


    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
	perror("sigprocmask");
    signal_fd = signalfd(-1, &mask, 0);
    assert(signal_fd > -1);

    if (read_ini())
	exit(1);
    if (setup_hal())
	exit(1);
    if (setup_io()) {
	hal_exit(comp_id);
	exit(1);
    }
    if (setup_reports()) {
	exit(1);
    }
    mainloop();
    exit(0);
}

static const char *data_type(int type)
{
    const char *type_str;

    switch (type) {
    case HAL_BIT:
	type_str = "bit";
	break;
    case HAL_FLOAT:
	type_str = "float";
	break;
    case HAL_S32:
	type_str = "s32";
	break;
    case HAL_U32:
	type_str = "u32";
	break;
    default:
	type_str = "undef";
    }
    return type_str;
}

static const char *data_value(int type, void *valptr)
{
    char *value_str;
    static char buf[15];

    switch (type) {
    case HAL_BIT:
	if (*((char *) valptr) == 0)
	    value_str = "FALSE";
	else
	    value_str = "TRUE";
	break;
    case HAL_FLOAT:
	snprintf(buf, 14, "%g", (double)*((hal_float_t *) valptr));
	value_str = buf;
	break;
    case HAL_S32:
	snprintf(buf, 14, "%ld", (long)*((hal_s32_t *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 14, "%lu", (unsigned long)*((hal_u32_t *) valptr));
	value_str = buf;
	break;
    default:
	value_str = "undef";
    }
    return value_str;
}
