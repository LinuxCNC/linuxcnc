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
# $Revision$
* $Author$
* $Date$
********************************************************************/

#include <stddef.h>		/* NULL */
#include <stdio.h>		/* printf */
#include <malloc.h>		/* malloc(), free() */
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "rtapi.h"
#include <unistd.h>

int rtapi_init(char *modname)
{
  /* does nothing, for now */
  return getpid();
}


int rtapi_exit(int module_id)
{
  /* does nothing, for now */
  return RTAPI_SUCCESS;
}


/* FIXME - no support for fifos */

int rtapi_fifo_new(int key, int module_id, unsigned long int size, char mode)

{
  return RTAPI_UNSUP;
}

int rtapi_fifo_delete(int fifo_id, int module_id)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_read(int fifo_id, char *buf, unsigned long size)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_write(int fifo_id, char *buf, unsigned long int size)
{
  return RTAPI_UNSUP;
}

#include "rtapi/sim_common.h"
