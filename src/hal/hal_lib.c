/** HAL stands for Hardware Abstraction Layer, and is used by EMC to
    transfer realtime data to and from I/O devices and other low-level
    modules.
*/
/********************************************************************
* Description:  hal_libc.c
*               This file, 'hal_lib.c', implements the HAL API, for 
*               both user space and realtime modules.  It uses the 
*               RTAPI and ULAPI #define symbols to determine which 
*               version to compile.
*
* Author: John Kasunich
* License: LGPL Version 2
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
		       Alex Joni
		       <alex_joni AT users DOT sourceforge DOT net>
*/

/** This library is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU Library General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Library General Public
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

// Special case to get gettid() to work properly for recursive mutex.
// Doing this in a rtapi_*.h header is a pain because hal_priv.h is still
// included everywhere and it would fail because of include ordering.
#ifdef __KERNEL__
#include <asm/current.h>
#define halpr_gettid() ((int)(get_current()->pid))
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#define halpr_gettid() ((int)gettid())
#endif

#include <rtapi.h>		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */

#include <rtapi_string.h>
#include <rtapi_atomic.h>

#ifdef RTAPI
#include <rtapi_app.h>
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Hardware Abstraction Layer for EMC");
MODULE_LICENSE("GPL");
#endif /* RTAPI */

#if defined(ULAPI)
#include <sys/types.h>		/* pid_t */
#include <unistd.h>		/* getpid() */
#include <time.h>
#endif

char *hal_shmem_base = NULL;
hal_data_t *hal_data = NULL;
static int lib_module_id = -1;	/* RTAPI module ID for library module */
static int lib_mem_id = 0;	/* RTAPI shmem ID for library module */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/* These functions are used internally by this file.  The code is at
   the end of the file.  */

/** init_hal_data() initializes the entire HAL data structure, only
    if the structure has not already been initialized.  (The init
    is done by the first HAL component to be loaded.
*/
static int init_hal_data(void);

/** The 'shmalloc_xx()' functions allocate blocks of shared memory.
    Each function allocates a block that is 'size' bytes long.
    If 'size' is 3 or more, the block is aligned on a 4 byte
    boundary.  If 'size' is 2, it is aligned on a 2 byte boundary,
    and if 'size' is 1, it is unaligned.
    These functions do not test a mutex - they are called from
    within the hal library by code that already has the mutex.
    (The public function 'hal_malloc()' is a wrapper that gets the
    mutex and then calls 'shmalloc_up()'.)
    The only difference between the two functions is the location
    of the allocated memory.  'shmalloc_up()' allocates from the
    base of shared memory and works upward, while 'shmalloc_dn()'
    starts at the top and works down.
    This is done to improve realtime performance.  'shmalloc_up()'
    is used to allocate data that will be accessed by realtime
    code, while 'shmalloc_dn()' is used to allocate the much
    larger structures that are accessed only occasionally during
    init.  This groups all the realtime data together, improving
    cache performance.
*/
static void *shmalloc_up(long int size);
static void *shmalloc_dn(long int size);

/** The alloc_xxx_struct() functions allocate a structure of the
    appropriate type and return a pointer to it, or 0 if they fail.
    They attempt to re-use freed structs first, if none are
    available, then they call hal_malloc() to create a new one.
    The free_xxx_struct() functions add the structure at 'p' to
    the appropriate free list, for potential re-use later.
    All of these functions assume that the caller has already
    grabbed the hal_data mutex.
*/
hal_comp_t *halpr_alloc_comp_struct(void);
static hal_pin_t *alloc_pin_struct(void);
static hal_sig_t *alloc_sig_struct(void);
static hal_param_t *alloc_param_struct(void);
static hal_oldname_t *halpr_alloc_oldname_struct(void);
#ifdef RTAPI
static hal_funct_t *alloc_funct_struct(void);
#endif /* RTAPI */
static hal_funct_entry_t *alloc_funct_entry_struct(void);
#ifdef RTAPI
static hal_thread_t *alloc_thread_struct(void);
#endif /* RTAPI */

static void free_comp_struct(hal_comp_t * comp);
static void unlink_pin(hal_pin_t * pin);
static void free_pin_struct(hal_pin_t * pin);
static void free_sig_struct(hal_sig_t * sig);
static void free_param_struct(hal_param_t * param);
static void free_oldname_struct(hal_oldname_t * oldname);
#ifdef RTAPI
static void free_funct_struct(hal_funct_t * funct);
#endif /* RTAPI */
static void free_funct_entry_struct(hal_funct_entry_t * funct_entry);
#ifdef RTAPI
static void free_thread_struct(hal_thread_t * thread);
#endif /* RTAPI */

#ifdef RTAPI
/** 'thread_task()' is a function that is invoked as a realtime task.
    It implements a thread, by running down the thread's function list
    and calling each function in turn.
*/
static void thread_task(void *arg);
#endif /* RTAPI */

//
// Recursive locking/unlocking with a mutex
// The recursion resilience is required because callbacks could potentially
// call back into hal_lib and cause severe problems. We must either prevent
// callbacks from re-entering hal_lib or deal with it and use recursion. The
// lesser evil is recursion. None of the code that is called using the mutex is
// real-time anyway.
//
// ==> The initial state requires the mutex to be in the locked state.
// ==> That is why it uses a 'reverse default' mutex, where zero means
// ==> that it is in the locked state. The shared memory is initialized
// ==> with zeros at creation time so this works as expected.
//
// When a thread acquires the lock, then 'lockcnt' will be increased. The first
// thread will fall-through and set its thread ID and the recursion counter. A
// next thread wanting the lock will see a 'lockcnt' of at least 2 and will
// wait on the mutex if it is not already the owner. If it is the same thread,
// then it will increase the recursion level without waiting.
// The unlock decreases the recursion level and, when the recursion level
// reaches zero, clears the thread ID storage. If then 'lockcnt' is still
// larger than zero after its decrease when the recursion level is zero, then
// we know that there is a thread waiting and the mutex is signalled.
//
// Proper function depends on the interlocked inc/dec operation on 'lockcnt'.
//
int halpr_mutex_acquire(void)
{
    int tid = halpr_gettid(); // Caller thread ID
    int cnt = __sync_add_and_fetch(&hal_data->lockcnt, 1); // Interlocked increment
    if(cnt > 1) {
        // A call to lock beyond the first one
        if(tid != hal_data->locktid)
            rtapi_mutex_get_rd(&(hal_data->priv_rdmutex));
    }
    // Getting here means we have the lock
    hal_data->locktid = tid;
    return ++hal_data->locklvl; // Increase recursion level and return it
}

int halpr_mutex_release(void)
{
    int lvl = --hal_data->locklvl; // Decrease recursion level
    if(lvl < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "halpr_mutex_release: release at recursion level %d < 0. "
                                       "Prepare for a lockup/crash.\n", lvl);
        hal_data->locklvl = lvl = 0; // Try some 'sanity'
    }
    if(0 == lvl)
        hal_data->locktid = 0; // Recursion level reached zero
    int cnt = __sync_sub_and_fetch(&hal_data->lockcnt, 1); // Interlocked decrement
    if(cnt < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "halpr_mutex_release: lockcnt %d < 0. "
                                       "Prepare for a lockup/crash.\n", lvl);
        cnt = __sync_sub_and_fetch(&hal_data->lockcnt, cnt); // Increase to try 'sanity'
    }
    if(cnt > 0 && 0 == lvl) {
        // We are at the lowest recursion level and someone is waiting,
        // release the mutex for the waiting thread. That thread will wake up
        // and reacquire the mutex.
        rtapi_mutex_give_rd(&(hal_data->priv_rdmutex));
    }
    return lvl;
}

void halpr_mutex_force_release(void)
{
    int lvl = hal_data->locklvl; // Save recursion level
    hal_data->locktid = 0;       // Clear ownership
    hal_data->locklvl = 0;       // Clear recursion
    int cnt = __sync_add_and_fetch(&hal_data->lockcnt, 0); // Get the value
    if(lvl > cnt) {
        rtapi_print_msg(RTAPI_MSG_ERR, "halpr_mutex_force_release: inconsistent recursion state: lvl=%d > cnt=%d\n", lvl, cnt);
        cnt = lvl; // Never subtract more than recursion level
    } else if(lvl == cnt && hal_data->priv_rdmutex) {
        rtapi_print_msg(RTAPI_MSG_ERR, "halpr_mutex_force_release: inconsistent lock state: locked and lvl=cnt=%d\n", cnt);
    }
    __sync_sub_and_fetch(&hal_data->lockcnt, lvl);  // Interlocked subtract
    rtapi_mutex_give_rd(&(hal_data->priv_rdmutex)); // Unconditional release
}

/***********************************************************************
*                  PUBLIC (API) FUNCTION CODE                          *
************************************************************************/

static int ref_cnt = 0;

#ifdef ULAPI
static int init_cnt = 0; // Reference count on hal_lib_init()/hal_lib_exit()

// Only user-space applications linking to hal_lib have access to this part.
// Especially halmodule will call it on import to map the shared memory.
int hal_lib_init(void)
{
    init_cnt++;  // Reference count init calls

    if(0 != lib_mem_id)
        return 0;  // Already initialized and mapped

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: initializing hal_lib\n");
    char rtapi_name[RTAPI_NAME_LEN + 1];
    rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_LIB_%d", (int)getpid());
    lib_module_id = rtapi_init(rtapi_name);
    if (lib_module_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: could not initialize RTAPI\n");
        init_cnt--;
        return lib_module_id;
    }

    /* get HAL shared memory block from RTAPI */
    lib_mem_id = rtapi_shmem_new(HAL_KEY, lib_module_id, HAL_SIZE);
    if (lib_mem_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: could not open shared memory\n");
        rtapi_exit(lib_module_id);
        // Reset the 'lib_mem_id' because it is used as a key in the test to
        // perform the memory mapping at the start of the function.
        lib_mem_id = 0;
        lib_module_id = -1;
        init_cnt--;
        return -EINVAL;
    }
    /* get address of shared memory area */
    void *mem;
    int retval = rtapi_shmem_getptr(lib_mem_id, &mem);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: could not access shared memory\n");
        rtapi_shmem_delete(lib_mem_id, lib_module_id);
        rtapi_exit(lib_module_id);
        lib_mem_id = 0;
        lib_module_id = -1;
        init_cnt--;
        return -EINVAL;
    }
    /* set up internal pointers to shared mem and data structure */
    hal_shmem_base = (char *)mem;
    hal_data = (hal_data_t *)mem;
    /* perform a global init if needed */
    retval = init_hal_data();
    if (0 != retval) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: could not init shared memory\n");
        rtapi_shmem_delete(lib_mem_id, lib_module_id);
        rtapi_exit(lib_module_id);
        hal_shmem_base = NULL;
        hal_data = NULL;
        lib_mem_id = 0;
        lib_module_id = -1;
        init_cnt--;
        return -EINVAL;
    }
    return 0;
}

static int dangling_comp_cb(hal_query_t *q, void *arg)
{
    (void)arg;
    if(q->callerdata.sival == q->comp.pid) {
        rtapi_print_msg(RTAPI_MSG_ERR, "  dangling component: '%s' id=%d pid=%d ready=%d insmod='%s'\n",
            q->name, q->comp.comp_id, q->comp.pid, (int)q->comp.ready, q->comp.insmod ? q->comp.insmod : "(null)");
    }
    return 0;
}

void hal_lib_exit(void)
{
    if(init_cnt <= 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: hal_lib_exit() called too often (init_cnt=%d)\n", init_cnt);
        return;
    }

    init_cnt--;  // Decrease the ref count

    if(init_cnt > 0) {
        return;  // Not yet final exit.
    }

    // We check the hal_init() refcount. We can't delete until hal_exit() was
    // called on all of them. We alse check whether we actually have been
    // initialized and have a valid shared memory ID. This ID is non-zero when
    // we sucessfully initialized.
    if(0 == ref_cnt) {
        if(0 != lib_mem_id) {
            rtapi_print_msg(RTAPI_MSG_DBG, "HAL: releasing RTAPI resources\n");
            /* release RTAPI resources */
            rtapi_shmem_delete(lib_mem_id, lib_module_id);
            rtapi_exit(lib_module_id);
            lib_mem_id = 0;
            lib_module_id = -1;
            hal_shmem_base = NULL;
            hal_data = NULL;
        } // else it is a no-op
    } else {
        int pid = (int)getpid();
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: hal_lib_exit() called"
                        " while holding %d component references (pid=%d)\n", ref_cnt, pid);
        hal_query_t q = {};
        q.callerdata.sival = pid;
        hal_list_comp(&q, dangling_comp_cb, NULL);
    }
}
#endif /* ULAPI */

int hal_init(const char *name)
{
    int comp_id;
    char rtapi_name[RTAPI_NAME_LEN + 1];
    char hal_name[HAL_NAME_LEN + 1];
    hal_comp_t *comp;

    if (name == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: no component name\n");
	return -EINVAL;
    }
    if (strlen(name) > HAL_NAME_LEN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component name '%s' is too long\n", name);
	return -EINVAL;
    }

#ifdef ULAPI
    if(ref_cnt == 0) {
        int retval = hal_lib_init();
        if (0 != retval)
            return retval;
    }
#endif
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: initializing component '%s'\n",
	name);
    /* copy name to local vars, truncating if needed */
    rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_%s", name);
    rtapi_snprintf(hal_name, sizeof(hal_name), "%s", name);
    /* do RTAPI init */
    comp_id = rtapi_init(rtapi_name);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: rtapi init failed\n");
	return comp_id;
    }
    /* get mutex before manipulating the shared data */
    halpr_mutex_acquire();
    /* make sure name is unique in the system */
    if (halpr_find_comp_by_name(hal_name) != NULL) {
	/* a component with this name already exists */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: duplicate component name '%s'\n", hal_name);
	rtapi_exit(comp_id);
	return -EEXIST;
    }
    /* allocate a new component structure */
    comp = halpr_alloc_comp_struct();
    if (comp == NULL) {
	/* couldn't allocate structure */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for component '%s'\n", hal_name);
	rtapi_exit(comp_id);
	return -ENOMEM;
    }
    /* initialize the structure */
    comp->comp_id = comp_id;
#ifdef RTAPI
    comp->type = HAL_COMP_TYPE_REALTIME;
    comp->pid = 0;
#else /* ULAPI */
    comp->type = HAL_COMP_TYPE_USER;
    comp->pid = getpid();
#endif
    comp->ready = 0;
    comp->shmem_base = hal_shmem_base;
    comp->insmod_args = 0;
    rtapi_snprintf(comp->name, sizeof(comp->name), "%s", hal_name);
    /* insert new structure at head of list */
    comp->next_ptr = hal_data->comp_list_ptr;
    hal_data->comp_list_ptr = SHMOFF(comp);
    /* done with list, release mutex */
    halpr_mutex_release();
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: component '%s' initialized, ID = %02d\n", hal_name, comp_id);
    ref_cnt ++;
    return comp_id;
}

int hal_exit(int comp_id)
{
    rtapi_intptr_t *prev, next;
    hal_comp_t *comp;
    char name[HAL_NAME_LEN + 1];

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: exit called before init\n");
	return -EFAULT;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: removing component %02d\n", comp_id);
    /* grab mutex before manipulating list */
    halpr_mutex_acquire();
    /* search component list for 'comp_id' */
    prev = &(hal_data->comp_list_ptr);
    next = *prev;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -ENOENT;
    }
    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	prev = &(comp->next_ptr);
	next = *prev;
	if (next == 0) {
	    /* reached end of list without finding component */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: component %d not found\n", comp_id);
	    return -ENOENT;
	}
	comp = SHMPTR(next);
    }
    /* found our component, unlink it from the list */
    *prev = comp->next_ptr;
    /* save component name for later */
    rtapi_snprintf(name, sizeof(name), "%s", comp->name);
    /* get rid of the component */
    free_comp_struct(comp);
/*! \todo Another #if 0 */
#if 0
    /*! \todo FIXME - this is the beginning of a two pronged approach to managing
       shared memory.  Prong 1 - re-init the shared memory allocator whenever 
       it is known to be safe.  Prong 2 - make a better allocator that can
       reclaim memory allocated by components when those components are
       removed. To be finished later. */
    /* was that the last component? */
    if (hal_data->comp_list_ptr == 0) {
	/* yes, are there any signals or threads defined? */
	if ((hal_data->sig_list_ptr == 0) && (hal_data->thread_list_ptr == 0)) {
	    /* no, invalidate "magic" number so shmem will be re-inited when
	       a new component is loaded */
	    hal_data->magic = 0;
	}
    }
#endif
    /* release mutex */
    halpr_mutex_release();
    --ref_cnt;
#ifdef ULAPI
    if (0 == ref_cnt) {
        hal_lib_exit();
    } else if(ref_cnt < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: hal_exit() made reference count negative\n");
    }
#endif
    rtapi_exit(comp_id);
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: component %02d removed, name = '%s'\n", comp_id, name);


    return 0;
}

int hal_is_init(void)
{
    return hal_shmem_base != NULL;
}

rtapi_intptr_t hal_reference_unmap(const void *ref)
{
    if(!hal_shmem_base || !ref || !SHMCHK(ref))
        return 0;
    return (rtapi_intptr_t)SHMOFF(ref);
}

void *hal_reference_map(rtapi_intptr_t ref)
{
    if(!hal_shmem_base || ref <= 0 || ref >= HAL_SIZE)
        return NULL;
    return SHMPTR(ref);
}

void *hal_malloc(long int size)
{
    void *retval;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: hal_malloc called before init\n");
	return NULL;
    }
    if (size <= 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: hal_malloc bad size: %ld\n", size);
        return NULL;
    }

    /* get the mutex */
    halpr_mutex_acquire();
    /* allocate memory */
    retval = shmalloc_up(size);
    /* release the mutex */
    halpr_mutex_release();
    /* check return value */
    if (retval == NULL) {
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "HAL: hal_malloc() can't allocate %ld bytes\n", size);
    }
    return retval;
}

#ifdef RTAPI
int hal_set_constructor(int comp_id, constructor make) {
    int next;
    hal_comp_t *comp;

    halpr_mutex_acquire();

    /* search component list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }

    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	next = comp->next_ptr;
	if (next == 0) {
	    /* reached end of list without finding component */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: component %d not found\n", comp_id);
	    return -EINVAL;
	}
	comp = SHMPTR(next);
    }
    
    comp->make = make;

    halpr_mutex_release();
    return 0;
}
#endif

int hal_set_unready(int comp_id) {
    hal_comp_t *comp;
    halpr_mutex_acquire();
    comp = halpr_find_comp_by_id(comp_id);
    if (comp) { comp->ready = 0; }
    halpr_mutex_release();
    if (comp) {return 0;}
    rtapi_print_msg(RTAPI_MSG_ERR,
         "HAL: ERROR: hal_set_unready(): component %d not found\n", comp_id);
    return -EINVAL;
}

int hal_ready(int comp_id) {
    int next;
    hal_comp_t *comp;

    halpr_mutex_acquire();

    /* search component list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }

    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	next = comp->next_ptr;
	if (next == 0) {
	    /* reached end of list without finding component */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: component %d not found\n", comp_id);
	    return -EINVAL;
	}
	comp = SHMPTR(next);
    }
    if(comp->ready > 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "HAL: ERROR: Component '%s' already ready\n", comp->name);
        halpr_mutex_release();
        return -EINVAL;
    }
    comp->ready = 1;
    halpr_mutex_release();
    return 0;
}

int hal_unready(int comp_id) {
    int next;
    hal_comp_t *comp;

    halpr_mutex_acquire();

    /* search component list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }

    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	next = comp->next_ptr;
	if (next == 0) {
	    /* reached end of list without finding component */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: component %d not found\n", comp_id);
	    return -EINVAL;
	}
	comp = SHMPTR(next);
    }
    if(comp->ready < 1) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "HAL: ERROR: Component '%s' already unready\n", comp->name);
        halpr_mutex_release();
        return -EINVAL;
    }
    comp->ready = 0;
    halpr_mutex_release();
    return 0;
}

//
// Return an appropriate error string for HAL error values
//
// The error value should be a negative errno value returned by
// a HAL function.
//
const char *hal_strerror(int err)
{
    switch(err) {
    case -EINVAL:  return "invalid argument";
    case -ENOENT:  return "not found";
    case -EACCES:  return "not writable";
    case -EIO:     return "missing target storage";
    case -EBADF:   return "invalid type";
    case -ERANGE:  return "value out of range";
    case -ENOMEM:  return "out of memory";
    case -EISCONN: return "queue already allocated";
    case -EEXIST:  return "name exists";  // also when pin/param/sig matched with different type
    case -EFAULT:  return "shared memory not mapped";
    case -EMFILE:  return "too many modules";
    case -EPROTO:  return "hal data version mismatch";
    case -EPERM:   return "hal is locked";
    case -ENOEXEC: return "missing constructor";
    default:       return "unknown error";
    }
}

const char *hal_comp_name(int comp_id)
{
    hal_comp_t *comp;
    const char *result = NULL;
    halpr_mutex_acquire();
    comp = halpr_find_comp_by_id(comp_id);
    if(comp) result = comp->name;
    halpr_mutex_release();
    return result;
}

hal_realtime_type_t hal_get_realtime_type() {
    if (hal_data == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: hal_get_realtime_type called before init\n");
        return -EFAULT;
    }
    return hal_data->realtime_type;
}

/***********************************************************************
*                      "LOCKING" FUNCTIONS                             *
************************************************************************/
/** The 'hal_set_lock()' function sets locking based on one of the 
    locking types defined in hal.h
*/
int hal_set_lock(unsigned char lock_type) {
    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: set_lock called before init\n");
	return -EFAULT;
    }
    hal_data->lock = lock_type;
    return 0;
}

/** The 'hal_get_lock()' function returns the current locking level 
    locking types defined in hal.h
*/

unsigned char hal_get_lock() {
    if (hal_data == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: get_lock called before init\n");
        // There is no point in returning a negative error value. The return
        // type wont allow it. We just pretend that everything is locked tight
        // and nothing can change, which would adequately describe the
        // unavailability of shared memory.
        return HAL_LOCK_ALL;
    }
    return hal_data->lock;
}


/***********************************************************************
*                        "PIN" FUNCTIONS                               *
************************************************************************/

/* wrapper functs for typed pins - these call the generic funct below */

int hal_pin_bit_new(const char *name, hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_BIT, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_float_new(const char *name, hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_FLOAT, dir, (void **) data_ptr_addr,
	comp_id);
}

int hal_pin_u32_new(const char *name, hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_U32, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_s32_new(const char *name, hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_S32, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_u64_new(const char *name, hal_pin_dir_t dir,
    hal_u64_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_U64, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_s64_new(const char *name, hal_pin_dir_t dir,
    hal_s64_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_S64, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_port_new(const char *name, hal_pin_dir_t dir,
    hal_port_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_PORT, dir, (void **)data_ptr_addr, comp_id);
}


static int hal_pin_newfv(hal_type_t type, hal_pin_dir_t dir,
    void ** data_ptr_addr, int comp_id, const char *fmt, va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        rtapi_print_msg(RTAPI_MSG_ERR,
	    "hal_pin_newfv: length %d too long for name starting '%s'\n",
	    sz, name);
        return -ENOMEM;
    }
    return hal_pin_new(name, type, dir, data_ptr_addr, comp_id);
}

int hal_pin_bit_newf(hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_BIT, dir, (void**)data_ptr_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_float_newf(hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_FLOAT, dir, (void**)data_ptr_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_u32_newf(hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_U32, dir, (void**)data_ptr_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_s32_newf(hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_S32, dir, (void**)data_ptr_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_u64_newf(hal_pin_dir_t dir,
    hal_u64_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_U64, dir, (void**)data_ptr_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_s64_newf(hal_pin_dir_t dir,
    hal_s64_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_S64, dir, (void**)data_ptr_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_pin_port_newf(hal_pin_dir_t dir,
    hal_port_t **data_ptr_addr, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_pin_newfv(HAL_PORT, dir, (void**)data_ptr_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

// *** New interface ***

int hal_pin_new_bool(int compid, hal_pdir_t dir, hal_bool_t *ref, rtapi_bool def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_pin_newfv(HAL_BIT, dir, (void**)ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_bool(*ref, def);
    return 0;
}

int hal_pin_new_si32(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_s32 def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_pin_newfv(HAL_S32, dir, (void**)ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_si32(*ref, def);
    return 0;
}

int hal_pin_new_ui32(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_u32 def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_pin_newfv(HAL_U32, dir, (void**)ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_ui32(*ref, def);
    return 0;
}

int hal_pin_new_sint(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_sint def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_pin_newfv(HAL_S64, dir, (void**)ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_sint(*ref, def);
    return 0;
}

int hal_pin_new_uint(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_uint def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_pin_newfv(HAL_U64, dir, (void**)ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_uint(*ref, def);
    return 0;
}

int hal_pin_new_real(int compid, hal_pdir_t dir, hal_real_t *ref, rtapi_real def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_pin_newfv(HAL_FLOAT, dir, (void**)ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_real(*ref, def);
    return 0;
}

// FIXME: This needs to change into hal_port_t argument when we break the API
int hal_pin_new_port(int compid, hal_pdir_t dir, hal_sint_t *ref, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_pin_newfv(HAL_PORT, dir, (void**)ref, compid, fmt, ap);
    va_end(ap);
    return ret;
}

/* this is a generic function that does the majority of the work. */

int hal_pin_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
    void **data_ptr_addr, int comp_id)
{
    rtapi_intptr_t *prev, next;
    int cmp;
    hal_pin_t *new, *ptr;
    hal_comp_t *comp;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_new called before init\n");
	return -EFAULT;
    }

    if(*data_ptr_addr) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: pin_new(%s) called with already-initialized memory\n",
            name);
    }
    if (type != HAL_BIT && type != HAL_FLOAT && type != HAL_S32 && type != HAL_U32 && type != HAL_S64 && type != HAL_U64 && type != HAL_PORT) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin type not one of HAL_BIT, HAL_FLOAT, HAL_S32, HAL_U32, HAL_S64, HAL_U64 or HAL_PORT\n");
	return -EINVAL;
    }
    
    if(dir != HAL_IN && dir != HAL_OUT && dir != HAL_IO) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin direction not one of HAL_IN, HAL_OUT, or HAL_IO\n");
	return -EINVAL;
    }
    if(type == HAL_PORT && dir == HAL_IO) {
    rtapi_print_msg(RTAPI_MSG_ERR,
        "HAL: ERROR: pin direction must be one of HAL_IN or HAL_OUT for hal port\n");
    return -EINVAL;
    }
    if (strlen(name) > HAL_NAME_LEN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin name '%s' is too long\n", name);
	return -EINVAL;
    }
    if (hal_data->lock & HAL_LOCK_LOAD)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_new called while HAL locked\n");
	return -EPERM;
    }

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating pin '%s'\n", name);
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    /* validate comp_id */
    comp = halpr_find_comp_by_id(comp_id);
    if (comp == NULL) {
	/* bad comp_id */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }
    // Already check duplicate before allocating
    if(halpr_find_pin_by_name(name)) {
        // Duplicate pin name
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: duplicate pin '%s'\n", name);
        return -EINVAL;
    }
    if(halpr_find_param_by_name(name)) {
        // Overlapping pin/parameter name
        // This is a problem because setp does not distinguish and
        // cannot set pin or param when the names collide.
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: pin '%s' also is the name of a parameter\n", name);
        // We continue, as was done before...
    }
    /* validate passed in pointer - must point to HAL shmem */
    if (! SHMCHK(data_ptr_addr)) {
	/* bad pointer */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: data_ptr_addr not in shared memory\n");
	return -EINVAL;
    }
    if(comp->ready) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_new called after hal_ready\n");
	return -EINVAL;
    }
    /* allocate a new variable structure */
    new = alloc_pin_struct();
    if (new == NULL) {
	/* alloc failed */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for pin '%s'\n", name);
	return -ENOMEM;
    }
    /* initialize the structure */
    new->data_ptr_addr = SHMOFF(data_ptr_addr);
    new->owner_ptr = SHMOFF(comp);
    new->type = type;
    new->dir = dir;
    new->signal = 0;
    memset(&new->dummysig, 0, sizeof(hal_data_u));
    rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
    /* make 'data_ptr' point to dummy signal */
    *data_ptr_addr = (char *)comp->shmem_base + SHMOFF(&(new->dummysig));
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    halpr_mutex_release();
	    return 0;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    halpr_mutex_release();
	    return 0;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_pin_struct(new);
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate variable '%s'\n", name);
	    return -EINVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

int hal_pin_alias(const char *pin_name, const char *alias)
{
    rtapi_intptr_t *prev, next;
    int cmp;
    hal_pin_t *pin, *ptr;
    hal_oldname_t *oldname;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_alias called before init\n");
	return -EFAULT;
    }
    if (hal_data->lock & HAL_LOCK_CONFIG)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_alias called while HAL locked\n");
	return -EPERM;
    }
    if (alias != NULL ) {
	if (strlen(alias) > HAL_NAME_LEN) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
	        "HAL: ERROR: alias name '%s' is too long\n", alias);
	    return -EINVAL;
	}
    }
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    if (alias != NULL ) {
	pin = halpr_find_pin_by_name(alias);
	if ( pin != NULL ) {
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
	        "HAL: ERROR: duplicate pin/alias name '%s'\n", alias);
	    return -EINVAL;
	}
    }
    /* once we unlink the pin from the list, we don't want to have to
       abort the change and repair things.  So we allocate an oldname
       struct here, then free it (which puts it on the free list).  This
       allocation might fail, in which case we abort the command.  But
       if we actually need the struct later, the next alloc is guaranteed
       to succeed since at least one struct is on the free list. */
    oldname = halpr_alloc_oldname_struct();
    if ( oldname == NULL ) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for pin_alias\n");
	return -EINVAL;
    }
    free_oldname_struct(oldname);
    /* find the pin and unlink it from pin list */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, not found */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: pin '%s' not found\n", pin_name);
	    return -EINVAL;
	}
	pin = SHMPTR(next);
	if ( strcmp(pin->name, pin_name) == 0 ) {
	    /* found it, unlink from list */
	    *prev = pin->next_ptr;
	    break;
	}
	if (pin->oldname != 0 ) {
	    oldname = SHMPTR(pin->oldname);
	    if (strcmp(oldname->name, pin_name) == 0) {
		/* found it, unlink from list */
		*prev = pin->next_ptr;
		break;
	    }
	}
	/* didn't find it yet, look at next one */
	prev = &(pin->next_ptr);
	next = *prev;
    }
    if ( alias != NULL ) {
	/* adding a new alias */
	if ( pin->oldname == 0 ) {
	    /* save old name (only if not already saved) */
	    oldname = halpr_alloc_oldname_struct();
	    pin->oldname = SHMOFF(oldname);
	    rtapi_snprintf(oldname->name, sizeof(oldname->name), "%s", pin->name);
	}
	/* change pin's name to 'alias' */
	rtapi_snprintf(pin->name, sizeof(pin->name), "%s", alias);
    } else {
	/* removing an alias */
	if ( pin->oldname != 0 ) {
	    /* restore old name (only if pin is aliased) */
	    oldname = SHMPTR(pin->oldname);
	    rtapi_snprintf(pin->name, sizeof(pin->name), "%s", oldname->name);
	    pin->oldname = 0;
	    free_oldname_struct(oldname);
	}
    }
    /* insert pin back into list in proper place */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    pin->next_ptr = next;
	    *prev = SHMOFF(pin);
	    halpr_mutex_release();
	    return 0;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, pin->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    pin->next_ptr = next;
	    *prev = SHMOFF(pin);
	    halpr_mutex_release();
	    return 0;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

/***********************************************************************
*                      "SIGNAL" FUNCTIONS                              *
************************************************************************/

int hal_signal_new(const char *name, hal_type_t type)
{

    rtapi_intptr_t *prev, next;
    int cmp;
    hal_sig_t *new, *ptr;
    void *data_addr;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal_new called before init\n");
	return -EFAULT;
    }

    if (strlen(name) > HAL_NAME_LEN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal name '%s' is too long\n", name);
	return -EINVAL;
    }
    if (hal_data->lock & HAL_LOCK_CONFIG) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal_new called while HAL is locked\n");
	return -EPERM;
    }

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating signal '%s'\n", name);
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    /* check for an existing signal with the same name */
    if (halpr_find_sig_by_name(name) != NULL) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: duplicate signal '%s'\n", name);
	return -EINVAL;
    }
    /* allocate memory for the signal value */
    /* It is always the size of the data union. This does increase memory usage
     * somewhat, but is required for compliance with the C standard. It also
     * fixes an old memory corruption bug.
     * See: #421 and https://github.com/machinekit/machinekit/issues/524
     */
    switch (type) {
    case HAL_BOOL:
    case HAL_S32:
    case HAL_U32:
    case HAL_SINT:
    case HAL_UINT:
    case HAL_REAL:
    case HAL_PORT:
        data_addr = shmalloc_up(sizeof(hal_data_u));
        // Initialize the signal value
        memset(data_addr, 0, sizeof(hal_data_u));
        break;
    default:
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: illegal signal type %d'\n", type);
	return -EINVAL;
    }
    /* allocate a new signal structure */
    new = alloc_sig_struct();
    if ((new == NULL) || (data_addr == NULL)) {
	/* alloc failed */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for signal '%s'\n", name);
	return -ENOMEM;
    }
    /* initialize the structure */
    new->data_ptr = SHMOFF(data_addr);
    new->type = type;
    new->readers = 0;
    new->writers = 0;
    new->bidirs = 0;
    rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->sig_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    halpr_mutex_release();
	    return 0;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    halpr_mutex_release();
	    return 0;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

int hal_signal_delete(const char *name)
{
    hal_sig_t *sig;
    rtapi_intptr_t *prev, next;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal_delete called before init\n");
	return -EFAULT;
    }
    
    if (hal_data->lock & HAL_LOCK_CONFIG)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal_delete called while HAL locked\n");
	return -EPERM;
    }
    
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: deleting signal '%s'\n", name);
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    /* search for the signal */
    prev = &(hal_data->sig_list_ptr);
    next = *prev;
    while (next != 0) {
	sig = SHMPTR(next);
	if (strcmp(sig->name, name) == 0) {
	    /* this is the right signal, unlink from list */
	    *prev = sig->next_ptr;
	    /* and delete it */
	    free_sig_struct(sig);
	    /* done */
	    halpr_mutex_release();
	    return 0;
	}
	/* no match, try the next one */
	prev = &(sig->next_ptr);
	next = *prev;
    }
    /* if we get here, we didn't find a match */
    halpr_mutex_release();
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: signal '%s' not found\n",
	name);
    return -EINVAL;
}

int hal_link(const char *pin_name, const char *sig_name)
{
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_comp_t *comp;
    void **data_ptr_addr, *data_addr;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: link called before init\n");
	return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_CONFIG)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: link called while HAL locked\n");
	return -EPERM;
    }
    /* make sure we were given a pin name */
    if (pin_name == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: pin name not given\n");
	return -EINVAL;
    }
    /* make sure we were given a signal name */
    if (sig_name == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: signal name not given\n");
	return -EINVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: linking pin '%s' to '%s'\n", pin_name, sig_name);
    /* get mutex before accessing data structures */
    halpr_mutex_acquire();
    /* locate the pin */
    pin = halpr_find_pin_by_name(pin_name);
    if (pin == NULL) {
	/* not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin '%s' not found\n", pin_name);
	return -EINVAL;
    }
    /* locate the signal */
    sig = halpr_find_sig_by_name(sig_name);
    if (sig == NULL) {
	/* not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' not found\n", sig_name);
	return -EINVAL;
    }
    /* found both pin and signal, are they already connected? */
    if (SHMPTR(pin->signal) == sig) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_WARN,
	    "HAL: Warning: pin '%s' already linked to '%s'\n", pin_name, sig_name);
	return 0;
    }
    /* is the pin connected to something else? */
    if(pin->signal) {
	halpr_mutex_release();
	sig = SHMPTR(pin->signal);
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin '%s' is linked to '%s', cannot link to '%s'\n",
	    pin_name, sig->name, sig_name);
	return -EINVAL;
    }
    /* check types */
    if (pin->type != sig->type) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: type mismatch '%s' <- '%s'\n", pin_name, sig_name);
	return -EINVAL;
    }
    /* linking output pin to sig that already has output or I/O pins? */
    if ((pin->dir == HAL_OUT) && ((sig->writers > 0) || (sig->bidirs > 0 ))) {
	/* yes, can't do that */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' already has output or I/O pin(s)\n", sig_name);
	return -EINVAL;
    }
    /* linking bidir pin to sig that is a port?*/
    if ((pin->dir == HAL_IO) && (pin->type == HAL_PORT)) {
    halpr_mutex_release();
    rtapi_print_msg(RTAPI_MSG_ERR,
        "HAL: ERROR: signal '%s' is a port and cannot have I/O pin(s)\n", sig_name);
    return -EINVAL;
    }
    /* linking bidir pin to sig that already has output pin? */
    if ((pin->dir == HAL_IO) && (sig->writers > 0)) {
	/* yes, can't do that */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' already has output pin\n", sig_name);
	return -EINVAL;
    }

    /* linking input pin to port sig that already has an input port? */
    if ((pin->type == HAL_PORT) && (pin->dir == HAL_IN) && (sig->readers > 0)) {
	/* ports can only have one reader */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' can only have one input pin\n", sig_name);
	return -EINVAL;
    }
    
    /* everything is OK, make the new link */
    data_ptr_addr = SHMPTR(pin->data_ptr_addr);
    comp = SHMPTR(pin->owner_ptr);
    data_addr = (char *)comp->shmem_base + sig->data_ptr;
    *data_ptr_addr = data_addr;

    /* if the pin is a HAL_PORT the buffer belongs to the signal, port pins not linked
      to a signal will always be empty (unreadable and unwritable)*/
    bool drive_pin_default_value_onto_signal =
        (pin->type != HAL_PORT) && (pin->dir != HAL_IN || sig->readers == 0 )
            && ( sig->writers == 0 ) && ( sig->bidirs == 0 );
    if (drive_pin_default_value_onto_signal) {
	/* this is the first pin for this signal, copy value from pin's "dummy" field */
	data_addr = hal_shmem_base + sig->data_ptr;

        // assure proper typing on assignment, assigning a hal_data_u is
        // a surefire cause for memory corrupion as hal_data_u is larger
        // than hal_bit_t, hal_s32_t, and hal_u32_t - this works only for 
        // hal_float_t (!)
        // my old, buggy code:
        //*((hal_data_u *)data_addr) = pin->dummysig;

        switch (pin->type) {
        case HAL_BOOL:
            *((hal_bit_t *) data_addr) = pin->dummysig.b;
            break;
        case HAL_S32:
            *((hal_s32_t *) data_addr) = pin->dummysig.s;
            break;
        case HAL_U32:
            *((hal_u32_t *) data_addr) = pin->dummysig.u;
            break;
        case HAL_SINT:
            *((hal_s64_t *) data_addr) = pin->dummysig.ls;
            break;
        case HAL_UINT:
            *((hal_u64_t *) data_addr) = pin->dummysig.lu;
            break;
        case HAL_REAL:
            *((hal_float_t *) data_addr) = pin->dummysig.f;
            break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR,
                          "HAL: BUG: pin '%s' has invalid type %d !!\n",
                          pin->name, pin->type);
            return -EINVAL;
        }
    }
    /* update the signal's reader/writer/bidir counts */
    if ((pin->dir & HAL_IN) != 0) {
	sig->readers++;
    }
    if (pin->dir == HAL_OUT) {
	sig->writers++;
    }
    if (pin->dir == HAL_IO) {
	sig->bidirs++;
    }
    /* and update the pin */
    pin->signal = SHMOFF(sig);
    /* done, release the mutex and return */
    halpr_mutex_release();
    return 0;
}

int hal_unlink(const char *pin_name)
{
    hal_pin_t *pin;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: unlink called before init\n");
	return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_CONFIG)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: unlink called while HAL locked\n");
	return -EPERM;
    }
    /* make sure we were given a pin name */
    if (pin_name == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: pin name not given\n");
	return -EINVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: unlinking pin '%s'\n", pin_name);
    /* get mutex before accessing data structures */
    halpr_mutex_acquire();
    /* locate the pin */
    pin = halpr_find_pin_by_name(pin_name);
    if (pin == NULL) {
	/* not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin '%s' not found\n", pin_name);
	return -EINVAL;
    }
    /* found pin, unlink it */
    unlink_pin(pin);
    /* done, release the mutex and return */
    halpr_mutex_release();
    return 0;
}

/***********************************************************************
*                       "PARAM" FUNCTIONS                              *
************************************************************************/

/* wrapper functs for typed params - these call the generic funct below */

int hal_param_bit_new(const char *name, hal_param_dir_t dir, hal_bit_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_BIT, dir, (void *) data_addr, comp_id);
}

int hal_param_float_new(const char *name, hal_param_dir_t dir, hal_float_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_FLOAT, dir, (void *) data_addr, comp_id);
}

int hal_param_u32_new(const char *name, hal_param_dir_t dir, hal_u32_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_U32, dir, (void *) data_addr, comp_id);
}

int hal_param_s32_new(const char *name, hal_param_dir_t dir, hal_s32_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_S32, dir, (void *) data_addr, comp_id);
}

int hal_param_u64_new(const char *name, hal_param_dir_t dir, hal_u64_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_U64, dir, (void *) data_addr, comp_id);
}
int hal_param_s64_new(const char *name, hal_param_dir_t dir, hal_s64_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_S64, dir, (void *) data_addr, comp_id);
}

static int hal_param_newfv(hal_type_t type, hal_param_dir_t dir,
	void *data_addr, int comp_id, const char *fmt, va_list ap) {
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        rtapi_print_msg(RTAPI_MSG_ERR,
	    "hal_param_newfv: length %d too long for name starting '%s'\n",
	    sz, name);
	return -ENOMEM;
    }
    return hal_param_new(name, type, dir, (void *) data_addr, comp_id);
}

int hal_param_bit_newf(hal_param_dir_t dir, hal_bit_t * data_addr,
    int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_BIT, dir, (void*)data_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_float_newf(hal_param_dir_t dir, hal_float_t * data_addr,
    int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_FLOAT, dir, (void*)data_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_u32_newf(hal_param_dir_t dir, hal_u32_t * data_addr,
    int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_U32, dir, (void*)data_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_s32_newf(hal_param_dir_t dir, hal_s32_t * data_addr,
    int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_S32, dir, (void*)data_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_u64_newf(hal_param_dir_t dir, hal_u64_t * data_addr,
    int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_U64, dir, (void*)data_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_s64_newf(hal_param_dir_t dir, hal_s64_t * data_addr,
    int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_S64, dir, (void*)data_addr, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

/* this is a generic function that does the majority of the work. */

// The old API parameter style uses the 'data_addr' as the actual data storage
// location.
// The new API uses local data in the hal param structure and the data_addr is
// a reference to the data into the hal param structure (just like pins).
static int hal_param_new_anyapi(const char *name, hal_type_t type, hal_pdir_t dir, void *data_addr,
    int comp_id, bool newapi)
{
    rtapi_intptr_t *prev, next;
    int cmp;
    hal_param_t *new, *ptr;
    hal_comp_t *comp;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_new called before init\n");
	return -EFAULT;
    }

    if (type != HAL_BIT && type != HAL_FLOAT && type != HAL_S32 && type != HAL_U32 && type != HAL_S64 && type != HAL_U64) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin type not one of HAL_BIT, HAL_FLOAT, HAL_S32, HAL_U32, Hal_S64 or HAL_U64\n");
	return -EINVAL;
    }

    if(dir != HAL_RO && dir != HAL_RW) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param direction not one of HAL_RO, or HAL_RW\n");
	return -EINVAL;
    }

    if (strlen(name) > HAL_NAME_LEN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: parameter name '%s' is too long\n", name);
	return -EINVAL;
    }
    if (hal_data->lock & HAL_LOCK_LOAD)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_new called while HAL locked\n");
	return -EPERM;
    }

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating parameter '%s'\n", name);
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    /* validate comp_id */
    comp = halpr_find_comp_by_id(comp_id);
    if (comp == NULL) {
	/* bad comp_id */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }
    // Already check duplicate before allocating
    if(halpr_find_param_by_name(name)) {
        // Duplicate parameter name
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: duplicate parameter '%s'\n", name);
        return -EINVAL;
    }
    if(halpr_find_pin_by_name(name)) {
        // Overlapping pin/parameter name
        // This is a problem because setp does not distinguish and
        // cannot set pin or param when the names collide.
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: parameter '%s' also is the name of a pin\n", name);
        // We continue, as was done before...
    }
    /* validate passed in pointer - must point to HAL shmem */
    if (! SHMCHK(data_addr)) {
	/* bad pointer */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: data_addr not in shared memory\n");
	return -EINVAL;
    }
    if(comp->ready) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_new called after hal_ready\n");
	return -EINVAL;
    }
    /* allocate a new parameter structure */
    new = alloc_param_struct();
    if (new == NULL) {
	/* alloc failed */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for parameter '%s'\n", name);
	return -ENOMEM;
    }
    /* initialize the structure */
    new->owner_ptr = SHMOFF(comp);
    if (newapi) {
        // New API has the parameter value as part of the param structure
        new->data_ptr = SHMOFF(&(new->data));
        *(void **)data_addr = (char *)comp->shmem_base + SHMOFF(&(new->data));
    } else {
        // Old API has the parameter value as user supplied pointer
        new->data_ptr = SHMOFF(data_addr);
    }
    new->type = type;
    new->dir = dir;
    rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    halpr_mutex_release();
	    return 0;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    halpr_mutex_release();
	    return 0;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_param_struct(new);
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate parameter '%s'\n", name);
	    return -EINVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

// Old API interface
int hal_param_new(const char *name, hal_type_t type, hal_pdir_t dir, void *data_addr,
    int comp_id)
{
    return hal_param_new_anyapi(name, type, dir, data_addr, comp_id, 0);
}

// New API interface only used locally
static int hal_param_new_newapi(hal_type_t type, hal_pdir_t dir, void *data_addr,
    int comp_id, const char *fmt, va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        rtapi_print_msg(RTAPI_MSG_ERR,
	    "hal_param_new_newapi: length %d too long for name starting '%s'\n",
	    sz, name);
	return -ENOMEM;
    }
    return hal_param_new_anyapi(name, type, dir, data_addr, comp_id, 1);
}

// *** New interface ***

int hal_param_new_bool(int compid, hal_pdir_t dir, hal_bool_t *ref, rtapi_bool def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_param_new_newapi(HAL_BIT, dir, ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_bool(*ref, def);
    return 0;
}

int hal_param_new_si32(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_s32 def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_param_new_newapi(HAL_S32, dir, ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_si32(*ref, def);
    return 0;
}

int hal_param_new_ui32(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_u32 def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_param_new_newapi(HAL_U32, dir, ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_ui32(*ref, def);
    return 0;
}

int hal_param_new_sint(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_sint def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_param_new_newapi(HAL_S64, dir, ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_sint(*ref, def);
    return 0;
}

int hal_param_new_uint(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_uint def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_param_new_newapi(HAL_U64, dir, ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_uint(*ref, def);
    return 0;
}

int hal_param_new_real(int compid, hal_pdir_t dir, hal_real_t *ref, rtapi_real def, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = hal_param_new_newapi(HAL_FLOAT, dir, ref, compid, fmt, ap);
    va_end(ap);
    if(ret)
        return ret;
    hal_set_real(*ref, def);
    return 0;
}

// This is a special case for parameters, where you want them available, but
// not exposed. This is for example used in hostmot2.
// We ensure a valid target for the pointer destination but do not have a name
// attached. The code can then use whatever type read/write and run.
int hal_param_new_fake(int compid, hal_refs_u *ref)
{
    (void)compid;
    if(!ref)
        return -EINVAL;
    __hal_private_vals_u *ptr = hal_malloc(sizeof(*ptr));
    if(!ptr)
        return -ENOMEM;
    memset(ptr, 0, sizeof(*ptr));
    *(void **)ref = ptr;
    return 0;
}

/* wrapper functs for typed params - these call the generic funct below */

// We don't want our library to emit the deprecation warning.
// We already know it and need to provide them until removed.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
int hal_param_bit_set(const char *name, int value)
{
    return hal_param_set(name, HAL_BIT, &value);
}

int hal_param_float_set(const char *name, double value)
{
    return hal_param_set(name, HAL_FLOAT, &value);
}

int hal_param_u32_set(const char *name, unsigned long value)
{
    return hal_param_set(name, HAL_U32, &value);
}

int hal_param_s32_set(const char *name, signed long value)
{
    return hal_param_set(name, HAL_S32, &value);
}

int hal_param_u64_set(const char *name, unsigned long value)
{
    return hal_param_set(name, HAL_U64, &value);
}

int hal_param_s64_set(const char *name, signed long value)
{
    return hal_param_set(name, HAL_S64, &value);
}
#pragma GCC diagnostic pop

/* this is a generic function that does the majority of the work */

int hal_param_set(const char *name, hal_type_t type, void *value_addr)
{
    hal_param_t *param;
    void *d_ptr;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_set called before init\n");
	return -EFAULT;
    }
    
    if (hal_data->lock & HAL_LOCK_PARAMS)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_set called while HAL locked\n");
	return -EPERM;
    }
    
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: setting parameter '%s'\n", name);
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();

    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param == NULL) {
	/* parameter not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: parameter '%s' not found\n", name);
	return -EINVAL;
    }
    /* found it, is type compatible? */
    if (param->type != type) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: type mismatch setting param '%s'\n", name);
	return -EINVAL;
    }
    /* is it read only? */
    if (param->dir == HAL_RO) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param '%s' is not writable\n", name);
	return -EINVAL;
    }
    /* everything is OK, set the value */
    d_ptr = SHMPTR(param->data_ptr);
    switch (param->type) {
    case HAL_BIT:
	if (*((int *) value_addr) == 0) {
	    *(hal_bit_t *) (d_ptr) = 0;
	} else {
	    *(hal_bit_t *) (d_ptr) = 1;
	}
	break;
    case HAL_FLOAT:
	*((hal_float_t *) (d_ptr)) = *((double *) (value_addr));
	break;
    case HAL_S32:
	*((hal_s32_t *) (d_ptr)) = *((signed long *) (value_addr));
	break;
    case HAL_U32:
	*((hal_u32_t *) (d_ptr)) = *((unsigned long *) (value_addr));
	break;
    case HAL_S64:
	*((hal_s64_t *) (d_ptr)) = *((signed long *) (value_addr));
	break;
    case HAL_U64:
	*((hal_u64_t *) (d_ptr)) = *((unsigned long *) (value_addr));
	break;
    default:
	/* Shouldn't get here, but just in case... */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: bad type %d setting param\n", param->type);
	return -EINVAL;
    }
    halpr_mutex_release();
    return 0;
}

int hal_param_alias(const char *param_name, const char *alias)
{
    rtapi_intptr_t *prev, next;
    int cmp;
    hal_param_t *param, *ptr;
    hal_oldname_t *oldname;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_alias called before init\n");
	return -EFAULT;
    }
    if (hal_data->lock & HAL_LOCK_CONFIG)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_alias called while HAL locked\n");
	return -EPERM;
    }
    if (alias != NULL ) {
	if (strlen(alias) > HAL_NAME_LEN) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
	        "HAL: ERROR: alias name '%s' is too long\n", alias);
	    return -EINVAL;
	}
    }
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    if (alias != NULL ) {
	param = halpr_find_param_by_name(alias);
	if ( param != NULL ) {
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
	        "HAL: ERROR: duplicate pin/alias name '%s'\n", alias);
	    return -EINVAL;
	}
    }
    /* once we unlink the param from the list, we don't want to have to
       abort the change and repair things.  So we allocate an oldname
       struct here, then free it (which puts it on the free list).  This
       allocation might fail, in which case we abort the command.  But
       if we actually need the struct later, the next alloc is guaranteed
       to succeed since at least one struct is on the free list. */
    oldname = halpr_alloc_oldname_struct();
    if ( oldname == NULL ) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for param_alias\n");
	return -EINVAL;
    }
    free_oldname_struct(oldname);
    /* find the param and unlink it from pin list */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, not found */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: param '%s' not found\n", param_name);
	    return -EINVAL;
	}
	param = SHMPTR(next);
	if ( strcmp(param->name, param_name) == 0 ) {
	    /* found it, unlink from list */
	    *prev = param->next_ptr;
	    break;
	}
	if (param->oldname != 0 ) {
	    oldname = SHMPTR(param->oldname);
	    if (strcmp(oldname->name, param_name) == 0) {
		/* found it, unlink from list */
		*prev = param->next_ptr;
		break;
	    }
	}
	/* didn't find it yet, look at next one */
	prev = &(param->next_ptr);
	next = *prev;
    }
    if ( alias != NULL ) {
	/* adding a new alias */
	if ( param->oldname == 0 ) {
	    /* save old name (only if not already saved) */
	    oldname = halpr_alloc_oldname_struct();
	    param->oldname = SHMOFF(oldname);
	    rtapi_snprintf(oldname->name, sizeof(oldname->name), "%s", param->name);
	}
	/* change param's name to 'alias' */
	rtapi_snprintf(param->name, sizeof(param->name), "%s", alias);
    } else {
	/* removing an alias */
	if ( param->oldname != 0 ) {
	    /* restore old name (only if param is aliased) */
	    oldname = SHMPTR(param->oldname);
	    rtapi_snprintf(param->name, sizeof(param->name), "%s", oldname->name);
	    param->oldname = 0;
	    free_oldname_struct(oldname);
	}
    }
    /* insert param back into list in proper place */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    param->next_ptr = next;
	    *prev = SHMOFF(param);
	    halpr_mutex_release();
	    return 0;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, param->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    param->next_ptr = next;
	    *prev = SHMOFF(param);
	    halpr_mutex_release();
	    return 0;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

/***********************************************************************
*                 PIN/SIG/PARAM GETTER FUNCTIONS                       *
************************************************************************/

int hal_get_pin_value_by_name(
    const char *hal_name, hal_type_t *type, hal_data_u **data, bool *connected)
{
    hal_pin_t *pin;
    hal_sig_t *sig;
    if ((pin = halpr_find_pin_by_name(hal_name)) == NULL)
        return -1;

    if (connected != NULL)
        *connected = pin && pin->signal;
    *type = pin->type;
    if (pin->signal != 0) {
        sig = (hal_sig_t *) SHMPTR(pin->signal);
        *data = (hal_data_u *) SHMPTR(sig->data_ptr);
    } else {
        *data = (hal_data_u *) &(pin->dummysig);
    }
    return 0;
}

int hal_get_signal_value_by_name(
    const char *hal_name, hal_type_t *type, hal_data_u **data, bool *has_writers)
{
    hal_sig_t *sig;
    if ((sig = halpr_find_sig_by_name(hal_name)) == NULL)
        return -1;

    if (has_writers != NULL)
        *has_writers = !!sig->writers;
    *type = sig->type;
    *data = (hal_data_u *) SHMPTR(sig->data_ptr);
    return 0;
}

int hal_get_param_value_by_name(
    const char *hal_name, hal_type_t *type, hal_data_u **data)
{
    hal_param_t *param;
    if ((param = halpr_find_param_by_name(hal_name)) == NULL)
        return -1;

    *type = param->type;
    *data = (hal_data_u *) SHMPTR(param->data_ptr);
    return 0;
}


/***********************************************************************
*                   EXECUTION RELATED FUNCTIONS                        *
************************************************************************/

#ifdef RTAPI

static int hal_export_functfv(void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id, const char *fmt, va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        rtapi_print_msg(RTAPI_MSG_ERR,
	    "hal_export_functfv: length %d too long for name starting '%s'\n",
	    sz, name);
        return -ENOMEM;
    }
    return hal_export_funct(name, funct, arg, uses_fp, reentrant, comp_id);
}

int hal_export_functf(void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_export_functfv(funct, arg, uses_fp, reentrant, comp_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_export_funct(const char *name, void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id)
{
    (void)uses_fp;
    rtapi_intptr_t *prev, next;
    int cmp;
    hal_funct_t *new, *fptr;
    hal_comp_t *comp;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: export_funct called before init\n");
	return -EFAULT;
    }

    if (strlen(name) > HAL_NAME_LEN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function name '%s' is too long\n", name);
	return -EINVAL;
    }
    if (hal_data->lock & HAL_LOCK_LOAD)  {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: export_funct called while HAL locked\n");
	return -EPERM;
    }
    
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: exporting function '%s'\n", name);
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    /* validate comp_id */
    comp = halpr_find_comp_by_id(comp_id);
    if (comp == 0) {
	/* bad comp_id */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return -EINVAL;
    }
    if (comp->type == HAL_COMP_TYPE_USER) {
	/* not a realtime component */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d is not realtime\n", comp_id);
	return -EINVAL;
    }
    if(comp->ready) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: export_funct called after hal_ready\n");
	return -EINVAL;
    }
    /* allocate a new function structure */
    new = alloc_funct_struct();
    if (new == 0) {
	/* alloc failed */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for function '%s'\n", name);
	return -ENOMEM;
    }
    /* initialize the structure.
       uses_fp is deprecated and ignored; always report as FP-capable. */
    new->uses_fp = 1;
    new->owner_ptr = SHMOFF(comp);
    new->reentrant = reentrant;
    new->users = 0;
    new->arg = arg;
    new->funct = funct;
    rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->funct_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    /* break out of loop and init the new function */
	    break;
	}
	fptr = SHMPTR(next);
	cmp = strcmp(fptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    /* break out of loop and init the new function */
	    break;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_funct_struct(new);
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate function '%s'\n", name);
	    return -EINVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(fptr->next_ptr);
	next = *prev;
    }
    /* at this point we have a new function and can yield the mutex */
    halpr_mutex_release();

    /* create a pin with the function's runtime in it */
    if (hal_pin_new_si32(comp_id, HAL_OUT, &(new->runtime), 0, "%s.time", name) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: fail to create pin '%s.time'\n", name);
	return -EINVAL;
    }

    /* note that failure to successfully create the following params
       does not cause the "export_funct()" call to fail - they are
       for debugging and testing use only */
    /* create a parameter with the function's maximum runtime in it */
    if(hal_param_new_si32(comp_id, HAL_RW, &(new->maxtime), 0, "%s.tmax", name) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: fail to create param '%s.tmax'\n", name);
	return -EINVAL;
    }

    /* create a parameter with the function's maximum runtime in it */
    if(hal_param_new_bool(comp_id, HAL_RO, &(new->maxtime_increased), 0, "%s.tmax-increased", name) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: fail to create param '%s.tmax-increased'\n", name);
	return -EINVAL;
    }

    return 0;
}

int hal_create_thread(const char *name, unsigned long period_nsec, int uses_fp)
{
    (void)uses_fp;
    int next, cmp, prev_priority;
    int retval, n;
    hal_thread_t *new, *tptr;
    long prev_period, curr_period;
    char buf[HAL_NAME_LEN + 1];

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: creating thread %s, %ld nsec\n", name, period_nsec);
    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: create_thread called before init\n");
	return -EFAULT;
    }
    if (period_nsec == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: create_thread called with period of zero\n");
	return -EINVAL;
    }

    if (strlen(name) > HAL_NAME_LEN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: thread name '%s' is too long\n", name);
	return -EINVAL;
    }
    if (hal_data->lock & HAL_LOCK_CONFIG) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: create_thread called while HAL is locked\n");
	return -EPERM;
    }

    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    /* make sure name is unique on thread list */
    next = hal_data->thread_list_ptr;
    while (next != 0) {
	tptr = SHMPTR(next);
	cmp = strcmp(tptr->name, name);
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate thread name %s\n", name);
	    return -EEXIST;
	}
	/* didn't find it yet, look at next one */
	next = tptr->next_ptr;
    }
    /* allocate a new thread structure */
    new = alloc_thread_struct();
    if (new == 0) {
	/* alloc failed */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory to create thread\n");
	return -ENOMEM;
    }
    /* initialize the structure.
       uses_fp is deprecated and ignored; always enable FPU save/restore. */
    new->uses_fp = 1;
    rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
    /* have to create and start a task to run the thread */
    if (hal_data->thread_list_ptr == 0) {
	/* this is the first thread created */
	/* is timer started? if so, what period? */
	curr_period = rtapi_clock_set_period(0);
	if (curr_period == 0) {
	    /* not running, start it */
	    curr_period = rtapi_clock_set_period(period_nsec);
	    if (curr_period < 0) {
		halpr_mutex_release();
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL_LIB: ERROR: clock_set_period returned %ld\n",
		    curr_period);
		return -EINVAL;
	    }
	}
	/* make sure period <= desired period (allow 1% roundoff error) */
	if (curr_period > (long)(period_nsec + (period_nsec / 100))) {
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL_LIB: ERROR: clock period too long: %ld\n", curr_period);
	    return -EINVAL;
	}
	if(hal_data->exact_base_period) {
		hal_data->base_period = period_nsec;
	} else {
		hal_data->base_period = curr_period;
	}
	/* reserve the highest priority (maybe for a watchdog?) */
	prev_priority = rtapi_prio_highest();
	/* no previous period to worry about */
	prev_period = 0;
    } else {
	/* there are other threads, slowest (and lowest
	   priority) is at head of list */
	tptr = SHMPTR(hal_data->thread_list_ptr);
	prev_period = tptr->period;
	prev_priority = tptr->priority;
    }
    if ( (long)period_nsec < hal_data->base_period) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: ERROR: new thread period %ld is less than clock period %ld\n",
	     period_nsec, hal_data->base_period);
	return -EINVAL;
    }
    /* make period an integer multiple of the timer period */
    n = (period_nsec + hal_data->base_period / 2) / hal_data->base_period;
    new->period = hal_data->base_period * n;
    if ( new->period < prev_period ) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: ERROR: new thread period %ld is less than existing thread period %ld\n",
	     period_nsec, prev_period);
	return -EINVAL;
    }
    /* make priority one lower than previous */
    new->priority = rtapi_prio_next_lower(prev_priority);
    /* create task - owned by library module, not caller.
       Always pass 1 for uses_fp (deprecated, ignored by rtapi). */
    retval = rtapi_task_new(thread_task, new, new->priority,
	lib_module_id, HAL_STACKSIZE, 1);
    if (retval < 0) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: could not create task for thread %s\n", name);
	return -EINVAL;
    }
    new->task_id = retval;
    /* start task */
    retval = rtapi_task_start(new->task_id, new->period);
    if (retval < 0) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: could not start task for thread %s: %d\n", name, retval);
	return -EINVAL;
    }
    /* insert new structure at head of list */
    new->next_ptr = hal_data->thread_list_ptr;
    hal_data->thread_list_ptr = SHMOFF(new);

    // The counter that increases monotonically once every thread loop cycle.
    // This is a 'fast' counter with normal access semantics. Separated from
    // the param 'threadbeat' so it only needs to write _once_ to volatile
    // memory instead of a very expensive read-modify-write cycle.
    new->beatcnt = 0;

    /* done, release mutex */
    halpr_mutex_release();

    rtapi_snprintf(buf,sizeof(buf), HAL_PSEUDO_COMP_PREFIX"%s",new->name); // pseudo prefix
    new->comp_id = hal_init(buf);
    if (new->comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
           "HAL: ERROR: fail to create pseudo comp for thread: '%s'\n", new->name);
        return new->comp_id;
    }

    if ((retval = hal_param_new_si32(new->comp_id, HAL_RW, &(new->maxtime), 0, "%s.tmax", new->name)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
           "HAL: ERROR: fail to create param '%s.tmax'\n", new->name);
        return retval;
    }

    if ((retval = hal_pin_new_si32(new->comp_id, HAL_OUT, &(new->runtime), 0, "%s.time", new->name)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
           "HAL: ERROR: fail to create pin '%s.time'\n", new->name);
        return retval;
    }

    // A parameter that increases monotonically once every thread loop cycle.
    // This way we can detect when a RT-cycle was completed from non-RT and
    // prevent racy indeterministic sleep() constructs.
    if((retval = hal_param_new_sint(new->comp_id, HAL_RO, &(new->threadbeat), 0, "%s.threadbeat", name)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: fail to create param '%s.threadbeat'\n", name);
	return retval;
    }

    hal_ready(new->comp_id);

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: thread created\n");
    return new->comp_id;
}

extern int hal_thread_delete(const char *name)
{
    hal_thread_t *thread;
    rtapi_intptr_t *prev, next;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: thread_delete called before init\n");
	return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_CONFIG) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: thread_delete called while HAL is locked\n");
	return -EPERM;
    }
    
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: deleting thread '%s'\n", name);
    /* get mutex before accessing shared data */
    halpr_mutex_acquire();
    /* search for the signal */
    prev = &(hal_data->thread_list_ptr);
    next = *prev;
    while (next != 0) {
	thread = SHMPTR(next);
	if (strcmp(thread->name, name) == 0) {
	    /* this is the right thread, unlink from list */
	    if (thread->comp_id != 0) {
	        hal_exit(thread->comp_id);
	        thread->comp_id = 0;
	    }
	    *prev = thread->next_ptr;
	    /* and delete it */
	    free_thread_struct(thread);
	    /* done */
	    halpr_mutex_release();
	    return 0;
	}
	/* no match, try the next one */
	prev = &(thread->next_ptr);
	next = *prev;
    }
    /* if we get here, we didn't find a match */
    halpr_mutex_release();
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: thread '%s' not found\n",
	name);
    return -ENOENT;
}

#endif /* RTAPI */

int hal_add_funct_to_thread(const char *funct_name, const char *thread_name, int position)
{
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_list_t *list_root, *list_entry;
    int n;
    hal_funct_entry_t *funct_entry;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: add_funct called before init\n");
	return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_CONFIG) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: add_funct_to_thread called while HAL is locked\n");
	return -EPERM;
    }

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: adding function '%s' to thread '%s'\n",
	funct_name, thread_name);
    /* get mutex before accessing data structures */
    halpr_mutex_acquire();
    /* make sure position is valid */
    if (position == 0) {
	/* zero is not allowed */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: bad position: 0\n");
	return -EINVAL;
    }
    /* make sure we were given a function name */
    if (funct_name == NULL) {
	/* no name supplied */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing function name\n");
	return -EINVAL;
    }
    /* make sure we were given a thread name */
    if (thread_name == NULL) {
	/* no name supplied */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing thread name\n");
	return -EINVAL;
    }
    /* search function list for the function */
    funct = halpr_find_funct_by_name(funct_name);
    if (funct == NULL) {
	/* function not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' not found\n", funct_name);
	return -EINVAL;
    }
    /* found the function, is it available? */
    if ((funct->users > 0) && (funct->reentrant == 0)) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' may only be added to one thread\n", funct_name);
	return -EINVAL;
    }
    /* search thread list for thread_name */
    thread = halpr_find_thread_by_name(thread_name);
    if (thread == NULL) {
	/* thread not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: thread '%s' not found\n", thread_name);
	return -EINVAL;
    }
    /* uses_fp is deprecated and ignored; all threads are FP-capable.
       The FP compatibility check has been removed since all threads
       and functions now effectively have uses_fp=1. */
    /* find insertion point */
    list_root = &(thread->funct_list);
    list_entry = list_root;
    n = 0;
    if (position > 0) {
	/* insertion is relative to start of list */
	while (++n < position) {
	    /* move further into list */
	    list_entry = list_next(list_entry);
	    if (list_entry == list_root) {
		/* reached end of list */
		halpr_mutex_release();
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL: ERROR: position '%d' is too high\n", position);
		return -EINVAL;
	    }
	}
    } else {
	/* insertion is relative to end of list */
	while (--n > position) {
	    /* move further into list */
	    list_entry = list_prev(list_entry);
	    if (list_entry == list_root) {
		/* reached end of list */
		halpr_mutex_release();
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL: ERROR: position '%d' is too low\n", position);
		return -EINVAL;
	    }
	}
	/* want to insert before list_entry, so back up one more step */
	list_entry = list_prev(list_entry);
    }
    /* allocate a funct entry structure */
    funct_entry = alloc_funct_entry_struct();
    if (funct_entry == NULL) {
	/* alloc failed */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for thread->function link\n");
	return -ENOMEM;
    }
    /* init struct contents */
    funct_entry->funct_ptr = SHMOFF(funct);
    funct_entry->arg = funct->arg;
    funct_entry->funct = funct->funct;
    /* add the entry to the list */
    list_add_after((hal_list_t *) funct_entry, list_entry);
    /* update the function usage count */
    funct->users++;
    halpr_mutex_release();
    return 0;
}

int hal_init_funct_to_thread(const char *funct_name, const char *thread_name, int position)
{
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_list_t *list_root, *list_entry;
    int n;
    hal_funct_entry_t *funct_entry;

    if (hal_data == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: init_funct called before init\n");
        return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_CONFIG) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: init_funct_to_thread called while HAL is locked\n");
        return -EPERM;
    }

    if (position == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: bad position: 0\n");
        return -EINVAL;
    }

    if (funct_name == NULL || thread_name == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: missing function or thread name\n");
        return -EINVAL;
    }

    rtapi_print_msg(RTAPI_MSG_DBG,
        "HAL: adding init function '%s' to thread '%s'\n",
        funct_name, thread_name);

    halpr_mutex_acquire();

    funct = halpr_find_funct_by_name(funct_name);
    if (funct == NULL) {
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: function '%s' not found\n", funct_name);
        return -ENOENT;
    }

    thread = halpr_find_thread_by_name(thread_name);
    if (thread == NULL) {
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: thread '%s' not found\n", thread_name);
        return -ENOENT;
    }

    /* once the special init cycle has executed, further initf calls are a
       no-op so config order doesn't depend on whether start_threads has been
       issued. Surface this with -EALREADY so halcmd can warn loudly. */
    if (thread->init_done) {
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_WARN,
            "HAL: WARNING: thread '%s' init cycle already ran; '%s' will not be invoked\n",
            thread_name, funct_name);
        return -EALREADY;
    }

    /* find insertion point in init list (same semantics as
       hal_add_funct_to_thread: +N from head, -N from tail) */
    list_root = &(thread->init_funct_list);
    list_entry = list_root;
    n = 0;
    if (position > 0) {
        while (++n < position) {
            list_entry = list_next(list_entry);
            if (list_entry == list_root) {
                halpr_mutex_release();
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "HAL: ERROR: position '%d' is too high\n", position);
                return -EINVAL;
            }
        }
    } else {
        while (--n > position) {
            list_entry = list_prev(list_entry);
            if (list_entry == list_root) {
                halpr_mutex_release();
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "HAL: ERROR: position '%d' is too low\n", position);
                return -EINVAL;
            }
        }
        list_entry = list_prev(list_entry);
    }

    /* allow the same funct to be on funct_list and init_funct_list, and to be
       referenced multiple times in the init list itself (no users-cap check) */
    funct_entry = alloc_funct_entry_struct();
    if (funct_entry == NULL) {
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: insufficient memory for thread->init function link\n");
        return -ENOMEM;
    }
    funct_entry->funct_ptr = SHMOFF(funct);
    funct_entry->arg = funct->arg;
    funct_entry->funct = funct->funct;

    list_add_after((hal_list_t *) funct_entry, list_entry);

    funct->users++;

    halpr_mutex_release();
    return 0;
}

int hal_del_funct_from_thread(const char *funct_name, const char *thread_name)
{
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *funct_entry;

    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: del_funct called before init\n");
	return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_CONFIG) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: del_funct_from_thread called while HAL is locked\n");
	return -EPERM;
    }

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: removing function '%s' from thread '%s'\n",
	funct_name, thread_name);
    /* get mutex before accessing data structures */
    halpr_mutex_acquire();
    /* make sure we were given a function name */
    if (funct_name == NULL) {
	/* no name supplied */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing function name\n");
	return -EINVAL;
    }
    /* make sure we were given a thread name */
    if (thread_name == NULL) {
	/* no name supplied */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing thread name\n");
	return -EINVAL;
    }
    /* search function list for the function */
    funct = halpr_find_funct_by_name(funct_name);
    if (funct == NULL) {
	/* function not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' not found\n", funct_name);
	return -EINVAL;
    }
    /* found the function, is it in use? */
    if (funct->users == 0) {
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' is not in use\n", funct_name);
	return -EINVAL;
    }
    /* search thread list for thread_name */
    thread = halpr_find_thread_by_name(thread_name);
    if (thread == NULL) {
	/* thread not found */
	halpr_mutex_release();
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: thread '%s' not found\n", thread_name);
	return -EINVAL;
    }
    /* ok, we have thread and function, does thread use funct? */
    list_root = &(thread->funct_list);
    list_entry = list_next(list_root);
    while (1) {
	if (list_entry == list_root) {
	    /* reached end of list, funct not found */
	    halpr_mutex_release();
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: thread '%s' doesn't use %s\n", thread_name,
		funct_name);
	    return -EINVAL;
	}
	funct_entry = (hal_funct_entry_t *) list_entry;
	if (SHMPTR(funct_entry->funct_ptr) == funct) {
	    /* this funct entry points to our funct, unlink */
	    list_remove_entry(list_entry);
	    /* and delete it */
	    free_funct_entry_struct(funct_entry);
	    /* done */
	    halpr_mutex_release();
	    return 0;
	}
	/* try next one */
	list_entry = list_next(list_entry);
    }
}

int hal_start_threads(void)
{
    /* a trivial function for a change! */
    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: start_threads called before init\n");
	return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_RUN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: start_threads called while HAL is locked\n");
	return -EPERM;
    }


    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: starting threads\n");
    hal_data->threads_running = 1;
    return 0;
}

int hal_stop_threads(void)
{
    /* wow, two in a row! */
    if (hal_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: stop_threads called before init\n");
	return -EFAULT;
    }

    if (hal_data->lock & HAL_LOCK_RUN) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: stop_threads called while HAL is locked\n");
	return -EPERM;
    }

    hal_data->threads_running = 0;
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: threads stopped\n");
    return 0;
}

/***********************************************************************
*                    PRIVATE FUNCTION CODE                             *
************************************************************************/

hal_list_t *list_prev(hal_list_t * entry)
{
    /* this function is only needed because of memory mapping */
    return SHMPTR(entry->prev);
}

hal_list_t *list_next(hal_list_t * entry)
{
    /* this function is only needed because of memory mapping */
    return SHMPTR(entry->next);
}

void list_init_entry(hal_list_t * entry)
{
    int entry_n;

    entry_n = SHMOFF(entry);
    entry->next = entry_n;
    entry->prev = entry_n;
}

void list_add_after(hal_list_t * entry, hal_list_t * prev)
{
    int entry_n, prev_n, next_n;
    hal_list_t *next;

    /* messiness needed because of memory mapping */
    entry_n = SHMOFF(entry);
    prev_n = SHMOFF(prev);
    next_n = prev->next;
    next = SHMPTR(next_n);
    /* insert the entry */
    entry->next = next_n;
    entry->prev = prev_n;
    prev->next = entry_n;
    next->prev = entry_n;
}

void list_add_before(hal_list_t * entry, hal_list_t * next)
{
    int entry_n, prev_n, next_n;
    hal_list_t *prev;

    /* messiness needed because of memory mapping */
    entry_n = SHMOFF(entry);
    next_n = SHMOFF(next);
    prev_n = next->prev;
    prev = SHMPTR(prev_n);
    /* insert the entry */
    entry->next = next_n;
    entry->prev = prev_n;
    prev->next = entry_n;
    next->prev = entry_n;
}

hal_list_t *list_remove_entry(hal_list_t * entry)
{
    int entry_n;
    hal_list_t *prev, *next;

    /* messiness needed because of memory mapping */
    entry_n = SHMOFF(entry);
    prev = SHMPTR(entry->prev);
    next = SHMPTR(entry->next);
    /* remove the entry */
    prev->next = entry->next;
    next->prev = entry->prev;
    entry->next = entry_n;
    entry->prev = entry_n;
    return next;
}

hal_comp_t *halpr_find_comp_by_name(const char *name)
{
    int next;
    hal_comp_t *comp;

    /* search component list for 'name' */
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if (strcmp(comp->name, name) == 0) {
	    /* found a match */
	    return comp;
	}
	/* didn't find it yet, look at next one */
	next = comp->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return NULL;
}

hal_pin_t *halpr_find_pin_by_name(const char *name)
{
    int next;
    hal_pin_t *pin;
    hal_oldname_t *oldname;

    /* search pin list for 'name' */
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if (strcmp(pin->name, name) == 0) {
	    /* found a match */
	    return pin;
	}
	if (pin->oldname != 0 ) {
	    oldname = SHMPTR(pin->oldname);
	    if (strcmp(oldname->name, name) == 0) {
		/* found a match */
		return pin;
	    }
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return NULL;
}

hal_sig_t *halpr_find_sig_by_name(const char *name)
{
    int next;
    hal_sig_t *sig;

    /* search signal list for 'name' */
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if (strcmp(sig->name, name) == 0) {
	    /* found a match */
	    return sig;
	}
	/* didn't find it yet, look at next one */
	next = sig->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return NULL;
}

hal_param_t *halpr_find_param_by_name(const char *name)
{
    int next;
    hal_param_t *param;
    hal_oldname_t *oldname;

    /* search parameter list for 'name' */
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if (strcmp(param->name, name) == 0) {
	    /* found a match */
	    return param;
	}
	if (param->oldname != 0 ) {
	    oldname = SHMPTR(param->oldname);
	    if (strcmp(oldname->name, name) == 0) {
		/* found a match */
		return param;
	    }
	}
	/* didn't find it yet, look at next one */
	next = param->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return NULL;
}

hal_thread_t *halpr_find_thread_by_name(const char *name)
{
    int next;
    hal_thread_t *thread;

    /* search thread list for 'name' */
    next = hal_data->thread_list_ptr;
    while (next != 0) {
	thread = SHMPTR(next);
	if (strcmp(thread->name, name) == 0) {
	    /* found a match */
	    return thread;
	}
	/* didn't find it yet, look at next one */
	next = thread->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return NULL;
}

hal_funct_t *halpr_find_funct_by_name(const char *name)
{
    int next;
    hal_funct_t *funct;

    /* search function list for 'name' */
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	funct = SHMPTR(next);
	if (strcmp(funct->name, name) == 0) {
	    /* found a match */
	    return funct;
	}
	/* didn't find it yet, look at next one */
	next = funct->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return NULL;
}

hal_comp_t *halpr_find_comp_by_id(int id)
{
    int next;
    hal_comp_t *comp;

    /* search list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if (comp->comp_id == id) {
	    /* found a match */
	    return comp;
	}
	/* didn't find it yet, look at next one */
	next = comp->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return NULL;
}

hal_pin_t *halpr_find_pin_by_owner(hal_comp_t * owner, hal_pin_t * start)
{
    int owner_ptr, next;
    hal_pin_t *pin;

    /* get offset of 'owner' component */
    owner_ptr = SHMOFF(owner);
    /* is this the first call? */
    if (start == NULL) {
	/* yes, start at beginning of pin list */
	next = hal_data->pin_list_ptr;
    } else {
	/* no, start at next pin */
	next = start->next_ptr;
    }
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->owner_ptr == owner_ptr) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return NULL;
}

hal_param_t *halpr_find_param_by_owner(hal_comp_t * owner,
    hal_param_t * start)
{
    int owner_ptr, next;
    hal_param_t *param;

    /* get offset of 'owner' component */
    owner_ptr = SHMOFF(owner);
    /* is this the first call? */
    if (start == NULL) {
	/* yes, start at beginning of param list */
	next = hal_data->param_list_ptr;
    } else {
	/* no, start at next param */
	next = start->next_ptr;
    }
    while (next != 0) {
	param = SHMPTR(next);
	if (param->owner_ptr == owner_ptr) {
	    /* found a match */
	    return param;
	}
	/* didn't find it yet, look at next one */
	next = param->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return NULL;
}

hal_funct_t *halpr_find_funct_by_owner(hal_comp_t * owner,
    hal_funct_t * start)
{
    int owner_ptr, next;
    hal_funct_t *funct;

    /* get offset of 'owner' component */
    owner_ptr = SHMOFF(owner);
    /* is this the first call? */
    if (start == NULL) {
	/* yes, start at beginning of function list */
	next = hal_data->funct_list_ptr;
    } else {
	/* no, start at next function */
	next = start->next_ptr;
    }
    while (next != 0) {
	funct = SHMPTR(next);
	if (funct->owner_ptr == owner_ptr) {
	    /* found a match */
	    return funct;
	}
	/* didn't find it yet, look at next one */
	next = funct->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return NULL;
}

hal_pin_t *halpr_find_pin_by_sig(hal_sig_t * sig, hal_pin_t * start)
{
    int sig_ptr, next;
    hal_pin_t *pin;

    /* get offset of 'sig' component */
    sig_ptr = SHMOFF(sig);
    /* is this the first call? */
    if (start == NULL) {
	/* yes, start at beginning of pin list */
	next = hal_data->pin_list_ptr;
    } else {
	/* no, start at next pin */
	next = start->next_ptr;
    }
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->signal == sig_ptr) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return NULL;
}

/***********************************************************************
*                     LOCAL FUNCTION CODE                              *
************************************************************************/

#ifdef RTAPI
/* these functions are called when the hal_lib module is insmod'ed
   or rmmod'ed.
*/

#if defined(__KERNEL__) && defined( CONFIG_PROC_FS ) && LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#include <linux/proc_fs.h>
extern struct proc_dir_entry *rtapi_dir;
static struct proc_dir_entry *hal_dir = 0;
static struct proc_dir_entry *hal_newinst_file = 0;

static int proc_write_newinst(struct file *file,
        const char *buffer, unsigned long count, void *data)
{
    if(hal_data->pending_constructor) {
        rtapi_print_msg(RTAPI_MSG_DBG,
                "HAL: running constructor for %s %s\n",
                hal_data->constructor_prefix,
                hal_data->constructor_arg);
        hal_data->pending_constructor(hal_data->constructor_prefix,
                hal_data->constructor_arg);
        hal_data->pending_constructor = 0;
    }
    return count;
}

static void hal_proc_clean(void) {
    if(hal_newinst_file)
        remove_proc_entry("newinst", hal_dir);
    if(hal_dir)
        remove_proc_entry("hal", rtapi_dir);
    hal_newinst_file = hal_dir = 0;
}
static int hal_proc_init(void) {
    if(!rtapi_dir) return 0;
    hal_dir = create_proc_entry("hal", S_IFDIR, rtapi_dir);
    if(!hal_dir) { hal_proc_clean(); return -1; }
    hal_newinst_file = create_proc_entry("newinst", 0666, hal_dir);
    if(!hal_newinst_file) { hal_proc_clean(); return -1; }
    hal_newinst_file->data = NULL;
    hal_newinst_file->read_proc = NULL;
    hal_newinst_file->write_proc = proc_write_newinst;
    return 0;
}
#else
static int hal_proc_clean(void) { return 0; }
static int hal_proc_init(void) { return 0; }
#endif

int rtapi_app_main(void)
{
    int retval;
    void *mem;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL_LIB: loading kernel lib\n");
    /* do RTAPI init */
    lib_module_id = rtapi_init("HAL_LIB");
    if (lib_module_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL_LIB: ERROR: rtapi init failed\n");
	return -EINVAL;
    }
    /* get HAL shared memory block from RTAPI */
    lib_mem_id = rtapi_shmem_new(HAL_KEY, lib_module_id, HAL_SIZE);
    if (lib_mem_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: ERROR: could not open shared memory\n");
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }
    /* get address of shared memory area */
    retval = rtapi_shmem_getptr(lib_mem_id, &mem);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: ERROR: could not access shared memory\n");
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }
    /* set up internal pointers to shared mem and data structure */
    hal_shmem_base = (char *) mem;
    hal_data = (hal_data_t *) mem;
    /* perform a global init if needed */
    retval = init_hal_data();
    if ( retval ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: ERROR: could not init shared memory\n");
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }
    retval = hal_proc_init();
    if ( retval ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: ERROR: could not init /proc files\n");
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL_LIB: kernel lib installed successfully\n");

    hal_data->realtime_type = rtapi_get_realtime_type();

    return 0;
}

void rtapi_app_exit(void)
{
    hal_thread_t *thread;

    hal_data->realtime_type = REALTIME_TYPE_UNINITIALIZED;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL_LIB: removing kernel lib\n");
    hal_proc_clean();
    /* grab mutex before manipulating list */
    halpr_mutex_acquire();
    /* must remove all threads before unloading this module */
    while (hal_data->thread_list_ptr != 0) {
	/* point to a thread */
	thread = SHMPTR(hal_data->thread_list_ptr);
	/* unlink from list */
	hal_data->thread_list_ptr = thread->next_ptr;
	/* and delete it */
	free_thread_struct(thread);
    }
    /* release mutex */
    halpr_mutex_release();
    /* release RTAPI resources */
    rtapi_shmem_delete(lib_mem_id, lib_module_id);
    rtapi_exit(lib_module_id);
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL_LIB: kernel lib removed successfully\n");
}

/* this is the task function that implements threads in realtime */

static void thread_task(void *arg)
{
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_funct_entry_t *funct_root, *funct_entry;
    long long int start_time, end_time;
    long long int thread_start_time;

    thread = arg;
    while (1) {
        if (hal_data->threads_running > 0 && !thread->init_done) {
            /* special init cycle: run init_funct_list once with no
               timing measurement, re-anchor the period so the long
               init does not poison maxtime, does not trip the
               "unexpected realtime delay" catch-up loop, and lands
               the next wakeup at a clean period boundary (used to
               keep EtherCAT send clear of SYNC0). The cyclic
               funct_list is intentionally NOT executed in this
               cycle -- the next cycle is the first clean cyclic
               pass. After init_done is latched, drain the list back
               to the free pool: every entry has already run, the
               list serves no further purpose, and a halcmd 'initf'
               arriving later is rejected by the init_done check. */
            funct_root = (hal_funct_entry_t *) & (thread->init_funct_list);
            funct_entry = SHMPTR(funct_root->links.next);
            while (funct_entry != funct_root) {
                funct_entry->funct(funct_entry->arg, thread->period);
                funct_entry = SHMPTR(funct_entry->links.next);
            }
            rtapi_task_self_resync();
            thread->init_done = 1;
            /* drain the list now that the init pass is complete */
            funct_entry = SHMPTR(funct_root->links.next);
            while (funct_entry != funct_root) {
                hal_funct_entry_t *next_entry = SHMPTR(funct_entry->links.next);
                list_remove_entry((hal_list_t *) funct_entry);
                free_funct_entry_struct(funct_entry);
                funct_entry = next_entry;
            }
        } else if (hal_data->threads_running > 0) {
	    /* point at first function on function list */
	    funct_root = (hal_funct_entry_t *) & (thread->funct_list);
	    funct_entry = SHMPTR(funct_root->links.next);
	    /* execution time logging */
	    start_time = rtapi_get_time();
	    end_time = start_time;
	    thread_start_time = start_time;
	    /* run thru function list */
	    while (funct_entry != funct_root) {
		/* call the function */
		funct_entry->funct(funct_entry->arg, thread->period);
		/* capture execution time */
		end_time = rtapi_get_time();
		/* point to function structure */
		funct = SHMPTR(funct_entry->funct_ptr);
		/* update execution time data */
		rtapi_s32 runtime = hal_set_si32(funct->runtime, end_time - start_time);
		if ( runtime > hal_get_si32(funct->maxtime)) {
		    hal_set_si32(funct->maxtime, runtime);
		    hal_set_bool(funct->maxtime_increased, 1);
		} else {
		    hal_set_bool(funct->maxtime_increased, 0);
		}
		/* point to next next entry in list */
		funct_entry = SHMPTR(funct_entry->links.next);
		/* prepare to measure time for next funct */
		start_time = end_time;
	    }
	    /* update thread execution time */
	    rtapi_s32 runtime = hal_set_si32(thread->runtime, end_time - thread_start_time);
	    if ( runtime > hal_get_si32(thread->maxtime)) {
	        hal_set_si32(thread->maxtime, runtime);
	    }
            hal_set_sint(thread->threadbeat, ++thread->beatcnt);
	}
	/* wait until next period */
	rtapi_wait();
    }
}
#endif /* RTAPI */

/* see the declarations of these functions (near top of file) for
   a description of what they do.
*/

static int init_hal_data(void)
{
    /* has the hal_data block already been initialized? */

    // Lock hal_data by taking the mutex, so that two processes don't both try
    // to initialize hal_data at the same time.
    // NOTE: The first time through, the hal_data memory buffer is fresh from
    // rtapi_shmem_new(), which means it's initialized to all zero bytes.
    // The used mutex in halpr_mutex_acquire()/halpr_mutex_release() uses a
    // reversed default mutex, where zero means it is locked and non-zero when
    // unlocked. This is required for recursive mutex functionality where the
    // default is locked at init.
    halpr_mutex_acquire();

    if (hal_data->version != 0) {
        /* hal_data has been initialized already, verify version code */
        if (hal_data->version == HAL_VER) {
            halpr_mutex_release();
            return 0;
        } else {
            halpr_mutex_release();
            rtapi_print("HAL: version:%d expected:%d\n",hal_data->version,HAL_VER);
            rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: version code mismatch\n");
            return -EPROTO;
        }
    }

    /* hal_data has *NOT* been initialized yet, we get the honor */

    /* set version code so nobody else init's the block */
    hal_data->version = HAL_VER;
    /* initialize everything */
    hal_data->comp_list_ptr = 0;
    hal_data->pin_list_ptr = 0;
    hal_data->sig_list_ptr = 0;
    hal_data->param_list_ptr = 0;
    hal_data->funct_list_ptr = 0;
    hal_data->thread_list_ptr = 0;
    hal_data->base_period = 0;
    hal_data->threads_running = 0;
    hal_data->oldname_free_ptr = 0;
    hal_data->comp_free_ptr = 0;
    hal_data->pin_free_ptr = 0;
    hal_data->sig_free_ptr = 0;
    hal_data->param_free_ptr = 0;
    hal_data->funct_free_ptr = 0;
    hal_data->pending_constructor = NULL;
    hal_data->constructor_prefix[0] = 0;
    list_init_entry(&(hal_data->funct_entry_free));
    hal_data->thread_free_ptr = 0;
    hal_data->exact_base_period = 0;
    hal_data->realtime_type = REALTIME_TYPE_UNINITIALIZED;
    /* set up for shmalloc_xx() */
    hal_data->shmem_bot = sizeof(hal_data_t);
    hal_data->shmem_top = HAL_SIZE;
    hal_data->lock = HAL_LOCK_NONE;
    /* done, release mutex */
    halpr_mutex_release();
    return 0;
}

static void *shmalloc_up(long int size)
{
    long int tmp_bot;
    void *retval;

    if (size < 1) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: smalloc_up: attempt to allocate zero or less (size=%ld)\n", size);
        return NULL;
    }

    // We want to keep allocations on proper 64-bit alignment. The value
    // allocations are already sizeof(hal_data_u) and this will just enforce
    // the minimum size so we won't see any memory corruptions.
    if (size < 8)
        size = 8;

    /* deal with alignment requirements */
    tmp_bot = hal_data->shmem_bot;
    if (size >= 16) {
	/* align on 16 byte boundary */
	tmp_bot = (tmp_bot + 15) & (~15);
    } else {
	/* align on 8 byte boundary */
	tmp_bot = (tmp_bot + 7) & (~7);
    }

    /* is there enough memory available? */
    if ((hal_data->shmem_top - tmp_bot) < size) {
	/* no */
	return NULL;
    }
    /* memory is available, allocate it */
    retval = SHMPTR(tmp_bot);
    hal_data->shmem_bot = tmp_bot + size;
    hal_data->shmem_avail = hal_data->shmem_top - hal_data->shmem_bot;
    rtapi_print_msg(RTAPI_MSG_DBG, "smalloc_up: shmem available %d\n", hal_data->shmem_avail);
    return retval;
}

static void *shmalloc_dn(long int size)
{
    long int tmp_top;
    void *retval;

    if (size < 1) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: smalloc_dn: attempt to allocate zero or less (size=%ld)\n", size);
        return NULL;
    }

    // We want to keep allocations on proper 64-bit alignment. The value
    // allocations are already sizeof(hal_data_u) and this will just enforce
    // the minimum size so we won't see any memory corruptions.
    if (size < 8)
        size = 8;

    /* tentatively allocate memory */
    tmp_top = hal_data->shmem_top - size;
    /* deal with alignment requirements */
    if (size >= 16) {
	/* align on 16 byte boundary */
	tmp_top &= (~15);
    } else {
	/* align on 8 byte boundary */
	tmp_top &= (~7);
    }

    /* is there enough memory available? */
    if (tmp_top < hal_data->shmem_bot) {
	/* no */
	return NULL;
    }
    /* memory is available, allocate it */
    retval = SHMPTR(tmp_top);
    hal_data->shmem_top = tmp_top;
    hal_data->shmem_avail = hal_data->shmem_top - hal_data->shmem_bot;
    rtapi_print_msg(RTAPI_MSG_DBG, "smalloc_dn: shmem available %d\n", hal_data->shmem_avail);
    return retval;
}

hal_comp_t *halpr_alloc_comp_struct(void)
{
    hal_comp_t *p;

    /* check the free list */
    if (hal_data->comp_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->comp_free_ptr);
	/* unlink it from the free list */
	hal_data->comp_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_comp_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->comp_id = 0;
	p->mem_id = 0;
	p->type = HAL_COMP_TYPE_USER;
	p->shmem_base = NULL;
	p->name[0] = '\0';
    }
    return p;
}

static hal_pin_t *alloc_pin_struct(void)
{
    hal_pin_t *p;

    /* check the free list */
    if (hal_data->pin_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->pin_free_ptr);
	/* unlink it from the free list */
	hal_data->pin_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_pin_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr_addr = 0;
	p->owner_ptr = 0;
	p->type = 0;
	p->dir = 0;
	p->signal = 0;
	memset(&p->dummysig, 0, sizeof(hal_data_u));
	p->name[0] = '\0';
    }
    return p;
}

static hal_sig_t *alloc_sig_struct(void)
{
    hal_sig_t *p;

    /* check the free list */
    if (hal_data->sig_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->sig_free_ptr);
	/* unlink it from the free list */
	hal_data->sig_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_sig_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr = 0;
	p->type = 0;
	p->readers = 0;
	p->writers = 0;
	p->bidirs = 0;
	p->name[0] = '\0';
    }
    return p;
}

static hal_param_t *alloc_param_struct(void)
{
    hal_param_t *p;

    /* check the free list */
    if (hal_data->param_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->param_free_ptr);
	/* unlink it from the free list */
	hal_data->param_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_param_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr = 0;
	p->owner_ptr = 0;
	p->type = 0;
	p->name[0] = '\0';
    }
    return p;
}

static hal_oldname_t *halpr_alloc_oldname_struct(void)
{
    hal_oldname_t *p;

    /* check the free list */
    if (hal_data->oldname_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->oldname_free_ptr);
	/* unlink it from the free list */
	hal_data->oldname_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_oldname_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->name[0] = '\0';
    }
    return p;
}

#ifdef RTAPI
static hal_funct_t *alloc_funct_struct(void)
{
    hal_funct_t *p;

    /* check the free list */
    if (hal_data->funct_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->funct_free_ptr);
	/* unlink it from the free list */
	hal_data->funct_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_funct_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->uses_fp = 0;
	p->owner_ptr = 0;
	p->reentrant = 0;
	p->users = 0;
	p->arg = 0;
	p->funct = 0;
	p->name[0] = '\0';
    }
    return p;
}
#endif /* RTAPI */

static hal_funct_entry_t *alloc_funct_entry_struct(void)
{
    hal_list_t *freelist, *l;
    hal_funct_entry_t *p;

    /* check the free list */
    freelist = &(hal_data->funct_entry_free);
    l = list_next(freelist);
    if (l != freelist) {
	/* found a free structure, unlink from the free list */
	list_remove_entry(l);
	p = (hal_funct_entry_t *) l;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_funct_entry_t));
	l = (hal_list_t *) p;
	list_init_entry(l);
    }
    if (p) {
	/* make sure it's empty */
	p->funct_ptr = 0;
	p->arg = NULL;
	p->funct = NULL;
    }
    return p;
}

#ifdef RTAPI
static hal_thread_t *alloc_thread_struct(void)
{
    hal_thread_t *p;

    /* check the free list */
    if (hal_data->thread_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->thread_free_ptr);
	/* unlink it from the free list */
	hal_data->thread_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_thread_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->uses_fp = 0;
	p->period = 0;
	p->priority = 0;
	p->task_id = 0;
	list_init_entry(&(p->funct_list));
	list_init_entry(&(p->init_funct_list));
	p->init_done = 0;
	p->name[0] = '\0';
    }
    return p;
}
#endif /* RTAPI */

static void free_comp_struct(hal_comp_t * comp)
{
    rtapi_intptr_t *prev, next;
#ifdef RTAPI
    hal_funct_t *funct;
#endif /* RTAPI */
    hal_pin_t *pin;
    hal_param_t *param;

    /* can't delete the component until we delete its "stuff" */
    /* need to check for functs only if a realtime component */
#ifdef RTAPI
    /* search the function list for this component's functs */
    prev = &(hal_data->funct_list_ptr);
    next = *prev;
    while (next != 0) {
	funct = SHMPTR(next);
	if (SHMPTR(funct->owner_ptr) == comp) {
	    /* this function belongs to our component, unlink from list */
	    *prev = funct->next_ptr;
	    /* and delete it */
	    free_funct_struct(funct);
	} else {
	    /* no match, try the next one */
	    prev = &(funct->next_ptr);
	}
	next = *prev;
    }
#endif /* RTAPI */
    /* search the pin list for this component's pins */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (next != 0) {
	pin = SHMPTR(next);
	if (SHMPTR(pin->owner_ptr) == comp) {
	    /* this pin belongs to our component, unlink from list */
	    *prev = pin->next_ptr;
	    /* and delete it */
	    free_pin_struct(pin);
	} else {
	    /* no match, try the next one */
	    prev = &(pin->next_ptr);
	}
	next = *prev;
    }
    /* search the parameter list for this component's parameters */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (next != 0) {
	param = SHMPTR(next);
	if (SHMPTR(param->owner_ptr) == comp) {
	    /* this param belongs to our component, unlink from list */
	    *prev = param->next_ptr;
	    /* and delete it */
	    free_param_struct(param);
	} else {
	    /* no match, try the next one */
	    prev = &(param->next_ptr);
	}
	next = *prev;
    }
    /* now we can delete the component itself */
    /* clear contents of struct */
    comp->comp_id = 0;
    comp->mem_id = 0;
    comp->type = HAL_COMP_TYPE_USER;
    comp->shmem_base = NULL;
    comp->name[0] = '\0';
    /* add it to free list */
    comp->next_ptr = hal_data->comp_free_ptr;
    hal_data->comp_free_ptr = SHMOFF(comp);
}

static void unlink_pin(hal_pin_t * pin)
{
    hal_sig_t *sig;
    hal_comp_t *comp;
    void **data_ptr_addr;
    hal_data_u *dummy_addr, *sig_data_addr;

    /* is this pin linked to a signal? */
    if (pin->signal != 0) {
    /* yes, need to unlink it */
    sig = SHMPTR(pin->signal);
    /* make pin's 'data_ptr' point to its dummy signal */
    data_ptr_addr = SHMPTR(pin->data_ptr_addr);
    comp = SHMPTR(pin->owner_ptr);
    dummy_addr = (void *)((char *)comp->shmem_base + SHMOFF(&(pin->dummysig)));
    *data_ptr_addr = dummy_addr;

    /* copy current signal value to dummy */
    sig_data_addr = (hal_data_u *)(hal_shmem_base + sig->data_ptr);
    dummy_addr = (hal_data_u *)(hal_shmem_base + SHMOFF(&(pin->dummysig)));

    switch (pin->type) {
    case HAL_BOOL:
        dummy_addr->b = sig_data_addr->b;
        break;
    case HAL_S32:
        dummy_addr->s = sig_data_addr->s;
        break;
    case HAL_U32:
        dummy_addr->u = sig_data_addr->u;
        break;
    case HAL_SINT:
        dummy_addr->s = sig_data_addr->s;
        break;
    case HAL_UINT:
        dummy_addr->u = sig_data_addr->u;
        break;
    case HAL_REAL:
        dummy_addr->f = sig_data_addr->f;
        break;
    case HAL_PORT:
	/*once a pin is unlinked from its signal, it gets set to the empty_port*/
        dummy_addr->p = 0;
        break;
    default:
        rtapi_print_msg(RTAPI_MSG_ERR,
              "HAL: BUG: pin '%s' has invalid type %d !!\n",
              pin->name, pin->type);
    }

    /* update the signal's reader/writer counts */
    if ((pin->dir & HAL_IN) != 0) {
        sig->readers--;
    }
    if (pin->dir == HAL_OUT) {
        sig->writers--;
    }
    if (pin->dir == HAL_IO) {
        sig->bidirs--;
    }
    /* mark pin as unlinked */
    pin->signal = 0;
    }
}

static void free_pin_struct(hal_pin_t * pin)
{
    unlink_pin(pin);
    /* clear contents of struct */
    if ( pin->oldname != 0 ) {
        free_oldname_struct(SHMPTR(pin->oldname));
        pin->oldname = 0;
    }
    pin->data_ptr_addr = 0;
    pin->owner_ptr = 0;
    pin->type = 0;
    pin->dir = 0;
    pin->signal = 0;
    memset(&pin->dummysig, 0, sizeof(hal_data_u));
    pin->name[0] = '\0';
    /* add it to free list */
    pin->next_ptr = hal_data->pin_free_ptr;
    hal_data->pin_free_ptr = SHMOFF(pin);
}

static void free_sig_struct(hal_sig_t * sig)
{
    hal_pin_t *pin;

    /* look for pins linked to this signal */
    pin = halpr_find_pin_by_sig(sig, NULL);
    while (pin != NULL) {
	/* found one, unlink it */
	unlink_pin(pin);
	/* check for another pin linked to the signal */
	pin = halpr_find_pin_by_sig(sig, pin);
    }
    /* clear contents of struct */
    sig->data_ptr = 0;
    sig->type = 0;
    sig->readers = 0;
    sig->writers = 0;
    sig->bidirs = 0;
    sig->name[0] = '\0';
    /* add it to free list */
    sig->next_ptr = hal_data->sig_free_ptr;
    hal_data->sig_free_ptr = SHMOFF(sig);
}

static void free_param_struct(hal_param_t * p)
{
    /* clear contents of struct */
    if ( p->oldname != 0 ) {
        free_oldname_struct(SHMPTR(p->oldname));
        p->oldname = 0;
    }
    p->data_ptr = 0;
    p->owner_ptr = 0;
    p->type = 0;
    p->dir = 0;
    p->reserved = 0;
    memset(&p->data, 0, sizeof(p->data));
    p->name[0] = '\0';
    /* add it to free list (params use the same struct as src vars) */
    p->next_ptr = hal_data->param_free_ptr;
    hal_data->param_free_ptr = SHMOFF(p);
}

static void free_oldname_struct(hal_oldname_t * oldname)
{
    /* clear contents of struct */
    oldname->name[0] = '\0';
    /* add it to free list */
    oldname->next_ptr = hal_data->oldname_free_ptr;
    hal_data->oldname_free_ptr = SHMOFF(oldname);
}

#ifdef RTAPI
static void free_funct_struct(hal_funct_t * funct)
{
    int next_thread;
    hal_thread_t *thread;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *funct_entry;

/*  int next_thread, next_entry;*/

    if (funct->users > 0) {
	/* We can't casually delete the function, there are thread(s) which
	   will call it.  So we must check all the threads and remove any
	   funct_entrys that call this function */
	/* start at root of thread list */
	next_thread = hal_data->thread_list_ptr;
	/* run through thread list */
	while (next_thread != 0) {
	    /* point to thread */
	    thread = SHMPTR(next_thread);
	    /* start at root of funct_entry list */
	    list_root = &(thread->funct_list);
	    list_entry = list_next(list_root);
	    /* run thru funct_entry list */
	    while (list_entry != list_root) {
		/* point to funct entry */
		funct_entry = (hal_funct_entry_t *) list_entry;
		/* test it */
		if (SHMPTR(funct_entry->funct_ptr) == funct) {
		    /* this funct entry points to our funct, unlink */
		    list_entry = list_remove_entry(list_entry);
		    /* and delete it */
		    free_funct_entry_struct(funct_entry);
		} else {
		    /* no match, try the next one */
		    list_entry = list_next(list_entry);
		}
	    }
	    /* also sweep the init function list so unloaded comps don't
	       leave dangling init entries that would crash on the next
	       start_threads */
	    list_root = &(thread->init_funct_list);
	    list_entry = list_next(list_root);
	    while (list_entry != list_root) {
		funct_entry = (hal_funct_entry_t *) list_entry;
		if (SHMPTR(funct_entry->funct_ptr) == funct) {
		    list_entry = list_remove_entry(list_entry);
		    free_funct_entry_struct(funct_entry);
		} else {
		    list_entry = list_next(list_entry);
		}
	    }
	    /* move on to the next thread */
	    next_thread = thread->next_ptr;
	}
    }
    /* clear contents of struct */
    funct->uses_fp = 0;
    funct->owner_ptr = 0;
    funct->reentrant = 0;
    funct->users = 0;
    funct->arg = 0;
    funct->funct = 0;
    funct->runtime = 0;
    funct->name[0] = '\0';
    /* add it to free list */
    funct->next_ptr = hal_data->funct_free_ptr;
    hal_data->funct_free_ptr = SHMOFF(funct);
}
#endif /* RTAPI */

static void free_funct_entry_struct(hal_funct_entry_t * funct_entry)
{
    hal_funct_t *funct;

    if (funct_entry->funct_ptr > 0) {
	/* entry points to a function, update the function struct */
	funct = SHMPTR(funct_entry->funct_ptr);
	funct->users--;
    }
    /* clear contents of struct */
    funct_entry->funct_ptr = 0;
    funct_entry->arg = NULL;
    funct_entry->funct = NULL;
    /* add it to free list */
    list_add_after((hal_list_t *) funct_entry, &(hal_data->funct_entry_free));
}

#ifdef RTAPI
static void free_thread_struct(hal_thread_t * thread)
{
    hal_funct_entry_t *funct_entry;
    hal_list_t *list_root, *list_entry;
/*! \todo Another #if 0 */
#if 0
    rtapi_intptr_t *prev, next;
    char time[HAL_NAME_LEN + 1], tmax[HAL_NAME_LEN + 1];
    hal_param_t *param;
#endif

    /* if we're deleting a thread, we need to stop all threads */
    hal_data->threads_running = 0;
    /* and stop the task associated with this thread */
    rtapi_task_pause(thread->task_id);
    rtapi_task_delete(thread->task_id);
    /* clear contents of struct */
    thread->uses_fp = 0;
    thread->period = 0;
    thread->priority = 0;
    thread->task_id = 0;
    /* clear the function entry list */
    list_root = &(thread->funct_list);
    list_entry = list_next(list_root);
    while (list_entry != list_root) {
	/* entry found, save pointer to it */
	funct_entry = (hal_funct_entry_t *) list_entry;
	/* unlink it, point to the next one */
	list_entry = list_remove_entry(list_entry);
	/* free the removed entry */
	free_funct_entry_struct(funct_entry);
    }
/*! \todo Another #if 0 */
#if 0
/* Currently these don't get created, so we don't have to worry
   about deleting them.  They will come back when the HAL refactor
   is done, at that time this code or something like it will be
   needed.
*/
    /* need to delete <thread>.time and <thread>.tmax params */
    rtapi_snprintf(time, sizeof(time), "%s.time", thread->name);
    rtapi_snprintf(tmax, sizeof(tmax), "%s.tmax", thread->name);
    /* search the parameter list for those parameters */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (next != 0) {
	param = SHMPTR(next);
	/* does this param match either name? */
	if ((strcmp(param->name, time) == 0)
	    || (strcmp(param->name, tmax) == 0)) {
	    /* yes, unlink from list */
	    *prev = param->next_ptr;
	    /* and delete it */
	    free_param_struct(param);
	} else {
	    /* no match, try the next one */
	    prev = &(param->next_ptr);
	}
	next = *prev;
    }
#endif
    thread->name[0] = '\0';
    /* add thread to free list */
    thread->next_ptr = hal_data->thread_free_ptr;
    hal_data->thread_free_ptr = SHMOFF(thread);
}
#endif /* RTAPI */


/******************************************************************************
HAL PORT functions
******************************************************************************/

#define HAL_PORT_MAGIC_NUM 0x68707274  // Ascii coded 'hprt'
// This struct is purely local to this file to keep track of the
// port data in shmem.
typedef struct __hal_port_shm_t {
    rtapi_u32 magic;   // Magic marker (to detect *hal_port_t value tampering)
    atomic_uint read;  // offset into buff that outgoing data gets read from
    atomic_uint write; // offset into buff that incoming data gets written to
    unsigned int size; // size of allocated buffer
    char buff[];
} hal_port_shm_t;

static inline void hal_port_atomic_load(hal_port_shm_t* port_shm, unsigned* read, unsigned* write)
{
    *read = atomic_load_explicit(&port_shm->read, memory_order_acquire);
    *write = atomic_load_explicit(&port_shm->write, memory_order_acquire);
}


static inline void hal_port_atomic_store_read(hal_port_shm_t* port_shm, unsigned read)
{
    atomic_store_explicit(&port_shm->read, read, memory_order_release);
}


static inline void hal_port_atomic_store_write(hal_port_shm_t* port_shm, unsigned write)
{
    atomic_store_explicit(&port_shm->write, write, memory_order_release);
}


static unsigned hal_port_bytes_readable(unsigned read, unsigned write, unsigned size) {
    if(size == 0) {
        return 0;
    } else if (read <= write) {
        return write - read;
    } else {
        return size - read + write;
    }
}


static unsigned hal_port_bytes_writable(unsigned read, unsigned write, unsigned size) {
    if(size == 0) {
        return 0;
    } else if(write < read) {
        return read - write - 1;
    } else {
        return size - write + read - 1;
    }

}


static bool hal_port_compute_copy(unsigned read,
                                  unsigned write,
                                  unsigned size,
                                  unsigned count, 
                                  unsigned* end_bytes_to_read, 
                                  unsigned* beg_bytes_to_read, 
                                  unsigned* final_pos)
{
    unsigned  bytes_avail,
              end_bytes_avail;

    bytes_avail = hal_port_bytes_readable(read, write, size); 

    if(count > bytes_avail) {
        return false;
    } else if(read <= write) {
        //can read up to write position (no wraparound condition in buffer)
        *end_bytes_to_read = count;
        *beg_bytes_to_read = 0;
        *final_pos = read + count;
    } else {
        end_bytes_avail = size - read;

        if(count < end_bytes_avail) {
            //still only reading from end of buffer
            *end_bytes_to_read = count;
            *beg_bytes_to_read = 0;
            *final_pos = read + count;
        } else {
            //read porition of buffer with wrap around to beginning of buffer
            *end_bytes_to_read = end_bytes_avail;
            *beg_bytes_to_read = count - end_bytes_avail;
            *final_pos = *beg_bytes_to_read;
        }
    }
    
   return true;
}


// hal_port_alloc() is DEPRECATED
// Access to port allocation is done by hal_set_s() on the signal that has the
// port pins connected.
int hal_port_alloc(unsigned size, hal_port_t *port)
{
    return halpr_port_alloc(size, port);
}

int halpr_port_alloc(unsigned size, hal_port_t *port) {
    if(!port || size < 1 || size > HAL_PORT_SIZE_MAX)
        return -EINVAL;

    hal_port_shm_t* new_port = shmalloc_up(sizeof(hal_port_shm_t) + size);

    if(!new_port)
        return -ENOMEM;

    new_port->magic = HAL_PORT_MAGIC_NUM;
    new_port->read  = 0;
    new_port->write = 0;
    new_port->size  = size;

    *port = SHMOFF(new_port);
    return 0;
}

static inline hal_port_shm_t *hal_port_to_shm(const hal_port_t *port)
{
    hal_port_shm_t *shm;
    if(!port)
        return NULL;
    long ofs = *port;
    // The offset must be positive non-zero
    // The offset must be modulo the magic field size (4 bytes)
    // The offset must not point beyond shared memory for the control structure
    if(ofs <= 0 || 0 != (ofs & (long)(sizeof(shm->magic)-1)) || ofs > (long)(HAL_SIZE - sizeof(hal_port_shm_t)))
        return NULL;
    shm = SHMPTR(ofs);
    // The magic number must match and size be valid
    if (shm->magic != HAL_PORT_MAGIC_NUM || shm->size > HAL_PORT_SIZE_MAX)
        return NULL;
    // The ofset must not point beyond shared memory considering the size
    if (ofs > (long)(HAL_SIZE - (sizeof(hal_port_shm_t) + shm->size)))
        return NULL;
    return shm;
}

bool hal_port_read(const hal_port_t *port, char* dest, unsigned count) {
    unsigned read,
             write,
             end_bytes_to_read,   //number of bytes to read after read position and before end of buffer
             beg_bytes_to_read,   //number of bytes to read at beginning of buffer
             final_pos;           //final position after read

    if(!count) {
        return false;
    } else {
        hal_port_shm_t* port_shm = hal_port_to_shm(port);
        if(!port_shm) {
            rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_read: invalid port %p\n", port);
            return false;
        }
        hal_port_atomic_load(port_shm, &read, &write);

        if(hal_port_compute_copy(read, 
                                 write, 
                                 port_shm->size, 
                                 count, 
                                 &end_bytes_to_read, 
                                 &beg_bytes_to_read, 
                                 &final_pos)) {

            memcpy(dest, port_shm->buff + read, end_bytes_to_read);
            memcpy(dest+end_bytes_to_read, port_shm->buff, beg_bytes_to_read);
            hal_port_atomic_store_read(port_shm, final_pos);
            return true;
        } else {
            return false;
        }
    }
}   


bool hal_port_peek(const hal_port_t *port, char* dest, unsigned count) {
    unsigned read,
             write,
             end_bytes_to_read,   //number of bytes to read after read position and before end of buffer
             beg_bytes_to_read,   //number of bytes to read at beginning of buffer
             final_pos;           //final position of read

    if(!count) {
        return false;
    } else {
        hal_port_shm_t* port_shm = hal_port_to_shm(port);
        if(!port_shm) {
            rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_peek: invalid port %p\n", port);
            return false;
        }
        hal_port_atomic_load(port_shm, &read, &write);

        if(hal_port_compute_copy(read, 
                                 write, 
                                 port_shm->size, 
                                 count, 
                                 &end_bytes_to_read, 
                                 &beg_bytes_to_read, 
                                 &final_pos)) {

            memcpy(dest, port_shm->buff + read, end_bytes_to_read);
            memcpy(dest+end_bytes_to_read, port_shm->buff, beg_bytes_to_read);
            return true;
        } else {
            return false;
        }      
    }
}


bool hal_port_peek_commit(const hal_port_t *port, unsigned count) {
    unsigned read,
             write,
             end_bytes_to_read,   //number of bytes to read after read position and before end of buffer
             beg_bytes_to_read,   //number of bytes to read at beginning of buffer
             final_pos;           //final position of read

    if(!count) {
        return false;
    } else {
        hal_port_shm_t* port_shm = hal_port_to_shm(port);
        if(!port_shm) {
            rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_peek_commit: invalid port %p\n", port);
            return false;
        }
        hal_port_atomic_load(port_shm, &read, &write);

        if(hal_port_compute_copy(read,
                                 write,
                                 port_shm->size,
                                 count,
                                 &end_bytes_to_read,
                                 &beg_bytes_to_read,
                                 &final_pos)) {

            hal_port_atomic_store_read(port_shm, final_pos);
            return true;
        } else {
            return false;
        }
    }
}


bool hal_port_write(const hal_port_t *port, const char* src, unsigned count) {
    unsigned read,
             write,
             bytes_avail,
             end_bytes_avail,
             end_bytes_to_write,
             beg_bytes_to_write,
             final_pos;
 
    if(!count) {
        return false;
    } else {
        hal_port_shm_t* port_shm = hal_port_to_shm(port);
        if(!port_shm) {
            rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_write: invalid port %p\n", port);
            return false;
        }
       hal_port_atomic_load(port_shm, &read, &write);
       bytes_avail = hal_port_bytes_writable(read, write, port_shm->size);

        if(count > bytes_avail) {
            return false;
        } else {
            if (write < read) {
                //we can write up to one byte behind read
                end_bytes_to_write = count;
                beg_bytes_to_write = 0;
                final_pos = write + count;
            } else {
                if(read == 0) {
                    end_bytes_avail = bytes_avail;
                } else {
                    end_bytes_avail = port_shm->size - write;
                }

                if (count < end_bytes_avail) {
                    end_bytes_to_write = count;
                    beg_bytes_to_write = 0;
                    final_pos = write + count;
                } else {
                    end_bytes_to_write = end_bytes_avail;
                    beg_bytes_to_write = count - end_bytes_to_write;
                    final_pos = beg_bytes_to_write;
                }
            }
     
            memcpy(port_shm->buff+write, src, end_bytes_to_write);
            memcpy(port_shm->buff, src+end_bytes_to_write, beg_bytes_to_write);

            hal_port_atomic_store_write(port_shm, final_pos);
            return true;
        }
    }
}


unsigned hal_port_readable(const hal_port_t *port) {
    hal_port_shm_t* port_shm = hal_port_to_shm(port);
    if(!port_shm) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_readable: invalid port %p\n", port);
        return 0;
    }
    return hal_port_bytes_readable(port_shm->read, port_shm->write, port_shm->size);
}


unsigned hal_port_writable(const hal_port_t *port) {
    hal_port_shm_t* port_shm = hal_port_to_shm(port);
    if(!port_shm) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_writable: invalid port %p\n", port);
        return 0;
    }
    return hal_port_bytes_writable(port_shm->read, port_shm->write, port_shm->size);
}


unsigned hal_port_buffer_size(const hal_port_t *port) {
    hal_port_shm_t* port_shm = hal_port_to_shm(port);
    if(!port_shm) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_buffer_size: invalid port %p\n", port);
        return 0;
    }
    return port_shm->size;
}


void hal_port_clear(const hal_port_t *port) {
    unsigned read,write;
    hal_port_shm_t* port_shm = hal_port_to_shm(port);
    if(!port_shm) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_port_clear: invalid port address %p\n", port);
        return;
    }
    hal_port_atomic_load(port_shm, &read, &write);
    hal_port_atomic_store_read(port_shm, write);
}


#ifdef ULAPI
void hal_port_wait_readable(hal_port_t** port, unsigned count, sig_atomic_t* stop) {
    while((hal_port_readable(*port) < count) && (!stop || !*stop)) {
        rtapi_delay(10000000);
    }
}


void hal_port_wait_writable(hal_port_t** port, unsigned count, sig_atomic_t* stop) {
    while((hal_port_writable(*port) < count) && (!stop || !*stop)) {
        rtapi_delay(10000000);
    }
}
#endif



/*********************************************************************
 * HAL stream implementation
 *********************************************************************/

// Spells 'FIFO'
#define HAL_STREAM_MAGIC_NUM		0x4649464F

// This struct is purely local to this file to keep track of the stream
// data in shmem.
typedef struct __hal_stream_shm_t {
    unsigned magic;
    atomic_uint in;
    atomic_uint out;
    unsigned this_sample;
    unsigned depth;
    int num_pins;
    atomic_uint num_overruns;
    atomic_uint num_underruns;
    hal_type_t type[HAL_STREAM_MAX_PINS];
    hal_stream_data_u data[];
} hal_stream_shm_t;

static char *hal_stream_type_string(int type, char *buf, size_t nbuf)
{
    const char *cptr;
    switch(type) {
    case HAL_BIT:   cptr = "bit";   break;
    case HAL_FLOAT: cptr = "float"; break;
    case HAL_S32:   cptr = "s32";   break;
    case HAL_U32:   cptr = "u32";   break;
    case HAL_S64:   cptr = "s64";   break;
    case HAL_U64:   cptr = "u64";   break;
    case HAL_PORT:  cptr = "port";  break;
    default:
        rtapi_snprintf(buf, nbuf, "UNK#%d", type);
        return buf;
    }
    rtapi_snprintf(buf, nbuf, "%s", cptr);
    return buf;
}

static int hal_stream_parse_types(hal_type_t type[HAL_STREAM_MAX_PINS], const char *cfg)
{
    int n;

    if(!cfg || !*cfg) {
        rtapi_print_msg(RTAPI_MSG_ERR, "stream: ERROR: invalid null or empty type-string\n");
        return 0;
    }

    for(n = 0; n < HAL_STREAM_MAX_PINS && *cfg != '\0'; n++) {
        switch (*cfg) {
        case 'r': case 'R':
        case 'f': case 'F': type[n] = HAL_FLOAT; break;
        case 'b': case 'B': type[n] = HAL_BIT; break;
        case 'u': case 'U': type[n] = HAL_U32; break;
        case 's': case 'S': type[n] = HAL_S32; break;
        case 'l': case 'L': type[n] = HAL_S64; break;
        case 'k': case 'K': type[n] = HAL_U64; break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "stream: ERROR: unknown type '%c', must be R (F), B, U, S, L or K\n", *cfg);
            return 0;
        }
        cfg++;
    }
    if ( *cfg != '\0' ) {
        /* didn't reach end of cfg string */
        rtapi_print_msg(RTAPI_MSG_ERR, "stream: ERROR: more than %d items\n", HAL_STREAM_MAX_PINS);
        return 0;
    }
    return n;
}

int hal_stream_create(hal_stream_t *stream, int comp, int key, unsigned depth, const char *typestring)
{
    if(!stream) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_create: Invalid stream\n");
        return -EINVAL;
    }
    int result = 0;
    hal_type_t type[HAL_STREAM_MAX_PINS];
    result = hal_stream_parse_types(type, typestring);
    if(!result) return -EINVAL;
    int pin_count = result;

    size_t size = sizeof(hal_stream_shm_t) + sizeof(hal_stream_data_u) * depth * (1+pin_count);
    result = rtapi_shmem_new(key, comp, size);
    if(result < 0) return result;
    stream->shmem_id = result;

    result = rtapi_shmem_getptr(stream->shmem_id, (void**)&stream->fifo);
    if(result < 0) {
        rtapi_shmem_delete(key, comp);
        return result;
    }
    stream->comp_id = comp;

    memset(stream->fifo, 0, sizeof(*stream->fifo));
    memcpy(stream->fifo->type, type, sizeof(type));
    stream->fifo->depth = depth;
    stream->fifo->num_pins = pin_count;
    stream->fifo->magic = HAL_STREAM_MAGIC_NUM;
    atomic_thread_fence(memory_order_release);
    return 0;
}

void hal_stream_destroy(hal_stream_t *stream)
{
    hal_stream_detach(stream);
}

static inline unsigned hal_stream_advance(hal_stream_t *stream, unsigned n)
{
    n += 1;
    if(n >= stream->fifo->depth) return 0;
    return n;
}

static inline unsigned hal_stream_atomic_load_in(hal_stream_t *stream)
{
    return atomic_load_explicit(&stream->fifo->in, memory_order_acquire);
}

static inline unsigned hal_stream_atomic_load_out(hal_stream_t *stream)
{
    return atomic_load_explicit(&stream->fifo->out, memory_order_acquire);
}

static inline void hal_stream_atomic_store_in(hal_stream_t *stream, unsigned newin)
{
    atomic_store_explicit(&stream->fifo->in, newin, memory_order_release);
}

static inline void hal_stream_atomic_store_out(hal_stream_t *stream, unsigned newout)
{
    atomic_store_explicit(&stream->fifo->out, newout, memory_order_release);
}

static inline unsigned hal_stream_newin(hal_stream_t *stream)
{
    return hal_stream_advance(stream, hal_stream_atomic_load_in(stream));
}

bool hal_stream_writable(hal_stream_t *stream)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_writable: Invalid stream\n");
        return false;
    }
    return hal_stream_newin(stream) != hal_stream_atomic_load_out(stream);
}

bool hal_stream_readable(hal_stream_t *stream)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_readable: Invalid stream\n");
        return false;
    }
    return hal_stream_atomic_load_in(stream) != hal_stream_atomic_load_out(stream);
}

unsigned hal_stream_maxdepth(hal_stream_t *stream)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_maxdepth: Invalid stream\n");
        return 0;
    }
    return stream->fifo->depth;
}

#ifdef ULAPI
void hal_stream_wait_writable(hal_stream_t *stream, sig_atomic_t *stop) {
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_wait_writable: Invalid stream\n");
        return;
    }
    while(!hal_stream_writable(stream) && (!stop || !*stop)) {
        /* fifo full, sleep for 10ms */
        rtapi_delay(10000000);
    }
}

void hal_stream_wait_readable(hal_stream_t *stream, sig_atomic_t *stop)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_wait_readable: Invalid stream\n");
        return;
    }
    while(!hal_stream_readable(stream) && (!stop || !*stop)) {
        /* fifo full, sleep for 10ms */
        rtapi_delay(10000000);
    }
}
#endif

int hal_stream_depth(hal_stream_t *stream)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_depth: Invalid stream\n");
        return 0;
    }
    unsigned out = hal_stream_atomic_load_out(stream);
    unsigned in  = hal_stream_atomic_load_in(stream);
    if(out > in)
        return stream->fifo->depth + in - out;
    return in - out;
}

int hal_stream_write(hal_stream_t *stream, hal_stream_data_u *buf)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_write: Invalid stream\n");
        return -EINVAL;
    }
    if(!hal_stream_writable(stream)) {
        stream->fifo->num_overruns++;
        return -ENOSPC;
    }
    unsigned in = hal_stream_atomic_load_in(stream);
    unsigned newin = hal_stream_advance(stream, in);
    int num_pins = stream->fifo->num_pins;
    int stride = num_pins + 1;
    hal_stream_data_u *dptr = &stream->fifo->data[in * stride];
    memcpy(dptr, buf, sizeof(hal_stream_data_u) * num_pins);
    dptr[num_pins].s = ++stream->fifo->this_sample;
    hal_stream_atomic_store_in(stream, newin);
    return 0;
}

int hal_stream_read(hal_stream_t *stream, hal_stream_data_u *buf, unsigned *this_sample)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_read: Invalid stream\n");
        return -EINVAL;
    }
    if(!hal_stream_readable(stream)) {
        stream->fifo->num_underruns++;
        return -ENOSPC;
    }
    unsigned out = hal_stream_atomic_load_out(stream);
    unsigned newout = hal_stream_advance(stream, out);
    int num_pins = stream->fifo->num_pins;
    int stride = num_pins + 1;
    hal_stream_data_u *dptr = &stream->fifo->data[out * stride];
    memcpy(buf, dptr, sizeof(hal_stream_data_u) * num_pins);
    if(this_sample) *this_sample = dptr[num_pins].s;
    hal_stream_atomic_store_out(stream, newout);
    return 0;
}

int hal_stream_attach(hal_stream_t *stream, int comp_id, int key, const char *typestring)
{
    if(!stream) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_attach: Invalid stream\n");
        return -EINVAL;
    }
    stream->shmem_id = stream->comp_id = -1;
    stream->fifo = NULL;
    int result = rtapi_shmem_new(key, comp_id, sizeof(hal_stream_shm_t));
    int shmem_id = result;
    if ( result < 0 ) goto fail;

    hal_stream_shm_t *fifo;
    result = rtapi_shmem_getptr(shmem_id, (void **)&fifo);
    if ( result < 0 ) goto fail;

    if ( fifo->magic != HAL_STREAM_MAGIC_NUM ) {
        result = -EINVAL;
        goto fail;
    }
    if(typestring) {
        hal_type_t type[HAL_STREAM_MAX_PINS];
        result = hal_stream_parse_types(type, typestring);
        if(!result) {
            result = -EINVAL;
            goto fail;
        }
        for(int i = 0; i < result; i++) {
            if(fifo->type[i] != type[i]) {
                char tn0[32], tn1[32];
                hal_stream_type_string(fifo->type[i], tn0, sizeof(tn0));
                hal_stream_type_string(type[i], tn1, sizeof(tn1));
                rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_attach: Type mismatch: types[%d]: '%s' != '%s'\n", i, tn0, tn1);
                result = -EINVAL;
                goto fail;
            }
        }
    }
    /* now use data in fifo structure to calculate proper shmem size */
    unsigned depth = fifo->depth;
    int pin_count = fifo->num_pins;
    size_t size = sizeof(hal_stream_shm_t) + sizeof(hal_stream_data_u) * depth * (1+pin_count);
    /* close shmem, re-open with proper size */
    rtapi_shmem_delete(shmem_id, comp_id);
    shmem_id = result = rtapi_shmem_new(key, comp_id, size);
    if ( result < 0 ) goto fail;
    result = rtapi_shmem_getptr(shmem_id, (void **)&fifo);
    if ( result < 0 ) goto fail;

    stream->shmem_id = shmem_id;
    stream->comp_id = comp_id;
    stream->fifo = fifo;
    return 0;

fail:
    if(shmem_id >= 0)
        rtapi_shmem_delete(shmem_id, comp_id);
    return result;
}

int hal_stream_detach(hal_stream_t *stream) {
    if(!stream) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_detach: Invalid stream\n");
        return -EINVAL;
    }
    if(stream->shmem_id >= 0) {
        int res = rtapi_shmem_delete(stream->shmem_id, stream->comp_id);
        if(res < 0)
            rtapi_print_msg(RTAPI_MSG_ERR,
                "hal_stream_detach: rtapi_shmem_delete: failed with code %d\n",
                res);
    }
    stream->shmem_id = stream->comp_id = -1;
    stream->fifo = NULL;
    return 0;
}

int hal_stream_element_count(hal_stream_t *stream)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_element_count: Invalid stream\n");
        return 0;
    }
    return stream->fifo->num_pins;
}

hal_type_t hal_stream_element_type(hal_stream_t *stream, int idx)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_element_type: Invalid stream\n");
        return HAL_TYPE_UNSPECIFIED;
    }
    if(idx < 0 || idx >= stream->fifo->num_pins) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_element_type: Invalid type index %d (valid [0,%d])\n",
                        idx, stream->fifo->num_pins - 1);
        return HAL_TYPE_UNSPECIFIED;
    }
    return stream->fifo->type[idx];
}

int hal_stream_num_overruns(hal_stream_t *stream)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_num_overruns: Invalid stream\n");
        return HAL_TYPE_UNSPECIFIED;
    }
    return atomic_load_explicit(&stream->fifo->num_overruns, memory_order_acquire);
}

int hal_stream_num_underruns(hal_stream_t *stream)
{
    if(!stream || !stream->fifo) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_stream_num_underruns: Invalid stream\n");
        return 0;
    }
    return atomic_load_explicit(&stream->fifo->num_underruns, memory_order_acquire);
}

#ifdef RTAPI
/* only export symbols when we're building a kernel module */

EXPORT_SYMBOL(hal_init);
EXPORT_SYMBOL(hal_is_init);
EXPORT_SYMBOL(hal_ready);
EXPORT_SYMBOL(hal_set_unready);
EXPORT_SYMBOL(hal_exit);
EXPORT_SYMBOL(hal_malloc);
EXPORT_SYMBOL(hal_comp_name);
EXPORT_SYMBOL(hal_get_realtime_type);
EXPORT_SYMBOL(hal_strerror);

EXPORT_SYMBOL(hal_reference_map);
EXPORT_SYMBOL(hal_reference_unmap);

EXPORT_SYMBOL(hal_pin_new_bool);
EXPORT_SYMBOL(hal_pin_new_si32);
EXPORT_SYMBOL(hal_pin_new_ui32);
EXPORT_SYMBOL(hal_pin_new_sint);
EXPORT_SYMBOL(hal_pin_new_uint);
EXPORT_SYMBOL(hal_pin_new_real);
EXPORT_SYMBOL(hal_pin_new_port);

EXPORT_SYMBOL(hal_pin_bit_new);
EXPORT_SYMBOL(hal_pin_float_new);
EXPORT_SYMBOL(hal_pin_u32_new);
EXPORT_SYMBOL(hal_pin_s32_new);
EXPORT_SYMBOL(hal_pin_u64_new);
EXPORT_SYMBOL(hal_pin_s64_new);
EXPORT_SYMBOL(hal_pin_port_new);
EXPORT_SYMBOL(hal_pin_new);

EXPORT_SYMBOL(hal_pin_bit_newf);
EXPORT_SYMBOL(hal_pin_float_newf);
EXPORT_SYMBOL(hal_pin_u32_newf);
EXPORT_SYMBOL(hal_pin_s32_newf);
EXPORT_SYMBOL(hal_pin_u64_newf);
EXPORT_SYMBOL(hal_pin_s64_newf);
EXPORT_SYMBOL(hal_pin_port_newf);


EXPORT_SYMBOL(hal_signal_new);
EXPORT_SYMBOL(hal_signal_delete);
EXPORT_SYMBOL(hal_link);
EXPORT_SYMBOL(hal_unlink);

EXPORT_SYMBOL(hal_param_bit_new);
EXPORT_SYMBOL(hal_param_float_new);
EXPORT_SYMBOL(hal_param_u32_new);
EXPORT_SYMBOL(hal_param_s32_new);
EXPORT_SYMBOL(hal_param_u64_new);
EXPORT_SYMBOL(hal_param_s64_new);
EXPORT_SYMBOL(hal_param_new);

EXPORT_SYMBOL(hal_param_bit_newf);
EXPORT_SYMBOL(hal_param_float_newf);
EXPORT_SYMBOL(hal_param_u32_newf);
EXPORT_SYMBOL(hal_param_s32_newf);
EXPORT_SYMBOL(hal_param_u64_newf);
EXPORT_SYMBOL(hal_param_s64_newf);

EXPORT_SYMBOL(hal_param_new_bool);
EXPORT_SYMBOL(hal_param_new_si32);
EXPORT_SYMBOL(hal_param_new_ui32);
EXPORT_SYMBOL(hal_param_new_sint);
EXPORT_SYMBOL(hal_param_new_uint);
EXPORT_SYMBOL(hal_param_new_real);
EXPORT_SYMBOL(hal_param_new_fake);

// Parameter set functions have been deprecated.
// Use the generic hal_set_p() instead.
EXPORT_SYMBOL(hal_param_bit_set);
EXPORT_SYMBOL(hal_param_float_set);
EXPORT_SYMBOL(hal_param_u32_set);
EXPORT_SYMBOL(hal_param_s32_set);
EXPORT_SYMBOL(hal_param_u64_set);
EXPORT_SYMBOL(hal_param_s64_set);
EXPORT_SYMBOL(hal_param_set);

EXPORT_SYMBOL(hal_set_constructor);

EXPORT_SYMBOL(hal_export_funct);
EXPORT_SYMBOL(hal_export_functf);

EXPORT_SYMBOL(hal_create_thread);

EXPORT_SYMBOL(hal_add_funct_to_thread);
EXPORT_SYMBOL(hal_init_funct_to_thread);
EXPORT_SYMBOL(hal_del_funct_from_thread);

EXPORT_SYMBOL(hal_start_threads);
EXPORT_SYMBOL(hal_stop_threads);

// All HAL lib internals' access has bee deprecated. All function prefixed with
// halpr_ are only for private internal use.
EXPORT_SYMBOL(hal_shmem_base);
EXPORT_SYMBOL(halpr_find_comp_by_name);
EXPORT_SYMBOL(halpr_find_pin_by_name);
EXPORT_SYMBOL(halpr_find_sig_by_name);
EXPORT_SYMBOL(halpr_find_param_by_name);
EXPORT_SYMBOL(halpr_find_thread_by_name);
EXPORT_SYMBOL(halpr_find_funct_by_name);
EXPORT_SYMBOL(halpr_find_comp_by_id);

EXPORT_SYMBOL(halpr_find_pin_by_owner);
EXPORT_SYMBOL(halpr_find_param_by_owner);
EXPORT_SYMBOL(halpr_find_funct_by_owner);

EXPORT_SYMBOL(halpr_find_pin_by_sig);

EXPORT_SYMBOL(hal_pin_alias);
EXPORT_SYMBOL(hal_param_alias);

EXPORT_SYMBOL(hal_port_alloc); // Deprecated
EXPORT_SYMBOL(hal_port_read);
EXPORT_SYMBOL(hal_port_peek);
EXPORT_SYMBOL(hal_port_peek_commit);
EXPORT_SYMBOL(hal_port_write);
EXPORT_SYMBOL(hal_port_readable);
EXPORT_SYMBOL(hal_port_writable);
EXPORT_SYMBOL(hal_port_buffer_size);
EXPORT_SYMBOL(hal_port_clear);

EXPORT_SYMBOL_GPL(hal_stream_create);
EXPORT_SYMBOL_GPL(hal_stream_destroy);
EXPORT_SYMBOL_GPL(hal_stream_readable);
EXPORT_SYMBOL_GPL(hal_stream_writable);
EXPORT_SYMBOL_GPL(hal_stream_depth);
EXPORT_SYMBOL_GPL(hal_stream_maxdepth);
EXPORT_SYMBOL_GPL(hal_stream_write);
EXPORT_SYMBOL_GPL(hal_stream_read);
EXPORT_SYMBOL_GPL(hal_stream_attach);
EXPORT_SYMBOL_GPL(hal_stream_detach);
EXPORT_SYMBOL_GPL(hal_stream_element_count);
EXPORT_SYMBOL_GPL(hal_stream_element_type);
EXPORT_SYMBOL_GPL(hal_stream_num_overruns);
EXPORT_SYMBOL_GPL(hal_stream_num_underruns);
#endif /* rtapi */
