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

int rtapi_instance;
static int log_stderr;
static int foreground;
static int poll_ms = 200; // default msg q checking interval
static int shmdrv_loaded;
static long page_size;
static int global_fd;
global_data_t *global_data;
static ringbuffer_t rtapi_msg_buffer;   // rtapi ring access strcuture

static const char *progname;
static char proctitle[20];

static const char *origins[] = { "kernel","rt","user" };
//static const char *encodings[] = { "ascii","stashf","protobuf" };

static void usage(int argc, char **argv) 
{
    printf("Usage:  %s [options]\n", argv[0]);
}

static struct option long_options[] = {
    {"help",  no_argument,          0, 'h'},
    {"stderr",  no_argument,        0, 's'},
    {"foreground",  no_argument,    0, 'F'},
    {"instance", required_argument, 0, 'I'},
    {"pollms", required_argument, 0, 'p'},
    {0, 0, 0, 0}
};

static int setup_global()
{
    int retval = 0;
    struct shm_status sm;

    page_size = sysconf(_SC_PAGESIZE);
    shmdrv_loaded  = shmdrv_available();

    if (global_data == NULL) {
	if (shmdrv_available()) {
	    sm.size = sizeof(global_data_t);
	    sm.flags = 0;
	    sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
	    sm.driver_fd = shmdrv_driver_fd();
	    retval = shmdrv_attach(&sm, (void **)&global_data);
	    if (retval < 0) {
		syslog(LOG_ERR,"global shmdrv attach failed %d\n", retval);
		return retval;
	    }
	} else {
	    char segment_name[LINELEN];
	    sprintf(segment_name, "0x%8.8x",OS_KEY(GLOBAL_KEY, rtapi_instance));

	    if((global_fd = shm_open(segment_name, (O_CREAT | O_RDWR),
				     (S_IREAD | S_IWRITE))) < 0) {
		syslog(LOG_ERR,"ERROR: cant shm_open(%s) : %s\n",
				segment_name, strerror(errno));
		retval = errno;
	    }
	    if ((global_data = mmap(0, sizeof(global_data_t), (PROT_READ | PROT_WRITE),
				MAP_SHARED, global_fd, 0)) == MAP_FAILED) {
		syslog(LOG_ERR, "ERROR: mmap(%s) failed: %s\n",
				segment_name, strerror(errno));
		retval = errno;
	    }
	}
    }
    return retval;
}


static void msgd_exit(int sig, siginfo_t *si, void *context)
{
    if (global_data) {
	global_data->rtapi_msgd_pid = 0;
	if (rtapi_msg_buffer.header != NULL)
	    rtapi_msg_buffer.header->refcount--;
    }
    syslog(LOG_INFO,"exiting - got signal: %s", strsignal(sig));
    exit(0);
}

static int message_thread()
{
    rtapi_msgheader_t *msg;
    size_t msg_size;
    size_t payload_length;
    int retval;
    char *cp;

    struct timespec ts = {0, poll_ms * 1000 * 1000};

    rtapi_ringbuffer_init(&global_data->rtapi_messages, &rtapi_msg_buffer);
    rtapi_msg_buffer.header->refcount++;

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
		       payload_length, msg->buf);
		break;
	    case MSG_STASHF:
		break;
	    case MSG_PROTOBUF:
		break;
	    default: ;
		// whine
	    }
	    rtapi_record_shift(&rtapi_msg_buffer);
	}
	nanosleep(&ts, NULL);
    } while (1); // !global_data->shutting_down;

    rtapi_msg_buffer.header->refcount--;
    return 0;
}


int main(int argc, char **argv)
{
    int c, i, retval;
    int option = LOG_NDELAY;
    pid_t pid, sid;
    struct sigaction sig_act;
    size_t argv0_len, procname_len, max_procname_len;

    progname = argv[0];

    while (1) {
	int option_index = 0;
	int curind = optind;
	c = getopt_long (argc, argv, "hI:sp:F",
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
	case 'p':
	    poll_ms = atoi(optarg);
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

    for (i = 1; i < argc; i++) {
	memset(argv[i], '\0', strlen(argv[i]));
    }

    openlog(proctitle, option , LOG_LOCAL1);
    setlogmask(LOG_UPTO(LOG_INFO));


    if ((retval = setup_global()) != 0)
	exit(retval);

    syslog(LOG_INFO,"startup instance=%s pid=%d",
	   global_data->instance_name, getpid());

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
    close(STDERR_FILENO);

    sigemptyset( &sig_act.sa_mask );
    sig_act.sa_handler = SIG_IGN; 
    sig_act.sa_sigaction = NULL;

    // prevent stopping by ^Z
    sigaction(SIGTSTP, &sig_act, (struct sigaction *) NULL); 

    sig_act.sa_sigaction = msgd_exit;
    sig_act.sa_flags   = SA_SIGINFO;

    sigaction(SIGINT, &sig_act, (struct sigaction *) NULL);
    sigaction(SIGTERM, &sig_act, (struct sigaction *) NULL);
    sigaction(SIGSEGV, &sig_act, (struct sigaction *) NULL);

    message_thread();

    exit(EXIT_SUCCESS);
}
