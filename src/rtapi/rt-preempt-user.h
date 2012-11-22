/********************************************************************
* Description:  rt-preempt-user.h
*               This file defines the differences specific to the
*               the RT_PREEMPT thread system
*
*		It should be included in rtapi_common.h
********************************************************************/

#define REV_CODE 5	// see rtapi_common.h


/***********************************************************************
*                           TASK FUNCTIONS                             *
************************************************************************/

#include <sched.h>		// sched_get_priority_*()
#include <pthread.h>		// pthread_*

// extra task data accessible as task->tdata
typedef struct {
    int deleted;
    int destroyed;
    int deadline_scheduling;
    struct timespec next_time;

    /* The realtime thread. */
    pthread_t thread;
    pthread_barrier_t thread_init_barrier;
    void *stackaddr;

    /* Statistics */
    unsigned long minfault_base;
    unsigned long majfault_base;
    unsigned int failures;
} thread_task_data;
#define THREAD_TASK_DATA thread_task_data tdata;

// extra module data
#define THREAD_MODULE_DATA int magic;


/* rtapi_prio_*() macros */
#define PRIO_LOWEST sched_get_priority_min(SCHED_FIFO)
#define PRIO_HIGHEST sched_get_priority_max(SCHED_FIFO)

/* hooks defined for this thread system */
#define HAVE_RTAPI_TASK_NEW_HOOK
#define HAVE_RTAPI_TASK_DELETE_HOOK
#define HAVE_RTAPI_TASK_START_HOOK
#define HAVE_RTAPI_TASK_STOP_HOOK
#define HAVE_RTAPI_WAIT_HOOK

/* mutex functions */
#define HAVE_RTAPI_TASK_ARRAY_LOCK
#define HAVE_RTAPI_TASK_ARRAY_UNLOCK
#define HAVE_RTAPI_SHMEM_ARRAY_LOCK
#define HAVE_RTAPI_SHMEM_ARRAY_UNLOCK

/* misc */
#define HAVE_RTAPI_TASK_FREE
#define HAVE_DROP_RESTORE_PRIVS
