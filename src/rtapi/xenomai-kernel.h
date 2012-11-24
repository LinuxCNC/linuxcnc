/********************************************************************
* Description:  xenomai-kernel.h
*               This file defines the differences specific to the
*               the Xenomai kernel thread system
********************************************************************/

/* Priority functions settings */

#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99

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
