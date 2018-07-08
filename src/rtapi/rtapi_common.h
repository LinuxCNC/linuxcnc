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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include <rtapi_mutex.h>

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
static unsigned int rev_code = 1;  // increment this whenever you change the data structures

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
    ENDED
} task_state_t;

typedef struct {
    task_state_t state;		/* task state */
    int prio;			/* priority */
    int owner;			/* owning module */
    void (*taskcode) (void *);	/* task code */
    void *arg;			/* task argument */
} task_data;

typedef struct {
    int key;			/* key to shared memory area */
    int rtusers;		/* number of realtime modules using block */
    int ulusers;		/* number of user processes using block */
    unsigned long size;		/* size of shared memory area */
    unsigned long bitmap[(RTAPI_MAX_SHMEMS / 8) + 1];	/* which modules are
							   using block */
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
    rtapi_mutex_t mutex;	/* mutex against simultaneous access */
    int rt_module_count;	/* loaded RT modules */
    int ul_module_count;	/* running UL processes */
    int task_count;		/* task IDs in use */
    int shmem_count;		/* shared memory blocks in use */
    int sem_count;		/* semaphores in use */
    int fifo_count;		/* fifos in use */
    int irq_count;		/* interrupts hooked */
    int timer_running;		/* state of HW timer */
    int rt_cpu;			/* CPU to use for RT tasks */
    long int timer_period;	/* HW timer period */
    module_data module_array[RTAPI_MAX_MODULES + 1];	/* data for modules */
    task_data task_array[RTAPI_MAX_TASKS + 1];	/* data for tasks */
    shmem_data shmem_array[RTAPI_MAX_SHMEMS + 1];	/* data for shared
							   memory */
    sem_data sem_array[RTAPI_MAX_SEMS + 1];	/* data for semaphores */
    fifo_data fifo_array[RTAPI_MAX_FIFOS + 1];	/* data for fifos */
    irq_data irq_array[RTAPI_MAX_IRQS + 1];	/* data for hooked irqs */
} rtapi_data_t;

#define RTAPI_KEY   0x90280A48	/* key used to open RTAPI shared memory */
#define RTAPI_MAGIC 0x12601409	/* magic number used to verify shmem */

/* these pointers are initialized at startup to point
   to resource data in the master data structure above
   all access to the data structure should uses these
   pointers, they take into account the mapping of
   shared memory into either kernel or user space.
   (the RTAPI kernel module and each ULAPI user process
   has its own set of these vars, initialized to match
   that process's memory mapping.)
*/

rtapi_data_t *rtapi_data = NULL;
module_data *module_array = NULL;
task_data *task_array = NULL;
shmem_data *shmem_array = NULL;
sem_data *sem_array = NULL;
fifo_data *fifo_array = NULL;
irq_data *irq_array = NULL;

/* global init code */

static void init_rtapi_data(rtapi_data_t * data)
{
    int n, m;

    /* has the block already been initialized? */
    if (data->magic == RTAPI_MAGIC) {
	/* yes, nothing to do */
	return;
    }
    /* no, we need to init it, grab mutex unconditionally */
    rtapi_mutex_try(&(data->mutex));
    /* set magic number so nobody else init's the block */
    data->magic = RTAPI_MAGIC;
    /* set version code so other modules can check it */
    data->rev_code = rev_code;
    /* and get busy */
    data->rt_module_count = 0;
    data->ul_module_count = 0;
    data->task_count = 0;
    data->shmem_count = 0;
    data->sem_count = 0;
    data->fifo_count = 0;
    data->irq_count = 0;
    data->timer_running = 0;
    data->timer_period = 0;
    /* init the arrays */
    for (n = 0; n <= RTAPI_MAX_MODULES; n++) {
	data->module_array[n].state = EMPTY;
	data->module_array[n].name[0] = '\0';
    }
    for (n = 0; n <= RTAPI_MAX_TASKS; n++) {
	data->task_array[n].state = EMPTY;
	data->task_array[n].prio = 0;
	data->task_array[n].owner = 0;
	data->task_array[n].taskcode = NULL;
    }
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	data->shmem_array[n].key = 0;
	data->shmem_array[n].rtusers = 0;
	data->shmem_array[n].ulusers = 0;
	data->shmem_array[n].size = 0;
	for (m = 0; m < (RTAPI_MAX_SHMEMS / 8) + 1; m++) {
	    data->shmem_array[n].bitmap[m] = 0;
	}
    }
    for (n = 0; n <= RTAPI_MAX_SEMS; n++) {
	data->sem_array[n].users = 0;
	data->sem_array[n].key = 0;
	for (m = 0; m < (RTAPI_MAX_SEMS / 8) + 1; m++) {
	    data->sem_array[n].bitmap[m] = 0;
	}
    }
    for (n = 0; n <= RTAPI_MAX_FIFOS; n++) {
	data->fifo_array[n].state = UNUSED;
	data->fifo_array[n].key = 0;
	data->fifo_array[n].size = 0;
	data->fifo_array[n].reader = 0;
	data->fifo_array[n].writer = 0;
    }
    for (n = 0; n <= RTAPI_MAX_IRQS; n++) {
	data->irq_array[n].irq_num = 0;
	data->irq_array[n].owner = 0;
	data->irq_array[n].handler = NULL;
    }
    /* done, release the mutex */
    rtapi_mutex_give(&(data->mutex));
    return;
}

#endif /* RTAPI_COMMON_H */
