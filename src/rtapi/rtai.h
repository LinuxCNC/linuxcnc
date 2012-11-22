/********************************************************************
* Description:  rtai.h
*               This file defines the differences specific to the
*               the RTAI thread system
********************************************************************/


/***********************************************************************
*                        MESSAGING FUNCTIONS                           *
************************************************************************/
#ifdef RTAPI_MSG
/* Messaging functions settings */

// RTAI uses rt_printk() instead of printk()
#define PRINTK rt_printk

#endif  /* RTAPI_MSG */


/***********************************************************************
*                           TASK FUNCTIONS                             *
************************************************************************/
#ifdef RTAPI_TASK
/* Priority functions settings */

/* RTAI uses 0 as the highest priority; higher numbers are lower
   priority */
#define INVERSE_PRIO
#define PRIO_LOWEST 0xFFF
#define PRIO_HIGHEST 0

#endif  /* RTAPI_TASK  */
