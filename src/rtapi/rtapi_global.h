#ifndef _RTAPI_GLOBAL_H
#define _RTAPI_GLOBAL_H
/***********************************************************************
*                            GLOBAL SEGMENET                           *
************************************************************************/
/** the global_data structure resides in a shared memory segment and is
    unversally accessible to all entities with the HAL/RTAPI universe,
    including (but not limited to):

    The realtime RTAPI - thread, module, shm support - regardless of
    threadstyle (both kernel threads and userland threads have uniform
    access)

    The realtime ULAPI - the support code used by RT components

    The userland HAL and RT support API's - arbitrary user processes
    like halcmd, or userland HAL drivers and components

    The global_data structure carries the following types of information:

    Session parameters - e.g. the desired threadstyle since the build now supports
    runtime choice of threadstyle - for instance a non-RT (formerly 'sim',
    nowadays 'posix threads') session, or a an RT session as supported
    by the currently running kernel

    Dynamic sizing values, e.g. the HAL shm segment size. These values 
    change from 'compiled in' to 'startup parameter'.

    Data which should be shared within a session, for instance, the
    RTAPI message level. This was not possible so far and fairly confusing.

    Support data:

    a global counter for the next module ID (next_handle) which is
    needed by userland threadstyles since they do not arrays for modules.
    (this was formerly Jeff Epler's 'uuid' mechanism). next_handle can
    also be used to generated integer ID's unique throughout an RTAPI
    instance.

    Other possible uses of global_data include, but are not limited to,
    for instance instance management if one were to support multiple 
    HAL/RTAPI instances within a single machine.
*/

/********************************************************************
* Copyright (C) 2012 - 2013 John Morris <john AT zultron DOT com>
*                           Michael Haberler <license AT mah DOT priv DOT at>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
********************************************************************/

#include "rtapi_shmkeys.h"
#include "rtapi_bitops.h"     // rtapi_atomic_type
#include "rtapi_exception.h"  // thread status descriptors
#include "ring.h"             // ring buffer ops & structures
#include "rtapi_heap.h"       // shared memory allocator
#include "rtapi_heap_private.h"


#define MESSAGE_RING_SIZE (4096 * 128)

// the universally shared global structure
typedef struct {
    unsigned magic;
    int layout_version; 
    unsigned long mutex;

    // this is set once on startup by rtapi_msgd and is to be considered a constant
    // throughout the session:
    int instance_id;
    int rtapi_thread_flavor; 

    // runtime parameters
    int rt_msg_level;              // message level for RT 
    int user_msg_level;            // message level for non-RT 
    rtapi_atomic_type next_handle;               // next unique ID
    int hal_size;                  // make HAL data segment size configurable
    int hal_thread_stack_size;     // stack size passed to rtapi_task_new()
                                   // in hal_create_thread()

    // service uuid - the unique machinekit instance identifier
    // set once by rtapi_msgd, visible to all of HAL and RTAPI since
    // the global segment is attached right at startup
    unsigned char service_uuid[16];

    int rtapi_app_pid;
    int rtapi_msgd_pid;

    // unified thread status monitoring
    rtapi_threadstatus_t thread_status[RTAPI_MAX_TASKS + 1];

    // stats for rtapi_messages
    int error_ring_full;
    int error_ring_locked;

    ringheader_t rtapi_messages;   // ringbuffer for RTAPI messages
    char buf[SIZE_ALIGN(MESSAGE_RING_SIZE)];
    ringtrailer_t rtapi_messages_trailer;

    struct rtapi_heap heap;
    //size_t heap_size;
#define GLOBAL_HEAP_SIZE (512*512)
    unsigned char arena[GLOBAL_HEAP_SIZE] __attribute__((aligned(16)));

} global_data_t;

#define GLOBAL_LAYOUT_VERSION 42   // bump on layout changes of global_data_t

// use global_data->magic to reflect rtapi_msgd state
#define GLOBAL_INITIALIZING  0x0eadbeefU
#define GLOBAL_READY         0x0eadbeadU
#define GLOBAL_EXITED        0x0eadfeefU // trap attach to leftover shm segments

#define GLOBAL_DATA_PERMISSIONS	0666

extern global_data_t *global_data;

#endif // _RTAPI_GLOBAL_H
