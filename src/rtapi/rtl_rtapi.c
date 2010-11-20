/** RTAPI is a library providing a uniform API for several real time
    operating systems.  As of ver 2.0, RTLinux and RTAI are supported.
*/
/********************************************************************
* Description:  rtl_rtapi.c
*               This file, 'rtl_rtapi.c', implements the realtime 
*               portion of the API for the RTlinux platform.
*
* Author: John Kasunich, Paul Corner
* License: GPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

/** This file, 'rtl_rtapi.c', implements the realtime portion of the
    API for the RTLinux platform.  The API is defined in rtapi.h, which
    includes documentation for all of the API functions.  The non-real-
    time portion of the API is implemented in rtl_ulapi.c (for the
    RTLinux platform).  This implementation attempts to prevent kernel
    panics, 'oops'es, and other nasty stuff that can happen when writing
    and testing realtime code.  Wherever possible, common errors are
    detected and corrected before they can cause a crash.  This
    implementation also includes several /proc filesystem entries and
    numerous debugging print statements.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
    Copyright (C) 2003 Paul Corner
                       <paul_c AT users DOT sourceforge DOT net>
    This library is based on version 1.0, which was released into
    the public domain by its author, Fred Proctor.  Thanks Fred!
*/

/* This library is free software; you can redistribute it and/or
   modify it under the terms of version 2 of the GNU General Public
   License as published by the Free Software Foundation.
   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
*/

/** THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
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

#include <stdarg.h>		/* va_* */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>		/* replaces malloc.h in recent kernels */
#include <linux/ctype.h>	/* isdigit */
#include <linux/delay.h>	/* udelay */
#include <asm/uaccess.h>	/* copy_from_user() */
#include <asm/msr.h>		/* rdtscll() */

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))*/
#endif

/* get inb(), outb(), ioperm() */
#include <asm/io.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
/* Kernel is 2.4 or higher, use it's vsnprintf() implementation */
#define vsn_printf vsnprintf
#else
/* 2.2 and older kernels don't have vsnprintf() so we bring in
   our own implementation (vsn_printf) of it here.*/
#include "vsnprintf.h"
#endif

#include <rtl.h>		/* top level config */
#include <rtl_sched.h>		/* rtl_getschedclock(),
				   RTL_CLOCK_MODE_PERIODIC, struct
				   sched_param, pthread_*_np() */
#include <rtl_core.h>
#include <rtl_time.h>		/* clock_gethrtime() */
#include <rtl_sync.h>
#include <rtl_fifo.h>
#include <rtl_sema.h>		/* sem_t, sem_init,post,wait() */
#include <posix/pthread.h>	/* pthread_* */
#include <posix/time.h>		/*! \todo FIX ME - This is needed with rtl-3.2, but
				   is clock_getres in the same place with
				   earlier versions. */
#include <mbuff.h>		/* mbuff_alloc,free() */

#include "rtapi.h"		/* public RTAPI decls */
#include "rtapi_common.h"	/* shared realtime/nonrealtime stuff */

/* resource data unique to kernel space */
static pthread_t ostask_array[RTAPI_MAX_TASKS + 1];
static void *shmem_addr_array[RTAPI_MAX_SHMEMS + 1];
static sem_t ossem_array[RTAPI_MAX_SEMS + 1];

#define DEFAULT_MAX_DELAY 10000
static long int max_delay = DEFAULT_MAX_DELAY;

/* module parameters */

static int msg_level = RTAPI_MSG_INFO;		/* message printing level */
RTAPI_MP_INT(msg_level, "debug message level (default=3)");

/* other module information */
MODULE_AUTHOR("John Kasunich, Fred Proctor, & Paul Corner");
MODULE_DESCRIPTION("Portable Real Time API for RTLinux");
MODULE_LICENSE("GPL");

#include "rtapi_proc.h"		/* proc filesystem decls & code */

/* the following are internal functions that do the real work associated
   with deleting tasks, etc.  They do not check the mutex that protects
   the internal data structures.  When someone calls an rtapi_xxx_delete()
   function, the rtapi funct gets the mutex before calling one of these
   internal functions.  When internal code that already has the mutex
   needs to delete something, it calls these functions directly.
*/
static int module_delete(int module_id);
static int task_delete(int task_id);
static int shmem_delete(int shmem_id, int module_id);
static int sem_delete(int sem_id, int module_id);
static int fifo_delete(int fifo_id, int module_id);
static int irq_delete(unsigned int irq_num);

/***********************************************************************
*                   Internal routines                                  *
************************************************************************/

/*
  RTAPI uses integers as keys, since these can be mapped onto either
  integers or strings easily, whereas the reverse is not true: you can't
  map an arbitrary string to an integer uniquely. Since mbuff takes
  string keys, we need to convert them to some unique string using genstr().

  genstr() generates a string 'str' unique for unsigned integers 'i',
  as the reverse, e.g., 120 -> "012", -1 -> "5927694924"
*/

#define KEYSTR_LEN 16		/* larger than number of digits in MAX_INT */

static void genstr(unsigned int i, char *str)
{
    unsigned int x, d;

    if (i == 0) {
	*str++ = '0';
	*str = 0;
	return;
    }

    x = i;
    while (x > 0) {
	i = x / 10;
	d = x - (i * 10);
	*str++ = d + '0';
	x = i;
    }
    *str = 0;

    return;
}

/***********************************************************************
*                   INIT AND SHUTDOWN FUNCTIONS                        *
************************************************************************/

int init_module(void)
{
    int n;
    char keystr[KEYSTR_LEN];

    /* say hello */
    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Init\n");
    /* convert RTAPI_KEY to a string */
    genstr(RTAPI_KEY, keystr);
    /* get master shared memory block from OS and save its address */
    rtapi_data = mbuff_alloc(keystr, sizeof(rtapi_data_t));
    if (rtapi_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: Could not open shared memory area\n");
	return -ENOMEM;
    }
    /* perform a global init if needed */
    init_rtapi_data(rtapi_data);
    /* check revision code */
    if (rtapi_data->rev_code != rev_code) {
	/* mismatch - release master shared memory block */
	mbuff_free(keystr, rtapi_data);
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: Version mismatch\n");
	return -EINVAL;
    }
    /* set up local pointers to global data */
    module_array = rtapi_data->module_array;
    task_array = rtapi_data->task_array;
    shmem_array = rtapi_data->shmem_array;
    sem_array = rtapi_data->sem_array;
    fifo_array = rtapi_data->fifo_array;
    irq_array = rtapi_data->irq_array;
    /* perform local init */
    for (n = 0; n <= RTAPI_MAX_TASKS; n++) {
	ostask_array[n] = NULL;
    }
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	shmem_addr_array[n] = NULL;
    }
    rtapi_data->timer_running = 0;
    rtapi_data->timer_period = 0;
    /* all RT tasks run on CPU 0 - non-optimal, but works everywhere */
    rtapi_data->rt_cpu = 0;
    max_delay = DEFAULT_MAX_DELAY;
#ifdef CONFIG_PROC_FS
    /* set up /proc/rtapi */
    if (proc_init() != 0) {
	rtapi_print_msg(RTAPI_MSG_WARN,
	    "RTAPI: WARNING: Could not activate /proc entries\n");
	proc_clean();
    }
#endif
    /* done */
    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Init complete\n");
    return 0;
}

/* This cleanup code attempts to fix any messes left by modules
that fail to load properly, or fail to clean up after themselves */

void cleanup_module(void)
{
    int n;
    char keystr[KEYSTR_LEN];

    if (rtapi_data == NULL) {
	/* never got inited, nothing to do */
	return;
    }
    /* grab the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Exiting\n");
    /* clean up leftover modules (start at 1, we don't use ID 0 */
    for (n = 1; n <= RTAPI_MAX_MODULES; n++) {
	if (module_array[n].state == REALTIME) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: Module '%s' (ID: %02d) did not call rtapi_exit()\n",
		module_array[n].name, n);
	    module_delete(n);
	}
    }
    /* cleaning up modules should clean up everything, if not there has
       probably been an unrecoverable internal error.... */
    for (n = 1; n <= RTAPI_MAX_IRQS; n++) {
	if (irq_array[n].irq_num != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI: ERROR: Interrupt handler %02d not deleted (IRQ %d)\n",
		n, irq_array[n].irq_num);
	    /* probably un-recoverable, but try anyway */
	    irq_delete(irq_array[n].irq_num);
	}
    }
    for (n = 1; n <= RTAPI_MAX_FIFOS; n++) {
	if (fifo_array[n].state != UNUSED) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI: ERROR: FIFO %02d not deleted\n", n);
	}
    }
    for (n = 1; n <= RTAPI_MAX_SEMS; n++) {
	while (sem_array[n].users > 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI: ERROR: Semaphore %02d not deleted\n", n);
	}
    }
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmem_array[n].rtusers > 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI: ERROR: Shared memory block %02d not deleted\n", n);
	}
    }
    for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
	if (task_array[n].state != EMPTY) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI: ERROR: Task %02d not deleted\n", n);
	    /* probably un-recoverable, but try anyway */
	    rtapi_task_pause(n);
	    task_delete(n);
	}
    }
    if (rtapi_data->timer_running != 0) {
	/* in RTLinux, you can't really stop the timer.  Since no realtime
	   task needs it running, we set it to 10mS, and it serves as the
	   Linux jiffies clock.  This seems like a kluge, and probably won't
	   work on platforms where jiffies are not 100Hz, but the RTLinux
	   docs don't list a "timer stop" function */
	rtl_setclockmode(rtl_getschedclock(), RTL_CLOCK_MODE_PERIODIC,
	    10000000);
	rtapi_data->timer_period = 0;
	rtapi_data->timer_running = 0;
	max_delay = DEFAULT_MAX_DELAY;
    }
    rtapi_mutex_give(&(rtapi_data->mutex));
#ifdef CONFIG_PROC_FS
    proc_clean();
#endif
    /* convert RTAPI_KEY to a string */
    genstr(RTAPI_KEY, keystr);
    /* release master shared memory block */
    mbuff_free(keystr, rtapi_data);
    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Exit complete\n");
    return;
}

/***********************************************************************
*                   GENERAL PURPOSE FUNCTIONS                          *
************************************************************************/

/* all RTAPI init is done when the rtapi kernel module
is insmoded.  The rtapi_init() and rtapi_exit() functions
simply register that another module is using the RTAPI.
For other RTOSes, things might be different, especially
if the RTOS does not use modules. */

int rtapi_init(const char *modname)
{
    int n, module_id;
    module_data *module;

    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* find empty spot in module array */
    n = 1;
    while ((n <= RTAPI_MAX_MODULES) && (module_array[n].state != NO_MODULE)) {
	n++;
    }
    if (n > RTAPI_MAX_MODULES) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EMFILE;
    }
    /* we have space for the module */
    module_id = n;
    module = &(module_array[n]);
    /* update module data */
    module->state = REALTIME;
    if (modname != NULL) {
	/* use name supplied by caller, truncating if needed */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "%s", modname);
    } else {
	/* make up a name */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "RTMOD%03d", module_id);
    }
    rtapi_data->rt_module_count++;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module %02d loaded, name: '%s'\n",
	module_id, module->name);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return module_id;
}

int rtapi_exit(int module_id)
{
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = module_delete(module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int module_delete(int module_id)
{
    module_data *module;
    char name[RTAPI_NAME_LEN + 1];
    int n;

    /* validate module ID */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	return -EINVAL;
    }
    /* point to the module's data */
    module = &(module_array[module_id]);
    /* check module status */
    if (module->state != REALTIME) {
	/* not an active realtime module */
	return -EINVAL;
    }
    /* clean up any mess left behind by the module */
    for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
	if ((task_array[n].state != EMPTY)
	    && (task_array[n].owner == module_id)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: module '%s' failed to delete task %02d\n",
		module->name, n);
	    task_delete(n);
	}
    }
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (test_bit(module_id, shmem_array[n].bitmap)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: module '%s' failed to delete shmem %02d\n",
		module->name, n);
	    shmem_delete(n, module_id);
	}
    }
    for (n = 1; n <= RTAPI_MAX_SEMS; n++) {
	if (test_bit(module_id, sem_array[n].bitmap)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: module '%s' failed to delete sem %02d\n",
		module->name, n);
	    sem_delete(n, module_id);
	}
    }
    for (n = 1; n <= RTAPI_MAX_FIFOS; n++) {
	if ((fifo_array[n].reader == module_id) ||
	    (fifo_array[n].writer == module_id)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: module '%s' failed to delete fifo %02d\n",
		module->name, n);
	    fifo_delete(n, module_id);
	}
    }
    for (n = 1; n <= RTAPI_MAX_IRQS; n++) {
	if (irq_array[n].owner == module_id) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: module '%s' failed to delete handler for IRQ %d\n",
		module->name, irq_array[n].irq_num);
	    irq_delete(irq_array[n].irq_num);
	}
    }
    /* use snprintf() to do strncpy(), since we don't have string.h */
    rtapi_snprintf(name, RTAPI_NAME_LEN, "%s", module->name);
    /* update module data */
    module->state = NO_MODULE;
    module->name[0] = '\0';
    rtapi_data->rt_module_count--;
    if (rtapi_data->rt_module_count == 0) {
	if (rtapi_data->timer_running != 0) {
	    /* in RTLinux, you can't really stop the timer.  Since no
	       realtime task needs it running, we set it to 10mS, and it
	       serves as the Linux jiffies clock.  This seems like a kluge,
	       and probably won't work on platforms where jiffies are not
	       100Hz, but the RTLinux docs don't list a "timer stop" function
	     */
	    rtl_setclockmode(rtl_getschedclock(), RTL_CLOCK_MODE_PERIODIC,
		10000000);
	    rtapi_data->timer_period = 0;
	    max_delay = DEFAULT_MAX_DELAY;
	    rtapi_data->timer_running = 0;
	}
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module exit: ID: %d, name: '%s'\n",
	module_id, name);
    return 0;
}

int rtapi_vsnprintf(char *buf, unsigned long int size, const char *fmt, va_list ap) {
    return vsn_printf(buf, size, fmt, ap);
}

int rtapi_snprintf(char *buf, unsigned long int size, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    /* call our own vsn_printf(), which is #defined to vsnprintf() if the
       kernel supplies one. */
    i = vsn_printf(buf, size, fmt, args);
    va_end(args);
    return i;
}

#define BUFFERLEN 1024

void default_rtapi_msg_handler(msg_level_t *level, char *buffer) {
    rt_printk(buffer);
}
static rtapi_msg_handler_t rtapi_msg_handler = default_rtapi_msg_handler;

rtapi_msg_handler_t rtapi_get_msg_handler(void) {
    return rtapi_msg_handler;
}

void rtapi_set_msg_handler(rtapi_msg_handler_t handler) {
    if(handler == NULL) rtapi_msg_handler = default_rtapi_msg_handler;
    else rtapi_msg_handler = handler;
}

void rtapi_print(const char *fmt, ...)
{
    char buffer[BUFFERLEN];
    va_list args;

    va_start(args, fmt);
    /* call our own vsn_printf(), which is #defined to vsnprintf() if the
       kernel supplies one. */
    vsn_printf(buffer, BUFFERLEN, fmt, args);
    rtapi_msg_handler(RTAPI_MSG_ALL, buffer);
    va_end(args);
}

void rtapi_print_msg(int level, const char *fmt, ...)
{
    char buffer[BUFFERLEN];
    va_list args;

    if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
	va_start(args, fmt);
	/* call our own vsn_printf(), which is #defined to vsnprintf() if the 
	   kernel supplies one. */
	vsn_printf(buffer, BUFFERLEN, fmt, args);
	rtapi_msg_handler(level, buffer);
	va_end(args);
    }
}

int rtapi_set_msg_level(int level)
{
    if ((level < RTAPI_MSG_NONE) || (level > RTAPI_MSG_ALL)) {
	return -EINVAL;
    }
    msg_level = level;
    return 0;
}

int rtapi_get_msg_level(void)
{
    return msg_level;
}

/***********************************************************************
*                     CLOCK RELATED FUNCTIONS                          *
************************************************************************/

long int rtapi_clock_set_period(long int nsecs)
{
    struct timespec res;

    if (nsecs == 0) {
	/* it's a query, not a command */
	return rtapi_data->timer_period;
    }
    if (rtapi_data->timer_running) {
	/* already started, can't restart */
	return -EINVAL;
    }
    /* limit period to 2 micro-seconds min, 10 milli-second max (RTLinux
       limit) */
    if ((nsecs < 2000) || (nsecs > 10000000L)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERR: clock_set_period: %ld nsecs,  out of range\n",
	    nsecs);
	return -EINVAL;
    }

    rtl_setclockmode(rtl_getschedclock(), RTL_CLOCK_MODE_PERIODIC, nsecs);

    /* This _should_ return the actual time set, but... Does it ? */
    clock_getres(rtl_getschedclock(), &res);
    rtapi_data->timer_period = timespec_to_ns(&res);

    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: clock_set_period requested: %ld  actual: %ld\n",
	nsecs, rtapi_data->timer_period);
    rtapi_data->timer_running = 1;
    max_delay = rtapi_data->timer_period / 4;
    return rtapi_data->timer_period;
}

long long int rtapi_get_time(void)
{
    return gethrtime();
}

long long int rtapi_get_clocks(void)
{
    long long int retval;
    
    rdtscll(retval);
    return retval;
}

void rtapi_delay(long int nsec)
{
    if (nsec > max_delay) {
	nsec = max_delay;
    }
    udelay(nsec / 1000);
}

long int rtapi_delay_max(void)
{
    return max_delay;
}

/***********************************************************************
*                     TASK RELATED FUNCTIONS                           *
************************************************************************/

/* Priority functions.  RTL uses 0 as the highest priority, as the
number increases, the actual priority of the task increases. */

/* RTLinux has LOTS of different priorities - The highest seems to
   be 1000000!  I don't want such ugly numbers, and we only
   need a few levels, so we use 0xFFF (4095) instead */

int rtapi_prio_highest(void)
{
    return 0xFFF;
}

int rtapi_prio_lowest(void)
{
    return 0;
}

int rtapi_prio_next_higher(int prio)
{
    /* return a valid priority for out of range arg */
    if (prio >= rtapi_prio_highest()) {
	return rtapi_prio_highest();
    }
    if (prio < rtapi_prio_lowest()) {
	return rtapi_prio_lowest();
    }

    /* return next higher priority for in-range arg */
    return prio + 1;
}

int rtapi_prio_next_lower(int prio)
{
    /* return a valid priority for out of range arg */
    if (prio <= rtapi_prio_lowest()) {
	return rtapi_prio_lowest();
    }
    if (prio > rtapi_prio_highest()) {
	return rtapi_prio_highest();
    }
    /* return next lower priority for in-range arg */
    return prio - 1;
}

/* We define taskcode as taking a void pointer and returning void, but
   pthread wants it to take a void pointer and return a void pointer.
   We solve this with a wrapper function that meets pthread's needs.
   The wrapper functions also properly deals with tasks that return.
   (Most tasks are infinite loops, and don't return.)
*/

static void *wrapper(void *wrapper_arg)
{
    int task_id;
    task_data *task;

    /* argument is actually an int, containing the task ID */
    task_id = (int) wrapper_arg;
    /* point to the task data */
    task = &task_array[task_id];
    /* This wrapper starts executing as soon as the thread is created because 
       RTLinux does not wait for a task_start call.  So the wrapper must
       suspend itself right away.  Then task_start or task_resume can be used 
       to start the task when desired. */
    task->state = PAUSED;
    pthread_suspend_np(ostask_array[task_id]);
    /* when pthread_suspend_np() returns, it means that the task has been
       started, so call the task function with the task argument */
    (task->taskcode) (task->arg);
    /* if the task ever returns, we record that fact */
    task->state = ENDED;
    /* and return to end the thread */
    return NULL;
}

int rtapi_task_new(void (*taskcode) (void *), void *arg,
    int prio, int owner, unsigned long int stacksize, int uses_fp)
{
    int n;
    int task_id;
    int retval = 0;
    task_data *task;
    pthread_attr_t attr;
    struct sched_param sched_param;

    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate owner */
    if ((owner < 1) || (owner > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    if (module_array[owner].state != REALTIME) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* find empty spot in task array */
    n = 1;
    while ((n <= RTAPI_MAX_TASKS) && (task_array[n].state != EMPTY)) {
	n++;
    }
    if (n > RTAPI_MAX_TASKS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EMFILE;
    }
    /* we have space for the task */
    task_id = n;
    task = &(task_array[n]);
    /* check requested priority */
    if ((prio > rtapi_prio_highest()) || (prio < rtapi_prio_lowest())) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* set up task attributes */
    retval = pthread_attr_init(&attr);
    if (retval != 0) {
	return -ENOMEM;
    }
    attr.stack_size = stacksize;
    sched_param.sched_priority = prio;
    retval = pthread_attr_setschedparam(&attr, &sched_param);
    if (retval != 0) {
	return -EINVAL;
    }
    /* use pre-determined CPU for RT tasks */
    pthread_attr_setcpu_np(&attr, rtapi_data->rt_cpu);
    pthread_attr_setfp_np(&attr, uses_fp);
    task->taskcode = taskcode;
    task->arg = arg;
    /* call OS to initialize the task - use the wrapper function, passing it
       the task ID so it can call the actual task function */
    retval = pthread_create(&(ostask_array[task_id]), &attr,
	wrapper, (void *) task_id);
    if (retval != 0) {
	/* create failed, free task data memory */
	rtapi_mutex_give(&(rtapi_data->mutex));
	if (retval == ENOMEM) {
	    /* not enough space for stack */
	    return -ENOMEM;
	}
	/* unknown error */
	return -EINVAL;
    }
    /* create worked, set scheduling policy */
    pthread_setschedparam(ostask_array[task_id], SCHED_FIFO, &sched_param);
    /* the task has been created, update data */
    task->state = PAUSED;
    task->prio = prio;
    task->owner = owner;
    rtapi_data->task_count++;
    /* announce the birth of a brand new baby task */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: task %02d installed by module %02d, priority %d, code: %p\n",
	task_id, task->owner, task->prio, taskcode);
    /* and return the ID to the proud parent */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return task_id;
}

int rtapi_task_delete(int task_id)
{
    int retval = 0;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = task_delete(task_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int task_delete(int task_id)
{
    task_data *task;

    /* validate task ID */
    if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
	return -EINVAL;
    }
    /* point to the task's data */
    task = &(task_array[task_id]);
    /* check task status */
    if (task->state == EMPTY) {
	/* nothing to delete */
	return -EINVAL;
    }
    if ((task->state == PERIODIC) || (task->state == FREERUN)) {
	/* task is running, need to stop it */
	rtapi_print_msg(RTAPI_MSG_WARN,
	    "RTAPI: WARNING: tried to delete task %02d while running\n",
	    task_id);
	pthread_suspend_np(ostask_array[task_id]);
	task->state = PAUSED;
    }
    /* get rid of task - delete may be more robust than cancel/join */
    pthread_delete_np(ostask_array[task_id]);
    /* update data */
    task->state = EMPTY;
    task->prio = 0;
    task->owner = 0;
    task->taskcode = NULL;
    ostask_array[task_id] = NULL;
    rtapi_data->task_count--;
    /* if no more tasks, stop the timer */
    if (rtapi_data->task_count == 0) {
	if (rtapi_data->timer_running != 0) {
	    /* in RTLinux, you can't really stop the timer.  Since no
	       realtime task needs it running, we set it to 10mS, and it
	       serves as the Linux jiffies clock.  This seems like a kluge,
	       and probably won't work on platforms where jiffies are not
	       100Hz, but the RTLinux docs don't list a "timer stop" function
	     */
	    rtl_setclockmode(rtl_getschedclock(), RTL_CLOCK_MODE_PERIODIC,
		10000000);
	    rtapi_data->timer_period = 0;
	    max_delay = DEFAULT_MAX_DELAY;
	    rtapi_data->timer_running = 0;
	}
    }
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: task %02d deleted\n", task_id);
    return 0;
}

int rtapi_task_start(int task_id, unsigned long int period_nsec)
{
    int retval = 0;
    unsigned long int quo, rem;
    task_data *task;

    /* validate task ID */
    if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
	return -EINVAL;
    }
    /* point to the task's data */
    task = &(task_array[task_id]);
    /* is task ready to be started? */
    if (task->state != PAUSED) {
	return -EINVAL;
    }
    /* can't start periodic tasks if timer isn't running */
    if ((rtapi_data->timer_running == 0) || (rtapi_data->timer_period == 0)) {
	return -EINVAL;
    }
    /* make period_nsec an integer multiple of timer_period */
    quo = period_nsec / rtapi_data->timer_period;
    rem = period_nsec - (quo * rtapi_data->timer_period);
    /* round to nearest */
    if (rem > (rtapi_data->timer_period / 2)) {
	quo++;
    }
    /* must be at least 1 * timer_period */
    if (quo == 0) {
	quo = 1;
    }
    period_nsec = quo * rtapi_data->timer_period;
    /* start the task */
    retval = pthread_make_periodic_np(ostask_array[task_id],
	gethrtime(), (hrtime_t) period_nsec);
    if (retval != 0) {
	return -EINVAL;
    }
    /* ok, task is started */
    task->state = PERIODIC;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: start_task id: %02d\n", task_id);
    return retval;
}

void rtapi_wait(void)
{
    pthread_wait_np();
}

int rtapi_task_resume(int task_id)
{
    int retval = 0;
    task_data *task;

    /* validate task ID */
    if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
	return -EINVAL;
    }
    /* point to the task's data */
    task = &(task_array[task_id]);
    /* is task ready to be started? */
    if (task->state != PAUSED) {
	return -EINVAL;
    }
    /* start the task */
    retval = pthread_wakeup_np(ostask_array[task_id]);

    if (retval != 0) {
	return -EINVAL;
    }
    /* update task data */
    task->state = FREERUN;
    return 0;
}

int rtapi_task_pause(int task_id)
{
    int retval = 0;
    int oldstate;
    task_data *task;

    /* validate task ID */
    if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
	return -EINVAL;
    }
    /* point to the task's data */
    task = &(task_array[task_id]);
    /* is it running? */
    if ((task->state != PERIODIC) && (task->state != FREERUN)) {
	return -EINVAL;
    }
    /* pause the task */
    oldstate = task->state;
    task->state = PAUSED;
    retval = pthread_suspend_np(ostask_array[task_id]);
    if (retval != 0) {
	task->state = oldstate;
	return -EINVAL;
    }
    /* update task data */
    return 0;
}

int rtapi_task_self(void)
{
    pthread_t ptr;
    int n;

    /* ask OS for pointer to its data for the current task */
    ptr = pthread_self();
    if (ptr == NULL) {
	/* called from outside a task? */
	return -EINVAL;
    }
    /* find matching entry in task array */
    n = 1;
    while (n <= RTAPI_MAX_TASKS) {
	if (ostask_array[n] == ptr) {
	    /* found a match */
	    return n;
	}
	n++;
    }
    return -EINVAL;
}

/***********************************************************************
*                  SHARED MEMORY RELATED FUNCTIONS                     *
************************************************************************/

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
    int n;
    int shmem_id;
    shmem_data *shmem;
    char keystr[KEYSTR_LEN];

    /* key must be non-zero, and also cannot match the key that RTAPI uses */
    if ((key == 0) || (key == RTAPI_KEY)) {
	return -EINVAL;
    }
    /* convert the key to a string */
    genstr((unsigned int) key, keystr);

    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* check if a block is already open for this key */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmem_array[n].key == key) {
	    /* found a match */
	    shmem_id = n;
	    shmem = &(shmem_array[n]);
	    /* is it big enough? */
	    if (shmem->size < size) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	    }
	    /* yes, has it been mapped into kernel space? */
	    if (shmem->rtusers == 0) {
		/* no, map it and save the address */

		/* convert the key to a string */
		genstr((unsigned int) key, keystr);
		shmem_addr_array[shmem_id] = mbuff_alloc(keystr, shmem->size);
		if (shmem_addr_array[shmem_id] == NULL) {
		    rtapi_mutex_give(&(rtapi_data->mutex));
		    return -ENOMEM;
		}
	    }
	    /* is this module already using it? */
	    if (test_bit(module_id, shmem->bitmap)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	    }
	    /* update usage data */
	    set_bit(module_id, shmem->bitmap);
	    shmem->rtusers++;
	    /* announce another user for this shmem */
	    rtapi_print_msg(RTAPI_MSG_DBG,
		"RTAPI: shmem %02d opened by module %02d\n",
		shmem_id, module_id);
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return shmem_id;
	}
    }
    /* find empty spot in shmem array */
    n = 1;
    while ((n <= RTAPI_MAX_SHMEMS) && (shmem_array[n].key != 0)) {
	n++;
    }
    if (n > RTAPI_MAX_SHMEMS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EMFILE;
    }
    /* we have space for the block data */
    shmem_id = n;
    shmem = &(shmem_array[n]);

    /* convert the key to a string */
    genstr((unsigned int) key, keystr);

    /* get shared memory block from OS and save its address */
    shmem_addr_array[shmem_id] = mbuff_alloc(keystr, size);
    if (shmem_addr_array[shmem_id] == NULL) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -ENOMEM;
    }
    /* the block has been created, update data */
    set_bit(module_id, shmem->bitmap);
    shmem->key = key;
    shmem->rtusers = 1;
    shmem->ulusers = 0;
    shmem->size = size;
    rtapi_data->shmem_count++;
    /* zero the first word of the shmem area */
    *((long int *)(shmem_addr_array[shmem_id])) = 0;
    /* announce the birth of a brand new baby shmem */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: shmem %02d created by module %02d, key: %d, size: %lu\n",
	shmem_id, module_id, key, size);
    /* and return the ID to the proud parent */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return shmem_id;
}

int rtapi_shmem_delete(int shmem_id, int module_id)
{
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = shmem_delete(shmem_id, module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int shmem_delete(int shmem_id, int module_id)
{
    shmem_data *shmem;
    char keystr[KEYSTR_LEN];

    /* validate shmem ID */
    if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
	return -EINVAL;
    }
    /* point to the shmem's data */
    shmem = &(shmem_array[shmem_id]);
    /* is the block valid? */
    if (shmem->key == 0) {
	return -EINVAL;
    }
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	return -EINVAL;
    }
    /* is this module using the block? */
    if (test_bit(module_id, shmem->bitmap) == 0) {
	return -EINVAL;
    }
    /* OK, we're no longer using it */
    clear_bit(module_id, shmem->bitmap);
    shmem->rtusers--;
    /* is somebody else still using the block? */
    if (shmem->rtusers > 0) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: shmem %02d closed by module %02d\n", shmem_id, module_id);
	return 0;
    }
    /* convert the key to a string */
    genstr(shmem->key, keystr);
    /* no other realtime users, free the shared memory from kernel space */
    mbuff_free(keystr, shmem_addr_array[shmem_id]);
    shmem_addr_array[shmem_id] = NULL;
    shmem->rtusers = 0;
    /* are any user processes using the block? */
    if (shmem->ulusers > 0) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: shmem %02d unmapped by module %02d\n", shmem_id,
	    module_id);
	return 0;
    }
    /* no other users at all, this ID is now free */
    /* update the data array and usage count */
    shmem->key = 0;
    shmem->size = 0;
    rtapi_data->shmem_count--;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: shmem %02d freed by module %02d\n",
	shmem_id, module_id);
    return 0;
}

int rtapi_shmem_getptr(int shmem_id, void **ptr)
{
    /* validate shmem ID */
    if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
	return -EINVAL;
    }
    /* is the block mapped? */
    if (shmem_addr_array[shmem_id] == NULL) {
	return -EINVAL;
    }
    /* pass memory address back to caller */
    *ptr = shmem_addr_array[shmem_id];
    return 0;
}

/***********************************************************************
*                    SEMAPHORE RELATED FUNCTIONS                       *
************************************************************************/

int rtapi_sem_new(int key, int module_id)
{
    int n;
    int sem_id;
    sem_data *sem;

    /* key must be non-zero */
    if (key == 0) {
	return -EINVAL;
    }
    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* check if a semaphore already exists for this key */
    for (n = 1; n <= RTAPI_MAX_SEMS; n++) {
	if ((sem_array[n].users > 0) && (sem_array[n].key == key)) {
	    /* found a match */
	    sem_id = n;
	    sem = &(sem_array[n]);
	    /* is this module already using it? */
	    if (test_bit(module_id, sem->bitmap)) {
		/* yes, can't open it again */
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	    }
	    /* update usage data */
	    set_bit(module_id, sem->bitmap);
	    sem->users++;
	    /* announce another user for this semaphore */
	    rtapi_print_msg(RTAPI_MSG_DBG,
		"RTAPI: sem %02d opened by module %02d\n", sem_id, module_id);
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return sem_id;
	}
    }
    /* find empty spot in sem array */
    n = 1;
    while ((n <= RTAPI_MAX_SEMS) && (sem_array[n].users != 0)) {
	n++;
    }
    if (n > RTAPI_MAX_SEMS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EMFILE;
    }
    /* we have space for the semaphore */
    sem_id = n;
    sem = &(sem_array[n]);
    /* ask the OS to initialize the semaphore */
    sem_init(&(ossem_array[sem_id]), 0, 0);
    /* the semaphore has been created, update data */
    set_bit(module_id, sem->bitmap);
    sem->users = 1;
    sem->key = key;
    rtapi_data->sem_count++;
    /* announce the birth of a brand new baby semaphore */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: sem %02d created by module %02d, key: %d\n",
	sem_id, module_id, key);
    /* and return the ID to the proud parent */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return sem_id;
}

int rtapi_sem_delete(int sem_id, int module_id)
{
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = sem_delete(sem_id, module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int sem_delete(int sem_id, int module_id)
{
    sem_data *sem;

    /* validate sem ID */
    if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
	return -EINVAL;
    }
    /* point to the semaphores's data */
    sem = &(sem_array[sem_id]);
    /* is the semaphore valid? */
    if (sem->users == 0) {
	return -EINVAL;
    }
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	return -EINVAL;
    }
    /* is this module using the semaphore? */
    if (test_bit(module_id, sem->bitmap) == 0) {
	return -EINVAL;
    }
    /* OK, we're no longer using it */
    clear_bit(module_id, sem->bitmap);
    sem->users--;
    /* is somebody else still using the semaphore */
    if (sem->users > 0) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: sem %02d closed by module %02d\n", sem_id, module_id);
	return 0;
    }
    /* no other users, ask the OS to shut down the semaphore */
    sem_destroy(&(ossem_array[sem_id]));
    /* update the data array and usage count */
    sem->users = 0;
    sem->key = 0;
    rtapi_data->sem_count--;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: sem %02d deleted by module %02d\n",
	sem_id, module_id);
    return 0;
}

int rtapi_sem_give(int sem_id)
{
    sem_data *sem;

    /* validate sem ID */
    if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
	return -EINVAL;
    }
    /* point to the semaphores's data */
    sem = &(sem_array[sem_id]);
    /* is the semaphore valid? */
    if (sem->users == 0) {
	return -EINVAL;
    }
    /* give up the semaphore */
    sem_post(&(ossem_array[sem_id]));
    return 0;
}

int rtapi_sem_take(int sem_id)
{
    sem_data *sem;

    /* validate sem ID */
    if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
	return -EINVAL;
    }
    /* point to the semaphores's data */
    sem = &(sem_array[sem_id]);
    /* is the semaphore valid? */
    if (sem->users == 0) {
	return -EINVAL;
    }
    /* get the semaphore */
    sem_wait(&(ossem_array[sem_id]));
    return 0;
}

int rtapi_sem_try(int sem_id)
{
    sem_data *sem;

    /* validate sem ID */
    if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
	return -EINVAL;
    }
    /* point to the semaphores's data */
    sem = &(sem_array[sem_id]);
    /* is the semaphore valid? */
    if (sem->users == 0) {
	return -EINVAL;
    }
    /* try the semaphore */
    if (sem_trywait(&(ossem_array[sem_id])) <= 0) {
	return -EBUSY;
    }
    return 0;
}

/***********************************************************************
*                        FIFO RELATED FUNCTIONS                        *
************************************************************************/

int rtapi_fifo_new(int key, int module_id, unsigned long int size, char mode)
{
    int n, retval;
    int fifo_id;
    fifo_data *fifo;

    /* key must be non-zero */
    if (key == 0) {
	return -EINVAL;
    }
    /* mode must be "R" or "W" */
    if ((mode != 'R') && (mode != 'W')) {
	return -EINVAL;
    }
    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* check if a fifo already exists for this key */
    for (n = 1; n <= RTAPI_MAX_FIFOS; n++) {
	if ((fifo_array[n].state != UNUSED) && (fifo_array[n].key == key)) {
	    /* found a match */
	    fifo_id = n;
	    fifo = &(fifo_array[n]);
	    /* is the desired mode available */
	    if (mode == 'R') {
		if (fifo->state & HAS_READER) {
		    rtapi_mutex_give(&(rtapi_data->mutex));
		    return -EBUSY;
		}
		/* available, update status */
		fifo->state |= HAS_READER;
		fifo->reader = module_id;
		/* announce */
		rtapi_print_msg(RTAPI_MSG_DBG,
		    "RTAPI: fifo %02d opened for read by module %02d\n",
		    fifo_id, module_id);
		rtapi_mutex_give(&(rtapi_data->mutex));
		return fifo_id;
	    } else {		/* mode == 'W' */

		if (fifo->state & HAS_WRITER) {
		    rtapi_mutex_give(&(rtapi_data->mutex));
		    return -EBUSY;
		}
		/* available, update status */
		fifo->state |= HAS_WRITER;
		fifo->writer = module_id;
		/* announce */
		rtapi_print_msg(RTAPI_MSG_DBG,
		    "RTAPI: fifo %02d opened for write by module %02d\n",
		    fifo_id, module_id);
		rtapi_mutex_give(&(rtapi_data->mutex));
		return fifo_id;
	    }
	}
    }
    /* find empty spot in fifo array */
    n = 1;
    while ((n <= RTAPI_MAX_FIFOS) && (fifo_array[n].state != UNUSED)) {
	n++;
    }
    if (n > RTAPI_MAX_FIFOS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EMFILE;
    }
    /* we have a free ID for the fifo */
    fifo_id = n;
    fifo = &(fifo_array[n]);
    /* create the fifo */
    retval = rtf_create(fifo_id, size);
    /* rtf_create() returns 0 on success */
    if (retval != 0) {
	/* create failed */
	rtapi_mutex_give(&(rtapi_data->mutex));
	if (retval == ENOMEM) {
	    /* couldn't allocate memory */
	    return -ENOMEM;
	}
	/* some other failure */
	return -EINVAL;
    }
    /* the fifo has been created, update data */
    if (mode == 'R') {
	fifo->state = HAS_READER;
	fifo->reader = module_id;
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: fifo %02d created for read by module %02d, key: %d, size: %ld\n",
	    fifo_id, module_id, key, size);
    } else {			/* mode == 'W' */

	fifo->state = HAS_WRITER;
	fifo->writer = module_id;
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: fifo %02d created for write by module %02d, key: %d, size: %ld\n",
	    fifo_id, module_id, key, size);
    }
    fifo->key = key;
    fifo->size = size;
    rtapi_data->fifo_count++;
    /* and return the ID */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return fifo_id;
}

int rtapi_fifo_delete(int fifo_id, int module_id)
{
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = fifo_delete(fifo_id, module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int fifo_delete(int fifo_id, int module_id)
{
    fifo_data *fifo;

    /* validate fifo ID */
    if ((fifo_id < 1) || (fifo_id > RTAPI_MAX_FIFOS)) {
	return -EINVAL;
    }
    /* point to the fifo's data */
    fifo = &(fifo_array[fifo_id]);
    /* is the fifo valid? */
    if (fifo->state == UNUSED) {
	return -EINVAL;
    }
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	return -EINVAL;
    }
    /* is this module using the fifo? */
    if ((fifo->reader != module_id) && (fifo->writer != module_id)) {
	return -EINVAL;
    }
    /* update fifo state */
    if (fifo->reader == module_id) {
	fifo->state &= ~HAS_READER;
	fifo->reader = 0;
    }
    if (fifo->writer == module_id) {
	fifo->state &= ~HAS_WRITER;
	fifo->writer = 0;
    }
    /* is somebody else still using the fifo */
    if (fifo->state != UNUSED) {
	/* yes, done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: fifo %02d closed by module %02d\n", fifo_id, module_id);
	return 0;
    }
    /* no other users, call the OS to destroy the fifo */
    /* OS returns open count, loop until truly destroyed */
    while (rtf_destroy(fifo_id) > 0);
    /* update the data array and usage count */
    fifo->state = UNUSED;
    fifo->key = 0;
    fifo->size = 0;
    rtapi_data->fifo_count--;
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: fifo %02d deleted by module %02d\n", fifo_id, module_id);
    return 0;
}

int rtapi_fifo_read(int fifo_id, char *buf, unsigned long int size)
{
    int retval;
    fifo_data *fifo;

    /* validate fifo ID */
    if ((fifo_id < 1) || (fifo_id > RTAPI_MAX_FIFOS)) {
	return -EINVAL;
    }
    /* point to the fifo's data */
    fifo = &(fifo_array[fifo_id]);
    /* is the fifo valid? */
    if ((fifo->state & HAS_READER) == 0) {
	return -EINVAL;
    }
    /* get whatever data is available */
    retval = rtf_get(fifo_id, &buf, size);
    if (retval < 0) {
	return -EINVAL;
    }
    return retval;
}

int rtapi_fifo_write(int fifo_id, char *buf, unsigned long int size)
{
    int retval;
    fifo_data *fifo;

    /* validate fifo ID */
    if ((fifo_id < 1) || (fifo_id > RTAPI_MAX_FIFOS)) {
	return -EINVAL;
    }
    /* point to the fifo's data */
    fifo = &(fifo_array[fifo_id]);
    /* is the fifo valid? */
    if ((fifo->state & HAS_WRITER) == 0) {
	return -EINVAL;
    }
    /* put as much data as possible */
    retval = rtf_put(fifo_id, buf, size);
    if (retval < 0) {
	return -EINVAL;
    }
    return retval;
}

/***********************************************************************
*                    INTERRUPT RELATED FUNCTIONS                       *
************************************************************************/

typedef unsigned int (*handler_t) (unsigned int irq_number,
    struct pt_regs * p);

int rtapi_irq_new(unsigned int irq_num, int owner, void (*handler) (void))
{
    int n;
    int retval = 0;
    int irq_id;
    irq_data *irq;

    /* validate irq */
    if ((irq_num < 1) || (irq_num > 255)) {
	return -EINVAL;
    }
    /* validate handler */
    if (handler == NULL) {
	return -EINVAL;
    }
    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate owner */
    if ((owner < 1) || (owner > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    if (module_array[owner].state != REALTIME) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* check if a handler already exists for this irq */
    for (n = 1; n <= RTAPI_MAX_IRQS; n++) {
	if (irq_array[n].irq_num == irq_num) {
	    /* found a match */
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return -EBUSY;
	}
    }
    /* find empty spot in irq array */
    n = 1;
    while ((n <= RTAPI_MAX_IRQS) && (irq_array[n].irq_num != 0)) {
	n++;
    }
    if (n > RTAPI_MAX_IRQS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EMFILE;
    }
    /* we have space for the irq */
    irq_id = n;
    irq = &(irq_array[n]);
    /* install the handler */
    retval = rtl_request_irq(irq_num, (handler_t) handler);
    if (retval != 0) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	if (retval == EBUSY) {
	    return -EBUSY;
	} else {
	    return -EINVAL;
	}
    }
    /* update data */
    irq->irq_num = irq_num;
    irq->owner = owner;
    irq->handler = handler;
    rtapi_data->irq_count++;
    /* announce the new interrupt handler */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: handler for IRQ %d installed by module %02d\n",
	irq_num, owner);
    /* and return success */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return 0;
}

int rtapi_irq_delete(unsigned int irq_num)
{
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = irq_delete(irq_num);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int irq_delete(unsigned int irq_num)
{
    int n;
    int retval = 0;
    int irq_id;
    int old_irq_state;
    irq_data *irq;

    /* validate irq */
    if ((irq_num < 1) || (irq_num > 255)) {
	return -EINVAL;
    }
    /* check if a handler exists for this irq */
    n = 1;
    while ((n <= RTAPI_MAX_IRQS) && (irq_array[n].irq_num != irq_num)) {
	n++;
    }
    if (n > RTAPI_MAX_IRQS) {
	/* not found */
	return -EINVAL;
    }
    /* found the irq */
    irq_id = n;
    irq = &(irq_array[n]);
    /* disable interrupts */
    rtl_no_interrupts(old_irq_state);
    /* get rid of the handler */
    retval = rtl_free_irq(irq_num);
    /* re-enable interrupts */
    rtl_restore_interrupts(old_irq_state);

    if (retval != 0) {
	return -EINVAL;
    }
    /* update data */
    irq->irq_num = 0;
    irq->owner = 0;
    irq->handler = NULL;
    rtapi_data->irq_count--;
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: handler for IRQ %d deleted\n", irq_num);
    return 0;
}

int rtapi_enable_interrupt(unsigned int irq)
{
    rtl_hard_enable_irq(irq);

    return 0;
}

int rtapi_disable_interrupt(unsigned int irq)
{
    rtl_hard_disable_irq(irq);

    return 0;
}

/***********************************************************************
*                        I/O RELATED FUNCTIONS                         *
************************************************************************/

void rtapi_outb(unsigned char byte, unsigned int port)
{
    outb(byte, port);
}

unsigned char rtapi_inb(unsigned int port)
{
    return inb(port);
}

/*export necessary function symbol*/
EXPORT_SYMBOL(rtapi_init);
EXPORT_SYMBOL(rtapi_exit);
EXPORT_SYMBOL(rtapi_task_new);
EXPORT_SYMBOL(rtapi_prio_next_lower);
EXPORT_SYMBOL(rtapi_prio_highest);
EXPORT_SYMBOL(rtapi_vsnprintf);
EXPORT_SYMBOL(rtapi_wait);
EXPORT_SYMBOL(rtapi_task_pause);
EXPORT_SYMBOL(rtapi_clock_set_period);
EXPORT_SYMBOL(rtapi_print_msg);
EXPORT_SYMBOL(rtapi_shmem_getptr);
EXPORT_SYMBOL(rtapi_get_clocks);
EXPORT_SYMBOL(rtapi_shmem_delete);
EXPORT_SYMBOL(rtapi_task_delete);
EXPORT_SYMBOL(rtapi_shmem_new);
EXPORT_SYMBOL(rtapi_snprintf);
EXPORT_SYMBOL(rtapi_task_start);
EXPORT_SYMBOL(rtapi_set_msg_handler);
EXPORT_SYMBOL(rtapi_get_msg_handler);
EXPORT_SYMBOL(rtapi_set_msg_level);
EXPORT_SYMBOL(rtapi_get_msg_level);
EXPORT_SYMBOL(rtapi_get_time);
EXPORT_SYMBOL(rtapi_print);
