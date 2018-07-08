//    Copyright 2003-2009, various authors
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
  extint.c

  External interrupt handler for parallel port interrupts. To trigger
  this, physically toggle the interrupt pin on the parallel port.
*/

#include "rtapi.h"
#include "rtapi_app.h"

#define PARPORT_BASE_ADDRESS 0x378
#define PARPORT_IRQ 7

static int module = 0;		/* the module ID */
static int timer_count = 0;	/* the output variable */

static void parport_irq_handler(void)
{
    timer_count++;

    rtapi_enable_interrupt(PARPORT_IRQ);

    return;
}

int rtapi_app_main(void)
{
    int retval;

    module = rtapi_init("EXTINT");
    if (module < 0) {
	rtapi_print("extint init: rtapi_init returned %d\n", module);
	return -1;
    }
    /* set up ISR */
    retval = rtapi_irq_new(PARPORT_IRQ, module, parport_irq_handler);
    if (retval < 0) {
	rtapi_print("extint init: rtapi_irq_new returned %d\n", retval);
	return -1;
    }
    retval = rtapi_enable_interrupt(PARPORT_IRQ);
    if (retval < 0) {
	rtapi_print("extint init: rtapi_enable_interrupt returned %d\n",
	    retval);
	return -1;
    }

    /* enable parallel port hardware interrupts */
    rtapi_outb(rtapi_inb(PARPORT_BASE_ADDRESS + 2) | 0x10,
	PARPORT_BASE_ADDRESS + 2);

    return 0;
}

void rtapi_app_exit(void)
{
    int retval;

    /* disable parallel port hardware interrupts */
    rtapi_outb(rtapi_inb(PARPORT_BASE_ADDRESS + 2) & (~0x10),
	PARPORT_BASE_ADDRESS + 2);

    /* clear ISR */
    retval = rtapi_disable_interrupt(PARPORT_IRQ);
    if (retval < 0) {
	rtapi_print("extint exit: rtapi_disable_interrupt returned %d\n",
	    retval);
	return;
    }
    retval = rtapi_irq_delete(PARPORT_IRQ);
    if (retval < 0) {
	rtapi_print("extint exit: rtapi_irq_delete returned %d\n", retval);
	return;
    }

    rtapi_print("extint exit: interrupt count is %d\n", timer_count);

    retval = rtapi_exit(module);
    if (retval < 0) {
	rtapi_print("extint exit: rtapi_exit returned %d\n", retval);
	return;
    }
}
