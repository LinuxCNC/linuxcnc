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
#include "hal_logging.h"

#ifdef ULAPI
#include <rtapi_compat.h>
#endif


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

#define MAX_EPSILON 5   // canned epsilon values for double pin/signal change detection
#define DEFAULT_EPSILON 0.00001  // epsiolon[0]

// extending this beyond 255 might require adapting rtapi_shmkeys.h
#define HAL_MAX_RINGS	        255


/* These pointers are set by hal_init() to point to the shmem block
   and to the master data structure. All access should use these
   pointers, they takes into account the mapping of shared memory
   into either kernel or user space.  (The HAL kernel module and
   each HAL user process have their own copy of these vars,
   initialized to match that process's memory mapping.)
*/

extern char *hal_shmem_base;

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

// avoid pulling in math.h
#define HAL_FABS(a) ((a) > 0.0 ? (a) : -(a))	/* floating absolute value */



typedef struct halhdr halhdr_t;

typedef union {
    halhdr_t     *hdr;
    hal_comp_t   *comp;
    hal_inst_t   *inst;
    hal_pin_t    *pin;
    hal_param_t  *param;
    hal_sig_t    *sig;
    hal_group_t  *group;
    hal_member_t *member;
    hal_funct_t  *funct;
    hal_thread_t *thread;
    hal_vtable_t *vtable;
    hal_ring_t   *ring;
    hal_plug_t   *plug;
    void         *any;

} hal_object_ptr;

#define HO_NULL ((hal_object_ptr)NULL)

#include "hal_list.h"    // needs SHMPTR/SHMOFF
#include "hal_object.h"  // needs hal_list_t

/***********************************************************************
*            PRIVATE HAL DATA STRUCTURES AND DECLARATIONS              *
************************************************************************/

// grab half of the shm segment for the HAL heap
#define HAL_HEAP_INITIAL     (2) // fraction of global_data->hal_size
// and grow incrementally thereafter
#define HAL_HEAP_INCREMENT   (hal_freemem() / 2)
#define HAL_HEAP_MINFREE     (1024)   // shmem_top - shmem_bot


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
    int shmem_bot;		/* bottom of free shmem (first free byte) */
    int shmem_top;		/* top of free shmem (1 past last free) */

    hal_list_t halobjects;       // list of all named HAL objects
    hal_list_t threads;          // list of threads in ascending priority
    hal_list_t funct_entry_free; // list of free funct entry structs

    long base_period;		/* timer period for realtime tasks */
    int exact_base_period;      /* if set, pretend that rtapi satisfied our
				   period request exactly */

    int threads_running;	/* non-zero if threads are started */

    unsigned char lock;         /* hal locking, can be one of the HAL_LOCK_* types */

    unsigned long long dead_beef; // value poison for legacy pin data_ptr_addr use
    size_t default_ringsize;    // if no exlicit size given

    // since rings are realy just glorified named shm segments, allocate by map
    // this gets around the unexposed rtapi_data segment in userland flavors
    RTAPI_DECLARE_BITMAP(rings, HAL_MAX_RINGS);

    double epsilon[MAX_EPSILON];

    // running count of HAL names memory usage
    size_t str_alloc;
    size_t str_freed;

    // alignment loss in shmalloc_rt()
    size_t rt_alignment_loss;
    size_t hal_malloced; // mostly by comps doing hal_malloc()


    // HAL heap for shmalloc_desc()
    struct rtapi_heap heap;
    unsigned char arena[0] __attribute__((aligned(RTAPI_CACHELINE)));
    // heap grows from here

} hal_data_t;

#define  HAL_VALUE_POISON 0xDEADBEEFBADF00D // hal_data_u poison value for legacy pins

/* This pointer is set by hal_init() to point to the  master data structure.
   All access should use these
   pointers, they takes into account the mapping of shared memory
   into either kernel or user space.  */

extern hal_data_t *hal_data;

static inline size_t hal_freemem(void) {
    if (hal_data == NULL)
	return 0;
    return hal_data->shmem_top - hal_data->shmem_bot;
}

/** HAL 'component' data structure.
    This structure contains information that is unique to a HAL component.
    An instance of this structure is added to a linked list when the
    component calls hal_init().
*/
typedef struct hal_comp {
    halhdr_t hdr;		// common HAL object header
    int type  : 8;              // subtype, one of:
                                // TYPE_RT, TYPE_USER, TYPE_REMOTE
    int state : 8;              /* one of: COMP_INITIALIZING, COMP_UNBOUND, */
                                /* COMP_BOUND, COMP_READY */
    void *shmem_base;           /* base of shmem for this component */

    // the next two should be time_t, but kernel wont like them
    // so fudge it as long int
    long int last_update;            /* timestamp of last remote update */
    long int last_bound;             /* timestamp of last bind operation */
    long int last_unbound;           /* timestamp of last unbind operation */
    int pid;			     /* PID of component (user components only) */

    hal_constructor_t ctor;     // NULL for non-instantiable legacy comps
    hal_destructor_t dtor;      // may be NULL
                                // the default destructor will be called regardless
                                // after any custom destructor, and will delete any
                                // pin, params and functs
                                // owned by a particular instance
                                // hal_exit() will delete any pins/params/functs
                                // directlly owned by the component (i.e. not an inst)

    int insmod_args;		/* args passed to insmod when loaded */
    int userarg1;	        /* interpreted by using layer */
    int userarg2;	        /* interpreted by using layer */
} hal_comp_t;

static inline int is_instantiable(const hal_comp_t *comp) {
    return ((comp != NULL) &&
	    (comp->type == TYPE_RT) &&
	    (comp->ctor != NULL));
}

// HAL instance data structure
// An instance has a name, and a unique ID
// it is owned by exactly one component pointed to by comp_ptr
// it holds a void * to the instance data as required by the instance
typedef struct hal_inst {
    halhdr_t hdr;		// common HAL object header
    int inst_data_ptr;          // offset of instance data in HAL shm segment
    int inst_size;              // size of instdata blob
    char **frozen_argv;         // copy of the newinst argument vectors
                                // lives in global heap
                                // destroyed in free_inst_struct
} hal_inst_t;

/** HAL 'pin' data structure.
    This structure contains information about a 'pin' object.
*/
typedef struct hal_pin {
    halhdr_t hdr;		// common HAL object header
    int _data_ptr_addr;		/* address of pin data pointer */
    int data_ptr;		// v2: just the signal's hal_data_u offset
    int _signal;		// PRIVATE: signal descriptor to which pin is linked
    hal_data_u dummysig;	/* if unlinked, data_ptr points here */
    hal_type_t type;		/* data type */
    hal_pin_dir_t dir;		/* pin direction */
    int flags;
    __u8 eps_index;
} hal_pin_t;

typedef enum {
    PIN_DO_NOT_TRACK=1, // no change monitoring, no reporting
} pinflag_t;

/** HAL 'signal' data structure.
    This structure contains information about a 'signal' object.
*/
typedef struct hal_sig {
    halhdr_t hdr;		// common HAL object header
    hal_type_t type;		/* data type */
    hal_data_u value;           // v2 - store value in descriptor
    int readers;		/* number of input pins linked */
    int writers;		/* number of output pins linked */
    int bidirs;			/* number of I/O pins linked */
} hal_sig_t;



/** HAL 'parameter' data structure.
    This structure contains information about a 'parameter' object.
*/
typedef struct hal_param {
    halhdr_t hdr;		// common HAL object header
    hal_data_u value;       	// v2: actual value, data_ptr == 0
    int data_ptr;		/* offset of parameter value */
    hal_type_t type;		/* data type */
    hal_param_dir_t dir;	/* data direction */

} hal_param_t;

//hal_param_t MK_DEPRECATED;


static inline const hal_type_t sig_type(const hal_sig_t *sig) {
    return sig->type;
}
static inline const hal_type_t pin_type(const hal_pin_t *pin) {
    return pin->type;
}
static inline const hal_type_t param_type(const hal_param_t *param) {
    return param->type;
}
static inline const hal_param_dir_t param_dir(const hal_param_t *param) {
    return param->dir;
}
static inline const hal_pin_dir_t pin_dir(const hal_pin_t *pin) {
    return pin->dir;
}

static inline hal_data_u *sig_value(hal_sig_t *sig) {
    return &sig->value;
}

static inline hal_data_u *param_value(const hal_param_t *param)
{
    return (hal_data_u *)SHMPTR(param->data_ptr);
}

// a pin always has a value - linked or not
static inline hal_data_u *pin_value(hal_pin_t *pin) {
    // XXX simplify eventually to SHMPTR(pin->data_ptr)
    // once v1 pins are history
    if (pin->_signal != 0) {
	hal_sig_t *s = (hal_sig_t *)SHMPTR(pin->_signal);
	return &s->value;
    }
    return &pin->dummysig;
}

// a pin may refer to a signal if linked
static inline hal_sig_t *signal_of(const hal_pin_t *pin) {
    if (pin->_signal != 0)
	return (hal_sig_t *) SHMPTR(pin->_signal);
    return NULL;
}

// make a reference from pin to signal.
static inline void set_signal(hal_pin_t *pin, const hal_sig_t *sig) {
    pin->_signal = SHMOFF(sig);
}

// test if a pin and a signal are linked
static inline bool pin_linked_to(const hal_pin_t *pin, const hal_sig_t *sig) {
    return (pin->_signal == SHMOFF(sig));
}

// test if a pin is linked at all.
static inline bool pin_is_linked(const hal_pin_t *pin) {
    return (pin->data_ptr != SHMOFF(&pin->dummysig));
}

// NB this is not equivalent to unlink_pin()!
// set a pin to 'unlinked' state.
static inline void pin_set_unlinked(hal_pin_t *pin) {
    pin->_signal = 0;
    pin->data_ptr = SHMOFF(&pin->dummysig);
}



// strongly typed hal_data_u setters and getters, once and for all.
static inline const hal_bit_t set_bit_value(hal_data_u *h, const hal_bit_t value) {
    h->_b = value;
    return h->_b;
}
static inline const hal_s32_t set_s32_value(hal_data_u *h, const hal_s32_t value) {
    h->_s = value;
    return h->_s;
}
static inline const hal_u32_t set_u32_value(hal_data_u *h, const hal_u32_t value) {
    h->_u = value;
    return h->_u;
}
static inline const hal_s64_t set_s64_value(hal_data_u *h, const hal_s64_t value) {
    h->_ls = value;
    return h->_ls;
}
static inline const hal_u64_t set_u64_value(hal_data_u *h, const hal_u64_t value) {
    h->_lu = value;
    return h->_lu;
}
static inline const hal_float_t set_float_value(hal_data_u *h, const hal_float_t value) {
    h->_f = value;
    return h->_f;
}
static inline const hal_bit_t get_bit_value(const hal_data_u *h) {
    return h->_b;
}
static inline const hal_s32_t get_s32_value(const hal_data_u *h) {
    return h->_s;
}
static inline const hal_u32_t get_u32_value(const hal_data_u *h) {
    return h->_u;
}
static inline const hal_s64_t get_s64_value(const hal_data_u *h) {
    return h->_ls;
}
static inline const hal_u64_t get_u64_value(const hal_data_u *h) {
    return h->_lu;
}
static inline const hal_float_t get_float_value(const hal_data_u *h) {
    return h->_f;
}


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

// functs are now typed, according to the signature expected by
// the function to be called.
//
// (1) legacy thread functions remain as-is: void (*funct) (void *, long)
//     this is as traditionally exported by hal_export_funct() and is
//     kept for backwards compatibility reasons even if severely limited.
//
// (2) there's an extended API for thread functions which exports more
// interesting data to function, including actual invocation time
// and other data permitting for better introspection. Those functs
// can still be addf'd to a thread, but are called with a more flexible
// signature.
//
// (3) functs can now also be called via userland action, for instance
// by halcmd 'call compname funcname <optional args>'; that is - not by
// a thread at all. These functs evidently have a signature which
// is not compatible with threading (more like 'main(argc, argv)',
// so they cannot be add'fd to a thread function.
// However, as they are owned by a comp, they can be used for
// creating/deleting component instances post-loading.

typedef enum {
    FS_LEGACY_THREADFUNC,  // legacy API
    FS_XTHREADFUNC,        // extended API
    FS_USERLAND,           // userland-callable, with argc/arv vector
} hal_funct_signature_t;

typedef struct hal_thread hal_thread_t;
typedef struct hal_funct  hal_funct_t;

// keeps values pertaining to thread invocation in a struct,
// so a reference can be cheaply passed to the invoked funct
typedef struct hal_funct_args {
    // actual invocation time of this thread cycle
    // (i.e. before calling the first funct in chain)
    long long int thread_start_time;

    // invocation time of the current funct.
    // accounts for the time being used by previous functs,
    // without calling rtapi_get_time() yet once more
    // (RTAPI thread_task already does this, so it's all about making an
    // existing value accessible to the funct)
    long long int start_time;

    long long int last_start_time; // used to determine the actual period

    hal_thread_t  *thread; // descriptor of invoking thread, NULL with FS_USERLAND
    hal_funct_t   *funct;  // descriptor of invoked funct

    // argument vector for FS_USERLAND; 0/NULL for others
    int argc;
    char * const *argv;
} hal_funct_args_t ;

// signatures
typedef void (*legacy_funct_t) (void *, long);
typedef int  (*xthread_funct_t) (void *, const hal_funct_args_t *);
typedef int  (*userland_funct_t) (const hal_funct_args_t *);

typedef union {
    legacy_funct_t   l;       // FS_LEGACY_THREADFUNC
    xthread_funct_t  x;       // FS_XTHREADFUNC
    userland_funct_t u;       // FS_USERLAND
} hal_funct_u;

// hal_export_xfunc argument struct
typedef struct hal_export_xfunct_args {
    hal_funct_signature_t type;
    hal_funct_u funct;
    void *arg;
    int uses_fp;
    int reentrant;
    int owner_id;
} hal_export_xfunct_args_t;

int hal_export_xfunctf( const hal_export_xfunct_args_t *xf, const char *fmt, ...);
int halg_export_xfunctf(const int use_halmutex,
			const hal_export_xfunct_args_t *xf,
			const char *fmt, ...);

typedef struct hal_funct {
    halhdr_t hdr;
    void *arg;			/* argument for function */
    hal_funct_u funct;          // ptr to function code
    hal_funct_signature_t type; // drives call signature, addf
    s32_pin_ptr f_runtime;	// (pin) duration of last run, in nsec
    s32_pin_ptr f_maxtime;	// duration of longest run, in nsec
    bit_pin_ptr f_maxtime_increased;	// on last call, maxtime increased
    int uses_fp;		/* floating point flag */
    int reentrant;		/* non-zero if function is re-entrant */
    int users;			/* number of threads using function */
} hal_funct_t;

typedef struct hal_funct_entry {
    hal_list_t links;		/* linked list data */
    __u8 rmb;                   // issue a read barrier before calling this funct
    __u8 wmb;                   // issue a write barrier after calling this funct
    __u8 type;
    __u8 spare;
    void *arg;			/* argument for function */
    hal_funct_u funct;          // ptr to function code
    int funct_ptr;		/* pointer to function */
} hal_funct_entry_t;

// argument struct for hal_create_xthread()
typedef struct {
    const char *name;
    unsigned long period_nsec;
    int uses_fp;
    int cpu_id;
    rtapi_thread_flags_t flags;
} hal_threadargs_t;

// extended arguments version of hal_create_thread().
int hal_create_xthread(const hal_threadargs_t *args);

typedef struct hal_thread {
    halhdr_t hdr;
    int uses_fp;		/* floating point flag */
    long int period;		/* period of the thread, in nsec */
    int priority;		/* priority of the thread */
    int task_id;		/* ID of the task that runs this thread */
    s32_pin_ptr runtime;         // owned by hal_lib during thread lifetime
    s32_pin_ptr maxtime;
    s32_pin_ptr curr_period;    // actual period measured at cycle start
    hal_float_t mean;           // online jitter (really variance) calculation
    hal_float_t m2;
    hal_u32_t  cycles;
    hal_list_t funct_list;	/* list of functions to run */
    hal_list_t thread;          // list of threads in ascending priority
                                // root: hal_data.threads
    int cpu_id;                 /* cpu to bind on, or -1 */
    rtapi_thread_flags_t flags;             // eg Posix, nowait
} hal_thread_t;


// public accessors for hal_funct_args_t argument
static inline long long int fa_start_time(const hal_funct_args_t *fa)
 { return fa->start_time; }

static inline long long int fa_thread_start_time(const hal_funct_args_t *fa)
{ return fa->thread_start_time; }

static inline long fa_period(const hal_funct_args_t *fa)
{
    if (fa->thread)
	return fa->thread->period;
    return 0;
}

// the actual invocation period including jitter - replaces fa_actual_period()
static inline const hal_s32_t get_s32_pin(const s32_pin_ptr p);
static inline hal_s32_t fa_current_period(const hal_funct_args_t *fa)
{
    if (fa->thread)
	return get_s32_pin(fa->thread->curr_period);
    return 0;
}

static inline const char* fa_thread_name(const hal_funct_args_t *fa)
{
    if (fa->thread)
	return ho_name(fa->thread);
    return "";
}
static inline const char* fa_funct_name(const hal_funct_args_t *fa)
{
    return ho_name(fa->funct);
}

static inline const int fa_argc(const hal_funct_args_t *fa) { return fa->argc; }
static inline char* const * fa_argv(
    const hal_funct_args_t *fa) { return fa->argv; }
static inline const void * fa_arg(const hal_funct_args_t *fa) { return fa->funct->arg; }


// represents a HAL vtable object
typedef struct hal_vtable {
    halhdr_t hdr;		   // common HAL object header
    int context;                   // 0 for RT, pid for userland
    int version;                   // tags switchs struct version
    void *vtable;     // pointer to vtable (valid in loading context only)
} hal_vtable_t;


// only after all descriptors are defined
// (hal_accessor.h wont work with only the incomplete typedefs from hal.h)
#include "hal_accessor.h"


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
#define HAL_VER   13	/* version code */


/***********************************************************************
*            PRIVATE HAL FUNCTIONS - NOT PART OF THE API               *
************************************************************************/

/** the legacy halpr functions do not get or release any mutex.  They all assume
    that the mutex has already been obtained.  Calling them without
    having the mutex may give incorrect results if other processes are
    accessing the data structures at the same time.

    The underlying halg_ generic functions have a use_hal_mutex parameter
    to conditionally use the HAL lock.
*/

// generic lookup by name
hal_object_ptr halg_find_object_by_name(const int use_hal_mutex,
					const int type,
					const char *name);
// generic lookup by ID
hal_object_ptr halg_find_object_by_id(const int use_hal_mutex,
				      const int type,
				      const int id);

/** The 'find_xxx_by_name()' functions search the appropriate list for
    an object that matches 'name'.  They return a pointer to the object,
    or NULL if no matching object is found.
*/
static inline hal_thread_t *halpr_find_thread_by_name(const char *name){
    return halg_find_object_by_name(0, HAL_THREAD, name).thread;
}

static inline  hal_comp_t *halpr_find_comp_by_name(const char *name) {
    return halg_find_object_by_name(0, HAL_COMPONENT, name).comp;
}

static inline  hal_sig_t *halpr_find_sig_by_name(const char *name) {
    return halg_find_object_by_name(0, HAL_SIGNAL, name).sig;
}

static inline hal_funct_t *halpr_find_funct_by_name(const char *name) {
    return halg_find_object_by_name(0, HAL_FUNCT, name).funct;
}

static inline hal_inst_t *halpr_find_inst_by_name(const char *name) {
    return halg_find_object_by_name(0, HAL_INST, name).inst;
}

static inline  hal_param_t *halpr_find_param_by_name(const char *name) {
    return halg_find_object_by_name(0, HAL_PARAM, name).param;
}

static inline hal_pin_t *halpr_find_pin_by_name(const char *name) {
    return halg_find_object_by_name(0, HAL_PIN, name).pin;
}

hal_vtable_t *halg_find_vtable_by_name(const int use_hal_mutex,
				       const char *name,
				       const int version);
static inline hal_vtable_t *halpr_find_vtable_by_name(const char *name,
						      const int version) {
    return halg_find_vtable_by_name(0, name, version);
}

/** The 'find_xxx_by_ID()' functions search for
    an object that matches 'name'.  They return a pointer to the object,
    or NULL if no matching object is found.
*/
/** 'find_comp_by_id()' searches for an object whose
    component ID matches 'id'.  It returns a pointer to that component,
    or NULL if no match is found.
*/

static inline hal_comp_t *halpr_find_comp_by_id(const int id) {
    return halg_find_object_by_id(0, HAL_COMPONENT, id).comp;
}
static inline hal_vtable_t *halpr_find_vtable_by_id(const int id) {
    return halg_find_object_by_id(0, HAL_VTABLE, id).vtable;
}

// observers needed in haltalk
// I guess we better come up with generic iterators for this kind of thing

// return number of pins in a component
int halpr_pin_count(const char *name);

// return number of params in a component
int halpr_param_count(const char *name);

// hal mutex scope-locked version of halpr_find_pin_by_name()
hal_pin_t *
hal_find_pin_by_name(const char *name);

/** Allocates a HAL component structure */
// extern hal_comp_t *halpr_alloc_comp_struct(void);




// private instance API:

// given the owner_id of pin, param or funct,
// find the owning instance
// succeeds only for pins, params, functs owned by a hal_inst_t
// returns NULL for legacy code using comp_id for pins/params/functs

/* hal_inst_t *halg_find_inst_by_id(const int use_hal_mutex, */
/* 				 const int id); */
static inline hal_inst_t *halpr_find_inst_by_id(const int id)
{ return halg_find_object_by_id(0, HAL_INST, id).inst; }

// given the owner_id of pin, param or funct,
// find the owning component regardless whether the object
// was created by an instance id, or a comp id
// always succeeds for pins, params, functs
hal_comp_t *halpr_find_owning_comp(const int owner_id);

/* // iterators - by instance id */
/* hal_pin_t *halpr_find_pin_by_instance_id(const int inst_id, */
/* 					 const hal_pin_t * start); */
/* hal_param_t *halpr_find_param_by_instance_id(const int inst_id, */
/* 					     const hal_param_t * start); */
/* hal_funct_t *halpr_find_funct_by_instance_id(const int inst_id, */
/* 					     const hal_funct_t * start); */

// iterate over insts owned by a particular comp.
// if comp_id < 0, return ALL instances, regardless which comp owns them.
// hal_inst_t *halpr_find_inst_by_owning_comp(const int comp_id, hal_inst_t *start);

// iterators - by owner id, which can refer to either a comp or an instance
hal_pin_t *halpr_find_pin_by_owner_id(const int owner_id, hal_pin_t * start);
// hal_param_t *halpr_find_param_by_owner_id(const int owner_id, hal_param_t * start);
hal_funct_t *halpr_find_funct_by_owner_id(const int owner_id, hal_funct_t * start);


// callback return value convention applies
typedef int (*hal_pin_signal_callback_t)(hal_pin_t *pin,
					 hal_sig_t *sig,
					 void *user);

// 'halg_foreach_pin_by_signal' finds pin(s) that are linked to a specific signal.
// the callback is executed for each pin linked to sig.
// any user-specific argumens can be passed via 'user'
int halg_foreach_pin_by_signal(const int use_hal_mutex,
			       hal_sig_t *sig,
			       hal_pin_signal_callback_t cb,
			       void *user);

int halg_signal_propagate_barriers(const int use_hal_mutex,
				   const hal_sig_t *sig);

void report_memory_usage(void);

char *halg_strdup(const int use_hal_mutex, const char *paramptr);
int halg_free_str(char **s);  // will set *s to NULL
char **halg_dupargv(const bool use_hal_mutex, const int argc,
                    char * const *argv);
int halg_free_argv(const bool use_hal_mutex, char **argv);
int halg_free_single_str(char *s);

// for rtapi_app shutdown
int hal_exit_usercomps(char *name);

#define WITH_HAL_MUTEX_IF(intval) _WITH_MUTEX_IF(&hal_data->mutex, __LINE__, intval)
#define WITH_HAL_MUTEX() _WITH_MUTEX_IF(&hal_data->mutex, __LINE__, 1)



RTAPI_END_DECLS
#endif /* HAL_PRIV_H */
