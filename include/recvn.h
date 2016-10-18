/********************************************************************
* Description: recvn.c
*   Provides a C file for the recvn function from the book Advanced
*   Programming in the UNIX Environment by Richard Stevens.
*   The function is called repeatedly until n bytes have been received
*   from the file descriptor.
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

#ifndef RECVN_H
#define RECVN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>		/* size_t */

    int recvn(int fd, void *vptr, int n, int flags, double timeout,
	int *bytes_read_ptr);
    extern int recvn_timedout;
    extern int print_recvn_timeout_errors;
#ifdef __cplusplus
};
#endif

#endif /* RECVN_H */
