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
#include "rtapi_common.h"		/* these decls */
#include <errno.h>
#include <string.h>

/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

#if !defined(RTAPI_XENOMAI_USER)
static struct timeval schedule;
static int base_periods;
static pth_uctx_t main_ctx, this_ctx;
#endif

// task descriptors are now banned from rtapi_data
// that's just too dangerous

#if defined(RTAPI_XENOMAI_USER)

RT_TASK ostask_array[RTAPI_MAX_TASKS + 1];

// this is needed due to the weirdness of the rt_task_self return value -
// it does _not_ match the address of the RT_TASK structure it was 
// created with
RT_TASK *ostask_self[RTAPI_MAX_TASKS + 1];

#endif

#if defined(RTAPI_POSIX)
pth_uctx_t ostask_array[RTAPI_MAX_TASKS + 1]; /* thread's context */
#endif

#define MODULE_MAGIC  30812
#define TASK_MAGIC    21979	/* random numbers used as signatures */
#define SHMEM_MAGIC   25453

#define MAX_TASKS  64
#define MAX_MODULES  64
#define MODULE_OFFSET 32768


#if defined(RTAPI_POSIX)

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
#if !defined(RTAPI_XENOMAI_USER)
  gettimeofday(&schedule, NULL);
#endif
  return period;
}

int rtapi_task_new(void (*taskcode) (void*), void *arg,
		   int prio, int owner, unsigned long int stacksize, 
		   int uses_fp, char *name, int cpu_id) 
{
    int n;
  int task_id;
  task_data *task;

  /* find an empty entry in the task array */
  /*! \todo  FIXME - this is not 100% thread safe.  If another thread
     calls this function after the first thread breaks out of
     the loop but before it sets the magic number, two tasks
     might wind up assigned to the same structure.  Need an
     atomic test and set for the magic number.  Not tonight! */

  n = 1; // tasks start at one!
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

  ostask_array[n] = NULL;
#endif

  /* label as a valid task structure */
  /*! \todo FIXME - end of non-threadsafe window */
  if(stacksize < 16384) stacksize = 16384;
  task->magic = TASK_MAGIC;
  task->owner = owner;
  task->arg = arg;
  task->stacksize = stacksize;
  task->taskcode = taskcode;
  task->prio = prio;
  task->uses_fp = uses_fp;
  task->cpu = cpu_id;
  strncpy(task->name, name, sizeof(task->name));
  task->name[sizeof(task->name) - 1]= '\0';

  /* and return handle to the caller */

  return n;
}


int rtapi_task_delete(int id) {
  task_data *task;
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
  if ((retval = rt_task_delete( &ostask_array[id] )) < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,"ERROR: rt_task_delete() = %d %s\n", retval, strerror(retval));
      return retval;
  }
#endif
#if defined(RTAPI_POSIX)
  pth_uctx_destroy(ostask_array[id]);
#endif
  task->magic = 0;
  return 0;
}

static void wrapper(void *arg)
{

    int task_id = (int) arg;
    task_data *task = &task_array[task_id];
#if defined(RTAPI_XENOMAI_USER)
    int ret;
#endif

    /* use the argument to point to the task data */
    if (task->period < period) task->period = period;
    task->ratio = task->period / period;
    rtapi_print_msg(RTAPI_MSG_DBG, "wrapper: task %p '%s' period=%d prio=%d ratio=%d\n",
		    task, task->name, task->ratio * period, task->prio, task->ratio);
    
#if defined(RTAPI_XENOMAI_USER)
    ostask_self[task_id]  = rt_task_self();
    
    if ((ret = rt_task_set_periodic(NULL, 
				    TM_NOW , 
				    task->ratio * period)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"ERROR: rt_task_set_periodic(%d,%s) failed %d\n", 
			task_id, task->name, ret);
	abort();
    }
#endif
    
  /* call the task function with the task argument */
    (task->taskcode) (task->arg);
    
    rtapi_print_msg(RTAPI_MSG_ERR,"ERROR: reached end of wrapper for task %d '%s'\n", 
		    task_id, task->name);
}

int rtapi_task_start(int task_id, unsigned long int period_nsec)
{
  task_data *task;
  int retval;

#if defined(RTAPI_XENOMAI_USER)
  int which_cpu = 0;
#endif

  if (task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];

  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

  if (period_nsec < period) period_nsec = period;
  task->period = period_nsec;
  task->ratio = period_nsec / period;

  /* create the thread - use the wrapper function, pass it a pointer
   * to the task structure so it can call the actual task function 
   */

#if  defined(RTAPI_XENOMAI_USER)

#if !defined(BROKEN_XENOMAU_CPU_AFFINITY)
  // seems to work for me
  // not sure T_CPU(n) is possible - see:
  // http://www.xenomai.org/pipermail/xenomai-help/2010-09/msg00081.html

  if (task->cpu > -1)  // explicitly set by threads, motmod
      which_cpu = T_CPU(task->cpu);
#endif

  // http://www.xenomai.org/documentation/trunk/html/api/group__task.html#ga03387550693c21d0223f739570ccd992
  // Passing T_FPU|T_CPU(1) in the mode parameter thus creates a 
  // task with FPU support enabled and which will be affine to CPU #1
  // the task will start out dormant; execution begins with rt_task_start()

  // since this is a usermode RT task, it will be FP anyway
  if ((retval = rt_task_create (&ostask_array[task_id], task->name, task->stacksize, task->prio, 
				(task->uses_fp ? T_FPU : 0) | which_cpu )) != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "rt_task_create failed, rc = %d\n", retval );
      return -ENOMEM;
  }
  if ((retval = rt_task_start( &ostask_array[task_id], wrapper, (void*)task_id))) {
      rtapi_print_msg(RTAPI_MSG_INFO, "rt_task_start failed, rc = %d\n", retval );
      return -ENOMEM;
  }
#endif

#if defined(RTAPI_POSIX)
  retval = pth_uctx_create(&ostask_array[task_id]);
  if (retval == FALSE)
    return -ENOMEM;
  retval = pth_uctx_make(ostask_array[task_id], NULL, task->stacksize, NULL,
			 wrapper, (void*)task_id, 0);
  if (retval == FALSE)
    return -ENOMEM;
#endif

  return 0;
}


int rtapi_task_stop(int task_id)
{
  task_data *task;
#if defined(RTAPI_XENOMAI_USER)
  int retval;
#endif

  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];

  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

#if defined(RTAPI_XENOMAI_USER)
 if ((retval = rt_task_delete( &ostask_array[task_id] )) < 0) {
     rtapi_print_msg(RTAPI_MSG_ERR,"rt_task_delete() = %d\n", retval);
     return retval;
 }
#endif

#if defined(RTAPI_POSIX)
  pth_uctx_destroy(ostask_array[task_id]);
#endif

  return 0;
}

int rtapi_task_pause(int task_id)
{
  task_data *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

#if defined(RTAPI_XENOMAI_USER)
  return rt_task_suspend( &ostask_array[task_id] );
#else
  return -ENOSYS;
#endif
}

int rtapi_task_resume(int task_id)
{
  task_data *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

#if defined(RTAPI_XENOMAI_USER)
  return rt_task_resume( &ostask_array[task_id] );
#else
  return -ENOSYS;
#endif
}


#if defined(RTAPI_POSIX)
int rtapi_wait(void)
{
  pth_uctx_switch(this_ctx, main_ctx);
  return 0;
}
#endif


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
		task_data *task = &task_array[t];
		if(task->magic == TASK_MAGIC && ostask_array[t] && 
			(base_periods % task->ratio == 0)) {
		    this_ctx = ostask_array[t];
		    if(pth_uctx_switch(main_ctx,  ostask_array[t]) == FALSE) _exit(1);
		}
	    }
	}
    }
}
#endif

