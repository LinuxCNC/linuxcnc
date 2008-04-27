/********************************************************************
* Description: tcp_opts.cc
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

#include "tcp_opts.hh"
#include "rcs_print.hh"		/* rcs_print_error() */

#include <errno.h>		// errno
#include <string.h>		// strerror
#include <netinet/tcp.h>	// TCP_NODELAY
#include <netinet/in.h>		// TCP_NODELAY
#include <sys/fcntl.h>		// fcntl, O_NONBLOCK

#include <sys/types.h>
#include <sys/socket.h>

int set_tcp_socket_options(int socket_fd)
{
    if (socket_fd <= 0) {
	return -1;
    }
    int optval = 1;
#ifdef TCP_NODELAY
    if (setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
	    (char *) &optval, sizeof(optval)) < 0) {
	rcs_print_error(" Can`t set a socket option.\n");
	rcs_print_error("errno = %d = %s\n", errno, strerror(errno));
	return -1;
    }
#endif

    optval = 1;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
	    (char *) &optval, sizeof(optval)) < 0) {
	rcs_print_error(" Can`t set a socket option.\n");
	rcs_print_error("errno = %d = %s\n", errno, strerror(errno));
	return -1;
    }
    struct linger linger_opt;
    linger_opt.l_onoff = 0;
    linger_opt.l_linger = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_LINGER,
	    (char *) &linger_opt, sizeof(linger_opt)) < 0) {
	rcs_print_error(" Can`t set a socket option.\n");
	rcs_print_error("errno = %d = %s\n", errno, strerror(errno));
	return -1;
    }
    return 0;
}

int make_tcp_socket_nonblocking(int socket_fd)
{
#ifdef O_NONBLOCK
    if (-1 == fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
	rcs_print_error("Couldn's set flag for non-blocking on socket.\n");
	return -1;
    }
#else
#ifdef O_NDELAY
    if (-1 == fcntl(socket_fd, F_SETFL, O_NDELAY)) {
	rcs_print_error("Couldn's set flag for no delay on socket.\n");
	return -1;
    }
#endif
#endif
    return (0);
}

int make_tcp_socket_blocking(int socket_fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
    int val = fcntl(socket_fd, F_GETFL, 0);
    if (val < 0) {
	rcs_print_error("fcntl error %d %s\n", errno, strerror(errno));
	return -1;
    }
#ifdef O_NONBLOCK
    val &= ~O_NONBLOCK;
#endif
#ifdef O_NDELAY
    val &= ~O_NDELAY;
#endif
    if (fcntl(socket_fd, F_SETFL, val) < 0) {
	rcs_print_error("Couldn's set flag for blocking on socket.: %d,%s\n",
	    errno, strerror(errno));
	return -1;
    }
#endif
    return (0);
}
