#ifndef RTAPI_H
#define RTAPI_H

/** RTAPI is a library providing a uniform API for realtime operations.
    Only the POSIX/uspace implementation is supported.
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

/** 'rtapi_print_msg()' prints a printf-style message at the given level.
    The level is passed through to the log ring for filtering at the output
    stage.  May be called from user, init/cleanup, and realtime code.
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

/***********************************************************************
*                      TIME RELATED FUNCTIONS                          *
************************************************************************/

/** NOTE: These timing related functions are only available in
    realtime modules.  User processes may not call them!
*/

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

/** rtapi_delay() is a simple delay.  It is intended only for short
    delays, since it simply loops, wasting CPU cycles.  'nsec' is the
    desired delay, in nano-seconds.  'rtapi_delay_max() returns the
    max delay permitted (usually approximately 1/4 of the clock period).
    Any call to 'rtapi_delay()' requesting a delay longer than the max
    will delay for the max time only.  'rtapi_delay_max()' should be
    called before using 'rtapi_delay()' to make sure the required delays
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
    the task code.  'arg' will be passed to 'taskcode' as an arbitrary
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

/** 'rtapi_task_set_cpu()' sets the CPU affinity for a task before it
    is started.  'task_id' is a task ID from rtapi_task_new().
    'cpu_number' is the CPU core to pin the task to, or -1 for no
    affinity.  Must be called between rtapi_task_new() and
    rtapi_task_start().  Returns 0 on success, negative errno on error.
*/
    extern int rtapi_task_set_cpu(int task_id, int cpu_number);

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

/** 'rtapi_task_self_ptr()' returns a pointer to the current task's
    rtapi_task struct, or NULL if not called from a task context.
    Used by thread_task() to check the cooperative exit flag.
*/
    struct rtapi_task;
    extern struct rtapi_task *rtapi_task_self_ptr(void);

#define RTAPI_TASK_PLL_SUPPORT

/** 'rtapi_task_pll_get_reference()' gets the reference timestamp
    for the start of the current cycle.
    Returns 0 if not called from within task context or on
    platforms that do not support this.
*/
    extern long long rtapi_task_pll_get_reference(void);

/** 'rtapi_task_pll_set_correction()' sets the correction value for
    the next scheduling cycle of the current task. This could be
    used to synchronize the task cycle to external sources.
    Returns -EINVAL if not called from within task context or on
    platforms that do not support this.
*/
    extern int rtapi_task_pll_set_correction(long value);

/***********************************************************************
*                  SHARED MEMORY RELATED FUNCTIONS                     *
************************************************************************/

/** 'rtapi_shmem_new()' allocates a block of shared memory.  'key'
    identifies the memory block, and must be non-zero.  All modules
    wishing to access the same memory must use the same key.
    'module_id' is the ID of the module that is making the call (see
    rtapi_init).  The block will be at least 'size' bytes, and may
    be rounded up.  Allocating many small blocks may be very wasteful.
    When a particular block is allocated for the first time, all
    bytes are zeroed.  Subsequent allocations of the same block
    by other modules or processes will not touch the contents of the
    block.
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

#define rtapi_request_region(base, size, name) ((void*)-1)
#define rtapi_release_region(base, size) ((void)0)

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

extern int rtapi_is_kernelspace(void);
extern int rtapi_is_realtime(void);

int rtapi_open_as_root(const char *filename, int mode);


void rtapi_set_namef(const char *fmt, ...);

/** 'rtapi_malloc()' allocates 'size' bytes of memory suitable for
    realtime use: all pages are pre-faulted and locked into physical RAM
    via mlock() so they will never be swapped out.  Returns a pointer to
    the allocated memory on success, or NULL on failure.
*/
extern void *rtapi_malloc(size_t size);
extern void *rtapi_calloc(size_t size);
extern void *rtapi_realloc(void *ptr, size_t size);

/** 'rtapi_free()' releases memory previously allocated by rtapi_malloc().
    call.  The pages are unlocked (munlock()) before the memory is freed.
*/
extern void rtapi_free(void *p);

extern int rtapi_lock_mem(void *p, size_t size, int prefault_rw);
extern void rtapi_unlock_mem(void *p, size_t size);

extern void *rtapi_dlopen(const char *path, int flags);
extern int rtapi_dlclose(void *handle);

/** 'rtapi_lock_dl_handle()' locks all PT_LOAD segments of a previously
    dlopen'd shared library into physical memory (mlock).  This is used
    by the launcher to lock cmod plugin .so files that contain RT code.
    'rtapi_unlock_dl_handle()' reverses the locking.
*/
extern void rtapi_lock_dl_handle(void *handle);
extern void rtapi_unlock_dl_handle(void *handle);

extern void rtapi_initialize_app(void);

RTAPI_END_DECLS

#endif /* RTAPI_H */
