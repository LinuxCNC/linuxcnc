/********************************************************************
* Description: sendn.h
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

#ifndef WRITEN_H
#define WRITEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>		/* size_t */

    int sendn(int fd, const void *vptr, int n, int flags, double timeout);

#ifdef __cplusplus
};
#endif

#endif /* WRITEN_H */
