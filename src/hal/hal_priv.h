#ifndef HAL_PRIV_H
#define HAL_PRIV_H

/** HAL stands for Hardware Abstraction Layer, and is used by EMC to
    transfer realtime data to and from I/O devices and other low-level
    modules.
*/

/** This file, 'hal_priv.c', contains declarations of most of the
    internal data structures used by the HAL.  It is NOT needed by
    most HAL components.  However, some components that interact
    more closely with the HAL internals, such as "halcmd", need to
    include this file.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This library is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU Lesser General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
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

/* SHMPTR(offset) converts 'offset' to a void pointer. */
#define SHMPTR(offset)  ( (void *)( hal_shmem_base + (offset) ) )

/* SHMOFF(ptr) converts 'ptr' to an offset from shared mem base.  */
#define SHMOFF(ptr)          ( ((char *)(ptr)) - hal_shmem_base )

/** The good news is that none of this linked list complexity is
    visible to the components that use this API.  Complexity here
    is a small price to pay for simplicity later.
*/

/***********************************************************************
*            PRIVATE HAL DATA STRUCTURES AND DECLARATIONS              *
************************************************************************/

/* Master HAL data structure
   There is a single instance of this structure in the machine.
   It resides at the base of the HAL shared memory block, where it
   can be accessed by both realtime and non-realtime versions of
   hal_lib.c.  It contains pointers (offsets) to other data items
   in the area, as well as some housekeeping data.  It is the root
   structure for all data in the HAL.
*/
typedef struct {
    int magic;			/* magic number to tag the struct */
    unsigned long  mutex;	/* protection for linked lists, etc. */
    hal_s32_t shmem_avail;	/* amount of shmem left free */
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
    int comp_free_ptr;		/* list of free component structs */
    int pin_free_ptr;		/* list of free pin structs */
    int sig_free_ptr;		/* list of free signal structs */
    int param_free_ptr;		/* list of free parameter structs */
    int funct_free_ptr;		/* list of free function structs */
    int funct_entry_free_ptr;	/* list of free funct entry structs */
    int thread_free_ptr;	/* list of free thread structs */
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
    int type;			/* 1 if realtime, 0 if not */
    void *shmem_base;		/* base of shmem for this component */
    char name[HAL_NAME_LEN + 1];	/* component name */
} hal_comp_t;

/** HAL 'pin' data structure.
    This structure contains information about a 'pin' object.
*/
typedef struct {
    int next_ptr;		/* next pin in linked list */
    int data_ptr_addr;		/* address of pin data pointer */
    int owner_ptr;		/* component that owns this pin */
    hal_type_t type;		/* data type */
    hal_dir_t dir;		/* pin direction */
    int signal;			/* signal to which pin is linked */
    long dummysig;		/* if unlinked, data_ptr points here */
    char name[HAL_NAME_LEN + 1];	/* pin name */
} hal_pin_t;

/** HAL 'signal' data structure.
    This structure contains information about a 'signal' object.
*/
typedef struct {
    int next_ptr;		/* next signal in linked list */
    int data_ptr;		/* offset of signal value */
    hal_type_t type;		/* data type */
    int readers;		/* number of read pins linked */
    int writers;		/* number of write pins linked */
    char name[HAL_NAME_LEN + 1];	/* signal name */
} hal_sig_t;

/** HAL 'parameter' data structure.
    This structure contains information about a 'parameter' object.
*/
typedef struct {
    int next_ptr;		/* next parameter in linked list */
    int data_ptr;		/* offset of parameter value */
    int owner_ptr;		/* component that owns this signal */
    hal_type_t type;		/* data type */
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
    char name[HAL_NAME_LEN + 1];	/* function name */
} hal_funct_t;

typedef struct {
    int next_ptr;		/* next function entry */
    void *arg;			/* argument for function */
    void (*funct) (void *, long);	/* ptr to function code */
    int funct_ptr;		/* pointer to function */
} hal_funct_entry_t;

#define HAL_STACKSIZE 16384	/* realtime task stacksize */

typedef struct {
    int next_ptr;		/* next thread in linked list */
    int uses_fp;		/* floating point flag */
    long int period;		/* period of the thread, in nsec */
    int priority;		/* priority of the thread */
    int owner_ptr;		/* component that added this thread */
    int task_id;		/* ID of the task that runs this thread */
    hal_s32_t runtime;		/* duration of last run, in nsec */
    hal_s32_t maxtime;		/* duration of longest run, in nsec */
    int funct_list;		/* first function to execute */
    char name[HAL_NAME_LEN + 1];	/* thread name */
} hal_thread_t;

#define HAL_KEY   0x48414C21	/* key used to open HAL shared memory */
#define HAL_MAGIC 0x84328212	/* magic number used to verify shmem */
#define HAL_SIZE  65500		/* so it fits inside 64K */

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

/** The 'find_xxx_by_name()' functions search the appropriate list for
    an object that matches 'name'.  They return a pointer to the object,
    or NULL if no matching object is found.
*/
extern hal_comp_t *halpr_find_comp_by_name(char *name);
extern hal_pin_t *halpr_find_pin_by_name(char *name);
extern hal_sig_t *halpr_find_sig_by_name(char *name);
extern hal_param_t *halpr_find_param_by_name(char *name);
extern hal_thread_t *halpr_find_thread_by_name(char *name);
extern hal_funct_t *halpr_find_funct_by_name(char *name);

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
extern hal_thread_t *halpr_find_thread_by_owner(hal_comp_t * owner,
    hal_thread_t * start);
extern hal_funct_t *halpr_find_funct_by_owner(hal_comp_t * owner,
    hal_funct_t * start);

/** 'find_pin_by_sig()' finds pin(s) that are linked to a specific signal.
    If 'start' is NULL, it starts at the beginning of the pin list, and
    returns the first pin that is linked to 'sig'.  Otherwise it assumes
    that 'start' is the value returned by a previous call, and it returns
    the next matching pin.  If no match is found, it returns NULL
*/
extern hal_pin_t *halpr_find_pin_by_sig(hal_sig_t * sig, hal_pin_t * start);

#endif /* HAL_PRIV_H */
