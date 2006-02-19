/********************************************************************
* Description:  hal_vti.c
*               This is the driver for the Vigilant Technologies
*               PCI-ENCDAC 4 channel controller.
*
* Author: Eric H. Johnson from template by Alex Joni and EMC1 driver
*         by Paul C.
* License: GPL Version 2
*
* Copyright (c) 2006 All rights reserved.
* see below for additional notes
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/

/** This is the driver for Vigilant Technologies (VTI) ENCDAC controller.
	The board includes 4 channels of quadrature encoder input,
	4 channels of analog input and output, 8 bits digital I/O plus up to
	128 bits digital I/O on digital expander module, and three
	timers with interrupt.

	Installation of the driver only realtime:

	insmod hal_vti num_chan=4 dio="IOiooi"
						   BNF -> dio="*nI|O|ii|io|oi|oo"
						   where n=number of enabled I/O ports
		- autodetects the address
	or

	insmod hal_vti base=0x200 num_chan=4 dio="I|O|ii|io|oi|oo..."

	Check your Hardware manual for your base address (ISA bus only).

	The digital inputs/outputs configuration is determined by a
	config string passed to insmod when loading the module.
	The format consists of a character string that sets the
	direction of each group of pins. Each character or character pair
	of the direction string is one of "I", "O", "ii", "io", "oi" or "oo". 
	The individual and character pair formats may be used interchangably in 
	the same string, however the lower case format must always appear in 
	pairs. The representatiom of each character or character pair is as follows:

	  I: 8 (0..7)  Inputs
	  O: 8 (0..7)  Outputs
	  ii: 4 (0..3) Inputs /  4 (4..7) Inputs
	  io: 4 (0..3) Inputs /  4 (4..7) Outputs
	  oi: 4 (0..3) Outputs / 4 (4..7) Inputs
	  oo: 4 (0..3) Outputs / 4 (4..7) Outputs

	There are characters or character pairs equal to the number of I/O
	ports (sets of 8 DIO) configured on the ENCDAC board and the expanded
	I/O module, where the first port is for the I/O on the ENCDAC board
	itself.

	The following items are exported to the HAL.

	Encoders:
	  Parameters:
	float	vti.<channel>.enc-scale

	  Pins:
	s32	vti.<channel>.enc-counts
	float	vti.<channel>.enc-position

/todo   bit	vti.<channel>.enc-index
/todo  	bit	vti.<channel>.enc-idx-latch
/todo  	bit	vti.<channel>.enc-latch-index
/todo  	bit	vti.<channel>.enc-reset-count

	  Functions:
		void    vti.<channel>.capture_position

	DACs:
	  Parameters:
	float	vti.<channel>.dac-offset
	float	vti.<channel>.dac-gain

	  Pins:
	float	vti.<channel>.dac-value

	  Functions:
	void    vti.<channel>.dac-write

	ADC:
	  Parameters:
/totest	float	vti.<channel>.adc-offset
/totest	float	vti.<channel>.adc-gain

	  Pins:
/totest	float	vti.<channel>.adc-value

	  Functions:
/totest	void    vti.<channel>.adc-read

	Digital In:
	  Pins:
	bit	vti.in-<pinnum>
	bit	vti.in-<pinnum>-not

	  Functions:
	void    vti.digital-in-read

	Digital Out:
	  Parameters:
	bit	vti.out-<pinnum>-invert

	  Pins:
	bit	vti.out-<pinnum>

	  Functions:
	void    vti.digital-out-write

*/

/** Copyright (C) 2006 Eric Johnson
					   <ejohnson AT aaainc DOT com>
*/

/** Copyright (C) 2004 Alex Joni
					   <alex DOT joni AT robcon DOT ro>
*/

/** Copyright (C) 2003 John Kasunich
					   <jmkasunich AT users DOT sourceforge DOT net>
*/

/* Also relates to the EMC1 code (very similar to STGMEMBS.CPP)
	work done by Fred Proctor, Will Shackleford */

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

#include <asm/io.h>
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include <linux/pci.h>
#include "hal.h"		/* HAL public API decls */
#include "hal_vti.h"		/* VTI related defines */

#ifndef MODULE
#define MODULE
#endif

#ifdef MODULE
/* module information */
MODULE_AUTHOR("Eric Johnson");
MODULE_DESCRIPTION
    ("Driver for Vigilant Technologies ENCDAC 4 channel controller");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
static int num_chan = MAX_CHANS;	/* number of channels - default = 8 */
MODULE_PARM(num_chan, "i");
MODULE_PARM_DESC(num_chan, "number of channels");
static long period = 0;		/* thread period - default = no thread */
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "thread period (nsecs)");
static char *dio = "ii";	/* dio config - default = port A&B inputs, port C&D outputs */
MODULE_PARM(dio, "s");
MODULE_PARM_DESC(dio, "dio config string - expects something like IOiooi");
#endif /* MODULE */

/***********************************************************************
 *                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

typedef struct {
    hal_bit_t *data;		/* basic pin for input or output */
    union {
	hal_bit_t *not;		/* pin for inverted data (input only) */
	hal_bit_t invert;	/* param for inversion (output only) */
    } io;
} io_pin;

typedef struct {
/* counter data */
    hal_s32_t *count[MAX_CHANS];	/* captured binary count value */
    hal_float_t *pos[MAX_CHANS];	/* scaled position (floating point) */
    hal_float_t pos_scale[MAX_CHANS];	/* parameter: scaling factor for pos */

/* dac data */
    hal_float_t *dac_value[MAX_CHANS];	/* value to be written to dac */
    hal_float_t dac_offset[MAX_CHANS];	/* offset value for DAC */
    hal_float_t dac_gain[MAX_CHANS];	/* gain to be applied */

/* adc data */
    hal_float_t *adc_value[MAX_CHANS];	/* value to be read from adc */
    hal_float_t adc_offset[MAX_CHANS];	/* offset value for ADC */
    hal_float_t adc_gain[MAX_CHANS];	/* gain to be applied */
    int adc_current_chan;	/* holds the currently converting channel */

/* dio data */
    io_pin port[MAX_IO_PORTS][PINS_PER_PORT];	/* Holds MAX_IO_PORTS X PINS_PER_PORT
						   number of discreet I/O points */
    unsigned char dir_bits[MAX_IO_PORTS * 2];	/* remembers config (which port is input which is output) */

    unsigned char model;

} vti_struct;

static vti_struct *vti_driver;
struct pci_dev *dev = NULL;
struct pci_access *device;
volatile struct encoder *encoder = NULL;
volatile struct timer *timer = NULL;
volatile struct dac *dac = NULL;
volatile struct ip *ip = NULL;

/* other globals */
static int comp_id;		/* component ID */
static int outpinnum = 0, inputpinnum = 0;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
/* helper functions, to export HAL pins & co. */
static int export_counter(int num, vti_struct * addr);
static int export_dac(int num, vti_struct * addr);
static int export_adc(int num, vti_struct * addr);
static int export_dio_pins(int io_points);
static int export_pin(int num, int dir, vti_struct * addr);
static int export_input_pin(int pinnum, io_pin * pin);
static int export_output_pin(int pinnum, io_pin * pin);

/* Board specific functions */

/* initializes the vti, takes care of autodetection, all initialisations */
static int vti_init_card(void);
/* sets up interrupt to be used - unused for now*/
/*static int vti_set_interrupt(short interrupt);*/
/* scans possible addresses for vti cards */
static int vti_autodetect(void);

/* counter related functions */
static int vti_counter_init(int channels);
static long vti_counter_read(int i);

/* dac related functions */
static int vti_dac_init(int channels);
static int vti_dac_write(int ch, short value);

/* adc related functions */
static int vti_adc_init(int channels);
static int vti_adc_start(void *arg, unsigned short wAxis);
static short vti_adc_read(void *arg, int ch);

/* dio related functions */
static int vti_dio_init(int nibbles);
static int vti_parse_dio(void);

/* periodic functions registered to HAL */
static void vti_adcs_read(void *arg, long period);	//reads adc data from the board, check long description at the beginning of the function
static void vti_dacs_write(void *arg, long period);	//writes dac's to the vti
static void vti_counter_capture(void *arg, long period);	//captures encoder counters
static void vti_di_read(void *arg, long period);	//reads digital inputs from the vti
static void vti_do_write(void *arg, long period);	//writes digital outputs to the vti
static void RawDacOut(int axis, double volts);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_CHAN 8

int rtapi_app_main(void)
{
    int retval;

    /* test for number of channels */
    if ((num_chan <= 0) || (num_chan > MAX_CHAN)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: invalid num_chan: %d\n", num_chan);
	return -1;
    }

    /* test for config string */
    if ((dio == 0) || (dio[0] == '\0')) {
	rtapi_print_msg(RTAPI_MSG_ERR, "VTI: ERROR: no dio config string\n");
	return -1;
    }

    /* have good config info, connect to the HAL */
    comp_id = hal_init("hal_vti");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "VTI: ERROR: hal_init() failed\n");
	return -1;
    }

    /* allocate shared memory for vti data */
    vti_driver = hal_malloc(num_chan * sizeof(vti_struct));
    if (vti_driver == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "VTI: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* takes care of all initialisations, also autodetection and model if necessary */
    if ((retval=vti_init_card()) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: vti_init_card() failed\n");
	hal_exit(comp_id);
	return retval;
    }

    retval = vti_parse_dio();
    if (retval == -1) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: bad config info for port.\n");
	return -1;
    }
    export_dio_pins(retval);

    vti_dio_init(retval);

    /* init counter chip */
    if (vti_counter_init(num_chan) == -1) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: bad config info counter.\n");
	return -1;
    }

    /* init dac chip */
    vti_dac_init(num_chan);

    /* init adc chip */
    vti_adc_init(0);		// VTI controller has no ADCs

    /* export functions */
    retval = hal_export_funct("vti.capture-position", vti_counter_capture,
	vti_driver, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: vti.counter-capture funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"VTI: installed %d encoder counters\n", num_chan);

    retval = hal_export_funct("vti.write-dacs", vti_dacs_write,
	vti_driver, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: vti.write-dacs funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: installed %d dacs\n", num_chan);

    retval = hal_export_funct("vti.read-adcs", vti_adcs_read,
	vti_driver, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: vti.read-adcs funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: installed %d adcs\n", 0);

    retval = hal_export_funct("vti.di-read", vti_di_read,
	vti_driver, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: vti.di-read funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"VTI: installed %d digital inputs\n", inputpinnum);

    retval = hal_export_funct("vti.do-write", vti_do_write,
	vti_driver, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: vti.do-write funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"VTI: installed %d digital outputs\n", outpinnum);

    /* was 'period' specified in the insmod command? */
    if (period > 0) {
	/* create a thread */
	retval = hal_create_thread("vti.thread", period, 0);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"VTI: ERROR: could not create thread\n");
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: created %d uS thread\n",
		period / 1000);
	}
    }
    return 0;
}

static int vti_parse_dio(void)
{
    int i = 0, nibble = 0;

    while (i < strlen(dio)) {
	switch (dio[i]) {
	case 'I':
	    vti_driver->dir_bits[nibble] = 0;
	    vti_driver->dir_bits[nibble + 1] = 0;
	    break;
	case 'O':
	    vti_driver->dir_bits[nibble] = 1;
	    vti_driver->dir_bits[nibble + 1] = 1;
	    break;
	case 'i':
	    vti_driver->dir_bits[nibble] = 0;
	    i++;
	    if (dio[i] == 'i')
		vti_driver->dir_bits[nibble + 1] = 0;
	    else if (dio[i] == 'o')
		vti_driver->dir_bits[nibble + 1] = 1;
	    else
		return -1;
	    break;
	case 'o':
	    vti_driver->dir_bits[nibble] = 1;
	    i++;
	    if (dio[i] == 'i')
		vti_driver->dir_bits[nibble + 1] = 0;
	    else if (dio[i] == 'o')
		vti_driver->dir_bits[nibble + 1] = 1;
	    else
		return -1;
	    break;
	default:
	    return -1;
	}
	nibble += 2;
	i++;
    }
    return nibble * 4;		// Number of IO points defined
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*            REALTIME ENCODER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static void vti_counter_capture(void *arg, long period)
{
    vti_struct *vti;
    int i;

    vti = arg;
    for (i = 0; i < num_chan; i++) {
	/* capture raw counts to latches */
	*(vti->count[i]) = vti_counter_read(i);
	/* scale count to make floating point position */
	*(vti->pos[i]) = *(vti->count[i]) * vti->pos_scale[i];
    }
    /* done */
}

/* vti_dacs_write() - writes all dac's to the board
	- calls vti_dac_write() */
static void vti_dacs_write(void *arg, long period)
{
    vti_struct *vti;
    float volts;
    short ncounts, i;

    vti = arg;
    for (i = 0; i < num_chan; i++) {
	/* scale the voltage to be written based on offset and gain */
	volts =
	    (*(vti->dac_value[i]) - vti->dac_offset[i]) * vti->dac_gain[i];
	/* compute the value for the DAC, the extra - in there is vti specific */
	ncounts = (short) ((-10.0 - volts) / 20.0 * 0x1FFF);
	/* write it to the card */
	vti_dac_write(i, ncounts);
    }
}

/* vti_adcs_read() - reads one adc at a time from the board to hal
	- calls vti_adc_read() */

/* long description :

	Because the conversion takes a while (first mux to the right channel ~5usecs,
	then start the conversion, and wait for it to finish ~16-20 usecs) for all the
	8 channels, it would be too much to start the conversion, wait for the result
	for all the 8 axes.
	Instead a different approach is chosen:
	- on the beginning of the function the conversion should already be done
	- it gets read and sent to HAL
	- the new channel gets mux'ed
	- and at the end of the function the new conversion is started, so that the data
	  will be available at the next run.
	This way 8 periods are needed to read 8 ADC's. It is possible to set the board
	to do faster conversions (AZ bit on INTC off), but that would make it less
	reliable (autozero takes care of temp. errors).*/
/*! \todo vti_ADC_Improvement (if any user requests it).
	Another improvement might be to let the user chose what channels he would like
	for ADC (having only 2 channels might speed things up considerably).
*/
static void vti_adcs_read(void *arg, long period)
{
    vti_struct *vti;
    float volts;
    short ncounts;
    int i;

    vti = arg;
    i = vti->adc_current_chan;
    if ((i >= 0) && (i < num_chan)) {
	/* we should have the conversion done for adc_num_chan */
	ncounts = vti_adc_read(vti, i);
	volts = 10.0 - (ncounts * 20.0 / 0x1FFF);
	*(vti->adc_value[i]) = volts * vti->adc_gain[i] - vti->adc_offset[i];
    }
    /* if adc_num_chan < 0, it's the first time this routine runs
       thus we don't have any ready data, we simply start the next conversion */
    if (vti->adc_current_chan++ >= num_chan)
	vti->adc_current_chan = 0;	//increase the channel, and roll back to 0 after all chans are done

    /* select the current channel with the mux, and start the conversion */
    vti_adc_start(vti, vti->adc_current_chan);
    /* the next time this function runs, the result should be available */
}

// helper function to extract the data out of a char and place it into HAL data
// written by JMK
static void split_input(unsigned char data, io_pin * dest, int num)
{
    int b;
    unsigned char mask;

    /* splits a byte into 'num' HAL pins (and their NOTs) */
    mask = 0x01;
    for (b = 0; b < num; b++) {
	if (data & mask) {
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

// helper function to extract the data out of HAL and place it into a char
// written by JMK
unsigned char build_output(io_pin * src, int num)
{
    int b;
    unsigned char data, mask;

    data = 0x00;
    mask = 0x01;
    /* assemble output byte for data port from 'num' source variables */
    for (b = 0; b < num; b++) {
	/* get the data, add to output byte */
	if (*(src->data)) {
	    if (!(src->io.invert)) {
		data |= mask;
	    }
	} else {
	    if ((src->io.invert)) {
		data |= mask;
	    }
	}
	mask <<= 1;
	src++;
    }
    return data;
}

static void vti_di_read(void *arg, long period)	//reads digital inputs from the vti
{
    vti_struct *vti;
    int i;

    vti = arg;
    /* Get ENCDAC onboard inputs */
    if (vti->dir_bits[0] == 0)
	split_input(encoder->DIO, &(vti->port[0][0]), 4);
    if (vti->dir_bits[1] == 0)
	split_input(encoder->DIO, &(vti->port[0][4]), 4);

    /* Get Extended I/O inputs */
    for (i = 1; i < MAX_IO_PORTS; i++) {
	if (vti->dir_bits[i * 2] == 0)
	    split_input(dac->DIO[i - 1], &(vti->port[i][0]), 4);
	if (vti->dir_bits[i * 2 + 1] == 0)
	    split_input(dac->DIO[i - 1], &(vti->port[i][4]), 4);
    }
}

static void vti_do_write(void *arg, long period)	//writes digital outputs to the vti
{
    vti_struct *vti;
    int i;
    vti = arg;
    /* Write ENCDAC onboard outputs */
    encoder->DIO = build_output(&(vti->port[0][0]), 8);
    /* Write Extended I/O outputs */
    for (i = 1; i < MAX_IO_PORTS; i++) {
	dac->DIO[i - 1] = build_output(&(vti->port[i][0]), 8);
    }

}

/***********************************************************************
*                      BOARD SPECIFIC FUNCTIONS                        *
*       execute board related things (write/read to/from the vti)      *
************************************************************************/

/***********************************************************************
*                            INIT FUNCTIONS                            *
************************************************************************/

/*
  vti_counter_init() - Initializes the channel

  works the same for both cards (vti & STG2)
*/
static int vti_counter_init(int counters)
{

    int i, retval=0;
    /* export all the variables for each counter, dac */
    for (i = 0; i < counters; i++) {
	/* export all vars */
	retval = export_counter(i, vti_driver);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"VTI: ERROR: counter %d var export failed\n", i + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	*(vti_driver->count[i]) = 0;
	*(vti_driver->pos[i]) = 0.0;
	vti_driver->pos_scale[i] = 1.0;
    }
    return 0;
}

static int vtiDacwrite(int axis, double volts)
{
    if ((axis > MAX_CHANS) || (axis < 0)) {
	return -1;
    }
    dac->mode;			// Set DACs to individual update mode
    RawDacOut(axis, volts);
    return 0;
}


/*
  vti_dac_init() - Initializes the dac channel

  works the same for both cards (vti & STG2)
*/
static int vti_dac_init(int channels)
{
    int retval, i;
    for (i = 0; i < channels; i++) {
	retval = export_dac(i, vti_driver);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"VTI: ERROR: dac %d var export failed\n", i + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	*(vti_driver->dac_value[i]) = 0;
	vti_driver->dac_offset[i] = 0.0;
	vti_driver->dac_gain[i] = 1.0;
	vtiDacwrite(i, 0.0);
    }
    return 0;
}


/*  vti_adc_init() - Initializes the adc channel */
static int vti_adc_init(int channels)
{
    int i, retval=0;
    for (i = 0; i < channels; i++) {
	retval = export_adc(i, vti_driver);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"VTI: ERROR: adc %d var export failed\n", i + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	*(vti_driver->adc_value[i]) = 0;
	vti_driver->adc_offset[i] = 0.0;
	vti_driver->adc_gain[i] = 1.0;
	vti_driver->adc_current_chan = -1;	/* notify that no conversion has been started yet */
    }
    return 0;
}

static int vti_dio_init(int nibbles)
{
    unsigned int mask;
    int i;
    /* we will select the directions of each port */
    /* First initialize the 8 on board I/O points */
    encoder->DIO = 0;		// Turn all inputs / outputs off
    mask = encoder->Interrupt;
    mask &= 0xfffffcff;		// Mask off direction bits
    if (vti_driver->dir_bits[0] == 1)
	mask |= 0x00000100;	// Set mask for bits 0-3
    if (vti_driver->dir_bits[1] == 1)
	mask |= 0x00000200;	// Set mask for bits 4-7
    encoder->Interrupt = mask;
    /* Now initialize all extended I/O */
    for (i = 0; i < 0x10; i++) {
	dac->DIO[i] = 0;	// Turn all extended I/O off
    }

    mask = 0;
    for (i = 0; i < 16; i++) {	// Set direction for extended I/O points 0..63
	if (vti_driver->dir_bits[i + 2] == 1)
	    mask |= (1 << i);
    }
    dac->config0 = mask;
    mask = 0;
    for (i = 0; i < 16; i++) {	// Set direction for extended I/O points 64..127
	if (vti_driver->dir_bits[i + 18] == 1)
	    mask |= (1 << i);
    }
    dac->config1 = mask;
    return 0;
}

/***********************************************************************
*                          ACTION FUNCTIONS                            *
*            these do the actual data exchange with the board          *
************************************************************************/

/*
  vti_counter_read() - reads one channel
  FIXME - todo, extend to 32 bits in software

  works the same for both cards (vti & STG2)
*/
static long vti_counter_read(int axis)
{
    int status3 = encoder->Status;
    int status7 = 0;
    char overflow;
    char direction;
    Longword EncData;
    if ((axis > MAX_CHANS) || (axis < 0)) {
	return 0x80000000;	// Return encoder error value
    }
    overflow = (status7 << 4) | (status3 & 0x0f);
    direction = (status7 & 0x0f) | ((status3 & 0x0f) >> 4);
    EncData.Long = (long int)vti_driver->count[axis];
    if (status3 != encoder->Status) {
	if ((overflow >> axis) & 0x01) {
	    if ((direction >> axis) & 0x01) {
		EncData.Word[1] += 1;
	    } else {
		EncData.Word[1] -= 1;
	    }
	}
    }
    if (axis < 4)
	EncData.Word[0] = encoder->Counter[axis];
    else
//	EncData.Word[0] = ip->Counter[axis-4]  - I don't understand the address space layout for the IP module
	;
    return EncData.Long;
}

/*
  vti_dac_write() - writes a dac channel

  works the same for both cards (vti & STG2)
*/
static int vti_dac_write(int axis, short value)
{
    /* write the DAC */
    if ((axis > MAX_CHANS) || (axis < 0)) {
	return -1;
    }

    dac->mode;
    dac->dac[axis] = value;
    return 0;
}

int vti_adc_start(void *arg, unsigned short wAxis)
{
    return 0;			// VTI card has do ADCs
}

static short vti_adc_read(void *arg, int axis)
{

    return 0;			// VTI card has no ADCs
}

/***********************************************************************
*                       BOARD INIT FUNCTIONS                           *
*                  functions for autodetec, irq init                   *
************************************************************************/
/*
static int vti_set_interrupt(short interrupt)
{
    return 0;			// No interrupts necessary for VTI board
}*/

static int vti_init_card()
{
    int retval=vti_autodetect();
    if (retval == 0) {
	encoder = (volatile struct encoder *)
	    ioremap(pci_resource_start(dev, 2), sizeof(encoder));
	dac =
	    (volatile struct dac *) ioremap(pci_resource_start(dev,
		4), sizeof(dac));
	timer =
	    (volatile struct timer *) ioremap(pci_resource_start(dev,
		3), sizeof(timer));
	ip = (volatile struct ip *) ioremap(pci_resource_start(dev, 5),
	    sizeof(ip));
    } else {
	return (retval);
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: Encoders mapped to : %x2\n",
	(int) encoder);
    encoder->Status = 0;
    encoder->Reset = 0;
    // all ok
    return 0;
}

/* scans possible addresses for vti cards */
static int vti_autodetect()
{
    dev = pci_find_device(VENDOR, DEVICE, dev);
    if (dev) {
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "VTI: Card detected in slot: %2x\n", PCI_SLOT(dev->devfn));
	return (0);
    } else {
	rtapi_print_msg(RTAPI_MSG_INFO, "VTI: Exiting with auto detect failed\n");
	return (-ENODEV);
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
*     these are functions used for exporting various HAL pins/prams    *
************************************************************************/
static int export_counter(int num, vti_struct * addr)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);
    /* export pin for counts captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.counts", num);
    retval = hal_pin_s32_new(buf, HAL_WR, &addr->count[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.position", num);
    retval = hal_pin_float_new(buf, HAL_WR, &addr->pos[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.position-scale", num);
    retval = hal_param_float_new(buf, HAL_WR, &addr->pos_scale[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_dac(int num, vti_struct * addr)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);
    /* export pin for voltage received by the board() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.dac-value", num);
    retval = hal_pin_float_new(buf, HAL_RD, &addr->dac_value[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for offset */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.dac-offset", num);
    retval =
	hal_param_float_new(buf, HAL_WR, &addr->dac_offset[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for gain */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.dac-gain", num);
    retval = hal_param_float_new(buf, HAL_WR, &addr->dac_gain[num], comp_id);
    if (retval != 0) {
	return retval;
    }

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_adc(int num, vti_struct * addr)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);
    /* export pin for voltage received by the board() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.adc-value", num);
    retval = hal_pin_float_new(buf, HAL_WR, &addr->adc_value[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for offset */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.adc-offset", num);
    retval =
	hal_param_float_new(buf, HAL_WR, &addr->adc_offset[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for gain */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.%d.adc-gain", num);
    retval = hal_param_float_new(buf, HAL_WR, &addr->adc_gain[num], comp_id);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_dio_pins(int io_points)
{
    int i, msg, retval=0;
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);
    for (i = 0; i < io_points; i++) {
	/*          point, direction, driver */
	retval |= export_pin(i, vti_driver->dir_bits[i / 4], vti_driver);
    }
    /* restore saved message level */ rtapi_set_msg_level(msg);
    return retval;
}

static int export_pin(int num, int dir, vti_struct * addr)
{
    int retval;
    if (dir != 0)
	retval =
	    export_output_pin(outpinnum++, &(addr->port[num / 8][num % 8]));
    else
	retval =
	    export_input_pin(inputpinnum++, &(addr->port[num / 8][num % 8]));
    if (retval != 0)
	return retval;
    return 0;
}
static int export_input_pin(int pinnum, io_pin * pin)
{
    char buf[HAL_NAME_LEN + 2];
    int retval;
    /* export read only HAL pin for input data */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.in-%02d", pinnum);
    retval = hal_pin_bit_new(buf, HAL_WR, &(pin->data), comp_id);
    if (retval != 0)
	return retval;
    /* export additional pin for inverted input data */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.in-%02d-not", pinnum);
    retval = hal_pin_bit_new(buf, HAL_WR, &(pin->io.not), comp_id);
    /* initialize HAL pins */
    *(pin->data) = 0;
    *(pin->io.not) = 1;
    return retval;
}
static int export_output_pin(int pinnum, io_pin * pin)
{
    char buf[HAL_NAME_LEN + 2];
    int retval;
    /* export read only HAL pin for output data */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.out-%02d", pinnum);
    retval = hal_pin_bit_new(buf, HAL_RD, &(pin->data), comp_id);
    if (retval != 0)
	return retval;
    /* export parameter for polarity */
    rtapi_snprintf(buf, HAL_NAME_LEN, "vti.out-%02d-invert", pinnum);
    retval = hal_param_bit_new(buf, HAL_WR, &(pin->io.invert), comp_id);
    /* initialize HAL pin and param */
    *(pin->data) = 0;
    pin->io.invert = 0;
    return retval;
}
static void RawDacOut(int axis, double volts)
{
    unsigned short rawVolts;
    /* convert volts to unsigned hex
       -10V = 0x0000
       0V = 0x0800
       10V = 0x0fff */
    if (volts > 10)
	volts = 10;
    if (volts < -10)
	volts = -10;
    rawVolts = ((volts / 10) * 0x07ff) + 0x800;	// convert to hex
    dac->dac[axis] = rawVolts;
}
