//
// halreport
//
// this is the messaging interface to HAL groups and events.
// it:
//    - consults HAL for HAL groups which need periodic, or change
//      based reporting
//    - for each HAL group, it publishes the member list on a ZMQ
//      topic with the group name
//    - provide a ZMQ REQ/REPLY service to retrieve bindings.
//    - listens for hal_notify() events from kernel RT components which
//      come through sysfsnotify()
//    - listens for events written to the halnotify named pipe
//      from usermode HAL components
//
// uses zmq v3
//
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
#include <sys/timerfd.h>
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
#include <hal.h>
#include <hal_priv.h>
#include <inifile.h>
#include <czmq.h>

#include <protobuf/generated/types.pb-c.h>
#include <protobuf/generated/value.pb-c.h>
#include <protobuf/generated/object.pb-c.h>
#include <protobuf/generated/message.pb-c.h>


#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)

#define SAFE(x) (((x) ? x : "<NULL>"))

// group->userarg1 meaning:
// if bit1 is set, this group has a timer
// the group->userarg2 is the timer period in mS
#define GROUP_TIMER  1

// report the group unconditionally on each
// timer period
// if the group has MEMBER_CHANGE_DETECT members, the
// group is checked for changes before the periodic report
#define GROUP_REPORT_PERIODIC    2

// member->userarg1 meaning:
// cause a report when this member changes value
#define MEMBER_CHANGE_DETECT 1

#define MAX_MEMBERS 500

static const char *data_value(int type, void *valptr);
static const char *data_type(int type);

#if defined(ZMQ_MONITOR)  // the monitor code in libzmq is heavily in flux right now
static void socket_monitor (void *s, int event_, zmq_event_data_t *data_);

static int zctx_set_monitor(void *foo, zmq_monitor_fn func); // work around missing function
#endif
//typedef struct {
//} haldata_t;
//haldata_t *haldata;

typedef struct {
    hal_group_t *group;
    int n_monitored; // count of objects to monitor for change
    int n_members;
    hal_data_u *shadow_signals; // tracking values of monitored members
    unsigned   *member_attributes;
    hal_sig_t  **member_signals;
    char *changed; // BITMASK
} report_t;

static report_t *reports[HAL_NGROUPS];
static int nreports;
int comp_id;

void *ctx, *rpcserver, *publisher;
int pipe_fd, sysfs_fd, signal_fd, timer_fd;
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
    char *pipe_name;
    char *sysfs;
    char *publish;
    char *rpc;
    int msglevel;
    int debug;
    int highwater_mark;
    int zmq_monitor;
    int do_encode;
    int pid;
} conf = {
    .section = "HALREPORT",
    .modname = "halreport",
    .pipe_name = "/tmp/halnotify",
    .sysfs = NULL,
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
    if ((s = iniFind(inifp, "PIPE", conf.section)))
	conf.pipe_name = strdup(s);
    if ((s = iniFind(inifp, "SYSFS", conf.section)))
	conf.sysfs = strdup(s);
    if ((s = iniFind(inifp, "PUBLISH", conf.section)))
	conf.publish = strdup(s);
    if ((s = iniFind(inifp, "RPC", conf.section)))
	conf.rpc = strdup(s);
    iniFindInt(inifp, "ZMQ_MONITOR", conf.section, &conf.zmq_monitor);
    fclose(inifp);
    return 0;
}


int member_cb(hal_group_t *group, hal_member_t *member, void *cb_data)
{
    hal_sig_t *sig;
    sig = SHMPTR(member->member_ptr);
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "%s: userarg=%d type=%s value=%s nread=%d nwrite=%d nbidir=%d\n",
		    sig->name, member->userarg1, data_type(sig->type),
		    data_value(sig->type, SHMPTR(sig->data_ptr)),
		    sig->readers, sig->writers, sig->bidirs);
    return 0; // run to end.
}

int group_cb(hal_group_t *group, void *cb_data)
{
    report_t *rep;
    int id;

    id = group->id;
    assert(id >= 0);
    assert(id < HAL_NGROUPS);
    if (reports[id] != NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: group %s (id=%d) redefined by group %s\n",
			conf.progname, reports[id]->group->name, id, group->name);
	return -1;
    }
    if ((rep = calloc(sizeof(report_t), 1)) == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: out of memory allocating report\n",
			conf.progname);
	return -1;
    }
    rep->group = group;
    reports[id] = rep;
    nreports++;
    return 0; // run to end.
}

int setup_hal()
{
    int retval, i;

    if ((comp_id = hal_init(conf.modname)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init(%s) failed: HAL error code=%d\n",
			conf.progname, conf.modname, comp_id);
	return -1;
    }
    /* haldata = (haldata_t *)hal_malloc(sizeof(haldata_t)); */
    /* if (haldata == 0) { */
    /* 	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: unable to allocate shared memory\n", */
    /* 			conf.progname); */
    /* 	return -1; */
    /* } */


    // run through all groups, execute a callback for each group found.
    retval = halpr_foreach_group(NULL, group_cb, NULL);
    rtapi_print_msg(RTAPI_MSG_DBG,"found %d group(s)\n", retval);

    for (i = 0; i <  HAL_NGROUPS; i++) {
	if (reports[i] != NULL) {
	    retval = halpr_foreach_member(reports[i]->group->name, NULL, member_cb,
					  "group=%s id=%d arg1=%d arg2=%d serial=%d\n");
	}
    }
    hal_ready(comp_id);
    return 0;
}

int setup_io()
{
    int major, minor, patch, retval;
    struct stat st;

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

#if defined(ZMQ_MONITOR)
    if (conf.zmq_monitor) {
	// set socket monitor to trace ZMQ events
	retval = zctx_set_monitor(ctx, socket_monitor);
	if (retval) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: zmq_ctx_set_monitor() failed: %s\n",
			conf.progname, zmq_strerror (errno));
	}
    }
#endif
    memset(&st, 0, sizeof(st));
    do {
	retval = stat(conf.pipe_name, &st);
	if (retval < 0) {
	    if (errno == ENOENT) { // doesnt exist - create fifo
		if (mkfifo(conf.pipe_name, S_IRWXU) < 0) {
		    rtapi_print_msg(RTAPI_MSG_ERR,
				    "%s: ERROR: cant create pipe %s: %s\n",
				    conf.progname, conf.pipe_name,
				    strerror (errno));
		    return -1;
		}
		rtapi_print_msg(RTAPI_MSG_INFO,
				"%s: created pipe %s\n",
				conf.progname, conf.pipe_name);

	    } else {
		rtapi_print_msg(RTAPI_MSG_INFO,
				"%s: ERROR: stat(%s):  %s\n",
				conf.progname, conf.pipe_name,
				strerror (errno));
		return -1;
	    }
	} else {
	    if (!S_ISFIFO(st.st_mode)) {
		rtapi_print_msg(RTAPI_MSG_INFO,
				"%s: %s exists, but not a pipe - unlinking\n",
				conf.progname, conf.pipe_name);
		if (unlink(conf.pipe_name) < 0) {
		    rtapi_print_msg(RTAPI_MSG_ERR,
				"%s: ERROR: unlink(%s): %s\n",
				conf.progname, conf.pipe_name,
				strerror (errno));
		    return -1;
		}
	    }
	}
    } while (!S_ISFIFO(st.st_mode));
    // conf.pipe_name exists and is a fifo
    pipe_fd = open(conf.pipe_name, O_RDONLY|O_NONBLOCK);
    if (pipe_fd < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: cant open pipe %s: %s\n",
			conf.progname, conf.pipe_name,
			strerror (errno));
	return -1;
    }
    if (conf.sysfs) {
	sysfs_fd = open(conf.sysfs, O_RDONLY);
	if (sysfs_fd < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: cant open sysfs entry %s: %s\n",
			    conf.progname, conf.sysfs,
			    strerror (errno));
	    return -1;
	}
    }
    return 0;
}

int detect_changes(report_t *rep)
{
    int i;
    hal_sig_t *sig;
    int nchanged = 0, nshadow = 0;
    hal_data_u *track;
    void *current;

    memset(rep->changed, 0, BITNSLOTS(rep->n_monitored));
    for (i = 0; i < rep->n_members; i++) {
	if (!(rep->member_attributes[i] & MEMBER_CHANGE_DETECT))
	    continue;
	sig = rep->member_signals[i];
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
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: detect_changes(%s): invalid type for signal %s: %d\n",
			    conf.progname, rep->group->name, sig->name, sig->type);
	    return -1;
	}
	nshadow++;
    }
    return nchanged;
}

int assemble_cb(hal_group_t *group, hal_member_t *member, void  *cb_data)
{
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
    return 0; // run to end.
}

int setup_reports()
{
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
    return 0;
}
 /* msg.set_type(MT_SET_HAL_SIGNAL); */

 /*  cmd = msg.mutable_command();  */
 /*  cmd->set_serial(56789); */
 /*  cmd->set_rsvp(NONE); */

 /*  origin = cmd->mutable_origin(); */
 /*  origin->set_origin(PROCESS); */
 /*  origin->set_name(argv[0]); */
 /*  origin->set_id(getpid()); */

 /*    // repeated Object           args   = 50; */
 /*    // optional int32         timestamp = 60; */

 /*  o = cmd->add_args(); */
 /*  o->set_type(HAL_PIN); */
 /*  pin = o->mutable_pin(); */

 /*  pin->set_type(HAL_S32); */
 /*  pin->set_name("foo.1.bar"); */
 /*  pin->set_hals32(4711); */

static Signal default_signal = SIGNAL__INIT;
static Pin default_pin = PIN__INIT;
static int serial;

int pb_encode_report(report_t *rep, void *socket, int cause, int n_changes)
{
    void *buf;
    unsigned len;
    int i;
    void *valptr;
    zframe_t *frame;

    hal_sig_t *hsig;
    hal_pin_t *hpin;

    Msg  msg = MSG__INIT;
    Command command = COMMAND__INIT;
    Originator origin = ORIGINATOR__INIT;

    Pin *pins, *pp;
    Signal *signals, *sp;
    Object *objects, *op;

    msg.type = MSG_TYPE__MT_HAL_UPDATE;

    origin.has_id = origin.has_origin = 1;
    origin.name = rep->group->name;
    origin.id = conf.pid;
    origin.origin = ORIGIN_TYPE__GROUP;

    command.serial = serial++;
    command.has_rsvp = 1;
    command.rsvp = REPLY_REQUIRED__ON_RECEPTION;
    command.origin = &origin;
    command.has_timestamp = 1;
    command.timestamp = time(NULL);

    // repeated Signal submessage
    //command.n_signals = rep->n_members;
    command.signals = alloca(sizeof(Signal *) *rep->n_members);
    signals = alloca(sizeof(Signal) * rep->n_members);
    for (i = 0; i < rep->n_members; i++) {

	signals[i] = default_signal;

	sp = &signals[i];
	sig = rep->member_signals[i];
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
	command.signals[i] = &signals[i];
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
    report_t *rep;

    for (i = 0; i < HAL_NGROUPS; i++) {
	if (events & (1 << i)) {
	    rep = reports[i];
	    if (rep) {
		// the PUB topic - use the HAL group name
		if (zstr_sendm (publisher, rep->group->name) < 0) {
		    rtapi_print_msg(RTAPI_MSG_ERR,"zstr_sendm(%s) failed: %s\n",
				    rep->group->name, zmq_strerror (errno));
		}
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

int s_handle_pipe(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    char buffer[20], *endptr;
    unsigned eventmask;
    int retval;

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: s_handle_pipe revents=0x%x\n",
		    conf.progname, poller->revents);


    // if 'echo 123 >pipe' is executed, this handler is called twice:
    // - first time when there's actually something to read
    // - a second time when the pipe write side is closed

    // depending on how libzmq is configured, revents may be subtly different
    // on pipe write side close:
    // if configured with --with-poller=poll|epoll, a ZMQ_POLLERR is signaled
    // if configured with --with-poller=select, a ZMQ_POLLIN  is signaled on

    if (poller->revents & (ZMQ_POLLIN|ZMQ_POLLERR)) {
	// check if pipe readable
	retval = read(poller->fd, buffer, sizeof(buffer));
	if (retval == 0) {
	    // the write side was closed.
	    zloop_poller_end(loop, poller);
	    // close & reopen pipe,
	    close(poller->fd);
	    poller->fd = pipe_fd = open(conf.pipe_name, O_RDONLY|O_NONBLOCK);
	    if (poller->fd < 0) {
		// real bad - pipe removed?
		rtapi_print_msg(RTAPI_MSG_ERR,
				"%s: ERROR: reopening pipe %s : %s - disabling pipe notifications\n",
				conf.progname, conf.pipe_name,
				strerror (errno));
		return 0;
	    }
	    // reestablish zloop poller.
	    zloop_poller(loop, poller, s_handle_pipe, NULL);

	} else if (retval < 0) {
	    // 'should not happen'
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: read(%s): %s - disabling pipe notifications\n",
			    conf.progname, conf.pipe_name,
			    strerror (errno));
	} else {
	    if (buffer[retval-1] == '\n')
		buffer[retval-1] = '\0';
	    eventmask = strtoul(buffer, &endptr, 0);
	    if (endptr && !(*endptr == '\0') && !isspace(*endptr)) {
		rtapi_print_msg(RTAPI_MSG_INFO,
				"%s: reading pipe: '%s': unrecognized characters '%s'\n",
				conf.progname, buffer, endptr);
	    } else {
		send_report(REPORT_CAUSE__NOTIFY, eventmask);
	    }
	}
    }
    return 0;
}

int s_handle_sysfs(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    char buffer[256], *endptr;
    unsigned eventmask;
    int retval;

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: s_handle_sysfs revents=0x%x\n",
		    conf.progname, poller->revents);

    // with a sysfs device, poll returns POLLPRI|POLLERR which is mapped
    // to ZMQ_POLLERR by libzmq; same for select() version of libzmq.

    if (poller->revents & ZMQ_POLLERR) {
	lseek(poller->fd, 0, SEEK_SET);
	retval = read(poller->fd, buffer, sizeof(buffer));
	if (retval > 0) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s: s_handle_sysfs read(%d): '%s'\n",
			    conf.progname, retval, buffer);

	    eventmask = strtoul(buffer, &endptr, 0);
	    if (endptr && (*endptr != '\n')) {
		rtapi_print_msg(RTAPI_MSG_WARN,
				"%s: reading sysfs entry %s: '%s': unrecognized characters '%s'\n",
				conf.progname, conf.sysfs, buffer, endptr);
	    } else {
		send_report(REPORT_CAUSE__NOTIFY, eventmask);
	    }
	} else {
	    // pretty bad, but not much to do about it
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR reading sysfs entry %s - %s : disabling sysfs notifications\n",
			    conf.progname, conf.sysfs, strerror(retval));
	    zloop_poller_end (loop, poller);
	}
    }
    return 0;
}

int s_handle_timer(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    report_t *rep = arg;
    send_report(REPORT_CAUSE__PERIODIC, 1 << rep->group->id);
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
    report_t *rep;
    zmq_pollitem_t rpcserver_poller = { rpcserver, 0, ZMQ_POLLIN };
    zmq_pollitem_t signal_poller =    { 0, signal_fd,   ZMQ_POLLIN };

#if 0
    zmq_pollitem_t pipe_poller =      { 0, pipe_fd,   ZMQ_POLLIN |ZMQ_IGNERR };
    zmq_pollitem_t sysfs_poller =     { 0, sysfs_fd,  ZMQ_POLLERR|ZMQ_IGNERR };
#endif

    startup_time = time(NULL);
    loop = zloop_new();
    assert (loop);
    zloop_set_verbose (loop, conf.debug);

    zloop_poller (loop, &rpcserver_poller, s_handle_rpcserver, NULL);
    if (signal_fd > 0)
	zloop_poller (loop, &signal_poller, s_handle_signal, NULL);

#if 0
    if (pipe_fd > 0)
	zloop_poller (loop, &pipe_poller, s_handle_pipe, NULL);
    if (sysfs_fd > 0)
	zloop_poller (loop, &sysfs_poller, s_handle_sysfs, NULL);
#endif
    for (i = 0; i < HAL_NGROUPS; i++) {
	rep = reports[i];
	if (rep && (rep->group->userarg1 & (GROUP_TIMER)))
	    zloop_timer(loop, rep->group->userarg2, 0, s_handle_timer, rep);
    }
    // TODO: send an initial report here

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
    conf.pid = getpid();
    if (conf.msglevel > -1)
	rtapi_set_msg_level(conf.msglevel);


    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
	perror("sigprocmask");
    signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1)
	perror("signalfd");

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

#if defined(ZMQ_MONITOR)
static void socket_monitor (void *s, int event_, zmq_event_data_t *data_)
{
    switch (event_) {
    case ZMQ_EVENT_LISTENING:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: listening fd=%d addr=%s\n",
			data_->listening.fd,data_->listening.addr);
        break;
    case ZMQ_EVENT_ACCEPTED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: accepted fd=%d addr=%s\n",
			data_->accepted.fd,data_->accepted.addr);
	break;
    case ZMQ_EVENT_ACCEPT_FAILED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: accept failed err=%d:%s addr=%s\n",
			data_->accept_failed.err,
			zmq_strerror (data_->accept_failed.err),
			data_->accept_failed.addr);
	break;

    case ZMQ_EVENT_BIND_FAILED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: bind failed err=%d:%s addr=%s\n",
			data_->bind_failed.err,
			zmq_strerror (data_->bind_failed.err),
			data_->bind_failed.addr);
	break;

    case ZMQ_EVENT_CONNECT_RETRIED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: connect retried interval=%d addr=%s\n",
			data_->connect_retried.interval,data_->connect_retried.addr);
        break;
    case ZMQ_EVENT_CONNECTED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: connected fd=%d addr=%s\n",
			data_->connected.fd,data_->connected.addr);
        break;
    case ZMQ_EVENT_CONNECT_DELAYED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: connect delayed err=%d:%s addr=%s\n",
			data_->connect_delayed.err,
			zmq_strerror (data_->connect_delayed.err),
			data_->connect_delayed.addr);
        break;
    case ZMQ_EVENT_CLOSE_FAILED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: close failed err=%d:%s addr=%s\n",
			data_->close_failed.err,
			zmq_strerror (data_->close_failed.err),
			data_->close_failed.addr);
        break;
    case ZMQ_EVENT_CLOSED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: closed fd=%d addr=%s\n",
			data_->closed.fd,data_->closed.addr);
        break;
    case ZMQ_EVENT_DISCONNECTED:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: disconnected fd=%d addr=%s\n",
			data_->disconnected.fd,data_->disconnected.addr);
        break;
    default:
	rtapi_print_msg(RTAPI_MSG_DBG, "zmq: unexpected event  %d\n",event_);
    }
}


// temporary workaround to enable zmq_ctx_set_monitor() on a czmq context:
// typedef void (zmq_monitor_fn) (void *s, int event, zmq_event_data_t *data);
static int zctx_set_monitor(void *foo, zmq_monitor_fn func)
{
    struct _foo_ctx_t {
	void *context;              //  Our 0MQ context
	zlist_t *sockets;           //  Sockets held by this thread
	Bool main;                  //  TRUE if we're the main thread
	int iothreads;              //  Number of IO threads, default 1
	int linger;                 //  Linger timeout, default 0
	int hwm;                    //  HWM, default 1
    };

    struct _foo_ctx_t *ctx = foo;
    return  zmq_ctx_set_monitor (ctx->context, func);
}
#endif
