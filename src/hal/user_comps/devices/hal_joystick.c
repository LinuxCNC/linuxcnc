/********************************************************************
* Description:  hal_joystick.c
*               Joystick driver for HAL
*
* Author: John Kasunich
* License: GPL2
*
* Copyright (c) 2006 All rights reserved.
*
* Last change: 
********************************************************************/

/** 'hal_joystick.c', is a user space driver that allows  HAL pins to
    be controlled by a joystick.  It uses standard Linux  methods to 
    access the joystick, and should work with any joystick that Linux
    recognizes.  It is a user space program, so there is no hard
    realtime guarantee, but under all but the worst loading it is
    quite responsive.
*/

/** Copyright (C) 2006 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of 
    harming persons must have provisions for completely removing 
    power from all motors, etc, before persons enter any danger area.
    All machinery must be designed to comply with local and national 
    safety codes, and the authors of this software can not, and do 
    not, take any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/joystick.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */

#define MAX_AXIS 8
#define MAX_BUTTON 32

typedef struct {
    hal_float_t *axis[MAX_AXIS];
    hal_bit_t *button[MAX_BUTTON];
} hal_js_t;

/* global variables */
int comp_id;		/* HAL component ID */
int done;

/* signal handler */
static void quit(int sig)
{
    done = 1;
}

int main(int argc, char * argv[])
{

    int jsfd, rdlen, n, v, retval;
    struct js_event event;
    int axis_exported[MAX_AXIS];
    int button_exported[MAX_BUTTON];
    hal_js_t *js_data;
    const char *device, *comp_name, *prefix;
    char name[HAL_NAME_LEN + 2];
    struct stat sb;
    int devmajor, devminor, chardev;
    fd_set fds;
    struct timeval tv;

    done = 0;
    /* parse command line */
    device = "/dev/input/js0";
    comp_name = "hal_joystick";
    prefix = "joystick.0";

    n = 1;
    while ( n < argc ) {
	if ( strcmp("-d", argv[n]) == 0 ) {
	    n++;
	    device = argv[n];
	} else if ( strcmp ("-p", argv[n]) == 0 ) {
	    n++;
	    prefix = argv[n];
	    comp_name = argv[n];
	} else {
	    printf ( "Unknown option: '%s'\n", argv[n] );
	    printf ( "Usage: hal_joystick [-d <device>] [-p <prefix>]\n" );
	    return -1;
	} 
    n++;
    }
    /* make sure prefix + suffix (12 chars) fits in HAL_NAME_LEN */
    if ( (strlen(prefix)+12) > HAL_NAME_LEN ) {
	printf ( "ERROR: prefix '%s' is too long, limit is %d chars\n", prefix, HAL_NAME_LEN-12 );
	return -1;
    }

    /* validate device */
    if ( stat (device, &sb) != 0 ) {
	printf ( "ERROR: can't stat() device '%s'\n", device );
	return -1;
    }
    chardev = S_ISCHR(sb.st_mode);
    devmajor = major(sb.st_rdev);
    devminor = minor(sb.st_rdev);
    if ( ! (( chardev ) && ( devmajor == 13 ) && ( devminor <= 31 )) && 
	 ! (( chardev ) && ( devmajor == 15 ) && ( devminor <= 127 )) ) {
	printf ( "ERROR: '%s' is not a joystick\n", device );
	return -1;
    }

    /* STEP 1: open the joystick device file */
    jsfd = open (device, O_RDONLY);
    if ( jsfd < 0 ) {
        printf ( "ERROR: can't open device '%s'\n", device );
        return -1;
    }
    /* STEP 2: initialise the hal component */
    comp_id = hal_init(comp_name);
    if (comp_id < 0) {
	printf( "ERROR: hal_init() failed\n");
	close(jsfd);
	return -1;
    }
    /* Register the routine that catches the SIGINT signal */
    signal(SIGINT, quit);
    /* catch SIGTERM too - the run script uses it to shut things down */
    signal(SIGTERM, quit);
    /* STEP 3: allocate shared memory for joystick data */
    js_data = (hal_js_t *) hal_malloc(sizeof(hal_js_t));
    if (js_data == 0) {
	printf( "ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	close(jsfd);
	return -1;
    }
    /* clear the exported flags */
    for ( n = 0 ; n < MAX_AXIS ; n++ ) {
	axis_exported[n] = 0;
    }
    for ( n = 0 ; n < MAX_BUTTON ; n++ ) {
	button_exported[n] = 0;
    }
    /* STEP 4: go into the main loop */
    hal_ready(comp_id);
    while ( !done ) {
	/* use select to wait for joystick action or a timeout */
	FD_ZERO(&fds);
	FD_SET(jsfd, &fds);
	/* timeout is 0.1 second */
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	/* do the select call */
	retval = select(jsfd+1, &fds, NULL, NULL, &tv );
	/* check for timeout */
	if ( retval == 0 ) {
	    /* timeout, just keep looping (unless 'done' is set */
	    continue;
	}
	/* check for select() errors */
	if ( retval < 0 ) {
	    if ( errno == EINTR ) {
		/* caught a signal, just keep looping */
		continue;
	    }
	    /* a real error */
	    perror ( "ERROR: select() failed: " );
	    break;
	}
	rdlen = read (jsfd, &event, sizeof(struct js_event));
	if ( rdlen != sizeof(struct js_event) ) {
	    printf ( "ERROR: expected %ld bytes, got %d\n", (long)sizeof(struct js_event), rdlen );
	    break;
	}
	/* detect init events, and export a pin for each one */
	if ( event.type & JS_EVENT_INIT ) {
	    if ( event.type & JS_EVENT_AXIS ) {
		n = event.number;
		if ( n < MAX_AXIS ) {
		    if ( !axis_exported[n] ) {
			/* gotta export the HAL pin */
			snprintf(name, HAL_NAME_LEN, "%s.axis.%d", prefix, n);
			retval = hal_pin_float_new(name, HAL_OUT, &(js_data->axis[n]), comp_id);
			if (retval < 0) {
			    printf( "ERROR: axis %d export failed with err=%d\n", n, retval);
			    break;
			}
			axis_exported[n] = 1;
		    }
		} else {
		    printf( "WARNING: axis %d exceeds MAX_AXIS (%d)\n", n, MAX_AXIS);
		}
	    } /* if JS_EVENT_AXIS */
	    if ( event.type & JS_EVENT_BUTTON ) {
		n = event.number;
		if ( n < MAX_BUTTON ) {
		    if ( !button_exported[n] ) {
			/* gotta export the HAL pin */
			snprintf(name, HAL_NAME_LEN, "%s.button.%d", prefix, n);
			retval = hal_pin_bit_new(name, HAL_OUT, &(js_data->button[n]), comp_id);
			if (retval < 0) {
			    printf( "ERROR: button %d export failed with err=%d\n", n, retval);
			    break;
			}
			button_exported[n] = 1;
		    }
		} else {
		    printf( "WARNING: button %d exceeds MAX_BUTTON (%d)\n", n, MAX_BUTTON);
		}
	    } /* if JS_EVENT_BUTTON */
	} /* if JS_EVENT_INIT */
	/* mask off init bit */
	event.type &= ~JS_EVENT_INIT;
	/* detect change events and write to HAL */
	if ( event.type & JS_EVENT_AXIS ) {
	    n = event.number;
	    v = event.value;
	    if ( n < MAX_AXIS ) {
		*(js_data->axis[n]) = v * ( 1.0 / 32767 );
	    }
	}
	if ( event.type & JS_EVENT_BUTTON ) {
	    n = event.number;
	    v = event.value;
	    if ( n < MAX_BUTTON ) {
		*(js_data->button[n]) = v;
	    }
	}
    } /* main loop */
    /* end of main loop, cleanup and exit */
    close (jsfd);
    hal_exit(comp_id);
    return (0);
}
