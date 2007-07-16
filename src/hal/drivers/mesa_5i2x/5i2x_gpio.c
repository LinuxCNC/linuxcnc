/*************************************************************************

Copyright (C) 2007 John Kasunich <jmkasunich AT sourceforge DOT net>

$RCSfile$
$Author$
$Locker$
$Revision$
$State$
$Date$

This is the driver the General Purpose I/O pins on the Mesa Electronics
5i2x boards.  It configures the I/O as defined by data stored in the
FPGA RAM, and exports the corresponding HAL pins.

*************************************************************************/


/*************************************************************************

Each physical pin has a number of options that are selected as part of
the FPGA configuration.  Pins can be input, output, tristate, or open
collector.  Additionally, they can be active high or active low.  When
used for inputs, data from a physical pin appears on an optional HAL pin
as well as at the inputs to any FPGA modules (such as encoder counters)
that are connected to the pin.  When used as an output, the pin data can
be driven from only one source, either general purpose I/O, or an FPGA
module (such as a step generator or PWM generator).

Items are exported to the HAL based on the capabilities that
the config supplies.  <boardId> is the PCI board number and
is formated as "%d". <channel> goes from "A00" (first pin on
first connector) to "C23" (last pin on last connector).

Digital In:
  Pins:
	bit	m5i20.<boardId>.pin-<channel>-in
	bit	m5i20.<boardId>.pin-<channel>-in-not

  Functions:
	void    m5i20.<boardId>.digital-in-read


Digital Out:
  Parameters:
	bit	m5i20.<boardId>.pin-<channel>-out-invert

  Pins:
	bit	m5i20.<boardId>.pin-<channel>-out
	bit	m5i20.<boardId>.pin-<channel>-out-en

  Functions:
	void    m5i20.<boardId>.digital-out-write

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


/***************************************************************************
                          config ROM defines
***************************************************************************/

#define HAL_INPUT_PIN_MASK	0x80
#define HAL_OUTPUT_PIN_MASK	0x40
#define HAL_ENABLE_PIN_MASK	0x20
#define POLARITY_MASK		0x10
#define SOURCE_MASK		0xC0
#define SOURCE1_MASK		0x08
#define SOURCE0_MASK		0x04
#define MODE_MASK		0x03
#define MODE1_MASK		0x02
#define MODE0_MASK		0x01

/*************************************************************************
                          typedefs and defines
*************************************************************************/

/* register addresses (relative to base addr) */
#define GPIO_IN		0
#define GPIO_OUT	0
#define GPIO_OUT_ENA	1
#define GPIO_MODE0	2
#define GPIO_MODE1	3
#define GPIO_SOURCE0	4
#define GPIO_SOURCE1	5
#define GPIO_POLARITY	6

/***************************************************************************
                         Data Structures
***************************************************************************/

/* one 24-pin physical port */
typedef struct gpio_t {
    struct gpio_t *next;
    hal_bit_t *in[PINS_PER_PORT];
    hal_bit_t *in_not[PINS_PER_PORT];
    hal_bit_t *out[PINS_PER_PORT];
    hal_bit_t out_invert[PINS_PER_PORT];
    hal_bit_t *out_ena[PINS_PER_PORT];
    void __iomem *addr;
} gpio_t;



/*************************************************************************
                                   Globals
*************************************************************************/

static hal_bit_t dummy_in;
static hal_bit_t dummy_out = 0;
static hal_bit_t dummy_oe = 1;

/*************************************************************************
                                 Realtime Code
 ************************************************************************/

static void read_gpio(void *arg, long period)
{
    gpio_t *g;
    int n;
    __u32 data;

    g = arg;
    data = ioread32(g->addr+GPIO_IN);
    for ( n = 0 ; n < PINS_PER_PORT; n++ ) {
	*(g->in[n]) = (data & 1);
	*(g->in_not[n]) = ~(data & 1);
    }
}

static void read_gpios(void *arg, long period)
{
    gpio_t *s;

    s = arg;
    while ( s != NULL ) {
	read_gpio(s, period);
	s = s->next;
    }
}


static void write_gpio(void *arg, long period)
{
    gpio_t *g;
    int n;
    __u32 data, enable, mask;

    g = arg;
    data = 0;
    enable = 0;
    mask = 1;
    for ( n = 0 ; n < PINS_PER_PORT; n++ ) {
	if ( *(g->out_ena[n]) ) {
	    enable |= mask;
	}
	if (( *(g->out[n]) && ~g->out_invert[n] ) ||
	    ( ~*(g->out[n]) && g->out_invert[n] )) {
	    data |= mask;
	}
	mask <<= 1;
    }
    iowrite32(data, g->addr+GPIO_OUT);
    iowrite32(enable, g->addr+GPIO_OUT_ENA);
}


static void write_gpios(void *arg, long period)
{
    gpio_t *s;

    s = arg;
    while ( s != NULL ) {
	write_gpio(s, period);
	s = s->next;
    }
}


/******************************************************************************
                                HAL export code
 ******************************************************************************/

/* the data for a "gpio" consists of a code byte, a two byte base address,
   and 24 pin config bytes - formatted as follows:

   IOEPSSMM

   I = export input HAL pin
   O = export output HAL pin
   E = export enable HAL pin
   P = polarity
   SS = source (one of four, 00 = GPIO)
   MM = mode (input, output, tristate, open-collector)

*/

int export_gpio(__u8 **ppcfg, board_data_t *board)
{
    __u8 *data;
    int retval;
    int portnum, boardnum, pin;
    char portchar;
    gpio_t *gpio, **p;
    int code, addr;
    __u8 *cfg_bytes;
    char name[HAL_NAME_LEN + 2];
    __u32 mode0, mode1, source0, source1, polarity, mask;

    /* read and validate config data */
    data = *ppcfg;
    code = data[0];
    addr = (data[1] << 8) + data[2];
    cfg_bytes = &(data[3]);
    /* return ptr to next block */
    *ppcfg = &(cfg_bytes[24]);
    /* Allocate HAL memory */
    gpio = (gpio_t *)(hal_malloc(sizeof(gpio_t)));
    if ( gpio == NULL ) {
	rtapi_print_msg(RTAPI_MSG_ERR, "5i2x: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* find end of linked list */
    boardnum = board->num;
    portnum = 0;
    p = &(board->gpio);
    while ( *p != NULL ) {
	p = &((*p)->next);
	portnum++;
    }
    portchar = 'A' + portnum;
    /* add to end of list */
    *p = gpio;
    gpio->next = NULL;
    /* save base address */
    gpio->addr = board->base + addr;
    mode0 = 0;
    mode1 = 0;
    source0 = 0;
    source1 = 0;
    polarity = 0;
    mask = 1;
    for ( pin = 0 ; pin < PINS_PER_PORT ; pin++ ) {
	if ( cfg_bytes[pin] & HAL_INPUT_PIN_MASK ) {
	    /* export output HAL pins for the input bit */
	    retval = hal_pin_bit_newf(HAL_OUT, &(gpio->in[pin]),
		comp_id, "5i20.%d.pin-%c%02d-in", boardnum, portchar, pin);
	    if (retval != 0) return retval;
	    retval = hal_pin_bit_newf(HAL_OUT, &(gpio->in_not[pin]),
		comp_id, "5i20.%d.pin-%c%02d-in-not", boardnum, portchar, pin);
	    if (retval != 0) return retval;
	    /* initial values for pins */
	    *(gpio->in[pin]) = 0;
	    *(gpio->in_not[pin]) = 1;
	} else {
	    /* input pins not used, point them at a dummy */
	    gpio->in[pin] = &dummy_in;
	    gpio->in_not[pin] = &dummy_in;
	}
	if ( cfg_bytes[pin] & HAL_OUTPUT_PIN_MASK ) {
	    /* export input HAL pin for the output bit */
	    retval = hal_pin_bit_newf(HAL_IN, &(gpio->out[pin]),
		comp_id, "5i20.%d.pin-%c%02d-out", boardnum, portchar, pin);
	    if (retval != 0) return retval;
	    /* export HAL param for inversion */
	    retval = hal_param_bit_newf(HAL_RW, &(gpio->out_invert[pin]),
		comp_id, "5i20.%d.pin-%c%02d-invert", boardnum, portchar, pin);
	    if (retval != 0) return retval;
	    /* set initial value for pin and param */
	    *(gpio->out[pin]) = 0;
	    gpio->out_invert[pin] = 0;
	} else {
	    /* output pin not used, point at a dummy */
	    gpio->out[pin] = &dummy_out;
	}
	if ( cfg_bytes[pin] & HAL_ENABLE_PIN_MASK ) {
	    /* export input HAL pin for the output enable bit */
	    retval = hal_pin_bit_newf(HAL_IN, &(gpio->out_ena[pin]),
		comp_id, "5i20.%d.pin-%c%02d-out-en", boardnum, portchar, pin);
	    if (retval != 0) return retval;
	    /* set initial value for pin */
	    *(gpio->out_ena[pin]) = 0;
	} else {
	    /* output enable pin not used, point at a dummy */
	    gpio->out_ena[pin] = &dummy_oe;
	}
	/* set appropriate bits in config register words */
	if ( cfg_bytes[pin] & SOURCE1_MASK ) source1 |= mask;
	if ( cfg_bytes[pin] & SOURCE0_MASK ) source0 |= mask;
	if ( cfg_bytes[pin] & MODE1_MASK ) mode1 |= mask;
	if ( cfg_bytes[pin] & MODE0_MASK ) mode0 |= mask;
	if ( cfg_bytes[pin] & POLARITY_MASK ) polarity |= mask;
	mask <<= 1;
    }
    /* write config to registers */
    iowrite32(source1, gpio->addr+GPIO_SOURCE1);
    iowrite32(source0, gpio->addr+GPIO_SOURCE0);
    iowrite32(mode1, gpio->addr+GPIO_MODE1);
    iowrite32(mode0, gpio->addr+GPIO_MODE0);
    iowrite32(polarity, gpio->addr+GPIO_POLARITY);
    /* export functions - one funct serves all ports */
    if ( portnum > 0 ) {
	/* already exported */
	return 0;
    }
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.gpio.read", boardnum);
    retval = hal_export_funct(name, read_gpios, gpio, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "5i20: ERROR: board %d GPIO read funct export failed\n",
	    boardnum);
	return -1;
    }
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.gpio.write", boardnum);
    retval = hal_export_funct(name, write_gpios, gpio, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "5i20: ERROR: board %d GPIO write funct export failed\n",
	    boardnum);
	return -1;
    }
    return 0;
}
