/*
  slave.c

  Set up a periodic task that gives a semaphore every several
  task cycles. Load this after master task, since that defines
  the global semaphore 'master_sem'.
*/

#include "rtapi.h"
#include "rtapi_app.h"		/* rtapi_app_main,exit() */
#include "common.h"		/* semaphore key */

static int module;
static int slave_sem;		/* the semaphore ID */
static int slave_task;		/* the task ID */
static unsigned int slave_count = 0;
enum { SLAVE_STACKSIZE = 1024 };	/* how big the stack is */

/* task code, executed when semaphore is released by master task */
void slave_code(void *arg)
{
  while (1) {
    rtapi_sem_take(slave_sem);
    slave_count++;
    rtapi_print("slave: got sem, count = %d\n", slave_count);
  }

  return;
}

/* part of the Linux kernel module that kicks off the timer task */
int rtapi_app_main(void)
{
  int retval;
  int slave_prio;

  module = rtapi_init("SEM_SLAVE");
  if (module < 0) {
    rtapi_print("sem slave init: rtapi_init returned %d\n", module);
    return -1;
  }

  /* open the semaphore */
  slave_sem = rtapi_sem_new(SEM_KEY, module);
  if (slave_sem < 0) {
    rtapi_print("sem slave init: rtapi_sem_new returned %d\n", slave_sem);
    rtapi_exit(module);
    return -1;
  }

  /* set the task priority to the lowest one; the master is higher */
  slave_prio = rtapi_prio_lowest();

  /* create the slave task */
  slave_task = rtapi_task_new(slave_code, 0 /* arg */ , slave_prio, module,
			      SLAVE_STACKSIZE, RTAPI_NO_FP);
  if (slave_task < 0) {
    rtapi_print("sem slave init: rtapi_task_new returned %d\n", slave_task);
    rtapi_exit(module);
    return -1;
  }

  /* start the slave task */
  retval = rtapi_task_resume(slave_task);
  if (retval != RTAPI_SUCCESS) {
    rtapi_print("sem slave init: rtapi_task_start returned %d\n", retval);
    rtapi_exit(module);
    return -1;
  }

  rtapi_print("sem slave init: started slave task\n");

  return 0;
}

/* part of the Linux kernel module that stops the slave task */
void rtapi_app_exit(void)
{
  int retval;

  retval = rtapi_task_pause(slave_task);
  if (retval != RTAPI_SUCCESS) {
    rtapi_print("sem slave exit: rtapi_task_stop returned %d\n", retval);
  }
  retval = rtapi_task_delete(slave_task);
  if (retval != RTAPI_SUCCESS) {
    rtapi_print("sem slave exit: rtapi_task_delete returned %d\n", retval);
  }

  retval = rtapi_sem_delete(slave_sem, module);
  if (retval != RTAPI_SUCCESS) {
    rtapi_print("sem slave exit: rtapi_sem_delete returned %d\n", retval);
  }
  rtapi_print("sem slave exit: slave count is %d\n", slave_count);

  rtapi_exit(module);
}
