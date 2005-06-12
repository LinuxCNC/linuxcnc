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
#include <signal.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "../hal_priv.h"	/* private HAL decls */
/* non-EMC related uses of halcmd may want to avoid libnml dependency */
#ifndef NO_INI
#include "inifile.h"		/* iniFind() from libnml */
#endif

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/* These functions are used internally by this file.  The code is at
   the end of the file.  */

#define MAX_TOK 20
#define MAX_CMD_LEN 1024

static int parse_cmd(char *tokens[]);
static int do_help_cmd(char *command);
static int do_lock_cmd(char *command);
static int do_unlock_cmd(char *command);
static int do_link_cmd(char *pin, char *sig);
static int do_newsig_cmd(char *name, char *type);
static int do_setp_cmd(char *name, char *value);
static int do_getp_cmd(char *name);
static int do_sets_cmd(char *name, char *value);
static int do_gets_cmd(char *name);
static int do_show_cmd(char *type, char *pattern);
static int do_list_cmd(char *type, char *pattern);
static int do_status_cmd(char *type);
static int do_loadrt_cmd(char *mod_name, char *args[]);
static int do_delsig_cmd(char *mod_name);
static int do_unloadrt_cmd(char *mod_name);
static int unloadrt_comp(char *mod_name);
static void print_comp_info(char *pattern);
static void print_pin_info(char *pattern);
static void print_sig_info(char *pattern);
static void print_param_info(char *pattern);
static void print_funct_info(char *pattern);
static void print_thread_info(char *pattern);
static void print_comp_names(char *pattern);
static void print_pin_names(char *pattern);
static void print_sig_names(char *pattern);
static void print_param_names(char *pattern);
static void print_funct_names(char *pattern);
static void print_thread_names(char *pattern);
static void print_lock_status();
static char *data_type(int type);
static char *data_dir(int dir);
static char *data_arrow1(int dir);
static char *data_arrow2(int dir);
static char *data_value(int type, void *valptr);
static char *data_value2(int type, void *valptr);
static int do_save_cmd(char *type);
static void save_signals(void);
static void save_links(int arrows);
static void save_nets(int arrows);
static void save_params(void);
static void save_threads(void);
static void print_help_general(void);
static void print_help_commands(void);

/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

int comp_id = -1;	/* -1 means hal_init() not called yet */
int hal_flag = 0;	/* used to indicate that halcmd might have the
			   hal mutex, so the sig handler can't just
			   exit, instead it must set 'done' */
int done = 0;		/* used to break out of processing loop */
#ifndef NO_INI
    FILE *inifile = NULL;
#endif


/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/

/* signal handler */
static void quit(int sig)
{
    if ( hal_flag ) {
	/* this process might have the hal mutex, so just set the
	   'done' flag and return, exit after mutex work finishes */
	done = 1;
    } else {
	/* don't have to worry about the mutex, but if we just
	   return, we might return into the fgets() and wait 
	   all day instead of exiting.  So we exit from here. */
	if ( comp_id > 0 ) {
	    hal_exit(comp_id);
	}
	_exit(1);
    }
}

/* main() is responsible for parsing command line options, and then
   parsing either a single command from the command line or a series
   of commands from a file or standard input.  It breaks the command[s]
   into tokens, and passes them to parse_cmd() which does the actual
   work for each command.
*/

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
    FILE *srcfile = NULL;
    char buf[30];
    char cmd_buf[MAX_CMD_LEN+1];
    char *tokens[MAX_TOK+1];

    if (argc < 2) {
	/* no args specified, print help */
	print_help_general();
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
		print_help_general();
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
		if (srcfile == NULL) {
		    /* it's the first -f (ignore repeats) */
		    if ((n < argc) && (argv[n][0] != '-')) {
			/* there is a following arg, and it's not an option */
			filename = argv[n++];
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
		    }
		}
		break;
#ifndef NO_INI
	    case 'i':
		/* -i = allow reading 'setp' values from an ini file */
		if (inifile == NULL) {
		    /* it's the first -i (ignore repeats) */
		    if ((n < argc) && (argv[n][0] != '-')) {
			/* there is a following arg, and it's not an option */
			filename = argv[n++];
			inifile = fopen(filename, "r");
			if (inifile == NULL) {
			    fprintf(stderr,
				"Could not open ini file '%s'\n",
				filename);
			    exit(-1);
			}
			/* make sure file is closed on exec() */
			fd = fileno(inifile);
			fcntl(fd, F_SETFD, FD_CLOEXEC);
		    } else {
			/* no filename followed -i option, error */
			fprintf(stderr,
			    "No missing ini filename for -i option\n");
			exit(-1);
		    }
		}
		break;
#endif /* NO_INI */
	    default:
		/* unknown option */
		printf("Unknown option '-%c'\n", *cp1);
		break;
	    }
	}
    }
    /* register signal handlers - if the process is killed
       we need to call hal_exit() to free the shared memory */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    /* at this point all options are parsed, connect to HAL */
    /* create a unique module name, to allow for multiple halcmd's */
    snprintf(buf, 29, "halcmd%d", getpid());
    /* connect to the HAL */
    comp_id = hal_init(buf);
    if (comp_id < 0) {
	fprintf(stderr, "halcmd: hal_init() failed\n" );
	fprintf(stderr, "NOTE: 'rtapi' kernel module must be loaded\n" );
	return 1;
    }
    retval = 0;
    errorcount = 0;
    /* HAL init is OK, let's process the command(s) */
    if (srcfile == NULL) {
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
	/* read command line(s) from 'srcfile' */
	while (fgets(cmd_buf, MAX_CMD_LEN, srcfile) != NULL) {
	    /* convert a line of text into individual tokens */
	    m = 0;
	    cp1 = cmd_buf;
	    state = BETWEEN_TOKENS;
	    while ( m < MAX_TOK ) {
		switch ( state ) {
		case BETWEEN_TOKENS:
		    if (( *cp1 == '\n' ) || ( *cp1 == '\0' )) {
			/* end of the line */
			state = END_OF_LINE;
		    } else if ( isspace(*cp1) ) {
			/* whitespace, skip it */
			cp1++;
		    } else {
			/* first char of a token */
			tokens[m] = cp1++;
			state = IN_TOKEN;
		    }
		    break;
		case IN_TOKEN:
		    if (( *cp1 == '\n' ) || ( *cp1 == '\0' )) {
			/* end of the line, terminate current token */
			*cp1++ = '\0';
			m++;
			state = END_OF_LINE;
		    } else if ( *cp1 == '\'' ) {
			/* start of a quoted string */
			cp1++;
			state = SINGLE_QUOTE;
		    } else if ( *cp1 == '\"' ) {
			/* start of a quoted string */
			cp1++;
			state = DOUBLE_QUOTE;
		    } else if ( isspace(*cp1) ) {
			/* end of the current token */
			*cp1++ = '\0';
			m++;
			state = BETWEEN_TOKENS;
		    } else {
			/* ordinary character */
			cp1++;
		    }
		    break;
		case SINGLE_QUOTE:
		    if (( *cp1 == '\n' ) || ( *cp1 == '\0' )) {
			/* end of the line, terminate current token */
			*cp1++ = '\0';
			m++;
			state = END_OF_LINE;
		    } else if ( *cp1 == '\'' ) {
			/* end of quoted string */
			cp1++;
			state = IN_TOKEN;
		    } else {
			/* ordinary character */
			cp1++;
		    }
		    break;
		case DOUBLE_QUOTE:
		    if (( *cp1 == '\n' ) || ( *cp1 == '\0' )) {
			/* end of the line, terminate current token */
			*cp1++ = '\0';
			m++;
			state = END_OF_LINE;
		    } else if ( *cp1 == '\"' ) {
			/* end of quoted string */
			cp1++;
			state = IN_TOKEN;
		    } else {
			/* ordinary character, copy to buffer */
			cp1++;
		    }
		    break;
		case END_OF_LINE:
		    *cp1 = '\0';
		    tokens[m++] = cp1;
		    break;
		default:
		    /* should never get here */
		    rtapi_print_msg(RTAPI_MSG_ERR,
			"Bad state in token parser\n");
		    done = 1;
		}
	    }
	    /* tokens[] contains MAX_TOK+1 elements so there is always
	       at least one empty one at the end... make it empty now */
	    tokens[MAX_TOK] = "";
	    /* the "quit" command is not handled by parse_cmd() */
	    if ( strcasecmp(tokens[0],"quit") == 0 ) {
		break;
	    }
	    /* let the signal handler know that we might have the mutex */
	    hal_flag = 1;
	    /* process command */
            retval = parse_cmd(tokens);
	    /* done with the mutex, can simply exit on a signal */
	    hal_flag = 0;
	    /* did a signal happen while we were busy? */
	    if ( done ) {
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
    hal_exit(comp_id);
    if ( errorcount > 0 ) {
	return 1;
    } else {
	return 0;
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

/* parse_cmd() decides which command has been invoked, gathers any
   arguments that are needed, and calls a function to execute the
   command.
*/

static int parse_cmd(char *tokens[])
{
    int retval;

#if 0
    int n;
    /* for testing: prints tokens that make up the command */
    for ( n = 0 ; n < MAX_TOK ; n++ ) {
	printf ( "%02d:{%s}\n", n, tokens[n] );
    }
#endif
    /* tokens[0] is the command */
    if ((tokens[0][0] == '#') || (tokens[0][0] == '\0')) {
	/* comment or blank line, do nothing */
	retval = 0;
    } else if (strcmp(tokens[0], "help") == 0) {
	/* help for a specific command? */
	if (tokens[1][0] != '\0') {
	    /* yes, get info for that command */
	    retval = do_help_cmd(tokens[1]);
	} else {
	    /* no, print list of commands */
	    print_help_commands();
	    retval = 0;
	}
    } else if (strcmp(tokens[0], "linkps") == 0) {
	/* check for an arrow */
	if ((tokens[2][0] == '=') || (tokens[2][0] == '<')) {
	    retval = do_link_cmd(tokens[1], tokens[3]);
	} else {
	    retval = do_link_cmd(tokens[1], tokens[2]);
	}
    } else if (strcmp(tokens[0], "linksp") == 0) {
	/* check for an arrow */
	if ((tokens[2][0] == '=') || (tokens[2][0] == '<')) {
	    /* arrow found, skip it */
	    retval = do_link_cmd(tokens[3], tokens[1]);
	} else {
	    /* no arrow */
	    retval = do_link_cmd(tokens[2], tokens[1]);
	}
    } else if (strcmp(tokens[0], "unlinkp") == 0) {
	retval = do_link_cmd(tokens[1], "\0");
    } else if (strcmp(tokens[0], "newsig") == 0) {
	retval = do_newsig_cmd(tokens[1], tokens[2]);
    } else if (strcmp(tokens[0], "delsig") == 0) {
	retval = do_delsig_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "setp") == 0) {
	retval = do_setp_cmd(tokens[1], tokens[2]);
    } else if (strcmp(tokens[1], "=") == 0) {
	retval = do_setp_cmd(tokens[0], tokens[2]);
    } else if (strcmp(tokens[0], "sets") == 0) {
	retval = do_sets_cmd(tokens[1], tokens[2]);
    } else if (strcmp(tokens[0], "getp") == 0) {
	retval = do_getp_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "gets") == 0) {
	retval = do_gets_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "show") == 0) {
	retval = do_show_cmd(tokens[1], tokens[2]);
    } else if (strcmp(tokens[0], "list") == 0) {
	retval = do_list_cmd(tokens[1], tokens[2]);
    } else if (strcmp(tokens[0], "status") == 0) {
	retval = do_status_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "lock") == 0) {
	retval = do_lock_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "unlock") == 0) {
	retval = do_unlock_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "loadrt") == 0) {
	retval = do_loadrt_cmd(tokens[1], &tokens[2]);
    } else if (strcmp(tokens[0], "unloadrt") == 0) {
	retval = do_unloadrt_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "save") == 0) {
	retval = do_save_cmd(tokens[1]);
    } else if (strcmp(tokens[0], "addf") == 0) {
	/* did the user specify a position? */
	if (tokens[3][0] == '\0') {
	    /* no - add function at end of thread */
	    retval = hal_add_funct_to_thread(tokens[1], tokens[2], -1);
	} else {
	    retval =
		hal_add_funct_to_thread(tokens[1], tokens[2],
		atoi(tokens[3]));
	}
	if (retval == 0) {
	    /* print success message */
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"Function '%s' added to thread '%s'\n", tokens[1], tokens[2]);
	}
    } else if (strcmp(tokens[0], "delf") == 0) {
	retval = hal_del_funct_from_thread(tokens[1], tokens[2]);
	if (retval == 0) {
	    /* print success message */
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"Function '%s' removed from thread '%s'\n", tokens[1],
		tokens[2]);
	}
    } else if (strcmp(tokens[0], "start") == 0) {
	retval = hal_start_threads();
	if (retval == 0) {
	    /* print success message */
	    rtapi_print_msg(RTAPI_MSG_INFO, "Realtime threads started\n");
	}
    } else if (strcmp(tokens[0], "stop") == 0) {
	retval = hal_stop_threads();
	if (retval == 0) {
	    /* print success message */
	    rtapi_print_msg(RTAPI_MSG_INFO, "Realtime threads stopped\n");
	}
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "Unknown command '%s'\n", tokens[0]);
	retval = -1;
    }
    return retval;
}

static int do_lock_cmd(char *command)
{
    int retval=0;

    /* are we running as root? */
    if ( getuid() != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Must be root to lock HAL\n");
	return -2;
    }

    /* if command is blank, want to lock everything */
    if (*command == '\0') {
	retval = hal_set_lock(HAL_LOCK_ALL);
    } else if (strcmp(command, "none") == 0) {
	retval = hal_set_lock(HAL_LOCK_NONE);
    } else if (strcmp(command, "tune") == 0) {
	retval = hal_set_lock(HAL_LOCK_LOAD & HAL_LOCK_CONFIG);
    } else if (strcmp(command, "all") == 0) {
	retval = hal_set_lock(HAL_LOCK_ALL);
    }

    if (retval == 0) {
	/* print success message */
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "Locking completed");
    } else {
	rtapi_print_msg(RTAPI_MSG_INFO, "Locking failed\n");
    }
    return retval;
}

static int do_unlock_cmd(char *command)
{
    int retval=0;

    /* are we running as root? */
    if ( getuid() != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Must be root to unlock HAL\n");
	return -2;
    }

    /* if command is blank, want to lock everything */
    if (*command == '\0') {
	retval = hal_set_lock(HAL_LOCK_NONE);
    } else if (strcmp(command, "all") == 0) {
	retval = hal_set_lock(HAL_LOCK_NONE);
    } else if (strcmp(command, "tune") == 0) {
	retval = hal_set_lock(HAL_LOCK_LOAD & HAL_LOCK_CONFIG);
    }

    if (retval == 0) {
	/* print success message */
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "Unlocking completed");
    } else {
	rtapi_print_msg(RTAPI_MSG_INFO, "Unlocking failed\n");
    }
    return retval;
}

static int do_link_cmd(char *pin, char *sig)
{
    int retval;

    /* if sig is blank, want to unlink pin */
    if (*sig == '\0') {
	/* tell hal_link() to unlink the pin */
	sig = NULL;
    }
    /* make the link */
    retval = hal_link(pin, sig);
    if (retval == 0) {
	/* print success message */
	if (sig != NULL) {
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"Pin '%s' linked to signal '%s'\n", pin, sig);
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "Pin '%s' unlinked\n", pin);
	}
    }
    return retval;
}

static int do_newsig_cmd(char *name, char *type)
{
    int retval;

    if (strcasecmp(type, "bit") == 0) {
	retval = hal_signal_new(name, HAL_BIT);
    } else if (strcasecmp(type, "float") == 0) {
	retval = hal_signal_new(name, HAL_FLOAT);
    } else if (strcasecmp(type, "u8") == 0) {
	retval = hal_signal_new(name, HAL_U8);
    } else if (strcasecmp(type, "s8") == 0) {
	retval = hal_signal_new(name, HAL_S8);
    } else if (strcasecmp(type, "u16") == 0) {
	retval = hal_signal_new(name, HAL_U16);
    } else if (strcasecmp(type, "s16") == 0) {
	retval = hal_signal_new(name, HAL_S16);
    } else if (strcasecmp(type, "u32") == 0) {
	retval = hal_signal_new(name, HAL_U32);
    } else if (strcasecmp(type, "s32") == 0) {
	retval = hal_signal_new(name, HAL_S32);
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "Unknown signal type '%s'\n", type);
	retval = HAL_INVAL;
    }
    return retval;
}

static int do_setp_cmd(char *name, char *value)
{
    int retval;
    hal_param_t *param;
    hal_type_t type;
    void *d_ptr;
    char *cp;
    float fval;
    long lval;
    unsigned long ulval;
    char *envvar = NULL;
#ifndef NO_INI
    char *section = NULL;
    char *variable = NULL;
#endif

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: setting parameter '%s' to '%s'\n", name, value);
    /* check for special cases of 'value' */
    cp = value;
    if ( *cp == '$' ) {
	/* environment variable reference */
	/* skip over '$' to name of variable */
	cp++;
	envvar = getenv(cp);
	if ( envvar == NULL ) {
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: env variable '%s' not found\n", cp);
	    return HAL_INVAL;
	}
	value = envvar;
    }
#ifndef NO_INI
    else if ( *cp == '[' ) {
	/* ini file variable reference */
	/* skip over leading '[' and save ptr to start of section name */
	section = ++cp;
	/* look for end of section name */
	while ( *cp != ']' ) {
	    if ( *cp == '\0' ) {
		rtapi_mutex_give(&(hal_data->mutex));
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL: ERROR: ini file reference '%s' missing ']'\n", value);
		return HAL_INVAL;
	    }
	    cp++;
	}
	/* found trailing ']', replace with '\0' to end section */
	*cp = '\0';
	/* skip over '\0' and save pointer to variable name */
	variable = ++cp;
	if ( *section != '\0' ) {
	    /* get value from ini file */
	    /* cast to char ptr, we are discarding the 'const' */
	    value = (char *) iniFind(inifile, variable, section);
	} else {
	    /* no section specified */
	    value = (char *) iniFind(inifile, variable, NULL);
	}
	if ( value == NULL ) {
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: ini file variable '[%s]%s' not found\n", section, variable);
	    return HAL_INVAL;
	}
    }
#endif /* NO_INI */
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: parameter '%s' not found\n", name);
	return HAL_INVAL;
    }
    /* found it */
    type = param->type;
    /* is it writable? */
    if (param->dir == HAL_RD) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param '%s' is not writable\n", name);
	return HAL_INVAL;
    }
    d_ptr = SHMPTR(param->data_ptr);
    retval = 0;
    switch (type) {
    case HAL_BIT:
	if ((strcmp("1", value) == 0) || (strcasecmp("TRUE", value) == 0)) {
	    *(hal_bit_t *) (d_ptr) = 1;
	} else if ((strcmp("0", value) == 0)
	    || (strcasecmp("FALSE", value)) == 0) {
	    *(hal_bit_t *) (d_ptr) = 0;
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for bit parameter\n", value);
	    retval = HAL_INVAL;
	}
	break;
    case HAL_FLOAT:
	fval = strtod ( value, &cp );
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid character(s) in string */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for float parameter\n",
		value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_float_t *) (d_ptr)) = fval;
	}
	break;
    case HAL_S8:
	lval = strtol(value, &cp, 0);
	if (((*cp != '\0') && (!isspace(*cp))) || (lval > 127) || (lval < -128)) {
	    /* invalid chars in string, or outside limits of S8 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for S8 parameter\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_s8_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U8:
	ulval = strtoul(value, &cp, 0);
	if (((*cp != '\0') && (!isspace(*cp))) || (ulval > 255)) {
	    /* invalid chars in string, or outside limits of U8 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for U8 parameter\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_u8_t *) (d_ptr)) = ulval;
	}
	break;
    case HAL_S16:
	lval = strtol(value, &cp, 0);
	if (((*cp != '\0') && (!isspace(*cp))) || (lval > 32767) || (lval < -32768)) {
	    /* invalid chars in string, or outside limits of S16 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for S16 parameter\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_s16_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U16:
	ulval = strtoul(value, &cp, 0);
	if (((*cp != '\0') && (!isspace(*cp))) || (ulval > 65535)) {
	    /* invalid chars in string, or outside limits of U16 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for U16 parameter\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_u16_t *) (d_ptr)) = ulval;
	}
	break;
    case HAL_S32:
	lval = strtol(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for S32 parameter\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_s32_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U32:
	ulval = strtoul(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for U32 parameter\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_u32_t *) (d_ptr)) = ulval;
	}
	break;
    default:
	/* Shouldn't get here, but just in case... */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: bad type %d setting param '%s'\n", type, name);
	retval = HAL_INVAL;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    if (retval == 0) {
	/* print success message */
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "Parameter '%s' set to %s\n", name, value);
    }
    return retval;

}

static int do_getp_cmd(char *name)
{
    hal_param_t *param;
    hal_type_t type;
    void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: getting parameter '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: parameter '%s' not found\n", name);
	return HAL_INVAL;
    }
    /* found it */
    type = param->type;
    d_ptr = SHMPTR(param->data_ptr);
    rtapi_print("%s\n", data_value2((int) type, d_ptr));
    rtapi_mutex_give(&(hal_data->mutex));
    return HAL_SUCCESS;
}

static int do_sets_cmd(char *name, char *value)
{
    int retval;
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;
    char *cp;
    float fval;
    long lval;
    unsigned long ulval;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: setting signal '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));

    /* search signal list for name */
    sig = halpr_find_sig_by_name(name);
    if (sig == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' not found\n", name);
	return HAL_INVAL;
    }
    /* found it - does it have a writer? */
    if (sig->writers > 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' already has writer(s)\n", name);
	return HAL_INVAL;
    }
    /* no writer, so we can safely set it */
    type = sig->type;
    d_ptr = SHMPTR(sig->data_ptr);
    retval = 0;
    switch (type) {
    case HAL_BIT:
	if ((strcmp("1", value) == 0) || (strcasecmp("TRUE", value) == 0)) {
	    *(hal_bit_t *) (d_ptr) = 1;
	} else if ((strcmp("0", value) == 0)
	    || (strcasecmp("FALSE", value)) == 0) {
	    *(hal_bit_t *) (d_ptr) = 0;
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for bit signal\n", value);
	    retval = HAL_INVAL;
	}
	break;
    case HAL_FLOAT:
	fval = strtod(value, &cp);
	if (*cp != '\0') {
	    /* invalid chars in string */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for float signal\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_float_t *) (d_ptr)) = fval;
	}
	break;
    case HAL_S8:
	lval = strtol(value, &cp, 0);
	if ((*cp != '\0') || (lval > 127) || (lval < -128)) {
	    /* invalid chars in string, or outside limits of S8 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for S8 signal\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_s8_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U8:
	ulval = strtoul(value, &cp, 0);
	if ((*cp != '\0') || (ulval > 255)) {
	    /* invalid chars in string, or outside limits of U8 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for U8 signal\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_u8_t *) (d_ptr)) = ulval;
	}
	break;
    case HAL_S16:
	lval = strtol(value, &cp, 0);
	if ((*cp != '\0') || (lval > 32767) || (lval < -32768)) {
	    /* invalid chars in string, or outside limits of S16 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for S16 signal\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_s16_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U16:
	ulval = strtoul(value, &cp, 0);
	if ((*cp != '\0') || (ulval > 65535)) {
	    /* invalid chars in string, or outside limits of U16 */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for U16 signal\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_u16_t *) (d_ptr)) = ulval;
	}
	break;
    case HAL_S32:
	lval = strtol(value, &cp, 0);
	if (*cp != '\0') {
	    /* invalid chars in string */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for S32 signal\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_s32_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U32:
	ulval = strtoul(value, &cp, 0);
	if (*cp != '\0') {
	    /* invalid chars in string */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: value '%s' invalid for U32 signal\n", value);
	    retval = HAL_INVAL;
	} else {
	    *((hal_u32_t *) (d_ptr)) = ulval;
	}
	break;
    default:
	/* Shouldn't get here, but just in case... */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: bad type %d setting signal '%s'\n", type, name);
	retval = HAL_INVAL;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    if (retval == 0) {
	/* print success message */
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "Signal '%s' set to %s\n", name, value);
    }
    return retval;

}

static int do_gets_cmd(char *name)
{
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: getting signal '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search signal list for name */
    sig = halpr_find_sig_by_name(name);
    if (sig == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' not found\n", name);
	return HAL_INVAL;
    }
    /* found it */
    type = sig->type;
    d_ptr = SHMPTR(sig->data_ptr);
    rtapi_print("%s\n", data_value2((int) type, d_ptr));
    rtapi_mutex_give(&(hal_data->mutex));
    return HAL_SUCCESS;
}

static int do_show_cmd(char *type, char *pattern)
{

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if (*type == '\0') {
	/* print everything */
	print_comp_info("");
	print_pin_info("");
	print_sig_info("");
	print_param_info("");
	print_funct_info("");
	print_thread_info("");
    } else if (strcmp(type, "all") == 0) {
	/* print everything, using the pattern */
	print_comp_info(pattern);
	print_pin_info(pattern);
	print_sig_info(pattern);
	print_param_info(pattern);
	print_funct_info(pattern);
	print_thread_info(pattern);
    } else if (strcmp(type, "comp") == 0) {
	print_comp_info(pattern);
    } else if (strcmp(type, "pin") == 0) {
	print_pin_info(pattern);
    } else if (strcmp(type, "sig") == 0) {
	print_sig_info(pattern);
    } else if (strcmp(type, "param") == 0) {
	print_param_info(pattern);
    } else if (strcmp(type, "funct") == 0) {
	print_funct_info(pattern);
    } else if (strcmp(type, "thread") == 0) {
	print_thread_info(pattern);
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "Unknown 'show' type '%s'\n", type);
	return -1;
    }
    return 0;
}

static int do_list_cmd(char *type, char *pattern)
{

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if (strcmp(type, "comp") == 0) {
	print_comp_names(pattern);
    } else if (strcmp(type, "pin") == 0) {
	print_pin_names(pattern);
    } else if (strcmp(type, "sig") == 0) {
	print_sig_names(pattern);
    } else if (strcmp(type, "param") == 0) {
	print_param_names(pattern);
    } else if (strcmp(type, "funct") == 0) {
	print_funct_names(pattern);
    } else if (strcmp(type, "thread") == 0) {
	print_thread_names(pattern);
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "Unknown 'list' type '%s'\n", type);
	return -1;
    }
    return 0;
}

static int do_status_cmd(char *type)
{

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if ((*type == '\0') || (strcmp(type, "all") == 0)) {
	/* print everything */
	/*! \todo FIXME - add other status functions here */
	print_lock_status();
    } else if (strcmp(type, "lock") == 0) {
	print_lock_status();
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "Unknown 'status' type '%s'\n", type);
	return -1;
    }
    return 0;
}

static int do_loadrt_cmd(char *mod_name, char *args[])
{
    static char *insmod_path = NULL;
    static char *rtmod_dir = NULL;
    struct stat stat_buf;
    static char path_buf[MAX_CMD_LEN];
    char mod_path[MAX_CMD_LEN];
    char *cp1, *cp2;
    char *argv[MAX_TOK+1];
    int n, m, retval, status;
    pid_t pid;

    /* are we running as root? */
    if ( getuid() != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Must be root to load realtime modules\n");
	return -2;
    }
    
    if (hal_get_lock()&HAL_LOCK_LOAD) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: HAL is locked, loading of modules is not permitted\n");
	return -2;
    }
    
    /* check for insmod */
    if ( insmod_path == NULL ) {
	/* need to find insmod */
	if ( stat("/sbin/insmod", &stat_buf) == 0 ) {
	    insmod_path = "/sbin/insmod";
	}
    }
    if ( insmod_path == NULL ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Cannot locate 'insmod' command\n");
	return -2;
    }
    /* check environment for path to realtime HAL modules */
    if ( rtmod_dir == NULL ) {
	rtmod_dir = getenv ( "HAL_RTMOD_DIR" );
    }
    /*! \todo FIXME - the following is an attempt to locate the directory
       where the HAL modules are stored.  It depends on the emc2
       directory tree being laid out per 'emc2/directory.map'.
       There is probably a better way of doing this... it will
       certainly need changed once we have a 'make install' that
       puts modules somewhere else. */
    if ( rtmod_dir == NULL ) {
	/* no env variable, try to locate the directory based on
	   where this executable lives (should be in the emc2 tree) */
	n = readlink("/proc/self/exe", path_buf, MAX_CMD_LEN-10);
	if ( n > 0 ) {
	    path_buf[n] = '\0';
	    /* have path to executabie, find last instance of 'emc2' */
	    cp2 = "";
	    cp1 = path_buf;
	    while ( (cp1 = strstr ( cp1, "/emc2" )) != NULL ) {
		cp2 = cp1++;
	    }
	    /* is the current dir a subdir of emc2? */
	    if ( cp2[5] == '/' ) {
		/* yes, chop subdirectories */
		cp2[5] = '\0';
	    }
	    /* is it something like '/emc22'? */
	    if ( strcmp ( cp2, "/emc2" ) == 0 ) {
		/* nope, maybe we found the right place... */
		strcat ( path_buf, "/rtlib" );
		rtmod_dir = path_buf;
	    }
	}
    }
    /*! \todo FIXME - the following code works if you are anywhere in
       the standard emc2 directory tree, but it is a kludge
       that depends on the structure of the tree, and fails
       completely if you run it from elsewhere */
    /* update - the above code using /proc/self/exe was added
       later, and works from anywhere, as long as the halcmd
       executable is in the emc2 tree.  So the following would
       only be used on machines without a /proc filesystem */
    if ( rtmod_dir == NULL ) {
	/*still can't find directory, try to locate it based
	   on the current working dir */
        if ( getcwd(path_buf, MAX_CMD_LEN-10) != NULL ) {
	    /* got current path, find last instance of 'emc2' */
	    cp2 = "";
	    cp1 = path_buf;
	    while ( (cp1 = strstr ( cp1, "/emc2" )) != NULL ) {
		cp2 = cp1++;
	    }
	    /* is the current dir a subdir of emc2? */
	    if ( cp2[5] == '/' ) {
		/* yes, chop subdirectories */
		cp2[5] = '\0';
	    }
	    /* is it something like '/emc22'? */
	    if ( strcmp ( cp2, "/emc2" ) == 0 ) {
		/* nope, maybe we found the right place... */
		strcat ( path_buf, "/rtlib" );
		rtmod_dir = path_buf;
	    }
	}
    }
    if ( rtmod_dir == NULL ) {
	/* still don't know where the modules are */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Cannot locate realtime modules directory\n");
	return -2;
    }
    if ( (strlen(rtmod_dir)+strlen(mod_name)+5) > MAX_CMD_LEN ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Module path too long\n");
	return -1;
    }
    /* make full module name '<path>/<name>.o' */
    strcpy (mod_path, rtmod_dir);
    strcat (mod_path, "/");
    strcat (mod_path, mod_name);
    strcat (mod_path, ".o");
    /* is there a file with that name? */
    if ( stat(mod_path, &stat_buf) != 0 ) {
	/* nope, try .ko (for kernel 2.6 */
	cp2 = strrchr(mod_path, '.' );
	strcpy(cp2, ".ko" );
	if ( stat(mod_path, &stat_buf) != 0 ) {
	    /* can't find it */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: Can't find module '%s' in %s\n", mod_name, path_buf);
	    return -1;
	}
    }
    /* now we need to fork, and then exec insmod.... */
    /* disconnect from the HAL shmem area before forking */
    hal_exit(comp_id);
    comp_id = 0;
    /* now the fork() */
    pid = fork();
    if ( pid < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: fork() failed\n");
	return -1;
    }
    if ( pid == 0 ) {
	/* this is the child process - prepare to exec() insmod */
	argv[0] = insmod_path;
	argv[1] = mod_path;
	/* loop thru remaining arguments */
	n = 0;
	m = 2;
	while ( args[n][0] != '\0' ) {
	    argv[m++] = args[n++];
	}
	/* add a NULL to terminate the argv array */
	argv[m] = NULL;
	/* print debugging info if "very verbose" (-V) */
	rtapi_print_msg(RTAPI_MSG_DBG, "%s %s ", argv[0], argv[1] );
	n = 2;
	while ( argv[n] != NULL ) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s ", argv[n++] );
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "\n" );
	/* call execv() to invoke insmod */
	execv(insmod_path, argv);
	/* should never get here */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: execv(%s) failed\n", insmod_path );
	exit(1);
    }
    /* this is the parent process, wait for child to end */
    retval = waitpid ( pid, &status, 0 );
    /* reconnect to the HAL shmem area */
    comp_id = hal_init("halcmd");
    if (comp_id < 0) {
	fprintf(stderr, "halcmd: hal_init() failed\n" );
	exit(-1);
    }
    /* check result of waitpid() */
    if ( retval < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: waitpid(%d) failed\n", pid );
	return -1;
    }
    if ( WIFEXITED(status) == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: child did not exit normally\n" );
	return -1;
    }
    retval = WEXITSTATUS(status);
    if ( retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insmod failed, returned %d\n", retval );
	return -1;
    }
    /* print success message */
    rtapi_print_msg(RTAPI_MSG_INFO, "Realtime module '%s' loaded\n",
	mod_name);
    return 0;
}

static int do_delsig_cmd(char *mod_name)
{
    int next, retval, retval1, n;
    hal_sig_t *sig;
    char sigs[64][HAL_NAME_LEN+1];

    /* check for "all" */
    if ( strcmp(mod_name, "all" ) != 0 ) {
	retval = hal_signal_delete(mod_name);
	if (retval == 0) {
	    /* print success message */
	    rtapi_print_msg(RTAPI_MSG_INFO, "Signal '%s' deleted'\n",
		mod_name);
	}
	return retval;
    } else {
	/* build a list of signal(s) to delete */
	n = 0;
	rtapi_mutex_get(&(hal_data->mutex));

	next = hal_data->sig_list_ptr;
	while (next != 0) {
	    sig = SHMPTR(next);
	    /* we want to unload this signal, remember it's name */
	    if ( n < 63 ) {
	        strncpy(sigs[n++], sig->name, HAL_NAME_LEN );
	    }
	    next = sig->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	sigs[n][0] = '\0';

	if ( ( sigs[0][0] == '\0' )) {
	    /* desired signals not found */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: no signals found to be deleted\n");
	    return -1;
	}
	/* we now have a list of components, unload them */
	n = 0;
	retval1 = 0;
	while ( sigs[n][0] != '\0' ) {
	    retval = hal_signal_delete(sigs[n]);
	/* check for fatal error */
	    if ( retval < -1 ) {
		return retval;
	    }
	    /* check for other error */
	    if ( retval != 0 ) {
		retval1 = retval;
	    }
	    if (retval == 0) {
		/* print success message */
		rtapi_print_msg(RTAPI_MSG_INFO, "Signal '%s' deleted'\n",
		sigs[n]);
	    }
	    n++;
	}
    }
    return retval1;
}

static int do_unloadrt_cmd(char *mod_name)
{
    int next, retval, retval1, n, all;
    hal_comp_t *comp;
    char comps[64][HAL_NAME_LEN+1];

    /* check for "all" */
    if ( strcmp(mod_name, "all" ) == 0 ) {
	all = 1;
    } else {
	all = 0;
    }
    /* build a list of component(s) to unload */
    n = 0;
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( comp->type == 1 ) {
	    /* found a realtime component */
	    if ( all || ( strcmp(mod_name, comp->name) == 0 )) {
		/* we want to unload this component, remember it's name */
		if ( n < 63 ) {
		    strncpy(comps[n++], comp->name, HAL_NAME_LEN );
		}
	    }
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    /* mark end of list */
    comps[n][0] = '\0';
    if ( !all && ( comps[0][0] == '\0' )) {
	/* desired component not found */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component '%s' is not loaded\n", mod_name );
	return -1;
    }
    /* we now have a list of components, unload them */
    n = 0;
    retval1 = 0;
    while ( comps[n][0] != '\0' ) {
	retval = unloadrt_comp(comps[n++]);
	/* check for fatal error */
	if ( retval < -1 ) {
	    return retval;
	}
	/* check for other error */
	if ( retval != 0 ) {
	    retval1 = retval;
	}
    }
    return retval1;
}

static int unloadrt_comp(char *mod_name)
{
    static char *rmmod_path = NULL;
    struct stat stat_buf;
    int retval, status;
    char *argv[3];
    pid_t pid;

    /* are we running as root? */
    if ( getuid() != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Must be root to unload realtime modules\n");
	return -2;
    }
    /* check for rmmod */
    if ( rmmod_path == NULL ) {
	/* need to find rmmod */
	if ( stat("/sbin/rmmod", &stat_buf) == 0 ) {
	    rmmod_path = "/sbin/rmmod";
	}
    }
    if ( rmmod_path == NULL ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: Cannot locate 'rmmod' command\n");
	return -2;
    }
    /* now we need to fork, and then exec rmmod.... */
    /* disconnect from the HAL shmem area before forking */
    hal_exit(comp_id);
    comp_id = 0;
    /* now the fork() */
    pid = fork();
    if ( pid < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: fork() failed\n");
	return -1;
    }
    if ( pid == 0 ) {
	/* this is the child process - prepare to exec() rmmod */
	argv[0] = rmmod_path;
	argv[1] = mod_name;
	/* add a NULL to terminate the argv array */
	argv[2] = NULL;
	/* print debugging info if "very verbose" (-V) */
	rtapi_print_msg(RTAPI_MSG_DBG, "%s %s\n", argv[0], argv[1] );
	/* call execv() to invoke rmmod */
	execv(rmmod_path, argv);
	/* should never get here */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: execv(%s) failed\n", rmmod_path );
	exit(1);
    }
    /* this is the parent process, wait for child to end */
    retval = waitpid ( pid, &status, 0 );
    /* reconnect to the HAL shmem area */
    comp_id = hal_init("halcmd");
    if (comp_id < 0) {
	fprintf(stderr, "halcmd: hal_init() failed\n" );
	exit(-1);
    }
    /* check result of waitpid() */
    if ( retval < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: waitpid(%d) failed\n", pid );
	return -1;
    }
    if ( WIFEXITED(status) == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: child did not exit normally\n" );
	return -1;
    }
    retval = WEXITSTATUS(status);
    if ( retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: rmmod failed, returned %d\n", retval );
	return -1;
    }
    /* print success message */
    rtapi_print_msg(RTAPI_MSG_INFO, "Realtime module '%s' unloaded\n",
	mod_name);
    return 0;
}

static void print_comp_info(char *pattern)
{
    int next, len;
    hal_comp_t *comp;

    rtapi_print("Loaded HAL Components:\n");
    rtapi_print("ID  Type  Name\n");
    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( strncmp(pattern, comp->name, len) == 0 ) {
	    rtapi_print("%02d  %s  %s\n",
		comp->comp_id, (comp->type ? "RT  " : "User"), comp->name);
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_pin_info(char *pattern)
{
    int next, len;
    hal_pin_t *pin;
    hal_comp_t *comp;
    hal_sig_t *sig;
    void *dptr;

    rtapi_print("Component Pins:\n");
    rtapi_print("Owner  Type  Dir    Value      Name\n");
    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( strncmp(pattern, pin->name, len) == 0 ) {
	    comp = SHMPTR(pin->owner_ptr);
	    if (pin->signal != 0) {
		sig = SHMPTR(pin->signal);
		dptr = SHMPTR(sig->data_ptr);
	    } else {
		sig = 0;
		dptr = &(pin->dummysig);
	    }
	    rtapi_print(" %02d    %s %s  %s  %s",
		comp->comp_id, data_type((int) pin->type),
		data_dir((int) pin->dir),
		data_value((int) pin->type, dptr), pin->name);
	    if (sig == 0) {
		rtapi_print("\n");
	    } else {
		rtapi_print(" %s %s\n", data_arrow1((int) pin->dir), sig->name);
	    }
	}
	next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_sig_info(char *pattern)
{
    int next, len;
    hal_sig_t *sig;
    void *dptr;
    hal_pin_t *pin;

    rtapi_print("Signals:\n");
    rtapi_print("Type      Value      Name\n");
    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if ( strncmp(pattern, sig->name, len) == 0 ) {
	    dptr = SHMPTR(sig->data_ptr);
	    rtapi_print("%s  %s  %s\n", data_type((int) sig->type),
		data_value((int) sig->type, dptr), sig->name);
	    /* look for pin(s) linked to this signal */
	    pin = halpr_find_pin_by_sig(sig, 0);
	    while (pin != 0) {
		rtapi_print("                         %s %s\n",
		    data_arrow2((int) pin->dir), pin->name);
		pin = halpr_find_pin_by_sig(sig, pin);
	    }
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_param_info(char *pattern)
{
    int next, len;
    hal_param_t *param;
    hal_comp_t *comp;

    rtapi_print("Parameters:\n");
    rtapi_print("Owner  Type  Dir    Value      Name\n");
    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( strncmp(pattern, param->name, len) == 0 ) {
	    comp = SHMPTR(param->owner_ptr);
	    rtapi_print(" %02d    %s  %s  %s  %s\n",
		comp->comp_id, data_type((int) param->type),
		data_dir((int) param->dir),
		data_value((int) param->type, SHMPTR(param->data_ptr)),
		param->name);
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_funct_info(char *pattern)
{
    int next, len;
    hal_funct_t *fptr;
    hal_comp_t *comp;

    rtapi_print("Exported Functions:\n");
    rtapi_print("Owner CodeAddr   Arg    FP  Users  Name\n");
    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	fptr = SHMPTR(next);
	if ( strncmp(pattern, fptr->name, len) == 0 ) {
	    comp = SHMPTR(fptr->owner_ptr);
	    rtapi_print(" %02d   %08X %08X %s  %3d   %s\n",
		comp->comp_id,
		fptr->funct,
		fptr->arg, (fptr->uses_fp ? "YES" : "NO "),
		fptr->users, fptr->name);
	}
	next = fptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_thread_info(char *pattern)
{
    int next_thread, len, n;
    hal_thread_t *tptr;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t *funct;

    rtapi_print("Realtime Threads:\n");
    rtapi_print("   Period   FP   Name\n");
    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
	tptr = SHMPTR(next_thread);
	if ( strncmp(pattern, tptr->name, len) == 0 ) {
	    rtapi_print("%11d %s  %s\n",
		tptr->period, (tptr->uses_fp ? "YES" : "NO "), tptr->name);
	    list_root = &(tptr->funct_list);
	    list_entry = list_next(list_root);
	    n = 1;
	    while (list_entry != list_root) {
		/* print the function info */
		fentry = (hal_funct_entry_t *) list_entry;
		funct = SHMPTR(fentry->funct_ptr);
		rtapi_print("                 %2d %s\n", n, funct->name);
		n++;
		list_entry = list_next(list_entry);
	    }
	}
	next_thread = tptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_comp_names(char *pattern)
{
    int next, len;
    hal_comp_t *comp;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( strncmp(pattern, comp->name, len) == 0 ) {
	    rtapi_print("%s ", comp->name);
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_pin_names(char *pattern)
{
    int next, len;
    hal_pin_t *pin;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( strncmp(pattern, pin->name, len) == 0 ) {
	    rtapi_print("%s ", pin->name);
	}
	next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_sig_names(char *pattern)
{
    int next, len;
    hal_sig_t *sig;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if ( strncmp(pattern, sig->name, len) == 0 ) {
	    rtapi_print("%s ", sig->name);
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_param_names(char *pattern)
{
    int next, len;
    hal_param_t *param;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( strncmp(pattern, param->name, len) == 0 ) {
	    rtapi_print("%s ", param->name);
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_funct_names(char *pattern)
{
    int next, len;
    hal_funct_t *fptr;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	fptr = SHMPTR(next);
	if ( strncmp(pattern, fptr->name, len) == 0 ) {
	    rtapi_print("%s ", fptr->name);
	}
	next = fptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_thread_names(char *pattern)
{
    int next_thread, len;
    hal_thread_t *tptr;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
	tptr = SHMPTR(next_thread);
	if ( strncmp(pattern, tptr->name, len) == 0 ) {
	    rtapi_print("%s ", tptr->name);
	}
	next_thread = tptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print("\n");
}

static void print_lock_status()
{
    int lock;

    lock = hal_get_lock();

    rtapi_print("HAL locking status:\n");
    rtapi_print("  current lock value %d (%02x)\n", lock, lock);
    
    if (lock == HAL_LOCK_NONE) 
	rtapi_print("  HAL_LOCK_NONE - nothing is locked\n");
    if (lock & HAL_LOCK_LOAD) 
	rtapi_print("  HAL_LOCK_LOAD    - loading of new components is locked\n");
    if (lock & HAL_LOCK_CONFIG) 
	rtapi_print("  HAL_LOCK_CONFIG  - link and addf is locked\n");
    if (lock & HAL_LOCK_PARAMS) 
	rtapi_print("  HAL_LOCK_PARAMS  - setting params is locked\n");
    if (lock & HAL_LOCK_RUN) 
	rtapi_print("  HAL_LOCK_RUN     - running/stopping HAL is locked\n");
}

/* Switch function for pin/sig/param type for the print_*_list functions */
static char *data_type(int type)
{
    char *type_str;

    switch (type) {
    case HAL_BIT:
	type_str = "bit  ";
	break;
    case HAL_FLOAT:
	type_str = "float";
	break;
    case HAL_S8:
	type_str = "s8   ";
	break;
    case HAL_U8:
	type_str = "u8   ";
	break;
    case HAL_S16:
	type_str = "s16  ";
	break;
    case HAL_U16:
	type_str = "u16  ";
	break;
    case HAL_S32:
	type_str = "s32  ";
	break;
    case HAL_U32:
	type_str = "u32  ";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	type_str = "undef";
    }
    return type_str;
}

/* Switch function for pin direction for the print_*_list functions  */
static char *data_dir(int dir)
{
    char *pin_dir;

    switch (dir) {
    case HAL_RD:
	pin_dir = "R-";
	break;
    case HAL_WR:
	pin_dir = "-W";
	break;
    case HAL_RD_WR:
	pin_dir = "RW";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	pin_dir = "??";
    }
    return pin_dir;
}

/* Switch function for arrow direction for the print_*_list functions  */
static char *data_arrow1(int dir)
{
    char *arrow;

    switch (dir) {
    case HAL_RD:
	arrow = "<==";
	break;
    case HAL_WR:
	arrow = "==>";
	break;
    case HAL_RD_WR:
	arrow = "<=>";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	arrow = "???";
    }
    return arrow;
}

/* Switch function for arrow direction for the print_*_list functions  */
static char *data_arrow2(int dir)
{
    char *arrow;

    switch (dir) {
    case HAL_RD:
	arrow = "==>";
	break;
    case HAL_WR:
	arrow = "<==";
	break;
    case HAL_RD_WR:
	arrow = "<=>";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	arrow = "???";
    }
    return arrow;
}

/* Switch function to return var value for the print_*_list functions  */
/* the value is printed in a fixed width field */
static char *data_value(int type, void *valptr)
{
    char *value_str;
    static char buf[15];

    switch (type) {
    case HAL_BIT:
	if (*((char *) valptr) == 0)
	    value_str = "   FALSE    ";
	else
	    value_str = "   TRUE     ";
	break;
    case HAL_FLOAT:
	snprintf(buf, 14, "%12.5e", *((float *) valptr));
	value_str = buf;
	break;
    case HAL_S8:
	snprintf(buf, 14, "    %4d    ", *((signed char *) valptr));
	value_str = buf;
	break;
    case HAL_U8:
	snprintf(buf, 14, "  %3u  (%02X) ",
	    *((unsigned char *) valptr), *((unsigned char *) valptr));
	value_str = buf;
	break;
    case HAL_S16:
	snprintf(buf, 14, "  %6d    ", *((signed short *) valptr));
	value_str = buf;
	break;
    case HAL_U16:
	snprintf(buf, 14, "%5u (%04X)",
	    *((unsigned short *) valptr), *((unsigned short *) valptr));
	value_str = buf;
	break;
    case HAL_S32:
	snprintf(buf, 14, " %10ld ", *((signed long *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 14, "  %08lX  ", *((unsigned long *) valptr));
	value_str = buf;
	break;
    default:
	/* Shouldn't get here, but just in case... */
	value_str = "   undef    ";
    }
    return value_str;
}

/* Switch function to return var value in string form  */
/* the value is printed as a packed string (no whitespace */
static char *data_value2(int type, void *valptr)
{
    char *value_str;
    static char buf[15];

    switch (type) {
    case HAL_BIT:
	if (*((char *) valptr) == 0)
	    value_str = "FALSE";
	else
	    value_str = "TRUE";
	break;
    case HAL_FLOAT:
	snprintf(buf, 14, "%e", *((float *) valptr));
	value_str = buf;
	break;
    case HAL_S8:
	snprintf(buf, 14, "%d", *((signed char *) valptr));
	value_str = buf;
	break;
    case HAL_U8:
	snprintf(buf, 14, "%u", *((unsigned char *) valptr));
	value_str = buf;
	break;
    case HAL_S16:
	snprintf(buf, 14, "%d", *((signed short *) valptr));
	value_str = buf;
	break;
    case HAL_U16:
	snprintf(buf, 14, "%u", *((unsigned short *) valptr));
	value_str = buf;
	break;
    case HAL_S32:
	snprintf(buf, 14, "%ld", *((signed long *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 14, "%ld", *((unsigned long *) valptr));
	value_str = buf;
	break;
    default:
	/* Shouldn't get here, but just in case... */
	value_str = "unknown_type";
    }
    return value_str;
}

static int do_save_cmd(char *type)
{

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if (*type == '\0') {
	/* save everything */
	save_signals();
	save_links(0);
	save_params();
	save_threads();
    } else if (strcmp(type, "sig") == 0) {
	save_signals();
    } else if (strcmp(type, "link") == 0) {
	save_links(0);
    } else if (strcmp(type, "linka") == 0) {
	save_links(1);
    } else if (strcmp(type, "net") == 0) {
	save_nets(0);
    } else if (strcmp(type, "neta") == 0) {
	save_nets(1);
    } else if (strcmp(type, "param") == 0) {
	save_params();
    } else if (strcmp(type, "thread") == 0) {
	save_threads();
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "Unknown 'save' type '%s'\n", type);
	return -1;
    }
    return 0;
}

static void save_signals(void)
{
    int next;
    hal_sig_t *sig;

    rtapi_print("# signals\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	rtapi_print("newsig %s %s\n", sig->name, data_type((int) sig->type));
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_links(int arrow)
{
    int next;
    hal_pin_t *pin;
    hal_sig_t *sig;
    char *arrow_str;

    rtapi_print("# links\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->signal != 0) {
	    sig = SHMPTR(pin->signal);
	    if (arrow != 0) {
		arrow_str = data_arrow1((int) pin->dir);
	    } else {
		arrow_str = "\0";
	    }
	    rtapi_print("linkps %s %s %s\n", pin->name, arrow_str, sig->name);
	}
	next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_nets(int arrow)
{
    int next;
    hal_pin_t *pin;
    hal_sig_t *sig;
    char *arrow_str;

    rtapi_print("# nets\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	rtapi_print("newsig %s %s\n", sig->name, data_type((int) sig->type));
	pin = halpr_find_pin_by_sig(sig, 0);
	while (pin != 0) {
	    if (arrow != 0) {
		arrow_str = data_arrow2((int) pin->dir);
	    } else {
		arrow_str = "\0";
	    }
	    rtapi_print("linksp %s %s %s\n", sig->name, arrow_str, pin->name);
	    pin = halpr_find_pin_by_sig(sig, pin);
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_params(void)
{
    int next;
    hal_param_t *param;

    rtapi_print("# parameter values\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if (param->dir != HAL_RD) {
	    /* param is writable, save it's value */
	    rtapi_print("setp %s %s\n", param->name,
		data_value((int) param->type, SHMPTR(param->data_ptr)));
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_threads(void)
{
    int next_thread;
    hal_thread_t *tptr;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t *funct;

    rtapi_print("# realtime thread/function links\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
	tptr = SHMPTR(next_thread);
	list_root = &(tptr->funct_list);
	list_entry = list_next(list_root);
	while (list_entry != list_root) {
	    /* print the function info */
	    fentry = (hal_funct_entry_t *) list_entry;
	    funct = SHMPTR(fentry->funct_ptr);
	    rtapi_print("addf %s %s\n", funct->name, tptr->name);
	    list_entry = list_next(list_entry);
	}
	next_thread = tptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static int do_help_cmd(char *command)
{

    if (strcmp(command, "help") == 0) {
	printf("If you need help to use 'help', then I can't help you.\n");
    } else if (strcmp(command, "loadrt") == 0) {
	printf("loadrt modname [modarg(s)]\n");
	printf("  Loads realtime HAL module 'modname', passing 'modargs'\n");
	printf("  to the module.  Must be root to run this command.\n");
    } else if (strcmp(command, "unloadrt") == 0) {
	printf("unloadrt modname\n");
	printf("  Unloads realtime HAL module 'modname'.  If 'modname'\n");
	printf("  is 'all', unloads all realtime modules.  Must be root\n" );
	printf("  to run this command.\n");
    } else if ((strcmp(command, "linksp") == 0) || (strcmp(command,"linkps") == 0)) {
	printf("linkps pinname [arrow] signame\n");
	printf("linksp signame [arrow] pinname\n");
	printf("  Links pin 'pinname' to signal 'signame'.  Both forms do\n");
	printf("  the same thing.  Use whichever makes sense.  The optional\n");
	printf("  'arrow' can be '=>', '<=', or '<=>' and is ignored.  It\n");
	printf("  can be used in files to show the direction of data flow,\n");
	printf("  but don't use arrows on the command line.\n");
    } else if (strcmp(command, "unlinkp") == 0) {
	printf("unlinkp pinname\n");
	printf("  Unlinks pin 'pinname' if it is linked to any signal.\n");
    } else if (strcmp(command, "lock") == 0) {
	printf("lock [all|tune|none]\n");
	printf("  Locks HAL to some degree.\n");
	printf("  none - no locking done.\n");
	printf("  tune - some tuning is possible (setp & such).\n");
	printf("  all  - HAL completely locked.\n");
    } else if (strcmp(command, "unlock") == 0) {
	printf("unlock [all|tune]\n");
	printf("  Unlocks HAL to some degree.\n");
	printf("  tune - some tuning is possible (setp & such).\n");
	printf("  all  - HAL completely unlocked.\n");
    } else if (strcmp(command, "newsig") == 0) {
	printf("newsig signame type\n");
	printf("  Creates a new signal called 'signame'.  Type is 'bit',\n");
	printf("  'float', 'u8', 's8', 'u16', 's16', 'u32', or 's32'.\n");
    } else if (strcmp(command, "delsig") == 0) {
	printf("delsig signame\n");
	printf("  Deletes signal 'signame'.  If 'signame is 'all',\n");
	printf("  deletes all signals\n");
    } else if (strcmp(command, "setp") == 0) {
	printf("setp paramname value\n");
	printf("paramname = value\n");
	printf("  Sets parameter 'paramname' to 'value' (if writable).\n");
	printf("  'setp' and '=' work the same, don't use '=' on the\n");
	printf("  command line.  'value' may be a constant such as 1.234\n");
	printf("  or TRUE, or a reference to an environment variable,\n");
#ifdef NO_INI
	printf("  using the syntax '$name'./n");
#else
	printf("  using the syntax '$name'.  If option -i was given,\n");
	printf("  'value' may also be a reference to an ini file entry\n");
	printf("  using the syntax '[section]name'.\n");
#endif
    } else if (strcmp(command, "sets") == 0) {
	printf("sets signame value\n");
	printf("  Sets signal 'signame' to 'value' (if sig has no writers).\n");
    } else if (strcmp(command, "getp") == 0) {
	printf("getp paramname\n");
	printf("  Gets the value of parameter 'paramname'.\n");
    } else if (strcmp(command, "gets") == 0) {
	printf("gets signame\n");
	printf("  Gets the value of signal 'signame'.\n");
    } else if (strcmp(command, "addf") == 0) {
	printf("addf functname threadname [position]\n");
	printf("  Adds function 'functname' to thread 'threadname'.  If\n");
	printf("  'position' is specified, adds the function to that spot\n");
	printf("  in the thread, otherwise adds it to the end.  Negative\n");
	printf("  'position' means position with respect to the end of the\n");
	printf("  thread.  For example '1' is start of thread, '-1' is the\n");
	printf("  end of the thread, '-3' is third from the end.\n");
    } else if (strcmp(command, "delf") == 0) {
	printf("delf functname threadname\n");
	printf("  Removes function 'functname' from thread 'threadname'.\n");
    } else if (strcmp(command, "show") == 0) {
	printf("show [type] [pattern]\n");
	printf("  Prints info about HAL items of the specified type.\n");
	printf("  'type' is 'comp', 'pin', 'sig', 'param', 'funct',\n");
	printf("  'thread', or 'all'.  If 'type' is omitted, it assumes\n");
	printf("  'all' with no pattern.  If 'pattern' is specified\n");
	printf("  it prints only those items whose names match the\n");
	printf("  pattern (no fancy regular expressions, just a simple\n");
	printf("  match: 'foo' matches 'foo', 'foobar' and 'foot' but\n");
	printf("  not 'fo' or 'frobz' or 'ffoo').\n");
    } else if (strcmp(command, "list") == 0) {
	printf("list type [pattern]\n");
	printf("  Prints the names of HAL items of the specified type.\n");
	printf("  'type' is 'comp', 'pin', 'sig', 'param', 'funct', or\n");
	printf("  'thread'.  If 'pattern' is specified it prints only\n");
	printf("  those names that match the pattern (no fancy regular\n");
	printf("  expressions, just a simple match: 'foo' matches 'foo',\n");
	printf("  'foobar' and 'foot' but not 'fo' or 'frobz' or 'ffoo').\n");
	printf("  Names are printed on a single line, space separated.\n");
    } else if (strcmp(command, "status") == 0) {
	printf("status [type]\n");
	printf("  Prints status info about HAL.\n");
	printf("  'type' is 'lock', or 'all'. \n");
	printf("  If 'type' is omitted, it assumes\n");
	printf("  'all'.\n");
    } else if (strcmp(command, "save") == 0) {
	printf("save [type]\n");
	printf("  Prints HAL items as commands that can be redirected to\n");
	printf("  a file and later restored using \"halcmd -f filename\".\n");
	printf("  Type can be 'sig', 'link[a]', 'net[a]', 'param', or\n");
	printf("  'thread'.  ('linka' and 'neta' show arrows for pin\n");
	printf("  direction.)  If 'type' is omitted, does the equivalent\n");
	printf("  of 'sig', 'link', 'param', and 'thread'.\n");
    } else if (strcmp(command, "start") == 0) {
	printf("start\n");
	printf("  Starts all realtime threads.\n");
    } else if (strcmp(command, "stop") == 0) {
	printf("stop\n");
	printf("  Stops all realtime threads.\n");
    } else if (strcmp(command, "quit") == 0) {
	printf("quit\n");
	printf("  Stop processing input and terminate halcmd (when\n");
	printf("  reading from a file or stdin).\n");
    } else {
	printf("No help for unknown command '%s'\n", command);
    }
    return 0;
}

static void print_help_general(void)
{
    printf("\nUsage:   halcmd [options] [cmd [args]]\n\n");
    printf("options:\n\n");
    printf("  -f [filename]  Read commands from 'filename', not command\n");
    printf("                 line.  If no filename, read from stdin.\n");
#ifndef NO_INI
    printf("  -i filename    Open .ini file 'filename', allow commands\n");
    printf("                 to get their values from ini file.\n");
#endif
    printf("  -k             Keep going after failed command.  Default\n");
    printf("                 is to exit if any command fails.\n");
    printf("  -q             Quiet - print errors only (default).\n");
    printf("  -Q             Very quiet - print nothing.\n");
    printf("  -v             Verbose - print result of every command.\n");
    printf("  -V             Very verbose - print lots of junk.\n");
    printf("  -h             Help - print this help screen and exit.\n\n");
    printf("commands:\n\n");
    printf("  loadrt, unloadrt, lock, unlock, linkps, linksp, unlinkp, newsig,\n");
    printf("  delsig, setp, getp, sets, gets, addf, delf, show, list, save,\n");
    printf("  status, start, stop, quit\n");
    printf("  help           Lists all commands with short descriptions\n");
    printf("  help command   Prints detailed help for 'command'\n\n");
}

static void print_help_commands(void)
{
    printf("Use 'help <command>'  for more details about each command\n");
    printf("Available commands:\n");
    printf("  loadrt     Load realtime module\n");
    printf("  unloadrt   Unload realtime module[s]\n");
    printf("  lock       Lock HAL behaviour\n");
    printf("  unlock     Unlock HAL behaviour\n");
    printf("  linkps     Link pin to signal\n");
    printf("  linksp     Link signal to pin\n");
    printf("  unlinkp    Unlink pin\n");
    printf("  newsig     Create new signal\n");
    printf("  delsig     Delete a signal\n");
    printf("  setp       Set value of a parameter\n");
    printf("  sets       Set value of a signal\n");
    printf("  getp       Get value of a parameter\n");
    printf("  gets       Get value of a signal\n");
    printf("  addf       Add function to thread\n");
    printf("  delf       Remove function from thread\n");
    printf("  show       Display info about HAL objects\n");
    printf("  list       Display names of HAL objects\n");
    printf("  status     Display status information\n");
    printf("  save       Print config as commands\n");
    printf("  start      Start realtime threads\n");
    printf("  stop       Stop realtime threads\n");
    printf("  quit       Stop processing commands, exit from halcmd\n");
}
