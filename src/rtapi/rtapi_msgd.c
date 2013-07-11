// the RTAPI message deamon
//
// polls the rtapi message ring in the global data segment and eventually logs them
// this is the single place for RTAPI and any ULAPI processes where log messages
// pass through, regardless of origin or thread style (kernel, rtapi_app, ULAPI)

// eventually this will become a zeroMQ PUBLISH server making messages available
// to any interested subscribers
// the PUBLISH/SUBSCRIBE pattern will also fix the current situation where an error 
// message consumed by an entity is not seen by any other entities
//
// Michael Haberler fecit A.D. 2013

#include "rtapi.h"

#include <sys/types.h>
#include <signal.h>
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
#include <syslog.h>

#include <rtapi.h>
#include "rtapi/shmdrv/shmdrv.h"
#include "rtapi_kdetect.h"          // environment autodetection

#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_LOCAL1  // where all rtapi/ulapi logging goes
#endif

int rtapi_instance;
static int log_stderr;
static int foreground;
static int use_shmdrv;
static flavor_ptr flavor;
static int usr_msglevel = RTAPI_MSG_INFO ;
static int rt_msglevel = RTAPI_MSG_INFO ;
static int halsize = HAL_SIZE;
static const char *instance_name;

// messages tend to come bunched together, e.g during startup and shutdown
// poll faster if a message was read, and decay the poll timer up to msg_poll_max
// if no messages are pending
// this way we retrieve bunched messages fast without much overhead in idle times

static int msg_poll_max = 200; // maximum msgq checking interval
static int msg_poll_min = 1;   // minimum msgq checking interval
static int msg_poll_inc = 2;   // increment interval if no message read up to msg_poll_max
static int msg_poll = 1;       // current delay; startup fast
static int msgd_exit;          // flag set by signal handler to start shutdown

int shmdrv_loaded;
long page_size;
global_data_t *global_data;
static ringbuffer_t rtapi_msg_buffer;   // ring access strcuture for messages
static const char *progname;
static char proctitle[20];

static const char *origins[] = { "kernel","rt","user" };
//static const char *encodings[] = { "ascii","stashf","protobuf" };

static void usage(int argc, char **argv) 
{
    printf("Usage:  %s [options]\n", argv[0]);
}

static int create_global_segment()
{
    int retval = 0;
    int globalkey = OS_KEY(GLOBAL_KEY, rtapi_instance);
    int size = sizeof(global_data_t);


    if (shm_common_exists(globalkey)) {
	syslog(LOG_ERR, "MSGD:%d ERROR: found existing global segment key=0x%x\n",
	       rtapi_instance, globalkey);
	return -EEXIST;
    }

    // since the HAL data segment has been determined to exist, global
    // must exist here too or the RT stack wouldnt have become ready
    retval = shm_common_new(globalkey, &size,
			    rtapi_instance, (void **) &global_data, 1);
    if (retval < 0) {
	syslog(LOG_ERR, "MSGD:%d ERROR: cannot create global segment key=0x%x %s\n",
	       rtapi_instance, globalkey, strerror(-retval));
    }
    if (size != sizeof(global_data_t)) {
	syslog(LOG_ERR, "MSGD:%d global segment size mismatch: expect %d got %d\n", 
	       rtapi_instance, sizeof(global_data_t), size);
	return -EINVAL;
    }
    return retval;
}

void init_global_data(global_data_t * data, int flavor,
		      int instance_id, int hal_size, 
		      int rt_level, int user_level,
		      const char *name)
{
    // force-lock - we're first, so thats a bit theoretical
    rtapi_mutex_try(&(data->mutex));
    /* set magic number so nobody else init's the block */
    data->magic = GLOBAL_MAGIC;
    /* set version code so other modules can check it */
    data->layout_version = GLOBAL_LAYOUT_VERSION;

    data->instance_id = instance_id;

    if ((name == NULL) || (strlen(name) == 0)) {
	snprintf(data->instance_name, sizeof(data->instance_name), 
		 "inst%d",rtapi_instance);
    } else {
	strncpy(data->instance_name,name, sizeof(data->instance_name));
    }

    // separate message levels for RT and userland
    data->rt_msg_level = rt_level;
    data->user_msg_level = user_level;

    // next value returned by rtapi_init (userland threads)
    // those dont use fixed sized arrays 
    data->next_module_id = 0;

    // tell the others what we determined as the proper flavor
    data->rtapi_thread_flavor = flavor;

    // HAL segment size
    data->hal_size = hal_size;

    // init the error ring
    rtapi_ringheader_init(&data->rtapi_messages, 0, SIZE_ALIGN(MESSAGE_RING_SIZE), 0);
    memset(&data->rtapi_messages.buf[0], 0, SIZE_ALIGN(MESSAGE_RING_SIZE));

    // attach to the message ringbuffer
    rtapi_ringbuffer_init(&data->rtapi_messages, &rtapi_msg_buffer);
    rtapi_msg_buffer.header->refcount = 1; // rtapi not yet attached
    rtapi_msg_buffer.header->reader = getpid();
    data->rtapi_messages.use_wmutex = 1; // locking hint

    // demon pids
    data->rtapi_app_pid = 0;
    data->rtapi_msgd_pid = 0;

    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return;
}

// determine if we can run this flavor on the current kernel
static int flavor_and_kernel_compatible(flavor_ptr f)
{
    int retval = 1;

    if (f->id == RTAPI_POSIX_ID)
	return 1; // no prerequisites

    if (kernel_is_xenomai() &&
	((f->id != RTAPI_XENOMAI_ID) && 
	 (f->id != RTAPI_XENOMAI_KERNEL_ID))) {
	syslog(LOG_ERR, "started %s RTAPI on a Xenomai kernel\n", f->name);
	return 0;
    }

    if (kernel_is_rtai() &&
	(f->id != RTAPI_RTAI_KERNEL_ID)) {
	syslog(LOG_ERR, "started %s RTAPI on an RTAI kernel\n", f->name);
	return 0;
    }

    if (kernel_is_rtpreempt() &&
	(f->id != RTAPI_RT_PREEMPT_ID)) {
	syslog(LOG_ERR, "started %s RTAPI on an RT PREEMPT kernel\n", f->name);
	return 0;
    }
    return retval;
}

static void signal_handler(int sig, siginfo_t *si, void *context)
{
    syslog(LOG_INFO,"exiting - got signal: %s", strsignal(sig));
    msgd_exit++;
}

static void
cleanup_actions(void)
{
    int retval;

    if (global_data) {
	global_data->rtapi_msgd_pid = 0;
	if (rtapi_msg_buffer.header != NULL)
	    rtapi_msg_buffer.header->refcount--;
	retval = shm_common_detach(sizeof(global_data_t), global_data);
	if (retval < 0) {
	    syslog(LOG_ERR,"shm_common_detach(global) failed: %s\n",
		   strerror(-retval));
	} else {
	    shm_common_unlink(OS_KEY(GLOBAL_KEY, rtapi_instance));
	    syslog(LOG_DEBUG,"shutdown - global segment detached");
	}
	global_data = NULL;
    }
}

static int message_thread()
{
    rtapi_msgheader_t *msg;
    size_t msg_size;
    size_t payload_length;
    int retval;
    char *cp;

    do {
	while ((retval = rtapi_record_read(&rtapi_msg_buffer, 
					   (const void **) &msg, &msg_size)) == 0) {
	    payload_length = msg_size - sizeof(rtapi_msgheader_t);

	    switch (msg->encoding) {
	    case MSG_ASCII:
		// strip trailing newlines
		while ((cp = strrchr(msg->buf,'\n')))
		    *cp = '\0';

		switch (msg->origin) {
		case MSG_ULAPI:
		    setlogmask(LOG_UPTO (rtapi2syslog(global_data->user_msg_level)));
		    break;
		case MSG_RTUSER:
		case MSG_KERNEL:
		    setlogmask(LOG_UPTO (rtapi2syslog(global_data->rt_msg_level)));
		    break;
		}
		syslog(rtapi2syslog(msg->level), "%s:%d:%s %.*s",
		       msg->tag, msg->pid, origins[msg->origin],
		       (int) payload_length, msg->buf);
		break;
	    case MSG_STASHF:
		break;
	    case MSG_PROTOBUF:
		break;
	    default: ;
		// whine
	    }
	    rtapi_record_shift(&rtapi_msg_buffer);
	    msg_poll = msg_poll_min;
	}
	struct timespec ts = {0, msg_poll * 1000 * 1000};

	nanosleep(&ts, NULL);
	msg_poll += msg_poll_inc;
	if (msg_poll > msg_poll_max)
	    msg_poll = msg_poll_max;

    } while (!msgd_exit);

    return 0;
}

static struct option long_options[] = {
    {"help",  no_argument,          0, 'h'},
    {"stderr",  no_argument,        0, 's'},
    {"foreground",  no_argument,    0, 'F'},
    {"usrmsglevel", required_argument, 0, 'u'},
    {"rtmsglevel", required_argument, 0, 'r'},
    {"instance", required_argument, 0, 'I'},
    {"instance_name", required_argument, 0, 'i'},
    {"flavor",   required_argument, 0, 'f'},
    {"halsize",  required_argument, 0, 'H'},
    {"shmdrv",  no_argument,        0, 'S'},

    {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
    int c, i, retval;
    int option = LOG_NDELAY;
    pid_t pid, sid;
    struct sigaction sig_act;
    size_t argv0_len, procname_len, max_procname_len;

    progname = argv[0];
    shm_common_init();

    while (1) {
	int option_index = 0;
	int curind = optind;
	c = getopt_long (argc, argv, "hI:sFf:i:S",
			 long_options, &option_index);
	if (c == -1)
	    break;
	switch (c)	{
	case 'F':
	    foreground++;
	    break;
	case 'I':
	    rtapi_instance = atoi(optarg);
	    break;
	case 'i':
	    instance_name = optarg;
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
	case 's':
	    log_stderr++;
	    option |= LOG_PERROR;
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

    // do we need the shmdrv module?
    if (((flavor->flags & FLAVOR_KERNEL_BUILD) ||
	 use_shmdrv) &&
	!shmdrv_available()) {
	fprintf(stderr, "%s: FATAL - %s requires the shmdrv module loaded\n",
		progname, use_shmdrv ? "--shmdrv" : flavor->name);
	exit(EXIT_FAILURE);
    }

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
        if ((chdir("/")) < 0) {
	    exit(EXIT_FAILURE);
        }
    }

    // set new process name
    snprintf(proctitle, sizeof(proctitle), "msgd:%d",rtapi_instance);
    argv0_len = strlen(argv[0]);
    procname_len = strlen(proctitle);
    max_procname_len = (argv0_len > procname_len) ? (procname_len) : (argv0_len);

    strncpy(argv[0], proctitle, max_procname_len);
    memset(&argv[0][max_procname_len], '\0', argv0_len - max_procname_len);

    for (i = 1; i < argc; i++)
	memset(argv[i], '\0', strlen(argv[i]));

    openlog(proctitle, option , SYSLOG_FACILITY);
    setlogmask(LOG_UPTO(SYSLOG_FACILITY));

    // the global segment every entity in HAL/RTAPI land attaches to
    if ((retval = create_global_segment()) != 1) // must be a new shm segment
	exit(retval);

    // this is the single place in all of linuxCNC where the global segment
    // gets initialized - no reinitialization from elsewhere
    init_global_data(global_data, flavor->id, rtapi_instance,
    		     halsize, rt_msglevel, usr_msglevel,
    		     instance_name);

    syslog(LOG_INFO,
	   "startup instance=%s pid=%d flavor=%s "
	   "rtlevel=%d usrlevel=%d halsize=%d shm=%s",
	   global_data->instance_name, getpid(),
	   flavor->name,
	   global_data->rt_msg_level,
	   global_data->user_msg_level,
	   global_data->hal_size,
	   shmdrv_loaded ? "shmdrv" : "Posix");

    if ((global_data->rtapi_msgd_pid != 0) &&
	kill(global_data->rtapi_msgd_pid, 0) == 0) {
	fprintf(stderr,"%s: another rtapi_msgd is already running (pid %d), exiting\n",
		progname, global_data->rtapi_msgd_pid);
	exit(EXIT_FAILURE);
    } else {
	global_data->rtapi_msgd_pid = getpid();
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    if (!log_stderr)
	close(STDERR_FILENO);

    sigemptyset( &sig_act.sa_mask );
    sig_act.sa_handler = SIG_IGN;
    sig_act.sa_sigaction = NULL;

    // prevent stopping by ^Z
    sigaction(SIGTSTP, &sig_act, (struct sigaction *) NULL); 

    sig_act.sa_sigaction = signal_handler;
    sig_act.sa_flags   = SA_SIGINFO;

    sigaction(SIGINT, &sig_act, (struct sigaction *) NULL);
    sigaction(SIGTERM, &sig_act, (struct sigaction *) NULL);
    sigaction(SIGSEGV, &sig_act, (struct sigaction *) NULL);

    message_thread();

    // signal received - check if rtapi_app running, and shut it down
    // TBD.
    cleanup_actions();

    exit(EXIT_SUCCESS);
}
