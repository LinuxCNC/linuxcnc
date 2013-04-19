/********************************************************************
* Description:  hal_tiro.c
*               This is the driver for an ISA (PC104) encoder reading
*               board.
*
* Author: Alex Joni
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/

/** This is the driver for an ISA (PC104) encoder reading board.
    The board includes up to 4 channels of LS7166 chips for counting
    quadrature encoders. Schematics of the board will be included on
    my webpage www.juve.ro.
    
    Installation of the driver only realtime:
    
    insmod hal_tiro base=0x300 num_chan=4
    
    This code can to some extent be used for the DRO board (with minor
    adjustments). If it is required a DRO driver will follow.
*/

/** Copyright (C) 2004 Alex Joni
                       <alex DOT joni AT robcon DOT ro>
*/

/** Copyright (C) 2003 John Kasunich
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
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

/* module information */
MODULE_AUTHOR("Alex Joni");
MODULE_DESCRIPTION("Driver for Tiro-PC104 board for EMC HAL");
MODULE_LICENSE("GPL");
static int base = 0x300;	/* board base address */
RTAPI_MP_INT(base, "board base address");
static int num_chan = 4;	/* number of channels - default = 4 */
RTAPI_MP_INT(num_chan, "number of channels");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data for a single counter */

typedef struct {
    hal_s32_t *count;		/* captured binary count value */
    hal_float_t *pos;		/* scaled position (floating point) */
    hal_float_t pos_scale;	/* parameter: scaling factor for pos */
} counter_t;

/* pointer to array of counter_t structs in shmem, 1 per counter */
static counter_t *counter_array;

/* other globals */
static int comp_id;								/* component ID */

#define DATA(x) (base + (2 * x))  	/* Address of Data register */
#define CTRL(x) (base + (2 * x) + 1) 	/* Address of Control register */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_counter(int num, counter_t * addr);
static void capture(void *arg, long period);
static int LS7166Init(int ch);
static long LS7166Read(int i);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_CHAN 4

int rtapi_app_main(void)
{
    int n, retval;

    /* test for number of channels */
    if ((num_chan <= 0) || (num_chan > MAX_CHAN)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIRO: ERROR: invalid num_chan: %d\n", num_chan);
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("hal_tiro");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "TIRO: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for counter data */
    counter_array = hal_malloc(num_chan * sizeof(counter_t));
    if (counter_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIRO: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the variables for each counter */
    for (n = 0; n < num_chan; n++) {
		/* export all vars */
		retval = export_counter(n, &(counter_array[n]));
		if (retval != 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,
			"TIRO: ERROR: counter %d var export failed\n", n + 1);
			hal_exit(comp_id);
			return -1;
		}
		/* init counter */
		*(counter_array[n].count) = 0;
		*(counter_array[n].pos) = 0.0;
		counter_array[n].pos_scale = 1.0;
		
		/* init counter chip */		
		LS7166Init(n);
    }
    /* export functions */
    retval = hal_export_funct("tiro.capture-position", capture,
	counter_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIRO: ERROR: capture funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"TIRO: installed %d encoder counters\n", num_chan);
    hal_ready(comp_id);
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
    counter_t *cntr;
    int n;

    cntr = arg;
    for (n = 0; n < num_chan; n++) {

	/* capture raw counts to latches */
	*(cntr->count) = LS7166Read(n);
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
  LS7166Init() - Initializes the channel
*/

int LS7166Init(int ch)
{
	rtapi_outb(CTRL(ch), 0x49);
	rtapi_outb(CTRL(ch), 0xC3);
	rtapi_outb(CTRL(ch), 0x80);
	return 0;
}


/*
  LS7166Read() - reads one channel
*/
long LS7166Read(int i)
{
  union pos_tag {
    long l;
    struct byte_tag { char b0; char b1; char b2; char b3;} byte;
  } pos;

  rtapi_outb(CTRL(i), 0x03);
  pos.byte.b0=rtapi_inb(DATA(i));
  pos.byte.b1=rtapi_inb(DATA(i));
  pos.byte.b2=rtapi_inb(DATA(i));
  if (pos.byte.b2 < 0) {
    pos.byte.b3 = -1;
  }
  else {
    pos.byte.b3 = 0;
  }
  return pos.l;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_counter(int num, counter_t * addr)
{
    int retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export pin for counts captured by update() */
    retval = hal_pin_s32_newf(HAL_OUT, &(addr->count), comp_id,
			      "tiro.%d.counts", num);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    retval = hal_pin_float_newf(HAL_OUT, &(addr->pos), comp_id,
				"tiro.%d.position", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    retval = hal_param_float_newf(HAL_RW, &(addr->pos_scale), comp_id,
				  "tiro.%d.position-scale", num);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
