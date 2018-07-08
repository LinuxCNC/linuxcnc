//    Copyright 2003 John Kasunich
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
/*
  master.c

  Set up a periodic task that gives a semaphore every several
  task cycles. Load this first, since it defines 'master_sem',
  the global referenced by slave task.
*/

#include "rtapi.h"
#include "rtapi_app.h"		/* rtapi_app_main,exit() */
#include "common.h"		/* semaphore key */

static int module;
static int master_sem;		/* the semaphore ID */
static int master_task;		/* the task ID */
static unsigned int master_count = 0;
enum { TIMER_PERIOD_NSEC = 10000000 };	/* timer period, in nanoseconds */
enum { MASTER_PERIOD_NSEC = 1000000000 };	/* timer period, in
						   nanoseconds */
enum { MASTER_STACKSIZE = 1024 };	/* how big the stack is */

/* task code, executed each timer interrupt */
void master_code(void *arg)
{
    while (1) {
	rtapi_print("master: giving semaphore, count = %d\n", master_count);
	rtapi_sem_give(master_sem);
	rtapi_print("master: gave semaphore, count = %d\n", master_count);
	master_count++;
	rtapi_wait();
    }

    return;
}

/* part of the Linux kernel module that kicks off the timer task */
int rtapi_app_main(void)
{
    int retval;
    int master_prio;
    long period;

    module = rtapi_init("SEM_MASTER");
    if (module < 0) {
	rtapi_print("sem master init: rtapi_init returned %d\n", module);
	return -1;
    }

    /* create the semaphore */
    master_sem = rtapi_sem_new(SEM_KEY, module);
    if (master_sem < 0) {
	rtapi_print("sem master init: rtapi_sem_new returned %d\n",
	    master_sem);
	rtapi_exit(module);
	return -1;
    }

    /* is timer started? if so, what period? */
    period = rtapi_clock_set_period(0);
    if (period == 0) {
	/* not running, start it */
	rtapi_print("sem master init: starting timer with period %ld\n",
	    TIMER_PERIOD_NSEC);
	period = rtapi_clock_set_period(TIMER_PERIOD_NSEC);
	if (period < 0) {
	    rtapi_print
		("sem master init: rtapi_clock_set_period failed with %ld\n",
		period);
	    rtapi_exit(module);
	    return -1;
	}
    }
    /* make sure period <= desired period (allow 1% roundoff error) */
    if (period > (TIMER_PERIOD_NSEC + (TIMER_PERIOD_NSEC / 100))) {
	/* timer period too long */
	rtapi_print("sem master init: clock period too long: %ld\n", period);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("sem master init: desired clock %ld, actual %ld\n",
	TIMER_PERIOD_NSEC, period);

    /* set the task priority to one above the lowest; the slave task will be
       the lowest */
    master_prio = rtapi_prio_next_higher(rtapi_prio_lowest());

    /* create the master task */
    master_task =
	rtapi_task_new(master_code, 0 /* arg */ , master_prio, module,
	MASTER_STACKSIZE, RTAPI_NO_FP);
    if (master_task < 0) {
	rtapi_print("sem master init: rtapi_task_new returned %d\n",
	    master_task);
	rtapi_exit(module);
	return -1;
    }

    /* start the master task */
    retval = rtapi_task_start(master_task, MASTER_PERIOD_NSEC);
    if (retval < 0) {
	rtapi_print("sem master init: rtapi_task_start returned %d\n",
	    retval);
	rtapi_exit(module);
	return -1;
    }

    rtapi_print("sem master init: started master task\n");

    return 0;
}

/* part of the Linux kernel module that stops the master task */
void rtapi_app_exit(void)
{
    int retval;

    retval = rtapi_task_pause(master_task);
    if (retval < 0) {
	rtapi_print("sem master exit: rtapi_task_stop returned %d\n", retval);
    }
    retval = rtapi_task_delete(master_task);
    if (retval < 0) {
	rtapi_print("sem master exit: rtapi_task_delete returned %d\n",
	    retval);
    }

    retval = rtapi_sem_delete(master_sem, module);
    if (retval < 0) {
	rtapi_print("sem master exit: rtapi_sem_delete returned %d\n",
	    retval);
    }

    rtapi_print("sem master exit: master count is %d\n", master_count);

    rtapi_exit(module);
}
