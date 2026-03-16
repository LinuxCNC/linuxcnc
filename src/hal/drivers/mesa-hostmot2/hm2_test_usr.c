/********************************************************************
* Description:  hm2_test_usr.c
*               User space part of "hm2_test", a HAL component that
*		can be used to verify or simulate a mesa driver.
*	Based on sampler_usr.d for the sample component.
*
* Author: Jerry J. Trantow <Jerry Trantow at gmail dot com>
* License: GPL Version 2
*    
* Copyright (c) 2026 All rights reserved.
*
********************************************************************/
/** This file, 'hm2_test_user.c', is the user part of a HAL driver
    that allows a memory buffer representing the hostmot2 variables to be simulated.
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the LinuxCNC/EMC HAL project.  For more
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

#include "rtapi.h"		// RTAPI realtime OS API.
#include "hal.h"        // HAL public API decls.
//#include "hostmot2.h"
#include "hostmot2-lowlevel.h" // Contains lowlevel structure required by hm2_test.h
#include "hm2_test.h"	// Contains structures used by hm2_test component. 
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
char comp_name[HAL_NAME_LEN+1];	/* name for this instance */



hm2_test_t mesa_variables; // Structure used by hw_test component.

/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/

/* signal handler */
static sig_atomic_t stop;
static void quit(int sig)
{
    if ( ignore_sig ) {
		return;
    }else {
    	stop = 1;
	}
}


/*
	This program will open and read an input file and write it to the shared memory of the hm2_test component.	
*/
int main(int argc, char **argv)
{

    /* set return code to "fail", clear it later if all goes well */
    exitval = 1;

	if ((2 != argc) || (NULL == argv[0])) {
		fprintf(stderr, "hm2_test_usr requires exactly one filename, argc = %d.", argc);
		return(-1);
	}

	FILE *fp = NULL;
	fp = fopen(argv[1], "rb");
	if (NULL != fp) {
		fprintf(stdout, "file %s open.\n", argv[1]);

		/*
			We don't know the size of the file.
			Ok, as long as it's <= test_pattern reserved inside hm2_test component.
		*/
		size_t num_char_read = fread(mesa_variables.test_pattern.tp8, 1, sizeof(mesa_variables.test_pattern), fp);
		if (!ferror(fp)) {
			fprintf(stderr, "Read %d bytes from %s.\n", num_char_read, argv[1]);
		}else {
			fprintf(stdout, "Error reading file %s num read %d", argv[1], num_char_read);
		}
		(void)fclose(fp);
	}else{
		fprintf(stderr, "file %s didn't open.\n", argv[1]);
		return(1);
	}

#if (0)
    /* FIXME - if I wasn't so lazy I'd learn how to use getopt() here */
    for ( n = 1 ; n < argc ; n++ ) {
	cp = argv[n];
	if ( *cp != '-' ) {
	    break;
	}
	switch ( *(++cp) ) {
	case 'c':
	    if (( *(++cp) == '\0' ) && ( ++n < argc )) { 
		cp = argv[n];
	    }
	    channel = strtol(cp, &cp2, 10);
	    if (( *cp2 ) || ( channel < 0 ) || ( channel >= MAX_SAMPLERS )) {
		fprintf(stderr,"ERROR: invalid channel number '%s'\n", cp );
		exit(1);
	    }
	    break;
	case 'n':
	    if (( *(++cp) == '\0' ) && ( ++n < argc )) { 
		cp = argv[n];
	    }
	    samples = strtol(cp, &cp2, 10);
	    if (( *cp2 ) || ( samples < 0 )) {
		fprintf(stderr, "ERROR: invalid sample count '%s'\n", cp );
		exit(1);
	    }
	    break;
	case 't':
	    tag = 1;
	    break;
	default:
	    fprintf(stderr,"ERROR: unknown option '%s'\n", cp );
	    exit(1);
	    break;
	}
    }
    if(n < argc) {
	int fd;
	if(argc > n+1) {
	    fprintf(stderr, "ERROR: At most one filename may be specified\n");
	    exit(1);
	}
	// make stdout be the named file
	fd = open(argv[n], O_WRONLY | O_CREAT | O_TRUNC, 0666);
	close(1);
	dup2(fd, 1);

    }
#endif
    /*
		register signal handlers - if the process is killed, we need to call hal_exit() to free the shared memory 
	*/
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGPIPE, quit);
    /* connect to HAL */
    /* create a unique module name, to allow for multiple samplers */
    snprintf(comp_name, sizeof(comp_name), "hm2_test_usr_%d", getpid());
    /* connect to the HAL */
    ignore_sig = 1;
    comp_id = hal_init(comp_name);
    ignore_sig = 0;
    /* check result */
    if (comp_id < 0) {
		fprintf(stderr, "ERROR: hal_init() failed: %d\n", comp_id );
		goto out;
    }
    hal_ready(comp_id);

	fprintf(stdout, "Component %s ready id %d.\n", comp_name, comp_id);

	for (;;); 
#if (0)	
    int res = hal_stream_attach(&stream, comp_id, SAMPLER_SHMEM_KEY+channel, 0);
    if (res < 0) {
		errno = -res;
		perror("hal_stream_attach");
		goto out;
    }
    int num_pins = hal_stream_element_count(&stream);
    while ( samples != 0 ) {
		union hal_stream_data buf[num_pins];
	hal_stream_wait_readable(&stream, &stop);
	if(stop) break;
	int res = hal_stream_read(&stream, buf, &this_sample);
	if (res < 0) {
	    errno = -res;
	    perror("hal_stream_read");
	    goto out;
	}
	++last_sample;
	if ( this_sample != last_sample ) {
	    printf ( "overrun\n");
	    last_sample = this_sample;
	}
	if ( tag ) {
	    printf ( "%d ", this_sample-1 );
	}
	for ( n = 0 ; n < num_pins; n++ ) {
	    switch ( hal_stream_element_type(&stream, n) ) {
	    case HAL_FLOAT:
		printf ( "%f ", buf[n].f);
		break;
	    case HAL_BIT:
		if ( buf[n].b ) {
		    printf ( "1 " );
		} else {
		    printf ( "0 " );
		}
		break;
	    case HAL_U32:
		printf ( "%lu ", (unsigned long)buf[n].u);
		break;
	    case HAL_S32:
		printf ( "%ld ", (long)buf[n].s);
		break;
	    default:
		/* better not happen */
		goto out;
	    }
	}
	printf ( "\n" );
	if ( samples > 0 ) {
	    samples--;
	}
    }
#endif	
    exitval = 0;	// run was successful.
out:
    ignore_sig = 1;
    if ( comp_id >= 0 ) {
		hal_exit(comp_id);
    }
    return exitval;
}
