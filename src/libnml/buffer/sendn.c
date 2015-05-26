/********************************************************************
* Description: sendn.c
*   Provides a C file for the sendn function from the book Advanced
*   Programming in the UNIX Environment by Richard Stevens.
*   The sendn function calls the send function repeatedly until n bytes
*   have been written to the file descriptor.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#include <string.h>		/* strerror */
#include <stdlib.h>		/* memset() */
#include <errno.h>		/* errno */
#include "rtapi_math.h"		/* fabs() */
#include <sys/socket.h>		/* send(), recv(), socket(), accept(),
				   bind(), listen() */
#include <sys/time.h>		/* struct timeval */
#include "sendn.h"		/* sendn() */
#include "rcs_print.hh"		/* rcs_print_error() */
#include "_timer.h"		/* etime(), esleep() */

int sendn_timedout = 0;
int print_sendn_timeout_errors = 1;

/* Write "n" bytes to a descriptor. */
int sendn(int fd, const void *vptr, int n, int _flags, double _timeout)
{
    int nleft;
    long nwritten;
    int select_ret;
    double start_time;
    char *ptr;
    struct timeval timeout_tv;
    fd_set send_fd_set;

    timeout_tv.tv_sec = (long) _timeout;
    timeout_tv.tv_usec = (long) (_timeout * 1000000.0);
    if (timeout_tv.tv_usec >= 1000000) {
	timeout_tv.tv_usec = timeout_tv.tv_usec % 1000000;
    }
    FD_ZERO(&send_fd_set);
    FD_SET(fd, &send_fd_set);

    ptr = (char *) vptr;	/* can't do pointer arithmetic on void* */
    nleft = n;
    start_time = etime();
    while (nleft > 0) {
	if (rtapi_fabs(_timeout) > 1E-6) {
	    if (_timeout > 0) {
                double timeleft;
		timeleft = start_time + _timeout - etime();
		if (timeleft <= 0.0) {
		    if (print_sendn_timeout_errors) {
			rcs_print_error
			    ("sendn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f) timed out.\n",
			    fd, vptr, n, _flags, _timeout);
		    }
		    sendn_timedout = 1;
		    return -1;
		}
		timeout_tv.tv_sec = (long) timeleft;
		timeout_tv.tv_usec = (long) (timeleft * 1000000.0);
		if (timeout_tv.tv_usec >= 1000000) {
		    timeout_tv.tv_usec = timeout_tv.tv_usec % 1000000;
		}
		select_ret =
		    select(fd + 1, (fd_set *) NULL, &send_fd_set,
		    (fd_set *) NULL, &timeout_tv);
	    } else {
		select_ret =
		    select(fd + 1, (fd_set *) NULL, &send_fd_set,
		    (fd_set *) NULL, NULL);
	    }
	    switch (select_ret) {
	    case -1:
		rcs_print_error("Error in select: %d -> %s\n", errno,
		    strerror(errno));
		rcs_print_error
		    ("sendn(fd=%d, vptr=%p, int n=%d, int _flags=%d, double _timeout=%f) failed.\n",
		    fd, vptr, n, _flags, _timeout);
		return -1;

	    case 0:
		rcs_print_error
		    ("sendn(fd=%d, vptr=%p, int n=%d, int _flags=%d, double _timeout=%f) timed out.\n",
		    fd, vptr, n, _flags, _timeout);
		return -1;

	    default:
		break;
	    }
	}
	if ((nwritten = send(fd, ptr, nleft, _flags)) == -1) {
	    rcs_print_error("Send error: %d = %s\n", errno, strerror(errno));
	    return (-1);	/* error */
	}
	nleft -= nwritten;
	ptr += nwritten;
	if (nleft > 0 && _timeout > 0.0) {
            double duration;
            duration = etime() - start_time;
            if (duration > _timeout) {
                rcs_print_error("sendn: timed out after %f seconds.\n", duration);
		return (-1);
	    }
	    esleep(0.001);
	}
    }
    rcs_print_debug(PRINT_SOCKET_WRITE_SIZE, "wrote %d bytes to %d\n", n, fd);
    return (n);
}
