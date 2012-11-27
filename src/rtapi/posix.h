/********************************************************************
* Description:  posix.h
*               This file defines the differences specific to the
*               the POSIX thread system
********************************************************************/

#define REV_CODE 4

#include <pth.h>		/* pth_uctx_* */

/***********************************************************************
*                           TASK FUNCTIONS                             *
************************************************************************/
#ifdef RTAPI_TASK
/* Task functions. */

// task_array special entries
typedef struct {
    RT_TASK ostask;
} task_data_extra;
#define TASK_DATA_EXTRA task_data_extra;

// task_array mutex functions; must define, even if empty
#define RTAPI_TASK_ARRAY_LOCK()
#define RTAPI_TASK_ARRAY_UNLOCK()

// The RTAPI_TASK_NEW_HOOK allocates memory for the thread's stack
#define RTAPI_TASK_NEW_HOOK

#endif  /* RTAPI_TASK  */
