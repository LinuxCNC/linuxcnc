/** This file, 'parport.c', is a HAL component that provides a driver
    for the standard PC parallel port.  It is a realtime component.
    (It could be modified to work as a non-realtime component as well,
    but I don't feel like arguing with Linux's ioperm for permission
    to access the I/O ports.)
    It supports up to eight parallel ports, and if the port hardware
    is bidirectional, the eight data bits can be configured as inputs
    or outputs.
    The configuration is determined by a config string, passed into
    the module parameter 'cfg'.  The string format consists of a
    port address, followed by an optional direction, repeated for
    each port.  The direction is either "in" or "out" and determines
    the direction of the 8 bit data port.  The default is out.  The
    5 bits of the status port are always inputs, and the 4 bits of
    the control port are always outputs.  An example command line is:
                   insmod parport.o cfg="378 in 278"
    This installs the driver, and configures parports at base addresses
    0x0378 (using data port as input) and 0x0278 (using data port as
    output).
    The driver can optionally create a realtime thread, which is
    useful if a free-running driver is desired.  The module parameter
    'period' is a long int, corresponding to the thread period in
    nano-seconds.  If omitted, no thread will be created.
    The driver creates HAL pins for each port pin, and functions to
    read and write each port, as well as functions that read and write
    all the ports.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
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

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <linux/ctype.h>	/* isspace() */
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

#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Parallel Port Driver for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
static char *cfg = 0;		/* config string */
MODULE_PARM(cfg, "s");
MODULE_PARM_DESC(cfg, "config string");
static long period = 0;		/* thread period */
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "thread period (nsecs)");
#endif /* MODULE */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   parallel port driver for a single port
*/

typedef struct
{
    unsigned short base_addr;	/* base I/O address (0x378, etc.) */
    unsigned short data_dir;	/* non-zero if pins 2-9 are input */
    hal_bit_t *status_in[10];	/* ptrs for in pins 15, 13, 12, 10, 11 */
    hal_bit_t *data_in[16];	/* ptrs for input pins 2 - 9 */
    hal_bit_t *data_out[8];	/* ptrs for output pins 2 - 9 */
    hal_bit_t data_inv[8];	/* polarity params for output pins 2 - 9 */
    hal_bit_t *control_out[4];	/* ptrs for out pins 1, 14, 16, 17 */
    hal_bit_t control_inv[4];	/* pol. params for pins 1, 14, 16, 17 */
}
parport_t;

/* pointer to array of parport_t structs in shared memory, 1 per port */
static parport_t *port_data_array;

/* other globals */
static int comp_id;		/* component ID */
static int num_ports;		/* number of ports configured */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static void read_port(void *arg, long period);
static void write_port(void *arg, long period);
static void read_all(void *arg, long period);
static void write_all(void *arg, long period);

static unsigned short parse_port_addr(char *cp);
static int export_port(int portnum, parport_t * addr);
static int export_input_pin(int portnum, int pin, hal_bit_t ** base, int n);
static int export_output_pin(int portnum, int pin, hal_bit_t ** dbase,
    hal_bit_t * pbase, int n);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_PORTS 8
#define MAX_TOK ((MAX_PORTS*2)+2)

int rtapi_app_main(void)
{
    char *cp;
    char *tokens[MAX_TOK];
    unsigned short port_addr[MAX_PORTS];
    int data_dir[MAX_PORTS];
    char name[HAL_NAME_LEN + 2];
    int n, retval;

    /* test for config string */
    if (cfg == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "PARPORT: ERROR: no config string\n");
	return -1;
    }
    /* point to config string */
    cp = cfg;
    /* break it into tokens */
    for (n = 0; n < MAX_TOK; n++) {
	/* strip leading whitespace */
	while ((*cp != '\0') && (isspace(*cp)))
	    cp++;
	/* mark beginning of token */
	tokens[n] = cp;
	/* find end of token */
	while ((*cp != '\0') && (!isspace(*cp)))
	    cp++;
	/* mark end of this token, prepare to search for next one */
	if (*cp != '\0') {
	    *cp = '\0';
	    cp++;
	}
    }
    /* clear port_addr and data_dir arrays */
    for (n = 0; n < MAX_PORTS; n++) {
	port_addr[n] = 0;
	data_dir[n] = 0;
    }
    /* parse config string, results in port_addr[] and data_dir[] arrays */
    num_ports = 0;
    n = 0;
    while ((num_ports < MAX_PORTS) && (n < MAX_TOK)) {
	if (tokens[n][0] != '\0') {
	    /* something here, is it a port address? */
	    port_addr[num_ports] = parse_port_addr(tokens[n]);
	    if (port_addr[num_ports] == 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "PARPORT: ERROR: bad port address '%s'\n", tokens[n]);
		return -1;
	    }
	    /* is the next one 'in' or 'out' ? */
	    n++;
	    if ((tokens[n][0] == 'i') || (tokens[n][0] == 'I')) {
		/* we aren't picky, anything starting with 'i' means 'in' ;-) 
		 */
		data_dir[num_ports] = 1;
		n++;
	    } else if ((tokens[n][0] == 'o') || (tokens[n][0] == 'O')) {
		/* anything starting with 'o' means 'out' */
		data_dir[num_ports] = 0;
		n++;
	    }
	    num_ports++;
	} else {
	    n++;
	}
    }
    /* OK, now we've parsed everything */
    if (num_ports == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: no ports configured\n");
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("parport");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "PARPORT: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for parport data */
    port_data_array = hal_malloc(num_ports * sizeof(parport_t));
    if (port_data_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the pins and params for each port */
    for (n = 0; n < num_ports; n++) {
	/* config addr and direction */
	port_data_array[n].base_addr = port_addr[n];
	port_data_array[n].data_dir = data_dir[n];
	/* export all vars */
	retval = export_port(n + 1, &(port_data_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d var export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    /* export functions for each port */
    for (n = 0; n < num_ports; n++) {
	/* make read function name */
	rtapi_snprintf(name, HAL_NAME_LEN, "parport.%d.read", n + 1);
	/* export read function */
	retval = hal_export_funct(name, read_port, &(port_data_array[n]),
	    0, 0, comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d read funct export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* make write function name */
	rtapi_snprintf(name, HAL_NAME_LEN, "parport.%d.write", n + 1);
	/* export write function */
	retval = hal_export_funct(name, write_port, &(port_data_array[n]),
	    0, 0, comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d write funct export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    /* export functions that read and write all ports */
    retval = hal_export_funct("parport.read_all", read_all,
	port_data_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: read all funct export failed\n", n + 1);
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("parport.write_all", write_all,
	port_data_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: write all funct export failed\n", n + 1);
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"PARPORT: installed driver for %d ports\n", num_ports);
    /* was 'period' specified in the insmod command? */
    if (period > 0) {
	/* create a thread */
	retval = hal_create_thread("parport.thread", period, 0, comp_id);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: could not create thread\n");
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "PARPORT: created %d uS thread\n",
		period / 1000);
	}
    }
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                  REALTIME PORT READ AND WRITE FUNCTIONS              *
************************************************************************/

static void read_port(void *arg, long period)
{
    parport_t *port;
    int b;
    unsigned char indata, mask;

    port = arg;
    /* read the status port */
    indata = rtapi_inb(port->base_addr + 1);
    /* invert bit 7 (pin 11) to compensate for hardware inverter */
    indata ^= 0x80;
    /* split the bits into 10 variables (5 regular, 5 inverted) */
    mask = 0x08;
    for (b = 0; b < 10; b += 2) {
	*(port->status_in[b]) = indata & mask;
	*(port->status_in[b + 1]) = !(indata & mask);
	mask <<= 1;
    }
    /* are we using the data port for input? */
    if (port->data_dir != 0) {
	/* yes, read the data port */
	indata = rtapi_inb(port->base_addr);
	/* split the bits into 16 variables (8 regular, 8 inverted) */
	mask = 0x01;
	for (b = 0; b < 16; b += 2) {
	    *(port->data_in[b]) = indata & mask;
	    *(port->data_in[b + 1]) = !(indata & mask);
	    mask <<= 1;
	}
    }
}

static void write_port(void *arg, long period)
{
    parport_t *port;
    int b;
    unsigned char outdata, mask;

    port = arg;
    /* are we using the data port for output? */
    if (port->data_dir == 0) {
	/* yes */
	outdata = 0x00;
	mask = 0x01;
	/* assemble output byte for data port from 8 source variables */
	for (b = 0; b < 8; b++) {
	    /* get the data, add to output byte */
	    if ((*(port->data_out[b])) && (!port->data_inv[b])) {
		outdata |= mask;
	    }
	    if ((!*(port->data_out[b])) && (port->data_inv[b])) {
		outdata |= mask;
	    }
	    mask <<= 1;
	}
	/* write it to the hardware */
	rtapi_outb(outdata, port->base_addr);
	/* prepare to build control port byte, with direction bit clear */
	outdata = 0x00;
    } else {
	/* prepare to build control port byte, with direction bit set */
	outdata = 0x20;
    }
    mask = 0x01;
    /* assemble output byte for control port from 4 source variables */
    for (b = 0; b < 4; b++) {
	/* get the data, add to output byte */
	if ((*(port->control_out[b])) && (!port->control_inv[b])) {
	    outdata |= mask;
	}
	if ((!*(port->control_out[b])) && (port->control_inv[b])) {
	    outdata |= mask;
	}
	mask <<= 1;
    }
    /* correct for hardware inverters on pins 1, 14, & 17 */
    outdata ^= 0x0B;
    /* write it to the hardware */
    rtapi_outb(outdata, port->base_addr + 2);
}

void read_all(void *arg, long period)
{
    parport_t *port;
    int n;

    port = arg;
    for (n = 0; n < num_ports; n++) {
	read_port(&(port[n]), period);
    }
}

void write_all(void *arg, long period)
{
    parport_t *port;
    int n;

    port = arg;
    for (n = 0; n < num_ports; n++) {
	write_port(&(port[n]), period);
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static unsigned short parse_port_addr(char *cp)
{
    unsigned short result;

    /* initial value */
    result = 0;
    /* test for leading '0x' */
    if (cp[0] == '0') {
	if ((cp[1] == 'X') || (cp[1] == 'x')) {
	    /* leading '0x', skip it */
	    cp += 2;
	}
    }
    /* ok, now parse digits */
    while (*cp != '\0') {
	/* if char is a hex digit, add it to result */
	if ((*cp >= '0') && (*cp <= '9')) {
	    result <<= 4;
	    result += *cp - '0';
	} else if ((*cp >= 'A') && (*cp <= 'F')) {
	    result <<= 4;
	    result += (*cp - 'A') + 10;
	} else if ((*cp >= 'a') && (*cp <= 'f')) {
	    result <<= 4;
	    result += (*cp - 'a') + 10;
	} else {
	    /* not a valid hex digit */
	    return 0;
	}
	/* next char */
	cp++;
    }
    return result;
}

static int export_port(int portnum, parport_t * port)
{
    int retval;

    retval = 0;
    /* declare input pins (status port) */
    retval += export_input_pin(portnum, 15, port->status_in, 0);
    retval += export_input_pin(portnum, 13, port->status_in, 1);
    retval += export_input_pin(portnum, 12, port->status_in, 2);
    retval += export_input_pin(portnum, 10, port->status_in, 3);
    retval += export_input_pin(portnum, 11, port->status_in, 4);
    if (port->data_dir != 0) {
	/* declare input pins (data port) */
	retval += export_input_pin(portnum, 2, port->data_in, 0);
	retval += export_input_pin(portnum, 3, port->data_in, 1);
	retval += export_input_pin(portnum, 4, port->data_in, 2);
	retval += export_input_pin(portnum, 5, port->data_in, 3);
	retval += export_input_pin(portnum, 6, port->data_in, 4);
	retval += export_input_pin(portnum, 7, port->data_in, 5);
	retval += export_input_pin(portnum, 8, port->data_in, 6);
	retval += export_input_pin(portnum, 9, port->data_in, 7);
    } else {
	/* declare output pins (data port) */
	retval += export_output_pin(portnum, 2,
	    port->data_out, port->data_inv, 0);
	retval += export_output_pin(portnum, 3,
	    port->data_out, port->data_inv, 1);
	retval += export_output_pin(portnum, 4,
	    port->data_out, port->data_inv, 2);
	retval += export_output_pin(portnum, 5,
	    port->data_out, port->data_inv, 3);
	retval += export_output_pin(portnum, 6,
	    port->data_out, port->data_inv, 4);
	retval += export_output_pin(portnum, 7,
	    port->data_out, port->data_inv, 5);
	retval += export_output_pin(portnum, 8,
	    port->data_out, port->data_inv, 6);
	retval += export_output_pin(portnum, 9,
	    port->data_out, port->data_inv, 7);
    }
    /* declare output variables (control port) */
    retval += export_output_pin(portnum, 1,
	port->control_out, port->control_inv, 0);
    retval += export_output_pin(portnum, 14,
	port->control_out, port->control_inv, 1);
    retval += export_output_pin(portnum, 16,
	port->control_out, port->control_inv, 2);
    retval += export_output_pin(portnum, 17,
	port->control_out, port->control_inv, 3);
    return retval;
}

static int export_input_pin(int portnum, int pin, hal_bit_t ** base, int n)
{
    char buf[HAL_NAME_LEN + 2];
    int retval;

    /* export write only HAL pin for the input bit */
    rtapi_snprintf(buf, HAL_NAME_LEN, "parport.%d.pin-%02d-in", portnum, pin);
    retval = hal_pin_bit_new(buf, HAL_WR, base + (2 * n), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export another write only HAL pin for the same bit inverted */
    rtapi_snprintf(buf, HAL_NAME_LEN, "parport.%d.pin-%02d-in-not", portnum,
	pin);
    retval = hal_pin_bit_new(buf, HAL_WR, base + (2 * n) + 1, comp_id);
    return retval;
}

static int export_output_pin(int portnum, int pin, hal_bit_t ** dbase,
    hal_bit_t * pbase, int n)
{
    char buf[HAL_NAME_LEN + 2];
    int retval;

    /* export read only HAL pin for output data */
    rtapi_snprintf(buf, HAL_NAME_LEN, "parport.%d.pin-%02d-out", portnum,
	pin);
    retval = hal_pin_bit_new(buf, HAL_RD, dbase + n, comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for polarity */
    rtapi_snprintf(buf, HAL_NAME_LEN, "parport.%d.pin-%02d-out-invert",
	portnum, pin);
    retval = hal_param_bit_new(buf, pbase + n, comp_id);
    return retval;
}
