/** This is the driver for an Servo-To-Go Model I board.
    The board includes 8 channels of quadrature encoder input,
    8 channels of analog input and output, 32 bits digital I/O,
    and an interval timer with interrupt.
        
    Installation of the driver only realtime:
    
	insmod hal_stg num_chan=8
	    - autodetects the address
	or
    
	insmod hal_stg base=0x200 num_chan=8
    
    Check your Hardware manual for your base address.
    
    The following items are exported to the HAL.
   
    Encoders:
      Parameters:
/totest	float	stg.<channel>.enc-scale
   
      Pins:
/totest	s32	stg.<channel>.enc-counts
/totest	float	stg.<channel>.enc-position

/todo   bit	stg.<channel>.enc-index
/todo  	bit	stg.<channel>.enc-idx-latch
/todo  	bit	stg.<channel>.enc-latch-index
/todo  	bit	stg.<channel>.enc-reset-count
   
      Functions:
/totest void    stg.<channel>.capture_position
   
   
    DACs:
      Parameters:
/todo  	float	stg.<channel>.dac-offset
/todo  	float	stg.<channel>.dac-gain
   
      Pins:
/todo  	float	stg.<channel>.dac-value
   
      Functions:
/todo  	void    stg.<channel>.dac_write
   
   
    ADC:
      Parameters:
/todo  	float	stg.<channel>.adc-offset
/todo  	float	stg.<channel>.adc-gain
   
      Pins:
/todo  	float	stg.<channel>.adc-value
   
      Functions:
/todo  	void    stg.<channel>.adc_read
   
   
    Digital In:
      Pins:
/todo  	bit	stg.<channel>.pin-in
/todo  	bit	stg.<channel>.pin-in-not
   
      Functions:
/todo  	void    stg.<channel>.digital_in_read
   
   
    Digital Out:
      Parameters:
/todo  	bit	stg.<channel>.pin-out-invert
   
      Pins:
/todo  	bit	stg.<channel>.pin-out
   
      Functions:
/todo  	void    stg.<channel>.digital_out_write

*/

/** Copyright (C) 2004 Alex Joni
                       <alex DOT joni AT robcon DOT ro>
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

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include "hal_stg.h"		/* STG related defines */

#define FASTIO

#ifdef FASTIO
#define rtapi_inb inb
#define rtapi_outb outb
#ifdef RTAPI			/* for ULAPI, sys/io.h defines these functs */
#include <asm/io.h>
#endif
#endif

#ifndef MODULE
#define MODULE
#endif

#ifdef MODULE
/* module information */
MODULE_AUTHOR("Alex Joni");
MODULE_DESCRIPTION("Driver for Servo-to-Go Model I for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
static int base = 0x000;	/* board base address, 0 means autodetect */
MODULE_PARM(base, "i");
MODULE_PARM_DESC(base, "board base address, don't use for autodetect");
static int num_chan = 8;	/* number of channels - default = 8 */
MODULE_PARM(num_chan, "i");
MODULE_PARM_DESC(num_chan, "number of channels");
static long period = 0;		/* thread period - default = no thread */
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "thread period (nsecs)");
#endif /* MODULE */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data for a single counter */

typedef struct {
    hal_s32_t *count;		/* captured binary count value */
    hal_float_t *pos;		/* scaled position (floating point) */
    hal_float_t pos_scale;	/* parameter: scaling factor for pos */
} counter;

/* pointer to array of counter_t structs in shmem, 1 per counter */
static counter *counter_array;

/* other globals */
static int comp_id;		/* component ID */

#define DATA(x) (base + (2 * x) - (x % 2))	/* Address of Data register 0 
						 */
#define CTRL(x) (base + (2 * (x+1)) - (x % 2))	/* Address of Control
						   register 0 */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_counter(int num, counter * addr);
static void capture(void *arg, long period);

/* initializes the STG, takes care of autodetection, all initialisations */
static int stg_init_card(void);
/* scans possible addresses for STG cards */
unsigned short stg_autodetect(void);

int CNTInit(int ch);
long CNTRead(int i);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_CHAN 8

int rtapi_app_main(void)
{
    int n, retval;

    /* test for number of channels */
    if ((num_chan <= 0) || (num_chan > MAX_CHAN)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: invalid num_chan: %d\n", num_chan);
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("hal_stg");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for counter data */
    counter_array = hal_malloc(num_chan * sizeof(counter));
    if (counter_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* takes care of all initialisations, also autodetection if necessary */
    if (stg_init_card() != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: stg_init_card() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* export all the variables for each counter */
    for (n = 0; n < num_chan; n++) {
	/* export all vars */
	retval = export_counter(n, &(counter_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STG: ERROR: counter %d var export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	*(counter_array[n].count) = 0;
	*(counter_array[n].pos) = 0.0;
	counter_array[n].pos_scale = 1.0;

	/* init counter chip */
	CNTInit(n);
    }
    /* export functions */
    retval = hal_export_funct("stg.capture_position", capture,
	counter_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STG: ERROR: capture funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"STG: installed %d encoder counters\n", num_chan);
    /* was 'period' specified in the insmod command? */
    if (period > 0) {
	/* create a thread */
	retval = hal_create_thread("stg.thread", period, 0);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STG: ERROR: could not create thread\n");
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "STG: created %d uS thread\n",
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
*            REALTIME ENCODER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static void capture(void *arg, long period)
{
    counter *cntr;
    int n;

    cntr = arg;
    for (n = 0; n < num_chan; n++) {

	/* capture raw counts to latches */
	*(cntr->count) = CNTRead(n);
	/* scale count to make floating point position */
	*(cntr->pos) = *(cntr->count) * cntr->pos_scale;
	/* move on to next channel */
	cntr++;
    }
    /* done */
}

/***********************************************************************
*                      BOARD SPECIFIC FUNCTIONS                        *
************************************************************************/
/*
  CNTInit() - Initializes the channel
*/

int CNTInit(int ch)
{
    /* Set Counter Command Register - Master Control, Master Reset (MRST), */
    /* and Reset address pointer (RADR). */
    rtapi_outb(CTRL(ch), 0x23);

    /* Set Counter Command Register - Input Control, OL Load (P3), */
    /* and Enable Inputs A and B (INA/B). */
    rtapi_outb(CTRL(ch), 0x68);

    /* Set Counter Command Register - Output Control */
    rtapi_outb(CTRL(ch), 0x80);

    /* Set Counter Command Register - Quadrature */
    rtapi_outb(CTRL(ch), 0xC3);
    return 0;
}

/*
  CNTRead() - reads one channel
*/
long CNTRead(int i)
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

    rtapi_outb(CTRL(i), 0x03);
    pos.byte.b0 = rtapi_inb(DATA(i));
    pos.byte.b1 = rtapi_inb(DATA(i));
    pos.byte.b2 = rtapi_inb(DATA(i));
    if (pos.byte.b2 < 0) {
	pos.byte.b3 = -1;
    } else {
	pos.byte.b3 = 0;
    }
    return pos.l;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_counter(int num, counter * addr)
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
    rtapi_snprintf(buf, HAL_NAME_LEN, "stg.%d.counts", num);
    retval = hal_pin_s32_new(buf, HAL_WR, &(addr->count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stg.%d.position", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->pos), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stg.%d.position-scale", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->pos_scale), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int stg_init_card()
{
    if (base == 0x00) {
	base = stg_autodetect();
    }
    if (base == 0x00) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STG: ERROR: no STG card found\n");
	return -1;
    }
    // all ok
    return 0;
}

/* scans possible addresses for STG cards */
unsigned short stg_autodetect()
{

    short i, j, k, ofs;
    unsigned short address;

    /* search all possible addresses */
    for (i = 15; i >= 0; i--) {
	address = i * 0x20 + 0x200;

	/* does jumper = i? */
	if ((rtapi_inb(address + BRDTST) & 0x0f) == i) {
	    k = 0;		// var for getting the serial
	    for (j = 0; j < 8; j++) {
		ofs = (rtapi_inb(address + BRDTST) >> 4);

		if (ofs & 8) {	/* is SER set? */
		    ofs = ofs & 7;	/* mask for Q2,Q1,Q0 */
		    k += (1 << ofs);	/* shift bit into position specified
					   by Q2, Q1, Q0 */
		}
	    }
	    if (k == 0x75)
		return address;	/* SER sequence is 01110101 */
	    if (k == 0x74) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "STG: ERROR: found version 2 card, not suported by this driver\n");
		hal_exit(comp_id);
		return -1;
	    }
	}
    }
    return (0);
}
