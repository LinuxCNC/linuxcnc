/** This file, 'scope_files.c', handles file I/O for halscope.
    It includes code to save and restore front panel setups,
    and eventually to save captured scope data.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
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
#ifndef ULAPI
#error This is a user mode component only!
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */

#include <gtk/gtk.h>
#include "miscgtk.h"		/* generic GTK stuff */
#include "scope_usr.h"		/* scope related declarations */

/***********************************************************************
*                         DOCUMENTATION                                *
************************************************************************/

/* Scope setup is stored in the form of a script containing commands
   that set various parameters.
   
   Each command consists of a keyword followed by one or more values.
   Keywords are not case sensitive.
*/

/*
   THREAD <string>	name of thread to sample in
   MAXCHAN <int>	1,2,4,8,16, maxumum channel count
   HMULT <int>		multiplier, sample every N runs of thread
   HZOOM <int>		1-9, horizontal zoom setting
   HPOS <float>		0.0-1.0, horizontal position setting
   CHAN <int>		sets channel for subsequent commands
   PIN <string>		named pin becomes source for channel
   PARAM <string>	named parameter becomes source for channel
   SIG <string>		named signal becomes source for channel
   CHOFF		disables selected channel
   VSCALE <int>		vertical scaling
   VPOS <float>		0.0-1.0, vertical position setting
   VOFF <float>		vertical offset
   TSOURCE <int>	channel number for trigger source
   TLEVEL <?>		trigger level
   TPOLAR <enum>	triger polarity, RISE or FALL
   TPOS <float>		0.0-1.0, trigger position
   TMODE <?>		trigger mode
   RMODE <?>		run mode   
  
*/


/***********************************************************************
*                         TYPEDEFS AND DEFINES                         *
************************************************************************/

typedef enum {
  INT,
  FLOAT,
  STRING
} arg_type_t;

typedef struct {
  char* name;
  arg_type_t arg_type;
  char * (*handler)(void *arg);
} cmd_lut_entry_t;


/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/



/***********************************************************************
*                     LOCAL FUNCTION PROTOTYPES                        *
************************************************************************/

static int parse_command(char *in);
static char *dummy_cmd(void * arg);
static char *thread_cmd(void * arg);
static char *maxchan_cmd(void * arg);
static char *hzoom_cmd(void * arg);
static char *hpos_cmd(void * arg);
static char *hmult_cmd(void * arg);
static char *chan_cmd(void * arg);
static char *choff_cmd(void * arg);
static char *pin_cmd(void * arg);
static char *sig_cmd(void * arg);
static char *param_cmd(void * arg);
static char *vscale_cmd(void * arg);
static char *vpos_cmd(void * arg);
static char *voff_cmd(void * arg);

/***********************************************************************
*                         LOCAL VARIABLES                              *
************************************************************************/

static const cmd_lut_entry_t cmd_lut[25] = 
{
  { "thread",	STRING,	thread_cmd },
  { "maxchan",	INT,	maxchan_cmd },
  { "hmult",	INT,	hmult_cmd },
  { "hzoom",	INT,	hzoom_cmd },
  { "hpos",	FLOAT,	hpos_cmd },
  { "chan",	INT,	chan_cmd },
  { "choff",	INT,	choff_cmd },
  { "pin",	STRING,	pin_cmd },
  { "sig",	STRING,	sig_cmd },
  { "param",	STRING,	param_cmd },
  { "vscale",	INT,	vscale_cmd },
  { "vpos",	FLOAT,	vpos_cmd },
  { "voff",	FLOAT,	voff_cmd },
  { "tsource",	INT,	dummy_cmd },
  { "tlevel",	FLOAT,	dummy_cmd },
  { "tpolar",	STRING,	dummy_cmd },
  { "tmode",	STRING,	dummy_cmd },
  { "rmode",	STRING,	dummy_cmd },
  { "", 0, dummy_cmd }
};

static int deferred_channel;
  
/***********************************************************************
*                        PUBLIC FUNCTION CODE                          *
************************************************************************/

int read_config_file (char *filename)
{
    FILE *fp;
    char cmd_buf[100];
    char *cp;
    int retval;

    deferred_channel = 0;    
    fp = fopen(filename, "r");
    if ( fp == NULL ) {
	fprintf(stderr, "ERROR: config file '%s' could not be opened\n", filename );
	return -1;
    }
    retval = 0;
    while ( fgets(cmd_buf, 99, fp) != NULL ) {
	/* remove trailing newline if present */
	cp = cmd_buf;
	while (( *cp != '\n' ) && ( *cp != '\0' )) {
	    cp++;
	}
	*cp = '\0';
	/* parse and execute the command */
	retval += parse_command(cmd_buf);
    }
    fclose(fp);
    if ( retval < 0 ) {
	fprintf(stderr, "ERROR: config file '%s' caused %d errors\n", filename, -retval );
	return -1;
    }
    return 0;
}


void write_config_file (char *filename)
{
    FILE *fp;
    
    fp = fopen(filename, "w");
    if ( fp == NULL ) {
	fprintf(stderr, "ERROR: config file '%s' could not be created\n", filename );
	return;
    }
    write_horiz_config(fp);
    write_vert_config(fp);
//    write_trig_config(fp);
    fclose(fp);
}


/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

static int parse_command(char *in)
{
    int n;
    char *cp1, *cp2, *rv;
    int arg_int;
    double arg_float;
    char *arg_string;

    n = -1;
    do {  
	cp1 = in;
	cp2 = cmd_lut[++n].name;
	/* skip all matching chars */
	while (( *cp2 != '\0') && (tolower(*cp1) == *cp2 )) {
	    cp1++;
	    cp2++;
	}
    } while ( *cp2 != '\0' );
    /* either a match, or zero length name (last entry) */
    if ( cp1 == in ) {
	/* zero length name, last entry, no match */
	if ( *in != '#' ) {
	    /* not a comment, must be a mistake */
	    fprintf (stderr, "ERROR: unknown config command: '%s'\n", in );
	    return -1;
	}
    }
    switch ( cmd_lut[n].arg_type ) {
    case STRING:
	while ( isspace(*cp1) ) {
	    cp1++;
	}
	arg_string = cp1;
	/* find and replace newline at end */
	while (( *cp1 != '\n' ) && ( *cp1 != '\0' )) {
	    cp1++;
	}
	*cp1 = '\0';
	rv = cmd_lut[n].handler(arg_string);
	break;
    case FLOAT:
	arg_float = strtod(cp1, &cp1);
	rv = cmd_lut[n].handler(&arg_float);
	break;
    case INT:
	arg_int = strtol(cp1, &cp1, 10);
	rv = cmd_lut[n].handler(&arg_int);
	break;
    default:
	fprintf(stderr, "ERROR: unknown arg type for command %s\n", in );
	return -1;
	break;
    }
    if ( rv != NULL ) {
	fprintf(stderr, "ERROR: %s: '%s'\n", rv, in );
	return -1;
    }
    return 0;
}
   
static char *dummy_cmd(void * arg)
{
    return "command not implemented";
}

static char *thread_cmd(void * arg)
{
    char *name;
    int rv;
    
    name = (char *)(arg);
    rv = set_sample_thread(name);
    if ( rv < 0 ) {
	return "could not find thread";
    }
    return NULL;
}

static char *maxchan_cmd(void * arg)
{
    int *argp, rv;
    
    argp = (int *)(arg);
    rv = set_rec_len(*argp);
    if ( rv < 0 ) {
	return "could not set record length";
    }
    return NULL;
}

static char *hzoom_cmd(void * arg)
{
    int *argp, rv;
    
    argp = (int *)(arg);
    rv = set_horiz_zoom(*argp);
    if ( rv < 0 ) {
	return "could not set horizontal zoom";
    }
    return NULL;
}    

static char *hpos_cmd(void * arg)
{
    double *argp;
    int rv;
    
    argp = (double *)(arg);
    rv = set_horiz_pos(*argp);
    if ( rv < 0 ) {
	return "could not set horizontal position";
    }
    return NULL;
}    

static char *hmult_cmd(void * arg)
{
    int *argp, rv;
    
    argp = (int *)(arg);
    rv = set_horiz_mult(*argp);
    if ( rv < 0 ) {
	return "could not set horizontal multiplier";
    }
    return NULL;
}

static char *chan_cmd(void * arg)
{
    int *argp, chan_num, rv;
    
    argp = (int *)(arg);
    chan_num = *argp;
    deferred_channel = 0;
    rv = set_active_channel(chan_num);
    switch (rv) {
    case 0:
	// successfull return
	return NULL;
    case -1:
	return "illegal channel number";
    case -2:
	return "too many active channels";
    case -3:
	// no source for channel, OK as long as we get
	// a subsequent command that specifies a source
	deferred_channel = chan_num;
	return NULL;
    default:
	return "unknown result";
    }
}    

static char *choff_cmd(void * arg)
{
    int chan_num;
    
    if ( deferred_channel != 0 ) {
	deferred_channel = 0;
	return NULL;
    }
    chan_num = ctrl_usr->vert.selected;
    set_channel_off(chan_num);
    return NULL;
}    

static char *chan_src_cmd(int src_type, char *src_name)
{
    int chan_num, rv;

    if ( deferred_channel == 0 ) {
	// changing currently active channel
	chan_num = ctrl_usr->vert.selected;
	rv = set_channel_source(chan_num, src_type, src_name);
    } else {
	// setting source for previously empty channel
	chan_num = deferred_channel;
	rv = set_channel_source(chan_num, src_type, src_name);
	if ( rv == 0 ) {
	    // got a source now, select the channel
	    return chan_cmd(&chan_num);
	}
    }
    if ( rv < 0 ) {
	return "object not found";
    }
    return NULL;
}


static char *pin_cmd(void * arg)
{
    return chan_src_cmd(0, (char *)(arg));
}

static char *sig_cmd(void * arg)
{
    return chan_src_cmd(1, (char *)(arg));
}

static char *param_cmd(void * arg)
{
    return chan_src_cmd(2, (char *)(arg));
}

static char *vscale_cmd(void * arg)
{
    int *argp, rv;
    
    argp = (int *)(arg);
    rv = set_vert_scale(*argp);
    if ( rv < 0 ) {
	return "could not set vertical scale";
    }
    return NULL;
}    

static char *vpos_cmd(void * arg)
{
    double *argp;
    int rv;
    
    argp = (double *)(arg);
    rv = set_vert_pos(*argp);
    if ( rv < 0 ) {
	return "could not set vertical position";
    }
    return NULL;
}    

static char *voff_cmd(void * arg)
{
    double *argp;
    int rv;
    
    argp = (double *)(arg);
    rv = set_vert_offset(*argp);
    if ( rv < 0 ) {
	return "could not set vertical offset";
    }
    return NULL;
}    

