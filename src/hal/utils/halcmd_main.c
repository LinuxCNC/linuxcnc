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

#include "rtapi.h"
#include "hal.h"
#include "../hal_priv.h"
#include "halcmd.h"
#include "halcmd_commands.h"

#ifndef NO_INI
#include "inifile.hh"		/* iniFind() from libnml */
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

static int preprocess_line ( char *line, char **tokens);
static int strip_comments ( char *buf );
static int strlimcpy(char **dest, char *src, int srclen, int *destspace);
static int replace_vars(char *source_str, char *dest_str, int max_chars);
static int tokenize(char *src, char **tokens);
static int get_input(FILE *srcfile, char *buf, size_t bufsize);
static void print_help_general(int showR);
static int release_HAL_mutex(void);

#ifndef NO_INI
FILE *inifile = NULL;
#endif

char *replace_errors[] = {
	"Missing close parenthesis",
	"Zero length variable name",
	"Missing close square bracket",
	"Environment variable not found",
	"Ini variable not found",
	"Replacement would overflow output buffer",
	"Variable name too long",
};

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
    char *tokens[MAX_TOK+1];

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
		if (inifile == NULL) {
		    /* it's the first -i (ignore repeats) */
                    /* there is a following arg, and it's not an option */
                    filename = optarg;
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

    halcmd_startup();

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
	/* remove comments, do var substitution, and tokenise */
	retval = preprocess_line(raw_buf, tokens);
	/* process the command */
	if (retval == 0) {
	    retval = parse_cmd(tokens);
	    if (retval != 0) {
		errorcount++;
	    }
	}
    } else {
	/* read command line(s) from 'srcfile' */
	while (get_input(srcfile, raw_buf, MAX_CMD_LEN)) {
	    linenumber++;
	    /* remove comments, do var substitution, and tokenise */
	    retval = preprocess_line(raw_buf, tokens);
	    if (retval == 0) {
		/* the "quit" command is not handled by parse_cmd() */
		if ( ( strcasecmp(tokens[0],"quit") == 0 ) ||
		     ( strcasecmp(tokens[0],"exit") == 0 ) ) {
		    break;
		}
		/* process command */
		retval = parse_cmd(tokens);
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


static int preprocess_line ( char *line, char **tokens )
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
		    replacement = (char *) iniFind(inifile, var, sec);
		} else {
		/* no section specified */
		    replacement = (char *) iniFind(inifile, var, NULL);
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


