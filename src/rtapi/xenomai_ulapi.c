/** RTAPI is a library providing a uniform API for several real time
    operating systems.  As of ver 2.0, RTLinux and RTAI are supported.
*/
/********************************************************************
* Description:  rtai_ulapi.c
*               This file, 'rtai_ulapi.c', implements the nonrealtime 
*               portion of the API for the RTAI platform.
*
* Author: John Kasunich, Paul Corner
* License: LGPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

/** This file, 'rtai_ulapi.c', implements the non-realtime portion of
    the API for the RTAI platform.  The API is defined in rtapi.h,
    which includes documentation for all the API functions.  The
    realtime portion of the API is implemented in rtai_rtapi.c
    (for the RTAI platform).
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
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU General Lesser Public
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

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"

#include <stdio.h>		/* sprintf() */
#include <string.h>		/* strcpy, etc. */
#include <stddef.h>		/* NULL, needed for rtai_shm.h */
#include <stdarg.h>		/* va_arg, etc. */
#include <unistd.h>		/* open(), close() */
#include <sys/mman.h>		/* PROT_READ, needed for rtai_shm.h */
#include <sys/types.h>		/* off_t, needed for rtai_shm.h */
#include <sys/fcntl.h>		/* O_RDWR, needed for rtai_shm.h */

#include <native/heap.h>
#include <native/task.h>
#include "xenomai_common.h"

#include <malloc.h>		/* malloc(), free() */
#include <sys/io.h>		/* inb(), outb() */
#include <errno.h>		/* errno */


#include "rtapi.h"		/* public RTAPI decls */
#include "rtapi_common.h"	/* shared realtime/nonrealtime stuff */

/* the following are internal functions that do the real work associated
   with deleting resources.  They do not check the mutex that protects
   the internal data structures.  When someone calls a rtapi_xxx_delete()
   function, the ulapi funct gets the mutex before calling one of these
   internal functions.  When internal code that already has the mutex
   needs to delete something, it calls these functions directly.
*/

/* resource data unique to this process */

static void *shmem_addr_array[RTAPI_MAX_SHMEMS + 1];
static RT_HEAP shmem_heap_array[RTAPI_MAX_SHMEMS + 1];        

static int shmem_delete(int shmem_id, int module_id);

static int msg_level = RTAPI_MSG_ERR;	/* message printing level */

static void check_memlock_limit(const char *where);

RT_HEAP ul_heap_desc;

/***********************************************************************
*                      GENERAL PURPOSE FUNCTIONS                       *
************************************************************************/

int rtapi_init(const char *modname)
{
    int n, module_id;
    module_data *module;

    /* say hello */
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: initing module %s\n", modname);
    /* get shared memory block from OS and save its address */
    errno = 0;

    if ((n = rt_heap_bind(&ul_heap_desc, MASTER_HEAP, TM_NONBLOCK))) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: rtapi_init: rt_heap_bind() returns %d - %s\n", 
			n, strerror(-n));
	return -EINVAL;
    }
    if ((n = rt_heap_alloc(&ul_heap_desc, 0, TM_NONBLOCK, (void **)&rtapi_data)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: rt_heap_alloc() returns %d - %s\n", 
			n, strerror(n));
	return -EINVAL;
    }

    //rtapi_printall();

    /* perform a global init if needed */
    init_rtapi_data(rtapi_data);
    /* check revision code */
    if (rtapi_data->rev_code != REV_CODE) {
	/* mismatch - release master shared memory block */
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: version mismatch %d vs %d\n", rtapi_data->rev_code, REV_CODE);
	return -EINVAL;
    }
    /* set up local pointers to global data */
    module_array = rtapi_data->module_array;
    task_array = rtapi_data->task_array;
    shmem_array = rtapi_data->shmem_array;
     /* perform local init */
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	shmem_addr_array[n] = NULL;
    }

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
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: reached module limit %d\n",
	    n);
	return -EMFILE;
    }
    /* we have space for the module */
    module_id = n;
    module = &(module_array[n]);
    /* update module data */
    module->state = USERSPACE;
    if (modname != NULL) {
	/* use name supplied by caller, truncating if needed */
	snprintf(module->name, RTAPI_NAME_LEN, "%s", modname);
    } else {
	/* make up a name */
	snprintf(module->name, RTAPI_NAME_LEN, "ULMOD%03d", module_id);
    }
    rtapi_data->ul_module_count++;
    rtapi_mutex_give(&(rtapi_data->mutex));
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module '%s' inited, ID = %02d\n",
	module->name, module_id);
    return module_id;
}

int rtapi_exit(int module_id)
{
    module_data *module;
    int n;

    if (rtapi_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: exit called before init\n");
	return -EINVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module %02d exiting\n", module_id);
    /* validate module ID */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: bad module id\n");
	return -EINVAL;
    }
    /* get mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* point to the module's data */
    module = &(module_array[module_id]);
    /* check module status */
    if (module->state != USERSPACE) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: not a userspace module\n");
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* clean up any mess left behind by the module */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (test_bit(module_id, shmem_array[n].bitmap)) {
	    fprintf(stderr,
		"ULAPI: WARNING: module '%s' failed to delete shmem %02d\n",
		module->name, n);
	    shmem_delete(n, module_id);
	}
    }
    /* update module data */
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module %02d exited, name = '%s'\n",
	module_id, module->name);
    module->state = NO_MODULE;
    module->name[0] = '\0';
    rtapi_data->ul_module_count--;
    rtapi_mutex_give(&(rtapi_data->mutex));
    return 0;
}

int rtapi_vsnprintf(char *buf, unsigned long int size, const char *fmt, va_list ap) {
    return vsnprintf(buf, size, fmt, ap);
}

int rtapi_snprintf(char *buf, unsigned long int size, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    /* call the normal library vnsprintf() */
    i = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return i;
}

#define RTPRINTBUFFERLEN 1024

void rtapi_print(const char *fmt, ...)
{
    char buffer[RTPRINTBUFFERLEN + 1];
    va_list args;

    va_start(args, fmt);
    /* call the normal library vnsprintf() */
    vsnprintf(buffer, RTPRINTBUFFERLEN, fmt, args);
    fputs(buffer, stdout);
    va_end(args);
}

void rtapi_print_msg(int level, const char *fmt, ...)
{
    va_list args;

    if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
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

void rtapi_printall(void)
{
    module_data *modules;
    task_data *tasks;
    shmem_data *shmems;
    int n, m;

    if (rtapi_data == NULL) {
	printf("rtapi_data = NULL, not initialized\n");
	return;
    }
    printf("rtapi_data = %p\n", rtapi_data);
    printf("  magic = %d\n", rtapi_data->magic);
    printf("  rev_code = %08x\n", rtapi_data->rev_code);
    printf("  mutex = %lu\n", rtapi_data->mutex);
    printf("  rt_module_count = %d\n", rtapi_data->rt_module_count);
    printf("  ul_module_count = %d\n", rtapi_data->ul_module_count);
    printf("  task_count  = %d\n", rtapi_data->task_count);
    printf("  shmem_count = %d\n", rtapi_data->shmem_count);
    printf("  timer_running = %d\n", rtapi_data->timer_running);
    printf("  timer_period  = %ld\n", rtapi_data->timer_period);
    modules = &(rtapi_data->module_array[0]);
    tasks = &(rtapi_data->task_array[0]);
    shmems = &(rtapi_data->shmem_array[0]);
       printf("  module array = %p\n", modules);
    printf("  task array   = %p\n", tasks);
    printf("  shmem array  = %p\n", shmems);
    for (n = 0; n <= RTAPI_MAX_MODULES; n++) {
	if (modules[n].state != NO_MODULE) {
	    printf("  module %02d\n", n);
	    printf("    state = %d\n", modules[n].state);
	    printf("    name = %p\n", modules[n].name);
	    printf("    name = '%s'\n", modules[n].name);
	}
    }
    for (n = 0; n <= RTAPI_MAX_TASKS; n++) {
	if (tasks[n].state != EMPTY) {
	    printf("  task %02d\n", n);
	    printf("    state = %d\n", tasks[n].state);
	    printf("    prio  = %d\n", tasks[n].prio);
	    printf("    owner = %d\n", tasks[n].owner);
	    printf("    code  = %p\n", tasks[n].taskcode);
	}
    }
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmems[n].key != 0) {
	    printf("  shmem %02d\n", n);
	    printf("    key     = %d\n", shmems[n].key);
	    printf("    rtusers = %d\n", shmems[n].rtusers);
	    printf("    ulusers = %d\n", shmems[n].ulusers);
	    printf("    size    = %ld\n", shmems[n].size);
	    printf("    bitmap  = ");
	    for (m = 0; m <= RTAPI_MAX_MODULES; m++) {
		if (test_bit(m, shmems[n].bitmap)) {
		    putchar('1');
		} else {
		    putchar('0');
		}
	    }
	    putchar('\n');
	}
    }
 }

/***********************************************************************
*                  SHARED MEMORY RELATED FUNCTIONS                     *
************************************************************************/

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
    int n,retval;
    int shmem_id;
    shmem_data *shmem;
    char shm_name[20];

    /* key must be non-zero, and also cannot match the key that RTAPI uses */
    if ((key == 0) || (key == RTAPI_KEY)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: bad shmem key: %d\n",
	    key);
	return -EINVAL;
    }
    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: bad module ID: %d\n",
	    module_id);
	return -EINVAL;
    }
    if (module_array[module_id].state != USERSPACE) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: not a user space module ID: %d\n", module_id);
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
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "RTAPI: ERROR: shmem size mismatch\n");
		return -EINVAL;
	    }
	    /* is this module already using it? */
	    if (test_bit(module_id, shmem->bitmap)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		rtapi_print_msg(RTAPI_MSG_WARN,
		    "RTAPI: Warning: shmem already mapped\n");
		return -EINVAL;
	    }
	    /* no, map it */

	    snprintf(shm_name, sizeof(shm_name), "shm-%d", shmem_id);

	    if (shmem_addr_array[shmem_id] == NULL) {
		if ((retval = rt_heap_bind(&shmem_heap_array[n], shm_name, TM_NONBLOCK))) {
		    rtapi_print_msg(RTAPI_MSG_ERR, 
				    "ULAPI: ERROR: rtapi_shmem_new: rt_heap_bind(%s) returns %d - %s\n", 
				    shm_name, retval, strerror(-retval));
		    return -EINVAL;
		}
		if ((retval = rt_heap_alloc(&shmem_heap_array[n], 0, TM_NONBLOCK, 
					    &shmem_addr_array[shmem_id])) != 0) {
		    rtapi_print_msg(RTAPI_MSG_ERR, 
				    "RTAPI: ERROR: rt_heap_alloc() returns %d - %s\n", 
				    retval, strerror(retval));
		    return -EINVAL;
		}
	    } else {
		rtapi_print_msg(RTAPI_MSG_DBG,"ulapi %s already mapped \n",shm_name);
	    }
            // the check for -1 here is because rtai_malloc (in at least
            // rtai 3.6.1, and probably others) has a bug where it
            // sometimes returns -1 on error
            if (shmem_addr_array[shmem_id] == NULL || shmem_addr_array[shmem_id] == (void*)-1) {
		/* map failed */
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "RTAPI: ERROR: failed to map shmem\n");
		rtapi_mutex_give(&(rtapi_data->mutex));
		check_memlock_limit("failed to map shmem");
		return -ENOMEM;
	    }
	    /* update usage data */
	    set_bit(module_id, shmem->bitmap);
	    shmem->ulusers++;
	    /* done */
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return shmem_id;
	}
    }
    /* find empty spot in shmem array */
    n = 1;
    while ((n <= RTAPI_MAX_SHMEMS) && (shmem_array[n].key != 0)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "ULAPI: shmem %d occupuied \n",n);

	n++;
    }
    if (n > RTAPI_MAX_SHMEMS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: reached shmem limit %d\n",
	    n);
	return -EMFILE;
    }
    /* we have space for the block data */
	rtapi_print_msg(RTAPI_MSG_ERR, "ULAPI: using new shmem %d  \n",n);

    shmem_id = n;
    shmem = &(shmem_array[n]);
    /* now get shared memory block from OS and save its address */

    if (shmem_addr_array[shmem_id] == NULL) {
	snprintf(shm_name, sizeof(shm_name), "shm-%d", shmem_id);
	if ((retval = rt_heap_create(&shmem_heap_array[n], shm_name, size, H_SHARED))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "RTAPI: ERROR: rtapi_shmem_new: rt_heap_create(%s,%ld) returns %d\n", 
			    shm_name, size, retval);
	    return -EINVAL;
	}
	if ((retval = rt_heap_alloc(&shmem_heap_array[n], 0, TM_NONBLOCK, 
				    &shmem_addr_array[shmem_id])) != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "RTAPI: ERROR: rt_heap_alloc() returns %d - %s\n", 
			    retval, strerror(retval));
	    return -EINVAL;
	}
    } else {
	rtapi_print_msg(RTAPI_MSG_DBG, "ULAPI: %s already mapped\n", shm_name);
    }
    
    // the check for -1 here is because rtai_malloc (in at least
    // rtai 3.6.1, and probably others) has a bug where it
    // sometimes returns -1 on error
    if (shmem_addr_array[shmem_id] == NULL || shmem_addr_array[shmem_id] == (void*)-1) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: could not create shmem %d\n", n);
	return -ENOMEM;
    }
    /* the block has been created, update data */
    set_bit(module_id, shmem->bitmap);
    shmem->key = key;
    shmem->rtusers = 0;
    shmem->ulusers = 1;
    shmem->size = size;
    rtapi_data->shmem_count++;
    /* zero the first word of the shmem area */
    *((long int *) (shmem_addr_array[shmem_id])) = 0;
    /* done */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return shmem_id;
}

#include <sys/time.h>
#include <sys/resource.h>
#define RECOMMENDED (20480*1024lu)
static void check_memlock_limit(const char *where) {
    static int checked=0;
    struct rlimit lim;
    int result;
    if(checked) return;
    checked=1;

    result = getrlimit(RLIMIT_MEMLOCK, &lim);
    if(result < 0) { perror("getrlimit"); return; }
    if(lim.rlim_cur == (rlim_t)-1) return; // unlimited
    if(lim.rlim_cur >= RECOMMENDED) return; // limit is at least recommended
    rtapi_print_msg(RTAPI_MSG_ERR,
        "RTAPI: Locked memory limit is %luKiB, recommended at least %luKiB.\n"
        "This can cause the error '%s'.\n"
        "For more information, see\n"
        "\thttp://wiki.linuxcnc.org/cgi-bin/emcinfo.pl?LockedMemory\n",
        (unsigned long)lim.rlim_cur/1024, RECOMMENDED/1024, where);
    return;
}

int rtapi_shmem_delete(int shmem_id, int module_id)
{
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = shmem_delete(shmem_id, module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

int shmem_delete(int shmem_id, int module_id)
{
    shmem_data *shmem;

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
    if (module_array[module_id].state != USERSPACE) {
	return -EINVAL;
    }
    /* is this module using the block? */
    if (test_bit(module_id, shmem->bitmap) == 0) {
	return -EINVAL;
    }
    /* OK, we're no longer using it */
    clear_bit(module_id, shmem->bitmap);
    shmem->ulusers--;
    /* unmap the block */


    shmem_addr_array[shmem_id] = NULL;
    /* is somebody else still using the block? */
    if ((shmem->ulusers > 0) || (shmem->rtusers > 0)) {
	/* yes, we're done for now */
	return 0;
    }

    rt_heap_delete(&shmem_heap_array[shmem_id]);

    /* update the data array and usage count */
    shmem->key = 0;
    shmem->size = 0;
    rtapi_data->shmem_count--;
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
