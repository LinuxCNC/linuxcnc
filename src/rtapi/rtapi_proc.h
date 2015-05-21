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
*/

/** This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

/* Internal function for the proc_fs system. */

/* The proc file system is available in 2.2 and 2.4 kernels with 
  minor differences - The 2.4 kernels have a usefull helper function
  for creating the proc_fs entries.
  It is unlikely that the following implimentaion will work on a 2.0
  series kernel..
*/
#ifdef CONFIG_PROC_FS

#include <linux/ctype.h>	/* isdigit */
#include "procfs_macros.h"	/* macros for read functions */
#include <asm/uaccess.h>	/* copy_from_user() */

struct proc_dir_entry *rtapi_dir = 0;	/* /proc/rtapi directory */
static struct proc_dir_entry *status_file = 0;	/* /proc/rtapi/status */
static struct proc_dir_entry *modules_file = 0;	/* /proc/rtapi/modules */
static struct proc_dir_entry *tasks_file = 0;	/* /proc/rtapi/tasks */
static struct proc_dir_entry *shmem_file = 0;	/* /proc/rtapi/shmem */
static struct proc_dir_entry *debug_file = 0;	/* /proc/rtapi/debug */
static struct proc_dir_entry *instance_file = 0;	/* /proc/rtapi/instance */

/** The following are callback functions for the /proc filesystem
    When someone reads a /proc file, the appropriate function below
    is called, and it must generate output for the reader on the fly.
    These functions use the MOD_INC_USE_COUNT and MOD_DEC_USE_COUNT
    macros to make sure the RTAPI module is not removed while servicing
    a /proc request.
*/


// thread flavors may provide a function to make extra data available
// in procfs
#ifdef HAVE_RTAPI_READ_STATUS_HOOK
void rtapi_proc_read_status_hook(char *page, char **start, off_t off,
				 int count, int *eof, void *data);
#endif

PROC_READ_FUN(proc_read_status)
{
    PROC_PRINT_VARS;
    PROC_PRINT("******* RTAPI STATUS ********\n");
    PROC_PRINT("   RT Modules = %i\n", rtapi_data->rt_module_count);
    PROC_PRINT("   UL Modules = %i\n", rtapi_data->ul_module_count);
    PROC_PRINT("        Tasks = %i/%i\n", rtapi_data->task_count,
	       RTAPI_MAX_TASKS);
    PROC_PRINT("Shared memory = %i/%i\n", rtapi_data->shmem_count,
	       RTAPI_MAX_SHMEMS);
    PROC_PRINT("default RT task CPU = %i\n", rtapi_data->rt_cpu);
    if (rtapi_data->timer_running) {
	PROC_PRINT(" Timer status = Running\n");
	PROC_PRINT(" Timer period = %li nSec\n", rtapi_data->timer_period);
    } else {
	PROC_PRINT(" Timer status = Stopped\n");
    }
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}
PROC_READ_OPEN_OPS(status_file_fops, proc_read_status);

PROC_READ_FUN(proc_read_modules)
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
PROC_READ_OPEN_OPS(modules_file_fops, proc_read_modules);

PROC_READ_FUN(proc_read_tasks)
{
    int n;
    char *state_str;

    PROC_PRINT_VARS;
    PROC_PRINT("******** RTAPI TASKS ********\n");
    PROC_PRINT("ID CPU Own  Prio  State     Code\n");
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
	    PROC_PRINT("%02d %2d  %02d   %3d   %s  %p\n", 
		       n, 
		       task_array[n].cpu, 
		       task_array[n].owner,
		       task_array[n].prio,  
		       state_str, 
		       task_array[n].taskcode);
	}
    }
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}
PROC_READ_OPEN_OPS(tasks_file_fops, proc_read_tasks);

PROC_READ_FUN(proc_read_shmem)
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
PROC_READ_OPEN_OPS(shmem_file_fops, proc_read_shmem);

PROC_READ_FUN(proc_read_debug)
{
    PROC_PRINT_VARS;
    PROC_PRINT("******* RTAPI MESSAGES ******\n");
    PROC_PRINT("  Message Level  = RT:%i User:%i\n", 
	       global_data->rt_msg_level,global_data->user_msg_level);
    PROC_PRINT("RT ERROR messages = %s\n",
	       global_data->rt_msg_level >= RTAPI_MSG_ERR ? "ON" : "OFF");
    PROC_PRINT("WARNING messages = %s\n",
	       global_data->rt_msg_level >= RTAPI_MSG_WARN ? "ON" : "OFF");
    PROC_PRINT("   INFO messages = %s\n",
	       global_data->rt_msg_level >= RTAPI_MSG_INFO ? "ON" : "OFF");
    PROC_PRINT("  DEBUG messages = %s\n",
	       global_data->rt_msg_level >= RTAPI_MSG_DBG ? "ON" : "OFF");

    PROC_PRINT("User  ERROR messages = %s\n",
	       global_data->user_msg_level >= RTAPI_MSG_ERR ? "ON" : "OFF");
    PROC_PRINT("WARNING messages = %s\n",
	       global_data->user_msg_level >= RTAPI_MSG_WARN ? "ON" : "OFF");
    PROC_PRINT("   INFO messages = %s\n",
	       global_data->user_msg_level >= RTAPI_MSG_INFO ? "ON" : "OFF");
    PROC_PRINT("  DEBUG messages = %s\n",
	       global_data->user_msg_level >= RTAPI_MSG_DBG ? "ON" : "OFF");
    PROC_PRINT("\n");
    PROC_PRINT_DONE;
}

static ssize_t proc_write_debug(struct file *file,
				const char __user *buffer, size_t count,
				loff_t *data)
{
    char c;
    int msg_level;

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
	global_data-> rt_msg_level =  msg_level;
    }
    /* tell whoever called us that we used all the data, even though we
       really only used the first byte */
    return count;
}
PROC_READ_WRITE_OPEN_OPS(debug_file_fops, proc_read_debug, proc_write_debug)

PROC_READ_FUN(proc_read_instance)
{
    PROC_PRINT_VARS;
    PROC_PRINT("%i\n", rtapi_instance);
    PROC_PRINT_DONE;
}
PROC_READ_OPEN_OPS(instance_file_fops, proc_read_instance);

/** proc_init() initializes the /proc filesystem entries,
    creating the directory and files, and linking them
    to the appropriate callback functions. This function
    is called from the init_module() function of the
    RTAPI implementation.
*/

static int proc_init(void)
{
    /* create the rtapi directory "/proc/rtapi" */
    rtapi_dir = CREATE_PROC_ENTRY("rtapi",S_IFDIR,NULL,NULL);
    if (rtapi_dir == 0)
	return -1;

    /* create read only file "/proc/rtapi/status" */
    status_file = \
	CREATE_PROC_ENTRY("status",S_IRUGO,rtapi_dir,&status_file_fops);

    /* create read only file "/proc/rtapi/modules" */
    modules_file = \
	CREATE_PROC_ENTRY("modules",S_IRUGO,rtapi_dir,&modules_file_fops);

    /* create read only file "/proc/rtapi/tasks" */
    tasks_file = \
	CREATE_PROC_ENTRY("tasks",S_IRUGO,rtapi_dir,&tasks_file_fops);

    /* create read only file "/proc/rtapi/shmem" */
    shmem_file = \
	CREATE_PROC_ENTRY("shmem",S_IRUGO,rtapi_dir,&shmem_file_fops);

    /* create read/write file "/proc/rtapi/debug" */
    debug_file = \
	CREATE_PROC_ENTRY("debug",S_IRUGO|S_IWUGO,rtapi_dir,&debug_file_fops);

    /* create read only file "/proc/rtapi/instance" */
    instance_file = \
	CREATE_PROC_ENTRY("instance",S_IRUGO,rtapi_dir,&instance_file_fops);

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
	if (debug_file != NULL) {
	    remove_proc_entry("debug", rtapi_dir);
	    debug_file = NULL;
	}
	if (instance_file != NULL) {
	    remove_proc_entry("instance", rtapi_dir);
	    instance_file = NULL;
	}
	remove_proc_entry("rtapi", NULL);
    }
}

EXPORT_SYMBOL(rtapi_dir);

#endif /* CONFIG_PROC_FS */
#endif /* RTAPI_PROC_H */
