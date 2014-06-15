/********************************************************************
* Description:  streamer_usr.c
*               User space part of "streamer", a HAL component that
*		can be used to stream data from a file onto HAL pins
*		 at a specific realtime sample rate.
*
* Author: John Kasunich <jmkasunich at sourceforge dot net>
* License: GPL Version 2
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/
/** This file, 'streamer_usr.c', is the user part of a HAL component
    that allows numbers stored in a file to be "streamed" onto HAL
    pins at a uniform realtime sample rate.  When the realtime module
    is loaded, it creates a fifo in shared memory.  Then, the user
    space program 'hal_streamer' is invoked.  'hal_streamer' takes 
    input from stdin and writes it to the fifo, and the realtime
    part transfers the data from the fifo to HAL pins.

    Invoking:

    halstreamer [chan_num]

    'chan_num', if present, specifies the streamer channel to use.
    The default is channel zero.  Since hal_streamer takes its data
    from stdin, it will almost always either need to have stdin 
    redirected from a file, or have data piped into it from some
    other program.
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"                /* HAL public API decls */
#include "streamer.h"

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

int comp_id = -1;	/* -1 means hal_init() not called yet */
int shmem_id = -1;
int exitval = 1;	/* program return code - 1 means error */
int ignore_sig = 0;	/* used to flag critical regions */
int linenumber=0;	/* used to print linenumber on errors */
char comp_name[HAL_NAME_LEN+1];	/* name for this instance of streamer */

/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/

/* signal handler */
static void quit(int sig)
{
    if ( ignore_sig ) {
	return;
    }
    if ( shmem_id >= 0 ) {
	rtapi_shmem_delete(shmem_id, comp_id);
    }
    if ( comp_id >= 0 ) {
	hal_exit(comp_id);
    }
    exit(exitval);
}

#define BUF_SIZE 4000

int main(int argc, char **argv)
{
    int n, channel, retval, size, line;
    char *cp,*cp2;
    char *name = NULL;
    void *shmem_ptr;
    fifo_t *fifo;
    shmem_data_t *data, *dptr;
    char buf[BUF_SIZE];
	const char *errmsg;
    int tmpin, newin;
    struct timespec delay;

    /* set return code to "fail", clear it later if all goes well */
    exitval = 1;
    channel = 0;
    int  opt;
    while ((opt = getopt(argc, argv, "c:N:")) != -1) {
	switch (opt) {
        case 'c':
	    channel = strtol(optarg, &cp2, 10);
            if (( *cp2 ) || ( channel < 0 ) || ( channel >= MAX_STREAMERS )) {
		fprintf(stderr,"ERROR: invalid channel number '%s'\n", optarg );
                exit(1);
            }
            break;
	case 'N':
	    name = optarg;
	    break;
	default: /* '?' */
	    fprintf(stderr,"ERROR: unknown option '%c'\n", opt);
	    fprintf(stderr,"valid options are:\n" );
	    fprintf(stderr,"\t-c <int>\t channel number\n" );
	    fprintf(stderr,"\t-N <name>\t set HAL component name\n" );
	    exit(EXIT_FAILURE);
        }
    }
    if (optind < argc) {
        int fd;
	if(argc > optind+1) {
            fprintf(stderr, "ERROR: At most one filename may be specified\n");
            exit(1);
        }
	// make stdin be the named file
	fd = open(argv[optind], O_RDONLY);
	if (fd < 0) {
            fprintf(stderr, "ERROR: cannot open '%s' for reading: %s\n",
		    argv[optind], strerror(errno));
            exit(1);
	}
	close(0);
	dup2(fd, 0);
    }

    /* register signal handlers - if the process is killed
       we need to call hal_exit() to free the shared memory */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGPIPE, SIG_IGN);
    /* connect to HAL */

    if (name == NULL) {
       /* create a unique module name, to allow for multiple samplers */
       snprintf(comp_name, sizeof(comp_name), "halstreamer%d", getpid());
       name = comp_name;
    }

    /* connect to the HAL */
    ignore_sig = 1;
    comp_id = hal_init(name);
    ignore_sig = 0;
    /* check result */
    if (comp_id < 0) {
	fprintf(stderr, "ERROR: hal_init() failed: %d\n", comp_id );
	goto out;
    }
    hal_ready(comp_id);
    /* open shmem for user/RT comms (fifo) */
    /* initial size is unknown, assume only the fifo structure */
    shmem_id = rtapi_shmem_new(STREAMER_SHMEM_KEY+channel, comp_id, sizeof(fifo_t));
    if ( shmem_id < 0 ) {
	fprintf(stderr, "ERROR: couldn't allocate user/RT shared memory\n");
	goto out;
    }
    retval = rtapi_shmem_getptr(shmem_id, &shmem_ptr, 0);
    if ( retval < 0 ) {
	fprintf(stderr, "ERROR: couldn't map user/RT shared memory\n");
	goto out;
    }
    fifo = shmem_ptr;
    if ( fifo->magic != FIFO_MAGIC_NUM ) {
	fprintf(stderr, "ERROR: channel %d realtime part is not loaded\n", channel );
	goto out;
    }
    /* now use data in fifo structure to calculate proper shmem size */
    size = sizeof(fifo_t) + fifo->num_pins * fifo->depth * sizeof(shmem_data_t);
    /* close shmem, re-open with proper size */
    rtapi_shmem_delete(shmem_id, comp_id);
    shmem_id = rtapi_shmem_new(STREAMER_SHMEM_KEY+channel, comp_id, size);
    if ( shmem_id < 0 ) {
	fprintf(stderr, "ERROR: couldn't re-allocate user/RT shared memory\n");
	goto out;
    }
    retval = rtapi_shmem_getptr(shmem_id, &shmem_ptr, 0);
    if ( retval < 0 ) {
	fprintf(stderr, "ERROR: couldn't re-map user/RT shared memory\n");
	goto out;
    }
    line = 1;
    fifo = shmem_ptr;
    data = fifo->data;
    while ( fgets(buf, BUF_SIZE, stdin) ) {
	/* calculate _next_ value for in */
	tmpin = fifo->in;
	newin = tmpin + 1;
	if ( newin >= fifo->depth ) {
	    newin = 0;
	}
	/* wait until there is space in the buffer */
	while ( newin == fifo->out ) {
            /* fifo full, sleep for 10mS */
	    delay.tv_sec = 0;
	    delay.tv_nsec = 10000000;
	    nanosleep(&delay,NULL);
	}
	/* make pointer fifo entry */
	dptr = &data[tmpin*fifo->num_pins];
	/* parse input line, write results to fifo */
	cp = buf;
	errmsg = NULL;
	for ( n = 0 ; n < fifo->num_pins ; n++ ) {
	    /* strip leading whitespace */
	    while ( isspace(*cp) ) {
		cp++;
	    }
	    switch ( fifo->type[n] ) {
	    case HAL_FLOAT:
		dptr->f = strtod(cp, &cp2);
		break;
	    case HAL_BIT:
		if ( *cp == '0' ) {
		    dptr->b = 0;
		    cp2 = cp + 1;
		} else if ( *cp == '1' ) {
		    dptr->b = 1;
		    cp2 = cp + 1;
		} else {
		    errmsg = "bit value not 0 or 1";
		    cp2 = cp;
		}
		break;
	    case HAL_U32:
		dptr->u = strtoul(cp, &cp2, 10);
		break;
	    case HAL_S32:
		dptr->s = strtol(cp, &cp2, 10);
		break;
	    default:
		/* better not happen */
		goto out;
	    }
	    if ( errmsg == NULL ) {
		/* no error yet, check for other possibilties */
		/* whitespace separates fields, and there is a newline
		   at the end... so if there is not space or newline at
		   the end of a field, something is wrong. */
		if ( *cp2 == '\0' ) {
		    errmsg = "premature end of line";
		} else if ( ! isspace(*cp2) ) {
		    errmsg = "bad character";
		}
	    }
	    /* test for any error */
	    if ( errmsg != NULL ) {
		/* abort loop on error */
		break;
	    }
	    /* advance pointers for next field */
	    dptr++;
	    cp = cp2;
	}
	if ( errmsg != NULL ) {
	    /* print message */
	    fprintf (stderr, "line %d, field %d: %s, skipping the line\n", line, n, errmsg );
	    /** TODO - decide whether to skip this line and continue, or 
		abort the program.  Right now it skips the line. */
	} else {
	    /* good data, keep it */
	    fifo->in = newin;
	}
	line++;
    }
    /* run was succesfull */
    exitval = 0;

out:
    ignore_sig = 1;
    if ( shmem_id >= 0 ) {
	rtapi_shmem_delete(shmem_id, comp_id);
    }
    if ( comp_id >= 0 ) {
	hal_exit(comp_id);
    }
    return exitval;
}
