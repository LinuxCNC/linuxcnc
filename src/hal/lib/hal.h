#ifndef HAL_H
#define HAL_H

/** HAL stands for Hardware Abstraction Layer, and is used by EMC to
    transfer realtime data to and from I/O devices and other low-level
    modules.
*/

/********************************************************************
* Description:  hal.h
*               This file, 'hal.h', defines the API and data
*               structures used by the HAL
*
* Author: John Kasunich
* License: LGPL Version 2
*
* Copyright (c) 2003 All rights reserved.
*
* Last change:
********************************************************************/
/* testing */

/** This file, 'hal.h', defines the API and data structures used by
    the HAL.  This file is included in both realtime and non-realtime
    HAL components.  HAL uses the RTPAI real time interface, and the
    #define symbols RTAPI and ULAPI are used to distinguish between
    realtime and non-realtime code.  The API defined in this file
    is implemented in hal_lib.c and can be compiled for linking to
    either realtime or user space HAL components.
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
*                   GENERAL NOTES AND DOCUMENTATION                    *
************************************************************************/

/** The HAL is a very modular approach to the low level parts of a
    motion control system.  The goal of the HAL is to allow a systems
    integrator to connect a group of software components together to
    meet whatever I/O requirements he (or she) needs.  This includes
    realtime and non-realtime I/O, as well as basic motor control up
    to and including a PID position loop.  What these functions have
    in common is that they all process signals.  In general, a signal
    is a data item that is updated at regular intervals.  For example,
    a PID loop gets position command and feedback signals, and produces
    a velocity command signal.

    HAL is based on the approach used to design electronic circuits.
    In electronics, off-the-shelf components like integrated circuits
    are placed on a circuit board and their pins are interconnected
    to build whatever overall function is needed.  The individual
    components may be as simple as an op-amp, or as complex as a
    digital signal processor.  Each component can be individually
    tested, to make sure it works as designed.  After the components
    are placed in a larger circuit, the signals connecting them can
    still be monitored for testing and troubleshooting.

    Like electronic components, HAL components have pins, and the pins
    can be interconnected by signals.  At some point, I hope to be able
    to use a GPL'ed schematic capture program with a special HAL library
    to draw a "schematic" diagram with interconnected HAL components.
    You would then generate a netlist from the schematic, and the netlist
    would configure the HAL.  For now, however, HAL configuration will be
    done by a utility that parses a text file or works interactively.

    In the HAL, a 'signal' contains the actual data value that passes
    from one pin to another.  When a signal is created, space is allocated
    for the data value.  A 'pin' on the other hand, is a pointer, not a
    data value.  When a pin is connected to a signal, the pin's pointer
    is set to point at the signal's data value.  This allows the component
    to access the signal with very little run-time overhead.  (If a pin
    is not linked to any signal, the pointer points to a dummy location,
    so the realtime code doesn't have to deal with null pointers or treat
    unlinked variables as a special case in any way.)

    There are three approaches to writing a HAL component.  Those
    that do not require hard realtime performance can be written
    as a single user mode process.  Components that need hard realtime
    performance but have simple configuration and init requirements
    can be done as a single kernel module, using either pre-defined
    init info, or insmod-time parameters.  Finally, complex components
    may use both a kernel module for the realtime part, and a user
    space process to handle ini file access, user interface (possibly
    including GUI features), and other details.

    HAL uses the RTAPI/ULAPI interface.  If RTAPI is #defined, (done
    by the makefile) hal_lib.c would generate a kernel module hal_lib.o
    that is insmoded and provides the functions for all kernel module
    based components.  The same source file compiled with the ULAPI
    #define would make a user space hal_lib.o that is staticlly linked
    to user space code to make user space executables.  The variable
    lists and link information are stored in a block of shared memory
    and protected with mutexes, so that kernel modules and any of
    several user mode programs can access the data.

*/

#include <rtapi.h>
RTAPI_BEGIN_DECLS

#if ( !defined RTAPI ) && ( !defined ULAPI )
#error HAL needs RTAPI/ULAPI, check makefile and flags
#endif

#include <hal_types.h>
#include <rtapi_errno.h>

#define HAL_NAME_LEN     41	// legacy length limit
#define HAL_MAX_NAME_LEN 127	// actual limit at HAL layer (new-style halg_* methods)
#define HAL_DEFAULT_RINGSIZE    1024
#define MAX_ARGC         30     // realistic max argc count for module/inst params

/** These locking codes define the state of HAL locking, are used by most functions */
/** The functions locked will return a -EPERM error message **/

#define HAL_LOCK_NONE     0     /* no locking done, any command is permitted */
#define HAL_LOCK_LOAD     1     /* loading rt components is not permitted */
#define HAL_LOCK_CONFIG   2     /* locking of link and addf related commands */
#define HAL_LOCK_PARAMS   4     /* locking of parameter set commands */
#define HAL_LOCK_RUN      8     /* locking of start/stop of HAL threads */

#define HAL_LOCK_ALL      255   /* locks every action */

// opaque forward declaration of internal structs
struct halhdr;
struct hal_comp;
struct hal_inst;
struct hal_pin;
struct hal_param;
struct hal_sig;
struct hal_group;
struct hal_member;
struct hal_funct;
struct hal_thread;
struct hal_vtable;
struct hal_ring;
struct hal_plug;

typedef struct hal_comp hal_comp_t;
typedef struct hal_inst hal_inst_t;
typedef struct hal_pin hal_pin_t;
typedef struct hal_param hal_param_t;
typedef struct hal_sig hal_sig_t;
typedef struct hal_group hal_group_t;
typedef struct hal_member hal_member_t;
typedef struct hal_funct hal_funct_t;
typedef struct hal_thread hal_thread_t;
typedef struct hal_vtable hal_vtable_t;
typedef struct hal_ring hal_ring_t;
typedef struct hal_plug hal_plug_t;


// works like
// extern int _halerrno;
//  _halerrno is only set by the library, never cleared
// cleared only by calling hal_errorcount(1) which returns
// the number of errors logged since last call.
int *_halerrno_location(void);
#define _halerrno (*_halerrno_location())

// bumped by every reference to _halerrno (lvalue or rvalue)
// optionally clear _halerrno
// semi-useful
int hal_errorcount(int clear);


/***********************************************************************
*                   GENERAL PURPOSE FUNCTIONS                          *
************************************************************************/

/** 'hal_init()' is called by a HAL component before any other hal
    function is called, to open the HAL shared memory block and
    do other initialization.
    'name' is the name of the component.  It must be unique in the
    system.  It must be no longer than HAL_NAME_LEN.
    On success, hal_init() returns a positive integer component ID,
    which is used for subsequent calls to hal_xxx_new() and
    hal_exit().  On failure, returns an error code (see above).
    'hal_init()' calls rtapi_init(), so after calling hal_init(), a
    component can use any rtapi functions.  The component ID returned
    by 'hal_init()' is also the RTAPI module ID for the associated
    module, and can be used when calling rtapi functions.
    Call only from within user space or init/cleanup code, not from
    realtime code.
*/

/* traditional components have the following lifecycle:
 * their type is set to TYPE_RT or TYPE_USER
 * hal_init()  sets state to COMP_INITIALIZING
 * hal_ready() will transition the state to COMP_READY
 *
 * hal_init(comp) is a way of specifying:
 *   hal_init_mode(comp, TYPE_RT, 0, 0) (RTAPI)
 *   hal_init_mode(comp, TYPE_USER, 0, 0) (ULAPI)
 * to assure backwards compatibility.
 *
 * hal_bind() and hal_unbind() are undefined for MODE_LOCAL
 * components and cause a warning message.
 *
 * a remote component differs as follows:
 *
 * hal_init_mode(comp, TYPE_REMOTE,..) sets state to COMP_INITIALIZING
 * hal_ready() will transition the state to COMP_UNBOUND
 * hal_bind(comp)   will transition state to  COMP_BOUND
 * hal_unbind(comp) will transition state back to COMP_UNBOUND
 *
 * in both cases, hal_exit() will terminate the component.
 *
 * A remote component is conceptually a user component owned
 * by the process creating the component, for instance halcmd.
 * It can be repossessed to a different process by
 *
 * hal_acquire(name, process id)
 *
 * the only consequence is that on 'halcmd unloadusr <remote component>
 * this process will receive a signal to shut down.
 *
 * linking and unlinking of pins is possible as long as the
 * component state != COMP_INITIALIZING.
 */
typedef enum {
    TYPE_INVALID = 0,
    TYPE_RT,
    TYPE_USER,
    TYPE_REMOTE,

    // internal use only for initing the very first component, hal_lib
    // which needs extra care since the HAL shm segment needs to be
    // allocated
    TYPE_HALLIB,
} comp_type_t;

typedef enum {
    COMP_INVALID = 0,
    COMP_INITIALIZING,
    COMP_UNBOUND,
    COMP_BOUND,
    COMP_READY
} comp_state_t;

// if a component exports a constructor via hal_export_xfunctf(), it is instantiable
// the calling convention for hal_constructor_t is as follows:
//
// any instance parameters (key=value) up to the first appearance of '--' or a '--option' flag
// are removed from the argument vector and are already applied
// to the RTAPI_IP_* params when the constructor is called.
//
// the '--' separtor is skipped and not passed to the constructor.
//
// argv[0]: component name, string, always non-NULL
// argv[1]: instance  name, string, always non-NULL
// .. any other arguments ..
//
// other arguments are:
// any argument passed after an '--' separator
// any argument beginning with '--' (like a getopt_long() option)
//
// halcmd example 1: newinst foo bar instparm1=123 instparm2=3.14 --foo --bar baz key=value
//
// instance params = [instparm1=123, instparm2=3.14]
// argv = [foo, bar, --foo, --bar, baz, key=value]
// argc = 6
//
// halcmd example 2:  newinst foo bar -- instparm1=123 instparm2=3.14 --foo --bar baz key=value
// no instparms are applied as all arguments follow the '--' separator
// argv = [foo, bar, instparm1=123, instparm2=3.14, --foo, --bar, baz, key=value]
// argc = 8
typedef int (*hal_constructor_t) (const int argc, char* const *argv);
typedef int (*hal_destructor_t) (const char *name, void *inst, const int inst_size);

// generic base function
hal_comp_t *halg_xinitfv(const int use_hal_mutex,
			 const int type,
			 const int userarg1,
			 const int userarg2,
			 const hal_constructor_t ctor,
			 const hal_destructor_t dtor,
			 const char *fmt,
			 va_list ap);

// generic printf-style version
hal_comp_t *halg_xinitf(const int use_hal_mutex,
			const int type,
			const int userarg1,
			const int userarg2,
			const hal_constructor_t ctor,
			const hal_destructor_t dtor,
			const char *fmt, ...)
    __attribute__((format(printf,7, 8)));

// legacy
int hal_xinit(const int type,
	      const int userarg1,
	      const int userarg2,
	      const hal_constructor_t ctor,
	      const hal_destructor_t dtor,
	      const char *name);

// backwards compatibility:
static inline int hal_init(const char *name) {
#ifdef RTAPI
    return hal_xinit(TYPE_RT, 0, 0, NULL, NULL, name);
#else
    return hal_xinit(TYPE_USER, 0, 0, NULL, NULL, name);
#endif
}


#if defined(ULAPI)
// generic functions (with/without lock)
extern int halg_bind(const int use_hal_mutex, const char *comp);
extern int halg_unbind(const int use_hal_mutex, const char *comp);
extern int halg_acquire(const int use_hal_mutex, const char *comp, int pid);
extern int halg_release(const int use_hal_mutex, const char *comp);

// legacy wrappers
static inline int hal_bind(const char *comp) {
    return halg_bind(1, comp);
}
static inline  int hal_unbind(const char *comp) {
    return halg_unbind(1, comp);
}
static inline  int hal_acquire(const char *comp, int pid) {
    return halg_acquire(1, comp, pid);
}
static inline  int hal_release(const char *comp) {
        return halg_release(1, comp);
}
#endif


/** 'hal_exit()' must be called before a HAL component exits, to
    free resources associated with the component.
    'comp_id' is the ID of the component as returned from its initial
    call to 'hal_init()'.  'hal_exit()' will remove the component's
    realtime functions (if any) from realtime threads.  It also
    removes all pins and parameters exported by the component.  If
    the component created _any_ threads, when it exits _all_ threads
    will be stopped, and the ones it created will be deleted.
    It is assumed that the system will no longer function correctly
    after a component is removed, but this cleanup will prevent
    crashes when the component's code and data is unmapped.
    'hal_exit()' calls 'rtapi_exit()', so any rtapi reaources
    allocated should be discarded before calling hal_exit(), and
    rtapi functios should not be called afterwards.
    On success, hal_exit() returns 0, on failure it
    returns a negative error code.
*/
int halg_exit(const int use_hal_mutex, int comp_id);

static inline int hal_exit(int comp_id) {
    return halg_exit(1,  comp_id);
}

/** hal_malloc() allocates a block of memory from the main HAL
    shared memory area.  It should be used by all components to
    allocate memory for HAL pins and parameters.
    It allocates 'size' bytes, and returns a pointer to the
    allocated space, or NULL (0) on error.  The returned pointer
    will be properly aligned for any variable HAL supports (see
    HAL_TYPE below.)
    The allocator is very simple, and there is no 'free'.  It is
    assumed that a component will allocate all the memory it needs
    during initialization.  The entire HAL shared memory area is
    freed when the last component calls hal_exit().  This means
    that if you continuously install and remove one component
    while other components are present, you eventually will fill
    up the shared memory and an install will fail.  Removing
    all components completely clears memory and you start
    fresh.
*/
void *halg_malloc(const int use_hal_mutex, size_t size);

static inline void *hal_malloc(size_t size) {
    return halg_malloc(1, size);
}

/** hal_ready() indicates that this component is ready.  This allows
    halcmd 'loadusr -W hal_example' to wait until the userspace
    component 'hal_example' is ready before continuing.
*/
int halg_ready(const int use_hal_mutex, int comp_id);

static inline int hal_ready(int comp_id) {
    return halg_ready(1, comp_id);
}

/** hal_comp_name() returns the name of the given component, or NULL
    if comp_id is not a loaded component
*/
extern const char* hal_comp_name(int comp_id);

// return the state of a component, or -ENOENT on failure (e.g not existent)
int hal_comp_state_by_name(const char *name);

/** attach or detach the HAL shared memory segment
 *  this might be needed in using code as there have been issues
 *  in halcmd/RTAI builds.
 *  originally this was integrated into hal_exit()/hal_init().
 *  factored out as separate functions to enable a HAL component
 *  which is instantiated at library load time independent of
 *  hal_init/hal_exit calls from using code.
 *
 *  this cannot be used from RTAPI.
 */

#ifdef ULAPI
extern int hal_rtapi_attach();
#endif


// return the last HAL error message. Used for API binding error messages during setup
// not guaranteed to be related to last HAL API call if threads are running.
const char *hal_lasterror(void);

// same signature as rtapi_print_msg, but records last error message for hal_lasterror():
void hal_print_msg(int level, const char *fmt, ...)
    __attribute__((format(printf,2,3)));

// shorthand - prints "HAL error: " + formatted part
void hal_print_error(const char *fmt, ...)
    __attribute__((format(printf,1,2)));


/** The HAL maintains lists of variables, functions, and so on in
    a central database, located in shared memory so all components
    can access it.  To prevent contention, functions that may
    modify the database use a mutex before accessing it.  Since
    these functions may block on the mutex, they must not be called
    from realtime code.  They can only be called from user space
    components, and from the init code of realtime components.

    In general, realtime code won't call _any_ hal functions.
    Instead, realtime code simply does its job, accessing data
    using pointers that were set up by hal functions at startup.
*/


/***********************************************************************
*                     DATA RELATED TYPEDEFS                            *
************************************************************************/

/** HAL pins and signals are typed, and the HAL only allows pins
    to be attached to signals of the same type.
    All HAL types can be read or written atomically.  (Read-modify-
    write operations are not atomic.)
    Note that when a component reads or writes one of its pins, it
    is actually reading or writing the signal linked to that pin, by
    way of the pointer.
    'hal_type_t' is an enum used to identify the type of a pin, signal,
    or parameter.
*/
typedef enum {
    HAL_TYPE_UNSPECIFIED = -1,
    HAL_BIT = 1,
    HAL_FLOAT = 2,
    HAL_S32 = 3,
    HAL_U32 = 4,
    HAL_S64 = 5,
    HAL_U64 = 6,
    HAL_TYPE_MAX
} hal_type_t;

/** HAL pins have a direction attribute.  A pin may be an input to
    the HAL component, an output, or it may be bidirectional.
    Any number of HAL_IN or HAL_IO pins may be connected to the same
    signal, but only one HAL_OUT pin is permitted.  This is equivalent
    to connecting two output pins together in an electronic circuit.
    (HAL_IO pins can be thought of as tri-state outputs.)
*/

typedef enum {
    HAL_DIR_UNSPECIFIED = -1,
    HAL_IN = 16,
    HAL_OUT = 32,
    HAL_IO = (HAL_IN | HAL_OUT),
} hal_pin_dir_t;

/** HAL parameters also have a direction attribute.  For parameters,
    the attribute determines whether the user can write the value
    of the parameter, or simply read it.  HAL_RO parameters are
    read-only, and HAL_RW ones are writable with 'halcmd setp'.
*/

typedef enum {
    HAL_RO = 64,
    HAL_RW = 192,
} hal_param_dir_t;

/***********************************************************************
*                      "LOCKING" FUNCTIONS                             *
************************************************************************/
/** The 'hal_set_lock()' function sets locking based on one of the
    locking types defined in hal.h
    HAL_LOCK_NONE -locks none
    HAL_LOCK_* - intermediate locking levels
    HAL_LOCK_ALL - locks everything
*/
extern int hal_set_lock(unsigned char lock_type);

/** The 'hal_get_lock()' function returns the current locking level
    locking types defined in hal.h
    HAL_LOCK_NONE -locks none
    HAL_LOCK_* - intermediate locking levels
    HAL_LOCK_ALL - locks everything
*/

extern unsigned char hal_get_lock(void);

/***********************************************************************
*           opaque forward typedefs for HAL object pointers            *
************************************************************************/

// type aliases for hal_pin_t * - makes compiler
// detect type mismatch between <type1> and <type2>:
//
//   set_<type1>_pin(<type2>pinptr,...) and
//   get_<type1>_pin(<type2>pinptr)

// context-independent - use offsets

typedef struct { shmoff_t _bp;   } bit_pin_ptr;
typedef struct { shmoff_t _sp;   } s32_pin_ptr;
typedef struct { shmoff_t _up;   } u32_pin_ptr;
typedef struct { shmoff_t _lsp;  } s64_pin_ptr;
typedef struct { shmoff_t _lup;  } u64_pin_ptr;
typedef struct { shmoff_t _fp;   } float_pin_ptr;

// same trick for signals
typedef struct { shmoff_t _bs;   } bit_sig_ptr;
typedef struct { shmoff_t _ss;   } s32_sig_ptr;
typedef struct { shmoff_t _us;   } u32_sig_ptr;
typedef struct { shmoff_t _lss;  } s64_sig_ptr;
typedef struct { shmoff_t _lus;  } u64_sig_ptr;
typedef struct { shmoff_t _fs;   } float_sig_ptr;

#if 0
// params are on the way out, so dont bother
typedef struct { shmoff_t _bpar; } bit_param_ptr;
typedef struct { shmoff_t _spar; } s32_param_ptr;
typedef struct { shmoff_t _upar; } u32_param_ptr;
typedef struct { shmoff_t _fpar; } float_param_ptr;

#endif

/***********************************************************************
*                        "PIN" FUNCTIONS                               *
************************************************************************/

/** The 'hal_pin_xxx_new()' functions create a new 'pin' object.
    Once a pin has been created, it can be linked to a signal object
    using hal_link().  A pin contains a pointer, and the component
    that owns the pin can dereference the pointer to access whatever
    signal is linked to the pin.  (If no signal is linked, it points
    to a dummy signal.)
    There are eight functions, one for each of the data types that
    the HAL supports.  Pins may only be linked to signals of the same
    type.
    'name' is the name of the new pin.  It must be no longer than HAL_NAME_LEN.
    If there is already a pin with the same name the call will fail.
    'dir' is the pin direction.  It indicates whether the pin is
    an input or output from the component.
    'data_ptr_addr' is the address of the pointer that the component
    will use for the pin.  When the pin is linked to a signal, the
    pointer at 'data_ptr_addr' will be changed to point to the signal
    data location.  'data_ptr_addr' must point to memory allocated by
    hal_malloc().  Typically the component allocates space for a data
    structure with hal_malloc(), and 'data_ptr_addr' is the address
    of a member of that structure.
    'owner_id' is the ID of the component that will 'own' the
    variable.  Normally it should be the ID of the caller, but in
    some cases, a user mode component may be doing setup for a
    realtime component, so the ID should be that of the realtime
    component that will actually be using the pin.
    If successful, the hal_pin_xxx_new() functions return 0.
    On failure they return a negative error code.
*/
// generic base function, everything else layered ontop.
// the base functions always return descriptors.
hal_pin_t *halg_pin_newfv(const int use_hal_mutex,
			  const hal_type_t type,
			  const hal_pin_dir_t dir,
			  void **data_ptr_addr,
			  const int owner_id,
			  const hal_data_u defval,
			  const char *fmt, va_list ap);


// generic printf-style version of halg_pin_newfv()
hal_pin_t *halg_pin_newf(const int use_hal_mutex,
			 hal_type_t type,
			 hal_pin_dir_t dir,
			 void ** data_ptr_addr,
			 int owner_id,
			 const char *fmt, ...)
    __attribute__((format(printf,6,7)));

// legacy
static inline int hal_pin_new(const char *name,
			      const hal_type_t type,
			      const hal_pin_dir_t dir,
			      void **data_ptr_addr,
			      const int owner_id) {
    return halg_pin_newf(1, type,  dir,
			 data_ptr_addr, owner_id, "%s", name) == NULL ? _halerrno : 0;
}

// legacy - printf-style version of hal_pin_new()
int hal_pin_newf(hal_type_t type,
		 hal_pin_dir_t dir,
		 void ** data_ptr_addr,
		 int owner_id,
		 const char *fmt, ...)
    __attribute__((format(printf,5,6)));

static inline int hal_pin_bit_new(const char *name, hal_pin_dir_t dir,
				  hal_bit_t ** data_ptr_addr, int owner_id) {
    return hal_pin_new(name, HAL_BIT, dir, (void **) data_ptr_addr, owner_id);
}
static inline int hal_pin_float_new(const char *name, hal_pin_dir_t dir,
				    hal_float_t ** data_ptr_addr, int owner_id) {
    return hal_pin_new(name, HAL_FLOAT, dir, (void **) data_ptr_addr, owner_id);
}
static inline int hal_pin_u32_new(const char *name, hal_pin_dir_t dir,
				  hal_u32_t ** data_ptr_addr, int owner_id) {
    return hal_pin_new(name, HAL_U32, dir, (void **) data_ptr_addr, owner_id);
}
static inline int hal_pin_s32_new(const char *name, hal_pin_dir_t dir,
				  hal_s32_t ** data_ptr_addr, int owner_id) {
    return hal_pin_new(name, HAL_S32, dir, (void **) data_ptr_addr, owner_id);
}
static inline int hal_pin_u64_new(const char *name, hal_pin_dir_t dir,
				  hal_u64_t ** data_ptr_addr, int owner_id) {
    return hal_pin_new(name, HAL_U64, dir, (void **) data_ptr_addr, owner_id);
}
static inline int hal_pin_s64_new(const char *name, hal_pin_dir_t dir,
				  hal_s64_t ** data_ptr_addr, int owner_id) {
    return hal_pin_new(name, HAL_S64, dir, (void **) data_ptr_addr, owner_id);
}
/** The hal_pin_XXX_newf family of functions are similar to
    hal_pin_XXX_new except that they also do printf-style formatting to compute
    the pin name
    If successful, the hal_pin_xxx_newf() functions return 0.
    On failure they return a negative error code.
*/
extern int hal_pin_bit_newf(hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_float_newf(hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_u32_newf(hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_s32_newf(hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_u64_newf(hal_pin_dir_t dir,
    hal_u64_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_s64_newf(hal_pin_dir_t dir,
    hal_s64_t ** data_ptr_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
/** 'halg_pin_newf()' creates a new 'pin' object.  It is a generic
    version of the eight functions above.  It is provided ONLY for
    those special cases where a generic function is needed.  It is
    STRONGLY recommended that the functions above be used instead,
    because they check the type of 'data_ptr_addr' against the pin
    type at compile time.  Using this function requires a cast of
    the 'data_ptr_addr' argument that defeats type checking and can
    cause subtle bugs.
    'name', 'dir', 'data_ptr_addr' and 'owner_id' are the same as in
    the functions above.
    'type' is the hal type of the new pin - the type of data that
    will be passed in/out of the component through the new pin.
    If successful, halg_pin_newf() returns a pin descriptor reference (hal_pin_t *).
    On failure   it returns NULL, and _halerrno is set to a negative value.
*/


/** There is no 'hal_pin_delete()' function.  Once a component has
    created a pin, that pin remains as long as the component exists.
    All pins belonging to a component are removed when the component
    calls 'hal_exit()'.
*/

/***********************************************************************
*                      "SIGNAL" FUNCTIONS                              *
************************************************************************/

/** 'hal_signal_new()' creates a new signal object.  Once a signal has
    been created, pins can be linked to it with hal_link().  The signal
    object contains the actual storage for the signal data.  Pin objects
    linked to the signal have pointers that point to the data.
    'name' is the name of the new signal.  It must be no longer than
    HAL_NAME_LEN.  If there is already a signal with the same
    name the call will fail.
    'type' is the data type handled by the signal.  Pins can only be
    linked to a signal of the same type.
    Note that the actual address of the data storage for the signal is
    not accessible.  The data can be accessed only by linking a pin to
    the signal.  Also note that signals, unlike pins, do not have
    'owners'.  Once created, a signal remains in place until either it
    is deleted, or the last HAL component exits.
    If successful, 'hal_signal_new() returns 0.  On failure
    it returns a negative error code.
*/
int halg_signal_new(const int use_hal_mutex,
		    const char *name, hal_type_t type);

static inline  int hal_signal_new(const char *name, hal_type_t type) {
    return halg_signal_new(1, name, type);
}

/** 'hal_signal_delete()' deletes a signal object.  Any pins linked to
    the object are unlinked.
    'name' is the name of the signal to be deleted.
    If successful, 'hal_signal_delete()' returns 0.  On
    failure, it returns a negative error code.
*/

int halg_signal_delete(const int use_hal_mutex, const char *name);

static inline int hal_signal_delete(const char *name) {
    return halg_signal_delete(1, name);
}

int halg_signal_setbarriers(const int use_hal_mutex,
			    const char *name,
			    const int read_barrier,
			    const int write_barrier);

static inline int hal_signal_setbarriers(const char *name,
					 const int read_barrier,
					 const int write_barrier) {
    return halg_signal_setbarriers(1, name, read_barrier, write_barrier);
}

/** 'hal_link()' links a pin to a signal.  'pin_name' and 'sig_name' are
    strings containing the pin and signal names.  If the pin is already
    linked to the desired signal, the command succeeds.  If the pin is
    already linked to some other signal, it is an error.  In either
    case, the existing connection is not modified.  (Use 'hal_unlink'
    to break an existing connection.)  If the signal already has other
    pins linked to it, they are unaffected - one signal can be linked
    to many pins, but a pin can be linked to only one signal.
    On success, hal_link() returns 0, on failure it returns a
    negative error code.
*/
int halg_link(const int use_hal_mutex,
	      const char *pin_name,
	      const char *sig_name);
static inline int hal_link(const char *pin_name,
			   const char *sig_name){
    return halg_link(1, pin_name, sig_name);
}

/** 'hal_unlink()' unlinks any signal from the specified pin.  'pin_name'
    is a string containing the pin name.
    On success, hal_unlink() returns 0, on failure it
    returns a negative error code.
*/
int halg_unlink(const int use_hal_mutex, const char *pin_name);
static inline int hal_unlink(const char *pin_name) {
    return halg_unlink(1, pin_name);
}

/***********************************************************************
*                     "PARAMETER" FUNCTIONS                            *
************************************************************************/


/** 'hal_param_new()' creates a new 'parameter' object.  It is a generic
    version of the eight functions above.  It is provided ONLY for those
    special cases where a generic function is needed.  It is STRONGLY
    recommended that the functions above be used instead, because they
    check the type of 'data_addr' against the parameter type at compile
    time.  Using this function requires a cast of the 'data_addr' argument
    that defeats type checking and can cause subtle bugs.
    'name', 'data_addr' and 'owner_id' are the same as in the
    functions above.
    'type' is the hal type of the new parameter - the type of data
    that will be stored in the parameter.
    'dir' is the parameter direction.  HAL_RO paramters are read only from
    outside, and are written to by the component itself, typically to provide a
    view "into" the component for testing or troubleshooting.  HAL_RW
    parameters are writable from outside and also sometimes modified by the
    component itself as well.
    If successful, hal_param_new() returns 0.  On failure
    it returns a negative error code.
*/

// v2 base function
hal_param_t *halg_param_newfv(const int use_hal_mutex,
			      hal_type_t type,
			      hal_param_dir_t dir,
			      volatile void *data_addr,
			      int owner_id,
			      const char *fmt, va_list ap);

int hal_param_newf(hal_type_t type,
		   hal_param_dir_t dir,
		   volatile void *data_addr,
		   int owner_id,
		   const char *fmt, ...)
    __attribute__((format(printf,5,6)));

// generic printf-style version of halg_param_newfv()
hal_pin_t * halg_param_newf(const int use_hal_mutex,
		    hal_type_t type,
		    hal_param_dir_t dir,
		    volatile void *data_addr,
		    int owner_id,
		    const char *fmt, ...)
    __attribute__((format(printf,6,7)));

// legacy
static inline int
hal_param_new(const char *name,
	      hal_type_t type,
	      hal_param_dir_t dir,
	      volatile void *data_addr,
	      int owner_id)
{
    return halg_param_newf(1, type,  dir,
			    data_addr, owner_id, "%s", name) == NULL ? _halerrno : 0;
}

/** There is no 'hal_param_delete()' function.  Once a component has
    created a parameter, that parameter remains as long as the
    component exists.  All parameters belonging to a component are
    removed when the component calls 'hal_exit()'.
*/
/** The 'hal_param_xxx_new()' functions create a new 'parameter' object.
    A parameter is a value that is only used inside a component, but may
    need to be initialized or adjusted from outside the component to set
    up the system properly.
    Once a parameter has been created, it's value can be changed using
    the 'hal_param_xxx_set()' functions.
    There are eight functions, one for each of the data types that
    the HAL supports.  Pins may only be linked to signals of the same
    type.
    'name' is the name of the new parameter.  It must be no longer than
    .HAL_NAME_LEN.  If there is already a parameter with the same
    name the call will fail.
    'dir' is the parameter direction.  HAL_RO paramters are read only from
    outside, and are written to by the component itself, typically to provide a
    view "into" the component for testing or troubleshooting.  HAL_RW
    parameters are writable from outside and also sometimes modified by the
    component itself as well.
    'data_addr' is the address where the value of the parameter is to be
    stored.  'data_addr' must point to memory allocated by hal_malloc().
    Typically the component allocates space for a data structure with
    hal_malloc(), and 'data_addr' is the address of a member of that
    structure.  Creating the paremeter does not initialize or modify the
    value at *data_addr - the component should load a reasonable default
    value.
    'owner_id' is the ID of the component that will 'own' the parameter.
    Normally it should be the ID of the caller, but in some cases, a
    user mode component may be doing setup for a realtime component, so
    the ID should be that of the realtime component that will actually
    be using the parameter.
    If successful, the hal_param_xxx_new() functions return 0.
    On failure they return a negative error code.

    use for non-instantiable comps only.

*/

static inline int hal_param_bit_new(const char *name, hal_param_dir_t dir,
				    hal_bit_t * data_addr, int owner_id) {
    return  hal_param_new(name, HAL_BIT, dir,data_addr, owner_id);
}
static inline int hal_param_float_new(const char *name, hal_param_dir_t dir,
    hal_float_t * data_addr, int owner_id) {
    return  hal_param_new(name, HAL_FLOAT, dir,data_addr, owner_id);
}
static inline int hal_param_u32_new(const char *name, hal_param_dir_t dir,
    hal_u32_t * data_addr, int owner_id) {
    return  hal_param_new(name, HAL_U32, dir,data_addr, owner_id);
}
static inline int hal_param_s32_new(const char *name, hal_param_dir_t dir,
    hal_s32_t * data_addr, int owner_id) {
    return  hal_param_new(name, HAL_S32, dir,data_addr, owner_id);
}

/** 'hal_param_new()' creates a new 'parameter' object.  It is a generic
    version of the eight functions above.  It is provided ONLY for those
    special cases where a generic function is needed.  It is STRONGLY
    recommended that the functions above be used instead, because they
    check the type of 'data_addr' against the parameter type at compile
    time.  Using this function requires a cast of the 'data_addr' argument
    that defeats type checking and can cause subtle bugs.
    'name', 'data_addr' and 'owner_id' are the same as in the
    functions above.
    'type' is the hal type of the new parameter - the type of data
    that will be stored in the parameter.
    'dir' is the parameter direction.  HAL_RO paramters are read only from
    outside, and are written to by the component itself, typically to provide a
    view "into" the component for testing or troubleshooting.  HAL_RW
    parameters are writable from outside and also sometimes modified by the
    component itself as well.
    If successful, hal_param_new() returns 0.  On failure
    it returns a negative error code.
*/

/** printf_style-style versions of hal_param_XXX_new */
extern int hal_param_bit_newf(hal_param_dir_t dir,
    hal_bit_t * data_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_param_float_newf(hal_param_dir_t dir,
    hal_float_t * data_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_param_u32_newf(hal_param_dir_t dir,
    hal_u32_t * data_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_param_s32_newf(hal_param_dir_t dir,
    hal_s32_t * data_addr, int owner_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));

/** There is no 'hal_param_delete()' function.  Once a component has
    created a parameter, that parameter remains as long as the
    component exists.  All parameters belonging to a component are
    removed when the component calls 'hal_exit()'.
*/


/***********************************************************************
*                   EXECUTION RELATED FUNCTIONS                        *
************************************************************************/

#ifdef RTAPI

/** hal_export_funct() makes a realtime function provided by a
    component available to the system.  A subsequent call to
    hal_add_funct_to_thread() can be used to schedule the
    execution of the function as needed by the system.
    'name' is the name of the new function.  It must be no longer
    than HAL_NAME_LEN.  This is the name as it would appear in an ini
    file, which does not need to be the same as the C function name.
    'funct' is a pointer to the function code.  'funct' must be
    the address of a function that accepts a void pointer and
    a long int.  The pointer will be set to the value 'arg' below,
    and the long will be set to the thread period in nanoseconds.
    'arg' is a void pointer that will be passed to the function
    each time it is called.  This is useful when one actual
    C function will be exported several times with different HAL
    names, perhaps to deal with multiple instances of a hardware
    device.
    'uses_fp' should be non-zero if the function uses floating
    point.  When in doubt, make it non-zero.  If you are sure
    that the function doesn't use the FPU, then set 'uses_fp'
    to zero.
    'reentrant' should be zero unless the function (and any
    hardware it accesses) is completely reentrant.  If reentrant
    is non-zero, the function may be prempted and called again
    before the first call completes.
    'owner_id' is the ID of the calling component, as returned by
    a call to hal_init().
    On success, hal_export_funct() returns 0, on failure
    it returns a negative error code.
    Call only from realtime init code, not from user space or
    realtime code.
*/
extern int hal_export_funct(const char *name, void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int owner_id);

// printf-style version of the above
int hal_export_functf(void (*funct) (void *, long),
		      void *arg,
		      int uses_fp,
		      int reentrant,
		      int owner_id,
		      const char *fmt, ... )
    __attribute__((format(printf,6,7)));


/** hal_create_thread() establishes a realtime thread that will
    execute one or more HAL functions periodically.
    'name' is the name of the thread, which must be unique in
    the system.  It must be no longer than HAL_NAME_LEN.
    'period_nsec' is the desired period of the thread, in nano-
    seconds.  All threads must run at an integer multiple of the
    fastest thread, and the fastest thread must be created first.
    In general, threads should be created in order, from the
    fastest to the slowest.  HAL assigns decreasing priorities to
    threads that are created later, so creating them from fastest
    to slowest results in rate monotonic priority scheduling,
    usually a good thing.
    'uses_fp' should be non-zero if the thread will call any
    functions that use floating point.  In general, it should
    be non-zero for most threads, with the possible exception
    of the very fastest, most critical thread in a system.
    On success, hal_create_thread() returns a positive integer
    thread ID.  On failure, returns an error code as defined
    above.  Call only from realtime init code, not from user
    space or realtime code.
    cpu_id is intented to bind the thread explicitly to a
    specific CPU id.
*/
extern int hal_create_thread(const char *name, unsigned long period_nsec,
			     int uses_fp, int cpu_id);

// generic. delete a named thread, or all threads if name == NULL
int halg_exit_thread(const int use_hal_mutex, const char *name);

/** hal_thread_delete() deletes a realtime thread.
    'name' is the name of the thread, which must have been created
    by 'hal_create_thread()'.
    On success, hal_thread_delete() returns 0, on
    failure it returns a negative error code.
    Call only from realtime init code, not from user
    space or realtime code.
*/
extern int hal_thread_delete(const char *name);

#endif /* RTAPI */

/** hal_add_funct_to_thread() adds a function exported by a
    realtime HAL component to a realtime thread.  This determines
    how often and in what order functions are executed.
    'funct_name' is the name of the function, as specified in
    a call to hal_export_funct().
    'thread_name' is the name of the thread to which the function
    should be added.  When the thread runs, the functions will
    be executed in the order in which they were added to the
    thread.
    'position' is the desired location within the thread. This
    determines when the function will run, in relation to other
    functions in the thread.  A positive number indicates the
    desired location as measured from the beginning of the thread,
    and a negative is measured from the end.  So +1 means this
    function will become the first one to run, +5 means it will
    be the fifth one to run, -2 means it will be next to last,
    and -1 means it will be last.  Zero is illegal.
    Returns 0, or a negative error code.    Call
    only from within user space or init code, not from
    realtime code.
*/
extern int hal_add_funct_to_thread(const char *funct_name,
				   const char *thread_name,
				   const int position,
				   const int read_barrier,
				   const int write_barrier);

/** hal_del_funct_from_thread() removes a function from a thread.
    'funct_name' is the name of the function, as specified in
    a call to hal_export_funct().
    'thread_name' is the name of a thread which currently calls
    the function.
    Returns 0, or a negative error code.    Call
    only from within user space or init code, not from
    realtime code.
*/
extern int hal_del_funct_from_thread(const char *funct_name, const char *thread_name);

/** hal_start_threads() starts all threads that have been created.
    This is the point at which realtime functions start being called.
    On success it returns 0, on failure a negative
    error code.
*/
extern int hal_start_threads(void);

/** hal_stop_threads() stops all threads that were previously
    started by hal_start_threads().  It should be called before
    any component that is part of a system exits.
    On success it returns 0, on failure a negative
    error code.
*/
extern int hal_stop_threads(void);


// generic vtable methods (locked/unlocked)
int halg_export_vtable(const int use_hal_mutex,
		       const char *name,
		       int version,
		       void *vtref,
		       int comp_id);
int halg_remove_vtable(const int use_hal_mutex, const int vtable_id);
int halg_reference_vtable(const int use_hal_mutex,
			 const char *name,
			 int version,
			  void **vtableref);
int halg_unreference_vtable(const int use_hal_mutex, int vtable_id);
int halg_remove_vtable(const int use_hal_mutex, const int vtable_id);

// returns number of vtables exported by a component.
int halg_count_exported_vtables(const int use_hal_mutex, const int comp_id);

// returns vtable ID (handle, >0) or error code (< 0)
// mark as owned by comp comp_id (optional, zero if not owned)
static inline int hal_export_vtable(const char *name,
				    int version,
				    void *vtable,
				    int comp_id)
{
    return  halg_export_vtable(1, name, version, vtable, comp_id);
}

static inline int hal_remove_vtable(const int vtable_id)
{
    return  halg_remove_vtable(1, vtable_id);
}

// returns vtable_id (handle) or error code
// increases refcount
static inline int hal_reference_vtable(const char *name, int version, void **vtable)
{
    return  halg_reference_vtable(1, name, version, vtable);
}

// drops refcount
static inline int hal_unreference_vtable(int vtable_id)
{
    return  halg_unreference_vtable(1, vtable_id);
}



// call a usrfunct.
// if the return value < 0, this signifies a HAL library error code.
// if the return value is 0, and ureturn is not NULL,
// the usrfunct's return value is stored in *ureturn.
int hal_call_usrfunct(const char *name, const int argc,
                      char * const *argv, int *ureturn);

// public instance API:

// create named instance blob owned by a comp, returns instance ID
// the instance id can be used in lieu of the comp_id params of
// functs,pins and params.
// the corrsponding parameter in the funct/pin/parm calls (formerly
// comp_id) is now called owner_id, and can either originate from
// a hal_init/hal_xinit call, or a hal_inst_create call.
// returns < 0 on error.

int halg_inst_create(const int use_hal_mutex,
				   const char *name,
				   const int comp_id,
				   const int size,
		     void **inst_data);

static inline int hal_inst_create(const char *name,
				  const int comp_id,
				  const int size,
				  void **inst_data) {
    return halg_inst_create(1, name, comp_id, size, inst_data);
}


// delete a named instance.
// unlinks & deletes all pins owned by this instance
// deletes params owned by this instance
// delf's and deletes functs expored by this instance
// returns < 0 on error.
int halg_inst_delete(const int use_hal_mutex, const char *name);
static inline int hal_inst_delete(const char *name) {
    return halg_inst_delete(1, name);
}


// HAL-specific capabilities. Extend as needed.
// capabilities are intended to be added to a binary (a.out, .so, .ko)
// with RTAPI_TAG(HAL, cap1]cap2 ...);

#define HC_INSTANTIABLE 1
#define HC_SINGLETON 2
#define HC_SOMEFEATURE  3

// misc, XXX - move to rtapi_common.c?
char *fmt_ap(char *buffer,
	     size_t size,
	     const char *fmt,
	     va_list ap);

char *fmt_args(char *buffer,
	       size_t size,
	       const char *fmt,
	       ...)  __attribute__((format(printf,3,4)));

RTAPI_END_DECLS

#endif /* HAL_H */
