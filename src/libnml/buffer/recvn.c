/********************************************************************
* Description: recvn.c
*   Provides a C file for the recvn function from the book Advanced
*   Programming in the UNIX Environment by Richard Stevens.
*   The recvn function is called repeatedly until n bytes have been
*   received from the file descriptor. It uses select and FIONREAD
*   checks ahead of time to guarantee that even if the socket is
*   blocking the timeout will be enforced. To retry a socket to for
*   the data missed during past timeouts the application should pass
*   recvn the same buffer and address of a variable storing the number
*   of bytes read on previous attempts.
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

#include "recvn.h"		/* recvn(int, void *, int, double) */
#include <stddef.h>		/* size_t */
#include <errno.h>		/* errno */
#include <sys/types.h>		/* typedef fd_set, FD_ZERO, FD_SET */
#include <sys/ioctl.h>		/* FIONREAD */
#include <sys/socket.h>		/* recv() */
#include <sys/time.h>		/* struct timeval */
#include <stdlib.h>		/* malloc(), free() */
#include <string.h>		/* strerror() */
#include "rtapi_math.h"		/* modf() */
#include "rcs_print.hh"		/* rcs_print_error() */
#include "_timer.h"		/* etime(), esleep() */

int recvn_timedout = 0;
int print_recvn_timeout_errors = 1;

/* Read "n" bytes from a descriptor. */
int recvn(int fd, void *vptr, int n, int flags, double _timeout,
    int *bytes_read_ptr)
{
    int nleft, nrecv;
    char *ptr;
    double start_time;
    struct timeval timeout_tv;
    fd_set recv_fd_set;
    int bytes_ready;
    int bytes_to_read;
    if (etime_disabled) {
	_timeout = -1.0;
    }

    bytes_ready = 0;
    timeout_tv.tv_sec = (long) _timeout;
    timeout_tv.tv_usec = (long) (_timeout * 1000000.0);
    if (timeout_tv.tv_usec >= 1000000) {
	timeout_tv.tv_usec = timeout_tv.tv_usec % 1000000;
    }
    FD_ZERO(&recv_fd_set);
    FD_SET(fd, &recv_fd_set);

    recvn_timedout = 0;
    ptr = (char *) vptr;
    nleft = n;
    if (NULL != bytes_read_ptr) {
	if (*bytes_read_ptr >= n) {
	    rcs_print_error
		("recvn: Invalid parameter -- (*bytes_read_ptr = %d) must be less than (n = %d).\n",
		*bytes_read_ptr, n);
	    return -1;
	}
	if (*bytes_read_ptr < 0) {
	    rcs_print_error
		("recvn: Invalid parameter -- (*bytes_read_ptr = %d) must be greater than or equal to zero.\n",
                *bytes_read_ptr);
	    return -1;
	}
	ptr += *bytes_read_ptr;
	nleft -= *bytes_read_ptr;
    }

    start_time = etime();
    while (nleft > 0) {
	if (_timeout > 0.0) {
            double timeleft;
	    timeleft = start_time + _timeout - etime();
	    if (timeleft <= 0.0) {
		if (print_recvn_timeout_errors) {
		    rcs_print_error("Recv timed out.\n");
		    if (NULL == bytes_read_ptr) {
			rcs_print_error
			    ("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f) failed.\n",
			    fd, vptr, n, flags, _timeout);
		    } else {
			rcs_print_error
			    ("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f,bytes_read=%d) failed.\n",
			    fd, vptr, n, flags, _timeout, *bytes_read_ptr);
		    }
		}
		recvn_timedout = 1;
		if (NULL != bytes_read_ptr) {
		    *bytes_read_ptr = (n - nleft);
		}
		return -1;
	    }
	    timeout_tv.tv_sec = (long) timeleft;
	    timeout_tv.tv_usec = (long) (timeleft * 1000000.0);
	    if (timeout_tv.tv_usec >= 1000000) {
		timeout_tv.tv_usec = timeout_tv.tv_usec % 1000000;
	    }
	    switch (select(fd + 1, &recv_fd_set, (fd_set *) NULL,
		    (fd_set *) NULL, &timeout_tv)) {
	    case -1:
		rcs_print_error("Error in select: %d -> %s\n", errno,
		    strerror(errno));
		if (NULL == bytes_read_ptr) {
		    rcs_print_error
			("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f) failed.\n",
			fd, vptr, n, flags, _timeout);
		} else {
		    rcs_print_error
			("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f,bytes_read=%d) failed.\n",
			fd, vptr, n, flags, _timeout, *bytes_read_ptr);
		}
		return -1;

	    case 0:
		if (print_recvn_timeout_errors) {
		    rcs_print_error("Recv timed out.\n");
		    if (NULL == bytes_read_ptr) {
			rcs_print_error
			    ("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f) failed.\n",
			    fd, vptr, n, flags, _timeout);
		    } else {
			rcs_print_error
			    ("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f,bytes_read=%d) failed.\n",
			    fd, vptr, n, flags, _timeout, *bytes_read_ptr);
		    }
		}
		recvn_timedout = 1;
		if (NULL != bytes_read_ptr) {
		    *bytes_read_ptr = (n - nleft);
		}
		return -1;

	    default:
		break;
	    }
	    bytes_ready = 0;

	    ioctl(fd, FIONREAD, (caddr_t) & bytes_ready);

	    bytes_to_read = (nleft <= bytes_ready) ? nleft : bytes_ready;
	} else {
	    bytes_to_read = nleft;
	}
	nrecv = 0;
	if (bytes_to_read > 0) {
	    if ((nrecv = recv(fd, ptr, bytes_to_read, flags)) == -1) {
		if (errno == EWOULDBLOCK) {
		    if (rtapi_fabs(_timeout) < 1e-6) {
			recvn_timedout = 1;
			if (NULL != bytes_read_ptr) {
			    *bytes_read_ptr = (n - nleft);
			}
			return -1;
		    }
		} else {
		    rcs_print_error("Recv error: %d = %s\n", errno,
			strerror(errno));
		    if (NULL == bytes_read_ptr) {
			rcs_print_error
			    ("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f) failed.\n",
			    fd, vptr, n, flags, _timeout);
		    } else {
			rcs_print_error
			    ("recvn(fd=%d, vptr=%p, int n=%d, int flags=%d, double _timeout=%f,bytes_read=%d) failed.\n",
			    fd, vptr, n, flags, _timeout, *bytes_read_ptr);
		    }
		    if (NULL != bytes_read_ptr) {
			*bytes_read_ptr = (n - nleft);
		    }
		    return (-1);	/* error, return < 0 */
		}
		nrecv = 0;
	    } else if (nrecv == 0) {
		rcs_print_error("recvn: Premature EOF recieved.\n");
		return (-2);
	    }
	}
	nleft -= nrecv;
	ptr += nrecv;
	if (nleft > 0 && _timeout > 0.0) {
	    esleep(0.001);
	    if (etime() - start_time > _timeout) {
		rcs_print_error("Recv timed out.\n");
		recvn_timedout = 1;
		if (NULL != bytes_read_ptr) {
		    *bytes_read_ptr = (n - nleft);
		}
		return (-1);
	    }
	}
    }
    rcs_print_debug(PRINT_SOCKET_READ_SIZE, "read %d bytes from %d\n", n, fd);
    if (NULL != bytes_read_ptr) {
	*bytes_read_ptr = (n - nleft);
    }
    return (n - nleft);		/* return >= 0 */
}
