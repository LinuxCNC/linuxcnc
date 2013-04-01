#ifndef HAL_PRIV_H
#define HAL_PRIV_H

/** HAL stands for Hardware Abstraction Layer, and is used by EMC to
    transfer realtime data to and from I/O devices and other low-level
    modules.
*/
/********************************************************************
* Description:  hal_priv.h
*               This file, 'hal_priv.h', contains declarations of 
*               most of the internal data structures used by the HAL.  
*               It is NOT needed by most HAL components.  However, 
*               some components that interact more closely with the 
*               HAL internals, such as "halcmd", need to include this 
*               file.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/


/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>

    Other contributors:
                       Paul Fox
                       <pgf AT foxharp DOT boston DOT ma DOT us>
*/

/** This library is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.

*/

/***********************************************************************
*                       GENERAL INFORMATION                            *
************************************************************************/

/** The following data structures are used by the HAL but are not part
    of the HAL API and should not be visible to code that uses the HAL.
*/

/** At runtime, the HAL consists of a pile of interconnected data
    structures in a block of shared memory.  There are linked lists
    of components, pins, signals, parameters, functions, and threads.
    Most of the lists are sorted by name, and each of these lists is
    cross linked to the others.  All pins, parameters, functions, and
    threads are linked to the component that created them.  In addition,
    pins are linked to signals, and functions are linked to threads.
    On top of that, when items are deleted, they are stored in (you
    guessed it) linked lists for possible reuse later.

    As a result, the pointer manipulation needed to add or remove
    an item from the HAL gets a little complex and hard to follow.
    Sorry about that...  tread carefully, especially in the
    free_xxx_struct functions.  And mind your mutexes.

    And just to make it more fun: shared memory is mapped differently
    for each process and for the kernel, so you can't just pass pointers
    to shared memory objects from one process to another.  So we use
    integer offsets from the start of shared memory instead of pointers.
    In addition to adding overhead, this makes the code even harder to
    read.  Very annoying!  Like near/far pointers in the old days!
    In areas where speed is critical, we sometimes use both offsets
    (for general access) and pointers (for fast access by the owning
    component only).  The macros below are an attempt to neaten things
    up a little.
*/

#include <rtapi.h>
#include <rtapi_global.h>

#include "rtapi_mbarrier.h"	/* memory barrier primitves */

#if defined(BUILD_SYS_USER_DSO)
#include <stdbool.h>
#else // kernel thread styles
#if defined(RTAPI)
#include <linux/types.h>
#else // ULAPI
#include <stdbool.h>
#include <time.h>               /* remote comp timestamps */
#endif
#endif
RTAPI_BEGIN_DECLS

/* SHMPTR(offset) converts 'offset' to a void pointer. */
#define SHMPTR(offset)  ( (void *)( hal_shmem_base + (offset) ) )

/* SHMOFF(ptr) converts 'ptr' to an offset from shared mem base.  */
#define SHMOFF(ptr)     ( ((char *)(ptr)) - hal_shmem_base )

/* SHMCHK(ptr) verifies that a pointer actually points to a
   location that is part of the HAL shared memory block. */

#define SHMCHK(ptr)  ( ((char *)(ptr)) > (hal_shmem_base) && \
                       ((char *)(ptr)) < (hal_shmem_base + global_data->hal_size) )

/** The good news is that none of this linked list complexity is
    visible to the components that use this API.  Complexity here
    is a small price to pay for simplicity later.
*/

#ifndef MAX
#define MAX(x, y) (((x) > (y))?(x):(y))
#endif


/***********************************************************************
*            PRIVATE HAL DATA STRUCTURES AND DECLARATIONS              *
************************************************************************/

/** HAL "data union" structure
 ** This structure may hold any type of hal data
*/
typedef union {
    hal_bit_t b;
    hal_s32_t s;
    hal_s32_t u;
    hal_float_t f;
} hal_data_u;

/** HAL "list element" data structure.
    This structure is used to implement generic double linked circular
    lists.  Such lists have the following characteristics:
    1) One "dummy" element that serves as the root of the list.
    2) 'next' and 'previous' pointers are never NULL.
    3) Insertion and removal of elements is clean and fast.
    4) No special case code to deal with empty lists, etc.
    5) Easy traversal of the list in either direction.
    This structure has no data, only links.  To use it, include it
    inside a larger structure.
*/
typedef struct {
    int next;			/* next element in list */
    int prev;			/* previous element in list */
} hal_list_t;

/** HAL "oldname" data structure.
    When a pin or parameter gets an alias, this structure is used to
    store the original name.
*/
typedef struct {
    int next_ptr;		/* next struct (used for free list only) */
    char name[HAL_NAME_LEN + 1];	/* the original name */
} hal_oldname_t;

/* Master HAL data structure
   There is a single instance of this structure in the machine.
   It resides at the base of the HAL shared memory block, where it
   can be accessed by both realtime and non-realtime versions of
   hal_lib.c.  It contains pointers (offsets) to other data items
   in the area, as well as some housekeeping data.  It is the root
   structure for all data in the HAL.
*/
typedef struct {
    int version;		/* version code for structs, etc */
    unsigned long mutex;	/* protection for linked lists, etc. */
    hal_s32_t shmem_avail;	/* amount of shmem left free */
    constructor pending_constructor;
			/* pointer to the pending constructor function */
    char constructor_prefix[HAL_NAME_LEN+1];
			        /* prefix of name for new instance */
    char constructor_arg[HAL_NAME_LEN+1];
			        /* prefix of name for new instance */
    int shmem_bot;		/* bottom of free shmem (first free byte) */
    int shmem_top;		/* top of free shmem (1 past last free) */
    int comp_list_ptr;		/* root of linked list of components */
    int pin_list_ptr;		/* root of linked list of pins */
    int sig_list_ptr;		/* root of linked list of signals */
    int param_list_ptr;		/* root of linked list of parameters */
    int funct_list_ptr;		/* root of linked list of functions */
    int thread_list_ptr;	/* root of linked list of threads */
    long base_period;		/* timer period for realtime tasks */
    int threads_running;	/* non-zero if threads are started */
    int oldname_free_ptr;	/* list of free oldname structs */
    int comp_free_ptr;		/* list of free component structs */
    int pin_free_ptr;		/* list of free pin structs */
    int sig_free_ptr;		/* list of free signal structs */
    int param_free_ptr;		/* list of free parameter structs */
    int funct_free_ptr;		/* list of free function structs */
    hal_list_t funct_entry_free;	/* list of free funct entry structs */
    int thread_free_ptr;	/* list of free thread structs */
    int exact_base_period;      /* if set, pretend that rtapi satisfied our
				   period request exactly */
    unsigned char lock;         /* hal locking, can be one of the HAL_LOCK_* types */


#define BITS_PER_BYTE 8
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

#define HAL_NGROUPS 32
#define HAL_GROUP_WORDS BITS_TO_LONGS(HAL_NGROUPS)
    unsigned long group_map[HAL_GROUP_WORDS];    /* set of allocated group id's */
    unsigned long group_trigger[HAL_GROUP_WORDS];	/* bitmap of groups due for notification */
    int group_list_ptr;	        /* list of group structs */
    int member_list_ptr;	/* list of member structs */
    int ring_list_ptr;          /* list of ring structs */
    int group_free_ptr;	        /* list of free group structs */
    int member_free_ptr;	/* list of free member structs */
    int ring_free_ptr;          /* list of free ring structs */
    int ring_deleted_ptr;       /* list of deletd rings - keep for dangling refs */
} hal_data_t;


/** HAL 'component' data structure.
    This structure contains information that is unique to a HAL component.
    An instance of this structure is added to a linked list when the
    component calls hal_init().
*/
typedef struct {
    int next_ptr;		/* next component in the list */
    int comp_id;		/* component ID (RTAPI module id) */
    int mem_id;			/* RTAPI shmem ID used by this comp */
    int type;			/* one of: TYPE_RT, TYPE_USER, TYPE_INSTANCE, TYPE_REMOTE */
    int state;                  /* one of: COMP_INITIALIZING, COMP_UNBOUND, */
                                /* COMP_BOUND, COMP_READY */
    // the next two should be time_t, but kernel wont like them
    // so fudge it as long int
    long int last_update;            /* timestamp of last remote update */
    long int last_bound;             /* timestamp of last bind operation */
    long int last_unbound;           /* timestamp of last unbind operation */
    int pid;			/* PID of component (user components only) */
    void *shmem_base;		/* base of shmem for this component */
    char name[HAL_NAME_LEN + 1];	/* component name */
    constructor make;
    int insmod_args;		/* args passed to insmod when loaded */
} hal_comp_t;

/** HAL 'pin' data structure.
    This structure contains information about a 'pin' object.
*/
typedef struct {
    int next_ptr;		/* next pin in linked list */
    int data_ptr_addr;		/* address of pin data pointer */
    int owner_ptr;		/* component that owns this pin */
    int signal;			/* signal to which pin is linked */
    hal_data_u dummysig;	/* if unlinked, data_ptr points here */
    int oldname;		/* old name if aliased, else zero */
    hal_type_t type;		/* data type */
    hal_pin_dir_t dir;		/* pin direction */
    char name[HAL_NAME_LEN + 1];	/* pin name */
#ifdef USE_PIN_USER_ATTRIBUTES
    double epsilon;
    int flags;
#endif
} hal_pin_t;

/** HAL 'signal' data structure.
    This structure contains information about a 'signal' object.
*/
typedef struct {
    int next_ptr;		/* next signal in linked list */
    int data_ptr;		/* offset of signal value */
    hal_type_t type;		/* data type */
    int readers;		/* number of input pins linked */
    int writers;		/* number of output pins linked */
    int bidirs;			/* number of I/O pins linked */
    char name[HAL_NAME_LEN + 1];	/* signal name */
} hal_sig_t;

/** HAL 'parameter' data structure.
    This structure contains information about a 'parameter' object.
*/
typedef struct {
    int next_ptr;		/* next parameter in linked list */
    int data_ptr;		/* offset of parameter value */
    int owner_ptr;		/* component that owns this signal */
    int oldname;		/* old name if aliased, else zero */
    hal_type_t type;		/* data type */
    hal_param_dir_t dir;	/* data direction */
    char name[HAL_NAME_LEN + 1];	/* parameter name */
} hal_param_t;

/** the HAL uses functions and threads to handle synchronization of
    code.  In general, most control systems need to read inputs,
    perform control calculations, and write outputs, in that order.
    A given component may perform one, two, or all three of those
    functions, but usually code from several components will be
    needed.  Components make that code available by exporting
    functions, then threads are used to run the functions in the
    correct order and at the appropriate rate.

    The following structures implement the function/thread portion
    of the HAL API.  There are two linked lists, one of functions,
    sorted by name, and one of threads, sorted by execution freqency.
    Each thread has a linked list of 'function entries', structs
    that identify the functions connected to that thread.
*/

typedef struct {
    int next_ptr;		/* next function in linked list */
    int uses_fp;		/* floating point flag */
    int owner_ptr;		/* component that added this funct */
    int reentrant;		/* non-zero if function is re-entrant */
    int users;			/* number of threads using function */
    void *arg;			/* argument for function */
    void (*funct) (void *, long);	/* ptr to function code */
    hal_s32_t runtime;		/* duration of last run, in nsec */
    hal_s32_t maxtime;		/* duration of longest run, in nsec */
    char name[HAL_NAME_LEN + 1];	/* function name */
} hal_funct_t;

typedef struct {
    hal_list_t links;		/* linked list data */
    void *arg;			/* argument for function */
    void (*funct) (void *, long);	/* ptr to function code */
    int funct_ptr;		/* pointer to function */
} hal_funct_entry_t;

#define HAL_STACKSIZE 32768	/* realtime task stacksize */

typedef struct {
    int next_ptr;		/* next thread in linked list */
    int uses_fp;		/* floating point flag */
    long int period;		/* period of the thread, in nsec */
    int priority;		/* priority of the thread */
    int task_id;		/* ID of the task that runs this thread */
    hal_s32_t runtime;		/* duration of last run, in nsec */
    hal_s32_t maxtime;		/* duration of longest run, in nsec */
    hal_list_t funct_list;	/* list of functions to run */
    char name[HAL_NAME_LEN + 1];	/* thread name */
    int cpu_id;                 /* cpu to bind on, or -1 */
} hal_thread_t;

typedef struct {
    int next_ptr;		/* next member in linked list */
    int member_ptr;             /* offset of hal_signal_t  */
    int userarg1;                /* interpreted by using layer */
    double epsilon;
} hal_member_t;

typedef struct {
    int next_ptr;		/* next group in free list */
    int id;                     /* the group id */
    int userarg1;	        /* interpreted by using layer */
    int userarg2;	        /* interpreted by using layer */
    int serial;                 /* incremented each time a signal is added/deleted*/
    char name[HAL_NAME_LEN + 1];	/* group name */
    int member_ptr;             /* list of group members */
} hal_group_t;

// record mode: Negative numbers are needed for skips
typedef int ring_size_t;

// the ringbuffer shared data as exposed to a user
// this is part of HAL shared memory and member of hal_ring_t
//
// defaults: record mode, HAL memory, no rmutex/wmutex use
typedef struct {
    char name[HAL_NAME_LEN + 1];  // ring HAL name
    bool is_stream;      // record or stream mode
    bool use_rtapishm;   // use RTAPI shm segment (default: HAL memory)
    bool use_rmutex;     // hint to using code - use ringheader_t.rmutex
    bool use_wmutex;     // hint to using code - use ringheader_t.wmutex

    int reader, writer;  // HAL module id's - informational
    unsigned long rmutex, wmutex; // optional use - if used by multiple readers/writers
    size_t scratchpad_size;
    size_t size_mask;    // stream mode only
    size_t size;         // common to stream and record mode
    size_t head;
    size_t tail;
} ringheader_t;

typedef struct {
    // private
    int next_ptr;		 // next ring in free list
    int owner;                   // creating HAL module
    // if rhdr.uses_rtapishm nonzero, use am RTAPI shm segment
    int shm_handle;              // as returned by rtapi_shmem_new
    int shm_key;                 // HAL_RING_SHM_KEY + running serial
    //  if rhdr.uses_rtapishm nonzero, this points to HAL memory
    unsigned int hal_buf_offset;
    // public: the ring descriptor - this part is visible to using code
    ringheader_t rhdr;
} hal_ring_t;

// descriptor structure for an accessible (attached) ringbuffer
// this structure is in per-user memory, NOT HAL/RTAPI shm
// it is filled in by hal_ring_attach()
// NB: this is actually Pavel's ringheader_t

// some components use a fifo and a scratchpad shared memory area,
// like sampler.c and streamer.c. ringbuffer_t supports this through
// the optional scratchpad, which is created if spsize is > 0
// on hal_ring_new(). The scratchpad size is recorded in
// ringheader_t.scratchpad_size.
typedef struct {
    ringheader_t *header;
    char *buf;           // the actual ring storage (either HALmem or RTAPI shmsegs)
    void *scratchpad;
} ringbuffer_t;

typedef struct {
    void * rv_base;
    int rv_len;
} ringvec_t;

// mode flags passed in by hal_ring_new
// exposed in ringheader_t.mode
//#define MODE_RECORD      _BIT(0)
#define MODE_STREAM      _BIT(1)

// hal_ring_t.flags bits:
//#define ALLOC_HALMEM         _BIT(2)  default
#define ALLOC_RTAPISHMSEG    _BIT(3)
// future non-HAL/RTAPI use:
// #define ALLOC_MALLOC         _BIT(4)  // inproc use only
// #define ALLOC_MMAP           _BIT(5)

// force deallocation of RTAPI shm segment, and put ring descriptor onto free list
// instead of deleted list (might not be a good idea if still in use)
#define FORCE_DEALLOC    _BIT(16)

#define USE_RMUTEX       _BIT(17)
#define USE_WMUTEX       _BIT(18)


// auto-release the HAL mutex on scope exit
// if a local variable is declared like so:
//
// int foo  __attribute__((cleanup(hal_autorelease_mutex)));
//
// then leaving foo's scope will cause halpr_release_lock() to be called
// see http://git.mah.priv.at/gitweb?p=emc2-dev.git;a=shortlog;h=refs/heads/hal-lock-unlock
// make sure the mutex is actually held in the using code when leaving scope!
void hal_autorelease_mutex(void *variable);

// generic ring methods for all modes:

/* create a named ringbuffer, owned by comp module_id
 * mode is one of: MODE_RECORD, MODE_STREAM
 * optionally or'd with USE_RMUTEX, USE_WMUTEX
 * spsize > 0 will allocate a shm scratchpad buffer
 * accessible through ringbuffer_t.scratchpad/ringheader_t.scratchpad
 */
int hal_ring_new(const char *name, int size, int spsize, int module_id, int mode);

/* delete a named ringbuffer */
int hal_ring_delete(const char *name, int force);

/* make an existing ringbuffer accessible to a component
 * rb must point to storage of type ringbuffer_t
 */
int hal_ring_attach(const char *name, ringbuffer_t *rb, int module_id);

// given a ringbuffer_t, retrieve its HAL name
//const char *hal_ring_name(ringbuffer_t *rb);

// advisory flags for multiple reader/writer threads scenarios:
//
// if used, use the rmutex/wmutex fields in ringbuffer_t like so:
//
//  rtapi_mutex_get(&(rb->rmutex));
//  rtapi_mutex_try(&(rb->rmutex));
//  rtapi_mutex_give(&(rb->rmutex));
//int hal_ring_use_wmutex(ringbuffer_t *rb);
//int hal_ring_use_rmutex(ringbuffer_t *rb);

// the MODE_RECORD functions: hal_record_

/* write a variable-sized record into the ringbuffer.
 * record boundaries will be preserved.
 * returns 0 on success,
 * else errno:
 *     ERANGE:  record greater than ring buffer (fatal)
 *     EAGAIN:  currently not enough space in ring buffer (temporary)
 */
//int hal_record_write(ringbuffer_t *h, void * data, size_t sz);

/* return pointer to data available, or NULL if ringbuffer empty
 */
//void *hal_record_next(ringbuffer_t *h);

/* return size of the next readable record
 * return -1 if ringbuffer empty
 * NB: zero-sized records are supported
 */
//ring_size_t hal_record_next_size(ringbuffer_t *h);

/* return a ringvec_t to read the next record
 */
//ringvec_t hal_record_next_vec(ringbuffer_t *ring);

/* advance ring buffer by one record after consuming data
 * with hal_record_next/hal_record_next_size/hal_record_next_iovec
 *
 * NB: without hal_record_shift(), hal_record_next/hal_record_next_size/
 * hal_record_next_iovec are in effect a peek operation
 */
//void hal_record_shift(ringbuffer_t *h);

/* returns the largest contiguous block available
 * such that hal_record_write(h, data, hal_record_write_space(h))
 * will succeed
 *
 * NB: a hal_record_shift() may or may not affect the value
 * returned by this function; as it may or may not change the size
 * largest contiguous block even if more memory for smaller messages
 * is available
 */
//size_t hal_record_write_space(const ringheader_t *h);

/* empty a ring. Not thread safe.
 *
 * NB: only the reader should execute this function!
 */
//void hal_record_flush(ringbuffer_t *h);

#define RB_ALIGN 8

// Round up X to closest upper alignment boundary
static inline ring_size_t size_aligned(const ring_size_t x)
{
    return x + (-x & (RB_ALIGN - 1));
}

static inline ring_size_t * _size_at(const ringbuffer_t *rb, const size_t off)
{
    return (ring_size_t *) (rb->buf + off);
}

static inline int hal_record_write(ringbuffer_t *rb, void * data, size_t sz)
{
    int free;
    ringheader_t *h = rb->header;
    size_t a = size_aligned(sz + sizeof(ring_size_t));

    if (a > h->size) {
	// printf("a=%d h->size=%d\n",a,h->size);
	return ERANGE;
    }
    // -1 + 1 is needed for head==tail
    free = (h->size + h->head - h->tail - 1) % h->size + 1;

    if (free <= a) return EAGAIN;
    if (h->tail + a > h->size) {
	if (h->head <= a)
	    return EAGAIN;
	// Wrap
	*_size_at(rb, h->tail) = -1;

	rtapi_smp_wmb();

	h->tail = 0;
    }
    *_size_at(rb, h->tail) = sz;
    memmove(rb->buf + h->tail + sizeof(ring_size_t), data, sz);

    rtapi_smp_wmb();

    h->tail = (h->tail + a) % h->size;
    return 0;
}

static inline void *hal_record_next(ringbuffer_t *rb)
{
    ringheader_t *h = rb->header;

    if (h->head == h->tail)
	return 0;

    rtapi_smp_rmb();

    return rb->buf + h->head + sizeof(ring_size_t);
}

static inline ring_size_t hal_record_next_size(ringbuffer_t *rb)
{
    ring_size_t sz;
    ringheader_t *h = rb->header;

    if (h->head == h->tail)
	return -1;

    sz = *_size_at(rb, h->head);
    if (sz >= 0) return sz;

    h->head = 0;
    return hal_record_next_size(rb);
}

static inline ringvec_t hal_record_next_vec(ringbuffer_t *ring)
{
    ringvec_t vec;
    ring_size_t size = hal_record_next_size(ring);

    if (size < 0) {
#ifdef __cplusplus
	static const ringvec_t vec = { NULL, 0 };
#else
	static const ringvec_t vec = { .rv_base = NULL, .rv_len = 0 };
#endif
	return vec;
    }
    vec.rv_len = size;
    vec.rv_base = hal_record_next(ring);
    return vec;
}

static inline void hal_record_shift(ringbuffer_t *rb)
{
    ring_size_t size;
    ringheader_t *h = rb->header;

    if (h->head == h->tail)
	return;

    rtapi_smp_mb();

    size = *_size_at(rb, h->head);
    if (size < 0) {
	h->head = 0;
	return;
    }
    size = size_aligned(size + sizeof(ring_size_t));
    h->head = (h->head + size) % h->size;
}

static inline size_t hal_record_write_space(const ringheader_t *h)
{
    int avail = 0;

    if (h->tail < h->head)
        avail = h->head - h->tail;
    else
        avail = MAX(h->head, h->size - h->tail);
    return MAX(0, avail - (2 * RB_ALIGN));
}

static inline void hal_record_flush(ringbuffer_t *rb)
{
    ringheader_t *h = rb->header;
    h->head = h->tail;
}

static inline int hal_ring_isstream(ringbuffer_t *rb)
{
    return rb->header->is_stream;
}

static inline const char * hal_ring_name(ringbuffer_t *rb)
{
    return rb->header->name;
}

static inline int hal_ring_use_wmutex(ringbuffer_t *rb)
{
    return rb->header->use_wmutex ;
}

static inline int hal_ring_use_rmutex(ringbuffer_t *rb)
{
    return rb->header->use_rmutex;
}

static inline int hal_ring_scratchpad_size(ringbuffer_t *rb)
{
    return rb->header->scratchpad_size;
}

// compute the next highest power of 2 of 32-bit v
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static unsigned inline next_power_of_two(unsigned v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}





//  SMP barriers adapted from:
/*
 * $Id: pa_ringbuffer.c 1738 2011-08-18 11:47:28Z rossb $
 * Portable Audio I/O Library
 * Ring Buffer utility.
 *
 * Author: Phil Burk, http://www.softsynth.com
 * modified for SMP safety on Mac OS X by Bjorn Roche
 * modified for SMP safety on Linux by Leland Lucius
 * also, allowed for const where possible
 * modified for multiple-byte-sized data elements by Sven Fischer
 *
 * Note that this is safe only for a single-thread reader and a
 * single-thread writer.
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * The text above constitutes the entire PortAudio license; however,
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also
 * requested that these non-binding requests be included along with the
 * license above.
 */


//  MODE_STREAM ring operations: adapted from the jack2
//  https://github.com/jackaudio/jack2/blob/master/common/ringbuffer.c
//  XXX: I understand this to be compatible with the hal_lib.c license


/*
  Copyright (C) 2000 Paul Davis
  Copyright (C) 2003 Rohan Drape

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

  ISO/POSIX C version of Paul Davis's lock free ringbuffer C++ code.
  This is safe for the case of one read thread and one write thread.
*/

// the stream 'methods'
/* void   hal_stream_get_read_vector(const ringbuffer_t *rb, */
/* 				  ringvec_t *vec); */
/* void   hal_stream_get_write_vector(const ringbuffer_t *rb, */
/* 				   ringvec_t *vec); */
/* size_t hal_stream_read(ringbuffer_t *rb, char *dest, size_t cnt); */
/* size_t hal_stream_peek(ringbuffer_t *rb, char *dest, size_t cnt); */
/* void   hal_stream_read_advance(ringbuffer_t *rb, size_t cnt); */
/* size_t hal_stream_read_space(const ringheader_t *h); */
/* void   hal_stream_flush(ringbuffer_t *rb); */
/* size_t hal_stream_write(ringbuffer_t *rb, const char *src, */
/* 			size_t cnt); */
/* void   hal_stream_write_advance(ringbuffer_t *rb, size_t cnt); */
/* size_t hal_stream_write_space(const ringheader_t *h); */

/* The non-copying data reader.  `vec' is an array of two places.  Set
 * the values at `vec' to hold the current readable data at `rb'.  If
 * the readable data is in one segment the second segment has zero
 * length.
 */
static inline void hal_stream_get_read_vector(const ringbuffer_t *rb,
				ringvec_t *vec)
{
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;
    ringheader_t *h = rb->header;

    w = h->tail;
    r = h->head;

    if (w > r) {
	free_cnt = w - r;
    } else {
	free_cnt = (w - r + h->size) & h->size_mask;
    }
    cnt2 = r + free_cnt;
    if (cnt2 > h->size) {
	/* Two part vector: the rest of the buffer after the current write
	   ptr, plus some from the start of the buffer. */
	vec[0].rv_base = (void *) &(rb->buf[r]);
	vec[0].rv_len = h->size - r;
	vec[1].rv_base = (void *) rb->buf;
	vec[1].rv_len = cnt2 & h->size_mask;

    } else {
	/* Single part vector: just the rest of the buffer */
	vec[0].rv_base = (void *) &(rb->buf[r]);
	vec[0].rv_len = free_cnt;
	vec[1].rv_len = 0;
    }
}

/* The non-copying data writer.  `vec' is an array of two places.  Set
 * the values at `vec' to hold the current writeable data at `rb'.  If
 * the writeable data is in one segment the second segment has zero
 * length.
 */
static inline void hal_stream_get_write_vector(const ringbuffer_t *rb,
				 ringvec_t *vec)
{
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;
    ringheader_t *h = rb->header;

    w = h->tail;
    r = h->head;

    if (w > r) {
	free_cnt = ((r - w + h->size) & h->size_mask) - 1;
    } else if (w < r) {
	free_cnt = (r - w) - 1;
    } else {
	free_cnt = h->size - 1;
    }
    cnt2 = w + free_cnt;
    if (cnt2 > h->size) {
	// Two part vector: the rest of the buffer after the current write
	// ptr, plus some from the start of the buffer.
	vec[0].rv_base = (void *) &(rb->buf[w]);
	vec[0].rv_len = h->size - w;
	vec[1].rv_base = (void *) rb->buf;
	vec[1].rv_len = cnt2 & h->size_mask;
    } else {
	vec[0].rv_base = (void *) &(rb->buf[w]);
	vec[0].rv_len = free_cnt;
	vec[1].rv_len = 0;
    }
    if (free_cnt)
	rtapi_smp_mb();
}


/* Return the number of bytes available for reading.  This is the
 * number of bytes in front of the read pointer and behind the write
 * pointer.
 */
static inline size_t hal_stream_read_space(const ringheader_t *h)
{
    size_t w, r;

    w = h->tail;
    r = h->head;
    if (w > r) {
	return w - r;
    } else {
	return (w - r + h->size) & h->size_mask;
    }
}

/* The copying data reader.  Copy at most `cnt' bytes from `rb' to
 * `dest'.  Returns the actual number of bytes copied.
 */
static inline size_t hal_stream_read(ringbuffer_t *rb, char *dest, size_t cnt)
{
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;
    ringheader_t *h = rb->header;

    if ((free_cnt = hal_stream_read_space (h)) == 0) {
	return 0;
    }
    to_read = cnt > free_cnt ? free_cnt : cnt;
    cnt2 = h->head + to_read;
    if (cnt2 > h->size) {
	n1 = h->size - h->head;
	n2 = cnt2 & h->size_mask;
    } else {
	n1 = to_read;
	n2 = 0;
    }
    memcpy (dest, &(rb->buf[h->head]), n1);
    h->head = (h->head + n1) & h->size_mask;

    if (n2) {
	memcpy (dest + n1, &(rb->buf[h->head]), n2);
	h->head = (h->head + n2) & h->size_mask;
    }
    return to_read;
}


/* The copying data reader w/o read pointer advance.  Copy at most
 * `cnt' bytes from `rb' to `dest'.  Returns the actual number of bytes
 * copied.
 */
static inline size_t hal_stream_peek(ringbuffer_t *rb, char *dest, size_t cnt)
{
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;
    size_t tmp_head;
    ringheader_t *h = rb->header;

    tmp_head = h->head;
    if ((free_cnt = hal_stream_read_space (h)) == 0) {
	return 0;
    }
    to_read = cnt > free_cnt ? free_cnt : cnt;
    cnt2 = tmp_head + to_read;
    if (cnt2 > h->size) {
	n1 = h->size - tmp_head;
	n2 = cnt2 & h->size_mask;
    } else {
	n1 = to_read;
	n2 = 0;
    }
    memcpy (dest, &(rb->buf[tmp_head]), n1);
    tmp_head = (tmp_head + n1) & h->size_mask;

    if (n2) {
	memcpy (dest + n1, &(rb->buf[tmp_head]), n2);
    }
    return to_read;
}


/* Advance the read pointer `cnt' places. */
static inline void hal_stream_read_advance(ringbuffer_t *rb, size_t cnt)
{
    size_t tmp;
    ringheader_t *h = rb->header;

    /* ensure that previous reads (copies out of the ring buffer) are always
       completed before updating (writing) the read index.
       (write-after-read) => full barrier
    */
    rtapi_smp_mb();
    tmp = (h->head + cnt) & h->size_mask;
    h->head = tmp;
}


/* Reset the read and write pointers to zero. This is not thread safe.
 */
static inline void hal_stream_flush(ringbuffer_t *rb)
{
    ringheader_t *h = rb->header;

    h->head = 0;
    h->tail = 0;
    //memset(rb->buf, 0, h->size);
}

/* Return the number of bytes available for writing.  This is the
 * number of bytes in front of the write pointer and behind the read
 * pointer.
 */
static inline size_t hal_stream_write_space(const ringheader_t *h)
{
    size_t w, r;

    w = h->tail;
    r = h->head;
    if (w > r) {
	return ((r - w + h->size) & h->size_mask) - 1;
    } else if (w < r) {
	return (r - w) - 1;
    } else {
	return h->size - 1;
    }
}

/* The copying data writer.  Copy at most `cnt' bytes to `rb' from
 * `src'.  Returns the actual number of bytes copied.
 */
static inline size_t hal_stream_write(ringbuffer_t *rb, const char *src,
			size_t cnt)
{
    size_t free_cnt;
    size_t cnt2;
    size_t to_write;
    size_t n1, n2;
    ringheader_t *h = rb->header;

    if ((free_cnt = hal_stream_write_space (h)) == 0) {
	return 0;
    }
    to_write = cnt > free_cnt ? free_cnt : cnt;
    cnt2 = h->tail + to_write;
    if (cnt2 > h->size) {
	n1 = h->size - h->tail;
	n2 = cnt2 & h->size_mask;
    } else {
	n1 = to_write;
	n2 = 0;
    }
    memcpy (&(rb->buf[h->tail]), src, n1);
    h->tail = (h->tail + n1) & h->size_mask;
    if (n2) {
	memcpy (&(rb->buf[h->tail]), src + n1, n2);
	h->tail = (h->tail + n2) & h->size_mask;
    }
    return to_write;
}

/* Advance the write pointer `cnt' places. */
static inline void hal_stream_write_advance(ringbuffer_t *rb, size_t cnt)
{
    size_t tmp;
    ringheader_t *h = rb->header;

    /* ensure that previous writes are seen before we update the write index
       (write after write)
    */
    rtapi_smp_wmb();
    tmp = (h->tail + cnt) & h->size_mask;
    h->tail = tmp;
}

/* IMPORTANT:  If any of the structures in this file are changed, the
   version code (HAL_VER) must be incremented, to ensure that 
   incompatible utilities, etc, aren't used to manipulate data in
   shared memory.
*/

/* Historical note: in versions 2.0.0 and 2.0.1 of EMC, the key was
   0x48414C21, and instead of the structure starting with a version
   number, it started with a fixed magic number.  Mixing binaries or
   kernel modules from those releases with newer versions will result
   in two shmem regions being open, and really strange results (but 
   should _not_ result in segfaults or other crash type problems).
   This is unfortunate, but I can't retroactively make the old code
   detect version mismatches.  The alternative is worse: if the new
   code used the same shmem key, the result would be segfaults or
   kernel oopses.

   The use of version codes  means that any subsequent changes to
   the structs will be fully protected, with a clean shutdown and
   meaningfull error messages in case of a mismatch.
*/
#include "rtapi_shmkeys.h"
#define HAL_VER   0x0000000C	/* version code */
//#define HAL_SIZE  262000

/* These pointers are set by hal_init() to point to the shmem block
   and to the master data structure. All access should use these
   pointers, they takes into account the mapping of shared memory
   into either kernel or user space.  (The HAL kernel module and
   each HAL user process have their own copy of these vars,
   initialized to match that process's memory mapping.)
*/

extern char *hal_shmem_base;
extern hal_data_t *hal_data;

/***********************************************************************
*            PRIVATE HAL FUNCTIONS - NOT PART OF THE API               *
************************************************************************/

/** None of these functions get or release any mutex.  They all assume
    that the mutex has already been obtained.  Calling them without
    having the mutex may give incorrect results if other processes are
    accessing the data structures at the same time.
*/

/** These functions are used to manipulate double-linked circular lists.
    Every list entry has pointers to the next and previous entries.
    The pointers are never NULL.  If an entry is not in a list its
    pointers point back to itself (which effectively makes it a list
    with only one entry)

    'list_init_entry()' sets the pointers in the list entry to point
    to itself - making it a legal list with only one entry. It should
    be called when a list entry is first allocated.

    'list_prev()' and 'list_next()' simply return a pointer to the
    list entry that precedes or follows 'entry' in the list. If there
    is only one element in the list, they return 'entry'.

    'list_add_after()' adds 'entry' immediately after 'prev'.
    Entry must be a single entry, not in a larger list.

    'list_add_before()' adds 'entry' immediately before 'next'.
    Entry must be a single entry, not in a larger list.

    'list_remove_entry()' removes 'entry' from any list it may be in.
    It returns a pointer to the next entry in the list.  If 'entry'
    was the only entry in the list, it returns 'entry'.
*/
void list_init_entry(hal_list_t * entry);
hal_list_t *list_prev(hal_list_t * entry);
hal_list_t *list_next(hal_list_t * entry);
void list_add_after(hal_list_t * entry, hal_list_t * prev);
void list_add_before(hal_list_t * entry, hal_list_t * next);
hal_list_t *list_remove_entry(hal_list_t * entry);

/** The 'find_xxx_by_name()' functions search the appropriate list for
    an object that matches 'name'.  They return a pointer to the object,
    or NULL if no matching object is found.
*/
extern hal_comp_t *halpr_find_comp_by_name(const char *name);
extern hal_pin_t *halpr_find_pin_by_name(const char *name);
extern hal_sig_t *halpr_find_sig_by_name(const char *name);
extern hal_param_t *halpr_find_param_by_name(const char *name);
extern hal_thread_t *halpr_find_thread_by_name(const char *name);
extern hal_funct_t *halpr_find_funct_by_name(const char *name);

/** Allocates a HAL component structure */
extern hal_comp_t *halpr_alloc_comp_struct(void);

/** 'find_comp_by_id()' searches the component list for an object whose
    component ID matches 'id'.  It returns a pointer to that component,
    or NULL if no match is found.
*/
extern hal_comp_t *halpr_find_comp_by_id(int id);

/** The 'find_xxx_by_owner()' functions find objects owned by a specific
    component.  If 'start' is NULL, they start at the beginning of the
    appropriate list, and return the first item owned by 'comp'.
    Otherwise they assume that 'start' is the value returned by a prior
    call, and return the next matching item.  If no match is found, they
    return NULL.
*/
extern hal_pin_t *halpr_find_pin_by_owner(hal_comp_t * owner,
    hal_pin_t * start);
extern hal_param_t *halpr_find_param_by_owner(hal_comp_t * owner,
    hal_param_t * start);
extern hal_funct_t *halpr_find_funct_by_owner(hal_comp_t * owner,
    hal_funct_t * start);

/** 'find_pin_by_sig()' finds pin(s) that are linked to a specific signal.
    If 'start' is NULL, it starts at the beginning of the pin list, and
    returns the first pin that is linked to 'sig'.  Otherwise it assumes
    that 'start' is the value returned by a previous call, and it returns
    the next matching pin.  If no match is found, it returns NULL
*/
extern hal_pin_t *halpr_find_pin_by_sig(hal_sig_t * sig, hal_pin_t * start);

// visible - locks then hal mutex
extern int halpr_group_new(const char *group, int id, int arg1, int arg2);
extern int halpr_group_delete(const char *group);

extern int halpr_member_new(const char *group, const char *member, int arg1, double epsilon);
extern int halpr_member_delete(const char *group, const char *member);

// group iterator
// for each defined group, call the callback function iff:
// - a NULL group argument is give: this will cause all groups to be visited.
// - a non-null group argument will visit exactly that group with an exact name match (no prefix matching).
// - if the group was found, 1 is returned, else 0.
// cb_data can be used in any fashion and it is not inspected.

// callback return values:
//    0:   this signals 'continue iterating'
//    >0:  this signals 'stop iteration and return count'
//    <0:  this signals an error. Stop iterating and return the error code.
// if iteration runs to completion and the callback has never returned a
// non-zero value, halpr_foreach_group returns the number of groups visited.

typedef int(*hal_group_callback_t)(hal_group_t *group,  void *cb_data);
extern int halpr_foreach_group(const char *group, hal_group_callback_t callback, void *cb_data);

// member iterator
// for each defined member, call the callback function iff its group name matches
// AND its member name matches (exact string match).
// - a NULL group name argument is given: this will cause all groups to be visited.
// - a NULL member name argument is given: this will cause all members to be visited,
//   depending on the previous group match.
//
// giving NULL as group name and 'name' as member name will cause the callback to executed
// for each group which has this signal as member.
//
// cb_data can be used in any fashion and it is not inspected.

// callback return values:
//    0:   this signals 'continue iterating'
//    >0:  this signals 'stop iteration and return count'
//    <0:  this signals an error. Stop iterating and return the error code.
// if iteration runs to completion and the callback has never returned a
// non-zero value, halpr_foreach_member returns the number of members visited.
//
// NB: both halpr_foreach_group and halpr_foreach_member lock the global HAL mutex, so it is
// not a good idea to say, call halpr_foreach_member from a halpr_foreach_group callback.
typedef int(*hal_member_callback_t)(hal_group_t *group, hal_member_t *member, void *cb_data);
extern int halpr_foreach_member(const char *group, const char *member,
				hal_member_callback_t callback, void *cb_data);



RTAPI_END_DECLS
#endif /* HAL_PRIV_H */
