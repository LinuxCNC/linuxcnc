/********************************************************************
* Description:  sim_ulapi.c
*               This file, 'sim_ulapi.c', implements the user-level  
*               API functions for machines without RT (simultated 
*               processes)
*
* Author: John Kasunich, Paul Corner
* License: LGPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#include <stddef.h>		/* NULL */
#include <stdio.h>		/* printf */
#include <malloc.h>		/* malloc(), free() */
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "rtapi.h"
#include <unistd.h>


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

#include "rtapi/sim_common.h"
