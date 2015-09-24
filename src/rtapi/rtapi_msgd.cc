/********************************************************************
 * Description:  the RTAPI message deamon
 *
 * polls the rtapi message ring in the global data segment and eventually logs them
 * this is the single place for RTAPI and any ULAPI processes where log messages
 * pass through, regardless of origin or thread style (kernel, rtapi_app, ULAPI)

 * doubles as zeroMQ PUBLISH server making messages available
 * to any interested subscribers
 * the PUBLISH/SUBSCRIBE pattern will also fix the current situation where an error
 * message consumed by an entity is not seen by any other entities
 *
 *
 * Copyright (C) 2012, 2013  Michael Haberler <license AT mah DOT priv DOT at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ********************************************************************/

#include "rtapi.h"
#include "rtapi_compat.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <syslog_async.h>
#include <uuid/uuid.h>
#include <string>
#include <vector>
#include <poll.h>
#include <assert.h>
#include <inifile.h>
#include <sys/resource.h>

using namespace std;

#include <rtapi.h>
#include <rtapi/shmdrv/shmdrv.h>
#include <ring.h>
#include <setup_signals.h>
#include <mk-backtrace.h>

#include <czmq.h>
#include <mk-service.hh>
#include <libwebsockets.h>  // version tags only

#include <google/protobuf/text_format.h>
#include <machinetalk/generated/message.pb.h>
using namespace google::protobuf;


#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_LOCAL1  // where all rtapi/ulapi logging goes
#endif
#define GRACE_PERIOD 2000 // ms to wait after rtapi_app exit detected

int rtapi_instance;

static const char *inifile;
static int log_stderr;
static int foreground;
static int use_shmdrv;
static flavor_ptr flavor;
static int usr_msglevel = RTAPI_MSG_INFO ;
static int rt_msglevel = RTAPI_MSG_INFO ;
static int halsize = HAL_SIZE;
static const char *instance_name;
static int hal_thread_stack_size = HAL_STACKSIZE;
static int signal_fd;
static bool trap_signals = true;
static int full, locked;
static size_t max_msgs, max_bytes; // stats

// messages tend to come bunched together, e.g during startup and shutdown
// poll faster if a message was read, and decay the poll timer up to msg_poll_max
// if no messages are pending
// this way we retrieve bunched messages fast without much overhead in idle times
static int msg_poll_max = 200; // maximum msgq checking interval, mS

#ifdef FASTLOG
static int msg_poll_min = 5;  // minimum msgq checking interval
static int msg_poll_inc = 1;  // increment interval if no message read up to msg_poll_max
static int msg_poll =     10;  // current delay; startup fast
#else
static int msg_poll_min = 10;  // minimum msgq checking interval
static int msg_poll_inc = 5;  // increment interval if no message read up to msg_poll_max
static int msg_poll =     10;  // current delay; startup fast
#endif

static int polltimer_id;      // as returned by zloop_timer()
static int shutdowntimer_id;

// zeroMQ related
static mk_netopts_t netopts;
static mk_socket_t  logpub;
static int port = -1; // defaults to ephemeral port

int shmdrv_loaded;
static const char *shmdrv_opts;
long page_size;
global_data_t *global_data;
static ringbuffer_t rtapi_msg_buffer;   // ring access strcuture for messages
static const char *progname;
static char proctitle[20];
static int exit_code  = 0;
static const char *origins[] = { "kernel", "rt", "user" };

static void usage(int argc, char **argv)
{
    printf("Usage:  %s [options]\n", argv[0]);
}

pid_t pid_of(const char *fmt, ...)
{
    char line[LINELEN];
    FILE *cmd;
    pid_t pid;
    va_list ap;

    strcpy(line, "pidof ");
    va_start(ap, fmt);
    vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, ap);
    va_end(ap);
    cmd = popen(line, "r");
    if (!fgets(line, sizeof(line), cmd))
	pid = -1;
    else
	pid = strtoul(line, NULL, 10);
    pclose(cmd);
    return pid;
}

static int create_global_segment()
{
    int retval = 0;

    int globalkey = OS_KEY(GLOBAL_KEY, rtapi_instance);
    int rtapikey = OS_KEY(RTAPI_KEY, rtapi_instance);
    int halkey = OS_KEY(HAL_KEY, rtapi_instance);

    int global_exists = shm_common_exists(globalkey);
    int hal_exists = shm_common_exists(halkey);
    int rtapi_exists = shm_common_exists(rtapikey);

    if (global_exists || rtapi_exists || hal_exists) {
	// hm, something is wrong here

	pid_t msgd_pid = pid_of("msgd:%d", rtapi_instance);

	if (rtapi_instance == kernel_instance_id()) {

	    // collision with a running kernel instance - not good.
	    int shmdrv_loaded = is_module_loaded("shmdrv");
	    int rtapi_loaded = is_module_loaded("rtapi");
	    int hal_loaded = is_module_loaded("hal_lib");

	    fprintf(stderr, "ERROR: found existing kernel "
		   "instance with the same instance id (%d)\n",
		   rtapi_instance);

	    fprintf(stderr,"kernel modules loaded: %s%s%s\n",
		   shmdrv_loaded ? "shmdrv " : "",
		   rtapi_loaded ? "rtapi " : "",
		   hal_loaded ? "hal_lib " : "");

	    if (msgd_pid > 0)
		fprintf(stderr,"the msgd process msgd:%d is "
		       "already running, pid: %d\n",
		       rtapi_instance, msgd_pid);
	    else
		fprintf(stderr,"msgd:%d not running!\n",
		       rtapi_instance);
	    return -EEXIST;
	}

	// running userthreads instance?
	pid_t app_pid = pid_of("rtapi:%d", rtapi_instance);

	if ((msgd_pid > -1) || (app_pid > -1)) {

	    fprintf(stderr, "ERROR: found existing RT "
		   "instance with the same instance id (%d)\n",
		   rtapi_instance);
	    if (msgd_pid > 0)
		fprintf(stderr,"the msgd process msgd:%d is "
		       "already running, pid: %d\n",
		       rtapi_instance, msgd_pid);
	    else
		fprintf(stderr,"msgd:%d not running!\n",
		       rtapi_instance);

	    if (app_pid > 0)
		fprintf(stderr,"the RT process rtapi:%d is "
		       "already running, pid: %d\n",
		       rtapi_instance, app_pid);
	    else
		fprintf(stderr,"the RT process rtapi:%d not running!\n",
		       rtapi_instance);

	    // TBD: might check for other user HAL processes still
	    // around. This might work with fuser on the HAL segment
	    // but might be tricky wit shmdrv.

	    return -EEXIST;
	}

	// leftover shared memory segments were around, but no using
	// entities (user process or kernel modules).
	// Remove and keep going:
	if (shmdrv_loaded) {
	    // since neiter rtapi.ko nor hal_lib.ko is loaded
	    // cause a garbage collect in shmdrv
	    shmdrv_gc();
	} else {
	    // Posix shm case.
	    char segment_name[LINELEN];

	    if (hal_exists) {
		sprintf(segment_name, SHM_FMT, rtapi_instance, halkey);
		fprintf(stderr,"warning: removing unused HAL shm segment %s\n",
			segment_name);
		if (shm_unlink(segment_name))
		    perror(segment_name);
	    }
	    if (rtapi_exists) {
		sprintf(segment_name, SHM_FMT, rtapi_instance, rtapikey);
		fprintf(stderr,"warning: removing unused RTAPI"
			" shm segment %s\n",
			segment_name);
		if (shm_unlink(segment_name))
		    perror(segment_name);
	    }
	    if (global_exists) {
		sprintf(segment_name, SHM_FMT, rtapi_instance, globalkey);
		fprintf(stderr,"warning: removing unused global"
			" shm segment %s\n",
			segment_name);
		if (shm_unlink(segment_name))
		    perror(segment_name);
	    }
	}
    }

    // now try again:
    if (shm_common_exists(globalkey)) {
	fprintf(stderr,
		"MSGD:%d ERROR: found existing global segment key=0x%x\n",
		rtapi_instance, globalkey);

	return -EEXIST;
    }

    int size = sizeof(global_data_t);

    retval = shm_common_new(globalkey, &size,
			    rtapi_instance, (void **) &global_data, 1);
    if (retval < 0) {
	fprintf(stderr,
		"MSGD:%d ERROR: cannot create global segment key=0x%x %s\n",
	       rtapi_instance, globalkey, strerror(-retval));
    }
    if (size != sizeof(global_data_t)) {
	fprintf(stderr,
		"MSGD:%d ERROR: global segment size mismatch: expect %zu got %d\n",
	       rtapi_instance, sizeof(global_data_t), size);
	return -EINVAL;
    }
    return retval;
}

// salvaged from rtapi_shmem.c - msgd doesnt link against rtapi though
static void check_memlock_limit(const char *where)
{
    struct rlimit lim;
    int result;

    result = getrlimit(RLIMIT_MEMLOCK, &lim);
    if(result < 0) { perror("getrlimit"); return; }
    if(lim.rlim_cur == (rlim_t)-1) return; // unlimited
    if(lim.rlim_cur >= RLIMIT_MEMLOCK_RECOMMENDED) return; // limit is at least recommended
    syslog_async(LOG_ERR, "Locked memory limit is %luKiB, recommended at least %luKiB.",
		 (unsigned long)lim.rlim_cur/1024, RLIMIT_MEMLOCK_RECOMMENDED/1024);
    syslog_async(LOG_ERR, "This can cause the error '%s'.", where);
    syslog_async(LOG_ERR, "For more information, see "
		 "http://wiki.linuxcnc.org/cgi-bin/emcinfo.pl?LockedMemory\n");
}

static int init_global_data(global_data_t * data, int flavor,
		      int instance_id, int hal_size,
		      int rt_level, int user_level,
		      const char *name, int stack_size,
		      const char *service_uuid)
{
    int retval = 0;

    // force-lock - we're first, so thats a bit theoretical
    rtapi_mutex_try(&(data->mutex));
    // touch all memory exposed to RT
    memset(data, 0, sizeof(global_data_t));

    // lock the global data segment
    if (flavor != RTAPI_POSIX_ID) {
	if (mlock(data, sizeof(global_data_t))) {
	    const char *errmsg = strerror(errno);
	    syslog_async(LOG_ERR, "mlock(global) failed: %d '%s'\n",
			 errno, errmsg);
	    check_memlock_limit(errmsg);
	}
    }
    // report progress
    data->magic = GLOBAL_INITIALIZING;
    /* set version code so other modules can check it */
    data->layout_version = GLOBAL_LAYOUT_VERSION;
    data->instance_id = instance_id;

    // separate message levels for RT and userland
    data->rt_msg_level = rt_level;
    data->user_msg_level = user_level;

    // counter for unique handles within an RTAPI instance
    // guaranteed not to collide with a any module ID, so start at
    // RTAPI_MAX_MODULES + 1 (relevant for khreads)
    // uthreads use arbitrary ints since those dont use fixed-size arrays
    data->next_handle = RTAPI_MAX_MODULES + 1;

    // tell the others what we determined as the proper flavor
    data->rtapi_thread_flavor = flavor;

    // HAL segment size
    data->hal_size = hal_size;

    // stack size passed to rtapi_task_new() in hal_create_thread()
    data->hal_thread_stack_size = stack_size;

    // export the service UUID in the global data segment
    // in binary form
    if (uuid_parse(service_uuid, data->service_uuid)) {
	// syntax error in UUID
	syslog_async(LOG_ERR,"%s %s: syntax error in UUID '%s'",
		     progname, __func__, service_uuid);
	retval--;
    }

    // init the error ring
    ringheader_init(&data->rtapi_messages, 0, SIZE_ALIGN(MESSAGE_RING_SIZE), 0);
    memset(&data->rtapi_messages.buf[0], 0, SIZE_ALIGN(MESSAGE_RING_SIZE));

    // attach to the message ringbuffer
    ringbuffer_init(&data->rtapi_messages, &rtapi_msg_buffer);
    rtapi_msg_buffer.header->refcount = 1; // rtapi not yet attached
    rtapi_msg_buffer.header->reader = getpid();
    data->rtapi_messages.use_wmutex = 1; // locking hint

    // demon pids
    data->rtapi_app_pid = -1; // not yet started
    data->rtapi_msgd_pid = 0;

    // for now, use a fixed-size memory arena
    rtapi_heap_init(&data->heap);
    rtapi_heap_addmem(&data->heap, data->arena, GLOBAL_HEAP_SIZE);

    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return retval;
}

// determine if we can run this flavor on the current kernel
static int flavor_and_kernel_compatible(flavor_ptr f)
{
    int retval = 1;

    if (f->id == RTAPI_POSIX_ID)
	return 1; // no prerequisites

    if (kernel_is_xenomai()) {
	if (f->id == RTAPI_RT_PREEMPT_ID) {
	    fprintf(stderr,
		    "MSGD:%d Warning: starting %s RTAPI on a Xenomai kernel\n",
		    rtapi_instance, f->name);
	    return 1;
	}
	if ((f->id != RTAPI_XENOMAI_ID) &&
	    (f->id != RTAPI_XENOMAI_KERNEL_ID)) {
	    fprintf(stderr,
		    "MSGD:%d ERROR: trying to start %s RTAPI on a Xenomai kernel\n",
		    rtapi_instance, f->name);
	    return 0;
	}
    }

    if (kernel_is_rtai() &&
	(f->id != RTAPI_RTAI_KERNEL_ID)) {
	fprintf(stderr, "MSGD:%d ERROR: trying to start %s RTAPI on an RTAI kernel\n",
		    rtapi_instance, f->name);
	return 0;
    }

    if (kernel_is_rtpreempt() &&
	(f->id != RTAPI_RT_PREEMPT_ID)) {
	fprintf(stderr, "MSGD:%d ERROR: trying to start %s RTAPI on an RT PREEMPT kernel\n",
		rtapi_instance, f->name);
	return 0;
    }
    return retval;
}

// actions common to sigaction and signalfd()
static void start_shutdown(int signal)
{
    if (global_data) {
	global_data->magic = GLOBAL_EXITED;
	global_data->rtapi_msgd_pid = 0;
    }
    // no point in keeping rtapi_app running if msgd exits
    if (global_data->rtapi_app_pid > 0) {
	kill(global_data->rtapi_app_pid, SIGTERM);
	syslog_async(LOG_ERR, "msgd:%d: got signal %d - sending SIGTERM to rtapi (pid %d)\n",
		     rtapi_instance, signal, global_data->rtapi_app_pid);
    }
}

static void btprint(const char *prefix, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsyslog_async(LOG_ERR, fmt, args);
}

// handle signals delivered via sigaction - not all signals
// can be dealt with through signalfd(2)
// log, try to do something sane, and dump core
static void sigaction_handler(int sig, siginfo_t *si, void *uctx)
{
    start_shutdown(sig);
    syslog_async(LOG_ERR,
		 "msgd:%d: signal %d - '%s' received, dumping core (current dir=%s)",
		 rtapi_instance, sig, strsignal(sig), get_current_dir_name());

    backtrace("", "msgd", btprint, 3);

    closelog_async(); // let syslog_async drain
    sleep(1);
    // reset handler for current signal to default
    signal(sig, SIG_DFL);
    // and re-raise so we get a proper core dump and stacktrace
    kill(getpid(), sig);
    sleep(1);
}

static int s_handle_signal(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{

    struct signalfd_siginfo fdsi;
    ssize_t s = read(poller->fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
	perror("read");
    }

    start_shutdown(fdsi.ssi_signo);

    switch (fdsi.ssi_signo) {
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
    case SIGKILL:
	syslog_async(LOG_INFO, "msgd:%d: %s - shutting down\n",
	       rtapi_instance, strsignal(fdsi.ssi_signo));

	// hint if error ring couldnt be served fast enough,
	// or there was contention
	// none observed so far - this might be interesting to hear about
	if (global_data && (global_data->error_ring_full ||
			    global_data->error_ring_locked))
	    syslog_async(LOG_INFO, "msgd:%d: message ring stats: full=%d locked=%d ",
		   rtapi_instance,
		   global_data->error_ring_full,
		   global_data->error_ring_locked);
	return -1; // exit reactor normally
	break;

    default:
	// this should be handled either above or in sigaction_handler
	syslog_async(LOG_ERR, "msgd:%d:  BUG: unhandled signal %d - '%s' received\n",
		     rtapi_instance,	fdsi.ssi_signo, strsignal(fdsi.ssi_signo));
	break;
    }
    return 0; // continue reactor
}

static void
cleanup_actions(void)
{
    int retval;
    size_t max_ringmem = max_bytes + max_msgs * 8; // includes record overhead
    syslog_async(LOG_DEBUG,"log buffer hwm: %zu% (%zu msgs, %zu bytes out of %d)",
		 (max_ringmem*100)/MESSAGE_RING_SIZE,
		 max_msgs, max_ringmem, MESSAGE_RING_SIZE);

    if (global_data) {
	if (global_data->rtapi_app_pid > 0) {
	    kill(global_data->rtapi_app_pid, SIGTERM);
	    syslog_async(LOG_INFO,"sent SIGTERM to rtapi (pid %d)\n",
		   global_data->rtapi_app_pid);
	}
	// in case some process catches a leftover shm segment
	global_data->magic = GLOBAL_EXITED;
	global_data->rtapi_msgd_pid = 0;
	if (rtapi_msg_buffer.header != NULL)
	    rtapi_msg_buffer.header->refcount--;
	retval = shm_common_detach(sizeof(global_data_t), global_data);
	if (retval < 0) {
	    syslog_async(LOG_ERR,"shm_common_detach(global) failed: %s\n",
		   strerror(-retval));
	} else {
	    shm_common_unlink(OS_KEY(GLOBAL_KEY, rtapi_instance));
	    syslog_async(LOG_DEBUG,"normal shutdown - global segment detached");
	}
	global_data = NULL;
    }
}

// react to subscribe/unsubscribe events
static int logpub_readable_cb(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    zframe_t *f = zframe_recv(poller->socket);
    const char *s = (const char *) zframe_data(f);
    syslog_async(LOG_ERR, "%s subscribe on '%s'",
		 *s ? "start" : "stop", s+1);
    zframe_destroy(&f);
    return 0;
}

static int
s_start_shutdown_cb(zloop_t *loop, int  timer_id, void *args)
{
    syslog_async(LOG_ERR, "msgd shutting down");
    return -1; // exit reactor
}

static int
message_poll_cb(zloop_t *loop, int  timer_id, void *args)
{
    rtapi_msgheader_t *msg;
    size_t msg_size;
    size_t payload_length;
    int retval;
    char *cp;
    pb::Container container;
    pb::LogMessage *logmsg;
    zframe_t *z_pbframe;
    int current_interval = msg_poll;

    if (global_data->error_ring_full > full) {
	syslog_async(LOG_ERR, "msgd:%d: message ring overrun (full): %d messages lost",
		     global_data->error_ring_full - full);
	full = global_data->error_ring_full;
    }
    if (global_data->error_ring_locked > locked) {
	syslog_async(LOG_ERR, "msgd:%d: message ring overrun (locked): %d messages lost",
		     global_data->error_ring_locked - locked);
	locked = global_data->error_ring_locked;
    }

    size_t n_msgs = 0, n_bytes = 0;

    while ((retval = record_read(&rtapi_msg_buffer,
				 (const void **) &msg, &msg_size)) == 0) {
	payload_length = msg_size - sizeof(rtapi_msgheader_t);
	n_msgs++;
	n_bytes += msg_size;

	switch (msg->encoding) {
	case MSG_ASCII:
	    // strip trailing newlines
	    while ((cp = strrchr(msg->buf,'\n')))
		*cp = '\0';
	    syslog_async(rtapi2syslog(msg->level), "%s:%d:%s %.*s",
		   msg->tag, msg->pid, origins[msg->origin],
		   (int) payload_length, msg->buf);


	    if (logpub.socket) {
		// publish protobuf-encoded log message
		container.set_type(pb::MT_LOG_MESSAGE);

		struct timespec timestamp;
		clock_gettime(CLOCK_REALTIME, &timestamp);
		container.set_tv_sec(timestamp.tv_sec);
		container.set_tv_nsec(timestamp.tv_nsec);

		logmsg = container.mutable_log_message();
		logmsg->set_origin((pb::MsgOrigin)msg->origin);
		logmsg->set_pid(msg->pid);
		logmsg->set_level((pb::MsgLevel) msg->level);
		logmsg->set_tag(msg->tag);
		logmsg->set_text(msg->buf, strlen(msg->buf));

		z_pbframe = zframe_new(NULL, container.ByteSize());
		assert(z_pbframe != NULL);

		if (container.SerializeWithCachedSizesToArray(zframe_data(z_pbframe))) {
		    // channel name:
		    if (zstr_sendm(logpub.socket, "log"))
			syslog_async(LOG_ERR,"zstr_sendm(): %s", strerror(errno));

		    // and the actual pb2-encoded message
		    // zframe_send() deallocates the frame after sending,
		    // and frees pb_buffer through zfree_cb()
		    if (zframe_send(&z_pbframe, logpub.socket, 0))
			syslog_async(LOG_ERR,"zframe_send(): %s", strerror(errno));

		} else {
		    syslog_async(LOG_ERR, "container serialization failed");
		}
	    }
	    break;
	case MSG_STASHF:
	    break;
	case MSG_PROTOBUF:
	    break;
	default: ;
	    // whine
	}
	record_shift(&rtapi_msg_buffer);
	msg_poll = msg_poll_min; // keep going quick
    }
    // done - decay the timer
    msg_poll += msg_poll_inc;
    if (msg_poll > msg_poll_max)
	msg_poll = msg_poll_max;

    // update stats
    if (n_msgs > max_msgs)
	max_msgs = n_msgs;
    if (n_bytes > max_bytes)
	max_bytes = n_bytes;

    if (current_interval != msg_poll) {
	zloop_timer_end(loop, polltimer_id);
	polltimer_id = zloop_timer (loop, current_interval, 0, message_poll_cb, NULL);
    }

    // check for rtapi_app exit only after all pending messages are logged:
    if ((global_data->rtapi_app_pid == 0) &&
	(shutdowntimer_id ==  0)) {
	// schedule a loop shutdown but keep reading messages for a while
	// so we dont loose messages
	syslog_async(LOG_ERR, "rtapi_app exit detected - scheduled shutdown");
	shutdowntimer_id = zloop_timer (loop, GRACE_PERIOD, 1,
					s_start_shutdown_cb, NULL);
    }
    return 0;
}


static struct option long_options[] = {
    { "help",  no_argument,          0, 'h'},
    { "stderr",  no_argument,        0, 's'},
    { "foreground",  no_argument,    0, 'F'},
    { "usrmsglevel", required_argument, 0, 'u'},
    { "rtmsglevel", required_argument, 0, 'r'},
    { "instance", required_argument, 0, 'I'},
    { "instance_name", required_argument, 0, 'i'},
    { "ini",      required_argument, 0, 'M'},     // default: getenv(INI_FILE_NAME)
    { "flavor",   required_argument, 0, 'f'},
    { "halsize",  required_argument, 0, 'H'},
    { "halstacksize",  required_argument, 0, 'T'},
    { "shmdrv",  no_argument,        0, 'S'},
    { "port",	required_argument, NULL, 'p' },
    { "svcuuid", required_argument, 0, 'R'},
    { "interfaces", required_argument, 0, 'n'},
    { "shmdrv_opts", required_argument, 0, 'o'},
    { "nosighdlr",   no_argument,    0, 'G'},

    {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
    int c, i, retval;
    int syslog_async_option = LOG_NDELAY;
    int syslog_async_delay = 1000;
    pid_t pid, sid;
    size_t argv0_len, procname_len, max_procname_len;

    inifile = getenv("MACHINEKIT_INI");

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    progname = argv[0];
    shm_common_init();

    while (1) {
	int option_index = 0;
	int curind = optind;
	c = getopt_long (argc, argv, "GhI:sFf:i:SW:u:r:T:M:p:",
			 long_options, &option_index);
	if (c == -1)
	    break;
	switch (c)	{
	case 'G':
	    trap_signals = false; // ease debugging with gdb
	    break;
	case 'F':
	    foreground++;
	    break;
	case 'I':
	    rtapi_instance = atoi(optarg);
	    break;
	case 'p':
	    port = atoi(optarg);
	    break;
	case 'T':
	    hal_thread_stack_size = atoi(optarg);
	    break;
	case 'i':
	    instance_name = strdup(optarg);
	    break;
	case 'M':
	    inifile = strdup(optarg);
	    break;
	case 'f':
	    if ((flavor = flavor_byname(optarg)) == NULL) {
		fprintf(stderr, "no such flavor: '%s' -- valid flavors are:\n", optarg);
		flavor_ptr f = flavors;
		while (f->name) {
		    fprintf(stderr, "\t%s\n", f->name);
		    f++;
		}
		exit(1);
	    }
	    break;
	case 'u':
	    usr_msglevel = atoi(optarg);
	    break;
	case 'r':
	    rt_msglevel = atoi(optarg);
	    break;
	case 'H':
	    halsize = atoi(optarg);
	    break;
	case 'S':
	    use_shmdrv++;
	    break;
	case 'o':
	    shmdrv_opts = strdup(optarg);
	    break;
	case 's':
	    log_stderr++;
	    syslog_async_option |= LOG_PERROR;
	    syslog_async_delay = 0;
	    break;
	case 'R':
	    netopts.service_uuid = strdup(optarg);
	    break;
	case '?':
	    if (optopt)  fprintf(stderr, "bad short opt '%c'\n", optopt);
	    else  fprintf(stderr, "bad long opt \"%s\"\n", argv[curind]);
	    exit(1);
	    break;
	default:
	    usage(argc, argv);
	    exit(0);
	}
    }

    if (trap_signals && (getenv("NOSIGHDLR") != NULL))
	trap_signals = false;

    // sanity
    if (getuid() == 0) {
	fprintf(stderr, "%s: FATAL - will not run as root\n", progname);
	exit(EXIT_FAILURE);
    }
    if (geteuid() == 0) {
	fprintf(stderr, "%s: FATAL - will not run as setuid root\n", progname);
	exit(EXIT_FAILURE);
    }

    if (flavor == NULL)
	flavor = default_flavor();

    if (flavor == NULL) {
	fprintf(stderr, "%s: FATAL - failed to detect thread flavor\n", progname);
	exit(EXIT_FAILURE);
    }

    // can we actually run what's being suggested?
    if (!flavor_and_kernel_compatible(flavor)) {
	fprintf(stderr, "%s: FATAL - cant run the %s flavor on this kernel\n",
		progname, flavor->name);
	exit(EXIT_FAILURE);
    }

    // catch installation error: user not in xenomai group
    if (flavor->id == RTAPI_XENOMAI_ID) {
	int retval = user_in_xenomai_group();

	switch (retval) {
	case 1:  // yes
	    break;
	case 0:
	    fprintf(stderr, "this user is not member of group xenomai\n");
	    fprintf(stderr, "please 'sudo adduser <username>  xenomai',"
		    " logout and login again\n");
	    exit(EXIT_FAILURE);

	default:
	    fprintf(stderr, "cannot determine if this user "
		    "is a member of group xenomai: %s\n",
		    strerror(-retval));
	    exit(EXIT_FAILURE);
	}
    }

    // do we need the shmdrv module?
    if (((flavor->flags & FLAVOR_KERNEL_BUILD) ||
	 use_shmdrv) &&
	!shmdrv_available()) {

	if (shmdrv_opts == NULL)
	    shmdrv_opts = getenv("SHMDRV_OPTS");
	if (shmdrv_opts == NULL)
	    shmdrv_opts = "";

	if (run_module_helper("insert shmdrv %s",shmdrv_opts)) {
	    fprintf(stderr, "%s: cant insert shmdrv module - needed by %s\n",
		    progname, use_shmdrv ? "--shmdrv" : flavor->name);
	    exit(EXIT_FAILURE);
	}

	shm_common_init();
	if (!shmdrv_available()) {
	    fprintf(stderr, "%s: BUG: shmdrv module not detected\n",
		    progname);
	    exit(EXIT_FAILURE);
	}
    }

    // the global segment every entity in HAL/RTAPI land attaches to
    if ((retval = create_global_segment()) != 1) // must be a new shm segment
	exit(retval);

    // good to go
    if (!foreground) {
        pid = fork();
        if (pid < 0) {
	    exit(EXIT_FAILURE);
        }
        if (pid > 0) {
	    exit(EXIT_SUCCESS);
        }
        umask(0);
        sid = setsid();
        if (sid < 0) {
	    exit(EXIT_FAILURE);
        }
    }

    snprintf(proctitle, sizeof(proctitle), "msgd:%d",rtapi_instance);
    backtrace_init(proctitle);

    openlog_async(proctitle, syslog_async_option, SYSLOG_FACILITY);
    // tune async syslog buffers:  max buffer size; 0 delay for stdout, else 1s
    tunelog_async(99,syslog_async_delay);

    // set new process name
    argv0_len = strlen(argv[0]);
    procname_len = strlen(proctitle);
    max_procname_len = (argv0_len > procname_len) ? (procname_len) : (argv0_len);

    strncpy(argv[0], proctitle, max_procname_len);
    memset(&argv[0][max_procname_len], '\0', argv0_len - max_procname_len);

    for (i = 1; i < argc; i++)
	memset(argv[i], '\0', strlen(argv[i]));


    // suppress default handling of signals in zctx_new()
    // since we're using signalfd()
    zsys_handler_set(NULL);

    netopts.rundir = RUNDIR;
    netopts.rtapi_instance = rtapi_instance;

    netopts.z_context = zctx_new ();
    assert(netopts.z_context);

    netopts.z_loop = zloop_new ();
    assert(netopts.z_loop);

    netopts.av_loop = avahi_czmq_poll_new(netopts.z_loop);
    assert(netopts.av_loop);

    // generic binding & announcement parameters
    // from $MACHINEKIT_INI
    if (mk_getnetopts(&netopts))
	exit(1);

    // this is the single place in all of linuxCNC where the global segment
    // gets initialized - no reinitialization from elsewhere
    if (init_global_data(global_data, flavor->id, rtapi_instance,
			 halsize, rt_msglevel, usr_msglevel,
			 instance_name,hal_thread_stack_size,
			 netopts.service_uuid)) {
	syslog_async(LOG_ERR, "%s: startup failed, exiting\n",
		     progname);
	exit(EXIT_FAILURE);
    } else {
	syslog_async(LOG_INFO,
		     "startup pid=%d flavor=%s "
		     "rtlevel=%d usrlevel=%d halsize=%d shm=%s gcc=%s version=%s",
		     getpid(),
		     flavor->name,
		     global_data->rt_msg_level,
		     global_data->user_msg_level,
		     global_data->hal_size,
		     shmdrv_loaded ? "shmdrv" : "Posix",
		     __VERSION__,
		     GIT_VERSION);
    }
    int major, minor, patch;
    zmq_version (&major, &minor, &patch);
    syslog_async(LOG_DEBUG,
		 "ØMQ=%d.%d.%d czmq=%d.%d.%d protobuf=%d.%d.%d libwebsockets=%s %s\n",
		 major, minor, patch,
		 CZMQ_VERSION_MAJOR, CZMQ_VERSION_MINOR,CZMQ_VERSION_PATCH,
		 GOOGLE_PROTOBUF_VERSION / 1000000,
		 (GOOGLE_PROTOBUF_VERSION / 1000) % 1000,
 		 GOOGLE_PROTOBUF_VERSION % 1000,
#ifdef LWS_LIBRARY_VERSION
		 LWS_LIBRARY_VERSION,
#else
		 "<no version symbol>",
#endif
#ifdef LWS_BUILD_HASH
		 LWS_BUILD_HASH
#else
		 ""
#endif
		 );

    syslog_async(LOG_INFO,"configured: sha=%s", GIT_CONFIG_SHA);
    syslog_async(LOG_INFO,"built:      %s %s sha=%s",  __DATE__, __TIME__, GIT_BUILD_SHA);
    if (strcmp(GIT_CONFIG_SHA,GIT_BUILD_SHA))
	syslog_async(LOG_WARNING, "WARNING: git SHA's for configure and build do not match!");


   if ((global_data->rtapi_msgd_pid != 0) &&
	kill(global_data->rtapi_msgd_pid, 0) == 0) {
	syslog_async(LOG_ERR, "%s: another rtapi_msgd is already running (pid %d), exiting\n",
	       progname, global_data->rtapi_msgd_pid);
	exit(EXIT_FAILURE);
    }

    int fd = open("/dev/null", O_RDONLY);
    dup2(fd, STDIN_FILENO);
    close(fd);

    if (trap_signals) {
	signal_fd = setup_signals(sigaction_handler, SIGINT, SIGQUIT, SIGKILL, SIGTERM, -1);
	assert(signal_fd > -1);
    }


    // pull msgd-specific port value from MACHINEKIT_INI
    iniFindInt(netopts.mkinifp, "LOGPUB_PORT", "MACHINEKIT", &port);

    logpub.port = port;
    logpub.dnssd_subtype = LOG_DNSSD_SUBTYPE;
    logpub.tag = "log";
    logpub.socket = zsocket_new (netopts.z_context, ZMQ_XPUB);

    zsocket_set_xpub_verbose (logpub.socket, 1);  // enable reception
    zsocket_set_linger(logpub.socket, 0);

    if (mk_bindsocket(&netopts, &logpub))
	return -1;
    //    assert(logpub.port > -1);

    if (mk_announce(&netopts, &logpub, "Log service", NULL))
	return -1;

    zmq_pollitem_t signal_poller =  { 0, signal_fd,   ZMQ_POLLIN };
    if (trap_signals)
	zloop_poller (netopts.z_loop, &signal_poller, s_handle_signal, NULL);

    if (logpub.socket) {
	zmq_pollitem_t logpub_poller = { logpub.socket, 0, ZMQ_POLLIN };
	zloop_poller (netopts.z_loop, &logpub_poller, logpub_readable_cb, NULL);
    }

    polltimer_id = zloop_timer (netopts.z_loop, msg_poll, 0, message_poll_cb, NULL);
    global_data->rtapi_msgd_pid = getpid();
    global_data->magic = GLOBAL_READY;

    do {
	retval = zloop_start(netopts.z_loop);
    } while (!(retval || zctx_interrupted));

    // stop the service announcement
    mk_withdraw(&logpub);

    // deregister poll adapter
    if (netopts.av_loop)
        avahi_czmq_poll_free(netopts.av_loop);

    // shutdown zmq context
    zctx_destroy(&netopts.z_context);

    cleanup_actions();
    closelog();
    exit(exit_code);
}
