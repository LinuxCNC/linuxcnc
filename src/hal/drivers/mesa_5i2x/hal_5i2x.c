/******************************************************************************

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

Installation of the driver (realtime only):

(first load the FPGA config: sudo m5i20cfg <bitfile> <boardnum>

insmod hal_m5i20

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

**********************************************************************

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

**********************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif


#include <linux/pci.h>

#include "rtapi.h"			// RTAPI realtime OS API.
#include "rtapi_app.h"			// RTAPI realtime module decls.
#include "hal.h"			// HAL public API decls.
//#include "plx9030.h"			// Hardware dependent defines.
//#include "m5i20.h"			// Hardware dependent defines.


#ifndef MODULE
#define MODULE
#endif


#ifdef MODULE
// Module information.
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Driver for Mesa Electronics 5i20 for EMC HAL");
MODULE_LICENSE("GPL");
#endif // MODULE

/***************************************************************************
                          config ROM defines
***************************************************************************/
#if 0  /* FIXME - no ROM yet */

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

/***************************************************************************
                          typedefs and defines
***************************************************************************/


// Vendor and device ID.
#define M5I20_VENDOR_ID			0x10B5		// PLX.
#define M5I20_DEVICE_ID			0x9030		// 9030 SMARTarget I/O Accelerator.
#define M5I20_SUBSYS_VENDOR_ID		0x10B5		// PLX.
#define M5I20_SUBSYS_DEVICE_ID		0x3131		// Mesa 5i20.


/* Step generator ports */
#define STEPGEN_BASE		0x2000
/* add the following to STEPGEN_BASE */
/* also add channel number times 4 */
#define STEPGEN_RATE_REG	0x0000
#define STEPGEN_ACCUM		0x0100
#define STEPGEN_MODE_REG	0x0200
#define STEPGEN_MODE_STEP_POL_MASK	0x01
#define STEPGEN_MODE_DIR_POL_MASK	0x02
#define STEPGEN_MODE_QUAD_MODE_MASK	0x04
#define STEPGEN_MODE_OUT_ENA_MASK	0x08
#define STEPGEN_DIR_SETUP	0x0300
#define STEPGEN_DIR_HOLD	0x0400
#define STEPGEN_STEP_LEN	0x0500
#define STEPGEN_MAX_TIMER	0x0FFF

/* shared stepgen port */
#define MASTER_DDS 0xC000

/* timing stuff */
#define STEPGEN_MASTER_CLOCK 33000000
#define COUNTS_PER_HZ ((double)(1ll << 32)/(double)STEPGEN_MASTER_CLOCK)
#define STEPGEN_MASTER_PERIOD (1000000000/STEPGEN_MASTER_CLOCK)

/* encoder stuff */



/* config ROM */
#define ROM	0x0000
/* default contents */
const u32 rom[] = { 0x00000000, 0x11111111, 0xabcd1234 };

/* general I/O */
/* add 4 for each group of 24 pins (one connector) */
#define GPIO_DATA	0x1000
#define GPIO_DIR	0x1100

/* code assumes that PINS_PER_PORT is <= 32 */
#define PINS_PER_PORT 24
#define PORTS_PER_BOARD 3


/***************************************************************************
                         Data Structures
***************************************************************************/

/* digital I/O pin */
typedef struct dig_io_t {
    hal_bit_t *in;
    hal_bit_t *in_not;
    hal_bit_t *out;
    hal_bit_t invert;
} dig_io_t;

/* one connector worth of digital I/O */
typedef struct dig_port_t {
    u32 ins;		/* bitmap marking all inputs */
    u32 outs;		/* bitmap marking all outputs */
    u32 ocs;		/* bitmap marking open collector outputs */
    void __iomem *data_addr;
    void __iomem *dir_addr;
    dig_io_t pins[24];
} dig_port_t;

/* one stepgen */
typedef struct stepgen_t {
    hal_s32_t *counts;
    hal_float_t *pos_fb;
    hal_bit_t *enable;
    hal_float_t *vel_cmd;
    hal_float_t frequency;
    hal_float_t maxaccel;
    hal_float_t maxvel;
    hal_float_t scale;
    hal_u32_t steplen;
    hal_u32_t stepspace;
    hal_u32_t dirsetup;
    hal_u32_t dirhold;
    void __iomem *addr;
    hal_float_t old_maxaccel;
    hal_float_t old_maxvel;
    hal_float_t old_scale;
    u32 old_steplen;
    u32 old_stepspace;
    u32 old_dirsetup;
    u32 old_dirhold;
    double internal_maxvel;
    double max_deltav;
    double current_vel;
    int update_max;
    s32 old_accum;
    long long int counts_hires;
} stepgen_t;

/* master board structure */
typedef struct board_data_t {
    struct pci_dev *pci_dev;
    int slot;
    int boardnum;
    void __iomem *base;
    int len;
    dig_port_t gpio[PORTS_PER_BOARD];
    stepgen_t stepgen[8];
} board_data_t; 

/**************************************************************************
                                   Globals
***************************************************************************/

#define MAX_BOARDS	4

static int comp_id;	/* HAL component ID */
static board_data_t *boards[MAX_BOARDS];

/******************************************************************************
                                 Realtime Code
 ******************************************************************************/

static void read_gpio(void *arg, long period)
{
    board_data_t *b;
    dig_port_t *p;
    int n, i;
    u32 ins, data;

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
    u32 outs, dirs, mask, data;

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

static void read_stepgen(void *arg, long period)
{
    stepgen_t *s;
    s32 accum, delta;

    s = arg;
    if ( s->scale != s->old_scale ) {
	s->old_scale = s->scale;
	/* validate the new scale value */
	if ((s->scale < 1e-20) && (s->scale > -1e-20)) {
	    /* value too small, divide by zero is a bad thing */
	    s->scale = 1.0;
	}
	/* flag the change for write_stepgen() */
	s->update_max = 1;
    }
    /* read the accumulator, this is 16.16 full/fractional steps */
    accum = ioread32(s->addr+STEPGEN_ACCUM);
    /* compute delta */
    delta = accum - s->old_accum;
    s->old_accum = accum;
    /* update integer and high resolution counts */
    *(s->counts) += (delta >> 16);
    s->counts_hires += delta;
    /* convert high res counts to position */
    *(s->pos_fb) = ((double)s->counts_hires / s->scale) * ( 1.0 / ( 1LL << 16 ));
}

static void write_stepgen(void *arg, long period)
{
    stepgen_t *s;
    int clocks;
    s32 addval;
    u32 min_period_ns;
    double max_freq, vel_cmd, vel_diff;
    u32 mode;

    s = arg;
    /* lots of parameter processing, do only if something has changed */
    if ( s->dirhold != s->old_dirhold ) {
	/* convert dirhold in ns to clock periods */
	clocks = s->dirhold * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	if ( clocks > STEPGEN_MAX_TIMER ) { clocks = STEPGEN_MAX_TIMER; }
	/* set parameter to actual (post rounding) value */
	s->old_dirhold = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->dirhold = s->old_dirhold;
	/* write to hardware */
	iowrite32(clocks, s->addr + STEPGEN_DIR_HOLD);
    }
    if ( s->dirsetup != s->old_dirsetup ) {
	/* convert dirsetup in ns to clock periods */
	clocks = s->dirsetup * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	if ( clocks > STEPGEN_MAX_TIMER ) { clocks = STEPGEN_MAX_TIMER; }
	/* set parameter to actual (post rounding) value */
	s->old_dirsetup = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->dirsetup = s->old_dirsetup;
	/* write to hardware */
	iowrite32(clocks, s->addr + STEPGEN_DIR_SETUP);
    }
    if ( s->steplen != s->old_steplen ) {
	/* convert steplen in ns to clock periods */
	clocks = s->steplen * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	if ( clocks > STEPGEN_MAX_TIMER ) { clocks = STEPGEN_MAX_TIMER; }
	/* set parameter to actual (post rounding) value */
	s->old_steplen = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->steplen = s->old_steplen;
	/* write to hardware */
	iowrite32(clocks, s->addr + STEPGEN_STEP_LEN);
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->stepspace != s->old_stepspace ) {
	/* convert stepspace in ns to clock periods */
	clocks = s->stepspace * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	/* set parameter to actual (post rounding) value */
	s->old_stepspace = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->stepspace = s->old_stepspace;
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->scale != s->old_scale ) {
	s->old_scale = s->scale;
	/* validate the new scale value */
	if ((s->scale < 1e-20) && (s->scale > -1e-20)) {
	    /* value too small, divide by zero is a bad thing */
	    s->scale = 1.0;
	}
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->maxvel != s->old_maxvel ) {
	if ( s->maxvel < 0.0 ) {
	    s->maxvel = -s->maxvel;
	}
	s->old_maxvel = s->maxvel;
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->maxaccel != s->old_maxaccel ) {
	if ( s->maxaccel < 0.0 ) {
	    s->maxaccel = -s->maxaccel;
	}
	s->old_maxaccel = s->maxaccel;
	s->max_deltav = s->maxaccel * period * 0.000000001;
    }
    if ( s->update_max ) {
	/* either maxvel, scale, steplen, or stepspace changed */
	min_period_ns = s->steplen + s->stepspace;
	max_freq = 1000000000.0 / min_period_ns;
	s->internal_maxvel = max_freq / s->scale;
	if ( s->maxvel > 0.0 ) {
	    if ( s->maxvel < s->internal_maxvel ) {
		s->internal_maxvel = s->maxvel;
	    } else {
		s->maxvel = s->internal_maxvel;
		s->old_maxvel = s->maxvel;
	    }
	}
    }
    /* apply velocity limits */
    vel_cmd = *(s->vel_cmd);
    if ( vel_cmd > s->internal_maxvel ) {
	vel_cmd = s->internal_maxvel;
	*(s->vel_cmd) = vel_cmd;
    } else if ( vel_cmd < -s->internal_maxvel ) {
	vel_cmd = -s->internal_maxvel;
	*(s->vel_cmd) = vel_cmd;
    }
    /* apply ramping */
    if ( s->max_deltav != 0.0 ) {
	vel_diff = vel_cmd - s->current_vel;
	if ( vel_diff > s->max_deltav ) {
	    s->current_vel += s->max_deltav;
	} else if ( vel_diff < -s->max_deltav ) {
	    s->current_vel -= s->max_deltav;
	} else {
	    s->current_vel = vel_cmd;
	}
    }
    /* convert vel to freq */
    s->frequency = s->current_vel * s->scale;
    /* convert frequency to adder value and write to hardware */
    addval = s->frequency * COUNTS_PER_HZ;
    iowrite32(addval, s->addr + STEPGEN_RATE_REG);

    mode = 0;
    if ( *(s->enable) != 0 ) {
	mode |= STEPGEN_MODE_OUT_ENA_MASK;
    }
    iowrite32(mode, s->addr + STEPGEN_MODE_REG);

}



/******************************************************************************
                                HAL export code
 ******************************************************************************/

/* exports pins and params, based on board->gpio[]->ins and ->outs */
static int export_gpio(int boardnum, board_data_t *board )
{
    dig_port_t *port;
    int portnum, pin, retval;
    u32 mask;
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
		"5i20: ERROR: board %d GPIO read funct export failed\n", boardnum);
	rtapi_app_exit();
	return -1;
    }
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.gpio-write", boardnum);
    retval = hal_export_funct(name, write_gpio, board, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		"5i20: ERROR: board %d GPIO write funct export failed\n", boardnum);
	rtapi_app_exit();
	return -1;
    }
    return 0;
}

/* exports pins and params for one stepgen */
static int export_stepgen(int boardnum, int gennum, board_data_t *board )
{
    int retval;
    stepgen_t *stepgen;
    char name[HAL_NAME_LEN + 2];

    /* point to the stepgen */
    stepgen = &(board->stepgen[gennum]);
    /* export output HAL pins for feedbacks */
    retval = hal_pin_s32_newf(HAL_OUT, &(stepgen->counts),
	comp_id, "5i20.%d.stepgen.%d.counts", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_pin_float_newf(HAL_OUT, &(stepgen->pos_fb),
	comp_id, "5i20.%d.stepgen.%d.pos-fb", boardnum, gennum);
    if (retval != 0) return retval;
    /* export HAL input pins for control */
    retval = hal_pin_bit_newf(HAL_IN, &(stepgen->enable),
	comp_id, "5i20.%d.stepgen.%d.enable", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_pin_float_newf(HAL_IN, &(stepgen->vel_cmd),
	comp_id, "5i20.%d.stepgen.%d.vel-cmd", boardnum, gennum);
    if (retval != 0) return retval;
    /* now the parameters */
    retval = hal_param_float_newf(HAL_RO, &(stepgen->frequency),
	comp_id, "5i20.%d.stepgen.%d.frequency", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_float_newf(HAL_RW, &(stepgen->maxaccel),
	comp_id, "5i20.%d.stepgen.%d.maxaccel", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_float_newf(HAL_RW, &(stepgen->maxvel),
	comp_id, "5i20.%d.stepgen.%d.maxvel", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_float_newf(HAL_RW, &(stepgen->scale),
	comp_id, "5i20.%d.stepgen.%d.scale", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->steplen),
	comp_id, "5i20.%d.stepgen.%d.steplen", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->stepspace),
	comp_id, "5i20.%d.stepgen.%d.stepspace", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->dirsetup),
	comp_id, "5i20.%d.stepgen.%d.dirsetup", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->dirhold),
	comp_id, "5i20.%d.stepgen.%d.dirhold", boardnum, gennum);
    if (retval != 0) return retval;
    /* set initial value for pin and params */
    *(stepgen->counts) = 0;
    *(stepgen->pos_fb) = 0.0;
    *(stepgen->enable) = 0;
    *(stepgen->vel_cmd) = 0.0;
    stepgen->frequency = 0.0;
    stepgen->maxaccel = 0.0;
    stepgen->maxvel = 0.0;
    stepgen->scale = 200.0;
    stepgen->steplen = 100;
    stepgen->stepspace = 100;
    stepgen->dirsetup = 100;
    stepgen->dirhold = 100;
    /* init other stuff */
    stepgen->addr = board->base + STEPGEN_BASE + 4*gennum;
    stepgen->old_maxaccel = -1.0;
    stepgen->old_maxvel = -1.0;
    stepgen->old_scale = 0.0;
    stepgen->counts_hires = 0;
    stepgen->old_steplen = 0;
    stepgen->old_stepspace = 0;
    stepgen->old_dirsetup = 0;
    stepgen->old_dirhold = 0;
    stepgen->internal_maxvel = 0;
    stepgen->max_deltav = 0;
    stepgen->current_vel = 0;
    stepgen->update_max = 1;
    stepgen->old_accum = 0;
    stepgen->counts_hires = 0;
    /* set master DDS (FIXME this goes away) */
    iowrite32(0xFFFFFFFF, board->base + MASTER_DDS);
    /* export functions */
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.stepgen.%d.read", boardnum, gennum);
    retval = hal_export_funct(name, read_stepgen, stepgen, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "5i20: ERROR: board %d stepgen %d read funct export failed\n",
	    boardnum, gennum);
	rtapi_app_exit();
	return -1;
    }
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.stepgen.%d.write", boardnum, gennum);
    retval = hal_export_funct(name, write_stepgen, stepgen, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "5i20: ERROR: board %d stepgen %d read funct export failed\n",
	    boardnum, gennum);
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
    u32 foo[256];
    board_data_t *board;
    struct pci_dev *pDev;

    // Connect to the HAL.
    comp_id = hal_init("hal_5i20");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "5i20: ERROR: hal_init() failed\n");
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
	/* Allocate HAL memory for the board */
	board = (board_data_t *)(hal_malloc(sizeof(board_data_t)));
	if ( board == NULL ) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "5i20: ERROR: hal_malloc() failed\n");
	    rtapi_app_exit();
	    return -1;
	}
	/* gather info about the board and save it */
	board->pci_dev = pDev;
	board->slot = PCI_SLOT(pDev->devfn);
	rtapi_print_msg(RTAPI_MSG_INFO,
	     "5i20: Board %d detected in Slot: %2x\n", n, board->slot);
	/* region 5 is the 32 bit memory mapped region */
	board->len = pci_resource_len(pDev, 5);
	board->base = ioremap_nocache(pci_resource_start(pDev, 5), board->len);
	if ( board->base == NULL ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i20: ERROR: could not map board %d FPGA data\n", n );
	    rtapi_app_exit();
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"5i20: board %d FPGA data mapped to %08lx, Len = %ld\n",
		n, (long)board->base, (long)board->len);
	}

/* FIXME - this assumes a particular config */

	for ( i = 0 ; i < 3 ; i++ ) {
	    foo[i] = ioread32(board->base+ROM+i*4);
	}
	if ( foo[2] != rom[2] ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i20: ERROR: board %d ROM check failed, is config loaded?\n", n );
	    rtapi_app_exit();
	    return -1;
	}
#if 0
	board->gpio[0].ins  = 0x003F0000;
	board->gpio[0].outs = 0x00F00000;
	board->gpio[0].ocs  = 0x00A00000;
	board->gpio[1].ins  = 0x00FFFFFF;
	board->gpio[1].outs = 0x00FFFFFF;
	board->gpio[1].ocs  = 0x00000000;
	board->gpio[2].ins  = 0x00FFFFFF;
	board->gpio[2].outs = 0x00FF0000;
	board->gpio[2].ocs  = 0x00000000;
#endif
	board->gpio[0].ins  = 0x00C00003;
	board->gpio[0].outs = 0x00C00000;
	board->gpio[0].ocs  = 0x00000000;
	board->gpio[1].ins  = 0x00000000;
	board->gpio[1].outs = 0x00000000;
	board->gpio[1].ocs  = 0x00000000;
	board->gpio[2].ins  = 0x00000000;
	board->gpio[2].outs = 0x00000000;
	board->gpio[2].ocs  = 0x00000000;
	retval = export_gpio(n, board);
	if ( retval != 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"5i20: ERROR: GPIO pin/param/function export failed\n" );
	    rtapi_app_exit();
	    return -1;
	}
	for ( i = 0 ; i < 1 ; i++ ) {
	    retval = export_stepgen(n, i, board);
	    if ( retval != 0 ) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "5i20: ERROR: stepgen %d pin/param/function export failed\n", i );
		rtapi_app_exit();
		return -1;
	    }
	}

#if 0
	// Initialize device.
	if(Device_Init(pDevice, pCard16, pCard32, pBridgeIc)){
	    hal_exit(driver.componentId);
	    return(-1);
	}

	// Export pins, parameters, and functions.
	if(Device_ExportPinsParametersFunctions(pDevice, driver.componentId, i++)){
	    hal_exit(driver.componentId);
	    return(-1);
	}
#endif
    }

    if(n == 0){
	/* No cards detected */
	rtapi_print_msg(RTAPI_MSG_ERR, "5I20: ERROR: No 5I20 card(s) detected\n");
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
