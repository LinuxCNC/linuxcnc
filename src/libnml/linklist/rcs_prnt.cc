
extern "C"
{
#include <stdarg.h>		/* va_list, va_start(), va_end() */
#include <stdio.h>		/* printf()'s */
#include <string.h>		/* strchr(), memmove() */
#include <stdlib.h>		/* malloc(), free(), realloc() */
#include <errno.h>		/* errno() */
#include <sys/types.h>
#include <unistd.h>		/* getpid() */
#include <ctype.h>
}
#include "rcs_prnt.hh"
#include "linklist.hh"
#ifndef _TIMER_H
extern "C" double etime(void);	/* This will disappear when rtapi is linked
				   in as part of the RTLMEM buffer support */
#endif

RCS_LINKED_LIST *rcs_print_list = NULL;
char **rcs_lines_table = NULL;
void (*rcs_print_notify) () = NULL;
RCS_PRINT_DESTINATION_TYPE rcs_print_destination = RCS_PRINT_TO_STDOUT;

int max_rcs_errors_to_print = 30;
int rcs_errors_printed = 0;

unsigned long rcs_print_mode_flags = PRINT_RCS_ERRORS;
int rcs_debugging_enabled = 0;

FILE *rcs_print_file_stream = NULL;
char rcs_print_file_name[80] = "rcs_out.txt";

char last_error_bufs[4][100];
int error_bufs_initialized = 0;
int last_error_buf_filled = 0;

void set_rcs_print_destination(RCS_PRINT_DESTINATION_TYPE _dest)
{
    if (rcs_print_destination == RCS_PRINT_TO_NULL) {
	rcs_errors_printed = 0;
    }
    rcs_print_destination = _dest;
}

/** Called by various functions - Replace with rtapi_print_msg functions */
RCS_PRINT_DESTINATION_TYPE get_rcs_print_destination()
{
    return (rcs_print_destination);
}

/** Used by rcs_vprint() */
void bad_char_to_print(char *ptr)
{
    if (ptr) {
	*ptr = '?';
    }
}

/** Used by rcs_print_error() and rcs_print_debug() */
int rcs_vprint(char *_fmt, va_list _args, int save_string)
{
    static char temp_string[4096];
    char *ptr = 0;
    char *endptr = 0;

    if (NULL == _fmt) {
	return (EOF);
    }

    /* Return EOF if we are likely overflow temp_string. */
    if (strlen(_fmt) > sizeof(temp_string) / 2) {
	return (EOF);
    }
    if (EOF == (int) vsprintf(temp_string, _fmt, _args)) {
	return (EOF);
    }

    if (save_string) {
	if (!error_bufs_initialized) {
	    memset(last_error_bufs[0], 0, 100);
	    memset(last_error_bufs[1], 0, 100);
	    memset(last_error_bufs[2], 0, 100);
	    memset(last_error_bufs[3], 0, 100);
	    error_bufs_initialized = 1;
	}
	last_error_buf_filled++;
	last_error_buf_filled %= 4;
	strncpy(last_error_bufs[last_error_buf_filled], temp_string, 99);
    }
    ptr = temp_string;
    endptr = temp_string + sizeof(temp_string);
    while (*ptr && ptr < endptr) {
	if (!isprint(*ptr)) {
	    if (!isspace(*ptr)) {
		bad_char_to_print(ptr);
	    }
	}
	ptr++;
    }
    *ptr = 0;
    return (rcs_fputs(temp_string));
}

/** Used by rcs_print_sys_error(), rcs_sem_init() , rcs_sem_open(), rcs_sem_post(), &  rcs_sem_flush() */
int rcs_puts(char *_str)
{
    int retval, retval2;
    retval = rcs_fputs(_str);
    if (retval != EOF) {
	retval2 = rcs_fputs("\n");
	if (retval2 != EOF) {
	    retval += retval;
	} else {
	    retval = EOF;
	}
    }
    return (retval);
}

/** Used by rcs_vprint(), rcs_puts(), & rcs_print() */
int rcs_fputs(char *_str)
{
    int retval = EOF;
    if (NULL != _str) {
	if (0 == _str[0]) {
	    return (0);
	}
	switch (rcs_print_destination) {

	case RCS_PRINT_TO_LOGGER:

	case RCS_PRINT_TO_STDOUT:

	    retval = fputs(_str, stdout);
	    fflush(stdout);
	    break;

	case RCS_PRINT_TO_STDERR:
	    retval = fputs(_str, stderr);
	    fflush(stderr);
	    break;

	case RCS_PRINT_TO_LIST:
	    if (NULL == rcs_print_list) {
		rcs_print_list = new RCS_LINKED_LIST;
		if (NULL != rcs_print_list) {
		    rcs_print_list->set_list_sizing_mode(256,
			DELETE_FROM_HEAD);
		}
	    }
	    if (NULL != rcs_print_list) {
		if (-1 ==
		    rcs_print_list->store_at_tail(_str,
			(retval = strlen(_str)) + 1, 1)) {
		    retval = EOF;
		}
	    }
	    break;
	case RCS_PRINT_TO_NULL:
	    retval = strlen(_str);
	    break;
	case RCS_PRINT_TO_FILE:
	    if (NULL == rcs_print_file_stream) {
		if (NULL == rcs_print_file_name) {
		    return EOF;
		}
		rcs_print_file_stream = fopen(rcs_print_file_name, "a+");
	    }
	    if (NULL == rcs_print_file_stream) {
		return EOF;
	    }
	    retval = fputs(_str, rcs_print_file_stream);
	    fflush(rcs_print_file_stream);
	    break;

	default:
	    break;
	}
	if (NULL != rcs_print_notify) {
	    (*rcs_print_notify) ();
	}
    }
    return (retval);
}

/** Used by rcs_print_debug(), rcs_print_sys_error(), rcs_print_error(), NML_MODULE::logText(), NML_MODULE::print_statistics(), autokey_getkey(), and assorted EMC functions.
This WILL be repalced with rtapi_print() */
int rcs_print(char *_fmt, ...)
{
    static char temp_buffer[1024];
    int retval;
    va_list args;
    if (strlen(_fmt) > 250) {
	return EOF;
    }
    va_start(args, _fmt);
    retval = vsprintf(temp_buffer, _fmt, args);
    va_end(args);
    if (retval == (EOF)) {
	return EOF;
    }
    temp_buffer[(sizeof(temp_buffer) - 1)] = 0;
    retval = rcs_fputs(temp_buffer);
    return (retval);
}

/** Used by assorted functions - WILL be replaced with rtapi_print_msg() */
int rcs_print_debug(long flag_to_check, char *_fmt, ...)
{
    int retval = 0;
    int pid = 0;
    va_list args;
    va_start(args, _fmt);

    if (flag_to_check & rcs_print_mode_flags) {
	pid = getpid();
	rcs_print("(time=%f,pid=%d): ", etime(), pid);
	retval = rcs_vprint(_fmt, args, 0);
    }
    va_end(args);
    return (retval);
}

/** Only called by some of the XML EMS test routines...
      Replace with rtapi_set_msg_lvl */
void set_rcs_print_flag(unsigned long flag_to_set)
{
    rcs_print_mode_flags |= flag_to_set;
    unsigned long debug_masked_flag_to_set = flag_to_set & ~(1);
    if (0 != debug_masked_flag_to_set) {
	rcs_debugging_enabled = 1;
    }
}

/** Used by CMS::CMS constructor, [cms_cfg.cc] get_buffer_line(), hostname_matches_bufferline(), find_proc_and_buffer_lines(), & cms_create_from_lines() */
int separate_words(char **_dest, int _max, char *_src)
{
    static char word_buffer[256];
    int i;
    if (NULL == _dest || NULL == _src) {
	return -1;
    }
    if (strlen(_src) > 255) {
	return -1;
    }
    strcpy(word_buffer, _src);
    _dest[0] = strtok(word_buffer, " \n\r\t");
    for (i = 0; NULL != _dest[i] && i < _max - 1; i++) {
	_dest[i + 1] = strtok(NULL, " \n\r\t");
    }
    if (_dest[_max - 1] == NULL && i == _max - 1) {
	i--;
    }
    return (i + 1);
}

/** Not used */
int rcs_print_sys_error(int error_source, char *_fmt, ...)
{
    static char temp_string[256];
    static char message_string[512];
    va_list args;
    va_start(args, _fmt);

    if (NULL == _fmt) {
	return (EOF);
    }
    if (strlen(_fmt) > 200) {	/* Might overflow temp_string. */
	return (EOF);
    }
    if (EOF == (int) vsprintf(temp_string, _fmt, args)) {
	return (EOF);
    }
    va_end(args);
    if (max_rcs_errors_to_print == rcs_errors_printed &&
	max_rcs_errors_to_print >= 0) {
	rcs_print("\nMaximum number of errors to print exceeded!\n");
    }
    rcs_errors_printed++;
    if (max_rcs_errors_to_print <= rcs_errors_printed &&
	max_rcs_errors_to_print >= 0) {
	return (EOF);
    }

    switch (error_source) {
    case ERRNO_ERROR_SOURCE:
	sprintf(message_string, "%s %d %s\n", temp_string, errno,
	    strerror(errno));
	rcs_puts(message_string);
	break;

    default:
	rcs_puts(temp_string);
	break;
    }
    return (strlen(temp_string));
}

extern "C"
{
    int rcs_print_error(char *_fmt, ...);
}

/** Used by just about everything !
      Replace with rtapi_print_msg() */
int rcs_print_error(char *_fmt, ...)
{
    int retval = 0;
    va_list args;

    va_start(args, _fmt);
    if ((rcs_print_mode_flags & PRINT_RCS_ERRORS)
	&& ((max_rcs_errors_to_print >= rcs_errors_printed)
	    || max_rcs_errors_to_print < 0)) {
	retval = rcs_vprint(_fmt, args, 1);
	if (max_rcs_errors_to_print == rcs_errors_printed &&
	    max_rcs_errors_to_print >= 0) {
	    rcs_print("\nMaximum number of errors to print exceeded!\n");
	}
    }
    if (rcs_print_destination != RCS_PRINT_TO_NULL) {
	rcs_errors_printed++;
    }
    va_end(args);
    return (retval);
}
