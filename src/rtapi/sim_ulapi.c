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
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <malloc.h>		/* malloc(), free() */
#include <sys/time.h>
#include <time.h>
#include "rtapi.h"

/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

typedef struct {
  int magic;			/* to check for valid handle */
  int key;			/* key to shared memory area */
  int id;			/* OS identifier for shmem */
  unsigned long int size;	/* size of shared memory area */
  void *mem;			/* pointer to the memory */
} rtapi_shmem_handle;

static int msg_level = RTAPI_MSG_INFO;	/* message printing level */

#define SHMEM_MAGIC   25453	/* random numbers used as signatures */


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


int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
  /* alloc space for shmem structure */
  rtapi_shmem_handle *shmem = malloc(sizeof(rtapi_shmem_handle));
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
  return (int)shmem;
}


int rtapi_shmem_getptr(int handle, void **ptr)
{
  rtapi_shmem_handle *shmem = (rtapi_shmem_handle*) handle;

  /* validate shmem handle */
  if (shmem == NULL)
    return RTAPI_BADID;
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADID;

  /* pass memory address back to caller */
  *ptr = shmem->mem;
  return RTAPI_SUCCESS;
}


int rtapi_shmem_delete(int handle, int module_id)
{
  struct shmid_ds d;
  int r1, r2;

  rtapi_shmem_handle *shmem = (rtapi_shmem_handle*) handle;

  /* validate shmem handle */
  if (shmem == NULL)
    return RTAPI_BADID;
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADID;

  /* unmap the shared memory */
  r1 = shmdt(shmem->mem);

  /* destroy the shared memory */
  r2 = shmctl(shmem->id, IPC_STAT, &d);
  fprintf(stderr, "r2=%d d.shm_nattch=%d\n", r2, (int)d.shm_nattch);
  if(r2 == 0 && d.shm_nattch == 0) {
      r2 = shmctl(shmem->id, IPC_RMID, &d);
  }
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

#define BUFFERLEN 1024

void rtapi_print(const char *fmt, ...)
{
    char buffer[BUFFERLEN + 1];
    va_list args;

    va_start(args, fmt);
    /* call the normal library vnsprintf() */
    vsnprintf(buffer, BUFFERLEN, fmt, args);
    fputs(buffer, stdout);
    va_end(args);
}


void rtapi_print_msg(int level, const char *fmt, ...)
{
    char buffer[BUFFERLEN + 1];
    va_list args;

    if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
	va_start(args, fmt);
	/* call the normal library vnsprintf() */
	vsnprintf(buffer, BUFFERLEN, fmt, args);
	fputs(buffer, stdout);
	va_end(args);
    }
}

int rtapi_snprintf(char *buffer, unsigned long int size, const char *msg, ...) {
    va_list args;
    int result;

    va_start(args, msg);
    /* call the normal library vnsprintf() */
    result = vsnprintf(buffer, size, msg, args);
    va_end(args);
    return result;
}

int rtapi_vsnprintf(char *buffer, unsigned long int size, const char *fmt,
	va_list args) {
    return vsnprintf(buffer, size, fmt, args);
}

int rtapi_set_msg_level(int level) {
    msg_level = level;
    return RTAPI_SUCCESS;
}

int rtapi_get_msg_level() { 
    return msg_level;
}

long long rtapi_get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

#define rdtscll(val) \
         __asm__ __volatile__("rdtsc" : "=A" (val))

long long rtapi_get_clocks(void)
{
    long long int retval;

    rdtscll(retval);
    return retval;    
}


