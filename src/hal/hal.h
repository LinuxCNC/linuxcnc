#ifndef HAL_H
#define HAL_H

/** HAL stands for Hardware Abstraction Layer, and is used by EMC to
    transfer realtime data to and from I/O devices and other low-level
    modules.
*/

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

#if ( !defined RTAPI ) && ( !defined ULAPI )
#error HAL needs RTAPI/ULAPI, check makefile and flags
#endif

/** These status codes are returned by many HAL functions. */

#define HAL_SUCCESS       0	/* call successfull */
#define HAL_UNSUP        -1	/* function not supported */
#define HAL_BADVAR       -2	/* duplicate or not-found variable name */
#define HAL_INVAL        -3	/* invalid argument */
#define HAL_NOMEM        -4	/* not enough memory */
#define HAL_LIMIT        -5	/* resource limit reached */
#define HAL_PERM         -6	/* permission denied */
#define HAL_BUSY         -7	/* resource is busy or locked */
#define HAL_NOTFND       -8	/* object not found */
#define HAL_FAIL         -9	/* operation failed */

#define HAL_NAME_LEN     31	/* length for pin, signal, etc, names */

/***********************************************************************
*                   GENERAL PURPOSE FUNCTIONS                          *
************************************************************************/

/*
  hal_module_info

  Info required to register a new module with the hal subsystem

  module_name - the name of the module (ie, 'motmod')
  author - the name of the person responsible (duck) for the module
  short_description - what this module does
  info_link - a URL or email address with where to get more information/help
*/

  
typedef struct hal_module_info
  {
  	char *module_name;		// Name of the module
  	char *author;			// Author who created this module
  	char *short_description;	// Description of what the module does
  	char *info_link;		// email and/or web reference info
  } hal_module_info;

/*  
  hal_register_module
  
  Registers a module with the hal subsystem.
  On successful call, returns a positive module_id for use in
  subsequent calls to the hal subsystem.  On error, returns
  a HAL_* error.
  
  The module_id, like a block_id, is a valid globally unique
  client_id which can be used to acquire memory for thie module
  from the memory routines.  This id should be used *only* for
  memory which will be used by the module generally, and *not*
  for memory which will used by a particular block instance.
  Memory for blocks should be allocated using the block_id for
  that specific block, and not the module_id.

NOTE:REFACTOR: this will replace hal_init()  
*/

extern int hal_register_module(hal_module_info *mb);



/** 'hal_init()' is called by a HAL component before any other hal
    function is called, to open the HAL shared memory block and
    do other initialization.
    'name' is the name of the component.  It must be unique in the
    system.  If it is longer than HAL_NAME_LEN it will be
    truncated.
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

extern int hal_init(char *name);

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
    On success, hal_exit() returns HAL_SUCCESS, on failure it
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

NOTE:REFACTOR hal_malloc will change to something like this:

extern void *hal_malloc(int client_id, long int size);

    client_id - the module_id or object_id provided by either
    hal_register_module or passed to the create callback when
    the block object was created.
*/

extern void *hal_malloc(long int size);

extern void *new_hal_malloc(int client_id, long int size);


/*
 * hal_nsmalloc
 *
 * Malloc routine to acquire memory for a module or block
 * which is not shared (ns).  This takes care of using
 * kmalloc for kernel modules and malloc for user space
 * modules, and also tracks block usage.
 */

extern void *hal_nsmalloc(int client_id, long int size);

/*
 * hal_nsfree
 *
 * Free's memory allocated with hal_nsmalloc
 */

extern void hal_nsfree(void *memory);

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


/*  HAL modules, when loaded, register their ability to create
    "block" objects, or "blocks".  Blocks can be thought of as
    components, or IC's on a breadboard.  Each type of block which
    a module can create (instantiate) is described to hal by
    a call to hal_register_block_type.  The call passes a block_type_id, 
    and a create function pointer.  The block_type_id must be unique 
    within the module, and tells the module what type of 
    block to create when the create function is called.  (This allows 
    one create function to be used to create multiple block_types.  
    Note that this is not required; you can pass in a different create 
    method for each different block_type that your module supports.)

    The block record also contains a type_name which is a short
    typically one word name for the type of block.

    The create function pointer points at a function that hal will
    call when it wants to create a new block.  Hal will pass in
    the comp_id of the module that registered this block, as well
    as the block_type_id found in this block.  create is expected
    to do whatever it needs to do to create a new block instance.
    If an error occurs, it should return an appropriate negative HAL_*
    error message.  Otherwise, it should return a positive (>=0) 
    block_id number which is unique within the scope of this comp_id.
*/


/*
  hal_block_type_info
  
  This structure holds the information required to register the ability
  of a module to create a new block type.
  
  block_type_id is a module specified id which must be unique to the
  type of block being registered.  It must only be unique within the
  scope of the calling module; (*Every* module is fairly likely to
  register at least 1 type of block it can create, and almost every
  module will probably register a block_type_id of 0 for the id of
  the first block type they can create, though this is not a requirement)
  
  This number will be passed back in the create callback function to let
  the module know which type of module should be created.
  
  type_name is a short, one word name for the type of block that
  can be created using this id.  Examples might be "and" or "or" or
  "8-bit-counter".
  
  short_description is a short human readable description about what
  this block type does.   An example might be "Provides a counter with
  8 bits of output and a clock input"
  
  int (*create)(int new_block_id, int block_type_id);
  
  create is a callback routine that the module must implement.  This module
  will be called by hal_lib when hal needs to create a new block object.
  The call takes a new_block_id, which is the unique block_id that
  the newly created object should use for all hal calls that require a
  client_id (like malloc) and a block_type_id, which will be the value the 
  module passed in for block_type_id in the  hal_block_type_info struct.
  The block_type_id allows the module implementor to use one "create"
  callback function to create multiple differen block types, if he
  so chooses.
  
  
  */
  
typedef struct
{
	int block_type_id;		// A unique (to this module) block id
	const char *type_name;		// Name of the block type
	const char *short_description;	// Description of what the block does
	int (*create)(int new_block_id, int block_type_id);
} hal_block_type_info;
 
/*
  hal_register_block_type
  
  Registers a block type with the hal subsystem.
  
  This call tells the HAL subsystem that the module that was assigned
  module_id can create blocks with the attributes specified in the
  hal_block_type_info block.
  
  Returns a HAL_* error or HAL_SUCCESS.
  
   
*/

extern int hal_register_block_type(int module_id, 
	hal_block_type_info *block_info);


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
    HAL_BIT = 1,
    HAL_FLOAT = 2,
    HAL_S8 = 3,
    HAL_U8 = 4,
    HAL_S16 = 5,
    HAL_U16 = 6,
    HAL_S32 = 7,
    HAL_U32 = 8
} hal_type_t;

/** HAL pins also have a direction attribute.  Any number of HAL_RD
    or HAL_RD_WR pins may be connected to the same signal, but only
    one HAL_WR pin is permitted.  This is equivalent to connecting
    two output pins together in an electronic circuit.  (HAL_RD_WR
    pins can be thought of as tri-state outputs.)
*/
typedef enum {
    HAL_RD = 16,
    HAL_WR = 32,
    HAL_RD_WR = (HAL_RD | HAL_WR),
} hal_dir_t;

#if 1
/* Use these for x86 machines, and anything else that can write to
   individual bytes in a machine word. */
typedef volatile unsigned char hal_bit_t;
typedef volatile unsigned char hal_u8_t;
typedef volatile signed char hal_s8_t;
typedef volatile unsigned short hal_u16_t;
typedef volatile signed short hal_s16_t;
typedef volatile unsigned long hal_u32_t;
typedef volatile signed long hal_s32_t;
typedef volatile float hal_float_t;
#else
/* Use these for weird machines that can't access bytes individually.
   It wastes memory and may be slower, but it can't be helped. */
typedef volatile unsigned int hal_bit_t;
typedef volatile unsigned int hal_u8_t;
typedef volatile signed int hal_s8_t;
typedef volatile unsigned int hal_u16_t;
typedef volatile signed int hal_s16_t;
typedef volatile unsigned long hal_u32_t;
typedef volatile signed long hal_s32_t;
typedef volatile float hal_float_t;
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
    'name' is the name of the new pin.  If longer than HAL_NAME_LEN it
    will be truncated.  If there is already a pin with the same name
    the call will fail.
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
    If successful, the hal_pin_xxx_new() functions return HAL_SUCCESS.
    On failure they return a negative error code.
*/
extern int hal_pin_bit_new(char *name, hal_dir_t dir,
    hal_bit_t ** data_ptr_addr, int comp_id);
extern int hal_pin_float_new(char *name, hal_dir_t dir,
    hal_float_t ** data_ptr_addr, int comp_id);
extern int hal_pin_u8_new(char *name, hal_dir_t dir,
    hal_u8_t ** data_ptr_addr, int comp_id);
extern int hal_pin_s8_new(char *name, hal_dir_t dir,
    hal_s8_t ** data_ptr_addr, int comp_id);
extern int hal_pin_u16_new(char *name, hal_dir_t dir,
    hal_u16_t ** data_ptr_addr, int comp_id);
extern int hal_pin_s16_new(char *name, hal_dir_t dir,
    hal_s16_t ** data_ptr_addr, int comp_id);
extern int hal_pin_u32_new(char *name, hal_dir_t dir,
    hal_u32_t ** data_ptr_addr, int comp_id);
extern int hal_pin_s32_new(char *name, hal_dir_t dir,
    hal_s32_t ** data_ptr_addr, int comp_id);

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
    If successful, hal_pin_new() returns HAL_SUCCESS.  On failure
    it returns a negative error code.
*/
extern int hal_pin_new(char *name, hal_type_t type, hal_dir_t dir,
    void **data_ptr_addr, int comp_id);

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
    'name' is the name of the new signal.  If longer than HAL_NAME_LEN
    it will be truncated.  If there is already a signal with the same
    name the call will fail.
    'type' is the data type handled by the signal.  Pins can only be
    linked to a signal of the same type.
    Note that the actual address of the data storage for the signal is
    not accessible.  The data can be accessed only by linking a pin to
    the signal.  Also note that signals, unlike pins, do not have
    'owners'.  Once created, a signal remains in place until either it
    is deleted, or the last HAL component exits.
    If successful, 'hal_signal_new() returns HAL_SUCCESS.  On failure
    it returns a negative error code.
*/
extern int hal_signal_new(char *name, hal_type_t type);

/** 'hal_signal_delete()' deletes a signal object.  Any pins linked to
    the object are unlinked.
    'name' is the name of the signal to be deleted.
    If successful, 'hal_signal_delete()' returns HAL_SUCCESS.  On
    failure, it returns a negative error code.
*/
extern int hal_signal_delete(char *name);

/** 'hal_link()' links a pin to a signal.  If the pin is already linked
    to a signal, the old link is broken.  If the signal already has
    other pins linked to it, they are unaffected - one signal can be
    linked to many pins, but a pin can be linked to only one signal.
    'pin_name' and 'sig_name' are strings containing the pin and signal
    names.  If 'sig_name' is NULL, then the pin will be unlinked
    (linked to a dummy variable).
    On success, hal_link() returns HAL_SUCCESS, on failure it returns a
    negative error code.
*/
extern int hal_link(char *pin_name, char *sig_name);

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
    'name' is the name of the new parameter.  If longer than HAL_NAME_LEN
    it will be truncated.  If there is already a parameter with the same
    name the call will fail.
    'dir' is the parameter direction.  HAL_WR parameters may be written
    from outside the component, and are not modified by the component
    itself.  They are typically used for tuning or configuring the
    component.  HAL_RD paramters are read only from outside, and are
    written to by the component itself, typically to provide a view
    "into" the component for testing or troubleshooting.  HAL_RD_WR
    parameters are writable from outside and also sometimes modified
    by the component itself as well.
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
    If successful, the hal_param_xxx_new() functions return HAL_SUCCESS.
    On failure they return a negative error code.
*/
extern int hal_param_bit_new(char *name, hal_dir_t dir, hal_bit_t * data_addr,
    int comp_id);
extern int hal_param_float_new(char *name, hal_dir_t dir,
    hal_float_t * data_addr, int comp_id);
extern int hal_param_u8_new(char *name, hal_dir_t dir, hal_u8_t * data_addr,
    int comp_id);
extern int hal_param_s8_new(char *name, hal_dir_t dir, hal_s8_t * data_addr,
    int comp_id);
extern int hal_param_u16_new(char *name, hal_dir_t dir, hal_u16_t * data_addr,
    int comp_id);
extern int hal_param_s16_new(char *name, hal_dir_t dir, hal_s16_t * data_addr,
    int comp_id);
extern int hal_param_u32_new(char *name, hal_dir_t dir, hal_u32_t * data_addr,
    int comp_id);
extern int hal_param_s32_new(char *name, hal_dir_t dir, hal_s32_t * data_addr,
    int comp_id);

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
    'dir' is the parameter direction.  HAL_WR parameters may be written
    from outside the component, and are not modified by the component
    itself.  They are typically used for tuning or configuring the
    component.  HAL_RD paramters are read only from outside, and are
    written to by the component itself, typically to provide a view
    "into" the component for testing or troubleshooting.  HAL_RD_WR
    parameters are writable from outside and also sometimes modified
    by the component itself as well.
    If successful, hal_param_new() returns HAL_SUCCESS.  On failure
    it returns a negative error code.
*/
extern int hal_param_new(char *name, hal_type_t type, hal_dir_t dir,
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
    On success, the hal_param_xxx_set() functions return HAL_SUCCESS,
    and on failure they return a negative error code.
*/
extern int hal_param_bit_set(char *name, int value);
extern int hal_param_float_set(char *name, float value);
extern int hal_param_u8_set(char *name, unsigned char value);
extern int hal_param_s8_set(char *name, signed char value);
extern int hal_param_u16_set(char *name, unsigned short value);
extern int hal_param_s16_set(char *name, signed short value);
extern int hal_param_u32_set(char *name, unsigned long value);
extern int hal_param_s32_set(char *name, signed long value);

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
    If successful, hal_param_set() returns HAL_SUCCESS.  On failure
    it returns a negative error code.
*/
extern int hal_param_set(char *name, hal_type_t type, void *value_addr);

/***********************************************************************
*                   EXECUTION RELATED FUNCTIONS                        *
************************************************************************/

#ifdef RTAPI

/** hal_export_funct() makes a realtime function provided by a
    component available to the system.  A subsequent call to
    hal_add_funct_to_thread() can be used to schedule the
    execution of the function as needed by the system.
    'name' is the name of the new function, if it is longer
    than HAL_VAR_NAME_LEN it will be truncated.  This is the
    name as it would appear in an ini file, which does not
    need to be the same as the C function name.
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
    On success, hal_export_funct() returns HAL_SUCCESS, on failure
    it returns a negative error code.
    Call only from realtime init code, not from user space or
    realtime code.
*/
extern int hal_export_funct(char *name, void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id);

/** hal_create_thread() establishes a realtime thread that will
    execute one or more HAL functions periodically.
    'name' is the name of the thread, which must be unique in
    the system.  If it is longer than HAL_NAME_LEN it will be
    truncated.
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
extern int hal_create_thread(char *name, unsigned long period_nsec,
    int uses_fp);

/** hal_thread_delete() deletes a realtime thread.
    'name' is the name of the thread, which must have been created
    by 'hal_thread_new()'.
    On success, hal_thread_delete() returns HAL_SUCCESS, on
    failure it returns a negative error code.
    Call only from realtime init code, not from user
    space or realtime code.
*/
extern int hal_thread_delete(char *name);

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
    Returns HAL_SUCCESS, or a negative error code.    Call
    only from within user space or init code, not from
    realtime code.
*/
extern int hal_add_funct_to_thread(char *funct_name, char *thread_name,
    int position);

/** hal_del_funct_from_thread() removes a function from a thread.
    'funct_name' is the name of the function, as specified in
    a call to hal_export_funct().
    'thread_name' is the name of a thread which currently calls
    the function.
    Returns HAL_SUCCESS, or a negative error code.    Call
    only from within user space or init code, not from
    realtime code.
*/
extern int hal_del_funct_from_thread(char *funct_name, char *thread_name);

/** hal_start_threads() starts all threads that have been created.
    This is the point at which realtime functions start being called.
    On success it returns HAL_SUCCESS, on failure a negative
    error code.
*/
extern int hal_start_threads(void);

/** hal_stop_threads() stops all threads that were previously
    started by hal_start_threads().  It should be called before
    any component that is part of a system exits.
    On success it returns HAL_SUCCESS, on failure a negative
    error code.
*/
extern int hal_stop_threads(void);

#endif /* HAL_H */
