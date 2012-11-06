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

#include "rtapi_bitops.h"

#if defined(RTAPI_XENOMAI_USER)
//FIXME minimize
#include <native/task.h>        /* Xenomai task */
#include <native/timer.h>
#include <native/mutex.h>
#include <rtdk.h>
#include <nucleus/types.h>     /* for XNOBJECT_NAME_LEN */

#include <sys/io.h>             /* inb, outb */
#endif
#if defined(RTAPI_POSIX)
#include <pth.h>		/* pth_uctx_* */
#endif

#if defined(RTAPI_RTPREEMPT_USER)
#include <pthread.h>
#endif


#undef RTAPI_FIFO  // drop support for RTAPI fifos

/* maximum number of various resources */
#define RTAPI_MAX_MODULES 64
#define RTAPI_MAX_TASKS   64
#define RTAPI_MAX_SHMEMS  32
#define RTAPI_MAX_SEMS    64
#define RTAPI_MAX_FIFOS   32
#define RTAPI_MAX_IRQS    16

/* This file contains data structures that live in shared memory and
   are accessed by multiple different programs, both user processes
   and kernel modules.  If the structure layouts used by various
   programs don't match, that's bad.  So we have revision checking.
   Whenever a module or program is loaded, the rev_code is checked
   against the code in the shared memory area.  If they don't match,
   the rtapi_init() call will faill.
*/
extern unsigned int rev_code; // see rtapi_common.c

/* These structs hold data associated with objects like tasks, etc. */

typedef enum {
    NO_MODULE = 0,
    REALTIME,
    USERSPACE
} mod_type_t;

typedef struct {
#if defined(RTAPI_RTPREEMPT_USER)
    int magic;
#endif
    mod_type_t state;
    char name[RTAPI_NAME_LEN + 1];
} module_data;

typedef enum {
    EMPTY = 0,
    PAUSED,
    PERIODIC,
    FREERUN,
    ENDED
} task_state_t;

typedef struct {
#if defined(RTAPI_XENOMAI_USER)
    RT_TASK ctx;		/* Xenomai task */
    char name[XNOBJECT_NAME_LEN];
#else
    char name[RTAPI_NAME_LEN];
#endif
#if defined(RTAPI_POSIX)
    pth_uctx_t ctx;		/* thread's context */
#endif
#if defined(RTAPI_RTPREEMPT_USER)
    int deleted;
    int destroyed;
    int deadline_scheduling;
    struct timespec next_time;

    /* The realtime thread. */
    pthread_t thread;
    pthread_barrier_t thread_init_barrier;
    void *stackaddr;

    /* Statistics */
    unsigned long minfault_base;
    unsigned long majfault_base;
    unsigned int failures;
#endif
    int magic;
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
} task_data;

typedef struct {
    int magic;			/* to check for valid handle */
    int key;			/* key to shared memory area */
    int id;			/* OS identifier for shmem */
    int count;                    /* count of maps in this process */
    int rtusers;		/* number of realtime modules using block */
    int ulusers;		/* number of user processes using block */
    unsigned long size;		/* size of shared memory area */
    unsigned long bitmap[(RTAPI_MAX_SHMEMS / 8) + 1];	/* which modules are
							   using block */
    void *mem;			/* pointer to the memory */
} shmem_data;

typedef struct {
    int users;			/* number of modules using the semaphore */
    int key;			/* key to semaphore */
    unsigned long bitmap[(RTAPI_MAX_SEMS / 8) + 1];	/* which modules are
							   using sem */
} sem_data;

typedef enum {
    UNUSED = 0,
    HAS_READER = 1,
    HAS_WRITER = 2,
    HAS_BOTH = 3
} fifo_state_t;			/* used as bitmasks */

typedef struct {
    fifo_state_t state;		/* task state */
    int key;			/* key to fifo */
    int reader;			/* module ID of reader */
    int writer;			/* module ID of writer */
    unsigned long int size;	/* size of fifo area */
} fifo_data;


typedef struct {
    int irq_num;		/* IRQ number */
    int owner;			/* owning module */
    void (*handler) (void);	/* interrupt handler function */
} irq_data;

/* Master RTAPI data structure
   There is a single instance of this structure in the machine.
   It resides in shared memory, where it can be accessed by both
   realtime (RTAPI) and non-realtime (ULAPI) code.  It contains
   all information about the current state of RTAPI/ULAPI and
   the associated resources (tasks, etc.).
*/

typedef struct {
    int magic;			/* magic number to validate data */
    int rev_code;		/* revision code for matching */
    unsigned long mutex;	/* mutex against simultaneous access */
    int rt_module_count;	/* loaded RT modules */
    int ul_module_count;	/* running UL processes */
    int task_count;		/* task IDs in use */
    int shmem_count;		/* shared memory blocks in use */
    int timer_running;		/* state of HW timer */
    int rt_cpu;			/* CPU to use for RT tasks */
#if defined(RTAPI_XENOMAI_KERNEL) || defined(RTAPI_XENOMAI_USER) 
    int rt_wait_error;          /* release point missed */
    int rt_last_overrun;       /* last  number of overruns reported by Xenomai */
    int rt_total_overruns;      /* total number of overruns reported by Xenomai */
#endif
    long int timer_period;	/* HW timer period */
    module_data module_array[RTAPI_MAX_MODULES + 1];	/* data for modules */
    task_data task_array[RTAPI_MAX_TASKS + 1];	/* data for tasks */
    shmem_data shmem_array[RTAPI_MAX_SHMEMS + 1];	/* data for shared
							   memory */
} rtapi_data_t;

#define RTAPI_KEY   0x90280A48	/* key used to open RTAPI shared memory */
#define RTAPI_MAGIC 0x12601409	/* magic number used to verify shmem */

extern rtapi_data_t *rtapi_data;
extern module_data *module_array;
extern task_data *task_array;
extern shmem_data *shmem_array;


extern void init_rtapi_data(rtapi_data_t * data);

#endif /* RTAPI_COMMON_H */
