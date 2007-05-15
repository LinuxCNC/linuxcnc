/*************************************************************************

Copyright (C) 2007 John Kasunich <jmkasunich AT sourceforge DOT net>

$RCSfile$
$Author$
$Locker$
$Revision$
$State$
$Date$

This is the driver for the Mesa Electronics 5i20 board.
The board includes a user programable FPGA. This driver
will ultimately handle a wide range of FPGA configurations.

*************************************************************************/


/*************************************************************************


Installation of the driver (realtime only):

first load the FPGA config:   bfload <bitfile> <boardnum>
then load the actual driver:  insmod hal_5i2x

using halcmd:

loadusr -w bfload <bitfile> <boardnum>
loadrt hal_5i2x


The FPGA contains a 1024 byte RAM, and the RAM contains FPGA specific
info that tells the driver what is in the FPGA (and thus what HAL stuff
needs to be exported).  The RAM is treated as a stream of bytes:

pre-header: 4 bytes of 00, treated as no-care
header:
	1 byte: protocol version (in case of future format changes
		that make the RAM incompatible with older drivers)
	1 byte: board ID, 1 for 5i20, 2 for 5i22

blocks: each block starts with a code that identifies the type of
	block it is and the code that parses it.  A code of zero means
	the end of the data (no more blocks).  The main driver simply
	calls an export function for each block, that function reads
	the block content, and passes back a pointer to the first
	byte of the following block.

The space after the data is filled with zeros.  The last two bytes
are a checksum, used to verify that the driver is actually talking
to a 5i2x board with a valid configuration in it.


Items are exported to the HAL based on the capabilities that
the config supplies.  <boardId> is the PCI board number and
is formated as "%d". <channel> is formated as "%02d".

Digital In:
  Pins:
	bit	m5i20.<boardId>.in-<channel>
	bit	m5i20.<boardId>.in-<channel>-not

  Functions:
	void    m5i20.<boardId>.digital-in-read


Digital Out:
  Parameters:
	bit	m5i20.<boardId>.out-<channel>-invert

  Pins:
	bit	m5i20.<boardId>.out-<channel>

  Functions:
	void    m5i20.<boardId>.digital-out-write

Stepgen:

**************************************************************************

This program is free software; you can redistribute it and/or
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

*************************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <linux/pci.h>
#include "rtapi.h"			// RTAPI realtime OS API.
#include "rtapi_app.h"			// RTAPI realtime module decls.
#include "hal.h"			// HAL public API decls.
#include "hal_5i2x.h"			// Hardware dependent defines.

#ifndef MODULE
#define MODULE
#endif

#ifdef MODULE
// Module information.
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Driver for Mesa Electronics 5i2x for EMC HAL");
MODULE_LICENSE("GPL");
#endif // MODULE

/***************************************************************************
                          config ROM defines
***************************************************************************/
#if 0  /* FIXME - this stuff is still up in the air */

/* input options: regular or not-an-input */
#define IN_MASK		0x1
#define GPIN_NONE	0x0
/* if GPIN, export p0-00.in and p0-00.in-not */
#define GPIN		0x1

/* output options: regular, open-collector, tri-state, or not-an-output */
#define OUT_MASK	0xE
#define GPOUT_NONE	0x0
/* if GPOUT_REG, export p0-00.out and p0-00.invert */
#define GPOUT_REG	0x2
/* if GPOUT_OC, export p0-00.out and p0-00.invert */
#define GPOUT_OC	0x4
/* if GPOUT_TS, export p0-00.out, p0-00.invert, and p0-00.out-ena */
#define GPOUT_TS	0x6
/* if SPOUT_REG, export p0-00.invert */
#define SPOUT_REG	0xA
/* if SPOUT_OC, export p0-00.invert */
#define SPOUT_OC	0xC

#endif

/*************************************************************************
                          typedefs and defines
*************************************************************************/


// Vendor and device ID.
#define M5I20_VENDOR_ID			0x10B5		// PLX.
#define M5I20_DEVICE_ID			0x9030		// 9030 SMARTarget I/O Accelerator.
#define M5I20_SUBSYS_VENDOR_ID		0x10B5		// PLX.
#define M5I20_SUBSYS_DEVICE_ID		0x3131		// Mesa 5i20.

/* general I/O */
/* add 4 for each group of 24 pins (one connector) */
#define GPIO_DATA	0x1000
#define GPIO_DIR	0x1100



/*************************************************************************
                                   Globals
*************************************************************************/

#define MAX_BOARDS	4

int comp_id;	/* HAL component ID */
static board_data_t *boards[MAX_BOARDS];

/*************************************************************************
                                 Realtime Code
 ************************************************************************/

static void read_gpio(void *arg, long period)
{
    board_data_t *b;
    dig_port_t *p;
    int n, i;
    __u32 ins, data;

    b = arg;
    for ( n = 0 ; n < PORTS_PER_BOARD; n++ ) {
	p = &(b->gpio[n]);
	ins = p->ins;
	data = ioread32(p->data_addr);
	i = 0;
	while ( ins ) {
	    if ( ins & 1 ) {
		/* this pin is used as input, update HAL pins */
		if ( data & 1 ) {
		    *(p->pins[i].in) = 1;
		    *(p->pins[i].in_not) = 0;
		} else {
		    *(p->pins[i].in) = 0;
		    *(p->pins[i].in_not) = 1;
		}
	    }
	    /* next */
	    ins >>= 1;
	    data >>= 1;
	    i++;
	}
    }
}

static void write_gpio(void *arg, long period)
{
    board_data_t *b;
    dig_port_t *p;
    int n, i;
    __u32 outs, dirs, mask, data;

    b = arg;
    for ( n = 0 ; n < PORTS_PER_BOARD; n++ ) {
	p = &(b->gpio[n]);
	outs = p->outs;
	mask = 1;
	data = 0;
	i = 0;
	while ( outs ) {
	    if ( outs & 1 ) {
		/* this pin is used as output, get data from HAL */
		if ( *(p->pins[i].out) != 0 ) {
		    data |= mask;
		}
		if ( p->pins[i].invert != 0 ) {
		    data ^= mask;
		}
	    }
	    /* next */
	    outs >>= 1;
	    mask <<= 1;
	    i++;
	}
	/* enable pin driver if pin is an output, unless it is open-
	   collector, and the data is high */
	dirs = p->outs & ~(p->ocs & data);
	iowrite32(dirs, p->dir_addr);
	iowrite32(data, p->data_addr);
    }
}

/******************************************************************************
                                HAL export code
 ******************************************************************************/

/* exports pins and params, based on board->gpio[]->ins and ->outs */
static int export_gpio(int boardnum, board_data_t *board )
{
    dig_port_t *port;
    int portnum, pin, retval;
    __u32 mask;
    char name[HAL_NAME_LEN + 2];

    for ( portnum = 0 ; portnum < PORTS_PER_BOARD ; portnum++ ) {
	/* point to port */
	port = &(board->gpio[portnum]);
	/* save FPGA addresses for accessing port data */
	port->data_addr = board->base + GPIO_DATA + 4*portnum;
	port->dir_addr  = board->base + GPIO_DIR  + 4*portnum;
	/* export HAL stuff for port */
	mask = 1;
	for ( pin = 0 ; pin < PINS_PER_PORT ; pin++ ) {
	    if ( port->ins & mask ) {
		/* export output HAL pins for the input bit */
		retval = hal_pin_bit_newf(HAL_OUT, &(port->pins[pin].in),
		    comp_id, "5i20.%d.p%1d-%02d-in", boardnum, portnum, pin);
		if (retval != 0) return retval;
		retval = hal_pin_bit_newf(HAL_OUT, &(port->pins[pin].in_not),
		    comp_id, "5i20.%d.p%1d-%02d-in-not", boardnum, portnum, pin);
		if (retval != 0) return retval;
		/* initial values for pins */
		*(port->pins[pin].in) = 0;
		*(port->pins[pin].in_not) = 1;
	    }
	    /* can be in and out at the same time - in will read back the
		pin value, handy if the output is open collector */
	    if ( port->outs & mask ) {
		/* export input HAL pin for the output bit */
		retval = hal_pin_bit_newf(HAL_IN, &(port->pins[pin].out),
		    comp_id, "5i20.%d.p%1d-%02d-out", boardnum, portnum, pin);
		if (retval != 0) return retval;
		/* export HAL param for inversion */
		retval = hal_param_bit_newf(HAL_RW, &(port->pins[pin].invert),
		    comp_id, "5i20.%d.p%1d-%02d-invert", boardnum, portnum, pin);
		if (retval != 0) return retval;
		/* set initial value for pin and param */
		*(port->pins[pin].out) = 0;
		port->pins[pin].invert = 0;
	    }
	    mask <<= 1;
	}
    }
    /* export functions */
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.gpio-read", boardnum);
    retval = hal_export_funct(name, read_gpio, board, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		"5i2x: ERROR: board %d GPIO read funct export failed\n", boardnum);
	rtapi_app_exit();
	return -1;
    }
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.gpio-write", boardnum);
    retval = hal_export_funct(name, write_gpio, board, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		"5i2x: ERROR: board %d GPIO write funct export failed\n", boardnum);
	rtapi_app_exit();
	return -1;
    }
    return 0;
}


/******************************************************************************
                              Init and exit code
 ******************************************************************************/

int rtapi_app_main(void)
{
    int n, i, retval;
    __u8 config_data[CFG_RAM_SIZE], *cfg_ptr, *end_ptr;
    __u32 data;
    __u16 checksum1, checksum2;
    __u8 protocol_version;
    __u8 board_code, block_code;
    board_data_t *board;
    struct pci_dev *pDev;

    // Connect to the HAL.
    comp_id = hal_init("hal_5i2x");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "5i2x: ERROR: hal_init() failed\n");
	return(-1);
    }

    for ( n = 0 ; n < MAX_BOARDS ; n++ ) {
	boards[n] = NULL;
    }
    pDev = NULL;
    for ( n = 0 ; n < MAX_BOARDS ; n++ ) {
	// Find a M5I20 card.
	pDev = pci_find_device(M5I20_VENDOR_ID, M5I20_DEVICE_ID, pDev);
	if ( pDev == NULL ) {
	    /* no more boards */
	    break;
	}
	/* FIXME: this code would probably detect ANY board using the
	   PLX9030 bridge chip as if it was a 5i20 board.  It is also
	   neccessary to check the subsystem ID.  Add that later... */

	/* Allocate HAL memory for the board */
	board = (board_data_t *)(hal_malloc(sizeof(board_data_t)));
	if ( board == NULL ) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "5i2x: ERROR: hal_malloc() failed\n");
	    rtapi_app_exit();
	    return -1;
	}
	/* gather info about the board and save it */
	board->pci_dev = pDev;
	board->slot = PCI_SLOT(pDev->devfn);
	board->num = n;
	rtapi_print_msg(RTAPI_MSG_INFO,
	     "5i2x: Board %d detected in Slot: %2x\n", board->num, board->slot);
	/* region 5 is the 32 bit memory mapped region */
	board->len = pci_resource_len(pDev, 5);
	board->base = ioremap_nocache(pci_resource_start(pDev, 5), board->len);
	if ( board->base == NULL ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i2x: ERROR: could not map board %d FPGA data\n", board->num );
	    rtapi_app_exit();
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"5i2x: board %d FPGA data mapped to %08lx, Len = %ld\n",
		board->num, (long)board->base, (long)board->len);
	}
	/* read the configuration RAM */
	i = 0;
	while ( i < CFG_RAM_SIZE ) {
	    data = ioread32(board->base+i);
	    config_data[i++] = data & 0xFF;
	    data >>= 8;
	    config_data[i++] = data & 0xFF;
	    data >>= 8;
	    config_data[i++] = data & 0xFF;
	    data >>= 8;
	    config_data[i++] = data & 0xFF;
	}
	/* mask out first four bytes */
	for ( i = 0 ; i < 4 ; i++ ) {
	    config_data[i] = 0;
	}
	/* calculate the checksum */
	/* we use a 16 bit variant on Adler32, it is more robust than
	   a simple checksum, and simpler to compute than a real CRC */
	checksum1 = 0;
	checksum2 = 0;
	for ( i = 0 ; i <= DATA_END_ADDR ; i++ ) {
	    checksum1 += config_data[i];
	    while ( checksum1 > 251 ) checksum1 -= 251;
	    checksum2 += checksum1;
	    while ( checksum2 > 251 ) checksum2 -= 251;
	}
	/* validate the checksum */
	if (( checksum1 != config_data[CHECKSUM1_ADDR] ) ||
	    ( checksum2 != config_data[CHECKSUM2_ADDR] )) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i2x0: ERROR: board %d RAM checksum, is config loaded?\n",
		board->num );
	    rtapi_app_exit();
	    return -1;
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "5i2x: board %d checksum OK\n", board->num );
	/* get header info and validate it */
	protocol_version = config_data[PROTOCOL_VERSION_ADDR];
	if (( protocol_version < MIN_PROTOCOL_VERSION ) ||
	    ( protocol_version > MAX_PROTOCOL_VERSION )) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i2x: ERROR: board %d data is version %d\n",
		board->num, protocol_version );
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i2x: ERROR: this driver works with version %d thru %d only\n",
		MIN_PROTOCOL_VERSION, MAX_PROTOCOL_VERSION );
	    rtapi_app_exit();
	    return -1;
	}
	board_code = config_data[BOARD_CODE_ADDR];
	if ( board_code == BOARD_CODE_5I20 ) {
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"5i2x: board %d code says 5i20\n", board->num );
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i2x: ERROR: board %d has unknown board code '%d'\n",
		board->num, board_code );
	    rtapi_app_exit();
	    return -1;
	}
	/* point to first block of data */
	cfg_ptr = &(config_data[DATA_START_ADDR]);
	end_ptr = &(config_data[DATA_END_ADDR]);
	/* loop thru all blocks, calling the appropriate export functions */
	while (( cfg_ptr < end_ptr ) && ( *cfg_ptr > 0 )) {
	    /* we have a block */
	    block_code = *cfg_ptr;
	    retval = -1;
	    switch ( block_code ) {
	    case STEPGEN_VEL_MODE:
		rtapi_print("stepgen, code is %d, at %d\n", block_code, i );
		retval = export_stepgen(&cfg_ptr, board);
		break;
	    default:
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "5i2x: ERROR: board %d: unknown block code '%d' at %d\n",
		    board->num, block_code, i );
		rtapi_app_exit();
		return -1;
	    }
	    if ( retval != 0 ) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "5i2x: ERROR: export failed: board %d, block code '%d' cfg RAM addr %04x\n",
		    board->num, block_code, i );
		rtapi_app_exit();
		return -1;
	    }
	}
    }
    if(n == 0){
	/* No cards detected */
	rtapi_print_msg(RTAPI_MSG_ERR, "5I2x: ERROR: No 5I20 card(s) detected\n");
	rtapi_app_exit();
	return(-1);
    }
    hal_ready(comp_id);
    return(0);
}

void rtapi_app_exit(void)
{
    int n;

    hal_exit(comp_id);
    for ( n = 0; n < MAX_BOARDS; n++ ) {
	if ( boards[n] != NULL) {
	    // Unmap board memory
	    if ( boards[n]->base != NULL ) {
		iounmap(boards[n]->base);
	    }
	}
    }
}
