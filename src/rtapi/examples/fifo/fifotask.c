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
  fifotask.c

  Set up a periodic task that writes to a FIFO to release a user-level task
*/

#include "rtapi.h"
#include "rtapi_app.h"		/* rtapi_app_main,exit() */
#include "common.h"		/* FIFO_KEY */

static int module;		/* the module ID */
static int fifo_task;		/* the task ID */
static int fifo;		/* the fifo ID */
enum { TIMER_PERIOD_NSEC = 10000000 };	/* timer period, in nanoseconds */
enum { FIFO_PERIOD_NSEC = 1000000000 };	/* timer period, in nanoseconds */
enum { FIFO_STACKSIZE = 2048 };	/* how big the stack is */

static int count = 0;
static int len = 0;
static int total = 0;

/* task code, executed each timer interrupt */
void fifo_code(void *arg)
{
    int n, tmp;
    char buffer[FIFO_SIZE + 1] = { '\n' };

    while (1) {
	/* write count to buffer as a 3 digit number */
	tmp = count;
	for (n = 0; n < 3; n++) {
	    buffer[2 - n] = (char) ('0' + (tmp % 10));
	    tmp /= 10;
	}
	buffer[3] = ':';
	/* write 'len' digits of 'len' */
	for (n = 0; n < len; n++) {
	    buffer[4 + n] = (char) ('0' + len);
	}
	rtapi_fifo_write(fifo, buffer, len + 4);
	total += len + 4;
	buffer[4 + n] = (char) '\0';
	rtapi_print_msg(2, "fifotask: total %d, wrote '%s'\n", total, buffer);
	count++;
	len++;
	if (len > 9)
	    len = 0;
	rtapi_wait();
    }

    return;
}

/* part of the Linux kernel module that kicks off the fifo task */
int rtapi_app_main(void)
{
    int retval;
    int fifo_prio;
    long period;

    module = rtapi_init("FIFOTASK");
    if (module < 0) {
	rtapi_print("fifotask init: rtapi_init failed with %d\n", module);
	return -1;
    }

    /* allocate and initialize the fifo */
    fifo = rtapi_fifo_new(FIFO_KEY, module, FIFO_SIZE, 'W');
    if (fifo < 0) {
	rtapi_print("fifotask init: rtapi_fifo_new failed with %d\n", fifo);
	rtapi_exit(module);
	return -1;
    }

    rtapi_print("fifotask: created fifo\n");

    /* is timer started? if so, what period? */
    period = rtapi_clock_set_period(0);
    if (period == 0) {
	/* not running, start it */
	rtapi_print("fifotask init: starting timer with period %ld\n",
	    TIMER_PERIOD_NSEC);
	period = rtapi_clock_set_period(TIMER_PERIOD_NSEC);
	if (period < 0) {
	    rtapi_print
		("fifotask init: rtapi_clock_set_period failed with %ld\n",
		period);
	    rtapi_exit(module);
	    return -1;
	}
    }
    /* make sure period <= desired period (allow 1% roundoff error) */
    if (period > (TIMER_PERIOD_NSEC + (TIMER_PERIOD_NSEC / 100))) {
	/* timer period too long */
	rtapi_print("fifotask init: clock period too long: %ld\n", period);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("fifotask init: desired clock %ld, actual %ld\n",
	TIMER_PERIOD_NSEC, period);

    /* set the task priority to lowest, since we only have one task */
    fifo_prio = rtapi_prio_lowest();

    /* create the fifo task */
    fifo_task = rtapi_task_new(fifo_code, 0 /* arg */ , fifo_prio, module,
	FIFO_STACKSIZE, RTAPI_NO_FP);
    if (fifo_task < 0) {
	rtapi_print("fifotask init: rtapi_task_new failed with %d\n",
	    fifo_task);
	rtapi_exit(module);
	return -1;
    }

    /* start the fifo task */
    retval = rtapi_task_start(fifo_task, FIFO_PERIOD_NSEC);
    if (retval < 0) {
	rtapi_print("fifotask init: rtapi_task_start failed with %d\n",
	    retval);
	rtapi_exit(module);
	return -1;
    }

    rtapi_print("fifotask: started fifo task\n");

    return 0;
}

/* part of the Linux kernel module that stops the fifo task */
void rtapi_app_exit(void)
{
    int retval;

    retval = rtapi_task_pause(fifo_task);
    if (retval < 0) {
	rtapi_print("fifotask exit: rtapi_task_stop failed with %d\n",
	    retval);
    }

    retval = rtapi_task_delete(fifo_task);
    if (retval < 0) {
	rtapi_print("fifotask exit: rtapi_task_delete failed with %d\n",
	    retval);
    }

    retval = rtapi_fifo_delete(fifo, module);
    if (retval < 0) {
	rtapi_print("fifotask exit: rtapi_fifo_delete failed with %d\n",
	    retval);
    }

    rtapi_print("fifotask exit: done\n");

    rtapi_exit(module);
}
