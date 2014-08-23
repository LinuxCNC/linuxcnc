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
#include <pth.h>		/* pth_uctx_* */
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
  pth_uctx_t ctx;		/* thread's context */
  size_t stacksize;
  int prio;
  int period;
  int ratio;
  void *arg;
  void (*taskcode) (void*);	/* pointer to task function */
};

static struct timeval schedule;
static int base_periods;
static pth_uctx_t main_ctx, this_ctx;

#define MODULE_MAGIC  30812
#define TASK_MAGIC    21979	/* random numbers used as signatures */
#define SHMEM_MAGIC   25453

#define MAX_TASKS  64
#define MAX_MODULES  64
#define MODULE_OFFSET 32768

/* data for all tasks */
static struct rtapi_task task_array[MAX_TASKS] = {{0},};

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
    int prio, int owner, unsigned long int stacksize, int uses_fp) {
  int n;
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
  task = &(task_array[n]);

  /* check requested priority */
  if ((prio < rtapi_prio_highest()) || (prio > rtapi_prio_lowest()))
    return -EINVAL;

  /* label as a valid task structure */
  /*! \todo FIXME - end of non-threadsafe window */
  if(stacksize < 16384) stacksize = 16384;
  task->magic = TASK_MAGIC;
  task->owner = owner;
  task->ctx = NULL;
  task->arg = arg;
  task->stacksize = stacksize;
  task->taskcode = taskcode;
  task->prio = prio;

  /* and return handle to the caller */

  return n;
}


int rtapi_task_delete(int id) {
  struct rtapi_task *task;

  if(id < 0 || id >= MAX_TASKS) return -EINVAL;

  task = &(task_array[id]);
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

  pth_uctx_destroy(task->ctx);
  
  task->magic = 0;
  return 0;
}


static void wrapper(void *arg)
{
  struct rtapi_task *task;

  /* use the argument to point to the task data */
  task = (struct rtapi_task*)arg;
  if(task->period < period) task->period = period;
  task->ratio = task->period / period;
  rtapi_print_msg(RTAPI_MSG_INFO, "task %p period = %d ratio=%d\n",
	  task, task->period, task->ratio);

  /* call the task function with the task argument */
  (task->taskcode) (task->arg);

  rtapi_print("ERROR: reached end of wrapper for task %d\n", (int)(task - task_array));
}


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
  retval = pth_uctx_create(&task->ctx);
  if (retval == FALSE)
    return -ENOMEM;
  retval = pth_uctx_make(task->ctx, NULL, task->stacksize, NULL,
	  wrapper, (void*)task, 0);
  if (retval == FALSE)
    return -ENOMEM;

  return 0;
}


int rtapi_task_stop(int task_id)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];

  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

  pth_uctx_destroy(task->ctx);

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

  return -ENOSYS;
}

int rtapi_task_resume(int task_id)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return -EINVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return -EINVAL;

  return -ENOSYS;
}


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

  return 0;
}

int rtapi_wait(void)
{
  pth_uctx_switch(this_ctx, main_ctx);
  return 0;
}


void rtapi_outb(unsigned char byte, unsigned int port)
{
  return;
}

unsigned char rtapi_inb(unsigned int port)
{
  return 0;
}

long int simple_strtol(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}

#define MIN_RUNS 13

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


#include "rtapi/sim_common.h"
