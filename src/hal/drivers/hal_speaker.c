
/********************************************************************
* Description:  hal_speaker.c
*               This file, 'hal_speaker.c', is a example that shows 
*               how drivers for HAL components will work and serve as 
*               a skeleton for new hardware drivers.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
# $Revision$
* $Author$
* $Date$
********************************************************************/

/** This file, 'hal_speaker.c', is a example that shows how
 drivers for HAL components will work and serve as a skeleton
 for new hardware drivers.
 
 Most of this code is taken from the hal_parport driver from John Kasunich,
 which is also a good starting point for new drivers.

 This driver supports only for demonstration how to write a byte (char)
 to a hardware adress, here we use the parallel port (0x378).

 This driver support no configuration strings so installing is easy:
 user: hal_speaker
 realtime: insmod hal_speaker

 The driver creates a HAL pin and if it run in realtime a function
 as follows:

 Pin: 'speaker.<portnum>.pin-<pinnum>-out'
 Function: 'speaker.<portnum>.write'

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
MODULE_AUTHOR("Jeff Epler");
MODULE_DESCRIPTION("PC Speaker Driver");
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
    hal_bit_t *signals[8];
    uint8_t last;
} speaker_t;

/* pointer to array of speaker_t structs in shared memory, 1 per port */
static speaker_t *port_data_array;

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
    int i, n, retval;

    /* only one port at the moment */
    num_ports = 1;
    n = 0; /* port number */

    /* STEP 1: initialise the driver */
    comp_id = hal_init("hal_speaker");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SPEAKER: ERROR: hal_init() failed\n");
	return -1;
    }

    /* STEP 2: allocate shared memory for skeleton data */
    port_data_array = hal_malloc(num_ports * sizeof(speaker_t));
    if (port_data_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SPEAKER: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 3: export the pin(s) */
    for(i = 0; i < 8; i++) {
        rtapi_snprintf(name, HAL_NAME_LEN, "speaker.%d.pin-%02d-out", n, i);
        retval = hal_pin_bit_new(
                name, HAL_RD, &(port_data_array->signals[i]), comp_id);
        if (retval != HAL_SUCCESS) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "SPEAKER: ERROR: port %d var export failed with err=%i\n", n,
                retval);
            hal_exit(comp_id);
            return -1;
        }
    }

    /* STEP 4: export write function */
    rtapi_snprintf(name, HAL_NAME_LEN, "speaker.%d.write", n);
    retval =
	hal_export_funct(name, write_port, &(port_data_array[n]), 0, 0,
	comp_id);
    if (retval != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SPEAKER: ERROR: port %d write funct export failed\n", n);
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"SPEAKER: installed driver for %d ports\n", num_ports);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

#else
#error Only realtime is supported
#endif

/**************************************************************
* REALTIME PORT WRITE FUNCTION                                *
**************************************************************/

static void write_port(void *arg, long period)
{
    uint8_t v = 0;
    uint8_t oldval;
    int i;
    speaker_t *port;
    port = arg;
    
    for(i=0; i<8; i++) {
        if(*(port->signals[i])) v = v | (1<<i);
    }

    /* write it to the hardware */
    oldval = rtapi_inb(0x61) & 0xfc;

    if(v != port->last) {
        rtapi_outb(oldval | 2, 0x61);
    } else {
        rtapi_outb(oldval, 0x61);
    }

    port->last = v;
}
