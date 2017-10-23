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
	float	vti.<channel>.position-scale

	  Pins:
	s32	vti.<channel>.counts
	float	vti.<channel>.position

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

#include <asm/io.h>
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include <linux/pci.h>
#include "hal.h"		/* HAL public API decls */
#include "hal_vti.h"		/* VTI related defines */

/* module information */
MODULE_AUTHOR("Eric Johnson");
MODULE_DESCRIPTION
    ("Driver for Vigilant Technologies ENCDAC 4 channel controller");
MODULE_LICENSE("GPL");
static int num_chan = MAX_CHANS;	/* number of channels - default = 4 */
RTAPI_MP_INT(num_chan, "number of channels");
static char *dio = "ii";	/* dio config - default = port A&B inputs, port C&D outputs */
RTAPI_MP_STRING(dio, "dio config string - expects something like IOiooi");

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
static int diocount = 0;
static hal_s32_t enc_counts[MAX_CHANS];

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
/* helper functions, to export HAL pins & co. */
static int export_counter(int num, vti_struct * addr);
static int export_dac(int num, vti_struct * addr);
// static int export_adc(int num, vti_struct * addr);
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
// static int vti_adc_start(void *arg, unsigned short wAxis);
// static short vti_adc_read(void *arg, int ch);

/* dio related functions */
static int vti_dio_init(int nibbles);
static int vti_parse_dio(void);

/* periodic functions registered to HAL */
static void vti_adcs_read(void *arg, long period);	//reads adc data from the board, check long description at the beginning of the function
static void vti_dacs_write(void *arg, long period);	//writes dac's to the vti
static void vti_counter_capture(void *arg, long period);	//captures encoder counters
static void vti_di_read(void *arg, long period);	//reads digital inputs from the vti
static void vti_do_write(void *arg, long period);	//writes digital outputs to the vti

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

    diocount = vti_parse_dio();
    if (diocount == -1) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VTI: ERROR: bad config info for port.\n");
	return -1;
    }
    export_dio_pins(diocount);

    vti_dio_init(diocount / 4);

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

    hal_ready(comp_id);
    return 0;
}

static int vti_parse_dio(void)
{
    int i = 0, nibble = 0;

    if (strlen(dio) == 0)
      return 0;
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
	if (vti->pos_scale[i] < 0.0) {
	  if (vti->pos_scale[i] > -EPSILON)
	    vti->pos_scale[i] = -1.0;}
	else {
	  if (vti->pos_scale[i] < EPSILON)
	    vti->pos_scale[i] = 1.0; }
	*(vti->pos[i]) = *(vti->count[i]) / vti->pos_scale[i];
    }
    /* done */
}

/* vti_dacs_write() - writes all dac's to the board
	- calls vti_dac_write() */
static void vti_dacs_write(void *arg, long period)
{
    vti_struct *vti;
    double volts;
    unsigned short ncounts, i;

    vti = arg;
    for (i = 0; i < num_chan; i++) {
	/* scale the voltage to be written based on offset and gain */
	volts =
	    (*(vti->dac_value[i]) - vti->dac_offset[i]) * vti->dac_gain[i];
	/* compute the value for the DAC, the extra - in there is vti specific */
        ncounts = ((volts / 10) * 0x7fff) + 0x8000;
	
	/* write it to the card */
	vti_dac_write(i, ncounts);
    }
}

/* The VTI board has no ADCs. Procedure is retained only as a stub, should it be called from
   elsewhere in the application. */
   
static void vti_adcs_read(void *arg, long period)
{
    return;
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
    char latchedVal;

    vti = arg;
    if (diocount == 0) return; // No DIO enabled
    /* Get ENCDAC onboard inputs */
    latchedVal = encoder->DIO;
    if (vti->dir_bits[0] == 0)
	split_input(latchedVal, &(vti->port[0][0]), 4);
    if (vti->dir_bits[1] == 0)
	split_input(latchedVal, &(vti->port[0][4]), 4);
		
    /* Get Extended I/O inputs */
   if (diocount <= 8) return; // No extended I/O enabled
   for (i = 1; i < (diocount / 8); i++) {
   	latchedVal = dac->DIO[i - 1];
	if (vti->dir_bits[i * 2] == 0)
	    split_input(latchedVal, &(vti->port[i][0]), 4);
	if (vti->dir_bits[i * 2 + 1] == 0)
	    split_input(latchedVal, &(vti->port[i][4]), 4);
      }
}

static void vti_do_write(void *arg, long period)	//writes digital outputs to the vti
{
    vti_struct *vti;
    int i;
    
    vti = arg;
    /* Write ENCDAC onboard outputs */
    if (diocount == 0) return; // No DIO
    encoder->DIO = build_output(&(vti->port[0][0]), 8);
    if (diocount <= 8) return; // No extended I/O
    
    /* Write Extended I/O outputs */
     for (i = 1; i < diocount / 8; i++) {
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

/*
  vti_dac_init() - Initializes the dac channel

  works the same for both cards (vti & STG2)
*/
static int vti_dac_init(int channels)
{
    int retval, i;
    
    encoder->DAC = DAC_IND_MODE;  // Enable DACs for output indpendent of watchdog
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
	vti_dac_write(i, DAC_ZERO_VOLTS);
    }
    return 0;
}

/*  vti_adc_init() - Initializes the adc channel */
/*  VTI card has no ADCs.                        */

static int vti_adc_init(int channels)
{
    return 0;
}

static int vti_dio_init(int nibbles)
{
    unsigned int mask;
    int i;
    
    /* we will select the directions of each port */
    /* First initialize the 8 on board I/O points */
    if (diocount == 0) return 0; // No DIO
    encoder->DIO = 0;		// Turn all inputs / outputs off
    mask = encoder->Interrupt;
    mask &= 0xfffffcff;		// Mask off direction bits
    if (vti_driver->dir_bits[0] == 1)
	mask |= 0x00000100;	// Set mask for bits 0-3
    if (vti_driver->dir_bits[1] == 1)
	mask |= 0x00000200;	// Set mask for bits 4-7
    encoder->Interrupt = mask;
    /* Now initialize all extended I/O */
    if (diocount <= 8) return 0; // No extended I/O
    for (i = 0; i < (diocount - 8) / 8; i++) {
	dac->DIO[i] = 0;	// Turn all extended I/O off
    }

    mask = 0;
    for (i = 0; i < 8; i++) {	// Set direction for extended I/O points 0..63
	if (vti_driver->dir_bits[(i * 2) + 2] == 1)
	    mask |= (1 << i);
    }
    dac->config0 = mask;
    mask = 0;
    for (i = 0; i < 8; i++) {	// Set direction for extended I/O points 64..127
	if (vti_driver->dir_bits[(i * 2) + 18] == 1)
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
    unsigned int status;
    unsigned int count;
    Longword EncData;
    static long int lastCount;
//    static unsigned short lastdac;
    
    if ((axis >= MAX_CHANS) || (axis < 0)) {
	return 0x80000000;	// Return encoder error value
      }
    lastCount = enc_counts[axis];
    status = encoder->Status;       // latch status from vti board
    count = encoder->Counter[axis]; // latch count from vti board
    
    EncData.Long = (long int)enc_counts[axis];
    if (status & (1 << axis)) {
      if (status & (1 << (axis + 4))) {
	EncData.Word[1] += 1;  
        }
      else {
	EncData.Word[1] -= 1;  
        }
      }
    
    
    EncData.Word[0] = count;
//    Filter out spurious roll overs / roll unders    
    if ((EncData.Long - lastCount) > 0x7fff) 
      EncData.Word[1] -= 1;
    else 
      if ((lastCount - EncData.Long) > 0x7fff)
        EncData.Word[1] += 1;
    enc_counts[axis] = EncData.Long;
	
    return EncData.Long;
}

/*
  vti_dac_write() - writes a dac channel
*/
static int vti_dac_write(int axis, short value)
{
    short junk;
    /* write the DAC */
    if ((axis >= MAX_CHANS) || (axis < 0)) {
	return -1;
      }

    junk = dac->mode;         // Read from mode to trigger update dac immediately
    dac->dac[axis] = value;   // Write dac value
    
     return 0;
}

/* int vti_adc_start(void *arg, unsigned short wAxis)
{
    return 0;			// VTI card has do ADCs
} */

/* static short vti_adc_read(void *arg, int axis)
{

    return 0;			// VTI card has no ADCs
} */

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
    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: Encoders mapped to : %p\n",
	encoder);
    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: DACs mapped to : %p\n",
	dac);
    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: Timers mapped to : %p\n",
	timer);
    rtapi_print_msg(RTAPI_MSG_INFO, "VTI: Industry pack mapped to : %p\n",
	ip);
    encoder->Status = 0;
    encoder->Reset = 0;
    // all ok
    return 0;
}

/* scans possible addresses for vti cards */
static int vti_autodetect()
{
    dev = pci_get_device(VENDOR, DEVICE, dev);
    if (dev) {
       pci_dev_put(dev);
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
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);
    /* export pin for counts captured by update() */
    retval = hal_pin_s32_newf(HAL_OUT, &addr->count[num],
			      comp_id, "vti.%d.counts", num);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    retval = hal_pin_float_newf(HAL_OUT, &addr->pos[num],
				comp_id, "vti.%d.position", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    retval = hal_param_float_newf(HAL_RW, &addr->pos_scale[num],
				  comp_id, "vti.%d.position-scale", num);
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
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);
    /* export pin for voltage received by the board() */
    retval = hal_pin_float_newf(HAL_IN, &addr->dac_value[num],
				comp_id, "vti.%d.dac-value", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for offset */
    retval = hal_param_float_newf(HAL_RW, &addr->dac_offset[num],
				  comp_id, "vti.%d.dac-offset", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for gain */
    retval = hal_param_float_newf(HAL_RW, &addr->dac_gain[num],
				  comp_id, "vti.%d.dac-gain", num);
    if (retval != 0) {
	return retval;
    }

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

/* static int export_adc(int num, vti_struct * addr)
{
    return 0;  // No ADCs to export
} */

static int export_dio_pins(int io_points)
{
    int i, msg, retval=0;
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    if (io_points == 0) return 0;
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
    int retval;
    /* export read only HAL pin for input data */
    retval = hal_pin_bit_newf(HAL_OUT, &(pin->data),
			      comp_id, "vti.in-%02d", pinnum);
    if (retval != 0)
	return retval;
    /* export additional pin for inverted input data */
    retval = hal_pin_bit_newf(HAL_OUT, &(pin->io.not),
			      comp_id, "vti.in-%02d-not", pinnum);
    /* initialize HAL pins */
    *(pin->data) = 0;
    *(pin->io.not) = 1;
    return retval;
}
static int export_output_pin(int pinnum, io_pin * pin)
{
    int retval;
    /* export read only HAL pin for output data */
    retval = hal_pin_bit_newf(HAL_IN, &(pin->data),
			      comp_id, "vti.out-%02d", pinnum);
    if (retval != 0)
	return retval;
    /* export parameter for polarity */
    retval = hal_param_bit_newf(HAL_RW, &(pin->io.invert),
				comp_id, "vti.out-%02d-invert", pinnum);
    /* initialize HAL pin and param */
    *(pin->data) = 0;
    pin->io.invert = 0;
    return retval;
}
