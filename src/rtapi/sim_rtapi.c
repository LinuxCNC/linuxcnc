/*
  sim_rtapi.c

  Implementations of RT API functions declared in rtapi.h, for simulated
  Linux process
*/

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

struct rtapi_task {
  int magic;			/* to check for valid handle */
  pthread_t id;			/* OS specific task identifier */
  int arg;			/* argument for task function */
  void (*taskcode) (int);	/* pointer to task function */
};

struct rtapi_shmem {
  int magic;			/* to check for valid handle */
  int key;			/* key to shared memory area */
  int id;			/* OS identifier for shmem */
  unsigned long int size;	/* size of shared memory area */
  void *mem;			/* pointer to the memory */
};

#define TASK_MAGIC    21979	/* random numbers used as signatures */
#define SHMEM_MAGIC   25453

#define MAX_TASKS  64

/* data for all tasks */
static struct rtapi_task task_array[MAX_TASKS];


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


int rtapi_init(void)
{
  int n;
  /* clear the task array - if magic doesn't contain the magic
     number, that means that array entry is empty */
  for (n = 0; n < MAX_TASKS; n++)
    task_array[n].magic = 0;

  return RTAPI_SUCCESS;
}


int rtapi_exit(void)
{
  return RTAPI_SUCCESS;
}


int rtapi_clock_set_period(unsigned long int nsecs)
{
  return RTAPI_SUCCESS;
}


int rtapi_task_new(rtapi_task_handle * taskptr)
{
  int n;
  rtapi_task_handle task;

  /* validate taskptr */
  if (taskptr == NULL)
    return RTAPI_INVAL;

  /* find an empty entry in the task array */
  /* FIXME - this is not 100% thread safe.  If another thread
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

  /* label as a valid task structure */
  /* FIXME - end of non-threadsafe window */
  task->magic = TASK_MAGIC;

  /* and return handle to the caller */
  *taskptr = task;

  return RTAPI_SUCCESS;
}


int rtapi_task_delete(rtapi_task_handle task)
{
  /* validate task handle */
  if (task == NULL)
    return RTAPI_BADH;
  if (task->magic != TASK_MAGIC)
    return RTAPI_BADH;

  /* mark the task struct as available */
  task->magic = 0;
  return 0;
}


/* we define taskcode as taking an int and returning void. */
/* pthread wants it to take a void pointer and return a void pointer */
/* we solve this with a wrapper function that meets pthread's needs */

static void *wrapper(void *arg)
{
  rtapi_task_handle task;

  /* use the argument to point to the task data */
  task = arg;
  /* call the task function with the task argument */
  (task->taskcode) (task->arg);
  /* done */
  return NULL;
}


int rtapi_task_start(rtapi_task_handle task,
		     void (*taskcode) (int),
		     int arg, int prio,
		     unsigned long int stacksize,
		     unsigned long int period_nsec,
		     unsigned long long when, unsigned char uses_fp)
{
  int retval;
  pthread_attr_t attr;
  struct sched_param sched_param;

  /* validate task handle */
  if (task == NULL)
    return RTAPI_BADH;
  if (task->magic != TASK_MAGIC)
    return RTAPI_BADH;

  /* check requested priority */
  if ((prio < rtapi_prio_highest()) || (prio > rtapi_prio_lowest()))
    return RTAPI_INVAL;

  /* get default thread attributes */
  pthread_attr_init(&attr);

  /* set priority */
  sched_param.sched_priority = prio;
  pthread_attr_setschedparam(&attr, &sched_param);

  /* create the thread - use the wrapper function, pass it a pointer
     to the task structure so it can call the actual task function */
  task->taskcode = taskcode;
  task->arg = arg;
  retval = pthread_create(&(task->id), &attr, wrapper, (void *) task);
  if (retval != 0)
    return RTAPI_NOMEM;
  retval = pthread_setschedparam(task->id, SCHED_FIFO, &sched_param);
  if (retval != 0)
    /* need to be root to set SCHED_FIFO */
    return RTAPI_PERM;
  return RTAPI_SUCCESS;
}


int rtapi_task_stop(rtapi_task_handle task)
{
  int retval;

  /* validate task handle */
  if (task == NULL)
    return RTAPI_BADH;
  if (task->magic != TASK_MAGIC)
    return RTAPI_BADH;

  retval = pthread_cancel(task->id);
  if (retval != 0)
    return RTAPI_FAIL;
  return RTAPI_SUCCESS;
}

int rtapi_task_pause(rtapi_task_handle task)
{

  /* validate task handle */
  if (task == NULL)
    return RTAPI_BADH;
  if (task->magic != TASK_MAGIC)
    return RTAPI_BADH;

  /* FIXME - Fred originally had this function return success.
     I changed it to return Not Supported.  Is that right? */
  return RTAPI_UNSUP;
}

int rtapi_task_resume(rtapi_task_handle task)
{

  /* validate task handle */
  if (task == NULL)
    return RTAPI_BADH;
  if (task->magic != TASK_MAGIC)
    return RTAPI_BADH;

  /* FIXME - Fred originally had this function return success.
     I changed it to return Not Supported.  Is that right? */
  return RTAPI_UNSUP;
}


int rtapi_task_set_period(rtapi_task_handle task,
			  unsigned long int period_nsec)
{
  /* validate task handle */
  if (task == NULL)
    return RTAPI_BADH;
  if (task->magic != TASK_MAGIC)
    return RTAPI_BADH;

  /* FIXME - Fred originally had this function return success.
     I changed it to return Not Supported.  Is that right? */
  return RTAPI_UNSUP;
}


int rtapi_wait(void)
{
  pthread_testcancel();
  usleep(1);
  pthread_testcancel();
  return RTAPI_SUCCESS;
}


int rtapi_task_get_handle(rtapi_task_handle * taskptr)
{
  int n;
  pthread_t task_id;

  /* validate taskptr */
  if (taskptr == NULL)
    return RTAPI_INVAL;

  /* ask OS for task id */
  task_id = pthread_self();

  /* search task array for a matching entry */
  n = 0;
  while (n < MAX_TASKS) {
    if ((task_array[n].magic == TASK_MAGIC) && (task_array[n].id == task_id)) {
      /* found it */
      *taskptr = &(task_array[n]);
      return RTAPI_SUCCESS;
    }
    n++;
  }
  return RTAPI_FAIL;
}


int rtapi_shmem_new(int key, unsigned long int size,
		    rtapi_shmem_handle * shmemptr)
{
  rtapi_shmem_handle shmem;

  /* validate shmemptr */
  if (shmemptr == NULL)
    return RTAPI_INVAL;

  /* alloc space for shmem structure */
  shmem = malloc(sizeof(struct rtapi_shmem));
  if (shmem == NULL)
    return RTAPI_NOMEM;

  /* now get shared memory block from OS */
  shmem->id = shmget((key_t) key, (int) size, IPC_CREAT | 0666);
  if (shmem->id == -1) {
    free(shmem);
    return RTAPI_NOMEM;
  }
  /* and map it into process space */
  shmem->mem = shmat(shmem->id, 0, 0);
  if ((int) (shmem->mem) == -1) {
    free(shmem);
    return RTAPI_NOMEM;
  }

  /* label as a valid shmem structure */
  shmem->magic = SHMEM_MAGIC;
  /* fill in the other fields */
  shmem->size = size;
  shmem->key = key;

  /* return handle to the caller */
  *shmemptr = shmem;
  return RTAPI_SUCCESS;
}


int rtapi_shmem_getptr(rtapi_shmem_handle shmem, void **ptr)
{
  /* validate shmem handle */
  if (shmem == NULL)
    return RTAPI_BADH;
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADH;

  /* pass memory address back to caller */
  *ptr = shmem->mem;
  return RTAPI_SUCCESS;
}


int rtapi_shmem_delete(rtapi_shmem_handle shmem)
{
  struct shmid_ds d;
  int r1, r2;

  /* validate shmem handle */
  if (shmem == NULL)
    return RTAPI_BADH;
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADH;

  /* unmap the shared memory */
  r1 = shmdt(shmem->mem);

  /* destroy the shared memory */
  r2 = shmctl(shmem->id, IPC_RMID, &d);
  /* FIXME - Fred had the first two arguments reversed.  I changed
     them to match the shmctl man page on my machine.  Since his way
     worked, maybe there is difference between different libs, or
     maybe my man page is just wrong. */

  /* free the shmem structure */
  shmem->magic = 0;
  free(shmem);

  if ((r1 != 0) || (r2 != 0))
    return RTAPI_FAIL;
  return RTAPI_SUCCESS;
}


void rtapi_print(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}


void rtapi_print_dbg(int dbg, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}


void rtapi_outb(unsigned char byte, unsigned int port)
{
  return;
}

unsigned char rtapi_inb(unsigned int port)
{
  return 0;
}

/*
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


/* FIXME - no support for semaphores */

int rtapi_sem_new(rtapi_sem_handle * semptr)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_delete(rtapi_sem_handle sem)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_give(rtapi_sem_handle sem)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_take(rtapi_sem_handle sem)
{
  return RTAPI_UNSUP;
}

int rtapi_sem_try(rtapi_sem_handle sem)
{
  return RTAPI_UNSUP;
}


/* FIXME - no support for fifos */

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
