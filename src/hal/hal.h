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

#ifdef ULAPI
#include <signal.h>
#endif

#include <rtapi_errno.h>

#define HAL_NAME_LEN     47	/* length for pin, signal, etc, names */

/** These locking codes define the state of HAL locking, are used by most functions */
/** The functions locked will return a -EPERM error message **/

#define HAL_LOCK_NONE     0     /* no locking done, any command is permitted */
#define HAL_LOCK_LOAD     1     /* loading rt components is not permitted */
#define HAL_LOCK_CONFIG   2     /* locking of link and addf related commands */
#define HAL_LOCK_PARAMS   4     /* locking of parameter set commands */
#define HAL_LOCK_RUN      8     /* locking of start/stop of HAL threads */

#define HAL_LOCK_ALL      255   /* locks every action */


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
extern int hal_init(const char *name);

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
extern int hal_exit(int comp_id);

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
extern void *hal_malloc(long int size);

/** hal_ready() indicates that this component is ready.  This allows
    halcmd 'loadusr -W hal_example' to wait until the userspace
    component 'hal_example' is ready before continuing.
*/
extern int hal_ready(int comp_id);

/** hal_comp_name() returns the name of the given component, or NULL
    if comp_id is not a loaded component
*/
extern char* hal_comp_name(int comp_id);

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

/* The hal enums are arranged as distinct powers-of-two, so
   that accidental confusion of one type with another (which ought
   to be diagnosed by the type system) can be diagnosed as unexpected
   values.  Note how HAL_RW is an exception to the powers-of-two rule,
   as it is the bitwise OR of HAL_RO and the (nonexistent and nonsensical)
   HAL_WO param direction.
 */

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
    HAL_U32 = 4
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
    HAL_RW = HAL_RO | 128 /* HAL_WO */,
} hal_param_dir_t;

/* Use these for x86 machines, and anything else that can write to
   individual bytes in a machine word. */
#include <rtapi_bool.h>
#include <rtapi_stdint.h>
typedef volatile bool hal_bit_t;
typedef volatile rtapi_u32 hal_u32_t;
typedef volatile rtapi_s32 hal_s32_t;
typedef double real_t __attribute__((aligned(8)));
typedef rtapi_u64 ireal_t __attribute__((aligned(8))); // integral type as wide as real_t / hal_float_t
#define hal_float_t volatile real_t

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
    'comp_id' is the ID of the component that will 'own' the
    variable.  Normally it should be the ID of the caller, but in
    some cases, a user mode component may be doing setup for a
    realtime component, so the ID should be that of the realtime
    component that will actually be using the pin.
    If successful, the hal_pin_xxx_new() functions return 0.
    On failure they return a negative error code.
*/
extern int hal_pin_bit_new(const char *name, hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int comp_id);
extern int hal_pin_float_new(const char *name, hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int comp_id);
extern int hal_pin_u32_new(const char *name, hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int comp_id);
extern int hal_pin_s32_new(const char *name, hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int comp_id);

/** The hal_pin_XXX_newf family of functions are similar to
    hal_pin_XXX_new except that they also do printf-style formatting to compute
    the pin name
    If successful, the hal_pin_xxx_newf() functions return 0.
    On failure they return a negative error code.
*/
extern int hal_pin_bit_newf(hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_float_newf(hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_u32_newf(hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_s32_newf(hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));


/** 'hal_pin_new()' creates a new 'pin' object.  It is a generic
    version of the eight functions above.  It is provided ONLY for
    those special cases where a generic function is needed.  It is
    STRONGLY recommended that the functions above be used instead,
    because they check the type of 'data_ptr_addr' against the pin
    type at compile time.  Using this function requires a cast of
    the 'data_ptr_addr' argument that defeats type checking and can
    cause subtle bugs.
    'name', 'dir', 'data_ptr_addr' and 'comp_id' are the same as in
    the functions above.
    'type' is the hal type of the new pin - the type of data that
    will be passed in/out of the component through the new pin.
    If successful, hal_pin_new() returns 0.  On failure
    it returns a negative error code.
*/
extern int hal_pin_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
    void **data_ptr_addr, int comp_id);

/** There is no 'hal_pin_delete()' function.  Once a component has
    created a pin, that pin remains as long as the component exists.
    All pins belonging to a component are removed when the component
    calls 'hal_exit()'.
*/

/** 'hal_pin_alias()' assigns an alternate name, aka an alias, to
    a pin.  Once assigned, the pin can be referred to by either its
    original name or the alias.  Calling this function with 'alias'
    set to NULL will remove any existing alias.
*/
extern int hal_pin_alias(const char *pin_name, const char *alias);


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
extern int hal_signal_new(const char *name, hal_type_t type);

/** 'hal_signal_delete()' deletes a signal object.  Any pins linked to
    the object are unlinked.
    'name' is the name of the signal to be deleted.
    If successful, 'hal_signal_delete()' returns 0.  On
    failure, it returns a negative error code.
*/
extern int hal_signal_delete(const char *name);

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
extern int hal_link(const char *pin_name, const char *sig_name);

/** 'hal_unlink()' unlinks any signal from the specified pin.  'pin_name'
    is a string containing the pin name.
    On success, hal_unlink() returns 0, on failure it
    returns a negative error code.
*/
extern int hal_unlink(const char *pin_name);

/***********************************************************************
*                     "PARAMETER" FUNCTIONS                            *
************************************************************************/

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
    'dir' is the parameter direction.  HAL_RO parameters are read only from
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
    'comp_id' is the ID of the component that will 'own' the parameter.
    Normally it should be the ID of the caller, but in some cases, a
    user mode component may be doing setup for a realtime component, so
    the ID should be that of the realtime component that will actually
    be using the parameter.
    If successful, the hal_param_xxx_new() functions return 0.
    On failure they return a negative error code.
*/
extern int hal_param_bit_new(const char *name, hal_param_dir_t dir,
    hal_bit_t * data_addr, int comp_id);
extern int hal_param_float_new(const char *name, hal_param_dir_t dir,
    hal_float_t * data_addr, int comp_id);
extern int hal_param_u32_new(const char *name, hal_param_dir_t dir,
    hal_u32_t * data_addr, int comp_id);
extern int hal_param_s32_new(const char *name, hal_param_dir_t dir,
    hal_s32_t * data_addr, int comp_id);

/** printf_style-style versions of hal_param_XXX_new */
extern int hal_param_bit_newf(hal_param_dir_t dir, 
    hal_bit_t * data_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_param_float_newf(hal_param_dir_t dir,
    hal_float_t * data_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_param_u32_newf(hal_param_dir_t dir,
    hal_u32_t * data_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_param_s32_newf(hal_param_dir_t dir,
    hal_s32_t * data_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));


/** 'hal_param_new()' creates a new 'parameter' object.  It is a generic
    version of the eight functions above.  It is provided ONLY for those
    special cases where a generic function is needed.  It is STRONGLY
    recommended that the functions above be used instead, because they
    check the type of 'data_addr' against the parameter type at compile
    time.  Using this function requires a cast of the 'data_addr' argument
    that defeats type checking and can cause subtle bugs.
    'name', 'data_addr' and 'comp_id' are the same as in the
    functions above.
    'type' is the hal type of the new parameter - the type of data
    that will be stored in the parameter.
    'dir' is the parameter direction.  HAL_RO parameters are read only from
    outside, and are written to by the component itself, typically to provide a
    view "into" the component for testing or troubleshooting.  HAL_RW
    parameters are writable from outside and also sometimes modified by the
    component itself as well.
    If successful, hal_param_new() returns 0.  On failure
    it returns a negative error code.
*/
extern int hal_param_new(const char *name, hal_type_t type, hal_param_dir_t dir,
    void *data_addr, int comp_id);

/** There is no 'hal_param_delete()' function.  Once a component has
    created a parameter, that parameter remains as long as the
    component exists.  All parameters belonging to a component are
    removed when the component calls 'hal_exit()'.
*/

/** The 'hal_param_xxx_set()' functions modify the value of a parameter.
    'name' is the name of the parameter that is to be set.  The
    parameter type must match the function type, and the parameter
    must not be read-only.
    'value' is the value to be loaded into the parameter.
    On success, the hal_param_xxx_set() functions return 0,
    and on failure they return a negative error code.
*/
extern int hal_param_bit_set(const char *name, int value);
extern int hal_param_float_set(const char *name, double value);
extern int hal_param_u32_set(const char *name, unsigned long value);
extern int hal_param_s32_set(const char *name, signed long value);

/** 'hal_param_alias()' assigns an alternate name, aka an alias, to
    a parameter.  Once assigned, the parameter can be referred to by
    either its original name or the alias.  Calling this function
    with 'alias' set to NULL will remove any existing alias.
*/
extern int hal_param_alias(const char *pin_name, const char *alias);

/** 'hal_param_set()' is a generic function that sets the value of a
    parameter.  It is provided ONLY for those special cases where a
    generic function is needed.  It is STRONGLY recommended that the
    functions above be used instead, because they are simpler and less
    prone to errors.
    'name', is the same as in the functions above.
    'type' is the hal type of the the data at *value_addr, and must
    match the type of the parameter.  The parameter must not be
    read only.
    'value_addr' is a pointer to the new value of the parameter.
    The data at that location will be interpreted according to the
    type of the parameter.
    If successful, hal_param_set() returns 0.  On failure
    it returns a negative error code.
*/
extern int hal_param_set(const char *name, hal_type_t type, void *value_addr);

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
    'comp_id' is the ID of the calling component, as returned by
    a call to hal_init().
    On success, hal_export_funct() returns 0, on failure
    it returns a negative error code.
    Call only from realtime init code, not from user space or
    realtime code.
*/
extern int hal_export_funct(const char *name, void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id);

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
*/
extern int hal_create_thread(const char *name, unsigned long period_nsec,
    int uses_fp);

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
extern int hal_add_funct_to_thread(const char *funct_name, const char *thread_name,
    int position);

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

/** HAL 'constructor' typedef
    If it is not NULL, this points to a function which can construct a new
    instance of its component.  Return value is >=0 for success,
    <0 for error.
*/
typedef int(*constructor)(char *prefix, char *arg);

/** hal_set_constructor() sets the constructor function for this component
*/
extern int hal_set_constructor(int comp_id, constructor make);

union hal_stream_data {
    real_t f;
    bool b;
    int32_t s;
    uint32_t u;
};

typedef struct {
    int comp_id, shmem_id;
    struct hal_stream_shm *fifo;
} hal_stream_t;

/**
 * HAL streams are modeled after sampler/stream and will hopefully replace
 * the independent implementations there.
 *
 * There may only be one reader and one writer but this is not enforced
 */

#define HAL_STREAM_MAX_PINS (21)
/** create and attach a stream */
extern int hal_stream_create(hal_stream_t *stream, int comp, int key, int depth, const char *typestring);
/** detach and destroy an open stream */
extern void hal_stream_destroy(hal_stream_t *stream);

/** attach to an existing stream */
extern int hal_stream_attach(hal_stream_t *stream, int comp, int key, const char *typestring);
/** detach from an open stream */
extern int hal_stream_detach(hal_stream_t *stream);

/** stream introspection */
extern int hal_stream_element_count(hal_stream_t *stream);
extern hal_type_t hal_stream_element_type(hal_stream_t *stream, int idx);

// only one reader and one writer is allowed.
extern int hal_stream_read(hal_stream_t *stream, union hal_stream_data *buf, unsigned *sampleno);
extern bool hal_stream_readable(hal_stream_t *stream);
extern int hal_stream_depth(hal_stream_t *stream);
extern int hal_stream_maxdepth(hal_stream_t *stream);
extern int hal_stream_num_underruns(hal_stream_t *stream);
extern int hal_stream_num_overruns(hal_stream_t *stream);
#ifdef ULAPI
extern void hal_stream_wait_readable(hal_stream_t *stream, sig_atomic_t *stop);
#endif

extern int hal_stream_write(hal_stream_t *stream, union hal_stream_data *buf);
extern bool hal_stream_writable(hal_stream_t *stream);
#ifdef ULAPI
extern void hal_stream_wait_writable(hal_stream_t *stream, sig_atomic_t *stop);
#endif

RTAPI_END_DECLS

#endif /* HAL_H */
