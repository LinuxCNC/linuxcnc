/** This file, 'hal_skeleton.c', is a example that shows how
 drivers for HAL components will work and serve as a skeleton
 for new hardware drivers.
 
 Most of this code is taken from the hal_parport driver from John Kasunich,
 which is also a good starting point for new drivers.

 This driver supports only for demonstration how to write a byte (char)
 to a hardware adress, here we use the parallel port (0x378).

 This driver support no configuration strings so installing is easy:
 user: hal_skeleton
 realtime: insmod hal_skeleton

 The driver creates a HAL pin and if it run in realtime a function
 as follows:

 Pin: 'skeleton.<portnum>.pin-<pinnum>-out'
 Function: 'skeleton.<portnum>.write'

    The user space version of the driver cannot export functions,
    instead it exports a parameters with the same name.  The main()
    function sits in a loop checking the parameters.  If it is
    zero, it does nothing.  If the parameter is greater than zero,
    the function runs once, then the parameter is reset to zero.
    If the parameter is less than zero, the function runs on every
    pass through the loop.
    The driver will loop forever, until it receives either
    SIGINT (ctrl-C) or SIGTERM, at which point it cleans up and
    exits.

 You can see that you need at minimum four steps for installing the driver.
 The steps 1-3 are in realtime and userspace exactly the same, so it is highly
 recommended that you put this steps into a function.
 Here this is not done because so the code is easier to read.
 
 This skeleton driver also doesn't use arguments you can pass to the driver
 at startup. Please look at the parport driver how to implement this if you need
 this for your driver.

*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
                       Martin Kuhnle
                       <mkuhnle AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#if ( !defined RTAPI ) && ( !defined ULAPI )
#error parport needs RTAPI/ULAPI, check makefile and flags
#endif

#ifdef RTAPI			/* realtime */
#include <linux/ctype.h>	/* isspace() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#else /* user space */
#include <ctype.h>		/* isspace() */
#include <signal.h>		/* signal() */
#include <sched.h>		/* sched_yield() */
#include <sys/io.h>		/* iopl() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#endif

#include "hal.h"		/* HAL public API decls */

/* If FASTIO is defined, uses outb() and inb() from <asm.io>,
   instead of rtapi_outb() and rtapi_inb() - the <asm.io> ones
   are inlined, and save a microsecond or two (on my 233MHz box)
*/
#define FASTIO

#ifdef FASTIO
#define rtapi_inb inb
#define rtapi_outb outb
#ifdef RTAPI			/* for ULAPI, sys/io.h defines these functs */
#include <asm/io.h>
#endif
#endif

#ifdef RTAPI			/* realtime */
#ifdef MODULE
/* module information */
MODULE_AUTHOR("Martin Kuhnle");
MODULE_DESCRIPTION("Test Driver for ISA-LED Board for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
/* static char *cfg = 0; */
/* config string
MODULE_PARM(cfg, "s");
MODULE_PARM_DESC(cfg, "config string"); */
#endif /* MODULE */
#endif /* RTAPI */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   driver for a single port/channel
*/

typedef struct {
    hal_u8_t *data_out;		/* ptrs for output */
} skeleton_t;

/* pointer to array of skeleton_t structs in shared memory, 1 per port */
static skeleton_t *port_data_array;

/* other globals */
static int comp_id;		/* component ID */
static int num_ports;		/* number of ports configured */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
/* These is the functions that actually do the I/O
   everything else is just init code
*/
static void write_port(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_PORTS 8

#ifdef RTAPI			/* realtime part of skeleton driver */

#define MAX_TOK ((MAX_PORTS*2)+3)

int rtapi_app_main(void)
{
    char name[HAL_NAME_LEN + 2];
    int n, retval;

    /* only one port at the moment */
    num_ports = 1;
    n = 0;

    /* STEP 1: initialise the driver */
    comp_id = hal_init("SKELETON");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: hal_init() failed\n");
	return -1;
    }

    /* STEP 2: allocate shared memory for skeleton data */
    port_data_array = hal_malloc(num_ports * sizeof(skeleton_t));
    if (port_data_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 3: export the pin(s) */
    rtapi_snprintf(name, HAL_NAME_LEN, "skeleton.%d.pin-%02d-out", 1, 1);
    retval =
	hal_pin_u8_new(name, HAL_RD, &(port_data_array->data_out), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: port %d var export failed with err=%i\n", n + 1,
	    retval);
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 4: export write function */
    rtapi_snprintf(name, HAL_NAME_LEN, "skeleton.%d.write", n + 1);
    retval =
	hal_export_funct(name, write_port, &(port_data_array[n]), 0, 0,
	comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: port %d write funct export failed\n", n + 1);
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"SKELETON: installed driver for %d ports\n", num_ports);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

#else
/******************** user space part of skeleton driver ********************/

static int done = 0;
static void quit(int sig)
{
    done = 1;
}

int main()
{
    char name[HAL_NAME_LEN + 2];
    int n, retval;
    hal_s8_t *write_funct_flags;
    num_ports = 1;
    n = 0;

    /* ask linux for permission to use the I/O ports */
    retval = iopl(3);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: could not get I/O permission\n");
	return -1;
    }

    /* STEP 1: initialise the driver */
    comp_id = hal_init("SKELETON");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: hal_init() failed\n");
	return -1;
    }

    /* STEP 2: allocate shared memory for skeleton data */
    port_data_array = hal_malloc(num_ports * sizeof(skeleton_t));
    if (port_data_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 3: export the pin(s) */
    rtapi_snprintf(name, HAL_NAME_LEN, "skeleton.%d.pin-%02d-out", 1, 1);
    retval =
	hal_pin_u8_new(name, HAL_RD, &(port_data_array->data_out), comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: port %d var export failed with err=%i\n", n + 1,
	    retval);
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 4a: allocate space for function run/stop parameter(s) */
    write_funct_flags = hal_malloc((num_ports + 1) * sizeof(hal_s8_t));
    if (write_funct_flags == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 4b: export read function parameter */
    rtapi_snprintf(name, HAL_NAME_LEN, "skeleton.%d.write", n + 1);
    retval =
	hal_param_s8_new(name, HAL_RD, &write_funct_flags[n + 1], comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: port %d write funct param failed\n", n + 1);
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"SKELETON: installed driver for %d ports\n", num_ports);

    /* capture INT (ctrl-C) and TERM signals */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    /*********************************************/
    /* STEP 6: main loop - loops forever until */
    /* SIGINT (ctrl-C) or SIGTERM (kill) */
    /*********************************************/
    while (!done) {

	n = 0;
	if (write_funct_flags[n + 1]) {
	    /* run the function */
	    write_port(&(port_data_array[n]), 0);
	    /* if flag is positive, reset it */
	    if (write_funct_flags[n + 1] > 0) {
		write_funct_flags[n + 1] = 0;
	    }
	}

	/* give up the CPU so other processes can run */
	sched_yield();
    }
    hal_exit(comp_id);
    return 0;
}

#endif

/**************************************************************
* REALTIME PORT WRITE FUNCTION                                *
**************************************************************/

static void write_port(void *arg, long period)
{
    skeleton_t *port;
    port = arg;

    /* write it to the hardware */
    rtapi_outb(*(port->data_out), 0x378);
}
