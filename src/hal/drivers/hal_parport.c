/** This file, 'hal_parport.c', is a HAL component that provides a
    driver for the standard PC parallel port.

    It supports up to eight parallel ports, and if the port hardware
    is bidirectional, the eight data bits can be configured as inputs
    or outputs.

    The configuration is determined by command line arguments for the
    user space version of the driver, and by a config string passed
    to insmod for the realtime version.  The format is similar for
    both, and consists of a port address, followed by an optional
    direction, repeated for each port.  The direction is either "in"
    or "out" and determines the direction of the 8 bit data port.
    The default is out.  The 5 bits of the status port are always
    inputs, and the 4 bits of the control port are always outputs.
    Example command lines are as follows:

    user:        hal_parport 378 in 278
    realtime:    insmod hal_parport.o cfg="378 in 278"

    Both of these commands install the driver and configure parports
    at base addresses 0x0378 (using data port as input) and 0x0278
    (using data port as output).

    The driver creates HAL pins and parameters for each port pin
    as follows:
    Each physical output has a correspinding HAL pin, named
    'parport.<portnum>.pin-<pinnum>-out', and a HAL parameter
    'parport.<portnum>.pin-<pinnum>-out-invert'.
    Each physical input has two corresponding HAL pins, named
    'parport.<portnum>.pin-<pinnum>-in' and
    'parport.<portnum>.pin-<pinnum>-in-not'.

    <portnum> is the port number, starting from zero.  <pinnum> is
    the physical pin number on the DB-25 connector.

    The realtime version of the driver exports two HAL functions for
    each port, 'parport.<portnum>.read' and 'parport.<portnum>.write'.
    It also exports two additional functions, 'parport.read_all' and
    'parport.write_all'.  Any or all of these functions can be added
    to realtime HAL threads to update the port data periodically.

    The user space version of the driver cannot export functions,
    instead it exports parameters with the same names.  The main()
    function sits in a loop checking the parameters.  If they are
    zero, it does nothing.  If any parameter is greater than zero,
    the corresponding function runs once, then the parameter is
    reset to zero.  If any parameter is less than zero, the
    corresponding function runs on every pass through the loop.
    The driver will loop forever, until it receives either
    SIGINT (ctrl-C) or SIGTERM, at which point it cleans up and
    exits.

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
#include <sys/time.h>		/* stuct timeval */
#include <sys/types.h>		/* for select() */
#include <unistd.h>		/* for select() */
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
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Parallel Port Driver for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
static char *cfg = 0;		/* config string */
MODULE_PARM(cfg, "s");
MODULE_PARM_DESC(cfg, "config string");
#endif /* MODULE */
#endif /* RTAPI */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   parallel port driver for a single port
*/

typedef struct {
    unsigned short base_addr;	/* base I/O address (0x378, etc.) */
    unsigned short data_dir;	/* non-zero if pins 2-9 are input */
    hal_bit_t *status_in[10];	/* ptrs for in pins 15, 13, 12, 10, 11 */
    hal_bit_t *data_in[16];	/* ptrs for input pins 2 - 9 */
    hal_bit_t *data_out[8];	/* ptrs for output pins 2 - 9 */
    hal_bit_t data_inv[8];	/* polarity params for output pins 2 - 9 */
    hal_bit_t *control_out[4];	/* ptrs for out pins 1, 14, 16, 17 */
    hal_bit_t control_inv[4];	/* pol. params for pins 1, 14, 16, 17 */
} parport_t;

/* pointer to array of parport_t structs in shared memory, 1 per port */
static parport_t *port_data_array;

/* other globals */
static int comp_id;		/* component ID */
static int num_ports;		/* number of ports configured */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/* These are the functions that actually do the I/O
   everything else is just init code
*/

static void read_port(void *arg, long period);
static void write_port(void *arg, long period);
static void read_all(void *arg, long period);
static void write_all(void *arg, long period);

/* 'pins_and_params()' does most of the work involved in setting up
   the driver.  It parses the command line (argv[]), then if the
   command line is OK, it calls hal_init(), allocates shared memory
   for the parport_t data structure(s), and exports pins and parameters
   It does not set up functions, since that is handled differently in
   realtime and user space.
*/
static int pins_and_params(char *argv[]);

static unsigned short parse_port_addr(char *cp);
static int export_port(int portnum, parport_t * addr);
static int export_input_pin(int portnum, int pin, hal_bit_t ** base, int n);
static int export_output_pin(int portnum, int pin, hal_bit_t ** dbase,
    hal_bit_t * pbase, int n);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_PORTS 8

#ifdef RTAPI			/* realtime */

#define MAX_TOK ((MAX_PORTS*2)+3)

int rtapi_app_main(void)
{
    char *cp;
    char *argv[MAX_TOK];
    char name[HAL_NAME_LEN + 2];
    int n, retval;

    /* test for config string */
    if (cfg == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "PARPORT: ERROR: no config string\n");
	return -1;
    }
    /* as a RT module, we don't get a nice argc/argv command line, we only
       get a single string... so we need to tokenize it ourselves */
    cp = cfg;
    for (n = 0; n < MAX_TOK; n++) {
	/* strip leading whitespace */
	while ((*cp != '\0') && (isspace(*cp)))
	    cp++;
	/* mark beginning of token */
	argv[n] = cp;
	/* find end of token */
	while ((*cp != '\0') && (!isspace(*cp)))
	    cp++;
	/* mark end of this token, prepare to search for next one */
	if (*cp != '\0') {
	    *cp = '\0';
	    cp++;
	}
    }
    for (n = 0; n < MAX_TOK; n++) {
	/* is token empty? */
	if (argv[n][0] == '\0') {
	    /* yes - make pointer NULL */
	    argv[n] = NULL;
	}
    }
    /* parse "command line", set up pins and parameters */
    retval = pins_and_params(argv);
    if (retval != 0) {
	return retval;
    }
    /* export functions for each port */
    for (n = 0; n < num_ports; n++) {
	/* make read function name */
	rtapi_snprintf(name, HAL_NAME_LEN, "parport.%d.read", n);
	/* export read function */
	retval = hal_export_funct(name, read_port, &(port_data_array[n]),
	    0, 0, comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d read funct export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
	/* make write function name */
	rtapi_snprintf(name, HAL_NAME_LEN, "parport.%d.write", n);
	/* export write function */
	retval = hal_export_funct(name, write_port, &(port_data_array[n]),
	    0, 0, comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d write funct export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    /* export functions that read and write all ports */
    retval = hal_export_funct("parport.read_all", read_all,
	port_data_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: read all funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("parport.write_all", write_all,
	port_data_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: write all funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"PARPORT: installed driver for %d ports\n", num_ports);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

#else /* user space */

static int done = 0;
static void quit(int sig)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    char name[HAL_NAME_LEN + 2];
    int n, retval;
    hal_s8_t *read_funct_flags;
    hal_s8_t *write_funct_flags;
    struct timeval tv;

    /* ask linux for permission to use the I/O ports */
    retval = iopl(3);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: could not get I/O permission\n");
	return -1;
    }
    /* parse command line, set up pins and parameters */
    retval = pins_and_params(&(argv[1]));
    if (retval != 0) {
	return retval;
    }
    /* allocate space for function run/stop parameters */
    read_funct_flags = hal_malloc((num_ports + 1) * sizeof(hal_s8_t) * 2);
    if (read_funct_flags == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    write_funct_flags = read_funct_flags + (num_ports + 1);
    /* export function run/stop parameters for each port */
    for (n = 0; n < num_ports; n++) {
	/* make read function name */
	rtapi_snprintf(name, HAL_NAME_LEN, "parport.%d.read", n);
	/* export read function parameter */
	retval =
	    hal_param_s8_new(name, HAL_RD_WR, &read_funct_flags[n + 1],
	    comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d read funct param failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
	/* make write function name */
	rtapi_snprintf(name, HAL_NAME_LEN, "parport.%d.write", n);
	/* export read function parameter */
	retval =
	    hal_param_s8_new(name, HAL_RD_WR, &write_funct_flags[n + 1],
	    comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d write funct param failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    /* export parameters for read/write all port functuons */
    retval =
	hal_param_s8_new("parport.read_all", HAL_RD_WR, &read_funct_flags[0],
	comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: read all funct param failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval =
	hal_param_s8_new("parport.write_all", HAL_RD_WR,
	&write_funct_flags[0], comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: write all funct param failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"PARPORT: installed driver for %d ports\n", num_ports);
    /* capture INT (ctrl-C) and TERM signals */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    /*************************************/
    /* main loop - loops forever until */
    /* SIGINT (ctrl-C) or SIGTERM (kill) */
    /*************************************/
    while (!done) {
	if (read_funct_flags[0]) {
	    /* run the function */
	    read_all(port_data_array, 0);
	    /* if flag is positive, reset it */
	    if (read_funct_flags[0] > 0) {
		read_funct_flags[0] = 0;
	    }
	}
	for (n = 0; n < num_ports; n++) {
	    if (read_funct_flags[n + 1]) {
		/* run the function */
		read_port(&(port_data_array[n]), 0);
		/* if flag is positive, reset it */
		if (read_funct_flags[n + 1] > 0) {
		    read_funct_flags[n + 1] = 0;
		}
	    }
	}
	if (write_funct_flags[0]) {
	    /* run the function */
	    write_all(port_data_array, 0);
	    /* if flag is positive, reset it */
	    if (write_funct_flags[0] > 0) {
		write_funct_flags[0] = 0;
	    }
	}
	for (n = 0; n < num_ports; n++) {
	    if (write_funct_flags[n + 1]) {
		/* run the function */
		write_port(&(port_data_array[n]), 0);
		/* if flag is positive, reset it */
		if (write_funct_flags[n + 1] > 0) {
		    write_funct_flags[n + 1] = 0;
		}
	    }
	}
	/* set timeout to 0.05 seconds (20 Hz update rate if nothing else is
	   running) */
	tv.tv_sec = 0;
	tv.tv_usec = 50000;
	/* call select() with no file descriptors and a timeout to yield the
	   CPU for (at least) 0,05 seconds - see NOTES section of man 2
	   select for details */
	select(0, 0, 0, 0, &tv);
    }
    hal_exit(comp_id);
    return 0;
}

#endif

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

static int pins_and_params(char *argv[])
{
    unsigned short port_addr[MAX_PORTS];
    int data_dir[MAX_PORTS];
    int n, retval;

    /* clear port_addr and data_dir arrays */
    for (n = 0; n < MAX_PORTS; n++) {
	port_addr[n] = 0;
	data_dir[n] = 0;
    }
    /* parse config string, results in port_addr[] and data_dir[] arrays */
    num_ports = 0;
    n = 0;
    while ((num_ports < MAX_PORTS) && (argv[n] != 0)) {
	port_addr[num_ports] = parse_port_addr(argv[n]);
	if (port_addr[num_ports] == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: bad port address '%s'\n", argv[n]);
	    return -1;
	}
	n++;
	if (argv[n] != 0) {
	    /* is the next token 'in' or 'out' ? */
	    if ((argv[n][0] == 'i') || (argv[n][0] == 'I')) {
		/* we aren't picky, anything starting with 'i' means 'in' ;-)
		 */
		data_dir[num_ports] = 1;
		n++;
	    } else if ((argv[n][0] == 'o') || (argv[n][0] == 'O')) {
		/* anything starting with 'o' means 'out' */
		data_dir[num_ports] = 0;
		n++;
	    }
	}
	num_ports++;
    }
    /* OK, now we've parsed everything */
    if (num_ports == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PARPORT: ERROR: no ports configured\n");
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("hal_parport");
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
	retval = export_port(n, &(port_data_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PARPORT: ERROR: port %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    return 0;
}

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
    int retval, msg;

    /* This function exports a lot of stuff, which results
       in a lot of logging if msg_level is at INFO or ALL.
       So we save the current value of msg_level and restore
       it later.  If you actually need to log this function's
       actions, change the second line below
    */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

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
    /* restore saved message level */
    rtapi_set_msg_level(msg);
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
    retval = hal_param_bit_new(buf, HAL_WR, pbase + n, comp_id);
    return retval;
}
