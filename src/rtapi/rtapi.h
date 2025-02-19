#ifndef RTAPI_H
#define RTAPI_H

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

/**
 * @file
 * @brief Defines the RTAPI for both realtime and non-realtime code.
 *
 * RTAPI is a library providing a uniform API for several real time operating
 * systems. As of ver 2.0, RTLinux and RTAI are supported.
 *
 * Defines the RTAPI for both realtime and non-realtime code. This is a change
 * from Rev 2, where the non-realtime (user space) API was defined in ulapi.h
 * and used different function names. The symbols RTAPI and ULAPI are used to
 * determine which mode is being compiled, RTAPI for realtime and ULAPI for
 * non-realtime. The API is implemented in files named @c xxx_rtapi.c, where
 * xxx is the RTOS.
*/

/*  Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
    Copyright (C) 2003 Paul Corner
                       <paul_c AT users DOT sourceforge DOT net>
    This library is based on version 1.0, which was released into
    the public domain by its author, Fred Proctor.  Thanks Fred!
*/

/*  This library is free software; you can redistribute it and/or
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

#include <stdarg.h>
#include <stddef.h> // provides NULL


#define RTAPI_NAME_LEN   31    /*!< Length for module, etc, names */

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

/**
 * @brief Sets up the RTAPI.
 *
 * It must be called by any module that intends to use the API, before any other
 * RTAPI calls.
 * @param modname Can optionally point to a string that identifies the module.
 *                The string will be truncated at ::RTAPI_NAME_LEN characters.
 *                If @c modname is @c NULL, the system will assign a name.
 * @return On success, returns a positive integer module ID, which is used for
 *         subsequent calls to rtapi_xxx_new(), rtapi_xxx_delete(), and
 *         rtapi_exit(). Negative value on failure.
 * @note Call only from within user or init/cleanup code, not from realtime
 *       tasks.
 */
    extern int rtapi_init(const char *modname);

/**
 * @brief Shuts down and cleans up the RTAPI.
 *
 * It must be called prior to exit by any module that called rtapi_init().
 * rtapi_exit() may attempt to clean up any tasks, shared memory, and other
 * resources allocated by the module, but should not be relied on to replace
 * proper cleanup code within the module.
 * @param module_id ID returned when that module called rtapi_init().
 * @return 0 on success, negative value on failure.
 * @note Call only from user or init/cleanup code, not from realtime tasks.
 */
    extern int rtapi_exit(int module_id);

/**
 * @brief Format and store a string in a buffer with a specified maximum length.
 *
 * Works like snprintf() from the normal C library, except that it may not
 * handle long longs. It is provided here because some RTOS kernels don't
 * provide a realtime safe version of the function, and those that do don't
 * provide support for printing doubles. On systems with a good kernel
 * snprintf(), or in user space, this function simply calls the normal
 * snprintf().
 * @note May be called from user, init/cleanup, and realtime code.
 */
    extern int rtapi_snprintf(char *buf, unsigned long int size,
	const char *fmt, ...)
	    __attribute__((format(printf,3,4)));

/**
 * @brief Works like vsnprintf() from the normal C library, except that it
 * doesn't handle long longs.
 *
 * It is provided here because some RTOS kernels don't provide a realtime safe
 * version of the function, and those that do don't provide support for printing
 * doubles. On systems with a good kernel vsnprintf(), or in user space, this
 * function simply calls the normal vsnrintf().
 * @note May be called from user, init/cleanup, and realtime code.
 */
    extern int rtapi_vsnprintf(char *buf, unsigned long size,
	const char *fmt, va_list ap);

/**
 * @brief Prints a printf() style message.
 *
 * Depending on the RTOS and whether the program is being compiled for user
 * space or realtime, the message may be printed to @c stdout, @c stderr, or to
 * a kernel message log, etc. The calling syntax and format string is similar
 * to printf() except that floating point and long longs are NOT supported in
 * realtime and may not be supported in user space. For some RTOS's, a 80 byte
 * buffer is used, so the format line and arguments should not produce a line
 * more than 80 bytes long. (The buffer is protected against overflow.) Does not
 * block, but can take a fairly long time, depending on the format string and
 * OS.
 * @note May be called from user, init/cleanup, and realtime code.
*/
    extern void rtapi_print(const char *fmt, ...)
	    __attribute__((format(printf,1,2)));


/** Message levels */
    typedef enum {
	RTAPI_MSG_NONE = 0,
	RTAPI_MSG_ERR,
	RTAPI_MSG_WARN,
	RTAPI_MSG_INFO,
	RTAPI_MSG_DBG,
	RTAPI_MSG_ALL
    } msg_level_t;

/**
 * @brief Prints a printf-style message when the level is less than or equal to
 * the current message level set by rtapi_set_msg_level().
 * @note May be called from user, init/cleanup, and realtime code.
 */
    extern void rtapi_print_msg(msg_level_t level, const char *fmt, ...)
	    __attribute__((format(printf,2,3)));


/**
 * @brief Set the maximum level of message to print.
 *
 * In userspace code, each component has its own independent message level.
 * In realtime code, all components share a single message level.
 * @param level Maximum level of message to print, should be one of
*              ::msg_level_t.
 * @return 0 for success or @c -EINVAL if the level is out of range.
 */
    extern int rtapi_set_msg_level(int level);

/** Retrieve the message level set by the last call to rtapi_set_msg_level() */
    extern int rtapi_get_msg_level(void);

/**
 * @brief Set the message handler.
 *
 * rtapi_get_msg_handler() and rtapi_set_msg_handler() access the function
 * pointer used by rtapi_print() and rtapi_print_msg(). By default, messages
 * appear in the kernel log, but by replacing the handler a user of the rtapi
 * library can send the messages to another destination.
 * @note Call from real-time init/cleanup code only.
 * @note When called from rtapi_print(), @c level is ::RTAPI_MSG_ALL,
 *       a level which should not normally be used with rtapi_print_msg().
 */
    typedef void(*rtapi_msg_handler_t)(msg_level_t level, const char *fmt, va_list ap);
#ifdef RTAPI
    extern void rtapi_set_msg_handler(rtapi_msg_handler_t handler);
    extern rtapi_msg_handler_t rtapi_get_msg_handler(void);
#endif

/***********************************************************************
*                      TIME RELATED FUNCTIONS                          *
************************************************************************/

/*  NOTE: These timing related functions are only available in
    realtime modules.  User processes may not call them!
*/
#ifdef RTAPI

/**
 * @brief Sets the basic time interval for realtime tasks.
 * All periodic tasks will run at an integer multiple of this period. The first
 * call to rtapi_clock_set_period() with @c nsecs greater than zero will start
 * the clock, using @c nsecs as the clock period in nano-seconds.  Due to
 * hardware and RTOS limitations, the actual period may not be exactly what was
 * requested.
 * @return On success, the actual clock period if it is available, otherwise it
 *         returns the requested period. If the requested period is outside the
 *         limits imposed by the hardware or RTOS, it returns @c -EINVAL and
 *         does not start the clock. Once the clock is started, subsequent calls
 *         with non-zero @c nsecs return @c -EINVAL and have no effect. Calling
 *         rtapi_clock_set_period() with @c nsecs set to zero queries the clock,
 *         returning the current clock period, or zero if the clock has not yet
 *         been started.
 * @note Call only from within init/cleanup code, not from realtime tasks.
*/
    extern long int rtapi_clock_set_period(long int nsecs);
#endif /* RTAPI */

/**
 * @brief Simple delay, intended only for short delays, since it simply loops,
 *        wasting CPU cycles.
 *
 * Any call to rtapi_delay() requesting a delay longer than the max will delay
 * for the max time only. rtapi_delay_max() should be called before using
 * rtapi_delay() to make sure the required delays can be achieved. The actual
 * resolution of the delay may be as good as one nano-second, or as bad as a
 * several microseconds.
 * @param nsec Desired delay in nano-seconds.
 * @note This timing function is only available in realtime modules. User
 *       processes may not call them!
 */
    extern void rtapi_delay(long int nsec);


/**
 * @brief Returns the max delay permitted for rtapi_delay().
 * @return Max delay permitted in nano-seconds.
 * @note This timing function is only available in realtime modules. User
 *       processes may not call them!
 */
    extern long int rtapi_delay_max(void);


/**
 * @brief Returns the current time in nanoseconds.
 *
 * Depending on the RTOS, this may be time since boot, or time since the clock
 * period was set, or some other time. Its absolute value means nothing, but it
 * is monotonically increasing and can be used to schedule future events, or to
 * time the duration of some activity. Returns a 64 bit value. The resolution of
 * the returned value may be as good as one nano-second, or as poor as several
 * microseconds.
 *
 * Experience has shown that the implementation of this function in some
 * RTOS/Kernel combinations is horrible. It can take up to  several
 * microseconds, which is at least 100 times longer than it should, and perhaps
 * a thousand times longer. Use it only if you MUST have results in seconds
 * instead of clocks, and use it sparingly. See rtapi_get_clocks() instead.
 *
 * Most time measurements are relative, and should be done like this:
 * @code
 * deltat = (long int)(end_time - start_time);
 * @endcode
 * where @c end_time and @c start_time are longlong values returned from
 * rtapi_get_time(), and @c deltat is an ordinary long int (32 bits). This will
 * work for times up to about 2 seconds.
 *
 * @note May be called from init/cleanup code, and from within realtime tasks.
 * @note The longlong may be poorly supported on some platforms, especially
 *       within kernel space.
 * @note rtapi_print() will NOT print longlong values.
 * @return Current time in nanoseconds
 */
    extern long long int rtapi_get_time(void);

/**
 * @brief Returns the current time in CPU clocks.
 *
 * It is  fast, since it just reads the TSC in the CPU instead of calling a
 * kernel or RTOS function. Of course, times measured in CPU clocks are not as
 * convenient, but for relative measurements this works fine. Its absolute value
 * means nothing, but it is monotonically increasing* and can be used to
 * schedule future events, or to time the duration of some activity. (* on SMP
 * machines, the two TSC's may get out of sync, so if a task reads the TSC, gets
 * swapped to the other CPU, and reads again, the value may decrease. RTAPI
 * tries to force all RT tasks to run on one CPU.)
 *
 * Most time measurements are relative, and should be done like this:
 * @code
 * deltat = (long int)(end_time - start_time);
 * @endcode
 * where @c end_time and @c start_time are longlong values returned from
 * rtapi_get_time(), and deltat is an ordinary long int (32 bits). This will
 * work for times up to a second or so, depending on the CPU clock frequency.
 * It is best used for millisecond and microsecond scale measurements though.
 * @return A 64 bit value.  The resolution of the returned value is one CPU
 *         clock, which is usually a few nanoseconds to a fraction of a
 *         nanosecond.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 * @note longlong math may be poorly supported on some platforms, especially in
 *       kernel space. Also note that rtapi_print() will NOT print longlong.

 */
    extern long long int rtapi_get_clocks(void);


/***********************************************************************
*                     TASK RELATED FUNCTIONS                           *
************************************************************************/

/*  NOTE: These realtime task related functions are only available in
    realtime modules.  User processes may not call them!
*/
#ifdef RTAPI

/*  NOTE: The RTAPI is designed to be a _simple_ API.  As such, it uses
    a very simple strategy to deal with SMP systems.  It ignores them!
    All tasks are scheduled on the first CPU.  That doesn't mean that
    additional CPUs are wasted, they will be used for non-realtime code.
*/

/*  The 'rtapi_prio_xxxx()' functions provide a portable way to set
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

/**
 * @brief Sets task priority to the highest possible value.
 * @return Value for the highest priority, which is 0.
 * @note Call these functions only from within init/cleanup code, not from
 *       realtime tasks.
 */
    extern int rtapi_prio_highest(void);

/**
 * @brief Sets task priority to the lowest possible value.
 * @return Value for the lowest priority, which is 0xFFF.
 * @note Call these functions only from within init/cleanup code, not from
 *       realtime tasks.
 */
    extern int rtapi_prio_lowest(void);

/**
 * @brief Returns the next higher priority
 * @param prio Previous priority.
 * @note Call these functions only from within init/cleanup code, not from
 *       realtime tasks.
 */
    extern int rtapi_prio_next_higher(int prio);

/**
 * @brief Returns the next lower priority
 * @param prio Previous priority.
 * @note Call these functions only from within init/cleanup code, not from
 *       realtime tasks.
 */
    extern int rtapi_prio_next_lower(int prio);


#define RTAPI_NO_FP   0 /*!< No floating point. */
#define RTAPI_USES_FP 1 /*!< Uses floating point. */
 /**
  * @brief Creates but does not start a realtime task.
  *
  * The task is created in the "paused" state. To start it, call either
  * rtapi_task_start() for periodic tasks, or rtapi_task_resume() for
  * free-running tasks.
  *
  * @c taskcode is the name of a function taking one int and returning void,
  * which contains the task code. @c arg will be passed to @c taskcode as an
  * arbitrary void pointer when the task is started, and can be used to pass any
  * amount of data to the task (by pointing to a struct, or other such tricks).
  *
  * @c uses_fp is a flag that tells the OS whether the task uses floating point
  * so it can save the FPU registers on a task switch. Failing to save registers
  * when needed causes the dreaded "NAN bug", so most tasks should set @c
  * uses_fp to ::RTAPI_USES_FP. If a task definitely does not use floating
  * point, setting @c uses_fp to ::RTAPI_NO_FP saves a few microseconds per task
  * switch.
  * @param taskcode Pointer to the function to be called when the task is
  *                 started.
  * @param arg Argument to be passed to the taskcode function.
  * @param prio Priority of the task, determined by one of rtapi_prio_xxxx().
  * @param owner ID of the module that is making the call see rtapi_init().
  * @param stacksize The amount of stack to be used for the task, be generous,
  *                  hardware interrupts may use the same stack.
  * @param uses_fp Whether the task uses floating point set with ::RTAPI_NO_FP
  *                or ::RTAPI_USES_FP.
  * @return On success, returns a positive integer task ID, @c task_id. This ID
  *         is used for all subsequent calls that need to act on the task. On
  *         failure, returns a negative error code as listed above.
 * @note Call only from within init/cleanup code, not from realtime tasks.
 */
    extern int rtapi_task_new(void (*taskcode) (void *), void *arg,
	int prio, int owner, unsigned long int stacksize, int uses_fp);

/**
 * @brief Deletes a task.
 *
 * It frees memory associated with @c task, and does any other cleanup needed.
 * If the task has been started, you should pause it before deleting it.
 * @param task_id ID from a previous call to rtapi_task_new().
 * @return 0 on success, negative value on failure.
 * @note Call only from within init/cleanup code, not from realtime tasks.
 */
    extern int rtapi_task_delete(int task_id);

/**
 * @brief Starts a task in periodic mode.
 *
 * @c period_nsec will be rounded to the nearest multiple of the global clock
 * period. A task period less than the clock period (including zero) will be set
 * equal to the clock period.
 * @param task_id ID from a previous call to rtapi_task_new().
 * @param period_nsec Period in nanoseconds.
 * @return 0 on success, negative value on failure. The task must be in the
 *         "paused" state, or it will return @c -EINVAL.
 * @note Call only from within init/cleanup code, not from realtime tasks.
 */
    extern int rtapi_task_start(int task_id, unsigned long int period_nsec);

/**
 * @brief Suspends execution of the current task until the next period.
 *
 * The task must be periodic, if not, the result is undefined.
 * @return The function will return at the beginning of the next period.
 * @note Call only from within a realtime task.
 */
    extern void rtapi_wait(void);

/**
 * @brief Starts a task in free-running mode.
 *
 * A free running task runs continuously until either:
 *    1) It is preempted by a higher priority task. It will resume as soon as the
 *       higher priority task releases the CPU.
 *    2) It calls a blocking function, like rtapi_sem_take(). It will resume
 *       when the function unblocks.
 *    3) it is returned to the "paused" state by rtapi_task_pause().
 * @param task_id ID from a previous call to rtapi_task_new().
 * @return 0 on success, negative value on failure. The task must be in the
 *         "paused" state, or it will return @c -EINVAL.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 */
    extern int rtapi_task_resume(int task_id);

/**
 * @brief Stop task  execution and change to the "paused" state.
 *
 * The task will resume execution when either rtapi_task_resume() or
 * rtapi_task_start() is called. The task can be free-running or periodic
 * @param task_id ID from a previous call to rtapi_task_new().
 * @return 0 on success, negative value on failure.
 * @note May called from any task, or from init or cleanup code, not just from
 *       the task that is to be paused.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 */
    extern int rtapi_task_pause(int task_id);

/**
 * Returns the @c task_id of the current task or -EINVAL.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 */
    extern int rtapi_task_self(void);

#if defined(RTAPI_USPACE) || defined(USPACE)

#define RTAPI_TASK_PLL_SUPPORT

/**
 * @brief Gets the reference timestamp for the start of the current cycle.
 * @return Reference value in nanoseconds on success, 0 on failure.
 */
    extern long long rtapi_task_pll_get_reference(void);

/**
 * @brief Sets the correction value for the next scheduling cycle of the current
 *        task.
 *
 * This could be used to synchronize the task cycle to external sources.
 * @param value New correction value.
 * @return 0 on success, negative value on failure.
 */
    extern int rtapi_task_pll_set_correction(long value);
#endif /* USPACE */

#endif /* RTAPI */

/***********************************************************************
*                  SHARED MEMORY RELATED FUNCTIONS                     *
************************************************************************/

/**
 * @brief Allocates a block of shared memory.
 *
 * All modules wishing to access the same memory must use the same @c key. The
 * block will be at least @c size bytes, and may be rounded up. Allocating many
 * small blocks may be very wasteful. When a particular block is allocated for
 * the first time, all bytes are zeroed. Subsequent allocations of the same
 * block by other modules or processes will not touch the contents of the block.
 * @param key Identifier for the memory block must be non-zero.
 * @param module_id ID of the calling module.
 * @param size Desired size of the shared memory block, in bytes.
 * @return On success, it returns a positive integer ID, which is used for
           all subsequent calls dealing with the block. On failure it returns a
           negative error code.
 * @note Call only from within user or init/cleanup code, not from realtime
 *       tasks.
 */
    extern int rtapi_shmem_new(int key, int module_id,
	unsigned long int size);

/**
 * @brief Frees the shared memory block associated with @c shmem_id.
 * @param shmem_id ID of the shared memory block created with rtapi_shmem_new().
 * @param module_id ID of the calling module.
 * @return 0 on success, negative value on failure.
 * @note Call only from within user or init/cleanup code, not from realtime
 *       tasks.
 */
    extern int rtapi_shmem_delete(int shmem_id, int module_id);

/**
 * @brief Sets @c *ptr to point to shared memory block associated with
 *         @c shmem_id.
 * @param shmem_id ID of the shared memory block created with rtapi_shmem_new().
 * @param ptr Pointer to shared memory block.
 * @return 0 on success, negative value on failure.
 * @note May be called from user code, init/cleanup code, or realtime tasks.
 */
    extern int rtapi_shmem_getptr(int shmem_id, void **ptr);

/***********************************************************************
*                    SEMAPHORE RELATED FUNCTIONS                       *
************************************************************************/

/*  NOTE: These semaphore related functions are only available in
    realtime modules.  User processes may not call them!  Consider
    the mutex functions listed in rtapi_mutex.h instead.
*/
#ifdef RTAPI

/**
 * @brief Create a new semaphore.
 *
 * All modules wishing to use the same semaphore, must specify the same @c key.
 * @param key Identifier for the semaphore, and must be non-zero.
 * @param module_id Is the ID of the module making the call, see rtapi_init().
 * @return On success, it returns a positive integer semaphore ID, sem_id, which
 *         is used for all subsequent calls dealing with the semaphore. On
 *         failure it returns a negative error code.
 * @note Call only from within init/cleanup code, not from realtime tasks.
 * @note Only available in realtime modules. User processes may not call them!
 *       Consider the mutex functions in rtapi_mutex.h instead.
 */
    extern int rtapi_sem_new(int key, int module_id);

/**
 * @brief Delete a semaphore.
 *
 * Any tasks blocked on @c sem will resume execution.
 * @param sem_id Semaphore ID returned by a previous call to rtapi_sem_new().
 * @param module_id is the ID of the calling module.
 * @return 0 on success, negative value on failure.
 * @note Call only from within init/cleanup code, not from realtime tasks.
 */
    extern int rtapi_sem_delete(int sem_id, int module_id);

/**
 * @brief Unlocks a semaphore.
 *
 * If a higher priority task is blocked on the semaphore, the calling task will
 * block and the higher priority task will begin to run.
 * @param sem_id Semaphore ID returned by a previous call to rtapi_sem_new().
 * @return 0 on success, negative value on failure.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 */
    extern int rtapi_sem_give(int sem_id);

/**
 * @brief Locks a semaphore.
 * @param sem_id Semaphore ID returned by a previous call to rtapi_sem_new().
 * @return 0 or @c  -EINVAL. If the semaphore is unlocked it returns 0
 *         immediately. If the semaphore is locked, the calling task blocks
 *         until the semaphore is unlocked, then it returns 0.
 * @note Call only from within a realtime task.
 */
    extern int rtapi_sem_take(int sem_id);

/**
 * @brief Non-blocking attempt to lock a semaphore.
 * @param sem_id Semaphore ID returned by a previous call to rtapi_sem_new().
 * @return If the semaphore is unlocked, it returns 0. On failure @c -EINVAL.
 *         If the semaphore is locked it does not block, instead it returns @c
 *         -EBUSY, and the caller can decide how to deal with the situation.
 * @note Call only from within a realtime task.
 */
    extern int rtapi_sem_try(int sem_id);

#endif /* RTAPI */

/***********************************************************************
*                        FIFO RELATED FUNCTIONS                        *
************************************************************************/

/**
 * @brief Creates a realtime fifo.
 *
 * All modules wishing to access the same fifo must use the same @c key.
 * @param key Identifies the fifo, must be non-zero.
 * @param module_id ID of the calling module. see rtapi_init().
 * @param size Size of the fifo.
 * @param mode Either @c 'R' or @c 'W', read or write access to the fifo.
 * @return On success a positive integer ID, the @c fifo_id, which is used for
 *         subsequent calls dealing with the fifo. On failure, returns a
 *         negative error code.
 * @note Call only from within user or init/cleanup code, not from realtime
 *       tasks.
 * @note RTAI fifos require \f$(stacksize >= fifosize + 256)\f$ to avoid oops
 *       messages on removal.
 */
    extern int rtapi_fifo_new(int key, int module_id,
	unsigned long int size, char mode);

/**
 * @brief Closes the fifo associated with @c fifo_id.
 * @param fifo_id ID of the fifo to close.
 * @param module_id ID of the calling module.
 * @return 0 on success, negative value on failure.
 * @note Call only from within user or init/cleanup code, not from realtime
 *       tasks.
 */
    extern int rtapi_fifo_delete(int fifo_id, int module_id);

/*  FIFO notes. These comments apply to both read and write functions.
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

/*  NOTE:  The fifo read and write functions operated differently in
    realtime and user space.  The realtime versions do not block,
    but the userspace ones do.  A future version of the RTAPI may
    define different names for the blocking and non-blocking
    functions, but for now, just read the following docs carefully!
*/

#ifdef RTAPI
/**
 * @brief Reads data from @c fifo_id.
 *
 * Does not block. If @c size bytes are not available, it will read whatever is
 * available, and return that count (which could be zero).
 * @param buf Buffer for the data.
 * @param size Maximum number of bytes to read.
 * @return Number of bytes actually read, or -EINVAL.
 * @note Call only from within a realtime task.
 */
#else /* ULAPI */
/**
 * @brief Reads data from @c fifo_id.
 * @param fifo_id ID of the fifo to read from.
 * @param buf Buffer for the data.
 * @param size Maximum number of bytes to read.
 * @return Number of bytes actually read, or -EINVAL. If there is no data in the
 *         fifo, it blocks until data appears (or a signal occurs). If @c size
 *         bytes are not available, it will read whatever is available, and
 *         return that count (will be greater than zero). If interrupted by a
 *         signal or some other error occurs, will return -EINVAL.
*/
#endif /* ULAPI */

    extern int rtapi_fifo_read(int fifo_id, char *buf,
	unsigned long int size);

#ifdef RTAPI
/**
 * @brief Writes data to @c fifo_id.
 * @param fifo_id ID of the fifo to write to.
 * @param buf Buffer for the data.
 * @param size Maximum number of bytes to write.
 * @return Number of bytes actually written, or @c -EINVAL. Does not block.  If
 *         @c size bytes of space are not available in the fifo, it will write
 *         as many bytes as it can and return that count (which may be zero).
 */
#else /* ULAPI */
/**
 * @brief Writes data to @c fifo_id.
 * @param fifo_id ID of the fifo to write to.
 * @param buf Buffer for the data.
 * @param size Maximum number of bytes to write.
 * @return Number of bytes actually written, or -EINVAL. If @c size bytes of
 *         space are not available in the fifo, rtapi_fifo_write() may block,
 *         or it may write as many bytes as it can and return that count (which
 *         may be zero).
 */
#endif /* ULAPI */

    extern int rtapi_fifo_write(int fifo_id, char *buf,
	unsigned long int size);

/***********************************************************************
*                    INTERRUPT RELATED FUNCTIONS                       *
************************************************************************/

/*  NOTE: These interrupt related functions are only available in
    realtime modules.  User processes may not call them!
*/
#ifdef RTAPI

/**
 * @brief Set up a handler for a hardware interrupt.
 * @param irq_num Interrupt number.
 * @param owner ID of the calling module, see rtapi_init().
 * @param handler Function pointer,taking no arguments and returning void.
 *                @c handler will be called when the interrupt occurs.
 * @return 0 on successfully installing the handler, negative value on failure.
 * @note The simulated RTOS does not support interrupts.
 * @note Call only from within init/cleanup code, not from realtime tasks.
 */
    extern int rtapi_irq_new(unsigned int irq_num, int owner,
	void (*handler) (void));

/**
 * @brief Removes an interrupt handler that was previously installed by
 *        rtapi_assign_interrupt_handler().
 * @param irq_num Interrupt number.
 * @return 0 on success, negative value on failure
 * @note Call only from within init/cleanup code, not from realtime tasks.
 * @warning Removing a realtime module without freeing any handlers it has
 *          installed will almost certainly crash the box.
 */
    extern int rtapi_irq_delete(unsigned int irq_num);

/**
 * @brief Enable interrupt.
 *
 * This is presumably ones that have handlers assigned to them.
 * @param irq Interrupt number.
 * @return Always returns 0.
 * @note May be called from init/cleanup code, and from within realtime tasks.

*/
    extern int rtapi_enable_interrupt(unsigned int irq);

/**
 * @brief Disable interrupt.
 *
 * This is presumably ones that have handlers assigned to them.
 * @param irq Interrupt number.
 * @return Always returns 0.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 */
    extern int rtapi_disable_interrupt(unsigned int irq);

#endif /* RTAPI */

/***********************************************************************
*                        I/O RELATED FUNCTIONS                         *
************************************************************************/

/**
 * @brief Write @c byte to a hardware I/O @c port.
 * @param byte Byte to write.
 * @param port Address of the I/O port.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 * @note This function does nothing on the simulated RTOS.
 * @note Many platforms provide an inline outb() that is faster.
 */
    extern void rtapi_outb(unsigned char byte, unsigned int port);

/**
 * @brief Read byte from @c port.
 * @param port Address of the I/O port.
 * @return Bytes read from the given I/O @c port.
 * @note May be called from init/cleanup code, and from within realtime tasks.
 * @note This function always returns zero on the simulated RTOS.
 * @note Many platforms provide an inline inb() that is faster.
 */
    extern unsigned char rtapi_inb(unsigned int port);

#if defined(__KERNEL__)
#include <linux/version.h>
#include <linux/ioport.h>

/**
 * @brief Reserve I/O memory.
 *
 * Reserve memory starting at @c base, going for @c size bytes, for component
 * @c name.
 * @param base Base address of the I/O region.
 * @param size Size of the I/O region.
 * @param name Name to be shown in @c /proc/ioports.
 * @return @c NULL on failure. Otherwise, a non-NULL value.
 * @note Note that on kernels before 2.4.0, this function always succeeds.
 */
    static __inline__ void *rtapi_request_region(unsigned long base,
            unsigned long size, const char *name) {
        return (void*)request_region(base, size, name);
    }

/**
 * @brief Release I/O memory.
 *
 * Release memory reserved by rtapi_request_region(), starting at @c base and
 * going for @c size bytes. @c base and @c size must exactly match an earlier
 * successful call to rtapi_request_region() or the result is undefined.
 * @param base Base address of the I/O region.
 * @param size Size of the I/O region.
 * @param name Name to be shown in @c /proc/ioports.
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
