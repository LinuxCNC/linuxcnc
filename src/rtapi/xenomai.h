/********************************************************************
* Description:  xenomai.h
*               This file defines the differences specific to the
*               the Xenomai user land thread system
********************************************************************/

/* rtapi_common.c */
// Init rt_stats for RTAPI
#ifdef RTAPI
#define HAVE_INIT_RTAPI_DATA_HOOK
#endif

/* rtapi_proc */
#ifdef RTAPI
#define HAVE_RTAPI_READ_STATUS_HOOK
#endif


/* rtapi_task.c */
// Xenomai rt_task priorities are 0: lowest .. 99: highest
#define PRIO_LOWEST 0
#define PRIO_HIGHEST 99

#define HAVE_RTAPI_TASK_DELETE_HOOK
#define HAVE_RTAPI_TASK_STOP_HOOK
#define HAVE_RTAPI_TASK_PAUSE_HOOK
#define HAVE_RTAPI_WAIT_HOOK
#define HAVE_RTAPI_TASK_RESUME_HOOK
#define HAVE_RTAPI_TASK_SELF_HOOK


/* rtapi_time.c */
#define RTAPI_TIME_NO_CLOCK_MONOTONIC  // Xenomai has its own time features
#define HAVE_RTAPI_GET_TIME_HOOK
#define HAVE_RTAPI_GET_CLOCKS_HOOK
