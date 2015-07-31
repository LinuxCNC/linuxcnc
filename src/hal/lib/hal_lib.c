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
int lib_mem_id = -1;	/* RTAPI shmem ID for library module */

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
int hal_set_lock(unsigned char lock_type)
{
    CHECK_HALDATA();
    hal_data->lock = lock_type;
    return 0;
}

/** The 'hal_get_lock()' function returns the current locking level
    locking types defined in hal.h
*/

unsigned char hal_get_lock()
{
    CHECK_HALDATA();
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
	HALERR("BUG: halpr_autorelease_mutex called before hal_data inited");
}


/***********************************************************************
*                     LOCAL FUNCTION CODE                              *
************************************************************************/

#ifdef RTAPI

extern int hal_exit_threads(void); // in hal_thread.c


/* these functions are called when the hal_lib RT module is insmod'ed
   or rmmod'ed, or the respective userland DSO is loaded and
   initialized by rtapi_app.
*/

int rtapi_app_main(void)
{
    rtapi_switch = rtapi_get_handle();

    // sanity: these must have been inited before by the
    // respective rtapi.so/.ko module
    CHECK_NULL(rtapi_switch);
    CHECK_NULL(global_data);

    HALDBG("initializing RT hal_lib support");

    int retval = hal_xinit(TYPE_HALLIB, 0, 0, NULL, NULL, "hal_lib");

    HALDBG("RT hal_lib support initialized rc=%d", retval);
    return retval;
}

void rtapi_app_exit(void)
{
    HALDBG("removing RT hal_lib support");
    hal_proc_clean();
    hal_exit_threads();
    hal_exit(lib_module_id);
    HALDBG("RT hal_lib support removed successfully");
}
#endif /* RTAPI */

/***********************************************************************
*               ULAPI hal_lib support                                  *
*                                                                      *
************************************************************************/

#ifdef ULAPI

// ULAPI-side initialisation happens only once the first comp is
// initialized. See hal_xinit().

// ULAPI-side cleanup:
// Exit the HAL library component, which in turn
// releases the HAL shared memory segment.
//
// Called at shared library unload time as a destructor.
static void  __attribute__ ((destructor))  ulapi_hal_lib_cleanup(void)
{
    // exit the HAL library component (hal_lib%d % pid)
    if (lib_module_id > -1)
	hal_exit(lib_module_id);

    // shut down ULAPI
    ulapi_cleanup();
}
#endif


/***********************************************************************
*               logging                                                *
*                                                                      *
************************************************************************/

#define HALPRINTBUFFERLEN 1024
static char _hal_errmsg[HALPRINTBUFFERLEN];

void hal_print_msg(int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    rtapi_vsnprintf(_hal_errmsg, HALPRINTBUFFERLEN, fmt, args);
    rtapi_print_msg(level, _hal_errmsg);
    va_end(args);
}

void hal_print_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const char *prefix = "HAL error: ";
    strncpy(_hal_errmsg, prefix, sizeof(_hal_errmsg));

    rtapi_vsnprintf(_hal_errmsg + strlen(_hal_errmsg), HALPRINTBUFFERLEN,
		    fmt, args);
    rtapi_print_msg(RTAPI_MSG_ERR, _hal_errmsg);
    va_end(args);
}

const char *hal_lasterror(void)
{
    return _hal_errmsg;
}

#ifdef RTAPI
/* only export symbols when we're building a realtime module */

// ------------ public API:  ------------
// hal_comp.c:
EXPORT_SYMBOL(hal_init);
EXPORT_SYMBOL(hal_xinit);
EXPORT_SYMBOL(hal_xinitf);
EXPORT_SYMBOL(hal_ready);
EXPORT_SYMBOL(hal_exit);
EXPORT_SYMBOL(hal_comp_name);

// hal_memory.c:
EXPORT_SYMBOL(hal_malloc);

// hal_pin.c:
EXPORT_SYMBOL(hal_pin_new);
EXPORT_SYMBOL(hal_pin_newf);

EXPORT_SYMBOL(hal_pin_bit_new);
EXPORT_SYMBOL(hal_pin_float_new);
EXPORT_SYMBOL(hal_pin_u32_new);
EXPORT_SYMBOL(hal_pin_s32_new);

EXPORT_SYMBOL(hal_pin_bit_newf);
EXPORT_SYMBOL(hal_pin_float_newf);
EXPORT_SYMBOL(hal_pin_u32_newf);
EXPORT_SYMBOL(hal_pin_s32_newf);

EXPORT_SYMBOL(hal_signal_new);
EXPORT_SYMBOL(hal_signal_delete);
EXPORT_SYMBOL(hal_link);
EXPORT_SYMBOL(hal_unlink);

// hal_param.c:
EXPORT_SYMBOL(hal_param_new);
EXPORT_SYMBOL(hal_param_newf);

EXPORT_SYMBOL(hal_param_bit_new);
EXPORT_SYMBOL(hal_param_float_new);
EXPORT_SYMBOL(hal_param_u32_new);
EXPORT_SYMBOL(hal_param_s32_new);

EXPORT_SYMBOL(hal_param_bit_newf);
EXPORT_SYMBOL(hal_param_float_newf);
EXPORT_SYMBOL(hal_param_u32_newf);
EXPORT_SYMBOL(hal_param_s32_newf);

EXPORT_SYMBOL(hal_param_bit_set);
EXPORT_SYMBOL(hal_param_float_set);
EXPORT_SYMBOL(hal_param_u32_set);
EXPORT_SYMBOL(hal_param_s32_set);
EXPORT_SYMBOL(hal_param_set);

// hal_funct.c:
EXPORT_SYMBOL(hal_export_funct);
EXPORT_SYMBOL(hal_export_functf);
EXPORT_SYMBOL(hal_export_xfunctf);
EXPORT_SYMBOL(hal_add_funct_to_thread);
EXPORT_SYMBOL(hal_del_funct_from_thread);
EXPORT_SYMBOL(hal_call_usrfunct);

// hal_thread.c:
EXPORT_SYMBOL(hal_create_xthread);
EXPORT_SYMBOL(hal_create_thread);
EXPORT_SYMBOL(hal_thread_delete);
EXPORT_SYMBOL(hal_start_threads);
EXPORT_SYMBOL(hal_stop_threads);

// hal_inst.c:
EXPORT_SYMBOL(hal_inst_create);
EXPORT_SYMBOL(hal_inst_delete);

// hal_lib.c:
EXPORT_SYMBOL(hal_print_msg);
EXPORT_SYMBOL(hal_print_error);
EXPORT_SYMBOL(hal_lasterror);
EXPORT_SYMBOL(hal_shmem_base);

// ------------ private API:  ------------
//  found in their respective source files:
EXPORT_SYMBOL(halpr_find_comp_by_name);
EXPORT_SYMBOL(halpr_find_pin_by_name);
EXPORT_SYMBOL(halpr_find_sig_by_name);
EXPORT_SYMBOL(halpr_find_param_by_name);
EXPORT_SYMBOL(halpr_find_thread_by_name);
EXPORT_SYMBOL(halpr_find_funct_by_name);
EXPORT_SYMBOL(halpr_find_inst_by_name);

EXPORT_SYMBOL(halpr_find_owning_comp);

EXPORT_SYMBOL(halpr_find_pin_by_instance_id);
EXPORT_SYMBOL(halpr_find_param_by_instance_id);
EXPORT_SYMBOL(halpr_find_funct_by_instance_id);
EXPORT_SYMBOL(halpr_find_inst_by_owning_comp);

EXPORT_SYMBOL(halpr_find_inst_by_id);
EXPORT_SYMBOL(halpr_find_comp_by_id);

EXPORT_SYMBOL(halpr_find_pin_by_sig);

#endif /* rtapi */
