/********************************************************************
* Description:  xenomai-kernel.h
*               This file defines the differences specific to the
*               the Xenomai kernel thread system
********************************************************************/

#define REV_CODE 2

/* add some fields to rtapi_data_t:
   rt_wait_error:		release point missed
   rt_last_overrun:		last number of overruns reported by Xenomai
   rt_total_overruns:		total number of overruns reported by Xenomai */
#define THREAD_RTAPI_DATA	\
    int rt_wait_error;		\
    int rt_last_overrun;	\
    int rt_total_overruns

/* ...and a hook to initialize it */
#define HAVE_INIT_RTAPI_DATA_HOOK


/* Priority functions settings */

#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99


/* task functions */
#define MASTER_HEAP "rtapi-heap"
