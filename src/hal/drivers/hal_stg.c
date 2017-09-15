/********************************************************************
* Description:  hal_stg.c
*               This is the driver for Servo-To-Go Model I & II board.
*
* Author: Alex Joni
* License: GPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
* see below for aditional notes
*
* Last change: 
********************************************************************/

/** This is the driver for Servo-To-Go Model I & II board.
    The board includes 8 channels of quadrature encoder input,
    8 channels of analog input and output, 32 bits digital I/O,
    and an interval timer with interrupt.
        
    Installation of the driver only realtime:
    
	insmod hal_stg num_chan=8 dio="IIOO"
	    - autodetects the address
	or
    
	insmod hal_stg base=0x200 num_chan=8 dio="IIOO"
    
    Check your Hardware manual for your base address.

    The digital inputs/outputs configuration is determined by a 
    config string passed to insmod when loading the module.  
    The format consists by a four character string that sets the
    direction of each group of pins. Each character of the direction
    string is either "I" or "O".  The first character sets the
    direction of port A (Port A - DIO.0-7), the next sets 
    port B (Port B - DIO.8-15), the next sets port C (Port C - DIO.16-23), 
    and the fourth sets port D (Port D - DIO.24-31).
    
    The following items are exported to the HAL.
   
    Encoders:
      Parameters:
	float	stg.<channel>.position-scale  (counts per unit)
   
      Pins:
	s32	stg.<channel>.counts
	float	stg.<channel>.position

/todo   bit	stg.<channel>.index-enable
/todo  	bit	stg.<channel>.enc-reset-count
   
      Functions:
        void    stg.<channel>.capture_position
   
   
    DACs:
      Parameters:
	float	stg.<channel>.dac-offset
	float	stg.<channel>.dac-gain
   
      Pins:
	float	stg.<channel>.dac-value
   
      Functions:
	void    stg.<channel>.dac-write
   
   
    ADC:
      Parameters:
	float	stg.<channel>.adc-offset
	float	stg.<channel>.adc-gain
   
      Pins:
	float	stg.<channel>.adc-value
   
      Functions:
	void    stg.<channel>.adc-read
   
   
    Digital In:
      Pins:
	bit	stg.in-<pinnum>
	bit	stg.in-<pinnum>-not
   
      Functions:
	void    stg.digital-in-read
   
   
    Digital Out:
      Parameters:
	bit	stg.out-<pinnum>-invert
   
      Pins:
	bit	stg.out-<pinnum>
   
      Functions:
	void    stg.digital-out-write

*/

/** Copyright (C) 2004 Alex Joni
                       <alex DOT joni AT robcon DOT ro>
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/* Based on STGMEMBS.CPP from the Servo To Go Windows drivers 
    - Copyright (c) 1996 Servo To Go Inc and released under GPL Version 2 */
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
#include "hal.h"		/* HAL public API decls */
#include "hal_stg.h"		/* STG related defines */

/* module information */
MODULE_AUTHOR("Alex Joni");
MODULE_DESCRIPTION("Driver for Servo-to-Go Model I  & II for EMC HAL");
MODULE_LICENSE("GPL");
static int base = 0x00;		/* board base address, 0 means autodetect */
RTAPI_MP_INT(base, "board base address, don't use for autodetect");
static int model = 0;		/* board model, leave empty and autodetect */
RTAPI_MP_INT(model, "board model, use with caution. it overrides the detected model");
static int num_chan = MAX_CHANS;	/* number of channels - default = 8 */
RTAPI_MP_INT(num_chan, "number of channels");
static char *dio = "IIOO";		/* dio config - default = port A&B inputs, port C&D outputs */
RTAPI_MP_STRING(dio, "dio config string - expects something like IIOO");

#define	EPSILON		1e-20

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
    hal_s32_t *count[MAX_CHANS];		/* captured binary count value */
	hal_s32_t offset[MAX_CHANS];		/* offset to hold latched position from index pulse */
    hal_float_t *pos[MAX_CHANS];		/* scaled position (floating point) */
    hal_float_t pos_scale[MAX_CHANS];		/* parameter: scaling factor for pos */
    hal_bit_t *index_enable[MAX_CHANS];		/* pins for index homing */
    hal_bit_t *index_latch[MAX_CHANS];    /* value of the index latch for the axis */
//    hal_s32_t check_index[MAX_CHANS];           /* internal marker for two stage index pulse check */
    hal_bit_t *index_polarity[MAX_CHANS];       /* Polarity of index pulse */

/* dac data */
    hal_float_t *dac_value[MAX_CHANS];		/* value to be written to dac */
    hal_float_t dac_offset[MAX_CHANS];		/* offset value for DAC */
    hal_float_t dac_gain[MAX_CHANS];		/* gain to be applied */

/* adc data */
    hal_float_t *adc_value[MAX_CHANS];		/* value to be read from adc */
    hal_float_t adc_offset[MAX_CHANS];		/* offset value for ADC */
    hal_float_t adc_gain[MAX_CHANS];		/* gain to be applied */
    int adc_current_chan;			/* holds the currently converting channel */

/* dio data */
    io_pin port[4][8];				/* holds 4 ports each 8 pins, either input or output */
    unsigned char dir_bits;			/* remembers config (which port is input which is output) */
    
    unsigned char model;

} stg_struct;

static stg_struct *stg_driver;

/* other globals */
static int comp_id;		/* component ID */
static int outpinnum=0, inputpinnum=0;
//const int STG_MSG_LEVEL = RTAPI_MSG_ALL;
const int STG_MSG_LEVEL = RTAPI_MSG_INFO;

#define DATA(x) (base + (2 * x) - (x % 2))	/* Address of Data register 0 */
#define CTRL(x) (base + (2 * (x+1)) - (x % 2))	/* Address of Control register 0 */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
/* helper functions, to export HAL pins & co. */
static int export_counter(int num, stg_struct * addr);
static int export_dac(int num, stg_struct * addr);
static int export_adc(int num, stg_struct * addr);
static int export_pins(int num, int dir, stg_struct * addr);
static int export_input_pin(int pinnum, io_pin * pin);
static int export_output_pin(int pinnum, io_pin * pin);

/* Board specific functions */

/* initializes the STG, takes care of autodetection, all initialisations */
static int stg_init_card(void);
/* sets up interrupt to be used */
static int stg_set_interrupt(short interrupt);
/* scans possible addresses for STG cards */
static unsigned short stg_autodetect(void);

/* counter related functions */
static int stg_counter_init(int ch);
static long stg_counter_read(int i);
static void stg_counter_latch(int i);
static void stg1_select_index_axis(void *arg, unsigned int chan);
static void stg1_reset_index_latch(void *arg, unsigned int chan);
static unsigned short stg1_get_index_pulse_latch(void *arg, unsigned int chan);

static void stg2_reset_all_index_latches( void *arg );
static void stg2_select_index_axes( void *arg, unsigned char mask );
static unsigned char stg2_get_all_index_pulse_latches( void *arg );

/* dac related functions */
static int stg_dac_init(int ch);
static int stg_dac_write(int ch, short value);

/* adc related functions */
static int stg_adc_init(int ch);
static int stg_adc_start(void *arg, unsigned short wAxis);
static short stg_adc_read(void *arg, int ch);

/* dio related functions */
static int stg_dio_init(void);

/* periodic functions registered to HAL */
static void stg_adcs_read(void *arg, long period); //reads adc data from the board, check long description at the beginning of the function
static void stg_dacs_write(void *arg, long period); //writes dac's to the STG
static void stg_counter_capture(void *arg, long period); //captures encoder counters
static void stg_di_read(void *arg, long period); //reads digital inputs from the STG
static void stg_do_write(void *arg, long period); //writes digital outputs to the STG
//static void stg_debug_print( void *, long );

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_CHAN 8

int rtapi_app_main(void)
{
    int n, retval, mask, m;
    unsigned char dir_bits;
  int msg;

  msg = rtapi_get_msg_level();
  rtapi_set_msg_level( STG_MSG_LEVEL );

    /* test for number of channels */
    if ((num_chan <= 0) || (num_chan > MAX_CHAN)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: invalid num_chan: %d\n", num_chan);
	return -1;
    }

    /* test for config string */
    if ((dio == 0) || (dio[0] == '\0')) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: no dio config string\n");
	return -1;
    }

    /* have good config info, connect to the HAL */
    comp_id = hal_init("hal_stg");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: hal_init() failed\n");
	return -1;
    }

    /* allocate shared memory for stg data */
    stg_driver = hal_malloc(num_chan * sizeof(stg_struct));
    if (stg_driver == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* takes care of all initialisations, also autodetection and model if necessary */
    if ((retval=stg_init_card()) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg_init_card() failed\n");
	hal_exit(comp_id);
	return retval;
    }

    /* dio should be a string of 4 'I" or "O" characters */
    dir_bits = 0;
    mask = 0x01;
    for ( m = 0 ; m < 4 ; m++ ) {
	/* test character and set/clear bit */
	if ((dio[m] == 'i') || (dio[m] == 'I')) {
	    /* input, set mask bit to zero */
	    dir_bits &= ~mask;
	} else if ((dio[m] == 'o') || (dio[m] == 'O')) {
	    /* output, set mask bit to one */
	    dir_bits |= mask;
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STG: ERROR: bad config info for port %d\n", m);
	    return -1;
	}
	/* shift mask for next bit */
	mask <<= 1;
    }

    /* we now should have directions figured out, next is exporting the pins based on that */
    mask = 0x01;
    for ( m = 0 ; m < 4 ; m++ ) {
    
	/*          port, direction, driver */
	export_pins(m, (dir_bits & mask), stg_driver);

	/* shift mask for next bit */
	mask <<= 1;
    }
    stg_driver->dir_bits = dir_bits; /* remember direction of each port, will be used in the write / read functions */

    stg_dio_init();
    
    /* export all the variables for each counter, dac */
    for (n = 0; n < num_chan; n++) {
	/* export all vars */
	retval = export_counter(n, stg_driver);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STG: ERROR: counter %d var export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	*(stg_driver->count[n]) = 0;
	stg_driver->offset[n] = 0;
	*(stg_driver->pos[n]) = 0.0;

    /* By default the index pulse is not processed/used */
	*(stg_driver->index_enable[n]) = 0;

    /* Default polarity for the index pulse is active high */
    if( stg_driver->model == 1 )
    {
	    *(stg_driver->index_polarity[n]) = 1;
    }

    /* Default value for the index latch output is false */
	  *(stg_driver->index_latch[n]) = 0;

	stg_driver->pos_scale[n] = 1.0;

	/* init counter chip */
	stg_counter_init(n);
	
	retval = export_dac(n, stg_driver);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STG: ERROR: dac %d var export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	*(stg_driver->dac_value[n]) = 0;
	stg_driver->dac_offset[n] = 0.0;
	stg_driver->dac_gain[n] = 1.0;

	/* init dac chip */
	stg_dac_init(n);

	retval = export_adc(n, stg_driver);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STG: ERROR: adc %d var export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	*(stg_driver->adc_value[n]) = 0;
	stg_driver->adc_offset[n] = 0.0;
	stg_driver->adc_gain[n] = 1.0;
	
	stg_driver->adc_current_chan = -1; /* notify that no conversion has been started yet */

	/* init adc chip */
	stg_adc_init(n);
    }
    
    /* export functions */
    retval = hal_export_funct("stg.capture-position", stg_counter_capture,
	stg_driver, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg.counter-capture funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"STG: installed %d encoder counters\n", num_chan);

    retval = hal_export_funct("stg.write-dacs", stg_dacs_write,
	stg_driver, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg.write-dacs funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"STG: installed %d dacs\n", num_chan);

    retval = hal_export_funct("stg.read-adcs", stg_adcs_read,
	stg_driver, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg.read-adcs funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"STG: installed %d adcs\n", num_chan);

    retval = hal_export_funct("stg.di-read", stg_di_read,
	stg_driver, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg.di-read funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"STG: installed %d digital inputs\n", inputpinnum);

    retval = hal_export_funct("stg.do-write", stg_do_write,
	stg_driver, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg.do-write funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"STG: installed %d digital outputs\n", outpinnum);

  /*
  retval = hal_export_funct("stg.debug_print", stg_debug_print, stg_driver, 0, 0, comp_id);
  if (retval != 0)
  {
  	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg.debug_print funct export failed\n");
   	hal_exit(comp_id);
   	return -1;
  }
  rtapi_print_msg(RTAPI_MSG_INFO,
   	"STG: installed periodic debug print\n");
  */
    hal_ready(comp_id);

  /* restore saved message level */
  rtapi_set_msg_level(msg);

    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
    // FIXME, check for return code ?
  return;
}

/***********************************************************************
*            REALTIME ENCODER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static void stg_counter_capture(void *arg, long period)
{
  stg_struct *stg = arg;
    int n;
//  int msg;
  unsigned char mask;
  unsigned char index_pulse_latches;

  if( stg->model == 1 )
  {
    /*
     * STG Model 1, stg1
     */
    for( n = 0; n < num_chan; n++ ) 
    {
    	/* reset and then select current axes pair to be reset by index
       *  Because they index polarity is configurable for the stg2 card be select
       *  even for the odd axes */
      stg1_select_index_axis(arg, n);

      if (stg1_get_index_pulse_latch(arg, n)) 
      {
        *(stg->index_latch[n]) = 1;

      	if ( *(stg->index_enable[n]) == 1 ) 
        {
          // read the value without latching, latching was done on index
          // remember this as an offset, it will be substracted from nominal
          stg->offset[n] = stg_counter_read(n);
    			/* set index-enable false, so outside knows we found the index, and reset the position */
  				*(stg->index_enable[n]) = 0;

          /*
          msg = rtapi_get_msg_level();
          rtapi_set_msg_level( STG_MSG_LEVEL );
          rtapi_print_msg(RTAPI_MSG_DBG, "STG: Index pulse detected on channel %1d\n", n );
          rtapi_set_msg_level(msg);
          */
        } else {
          /* NOP, no action needed, since the selection of an index pair is just valid until the next
           *  pair is selected */   
		} 
      } else {
        *(stg->index_latch[n]) = 0;
      }

    }
  } else if( stg->model == 2 )
  {
    /*
     * STG Model 2, stg2
     */

    // Set IDLEN
    for( mask = 0, n = 0; n < num_chan; n++ ) 
    {
      if( *(stg->index_enable[n]) == 1 )
      {
        mask |= ( 1<<n );
      }
    }
    stg2_select_index_axes( arg, mask );
    
    // Read all latches
    index_pulse_latches = stg2_get_all_index_pulse_latches( arg );

    // set or reset index_latch
    // index-enable reset if needed
    for( n = 0; n < num_chan; n++ ) 
    {
      if( index_pulse_latches & (1<<n) )
      {
        *(stg->index_latch[n]) = 1;

      	if ( *(stg->index_enable[n]) == 1 ) 
        {
          // read the value without latching, latching was done on index
														// remember this as an offset, it will be substracted from nominal
          stg->offset[n] = stg_counter_read(n);
				/* set index-enable false, so outside knows we found the index, and reset the position */
				*(stg->index_enable[n]) = 0;

          /*
          msg = rtapi_get_msg_level();
          rtapi_set_msg_level( STG_MSG_LEVEL );
          rtapi_print_msg(RTAPI_MSG_DBG, "STG: Index pulse detected on channel %1d\n", n );
          rtapi_set_msg_level(msg);
          */
        } else {
          /* NOP, no action needed, since all index latches will be clearer for the next iteration anyway */
        }
      } else {
        *(stg->index_latch[n]) = 0;
			}
		}
    // Reset all latches
    stg2_reset_all_index_latches( arg );
		
  } else {
    // NOP, only models stg1 and stg2, thus should never be reached */
	}

  for (n = 0; n < num_chan; n++) 
  {
	/* capture raw counts to latches */
	stg_counter_latch(n);
	/* read raw count, and substract the offset (determined by indexed homing) */
	*(stg->count[n]) = stg_counter_read(n) - stg->offset[n]; 
	/* make sure scale isn't zero or tiny to avoid divide error */
	if (stg->pos_scale[n] < 0.0) {
	    if (stg->pos_scale[n] > -EPSILON)
		stg->pos_scale[n] = -1.0;
	} else {
	    if (stg->pos_scale[n] < EPSILON)
		stg->pos_scale[n] = 1.0;
	}
	/* scale count to make floating point position */
	*(stg->pos[n]) = *(stg->count[n]) / stg->pos_scale[n];
    }
    /* done */
  return;
}

/* stg_debug_print 
 *  run this function from a very slow, 
 *  e.g. 1sec thread and it will give some information 
 */
/*
static void stg_debug_print( void *arg, long period )
{
  stg_struct *stg=arg;
  int msg;
  static int counter;

  // model 2 encoder index registers
  unsigned char idlen_reg;
  unsigned char seldi_reg;

  //model 1 encoder index registers
  unsigned char intc_reg;

  msg = rtapi_get_msg_level();
  rtapi_set_msg_level( STG_MSG_LEVEL );

  if( stg->model == 1 ) 
  {
		intc_reg = inb(base + INTC);
    rtapi_print_msg(RTAPI_MSG_DBG, "STG: %04d: IXS1 is %s\n", counter, ( intc_reg & IXS1 ) ? "TRUE" : "FALSE" );    
    rtapi_print_msg(RTAPI_MSG_DBG, "STG: %04d: IXS0 is %s\n", counter, ( intc_reg & IXS0 ) ? "TRUE" : "FALSE" );
    rtapi_print_msg(RTAPI_MSG_DBG, "STG: %04d: IXLVL is active %s\n", counter, ( intc_reg & IXLVL ) ? "TRUE" : "FALSE" );

  } else if (stg->model == 2 ) 
  {
    idlen_reg = inb( base + IDLEN );
    seldi_reg = inb( base + SELDI );
      
    rtapi_print_msg(RTAPI_MSG_DBG, "STG: %04d: IDLEN is 0x%02x\n", counter, idlen_reg );
    rtapi_print_msg(RTAPI_MSG_DBG, "STG: %04d: SELDI is 0x%02x\n", counter, seldi_reg );

  } else {
  // NOP, should never be reached
  }

  // restore saved message level
  rtapi_set_msg_level(msg);

  counter++;

  return;
}
*/

/* stg_dacs_write() - writes all dac's to the board
    - calls stg_dac_write() */
static void stg_dacs_write(void *arg, long period)
{    
    stg_struct *stg;
    double volts;
    short ncounts, i;

    stg=arg;
    for (i=0;i < num_chan; i++) {
	/* scale the voltage to be written based on offset and gain */
	volts = (*(stg->dac_value[i]) - stg->dac_offset[i]) * stg->dac_gain[i];
        /* clamp the scaled voltage value to the -10V to 10V output range of the STG */
        if (volts < -10.0)
                volts = -10.0;
        if (volts > 10.0)
                volts = 10.0;
	/* compute the value for the DAC, the extra - in there is STG specific */
	ncounts = (short) ((((-10.0 - volts) * 0x1FFF) / 20.0) - 1 );
	/* write it to the card */	
	stg_dac_write(i, ncounts);	
    }
  return;
}

/* stg_adcs_read() - reads one adc at a time from the board to hal
    - calls stg_adc_read() */

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
/*! \todo STG_ADC_Improvement (if any user requests it).
    Another improvement might be to let the user chose what channels he would like
    for ADC (having only 2 channels might speed things up considerably).
*/
static void stg_adcs_read(void *arg, long period)
{    
    stg_struct *stg;
    double volts;
    short ncounts;
    int i;

    stg = arg;
    i = stg->adc_current_chan;
    if ((i >= 0) && (i < num_chan)) { 
	/* we should have the conversion done for adc_num_chan */
	ncounts = stg_adc_read(stg,i);
	volts = ncounts * 10.0 / 4096;
	*(stg->adc_value[i]) = volts * stg->adc_gain[i] - stg->adc_offset[i];
    }
    /* if adc_num_chan < 0, it's the first time this routine runs
       thus we don't have any ready data, we simply start the next conversion */
    if (stg->adc_current_chan++ >= num_chan) 
	stg->adc_current_chan=0; //increase the channel, and roll back to 0 after all chans are done

    /* select the current channel with the mux, and start the conversion */
    stg_adc_start(stg,stg->adc_current_chan);
    /* the next time this function runs, the result should be available */
  return;
}


// helper function to extract the data out of a char and place it into HAL data
// written by JMK
static void split_input(unsigned char data, io_pin *dest, int num)
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
  return;
}    

// helper function to extract the data out of HAL and place it into a char
// written by JMK
unsigned char build_output(io_pin *src, int num)
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


static void stg_di_read(void *arg, long period) //reads digital inputs from the STG
{
    stg_struct *stg;
    unsigned char val;
    stg=arg;
    
    if ( (stg->dir_bits & 0x01) == 0) { // if port A is set as input, read the bits
	if (stg->model == 1)
	    val = inb(base + DIO_A);
	else
	    val = inb(base + PORT_A);
	split_input(val, &(stg->port[0][0]), 8);
    }
    if ( (stg->dir_bits & 0x02) == 0) { // if port B is set as input, read the bits
	if (stg->model == 1)
    	    val = inb(base + DIO_B);
	else
	    val = inb(base + PORT_B);
	split_input(val, &(stg->port[1][0]), 8);
    }
    if ( (stg->dir_bits & 0x04) == 0) { // if port C is set as input, read the bits
	if (stg->model == 1)
	    val = inb(base + DIO_C);
	else
	    val = inb(base + PORT_C);
	split_input(val, &(stg->port[2][0]), 8);
    }
    if ( (stg->dir_bits & 0x08) == 0) { // if port D is set as input, read the bits
	if (stg->model == 1)
    	    val = inb(base + DIO_D);
	else
	    val = inb(base + PORT_D);
	split_input(val, &(stg->port[3][0]), 8);
    }
}

static void stg_do_write(void *arg, long period) //writes digital outputs to the STG
{
    stg_struct *stg;
    unsigned char val;
    stg=arg;

    if ( (stg->dir_bits & 0x01) != 0) { // if port A is set as output, write the bits
	val = build_output(&(stg->port[0][0]), 8);
	if (stg->model == 1)
	    outb(val, base + DIO_A);
	else
	    outb(val, base + PORT_A);
    }
    if ( (stg->dir_bits & 0x02) != 0) { // if port B is set as output, write the bits
	val = build_output(&(stg->port[1][0]), 8);
	if (stg->model == 1)
	    outb(val, base + DIO_B);
	else
	    outb(val, base + PORT_B);
    }
    if ( (stg->dir_bits & 0x04) != 0) { // if port C is set as output, write the bits
	val = build_output(&(stg->port[2][0]), 8);
	if (stg->model == 1)
	    outb(val, base + DIO_C);
	else
	    outb(val, base + PORT_C);
    }
    if ( (stg->dir_bits & 0x08) != 0) { // if port D is set as output, write the bits
	val = build_output(&(stg->port[3][0]), 8);
	if (stg->model == 1)
	    outb(val, base + DIO_D);
	else
	    outb(val, base + PORT_D);
    }
}

/***********************************************************************
*                      BOARD SPECIFIC FUNCTIONS                        *
*       execute board related things (write/read to/from the stg)      *
************************************************************************/


/***********************************************************************
*                            INIT FUNCTIONS                            *
************************************************************************/

/*
  stg_counter_init() - Initializes the channel
  
  works the same for both cards (STG & STG2)
*/
static int stg_counter_init(int ch)
{
    /* Set Counter Command Register - Master Control, Master Reset (MRST), */
    /* and Reset address pointer (RADR). */
    outb(0x23, CTRL(ch));

    /* Set Counter Command Register - Input Control, OL Load (P3), */
    /* and Enable Inputs A and B (INA/B). */
    outb(0x68, CTRL(ch));

    /* Set Counter Command Register - Output Control */
    outb(0x80, CTRL(ch));

    /* Set Counter Command Register - Quadrature */
    outb(0xC3, CTRL(ch));
    return 0;
}

/*
  stg_dac_init() - Initializes the dac channel

  works the same for both cards (STG & STG2)
*/
static int stg_dac_init(int ch)
{
    int i;
    
    /* set all DAC's to 0 on startup */
    for (i=0; i < num_chan; i++) {
	stg_dac_write(i, 0x1000); //by Xuecheng, 0x1000 coresponds to 0V 
    }
    return 0;
}


/*
  stg_adc_init() - Initializes the adc channel
*/
static int stg_adc_init(int ch)
{

    /* not much to setup for the ADC's */
    /* only select the mode of operation we will work with AutoZero */
    if (stg_driver->model == 1)
	outb(0x0f, base + MIO_2);	// the second 82C55 is already configured (by running stg_dio_init)
					// we only set bit 8 (AZ) to 1 to enable it
    return 0;
}


/*
  stg_dio_init() - Initializes the dio's
*/
static int stg_dio_init(void)
{
    /* we will select the directions of each port */
    unsigned char control, tempINTC, tempIMR, tempCtrl0, tempCtrl1;

    control = 0x80; //set up |1|0|0|A|CH|0|B|CL|
    if ( (stg_driver->dir_bits & 0x01) == 0) // if port A is set as input, set bit accordingly
	control |= 0x10;
    if ( (stg_driver->dir_bits & 0x02) == 0) // if port B is set as input, set bit accordingly
	control |= 0x02;
    if ( (stg_driver->dir_bits & 0x04) == 0) // if port C is set as input, set bits accordingly
	control |= 0x09;
    
    if (stg_driver->model == 1) {
	// write the computed control to MIO_1
	outb(control, base+MIO_1);
    } else { //model STG2
	// write port A,B,C direction to ABC_DIR
	outb(control, base+ABC_DIR);
    }
    
	tempINTC = inb(base + INTC);
    
    if (stg_driver->model == 1) {
	// next compute the directions for port D, located on the second 82C55
	control = 0x82;
    
	if ( (stg_driver->dir_bits & 0x08) == 0)// if port D is set as input, set bits accordingly
	    control = 0x92;

	tempIMR = inb(base + IMR); // get the current interrupt mask
        
	outb(0xff, base + OCW1); //mask off all interrupts
    
	// write the computed control to MIO_2
	outb(control, base+MIO_2);
    
	outb(tempINTC, base + INTC); //restore interrupt control reg.
    
	outb(tempIMR, base+ OCW1); //restore int mask

    } else { //model STG2
    
	// save contents of CNTRL0, it will get reinitialized
	tempCtrl0 = inb(base+CNTRL0);
	tempCtrl1 = inb(base+CNTRL1);
    
	// CNTRL0 output, BRDTST input, D output
	control = 0x82;

	if ( (stg_driver->dir_bits & 0x08) == 0)// if port D is set as input, set bits accordingly
	    control = 0x8b;
	
	outb(0xff, base + CNTRL1); // disable interrupts
	
	outb(control, base + D_DIR); // set port D direction, also resets CNTRL0
	
	outb(tempCtrl0, base + CNTRL0);
	outb( (tempCtrl1 & 0x0f) | 0xf0, base + CNTRL1);
    }
    
    return 0;
}


/***********************************************************************
*                          ACTION FUNCTIONS                            *
*            these do the actual data exchange with the board          *
************************************************************************/

static void stg_counter_latch(int i) 
{
    outb(0x03, CTRL(i));
}


/*
  stg_counter_read() - reads one channel
  FIXME - todo, extend to 32 bits in software

  works the same for both cards (STG & STG2)
*/
static long stg_counter_read(int i)
{
    union pos_tag {
	long l;
	struct byte_tag {
	    char b0;
	    char b1;
	    char b2;
	    char b3;
	} byte;
    } pos;

    pos.byte.b0 = inb(DATA(i));
    pos.byte.b1 = inb(DATA(i));
    pos.byte.b2 = inb(DATA(i));
    if (pos.byte.b2 < 0) {
	pos.byte.b3 = -1;
    } else {
	pos.byte.b3 = 0;
    }
    return pos.l;
}

static void stg1_select_index_axis(void *arg, unsigned int channel)
{
    stg_struct *stg = arg;
    unsigned char byIntc,byAxis;
  unsigned char byPol = 1;

  if (stg->model == 1)
  { 
    /*
     * Set polarity to low active if that is requested
     */
    if( *(stg->index_polarity[channel]) == 0 ) 
    {
      byPol = 0;
    }

    /* Stg manual p. 21: "The bits are level triggered and cannot be reset if they are active"
     *  So it is save to reset them and only those which are really active will remain */
    stg1_reset_index_latch(arg, channel);
  
    // routine for Model 1
		// initialize stuff to poll index pulse
		byAxis = channel;
		
		byAxis &= 0x6;						// ignore low bit, we check 2 axes at a time
		byAxis <<= 3;						// shift into position for IXS1, IXS0
		byIntc = inb(base + INTC);			// get a copy of INTC, we'll change
											// some bits in it, not all
		byIntc &= ~(IXLVL | IXS1 | IXS0);	// zero bits for axis and polarity
		byIntc |= byAxis;					// put axes address in INTC
		if (byPol != 0)					// is index pulse active high?
			byIntc |= IXLVL;
		outb(byIntc, base + INTC);
	}
}

static void stg2_select_index_axes( void *arg, unsigned char mask )
{
    /* stg2 manual, p.21
     *  writing 0 to the corresponding bit disables the index pulse
     *  writing 1 enables it
     */
    outb( mask, base + IDLEN );
    return;
}

/* Note that for stg1 cards this function will clear the index pulse latches for the two selected axes,
 */
static void stg1_reset_index_latch(void *arg, unsigned int channel)
{
  stg_struct *stg = arg;

  if (stg->model == 1)
  {   // routine for Model 1
   	inb(base + ODDRST);        //reset index pulse latch for ODD axis
   	inb(base + BRDTST);        //reset index pulse latch for EVEN axis
  }
  return;
}

static void stg2_reset_all_index_latches( void *arg )
{
  stg_struct *stg = arg;
  if( stg->model == 2 )
  {
    /*     
     * stg2 manual p.22, 
     *  writing 0 to IDL resets the index latch, 
     *  writing 1 has no effect
     */
    outb( 0x00, base + IDL);
  }
  return;
}

unsigned char stg1_get_current_IRR(void)
{
    outb(base + OCW3, 0x0a);           // IRR on next read
    return inb(base + IRR);
}

static unsigned short stg1_get_index_pulse_latch(void *arg, unsigned int chan)
{
    // routine for Model 1 board

    stg_struct *stg = arg;
    unsigned char byIRR, byAxisMask;

    if (stg->model == 1){   // routine for Model 1
		byIRR = stg1_get_current_IRR();
		byAxisMask = (chan & 1) ? LIXODD : LIXEVN;  // even or odd axis?
		if (byIRR & byAxisMask)                          // check latched index pulse
			return 1;
		return 0;
	} else if (stg->model == 2) {
		//FIXME: return if latched index pulse is there
		return 0;
	}
	return 0;
}

static unsigned char stg2_get_all_index_pulse_latches( void *arg )
{
  stg_struct *stg = arg;
  unsigned char indexRegister = 0;

  if( stg-> model == 2 )
    indexRegister = inb( base + IDL );
  return indexRegister;
}


/*
  stg_dac_write() - writes a dac channel
  
  works the same for both cards (STG & STG2)
*/
static int stg_dac_write(int ch, short value)
{        
    /* write the DAC */
    outw(value, base + DAC_0 + (ch << 1));

    return 0;
}



int stg_adc_start(void *arg, unsigned short wAxis)
{
    stg_struct *stg;
    unsigned char tempCtrl0;

    stg = arg;
    
    if (stg->model == 1) {
	/* do a dummy read from the ADC, just to set the input multiplexer to
	 the right channel */
	inw(base + ADC_0 + (wAxis << 1));

	/* wait 4 uS for settling time on the multiplexer and ADC. You probably
	 shouldn't really have a delay in a driver */
	outb(0, 0x80);
	outb(0, 0x80);
	outb(0, 0x80);
	outb(0, 0x80);

	/* now start conversion */
	outw(0, base + ADC_0 + (wAxis << 1));
    } else { //model STG2

	tempCtrl0 = inb(base+CNTRL0) & 0x07; // save IRQ
	tempCtrl0 |= (wAxis << 4) | 0x88; //autozero & cal cycle
	outb(tempCtrl0, base + CNTRL0); // select channel

	/* wait 4 uS for settling time on the multiplexer and ADC. You probably
	 shouldn't really have a delay in a driver */
	outb(0, 0x80);
	outb(0, 0x80);
	outb(0, 0x80);
	outb(0, 0x80);
	
	/* now start conversion */
	outw(0, base + ADC_0);
    }
    return 0;
};

static short stg_adc_read(void *arg, int axis)
{
    short j;
    stg_struct *stg;

    stg = arg;

    /*
    there must have been a delay between stg_adc_start() and 
    stg_adc_read(), of 19 usec if autozeroing (we are), 4 usecs 
    otherwise. In code that calls this, make sure you split these 
    calls up with some intervening code
    */


    if (stg->model == 1) {

	/* make sure conversion is done, assume polling delay is done.
	EOC (End Of Conversion) is bit 0x08 in IIR (Interrupt Request
	Register) of Interrupt Controller.  Don't wait forever though
	bail out eventually. */

	for (j = 0; !(inb(base + IRR) & 0x08) && (j < 1000); j++);
    
	j = inw(base + ADC_0 + (axis << 1));

    } else { //model 2

	for (j = 0; (inb(base + BRDTST) & 0x08) && (j < 1000); j++);
	
	j = inw(base + ADC_0 + (axis << 1));	
    
    }


    if (j & 0x1000)       /* is sign bit negative? */
	j |= 0xf000;      /* sign extend */
    else
	j &= 0xfff;       /* make sure high order bits are zero. */

    return j;
};

/***********************************************************************
*                       BOARD INIT FUNCTIONS                           *
*                  functions for autodetec, irq init                   *
************************************************************************/

static int stg_set_interrupt(short interrupt)
{
    unsigned char tempINTC;
    
    if (stg_driver->model == 1)
	tempINTC=0x80;
    else
	tempINTC=0x88;//also CAL low, don|t want ADC to calibrate
    
    switch (interrupt) {
	case 3: break;
	case 5: tempINTC |= 4;break;
	case 7: tempINTC |= 2;break;
	case 9: tempINTC |= 6;break;
	case 10: tempINTC |= 5;break;
	case 11: tempINTC |= 7;break;
	case 12: tempINTC |= 3;break;
	case 15: tempINTC |= 1;break;
	default: tempINTC |= 4;break;
    }        
    if (stg_driver->model == 1)
	outb(tempINTC, base + INTC);
    else
	outb(tempINTC, base + CNTRL0);

    return 0;
}

static int stg_init_card()
{
  int msg;

  msg = rtapi_get_msg_level();
  rtapi_set_msg_level( STG_MSG_LEVEL );

  /*
   * If both stg card model and base address are set
   *  then no autodetecting is necessary.
   * Else we need to autodetect
   */
  if ( (model != 0) && (base != 0) )
  {
  	stg_driver->model = model;  
  } else {
  	base = stg_autodetect();
    }
    
  /*
   * Now check if the settings for a card a ok
   */
  if ( (base == 0x00) || (stg_driver->model == 0) )
  {
  	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: no stg1 or stg2 card could be initialised\n");
	return -ENODEV;
    }

  if (stg_driver->model == 1) {
    /*
     * STG1
     */
	// initialize INTC as output
	outb(0x92, base +  MIO_2);
    
	stg_set_interrupt(5); // initialize it to smthg, we won't use it anyways
    
	outb(0x1a, base + ICW1); // initialize the 82C59 as single chip (STG docs say so:)
	outb(0x00, base + ICW2); // ICW2 not used, must init it to 0
	outb(0xff, base + OCW1); // mask off all interrupts
    rtapi_print_msg(RTAPI_MSG_INFO,
      "STG: Initialised stg%1d card at address %x\n", stg_driver->model, base);
  } else if (stg_driver->model == 2 ) { 
    /*
     * STG2
     */
	outb(0x8b, base + D_DIR); // initialize CONTRL0 output, BRDTST input
    
    /* stg2 manual, p.21
     *  writing 0 to the corresponding bit disables the index pulse
     *  writing 1 enables it
     */
    outb( 0x00, base + IDLEN );

    /* stg2 manual, p.21
     *  writing 0 to the corresponding bit selects the index pulse to latch the counter
     *  writing 1 to the corresponding bit selects EXLATCH to latch the counter
     */
    outb( 0x00, base + SELDI );

  	stg_set_interrupt(5); // initialize it to something, we won't use it anyways
    rtapi_print_msg(RTAPI_MSG_INFO,
      "STG: Initialised stg%1d card at address %x\n", stg_driver->model, base);
  } else {
  	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: The model stg%1d is not correct\n", stg_driver->model );
  	return -ENODEV;
    }
    
  /* restore saved message level */
  rtapi_set_msg_level(msg);

    // all ok
    return 0;
}

/* scans possible addresses for STG cards */
unsigned short stg_autodetect()
{

    short i, j, k, ofs;
    unsigned short address;
  unsigned short retval = 0;
  int msg;

  msg = rtapi_get_msg_level();
  rtapi_set_msg_level( STG_MSG_LEVEL );

    /* search all possible addresses */
    for (i = 15; i >= 0; i--) {
	address = i * 0x20 + 0x200;

	/* does jumper = i? */
	//if ((inb(address + BRDTST) & 0x0f) == i) { // by Xuecheng, not necessary
	    k = 0;		// var for getting the serial
	    for (j = 0; j < 8; j++) {
		ofs = (inb(address + BRDTST) >> 4);

		if (ofs & 8) {	/* is SER set? */
		    ofs = ofs & 7;	/* mask for Q2,Q1,Q0 */
		    k += (1 << ofs);	/* shift bit into position specified
					   by Q2, Q1, Q0 */
		}
	    }

	    if (k == 0x75) {
		rtapi_print_msg(RTAPI_MSG_INFO,
        "STG: Autodetected stg1 card at address %x\n", address);
		stg_driver->model=1;
      retval = address;	/* SER sequence is 01110101 */
      break;
	    }

	    if (k == 0x74) {
		rtapi_print_msg(RTAPI_MSG_INFO,
        "STG: Autodetected stg2 card at address %x\n", address);
		stg_driver->model=2;
      retval = address;
      break;
	    }
	//}
    }

  if ( ( retval == 0 ) || ( stg_driver->model == 0 ) )
  {
    rtapi_print_msg(RTAPI_MSG_ERR,
      "STG: stg_autodetect() did not find any stg1 or stg2 card\n");
  }

  /* restore saved message level */
  rtapi_set_msg_level(msg);

  return retval;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
*     these are functions used for exporting various HAL pins/prams    *
************************************************************************/
static int export_counter(int num, stg_struct *addr)
{
    int retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level( STG_MSG_LEVEL );

    /* export pin for counts captured by update() */
    retval = hal_pin_s32_newf(HAL_OUT, &addr->count[num],
			      comp_id, "stg.%d.counts", num);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    retval = hal_pin_float_newf(HAL_OUT, &addr->pos[num],
				comp_id, "stg.%d.position", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    retval = hal_param_float_newf(HAL_RW, &addr->pos_scale[num],
				  comp_id, "stg.%d.position-scale", num);
    if (retval != 0) {
	return retval;
    }

  /* export pin for index homing */
    retval = hal_pin_bit_newf(HAL_IO, &addr->index_enable[num],
			      comp_id, "stg.%d.index-enable", num);
    if (retval != 0) {
	return retval;
    }

  /* export pin for reading the index latch */
  retval = hal_pin_bit_newf(HAL_OUT, &addr->index_latch[num],
			    comp_id, "stg.%d.index-latch", num);
  if (retval != 0) {
    return retval;
  }


  /*
   * The index polarity is configurable for the stg1 cards only, 
   *  but not for the stg2 cards
   */
  if( addr->model == 1 ) 
  {
    /* export read only HAL pin for index pulse polarity */
    retval = hal_pin_bit_newf(HAL_IN, &addr->index_polarity[num],
			      comp_id, "stg.%d.index-polarity", num);
    if (retval != 0) 
    {
      return retval;
    }
  }

    /* restore saved message level */
    rtapi_set_msg_level(msg);

    return 0;
}

static int export_dac(int num, stg_struct *addr)
{
    int retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level( STG_MSG_LEVEL );

    /* export pin for voltage received by the board() */
    retval = hal_pin_float_newf(HAL_IN, &addr->dac_value[num],
				comp_id, "stg.%d.dac-value", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for offset */
    retval = hal_param_float_newf(HAL_RW, &addr->dac_offset[num],
				  comp_id, "stg.%d.dac-offset", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for gain */
    retval = hal_param_float_newf(HAL_RW, &addr->dac_gain[num],
				  comp_id, "stg.%d.dac-gain", num);
    if (retval != 0) {
	return retval;
    }

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_adc(int num, stg_struct *addr)
{
    int retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level( STG_MSG_LEVEL );

    /* export pin for voltage received by the board() */
    retval = hal_pin_float_newf(HAL_OUT, &addr->adc_value[num],
				comp_id, "stg.%d.adc-value", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for offset */
    retval = hal_param_float_newf(HAL_RW, &addr->adc_offset[num],
				  comp_id, "stg.%d.adc-offset", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for gain */
    retval = hal_param_float_newf(HAL_RW, &addr->adc_gain[num],
				  comp_id, "stg.%d.adc-gain", num);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_pins(int num, int dir, stg_struct *addr)
{
    int retval, msg, i;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level( STG_MSG_LEVEL );

    for (i=0; i<8; i++) {

	if (dir != 0)
	    retval=export_output_pin(outpinnum++, &(addr->port[num][i]) );
	else
	    retval=export_input_pin(inputpinnum++, &(addr->port[num][i]) );

	if (retval != 0) {
	    return retval;
	}
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_input_pin(int pinnum, io_pin * pin)
{
    int retval;
    int msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level( STG_MSG_LEVEL );

    /* export read only HAL pin for input data */
    retval = hal_pin_bit_newf(HAL_OUT, &(pin->data), comp_id,
			      "stg.in-%02d", pinnum);
    if (retval != 0) {
	return retval;
    }
    /* export additional pin for inverted input data */
    retval = hal_pin_bit_newf(HAL_OUT, &(pin->io.not), comp_id,
			      "stg.in-%02d-not", pinnum);
    /* initialize HAL pins */
    *(pin->data) = 0;
    *(pin->io.not) = 1;

    /* restore saved message level */
    rtapi_set_msg_level(msg);

    return retval;
}

static int export_output_pin(int pinnum, io_pin * pin)
{
    int retval;
    int msg;

  /*
   * This function exports a lot of stuff, which results in a lot of
   *  logging if msg_level is at INFO or ALL. So we save the current value
   *  of msg_level and restore it later.  If you actually need to log this
   *  function's actions, change the second line below 
   */
  msg = rtapi_get_msg_level();
  rtapi_set_msg_level( STG_MSG_LEVEL );

    /* export read only HAL pin for output data */
    retval = hal_pin_bit_newf(HAL_IN, &(pin->data),
			      comp_id, "stg.out-%02d", pinnum);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for polarity */
    retval = hal_param_bit_newf(HAL_RW, &(pin->io.invert),
				comp_id, "stg.out-%02d-invert", pinnum);
    /* initialize HAL pin and param */
    *(pin->data) = 0;
    pin->io.invert = 0;


    /* restore saved message level */
    rtapi_set_msg_level(msg);

    return retval;
}
