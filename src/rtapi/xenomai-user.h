/********************************************************************
* Description:  xenomai-user.h
*               This file defines the differences specific to the
*               the Xenomai user land thread system
********************************************************************/

/* this needs to be fixed in rtapi_common.h
#undefine RTAPI_NAME_LEN
#define RTAPI_NAME_LEN XNOBJECT_NAME_LEN
*/
#define THREAD_TASK_DATA RT_TASK *self;

// Xenomai rt_task priorities are 0: lowest .. 99: highest
#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99

#define HAVE_RTAPI_OUTB_HOOK
#define HAVE_RTAPI_INB_HOOK


/* additional struct stored in rtapi_data->tdata... */
typedef struct {
    int rt_wait_error;		/* release point missed */
    int rt_last_overrun;	/* last number of overruns reported by
				   Xenomai */
    int rt_total_overruns;	/* total number of overruns reported
				   by Xenomai */
} thread_rtapi_data;
#define THREAD_RTAPI_DATA thread_rtapi_data tdata;

/* ...and a hook to initialize it */
#define HAVE_INIT_RTAPI_DATA_HOOK
