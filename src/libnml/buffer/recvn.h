/************************************************************************
* File: recvn.h
* Purpose: Provides a header file for the recvn function from
* the book Advanced Programming in the UNIX Environment by Richard Stevens.
* The writen function calls the recv function repeatedly until n bytes
* have been recv from the file descriptor.
*************************************************************************/

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
