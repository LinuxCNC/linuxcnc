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
                       Benn Lipkowitz
                       <fenn AT users DOT sourceforge DOT net>
                       Stephen Wille Padnos
                       <swpadnos AT users DOT sourceforge DOT net>
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

#ifndef ULAPI
#error This is a user mode component only!
#endif

#include "config.h"

#ifndef NO_INI
#include "inifile.hh"		/* iniFind() from libnml */
FILE *halcmd_inifile = NULL;
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
#include <errno.h>
#include <time.h>
#include <fnmatch.h>
#include <search.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "../hal_priv.h"	/* private HAL decls */
#include "halcmd_commands.h"

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/* These functions are used internally by this file.  The code is at
   the end of the file.  */

/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

int comp_id = -1;	/* -1 means hal_init() not called yet */
int hal_flag = 0;	/* used to indicate that halcmd might have the
			   hal mutex, so the sig handler can't just
			   exit, instead it must set 'done' */
int halcmd_done = 0;		/* used to break out of processing loop */
int linenumber=0;	/* used to print linenumber on errors */
int scriptmode = 0;	/* used to make output "script friendly" (suppress headers) */
int prompt_mode = 0;	/* when getting input from stdin, print a prompt */
char comp_name[HAL_NAME_LEN];	/* name for this instance of halcmd */

static void quit(int);

int halcmd_startup(void) {
    /* register signal handlers - if the process is killed
       we need to call hal_exit() to free the shared memory */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGPIPE, SIG_IGN);
    /* at this point all options are parsed, connect to HAL */
    /* create a unique module name, to allow for multiple halcmd's */
    snprintf(comp_name, HAL_NAME_LEN-1, "halcmd%d", getpid());
    /* tell the signal handler that we might have the mutex */
    hal_flag = 1;
    /* connect to the HAL */
    comp_id = hal_init(comp_name);
    /* done with mutex */
    hal_flag = 0;
    /* check result */
    if (comp_id < 0) {
	fprintf(stderr, "halcmd: hal_init() failed: %d\n", comp_id );
	fprintf(stderr, "NOTE: 'rtapi' kernel module must be loaded\n" );
	return HAL_FAIL;
    }
    hal_ready(comp_id);
    return HAL_SUCCESS;
}

void halcmd_shutdown(void) {
    /* tell the signal handler we might have the mutex */
    hal_flag = 1;
    hal_exit(comp_id);
}

/* parse_cmd() decides which command has been invoked, gathers any
   arguments that are needed, and calls a function to execute the
   command.
*/

enum argtype {
    A_ZERO,  /* prototype: f(void) */
    A_ONE,   /* prototype: f(char *arg) */
    A_TWO,   /* prototype: f(char *arg1, char *arg2) */

    A_PLUS = 0x100,          /* adds to prototype: char *args[] */
    A_REMOVE_ARROWS = 0x200, /* removes any arrows from command */
    A_OPTIONAL = 0x400       /* arguments may be NULL */
};

typedef int(*func_t)(void);

#define FUNCT(x) ((func_t)x)

struct command {
    char *name;
    func_t func;
    enum argtype type;
};

struct command commands[] = {
    {"addf",    FUNCT(do_addf_cmd),    A_TWO | A_PLUS },
    {"delf",    FUNCT(do_delf_cmd),    A_TWO | A_OPTIONAL },
    {"delsig",  FUNCT(do_delsig_cmd),  A_ONE },
    {"getp",    FUNCT(do_getp_cmd),    A_ONE },
    {"gets",    FUNCT(do_gets_cmd),    A_ONE },
    {"help",    FUNCT(do_help_cmd),    A_ONE | A_OPTIONAL },
    {"linkpp",  FUNCT(do_linkpp_cmd),  A_TWO | A_REMOVE_ARROWS },
    {"linkps",  FUNCT(do_linkps_cmd),  A_TWO | A_REMOVE_ARROWS },
    {"linksp",  FUNCT(do_linksp_cmd),  A_TWO | A_REMOVE_ARROWS },
    {"list",    FUNCT(do_list_cmd),    A_ONE | A_PLUS },
    {"loadrt",  FUNCT(do_loadrt_cmd),  A_ONE | A_PLUS },
    {"loadusr", FUNCT(do_loadusr_cmd), A_PLUS },
    {"lock",    FUNCT(do_lock_cmd),    A_ONE | A_OPTIONAL },
    {"net",     FUNCT(do_net_cmd),     A_ONE | A_PLUS | A_REMOVE_ARROWS },
    {"newsig",  FUNCT(do_newsig_cmd),  A_TWO },
    {"save",    FUNCT(do_save_cmd),    A_TWO | A_OPTIONAL },
    {"setexact_for_test_suite_only", FUNCT(do_setexact_cmd), A_ZERO },
    {"setp",    FUNCT(do_setp_cmd),    A_TWO },
    {"sets",    FUNCT(do_sets_cmd),    A_TWO },
    {"show",    FUNCT(do_show_cmd),    A_ONE | A_OPTIONAL | A_PLUS},
    {"source",  FUNCT(do_source_cmd),  A_ONE},
    {"start",   FUNCT(do_start_cmd),   A_ZERO},
    {"status",  FUNCT(do_status_cmd),  A_ONE},
    {"stop",    FUNCT(do_stop_cmd),    A_ZERO},
    {"unlinkp", FUNCT(do_unlinkp_cmd), A_ONE },
    {"unload",  FUNCT(do_unload_cmd),  A_ONE },
    {"unloadrt", FUNCT(do_unloadrt_cmd), A_ONE },
    {"unloadusr", FUNCT(do_unloadusr_cmd), A_ONE },
    {"unlock",  FUNCT(do_unlock_cmd),  A_ONE | A_OPTIONAL },
    {"waitusr", FUNCT(do_waitusr_cmd), A_ONE },
};
#define ncommands (sizeof(commands) / sizeof(commands[0]))

static int sort_command(const void *a, const void *b) {
    const struct command *ca = a, *cb = b;
    return strcmp(ca->name, cb->name);
}

static int compare_command(const void *namep, const void *commandp) {
    const char *name = namep;
    const struct command *command = commandp;
    return strcmp(name, command->name);
}


pid_t hal_systemv_nowait(char *const argv[]) {
    pid_t pid;
    int n;

    /* now we need to fork, and then exec .... */
    /* disconnect from the HAL shmem area before forking */
    hal_exit(comp_id);
    comp_id = 0;
    /* now the fork() */
    pid = fork();
    if ( pid < 0 ) {
	/* fork failed */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: fork() failed\n", linenumber);
	/* reconnect to the HAL shmem area */
	comp_id = hal_init(comp_name);
	if (comp_id < 0) {
	    fprintf(stderr, "halcmd: hal_init() failed after fork: %d\n",
                    comp_id );
	    exit(-1);
	}
        hal_ready(comp_id);
	return -1;
    }
    if ( pid == 0 ) {
	/* child process */
	/* print debugging info if "very verbose" (-V) */
        for(n=0; argv[n] != NULL; n++) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s ", argv[n] );
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "\n" );
        /* call execv() to invoke command */
	execvp(argv[0], argv);
	/* should never get here */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: execv(%s) failed\n", linenumber, argv[0] );
	exit(1);
    }
    /* parent process */
    /* reconnect to the HAL shmem area */
    comp_id = hal_init(comp_name);

    return pid;
}

int hal_systemv(char *const argv[]) {
    pid_t pid;
    int status;
    int retval;

    /* do the fork */
    pid = hal_systemv_nowait(argv);
    /* this is the parent process, wait for child to end */
    retval = waitpid ( pid, &status, 0 );
    if (comp_id < 0) {
	fprintf(stderr, "halcmd: hal_init() failed after systemv: %d\n", comp_id );
	exit(-1);
    }
    hal_ready(comp_id);
    /* check result of waitpid() */
    if ( retval < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: waitpid(%d) failed\n", linenumber, pid );
	return -1;
    }
    if ( WIFEXITED(status) == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: child did not exit normally\n", linenumber);
	return -1;
    }
    retval = WEXITSTATUS(status);
    if ( retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: systemv failed, returned %d\n", linenumber, retval );
	return -1;
    }
    return 0;
}


/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/

/* signal handler */
static void quit(int sig)
{
    if ( hal_flag ) {
	/* this process might have the hal mutex, so just set the
	   'done' flag and return, exit after mutex work finishes */
	halcmd_done = 1;
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

static int count_args(char **argv) {
    int i = 0;
    while(argv[i] && argv[i][0]) i++;
    return i;
}

#define ARG(i) (argc > i ? argv[i] : 0)
#define REST(i) (argc > i ? argv + i : argv + argc)

static int parse_cmd1(char **argv) {
    struct command *command = bsearch(argv[0], commands, ncommands,
		sizeof(struct command), compare_command);
    int argc = count_args(argv);

    if(argc == 0)
        return HAL_SUCCESS;

    if(!command) {
	// special case: sig = newvalue
	if(argc == 3 && strcmp(argv[1], "=")) {
	    return do_setp_cmd(argv[0], argv[2]);
	} else {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "HAL:%d: Unknown command '%s'\n", linenumber, argv[0]);
            return HAL_INVAL;
        }
    } else {
	int is_optional = command->type & A_OPTIONAL,
	    is_plus = command->type & A_PLUS,
	    nargs = command->type & 0xff,
	    posargs;

	if(command->type & A_REMOVE_ARROWS) {
	    int s, d;
	    for(s=d=0; argv[s] && argv[s][0]; s++) {
		if(argv[s][0] == '<' || argv[s][0] == '=') {
		    continue;
		} else {
		    argv[d++] = argv[s];
		}
	    }
	    argv[d] = 0;
	    argc = d;
	}

        posargs = argc - 1;
	if(posargs < nargs && !is_optional) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL:%d: %s requires %s%d arguments, %d given\n", linenumber,
		command->name, is_plus ? "at least " : "", nargs, posargs);
	    return HAL_INVAL;
	}

	switch(nargs | is_plus) {
	case A_ZERO: {
	    return command->func();
	    break;
	}

	case A_PLUS: {
	    int(*f)(char **args) = (int(*)(char**))command->func;
	    return f(REST(1));
	    break;
	}

	case A_ONE: {
	    int(*f)(char *arg) = (int(*)(char*))command->func;
	    return f(ARG(1));
	    break;
	}

	case A_ONE | A_PLUS: {
	    int(*f)(char *arg, char **rest) =
                (int(*)(char*,char**))command->func;
	    return f(ARG(1), REST(2));
	    break;
	}

	case A_TWO: {
	    int(*f)(char *arg, char *arg2) =
                (int(*)(char*,char*))command->func;
	    return f(ARG(1), ARG(2));
	    break;
	}

	case A_TWO | A_PLUS: {
	    int(*f)(char *arg, char *arg2, char **rest) =
                (int(*)(char*,char*,char**))command->func;
	    return f(ARG(1), ARG(2), REST(3));
	    break;
	}

	default:
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL:%d: BUG: unchandled case: command=%s type=0x%x",
		linenumber, command->name, command->type);
	    return HAL_INVAL;
	}
    }
}

int halcmd_parse_cmd(char *tokens[])
{
    int retval;
    static int first_time = 1;

    if(first_time) {
        /* ensure that commands is sorted when it is searched later */
        qsort(commands, ncommands, sizeof(struct command), sort_command);
        first_time = 0;
    }

    hal_flag = 1;
    retval = parse_cmd1(tokens);
    hal_flag = 0;
    return retval;
}

/* tokenize() sets an array of pointers to each non-whitespace
   token in the input line.  It expects that variable substitution
   and comment removal have already been done, and that any
   trailing newline has been removed.
*/
static int tokenize(char *cmd_buf, char **tokens)
{
    enum { BETWEEN_TOKENS,
           IN_TOKEN,
	   SINGLE_QUOTE,
	   DOUBLE_QUOTE,
	   END_OF_LINE } state;
    char *cp1;
    int m;
    /* convert a line of text into individual tokens */
    m = 0;
    cp1 = cmd_buf;
    state = BETWEEN_TOKENS;
    while ( m < MAX_TOK ) {
	switch ( state ) {
	case BETWEEN_TOKENS:
	    if ( *cp1 == '\0' ) {
		/* end of the line */
		state = END_OF_LINE;
	    } else if ( isspace(*cp1) ) {
		/* whitespace, skip it */
		cp1++;
	    } else if ( *cp1 == '\'' ) {
		/* start of a quoted string and a new token */
		tokens[m] = cp1++;
		state = SINGLE_QUOTE;
	    } else if ( *cp1 == '\"' ) {
		/* start of a quoted string and a new token */
		tokens[m] = cp1++;
		state = DOUBLE_QUOTE;
	    } else {
		/* first char of a token */
		tokens[m] = cp1++;
		state = IN_TOKEN;
	    }
	    break;
	case IN_TOKEN:
	    if ( *cp1 == '\0' ) {
		/* end of the line */
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
	    if ( *cp1 == '\0' ) {
		/* end of the line */
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
	    if ( *cp1 == '\0' ) {
		/* end of the line */
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
	    tokens[m++] = cp1;
	    break;
	default:
	    /* should never get here */
	    state = BETWEEN_TOKENS;
	}
    }
    if ( state != END_OF_LINE ) {
	return -1;
    }
    return 0;
}

/* strip_comments() removes any comment in the string.  It also
   removes any trailing newline.  Single- or double-quoted strings
   are respected - a '#' inside a quoted string does not indicate
   a comment.
*/
static int strip_comments ( char *buf )
{
    enum { NORMAL,
	   SINGLE_QUOTE,
	   DOUBLE_QUOTE
	 } state;
    char *cp1;

    cp1 = buf;
    state = NORMAL;
    while ( 1 ) {
	switch ( state ) {
	case NORMAL:
	    if (( *cp1 == '#' ) || ( *cp1 == '\n' ) || ( *cp1 == '\0' )) {
		/* end of the line */
		*cp1 = '\0';
		return 0;
	    } else if ( *cp1 == '\'' ) {
		/* start of a quoted string */
		cp1++;
		state = SINGLE_QUOTE;
	    } else if ( *cp1 == '\"' ) {
		/* start of a quoted string */
		cp1++;
		state = DOUBLE_QUOTE;
	    } else {
		/* normal character */
		cp1++;
	    }
	    break;
	case SINGLE_QUOTE:
	    if (( *cp1 == '\n' ) || ( *cp1 == '\0' )) {
		/* end of the line, unterminated quoted string */
		*cp1 = '\0';
		return -1;
	    } else if ( *cp1 == '\'' ) {
		/* end of quoted string */
		cp1++;
		state = NORMAL;
	    } else {
		/* ordinary character */
		cp1++;
	    }
	    break;
	case DOUBLE_QUOTE:
	    if (( *cp1 == '\n' ) || ( *cp1 == '\0' )) {
		/* end of the line, unterminated quoted string */
		*cp1 = '\0';
		return -1;
	    } else if ( *cp1 == '\"' ) {
		/* end of quoted string */
		cp1++;
		state = NORMAL;
	    } else {
		/* ordinary character */
		cp1++;
	    }
	    break;
	default:
	    /* should never get here */
	    state = NORMAL;
	}
    }
}

/* strlimcpy:
   a wrapper for strncpy which has two improvements:
   1) takes two variables for limits, and uses the lower of the two for the limit
   2) subtracts the number of characters copied from the second limit

   This allows one to keep track of remaining buffer size and use the correct limits
   simply

   Parameters are:
   pointer to destination string pointer
   source string pointer
   source length to copy
   pointer to remaining destination space

   return value:
   -1 if the copy was limited by destination space remaining
   0 otherwise
   Side Effects:
	the value of *destspace will be reduced by the copied length
	The value of *dest will will be advanced by the copied length

*/
static int strlimcpy(char **dest, char *src, int srclen, int *destspace) {
    if (*destspace < srclen) {
	return -1;
    } else {
	strncpy(*dest, src, srclen);
	(*dest)[srclen] = '\0';
	srclen = strlen(*dest);		/* use the actual number of bytes copied */
	*destspace -= srclen;
	*dest += srclen;
    }
    return 0;
}

/* replace_vars:
   replaces environment and ini var references in source_str.
   This routine does string replacement only
   return value is 0 on success (ie, no variable lookups failed)
   The source string is not modified

   In case of an error, dest_str will contain everything that was
   successfully replaced, but will not contain anything past the
   var that caused the error.

   environment vars are in the following formats:
   $envvar<whitespace>
   $(envvar)<any char>

   ini vars are in the following formats:
   [SECTION]VAR<whitespace>
   [SECTION](VAR)<any char>
   
   return values:
   0	success
   -1	missing close parenthesis
   -2	null variable name (either environment or ini varname)
   -3	missing close square bracket
   -4	environment variable not found
   -5	ini variable not found
   -6	replacement would overflow output buffer
   -7	var name exceeds limit
*/
static int replace_vars(char *source_str, char *dest_str, int max_chars)
{
    int retval = 0, loopcount=0;
    int next_delim, remaining, buf_space;
    char *replacement, sec[128], var[128];
    char *sp=source_str, *dp=dest_str, *secP, *varP;
    const char 
	* words = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_";

    dest_str[max_chars-1] = '\0';	/* make sure there's a terminator */
    *dest_str='\0';			/* return null string if input is null string */
    buf_space = max_chars-1;		/* leave space for terminating null */
    while ((remaining = strlen(sp)) > 0) {
	loopcount++;
	next_delim=strcspn(sp, "$[");
	if (strlimcpy(&dp, sp, next_delim, &buf_space) < 0)
	    return -6;
	sp += next_delim;
	if (next_delim < remaining) {		/* a new delimiter was found */
	    switch (*sp++) {	/* this is the found delimiter, since sp was incremented */
	    case '$':
		varP = sp;
		if (*sp=='(') {		/* look for a parenthesized var */
		    varP=++sp;
		    next_delim=strcspn(varP, ")");
		    if (next_delim >= strlen(varP))	/* error - no matching parens */
			return -1;
		    sp++;
		} else next_delim = strspn(varP, words);
		if (next_delim == 0)		/* null var name */
		    return -2;
		if (next_delim > 127)		/* var name too long */
		    return -7;
		strncpy(var, varP, next_delim);
		var[next_delim]='\0';
		replacement = getenv(var);
		if (replacement == NULL)
		    return -4;
		if (strlimcpy(&dp, replacement, strlen(replacement), &buf_space) <0)
		    return -6;
		sp += next_delim;
		break;
	    case '[':
		secP = sp;
		next_delim = strcspn(secP, "]");
		if (next_delim >= strlen(secP))	/* error - no matching square bracket */
		    return -3;
		if (next_delim > 127)		/* section name too long */
		    return -7;
		strncpy(sec, secP, next_delim);
		sec[next_delim]='\0';
		sp += next_delim+1;
		varP = sp;		/* should point past the ']' now */
		if (*sp=='(') {		/* look for a parenthesized var */
		    varP=++sp;
		    next_delim=strcspn(varP, ")");
		    if (next_delim > strlen(varP))	/* error - no matching parens */
			return -1;
		    sp++;
		} else next_delim = strspn(varP, words);
		if (next_delim == 0)
		    return -2;
		if (next_delim > 127)		/* var name too long */
		    return -7;
		strncpy(var, varP, next_delim);
		var[next_delim]='\0';
		if ( strlen(sec) > 0 ) {
		/* get value from ini file */
		/* cast to char ptr, we are discarding the 'const' */
		    replacement = (char *) iniFind(halcmd_inifile, var, sec);
		} else {
		/* no section specified */
		    replacement = (char *) iniFind(halcmd_inifile, var, NULL);
		}
		if (replacement==NULL)
		    return -5;
		if (strlimcpy(&dp, replacement, strlen(replacement), &buf_space) < 0)
		    return -6;
		sp += next_delim;
		break;
	    }
	}
    }
    return retval;
}


static char *replace_errors[] = {
	"Missing close parenthesis",
	"Zero length variable name",
	"Missing close square bracket",
	"Environment variable not found",
	"Ini variable not found",
	"Replacement would overflow output buffer",
	"Variable name too long",
};


int halcmd_preprocess_line ( char *line, char **tokens )
{
    int retval;
    static char cmd_buf[2*MAX_CMD_LEN];

    /* strip comments and trailing newline (if any) */
    retval = strip_comments(line);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: unterminated quoted string\n", linenumber);
	return -1;
    }
    /* copy to cmd_buf while doing variable replacements */
    retval = replace_vars(line, cmd_buf, sizeof(cmd_buf)-2);
    if (retval != 0) {
	if ((retval < 0) && (retval >= -7)) {  /* print better replacement errors */
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL:%d: %s.\n", linenumber, replace_errors[(-retval) -1]);
	} else {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL:%d: unknown variable replacement error\n", linenumber);
	}
	return -2;
    }
    /* split cmd_buff into tokens */
    retval = tokenize(cmd_buf, tokens);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: too many tokens on line\n", linenumber);
	return -3;
    }
    /* tokens[] contains MAX_TOK+1 elements so there is always
       at least one empty one at the end... make it empty now */
    tokens[MAX_TOK] = "";
    return 0;
}

int halcmd_parse_line(char *line) {
    char *tokens[MAX_TOK+1];
    int result = halcmd_preprocess_line(line, tokens);
    if(result < 0) return result;
    return halcmd_parse_cmd(tokens);
}


/* vim:sts=4:sw=4:et
 */
