/** This file, 'halcmd.c', is a HAL component that provides a simple
    command line interface to the hal.  It is a user space component.
    For detailed instructions, see "man halcmd".
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>

    Other contributers:
                       Martin Kuhnle
                       <mkuhnle AT users DOT sourceforge DOT net>
                       Alex Joni
                       <alex_joni AT users DOT sourceforge DOT net>
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "../hal_refactor.h"		/* HAL public API decls */
#include "../hal_cmds.h"	/* HAL command processing utilities */

static void print_help(void);

/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/

int main(int argc, char **argv)
{
    int n, m, fd;
    int keep_going, retval, errorcount;
    enum { BETWEEN_TOKENS,
           IN_TOKEN,
	   SINGLE_QUOTE,
	   DOUBLE_QUOTE,
	   END_OF_LINE } state;
    char *cp1, *filename = NULL;
    FILE *infile = NULL;
    char cmd_buf[MAX_CMD_LEN+1];
    char *tokens[MAX_TOK+1];

    open_hal_dev();

    if (argc < 2) {
	/* no args specified, print help */
	print_help();
	exit(0);
    }
    /* set default level of output - 'quiet' */
    rtapi_set_msg_level(RTAPI_MSG_ERR);
    /* set default for other options */
    keep_going = 0;
    /* start parsing the command line, options first */
    n = 1;
    while ((n < argc) && (argv[n][0] == '-')) {
	cp1 = argv[n++];
	/* loop to parse grouped options */
	while (*(++cp1) != '\0') {
	    switch (*cp1) {
	    case 'h':
		/* -h = help */
		print_help();
		return 0;
		break;
	    case 'k':
		/* -k = keep going */
		keep_going = 1;
		break;
	    case 'q':
		/* -q = quiet (default) */
		rtapi_set_msg_level(RTAPI_MSG_ERR);
		break;
	    case 'Q':
		/* -Q = very quiet */
		rtapi_set_msg_level(RTAPI_MSG_NONE);
		break;
	    case 'v':
		/* -v = verbose */
		rtapi_set_msg_level(RTAPI_MSG_INFO);
		break;
	    case 'V':
		/* -V = very verbose */
		rtapi_set_msg_level(RTAPI_MSG_ALL);
		break;
	    case 'f':
		/* -f = read from file (or stdin) */
		if (infile == NULL) {
		    /* it's the first -f (ignore repeats) */
		    if ((n < argc) && (argv[n][0] != '-')) {
			/* there is a following arg, and it's not an option */
			filename = argv[n++];
			infile = fopen(filename, "r");
			if (infile == NULL) {
			    fprintf(stderr,
				"Could not open command file '%s'\n",
				filename);
			    exit(-1);
			}
			/* make sure file is closed on exec() */
			fd = fileno(infile);
			fcntl(fd, F_SETFD, FD_CLOEXEC);
		    } else {
			/* no filename followed -f option, use stdin */
			infile = stdin;
		    }
		}
		break;
	    default:
		/* unknown option */
		printf("Unknown option '-%c'\n", *cp1);
		break;
	    }
	}
    }
    /* at this point all options are parsed */
#if 0
    comp_id = hal_init("halcmd");
    if (comp_id < 0) {
	fprintf(stderr, "halcmd: hal_init() failed\n" );
	return 1;
    }
#endif
    retval = 0;
    errorcount = 0;
    /* HAL init is OK, let's process the command(s) */
    if (infile == NULL) {
	/* the remaining command line args are parts of the command */
	m = 0;
	while ((n < argc) && (n < MAX_TOK)) {
	    tokens[m++] = argv[n++];
	}
	/* and the remaining space in the tokens array is empty */
	while (m < MAX_TOK) {
	    tokens[m++] = "";
	}
	/* tokens[] contains MAX_TOK+1 elements so there is always
	   at least one empty one at the end... make it empty now */
	tokens[MAX_TOK] = "";
	/* process the command */
	retval = parse_cmd(tokens);
	if ( retval != 0 ) {
	    errorcount++;
	}
    } else {
	/* read command line(s) from 'infile' */
	errorcount=run_script_file(infile);
    }
    /* all done */
#if 0
    hal_exit(comp_id);
#endif
    if ( errorcount > 0 ) {
	return 1;
    } else {
	return 0;
    }
}

static void print_help(void)
{

    printf("Hardware Abstraction Layer command line utility\n\n");
    printf("Usage:   halcmd [options] [cmd [args]]\n\n");
    printf("options:\n\n");
    printf
	("  -f [filename]    Read command(s) from 'filename' instead of command\n");
    printf
	("                   line.  If no file is specified, read from stdin.\n\n");
    printf("  -k               Keep going after failed command.  By default,\n");
    printf("                   halcmd will exit if any command fails.\n\n");
    printf("  -q               Quiet - Display errors only (default).\n\n");
    printf("  -Q               Very quiet - Display nothing.\n\n");
    printf
	("  -v               Verbose - Display results of every command.\n\n");
    printf("  -V               Very verbose - Display lots of junk.\n\n");
    printf("  -h               Help - Print this help screen and exit.\n\n");
    printf("If reading commands from a file or stdin, they are one per\n");
    printf("line, and use the same syntax as the command line version.\n\n");
    printf("Commands and their args are as follows:\n\n");
    printf("  loadrt modname [modarg[s]]\n");
    printf
	("         Loads realtime HAL module 'modname', using arguments 'modargs'\n\n" );
    printf("  unloadrt modname\n");
    printf
	("         Unloads realtime HAL module 'modname'.\n");
    printf
 	("         If 'modname' is 'all', unloads all realtime modules\n\n" );
    printf("  linkps pinname [arrow] signame\n");
    printf("  linksp signame [arrow] pinname\n");
    printf
	("         Links pin 'pinname' to signal 'signame'.  Both forms do the same\n");
    printf
	("         thing.  Use whichever makes sense.  Likewise, 'arrow' can be '=>',\n");
    printf
	("         '<=', or '<=>' and is ignored (use in command files to document\n");
    printf
	("         the direction of data flow to/from pin - don't use on cmd line).\n\n");
    printf("  unlinkp pinname\n");
    printf("         Unlinks pin 'pinname'\n\n");
    printf("  newsig signame type\n");
    printf
	("         Creates a new signal called 'signame'.  Type is 'bit', 'float',\n");
    printf("         'u8', 's8', 'u16', 's16', 'u32', or 's32'.\n\n");
    printf("  delsig signame\n");
    printf("         Deletes signal 'signame'.\n");
    printf("         If 'signame' is 'all' deletes all signals\n\n");
    printf("  setp paramname value\n");
    printf("  paramname = value\n");
    printf
	("         Sets parameter 'paramname' to 'value' (only if writable).\n");
    printf
	("         (Both forms are equivalent, don't use '=' on command line.)\n\n");
    printf("  sets signame value\n");
    printf
	("         Sets signal 'signame' to 'value' (only if sig has no writers).\n\n");
    printf("  addf functname threadname [position]\n");
    printf
	("         Adds function 'functname' to thread 'threadname'.  If 'position'\n");
    printf("         is specified, add function to that spot in thread.\n\n");
    printf("  delf functname threadname\n");
    printf
	("         Removes function 'functname' from thread 'threadname'.\n\n");
    printf("  show [type]\n");
    printf
	("         Prints HAL items of the specified type in human readable form\n");
    printf
	("         'type' is 'comp', 'pin', 'sig', 'param', 'funct', or 'thread'.\n");
    printf("         If 'type' is omitted, prints everything.\n\n");
    printf("  save [type]\n");
    printf
	("         Prints HAL items in a format that can be redirected to a file,\n");
    printf
	("         and later restored using \"halcmd -f filename\".  Type can be\n");
    printf
	("         'sig', 'link[a]', 'net[a]', 'param', or 'thread'.  ('linka' and\n");
    printf
	("         'neta' show arrows for pin direction.)  If 'type' is omitted,\n");
    printf
	("         it does the equivalend of 'sig', 'link', 'param', and 'thread'.\n\n");
    printf("  start\n");
    printf("         Starts all realtime threads.\n\n");
    printf("  stop\n");
    printf("         Stops all realtime threads.\n\n");
}

