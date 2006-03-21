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
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <malloc.h>		/* malloc(), free() */
#include "rtapi.h"

/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

struct rtapi_shmem {
  int magic;			/* to check for valid handle */
  int key;			/* key to shared memory area */
  int id;			/* OS identifier for shmem */
  unsigned long int size;	/* size of shared memory area */
  void *mem;			/* pointer to the memory */
};


#define SHMEM_MAGIC   25453	/* random numbers used as signatures */


int rtapi_init(char *modname)
{
  /* does nothing, for now */
  return RTAPI_SUCCESS;
}


int rtapi_exit(int module_id)
{
  /* does nothing, for now */
  return RTAPI_SUCCESS;
}


int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
  rtapi_shmem_handle shmem;

  /* validate shmemptr */
  if (shmemptr == NULL)
    return RTAPI_INVAL;

  /* alloc space for shmem structure */
  shmem = malloc(sizeof(struct rtapi_shmem));
  if (shmem == NULL)
    return RTAPI_NOMEM;

  /* now get shared memory block from OS */
  shmem->id = shmget((key_t) key, (int) size, IPC_CREAT | 0666);
  if (shmem->id == -1) {
    free(shmem);
    return RTAPI_NOMEM;
  }
  /* and map it into process space */
  shmem->mem = shmat(shmem->id, 0, 0);
  if ((int) (shmem->mem) == -1) {
    free(shmem);
    return RTAPI_NOMEM;
  }

  /* label as a valid shmem structure */
  shmem->magic = SHMEM_MAGIC;
  /* fill in the other fields */
  shmem->size = size;
  shmem->key = key;

  /* return handle to the caller */
  *shmemptr = shmem;
  return RTAPI_SUCCESS;
}


int rtapi_shmem_getptr(rtapi_shmem_handle shmem, void **ptr)
{
  /* validate shmem handle */
  if (shmem == NULL)
    return RTAPI_BADH;
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADH;

  /* pass memory address back to caller */
  *ptr = shmem->mem;
  return RTAPI_SUCCESS;
}


int rtapi_shmem_delete(rtapi_shmem_handle shmem)
{
  struct shmid_ds d;
  int r1, r2;

  /* validate shmem handle */
  if (shmem == NULL)
    return RTAPI_BADH;
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADH;

  /* unmap the shared memory */
  r1 = shmdt(shmem->mem);

  /* destroy the shared memory */
  r2 = shmctl(shmem->id, IPC_RMID, &d);
  /* FIXME - Fred had the first two arguments reversed.  I changed
     them to match the shmctl man page on my machine.  Since his way
     worked, maybe there is difference between different libs, or
     maybe my man page is just wrong. */

  /* free the shmem structure */
  shmem->magic = 0;
  free(shmem);

  if ((r1 != 0) || (r2 != 0))
    return RTAPI_FAIL;
  return RTAPI_SUCCESS;
}


/* FIXME - no support for fifos */

int rtapi_fifo_new(int key, unsigned long int size, char mode,
		   rtapi_fifo_handle * fifoptr)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_delete(rtapi_fifo_handle fifo)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_read(rtapi_fifo_handle fifo, char *buf, unsigned long int size)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_write(rtapi_fifo_handle fifo,
		     char *buf, unsigned long int size)
{
  return RTAPI_UNSUP;
}
