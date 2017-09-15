//    Copyright 2003 John Kasunich
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
/*
  fifousr.c - user-side FIFO code

*/

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/time.h>
#include "rtapi.h"
#include "common.h"		/* FIFO_KEY, FIFO_SIZE */

static int module;
static jmp_buf env;
static void quit(int sig)
{
    longjmp(env, 1);
}

int main()
{
    int fifo;
    char buffer[FIFO_SIZE + 1];
    int nchars;
    int retval;

    module = rtapi_init("FIFO_USR");
    if (module < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "fifousr main: rtapi_init returned %d\n", module);
	return -1;
    }

    /* open the fifo */
    fifo = rtapi_fifo_new(FIFO_KEY, module, FIFO_SIZE, 'R');
    if (fifo < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "fifousr main: rtapi_fifo_new returned %d\n", fifo);
	rtapi_exit(module);
	return -1;
    }

    if (1 == setjmp(env))
	goto END;
    signal(SIGINT, quit);

    printf("waiting for fifo data, ctrl-C to quit\n");

    while (1) {
	nchars = rtapi_fifo_read(fifo, buffer, FIFO_SIZE);
	if (nchars <= 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"fifousr main: rtapi_fifo_read returned %d\n", nchars);
	} else {
	    buffer[nchars] = '\0';
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"fifousr main: read %d chars: '%s'\n", nchars, buffer);
	}
    }

  END:

    rtapi_print_msg(RTAPI_MSG_INFO, "shutting down\n");

    retval = rtapi_fifo_delete(fifo, module);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "fifousr main: rtapi_fifo_delete returned %d\n", retval);
	rtapi_exit(module);
	return -1;
    }

    return rtapi_exit(module);
}
