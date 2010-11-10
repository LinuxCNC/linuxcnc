/********************************************************************
* Description: shmem.hh
*   C++ file for the Communication Management System (CMS).
*   Includes member Functions for class SHMEM.
*   Notes: The class SHMEM should be used by procedures accessing a
*   shared memory buffer on the same processor.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#ifndef SHMEM_HH
#define SHMEM_HH

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>		/* NULL */
#include <stddef.h>		/* size_t */
#include <sys/types.h>		/* key_t */

#ifdef __cplusplus
}
#endif
#include "cms.hh"		/* class CMS */
#include "shm.hh"		/* class RCS_SHAREDMEM */
#include "memsem.hh"		/* struct mem_access_object */

class SHMEM:public CMS {
  public:
    SHMEM(const char *name, long size, int neutral, key_t key, int m = 0);
    SHMEM(const char *bufline, const char *procline, int set_to_server = 0,
	int set_to_master = 0);
    virtual ~ SHMEM();

    CMS_STATUS main_access(void *_local);

  private:

    /* data buffer stuff */
    int fast_mode;
    int open();			/* get shared mem and sem */
    int close();		/* detach from shared mem and sem */
    key_t key;			/* key for shared mem and sem */
    key_t bsem_key;		// key for blocking semaphore
    int second_read;		// true only if the first read returned no
    // new data
    RCS_SHAREDMEM *shm;		/* shared memory */
    RCS_SEMAPHORE *sem;		/* semaphore */
    int master;			/* Is this process responsible for */
    /* clearing memory & semaphores? */
    double sem_delay;		/* Time to wait between polling the
				   semaphore. */
    struct mem_access_object mao;	/* passed to mem_get_access() */
    enum SHMEM_MUTEX_TYPE {
	NO_MUTEX,
	MAO_MUTEX,
	MAO_MUTEX_W_OS_SEM,
	OS_SEM_MUTEX,
	NO_INTERRUPTS_MUTEX,
	NO_SWITCHING_MUTEX
    };

    int use_os_sem;
    int use_os_sem_only;

    SHMEM_MUTEX_TYPE mutex_type;
    void *shm_addr_offset;

    RCS_SEMAPHORE *bsem;	// blocking semaphore
    int autokey_table_size;

};

#endif /* !SHMEM_HH */
