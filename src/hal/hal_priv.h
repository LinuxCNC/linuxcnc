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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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
#include <rtapi_mutex.h>
RTAPI_BEGIN_DECLS

/* SHMPTR(offset) converts 'offset' to a void pointer. */
#define SHMPTR(offset)  ( (void *)( hal_shmem_base + (offset) ) )

/* SHMOFF(ptr) converts 'ptr' to an offset from shared mem base.  */
#define SHMOFF(ptr)     ( ((char *)(ptr)) - hal_shmem_base )

/* SHMCHK(ptr) verifies that a pointer actually points to a
   location that is part of the HAL shared memory block. */

/* offset 0 is reserved for a null-ish pointer, so SHMCHK(hal_shmem_base) is
   false by design */
#define SHMCHK(ptr)  ( ((char *)(ptr)) > (hal_shmem_base) && \
                       ((char *)(ptr)) < (hal_shmem_base + HAL_SIZE) )

/** The good news is that none of this linked list complexity is
    visible to the components that use this API.  Complexity here
    is a small price to pay for simplicity later.
*/

/***********************************************************************
*            PRIVATE HAL DATA STRUCTURES AND DECLARATIONS              *
************************************************************************/

/** HAL "data union" structure
 ** This structure may hold any type of hal data
*/
typedef union {
    hal_bit_t b;
    hal_s32_t s;
    hal_u32_t u;
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
    rtapi_intptr_t next;			/* next element in list */
    rtapi_intptr_t prev;			/* previous element in list */
} hal_list_t;

/** HAL "oldname" data structure.
    When a pin or parameter gets an alias, this structure is used to
    store the original name.
*/
typedef struct {
    rtapi_intptr_t next_ptr;		/* next struct (used for free list only) */
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
    rtapi_mutex_t mutex;	/* protection for linked lists, etc. */
    hal_s32_t shmem_avail;	/* amount of shmem left free */
    constructor pending_constructor;
			/* pointer to the pending constructor function */
    char constructor_prefix[HAL_NAME_LEN+1];
			        /* prefix of name for new instance */
    char constructor_arg[HAL_NAME_LEN+1];
			        /* prefix of name for new instance */
    rtapi_intptr_t shmem_bot;		/* bottom of free shmem (first free byte) */
    rtapi_intptr_t shmem_top;		/* top of free shmem (1 past last free) */
    rtapi_intptr_t comp_list_ptr;		/* root of linked list of components */
    rtapi_intptr_t pin_list_ptr;		/* root of linked list of pins */
    rtapi_intptr_t sig_list_ptr;		/* root of linked list of signals */
    rtapi_intptr_t param_list_ptr;		/* root of linked list of parameters */
    rtapi_intptr_t funct_list_ptr;		/* root of linked list of functions */
    rtapi_intptr_t thread_list_ptr;	/* root of linked list of threads */
    long base_period;		/* timer period for realtime tasks */
    int threads_running;	/* non-zero if threads are started */
    rtapi_intptr_t oldname_free_ptr;	/* list of free oldname structs */
    rtapi_intptr_t comp_free_ptr;		/* list of free component structs */
    rtapi_intptr_t pin_free_ptr;		/* list of free pin structs */
    rtapi_intptr_t sig_free_ptr;		/* list of free signal structs */
    rtapi_intptr_t param_free_ptr;		/* list of free parameter structs */
    rtapi_intptr_t funct_free_ptr;		/* list of free function structs */
    hal_list_t funct_entry_free;	/* list of free funct entry structs */
    rtapi_intptr_t thread_free_ptr;	/* list of free thread structs */
    int exact_base_period;      /* if set, pretend that rtapi satisfied our
				   period request exactly */
    unsigned char lock;         /* hal locking, can be one of the HAL_LOCK_* types */
} hal_data_t;

/** HAL 'component' data structure.
    This structure contains information that is unique to a HAL component.
    An instance of this structure is added to a linked list when the
    component calls hal_init().
*/
typedef struct {
    rtapi_intptr_t next_ptr;		/* next component in the list */
    int comp_id;		/* component ID (RTAPI module id) */
    int mem_id;			/* RTAPI shmem ID used by this comp */
    int type;			/* 1 if realtime, 0 if not */
    int ready;                  /* nonzero if ready, 0 if not */
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
    rtapi_intptr_t next_ptr;		/* next pin in linked list */
    int data_ptr_addr;		/* address of pin data pointer */
    int owner_ptr;		/* component that owns this pin */
    int signal;			/* signal to which pin is linked */
    hal_data_u dummysig;	/* if unlinked, data_ptr points here */
    int oldname;		/* old name if aliased, else zero */
    hal_type_t type;		/* data type */
    hal_pin_dir_t dir;		/* pin direction */
    char name[HAL_NAME_LEN + 1];	/* pin name */
} hal_pin_t;

/** HAL 'signal' data structure.
    This structure contains information about a 'signal' object.
*/
typedef struct {
    rtapi_intptr_t next_ptr;		/* next signal in linked list */
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
    rtapi_intptr_t next_ptr;		/* next parameter in linked list */
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
    rtapi_intptr_t next_ptr;		/* next function in linked list */
    int uses_fp;		/* floating point flag */
    int owner_ptr;		/* component that added this funct */
    int reentrant;		/* non-zero if function is re-entrant */
    int users;			/* number of threads using function */
    void *arg;			/* argument for function */
    void (*funct) (void *, long);	/* ptr to function code */
    hal_s32_t* runtime;	/* (pin) duration of last run, in CPU cycles */
    hal_s32_t maxtime;	/* (param) duration of longest run, in CPU cycles */
    hal_bit_t maxtime_increased;	/* on last call, maxtime increased */
    char name[HAL_NAME_LEN + 1];	/* function name */
} hal_funct_t;

typedef struct {
    hal_list_t links;		/* linked list data */
    void *arg;			/* argument for function */
    void (*funct) (void *, long);	/* ptr to function code */
    int funct_ptr;		/* pointer to function */
} hal_funct_entry_t;

#define HAL_STACKSIZE 16384	/* realtime task stacksize */

typedef struct {
    rtapi_intptr_t next_ptr;		/* next thread in linked list */
    int uses_fp;		/* floating point flag */
    long int period;		/* period of the thread, in nsec */
    int priority;		/* priority of the thread */
    int task_id;		/* ID of the task that runs this thread */
    hal_s32_t* runtime;	/* (pin) duration of last run, in CPU cycles */
    hal_s32_t maxtime;	/* (param) duration of longest run, in CPU cycles */
    hal_list_t funct_list;	/* list of functions to run */
    char name[HAL_NAME_LEN + 1];	/* thread name */
    int comp_id;
} hal_thread_t;

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

#define HAL_KEY   0x48414C32	/* key used to open HAL shared memory */
#define HAL_VER   0x0000000F	/* version code */
#define HAL_SIZE  (85*4096)
#define HAL_PSEUDO_COMP_PREFIX "__" /* prefix to identify a pseudo component */

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

#define HAL_STREAM_MAGIC_NUM		0x4649464F
struct hal_stream_shm {
    unsigned int magic;
    volatile unsigned int in;
    volatile unsigned int out;
    unsigned this_sample;
    int depth;
    int num_pins;
    unsigned long num_overruns, num_underruns;
    hal_type_t type[HAL_STREAM_MAX_PINS];
    union hal_stream_data data[];
};

extern int halpr_parse_types(hal_type_t type[HAL_STREAM_MAX_PINS], const char *fcg);
RTAPI_END_DECLS
#endif /* HAL_PRIV_H */
