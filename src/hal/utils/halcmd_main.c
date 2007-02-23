/* Copyright (C) 2007 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2003 John Kasunich
 *                     <jmkasunich AT users DOT sourceforge DOT net>
 *
 *  Other contributers:
 *                     Martin Kuhnle
 *                     <mkuhnle AT users DOT sourceforge DOT net>
 *                     Alex Joni
 *                     <alex_joni AT users DOT sourceforge DOT net>
 *                     Benn Lipkowitz
 *                     <fenn AT users DOT sourceforge DOT net>
 *                     Stephen Wille Padnos
 *                     <swpadnos AT users DOT sourceforge DOT net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General
 *  Public License as published by the Free Software Foundation.
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
 *
 *  THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 *  ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 *  TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 *  harming persons must have provisions for completely removing power
 *  from all motors, etc, before persons enter any danger area.  All
 *  machinery must be designed to comply with local and national safety
 *  codes, and the authors of this software can not, and do not, take
 *  any responsibility for such compliance.
 *
 *  This code was written as part of the EMC HAL project.  For more
 *  information, go to www.linuxcnc.org.
 */

#include "config.h"
#include "rtapi.h"
#include "hal.h"
#include "../hal_priv.h"
#include "halcmd.h"
#include "halcmd_commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fnmatch.h>
#include <search.h>

static int get_input(FILE *srcfile, char *buf, size_t bufsize);
static void print_help_general(int showR);
static int release_HAL_mutex(void);


/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

int main(int argc, char **argv)
{
    int n, fd;
    int keep_going, retval, errorcount;
    int filemode = 0;
    char cp1, *filename = NULL;
    FILE *srcfile = NULL;
    char raw_buf[MAX_CMD_LEN+1];

    if (argc < 2) {
	/* no args specified, print help */
	print_help_general(0);
	exit(0);
    }
    /* set default level of output - 'quiet' */
    rtapi_set_msg_level(RTAPI_MSG_ERR);
    /* set default for other options */
    keep_going = 0;
    /* start parsing the command line, options first */
    n = 1;
    while(1) {
        cp1 = getopt(argc, argv, "+Rfi:kqQsvVh");
        if(cp1 == -1) break;

        switch(cp1) {
            case 'R':
       		/* force an unlock of the HAL mutex - to be used after a segfault in a hal program */
		if (release_HAL_mutex() != HAL_SUCCESS) {
			printf("HALCMD: Release Mutex failed!\n");
			return 1;
		}
		return 0;
		break;
	    case 'h':
		/* -h = help */
                if (argc > optind) { /* there are more arguments */
                    do_help_cmd(argv[optind]);
                } else {
		    print_help_general(1);
                }
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
	    case 's':
		/* script friendly mode */
		scriptmode = 1;
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
                filemode = 1;
		break;
#ifndef NO_INI
	    case 'i':
		/* -i = allow reading 'setp' values from an ini file */
		if (halcmd_inifile == NULL) {
		    /* it's the first -i (ignore repeats) */
                    /* there is a following arg, and it's not an option */
                    filename = optarg;
                    halcmd_inifile = fopen(filename, "r");
                    if (halcmd_inifile == NULL) {
                        fprintf(stderr,
                            "Could not open ini file '%s'\n",
                            filename);
                        exit(-1);
                    }
                    /* make sure file is closed on exec() */
                    fd = fileno(halcmd_inifile);
                    fcntl(fd, F_SETFD, FD_CLOEXEC);
		}
		break;
#endif /* NO_INI */
	    default:
		/* unknown option */
		printf("Unknown option '-%c'\n", cp1);
		break;
        }
    }
    if(filemode) {
        /* it's the first -f (ignore repeats) */
        if (argc > optind) {
            /* there is a following arg, and it's not an option */
            filename = argv[optind++];
            srcfile = fopen(filename, "r");
            if (srcfile == NULL) {
                fprintf(stderr,
                    "Could not open command file '%s'\n",
                    filename);
                exit(-1);
            }
            /* make sure file is closed on exec() */
            fd = fileno(srcfile);
            fcntl(fd, F_SETFD, FD_CLOEXEC);
        } else {
            /* no filename followed -f option, use stdin */
            srcfile = stdin;
            prompt_mode = 1;
        }
    }

    if ( halcmd_startup() != 0 ) return 1;

    retval = 0;
    errorcount = 0;
    /* HAL init is OK, let's process the command(s) */
    if (srcfile == NULL) {
	/* the remaining command line args are parts of the command */
	/* copy them to a long string for variable replacement */
	raw_buf[0] = '\0';
        n = optind;
	while (n < argc) {
	    strncat(raw_buf, argv[n++], MAX_CMD_LEN);
	    strncat(raw_buf, " ", MAX_CMD_LEN);
	}


	retval = halcmd_parse_line(raw_buf);
	if (retval != 0) {
	    errorcount++;
	}
    } else {
	/* read command line(s) from 'srcfile' */
	while (get_input(srcfile, raw_buf, MAX_CMD_LEN)) {
	    char *tokens[MAX_TOK+1];
	    linenumber++;
	    /* remove comments, do var substitution, and tokenise */
	    retval = halcmd_preprocess_line(raw_buf, tokens);
	    if (retval == 0) {
		/* the "quit" command is not handled by parse_line() */
		if ( ( strcasecmp(tokens[0],"quit") == 0 ) ||
		     ( strcasecmp(tokens[0],"exit") == 0 ) ) {
		    break;
		}
		/* process command */
		retval = halcmd_parse_cmd(tokens);
	    }
	    /* did a signal happen while we were busy? */
	    if ( halcmd_done ) {
		/* treat it as an error */
		errorcount++;
		/* exit from loop */
		break;
	    }
	    if ( retval != 0 ) {
		errorcount++;
	    }
	    if (( errorcount > 0 ) && ( keep_going == 0 )) {
		/* exit from loop */
		break;
	    }
	}
    }
    /* all done */
    halcmd_shutdown();
    if ( errorcount > 0 ) {
	return 1;
    } else {
	return 0;
    }

}

/* release_HAL_mutex() unconditionally releases the hal_mutex
   very useful after a program segfaults while holding the mutex
*/
static int release_HAL_mutex(void)
{
    int comp_id, mem_id, retval;
    void *mem;
    hal_data_t *hal_data;

    /* do RTAPI init */
    comp_id = rtapi_init("hal_unlocker");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ERROR: rtapi init failed\n");
        return HAL_FAIL;
    }
    /* get HAL shared memory block from RTAPI */
    mem_id = rtapi_shmem_new(HAL_KEY, comp_id, HAL_SIZE);
    if (mem_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "ERROR: could not open shared memory\n");
        rtapi_exit(comp_id);
        return HAL_FAIL;
    }
    /* get address of shared memory area */
    retval = rtapi_shmem_getptr(mem_id, &mem);
    if (retval != RTAPI_SUCCESS) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "ERROR: could not access shared memory\n");
        rtapi_exit(comp_id);
        return HAL_FAIL;
    }
    /* set up internal pointers to shared mem and data structure */
    hal_data = (hal_data_t *) mem;
    /* release mutex  */
    rtapi_mutex_give(&(hal_data->mutex));
    /* release RTAPI resources */
    rtapi_shmem_delete(mem_id, comp_id);
    rtapi_exit(comp_id);
    /* done */
    return HAL_SUCCESS;

}



static void print_help_general(int showR)
{
    printf("\nUsage:   halcmd [options] [cmd [args]]\n\n");
    printf("\n         halcmd [options] -f [filename]\n\n");
    printf("options:\n\n");
    printf("  -f [filename]  Read commands from 'filename', not command\n");
    printf("                 line.  If no filename, read from stdin.\n");
#ifndef NO_INI
    printf("  -i filename    Open .ini file 'filename', allow commands\n");
    printf("                 to get their values from ini file.\n");
#endif
    printf("  -k             Keep going after failed command.  Default\n");
    printf("                 is to exit if any command fails. (Useful with -f)\n");
    printf("  -q             Quiet - print errors only (default).\n");
    printf("  -Q             Very quiet - print nothing.\n");
    if (showR != 0) {
    printf("  -R             Release mutex (for crash recovery only).\n");
    }
    printf("  -s             Script friendly - don't print headers on output.\n");
    printf("  -v             Verbose - print result of every command.\n");
    printf("  -V             Very verbose - print lots of junk.\n");
    printf("  -h             Help - print this help screen and exit.\n\n");
    printf("commands:\n\n");
    printf("  loadrt, unloadrt, loadusr, lock, unlock, linkps, linksp, linkpp,\n");
    printf("  unlinkp, newsig, delsig, setp, getp, sets, gets, addf, delf, show,\n");
    printf("  list, save, status, start, stop, quit, exit\n");
    printf("  help           Lists all commands with short descriptions\n");
    printf("  help command   Prints detailed help for 'command'\n\n");
}

#ifdef HAVE_READLINE
#include "halcmd_completion.h"

static int get_input(FILE *srcfile, char *buf, size_t bufsize) {
    static int first_time = 1;
    char *rlbuf;

    if(!scriptmode && srcfile == stdin && isatty(0)) {
        if(first_time) {
            halcmd_init_readline();
            first_time = 0;
        }
        rlbuf = readline("halcmd: ");
        if(!rlbuf) return 0;
        strncpy(buf, rlbuf, bufsize);
        buf[bufsize-1] = 0;
        free(rlbuf);

        if(*buf) add_history(buf);

        return 1;
    }
    if(prompt_mode) {
	    fprintf(stdout, scriptmode ? "%%\n" : "halcmd: "); fflush(stdout);
    }
    return fgets(buf, bufsize, srcfile) != NULL;
}
#else
static int get_input(FILE *srcfile, char *buf, size_t bufsize) {
    if(prompt_mode) {
	    fprintf(stdout, scriptmode ? "%%\n" : "halcmd: "); fflush(stdout);
    }
    return fgets(buf, bufsize, srcfile) != NULL;
}
#endif

void halcmd_output(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

