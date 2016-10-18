#ifndef RTAPI_COMMON_H
#define RTAPI_COMMON_H

/** RTAPI is a library providing a uniform API for several real time
  operating systems.  As of ver 2.0, RTLinux and RTAI are supported.
*/
/********************************************************************
* Description:  rtapi_common.h
*               This file, 'rtapi_common.h', contains typedefs and 
*               other items common to both the realtime and 
*               non-realtime portions of the implementation.
*
* Author: John Kasunich, Paul Corner
* License: LGPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

/** This file, 'rtapi_common.h', contains typedefs and other items
    common to both the realtime and non-realtime portions of the
    implementation.  These items are also common to both the RTAI
    and RTLinux implementations, and most likely to any other
    implementations in the Linux environment.  This data is INTERNAL
    to the RTAPI implementation, and should not be included in any
    application modules.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
    Copyright (C) 2003 Paul Corner
                       <paul_c AT users DOT sourceforge DOT net>

  This library is based on version 1.0, which was released into
  the public domain by its author, Fred Proctor.  Thanks Fred!
*/

/* This library is free software; you can redistribute it and/or
  modify it under the terms of version 2.1 of the GNU Lesser General
  Public License as published by the Free Software Foundation.
  This library is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
*/

/** THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
  ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
  TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
  harming persons must have provisions for completely removing power
  from all motors, etc, before persons enter any danger area. All
  machinery must be designed to comply with local and national safety
  codes, and the authors of this software can not, and do not, take
  any responsibility for such compliance.
*/

/** This code was written as part of the EMC HAL project.  For more
  information, go to www.linuxcnc.org.
*/

/* Keep the includes here - It might get messy.. */

#ifdef RTAPI
#include <linux/sched.h>	/* for blocking when needed */
#else
#include <sched.h>		/* for blocking when needed */
#endif

#include "rtapi_bitops.h"	/* test_bit() et al. */

#if defined(BUILD_SYS_USER_DSO)
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>
#include <sys/types.h>  
#endif

#if defined(BUILD_SYS_USER_DSO)
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>
#include <sys/types.h>
#endif

#ifndef NULL
#define NULL 0
#endif


#include THREADS_HEADERS	/* thread-specific headers */


/* module information */
#ifdef MODULE
MODULE_AUTHOR("John Kasunich, Fred Proctor, & Paul Corner");
MODULE_DESCRIPTION("Portable Real Time API");
MODULE_LICENSE("GPL");
#endif

// RTAPI_MAX_* moved to config.h

#define DEFAULT_MAX_DELAY	10000

/* random numbers used as signatures */
#define TASK_MAGIC		21979
#define MODULE_MAGIC		30812
#define SHMEM_MAGIC             25453

#define MIN_STACKSIZE		32768

/* This file contains data structures that live in shared memory and
   are accessed by multiple different programs, both user processes
   and kernel modules.  If the structure layouts used by various
   programs don't match, that's bad.  So we have revision checking.
   Whenever a module or program is loaded, thread_flavor_id and
   serial is checked against the code in the shared memory area.  If
   they don't match, the rtapi_init() call will fail.
  */

/* These structs hold data associated with objects like tasks, etc. */

typedef enum {
    NO_MODULE = 0,
    REALTIME,
    USERSPACE
} mod_type_t;

typedef struct {
    mod_type_t state;
    char name[RTAPI_NAME_LEN + 1];
} module_data;

typedef enum {
    EMPTY = 0,
    PAUSED,
    PERIODIC,
    FREERUN,
    ENDED,
    USERLAND,
    DELETE_LOCKED	// task ready to be deleted; mutex already obtained
} task_state_t;

typedef struct {
    int magic;
    char name[RTAPI_NAME_LEN];
    int uses_fp;
    size_t stacksize;
    int period;
    int ratio;
    task_state_t state;		/* task state */
    int prio;			/* priority */
    int owner;			/* owning module */
    void (*taskcode) (void *);	/* task code */
    void *arg;			/* task argument */
    int cpu;
    rtapi_thread_flags_t flags;
} task_data;

typedef struct {
    int magic;			/* to check for valid handle */
    int key;			/* key to shared memory area */
    int id;			/* OS identifier for shmem */
    int count;                  /* count of maps in this process */
    int instance;               // if this was a cross-instance attach
    int rtusers;		/* number of realtime modules using block */
    int ulusers;		/* number of user processes using block */
    unsigned long size;		/* size of shared memory area */
    RTAPI_DECLARE_BITMAP(bitmap, RTAPI_MAX_SHMEMS+1);
				/* which modules are using block */
    void *mem;			/* pointer to the memory */
} shmem_data;

typedef struct {
    int magic;			/* to check for valid handle */
    int shmem_id;               /* index into shmem_array */
    int key;                    /* RTAPI shm key */
    int owner;                  /* module which created the ring */
} ring_data;


/* Master RTAPI data structure
   There is a single instance of this structure in the machine.
   It resides in shared memory, where it can be accessed by both
   realtime (RTAPI) and non-realtime (ULAPI) code.  It contains
   all information about the current state of RTAPI/ULAPI and
   the associated resources (tasks, etc.).
*/

typedef struct {
    int magic;			/* magic number to validate data */
    int serial;			/* revision code for matching */
    int thread_flavor_id;	/* unique ID for each thread style: rtapi.h */
    unsigned long mutex;	/* mutex against simultaneous access */
    unsigned long ring_mutex;	/* layering RTAPI functions requires per-layer locks */
    int rt_module_count;	/* loaded RT modules */
    int ul_module_count;	/* running UL processes */
    int task_count;		/* task IDs in use */
    int shmem_count;		/* shared memory blocks in use */
    int timer_running;		/* state of HW timer */
    int rt_cpu;			/* CPU to use for RT tasks */
    long int timer_period;	/* HW timer period */
    module_data module_array[RTAPI_MAX_MODULES + 1];	/* data for modules */
    task_data task_array[RTAPI_MAX_TASKS + 1];	/* data for tasks */
    shmem_data shmem_array[RTAPI_MAX_SHMEMS + 1];	/* data for shared
							   memory */
#ifdef THREAD_RTAPI_DATA
    THREAD_RTAPI_DATA;		/* RTAPI data defined in thread system */
#endif
} rtapi_data_t;


/* rtapi_common.c */
extern rtapi_data_t *rtapi_data;

#if defined(RTAPI) || defined(MODULE)
extern void init_rtapi_data(rtapi_data_t * data);
extern void init_global_data(global_data_t * data, 
			     int instance_id, int hal_size, 
			     int rtlevel, int userlevel, const char *name);
#endif

#if defined(RTAPI) && defined(BUILD_SYS_USER_DSO)
extern int  _next_handle(void);
#endif

// set first thing in rtapi_app_main
extern int shmdrv_loaded;
extern long page_size;  // for munmap

/* rtapi_task.c */
extern task_data *task_array;


/* $(THREADS).c */
/* RT_TASK is actually flavor-specific.  It ought to be hookified, but
   it isn't because the two kthreads flavors both use the same same
   for this data type (perhaps because they share history and the
   ipipe patch)
 */
#if defined(MODULE)
extern RT_TASK *ostask_array[];
#endif

/* rtapi_time.c */
#ifdef BUILD_SYS_USER_DSO
extern int period;
#else /* BUILD_SYS_KBUILD */
extern long int max_delay;
extern unsigned long timer_counts;
#endif
#ifdef HAVE_RTAPI_MODULE_TIMER_STOP
void _rtapi_module_timer_stop(void);
#endif


/* rtapi_shmem.c */
#define RTAPI_MAGIC 0x12601409	/* magic number used to verify shmem */
#define SHMEM_MAGIC_DEL_LOCKED 25454  /* don't obtain mutex when deleting */

extern shmem_data *shmem_array;
extern void *shmem_addr_array[];

/* rtapi_module.c */
#ifndef BUILD_SYS_USER_DSO
extern int _init_master_shared_memory(rtapi_data_t **rtapi_data);
#endif
extern module_data *module_array;

#endif /* RTAPI_COMMON_H */
