/********************************************************************
* Description:  xenomai-user.h
*               This file defines the differences specific to the
*               the Xenomai user land thread system
********************************************************************/

//FIXME minimize
#include <native/task.h>        /* Xenomai task */
#include <native/timer.h>
#include <native/mutex.h>
#include <rtdk.h>
#include <nucleus/types.h>     /* for XNOBJECT_NAME_LEN */

#include <sys/io.h>             /* inb, outb */

/* this needs to be fixed in rtapi_common.h
#undefine RTAPI_NAME_LEN
#define RTAPI_NAME_LEN XNOBJECT_NAME_LEN
*/
#define THREAD_TASK_DATA RT_TASK *self;

/* Priority functions settings */

#include <sched.h>		/* sched_get_priority_*() */

// Xenomai rt_task priorities are 0: lowest .. 99: highest
#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99

#define HAVE_RTAPI_OUTB_HOOK
#define HAVE_RTAPI_INB_HOOK

#endif  /* RTAPI_TASK  */
