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
  timertask.c

  Set up a periodic task that increments a counter. When the task
  is stopped, the cumulative counter value is printed.
*/

#include "rtapi.h"
#include "rtapi_app.h"		/* rtapi_app_main,exit() */

static int module;
static int timer_task;		/* the task ID */
static int timer_count = 0;	/* the output variable */
enum { TIMER_PERIOD_NSEC = 1000000 };	/* timer period, in nanoseconds */
enum { TASK_PERIOD_NSEC = 1000000 };	/* task period, in nanoseconds */
enum { TIMER_STACKSIZE = 1024 };	/* how big the stack is */

/* my BDI-2.18 machine does not correctly print long-longs.  I don't
know why, and don't have time to investigate right now.  These functions
will correctly print long-longs.  'buf' must point to a buffer at least
25 characters long, and 'v' is the value to print.  'v' will be
converted to a string which will be written to 'buf' */

/* these functions are crude hacks, but it turns out that even the
divide operation isn't implemented for longlongs in the kernel
libraries.  Hence the ugly cruft here, attempting to use only
extremely primitive operations, add, sub, and shift. */

void long_long_to_hex_str(char *buf, long long int v)
{
    int n;
    char *cp;
    unsigned char *ip, b, b1, b2;

    cp = buf;
    /* overlay an array of bytes on our longlong argument */
    ip = (unsigned char *) &v;
    /* process it a byte at a time */
    for (n = 0; n < 8; n++) {
	/* get the byte */
	b = ip[7 - n];
	/* break into hi and lo nibbles */
	b1 = b >> 4;
	b2 = b & 0x0F;
	/* print first nibble */
	if (b1 < 10) {
	    *(cp++) = '0' + b1;
	} else {
	    *(cp++) = 'A' + b1 - 10;
	}
	/* and second one */
	if (b2 < 10) {
	    *(cp++) = '0' + b2;
	} else {
	    *(cp++) = 'A' + b2 - 10;
	}
    }
    *cp = '\0';
}

unsigned long long int mul_ten(unsigned long long int x)
{
/*  perform multiply by 10 by shift and add (x*10 = x*8+x*2) */
    unsigned long long int x8, x2;

    x2 = x << 1;
    x8 = x2 << 2;
    return (x2 + x8);
}

/* General purpose unsigned longlong multiply routine.
   Untested for now, but straightforward.  I'm fairly
   confident that this is correct, will test later.
*/

unsigned long long int ull_mul(unsigned long long int a,
    unsigned long long int b)
{
/*  perform multiply by shift and add method */
    unsigned long long int r;

    r = 0;
    while (b > 0) {
	if (b & 1) {
	    r += a;
	}
	a <<= 1;
	b >>= 1;
    }
    return r;
}

unsigned long long int div_ten(unsigned long long int x)
{
/* perform divide by 10 by shifting and subtracting */
    unsigned long long int d, b, q;

    d = 10;
    d <<= 60;
    b = 1;
    b <<= 60;
    q = 0;
    while (b > 0) {
	if (x >= d) {
	    x -= d;
	    q += b;
	}
	d >>= 1;
	b >>= 1;
    }
    return q;
}

/* General purpose unsigned longlong divide routine.
   Untested.  I think the algorithm is correct, but
   this one needs testing before it can be trusted.
*/

unsigned long long int ull_div(unsigned long long int a,
    unsigned long long int b)
{
/* perform divide by shifting and subtracting */
    unsigned long long int p, q;

    if (b == 0) {
	/* divide by zero */
	return 0;
    }

    p = 1;
    while ((b < a) && ((signed long long int) (b) > 0)) {
	b <<= 1;
	p <<= 1;
    }
    q = 0;
    while (p > 0) {
	if (a >= b) {
	    a -= b;
	    q += p;
	}
	b >>= 1;
	p >>= 1;
    }
    /* quotient is in q, remainder is in a */
    return q;
}

void long_long_to_dec_str(char *buf, long long int v)
{
    char *cp, *cp1;
    unsigned long long int a, b;
    int n;

    cp = buf;
    /* deal with negative values by inverting them and prepending '-' to the
       result */
    if (v < 0) {
	*(cp++) = '-';
	/* need to use an unsigned because the largest negative number cannot 
	   be represented as a positive number */
	a = -v;
    } else {
	a = v;
    }
    /* build string of ascii digits, fixed width with leading zeros */
    for (n = 21; n >= 0; n--) {
	b = div_ten(a);
	cp[n] = '0' + ((unsigned int) (a - mul_ten(b)));
	a = b;
    }
    cp[22] = '\0';
    /* remove leading zeros */
    cp1 = cp;
    while (*cp == '0') {
	cp++;
    }
    /* check for special case, all zeros */
    if (*cp == '\0') {
	cp--;
    }
    while ((*(cp1++) = *(cp++)) != '\0');
}

static void timer_code(void *arg)
{
    long long int t1, t2, t3;
    long int tdiff1, tdiff2, tdiff3;
    char buf[30];

    while (1) {

	/* a simple counter... */
	timer_count++;
	/* print the results every second */
	if (timer_count % 1000 == 0) {
	    t1 = rtapi_get_time();
	    t2 = rtapi_get_time();
	    rtapi_delay(rtapi_delay_max());
	    t3 = rtapi_get_time();
	    tdiff1 = t2 - t1;
	    tdiff2 = t3 - t2;
	    tdiff3 = t3 - t1;
	    long_long_to_dec_str(buf, t1);
	    rtapi_print("T1 = %s\n", buf);
	    long_long_to_dec_str(buf, t2);
	    rtapi_print("T2 = %s\n", buf);
	    long_long_to_dec_str(buf, t3);
	    rtapi_print("T3 = %s\n", buf);
	    rtapi_print("Tdiff1 = %ld\n", tdiff1);
	    rtapi_print("Tdiff2 = %ld\n", tdiff2);
	    rtapi_print("Tdiff3 = %ld\n", tdiff3);
	}
	/* put the task to sleep until the next interrupt */
	rtapi_wait();
    }

    return;
}

/* part of the Linux kernel module that kicks off the timer task */
/* rtapi_app_main() is expanded to init_module() via a macro in
   rtapi_app.h */
int rtapi_app_main(void)
{
    int retval;
    int timer_prio;
    long period;

    module = rtapi_init("TIMERTASK");
    if (module < 0) {
	rtapi_print("timertask init: rtapi_init returned %d\n", module);
	return -1;
    }

    /* is timer started? if so, what period? */
    period = rtapi_clock_set_period(0);
    if (period == 0) {
	/* not running, start it */
	rtapi_print("timertask init: starting timer with period %ld\n",
	    TIMER_PERIOD_NSEC);
	period = rtapi_clock_set_period(TIMER_PERIOD_NSEC);
	if (period < 0) {
	    rtapi_print
		("timertask init: rtapi_clock_set_period failed with %ld\n",
		period);
	    rtapi_exit(module);
	    return -1;
	}
    }
    /* make sure period <= desired period (allow 1% roundoff error) */
    if (period > (TIMER_PERIOD_NSEC + (TIMER_PERIOD_NSEC / 100))) {
	/* timer period too long */
	rtapi_print("timertask init: clock period too long: %ld\n", period);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("timertask init: desired clock %ld, actual %ld\n",
	TIMER_PERIOD_NSEC, period);

    /* set the task priority to second lowest, since we only have one task */
    timer_prio = rtapi_prio_next_higher(rtapi_prio_lowest());

    /* create the timer task */
    /* the second arg is an abitrary int that is passed to the timer task on
       the first iterration */
    timer_task = rtapi_task_new(timer_code, 0 /* arg */ , timer_prio, module,
	TIMER_STACKSIZE, RTAPI_NO_FP);
    if (timer_task < 0) {
	/* See rtapi.h for the error codes returned */
	rtapi_print("timertask init: rtapi_task_new returned %d\n",
	    timer_task);
	rtapi_exit(module);
	return -1;
    }
    /* start the task running */
    retval = rtapi_task_start(timer_task, TASK_PERIOD_NSEC);
    if (retval < 0) {
	rtapi_print("timertask init: rtapi_task_start returned %d\n", retval);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("timertask init: started timer task\n");
    rtapi_print("timertask init: max delay = %ld\n", rtapi_delay_max());
    return 0;
}

/* part of the Linux kernel module that stops the timer task */
/* rtapi_app_exit() is substituted for cleanup_module() by a
   macro in rtapi_app.h */
void rtapi_app_exit(void)
{
    int retval;

    /* Stop the task */
    retval = rtapi_task_pause(timer_task);
    if (retval < 0) {
	rtapi_print("timertask exit: rtapi_task_pause returned %d\n", retval);
    }
    /* Remove the task from the list */

    retval = rtapi_task_delete(timer_task);
    if (retval < 0) {
	rtapi_print("timertask exit: rtapi_task_delete returned %d\n",
	    retval);
    }

    /* Print the final count just to show that the task did it's job */
    rtapi_print("timertask exit: timer count is %d\n", timer_count);
    /* Clean up and exit */
    rtapi_exit(module);

}
