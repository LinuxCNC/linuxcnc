/********************************************************************
* Description:  counter.c
*               This file, 'counter.c', is a HAL component that 
*               provides software-based counting of pulse streams
*               with an optional reset input.
*
* Author: Chris Radek <chris@timeguy.com>
* License: GPL Version 2
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/
/** This file, 'counter.c', is a HAL component that provides software-
    based counting that is useful for spindle position sensing and
    maybe other things.  Instead of using a real encoder that outputs
    quadrature, some lathes have a sensor that generates a simple pulse
    stream as the spindle turns and an index pulse once per revolution.
    This component simply counts up when a "count" pulse (phase-A)
    is received, and if reset is enabled, resets when the "index"
    (phase-Z) pulse is received.

    This is of course only useful for a unidirectional spindle, as it
    is not possible to sense the direction of rotation.

    The maximum count rate will depend on the speed of the PC, but is
    expected to exceed 2kHz for even the slowest computers, and may
    well be over 25kHz on fast ones.  It is a realtime component.

    It supports up to eight counters, with optional index pulses.
    The number of counters is set by the module parameter 'num_chan'
    when the component is insmod'ed.

    The driver can optionally create a realtime thread, which is
    useful if a free-running driver is desired.  The module parameter
    'period' is a long int, corresponding to the thread period in
    nanoseconds.  If omitted, no thread will be created.  The driver
    exports variables for each counter's inputs and outputs.  It also
    exports two functions:  "counter.update-counters" must be called in
    a high speed thread, at least twice the maximum desired count rate.
    "counter.capture-position" can be called at a much slower rate,
    and updates the output variables.
*/

/** Copyright (C) 2006 Chris Radek <chris@timeguy.com>
 *
 *  Based heavily on the "encoder" hal module by John Kasunich
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

#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "hal.h"                /* HAL public API decls */

#ifdef MODULE
/* module information */
MODULE_AUTHOR("Chris Radek");
MODULE_DESCRIPTION("Pulse Counter for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
static int num_chan = 1;        /* number of channels - default = 1 */
MODULE_PARM(num_chan, "i");
MODULE_PARM_DESC(num_chan, "number of channels");
static long period = 0;         /* thread period - default = no thread */
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "thread period (nsecs)");
#endif /* MODULE */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data for a single counter */

typedef struct {
    unsigned char oldZ;		/* previous value of phase Z */
    unsigned char oldA;		/* previous value of phase A */
    unsigned char reset_on_index;
    unsigned char pad;		/* padding for alignment */
    hal_s32_t raw_count;	/* parameter: raw binary count value */
    hal_bit_t *phaseA;		/* quadrature input */
    hal_bit_t *phaseZ;		/* index pulse input */
    hal_bit_t *index_ena;	/* index enable input */
    hal_bit_t *reset;		/* counter reset input */
    hal_s32_t *count;		/* captured binary count value */
    hal_float_t *pos;		/* scaled position (floating point) */
    hal_float_t pos_scale;	/* parameter: scaling factor for pos */
    float old_scale;		/* stored scale value */
    double scale;		/* reciprocal value used for scaling */
} counter_t;

/* pointer to array of counter_t structs in shmem, 1 per counter */
static counter_t *counter_array;

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
	    "COUNTER: ERROR: invalid num_chan: %d\n", num_chan);
	return -EINVAL;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("counter");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "COUNTER: ERROR: hal_init() failed\n");
	return -EINVAL;
    }
    /* allocate shared memory for counter data */
    counter_array = hal_malloc(num_chan * sizeof(counter_t));
    if (!counter_array) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "COUNTER: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -ENOMEM;
    }
    /* export all the variables for each counter */
    for (n = 0; n < num_chan; n++) {
	/* export all vars */
	retval = export_counter(n, &(counter_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"COUNTER: ERROR: counter %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -EIO;
	}
	/* init counter */
	counter_array[n].oldZ = 0;
	counter_array[n].oldA = 0;
	counter_array[n].reset_on_index = 0;
	counter_array[n].raw_count = 0;
	*(counter_array[n].count) = 0;
	*(counter_array[n].pos) = 0.0;
	counter_array[n].pos_scale = 1.0;
	counter_array[n].old_scale = 1.0;
	counter_array[n].scale = 1.0;
    }
    /* export functions */
    retval = hal_export_funct("counter.update-counters", update,
	counter_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "COUNTER: ERROR: count funct export failed\n");
	hal_exit(comp_id);
	return -EIO;
    }
    retval = hal_export_funct("counter.capture-position", capture,
	counter_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "COUNTER: ERROR: capture funct export failed\n");
	hal_exit(comp_id);
	return -EIO;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"COUNTER: installed %d counter counters\n", num_chan);
    /* was 'period' specified in the insmod command? */
    if (period > 0) {
	/* create a thread */
	retval = hal_create_thread("counter.thread", period, 0);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"COUNTER: ERROR: could not create thread\n");
	    hal_exit(comp_id);
	    return -EINVAL;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "COUNTER: created %d uS thread\n",
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
*            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static void update(void *arg, long period)
{
    counter_t *cntr;
    int n;

    for (cntr = arg, n = 0; n < num_chan; cntr++, n++) {
        // count on rising edge
        if(!cntr->oldA && *cntr->phaseA)
            cntr->raw_count++;
        cntr->oldA = *cntr->phaseA;

        // reset on rising edge
        if(cntr->reset_on_index && !cntr->oldZ && *cntr->phaseZ) {
            cntr->raw_count = 0;
            *(cntr->index_ena) = 0;
        }
        cntr->oldZ = *cntr->phaseZ;
    }
}

static void capture(void *arg, long period)
{
    counter_t *cntr;
    int n;

    for (cntr = arg, n = 0; n < num_chan; cntr++, n++) {
	/* check reset input */
	if (*(cntr->reset)) {
	    /* reset is active, reset the counter */
	    cntr->raw_count = 0;
	}
	/* capture raw counts to latches */
	*(cntr->count) = cntr->raw_count;
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
	/* update reset_on_index based on index_ena */
        cntr->reset_on_index = *(cntr->index_ena);
    }
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
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.phase-A", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->phaseA), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.phase-Z", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->phaseZ), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index enable input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.index-enable", num);
    retval = hal_pin_bit_new(buf, HAL_RD_WR, &(addr->index_ena), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the reset input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.reset", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->reset), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for raw counts */
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.rawcounts", num);
    retval = hal_param_s32_new(buf, HAL_RD, &(addr->raw_count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counts captured by capture() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.counts", num);
    retval = hal_pin_s32_new(buf, HAL_WR, &(addr->count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by capture() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.position", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->pos), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "counter.%d.position-scale", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->pos_scale), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
