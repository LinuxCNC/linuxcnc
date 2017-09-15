#ifndef RTAPI_H
#define RTAPI_H

/** RTAPI is a library providing a uniform API for several real time
    operating systems.  As of ver 2.0, RTLinux and RTAI are supported.
*/
/********************************************************************
* Description:  rtapi.h
*               This file, 'rtapi.h', defines the RTAPI for both 
*               realtime and non-realtime code.
*
* Author: John Kasunich, Paul Corner
* License: LGPL Version 2.1
*
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

/** This file, 'rtapi.h', defines the RTAPI for both realtime and
    non-realtime code.  This is a change from Rev 2, where the non-
    realtime (user space) API was defined in ulapi.h and used
    different function names.  The symbols RTAPI and ULAPI are used
    to determine which mode is being compiled, RTAPI for realtime
    and ULAPI for non-realtime.  The API is implemented in files
    named 'xxx_rtapi.c', where xxx is the RTOS.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
    Copyright (C) 2003 Paul Corner
                       <paul_c AT users DOT sourceforge DOT net>
    This library is based on version 1.0, which was released into
    the public domain by its author, Fred Proctor.  Thanks Fred!
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

#if ( !defined RTAPI ) && ( !defined ULAPI )
#error "Please define either RTAPI or ULAPI!"
#endif
#if ( defined RTAPI ) && ( defined ULAPI )
#error "Can't define both RTAPI and ULAPI!"
#endif

#include <stddef.h> // provides NULL


#define RTAPI_NAME_LEN   31	/* length for module, etc, names */

#ifdef __cplusplus
#define RTAPI_BEGIN_DECLS extern "C" {
#define RTAPI_END_DECLS }
#else
#define RTAPI_BEGIN_DECLS
#define RTAPI_END_DECLS
#endif

RTAPI_BEGIN_DECLS

/***********************************************************************
*                   GENERAL PURPOSE FUNCTIONS                          *
************************************************************************/

/** 'rtapi_init() sets up the RTAPI.  It must be called by any
    module that intends to use the API, before any other RTAPI
    calls.
    'modname' can optionally point to a string that identifies
    the module.  The string will be truncated at RTAPI_NAME_LEN
    characters.  If 'modname' is NULL, the system will assign a
    name.
    On success, returns a positive integer module ID, which is
    used for subsequent calls to rtapi_xxx_new, rtapi_xxx_delete,
    and rtapi_exit.  On failure, returns an error code as defined
    above.  Call only from within user or init/cleanup code, not
    from realtime tasks.
*/
    extern int rtapi_init(const char *modname);

/** 'rtapi_exit()' shuts down and cleans up the RTAPI.  It must be
    called prior to exit by any module that called rtapi_init.
    'module_id' is the ID code returned when that module called
    rtapi_init().
    Returns a status code.  rtapi_exit() may attempt to clean up
    any tasks, shared memory, and other resources allocated by the
    module, but should not be relied on to replace proper cleanup
    code within the module.  Call only from within user or
    init/cleanup code, not from realtime tasks.
*/
    extern int rtapi_exit(int module_id);

/** 'rtapi_snprintf()' works like 'snprintf()' from the normal
    C library, except that it may not handle long longs.
    It is provided here because some RTOS kernels don't provide
    a realtime safe version of the function, and those that do don't provide
    support for printing doubles.  On systems with a
    good kernel snprintf(), or in user space, this function
    simply calls the normal snprintf().  May be called from user,
    init/cleanup, and realtime code.
*/
    extern int rtapi_snprintf(char *buf, unsigned long int size,
	const char *fmt, ...)
	    __attribute__((format(printf,3,4)));

/** 'rtapi_vsnprintf()' works like 'vsnprintf()' from the normal
    C library, except that it doesn't handle long longs.
    It is provided here because some RTOS kernels don't provide
    a realtime safe version of the function, and those that do don't provide
    support for printing doubles.  On systems with a
    good kernel vsnprintf(), or in user space, this function
    simply calls the normal vsnrintf().  May be called from user,
    init/cleanup, and realtime code.
*/
#include <stdarg.h>
    extern int rtapi_vsnprintf(char *buf, unsigned long size,
	const char *fmt, va_list ap);

/** 'rtapi_print()' prints a printf style message.  Depending on the
    RTOS and whether the program is being compiled for user space
    or realtime, the message may be printed to stdout, stderr, or
    to a kernel message log, etc.  The calling syntax and format
    string is similar to printf except that floating point and
    longlongs are NOT supported in realtime and may not be supported
    in user space.  For some RTOS's, a 80 byte buffer is used, so the
    format line and arguments should not produce a line more than
    80 bytes long.  (The buffer is protected against overflow.)
    Does not block, but  can take a fairly long time, depending on
    the format string and OS.  May be called from user, init/cleanup,
    and realtime code.
*/
    extern void rtapi_print(const char *fmt, ...)
	    __attribute__((format(printf,1,2)));

/** 'rtapi_print_msg()' prints a printf-style message when the level
    is less than or equal to the current message level set by
    rtapi_set_msg_level().  May be called from user, init/cleanup,
    and realtime code.
*/
    typedef enum {
	RTAPI_MSG_NONE = 0,
	RTAPI_MSG_ERR,
	RTAPI_MSG_WARN,
	RTAPI_MSG_INFO,
	RTAPI_MSG_DBG,
	RTAPI_MSG_ALL
    } msg_level_t;

    extern void rtapi_print_msg(msg_level_t level, const char *fmt, ...)
	    __attribute__((format(printf,2,3)));


/** Set the maximum level of message to print.  In userspace code,
    each component has its own independent message level.  In realtime
    code, all components share a single message level.  Returns 0 for
    success or -EINVAL if the level is out of range. */
    extern int rtapi_set_msg_level(int level);
/** Retrieve the message level set by the last call to rtapi_set_msg_level */
    extern int rtapi_get_msg_level(void);

/** 'rtapi_get_msg_handler' and 'rtapi_set_msg_handler' access the function
    pointer used by rtapi_print and rtapi_print_msg.  By default, messages
    appear in the kernel log, but by replacing the handler a user of the rtapi
    library can send the messages to another destination.  Calling
    rtapi_set_msg_handler with NULL restores the default handler. Call from
    real-time init/cleanup code only.  When called from rtapi_print(),
    'level' is RTAPI_MSG_ALL, a level which should not normally be used
    with rtapi_print_msg().
*/
    typedef void(*rtapi_msg_handler_t)(msg_level_t level, const char *fmt, va_list ap);
#ifdef RTAPI
    extern void rtapi_set_msg_handler(rtapi_msg_handler_t handler);
    extern rtapi_msg_handler_t rtapi_get_msg_handler(void);
#endif

/***********************************************************************
*                      TIME RELATED FUNCTIONS                          *
************************************************************************/

/** NOTE: These timing related functions are only available in
    realtime modules.  User processes may not call them!
*/
#ifdef RTAPI

/** 'rtapi_clock_set_period() sets the basic time interval for realtime
    tasks.  All periodic tasks will run at an integer multiple of this
    period.  The first call to 'rtapi_clock_set_period() with 'nsecs'
    greater than zero will start the clock, using 'nsecs' as the clock
    period in nano-seconds.  Due to hardware and RTOS limitations, the
    actual period may not be exactly what was requested.  On success,
    the function will return the actual clock period if it is available,
    otherwise it returns the requested period.  If the requested period
    is outside the limits imposed by the hardware or RTOS, it returns
    -EINVAL and does not start the clock.  Once the clock is started,
    subsequent calls with non-zero 'nsecs' return -EINVAL and have
    no effect.  Calling 'rtapi_clock_set_period() with 'nsecs' set to
    zero queries the clock, returning the current clock period, or zero
    if the clock has not yet been started.  Call only from within
    init/cleanup code, not from realtime tasks.  This function is not
    available from user (non-realtime) code.
*/
    extern long int rtapi_clock_set_period(long int nsecs);
#endif /* RTAPI */

/** rtapi_delay() is a simple delay.  It is intended only for short
    delays, since it simply loops, wasting CPU cycles.  'nsec' is the
    desired delay, in nano-seconds.  'rtapi_delay_max() returns the
    max delay permitted (usually approximately 1/4 of the clock period).
    Any call to 'rtapi_delay()' requesting a delay longer than the max
    will delay for the max time only.  'rtapi_delay_max()' should be
    called befure using 'rtapi_delay()' to make sure the required delays
    can be achieved.  The actual resolution of the delay may be as good
    as one nano-second, or as bad as a several microseconds.  May be
    called from init/cleanup code, and from within realtime tasks.
*/
    extern void rtapi_delay(long int nsec);
    extern long int rtapi_delay_max(void);


/** rtapi_get_time returns the current time in nanoseconds.  Depending
    on the RTOS, this may be time since boot, or time since the clock
    period was set, or some other time.  Its absolute value means
    nothing, but it is monotonically increasing and can be used to
    schedule future events, or to time the duration of some activity.
    Returns a 64 bit value.  The resolution of the returned value may
    be as good as one nano-second, or as poor as several microseconds.
    May be called from init/cleanup code, and from within realtime tasks.
    
    Experience has shown that the implementation of this function in
    some RTOS/Kernel combinations is horrible.  It can take up to 
    several microseconds, which is at least 100 times longer than it
    should, and perhaps a thousand times longer.  Use it only if you
    MUST have results in seconds instead of clocks, and use it sparingly.
    See rtapi_get_clocks() instead.

    Note that longlong math may be poorly supported on some platforms,
    especially in kernel space. Also note that rtapi_print() will NOT
    print longlongs.  Most time measurements are relative, and should
    be done like this:  deltat = (long int)(end_time - start_time);
    where end_time and start_time are longlong values returned from
    rtapi_get_time, and deltat is an ordinary long int (32 bits).
    This will work for times up to about 2 seconds.
*/
    extern long long int rtapi_get_time(void);

/** rtapi_get_clocks returns the current time in CPU clocks.  It is 
    fast, since it just reads the TSC in the CPU instead of calling a
    kernel or RTOS function.  Of course, times measured in CPU clocks
    are not as convenient, but for relative measurements this works
    fine.  Its absolute value means nothing, but it is monotonically
    increasing* and can be used to schedule future events, or to time
    the duration of some activity.  (* on SMP machines, the two TSC's
    may get out of sync, so if a task reads the TSC, gets swapped to
    the other CPU, and reads again, the value may decrease.  RTAPI
    tries to force all RT tasks to run on one CPU.)
    Returns a 64 bit value.  The resolution of the returned value is
    one CPU clock, which is usually a few nanoseconds to a fraction of
    a nanosecond.
    May be called from init/cleanup code, and from within realtime tasks.
    
    Note that longlong math may be poorly supported on some platforms,
    especially in kernel space. Also note that rtapi_print() will NOT
    print longlongs.  Most time measurements are relative, and should
    be done like this:  deltat = (long int)(end_time - start_time);
    where end_time and start_time are longlong values returned from
    rtapi_get_time, and deltat is an ordinary long int (32 bits).
    This will work for times up to a second or so, depending on the
    CPU clock frequency.  It is best used for millisecond and 
    microsecond scale measurements though.
*/
    extern long long int rtapi_get_clocks(void);


/***********************************************************************
*                     TASK RELATED FUNCTIONS                           *
************************************************************************/

/** NOTE: These realtime task related functions are only available in
    realtime modules.  User processes may not call them!
*/
#ifdef RTAPI

/** NOTE: The RTAPI is designed to be a _simple_ API.  As such, it uses
    a very simple strategy to deal with SMP systems.  It ignores them!
    All tasks are scheduled on the first CPU.  That doesn't mean that
    additional CPUs are wasted, they will be used for non-realtime code.
*/

/** The 'rtapi_prio_xxxx()' functions provide a portable way to set
    task priority.  The mapping of actual priority to priority number
    depends on the RTOS.  Priorities range from 'rtapi_prio_lowest()'
    to 'rtapi_prio_highest()', inclusive. To use this API, use one of
    two methods:

    1) Set your lowest priority task to 'rtapi_prio_lowest()', and for
       each task of the next lowest priority, set their priorities to
       'rtapi_prio_next_higher(previous)'.

    2) Set your highest priority task to 'rtapi_prio_highest()', and
       for each task of the next highest priority, set their priorities
       to 'rtapi_prio_next_lower(previous)'.

    A high priority task will preempt a lower priority task.  The linux kernel
    and userspace are always a lower priority than all rtapi tasks.

    Call these functions only from within init/cleanup code, not from
    realtime tasks.
*/
    extern int rtapi_prio_highest(void);
    extern int rtapi_prio_lowest(void);
    extern int rtapi_prio_next_higher(int prio);
    extern int rtapi_prio_next_lower(int prio);

/** 'rtapi_task_new()' creates but does not start a realtime task.
    The task is created in the "paused" state.  To start it, call
    either rtapi_task_start() for periodic tasks, or rtapi_task_resume()
    for free-running tasks.
    On success, returns a positive integer task ID.  This ID is used
    for all subsequent calls that need to act on the task.  On failure,
    returns a negative error code as listed above.  'taskcode' is the
    name of a function taking one int and returning void, which contains
    the task code.  'arg' will be passed to 'taskcode' as an abitrary
    void pointer when the task is started, and can be used to pass
    any amount of data to the task (by pointing to a struct, or other
    such tricks).
    'prio' is the  priority, as determined by one of the priority
    functions above.  'owner' is the module ID of the module that
    is making the call (see rtapi_init).  'stacksize' is the amount
    of stack to be used for the task - be generous, hardware
    interrupts may use the same stack.  'uses_fp' is a flag that
    tells the OS whether the task uses floating point so it can
    save the FPU registers on a task switch.  Failing to save
    registers when needed causes the dreaded "NAN bug", so most
    tasks should set 'uses_fp' to RTAPI_USES_FP.  If a task
    definitely does not use floating point, setting 'uses_fp' to
    RTAPI_NO_FP saves a few microseconds per task switch.  Call
    only from within init/cleanup code, not from realtime tasks.
*/
#define RTAPI_NO_FP   0
#define RTAPI_USES_FP 1

    extern int rtapi_task_new(void (*taskcode) (void *), void *arg,
	int prio, int owner, unsigned long int stacksize, int uses_fp);

/** 'rtapi_task_delete()' deletes a task.  'task_id' is a task ID
    from a previous call to rtapi_task_new().  It frees memory
    associated with 'task', and does any other cleanup needed.  If
    the task has been started, you should pause it before deleting
    it.  Returns a status code.  Call only from within init/cleanup
    code, not from realtime tasks.
*/
    extern int rtapi_task_delete(int task_id);

/** 'rtapi_task_start()' starts a task in periodic mode.  'task_id' is
    a task ID from a call to rtapi_task_new().  The task must be in
    the "paused" state, or it will return -EINVAL.
    'period_nsec' is the task period in nanoseconds, which will be
    rounded to the nearest multiple of the global clock period.  A
    task period less than the clock period (including zero) will be
    set equal to the clock period.
    Call only from within init/cleanup code, not from realtime tasks.
*/
    extern int rtapi_task_start(int task_id, unsigned long int period_nsec);

/** 'rtapi_wait()' suspends execution of the current task until the
    next period.  The task must be periodic, if not, the result is
    undefined.  The function will return at the beginning of the
    next period.  Call only from within a realtime task.
*/
    extern void rtapi_wait(void);

/** 'rtapi_task_resume() starts a task in free-running mode. 'task_id'
    is a task ID from a call to rtapi_task_new().  The task must be in
    the "paused" state, or it will return -EINVAL.
    A free running task runs continuously until either:
    1) It is prempted by a higher priority task.  It will resume as
       soon as the higher priority task releases the CPU.
    2) It calls a blocking function, like rtapi_sem_take().  It will
       resume when the function unblocks.
    3) it is returned to the "paused" state by rtapi_task_pause().
    May be called from init/cleanup code, and from within realtime tasks.
*/
    extern int rtapi_task_resume(int task_id);

/** 'rtapi_task_pause() causes 'task_id' to stop execution and change
    to the "paused" state.  'task_id' can be free-running or periodic.
    Note that rtapi_task_pause() may called from any task, or from init
    or cleanup code, not just from the task that is to be paused.
    The task will resume execution when either rtapi_task_resume() or
    rtapi_task_start() is called.  May be called from init/cleanup code,
    and from within realtime tasks.
*/
    extern int rtapi_task_pause(int task_id);

/** 'rtapi_task_self()' returns the task ID of the current task or -EINVAL.
    May be called from init/cleanup code, and from within realtime tasks.
*/
    extern int rtapi_task_self(void);

#endif /* RTAPI */

/***********************************************************************
*                  SHARED MEMORY RELATED FUNCTIONS                     *
************************************************************************/

/** 'rtapi_shmem_new()' allocates a block of shared memory.  'key'
    identifies the memory block, and must be non-zero.  All modules
    wishing to access the same memory must use the same key.
    'module_id' is the ID of the module that is making the call (see
    rtapi_init).  The block will be at least 'size' bytes, and may
    be rounded up.  Allocating many small blocks may be very wasteful.
    When a particular block is allocated for the first time, the first
    4 bytes are zeroed.  Subsequent allocations of the same block
    by other modules or processes will not touch the contents of the
    block.  Applications can use those bytes to see if they need to
    initialize the block, or if another module already did so.
    On success, it returns a positive integer ID, which is used for
    all subsequent calls dealing with the block.  On failure it
    returns a negative error code.  Call only from within user or
    init/cleanup code, not from realtime tasks.
*/
    extern int rtapi_shmem_new(int key, int module_id,
	unsigned long int size);

/** 'rtapi_shmem_delete()' frees the shared memory block associated
    with 'shmem_id'.  'module_id' is the ID of the calling module.
    Returns a status code.  Call only from within user or init/cleanup
    code, not from realtime tasks.
*/
    extern int rtapi_shmem_delete(int shmem_id, int module_id);

/** 'rtapi_shmem_getptr()' sets '*ptr' to point to shared memory block
    associated with 'shmem_id'.  Returns a status code.  May be called
    from user code, init/cleanup code, or realtime tasks.
*/
    extern int rtapi_shmem_getptr(int shmem_id, void **ptr);

/***********************************************************************
*                    SEMAPHORE RELATED FUNCTIONS                       *
************************************************************************/

/** NOTE: These semaphore related functions are only available in
    realtime modules.  User processes may not call them!  Consider
    the mutex functions listed above instead.
*/
#ifdef RTAPI

/** 'rtapi_sem_new()' creates a realtime semaphore.  'key' identifies
    identifies the semaphore, and must be non-zero.  All modules wishing
    to use the same semaphore must specify the same key.  'module_id'
    is the ID of the module making the call (see rtapi_init).  On
    success, it returns a positive integer semaphore ID, which is used
    for all subsequent calls dealing with the semaphore.  On failure
    it returns a negative error code.  Call only from within init/cleanup
    code, not from realtime tasks.
*/
    extern int rtapi_sem_new(int key, int module_id);

/** 'rtapi_sem_delete()' is the counterpart to 'rtapi_sem_new()'.  It
    discards the semaphore associated with 'sem_id'.  Any tasks blocked
    on 'sem' will resume execution.  'module_id' is the ID of the calling
    module.  Returns a status code.  Call only from within init/cleanup
    code, not from realtime tasks.
*/
    extern int rtapi_sem_delete(int sem_id, int module_id);

/** 'rtapi_sem_give()' unlocks a semaphore.  If a higher priority task
    is blocked on the semaphore, the calling task will block and the
    higher priority task will begin to run.  Returns a status code.
    May be called from init/cleanup code, and from within realtime tasks.
*/
    extern int rtapi_sem_give(int sem_id);

/** 'rtapi_sem_take()' locks a semaphore.  Returns 0 or
    -EINVAL.  If the semaphore is unlocked it returns 0
    immediately.  If the semaphore is locked, the calling task blocks
    until the semaphore is unlocked, then it returns 0.
    Call only from within a realtime task.
*/
    extern int rtapi_sem_take(int sem_id);

/** 'rtapi_sem_try()' does a non-blocking attempt to lock a semaphore.
    Returns 0, -EINVAL, or -EBUSY.  If the semaphore
    is unlocked, it returns 0.  If the semaphore is locked
    it does not block, instead it returns -EBUSY, and the caller
    can decide how to deal with the situation.  Call only from within
    a realtime task.
*/
    extern int rtapi_sem_try(int sem_id);

#endif /* RTAPI */

/***********************************************************************
*                        FIFO RELATED FUNCTIONS                        *
************************************************************************/

/** 'rtapi_fifo_new()' creates a realtime fifo. 'key' identifies the
    fifo, all modules wishing to access the same fifo must use the same
    key.  'module_id' is the ID of the module making the call (see
    rtapi_init).  'size' is the depth of the fifo.  'mode' is either
    'R' or 'W', to request either read or write access to the fifo.
    On success, it returns a positive integer ID, which is used for
    subsequent calls dealing with the fifo.  On failure, returns a
    negative error code.  Call only from within user or init/cleanup
    code, not from realtime tasks.
*/

/* NOTE - RTAI fifos require (stacksize >= fifosze + 256) to avoid
   oops messages on removal.
*/
    extern int rtapi_fifo_new(int key, int module_id,
	unsigned long int size, char mode);

/** 'rtapi_fifo_delete()' is the counterpart to 'rtapi_fifo_new()'.
    It closes the fifo associated with 'fifo_ID'.  'module_id' is the
    ID of the calling module.  Returns status code.  Call only from
    within user or init/cleanup code, not from realtime tasks.
*/
    extern int rtapi_fifo_delete(int fifo_id, int module_id);

/** FIFO notes. These comments apply to both read and write functions.
    A fifo is a character device, an int is typically four bytes long...
    If less than four bytes are sent to the fifo, expect corrupt data
    out of the other end !
    The RTAI programming manual clearly states that the programmer is
    responsible for the data format and integrity.

    Additional NOTE:  IMHO you should be able to write any amount of
    data to a fifo, from 1 byte up to (and even beyond) the size of
    the fifo.  At a future date, the somewhat peculiar RTAI fifos
    will be replaced with something that works better.   John Kasunich
*/

/** NOTE:  The fifo read and write functions operated differently in
    realtime and user space.  The realtime versions do not block,
    but the userspace ones do.  A future version of the RTAPI may
    defined different names for the blocking and non-blocking
    functions, but for now, just read the following docs carefully!
*/

#ifdef RTAPI
/** 'rtapi_fifo_read()' reads data from 'fifo_id'.  'buf' is a buffer
    for the data, and 'size' is the maximum number of bytes to read.
    Returns the number of bytes actually read, or -EINVAL.  Does not
    block.  If 'size' bytes are not available, it will read whatever is
    available, and return that count (which could be zero).  Call only
    from within a realtime task.
*/
#else /* ULAPI */
/** 'rtapi_fifo_read()' reads data from 'fifo_id'.  'buf' is a buffer
    for the data, and 'size' is the maximum number of bytes to read.
    Returns the number of bytes actually read, or -EINVAL.  If
    there is no data in the fifo, it blocks until data appears (or
    a signal occurs).  If 'size' bytes are not available, it will
    read whatever is available, and return that count (will be
    greater than zero).  If interrupted by a signal or some other
    error occurs, will return -EINVAL.
*/
#endif /* ULAPI */

    extern int rtapi_fifo_read(int fifo_id, char *buf,
	unsigned long int size);

#ifdef RTAPI
/** 'rtapi_fifo_write()' writes data to 'fifo_id'. Up to 'size' bytes
    are taken from the buffer at 'buf'.  Returns the number of bytes
    actually written, or -EINVAL.  Does not block.  If 'size' bytes
    of space are not available in the fifo, it will write as many bytes
    as it can and return that count (which may be zero).
*/
#else /* ULAPI */
/** 'rtapi_fifo_write()' writes data to 'fifo_id'. Up to 'size' bytes
    are taken from the buffer at 'buf'.  Returns the number of bytes
    actually written, or -EINVAL.  If 'size' bytes of space are
    not available in the fifo, rtapi_fifo_write() may block, or it
    may write as many bytes as it can and return that count (which
    may be zero).
*/
#endif /* ULAPI */

    extern int rtapi_fifo_write(int fifo_id, char *buf,
	unsigned long int size);

/***********************************************************************
*                    INTERRUPT RELATED FUNCTIONS                       *
************************************************************************/

/** NOTE: These interrupt related functions are only available in
    realtime modules.  User processes may not call them!
*/
#ifdef RTAPI

/** 'rtapi_assign_interrupt_handler()' is used to set up a handler for
    a hardware interrupt.  'irq' is the interrupt number, and 'handler'
    is a pointer to a function taking no arguements and returning void.
    'handler will be called when the interrupt occurs.  'owner' is the
    ID of the calling module (see rtapi_init).  Returns a status
    code.  Note:  The simulated RTOS does not support interrupts.
    Call only from within init/cleanup code, not from realtime tasks.
*/
    extern int rtapi_irq_new(unsigned int irq_num, int owner,
	void (*handler) (void));

/** 'rtapi_free_interrupt_handler()' removes an interrupt handler that
    was previously installed by rtapi_assign_interrupt_handler(). 'irq'
    is the interrupt number.  Removing a realtime module without freeing
    any handlers it has installed will almost certainly crash the box.
    Returns 0 or -EINVAL.  Call only from within
    init/cleanup code, not from realtime tasks.
*/
    extern int rtapi_irq_delete(unsigned int irq_num);

/** 'rtapi_enable_interrupt()' and 'rtapi_disable_interrupt()' are
    are used to enable and disable interrupts, presumably ones that
    have handlers assigned to them.  Returns a status code.  May be
    called from init/cleanup code, and from within realtime tasks.

*/
    extern int rtapi_enable_interrupt(unsigned int irq);
    extern int rtapi_disable_interrupt(unsigned int irq);

#endif /* RTAPI */

/***********************************************************************
*                        I/O RELATED FUNCTIONS                         *
************************************************************************/

/** 'rtapi_outb() writes 'byte' to 'port'.  May be called from
    init/cleanup code, and from within realtime tasks.
    Note: This function does nothing on the simulated RTOS.
    Note: Many platforms provide an inline outb() that is faster.
*/
    extern void rtapi_outb(unsigned char byte, unsigned int port);

/** 'rtapi_inb() gets a byte from 'port'.  Returns the byte.  May
    be called from init/cleanup code, and from within realtime tasks.
    Note: This function always returns zero on the simulated RTOS.
    Note: Many platforms provide an inline inb() that is faster.
*/
    extern unsigned char rtapi_inb(unsigned int port);

#if defined(__KERNEL__)
/** 'rtapi_request_region() reserves I/O memory starting at 'base',
    going for 'size' bytes, for component 'name'.

    Note that on kernels before 2.4.0, this function always succeeds.

    If the allocation fails, this function returns NULL.  Otherwise, it returns
    a non-NULL value.
*/
#include <linux/version.h>
#include <linux/ioport.h>

    static __inline__ void *rtapi_request_region(unsigned long base,
            unsigned long size, const char *name) {
        return (void*)request_region(base, size, name);
    }

/** 'rtapi_release_region() releases I/O memory reserved by 
    'rtapi_request_region', starting at 'base' and going for 'size' bytes.
    'base' and 'size' must exactly match an earlier successful call to
    rtapi_request_region or the result is undefined.
*/
    static __inline__ void rtapi_release_region(unsigned long base,
            unsigned long int size) {
        release_region(base, size);
    }
#else
    #define rtapi_request_region(base, size, name) ((void*)-1)
    #define rtapi_release_region(base, size) ((void)0)
#endif

/***********************************************************************
*                      MODULE PARAMETER MACROS                         *
************************************************************************/

#ifdef RTAPI

/* The API for module parameters has changed as the kernel evolved,
   and will probably change again.  We define our own macro for
   declaring parameters, so the code that uses RTAPI can ignore
   the issue.
*/

/** RTAPI_MP_INT() declares a single integer module parameter.
    RTAPI_MP_LONG() declares a single long module parameter.
    RTAPI_MP_STRING() declares a single string module parameter.
    RTAPI_MP_ARRAY_INT() declares an array of integer module parameters.
    RTAPI_MP_ARRAY_LONG() declares an array of long module parameters.
    RTAPI_MP_ARRAY_STRING() declares a single string module parameters.
    'var' is the name of the variable used for the parameter, which
    should be initialized with the default value(s) when it is declared.
    'descr' is a short description of the parameter.
    'num' is the number of elements in an array.
*/

#if !defined(__KERNEL__)
#define MODULE_INFO1(t, a, c) __attribute__((section(".modinfo"))) \
    t rtapi_info_##a = c; EXPORT_SYMBOL(rtapi_info_##a);
#define MODULE_INFO2x(t, a, b, c) MODULE_INFO2(t,a,b,c)
#define MODULE_INFO2(t, a, b, c) __attribute__((section(".modinfo"))) \
    t rtapi_info_##a##_##b = c; EXPORT_SYMBOL(rtapi_info_##a##_##b);
#define MODULE_PARM(v,t) MODULE_INFO2(const char*, type, v, t) MODULE_INFO2(void*, address, v, &v)
#define MODULE_PARM_DESC(v,t) MODULE_INFO2(const char*, description, v, t)
#define MODULE_LICENSE(s) MODULE_INFO1(const char*, license, s)
#define MODULE_AUTHOR(s) MODULE_INFO1(const char*, author, s)
#define MODULE_DESCRIPTION(s) MODULE_INFO1(const char*, description, s)
#define MODULE_SUPPORTED_DEVICE(s) MODULE_INFO1(const char*, supported_device, s)
#define MODULE_DEVICE_TABLE(x,y) MODULE_INFO2(struct rtapi_pci_device_id*, device_table, x, y)
#define MODULE_INFO(x,y) MODULE_INFO2x(char*, x, __LINE__, y)
#define EXPORT_SYMBOL(x) __attribute__((section(".rtapi_export"))) \
    char rtapi_exported_##x[] = #x;
#define EXPORT_SYMBOL_GPL(x) __attribute__((section(".rtapi_export"))) \
    char rtapi_exported_##x[] = #x;
#else
#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#endif
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE 0
#endif

#if !defined(__KERNEL__)
#define RTAPI_STRINGIFY(x)    #x

   
#define RTAPI_MP_INT(var,descr)    \
  MODULE_PARM(var,"i");            \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_LONG(var,descr)   \
  MODULE_PARM(var,"l");            \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_STRING(var,descr) \
  MODULE_PARM(var,"s");            \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_ARRAY(type, var, num, descr)      \
  MODULE_PARM(var,type);                           \
  MODULE_INFO2(int, size, var, num);               \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_ARRAY_INT(var,num,descr)          \
  RTAPI_MP_ARRAY("i", var, num, descr);

#define RTAPI_MP_ARRAY_LONG(var,num,descr)         \
  RTAPI_MP_ARRAY("l", var, num, descr);

#define RTAPI_MP_ARRAY_STRING(var,num,descr)       \
  RTAPI_MP_ARRAY("s", var, num, descr);

#else /* kernel */

#include <linux/module.h>

#define RTAPI_MP_INT(var,descr)    \
  module_param(var, int, 0);       \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_LONG(var,descr)   \
  module_param(var, long, 0);      \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_STRING(var,descr) \
  module_param(var, charp, 0);     \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_ARRAY_INT(var,num,descr)                \
  int __dummy_##var;                                     \
  module_param_array(var, int, &(__dummy_##var), 0);     \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_ARRAY_LONG(var,num,descr)               \
  int __dummy_##var;                                     \
  module_param_array(var, long, &(__dummy_##var), 0);    \
  MODULE_PARM_DESC(var,descr);

#define RTAPI_MP_ARRAY_STRING(var,num,descr)             \
  int __dummy_##var;                                     \
  module_param_array(var, charp, &(__dummy_##var), 0);  \
  MODULE_PARM_DESC(var,descr);

#endif

#endif /* RTAPI */

#if !defined(__KERNEL__)
extern long int simple_strtol(const char *nptr, char **endptr, int base);

#include <spawn.h>

int rtapi_spawn_as_root(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[], char *const envp[]);

int rtapi_spawnp_as_root(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[], char *const envp[]);
#endif

extern int rtapi_is_kernelspace(void);
extern int rtapi_is_realtime(void);

int rtapi_open_as_root(const char *filename, int mode);

RTAPI_END_DECLS

#endif /* RTAPI_H */
