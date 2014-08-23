/********************************************************************
* Description: tcp_opts.hh
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
#ifndef TCP_OPTS_HH
#define TCP_OPTS_HH

/* Function shared by client and server to set desired options. */
int set_tcp_socket_options(int socket_fd);
int make_tcp_socket_nonblocking(int socket_fd);
int make_tcp_socket_blocking(int socket_fd);

#endif /* TCP_OPTS_HH */
