/********************************************************************
* Description:  sim_rtapi.c
*               This file, 'sim_rtapi.c', implements the RT API 
*               functions for machines without RT (simultated procs)
*
* Author: John Kasunich, Paul Corner
* License: GPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#include <stdio.h>		/* vprintf() */
#include <stdlib.h>		/* malloc(), sizeof() */
#include <stdarg.h>		/* va_* */
#include "config.h"

#if defined(RTAPI_XENOMAI_USER)
#include <native/task.h>        /* Xenomai task */
#include <native/timer.h>
#include <native/mutex.h>
#include <rtdk.h>
#include <nucleus/types.h>     /* for XNOBJECT_NAME_LEN */

#include <sys/io.h>             /* inb, outb */
#endif
#if defined(RTAPI_POSIX)
#include <pth.h>		/* pth_uctx_* */
#endif
#include <unistd.h>		/* usleep() */
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <time.h>               /* gettimeofday */
#include <sys/time.h>           /* gettimeofday */
#include "rtapi.h"		/* these decls */
#include <errno.h>
#include <string.h>

/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

struct rtapi_module {
  int magic;
};

struct rtapi_task {
  int magic;			/* to check for valid handle */
  int owner;
#if defined(RTAPI_XENOMAI_USER)
  RT_TASK ctx;		/* Xenomai task */
  char name[XNOBJECT_NAME_LEN];
#else
  char name[RTAPI_NAME_LEN];
#endif
#if defined(RTAPI_POSIX)
  pth_uctx_t ctx;		/* thread's context */
#endif
  int uses_fp;
  size_t stacksize;
  int prio;
  int period;
  int ratio;
  void *arg;
  void (*taskcode) (void*);	/* pointer to task function */
};

static struct timeval schedule;
#if !defined(RTAPI_XENOMAI_USER)
static int base_periods;
static pth_uctx_t main_ctx, this_ctx;
#endif

#define MODULE_MAGIC  30812
#define TASK_MAGIC    21979	/* random numbers used as signatures */
#define SHMEM_MAGIC   25453

#define MAX_TASKS  64
#define MAX_MODULES  64
#define MODULE_OFFSET 32768

/* data for all tasks */
static struct rtapi_task task_array[MAX_TASKS] = {{0},};

#if defined(RTAPI_XENOMAI_USER)
//FIXME do this once for xenomai (see xenomai_*c)

// Xenomai rt_task priorities are 0: lowest .. 99: highest
int rtapi_prio_highest(void)
{
  return 99;
}

int rtapi_prio_lowest(void)
{
  return 0;
}

int rtapi_prio_next_higher(int prio)
{
  /* return a valid priority for out of range arg */
  if (prio >= rtapi_prio_highest())
    return rtapi_prio_highest();
  if (prio <= rtapi_prio_lowest())
    return rtapi_prio_lowest();

  /* return next higher priority for in-range arg */
  return prio + 1;
}

int rtapi_prio_next_lower(int prio)
{
  /* return a valid priority for out of range arg */
  if (prio <= rtapi_prio_lowest())
    return rtapi_prio_lowest();
  if (prio >= rtapi_prio_highest())
    return rtapi_prio_highest();
  /* return next lower priority for in-range arg */
  return prio - 1;
}
#else
/* Priority functions.  SIM uses 0 as the highest priority, as the
number increases, the actual priority of the task decreases. */

int rtapi_prio_highest(void)
{
  return 0;
}

int rtapi_prio_lowest(void)
{
  return 31;
}

int rtapi_prio_next_higher(int prio)
{
  /* return a valid priority for out of range arg */
  if (prio <= rtapi_prio_highest())
    return rtapi_prio_highest();
  if (prio > rtapi_prio_lowest())
    return rtapi_prio_lowest();

  /* return next higher priority for in-range arg */
  return prio - 1;
}

int rtapi_prio_next_lower(int prio)
{
  /* return a valid priority for out of range arg */
  if (prio >= rtapi_prio_lowest())
    return rtapi_prio_lowest();
  if (prio < rtapi_prio_highest())
    return rtapi_prio_highest();
  /* return next lower priority for in-range arg */
  return prio + 1;
}
#endif

static int period = 0;
int rtapi_clock_set_period(unsigned long int nsecs)
{
  if(nsecs == 0) return period;
  if(period != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "attempt to set period twice\n");
      return -EINVAL;
  }
  period = nsecs;
  gettimeofday(&schedule, NULL);
  return period;
}


int rtapi_task_new(void (*taskcode) (void*), void *arg,
		   int prio, int owner, unsigned long int stacksize, 
		   int uses_fp, char *name) 
{
  int n;
  int task_id;
#if defined(RTAPI_XENOMAI_USER)
  int retval;
  int cpu_num = 0; // FIXME make this use csets eventually
#endif
  struct rtapi_task *task;

  /* find an empty entry in the task array */
  /*! \todo  FIXME - this is not 100% thread safe.  If another thread
     calls this function after the first thread breaks out of
     the loop but before it sets the magic number, two tasks
     might wind up assigned to the same structure.  Need an
     atomic test and set for the magic number.  Not tonight! */
  n = 0;
  while ((n < MAX_TASKS) && (task_array[n].magic == TASK_MAGIC))
    n++;
  if (n == MAX_TASKS)
    return -ENOMEM;
  task_id = n;
  task = &(task_array[n]);

  /* check requested priority */
#if defined(RTAPI_XENOMAI_USER)
  if ((prio < rtapi_prio_lowest()) || (prio > rtapi_prio_highest()))
    return -EINVAL;
#endif
#if defined(RTAPI_POSIX)
  if ((prio < rtapi_prio_highest()) || (prio > rtapi_prio_lowest()))
    return -EINVAL;
#endif

  /* label as a valid task structure */
  /*! \todo FIXME - end of non-threadsafe window */
  if(stacksize < 16384) stacksize = 16384;
  task->magic = TASK_MAGIC;
  task->owner = owner;
#if defined(RTAPI_POSIX)
  task->ctx = NULL;
#endif
  task->arg = arg;
  task->stacksize = stacksize;
  task->taskcode = taskcode;
  task->prio = prio;
  task->uses_fp = uses_fp;
  strncpy(task->name, name, sizeof(task->name));
  task->name[sizeof(task->name) - 1]= '\0';

#if defined(RTAPI_XENOMAI_USER)
  rtapi_print_msg(RTAPI_MSG_DBG, "rt_task_create %d \"%s\"\n", task_id, task->name );

  // http://www.xenomai.org/documentation/trunk/html/api/group__task.html#ga03387550693c21d0223f739570ccd992
  // Passing T_FPU|T_CPU(1) in the mode parameter thus creates a 
  // task with FPU support enabled and which will be affine to CPU #1
  // the task will start out dormant; execution begins with rt_task_start()

  // not sure T_CPU(1) is possible - see:
  // http://www.xenomai.org/pipermail/xenomai-help/2010-09/msg00081.html

  // since this is a usermode RT task, it will be FP anyway
  if ((retval = rt_task_create (&task->ctx, task->name, task->stacksize, task->prio, 
				(uses_fp ? T_FPU : 0) | T_CPU(cpu_num))) != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "rt_task_create failed, rc = %d\n", retval );
      return -ENOMEM;
  }
#endif
  /* and return handle to the caller */

  return n;
}


int rtapi_task_delete(int id) {
  struct rtapi_task *task;
#if defined(RTAPI_XENOMAI_USER)
  int retval;
#endif

  if(id < 0 || id >= MAX_TASKS) return -EINVAL;

  task = &(task_array[id]);
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

  rtapi_print_msg(RTAPI_MSG_DBG, "rt_task_delete %d \"%s\"\n", id, task->name );

#if defined(RTAPI_XENOMAI_USER)
  if ((retval = rt_task_delete( &task->ctx )) < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,"ERROR: rt_task_delete() = %d %s\n", retval, strerror(retval));
      return retval;
  }
#endif
#if defined(RTAPI_POSIX)
  pth_uctx_destroy(task->ctx);
#endif
  task->magic = 0;
  return 0;
}

#if !defined(RTAPI_XENOMAI_USER)
static void wrapper(void *arg)
{
  struct rtapi_task *task;
  int retval;

  /* use the argument to point to the task data */
  task = (struct rtapi_task*)arg;
  if(task->period < period) task->period = period;
  task->ratio = task->period / period;
  rtapi_print_msg(RTAPI_MSG_DBG, "task %p '%s' period=%d prio=%d ratio=%d\n",
		  task, task->name, task->prio, task->period, task->ratio);

  /* call the task function with the task argument */
  (task->taskcode) (task->arg);

  rtapi_print_msg(RTAPI_MSG_ERR,"ERROR: reached end of wrapper for task %d\n", (int)(task - task_array));
}
#endif

int rtapi_task_start(int task_id, unsigned long int period_nsec)
{
  struct rtapi_task *task;
  int retval;

  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];

  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

  if(period_nsec < period) period_nsec = period;
  task->period = period_nsec;
  task->ratio = period_nsec / period;

  /* create the thread - use the wrapper function, pass it a pointer
     to the task structure so it can call the actual task function */

#if defined(RTAPI_XENOMAI_USER)
  if ((retval = rt_task_set_periodic( &task->ctx, TM_NOW, task->period)) != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: rt_task_set_periodic() task_id %d periodns=%ld returns %d\n", 
		      task_id, period_nsec, retval);
      return -EINVAL;
  }
  retval = rt_task_start( &task->ctx, task->taskcode, (void*)task->arg);
  if( retval )
  {
	rtapi_print_msg(RTAPI_MSG_INFO, "rt_task_start failed, rc = %d\n", retval );
        return -ENOMEM;
  }
#endif
#if defined(RTAPI_POSIX)
  retval = pth_uctx_create(&task->ctx);
  if (retval == FALSE)
    return -ENOMEM;
  retval = pth_uctx_make(task->ctx, NULL, task->stacksize, NULL,
	  wrapper, (void*)task, 0);
  if (retval == FALSE)
    return -ENOMEM;
#endif

  return 0;
}


int rtapi_task_stop(int task_id)
{
  struct rtapi_task *task;
#if defined(RTAPI_XENOMAI_USER)
  int retval;
#endif

  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];

  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

#if defined(RTAPI_XENOMAI_USER)
 if ((retval = rt_task_delete( &task->ctx )) < 0) {
     rtapi_print_msg(RTAPI_MSG_ERR,"rt_task_delete() = %d\n", retval);
     return retval;
 }
#endif
#if defined(RTAPI_POSIX)
  pth_uctx_destroy(task->ctx);
#endif
  return 0;
}

int rtapi_task_pause(int task_id)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

#if defined(RTAPI_XENOMAI_USER)
  return rt_task_suspend( &task->ctx );
#else
  return -ENOSYS;
#endif
}

int rtapi_task_resume(int task_id)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

#if defined(RTAPI_XENOMAI_USER)
  return rt_task_resume( &task->ctx );
#else
  return -ENOSYS;
#endif
}

#if 0
int rtapi_task_set_period(int task_id,
			  unsigned long int period_nsec)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

  task->period = period_nsec;

#if defined(RTAPI_XENOMAI_USER)
  return rt_task_set_periodic(&task->ctx, TM_NOW, task->period * task->ratio );
#else
  return 0;
#endif
}
#endif 

int rtapi_wait(void)
{
#if defined(RTAPI_XENOMAI_USER)
  return rt_task_wait_period(NULL);
#endif
#if defined(RTAPI_POSIX)
  pth_uctx_switch(this_ctx, main_ctx);
#endif
  return 0;
}


void rtapi_outb(unsigned char byte, unsigned int port)
{
#if defined(RTAPI_XENOMAI_USER)
  outb(byte, port);
#else
  return;
#endif
}

unsigned char rtapi_inb(unsigned int port)
{
#if defined(RTAPI_XENOMAI_USER)
  return inb(port);
#else
  return 0;
#endif
}

long int simple_strtol(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}

#define MIN_RUNS 13

#if ! defined(RTAPI_XENOMAI_USER)
static int maybe_sleep(int fd) {
    struct timeval now;
    struct timeval interval;

    if(period == 0) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	return select(fd+1, &fds, NULL, NULL, NULL);
    } else {
	schedule.tv_usec += period / 1000;
	if(schedule.tv_usec > 1000000) {
	    schedule.tv_usec -= 1000000;
	    schedule.tv_sec ++;
	}

	if(period < 100000) {
	    // if base_period is fast (<.1ms) then run 10 times (e.g., enough
	    // for .5ms if base_period is 50uS) without any syscalls
	    if(base_periods % MIN_RUNS) return 0;
	}
	gettimeofday(&now, NULL);
	interval.tv_sec = schedule.tv_sec - now.tv_sec;
	interval.tv_usec = schedule.tv_usec - now.tv_usec;

	if(interval.tv_usec < 0) {
	    interval.tv_sec --;
	    interval.tv_usec += 1000000;
	}

	if(interval.tv_sec < -10) {
	    // Something happened, like getting stopped in the debugger
	    // for a long time.  Instead of playing catch-up, just forget
	    // about it
	    rtapi_print_msg(RTAPI_MSG_DBG, "Long pause, resetting schedule\n");
	    memcpy(&schedule, &now, sizeof(struct timeval));
	}
	if(interval.tv_sec > 0
		|| (interval.tv_sec == 0 &&  interval.tv_usec >= 0)) {
	    fd_set fds;
	    FD_ZERO(&fds);
	    FD_SET(fd, &fds);

	    return select(fd+1, &fds, NULL, NULL, &interval);
	}
    }
    return 0;
}


int sim_rtapi_run_threads(int fd) {
    static int first_time = 1;
    if(first_time) {
	int result = pth_uctx_create(&main_ctx);
	if(result == FALSE) _exit(1);
	first_time = 0;	
    }
    while(1) {
	int result = maybe_sleep(fd);
	if(result) {
	    return result;
	}

	if(period) {
	    int t;
	    base_periods++;
	    for(t=0; t<MAX_TASKS; t++) {
		struct rtapi_task *task = &task_array[t];
		if(task->magic == TASK_MAGIC && task->ctx && 
			(base_periods % task->ratio == 0)) {
		    this_ctx = task->ctx;
		    if(pth_uctx_switch(main_ctx, task->ctx) == FALSE) _exit(1);
		}
	    }
	}
    }
}
#endif

#if defined(RTAPI_XENOMAI_USER)
long long rtapi_get_time(void)
{
    RTIME now = rt_timer_read();
    return (long long) now;
}

long long rtapi_get_clocks(void)
{
 return (long long) rt_timer_tsc();
}
#endif

#include "rtapi/sim_common.h"
