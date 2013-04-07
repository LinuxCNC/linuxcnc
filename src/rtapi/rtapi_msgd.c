#include "rtapi.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include <assert.h>

#include <rtapi.h>
#include <hal.h>

static int comp_id;
static int instance_id;
static int rt_msglevel;
static int usr_msglevel;
const char *progname;

static void usage(int argc, char **argv) 
{
    printf("Usage:  %s [options]\n", argv[0]);
}

static struct option long_options[] = {
    {"help",  no_argument,       0, 'h'},
    {"instance", required_argument, 0, 'I'},
    {"usrmsglevel", required_argument, 0, 'u'},
    {"rtmsglevel", required_argument, 0, 'r'},
    {0, 0, 0, 0}
};

// for now go through hal_init to access global_data
// until there's a global API
static int setup_global()
{
    char hal_name[LINELEN];
    snprintf(hal_name, sizeof(hal_name), "msgd%d", getpid());

    if ((comp_id = hal_init(hal_name)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init(%s) failed: HAL error code=%d\n",
			progname, hal_name, comp_id);
	return -1;
    }
    hal_ready(comp_id);
    return 0;
}

int main(int argc, char **argv)
{
    int c, retval;
    progname = argv[0];

    while (1) {
	int option_index = 0;
	int curind = optind;
	c = getopt_long (argc, argv, "hI:u:r:",
			 long_options, &option_index);
	if (c == -1)
	    break;

	switch (c)	{
	case 'u':
	    usr_msglevel = atoi(optarg);
	    break;

     	case 'r':
	    rt_msglevel = atoi(optarg);
	    break;

	case 'I':
	    instance_id = atoi(optarg);
	    break;

	case '?':
	    if (optopt)  fprintf(stderr, "bad short opt '%c'\n", optopt);
	    else  fprintf(stderr, "bad long opt \"%s\"\n", argv[curind]);
	    //usage(argc, argv);
	    exit(1);
	    break;

	default:
	    usage(argc, argv);
	    exit(0);
	}
    }
    if ((retval = setup_global()) != 0)
	exit(retval);
    exit(0);
}
