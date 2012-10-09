/********************************************************************
* Description:  linux_ulapi.c
*               This file, 'linux_ulapi.c', implements the user-level
*               API functions for machines with Linux-realtime
*
* Author: John Kasunich, Paul Corner
* Copyright (c) 2004 All rights reserved.
*
* License: LGPL Version 2
*
********************************************************************/

#include <stddef.h>		/* NULL */
#include <stdio.h>		/* printf */
#include <malloc.h>		/* malloc(), free() */
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "rtapi.h"
#include <unistd.h>

int rtapi_init(const char *modname)
{
	/* does nothing, for now */
	return getpid();
}

int rtapi_exit(int module_id)
{
	/* does nothing, for now */
	return 0;
}

/* FIXME - no support for fifos */

int rtapi_fifo_new(int key, int module_id, unsigned long int size, char mode)
{
	return -ENOSYS;
}

int rtapi_fifo_delete(int fifo_id, int module_id)
{
	return -ENOSYS;
}

int rtapi_fifo_read(int fifo_id, char *buf, unsigned long size)
{
	return -ENOSYS;
}

int rtapi_fifo_write(int fifo_id, char *buf, unsigned long int size)
{
	return -ENOSYS;
}

long long rtapi_get_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

#include "rtapi/linux_common.h"
