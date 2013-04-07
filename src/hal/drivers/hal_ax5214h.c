/********************************************************************
* Description:  AX5214H.c
*               This file, 'AX5214H.c', is a HAL component that 
*               provides a driver for the Axiom Measurement & Control 
*               AX5241H 48 channel digital I/O board.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2005 All rights reserved.
*
* Last change: 
********************************************************************/
/** This file, 'AX5214H.c', is a HAL component that provides a
    driver for the Axiom Measurement & Control AX5241H 48 channel
    digital I/O board.

    The configuration is determined by a config string passed to 
    insmod when loading the module.  The format consists of a base
    address, followed by a eight character string that sets the
    direction of each group of pins, repeated for each card (if 
    more than one card is used).   Each character of the direction
    string is either "I" or "O".  The first character sets the
    direction of port A on channel 1 (Port 1A), the next sets 
    port B on channel 1 (port 1B), the next sets the low nibble
    of port C on channel 1 (port 1CL), and the fourth sets the 
    high nibble of port C on channel 1 (port 1CH).  The next four
    characters do the same thing for channel 2 (ports 2A, 2B, 
    2CL, and 2CH).

    example:    insmod AX5214.o cfg="0x220 IIIOIIOO"
    
    The example above is for one card, with its base address 
    set to hex 220, and with 36 channels of input (Ports 1A, 
    1B, 1CL, 2A, and 2B) and 12 channels of output (Ports 1CH,
    2CL, and 2CH).

    The driver creates HAL pins and parameters for each port pin
    as follows:
    Each physical output has a correspinding HAL pin, named
    'ax5214.<boardnum>.out-<pinnum>', and a HAL parameter
    'ax5214.<boardnum>.out-<pinnum>-invert'.
    Each physical input has two corresponding HAL pins, named
    'ax5214.<boardnum>.in-<pinnum>' and
    'ax5214.<boardnum>.in-<pinnum>-not'.

    <boardnum> is the board number, starting from zero.  
    <pinnum> is the pin number, from 0 to 47.
    
    Note that the driver assumes active LOW signals.  This
    is so that modules such as OPTO-22 will work correctly
    (TRUE means output ON, or input energized).  If the 
    signals are being used directly without buffering or
    isolation the inversion needs to be accounted for.

    The driver exports two HAL functions for each board, 
    'ax5214.<boardnum>.read' and 'ax5214.<boardnum>.write'.

*/

/** Copyright (C) 2005 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
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

#include "config.h"
#include "rtapi_ctype.h"	/* isspace() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#ifdef BUILD_SYS_USER_DSO
#include <sys/io.h> 
#else
#include <asm/io.h>
#endif

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Axiom AX5214H Driver for HAL");
MODULE_LICENSE("GPL");
static char *cfg = "";	/* config string, default no boards */
RTAPI_MP_STRING(cfg, "config string");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   driver for a single board
*/

typedef struct {
    hal_bit_t *data;		/* basic pin for input or output */
    union {
	hal_bit_t *not;		/* pin for inverted data (input only) */
	hal_bit_t invert;	/* param for inversion (output only) */
	} io;
} io_pin_t;


typedef struct {
    unsigned short base_addr;	/* base I/O address (0x220, etc.) */
    unsigned char dir_bits;	/* LSB is port 1A, MSB is port 2CH, */
				/*   1 means output, 0 means input */
    unsigned char port1config;	/* config register value for port 1 */
    unsigned char port2config;	/* config register value for port 1 */
    io_pin_t port_1A[8];
    io_pin_t port_1B[8];
    io_pin_t port_1CL[4];
    io_pin_t port_1CH[4];
    io_pin_t port_2A[8];
    io_pin_t port_2B[8];
    io_pin_t port_2CL[4];
    io_pin_t port_2CH[4];
} board_t;

/* pointer to array of board_t structs in shared memory, 1 per board */
static board_t *board_array;

/* other globals */
static int comp_id;		/* component ID */
static int num_boards;		/* number of ports configured */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/* These are the functions that actually do the I/O
   everything else is just init code
*/

static void read_board(void *arg, long period);
static void write_board(void *arg, long period);

/* 'pins_and_params()' does most of the work involved in setting up
   the driver.  It parses the command line (argv[]), then if the
   command line is OK, it calls hal_init(), allocates shared memory
   for the parport_t data structure(s), and exports pins and parameters
   It does not set up functions, since that is handled differently in
   realtime and user space.
*/
static int pins_and_params(char *argv[]);

static unsigned short parse_board_addr(char *cp);

static int export_board(int boardnum, board_t * board);
static int export_port(int boardnum, int pin_num, io_pin_t *pin, int num_pins, int dir);
static int export_input_pin(int boardnum, int pinnum, io_pin_t *pin);
static int export_output_pin(int boardnum, int pinnum, io_pin_t *pin);


/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_BOARDS 8

#define MAX_TOK ((MAX_BOARDS*2)+3)

int rtapi_app_main(void)
{
    char *cp;
    char *argv[MAX_TOK];
    char name[HAL_NAME_LEN + 1];
    int n, retval;

    /* test for config string */
    if ((cfg == 0) || (cfg[0] == '\0')) {
	rtapi_print_msg(RTAPI_MSG_ERR, "AX5214H: ERROR: no config string\n");
	return -1;
    }
    /* as a RT module, we don't get a nice argc/argv command line, we only
       get a single string... so we need to tokenize it ourselves */
    /* and to make things worse, it seems that insmod under kernel 2.6
       ends the config string at the first space, so I added the ability
       to use '_' as a token separator.  What an ugly hack...  HAL needs
       a better way to handle insmod time config data */
    cp = cfg;
    for (n = 0; n < MAX_TOK; n++) {
	/* strip leading whitespace or token separators */
	while ((*cp != '\0') && ( isspace(*cp) || ( *cp == '_') ))
	    cp++;
	/* mark beginning of token */
	argv[n] = cp;
	/* find end of token */
	while ((*cp != '\0') && !( isspace(*cp) || ( *cp == '_') ))
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
    /* export functions for each board */
    for (n = 0; n < num_boards; n++) {
	/* make read function name */
	rtapi_snprintf(name, sizeof(name), "ax5214h.%d.read", n);
	/* export read function */
	retval = hal_export_funct(name, read_board, &(board_array[n]),
	    0, 0, comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"AX5214H: ERROR: port %d read funct export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
	/* make write function name */
	rtapi_snprintf(name, sizeof(name), "ax5214h.%d.write", n);
	/* export write function */
	retval = hal_export_funct(name, write_board, &(board_array[n]),
	    0, 0, comp_id);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"AX5214H: ERROR: port %d write funct export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"AX5214H: installed driver for %d boards\n", num_boards);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    int n;
    board_t *board;
    
    for ( n = 0 ; n < num_boards ; n++ ) {
	board = &(board_array[n]);
	/* reset all outputs to high/off */
	outb(0xff, board->base_addr+0);
	outb(0xff, board->base_addr+1);
	outb(0xff, board->base_addr+2);
	outb(0xff, board->base_addr+3);
	outb(0xff, board->base_addr+4);
	outb(0xff, board->base_addr+5);
	outb(0xff, board->base_addr+6);
	outb(0xff, board->base_addr+7);
    }
    hal_exit(comp_id);
}


/***********************************************************************
*                  REALTIME PORT READ AND WRITE FUNCTIONS              *
************************************************************************/


static void split_input(unsigned char data, io_pin_t *dest, int num)
{
    int b;
    unsigned char mask;

    /* splits a byte into 'num' HAL pins (and their NOTs) */
    mask = 0x01;
    for (b = 0 ; b < num ; b++ ) {
	if ( data & mask ) {
	    /* input high, which means FALSE (active low) */
	    *(dest->data) = 0;
	    *(dest->io.not) = 1;
	} else {
	    /* input low, which means TRUE */
	    *(dest->data) = 1;
	    *(dest->io.not) = 0;
	}
	mask <<= 1;
	dest++;
    }
}    

static void read_board(void *arg, long period)
{
    board_t *board;
    unsigned char indata;
    
    board = arg;
    if ( (board->dir_bits & 0x01) == 0 ) {
	indata = rtapi_inb(board->base_addr+0);
	split_input(indata, &(board->port_1A[0]), 8);
    }
    if ( (board->dir_bits & 0x02) == 0 ) {
	indata = rtapi_inb(board->base_addr+1);
	split_input(indata, &(board->port_1B[0]), 8);
    }
    if ( (board->dir_bits & 0x0A) != 0x0A ) {
	indata = rtapi_inb(board->base_addr+2);
	if ( (board->dir_bits & 0x04) == 0 ) {
	    split_input(indata, &(board->port_1CL[0]), 4);
	}
	indata >>= 4;
	if ( (board->dir_bits & 0x08) == 0 ) {
	    split_input(indata, &(board->port_1CH[0]), 4);
	}
    }
    if ( (board->dir_bits & 0x10) == 0 ) {
	indata = rtapi_inb(board->base_addr+4);
	split_input(indata, &(board->port_2A[0]), 8);
    }
    if ( (board->dir_bits & 0x20) == 0 ) {
	indata = rtapi_inb(board->base_addr+5);
	split_input(indata, &(board->port_2B[0]), 8);
    }
    if ( (board->dir_bits & 0xA0) != 0xA0 ) {
	indata = rtapi_inb(board->base_addr+6);
	if ( (board->dir_bits & 0x40) == 0 ) {
	    split_input(indata, &(board->port_2CL[0]), 4);
	}
	indata >>= 4;
	if ( (board->dir_bits & 0x80) == 0 ) {
	    split_input(indata, &(board->port_2CH[0]), 4);
	}
    }
}

unsigned char build_output(io_pin_t *src, int num)
{
    int b;
    unsigned char data, mask;

    data = 0x00;
    mask = 0x01;
    /* assemble output byte for data port from 'num' source variables */
    for (b = 0; b < num; b++) {
	/* get the data, add to output byte */
	if ( *(src->data) ) {
	    if ( !(src->io.invert) ) {
		data |= mask;
	    }
	} else {
	    if ( (src->io.invert) ) {
		data |= mask;
	    }
	}
	mask <<= 1;
	src++;
    }
    return data;
}

static void write_board(void *arg, long period)
{
    board_t *board;
    unsigned char outdata, tmp;

    board = arg;
    if ( (board->dir_bits & 0x01) == 0x01 ) {
	outdata = build_output(&(board->port_1A[0]), 8);
	rtapi_outb(~outdata, board->base_addr+0);
    }
    if ( (board->dir_bits & 0x02) == 0x02 ) {
	outdata = build_output(&(board->port_1B[0]), 8);
	rtapi_outb(~outdata, board->base_addr+1);
    }
    if ( (board->dir_bits & 0x0A) != 0x00 ) {
	outdata = 0;
	if ( (board->dir_bits & 0x04) == 0x04 ) {
	    tmp = build_output(&(board->port_1CL[0]), 4);
	    outdata = tmp;
	}
	if ( (board->dir_bits & 0x08) == 0x08 ) {
	    tmp = build_output(&(board->port_1CH[0]), 4);
	    outdata = outdata | (tmp << 4);
	}
	rtapi_outb(~outdata, board->base_addr+2);
    }
    if ( (board->dir_bits & 0x10) == 0x10 ) {
	outdata = build_output(&(board->port_2A[0]), 8);
	rtapi_outb(~outdata, board->base_addr+4);
    }
    if ( (board->dir_bits & 0x20) == 0x20 ) {
	outdata = build_output(&(board->port_2B[0]), 8);
	rtapi_outb(~outdata, board->base_addr+5);
    }
    if ( (board->dir_bits & 0xA0) != 0x00 ) {
	outdata = 0;
	if ( (board->dir_bits & 0x40) == 0x40 ) {
	    tmp = build_output(&(board->port_2CL[0]), 4);
	    outdata = tmp;
	}
	if ( (board->dir_bits & 0x80) == 0x80 ) {
	    tmp = build_output(&(board->port_2CH[0]), 4);
	    outdata = outdata | (tmp << 4);
	}
	rtapi_outb(~outdata, board->base_addr+6);
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int pins_and_params(char *argv[])
{
    unsigned short board_addr[MAX_BOARDS];
    unsigned char dir_bits[MAX_BOARDS], mask;
    int n, m, retval;

    /* clear port_addr and dir_bits arrays */
    for (n = 0; n < MAX_BOARDS; n++) {
	board_addr[n] = 0;
	dir_bits[n] = 0;
    }
    /* parse config string, results in port_addr[] and data_dir[] arrays */
    num_boards = 0;
    n = 0;
    while ((num_boards < MAX_BOARDS) && (argv[n] != 0)) {
	board_addr[num_boards] = parse_board_addr(argv[n]);
	if (board_addr[num_boards] == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"AX5124H: ERROR: bad port address '%s'\n", argv[n]);
	    return -1;
	}
	n++;
	if (argv[n] == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"AX5124H: ERROR: no config info for port %s\n", argv[n-1]);
	    return -1;
	}
	/* should be a string of 8 'I" or "O" characters */
	dir_bits[num_boards] = 0;
	mask = 0x01;
	for ( m = 0 ; m < 8 ; m++ ) {
	    /* test character and set/clear bit */
	    if ((argv[n][m] == 'i') || (argv[n][m] == 'I')) {
		/* input, set mask bit to zero */
		dir_bits[num_boards] &= ~mask;
	    } else if ((argv[n][m] == 'o') || (argv[n][m] == 'O')) {
		/* output, set mask bit to one */
		dir_bits[num_boards] |= mask;
	    } else {
		rtapi_print_msg(RTAPI_MSG_ERR,
		"AX5124H: ERROR: bad config info for port %s: '%s'\n", argv[n-1], argv[n]);
		return -1;
	    }
	    /* shift mask for next but */
	    mask <<= 1;
	}
	n++;
	num_boards++;
    }
    /* OK, now we've parsed everything */
    if (num_boards == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "AX5214H: ERROR: no ports configured\n");
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("hal_ax5214h");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "AX5214H: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for board data */
    board_array = hal_malloc(num_boards * sizeof(board_t));
    if (board_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "AX5214H: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the pins and params for each board */
    for (n = 0; n < num_boards; n++) {
	/* config addr and direction */
	board_array[n].base_addr = board_addr[n];
	board_array[n].dir_bits = dir_bits[n];
	/* export all vars */
	retval = export_board(n, &(board_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"AX5214H: ERROR: board %d (%04X) var export failed\n", n, board_addr[n]);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    return 0;
}

static unsigned short parse_board_addr(char *cp)
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

static int export_board(int boardnum, board_t * board)
{
    int retval, msg, dir;
    unsigned char config;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */

    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);
    retval = 0;
    config = 0x80;
    dir = board->dir_bits & 0x01;
    retval += export_port ( boardnum, 0, &(board->port_1A[0]), 8, dir );
    if ( dir == 0 ) {
	config |= 0x10;
    }
    dir = board->dir_bits & 0x02;
    retval += export_port ( boardnum, 8, &(board->port_1B[0]), 8, dir );
    if ( dir == 0 ) {
	config |= 0x02;
    }
    dir = board->dir_bits & 0x04;
    retval += export_port ( boardnum, 16, &(board->port_1CL[0]), 4, dir );
    if ( dir == 0 ) {
	config |= 0x01;
    }
    dir = board->dir_bits & 0x08;
    retval += export_port ( boardnum, 20, &(board->port_1CH[0]), 4, dir );
    if ( dir == 0 ) {
	config |= 0x08;
    }
    board->port1config = config;
    config = 0x80;
    
    dir = board->dir_bits & 0x10;
    retval += export_port ( boardnum, 24, &(board->port_2A[0]), 8, dir );
    if ( dir == 0 ) {
	config |= 0x10;
    }
    dir = board->dir_bits & 0x20;
    retval += export_port ( boardnum, 32, &(board->port_2B[0]), 8, dir );
    if ( dir == 0 ) {
	config |= 0x02;
    }
    dir = board->dir_bits & 0x40;
    retval += export_port ( boardnum, 40, &(board->port_2CL[0]), 4, dir );
    if ( dir == 0 ) {
	config |= 0x01;
    }
    dir = board->dir_bits & 0x80;
    retval += export_port ( boardnum, 44, &(board->port_2CH[0]), 4, dir );
    if ( dir == 0 ) {
	config |= 0x08;
    }
    board->port2config = config;
    /* initialize hardware - all outputs high 
        (since outputs are active low) */
    outb(board->port1config, board->base_addr+3);
    outb(0xff, board->base_addr+0);
    outb(0xff, board->base_addr+1);
    outb(0xff, board->base_addr+2);
    outb(board->port2config, board->base_addr+7);
    outb(0xff, board->base_addr+4);
    outb(0xff, board->base_addr+5);
    outb(0xff, board->base_addr+6);
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return retval;
}

static int export_port(int boardnum, int pin_num, io_pin_t *pin, int num_pins, int dir)
{
    int n, retval;

    retval = 0;
    for ( n = 0 ; n < num_pins ; n++ ) {
	if ( dir == 0 ) {
	    retval += export_input_pin(boardnum, pin_num, pin );
	} else {
	    retval += export_output_pin(boardnum, pin_num, pin );
	}
	pin_num++;
	pin++;
    }
    return retval;
}

static int export_input_pin(int boardnum, int pinnum, io_pin_t *pin)
{
    int retval;

    /* export read only HAL pin for input data */
    retval = hal_pin_bit_newf(HAL_OUT, &(pin->data), comp_id,
			      "ax5214h.%d.in-%02d", boardnum, pinnum);
    if (retval != 0) {
	return retval;
    }
    /* export additional pin for inverted input data */
    retval = hal_pin_bit_newf(HAL_OUT, &(pin->io.not), comp_id,
			      "ax5214h.%d.in-%02d-not", boardnum, pinnum);
    /* initialize HAL pins */
    *(pin->data) = 0;
    *(pin->io.not) = 1;
    return retval;
}

static int export_output_pin(int boardnum, int pinnum, io_pin_t *pin)
{
    int retval;

    /* export read only HAL pin for output data */
    retval = hal_pin_bit_newf(HAL_IN, &(pin->data), comp_id,
			      "ax5214h.%d.out-%02d", boardnum, pinnum);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for polarity */
    retval = hal_param_bit_newf(HAL_RW, &(pin->io.invert), comp_id,
				"ax5214h.%d.out-%02d-invert", boardnum, pinnum);
    /* initialize HAL pin and param */
    *(pin->data) = 0;
    pin->io.invert = 0;
    return retval;
}
