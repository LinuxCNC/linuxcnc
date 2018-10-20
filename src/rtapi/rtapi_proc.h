#ifndef RTAPI_PROC_H
#define RTAPI_PROC_H

/** RTAPI is a library providing a uniform API for several real time
  operating systems.  As of ver 2.0, RTLinux and RTAI are supported.
*/
/********************************************************************
* Description:  rtai_proc.h
*               This file, 'rtapi_proc.h', contains code that 
*               implements several /proc filesystem entries that can 
*               display the status of the RTAPI.
*
* Author: John Kasunich, Paul Corner
* License: LGPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

/** This file, 'rtapi_proc.h', contains code that implements several
    /proc filesystem entries that can display the status of the RTAPI.
    This code is common to both the RTAI and RTLinux implementations,
    and most likely to any other implementations under Linux.  This
    data is INTERNAL to the RTAPI implementation, and should not be
    included in any application modules.  This data also applies
    only to kernel modules, and should be included only in the
    real-time portion of the implementation.  Items that are common
    to both the realtime and user-space portions of the implementation
    are in rtapi_common.h.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
    Copyright (C) 2003 Paul Corner
                       <paul_c AT users DOT sourceforge DOT net>

    This library is based on version 1.0, which was released into
    the public domain by its author, Fred Proctor.  Thanks Fred!
*/

/* This library is free software; you can redistribute it and/or
  modify it under the terms of version 2.1 of the GNU Lesser General
  Public License as published by the Free Software Foundation.
  This library is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/** THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.
*/

/** This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

/* Internal function for the proc_fs system. */

/* The proc file system is available in 2.2 and 2.4 kernels with 
  minor differences - The 2.4 kernels have a useful helper function
  for creating the proc_fs entries.
  It is unlikely that the following implimentaion will work on a 2.0
  series kernel..
*/
#if defined( CONFIG_PROC_FS ) && LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)

#define RTAPI_USE_PROCFS

#include "procfs_macros.h"	/* macros for read functions */

struct proc_dir_entry *rtapi_dir = 0;	/* /proc/rtapi directory */
static struct proc_dir_entry *status_file = 0;	/* /proc/rtapi/status */
static struct proc_dir_entry *modules_file = 0;	/* /proc/rtapi/modules */
static struct proc_dir_entry *tasks_file = 0;	/* /proc/rtapi/tasks */
static struct proc_dir_entry *shmem_file = 0;	/* /proc/rtapi/shmem */
static struct proc_dir_entry *sems_file = 0;	/* /proc/rtapi/sems */
static struct proc_dir_entry *fifos_file = 0;	/* /proc/rtapi/fifos */
static struct proc_dir_entry *debug_file = 0;	/* /proc/rtapi/debug */

/** The following are callback functions for the /proc filesystem
    When someone reads a /proc file, the appropriate function below
    is called, and it must generate output for the reader on the fly.
    These functions use the MOD_INC_USE_COUNT and MOD_DEC_USE_COUNT
    macros to make sure the RTAPI module is not removed while servicing
    a /proc request.
*/

static int proc_read_status(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    PROC_PRINT_VARS;
    PROC_PRINT("******* RTAPI STATUS ********\n");
    PROC_PRINT("   RT Modules = %i\n", rtapi_data->rt_module_count);
    PROC_PRINT("   UL Modules = %i\n", rtapi_data->ul_module_count);
    PROC_PRINT("        Tasks = %i/%i\n", rtapi_data->task_count,
	RTAPI_MAX_TASKS);
    PROC_PRINT("Shared memory = %i/%i\n", rtapi_data->shmem_count,
	RTAPI_MAX_SHMEMS);
    PROC_PRINT("        FIFOs = %i/%i\n", rtapi_data->fifo_count,
	RTAPI_MAX_FIFOS);
    PROC_PRINT("   Semaphores = %i/%i\n", rtapi_data->sem_count,
	RTAPI_MAX_SEMS);
    PROC_PRINT("   Interrupts = %i\n", rtapi_data->irq_count);
    PROC_PRINT("  RT task CPU = %i\n", rtapi_data->rt_cpu);
    if (rtapi_data->timer_running) {
	PROC_PRINT(" Timer status = Running\n");
	PROC_PRINT(" Timer period = %li nSec\n", rtapi_data->timer_period);
    } else {
	PROC_PRINT(" Timer status = Stopped\n");
    }
    PROC_PRINT("Message level = %i\n", msg_level);
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static int proc_read_modules(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    int n;
    char *state_str;

    PROC_PRINT_VARS;
    PROC_PRINT("******* RTAPI MODULES *******\n");
    PROC_PRINT("ID  Type  Name\n");
    for (n = 1; n <= RTAPI_MAX_MODULES; n++) {
	if (module_array[n].state != NO_MODULE) {
	    switch (module_array[n].state) {
	    case REALTIME:
		state_str = "RT  ";
		break;
	    case USERSPACE:
		state_str = "USER";
		break;
	    default:
		state_str = "????";
		break;
	    }
	    PROC_PRINT("%02d  %s  %s\n", n, state_str, module_array[n].name);
	}
    }
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static int proc_read_tasks(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    int n;
    char *state_str;

    PROC_PRINT_VARS;
    PROC_PRINT("******** RTAPI TASKS ********\n");
    PROC_PRINT("ID  Own  Prio  State     Code\n");
    for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
	if (task_array[n].state != EMPTY) {
	    switch (task_array[n].state) {
	    case PAUSED:
		state_str = "PAUSED  ";
		break;
	    case PERIODIC:
		state_str = "PERIODIC";
		break;
	    case FREERUN:
		state_str = "FREE RUN";
		break;
	    case ENDED:
		state_str = "ENDED   ";
		break;
	    default:
		state_str = "UNKNOWN ";
		break;
	    }
	    PROC_PRINT("%02d  %02d   %3d   %s  %p\n", n, task_array[n].owner,
		task_array[n].prio, state_str, task_array[n].taskcode);
	}
    }
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static int proc_read_shmem(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    int n;

    PROC_PRINT_VARS;
    PROC_PRINT("**** RTAPI SHARED MEMORY ****\n");
    PROC_PRINT("ID  Users  Key         Size\n");
    PROC_PRINT("    RT/UL                  \n");
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmem_array[n].key != 0) {
	    PROC_PRINT("%02d  %2d/%-2d  %-10d  %-10lu\n",
		n, shmem_array[n].rtusers, shmem_array[n].ulusers,
		shmem_array[n].key, shmem_array[n].size);
	}
    }
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static int proc_read_sems(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    int n;

    PROC_PRINT_VARS;
    PROC_PRINT("***** RTAPI SEMAPHORES ******\n");
    PROC_PRINT("ID  Users  Key\n");
    for (n = 1; n <= RTAPI_MAX_SEMS; n++) {
	if (sem_array[n].users != 0) {
	    PROC_PRINT("%02d  %3d    %-10d\n",
		n, sem_array[n].users, sem_array[n].key);
	}
    }
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static int proc_read_fifos(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    int n;
    char *state_str;

    PROC_PRINT_VARS;
    PROC_PRINT("******** RTAPI FIFOS ********\n");
    PROC_PRINT("ID  State  Key         Size\n");
    for (n = 1; n <= RTAPI_MAX_FIFOS; n++) {
	if (fifo_array[n].state != UNUSED) {
	    switch (fifo_array[n].state) {
	    case HAS_READER:
		state_str = "R-";
		break;
	    case HAS_WRITER:
		state_str = "-W";
		break;
	    case HAS_BOTH:
		state_str = "RW";
		break;
	    default:
		state_str = "UNKNOWN ";
		break;
	    }
	    PROC_PRINT("%02d   %s    %-10d  %-10ld\n",
		n, state_str, fifo_array[n].key, fifo_array[n].size);
	}
    }
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static int proc_read_debug(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    PROC_PRINT_VARS;
    PROC_PRINT("******* RTAPI MESSAGES ******\n");
    PROC_PRINT("  Message Level  = %i\n", msg_level);
    PROC_PRINT("  ERROR messages = %s\n",
	msg_level >= RTAPI_MSG_ERR ? "ON" : "OFF");
    PROC_PRINT("WARNING messages = %s\n",
	msg_level >= RTAPI_MSG_WARN ? "ON" : "OFF");
    PROC_PRINT("   INFO messages = %s\n",
	msg_level >= RTAPI_MSG_INFO ? "ON" : "OFF");
    PROC_PRINT("  DEBUG messages = %s\n",
	msg_level >= RTAPI_MSG_DBG ? "ON" : "OFF");
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static int proc_write_debug(struct file *file,
    const char *buffer, unsigned long count, void *data)
{
    char c;

    /* copy 1 byte from user space */
    if (copy_from_user(&c, buffer, 1)) {
	return -1;
    }
    /* check it is a digit */
    if (isdigit(c)) {
	/* convert to a number */
	msg_level = (int) (c - '0');
	/* cap the value if it is outside the valid range */
	if (msg_level < RTAPI_MSG_NONE) {
	    msg_level = RTAPI_MSG_NONE;
	}
	if (msg_level > RTAPI_MSG_ALL) {
	    msg_level = RTAPI_MSG_ALL;
	}
    }
    /* tell whoever called us that we used all the data, even though we
       really only used the first byte */
    return count;
}

/** proc_init() initializes the /proc filesystem entries,
    creating the directory and files, and linking them
    to the appropriate callback functions. This function
    is called from the init_module() function of the
    RTAPI implementation.
*/

static int proc_init(void)
{
    /* create the rtapi directory "/proc/rtapi" */
    rtapi_dir = create_proc_entry("rtapi", S_IFDIR, NULL);
    if (rtapi_dir == 0) {
	return -1;
    }

    /* create read only file "/proc/rtapi/status" */
    status_file = create_proc_entry("status", S_IRUGO, rtapi_dir);
    if (status_file == NULL) {
	return -1;
    }
    status_file->read_proc = proc_read_status;

    /* create read only file "/proc/rtapi/modules" */
    modules_file = create_proc_entry("modules", S_IRUGO, rtapi_dir);
    if (modules_file == NULL) {
	return -1;
    }
    modules_file->read_proc = proc_read_modules;

    /* create read only file "/proc/rtapi/tasks" */
    tasks_file = create_proc_entry("tasks", S_IRUGO, rtapi_dir);
    if (tasks_file == NULL) {
	return -1;
    }
    tasks_file->read_proc = proc_read_tasks;

    /* create read only file "/proc/rtapi/shmem" */
    shmem_file = create_proc_entry("shmem", S_IRUGO, rtapi_dir);
    if (shmem_file == NULL) {
	return -1;
    }
    shmem_file->read_proc = proc_read_shmem;

    /* create read only file "/proc/rtapi/sems" */
    sems_file = create_proc_entry("sems", S_IRUGO, rtapi_dir);
    if (sems_file == NULL) {
	return -1;
    }
    sems_file->read_proc = proc_read_sems;

    /* create read only file "/proc/rtapi/fifos" */
    fifos_file = create_proc_entry("fifos", S_IRUGO, rtapi_dir);
    if (fifos_file == NULL) {
	return -1;
    }
    fifos_file->read_proc = proc_read_fifos;

    /* create read/write file "/proc/rtapi/debug" */
    debug_file = create_proc_entry("debug", S_IRUGO | S_IWUGO, rtapi_dir);
    if (debug_file == NULL) {
	return -1;
    }
    debug_file->data = NULL;
    debug_file->read_proc = proc_read_debug;
    debug_file->write_proc = proc_write_debug;
    return 0;
}

/** proc_clean() is called from the cleanup_module function of
    of the RTAPI implementation.  It removes the rtapi entries
    from the /proc filesystem.  Failing to remove a /proc
    entry before the module is removed may cause kernel panics.
*/

static void proc_clean(void)
{
    /* remove /proc entries, only if they exist */
    if (rtapi_dir != NULL) {
	if (status_file != NULL) {
	    remove_proc_entry("status", rtapi_dir);
	    status_file = NULL;
	}
	if (modules_file != NULL) {
	    remove_proc_entry("modules", rtapi_dir);
	    modules_file = NULL;
	}
	if (tasks_file != NULL) {
	    remove_proc_entry("tasks", rtapi_dir);
	    tasks_file = NULL;
	}
	if (shmem_file != NULL) {
	    remove_proc_entry("shmem", rtapi_dir);
	    shmem_file = NULL;
	}
	if (sems_file != NULL) {
	    remove_proc_entry("sems", rtapi_dir);
	    sems_file = NULL;
	}
	if (fifos_file != NULL) {
	    remove_proc_entry("fifos", rtapi_dir);
	    fifos_file = NULL;
	}
	if (debug_file != NULL) {
	    remove_proc_entry("debug", rtapi_dir);
	    debug_file = NULL;
	}
	remove_proc_entry("rtapi", NULL);
    }
}

EXPORT_SYMBOL(rtapi_dir);

#endif /* CONFIG_PROC_FS */
#endif /* RTAPI_PROC_H */
