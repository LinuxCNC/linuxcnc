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

    extensively reworked for the 'new RTOS' support and unified build
                       Michael Haberler
                       <git AT mah DOT priv DOT at>
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

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_ring.h"		/* HAL ringbuffer decls */
#include "hal_group.h"		/* HAL group decls */
#include "hal_internal.h"

#include "rtapi_string.h"
#ifdef RTAPI
#include "rtapi_app.h"
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Hardware Abstraction Layer for EMC");
MODULE_LICENSE("GPL");
#define assert(e)
#endif /* RTAPI */


#if defined(ULAPI)
// if loading the ulapi shared object fails, we dont have RTAPI
// calls available for error messages, so fallback to stderr
// in this case.
#include <stdio.h>
#include <sys/types.h>		/* pid_t */
#include <unistd.h>		/* getpid() */
#include <assert.h>
#include <time.h>               /* remote comp bind/unbind/update timestamps */
#include <limits.h>             /* PATH_MAX */
#include <stdlib.h>		/* exit() */
#include "rtapi/shmdrv/shmdrv.h"
#endif

char *hal_shmem_base = 0;
hal_data_t *hal_data = 0;
int lib_module_id = -1; 	/* RTAPI module ID for library module */
static int lib_mem_id = -1;	/* RTAPI shmem ID for library module */

// the global data segment contains vital instance information and
// the error message ringbuffer, so all HAL entities attach it
// it is initialized and populated by rtapi_msgd which is assumed
// to run before any HAL/RTAPI activity starts, and until after it ends.
// RTAPI cannot execute without the global segment existing and attached.

// depending on ULAP/ vs RTAPI and thread system (userland vs kthreads)
// the way how
// pointer to the global data segment, initialized by calling the
// the ulapi.so ulapi_main() method (ULAPI) or by external reference
// to the instance module (kernel modes)


// defined(BUILD_SYS_KBUILD) && defined(RTAPI)
// defined & attached in, and exported from rtapi_module.c

// defined(BUILD_SYS_USER_DSO) && defined(RTAPI)
// defined & attached in, and exported from rtapi_main.c

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



/** The alloc_xxx_struct() functions allocate a structure of the
    appropriate type and return a pointer to it, or 0 if they fail.
    They attempt to re-use freed structs first, if none are
    available, then they call hal_malloc() to create a new one.
    The free_xxx_struct() functions add the structure at 'p' to
    the appropriate free list, for potential re-use later.
    All of these functions assume that the caller has already
    grabbed the hal_data mutex.
*/


/***********************************************************************
*                  PUBLIC (API) FUNCTION CODE                          *
************************************************************************/
/***********************************************************************
*                      "LOCKING" FUNCTIONS                             *
************************************************************************/
/** The 'hal_set_lock()' function sets locking based on one of the
    locking types defined in hal.h
*/
int hal_set_lock(unsigned char lock_type) {
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: set_lock called before init\n");
	return -EINVAL;
    }
    hal_data->lock = lock_type;
    return 0;
}

/** The 'hal_get_lock()' function returns the current locking level
    locking types defined in hal.h
*/

unsigned char hal_get_lock() {
    if (hal_data == 0) {
	hal_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: get_lock called before init\n");
	return -EINVAL;
    }
    return hal_data->lock;
}

/***********************************************************************
*         Scope exit unlock helper                                     *
*         see hal_priv.h for usage hints                               *
************************************************************************/
void halpr_autorelease_mutex(void *variable)
{
    if (hal_data != NULL)
	rtapi_mutex_give(&(hal_data->mutex));
    else
	// programming error
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL:%d BUG: halpr_autorelease_mutex called before hal_data inited\n",
			rtapi_instance);
}


/***********************************************************************
*                     LOCAL FUNCTION CODE                              *
************************************************************************/

#ifdef RTAPI
/* these functions are called when the hal_lib module is insmod'ed
   or rmmod'ed.
*/


int rtapi_app_main(void)
{
    int retval;
    void *mem;

    rtapi_switch = rtapi_get_handle();
    hal_print_msg(RTAPI_MSG_DBG,
		    "HAL_LIB:%d loading RT support gd=%pp\n",rtapi_instance,global_data);

    /* do RTAPI init */
    lib_module_id = rtapi_init("HAL_LIB");
    if (lib_module_id < 0) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d ERROR: rtapi init failed\n",
			rtapi_instance);
	return -EINVAL;
    }

    // paranoia
    if (global_data == NULL) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d ERROR: global_data == NULL\n",
			rtapi_instance);
	return -EINVAL;
    }

    /* get HAL shared memory block from RTAPI */
    lib_mem_id = rtapi_shmem_new(HAL_KEY, lib_module_id, global_data->hal_size);

    if (lib_mem_id < 0) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d ERROR: could not open shared memory\n",
			rtapi_instance);
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }
    /* get address of shared memory area */
    retval = rtapi_shmem_getptr(lib_mem_id, &mem, 0);

    if (retval < 0) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d ERROR: could not access shared memory\n",
			rtapi_instance);
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }
    /* set up internal pointers to shared mem and data structure */
    hal_shmem_base = (char *) mem;
    hal_data = (hal_data_t *) mem;
    /* perform a global init if needed */
    retval = init_hal_data();

    if ( retval ) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d ERROR: could not init shared memory\n",
			rtapi_instance);
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }

    retval = hal_proc_init();

    if ( retval ) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB: ERROR:%d could not init /proc files\n",
			rtapi_instance);
	rtapi_exit(lib_module_id);
	return -EINVAL;
    }

    /* done */
    hal_print_msg(RTAPI_MSG_DBG,
		    "HAL_LIB:%d kernel lib installed successfully\n",
		    rtapi_instance);
    return 0;
}

void rtapi_app_exit(void)
{
    int retval;

    hal_print_msg(RTAPI_MSG_DBG,
		    "HAL_LIB:%d removing RT support\n",rtapi_instance);
    hal_proc_clean();

    {
	hal_thread_t *thread __attribute__((cleanup(halpr_autorelease_mutex)));

	/* grab mutex before manipulating list */
	rtapi_mutex_get(&(hal_data->mutex));
	/* must remove all threads before unloading this module */
	while (hal_data->thread_list_ptr != 0) {
	    /* point to a thread */
	    thread = SHMPTR(hal_data->thread_list_ptr);
	    /* unlink from list */
	    hal_data->thread_list_ptr = thread->next_ptr;
	    /* and delete it */
	    free_thread_struct(thread);
	}
    }

    /* release RTAPI resources */
    retval = rtapi_shmem_delete(lib_mem_id, lib_module_id);
    if (retval) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d rtapi_shmem_delete(%d,%d) failed: %d\n",
			rtapi_instance, lib_mem_id, lib_module_id, retval);
    }

    retval = rtapi_exit(lib_module_id);
    if (retval) {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL_LIB:%d rtapi_exit(%d) failed: %d\n",
			rtapi_instance, lib_module_id, retval);
    }
    /* done */
    hal_print_msg(RTAPI_MSG_DBG,
		    "HAL_LIB:%d RT support removed successfully\n",
		    rtapi_instance);
}

#endif /* RTAPI */

/* see the declarations of these functions (near top of file) for
   a description of what they do.
*/

static int init_hal_data(void)
{

    /* has the block already been initialized? */
    if (hal_data->version != 0) {
	/* yes, verify version code */
	if (hal_data->version == HAL_VER) {
	    return 0;
	} else {
	    hal_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: version code mismatch\n");
	    return -1;
	}
    }
    /* no, we need to init it, grab the mutex unconditionally */
    rtapi_mutex_try(&(hal_data->mutex));

    // some heaps contain garbage, like xenomai
    memset(hal_data, 0, global_data->hal_size);

    /* set version code so nobody else init's the block */
    hal_data->version = HAL_VER;

    /* initialize everything */
    hal_data->comp_list_ptr = 0;
    hal_data->pin_list_ptr = 0;
    hal_data->sig_list_ptr = 0;
    hal_data->param_list_ptr = 0;
    hal_data->funct_list_ptr = 0;
    hal_data->thread_list_ptr = 0;
    hal_data->vtable_list_ptr = 0;
    hal_data->base_period = 0;
    hal_data->threads_running = 0;
    hal_data->oldname_free_ptr = 0;
    hal_data->comp_free_ptr = 0;
    hal_data->pin_free_ptr = 0;
    hal_data->sig_free_ptr = 0;
    hal_data->param_free_ptr = 0;
    hal_data->funct_free_ptr = 0;
    hal_data->vtable_free_ptr = 0;
    hal_data->pending_constructor = 0;
    hal_data->constructor_prefix[0] = 0;
    list_init_entry(&(hal_data->funct_entry_free));
    hal_data->thread_free_ptr = 0;
    hal_data->exact_base_period = 0;

    hal_data->group_list_ptr = 0;
    hal_data->member_list_ptr = 0;
    hal_data->ring_list_ptr = 0;

    hal_data->group_free_ptr = 0;
    hal_data->member_free_ptr = 0;
    hal_data->ring_free_ptr = 0;

    RTAPI_ZERO_BITMAP(&hal_data->rings, HAL_MAX_RINGS);
    // silly 1-based shm segment id allocation FIXED
    // yeah, 'user friendly', how could one possibly think zero might be a valid id
    RTAPI_BIT_SET(hal_data->rings,0);

    /* set up for shmalloc_xx() */
    hal_data->shmem_bot = sizeof(hal_data_t);
    hal_data->shmem_top = global_data->hal_size;
    hal_data->lock = HAL_LOCK_NONE;

    int i;
    for (i = 0; i < MAX_EPSILON; i++)
	hal_data->epsilon[i] = 0.0;
    hal_data->epsilon[0] = DEFAULT_EPSILON;

    /* done, release mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}

/***********************************************************************
*                     HAL data segment attach & detach                 *
*                                                                      *
* in place to work around a purported RTAI issue with shared memory    *
************************************************************************/

#ifdef ULAPI

// this is now delayed to first hal_init() in this process
int hal_rtapi_attach()
{
    int retval;
    void *mem;
    char rtapi_name[RTAPI_NAME_LEN + 1];

    if (lib_mem_id < 0) {
	hal_print_msg(RTAPI_MSG_DBG, "HAL: initializing hal_lib\n");
	rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_LIB_%d", (int)getpid());
	lib_module_id = rtapi_init(rtapi_name);
	if (lib_module_id < 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: could not not initialize RTAPI - realtime not started?\n");
	    return -EINVAL;
	}

	if (global_data == NULL) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL: ERROR: RTAPI shutting down - exiting\n");
	    exit(1);
	}

	/* get HAL shared memory block from RTAPI */
	lib_mem_id = rtapi_shmem_new(HAL_KEY, lib_module_id, global_data->hal_size);
	if (lib_mem_id < 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: could not open shared memory\n");
	    rtapi_exit(lib_module_id);
	    return -EINVAL;
	}
	/* get address of shared memory area */
	retval = rtapi_shmem_getptr(lib_mem_id, &mem, 0);
	if (retval < 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: could not access shared memory\n");
	    rtapi_exit(lib_module_id);
	    return -EINVAL;
	}
	/* set up internal pointers to shared mem and data structure */
        hal_shmem_base = (char *) mem;
        hal_data = (hal_data_t *) mem;
	/* perform a global init if needed */
	retval = init_hal_data();
	if ( retval ) {
	    hal_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: could not init shared memory\n");
	    rtapi_exit(lib_module_id);
	    return -EINVAL;
	}
    }
    return 0;
}

int hal_rtapi_detach()
{
    /* release RTAPI resources */
    if (lib_mem_id > -1) {
	// if they were actually initialized
	rtapi_shmem_delete(lib_mem_id, lib_module_id);
	rtapi_exit(lib_module_id);

	lib_mem_id = 0;
	lib_module_id = -1;
	hal_shmem_base = NULL;
	hal_data = NULL;
    }
    return 0;
}

// ULAPI-side cleanup. Called at shared library unload time as
// a destructor.
static void  __attribute__ ((destructor))  ulapi_hal_lib_cleanup(void)
{
    // detach the HAL data segment
    hal_rtapi_detach();
    // shut down ULAPI
    ulapi_cleanup();
}
#endif
#define HALPRINTBUFFERLEN 1024
static char _hal_errmsg[HALPRINTBUFFERLEN];

void hal_print_msg(int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vsnprintf(_hal_errmsg, HALPRINTBUFFERLEN, fmt, args);
    rtapi_print_msg(level, _hal_errmsg);
    va_end(args);
}

const char *hal_lasterror()
{
    return _hal_errmsg;
}

#ifdef RTAPI
/* only export symbols when we're building a realtime module */

EXPORT_SYMBOL(hal_init);
EXPORT_SYMBOL(hal_init_mode);
EXPORT_SYMBOL(hal_ready);
EXPORT_SYMBOL(hal_exit);
EXPORT_SYMBOL(hal_malloc);
EXPORT_SYMBOL(hal_comp_name);

EXPORT_SYMBOL(hal_pin_bit_new);
EXPORT_SYMBOL(hal_pin_float_new);
EXPORT_SYMBOL(hal_pin_u32_new);
EXPORT_SYMBOL(hal_pin_s32_new);
EXPORT_SYMBOL(hal_pin_new);
EXPORT_SYMBOL(hal_pin_newf);

EXPORT_SYMBOL(hal_pin_bit_newf);
EXPORT_SYMBOL(hal_pin_float_newf);
EXPORT_SYMBOL(hal_pin_u32_newf);
EXPORT_SYMBOL(hal_pin_s32_newf);


EXPORT_SYMBOL(hal_signal_new);
EXPORT_SYMBOL(hal_signal_delete);
EXPORT_SYMBOL(hal_link);
EXPORT_SYMBOL(hal_unlink);

EXPORT_SYMBOL(hal_param_bit_new);
EXPORT_SYMBOL(hal_param_float_new);
EXPORT_SYMBOL(hal_param_u32_new);
EXPORT_SYMBOL(hal_param_s32_new);
EXPORT_SYMBOL(hal_param_new);
EXPORT_SYMBOL(hal_param_newf);

EXPORT_SYMBOL(hal_param_bit_newf);
EXPORT_SYMBOL(hal_param_float_newf);
EXPORT_SYMBOL(hal_param_u32_newf);
EXPORT_SYMBOL(hal_param_s32_newf);

EXPORT_SYMBOL(hal_param_bit_set);
EXPORT_SYMBOL(hal_param_float_set);
EXPORT_SYMBOL(hal_param_u32_set);
EXPORT_SYMBOL(hal_param_s32_set);
EXPORT_SYMBOL(hal_param_set);

EXPORT_SYMBOL(hal_set_constructor);

EXPORT_SYMBOL(hal_export_funct);
EXPORT_SYMBOL(hal_export_functf);

EXPORT_SYMBOL(hal_create_thread);
EXPORT_SYMBOL(hal_thread_delete);

EXPORT_SYMBOL(hal_add_funct_to_thread);
EXPORT_SYMBOL(hal_del_funct_from_thread);

EXPORT_SYMBOL(hal_start_threads);
EXPORT_SYMBOL(hal_stop_threads);

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
EXPORT_SYMBOL(hal_print_msg);
EXPORT_SYMBOL(hal_lasterror);

#endif /* rtapi */
