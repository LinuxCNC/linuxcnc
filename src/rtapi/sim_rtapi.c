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
# $Revision$
* $Author$
* $Date$
********************************************************************/

#include <stdio.h>		/* vprintf() */
#include <stdlib.h>		/* malloc(), sizeof() */
#include <stdarg.h>		/* va_* */
#include <pthread.h>		/* pthread_create() */
#include <unistd.h>		/* usleep() */
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include "rtapi.h"		/* these decls */


/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

struct rtapi_module {
  int magic;
};

struct rtapi_task {
  int magic;			/* to check for valid handle */
  int owner;
  pthread_t id;			/* OS specific task identifier */
  size_t stacksize;
  int prio;
  void *arg;
  void (*taskcode) (void*);	/* pointer to task function */
};

#define MODULE_MAGIC  30812
#define TASK_MAGIC    21979	/* random numbers used as signatures */
#define SHMEM_MAGIC   25453

#define MAX_TASKS  64
#define MAX_MODULES  64
#define MODULE_OFFSET 32768

/* data for all tasks */
static struct rtapi_task task_array[MAX_TASKS] = {{0},};
static struct rtapi_module module_array[MAX_MODULES] = {{0},};

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


int rtapi_init(char *modname)
{
  int n, result=0;
  /* clear the task array - if magic doesn't contain the magic
     number, that means that array entry is empty */
  for (n = 0; n < MAX_MODULES; n++) {
      printf("rtapi_init n=%d [n].magic=%d\n", n, module_array[n].magic);
    if(module_array[n].magic != MODULE_MAGIC) {
      result = n + MODULE_OFFSET;
      module_array[n].magic = MODULE_MAGIC;
      return result;
    }
  }
  return RTAPI_NOMEM;
}


int rtapi_exit(int id)
{
  int n = id - MODULE_OFFSET;
  if(n < 0 || n >= MAX_MODULES) return -1;
  module_array[n].magic = 0;
  return RTAPI_SUCCESS;
}


static int period = 0;
int rtapi_clock_set_period(unsigned long int nsecs)
{
  if(nsecs == 0) return period;
  period = nsecs;
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
    return RTAPI_NOMEM;
  task = &(task_array[n]);

  /* check requested priority */
  if ((prio < rtapi_prio_highest()) || (prio > rtapi_prio_lowest()))
    return RTAPI_INVAL;

  /* label as a valid task structure */
  /*! \todo FIXME - end of non-threadsafe window */
  task->magic = TASK_MAGIC;
  task->owner = owner;
  task->id = -1;
  task->arg = arg;
  task->stacksize = stacksize;
  task->taskcode = taskcode;
  task->prio = prio;

  /* and return handle to the caller */

  return RTAPI_SUCCESS;
}


int rtapi_task_delete(int id) {
  struct rtapi_task *task;

  if(id < 0 || id >= MAX_TASKS) return RTAPI_INVAL;

  task = &(task_array[id]);
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return RTAPI_INVAL;

  /* mark the task struct as available */
  if(task->id != -1) pthread_cancel(task->id);
  task->magic = 0;
  return 0;
}


/* we define taskcode as taking an int and returning void. */
/* pthread wants it to take a void pointer and return a void pointer */
/* we solve this with a wrapper function that meets pthread's needs */

static void *wrapper(void *arg)
{
  struct rtapi_task *task;

  /* use the argument to point to the task data */
  task = (struct rtapi_task*)arg;
  /* call the task function with the task argument */
  (task->taskcode) (task->arg);
  /* done */
  return NULL;
}


int rtapi_task_start(int task_id, unsigned long int period_nsec)
{
  struct rtapi_task *task;
  int retval;
  pthread_attr_t attr;
  struct sched_param sched_param;

  if(task_id < 0 || task_id >= MAX_TASKS) return RTAPI_INVAL;
    
  task = &task_array[task_id];

  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return RTAPI_INVAL;

  /* get default thread attributes */
  pthread_attr_init(&attr);

  /* set priority */
  sched_param.sched_priority = task->prio;
  pthread_attr_setschedparam(&attr, &sched_param);
  pthread_attr_setstacksize(&attr, task->stacksize);

  /* create the thread - use the wrapper function, pass it a pointer
     to the task structure so it can call the actual task function */
  retval = pthread_create(&(task->id), &attr, wrapper, (void *) task);
  if (retval != 0)
    return RTAPI_NOMEM;

    /* need to be root to set SCHED_FIFO */
   retval = pthread_setschedparam(task->id, SCHED_FIFO, &sched_param);
   if (retval != 0) {
       rtapi_print_msg(RTAPI_MSG_DBG, "could not set SCHED_FIFO (not fatal)\n");
   }
  return RTAPI_SUCCESS;
}


int rtapi_task_stop(int task_id)
{
  int retval;
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return RTAPI_INVAL;
    
  task = &task_array[task_id];

  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return RTAPI_INVAL;

  retval = pthread_cancel(task->id);
  if (retval != 0)
    return RTAPI_FAIL;
  return RTAPI_SUCCESS;
}

int rtapi_task_pause(int task_id)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return RTAPI_INVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return RTAPI_INVAL;

  return RTAPI_UNSUP;
}

int rtapi_task_resume(int task_id)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return RTAPI_INVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return RTAPI_INVAL;

  return RTAPI_UNSUP;
}


int rtapi_task_set_period(int task_id,
			  unsigned long int period_nsec)
{
  struct rtapi_task *task;
  if(task_id < 0 || task_id >= MAX_TASKS) return RTAPI_INVAL;
    
  task = &task_array[task_id];
  
  /* validate task handle */
  if (task->magic != TASK_MAGIC)
    return RTAPI_INVAL;

  return RTAPI_UNSUP;
}


int rtapi_wait(void)
{
  usleep(1);
  pthread_testcancel();
  return RTAPI_SUCCESS;
}


void rtapi_outb(unsigned char byte, unsigned int port)
{
  return;
}

unsigned char rtapi_inb(unsigned int port)
{
  return 0;
}

/*! \todo 
  FIXME - no support for simulated interrupts
*/

int rtapi_assign_interrupt_handler(unsigned int irq, void (*handler) (void))
{
  return RTAPI_UNSUP;
}

int rtapi_free_interrupt_handler(unsigned int irq)
{
  return RTAPI_UNSUP;
}

int rtapi_enable_interrupt(unsigned int irq)
{
  return RTAPI_UNSUP;
}

int rtapi_disable_interrupt(unsigned int irq)
{
  return RTAPI_UNSUP;
}


/*! \todo FIXME - no support for semaphores */

int rtapi_sem_new(int key, int module_id)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_delete(int id)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_give(int id)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_take(int id)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_try(int id)
{
  return RTAPI_UNSUP;
}


#if 0
/*! \todo FIXME - no support for fifos */

int rtapi_fifo_new(int key, unsigned long int size,
		   rtapi_fifo_handle * fifoptr)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_delete(rtapi_fifo_handle fifo)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_read(rtapi_fifo_handle fifo, char *buf, unsigned long int size)
{
  return RTAPI_UNSUP;
}

int rtapi_fifo_write(rtapi_fifo_handle fifo,
		     char *buf, unsigned long int size)
{
  return RTAPI_UNSUP;
}
#endif

long int simple_strtol(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}


#include "rtapi/sim_common.h"
