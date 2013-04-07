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

    a global counter for the next module ID (next_module_id) which is 
    needed by userland threadstyles since they do not arrays for modules.
    (this was formerly Jeff Epler's 'uuid' mechanism).

    Other possible uses of global_data include, but are not limited to,
    for instance instance management if one were to support multiple 
    HAL/RTAPI instances within a single machine.
*/
#include "rtapi_shmkeys.h"


#define MESSAGE_RING_SIZE 32768

// the universally shared global structure
typedef struct {
    int magic;
    int layout_version; 
    unsigned long mutex;
    int instance_id;
    int rtapi_thread_flavor; 
    int rt_msg_level;              // message level for RT 
    int user_msg_level;            // message level for non-RT 
    int next_module_id;            // for userland threads module id's
    int hal_size;                  // make HAL data segment size configurable
    int rtapi_app_pid;
    int rtapi_msgd_pid;
    int error_ring_full;
    int error_ring_locked;
    ringheader_t rtapi_messages;   // ringbuffer for RTAPI messages
    char buf[SIZE_ALIGN(MESSAGE_RING_SIZE)];
} global_data_t;

#define GLOBAL_LAYOUT_VERSION 42   // bump on layout changes of global_data_t
#define GLOBAL_MAGIC 0xdeadbeef
#define GLOBAL_DATA_PERMISSIONS	0666

extern global_data_t *global_data;

typedef global_data_t *(*get_global_handle_t)(void);

extern global_data_t *get_global_handle(void);

#endif // _RTAPI_GLOBAL_H
