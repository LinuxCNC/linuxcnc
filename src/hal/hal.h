#ifndef __LINUXCNC_HAL_H
#define __LINUXCNC_HAL_H

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

#include "rtapi.h"
RTAPI_BEGIN_DECLS

#if ( !defined RTAPI ) && ( !defined ULAPI )
#error HAL needs RTAPI/ULAPI, check makefile and flags
#endif

#ifdef ULAPI
#include <signal.h>
#endif

#include "rtapi_stdint.h"
#include "rtapi_bool.h"
#include "rtapi_errno.h"

#define HAL_NAME_LEN     55	/* length for pin, signal, etc, names */
#define HAL_PSEUDO_COMP_PREFIX "__" /* prefix to identify a pseudo component */

/** These locking codes define the state of HAL locking, are used by most functions */
/** The functions locked will return a -EPERM error message **/

#define HAL_LOCK_NONE     0     /* no locking done, any command is permitted */
#define HAL_LOCK_LOAD     1     /* loading rt components is not permitted */
#define HAL_LOCK_CONFIG   2     /* locking of link and addf related commands */
#define HAL_LOCK_PARAMS   4     /* locking of parameter set commands */
#define HAL_LOCK_RUN      8     /* locking of start/stop of HAL threads */

/* locks required for the 'tune' command */
#define HAL_LOCK_TUNE (HAL_LOCK_LOAD | HAL_LOCK_CONFIG)

#define HAL_LOCK_ALL      255   /* locks every action */

/***********************************************************************
*                   GENERAL PURPOSE FUNCTIONS                          *
************************************************************************/

// hal_is_init() returns the current state of HAL for the calling process. It
// determines whether hal_init() has been called by checking whether the shared
// memory segment is present.
// The return value is one (1) if HAL is initialized and zero (0) if not.
int hal_is_init(void);

#ifdef ULAPI
// 'hal_lib_init()' will register an rtapi application (HAL_LIB_<pid>) and map
// the shared HAL memory segment.
// Only user-space applications linking to hal_lib can initialize the library
// in this way. This is useful for any program wishing to do set[ps], get[ps]
// and the like, which do not need a component. Any hal_lib function that does
// not require an associated component can be executed with HAL shared memory
// mapped.
// Note: A caller who creates a component with hal_init() does not need to call
//       hal_lib_init(). The initialization is automatically performed on
//       component creation.
//
// 'hal_lib_exit()' will unmap the shared HAL memory segment and de-register
// the rtapi application (HAL_LIB_<pid>).
// The de-initialization will only occur if no references are left in this
// instance. A reference is any call to hal_init() with an unmatched hal_exit()
// (i.e. a component exists). An error message will be emitted when you call
// hal_lib_exit() while there still are components referenced.
//
// Only user-space applications linking to hal_lib can de-init the library.
// This is useful for any program that accesses HAL constructs but does itself
// not need any components.
// Note: A caller is not required to call hal_lib_exit() when the library was
//       finalized through a call matching call pair to hal_init()/hal_exit.
//       The last component exit will invoke hal_lib_exit() automatically.
//
int  hal_lib_init(void);
void hal_lib_exit(void);
#endif

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
    rtapi functions should not be called afterwards.
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

/** hal_set_unready() sets a component state to unready so
    additional pins can be added.  A subsequent call to
    hal_ready() must be issued to make the component ready
    again. Kinematics modules created with halcompile use
    this function to add pins to a parent component.
*/
extern int hal_set_unready(int comp_id);

/** hal_unready() indicates that this component is ready.  This allows
    halcmd 'loadusr -W hal_example' to wait until the userspace
    component 'hal_example' is ready before continuing.
*/
extern int hal_unready(int comp_id);

// hal_strerror() returns a brief textual description string about the error
// identified. The argument should be the negative errno value, as returned by
// most HAL functions.
const char *hal_strerror(int err);

/** hal_comp_name() returns the name of the given component, or NULL
    if comp_id is not a loaded component
*/
extern const char *hal_comp_name(int comp_id);

/** hal_get_realtime_type() returns the type of the running real time
*/
typedef rtapi_realtime_type_t hal_realtime_type_t;
extern hal_realtime_type_t hal_get_realtime_type(void);

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
    HAL_TYPE_UNINITIALIZED = 0,
    HAL_BIT = 1,
    HAL_FLOAT = 2,
    HAL_S32 = 3,
    HAL_U32 = 4,
    HAL_PORT = 5,
    HAL_S64 = 6,
    HAL_U64 = 7,
    HAL_TYPE_MAX,
} hal_type_t;

#define HAL_BOOL HAL_BIT
#define HAL_REAL HAL_FLOAT
#define HAL_SINT HAL_S64
#define HAL_UINT HAL_U64

//
// hal_pdir_t - Unified HAL pin/param direction type. Specifies the direction
// of the pins and params while simultaneously allowing us to deduce whether we
// are dealing with a pin or a param.
//
// HAL pins have a direction attribute. A pin may be an input to the HAL
// component, an output, or it may be bidirectional. Any number of HAL_IN or
// HAL_IO pins may be connected to the same signal, but only one HAL_OUT pin is
// permitted. This is equivalent to connecting two output pins together in an
// electronic circuit. (HAL_IO pins can be thought of as tri-state outputs.)
//
// HAL parameters also have a direction attribute. For parameters, the
// attribute determines whether the user can write the value of the parameter,
// or simply read it. HAL_RO parameters are read-only, and HAL_RW ones are
// writable with 'halcmd setp'.
//
typedef enum {
    HAL_DIR_UNSPECIFIED = -1,
    HAL_IN  = (1 << 4),
    HAL_OUT = (1 << 5),
    HAL_IO  = (HAL_IN | HAL_OUT),
    HAL_RO  = (1 << 6),
    HAL_WO  = (1 << 7), // Actually fake value not enforced
    HAL_RW  = (HAL_RO | HAL_WO),
} hal_pdir_t;

// Map both old direction types to the new combined type
// FIXME: These should be retired at some point
typedef hal_pdir_t hal_pin_dir_t;
typedef hal_pdir_t hal_param_dir_t;

#define __HAL_ALWAYS_INLINE __attribute__((always_inline))

//
// bool hal_pdir_is_pin(hal_pdir_t)
// bool hal_pdir_is_param(hal_pdir_t)
// bool hal_pdir_is_neither(hal_pdir_t)
//
// Determine whether an I/O direction is a pin, a param or neither.
//
static inline __HAL_ALWAYS_INLINE bool hal_pdir_is_pin(hal_pdir_t v) {
    // No other bits than in HAL_IO may be set
    return (0 == (v & ~HAL_IO)) && (0 != (v & HAL_IO));
}
static inline __HAL_ALWAYS_INLINE bool hal_pdir_is_param(hal_pdir_t v) {
    // No other bits than in HAL_RW may be set
    return (0 == (v & ~HAL_RW)) && (0 != (v & HAL_RW));
}
static inline __HAL_ALWAYS_INLINE bool hal_pdir_is_neither(hal_pdir_t v) {
    // Any other bits than in HAL_IO|HAL_RW set or none of the set's bits
    return (0 != (v & ~(HAL_IO|HAL_RW))) || (0 == (v & (HAL_IO|HAL_RW)));
}

// FIXME: These alignment attributes should be removed.
// HAL now allocates on an 8-byte boundary and the rest should be left to the
// compiler.
// ==> Remove when we get rid of old hal_*_t typedefs. <==
typedef rtapi_real real_t;
typedef rtapi_u64 ireal_t __attribute__((aligned(8))) __attribute__((deprecated)); // integral type as wide as real_t / hal_float_t

typedef volatile bool hal_bit_t;
typedef volatile rtapi_u32 hal_u32_t;
typedef volatile rtapi_s32 hal_s32_t;
typedef volatile rtapi_u64 hal_u64_t;
typedef volatile rtapi_s64 hal_s64_t;
typedef volatile rtapi_real hal_float_t;
typedef volatile rtapi_port hal_port_t;
       
/** HAL "data union" structure
 ** This structure may hold any type of hal data
*/
typedef union {
    hal_bit_t b;
    hal_s32_t s;
    hal_u32_t u;
    hal_float_t f;
    hal_port_t p;
    hal_s64_t ls;
    hal_u64_t lu;
} hal_data_u;

// Fake forward declarations so we can make opaque pointers
struct __hal_stype_bool_t;
struct __hal_stype_sint_t;
struct __hal_stype_uint_t;
struct __hal_stype_real_t;
struct __hal_stype_port_t;

typedef struct __hal_stype_bool_t *hal_bool_t;
typedef struct __hal_stype_sint_t *hal_sint_t;
typedef struct __hal_stype_uint_t *hal_uint_t;
typedef struct __hal_stype_real_t *hal_real_t;
//typedef struct __hal_stype_port_t *hal_port_t;

typedef union {
    hal_bool_t b;
    hal_sint_t s;
    hal_uint_t u;
    hal_real_t r;
    //hal_port_t p;
} hal_refs_u;

// We rely on little-endian memory layout in the union where the smaller
// types are overlapping the larger type's least significant part.
#include "rtapi_byteorder.h"

// The 'defined()' clause is specifically added for cppcheck 2.13.0, used in
// Ubuntu 24.04, which appears to fail somewhere in including rtapi_byteorder.h
// and hits the #error directive.
#if defined(RTAPI_LITTLE_ENDIAN) && !RTAPI_LITTLE_ENDIAN
#error "HAL only supports little endian machines at this moment."
#endif

// This is a define so we don't export it to other code.
// It is undef'ed after we're done with it.
// FIXME: Get rid of the 32-bit types when we have upgraded everything using
// getter/setter access only so we have guaranteed content.
#define __HAL_MAPPED_TYPE union __hal_mapped_type { \
        volatile rtapi_bool _b; \
        volatile rtapi_s32  _ss; \
        volatile rtapi_u32  _su; \
        volatile rtapi_sint _s; \
        volatile rtapi_uint _u; \
        volatile rtapi_real _r; \
    }


#if 0
// The port change must be done later
// A 'hal_port_t' is a pin/param reference which content represents
// the integer offset in the HAL shared memory segment to a
// hal_port_shm_t structure.
static inline __HAL_ALWAYS_INLINE rtapi_sint hal_get_port(hal_port_t ref) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    return ((union __hal_mapped_type *)ref)->_s;
}
static inline __HAL_ALWAYS_INLINE rtapi_sint hal_set_port(hal_port_t ref, rtapi_sint val) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    ((union __hal_mapped_type *)ref)->_s = val; // Store in the larger type
    return val;
}
#endif
//
// The hal_{get,set}_si32() and hal_{get,set}_ui32() are only present for
// compatibility. They may be removed when all the remaining code has been
// updated properly. However, there is a case for letting them remain as
// they will simply use implicit truncation.
// The hal_get_{s,u}i32_clamped() functions will not truncate but clamp the
// read value to the appropriate min/max of the 32-bit type.
//
static inline __HAL_ALWAYS_INLINE rtapi_s32 hal_get_si32_clamped(const hal_sint_t ref) {
    __HAL_MAPPED_TYPE;
    // Down conversion from the larger type
    // cppcheck-suppress dangerousTypeCast
    rtapi_sint val = ((union __hal_mapped_type *)ref)->_s;
    if(val <= RTAPI_INT32_MIN) return RTAPI_INT32_MIN;
    if(val >= RTAPI_INT32_MAX) return RTAPI_INT32_MAX;
    return (rtapi_s32)val;
}
static inline __HAL_ALWAYS_INLINE rtapi_s32 hal_get_si32(const hal_sint_t ref) {
    __HAL_MAPPED_TYPE;
    // Implicitly Truncated from the larger type
    // cppcheck-suppress dangerousTypeCast
    return ((union __hal_mapped_type *)ref)->_ss;
}
static inline __HAL_ALWAYS_INLINE rtapi_s32 hal_set_si32(hal_sint_t ref, rtapi_s32 val) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    ((union __hal_mapped_type *)ref)->_s = val; // Store in the larger type
    return val;
}
static inline __HAL_ALWAYS_INLINE rtapi_u32 hal_get_ui32_clamped(const hal_uint_t ref) {
    __HAL_MAPPED_TYPE;
    // Down conversion from the larger type
    // cppcheck-suppress dangerousTypeCast
    rtapi_uint val = ((union __hal_mapped_type *)ref)->_u;
    if(val >= RTAPI_UINT32_MAX) return RTAPI_UINT32_MAX;
    return (rtapi_u32)val;
}
static inline __HAL_ALWAYS_INLINE rtapi_u32 hal_get_ui32(const hal_uint_t ref) {
    __HAL_MAPPED_TYPE;
    // Implicitly Truncated from the larger type
    // cppcheck-suppress dangerousTypeCast
    return ((union __hal_mapped_type *)ref)->_su;
}
static inline __HAL_ALWAYS_INLINE rtapi_u32 hal_set_ui32(hal_uint_t ref, rtapi_u32 val) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    ((union __hal_mapped_type *)ref)->_u = val; // Store in the larger type
    return val;
}
static inline __HAL_ALWAYS_INLINE rtapi_sint hal_get_sint(const hal_sint_t ref) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    return ((union __hal_mapped_type *)ref)->_s;
}
static inline __HAL_ALWAYS_INLINE rtapi_sint hal_set_sint(hal_sint_t ref, rtapi_sint val) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    ((union __hal_mapped_type *)ref)->_s = val;
    return val;
}
static inline __HAL_ALWAYS_INLINE rtapi_uint hal_get_uint(const hal_uint_t ref) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    return ((union __hal_mapped_type *)ref)->_u;
}
static inline __HAL_ALWAYS_INLINE rtapi_uint hal_set_uint(hal_uint_t ref, rtapi_uint val) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    ((union __hal_mapped_type *)ref)->_u = val;
    return val;
}
static inline __HAL_ALWAYS_INLINE rtapi_real hal_get_real(const hal_real_t ref) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    return ((union __hal_mapped_type *)ref)->_r;
}
static inline __HAL_ALWAYS_INLINE rtapi_real hal_set_real(hal_real_t ref, rtapi_real val) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    ((union __hal_mapped_type *)ref)->_r = val;
    return val;
}
static inline __HAL_ALWAYS_INLINE rtapi_bool hal_get_bool(const hal_bool_t ref) {
    __HAL_MAPPED_TYPE;
    // cppcheck-suppress dangerousTypeCast
    return ((union __hal_mapped_type *)ref)->_b;
}
static inline __HAL_ALWAYS_INLINE rtapi_bool hal_set_bool(hal_bool_t ref, rtapi_bool val) {
    __HAL_MAPPED_TYPE;
    // 'val' is declared bool and will therefore store a one (1)
    // or a zero (0) in the larger target. This still works if the
    // call is made using an integer type as original argument.
    // cppcheck-suppress dangerousTypeCast
    ((union __hal_mapped_type *)ref)->_u = val;
    return val;
}
#undef __HAL_ALWAYS_INLINE
#undef __HAL_MAPPED_TYPE

#define __HAL_PFMT(a,b) __attribute__((format(printf,a,b)))
int hal_pin_new_bool(int compid, hal_pdir_t dir, hal_bool_t *ref, rtapi_bool def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_pin_new_si32(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_s32  def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_pin_new_ui32(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_u32  def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_pin_new_sint(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_sint def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_pin_new_uint(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_uint def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_pin_new_real(int compid, hal_pdir_t dir, hal_real_t *ref, rtapi_real def, const char *fmt, ...) __HAL_PFMT(5,6);
// Note: port has no initial default as it is an 'internal' reference
// FIXME: This needs to change into hal_port_t argument when we break the API
int hal_pin_new_port(int compid, hal_pin_dir_t dir, hal_sint_t *ref, const char *fmt, ...) __HAL_PFMT(4,5);

int hal_param_new_bool(int compid, hal_pdir_t dir, hal_bool_t *ref, rtapi_bool def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_param_new_si32(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_s32  def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_param_new_ui32(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_u32  def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_param_new_sint(int compid, hal_pdir_t dir, hal_sint_t *ref, rtapi_sint def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_param_new_uint(int compid, hal_pdir_t dir, hal_uint_t *ref, rtapi_uint def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_param_new_real(int compid, hal_pdir_t dir, hal_real_t *ref, rtapi_real def, const char *fmt, ...) __HAL_PFMT(5,6);
int hal_param_new_fake(int compid, hal_refs_u *refs);
#undef __HAL_PFMT

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
extern int hal_pin_u64_new(const char *name, hal_pin_dir_t dir,
    hal_u64_t ** data_ptr_addr, int comp_id);
extern int hal_pin_s64_new(const char *name, hal_pin_dir_t dir,
    hal_s64_t ** data_ptr_addr, int comp_id);
extern int hal_pin_port_new(const char *name, hal_pin_dir_t dir,
    hal_port_t ** data_ptr_addr, int comp_id);


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
extern int hal_pin_u64_newf(hal_pin_dir_t dir,
    hal_u64_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_s64_newf(hal_pin_dir_t dir,
    hal_s64_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_pin_port_newf(hal_pin_dir_t dir,
    hal_port_t** data_ptr_addr, int comp_id, const char *fmt, ...)
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
    structure.  Creating the parameter does not initialize or modify the
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
extern int hal_param_u64_new(const char *name, hal_param_dir_t dir,
    hal_u64_t * data_addr, int comp_id);
extern int hal_param_s64_new(const char *name, hal_param_dir_t dir,
    hal_s64_t * data_addr, int comp_id);

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
extern int hal_param_u64_newf(hal_param_dir_t dir,
    hal_u64_t * data_addr, int comp_id, const char *fmt, ...)
	__attribute__((format(printf,4,5)));
extern int hal_param_s64_newf(hal_param_dir_t dir,
    hal_s64_t * data_addr, int comp_id, const char *fmt, ...)
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
int hal_param_bit_set(const char *name, int value) __attribute__((deprecated("Use hal_set_p()")));
int hal_param_float_set(const char *name, double value) __attribute__((deprecated("Use hal_set_p()")));
int hal_param_u32_set(const char *name, unsigned long value) __attribute__((deprecated("Use hal_set_p()")));
int hal_param_s32_set(const char *name, signed long value) __attribute__((deprecated("Use hal_set_p()")));
int hal_param_u64_set(const char *name, unsigned long value) __attribute__((deprecated("Use hal_set_p()")));
int hal_param_s64_set(const char *name, signed long value) __attribute__((deprecated("Use hal_set_p()")));

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
int hal_param_set(const char *name, hal_type_t type, void *value_addr) __attribute__((deprecated("Use hal_set_p()")));

/***********************************************************************
*                 PIN/SIG/PARAM GETTER FUNCTIONS                       *
************************************************************************/

/** 'hal_get_pin_value_by_name()' gets the value of any arbitrary HAL pin by
 * pin name.
 *
 * The 'type' and 'data' args are pointers to the returned values.  The function
 * returns 0 if successful, or -1 on error.  If 'connected' is non-NULL, its
 * value will be true if a signal is connected.
 */

extern int hal_get_pin_value_by_name(
    const char *name, hal_type_t *type, hal_data_u **data, bool *connected) __attribute__((deprecated("Use hal_get_p()")));

/** 'hal_get_signal_value_by_name()' returns the value of any arbitrary HAL
 * signal by signal name.
 *
 * The 'type' and 'data' args are pointers to the returned values.  The function
 * returns 0 if successful, or -1 on error.  If 'has_writers' is non-NULL, its
 * value will be true if the signal has writers.
 */

extern int hal_get_signal_value_by_name(
    const char *name, hal_type_t *type, hal_data_u **data, bool *has_writers) __attribute__((deprecated("Use hal_get_s()")));

/** 'hal_get_param_value_by_name()' returns the value of any arbitrary HAL
 * parameter by parameter name.
 *
 * The 'type' and 'data' args are pointers to the returned values.  The function
 * returns 0 if successful, or -1 on error.
 */

extern int hal_get_param_value_by_name(
    const char *name, hal_type_t *type, hal_data_u **data) __attribute__((deprecated("Use hal_get_p()")));


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
    'uses_fp' is deprecated and ignored.  All threads now
    unconditionally save and restore FPU/SSE state.  This
    parameter will be removed in a future version.
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

/** hal_export_functf is similar to hal_export_funct except that it also does
    printf-style formatting to compute the function name.
    If successful, it returns 0.
    On failure it returns a negative error code.
*/
extern int hal_export_functf(void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id, const char *fmt, ...);

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
    'uses_fp' is deprecated and ignored.  All threads now
    unconditionally save and restore FPU/SSE state.  This
    parameter will be removed in a future version.
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

/** hal_init_funct_to_thread() registers a function to run exactly once,
    in the realtime context of 'thread_name', before the thread executes
    any cyclic (addf-registered) function. The init list is invoked in a
    dedicated "special cycle" the first time the thread observes
    threads_running == 1; the cyclic funct list is skipped during that
    cycle. After the init list returns, the thread's period is re-anchored
    so the next cyclic cycle wakes one full period later, which both
    avoids the spurious "unexpected realtime delay" warning that would
    otherwise follow a long init and gives the cyclic pass a clean
    starting boundary.
    'position' uses the same semantics as hal_add_funct_to_thread():
    positive values count from the start of the init list (+1 runs first),
    negative values count from the end (-1 runs last); 0 is illegal.
    Calls made after the init cycle has already run return -EALREADY and
    have no effect.
    Returns 0, -EALREADY, or a negative error code. Call only from user
    space or init code, not from realtime code. */
extern int hal_init_funct_to_thread(const char *funct_name, const char *thread_name, int position);

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
typedef int(*constructor)(const char *prefix, const char *arg);

/** hal_set_constructor() sets the constructor function for this component
*/
extern int hal_set_constructor(int comp_id, constructor make);



/******************************************************************************
  A HAL port pin is an asynchronous one way byte stream
  
  A hal port should have only one reader and one writer. Both sides can
  read or write respectively at any time without interfering with the other
 
  A component that exports a PORT pin does not own the port buffer.
  The signal linking an input port to an output port owns the port buffer.
  The buffer is allocated by issuing a 'halcmd sets port-sig size'
  command.
*/

#define HAL_PORT_SIZE_MAX 65536

/** hal_port_read reads count bytes from the port into dest.
    This function should only be called by the component that owns 
    the IN PORT pin.
    returns
        true: count bytes were read into dest
        false: no bytes were read into dest
 */
extern bool hal_port_read(const hal_port_t *port, char* dest, unsigned count);


/** hal_port_peek operates the same as hal_port_read but no bytes are consumed
    from the input port. Repeated calls to hal_port_peek will return the same data.
    This function should only be called by the component that owns the IN PORT pin.
    returns 
        true: count bytes were read into dest
        false: no bytes were read into dest
*/
extern bool hal_port_peek(const hal_port_t *port, char* dest, unsigned count);

/** hal_port_peek_commit advances the read position in the port buffer
    by count bytes. A hal_port_peek followed by a hal_port_peek_commit
    with the same count value would function equivalently to 
    hal_port_read given the same count value. This function should only
    be called by the component that owns the IN PORT pin.
    returns:
       true: count readable bytes were skipped and are no longer accessible
       false: no bytes wer skipped
*/ 
extern bool hal_port_peek_commit(const hal_port_t *port, unsigned count);

/** hal_port_write writes count bytes from src into the port. 
    This function should only be called by the component that owns
    the OUT PORT pin.
    returns:
        true: count bytes were written
        false: no bytes were written into dest
    
*/
extern bool hal_port_write(const hal_port_t *port, const char* src, unsigned count);

/** hal_port_readable returns the number of bytes available
    for reading from the port.
*/
extern unsigned hal_port_readable(const hal_port_t *port);

/** hal_port_writable returns the number of bytes that
    can be written into the port
*/
extern unsigned hal_port_writable(const hal_port_t *port);

/** hal_port_buffer_size returns the total number of bytes
    that a port can buffer
*/
extern unsigned hal_port_buffer_size(const hal_port_t *port);

/** hal_port_clear emptys a given port of all data
    without consuming any of it.
    hal_port_clear should only be called by a reader
*/
extern void hal_port_clear(const hal_port_t *port);


#ifdef ULAPI
/** hal_port_wait_readable spin waits on a port until it has at least 
    count bytes available for reading, or *stop > 0
 */
extern void hal_port_wait_readable(hal_port_t** port, unsigned count, sig_atomic_t* stop);

/** hal_port_wait_writable spin waits on a port until it has at least
    count bytes available for writing or *stop > 0
 */
extern void hal_port_wait_writable(hal_port_t** port, unsigned count, sig_atomic_t* stop);
#endif


/**
 * HAL streams are modeled after sampler/stream and will hopefully replace
 * the independent implementations there.
 *
 * There may only be one reader and one writer but this is not enforced
 */

typedef union hal_stream_data {
    rtapi_real f;
    rtapi_bool b;
    rtapi_s32 s;
    rtapi_u32 u;
    rtapi_sint l;
    rtapi_uint k;
} hal_stream_data_u;
typedef hal_stream_data_u *hal_stream_data_ptr_u;

struct __hal_stream_shm_t;  // Forward declaration. Only relevant in hal_lib.c.

typedef struct __hal_stream_t {
    int comp_id;
    int shmem_id;
    struct __hal_stream_shm_t *fifo;
} hal_stream_t;
typedef hal_stream_t *hal_stream_ptr_t;

#define HAL_STREAM_MAX_PINS (21)
/** create and attach a stream */
extern int hal_stream_create(hal_stream_t *stream, int comp, int key, unsigned depth, const char *typestring);
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
extern int hal_stream_read(hal_stream_t *stream, hal_stream_data_u *buf, unsigned *sampleno);
extern bool hal_stream_readable(hal_stream_t *stream);
extern int hal_stream_depth(hal_stream_t *stream);
extern unsigned hal_stream_maxdepth(hal_stream_t *stream);
extern int hal_stream_num_underruns(hal_stream_t *stream);
extern int hal_stream_num_overruns(hal_stream_t *stream);
#ifdef ULAPI
extern void hal_stream_wait_readable(hal_stream_t *stream, sig_atomic_t *stop);
#endif

extern int hal_stream_write(hal_stream_t *stream, hal_stream_data_u *buf);
extern bool hal_stream_writable(hal_stream_t *stream);
#ifdef ULAPI
extern void hal_stream_wait_writable(hal_stream_t *stream, sig_atomic_t *stop);
#endif


/***********************************************************************
*                    MISC HELPER FUNCTIONS                             *
************************************************************************/


/** HAL_STATIC_ASSERT wrapper for compile time asserts
*/
#if __STDC_VERSION__ >= 202311L || defined(__cplusplus)
#define HAL_STATIC_ASSERT(expression, message) static_assert((expression), message)
#else
/* _Static_assert: GCC extension, in C standard since C11, deprecated in favour of
   static_assert (like C++) from C23 onwards*/
#define HAL_STATIC_ASSERT(expression, message) _Static_assert((expression), message)
#endif


/** hal_extend_counter() extends a counter value with nbits to 64 bits.
    
    For some hardware counters or encoders it may be desireable to
    extend their range beyond their native width.

    This function maintains a 64bit counter value and counts wrap
    arounds. It may be useful to e.g. keep track of full rotations on
    a gray disc absolute encoder.
    
    Changes of hardware encoder between calls to this function need to
    be less than 2**(nbits-1).

    Usage:

    Call with current 64bit counter value to be updated as @param old,
    new low-width counter value read from hardware as @param newlow 
    and width of counter as @param nbits.
    @returns new 64bit counter value which should be stored and
    supplied as old in the next call.
     */
static inline rtapi_s64 hal_extend_counter(rtapi_s64 old, rtapi_s64 newlow, int nbits)
{
    /* Extend low-bit-width counter value to 64bit, counting wrap arounds resp.
       "rotations" in additional bits. 

       see https://github.com/LinuxCNC/linuxcnc/pull/3932#issuecomment-4239206615
       This code avoids branches and undefined behaviour due to signed overflow.
       Idea from MicroPython.

       The tricky part is how to efficiently calculate the difference between
       two counter values that cross from minimum to maximum or vice versa.
       (underflow/overflow).

       To calculate this difference and avoid costly branches and explicit 
       verbose code, the difference is calculated from values shifted to the
       right, the MSB of the counter values are shifted to the MSB of the
       processor registers (64 bit). This difference is automatically
       truncated to 64bit and then shifted back right to its original position
       while preserving the sign (arithmetic right shift). This difference
       is added to the large counter value.
       
       The limit when using N-bit bit-width limited counters is that the maximum
       count difference between subsequent invocation may not be larger than
       2**(nshift-1)-1. Otherwise it is impossible to determine the direction of the
       counter.

       Heuristically, if your encoder can do more than half a rotation between
       calls to this function, it is impossible to deduce which direction it
       went.

       Code contributed by Jeff Epler.

       Example with 3bit absolute hardware encoder and 8bit extended counter
       (nshift would be 5):

       counter at 7, transition from 111 (7) to 000 (0) should increment the
       extended counter to 8.

       call hal_extend_counter(7, 0, 3)
       oldlow_shifted = 7<<5 = 224 (1110 0000)
       newlow_shifted = 0<<5 = 0
       delta_shifted = 0 - 224 = -224 (1 0010 0000) = 32 (0010 0000) truncated to 8 bits 
       old + (32 >> 5) = 7 + 1 = 8
    */

    /* tripwire if somebody tries to use this code on a Cray with wrong
       compiler flags. */

    /* prevent cppcheck to complain about the tripwire */
    /* cppcheck-suppress shiftNegativeLHS */
    HAL_STATIC_ASSERT((-2 >> 1) == -1, 
        "hal_extend_counter impl only works with arithmetic right shift");

    int nshift = 64 - nbits;
    rtapi_u64 oldlow_shifted = ((rtapi_u64)old << nshift);
    rtapi_u64 newlow_shifted = ((rtapi_u64)newlow << nshift);
    rtapi_s64 diff_shifted = newlow_shifted - oldlow_shifted;
    return (rtapi_u64)old + (diff_shifted >> nshift); // unsigned to avoid signed overflow
}

//***********************************************************************
// Mapping/umapping HAL memory segment pointers
//***********************************************************************

// Pointers into HAL memory are dependent on process memory. Transporting them
// between processes does not work. These need to be handled as offsets from
// where the memory is mapped.
rtapi_intptr_t hal_reference_unmap(const void *ref);
void *hal_reference_map(rtapi_intptr_t ref);

//***********************************************************************
//
// User-land only functions to query HAL's internals
//
//***********************************************************************

// HAL 'component' type.
//    Assigned according to RTAPI and ULAPI definitions.
typedef enum {
    HAL_COMP_TYPE_UNKNOWN = -1,
    HAL_COMP_TYPE_USER,
    HAL_COMP_TYPE_REALTIME,
    HAL_COMP_TYPE_OTHER
} hal_comp_type_t;
// These COMPONENT_TYPE_* names are for compatibility. The HAL_COMP_TYPE_*
// versions are better names for what they represent.
#define COMPONENT_TYPE_UNKNOWN  HAL_COMP_TYPE_UNKNOWN
#define COMPONENT_TYPE_USER     HAL_COMP_TYPE_USER
#define COMPONENT_TYPE_REALTIME HAL_COMP_TYPE_REALTIME
#define COMPONENT_TYPE_OTHER    HAL_COMP_TYPE_OTHER

// Only enable the query API when we are compiling the user-land HAL library
#ifdef ULAPI

// Query type of a HAL item
typedef enum {
    HAL_QTYPE_ANY = 0,
    HAL_QTYPE_PIN,
    HAL_QTYPE_PARAM,
    HAL_QTYPE_SIGNAL,
    HAL_QTYPE_COMP,
    HAL_QTYPE_FUNCT,
    HAL_QTYPE_THREAD,
    HAL_QTYPE_THREAD_FUNCT,
} hal_qtype_t;

typedef struct {
    int comp_id;          // Return: Component ID (RTAPI module id)
    hal_comp_type_t type; // Return: Component type (name in query struct)
    int pid;              // Return: PID of component (user components only)
    bool ready;           // Return: True if ready, false if not
    const char *insmod;   // Return: Arguments passed via insmod or NULL if none present
} hal_query_comp_t;

typedef struct {
    int comp_id;          // Return: Component ID
    const char *comp;     // Return: Component's name
    int users;            // Return: Number of threads using function
    rtapi_intptr_t funct; // Return: Pointer to function code
    rtapi_intptr_t arg;   // Return: Argument for function
    bool reentrant;       // Return: True if function is re-entrant
} hal_query_funct_t;

typedef struct {
    int comp_id;          // Return: Owning component
    const char *comp;     // Return: Component's name
    int priority;         // Return: Thread priority
    long int period;      // Return: Thread period in nsec
    int functidx;         // Return: Function iteration counter
    const char *funct;    // Return: Attached function name
    bool is_init;         // Return: True if funct is an init function
} hal_query_thread_t;

typedef union {
    rtapi_bool b;   // values used in hal_[gs]et_[ps]
    rtapi_sint s;
    rtapi_uint u;
    rtapi_real r;
} hal_query_value_u;

typedef struct {
    hal_type_t type;    // Request: Enforce specific type (any when == 0);
                        // Return: HAL_XXX type
    hal_refs_u ref;     // Return: Value reference
    hal_query_value_u value; // Request: Set in the callback or in advance for set_p with enforced type;
                             // Return(get_p): value read
    hal_pdir_t dir;     // Return: pin/param direction
    const char *alias;  // Return: non-NULL if there is an alias name
    const char *signal; // Return: get_p/getref_p/list_p signal name if connected
    const char *comp;   // Return: Owner's name (component name)
    int comp_id;        // Return: Owner ID
} hal_query_pp_t;

typedef struct {
    hal_type_t type;    // Request: Enforce specific type (any when == 0);
                        // Return: HAL_XXX type
    hal_refs_u ref;     // Return: Value reference
    hal_query_value_u value; // Request: Set in the callback or in advance for set_s with enforced type;
                             // Return(get_s): value read
    int writers;        // Return: Number of writer pins attached to the signal
    int readers;        // Return: Number of reader pins attached to the signal
    int bidirs;         // Return: Number of bidirectional pins attached to the signal
} hal_query_sig_t;

//
// HAL query structure used both for input and output
//
typedef struct {
    const char *name;              // Request: name to search for;
                                   // Return: name found (live pointer)
    hal_qtype_t qtype;             // Request: limit search;
                                   // Return: Connection type found
    union {
        void *vpval;         // Generic pointer
        const void *cpval;   // const pointer (for easier cast'ability)
        rtapi_intptr_t ipval;
        rtapi_uintptr_t upval;
        rtapi_sint sival;
        rtapi_uint uival;
    } callerdata;                  // Caller private data additional to 'arg'
    union {
        // Query data specific according to 'qtype'
        // See details above
        hal_query_pp_t     pp;     // Pins, params
        hal_query_sig_t    sig;    // Signals
        hal_query_comp_t   comp;   // Components
        hal_query_funct_t  funct;  // Functions
        hal_query_thread_t thread; // Threads
    };
} hal_query_t;

// Callback prototype
// A callback is invoked while holding the HAL mutex. You are allowed to call
// back into the HAL library from within the callback (the mutex is recursive).
// A fair warning:
//    You should not take too long in callbacks and _MUST_NOT_ terminate the
//    program inside a callback. Doing so will keep the mutex locked and other
//    processes will hang indefinitely when they call into the HAL library.
// Returning non-zero will automatically break any iteration loop. Use negative
// return values to signal error conditions and positive return values to
// simply terminate any iteration loop. The callback's return value is used as
// the iteration function's return value.
typedef int (*hal_query_cb)(hal_query_t *query, void *arg);

// Pin/Param/Signal setters based on name
// get_p/set_p - get or set pin or param
// get_s/set_s - get or set signal
// getref_p    - return only the pin/param reference
// getref_s    - return only the signal reference
//
// set_s(): If the type is HAL_PORT, then the action sets the port's
//          queue size according to the `query->value.u` setting.
//
// set_p/set_s: The callback function is called after it is determined that the
// name exists and optionally if the type matches. If the setter 'cb' callback
// is NULL, then it is required that you set both the query->{pp,sig}.type to
// the proper type and the matching query->{pp,sig}.value field to its
// associated value.
//
// get_p/get_s: The callback function is called after it is determined that
// the name exists and optionally if the type matches. The getter calls the
// callback with the appropriate query->{pp,sig}.value field set to the value
// read, according to the type. If the getter 'cb' callback is NULL, then it is
// simply skipped and the value is still available in the query structure.
//
// For all query functions: The callback should return zero when it succeeds
// and a negative errno value on error. If the callback returns with a non-zero
// value, then that value is returned.
// Returning a positive value from a callback function in iterations terminates
// the iteration loop. The caller can determine from the return value's sign
// whether it was an error or an intentional iteration loop termination.
//
int hal_getref_p(hal_query_t *query);
int hal_get_p(hal_query_t *query, hal_query_cb cb, void *arg);
int hal_set_p(hal_query_t *query, hal_query_cb cb, void *arg);

int hal_getref_s(hal_query_t *query);
int hal_get_s(hal_query_t *query, hal_query_cb cb, void *arg);
int hal_set_s(hal_query_t *query, hal_query_cb cb, void *arg);

//
// *** HAL structure iteration functions ***
//
// Each function will invoke the callback on each of the HAL structures
// of interest:
//   hal_list_p      - callback on pins and/or params
//   hal_list_p_s    - callback on pins connected to named signal
//   hal_list_s      - callback on signals
//   hal_list_comp   - callback on components
//   hal_list_funct  - callback on registered functions
//   hal_list_thread - callback on registered threads (and its functions)
//
int hal_list_p(hal_query_t *q, hal_query_cb cb, void *arg);
int hal_list_p_s(hal_query_t *q, hal_query_cb cb, void *arg);
int hal_list_s(hal_query_t *q, hal_query_cb cb, void *arg);
int hal_list_comp(hal_query_t *q, hal_query_cb cb, void *arg);
int hal_list_funct(hal_query_t *q, hal_query_cb cb, void *arg);
int hal_list_thread(hal_query_t *q, hal_query_cb cb, void *arg);

//
// *** Component queries ***
//
// Query components by name or ID
int hal_comp_by_name(const char *name, hal_query_t *q);
int hal_comp_by_id(int comp_id, hal_query_t *q);

//
// *** General HAL statistics ***
//
typedef struct {
    long mem_total;
    long mem_free;
    int ncomps;
    int ncomps_free;
    int npins;
    int npins_free;
    int nparams;
    int nparams_free;
    int naliases;
    int naliases_free;
    int nsignals;
    int nsignals_free;
    int nthreads;
    int nthreads_free;
    int nfuncts;
    int nfuncts_free;
} hal_statistics_t;

int hal_statistics(hal_statistics_t *sts);

//
// *** Special functions for rtapi_app and halcmd ***
//
// Invoke the constructor for a new instance
// Uspace/rtapi_app only. Not implemented in halcmd/halrmt.
int hal_comp_invoke_make(const char *compname, const char *newname, const char *arg);

// Add the insmod arguments to a named (and just loaded) RT component
// The 'args' argument must be in HAL memory.
int hal_comp_insmod_args(const char *compname, const char *args);

// -----------------------------------------------------
// Release the HAL mutex with brute force
// WARNING:
// *   Do not use this function in normal code. You will
// *   probably kill your running instance when you do.
// *   It is only to recover from an error and you need
// *   to be able to shut down your instance.
int hal_mutex_force_release(void);
// -----------------------------------------------------

// HAL will pretend that the exact base period requested is possible.
// This mode is not suitable for running real hardware.
// Returns zero (0) on success or a negative -EACCES error if already set.
int hal_enforce_exact_base_period(void);

#endif // ULAPI

RTAPI_END_DECLS

#endif /* HAL_H */
