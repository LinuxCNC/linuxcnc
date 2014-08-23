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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
/*
  shmemtask.c

  Set up a periodic task that increments a heartbeat in shared memory.
*/

#include "rtapi.h"
#include "rtapi_app.h"		/* rtapi_app_main,exit() */
#include "common.h"		/* shmem structure, SHMEM_KEY */

static int module;
static int shmem_task;		/* the task ID */
static int shmem_mem;		/* the shared memory ID */
enum { TIMER_PERIOD_NSEC = 1000000 };	/* timer period, in nanoseconds */
enum { SHMEM_PERIOD_NSEC = 1000000 };	/* task period, in nanoseconds */
enum { SHMEM_STACKSIZE = 1024 };	/* how big the stack is */

static int key = SHMEM_KEY;
static SHMEM_STRUCT *shmem_struct = 0;

/* task code, executed periodically */
void shmem_code(void *arg)
{
    while (1) {
	if (0 != shmem_struct) {
	    shmem_struct->heartbeat++;
	}
	rtapi_wait();
    }

    return;
}

/* part of the Linux kernel module that kicks off the shmem task */
int rtapi_app_main(void)
{
    int retval;
    int shmem_prio;
    long period;

    module = rtapi_init("SHMEMTASK");
    if (module < 0) {
	rtapi_print("shmemtask init: rtapi_init returned %d\n", module);
	return -1;
    }
    /* allocate and initialize the shared memory structure */
    shmem_mem = rtapi_shmem_new(key, module, sizeof(SHMEM_STRUCT));
    if (shmem_mem < 0) {
	rtapi_print("shmemtask init: rtapi_shmem_new returned %d\n",
	    shmem_mem);
	rtapi_exit(module);
	return -1;
    }
    retval = rtapi_shmem_getptr(shmem_mem, (void **) &shmem_struct);
    if (retval < 0) {
	rtapi_print("shmemtask init: rtapi_shmem_getptr returned %d\n",
	    retval);
	rtapi_exit(module);
	return -1;
    }

    shmem_struct->heartbeat = 0;

    /* is timer started? if so, what period? */
    period = rtapi_clock_set_period(0);
    if (period == 0) {
	/* not running, start it */
	rtapi_print("shmemtask init: starting timer with period %ld\n",
	    TIMER_PERIOD_NSEC);
	period = rtapi_clock_set_period(TIMER_PERIOD_NSEC);
	if (period < 0) {
	    rtapi_print
		("shmemtask init: rtapi_clock_set_period failed with %ld\n",
		period);
	    rtapi_exit(module);
	    return -1;
	}
    }
    /* make sure period <= desired period (allow 1% roundoff error) */
    if (period > (TIMER_PERIOD_NSEC + (TIMER_PERIOD_NSEC / 100))) {
	/* timer period too long */
	rtapi_print("shmemtask init: clock period too long: %ld\n", period);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("shmemtask init: desired clock %ld, actual %ld\n",
	TIMER_PERIOD_NSEC, period);

    /* set the task priority to lowest, since we only have one task */
    shmem_prio = rtapi_prio_lowest();

    /* create the shmem task */
    shmem_task = rtapi_task_new(shmem_code, 0 /* arg */ , shmem_prio, module,
	SHMEM_STACKSIZE, RTAPI_NO_FP);
    if (shmem_task < 0) {
	rtapi_print("shmemtask init: rtapi_task_new returned %d\n",
	    shmem_task);
	rtapi_exit(module);
	return -1;
    }

    /* start the shmem task */
    retval = rtapi_task_start(shmem_task, SHMEM_PERIOD_NSEC);
    if (retval < 0) {
	rtapi_print("shmemtask init: rtapi_task_start returned %d\n", retval);
	rtapi_exit(module);
	return -1;
    }

    rtapi_print("shmemtask init: started shmem task\n");

    return 0;
}

/* part of the Linux kernel module that stops the shmem task */
void rtapi_app_exit(void)
{
    int retval;

    if (0 != shmem_struct) {
	rtapi_print("shmemtask exit: heartbeat is %u\n",
	    shmem_struct->heartbeat);
    }

    retval = rtapi_task_pause(shmem_task);
    if (retval < 0) {
	rtapi_print("shmemtask exit: rtapi_task_stop returned %d\n", retval);
    }
    retval = rtapi_task_delete(shmem_task);
    if (retval < 0) {
	rtapi_print("shmemtask exit: rtapi_task_delete returned %d\n",
	    retval);
    }
    retval = rtapi_shmem_delete(shmem_mem, module);
    if (retval < 0) {
	rtapi_print("shmemtask exit: rtapi_shmem_delete returned %d\n",
	    retval);
    }
    /* Clean up and exit */
    rtapi_exit(module);
}
