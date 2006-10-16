/********************************************************************
* Description:  encoder.c
*               This file, 'encoder.c', is a HAL component that 
*               provides software based counting of quadrature 
*               encoder signals.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
# $Revision$
* $Author$
* $Date$
********************************************************************/
/** This file, 'encoder.c', is a HAL component that provides software
    based counting of quadrature encoder signals.  The maximum count
    rate will depend on the speed of the PC, but is expected to exceed
    1KHz for even the slowest computers, and may reach 10KHz on fast
    ones.  It is a realtime component.

    It supports up to eight counters, with optional index pulses.
    The number of counters is set by the module parameter 'num_chan'
    when the component is insmod'ed.

    The driver exports variables for each counters inputs and output.
    It also exports two functions.  "encoder.update-counters" must be
    called in a high speed thread, at least twice the maximum desired
    count rate.  "encoder.capture-position" can be called at a much
    slower rate, and updates the output variables.
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

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Encoder Counter for EMC HAL");
MODULE_LICENSE("GPL");
static int num_chan = 3;	/* number of channels - default = 3 */
RTAPI_MP_INT(num_chan, "number of channels");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data for a single counter */

typedef struct {
    unsigned char state;	/* quad decode state machine state */
    unsigned char oldZ;		/* previous value of phase Z */
    unsigned char Zmask;	/* mask for oldZ, based on index_ena */
    unsigned char pad;		/* padding for alignment */
    hal_s32_t raw_count;	/* parameter: raw binary count value */
    hal_bit_t *phaseA;		/* quadrature input */
    hal_bit_t *phaseB;		/* quadrature input */
    hal_bit_t *phaseZ;		/* index pulse input */
    hal_bit_t *index_ena;	/* index enable input */
    hal_bit_t *reset;		/* counter reset input */
    hal_s32_t *count;		/* captured binary count value */
    hal_float_t *pos;		/* scaled position (floating point) */
    hal_float_t *vel;		/* scaled velocity (floating point) */
    hal_float_t pos_scale;	/* parameter: scaling factor for pos */
    float old_scale;		/* stored scale value */
    double scale;		/* reciprocal value used for scaling */
    hal_bit_t x4_mode;		/* enables x4 counting (default) */
    hal_s32_t last_count;
    hal_s32_t last_index_count;
} counter_t;

/* pointer to array of counter_t structs in shmem, 1 per counter */
static counter_t *counter_array;

/* bitmasks for quadrature decode state machine */
#define SM_PHASE_A_MASK 0x01
#define SM_PHASE_B_MASK 0x02
#define SM_LOOKUP_MASK  0x0F
#define SM_CNT_UP_MASK  0x40
#define SM_CNT_DN_MASK  0x80

/* Lookup table for quadrature decode state machine.  This machine
   will reject glitches on either input (will count up 1 on glitch,
   down 1 after glitch), and on both inputs simultaneously (no count
   at all)  In theory, it can count once per cycle, in practice the
   maximum count rate should be at _least_ 10% below the sample rate,
   and preferrable around half the sample rate.  It counts every
   edge of the quadrature waveform, 4 counts per complete cycle.
*/
static const unsigned char lut_x4[16] = {
    0x00, 0x44, 0x88, 0x0C, 0x80, 0x04, 0x08, 0x4C,
    0x40, 0x04, 0x08, 0x8C, 0x00, 0x84, 0x48, 0x0C
};

/* same thing, but counts only once per complete cycle */

static const unsigned char lut_x1[16] = {
    0x00, 0x44, 0x08, 0x0C, 0x80, 0x04, 0x08, 0x0C,
    0x00, 0x04, 0x08, 0x0C, 0x00, 0x04, 0x08, 0x0C
};

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_counter(int num, counter_t * addr);
static void update(void *arg, long period);
static void capture(void *arg, long period);

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
	    "ENCODER: ERROR: invalid num_chan: %d\n", num_chan);
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("encoder");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "ENCODER: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for counter data */
    counter_array = hal_malloc(num_chan * sizeof(counter_t));
    if (counter_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "ENCODER: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the variables for each counter */
    for (n = 0; n < num_chan; n++) {
	/* export all vars */
	retval = export_counter(n, &(counter_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"ENCODER: ERROR: counter %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
	/* init counter */
	counter_array[n].state = 0;
	counter_array[n].oldZ = 0;
	counter_array[n].Zmask = 0;
	counter_array[n].raw_count = 0;
	*(counter_array[n].count) = 0;
	*(counter_array[n].pos) = 0.0;
	counter_array[n].pos_scale = 1.0;
	counter_array[n].old_scale = 1.0;
	counter_array[n].scale = 1.0;
	counter_array[n].x4_mode = 1;
    }
    /* export functions */
    retval = hal_export_funct("encoder.update-counters", update,
	counter_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "ENCODER: ERROR: count funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("encoder.capture-position", capture,
	counter_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "ENCODER: ERROR: capture funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"ENCODER: installed %d encoder counters\n", num_chan);
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

static void update(void *arg, long period)
{
    counter_t *cntr;
    int n;
    unsigned char state;

    cntr = arg;
    for (n = 0; n < num_chan; n++) {
	/* get state machine current state */
	state = cntr->state;
	/* add input bits to state code */
	if (*(cntr->phaseA)) {
	    state |= SM_PHASE_A_MASK;
	}
	if (*(cntr->phaseB)) {
	    state |= SM_PHASE_B_MASK;
	}
	/* look up new state */
	if ( cntr->x4_mode ) {
	    state = lut_x4[state & SM_LOOKUP_MASK];
	} else {
	    state = lut_x1[state & SM_LOOKUP_MASK];
	}
	/* should we count? */
	if (state & SM_CNT_UP_MASK) {
	    cntr->raw_count++;
	} else if (state & SM_CNT_DN_MASK) {
	    cntr->raw_count--;
	}
	/* save state machine state */
	cntr->state = state;
	/* get old phase Z state, make room for new bit value */
	state = cntr->oldZ << 1;
	/* add new value of phase Z */
	if (*(cntr->phaseZ)) {
	    state |= 1;
	}
	cntr->oldZ = state & 3;
	/* test for index enabled and rising edge on phase Z */
	if ((state & cntr->Zmask) == 1) {
	    /* reset counter, Zmask, and index enable */
	    cntr->last_index_count = cntr->raw_count;
	    cntr->Zmask = 0;
	    *(cntr->index_ena) = 0;
	}
	/* move on to next channel */
	cntr++;
    }
    /* done */
}

static void capture(void *arg, long period)
{
    counter_t *cntr;
    int n;

    cntr = arg;
    for (n = 0; n < num_chan; n++) {
        int raw_count;
        int counts;
    	/* check reset input */
	if (*(cntr->reset)) {
	    /* reset is active, reset the counter */
	    cntr->raw_count = 0;
            cntr->last_index_count = 0;
            cntr->last_count = 0;
	}
	/* capture raw counts to latches */
        raw_count = cntr->raw_count;
	*(cntr->count) = raw_count - cntr->last_index_count;
        counts = (raw_count - cntr->last_count);
        cntr->last_count = raw_count;

	/* check for change in scale value */
	if ( cntr->pos_scale != cntr->old_scale ) {
	    /* save new scale to detect future changes */
	    cntr->old_scale = cntr->pos_scale;
	    /* scale value has changed, test and update it */
	    if ((cntr->pos_scale < 1e-20) && (cntr->pos_scale > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		cntr->pos_scale = 1.0;
	    }
	    /* we actually want the reciprocal */
	    cntr->scale = 1.0 / cntr->pos_scale;
	}
	/* scale count to make floating point position */
	*(cntr->pos) = *(cntr->count) * cntr->scale;
	/* scale counts to make floating point velocity */
        *(cntr->vel) = counts * cntr->scale * 1e9 / period;

	/* update Zmask based on index_ena */
	if (*(cntr->index_ena)) {
	    cntr->Zmask = 3;
	} else {
	    cntr->Zmask = 0;
	}
	/* move on to next channel */
	cntr++;
    }
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_counter(int num, counter_t * addr)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export pins for the quadrature inputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.phase-A", num);
    retval = hal_pin_bit_new(buf, HAL_IN, &(addr->phaseA), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.phase-B", num);
    retval = hal_pin_bit_new(buf, HAL_IN, &(addr->phaseB), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.phase-Z", num);
    retval = hal_pin_bit_new(buf, HAL_IN, &(addr->phaseZ), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index enable input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.index-enable", num);
    retval = hal_pin_bit_new(buf, HAL_IO, &(addr->index_ena), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the reset input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.reset", num);
    retval = hal_pin_bit_new(buf, HAL_IN, &(addr->reset), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for raw counts */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.rawcounts", num);
    retval = hal_param_s32_new(buf, HAL_RO, &(addr->raw_count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counts captured by capture() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.counts", num);
    retval = hal_pin_s32_new(buf, HAL_OUT, &(addr->count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by capture() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.position", num);
    retval = hal_pin_float_new(buf, HAL_OUT, &(addr->pos), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled velocity captured by capture() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.velocity", num);
    retval = hal_pin_float_new(buf, HAL_OUT, &(addr->vel), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.position-scale", num);
    retval = hal_param_float_new(buf, HAL_RW, &(addr->pos_scale), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for x4 mode */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.x4-mode", num);
    retval = hal_param_bit_new(buf, HAL_RW, &(addr->x4_mode), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
