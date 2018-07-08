/********************************************************************
* Description:  hal_skeleton.c
*               This file, 'hal_skeleton.c', is a example that shows 
*               how drivers for HAL components will work and serve as 
*               a skeleton for new hardware drivers.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/

/** This file, 'hal_skeleton.c', is a example that shows how
 drivers for HAL components will work and serve as a skeleton
 for new hardware drivers.
 
 Most of this code is taken from the hal_parport driver from John Kasunich,
 which is also a good starting point for new drivers.

 This driver supports only for demonstration how to write a byte (char)
 to a hardware adress, here we use the parallel port (0x378).

 This driver support no configuration strings so installing is easy:
 realtime: halcmd loadrt hal_skeleton

 The driver creates a HAL pin and if it run in realtime a function
 as follows:

 Pin: 'skeleton.<portnum>.pin-<pinnum>-out'
 Function: 'skeleton.<portnum>.write'

 This skeleton driver also doesn't use arguments you can pass to the driver
 at startup. Please look at the parport driver how to implement this if you need
 this for your driver.

 (added 17 Nov 2006)
 The approach used for writing HAL drivers has evolved quite a bit over the
 three years since this was written.  Driver writers should consult the HAL
 User Manual for information about canonical device interfaces, and should
 examine some of the more complex drivers, before using this as a basis for
 a new driver.

*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
                       Martin Kuhnle
                       <mkuhnle AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */

#include "hal.h"		/* HAL public API decls */

/* If FASTIO is defined, uses outb() and inb() from <asm.io>,
   instead of rtapi_outb() and rtapi_inb() - the <asm.io> ones
   are inlined, and save a microsecond or two (on my 233MHz box)
*/
#define FASTIO

#ifdef FASTIO
#define rtapi_inb inb
#define rtapi_outb outb
#include <asm/io.h>
#endif

/* module information */
MODULE_AUTHOR("Martin Kuhnle");
MODULE_DESCRIPTION("Test Driver for ISA-LED Board for EMC HAL");
MODULE_LICENSE("GPL");
/* static char *cfg = 0; */
/* config string
RTAPI_MP_STRING(cfg, "config string"); */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   driver for a single port/channel
*/

typedef struct {
    hal_u32_t *data_out;		/* ptrs for output */
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

#define MAX_TOK ((MAX_PORTS*2)+3)

int rtapi_app_main(void)
{
    char name[HAL_NAME_LEN + 1];
    int n, retval;

    /* only one port at the moment */
    num_ports = 1;
    n = 0; /* port number */

    /* STEP 1: initialise the driver */
    comp_id = hal_init("hal_skeleton");
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
    retval = hal_pin_u32_newf(HAL_IN, &(port_data_array->data_out),
			     comp_id, "skeleton.%d.pin-%02d-out", n, 1);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: port %d var export failed with err=%i\n", n,
	    retval);
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 4: export write function */
    rtapi_snprintf(name, sizeof(name), "skeleton.%d.write", n);
    retval = hal_export_funct(name, write_port, &(port_data_array[n]), 0, 0,
	comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SKELETON: ERROR: port %d write funct export failed\n", n);
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"SKELETON: installed driver for %d ports\n", num_ports);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/**************************************************************
* REALTIME PORT WRITE FUNCTION                                *
**************************************************************/

static void write_port(void *arg, long period)
{
    skeleton_t *port;
    unsigned char outdata;
    port = arg;

    outdata = *(port->data_out) & 0xFF;
    /* write it to the hardware */
    rtapi_outb(outdata, 0x378);
}
